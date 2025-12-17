#include <SimpleMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;
using std::wstring;

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

class GameTimer {
 public:
  GameTimer();

  float TotalTime() const;  // в секундах
  float DeltaTime() const;  // в секундах

  void Reset();  // Вызывать перед циклом сообщений
  void Start();  // Вызывать при возобновлении
  void Stop();   // Вызывать при паузе
  void Tick();   // Вызывать каждый кадр

 private:
  double mSecondsPerCount;
  double mDeltaTime;

  __int64 mBaseTime;
  __int64 mPausedTime;
  __int64 mStopTime;
  __int64 mPrevTime;
  __int64 mCurrTime;

  bool mStopped;
};
// Класс таймера, со второй части лабы
GameTimer::GameTimer()
    : mSecondsPerCount(0.0),
      mDeltaTime(-1.0),
      mBaseTime(0),
      mPausedTime(0),
      mPrevTime(0),
      mCurrTime(0),
      mStopped(false) {
  __int64 countsPerSec;
  QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
  mSecondsPerCount = 1.0 / (double)countsPerSec;
}

void GameTimer::Reset() {
  __int64 currTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

  mBaseTime = currTime;
  mPrevTime = currTime;
  mStopTime = 0;
  mStopped = false;
}

void GameTimer::Start() {
  __int64 startTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

  // Если возобновляем после паузы
  if (mStopped) {
    // Накопляем время паузы
    mPausedTime += (startTime - mStopTime);

    // Сбрасываем предыдущее время, так как оно было получено во время паузы
    mPrevTime = startTime;

    // Сбрасываем стоп-время и флаг
    mStopTime = 0;
    mStopped = false;
  }
}

void GameTimer::Stop() {
  if (!mStopped) {
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    // Сохраняем время остановки
    mStopTime = currTime;
    mStopped = true;
  }
}

void GameTimer::Tick() {
  if (mStopped) {
    mDeltaTime = 0.0;
    return;
  }

  // Получаем текущее время
  __int64 currTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
  mCurrTime = currTime;

  // Вычисляем разницу с предыдущим кадром
  mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

  // Готовимся к следующему кадру
  mPrevTime = mCurrTime;

  // Гарантируем неотрицательность
  if (mDeltaTime < 0.0) {
    mDeltaTime = 0.0;
  }
}

float GameTimer::TotalTime() const {
  // Если таймер остановлен, не считаем время с момента остановки
  if (mStopped) {
    return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
  } else {
    return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
  }
}

float GameTimer::DeltaTime() const { return (float)mDeltaTime; }

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
  bool IsPaused() const { return m_appPaused || m_minimized || m_resizing; }
  bool IsResizing() const { return m_resizing; }
  void SetPaused(bool paused) { m_appPaused = paused; }

 private:
  static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

  HWND m_hWnd;
  int m_width;
  int m_height;

  // Флаги состояния (как в луне)
  bool m_appPaused = false;  // Приложение на паузе
  bool m_minimized = false;  // Окно свернуто
  bool m_maximized = false;  // Окно развернуто
  bool m_resizing = false;   // Изменение размера
};

bool D3DWindow::Initialize(HINSTANCE hInstance, int width, int height,
                           const wchar_t* title) {
  m_width = width;
  m_height = height;

  // Инициализируем класс окна
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
                        this);  // Передаём указатель на класс

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
      // Сообщение активации окна (как в луне)
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE) {
        m_appPaused = true;
      } else {
        m_appPaused = false;
      }
      return 0;

      // Изменение размера окна
    case WM_SIZE:
      m_width = LOWORD(lParam);
      m_height = HIWORD(lParam);

      if (wParam == SIZE_MINIMIZED) {
        m_minimized = true;
        m_maximized = false;
        m_appPaused = true;
      } else if (wParam == SIZE_MAXIMIZED) {
        m_minimized = false;
        m_maximized = true;
        m_appPaused = false;
      } else if (wParam == SIZE_RESTORED) {
        // Восстановление из свернутого/развернутого состояния
        if (m_minimized) {
          m_minimized = false;
          m_appPaused = false;
        } else if (m_maximized) {
          m_maximized = false;
          m_appPaused = false;
        }
      }
      return 0;

      // Начало изменения размера
    case WM_ENTERSIZEMOVE:
      m_resizing = true;
      m_appPaused = true;
      return 0;

      // Конец изменения размера
    case WM_EXITSIZEMOVE:
      m_resizing = false;
      m_appPaused = false;
      return 0;

      // Закрытие окна
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

      // Предотвращение слишком маленького окна
    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
      ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
      return 0;

      // Обработка перерисовки
    case WM_PAINT:
      PAINTSTRUCT ps;
      BeginPaint(m_hWnd, &ps);
      EndPaint(m_hWnd, &ps);
      return 0;
  }

  return DefWindowProc(m_hWnd, msg, wParam, lParam);
}

// Класс приложения
class D3DApp {
 public:
  D3DApp() = default;
  ~D3DApp() { Cleanup(); }

  bool Initialize(HINSTANCE hInstance);
  int Run();  // Главный цикл приложения

  virtual void Update(const GameTimer& gt);
  virtual void Draw(const GameTimer& gt);

  void Cleanup();
  HWND GetMainWnd() const { return m_window.GetHWND(); }

 private:
  void CreateDevice();
  void CreateCommandObjects();
  void CreateSwapChain();
  void CreateDescriptorHeaps();
  void CreateRTVs();
  void CreateDepthStencil();
  void FlushCommandQueue();
  void CalculateFrameStats();  // Статистика кадров

  D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

  // Окно приложения
  D3DWindow m_window;
  GameTimer m_timer;

  // Статистика кадров
  int m_frameCnt = 0;
  float m_timeElapsed = 0.0f;
  wstring m_mainWndCaption = L"Direct3D 12 App";

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

bool D3DApp::Initialize(HINSTANCE hInstance) {
  // Инициализируем окно
  if (!m_window.Initialize(hInstance, WIDTH, HEIGHT,
                           L"Direct3D 12 Initialization")) {
    return false;
  }

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

int D3DApp::Run() {
  MSG msg = {0};
  m_timer.Reset();

  while (msg.message != WM_QUIT) {
    // Если есть сообщения окна, обрабатываем их
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    // В противном случае, выполняем игровые операции
    else {
      m_timer.Tick();

      if (!m_window.IsPaused()) {
        CalculateFrameStats();  // Вычисляем статистику
        Update(m_timer);        // Обновляем логику
        Draw(m_timer);          // Рендерим кадр
      } else {
        // Если приложение на паузе, слипаем систему
        Sleep(100);
      }
    }
  }

  return (int)msg.wParam;
}

void D3DApp::CalculateFrameStats() {
  // Вычисляем FPS (по примеру из Луны)
  m_frameCnt++;

  // Вычисляем среднее за 1 секунду
  if ((m_timer.TotalTime() - m_timeElapsed) >= 1.0f) {
    float fps = (float)m_frameCnt;  // fps = frameCnt / 1 секунда
    float mspf = 1000.0f / fps;     // миллисекунд на кадр

    wstring fpsStr = std::to_wstring(fps);
    wstring mspfStr = std::to_wstring(mspf);

    wstring windowText =
        m_mainWndCaption + L"    fps: " + fpsStr + L"   mspf: " + mspfStr;

    SetWindowText(m_window.GetHWND(), windowText.c_str());

    // Сбрасываем для следующего усреднения
    m_frameCnt = 0;
    m_timeElapsed += 1.0f;
  }
}

void D3DApp::CreateDevice() {
  ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));

  // Включаем отладочный слой в режиме Debug
#if defined(DEBUG) || defined(_DEBUG)
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
  }
#endif

  // Пытаемся создать устройство
  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&m_device));

  // Если не получилось, используем WARP адаптер
  if (FAILED(hr)) {
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&m_device)));
  }

  // Создаем ограждение
  ThrowIfFailed(
      m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

  // Получаем размеры дескрипторов
  m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Проверяем поддержку MSAA
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
  sd.BufferDesc.Width = m_window.GetWidth();
  sd.BufferDesc.Height = m_window.GetHeight();
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = BackBufferFormat;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = SwapChainBufferCount;
  sd.OutputWindow = m_window.GetHWND();
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
  depthStencilDesc.Width = m_window.GetWidth();
  depthStencilDesc.Height = m_window.GetHeight();
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

void D3DApp::Update(const GameTimer& gt) {
  // Базовая реализация пуста
  // В производных классах можно переопределить для анимации
}

void D3DApp::Draw(const GameTimer& gt) {
  // Используем время для анимации цвета
  float time = gt.TotalTime();
  float r = (sinf(time) + 1.0f) * 0.5f;  // От 0 до 1
  float g = (cosf(time * 0.7f) + 1.0f) * 0.5f;
  float b = (sinf(time * 1.3f) + 1.0f) * 0.5f;

  float clearColor[] = {r, g, b, 1.0f};

  // Сброс команд
  ThrowIfFailed(m_commandAllocator->Reset());
  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

  // Переход заднего буфера в состояние RENDER_TARGET
  auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_swapChainBuffers[m_currBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);

  m_commandList->ResourceBarrier(1, &barrier1);

  // Очистка заднего буфера (цвет меняется со временем)
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
  m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

  // Очистка буфера глубины/трафарета
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

// Запуск
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine,
                   int showCmd) {
  (void)prevInstance;
  (void)cmdLine;

  // Включаем проверку утечек памяти в Debug
#if defined(DEBUG) || defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try {
    // Создаем приложение
    D3DApp app;
    if (!app.Initialize(hInstance)) {
      return 1;
    }

    // Показываем окно
    ShowWindow(app.GetMainWnd(), showCmd);
    UpdateWindow(app.GetMainWnd());

    // Запускаем главный цикл
    return app.Run();

  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
