#include <SimpleMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

// clamp для C++14
template <typename T>
const T& clamp_val(const T& value, const T& min, const T& max) {
  return (value < min) ? min : (value > max) ? max : value;
}

// Макросы для извлечения координат из lParam
#define GET_X_LPARAM(lParam) ((int)(short)LOWORD(lParam))
#define GET_Y_LPARAM(lParam) ((int)(short)HIWORD(lParam))

// Структуры вершин и констант
struct Vertex {
  DirectX::SimpleMath::Vector3 Pos;
  DirectX::SimpleMath::Vector3 Normal;  // Нормали для корректного освещения
  DirectX::SimpleMath::Vector4 Color;
};

struct ObjectConstants {
  DirectX::SimpleMath::Matrix World;          // Матрица мира для трансформации
  DirectX::SimpleMath::Matrix WorldViewProj;  // Матрица проекции
};

// Структура для параметров света
struct LightConstants {
  DirectX::SimpleMath::Vector4
      LightPosition;                        // Позиция света (x, y, z, w=1.0f)
  DirectX::SimpleMath::Vector4 LightColor;  // Цвет света (r, g, b, a)
  DirectX::SimpleMath::Vector4
      CameraPosition;  // Позиция камеры для вычисления viewDir
};

// Класс для загрузки ресурсов GPU (шаблонный)
template <typename T>
class UploadBuffer {
 public:
  UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
      : mElementCount(elementCount), mIsConstantBuffer(isConstantBuffer) {
    mElementByteSize = sizeof(T);

    // Для константных буферов требуется выравнивание до 256 байт
    if (isConstantBuffer) {
      mElementByteSize = (sizeof(T) + 255) & ~255;
    }

    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mUploadBuffer)));

    ThrowIfFailed(
        mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
  }

  ~UploadBuffer() {
    if (mUploadBuffer != nullptr) {
      mUploadBuffer->Unmap(0, nullptr);
    }
    mMappedData = nullptr;
  }

  UploadBuffer(const UploadBuffer&) = delete;
  UploadBuffer& operator=(const UploadBuffer&) = delete;

  void CopyData(int elementIndex, const T& data) {
    memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
  }

  ID3D12Resource* Resource() const { return mUploadBuffer.Get(); }

 private:
  ComPtr<ID3D12Resource> mUploadBuffer;
  BYTE* mMappedData = nullptr;
  UINT mElementCount;
  UINT mElementByteSize = 0;
  bool mIsConstantBuffer;
};

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

GameTimer::GameTimer()
    : mSecondsPerCount(0.0),
      mDeltaTime(-1.0),
      mBaseTime(0),
      mPausedTime(0),
      mStopTime(0),
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

  if (mStopped) {
    mPausedTime += (startTime - mStopTime);
    mPrevTime = startTime;
    mStopTime = 0;
    mStopped = false;
  }
}

void GameTimer::Stop() {
  if (!mStopped) {
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mStopTime = currTime;
    mStopped = true;
  }
}

void GameTimer::Tick() {
  if (mStopped) {
    mDeltaTime = 0.0;
    return;
  }

  __int64 currTime;
  QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
  mCurrTime = currTime;

  mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
  mPrevTime = mCurrTime;

  if (mDeltaTime < 0.0) {
    mDeltaTime = 0.0;
  }
}

float GameTimer::TotalTime() const {
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

  bool m_appPaused = false;
  bool m_minimized = false;
  bool m_maximized = false;
  bool m_resizing = false;
};

bool D3DWindow::Initialize(HINSTANCE hInstance, int width, int height,
                           const wchar_t* title) {
  m_width = width;
  m_height = height;

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

  RECT rect = {0, 0, m_width, m_height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  m_hWnd =
      CreateWindow(L"D3D12WindowClass", title, WS_OVERLAPPEDWINDOW,
                   CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                   rect.bottom - rect.top, nullptr, nullptr, hInstance, this);

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
    CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
    pWindow = reinterpret_cast<D3DWindow*>(pCreate->lpCreateParams);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
    if (pWindow) {
      pWindow->m_hWnd = hWnd;
    }
  } else {
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
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE) {
        m_appPaused = true;
      } else {
        m_appPaused = false;
      }
      return 0;

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
        if (m_minimized) {
          m_minimized = false;
          m_appPaused = false;
        } else if (m_maximized) {
          m_maximized = false;
          m_appPaused = false;
        }
      }
      return 0;

    case WM_ENTERSIZEMOVE:
      m_resizing = true;
      m_appPaused = true;
      return 0;

    case WM_EXITSIZEMOVE:
      m_resizing = false;
      m_appPaused = false;
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
      ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
      return 0;

    case WM_PAINT:
      PAINTSTRUCT ps;
      BeginPaint(m_hWnd, &ps);
      EndPaint(m_hWnd, &ps);
      return 0;
  }

  return DefWindowProc(m_hWnd, msg, wParam, lParam);
}

// Класс приложения с кубом
class BoxApp {
 public:
  BoxApp(HINSTANCE hInstance);
  BoxApp(const BoxApp& rhs) = delete;
  BoxApp& operator=(const BoxApp& rhs) = delete;
  ~BoxApp();

  bool Initialize();
  int Run();
  HWND GetMainWnd() const { return m_window.GetHWND(); }

 private:
  void OnResize();
  void Update(const GameTimer& gt);
  void Draw(const GameTimer& gt);
  void OnMouseDown(WPARAM btnState, int x, int y);
  void OnMouseUp(WPARAM btnState, int x, int y);
  void OnMouseMove(WPARAM btnState, int x, int y);

  void BuildDescriptorHeaps();
  void BuildConstantBuffers();
  void BuildRootSignature();
  void BuildShadersAndInputLayout();
  void BuildBoxGeometry();
  void BuildPSO();

  // D3D12 объекты
  ComPtr<IDXGIFactory4> mFactory;
  ComPtr<ID3D12Device> mDevice;
  ComPtr<IDXGISwapChain> mSwapChain;
  ComPtr<ID3D12CommandQueue> mCommandQueue;
  ComPtr<ID3D12CommandAllocator> mCommandAllocator;
  ComPtr<ID3D12GraphicsCommandList> mCommandList;
  ComPtr<ID3D12Fence> mFence;
  ComPtr<ID3D12DescriptorHeap> mRtvHeap;
  ComPtr<ID3D12DescriptorHeap> mDsvHeap;
  ComPtr<ID3D12DescriptorHeap> mCbvHeap;
  ComPtr<ID3D12RootSignature> mRootSignature;
  ComPtr<ID3D12PipelineState> mPSO;
  ComPtr<ID3D12Resource> mSwapChainBuffers[SwapChainBufferCount];
  ComPtr<ID3D12Resource> mDepthStencilBuffer;

  // Ресурсы геометрии
  ComPtr<ID3D12Resource> mVertexBufferGPU;
  ComPtr<ID3D12Resource> mIndexBufferGPU;
  ComPtr<ID3D12Resource> mVertexBufferUploader;
  ComPtr<ID3D12Resource> mIndexBufferUploader;

  // Шейдеры
  ComPtr<ID3DBlob> mVSByteCode;
  ComPtr<ID3DBlob> mPSByteCode;

  // Константные буферы
  std::unique_ptr<UploadBuffer<ObjectConstants>>
      mObjectCB;  // Для объекта (World, WorldViewProj)
  std::unique_ptr<UploadBuffer<LightConstants>>
      mLightCB;  // Для параметров света

  // Входной лейаут
  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  // Дескрипторы
  UINT mRtvDescriptorSize = 0;
  UINT mDsvDescriptorSize = 0;
  UINT mCbvSrvDescriptorSize = 0;

  // Статистика
  D3D12_VIEWPORT mScreenViewport;
  D3D12_RECT mScissorRect;
  UINT m4xMsaaQuality = 0;
  UINT64 mCurrentFence = 0;
  int mCurrBackBuffer = 0;

  // Окно и таймер
  D3DWindow m_window;
  GameTimer mTimer;

  // Матрицы и камера
  DirectX::SimpleMath::Matrix mWorld;
  DirectX::SimpleMath::Matrix mView;
  DirectX::SimpleMath::Matrix mProj;
  float mTheta = 1.5f * DirectX::XM_PI;
  float mPhi = DirectX::XM_PIDIV4;
  float mRadius = 5.0f;
  POINT mLastMousePos;

  // Геометрия куба
  UINT mVertexBufferByteSize = 0;
  UINT mIndexBufferByteSize = 0;
  D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
  D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
  UINT mIndexCount = 0;

  void CreateDevice();
  void CreateCommandObjects();
  void CreateSwapChain();
  void CreateRTVs();
  void CreateDepthStencil();
  void FlushCommandQueue();
  void CalculateFrameStats();
  D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
};

BoxApp::BoxApp(HINSTANCE hInstance) : m_window(), mTimer() {
  mWorld = DirectX::SimpleMath::Matrix::Identity;
  mView = DirectX::SimpleMath::Matrix::Identity;
  mProj = DirectX::SimpleMath::Matrix::Identity;
}

BoxApp::~BoxApp() { FlushCommandQueue(); }

bool BoxApp::Initialize() {
  if (!m_window.Initialize(GetModuleHandle(nullptr), WIDTH, HEIGHT,
                           L"Direct3D 12 Box with Phong Lighting")) {
    return false;
  }

  CreateDevice();
  CreateCommandObjects();
  CreateSwapChain();
  BuildDescriptorHeaps();
  CreateRTVs();
  CreateDepthStencil();

  // Создаем вьюпорт и ножницы
  mScreenViewport.TopLeftX = 0;
  mScreenViewport.TopLeftY = 0;
  mScreenViewport.Width = static_cast<float>(WIDTH);
  mScreenViewport.Height = static_cast<float>(HEIGHT);
  mScreenViewport.MinDepth = 0.0f;
  mScreenViewport.MaxDepth = 1.0f;

  mScissorRect = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};

  // Сброс командного списка для инициализации
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

  BuildRootSignature();
  BuildShadersAndInputLayout();
  BuildBoxGeometry();
  BuildConstantBuffers();
  BuildPSO();

  // Закрываем список команд
  ThrowIfFailed(mCommandList->Close());

  // Выполняем команды инициализации
  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);

  FlushCommandQueue();

  // Проекционная матрица
  OnResize();

  return true;
}

void BoxApp::OnResize() {
  // Используем проекционную матрицу для левосторонней системы
  // координат
  mProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
      0.25f * DirectX::XM_PI,
      static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 1000.0f);
}

void BoxApp::BuildDescriptorHeaps() {
  // Куча RTV
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

  // Куча DSV
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

  // Куча CBV (для константных буферов)
  D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
  cbvHeapDesc.NumDescriptors = 2;  // Два буфера: Object и Light
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
  // Константный буфер для объекта (World и WorldViewProj)
  mObjectCB = std::unique_ptr<UploadBuffer<ObjectConstants>>(
      new UploadBuffer<ObjectConstants>(mDevice.Get(), 1, true));

  // Константный буфер для параметров света
  mLightCB = std::unique_ptr<UploadBuffer<LightConstants>>(
      new UploadBuffer<LightConstants>(mDevice.Get(), 1, true));

  // Описание CBV для ObjectConstants (register(b0))
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescObject = {};
  cbvDescObject.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress();
  cbvDescObject.SizeInBytes =
      (sizeof(ObjectConstants) + 255) & ~255;  // Выравнивание до 256 байт

  // Описание CBV для LightConstants (register(b1))
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescLight = {};
  cbvDescLight.BufferLocation = mLightCB->Resource()->GetGPUVirtualAddress();
  cbvDescLight.SizeInBytes =
      (sizeof(LightConstants) + 255) & ~255;  // Выравнивание до 256 байт

  // Создаем CBV для ObjectConstants в начале кучи
  mDevice->CreateConstantBufferView(
      &cbvDescObject, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

  // Создаем CBV для LightConstants со смещением
  CD3DX12_CPU_DESCRIPTOR_HANDLE lightCbvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvDescriptorSize);
  mDevice->CreateConstantBufferView(&cbvDescLight, lightCbvHandle);
}

void BoxApp::BuildRootSignature() {
  // Создаем два слота: один для ObjectConstants (b0), другой для LightConstants
  // (b1)
  CD3DX12_ROOT_PARAMETER slotRootParameter[2];

  // Слот 0: ObjectConstants (b0)
  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

  // Слот 1: LightConstants (b1)
  CD3DX12_DESCRIPTOR_RANGE cbvTable1;
  cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

  // Описание корневой сигнатуры
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
      2, slotRootParameter, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;

  ThrowIfFailed(D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
      serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

  if (errorBlob != nullptr) {
    OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  }

  ThrowIfFailed(mDevice->CreateRootSignature(
      0, serializedRootSig->GetBufferPointer(),
      serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout() {
  // Вершинный шейдер
  const char* vsCode =
      "struct VS_INPUT {\n"
      "    float3 Pos : POSITION;\n"
      "    float3 Normal : NORMAL;\n"
      "    float4 Color : COLOR;\n"
      "};\n"
      "struct VS_OUTPUT {\n"
      "    float4 Pos : SV_POSITION;\n"
      "    float3 WorldPos : WORLDPOS;\n"  // Позиция в мировом пространстве
      "    float3 Normal : NORMAL;\n"      // Нормаль в мировом пространстве
      "    float4 Color : COLOR;\n"
      "};\n"
      "cbuffer cbPerObject : register(b0) {\n"
      "    float4x4 gWorld;\n"          // Матрица мира
      "    float4x4 gWorldViewProj;\n"  // Матрица проекции
      "};\n"
      "cbuffer cbLight : register(b1) {\n"
      "    float4 gLightPosition;\n"
      "    float4 gLightColor;\n"
      "    float4 gCameraPosition;\n"
      "};\n"
      "VS_OUTPUT VS(VS_INPUT input) {\n"
      "    VS_OUTPUT output;\n"
      "    output.Pos = mul(float4(input.Pos, 1.0f), gWorldViewProj);\n"
      "    output.WorldPos = mul(float4(input.Pos, 1.0f), gWorld).xyz;\n"
      "    // Трансформируем нормаль: используем матрицу мира (без "
      "транспонирования, так как нет масштабирования)\n"
      "    output.Normal = mul(float4(input.Normal, 0.0f), gWorld).xyz;\n"
      "    output.Color = input.Color;\n"
      "    return output;\n"
      "}";

  // Пиксельный шейдер
  const char* psCode =
      "struct PS_INPUT {\n"
      "    float4 Pos : SV_POSITION;\n"
      "    float3 WorldPos : WORLDPOS;\n"
      "    float3 Normal : NORMAL;\n"
      "    float4 Color : COLOR;\n"
      "};\n"
      "cbuffer cbLight : register(b1) {\n"
      "    float4 gLightPosition;\n"
      "    float4 gLightColor;\n"
      "    float4 gCameraPosition;\n"
      "};\n"
      "float4 PS(PS_INPUT input) : SV_Target {\n"
      "    // Нормализуем нормаль\n"
      "    float3 normal = normalize(input.Normal);\n"
      "    \n"
      "    // Направление света\n"
      "    float3 lightDir = normalize(gLightPosition.xyz - input.WorldPos);\n"
      "    \n"
      "    // Диффузное освещение\n"
      "    float diffuse = max(dot(normal, lightDir), 0.0f);\n"
      "    float3 diffuseColor = diffuse * gLightColor.rgb * input.Color.rgb;\n"
      "    \n"
      "    // Амбиентное освещение\n"
      "    float3 ambient = 0.1f * gLightColor.rgb * input.Color.rgb;\n"
      "    \n"
      "    // Специфическое освещение\n"
      "    float3 viewDir = normalize(gCameraPosition.xyz - input.WorldPos);\n"
      "    float3 reflectDir = reflect(-lightDir, normal);\n"
      "    float specular = pow(max(dot(reflectDir, viewDir), 0.0f), 32.0f);\n"
      "    float3 specularColor = specular * gLightColor.rgb * 0.5f;\n"
      "    \n"
      "    float3 finalColor = ambient + diffuseColor + specularColor;\n"
      "    return float4(finalColor, 1.0f);\n"
      "}";

  ComPtr<ID3DBlob> errorBlob;

  // Компилируем шейдеры
  HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
                          "VS", "vs_5_0", 0, 0, &mVSByteCode, &errorBlob);

  if (FAILED(hr)) {
    if (errorBlob != nullptr) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
  }

  hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "PS",
                  "ps_5_0", 0, 0, &mPSByteCode, &errorBlob);

  if (FAILED(hr)) {
    if (errorBlob != nullptr) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
  }

  // Входной лейаут
  mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                   12,  // Смещение после Pos (12 байт)
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                   24,  // Смещение после Normal (12 байт)
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void BoxApp::BuildBoxGeometry() {
  // Создаем 24 вершины (по 4 на грань) для корректных нормалей
  std::array<Vertex, 24> vertices = {
      // Front (z = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 0: белый
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 1: черный
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 2: красный
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 3: зеленый

      // Back (z = 1)
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 4: синий
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 5: желтый
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 6: голубой
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 7: пурпурный

      // Left (x = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 8
      Vertex({{-1.0f, 1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 9
      Vertex({{-1.0f, 1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 10
      Vertex({{-1.0f, -1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 11

      // Right (x = 1)
      Vertex({{1.0f, -1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 12
      Vertex({{1.0f, 1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 13
      Vertex({{1.0f, 1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 14
      Vertex({{1.0f, -1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 15

      // Top (y = 1)
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 16
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 17
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 18
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 19

      // Bottom (y = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 20
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 21
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 22
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 23
  };

  // Индексы: по 6 индексов на грань (36 индексов всего)
  std::array<uint16_t, 36> indices = {// Front face
                                      0, 1, 2, 0, 2, 3,
                                      // Back face
                                      4, 5, 6, 4, 6, 7,
                                      // Left face
                                      8, 9, 10, 8, 10, 11,
                                      // Right face
                                      12, 13, 14, 12, 14, 15,
                                      // Top face
                                      16, 17, 18, 16, 18, 19,
                                      // Bottom face
                                      20, 21, 22, 20, 22, 23};

  mVertexBufferByteSize = (UINT)(vertices.size() * sizeof(Vertex));
  mIndexBufferByteSize = (UINT)(indices.size() * sizeof(uint16_t));
  mIndexCount = (UINT)indices.size();

  // Создаем вершинный буфер в GPU
  {
    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mVertexBufferByteSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mVertexBufferGPU)));

    // Создаем промежуточный буфер для загрузки
    D3D12_HEAP_PROPERTIES uploadHeapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mVertexBufferUploader)));

    // Копируем данные
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.data();
    vertexData.RowPitch = mVertexBufferByteSize;
    vertexData.SlicePitch = mVertexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mVertexBufferGPU.Get(),
                       mVertexBufferUploader.Get(), 0, 0, 1, &vertexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mVertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    // Создаем вью вершинного буфера
    mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = mVertexBufferByteSize;
    mVertexBufferView.StrideInBytes = sizeof(Vertex);
  }

  // Создаем индексный буфер в GPU
  {
    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mIndexBufferByteSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mIndexBufferGPU)));

    D3D12_HEAP_PROPERTIES uploadHeapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mIndexBufferUploader)));

    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices.data();
    indexData.RowPitch = mIndexBufferByteSize;
    indexData.SlicePitch = mIndexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mIndexBufferGPU.Get(),
                       mIndexBufferUploader.Get(), 0, 0, 1, &indexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mIndexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    // Создаем вью индексного буфера
    mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = mIndexBufferByteSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
  }
}

void BoxApp::BuildPSO() {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

  psoDesc.InputLayout = {mInputLayout.data(), (UINT)mInputLayout.size()};
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.VS = {reinterpret_cast<BYTE*>(mVSByteCode->GetBufferPointer()),
                mVSByteCode->GetBufferSize()};
  psoDesc.PS = {reinterpret_cast<BYTE*>(mPSByteCode->GetBufferPointer()),
                mPSByteCode->GetBufferSize()};

  CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
  rasterizerDesc.FrontCounterClockwise = FALSE;

  psoDesc.RasterizerState = rasterizerDesc;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = BackBufferFormat;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;
  psoDesc.DSVFormat = DepthStencilFormat;

  ThrowIfFailed(
      mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void BoxApp::CreateDevice() {
  ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

#if defined(DEBUG) || defined(_DEBUG)
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
  }
#endif

  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&mDevice));

  if (FAILED(hr)) {
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(mFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&mDevice)));
  }

  ThrowIfFailed(
      mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

  mRtvDescriptorSize =
      mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  mDsvDescriptorSize =
      mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  mCbvSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void BoxApp::CreateCommandObjects() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

  ThrowIfFailed(mDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));

  ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           mCommandAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&mCommandList)));

  mCommandList->Close();
}

void BoxApp::CreateSwapChain() {
  mSwapChain.Reset();

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferDesc.Width = WIDTH;
  sd.BufferDesc.Height = HEIGHT;
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
      mFactory->CreateSwapChain(mCommandQueue.Get(), &sd, &mSwapChain));
}

void BoxApp::CreateRTVs() {
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart());

  for (UINT i = 0; i < SwapChainBufferCount; i++) {
    ThrowIfFailed(
        mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffers[i])));
    mDevice->CreateRenderTargetView(mSwapChainBuffers[i].Get(), nullptr,
                                    rtvHeapHandle);
    rtvHeapHandle.Offset(1, mRtvDescriptorSize);
  }
}

void BoxApp::CreateDepthStencil() {
  D3D12_RESOURCE_DESC depthStencilDesc = {};
  depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthStencilDesc.Width = WIDTH;
  depthStencilDesc.Height = HEIGHT;
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

  ThrowIfFailed(mDevice->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc,
      D3D12_RESOURCE_STATE_COMMON, &optClear,
      IID_PPV_ARGS(&mDepthStencilBuffer)));

  mDevice->CreateDepthStencilView(
      mDepthStencilBuffer.Get(), nullptr,
      mDsvHeap->GetCPUDescriptorHandleForHeapStart());

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
  mCommandList->ResourceBarrier(1, &barrier);
  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);
  FlushCommandQueue();
}

void BoxApp::Update(const GameTimer& gt) {
  // Преобразуем сферические координаты в декартовы
  float x = mRadius * sinf(mPhi) * cosf(mTheta);
  float z = mRadius * sinf(mPhi) * sinf(mTheta);
  float y = mRadius * cosf(mPhi);

  // Строим матрицу вида
  DirectX::SimpleMath::Vector3 pos(x, y, z);
  DirectX::SimpleMath::Vector3 target(0.0f, 0.0f, 0.0f);
  DirectX::SimpleMath::Vector3 up(0.0f, 1.0f, 0.0f);

  mView = DirectX::SimpleMath::Matrix::CreateLookAt(pos, target, up);

  // Комбинируем матрицы
  DirectX::SimpleMath::Matrix worldViewProj = mWorld * mView * mProj;

  // Обновляем константный буфер объекта
  ObjectConstants objConstants;
  objConstants.World = mWorld;
  objConstants.WorldViewProj = worldViewProj.Transpose();
  mObjectCB->CopyData(0, objConstants);

  // Обновляем константный буфер света
  LightConstants lightConstants;
  // Позиция света (вверху справа)
  lightConstants.LightPosition =
      DirectX::SimpleMath::Vector4(3.0f, 3.0f, 3.0f, 1.0f);
  // Белый свет
  lightConstants.LightColor =
      DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  // Позиция камеры для вычисления viewDir
  lightConstants.CameraPosition = DirectX::SimpleMath::Vector4(x, y, z, 1.0f);
  mLightCB->CopyData(0, lightConstants);
}

void BoxApp::Draw(const GameTimer& gt) {
  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  // Переход заднего буфера в состояние рендер-таргета
  auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
      mSwapChainBuffers[mCurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  mCommandList->ResourceBarrier(1, &barrier1);

  // Очистка буферов
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();

  const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
  mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  mCommandList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);

  // Устанавливаем рендер-таргеты
  mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

  // Устанавливаем дескрипторные кучи
  ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvHeap.Get()};
  mCommandList->SetDescriptorHeaps(1, descriptorHeaps);

  // Устанавливаем корневую сигнатуру
  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  // Привязываем константные буферы: Object (b0) и Light (b1)
  mCommandList->SetGraphicsRootDescriptorTable(
      0,
      mCbvHeap->GetGPUDescriptorHandleForHeapStart());  // b0: ObjectConstants
  mCommandList->SetGraphicsRootDescriptorTable(
      1, CD3DX12_GPU_DESCRIPTOR_HANDLE(
             mCbvHeap->GetGPUDescriptorHandleForHeapStart(), 1,
             mCbvSrvDescriptorSize));  // b1: LightConstants

  // Устанавливаем вершинный и индексный буферы
  mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
  mCommandList->IASetIndexBuffer(&mIndexBufferView);
  mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Рисуем куб
  mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);

  // Переход обратно в PRESENT
  auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      mSwapChainBuffers[mCurrBackBuffer].Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  mCommandList->ResourceBarrier(1, &barrier2);

  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);

  ThrowIfFailed(mSwapChain->Present(1, 0));
  mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

  FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y) {
  mLastMousePos.x = x;
  mLastMousePos.y = y;
  SetCapture(m_window.GetHWND());
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y) {
  if ((btnState & MK_LBUTTON) != 0) {
    // Вращение камеры
    float dx = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(x - mLastMousePos.x));
    float dy = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(y - mLastMousePos.y));

    mTheta += dx;
    mPhi += dy;

    // Ограничиваем угол phi
    mPhi = clamp_val(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
  } else if ((btnState & MK_RBUTTON) != 0) {
    // Приближение/отдаление
    float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
    float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

    mRadius += dx - dy;
    mRadius = clamp_val(mRadius, 3.0f, 15.0f);
  }

  mLastMousePos.x = x;
  mLastMousePos.y = y;
}

int BoxApp::Run() {
  MSG msg = {0};
  mTimer.Reset();

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      // Обработка сообщений мыши
      switch (msg.message) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
          OnMouseDown(msg.wParam, GET_X_LPARAM(msg.lParam),
                      GET_Y_LPARAM(msg.lParam));
          break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
          OnMouseUp(msg.wParam, GET_X_LPARAM(msg.lParam),
                    GET_Y_LPARAM(msg.lParam));
          break;
        case WM_MOUSEMOVE:
          OnMouseMove(msg.wParam, GET_X_LPARAM(msg.lParam),
                      GET_Y_LPARAM(msg.lParam));
          break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      mTimer.Tick();

      if (!m_window.IsPaused()) {
        Update(mTimer);
        Draw(mTimer);
      } else {
        Sleep(100);
      }
    }
  }

  return (int)msg.wParam;
}

void BoxApp::CalculateFrameStats() {
  static int frameCnt = 0;
  static float timeElapsed = 0.0f;

  frameCnt++;

  if ((mTimer.TotalTime() - timeElapsed) >= 1.0f) {
    float fps = (float)frameCnt;
    float mspf = 1000.0f / fps;

    wstring fpsStr = std::to_wstring(fps);
    wstring mspfStr = std::to_wstring(mspf);

    wstring windowText =
        L"Direct3D 12 Box    fps: " + fpsStr + L"   mspf: " + mspfStr;
    SetWindowText(m_window.GetHWND(), windowText.c_str());

    frameCnt = 0;
    timeElapsed += 1.0f;
  }
}

void BoxApp::FlushCommandQueue() {
  mCurrentFence++;

  ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

  if (mFence->GetCompletedValue() < mCurrentFence) {
    HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);

    if (eventHandle) {
      ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
    }
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE BoxApp::CurrentBackBufferView() const {
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer,
      mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE BoxApp::DepthStencilView() const {
  return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

// Запуск с аннотациями SAL
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstance,
                   _In_ LPSTR cmdLine, _In_ int showCmd) {
  (void)prevInstance;
  (void)cmdLine;

#if defined(DEBUG) || defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try {
    BoxApp app(hInstance);
    if (!app.Initialize()) {
      return 1;
    }

    ShowWindow(app.GetMainWnd(), showCmd);
    UpdateWindow(app.GetMainWnd());

    return app.Run();

  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
