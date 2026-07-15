#include "CameraLayoutFactory.h"
#include <vector>

CameraLayout CameraLayoutFactory::CreateGrid(int rows, int cols)
{
    if (rows <= 0) rows = 1;
    if (cols <= 0) cols = 1;

    CameraLayout layout;
    layout.name = L"Grid";
    layout.rows = rows;
    layout.cols = cols;

    int id = 0;
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            layout.cells.push_back({ id++, x, y, 1, 1, -1 });
        }
    }

    return layout;
}

CameraLayout CameraLayoutFactory::CreateStandard(int count)
{
    switch (count)
    {
    case 1:  return CreateGrid(1, 1);
    case 4:  return CreateGrid(2, 2);
    case 9:  return CreateGrid(3, 3);
    case 16: return CreateGrid(4, 4);
    case 25: return CreateGrid(5, 5);
    case 32: return CreateGrid(4, 8);
    case 36: return CreateGrid(6, 6);
    case 64: return CreateGrid(8, 8);

    case 6:
    {
        CameraLayout l;
        l.name = L"Standard 6";
        l.rows = 3;
        l.cols = 3;
        l.cells = {
            {0, 0, 0, 2, 2, -1},
            {1, 2, 0, 1, 1, -1},
            {2, 2, 1, 1, 1, -1},
            {3, 0, 2, 1, 1, -1},
            {4, 1, 2, 1, 1, -1},
            {5, 2, 2, 1, 1, -1}
        };
        return l;
    }

    case 8:
    {
        CameraLayout l;
        l.name = L"Standard 8";
        l.rows = 4;
        l.cols = 4;
        l.cells = {
            {0, 0, 0, 3, 3, -1},
            {1, 3, 0, 1, 1, -1},
            {2, 3, 1, 1, 1, -1},
            {3, 3, 2, 1, 1, -1},
            {4, 0, 3, 1, 1, -1},
            {5, 1, 3, 1, 1, -1},
            {6, 2, 3, 1, 1, -1},
            {7, 3, 3, 1, 1, -1}
        };
        return l;
    }

    case 13:
    {
        CameraLayout l;
        l.name = L"Standard 13";
        l.rows = 4;
        l.cols = 4;
        l.cells = {
            {0, 0, 0, 2, 2, -1},
            {1, 2, 0, 1, 1, -1},
            {2, 3, 0, 1, 1, -1},
            {3, 2, 1, 1, 1, -1},
            {4, 3, 1, 1, 1, -1},
            {5, 0, 2, 1, 1, -1},
            {6, 1, 2, 1, 1, -1},
            {7, 2, 2, 1, 1, -1},
            {8, 3, 2, 1, 1, -1},
            {9, 0, 3, 1, 1, -1},
            {10,1, 3, 1, 1, -1},
            {11,2, 3, 1, 1, -1},
            {12,3, 3, 1, 1, -1}
        };
        return l;
    }

    default:
        return CreateGrid(2, 2);
    }
}

CameraLayout CameraLayoutFactory::CreateWide(int count)
{
    switch (count)
    {
    case 4:  return CreateGrid(1, 4);
    case 6:  return CreateGrid(2, 3);
    case 9:  return CreateGrid(3, 3);
    case 12: return CreateGrid(3, 4);
    case 16: return CreateGrid(4, 4);
    case 24: return CreateGrid(4, 6);
    case 36: return CreateGrid(6, 6);
    case 48: return CreateGrid(6, 8);

    case 7:
    {
        CameraLayout l;
        l.name = L"Wide 7";
        l.rows = 3;
        l.cols = 4;
        l.cells = {
            {0, 0, 0, 2, 2, -1},
            {1, 2, 0, 1, 1, -1},
            {2, 3, 0, 1, 1, -1},
            {3, 2, 1, 1, 1, -1},
            {4, 3, 1, 1, 1, -1},
            {5, 0, 2, 2, 1, -1},
            {6, 2, 2, 2, 1, -1}
        };
        return l;
    }

    default:
        return CreateGrid(2, 2);
    }
}

bool CameraLayoutFactory::Validate(const CameraLayout& layout)
{
    if (layout.rows <= 0 || layout.cols <= 0 || layout.cells.empty())
        return false;

    std::vector<int> used(layout.rows * layout.cols, 0);

    for (const auto& c : layout.cells)
    {
        if (c.x < 0 || c.y < 0 || c.w <= 0 || c.h <= 0)
            return false;

        if (c.x + c.w > layout.cols || c.y + c.h > layout.rows)
            return false;

        for (int yy = c.y; yy < c.y + c.h; ++yy)
        {
            for (int xx = c.x; xx < c.x + c.w; ++xx)
            {
                int idx = yy * layout.cols + xx;
                if (idx < 0 || idx >= static_cast<int>(used.size()))
                    return false;

                if (used[idx] != 0)
                    return false;

                used[idx] = 1;
            }
        }
    }

    for (int v : used)
    {
        if (v == 0)
            return false;
    }

    return true;
}
