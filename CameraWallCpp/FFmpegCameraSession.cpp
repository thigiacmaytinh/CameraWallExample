#include "FFmpegCameraSession.h"
#include "D3D11WallRenderer.h"

#include <windows.h>
#include <chrono>
#include <cstdio>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

static enum AVPixelFormat g_d3d11PixelFormat = AV_PIX_FMT_D3D11;

static enum AVPixelFormat GetHwFormat(AVCodecContext*, const enum AVPixelFormat* pixFmts)
{
    for (const enum AVPixelFormat* p = pixFmts; *p != AV_PIX_FMT_NONE; ++p)
    {
        if (*p == g_d3d11PixelFormat)
            return *p;
    }
    return pixFmts[0];
}

static std::string WStringToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (size <= 0)
        return {};

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        &result[0],
        size,
        nullptr,
        nullptr);

    return result;
}

static AVBufferRef* CreateD3D11HwDeviceFromRenderer(D3D11WallRenderer* renderer)
{
    if (!renderer || !renderer->GetDevice() || !renderer->GetContext())
        return nullptr;

    AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!deviceRef)
        return nullptr;

    AVHWDeviceContext* hwctx = reinterpret_cast<AVHWDeviceContext*>(deviceRef->data);
    AVD3D11VADeviceContext* d3d11ctx = reinterpret_cast<AVD3D11VADeviceContext*>(hwctx->hwctx);

    d3d11ctx->device = renderer->GetDevice();
    d3d11ctx->device_context = renderer->GetContext();

    d3d11ctx->device->AddRef();
    d3d11ctx->device_context->AddRef();

    int ret = av_hwdevice_ctx_init(deviceRef);
    if (ret < 0)
    {
        av_buffer_unref(&deviceRef);
        return nullptr;
    }

    return deviceRef;
}

FFmpegCameraSession::FFmpegCameraSession(
    int cameraId,
    const std::wstring& rtspUrl,
    D3D11WallRenderer* renderer)
    : m_cameraId(cameraId),
      m_rtspUrl(rtspUrl),
      m_renderer(renderer)
{
}

FFmpegCameraSession::~FFmpegCameraSession()
{
    Stop();
}

void FFmpegCameraSession::Start()
{
    if (m_running)
    {
        Resume();
        return;
    }

    m_paused = false;
    m_pauseAfterCapture = false;
    m_running = true;
    m_thread = std::thread(&FFmpegCameraSession::DecodeLoop, this);
}

void FFmpegCameraSession::Stop()
{
    if (!m_running)
        return;

    m_running = false;
    m_paused = false;
    m_pauseAfterCapture = false;

    if (m_thread.joinable())
        m_thread.join();
}

void FFmpegCameraSession::Pause()
{
    if (m_running)
        m_paused = true;
}

void FFmpegCameraSession::Resume()
{
    if (m_running)
    {
        m_pauseAfterCapture = false;
        m_paused = false;
    }
}

bool FFmpegCameraSession::CaptureToFile(const std::wstring& filePath)
{
    if (!m_running || filePath.empty())
        return false;

    {
        std::lock_guard<std::mutex> lock(m_captureMutex);
        m_pendingCapturePath = filePath;
    }

    // A paused RTSP connection is closed to avoid buffering old frames. Temporarily
    // reconnect for one fresh capture, then return to the paused state.
    if (m_paused)
    {
        m_pauseAfterCapture = true;
        m_paused = false;
    }

    return true;
}

std::wstring FFmpegCameraSession::TakePendingCapturePath()
{
    std::lock_guard<std::mutex> lock(m_captureMutex);
    std::wstring result;
    result.swap(m_pendingCapturePath);
    return result;
}

int FFmpegCameraSession::InterruptCallback(void* opaque)
{
    FFmpegCameraSession* session = static_cast<FFmpegCameraSession*>(opaque);
    if (!session)
        return 1;

    return (!session->m_running || session->m_paused) ? 1 : 0;
}

void FFmpegCameraSession::DecodeLoop()
{
    while (m_running)
    {
        if (m_paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        DecodeOnce();

        if (m_running && !m_paused)
            std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void FFmpegCameraSession::DecodeOnce()
{
    std::string url = WStringToUtf8(m_rtspUrl);

    AVFormatContext* fmt = avformat_alloc_context();
    AVCodecContext* codecCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    AVDictionary* options = nullptr;

    int videoStreamIndex = -1;
    AVStream* videoStream = nullptr;
    const AVCodec* codec = nullptr;

    int ret = 0;

    if (!fmt)
        goto cleanup;

    fmt->interrupt_callback.callback = &FFmpegCameraSession::InterruptCallback;
    fmt->interrupt_callback.opaque = this;

    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "3000000", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);

    ret = avformat_open_input(&fmt, url.c_str(), nullptr, &options);
    av_dict_free(&options);
    options = nullptr;

    if (ret < 0)
        goto cleanup;

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0)
        goto cleanup;

    videoStreamIndex = av_find_best_stream(
        fmt,
        AVMEDIA_TYPE_VIDEO,
        -1,
        -1,
        nullptr,
        0);

    if (videoStreamIndex < 0)
        goto cleanup;

    videoStream = fmt->streams[videoStreamIndex];

    codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec)
        goto cleanup;

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
        goto cleanup;

    ret = avcodec_parameters_to_context(codecCtx, videoStream->codecpar);
    if (ret < 0)
        goto cleanup;

    hwDeviceCtx = CreateD3D11HwDeviceFromRenderer(m_renderer);
    if (!hwDeviceCtx)
        goto cleanup;

    codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
    if (!codecCtx->hw_device_ctx)
        goto cleanup;

    codecCtx->get_format = GetHwFormat;
    codecCtx->thread_count = 1;

    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0)
        goto cleanup;

    packet = av_packet_alloc();
    frame = av_frame_alloc();

    if (!packet || !frame)
        goto cleanup;

    while (m_running && !m_paused && av_read_frame(fmt, packet) >= 0)
    {
        if (packet->stream_index != videoStreamIndex)
        {
            av_packet_unref(packet);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, packet);
        av_packet_unref(packet);

        if (ret < 0)
            continue;

        while (m_running && !m_paused)
        {
            ret = avcodec_receive_frame(codecCtx, frame);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            if (ret < 0)
                break;

            if (frame->format == AV_PIX_FMT_D3D11)
            {
                ID3D11Texture2D* texture =
                    reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);

                int textureIndex =
                    static_cast<int>(reinterpret_cast<intptr_t>(frame->data[1]));

                if (m_renderer && texture)
                {
                    m_renderer->SubmitD3D11Frame(
                        m_cameraId,
                        texture,
                        textureIndex,
                        frame->width,
                        frame->height);
                }

                std::wstring capturePath = TakePendingCapturePath();
                if (!capturePath.empty())
                {
                    SaveFrameAsPng(frame, capturePath);
                    if (m_pauseAfterCapture.exchange(false))
                        m_paused = true;
                }
            }

            av_frame_unref(frame);
        }
    }

cleanup:

    if (options)
        av_dict_free(&options);

    if (frame)
        av_frame_free(&frame);

    if (packet)
        av_packet_free(&packet);

    if (codecCtx)
        avcodec_free_context(&codecCtx);

    if (hwDeviceCtx)
        av_buffer_unref(&hwDeviceCtx);

    if (fmt)
        avformat_close_input(&fmt);

    if (m_pauseAfterCapture.exchange(false))
    {
        m_paused = true;
        std::lock_guard<std::mutex> lock(m_captureMutex);
        m_pendingCapturePath.clear();
    }
}

bool FFmpegCameraSession::SaveFrameAsPng(AVFrame* hardwareFrame, const std::wstring& filePath)
{
    if (!hardwareFrame || filePath.empty())
        return false;

    AVFrame* softwareFrame = av_frame_alloc();
    AVFrame* rgbaFrame = av_frame_alloc();
    const AVCodec* pngCodec = nullptr;
    AVCodecContext* pngContext = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* swsContext = nullptr;
    FILE* output = nullptr;
    bool success = false;

    if (!softwareFrame || !rgbaFrame)
        goto cleanup;

    if (av_hwframe_transfer_data(softwareFrame, hardwareFrame, 0) < 0)
        goto cleanup;

    rgbaFrame->format = AV_PIX_FMT_RGBA;
    rgbaFrame->width = softwareFrame->width;
    rgbaFrame->height = softwareFrame->height;

    if (av_frame_get_buffer(rgbaFrame, 32) < 0)
        goto cleanup;

    swsContext = sws_getContext(
        softwareFrame->width,
        softwareFrame->height,
        static_cast<AVPixelFormat>(softwareFrame->format),
        rgbaFrame->width,
        rgbaFrame->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (!swsContext)
        goto cleanup;

    if (sws_scale(
        swsContext,
        softwareFrame->data,
        softwareFrame->linesize,
        0,
        softwareFrame->height,
        rgbaFrame->data,
        rgbaFrame->linesize) <= 0)
    {
        goto cleanup;
    }

    pngCodec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!pngCodec)
        goto cleanup;

    pngContext = avcodec_alloc_context3(pngCodec);
    if (!pngContext)
        goto cleanup;

    pngContext->width = rgbaFrame->width;
    pngContext->height = rgbaFrame->height;
    pngContext->pix_fmt = AV_PIX_FMT_RGBA;
    pngContext->time_base = AVRational{ 1, 25 };

    if (avcodec_open2(pngContext, pngCodec, nullptr) < 0)
        goto cleanup;

    packet = av_packet_alloc();
    if (!packet)
        goto cleanup;

    rgbaFrame->pts = 0;

    if (avcodec_send_frame(pngContext, rgbaFrame) < 0)
        goto cleanup;

    if (avcodec_receive_packet(pngContext, packet) < 0)
        goto cleanup;

    if (_wfopen_s(&output, filePath.c_str(), L"wb") != 0 || !output)
        goto cleanup;

    success = fwrite(packet->data, 1, static_cast<size_t>(packet->size), output) == static_cast<size_t>(packet->size);

cleanup:
    if (output)
        fclose(output);

    if (packet)
        av_packet_free(&packet);

    if (pngContext)
        avcodec_free_context(&pngContext);

    if (swsContext)
        sws_freeContext(swsContext);

    if (rgbaFrame)
        av_frame_free(&rgbaFrame);

    if (softwareFrame)
        av_frame_free(&softwareFrame);

    return success;
}
