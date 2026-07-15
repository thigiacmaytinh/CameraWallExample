#include "CameraWallApi.h"
#include "CameraWallEngine.h"
#include "CameraLayout.h"

VW_HANDLE VW_Create()
{
    return new CameraWallEngine();
}

void VW_Destroy(VW_HANDLE handle)
{
    if (!handle) return;
    delete reinterpret_cast<CameraWallEngine*>(handle);
}

bool VW_Initialize(VW_HANDLE handle, void* hwnd)
{
    if (!handle || !hwnd) return false;
    return reinterpret_cast<CameraWallEngine*>(handle)->Initialize(reinterpret_cast<HWND>(hwnd));
}

void VW_Resize(VW_HANDLE handle, int width, int height)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->Resize(width, height);
}

void VW_Start(VW_HANDLE handle)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->Start();
}

void VW_Stop(VW_HANDLE handle)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->Stop();
}

bool VW_AddCamera(VW_HANDLE handle, int cameraId, const wchar_t* rtspUrl)
{
    if (!handle || !rtspUrl) return false;
    return reinterpret_cast<CameraWallEngine*>(handle)->AddCamera(cameraId, rtspUrl);
}

void VW_RemoveCamera(VW_HANDLE handle, int cameraId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->RemoveCamera(cameraId);
}


void VW_PauseCamera(VW_HANDLE handle, int cameraId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->PauseCamera(cameraId);
}

void VW_ResumeCamera(VW_HANDLE handle, int cameraId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->ResumeCamera(cameraId);
}

bool VW_CaptureCamera(VW_HANDLE handle, int cameraId, const wchar_t* filePath)
{
    if (!handle || !filePath) return false;
    return reinterpret_cast<CameraWallEngine*>(handle)->CaptureCamera(cameraId, filePath);
}

void VW_SetGridLayout(VW_HANDLE handle, int rows, int cols)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->SetGridLayout(rows, cols);
}

void VW_SetStandardLayout(VW_HANDLE handle, int count)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->SetStandardLayout(count);
}

void VW_SetWideLayout(VW_HANDLE handle, int count)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->SetWideLayout(count);
}

bool VW_SetCustomLayout(VW_HANDLE handle, const wchar_t* name, int rows, int cols, const VW_CELL* cells, int cellCount)
{
    if (!handle || rows <= 0 || cols <= 0 || !cells || cellCount <= 0)
        return false;

    CameraLayout layout;
    layout.name = name ? name : L"Custom";
    layout.rows = rows;
    layout.cols = cols;

    for (int i = 0; i < cellCount; ++i)
    {
        VideoCell c;
        c.cellId = cells[i].cellId;
        c.x = cells[i].x;
        c.y = cells[i].y;
        c.w = cells[i].w;
        c.h = cells[i].h;
        c.cameraId = cells[i].cameraId;
        layout.cells.push_back(c);
    }

    reinterpret_cast<CameraWallEngine*>(handle)->SetCustomLayout(layout);
    return true;
}

void VW_AssignCameraToCell(VW_HANDLE handle, int cellId, int cameraId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->AssignCameraToCell(cellId, cameraId);
}


void VW_ClearCell(VW_HANDLE handle, int cellId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->ClearCell(cellId);
}

int VW_GetCellAt(VW_HANDLE handle, int x, int y)
{
    if (!handle) return -1;
    return reinterpret_cast<CameraWallEngine*>(handle)->GetCellAt(x, y);
}

bool VW_GetCellBounds(VW_HANDLE handle, int cellId, int* x, int* y, int* width, int* height)
{
    if (!handle || !x || !y || !width || !height)
        return false;

    return reinterpret_cast<CameraWallEngine*>(handle)->GetCellBounds(cellId, *x, *y, *width, *height);
}

void VW_SetSelectedCell(VW_HANDLE handle, int cellId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->SetSelectedCell(cellId);
}

void VW_SetHoveredCell(VW_HANDLE handle, int cellId)
{
    if (!handle) return;
    reinterpret_cast<CameraWallEngine*>(handle)->SetHoveredCell(cellId);
}

