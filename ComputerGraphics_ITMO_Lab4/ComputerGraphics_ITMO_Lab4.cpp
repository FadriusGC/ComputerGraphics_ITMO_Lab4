#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include <cassert>
#include <stdexcept>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;

#define ThrowIfFailed(x)                                                     \
  {                                                                          \
    HRESULT hr__ = (x);                                                      \
    if (FAILED(hr__)) {                                                      \
      MessageBox(nullptr, L"DirectX Error", L"Error", MB_OK | MB_ICONERROR); \
      std::terminate();                                                      \
    }                                                                        \
  }

// Константы
const int WIDTH = 800;
const int HEIGHT = 600;
const int SwapChainBufferCount = 2;
const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

// Класс окна
class D3DWindow {
 public:
  D3DWindow() : m_hWnd(nullptr), m_width(WIDTH), m_height(HEIGHT) {}
  ~D3DWindow() = default;

  bool Initialize(HINSTANCE hInstance, int width, int height,
                  const wchar_t* title);
  HWND GetHWND() const { return m_hWnd; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }

  // Основной цикл обработки сообщений
  int Run();

 private:
  static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

  HWND m_hWnd;
  int m_width;
  int m_height;
  bool m_isMinimized = false;
  bool m_isMaximized = false;
  bool m_isResizing = false;
};

bool D3DWindow::Initialize(HINSTANCE hInstance, int width, int height,
                           const wchar_t* title) {
  m_width = width;
  m_height = height;

  // Инициализируем класс кона
  WNDCLASS wc = {};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = StaticWindowProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = L"D3D12WindowClass";

  if (!RegisterClass(&wc)) {
    MessageBox(nullptr, L"Failed to register window class", L"Error",
               MB_OK | MB_ICONERROR);
    return false;
  }

  // Вычисляем размер окна с учётом рамок
  RECT rect = {0, 0, m_width, m_height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  // Создаём окно
  m_hWnd = CreateWindow(L"D3D12WindowClass", title, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                        rect.bottom - rect.top, nullptr, nullptr, hInstance,
                        this  // Передаём указатель на класс
  );

  if (!m_hWnd) {
    MessageBox(nullptr, L"Failed to create window", L"Error",
               MB_OK | MB_ICONERROR);
    return false;
  }

  return true;
}

LRESULT CALLBACK D3DWindow::StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam) {
  D3DWindow* pWindow = nullptr;

  if (msg == WM_NCCREATE) {
    // Извлекаем указатель на класс из параметра CREATESTRUCT
    CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
    pWindow = reinterpret_cast<D3DWindow*>(pCreate->lpCreateParams);

    // Сохраняем указатель в пользовательских данных окна
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));

    if (pWindow) {
      pWindow->m_hWnd = hWnd;
    }
  } else {
    // Получаем указатель из пользовательских данных окна
    pWindow =
        reinterpret_cast<D3DWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  }

  if (pWindow) {
    return pWindow->HandleMessage(msg, wParam, lParam);
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT D3DWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_SIZE:
      m_width = LOWORD(lParam);
      m_height = HIWORD(lParam);

      if (wParam == SIZE_MINIMIZED) {
        m_isMinimized = true;
        m_isMaximized = false;
      } else if (wParam == SIZE_MAXIMIZED) {
        m_isMinimized = false;
        m_isMaximized = true;
      } else if (wParam == SIZE_RESTORED) {
        if (m_isMinimized) {
          m_isMinimized = false;
        } else if (m_isMaximized) {
          m_isMaximized = false;
        }
      }
      return 0;

    case WM_ENTERSIZEMOVE:
      m_isResizing = true;
      return 0;

    case WM_EXITSIZEMOVE:
      m_isResizing = false;
      return 0;

    case WM_PAINT:
      // Обработка перерисовки окна
      PAINTSTRUCT ps;
      BeginPaint(m_hWnd, &ps);
      EndPaint(m_hWnd, &ps);
      return 0;
  }

  return DefWindowProc(m_hWnd, msg, wParam, lParam);
}

int D3DWindow::Run() {
  MSG msg = {};

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // Здесь будет основной цикл анимации и чего там хз крч когда таймеры
      // будут, пока тупо слип
      Sleep(10);
    }
  }

  return static_cast<int>(msg.wParam);
}

class D3DApp {
 public:
  D3DApp() = default;
  ~D3DApp() { Cleanup(); }

  bool Initialize(D3DWindow* window);
  void Render();
  void Cleanup();

 private:
  void CreateDevice();
  void CreateCommandObjects();
  void CreateSwapChain();
  void CreateDescriptorHeaps();
  void CreateRTVs();
  void CreateDepthStencil();
  void FlushCommandQueue();

  D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

  D3DWindow* m_window = nullptr;

  // D3D12 объекты
  ComPtr<IDXGIFactory4> m_factory;
  ComPtr<ID3D12Device> m_device;
  ComPtr<IDXGISwapChain> m_swapChain;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
  ComPtr<ID3D12CommandAllocator> m_commandAllocator;
  ComPtr<ID3D12GraphicsCommandList> m_commandList;
  ComPtr<ID3D12Fence> m_fence;
  ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
  ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
  ComPtr<ID3D12Resource> m_swapChainBuffers[SwapChainBufferCount];
  ComPtr<ID3D12Resource> m_depthStencilBuffer;

  UINT m_rtvDescriptorSize = 0;
  UINT m_dsvDescriptorSize = 0;
  UINT m_cbvSrvDescriptorSize = 0;
  UINT m_4xMsaaQuality = 0;
  UINT64 m_currentFence = 0;
  int m_currBackBuffer = 0;
};

bool D3DApp::Initialize(D3DWindow* window) {
  m_window = window;

  try {
    CreateDevice();
    CreateCommandObjects();
    CreateSwapChain();
    CreateDescriptorHeaps();
    CreateRTVs();
    CreateDepthStencil();
    return true;
  } catch (...) {
    Cleanup();
    return false;
  }
}

void D3DApp::CreateDevice() {
  ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));
  // отладочный слой в дебаг режиме
#if defined(DEBUG) || defined(_DEBUG)
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
  }
#endif

  // делаем двейс
  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&m_device));

  // если не получилось, используем WARP
  if (FAILED(hr)) {
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&m_device)));
  }

  // Создание ограждения
  ThrowIfFailed(
      m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

  // Получение размеров дескрипторов
  m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Проверка поддержки MSAA
  D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
  msQualityLevels.Format = BackBufferFormat;
  msQualityLevels.SampleCount = 4;
  msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
  msQualityLevels.NumQualityLevels = 0;

  ThrowIfFailed(
      m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                    &msQualityLevels, sizeof(msQualityLevels)));

  m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
  assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void D3DApp::CreateCommandObjects() {
  // Очередь команд
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  ThrowIfFailed(
      m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

  // Распределитель команд
  ThrowIfFailed(m_device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

  // Список команд
  ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            m_commandAllocator.Get(), nullptr,
                                            IID_PPV_ARGS(&m_commandList)));

  m_commandList->Close();
}

void D3DApp::CreateSwapChain() {
  // Очищаем предыдущую цепочку
  m_swapChain.Reset();

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferDesc.Width = m_window->GetWidth();
  sd.BufferDesc.Height = m_window->GetHeight();
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = BackBufferFormat;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = SwapChainBufferCount;
  sd.OutputWindow = m_window->GetHWND();
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  ThrowIfFailed(
      m_factory->CreateSwapChain(m_commandQueue.Get(), &sd, &m_swapChain));
}

void D3DApp::CreateDescriptorHeaps() {
  // Куча RTV
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

  // Куча DSV
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
}

void D3DApp::CreateRTVs() {
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
      m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

  for (UINT i = 0; i < SwapChainBufferCount; i++) {
    ThrowIfFailed(
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i])));
    m_device->CreateRenderTargetView(m_swapChainBuffers[i].Get(), nullptr,
                                     rtvHeapHandle);
    rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
  }
}

void D3DApp::CreateDepthStencil() {
  D3D12_RESOURCE_DESC depthStencilDesc = {};
  depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthStencilDesc.Width = m_window->GetWidth();
  depthStencilDesc.Height = m_window->GetHeight();
  depthStencilDesc.DepthOrArraySize = 1;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.Format = DepthStencilFormat;
  depthStencilDesc.SampleDesc.Count = 1;
  depthStencilDesc.SampleDesc.Quality = 0;
  depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE optClear = {};
  optClear.Format = DepthStencilFormat;
  optClear.DepthStencil.Depth = 1.0f;
  optClear.DepthStencil.Stencil = 0;

  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

  ThrowIfFailed(m_device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc,
      D3D12_RESOURCE_STATE_COMMON, &optClear,
      IID_PPV_ARGS(&m_depthStencilBuffer)));

  // Создаём DSV
  m_device->CreateDepthStencilView(
      m_depthStencilBuffer.Get(), nullptr,
      m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

  // Переводим ресурс в нужное состояние
  ThrowIfFailed(m_commandAllocator->Reset());
  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

  m_commandList->ResourceBarrier(1, &barrier);

  ThrowIfFailed(m_commandList->Close());

  ID3D12CommandList* cmdLists[] = {m_commandList.Get()};
  m_commandQueue->ExecuteCommandLists(1, cmdLists);

  FlushCommandQueue();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const {
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
      m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currBackBuffer,
      m_rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const {
  return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::Render() {
  // Сброс команд
  ThrowIfFailed(m_commandAllocator->Reset());
  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

  // Переход заднего буфера в состояние RENDER_TARGET
  auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_swapChainBuffers[m_currBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);

  m_commandList->ResourceBarrier(1, &barrier1);

  // Очистка заднего буфера
  float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};

  // Используем функцию CurrentBackBufferView
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();

  m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();

  m_commandList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);

  // Устанавливаем RTV и DSV
  m_commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

  // Переход обратно в PRESENT
  auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_swapChainBuffers[m_currBackBuffer].Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

  m_commandList->ResourceBarrier(1, &barrier2);

  // Завершаем запись команд
  ThrowIfFailed(m_commandList->Close());

  // Выполняем команды
  ID3D12CommandList* cmdLists[] = {m_commandList.Get()};
  m_commandQueue->ExecuteCommandLists(1, cmdLists);

  // Показываем кадр
  ThrowIfFailed(m_swapChain->Present(1, 0));
  m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

  // Ждём завершения кадра
  FlushCommandQueue();
}

void D3DApp::FlushCommandQueue() {
  m_currentFence++;

  ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

  if (m_fence->GetCompletedValue() < m_currentFence) {
    HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);

    if (eventHandle) {
      ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
    }
  }
}

void D3DApp::Cleanup() {
  FlushCommandQueue();

  // Освобождаем ресурсы в правильном порядке
  for (int i = 0; i < SwapChainBufferCount; ++i) {
    m_swapChainBuffers[i].Reset();
  }

  m_depthStencilBuffer.Reset();
  m_rtvHeap.Reset();
  m_dsvHeap.Reset();
  m_commandList.Reset();
  m_commandAllocator.Reset();
  m_commandQueue.Reset();
  m_swapChain.Reset();
  m_device.Reset();
  m_factory.Reset();
  m_fence.Reset();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine,
                   int showCmd) {
  (void)prevInstance;
  (void)cmdLine;

  // Создаём окно
  D3DWindow window;
  if (!window.Initialize(hInstance, WIDTH, HEIGHT,
                         L"Direct3D 12 Initialization")) {
    return 1;
  }

  // Инициализируем Direct3D
  D3DApp app;
  if (!app.Initialize(&window)) {
    return 1;
  }

  // Показываем окно
  ShowWindow(window.GetHWND(), showCmd);
  UpdateWindow(window.GetHWND());

  // Основной цикл
  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      app.Render();
    }
  }

  return 0;
}
