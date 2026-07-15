#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class D3D11WallRenderer;
struct AVFrame;

class FFmpegCameraSession
{
public:
    FFmpegCameraSession(
        int cameraId,
        const std::wstring& rtspUrl,
        D3D11WallRenderer* renderer);

    ~FFmpegCameraSession();

    void Start();
    void Stop();
    void Pause();
    void Resume();
    bool CaptureToFile(const std::wstring& filePath);

    int CameraId() const { return m_cameraId; }
    bool IsPaused() const { return m_paused.load(); }

private:
    static int InterruptCallback(void* opaque);
    void DecodeLoop();
    void DecodeOnce();
    bool SaveFrameAsPng(AVFrame* hardwareFrame, const std::wstring& filePath);
    std::wstring TakePendingCapturePath();

private:
    int m_cameraId = -1;
    std::wstring m_rtspUrl;
    D3D11WallRenderer* m_renderer = nullptr;

    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_paused{ false };
    std::atomic<bool> m_pauseAfterCapture{ false };
    std::thread m_thread;

    std::mutex m_captureMutex;
    std::wstring m_pendingCapturePath;
};
