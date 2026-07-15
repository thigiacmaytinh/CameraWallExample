#pragma once

#ifdef VIDEOWALLCORE_EXPORTS
#define VW_API __declspec(dllexport)
#else
#define VW_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* VW_HANDLE;

struct VW_CELL
{
    int cellId;
    int x;
    int y;
    int w;
    int h;
    int cameraId;
};

VW_API VW_HANDLE VW_Create();
VW_API void VW_Destroy(VW_HANDLE handle);

VW_API bool VW_Initialize(VW_HANDLE handle, void* hwnd);
VW_API void VW_Resize(VW_HANDLE handle, int width, int height);

VW_API void VW_Start(VW_HANDLE handle);
VW_API void VW_Stop(VW_HANDLE handle);

VW_API bool VW_AddCamera(VW_HANDLE handle, int cameraId, const wchar_t* rtspUrl);
VW_API void VW_RemoveCamera(VW_HANDLE handle, int cameraId);
VW_API void VW_PauseCamera(VW_HANDLE handle, int cameraId);
VW_API void VW_ResumeCamera(VW_HANDLE handle, int cameraId);
VW_API bool VW_CaptureCamera(VW_HANDLE handle, int cameraId, const wchar_t* filePath);

VW_API void VW_SetGridLayout(VW_HANDLE handle, int rows, int cols);
VW_API void VW_SetStandardLayout(VW_HANDLE handle, int count);
VW_API void VW_SetWideLayout(VW_HANDLE handle, int count);
VW_API bool VW_SetCustomLayout(VW_HANDLE handle, const wchar_t* name, int rows, int cols, const VW_CELL* cells, int cellCount);
VW_API void VW_AssignCameraToCell(VW_HANDLE handle, int cellId, int cameraId);
VW_API void VW_ClearCell(VW_HANDLE handle, int cellId);
VW_API int VW_GetCellAt(VW_HANDLE handle, int x, int y);
VW_API bool VW_GetCellBounds(VW_HANDLE handle, int cellId, int* x, int* y, int* width, int* height);
VW_API void VW_SetSelectedCell(VW_HANDLE handle, int cellId);
VW_API void VW_SetHoveredCell(VW_HANDLE handle, int cellId);

#ifdef __cplusplus
}
#endif
