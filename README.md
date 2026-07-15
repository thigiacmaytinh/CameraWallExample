# CameraWall Example

<div align="center">

![.NET](https://img.shields.io/badge/.NET-8.0-512BD4?style=for-the-badge&logo=dotnet)
![C++](https://img.shields.io/badge/C++-14-00599C?style=for-the-badge&logo=cplusplus)
![FFmpeg](https://img.shields.io/badge/FFmpeg-8.0.1-007808?style=for-the-badge&logo=ffmpeg)
![WPF](https://img.shields.io/badge/WPF-Windows-0078D4?style=for-the-badge&logo=windows)

**Ứng dụng hiển thị và quản lý nhiều camera IP đồng thời trên một màn hình**


![](camerawall.jpg)

</div>

---

## 1. Tổng quan

**CameraWallExample** là một giải pháp hiển thị và quản lý nhiều camera IP (RTSP/RTMP streams) trên một màn hình duy nhất với hiệu năng cao. Ứng dụng kết hợp sức mạnh của WPF UI với khả năng xử lý video hardware-accelerated của FFmpeg và Direct3D 11.

### 🎯 Điểm nổi bật

- ✅ **Hiệu năng cao**: Sử dụng hardware decoding (GPU) thông qua FFmpeg
- ✅ **Render mượt mà**: Direct3D 11 để hiển thị video với FPS cao
- ✅ **Đa luồng**: Mỗi camera chạy trên thread riêng biệt
- ✅ **Linh hoạt**: Hỗ trợ nhiều layout (1x1, 2x2, 3x3, 4x4, wide layouts, custom layouts)
- ✅ **Tương tác thân thiện**: Drag & drop camera, chụp ảnh, tạm dừng/tiếp tục stream
- ✅ **Ổn định**: Tự động reconnect khi mất kết nối

---

## 2. Kiến trúc hệ thống


#### 1️⃣ **UI Layer (C# - WPF)**
- **MainWindow.xaml/cs**: Giao diện chính, quản lý layout selector, camera list, drag & drop
- **CameraWallController.cs**: Business logic, quản lý danh sách camera và cell mapping
- **CameraWallHost.cs**: WinForms control hosting trong WPF để lấy HWND cho D3D11

#### 2️⃣ **Interop Layer (C++/CLI)**
- **CameraWallApi.cpp**: Bridge giữa managed .NET và native C++
- Expose các API để .NET gọi vào engine C++

#### 3️⃣ **Engine Layer (Native C++)**
- **CameraWallEngine**: Core engine quản lý tất cả camera sessions và renderer
- **FFmpegCameraSession**: Quản lý từng camera riêng biệt (connect, decode, pause/resume)
- **D3D11WallRenderer**: Render nhiều video streams lên một surface D3D11

### 🎯 Tại sao cần FFmpeg C++?

| Yếu tố | Giải thích |
|--------|-----------|
| **🚀 Hiệu năng** | FFmpeg C++ cho phép sử dụng **hardware decoding** (DXVA2, NVDEC) để decode video bằng GPU thay vì CPU. Điều này quan trọng khi decode đồng thời 9-16 camera HD. |
| **⚡ Low-latency** | FFmpeg có khả năng xử lý RTSP streams với độ trễ thấp, quan trọng cho camera giám sát real-time. |
| **🎬 Format support** | Hỗ trợ đầy đủ H.264, H.265, MJPEG và các codec/container phổ biến mà .NET không native support. |
| **🔧 Control** | Truy cập trực tiếp AVFrame, AVPacket để tích hợp với Direct3D 11 texture mapping hiệu quả. |
| **💾 Memory** | Quản lý memory chặt chẽ hơn, tránh GC overhead khi xử lý video frames liên tục. |
| **🔌 Protocol** | Hỗ trợ RTSP, RTMP, HTTP(S), UDP và các transport protocol cho IP camera. |


## 3. Yêu cầu hệ thống

### Phần cứng
- **CPU**: Intel Core i5 hoặc tương đương (i7/Ryzen 7 khuyến nghị cho 9+ cameras)
- **GPU**: Card đồ họa hỗ trợ Direct3D 11 (NVIDIA/AMD/Intel HD 4000+)
- **RAM**: 4GB tối thiểu, 8GB+ khuyến nghị
- **Mạng**: Kết nối ổn định (100Mbps+ cho nhiều camera HD)

### Phần mềm
- **OS**: Windows 10/11 (64-bit)
- **Visual Studio**: 2022 với:
  - .NET 8 SDK
  - C++ Desktop Development workload
  - Windows SDK 10.0.22621.0 hoặc cao hơn
- **DirectX**: DirectX 11 Runtime

### Thư viện đi kèm
- ✅ FFmpeg 8.0.1-r3 (đã bao gồm trong folder `lib/`)
- ✅ Tất cả DLLs đã copy vào folder output: **bin/ffmpeg-8.0.1-r3**

---

## 4. Hướng dẫn Build

### Bước 1: Clone repository

```git clone https://github.com/thigiacmaytinh/CameraWallExample.git```

### Bước 2: Mở Solution

1. Mở **Visual Studio 2022**
2. Mở file `CameraWallExample.sln`

### Bước 3: Build từ Visual Studio IDE

1. Chọn **Configuration**: `Release` hoặc `Debug` (Chỉ có 1 platform x64)
2. Click **Build > Build Solution** (Ctrl+Shift+B)


## 5. Hướng dẫn sử dụng
### Bước 1: Chọn Layout

- **1x1**: 1 camera toàn màn hình
- **2x2**: 4 cameras
- **3x3**: 9 cameras
- **4x4**: 16 cameras

### Bước 2: thêm camera bằng rtsp link

VD camera Hik Vision:

`rtsp://admin:Abc123456@192.168.1.64:554/Streaming/Channels/101`

### Bước 3: kéo thả (Drag & Drop) link vào panel

1. **Drag camera** từ danh sách bên trái
2. **Drop** vào cell trong camera wall
3. Stream tự động bắt đầu phát

### Các thao tác khác

| Thao tác | Cách thực hiện |
|----------|----------------|
| **Tạm dừng camera** | Right-click trên cell → **Pause** |
| **Tiếp tục camera** | Right-click trên cell → **Resume** |
| **Chụp ảnh** | Right-click trên cell → **Capture** |
| **Xóa camera khỏi cell** | Right-click trên cell → **Clear** |
| **Xóa camera khỏi danh sách** | Right-click trên camera list → **Remove** |
| **Zoom camera** | Double-click vào cell → Full screen |


## 📝 License

Project này được phát triển cho mục đích học tập và demo.

---

## 👤 Tác giả

https://thigiacmaytinh.com