#pragma once

#include <SimpleMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "D3DWindow.h"
#include "DDSTextureLoader.h"
#include "GameTimer.h"
#include "RenderingSystem.h"
#include "Structures.h"
#include "UploadBuffer.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;
using std::wstring;

// Структура для хранения текстуры
struct Texture {
  std::string name;
  std::wstring filepath;
  ComPtr<ID3D12Resource> Resource = nullptr;
  ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

struct FallingPointLight {
  DirectX::SimpleMath::Vector3 Position;
  DirectX::SimpleMath::Vector3 Color;
  float Intensity = 1.0f;
  float Range = 1.0f;
  float FallSpeed = 1.0f;
  float GroundY = 0.0f;
  float CooldownAfterLanding = 0.0f;
};

class BoxApp {
 public:
  BoxApp(HINSTANCE hInstance);
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
  void BuildBoxGeometry();  // загружает модель, создаёт буферы и текстуры
  void BuildPSO();
  void CreateFallbackCube();  // создаёт куб, если модель не загрузилась

  // Загрузка текстур для всех материалов
  void LoadAllTextures();

  // Создание SRV для конкретной текстуры и размещение в куче по указанному
  // индексу
  void CreateSRV(ComPtr<ID3D12Resource> textureResource, int heapIndex);

  void CreateSamplerHeap();

  void CreateDevice();
  void CreateCommandObjects();
  void CreateSwapChain();
  void CreateRTVs();
  void CreateDepthStencil();
  void FlushCommandQueue();
  void CalculateFrameStats();
  void ResetFallingLight(FallingPointLight& light);

  D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

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
  ComPtr<ID3D12DescriptorHeap>
      mCbvHeap;  // первые 2: object CBV, light CBV, затем SRV текстур
  ComPtr<ID3D12DescriptorHeap> mSamplerHeap;  // отдельный heap для сэмплера
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
  std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB;
  std::unique_ptr<UploadBuffer<LightConstants>> mLightCB;
  std::unique_ptr<UploadBuffer<ComposeConstants>> mComposeCB;
  std::unique_ptr<UploadBuffer<MaterialConstants>> mMaterialCB = nullptr;

  // Вектор всех загруженных текстур
  std::vector<std::unique_ptr<Texture>> mTextures;
  static constexpr int kTextureSrvHeapStart = RenderingSystem::kTextureSrvStart;
  // Входной лейаут
  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  // Дескрипторы
  UINT mRtvDescriptorSize = 0;
  UINT mDsvDescriptorSize = 0;
  UINT mCbvSrvDescriptorSize = 0;

  // Видовые параметры
  D3D12_VIEWPORT mScreenViewport;
  D3D12_RECT mScissorRect;
  UINT64 mCurrentFence = 0;
  int mCurrBackBuffer = 0;

  // Окно и таймер
  D3DWindow m_window;
  GameTimer mTimer;
  RenderingSystem mRenderingSystem;

  // Матрицы
  DirectX::SimpleMath::Matrix mWorld;
  DirectX::SimpleMath::Matrix mView;
  DirectX::SimpleMath::Matrix mProj;

  // фрикам
  DirectX::SimpleMath::Vector3 mCamPos;
  float mCamYaw;
  float mCamPitch;
  float mMoveSpeed;
  float mMouseSensitivity;
  POINT mLastMousePos;

  // Геометрия модели
  ModelGeometry mModelGeometry;
  std::vector<SceneObject> mSceneObjects;
  UINT mVertexBufferByteSize = 0;
  UINT mIndexBufferByteSize = 0;
  D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
  D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
  UINT mIndexCount = 0;

  static constexpr size_t kFallingLightCount = 58;
  std::array<FallingPointLight, kFallingLightCount> mFallingLights;
  std::mt19937 mRandomEngine;
};
