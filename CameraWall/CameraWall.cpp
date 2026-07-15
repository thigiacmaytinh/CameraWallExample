#include "CameraWall.h"
#include "CameraWallApi.h"
#include <vector>
#include <vcclr.h>

using namespace System;

namespace CameraWall
{
    CameraWall::CameraWall()
    {
        m_handle = VW_Create();
    }

    CameraWall::~CameraWall()
    {
        this->!CameraWall();
        GC::SuppressFinalize(this);
    }

    CameraWall::!CameraWall()
    {
        if (m_handle != nullptr)
        {
            VW_Stop(m_handle);
            VW_Destroy(m_handle);
            m_handle = nullptr;
        }
    }

    bool CameraWall::Initialize(IntPtr hwnd)
    {
        if (m_handle == nullptr)
            return false;

        return VW_Initialize(m_handle, hwnd.ToPointer());
    }

    void CameraWall::Resize(int width, int height)
    {
        if (m_handle == nullptr)
            return;

        VW_Resize(m_handle, width, height);
    }

    void CameraWall::Start()
    {
        if (m_handle == nullptr)
            return;

        VW_Start(m_handle);
    }

    void CameraWall::Stop()
    {
        if (m_handle == nullptr)
            return;

        VW_Stop(m_handle);
    }

    bool CameraWall::AddCamera(int cameraId, String^ rtspUrl)
    {
        if (m_handle == nullptr || String::IsNullOrWhiteSpace(rtspUrl))
            return false;

        pin_ptr<const wchar_t> urlPtr = PtrToStringChars(rtspUrl);
        return VW_AddCamera(m_handle, cameraId, urlPtr);
    }

    void CameraWall::RemoveCamera(int cameraId)
    {
        if (m_handle == nullptr)
            return;

        VW_RemoveCamera(m_handle, cameraId);
    }


    void CameraWall::PauseCamera(int cameraId)
    {
        if (m_handle == nullptr)
            return;

        VW_PauseCamera(m_handle, cameraId);
    }

    void CameraWall::ResumeCamera(int cameraId)
    {
        if (m_handle == nullptr)
            return;

        VW_ResumeCamera(m_handle, cameraId);
    }

    bool CameraWall::CaptureCamera(int cameraId, String^ filePath)
    {
        if (m_handle == nullptr || String::IsNullOrWhiteSpace(filePath))
            return false;

        pin_ptr<const wchar_t> filePathPtr = PtrToStringChars(filePath);
        return VW_CaptureCamera(m_handle, cameraId, filePathPtr);
    }

    void CameraWall::SetGridLayout(int rows, int cols)
    {
        if (m_handle == nullptr)
            return;

        VW_SetGridLayout(m_handle, rows, cols);
    }

    void CameraWall::SetStandardLayout(int count)
    {
        if (m_handle == nullptr)
            return;

        VW_SetStandardLayout(m_handle, count);
    }

    void CameraWall::SetWideLayout(int count)
    {
        if (m_handle == nullptr)
            return;

        VW_SetWideLayout(m_handle, count);
    }

    bool CameraWall::SetCustomLayout(String^ name, int rows, int cols, array<CellDto^>^ cells)
    {
        if (m_handle == nullptr || cells == nullptr || cells->Length == 0)
            return false;

        std::vector<VW_CELL> nativeCells;
        nativeCells.reserve(cells->Length);

        for each (CellDto^ cell in cells)
        {
            if (cell == nullptr)
                continue;

            VW_CELL c{};
            c.cellId = cell->CellId;
            c.x = cell->X;
            c.y = cell->Y;
            c.w = cell->W;
            c.h = cell->H;
            c.cameraId = cell->CameraId;
            nativeCells.push_back(c);
        }

        String^ safeName = String::IsNullOrWhiteSpace(name) ? "Custom" : name;
        pin_ptr<const wchar_t> namePtr = PtrToStringChars(safeName);

        return VW_SetCustomLayout(
            m_handle,
            namePtr,
            rows,
            cols,
            nativeCells.data(),
            static_cast<int>(nativeCells.size()));
    }

    void CameraWall::AssignCameraToCell(int cellId, int cameraId)
    {
        if (m_handle == nullptr)
            return;

        VW_AssignCameraToCell(m_handle, cellId, cameraId);
    }
}


namespace CameraWall
{
    void CameraWall::ClearCell(int cellId)
    {
        if (m_handle == nullptr)
            return;

        VW_ClearCell(m_handle, cellId);
    }

    int CameraWall::GetCellAt(int x, int y)
    {
        if (m_handle == nullptr)
            return -1;

        return VW_GetCellAt(m_handle, x, y);
    }
}

namespace CameraWall
{
    bool CameraWall::GetCellBounds(int cellId, int% x, int% y, int% width, int% height)
    {
        if (m_handle == nullptr)
            return false;

        int nativeX = 0;
        int nativeY = 0;
        int nativeWidth = 0;
        int nativeHeight = 0;

        bool result = VW_GetCellBounds(
            m_handle,
            cellId,
            &nativeX,
            &nativeY,
            &nativeWidth,
            &nativeHeight);

        x = nativeX;
        y = nativeY;
        width = nativeWidth;
        height = nativeHeight;
        return result;
    }

    void CameraWall::SetSelectedCell(int cellId)
    {
        if (m_handle != nullptr)
            VW_SetSelectedCell(m_handle, cellId);
    }

    void CameraWall::SetHoveredCell(int cellId)
    {
        if (m_handle != nullptr)
            VW_SetHoveredCell(m_handle, cellId);
    }
}

