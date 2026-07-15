#pragma once

#include <string>
#include <vector>

struct VideoCell
{
    int cellId = -1;
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
    int cameraId = -1;
};

struct CameraLayout
{
    std::wstring name;
    int rows = 1;
    int cols = 1;
    std::vector<VideoCell> cells;
};
