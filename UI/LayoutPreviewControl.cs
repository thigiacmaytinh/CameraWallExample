using System;
using System.Linq;
using System.Windows;
using System.Windows.Media;

namespace WpfCameraWall
{
    public sealed class LayoutPreviewControl : FrameworkElement
    {
        public static readonly DependencyProperty LayoutProperty =
            DependencyProperty.Register(
                nameof(Layout),
                typeof(CameraLayout),
                typeof(LayoutPreviewControl),
                new FrameworkPropertyMetadata(
                    null,
                    FrameworkPropertyMetadataOptions.AffectsRender));

        public CameraLayout Layout
        {
            get => (CameraLayout)GetValue(LayoutProperty);
            set => SetValue(LayoutProperty, value);
        }

        protected override void OnRender(DrawingContext dc)
        {
            base.OnRender(dc);

            double width = ActualWidth;
            double height = ActualHeight;

            // Very important: Button/template may measure preview as 0 initially.
            if (width < 2 || height < 2)
                return;

            var bgBrush = new SolidColorBrush(Color.FromRgb(92, 95, 98));
            var cellBrush = new SolidColorBrush(Color.FromRgb(105, 108, 111));
            var linePen = new Pen(new SolidColorBrush(Color.FromRgb(28, 30, 32)), 1.0);
            var borderPen = new Pen(new SolidColorBrush(Color.FromRgb(180, 180, 180)), 1.0);

            double safeWidth = Math.Max(0, width - 1);
            double safeHeight = Math.Max(0, height - 1);

            if (safeWidth <= 0 || safeHeight <= 0)
                return;

            dc.DrawRectangle(
                bgBrush,
                borderPen,
                new Rect(0.5, 0.5, safeWidth, safeHeight));

            if (Layout == null ||
                Layout.Rows <= 0 ||
                Layout.Cols <= 0 ||
                Layout.Cells == null ||
                Layout.Cells.Count == 0)
            {
                return;
            }

            double unitW = safeWidth / Layout.Cols;
            double unitH = safeHeight / Layout.Rows;

            foreach (LayoutCell cell in Layout.Cells.OrderBy(c => c.CellId))
            {
                double x = 0.5 + cell.X * unitW;
                double y = 0.5 + cell.Y * unitH;
                double w = cell.W * unitW;
                double h = cell.H * unitH;

                if (w <= 0 || h <= 0)
                    continue;

                Rect rect = new Rect(
                    x,
                    y,
                    Math.Max(1, w - 1),
                    Math.Max(1, h - 1));

                dc.DrawRectangle(cellBrush, linePen, rect);
            }
        }
    }
}
