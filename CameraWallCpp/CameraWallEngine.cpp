#include "CameraWallEngine.h"
#include "D3D11WallRenderer.h"
#include "FFmpegCameraSession.h"
#include "CameraLayoutFactory.h"

CameraWallEngine::CameraWallEngine()
{
}

CameraWallEngine::~CameraWallEngine()
{
    Stop();
}

bool CameraWallEngine::Initialize(HWND hwnd)
{
    m_renderer = std::make_unique<D3D11WallRenderer>();
    if (!m_renderer->Initialize(hwnd))
        return false;

    m_renderer->SetLayout(CameraLayoutFactory::CreateGrid(2, 2));
    return true;
}

void CameraWallEngine::Resize(int width, int height)
{
    if (m_renderer)
        m_renderer->Resize(width, height);
}

void CameraWallEngine::Start()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running)
        return;

    m_running = true;

    if (m_renderer)
        m_renderer->Start();

    for (auto& kv : m_cameras)
        kv.second->Start();
}

void CameraWallEngine::Stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running)
        return;

    for (auto& kv : m_cameras)
        kv.second->Stop();

    if (m_renderer)
        m_renderer->Stop();

    m_running = false;
}

bool CameraWallEngine::AddCamera(int cameraId, const std::wstring& rtspUrl)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_renderer)
        return false;

    auto found = m_cameras.find(cameraId);
    if (found != m_cameras.end())
        return true;

    auto session = std::make_unique<FFmpegCameraSession>(cameraId, rtspUrl, m_renderer.get());

    if (m_running)
        session->Start();

    m_cameras[cameraId] = std::move(session);
    return true;
}

void CameraWallEngine::RemoveCamera(int cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
        return;

    it->second->Stop();
    m_cameras.erase(it);

    if (m_renderer)
        m_renderer->ClearCamera(cameraId);
}


void CameraWallEngine::PauseCamera(int cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end())
        it->second->Pause();
}

void CameraWallEngine::ResumeCamera(int cameraId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end())
        it->second->Resume();
}

bool CameraWallEngine::CaptureCamera(int cameraId, const std::wstring& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cameras.find(cameraId);
    if (it == m_cameras.end())
        return false;

    return it->second->CaptureToFile(filePath);
}

void CameraWallEngine::SetGridLayout(int rows, int cols)
{
    if (m_renderer)
        m_renderer->SetLayout(CameraLayoutFactory::CreateGrid(rows, cols));
}

void CameraWallEngine::SetStandardLayout(int count)
{
    if (m_renderer)
        m_renderer->SetLayout(CameraLayoutFactory::CreateStandard(count));
}

void CameraWallEngine::SetWideLayout(int count)
{
    if (m_renderer)
        m_renderer->SetLayout(CameraLayoutFactory::CreateWide(count));
}

void CameraWallEngine::SetCustomLayout(const CameraLayout& layout)
{
    if (!m_renderer)
        return;

    if (!CameraLayoutFactory::Validate(layout))
        return;

    m_renderer->SetLayout(layout);
}

void CameraWallEngine::AssignCameraToCell(int cellId, int cameraId)
{
    if (m_renderer)
        m_renderer->AssignCameraToCell(cellId, cameraId);
}


void CameraWallEngine::ClearCell(int cellId)
{
    if (m_renderer)
        m_renderer->ClearCell(cellId);
}

int CameraWallEngine::GetCellAt(int x, int y)
{
    if (!m_renderer)
        return -1;

    return m_renderer->GetCellAt(x, y);
}

bool CameraWallEngine::GetCellBounds(int cellId, int& x, int& y, int& width, int& height)
{
    if (!m_renderer)
        return false;

    return m_renderer->GetCellBounds(cellId, x, y, width, height);
}

void CameraWallEngine::SetSelectedCell(int cellId)
{
    if (m_renderer)
        m_renderer->SetSelectedCell(cellId);
}

void CameraWallEngine::SetHoveredCell(int cellId)
{
    if (m_renderer)
        m_renderer->SetHoveredCell(cellId);
}

