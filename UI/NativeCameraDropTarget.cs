using System;
using System.Runtime.InteropServices;
using ComTypes = System.Runtime.InteropServices.ComTypes;

namespace WpfCameraWall
{
    [ComVisible(true)]
    [Guid("00000122-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IOleDropTarget
    {
        [PreserveSig]
        int DragEnter(
            [In, MarshalAs(UnmanagedType.Interface)] ComTypes.IDataObject dataObject,
            uint keyState,
            NativePoint point,
            ref uint effect);

        [PreserveSig]
        int DragOver(uint keyState, NativePoint point, ref uint effect);

        [PreserveSig]
        int DragLeave();

        [PreserveSig]
        int Drop(
            [In, MarshalAs(UnmanagedType.Interface)] ComTypes.IDataObject dataObject,
            uint keyState,
            NativePoint point,
            ref uint effect);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NativePoint
    {
        public int X;
        public int Y;
    }

    [ComVisible(true)]
    [ClassInterface(ClassInterfaceType.None)]
    public sealed class NativeCameraDropTarget : IOleDropTarget
    {
        private const int S_OK = 0;
        private const short CfUnicodeText = 13;
        private const uint DropEffectNone = 0;
        private const uint DropEffectCopy = 1;

        private readonly Func<int, int, bool> _dragOver;
        private readonly Func<int, int, int, bool> _drop;
        private readonly Action _dragLeave;
        private int _cameraId = -1;

        public NativeCameraDropTarget(
            Func<int, int, bool> dragOver,
            Func<int, int, int, bool> drop,
            Action dragLeave)
        {
            _dragOver = dragOver ?? throw new ArgumentNullException(nameof(dragOver));
            _drop = drop ?? throw new ArgumentNullException(nameof(drop));
            _dragLeave = dragLeave ?? throw new ArgumentNullException(nameof(dragLeave));
        }

        public int DragEnter(
            ComTypes.IDataObject dataObject,
            uint keyState,
            NativePoint point,
            ref uint effect)
        {
            try
            {
                _cameraId = TryGetCameraId(dataObject, out int cameraId) ? cameraId : -1;
                effect = _cameraId >= 0 && _dragOver(point.X, point.Y)
                    ? DropEffectCopy
                    : DropEffectNone;
            }
            catch
            {
                _cameraId = -1;
                effect = DropEffectNone;
            }

            return S_OK;
        }

        public int DragOver(uint keyState, NativePoint point, ref uint effect)
        {
            try
            {
                effect = _cameraId >= 0 && _dragOver(point.X, point.Y)
                    ? DropEffectCopy
                    : DropEffectNone;
            }
            catch
            {
                effect = DropEffectNone;
            }

            return S_OK;
        }

        public int DragLeave()
        {
            _cameraId = -1;
            try
            {
                _dragLeave();
            }
            catch
            {
                // Never allow a managed exception to escape through the COM callback.
            }

            return S_OK;
        }

        public int Drop(
            ComTypes.IDataObject dataObject,
            uint keyState,
            NativePoint point,
            ref uint effect)
        {
            try
            {
                int cameraId = _cameraId;
                if (cameraId < 0 && !TryGetCameraId(dataObject, out cameraId))
                {
                    effect = DropEffectNone;
                    _dragLeave();
                    return S_OK;
                }

                bool accepted = _drop(cameraId, point.X, point.Y);
                effect = accepted ? DropEffectCopy : DropEffectNone;

                if (accepted)
                    _dragOver(point.X, point.Y);
                else
                    _dragLeave();
            }
            catch
            {
                effect = DropEffectNone;
                try
                {
                    _dragLeave();
                }
                catch
                {
                    // Never allow a managed exception to escape through the COM callback.
                }
            }
            finally
            {
                _cameraId = -1;
            }

            return S_OK;
        }

        private static bool TryGetCameraId(ComTypes.IDataObject dataObject, out int cameraId)
        {
            cameraId = -1;
            if (dataObject == null)
                return false;

            var format = new ComTypes.FORMATETC
            {
                cfFormat = CfUnicodeText,
                dwAspect = ComTypes.DVASPECT.DVASPECT_CONTENT,
                lindex = -1,
                ptd = IntPtr.Zero,
                tymed = ComTypes.TYMED.TYMED_HGLOBAL
            };

            if (dataObject.QueryGetData(ref format) != S_OK)
                return false;

            ComTypes.STGMEDIUM medium = default;
            bool hasMedium = false;

            try
            {
                dataObject.GetData(ref format, out medium);
                hasMedium = true;

                IntPtr textPointer = GlobalLock(medium.unionmember);
                if (textPointer == IntPtr.Zero)
                    return false;

                try
                {
                    string? text = Marshal.PtrToStringUni(textPointer);
                    const string prefix = "CAMERA:";
                    return text != null &&
                           text.StartsWith(prefix, StringComparison.Ordinal) &&
                           int.TryParse(text.Substring(prefix.Length), out cameraId);
                }
                finally
                {
                    GlobalUnlock(medium.unionmember);
                }
            }
            catch (COMException)
            {
                return false;
            }
            finally
            {
                if (hasMedium)
                    ReleaseStgMedium(ref medium);
            }
        }

        [DllImport("kernel32.dll")]
        private static extern IntPtr GlobalLock(IntPtr memory);

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GlobalUnlock(IntPtr memory);

        [DllImport("ole32.dll")]
        private static extern void ReleaseStgMedium(ref ComTypes.STGMEDIUM medium);
    }
}
