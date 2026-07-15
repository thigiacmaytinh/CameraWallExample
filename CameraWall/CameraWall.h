#pragma once

using namespace System;

namespace CameraWall
{
    public ref class CellDto sealed
    {
    public:
        int CellId;
        int X;
        int Y;
        int W;
        int H;
        int CameraId;
    };

    public ref class CameraWall sealed : IDisposable
    {
    public:
        CameraWall();
        ~CameraWall();
        !CameraWall();

        bool Initialize(IntPtr hwnd);
        void Resize(int width, int height);

        void Start();
        void Stop();

        bool AddCamera(int cameraId, String^ rtspUrl);
        void RemoveCamera(int cameraId);
        void PauseCamera(int cameraId);
        void ResumeCamera(int cameraId);
        bool CaptureCamera(int cameraId, String^ filePath);

        void SetGridLayout(int rows, int cols);
        void SetStandardLayout(int count);
        void SetWideLayout(int count);
        bool SetCustomLayout(String^ name, int rows, int cols, array<CellDto^>^ cells);
        void AssignCameraToCell(int cellId, int cameraId);
        void ClearCell(int cellId);
        int GetCellAt(int x, int y);
        bool GetCellBounds(int cellId, int% x, int% y, int% width, int% height);
        void SetSelectedCell(int cellId);
        void SetHoveredCell(int cellId);

    private:
        void* m_handle;
    };
}
