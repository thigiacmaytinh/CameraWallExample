using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace WpfCameraWall
{
    public partial class AddCustomLayoutWindow : Window
    {
        private CameraLayout _layout;
        private readonly HashSet<int> _selectedCellIds = new HashSet<int>();

        public CameraLayout ResultLayout { get; private set; }

        public AddCustomLayoutWindow()
        {
            InitializeComponent();
            Loaded += (_, __) => BuildLayout(3, 3);
            LayoutCanvas.SizeChanged += (_, __) => RenderLayout();
        }

        private void BtnBuild_Click(object sender, RoutedEventArgs e)
        {
            int rows = ParsePositive(TxtRows.Text, 3);
            int cols = ParsePositive(TxtCols.Text, 3);
            BuildLayout(rows, cols);
        }

        private void BuildLayout(int rows, int cols)
        {
            _selectedCellIds.Clear();
            _layout = new CameraLayout
            {
                Name = string.IsNullOrWhiteSpace(TxtName.Text) ? $"Custom {rows}x{cols}" : TxtName.Text,
                Rows = rows,
                Cols = cols
            };

            int id = 0;
            for (int y = 0; y < rows; y++)
            {
                for (int x = 0; x < cols; x++)
                {
                    _layout.Cells.Add(new LayoutCell { CellId = id++, X = x, Y = y, W = 1, H = 1, CameraId = -1 });
                }
            }

            RenderLayout();
        }

        private void RenderLayout()
        {
            LayoutCanvas.Children.Clear();

            if (_layout == null || _layout.Rows <= 0 || _layout.Cols <= 0)
                return;

            double w = Math.Max(1, LayoutCanvas.ActualWidth);
            double h = Math.Max(1, LayoutCanvas.ActualHeight);
            double unitW = w / _layout.Cols;
            double unitH = h / _layout.Rows;

            foreach (LayoutCell cell in _layout.Cells.OrderBy(c => c.CellId))
            {
                Button b = new Button
                {
                    Content = cell.CellId.ToString(),
                    Tag = cell.CellId,
                    Background = _selectedCellIds.Contains(cell.CellId)
                        ? new SolidColorBrush(Color.FromRgb(90, 120, 170))
                        : new SolidColorBrush(Color.FromRgb(45, 48, 54)),
                    Foreground = Brushes.White,
                    BorderBrush = new SolidColorBrush(Color.FromRgb(25, 26, 28)),
                    BorderThickness = new Thickness(1)
                };

                b.Click += CellButton_Click;

                Canvas.SetLeft(b, cell.X * unitW + 1);
                Canvas.SetTop(b, cell.Y * unitH + 1);
                b.Width = Math.Max(1, cell.W * unitW - 2);
                b.Height = Math.Max(1, cell.H * unitH - 2);

                LayoutCanvas.Children.Add(b);
            }
        }

        private void CellButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button b && b.Tag is int id)
            {
                if (_selectedCellIds.Contains(id))
                    _selectedCellIds.Remove(id);
                else
                    _selectedCellIds.Add(id);

                RenderLayout();
            }
        }

        private void BtnJoint_Click(object sender, RoutedEventArgs e)
        {
            if (_layout == null || _selectedCellIds.Count < 2)
                return;

            List<LayoutCell> selected = _layout.Cells.Where(c => _selectedCellIds.Contains(c.CellId)).ToList();
            int minX = selected.Min(c => c.X);
            int minY = selected.Min(c => c.Y);
            int maxX = selected.Max(c => c.X + c.W);
            int maxY = selected.Max(c => c.Y + c.H);

            int expectedArea = (maxX - minX) * (maxY - minY);
            int selectedArea = selected.Sum(c => c.W * c.H);

            if (expectedArea != selectedArea)
            {
                MessageBox.Show("Selected cells must form one rectangle.");
                return;
            }

            int newCellId = selected.Min(c => c.CellId);
            _layout.Cells.RemoveAll(c => _selectedCellIds.Contains(c.CellId));
            _layout.Cells.Add(new LayoutCell
            {
                CellId = newCellId,
                X = minX,
                Y = minY,
                W = maxX - minX,
                H = maxY - minY,
                CameraId = -1
            });

            ReassignCellIds();
            _selectedCellIds.Clear();
            RenderLayout();
        }

        private void BtnRestore_Click(object sender, RoutedEventArgs e)
        {
            if (_layout == null || _selectedCellIds.Count != 1)
                return;

            int id = _selectedCellIds.First();
            LayoutCell target = _layout.Cells.FirstOrDefault(c => c.CellId == id);
            if (target == null)
                return;

            _layout.Cells.Remove(target);

            for (int y = target.Y; y < target.Y + target.H; y++)
            {
                for (int x = target.X; x < target.X + target.W; x++)
                {
                    _layout.Cells.Add(new LayoutCell { X = x, Y = y, W = 1, H = 1, CameraId = -1 });
                }
            }

            ReassignCellIds();
            _selectedCellIds.Clear();
            RenderLayout();
        }

        private void ReassignCellIds()
        {
            int id = 0;
            foreach (LayoutCell c in _layout.Cells.OrderBy(c => c.Y).ThenBy(c => c.X).ToList())
            {
                c.CellId = id++;
            }
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (_layout == null)
                return;

            _layout.Name = string.IsNullOrWhiteSpace(TxtName.Text) ? _layout.Name : TxtName.Text;
            ResultLayout = _layout;
            DialogResult = true;
            Close();
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }

        private static int ParsePositive(string text, int fallback)
        {
            return int.TryParse(text, out int value) && value > 0 ? value : fallback;
        }
    }
}
