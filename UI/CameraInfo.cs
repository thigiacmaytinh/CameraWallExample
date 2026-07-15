namespace WpfCameraWall
{
    public sealed class CameraInfo
    {
        public int Id { get; set; }
        public string Name { get; set; } = "";
        public string MainUrl { get; set; } = "";
        public string SubUrl { get; set; } = "";
        public bool IsOnline { get; set; } = true;
    }
}
