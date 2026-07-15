#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include "CameraLayout.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct LatestCameraTexture
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    int width = 0;
    int height = 0;
    uint64_t frameNumber = 0;
};

class D3D11WallRenderer
{
public:
    D3D11WallRenderer();
    ~D3D11WallRenderer();

    bool Initialize(HWND hwnd);
    void Resize(int width, int height);

    void Start();
    void Stop();

    void SetLayout(const CameraLayout& layout);
    void AssignCameraToCell(int cellId, int cameraId);
    void ClearCell(int cellId);
    int GetCellAt(int x, int y);
    bool GetCellBounds(int cellId, int& x, int& y, int& width, int& height);
    void SetSelectedCell(int cellId);
    void SetHoveredCell(int cellId);
    void ClearCamera(int cameraId);

    // Called by decoder threads. Source texture must come from the same D3D11 device.
    void SubmitD3D11Frame(
        int cameraId,
        ID3D11Texture2D* sourceTexture,
        int sourceArraySlice,
        int width,
        int height);

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

private:
    struct Vertex
    {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateShadersAndStates();
    void CreateRenderTarget();
    void ReleaseRenderTarget();

    void RenderLoop();
    void RenderOnce();

    bool CopyToRenderTexture(
        int cameraId,
        ID3D11Texture2D* sourceTexture,
        int sourceArraySlice,
        int width,
        int height);

    bool CreateOrUpdateSrvs(
        LatestCameraTexture& camera,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvY,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvUV,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvBgra);

    void DrawTextureTile(
        const LatestCameraTexture& camera,
        float left,
        float top,
        float width,
        float height);

    void DrawSolidRect(
        float left,
        float top,
        float width,
        float height,
        float red,
        float green,
        float blue,
        float alpha = 1.0f);

    void DrawBorder(
        float left,
        float top,
        float width,
        float height,
        float thickness,
        float red,
        float green,
        float blue,
        float alpha = 1.0f);

    void DrawEmptyTile(float left, float top, float width, float height);
    bool UpdateVertexBuffer(const Vertex* vertices, size_t count);
    void BindCommonPipeline();

private:
    HWND m_hwnd = nullptr;
    int m_width = 1;
    int m_height = 1;

    std::atomic<bool> m_running{ false };
    std::thread m_renderThread;

    std::mutex m_d3dMutex;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_psNv12;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_psBgra;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_psSolid;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;

    std::mutex m_layoutMutex;
    CameraLayout m_layout;
    int m_selectedCellId = -1;
    int m_hoveredCellId = -1;

    std::mutex m_textureMutex;
    std::unordered_map<int, LatestCameraTexture> m_cameraTextures;
    std::unordered_map<int, uint64_t> m_frameCounters;
};
