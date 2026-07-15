using System.Collections.Generic;
using System.Linq;
using CameraWall;

namespace WpfCameraWall
{
    public sealed class LayoutInfo
    {
        public string Name { get; set; } = "";
        public int Type { get; set; }
        public int WndSize { get; set; }
        public bool IsWidescreen { get; set; }
        public CameraLayout Layout { get; set; } = new CameraLayout();

        public override string ToString()
        {
            return Name;
        }
    }

    public sealed class LayoutCell
    {
        public int CellId { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public int W { get; set; } = 1;
        public int H { get; set; } = 1;
        public int CameraId { get; set; } = -1;
    }

    public sealed class CameraLayout
    {
        public string Name { get; set; } = "Custom";
        public int Rows { get; set; }
        public int Cols { get; set; }
        public List<LayoutCell> Cells { get; set; } = new List<LayoutCell>();

        public CellDto[] ToBridgeCells()
        {
            return Cells.Select(c => new CellDto
            {
                CellId = c.CellId,
                X = c.X,
                Y = c.Y,
                W = c.W,
                H = c.H,
                CameraId = c.CameraId
            }).ToArray();
        }
    }
}
