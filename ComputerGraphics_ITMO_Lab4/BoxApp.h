#pragma once
#pragma once

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

#include "Common.h"
#include "D3DWindow.h"
#include "GameTimer.h"
#include "Structures.h"
#include "UploadBuffer.h"
#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;
using std::wstring;

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
