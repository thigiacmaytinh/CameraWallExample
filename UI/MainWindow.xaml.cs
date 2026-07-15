using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;

namespace WpfCameraWall
{
    public partial class MainWindow : Window
    {
        private CameraWallController? _controller;
        private int _nextCameraId;
        private int _selectedCellId = -1;
        private int _popupCellId = -1;
        private Point _cameraDragStart;
        private CameraInfo? _dragCamera;

        private readonly DispatcherTimer _hoverCloseTimer;
        private readonly ObservableCollection<LayoutInfo> _standardLayouts = new ObservableCollection<LayoutInfo>();
        private readonly ObservableCollection<LayoutInfo> _wideLayouts = new ObservableCollection<LayoutInfo>();
        private readonly ObservableCollection<LayoutInfo> _customLayouts = new ObservableCollection<LayoutInfo>();

        public MainWindow()
        {
            InitializeComponent();

            StandardLayoutItems.ItemsSource = _standardLayouts;
            WideLayoutItems.ItemsSource = _wideLayouts;
            CustomLayoutItems.ItemsSource = _customLayouts;

            _hoverCloseTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(250)
            };
            _hoverCloseTimer.Tick += HoverCloseTimer_Tick;

            Loaded += MainWindow_Loaded;
            Closing += MainWindow_Closing;
            Deactivated += MainWindow_Deactivated;
            PreviewMouseDown += MainWindow_PreviewMouseDown;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            _controller = new CameraWallController(CameraWall);
            CameraList.ItemsSource = _controller.Cameras;

            CameraWall.CellClicked += CameraWall_CellClicked;
            CameraWall.CellDoubleClicked += CameraWall_CellDoubleClicked;
            CameraWall.CellRightClicked += CameraWall_CellRightClicked;
            CameraWall.CellHoverChanged += CameraWall_CellHoverChanged;
            CameraWall.CameraDropped += CameraWall_CameraDropped;

            LoadLayouts();

            LayoutInfo? defaultLayout =
                _standardLayouts.FirstOrDefault(x => x.Name == "2x2-4")
                ?? _standardLayouts.FirstOrDefault();

            if (defaultLayout != null)
                ApplyLayout(defaultLayout);

            // Replace with camera data loaded from your database/configuration.
            //AddCameraToList("Gate 1", "rtsp://user:password@192.168.1.100:554/Streaming/Channels/101");
            //AddCameraToList("Gate 2", "rtsp://user:password@192.168.1.101:554/Streaming/Channels/101");
            //AddCameraToList("Yard 1", "rtsp://user:password@192.168.1.102:554/Streaming/Channels/101");
            //AddCameraToList("Yard 2", "rtsp://user:password@192.168.1.103:554/Streaming/Channels/101");

            if (CameraList.Items.Count > 0)
                CameraList.SelectedIndex = 0;
        }

        private void MainWindow_Deactivated(object? sender, EventArgs e)
        {
            LayoutPopup.IsOpen = false;
            HideCellHoverPopup();
        }

        private void MainWindow_Closing(object? sender, CancelEventArgs e)
        {
            _hoverCloseTimer.Stop();
            CellHoverPopup.IsOpen = false;
            _controller?.ClearAllDisplayed();
            CameraWall.Stop();
            CameraWall.Dispose();
        }

        private void LoadLayouts()
        {
            _standardLayouts.Clear();
            _wideLayouts.Clear();
            _customLayouts.Clear();

            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string standardPath = Path.Combine(baseDir, "Layouts", "WidgetLayout.xml");
            string customPath = Path.Combine(baseDir, "Layouts", "CustomWidgetLayout.xml");

            foreach (LayoutInfo layout in LayoutLoader.Load(standardPath)
                         .Where(x => !x.IsWidescreen)
                         .OrderBy(x => x.WndSize)
                         .ThenBy(x => x.Type))
            {
                _standardLayouts.Add(layout);
            }

            foreach (LayoutInfo layout in LayoutLoader.Load(standardPath)
                         .Where(x => x.IsWidescreen)
                         .OrderBy(x => x.WndSize)
                         .ThenBy(x => x.Type))
            {
                _wideLayouts.Add(layout);
            }

            foreach (LayoutInfo layout in LayoutLoader.Load(customPath)
                         .OrderBy(x => x.WndSize)
                         .ThenBy(x => x.Name))
            {
                _customLayouts.Add(layout);
            }
        }

        private void MainWindow_PreviewMouseDown(object sender, MouseButtonEventArgs e)
        {
            if (!LayoutPopup.IsOpen || e.OriginalSource is not DependencyObject source)
                return;

            if (IsDescendantOf(source, BtnLayout) ||
                (LayoutPopup.Child is DependencyObject popupChild && IsDescendantOf(source, popupChild)))
            {
                return;
            }

            LayoutPopup.IsOpen = false;
        }

        private static bool IsDescendantOf(DependencyObject source, DependencyObject ancestor)
        {
            DependencyObject? current = source;
            while (current != null)
            {
                if (ReferenceEquals(current, ancestor))
                    return true;

                current = current is Visual || current is System.Windows.Media.Media3D.Visual3D
                    ? VisualTreeHelper.GetParent(current)
                    : LogicalTreeHelper.GetParent(current);
            }

            return false;
        }

        private void LayoutButton_Click(object sender, RoutedEventArgs e)
        {
            LayoutPopup.IsOpen = !LayoutPopup.IsOpen;
        }

        private void LayoutItem_Click(object sender, RoutedEventArgs e)
        {
            if (sender is FrameworkElement element &&
                element.DataContext is LayoutInfo layout)
            {
                ApplyLayout(layout);
                LayoutPopup.IsOpen = false;
            }
        }

        private void ApplyLayout(LayoutInfo layout)
        {
            if (layout?.Layout == null)
                return;

            _controller?.SetCustomLayout(layout.Layout);
            CurrentLayoutText.Text = layout.Name;

            LayoutCell? firstCell = layout.Layout.Cells
                .OrderBy(c => c.CellId)
                .FirstOrDefault();

            SelectCell(firstCell?.CellId ?? -1);
            HideCellHoverPopup();
            StatusText.Text = $"Layout: {layout.Name}";
        }

        private void AddCamera_Click(object sender, RoutedEventArgs e)
        {
            string name = string.IsNullOrWhiteSpace(TxtCameraName.Text)
                ? $"Camera {_nextCameraId}"
                : TxtCameraName.Text.Trim();

            string url = TxtCameraUrl.Text?.Trim() ?? string.Empty;
            if (string.IsNullOrWhiteSpace(url))
            {
                MessageBox.Show("RTSP URL is empty.", "Camera", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            AddCameraToList(name, url);
        }

        private void RemoveCamera_Click(object sender, RoutedEventArgs e)
        {
            if (CameraList.SelectedItem is CameraInfo camera)
            {
                _controller?.RemoveCamera(camera.Id);
                StatusText.Text = $"Removed {camera.Name}";
                RefreshHoverPopup();
            }
        }

        private void AddCameraToList(string name, string url)
        {
            var camera = new CameraInfo
            {
                Id = _nextCameraId++,
                Name = name,
                MainUrl = url
            };

            _controller?.AddCamera(camera);
        }

        private void CameraWall_CellClicked(int cellId)
        {
            LayoutPopup.IsOpen = false;
            SelectCell(cellId);
        }

        private void CameraWall_CellDoubleClicked(int cellId)
        {
            LayoutPopup.IsOpen = false;
            SelectCell(cellId);
            PlaySelectedCamera();
        }

        private void CameraWall_CellRightClicked(int cellId)
        {
            LayoutPopup.IsOpen = false;
            SelectCell(cellId);
            _controller?.ClearCell(cellId);
            StatusText.Text = $"Stopped cell {cellId + 1}";
            RefreshHoverPopup();
        }

        private void CameraWall_CellHoverChanged(int cellId)
        {
            if (cellId < 0)
            {
                ScheduleCellHoverPopupClose();
                return;
            }

            ShowCellHoverPopup(cellId);
        }

        private void CameraWall_CameraDropped(int cameraId, int cellId)
        {
            LayoutPopup.IsOpen = false;
            SelectCell(cellId);

            CameraInfo? camera = _controller?.GetCamera(cameraId);
            if (camera == null)
                return;

            if (_controller?.ShowCameraInCell(cameraId, cellId) == true)
                StatusText.Text = $"Playing {camera.Name} in cell {cellId + 1}";
            else
                StatusText.Text = $"Could not start {camera.Name}";

            RefreshHoverPopup();
        }

        private void SelectCell(int cellId)
        {
            if (cellId < 0)
            {
                _selectedCellId = -1;
                CameraWall.SetSelectedCell(-1);
                SelectedCellText.Text = "Cell: -";
                return;
            }

            _selectedCellId = cellId;
            CameraWall.SetSelectedCell(cellId);
            SelectedCellText.Text = $"Cell: {cellId + 1}";
        }

        private void CameraList_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            _cameraDragStart = e.GetPosition(this);
            _dragCamera = GetCameraFromSource(e.OriginalSource as DependencyObject)
                          ?? CameraList.SelectedItem as CameraInfo;
        }

        private void CameraList_PreviewMouseMove(object sender, MouseEventArgs e)
        {
            if (e.LeftButton != MouseButtonState.Pressed || _dragCamera == null)
                return;

            Point current = e.GetPosition(this);
            if (Math.Abs(current.X - _cameraDragStart.X) < SystemParameters.MinimumHorizontalDragDistance &&
                Math.Abs(current.Y - _cameraDragStart.Y) < SystemParameters.MinimumVerticalDragDistance)
            {
                return;
            }

            CameraInfo camera = _dragCamera;
            _dragCamera = null;

            var data = new DataObject();
            data.SetText($"CAMERA:{camera.Id}", TextDataFormat.UnicodeText);
            DragDrop.DoDragDrop(CameraList, data, DragDropEffects.Copy);
        }

        private void CameraList_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (GetCameraFromSource(e.OriginalSource as DependencyObject) is not CameraInfo camera)
                return;

            CameraList.SelectedItem = camera;
            PlaySelectedCamera();
        }

        private CameraInfo? GetCameraFromSource(DependencyObject? source)
        {
            if (source == null)
                return null;

            ListBoxItem? item = ItemsControl.ContainerFromElement(CameraList, source) as ListBoxItem;
            return item?.DataContext as CameraInfo;
        }

        private void PlaySelected_Click(object sender, RoutedEventArgs e)
        {
            PlaySelectedCamera();
        }

        private void PlaySelectedCamera()
        {
            if (_selectedCellId < 0)
            {
                StatusText.Text = "Select a cell first";
                return;
            }

            if (CameraList.SelectedItem is not CameraInfo camera)
            {
                StatusText.Text = "Select a camera first";
                return;
            }

            if (_controller?.ShowCameraInCell(camera.Id, _selectedCellId) == true)
                StatusText.Text = $"Playing {camera.Name} in cell {_selectedCellId + 1}";
            else
                StatusText.Text = $"Could not start {camera.Name}";

            RefreshHoverPopup();
        }

        private void PauseSelected_Click(object sender, RoutedEventArgs e)
        {
            PauseCell(_selectedCellId);
        }

        private void StopSelected_Click(object sender, RoutedEventArgs e)
        {
            StopCell(_selectedCellId);
        }

        private void CaptureSelected_Click(object sender, RoutedEventArgs e)
        {
            CaptureCell(_selectedCellId);
        }

        private void StopAll_Click(object sender, RoutedEventArgs e)
        {
            // Keep the D3D renderer running so layouts remain visible and cameras can be played again.
            _controller?.ClearAllDisplayed();
            StatusText.Text = "All cameras stopped";
            RefreshHoverPopup();
        }

        private void HoverPlay_Click(object sender, RoutedEventArgs e)
        {
            if (_popupCellId < 0)
                return;

            SelectCell(_popupCellId);
            CameraInfo? cameraInCell = _controller?.GetCameraInCell(_popupCellId);
            if (cameraInCell != null)
            {
                _controller?.ResumeCell(_popupCellId);
                StatusText.Text = $"Resumed {cameraInCell.Name}";
            }
            else
            {
                PlaySelectedCamera();
            }

            RefreshHoverPopup();
        }

        private void HoverPause_Click(object sender, RoutedEventArgs e)
        {
            PauseCell(_popupCellId);
        }

        private void HoverCapture_Click(object sender, RoutedEventArgs e)
        {
            CaptureCell(_popupCellId);
        }

        private void HoverStop_Click(object sender, RoutedEventArgs e)
        {
            StopCell(_popupCellId);
        }

        private void PauseCell(int cellId)
        {
            if (cellId < 0)
                return;

            CameraInfo? camera = _controller?.GetCameraInCell(cellId);
            if (camera == null)
            {
                StatusText.Text = $"Cell {cellId + 1} is empty";
                return;
            }

            _controller?.PauseCell(cellId);
            StatusText.Text = $"Paused {camera.Name}";
            RefreshHoverPopup();
        }

        private void StopCell(int cellId)
        {
            if (cellId < 0)
                return;

            CameraInfo? camera = _controller?.GetCameraInCell(cellId);
            _controller?.ClearCell(cellId);
            StatusText.Text = camera == null
                ? $"Cell {cellId + 1} is empty"
                : $"Stopped {camera.Name}";
            RefreshHoverPopup();
        }

        private void CaptureCell(int cellId)
        {
            if (cellId < 0)
                return;

            CameraInfo? camera = _controller?.GetCameraInCell(cellId);
            if (camera == null)
            {
                StatusText.Text = $"Cell {cellId + 1} is empty";
                return;
            }

            string pictures = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);
            string outputDirectory = Path.Combine(pictures, "IPCameraPlayer Captures");
            Directory.CreateDirectory(outputDirectory);

            string safeName = MakeSafeFileName(camera.Name);
            string fileName = $"{safeName}_{DateTime.Now:yyyyMMdd_HHmmss_fff}.png";
            string filePath = Path.Combine(outputDirectory, fileName);

            if (_controller?.CaptureCell(cellId, filePath) == true)
                StatusText.Text = $"Capture queued: {fileName}";
            else
                StatusText.Text = "Capture failed: camera is not running";
        }

        private static string GetCameraEndpoint(string url)
        {
            return Uri.TryCreate(url, UriKind.Absolute, out Uri? uri)
                ? uri.Host
                : string.Empty;
        }

        private static string MakeSafeFileName(string value)
        {
            char[] invalid = Path.GetInvalidFileNameChars();
            string result = new string(value.Select(ch => invalid.Contains(ch) ? '_' : ch).ToArray());
            return string.IsNullOrWhiteSpace(result) ? "camera" : result;
        }

        private void ShowCellHoverPopup(int cellId)
        {
            if (!TryGetCellBoundsInDips(cellId, out Rect bounds))
                return;

            _hoverCloseTimer.Stop();
            _popupCellId = cellId;

            CameraInfo? camera = _controller?.GetCameraInCell(cellId);
            HoverCameraName.Text = camera?.Name ?? "No camera";
            HoverCameraName.ToolTip = camera?.MainUrl;

            bool hasCamera = camera != null;
            string state = !hasCamera
                ? "Empty"
                : (_controller?.IsCellPaused(cellId) == true ? "Paused" : "Live");
            string endpoint = camera == null ? string.Empty : GetCameraEndpoint(camera.MainUrl);
            HoverCellText.Text = string.IsNullOrWhiteSpace(endpoint)
                ? $"Cell {cellId + 1} · {state}"
                : $"Cell {cellId + 1} · {state} · {endpoint}";
            HoverPlayButton.IsEnabled = hasCamera || CameraList.SelectedItem is CameraInfo;
            HoverPauseButton.IsEnabled = hasCamera;
            HoverCaptureButton.IsEnabled = hasCamera;
            HoverStopButton.IsEnabled = hasCamera;

            double toolbarWidth = Math.Clamp(bounds.Width - 8.0, 190.0, 430.0);
            double x = Math.Clamp(bounds.X + 4.0, 0.0, Math.Max(0.0, CameraWall.ActualWidth - toolbarWidth));
            double y = Math.Max(bounds.Y + 4.0, bounds.Bottom - 38.0);

            HoverToolbarBorder.Width = toolbarWidth;
            CellHoverPopup.HorizontalOffset = x;
            CellHoverPopup.VerticalOffset = y;
            CellHoverPopup.IsOpen = true;
        }

        private bool TryGetCellBoundsInDips(int cellId, out Rect bounds)
        {
            bounds = Rect.Empty;
            if (!CameraWall.TryGetCellBounds(cellId, out Rect pixelBounds))
                return false;

            PresentationSource? source = PresentationSource.FromVisual(CameraWall);
            if (source?.CompositionTarget == null)
            {
                bounds = pixelBounds;
                return true;
            }

            Matrix fromDevice = source.CompositionTarget.TransformFromDevice;
            Point topLeft = fromDevice.Transform(pixelBounds.TopLeft);
            Point bottomRight = fromDevice.Transform(pixelBounds.BottomRight);
            bounds = new Rect(topLeft, bottomRight);
            return true;
        }

        private void RefreshHoverPopup()
        {
            if (CellHoverPopup.IsOpen && _popupCellId >= 0)
                ShowCellHoverPopup(_popupCellId);
        }

        private void ScheduleCellHoverPopupClose()
        {
            _hoverCloseTimer.Stop();
            _hoverCloseTimer.Start();
        }

        private void HoverCloseTimer_Tick(object? sender, EventArgs e)
        {
            _hoverCloseTimer.Stop();
            HideCellHoverPopup();
        }

        private void HideCellHoverPopup()
        {
            _hoverCloseTimer.Stop();
            CellHoverPopup.IsOpen = false;
            _popupCellId = -1;
            CameraWall.SetHoveredCell(-1);
        }

        private void HoverToolbar_MouseEnter(object sender, MouseEventArgs e)
        {
            _hoverCloseTimer.Stop();
            if (_popupCellId >= 0)
                CameraWall.SetHoveredCell(_popupCellId);
        }

        private void HoverToolbar_MouseLeave(object sender, MouseEventArgs e)
        {
            CameraWall.SetHoveredCell(-1);
            ScheduleCellHoverPopupClose();
        }

        private void AddCustom_Click(object sender, RoutedEventArgs e)
        {
            LayoutPopup.IsOpen = false;

            var dialog = new AddCustomLayoutWindow { Owner = this };

            if (dialog.ShowDialog() == true && dialog.ResultLayout != null)
            {
                CameraLayout layout = dialog.ResultLayout;

                string customPath = Path.Combine(
                    AppDomain.CurrentDomain.BaseDirectory,
                    "Layouts",
                    "CustomWidgetLayout.xml");

                CustomLayoutXmlWriter.SaveOrReplace(customPath, layout);
                LoadLayouts();

                LayoutInfo? savedLayout = _customLayouts.FirstOrDefault(x => x.Name == layout.Name);
                if (savedLayout != null)
                    ApplyLayout(savedLayout);
            }
        }
    }
}
