using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace WpfCameraWall
{
    public sealed class CameraWallController
    {
        private readonly CameraWallHost _wall;
        private readonly Dictionary<int, CameraInfo> _cameraById = new Dictionary<int, CameraInfo>();
        private readonly Dictionary<int, int> _cellToCamera = new Dictionary<int, int>();
        private readonly HashSet<int> _activeStreams = new HashSet<int>();
        private readonly HashSet<int> _pausedStreams = new HashSet<int>();

        public ObservableCollection<CameraInfo> Cameras { get; } = new ObservableCollection<CameraInfo>();

        public CameraWallController(CameraWallHost wall)
        {
            _wall = wall;
        }

        public void AddCamera(CameraInfo camera)
        {
            if (camera == null || camera.Id < 0 || string.IsNullOrWhiteSpace(camera.MainUrl))
                return;

            if (_cameraById.ContainsKey(camera.Id))
            {
                _cameraById[camera.Id] = camera;
                CameraInfo? old = Cameras.FirstOrDefault(c => c.Id == camera.Id);
                if (old != null)
                {
                    int index = Cameras.IndexOf(old);
                    Cameras[index] = camera;
                }
                return;
            }

            _cameraById.Add(camera.Id, camera);
            Cameras.Add(camera);
        }

        public void RemoveCamera(int cameraId)
        {
            List<int> cells = _cellToCamera
                .Where(kv => kv.Value == cameraId)
                .Select(kv => kv.Key)
                .ToList();

            foreach (int cellId in cells)
                ClearCell(cellId);

            if (_activeStreams.Contains(cameraId))
            {
                _wall.RemoveCamera(cameraId);
                _activeStreams.Remove(cameraId);
                _pausedStreams.Remove(cameraId);
            }

            _cameraById.Remove(cameraId);

            CameraInfo? item = Cameras.FirstOrDefault(c => c.Id == cameraId);
            if (item != null)
                Cameras.Remove(item);
        }

        public bool ShowCameraInCell(int cameraId, int cellId)
        {
            if (!_cameraById.TryGetValue(cameraId, out CameraInfo? camera))
                return false;

            // One camera is displayed in only one cell, matching common VMS behavior.
            int oldCell = FindCellByCamera(cameraId);
            if (oldCell >= 0 && oldCell != cellId)
            {
                _cellToCamera.Remove(oldCell);
                _wall.ClearCell(oldCell);
            }

            int previousCameraInCell = -1;
            if (_cellToCamera.TryGetValue(cellId, out int existingCamera))
                previousCameraInCell = existingCamera;

            if (!EnsureStreamStarted(camera))
                return false;

            _wall.ResumeCamera(cameraId);
            _pausedStreams.Remove(cameraId);
            _cellToCamera[cellId] = cameraId;
            _wall.AssignCameraToCell(cellId, cameraId);

            if (previousCameraInCell != cameraId)
                StopIfUnused(previousCameraInCell);

            return true;
        }

        public void PauseCell(int cellId)
        {
            if (_cellToCamera.TryGetValue(cellId, out int cameraId))
            {
                _wall.PauseCamera(cameraId);
                _pausedStreams.Add(cameraId);
            }
        }

        public void ResumeCell(int cellId)
        {
            if (_cellToCamera.TryGetValue(cellId, out int cameraId))
            {
                _wall.ResumeCamera(cameraId);
                _pausedStreams.Remove(cameraId);
            }
        }

        public bool CaptureCell(int cellId, string filePath)
        {
            return _cellToCamera.TryGetValue(cellId, out int cameraId) &&
                   _wall.CaptureCamera(cameraId, filePath);
        }

        public void ClearCell(int cellId)
        {
            if (!_cellToCamera.TryGetValue(cellId, out int cameraId))
            {
                _wall.ClearCell(cellId);
                return;
            }

            _cellToCamera.Remove(cellId);
            _wall.ClearCell(cellId);
            StopIfUnused(cameraId);
        }

        public CameraInfo? GetCameraInCell(int cellId)
        {
            if (!_cellToCamera.TryGetValue(cellId, out int cameraId))
                return null;

            return GetCamera(cameraId);
        }

        public CameraInfo? GetCamera(int cameraId)
        {
            _cameraById.TryGetValue(cameraId, out CameraInfo? camera);
            return camera;
        }

        public bool IsCellPaused(int cellId)
        {
            return _cellToCamera.TryGetValue(cellId, out int cameraId) &&
                   _pausedStreams.Contains(cameraId);
        }

        public void SetStandardLayout(int count)
        {
            ClearAllDisplayed();
            _wall.SetStandardLayout(count);
        }

        public void SetWideLayout(int count)
        {
            ClearAllDisplayed();
            _wall.SetWideLayout(count);
        }

        public void SetCustomLayout(CameraLayout layout)
        {
            ClearAllDisplayed();
            _wall.SetCustomLayout(layout);
        }

        public void ClearAllDisplayed()
        {
            foreach (int cellId in _cellToCamera.Keys.ToList())
                _wall.ClearCell(cellId);

            foreach (int cameraId in _activeStreams.ToList())
                _wall.RemoveCamera(cameraId);

            _activeStreams.Clear();
            _pausedStreams.Clear();
            _cellToCamera.Clear();
        }

        private bool EnsureStreamStarted(CameraInfo camera)
        {
            if (_activeStreams.Contains(camera.Id))
                return true;

            if (!_wall.AddCamera(camera.Id, camera.MainUrl))
                return false;

            _activeStreams.Add(camera.Id);
            return true;
        }

        private void StopIfUnused(int cameraId)
        {
            if (cameraId < 0)
                return;

            if (_cellToCamera.Values.Any(id => id == cameraId))
                return;

            _wall.RemoveCamera(cameraId);
            _activeStreams.Remove(cameraId);
            _pausedStreams.Remove(cameraId);
        }

        private int FindCellByCamera(int cameraId)
        {
            foreach (KeyValuePair<int, int> kv in _cellToCamera)
            {
                if (kv.Value == cameraId)
                    return kv.Key;
            }

            return -1;
        }
    }
}
