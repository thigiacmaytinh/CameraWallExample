#pragma once

#include "CameraLayout.h"

class CameraLayoutFactory
{
public:
    static CameraLayout CreateGrid(int rows, int cols);
    static CameraLayout CreateStandard(int count);
    static CameraLayout CreateWide(int count);
    static bool Validate(const CameraLayout& layout);
};
