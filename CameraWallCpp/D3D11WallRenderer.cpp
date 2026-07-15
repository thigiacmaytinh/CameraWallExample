#include "D3D11WallRenderer.h"
#include "CameraLayoutFactory.h"
#include <d3d10_1.h>
#include <chrono>
#include <algorithm>
#include <cstring>

static const char* g_shaderCode = R"(
struct VS_IN
{
    float3 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_OUT
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

Texture2D texY    : register(t0);
Texture2D texUV   : register(t1);
Texture2D texBgra : register(t0);
SamplerState linearSampler : register(s0);

float4 PSNv12(VS_OUT input) : SV_Target
{
    float y = texY.Sample(linearSampler, input.uv).r;
    float2 uv = texUV.Sample(linearSampler, input.uv).rg;

    float u = uv.x - 0.5;
    float v = uv.y - 0.5;

    float r = y + 1.5748 * v;
    float g = y - 0.1873 * u - 0.4681 * v;
    float b = y + 1.8556 * u;

    return float4(saturate(r), saturate(g), saturate(b), 1.0);
}

float4 PSBgra(VS_OUT input) : SV_Target
{
    return texBgra.Sample(linearSampler, input.uv);
}

float4 PSSolid(VS_OUT input) : SV_Target
{
    return input.color;
}
)";

D3D11WallRenderer::D3D11WallRenderer()
{
}

D3D11WallRenderer::~D3D11WallRenderer()
{
    Stop();
}

bool D3D11WallRenderer::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    m_width = static_cast<int>(std::max(1L, rc.right - rc.left));
    m_height = static_cast<int>(std::max(1L, rc.bottom - rc.top));

    if (!CreateDeviceAndSwapChain(hwnd))
        return false;

    if (!CreateShadersAndStates())
        return false;

    m_layout = CameraLayoutFactory::CreateGrid(2, 2);
    return true;
}

bool D3D11WallRenderer::CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL created{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &sd,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &created,
        m_context.GetAddressOf());

    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(m_device.As(&multithread)))
        multithread->SetMultithreadProtected(TRUE);

    CreateRenderTarget();
    return true;
}

bool D3D11WallRenderer::CreateShadersAndStates()
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psNv12Blob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBgraBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psSolidBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr,
        "VSMain", "vs_4_0", 0, 0, vsBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr,
        "PSNv12", "ps_4_0", 0, 0, psNv12Blob.GetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr,
        "PSBgra", "ps_4_0", 0, 0, psBgraBlob.GetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr,
        "PSSolid", "ps_4_0", 0, 0, psSolidBlob.GetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vs.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psNv12Blob->GetBufferPointer(), psNv12Blob->GetBufferSize(), nullptr, m_psNv12.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psBgraBlob->GetBufferPointer(), psBgraBlob->GetBufferSize(), nullptr, m_psBgra.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psSolidBlob->GetBufferPointer(), psSolidBlob->GetBufferSize(), nullptr, m_psSolid.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_INPUT_ELEMENT_DESC inputDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = m_device->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = sizeof(Vertex) * 6;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&vbDesc, nullptr, m_vertexBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SAMPLER_DESC samp{};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp.MinLOD = 0;
    samp.MaxLOD = D3D11_FLOAT32_MAX;

    hr = m_device->CreateSamplerState(&samp, m_sampler.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState(&blend, m_blendState.GetAddressOf());

    return SUCCEEDED(hr);
}

void D3D11WallRenderer::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (SUCCEEDED(hr))
        m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
}

void D3D11WallRenderer::ReleaseRenderTarget()
{
    if (m_context)
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
}

void D3D11WallRenderer::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    std::lock_guard<std::mutex> lock(m_d3dMutex);

    m_width = width;
    m_height = height;

    if (!m_swapChain)
        return;

    ReleaseRenderTarget();
    HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (SUCCEEDED(hr))
        CreateRenderTarget();
}

void D3D11WallRenderer::Start()
{
    if (m_running)
        return;

    m_running = true;
    m_renderThread = std::thread(&D3D11WallRenderer::RenderLoop, this);
}

void D3D11WallRenderer::Stop()
{
    if (!m_running)
        return;

    m_running = false;
    if (m_renderThread.joinable())
        m_renderThread.join();
}

void D3D11WallRenderer::SetLayout(const CameraLayout& layout)
{
    if (!CameraLayoutFactory::Validate(layout))
        return;

    std::lock_guard<std::mutex> lock(m_layoutMutex);
    m_layout = layout;
    m_hoveredCellId = -1;

    bool selectedStillExists = false;
    for (const auto& cell : m_layout.cells)
    {
        if (cell.cellId == m_selectedCellId)
        {
            selectedStillExists = true;
            break;
        }
    }

    if (!selectedStillExists)
        m_selectedCellId = m_layout.cells.empty() ? -1 : m_layout.cells.front().cellId;
}

void D3D11WallRenderer::AssignCameraToCell(int cellId, int cameraId)
{
    std::lock_guard<std::mutex> lock(m_layoutMutex);
    for (auto& c : m_layout.cells)
    {
        if (c.cellId == cellId)
        {
            c.cameraId = cameraId;
            return;
        }
    }
}

void D3D11WallRenderer::ClearCell(int cellId)
{
    AssignCameraToCell(cellId, -1);
}

int D3D11WallRenderer::GetCellAt(int px, int py)
{
    std::lock_guard<std::mutex> d3dLock(m_d3dMutex);
    std::lock_guard<std::mutex> layoutLock(m_layoutMutex);

    if (m_layout.rows <= 0 || m_layout.cols <= 0 || m_width <= 0 || m_height <= 0)
        return -1;

    float unitW = static_cast<float>(m_width) / m_layout.cols;
    float unitH = static_cast<float>(m_height) / m_layout.rows;

    for (const auto& cell : m_layout.cells)
    {
        float x = cell.x * unitW;
        float y = cell.y * unitH;
        float w = cell.w * unitW;
        float h = cell.h * unitH;

        if (px >= x && px < x + w && py >= y && py < y + h)
            return cell.cellId;
    }

    return -1;
}

bool D3D11WallRenderer::GetCellBounds(int cellId, int& x, int& y, int& width, int& height)
{
    std::lock_guard<std::mutex> d3dLock(m_d3dMutex);
    std::lock_guard<std::mutex> layoutLock(m_layoutMutex);

    if (m_layout.rows <= 0 || m_layout.cols <= 0 || m_width <= 0 || m_height <= 0)
        return false;

    float unitW = static_cast<float>(m_width) / m_layout.cols;
    float unitH = static_cast<float>(m_height) / m_layout.rows;

    for (const auto& cell : m_layout.cells)
    {
        if (cell.cellId != cellId)
            continue;

        x = static_cast<int>(cell.x * unitW);
        y = static_cast<int>(cell.y * unitH);
        width = std::max(1, static_cast<int>(cell.w * unitW));
        height = std::max(1, static_cast<int>(cell.h * unitH));
        return true;
    }

    return false;
}

void D3D11WallRenderer::SetSelectedCell(int cellId)
{
    std::lock_guard<std::mutex> lock(m_layoutMutex);
    m_selectedCellId = cellId;
}

void D3D11WallRenderer::SetHoveredCell(int cellId)
{
    std::lock_guard<std::mutex> lock(m_layoutMutex);
    m_hoveredCellId = cellId;
}

void D3D11WallRenderer::ClearCamera(int cameraId)
{
    std::lock_guard<std::mutex> d3dLock(m_d3dMutex);

    {
        std::lock_guard<std::mutex> lock(m_textureMutex);
        m_cameraTextures.erase(cameraId);
        m_frameCounters.erase(cameraId);
    }

    std::lock_guard<std::mutex> lock(m_layoutMutex);
    for (auto& c : m_layout.cells)
    {
        if (c.cameraId == cameraId)
            c.cameraId = -1;
    }
}

void D3D11WallRenderer::SubmitD3D11Frame(
    int cameraId,
    ID3D11Texture2D* sourceTexture,
    int sourceArraySlice,
    int width,
    int height)
{
    if (!sourceTexture || width <= 0 || height <= 0)
        return;

    std::lock_guard<std::mutex> lock(m_d3dMutex);
    CopyToRenderTexture(cameraId, sourceTexture, sourceArraySlice, width, height);
}

bool D3D11WallRenderer::CopyToRenderTexture(
    int cameraId,
    ID3D11Texture2D* sourceTexture,
    int sourceArraySlice,
    int width,
    int height)
{
    D3D11_TEXTURE2D_DESC srcDesc{};
    sourceTexture->GetDesc(&srcDesc);

    std::lock_guard<std::mutex> textureLock(m_textureMutex);
    LatestCameraTexture& cam = m_cameraTextures[cameraId];

    bool needCreate = false;
    if (!cam.texture)
        needCreate = true;
    else
    {
        D3D11_TEXTURE2D_DESC oldDesc{};
        cam.texture->GetDesc(&oldDesc);
        needCreate = oldDesc.Width != srcDesc.Width || oldDesc.Height != srcDesc.Height || oldDesc.Format != srcDesc.Format;
    }

    if (needCreate)
    {
        D3D11_TEXTURE2D_DESC dst{};
        dst.Width = srcDesc.Width;
        dst.Height = srcDesc.Height;
        dst.MipLevels = 1;
        dst.ArraySize = 1;
        dst.Format = srcDesc.Format;
        dst.SampleDesc.Count = 1;
        dst.Usage = D3D11_USAGE_DEFAULT;
        dst.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        dst.CPUAccessFlags = 0;
        dst.MiscFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> newTexture;
        HRESULT hr = m_device->CreateTexture2D(&dst, nullptr, newTexture.GetAddressOf());
        if (FAILED(hr))
            return false;

        cam.texture = newTexture;
        cam.format = srcDesc.Format;
        cam.width = static_cast<int>(srcDesc.Width);
        cam.height = static_cast<int>(srcDesc.Height);
    }

    UINT srcSubresource = D3D11CalcSubresource(0, sourceArraySlice, 1);
    m_context->CopySubresourceRegion(cam.texture.Get(), 0, 0, 0, 0, sourceTexture, srcSubresource, nullptr);
    cam.frameNumber = ++m_frameCounters[cameraId];
    return true;
}

void D3D11WallRenderer::RenderLoop()
{
    while (m_running)
    {
        RenderOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void D3D11WallRenderer::RenderOnce()
{
    std::lock_guard<std::mutex> d3dLock(m_d3dMutex);

    if (!m_context || !m_swapChain || !m_rtv)
        return;

    float clear[4] = { 0.025f, 0.028f, 0.032f, 1.0f };
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_rtv.Get(), clear);

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xffffffff);

    CameraLayout layoutCopy;
    int selectedCellId = -1;
    int hoveredCellId = -1;
    {
        std::lock_guard<std::mutex> lock(m_layoutMutex);
        layoutCopy = m_layout;
        selectedCellId = m_selectedCellId;
        hoveredCellId = m_hoveredCellId;
    }

    if (layoutCopy.rows <= 0 || layoutCopy.cols <= 0)
        layoutCopy = CameraLayoutFactory::CreateGrid(1, 1);

    float unitW = static_cast<float>(m_width) / layoutCopy.cols;
    float unitH = static_cast<float>(m_height) / layoutCopy.rows;

    for (const auto& cell : layoutCopy.cells)
    {
        float x = cell.x * unitW;
        float y = cell.y * unitH;
        float w = cell.w * unitW;
        float h = cell.h * unitH;

        LatestCameraTexture cam;
        bool hasFrame = false;
        {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            auto it = m_cameraTextures.find(cell.cameraId);
            if (it != m_cameraTextures.end() && it->second.texture)
            {
                cam = it->second;
                hasFrame = true;
            }
        }

        const float contentInset = 2.0f;
        if (hasFrame)
            DrawTextureTile(cam, x + contentInset, y + contentInset, w - contentInset * 2, h - contentInset * 2);
        else
            DrawEmptyTile(x + contentInset, y + contentInset, w - contentInset * 2, h - contentInset * 2);

        DrawBorder(x, y, w, h, 1.0f, 0.16f, 0.18f, 0.21f);
    }

    auto drawCellOverlay = [&](int cellId, float thickness, float red, float green, float blue)
    {
        if (cellId < 0)
            return;

        for (const auto& cell : layoutCopy.cells)
        {
            if (cell.cellId != cellId)
                continue;

            float x = cell.x * unitW;
            float y = cell.y * unitH;
            float w = cell.w * unitW;
            float h = cell.h * unitH;
            DrawBorder(x, y, w, h, thickness, red, green, blue);
            return;
        }
    };

    if (hoveredCellId != selectedCellId)
        drawCellOverlay(hoveredCellId, 1.5f, 0.90f, 0.92f, 0.95f);

    drawCellOverlay(selectedCellId, 2.0f, 0.94f, 0.12f, 0.10f);

    m_swapChain->Present(1, 0);
}

bool D3D11WallRenderer::CreateOrUpdateSrvs(
    LatestCameraTexture& camera,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvY,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvUV,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvBgra)
{
    if (!camera.texture)
        return false;

    if (camera.format == DXGI_FORMAT_NV12)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC yDesc{};
        yDesc.Format = DXGI_FORMAT_R8_UNORM;
        yDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        yDesc.Texture2D.MipLevels = 1;
        yDesc.Texture2D.MostDetailedMip = 0;

        HRESULT hr = m_device->CreateShaderResourceView(camera.texture.Get(), &yDesc, srvY.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC uvDesc{};
        uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        uvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        uvDesc.Texture2D.MipLevels = 1;
        uvDesc.Texture2D.MostDetailedMip = 0;

        hr = m_device->CreateShaderResourceView(camera.texture.Get(), &uvDesc, srvUV.GetAddressOf());
        return SUCCEEDED(hr);
    }

    if (camera.format == DXGI_FORMAT_B8G8R8A8_UNORM || camera.format == DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = camera.format;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;

        HRESULT hr = m_device->CreateShaderResourceView(camera.texture.Get(), &desc, srvBgra.GetAddressOf());
        return SUCCEEDED(hr);
    }

    return false;
}

bool D3D11WallRenderer::UpdateVertexBuffer(const Vertex* vertices, size_t count)
{
    if (!vertices || count == 0 || count > 6)
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return false;

    memcpy(mapped.pData, vertices, sizeof(Vertex) * count);
    m_context->Unmap(m_vertexBuffer.Get(), 0);
    return true;
}

void D3D11WallRenderer::BindCommonPipeline()
{
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->VSSetShader(m_vs.Get(), nullptr, 0);
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
}

void D3D11WallRenderer::DrawTextureTile(
    const LatestCameraTexture& camera,
    float left,
    float top,
    float width,
    float height)
{
    if (!camera.texture || width <= 1 || height <= 1)
        return;

    float l = (left / m_width) * 2.0f - 1.0f;
    float r = ((left + width) / m_width) * 2.0f - 1.0f;
    float t = 1.0f - (top / m_height) * 2.0f;
    float b = 1.0f - ((top + height) / m_height) * 2.0f;

    Vertex vertices[6] =
    {
        { l, t, 0.0f, 0.0f, 0.0f, 1, 1, 1, 1 },
        { r, t, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1 },
        { l, b, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1 },
        { l, b, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1 },
        { r, t, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1 },
        { r, b, 0.0f, 1.0f, 1.0f, 1, 1, 1, 1 }
    };

    if (!UpdateVertexBuffer(vertices, ARRAYSIZE(vertices)))
        return;

    BindCommonPipeline();

    LatestCameraTexture tmp = camera;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvY;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvUV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvBgra;

    if (!CreateOrUpdateSrvs(tmp, srvY, srvUV, srvBgra))
        return;

    if (camera.format == DXGI_FORMAT_NV12)
    {
        ID3D11ShaderResourceView* srvs[2] = { srvY.Get(), srvUV.Get() };
        m_context->PSSetShaderResources(0, 2, srvs);
        m_context->PSSetShader(m_psNv12.Get(), nullptr, 0);
    }
    else
    {
        ID3D11ShaderResourceView* srvs[1] = { srvBgra.Get() };
        m_context->PSSetShaderResources(0, 1, srvs);
        m_context->PSSetShader(m_psBgra.Get(), nullptr, 0);
    }

    m_context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSrvs[2] = { nullptr, nullptr };
    m_context->PSSetShaderResources(0, 2, nullSrvs);
}

void D3D11WallRenderer::DrawSolidRect(
    float left,
    float top,
    float width,
    float height,
    float red,
    float green,
    float blue,
    float alpha)
{
    if (width <= 0.0f || height <= 0.0f || m_width <= 0 || m_height <= 0)
        return;

    float l = (left / m_width) * 2.0f - 1.0f;
    float r = ((left + width) / m_width) * 2.0f - 1.0f;
    float t = 1.0f - (top / m_height) * 2.0f;
    float b = 1.0f - ((top + height) / m_height) * 2.0f;

    Vertex vertices[6] =
    {
        { l, t, 0.0f, 0, 0, red, green, blue, alpha },
        { r, t, 0.0f, 0, 0, red, green, blue, alpha },
        { l, b, 0.0f, 0, 0, red, green, blue, alpha },
        { l, b, 0.0f, 0, 0, red, green, blue, alpha },
        { r, t, 0.0f, 0, 0, red, green, blue, alpha },
        { r, b, 0.0f, 0, 0, red, green, blue, alpha }
    };

    if (!UpdateVertexBuffer(vertices, ARRAYSIZE(vertices)))
        return;

    BindCommonPipeline();
    m_context->PSSetShader(m_psSolid.Get(), nullptr, 0);
    m_context->Draw(6, 0);
}

void D3D11WallRenderer::DrawBorder(
    float left,
    float top,
    float width,
    float height,
    float thickness,
    float red,
    float green,
    float blue,
    float alpha)
{
    if (width <= 0.0f || height <= 0.0f || thickness <= 0.0f)
        return;

    thickness = std::min(thickness, std::min(width * 0.5f, height * 0.5f));

    DrawSolidRect(left, top, width, thickness, red, green, blue, alpha);
    DrawSolidRect(left, top + height - thickness, width, thickness, red, green, blue, alpha);
    DrawSolidRect(left, top + thickness, thickness, height - thickness * 2, red, green, blue, alpha);
    DrawSolidRect(left + width - thickness, top + thickness, thickness, height - thickness * 2, red, green, blue, alpha);
}

void D3D11WallRenderer::DrawEmptyTile(float left, float top, float width, float height)
{
    DrawSolidRect(left, top, width, height, 0.30f, 0.31f, 0.34f, 1.0f);
}
