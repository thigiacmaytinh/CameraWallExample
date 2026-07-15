#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "CameraLayout.h"

class D3D11WallRenderer;
class FFmpegCameraSession;

class CameraWallEngine
{
public:
    CameraWallEngine();
    ~CameraWallEngine();

    bool Initialize(HWND hwnd);
    void Resize(int width, int height);

    void Start();
    void Stop();

    bool AddCamera(int cameraId, const std::wstring& rtspUrl);
    void RemoveCamera(int cameraId);
    void PauseCamera(int cameraId);
    void ResumeCamera(int cameraId);
    bool CaptureCamera(int cameraId, const std::wstring& filePath);

    void SetGridLayout(int rows, int cols);
    void SetStandardLayout(int count);
    void SetWideLayout(int count);
    void SetCustomLayout(const CameraLayout& layout);
    void AssignCameraToCell(int cellId, int cameraId);
    void ClearCell(int cellId);
    int GetCellAt(int x, int y);
    bool GetCellBounds(int cellId, int& x, int& y, int& width, int& height);
    void SetSelectedCell(int cellId);
    void SetHoveredCell(int cellId);

private:
    std::unique_ptr<D3D11WallRenderer> m_renderer;
    std::mutex m_mutex;
    std::unordered_map<int, std::unique_ptr<FFmpegCameraSession>> m_cameras;
    bool m_running = false;
};
