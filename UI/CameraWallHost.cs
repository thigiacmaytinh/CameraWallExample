using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using CameraWall;

namespace WpfCameraWall
{
    public sealed class CameraWallHost : HwndHost
    {
        private const string WindowClassName = "NativeCameraWallHostWindow";

        private const int WmMouseMove = 0x0200;
        private const int WmLButtonDown = 0x0201;
        private const int WmLButtonDoubleClick = 0x0203;
        private const int WmRButtonDown = 0x0204;
        private const int WmMouseLeave = 0x02A3;

        private const uint TmeLeave = 0x00000002;
        private const uint CsDoubleClicks = 0x0008;

        private const int WsChild = 0x40000000;
        private const int WsVisible = 0x10000000;

        private static readonly NativeWndProc WindowProcedure = DefWindowProc;

        private IntPtr _hwnd;
        private CameraWall.CameraWall? _bridge;
        private bool _classRegistered;
        private bool _trackingMouseLeave;
        private bool _dropTargetRegistered;
        private bool _oleInitialized;
        private int _hoveredCellId = -1;
        private NativeCameraDropTarget? _dropTarget;

        public event Action<int>? CellClicked;
        public event Action<int>? CellDoubleClicked;
        public event Action<int>? CellRightClicked;
        public event Action<int>? CellHoverChanged;
        public event Action<int, int>? CameraDropped;

        public CameraWallHost()
        {
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
            SizeChanged += OnSizeChanged;
        }

        public bool AddCamera(int cameraId, string rtspUrl)
        {
            // Start is idempotent. Calling it here also fixes replay after a previous wall Stop().
            _bridge?.Start();
            return _bridge?.AddCamera(cameraId, rtspUrl) ?? false;
        }

        public void RemoveCamera(int cameraId) => _bridge?.RemoveCamera(cameraId);
        public void PauseCamera(int cameraId) => _bridge?.PauseCamera(cameraId);
        public void ResumeCamera(int cameraId) => _bridge?.ResumeCamera(cameraId);
        public bool CaptureCamera(int cameraId, string filePath) => _bridge?.CaptureCamera(cameraId, filePath) ?? false;

        public void AssignCameraToCell(int cellId, int cameraId) => _bridge?.AssignCameraToCell(cellId, cameraId);
        public void ClearCell(int cellId) => _bridge?.ClearCell(cellId);
        public int GetCellAt(int x, int y) => _bridge?.GetCellAt(x, y) ?? -1;
        public void SetSelectedCell(int cellId) => _bridge?.SetSelectedCell(cellId);

        public void SetHoveredCell(int cellId)
        {
            _hoveredCellId = cellId;
            _bridge?.SetHoveredCell(cellId);
        }

        public bool TryGetCellBounds(int cellId, out Rect bounds)
        {
            bounds = Rect.Empty;
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;

            if (_bridge == null || !_bridge.GetCellBounds(cellId, ref x, ref y, ref width, ref height))
                return false;

            bounds = new Rect(x, y, width, height);
            return true;
        }

        public void SetGridLayout(int rows, int cols) => _bridge?.SetGridLayout(rows, cols);
        public void SetStandardLayout(int count) => _bridge?.SetStandardLayout(count);
        public void SetWideLayout(int count) => _bridge?.SetWideLayout(count);

        public bool SetCustomLayout(CameraLayout layout)
        {
            if (layout == null)
                return false;

            return _bridge?.SetCustomLayout(layout.Name, layout.Rows, layout.Cols, layout.ToBridgeCells()) ?? false;
        }

        public void Start() => _bridge?.Start();
        public void Stop() => _bridge?.Stop();

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            RegisterWindowClass();

            _hwnd = CreateWindowEx(
                0,
                WindowClassName,
                string.Empty,
                WsChild | WsVisible,
                0,
                0,
                Math.Max(1, (int)ActualWidth),
                Math.Max(1, (int)ActualHeight),
                hwndParent.Handle,
                IntPtr.Zero,
                GetModuleHandle(null),
                IntPtr.Zero);

            EnsureOleInitialized();
            RegisterNativeDropTarget();
            return new HandleRef(this, _hwnd);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            UnregisterNativeDropTarget();

            Stop();
            _bridge?.Dispose();
            _bridge = null;

            if (hwnd.Handle != IntPtr.Zero)
                DestroyWindow(hwnd.Handle);

            _hwnd = IntPtr.Zero;
            ReleaseOleInitialization();
        }

        protected override IntPtr WndProc(
            IntPtr hwnd,
            int msg,
            IntPtr wParam,
            IntPtr lParam,
            ref bool handled)
        {
            switch (msg)
            {
                case WmMouseMove:
                {
                    EnsureMouseLeaveTracking();
                    int cellId = GetCellFromLParam(lParam);
                    UpdateHoveredCell(cellId);
                    break;
                }

                case WmMouseLeave:
                    _trackingMouseLeave = false;
                    UpdateHoveredCell(-1);
                    break;

                case WmLButtonDown:
                {
                    SetFocus(hwnd);
                    int cellId = GetCellFromLParam(lParam);
                    if (cellId >= 0)
                    {
                        CellClicked?.Invoke(cellId);
                        handled = true;
                    }
                    break;
                }

                case WmLButtonDoubleClick:
                {
                    int cellId = GetCellFromLParam(lParam);
                    if (cellId >= 0)
                    {
                        CellDoubleClicked?.Invoke(cellId);
                        handled = true;
                    }
                    break;
                }

                case WmRButtonDown:
                {
                    int cellId = GetCellFromLParam(lParam);
                    if (cellId >= 0)
                    {
                        CellRightClicked?.Invoke(cellId);
                        handled = true;
                    }
                    break;
                }
            }

            return base.WndProc(hwnd, msg, wParam, lParam, ref handled);
        }

        private int GetCellFromLParam(IntPtr lParam)
        {
            long value = lParam.ToInt64();
            int x = unchecked((short)(value & 0xFFFF));
            int y = unchecked((short)((value >> 16) & 0xFFFF));
            return GetCellAt(x, y);
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            if (_hwnd == IntPtr.Zero)
                return;

            if (_bridge == null)
            {
                _bridge = new CameraWall.CameraWall();
                if (!_bridge.Initialize(_hwnd))
                    throw new InvalidOperationException("Failed to initialize native camera wall.");
            }

            _bridge.Start();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            Stop();
        }

        private void OnSizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (_hwnd == IntPtr.Zero)
                return;

            int width = Math.Max(1, (int)e.NewSize.Width);
            int height = Math.Max(1, (int)e.NewSize.Height);

            MoveWindow(_hwnd, 0, 0, width, height, true);
            _bridge?.Resize(width, height);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                UnregisterNativeDropTarget();
                ReleaseOleInitialization();
                Stop();
                _bridge?.Dispose();
                _bridge = null;
            }

            base.Dispose(disposing);
        }

        private void UpdateHoveredCell(int cellId)
        {
            if (_hoveredCellId == cellId)
                return;

            _hoveredCellId = cellId;
            _bridge?.SetHoveredCell(cellId);
            CellHoverChanged?.Invoke(cellId);
        }

        private void EnsureMouseLeaveTracking()
        {
            if (_trackingMouseLeave || _hwnd == IntPtr.Zero)
                return;

            var tracking = new TrackMouseEventData
            {
                Size = (uint)Marshal.SizeOf<TrackMouseEventData>(),
                Flags = TmeLeave,
                TrackWindow = _hwnd,
                HoverTime = 0
            };

            _trackingMouseLeave = TrackMouseEvent(ref tracking);
        }

        private void RegisterWindowClass()
        {
            if (_classRegistered)
                return;

            var windowClass = new WindowClass
            {
                style = CsDoubleClicks,
                lpfnWndProc = WindowProcedure,
                hInstance = GetModuleHandle(null),
                lpszClassName = WindowClassName
            };

            RegisterClass(ref windowClass);
            _classRegistered = true;
        }

        private void EnsureOleInitialized()
        {
            if (_oleInitialized)
                return;

            int result = OleInitialize(IntPtr.Zero);
            _oleInitialized = result >= 0;
        }

        private void ReleaseOleInitialization()
        {
            if (!_oleInitialized)
                return;

            OleUninitialize();
            _oleInitialized = false;
        }

        private void RegisterNativeDropTarget()
        {
            if (_hwnd == IntPtr.Zero || _dropTargetRegistered || !_oleInitialized)
                return;

            _dropTarget = new NativeCameraDropTarget(
                HandleNativeDragOver,
                HandleNativeDrop,
                () => UpdateHoveredCell(-1));

            int result = RegisterDragDrop(_hwnd, _dropTarget);
            _dropTargetRegistered = result >= 0;

            if (!_dropTargetRegistered)
                _dropTarget = null;
        }

        private void UnregisterNativeDropTarget()
        {
            if (!_dropTargetRegistered || _hwnd == IntPtr.Zero)
                return;

            RevokeDragDrop(_hwnd);
            _dropTargetRegistered = false;
            _dropTarget = null;
        }

        private bool HandleNativeDragOver(int screenX, int screenY)
        {
            int cellId = GetCellFromScreenPoint(screenX, screenY);
            UpdateHoveredCell(cellId);
            return cellId >= 0;
        }

        private bool HandleNativeDrop(int cameraId, int screenX, int screenY)
        {
            int cellId = GetCellFromScreenPoint(screenX, screenY);
            if (cellId < 0)
                return false;

            CameraDropped?.Invoke(cameraId, cellId);
            return true;
        }

        private int GetCellFromScreenPoint(int screenX, int screenY)
        {
            if (_hwnd == IntPtr.Zero)
                return -1;

            var point = new NativePoint { X = screenX, Y = screenY };
            if (!ScreenToClient(_hwnd, ref point))
                return -1;

            return GetCellAt(point.X, point.Y);
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct WindowClass
        {
            public uint style;
            public NativeWndProc lpfnWndProc;
            public int cbClsExtra;
            public int cbWndExtra;
            public IntPtr hInstance;
            public IntPtr hIcon;
            public IntPtr hCursor;
            public IntPtr hbrBackground;
            public string? lpszMenuName;
            public string lpszClassName;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct TrackMouseEventData
        {
            public uint Size;
            public uint Flags;
            public IntPtr TrackWindow;
            public uint HoverTime;
        }

        private delegate IntPtr NativeWndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandle(string? moduleName);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern ushort RegisterClass(ref WindowClass windowClass);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr CreateWindowEx(
            int extendedStyle,
            string className,
            string windowName,
            int style,
            int x,
            int y,
            int width,
            int height,
            IntPtr parent,
            IntPtr menu,
            IntPtr instance,
            IntPtr parameter);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool DestroyWindow(IntPtr hwnd);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool MoveWindow(IntPtr hwnd, int x, int y, int width, int height, bool repaint);

        [DllImport("user32.dll")]
        private static extern IntPtr DefWindowProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern IntPtr SetFocus(IntPtr hwnd);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool TrackMouseEvent(ref TrackMouseEventData eventData);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool ScreenToClient(IntPtr hwnd, ref NativePoint point);

        [DllImport("ole32.dll")]
        private static extern int OleInitialize(IntPtr reserved);

        [DllImport("ole32.dll")]
        private static extern void OleUninitialize();

        [DllImport("ole32.dll")]
        private static extern int RegisterDragDrop(IntPtr hwnd, [MarshalAs(UnmanagedType.Interface)] IOleDropTarget dropTarget);

        [DllImport("ole32.dll")]
        private static extern int RevokeDragDrop(IntPtr hwnd);
    }
}
