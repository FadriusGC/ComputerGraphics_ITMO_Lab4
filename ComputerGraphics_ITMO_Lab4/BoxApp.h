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

// Ñòðóêòóðà äëÿ õðàíåíèÿ òåêñòóðû
struct Texture {
  std::string name;
  std::wstring filepath;
  ComPtr<ID3D12Resource> Resource = nullptr;
  ComPtr<ID3D12Resource> UploadHeap = nullptr;
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
  void BuildBoxGeometry();  // çàãðóæàåò ìîäåëü, ñîçäà¸ò áóôåðû è òåêñòóðû
  void BuildPSO();
  void CreateFallbackCube();  // ñîçäà¸ò êóá, åñëè ìîäåëü íå çàãðóçèëàñü

  // Çàãðóçêà òåêñòóð äëÿ âñåõ ìàòåðèàëîâ
  void LoadAllTextures();

  // Ñîçäàíèå SRV äëÿ êîíêðåòíîé òåêñòóðû è ðàçìåùåíèå â êó÷å ïî óêàçàííîìó
  // èíäåêñó
  void CreateSRV(ComPtr<ID3D12Resource> textureResource, int heapIndex);

  void CreateSamplerHeap();

  void CreateDevice();
  void CreateCommandObjects();
  void CreateSwapChain();
  void CreateRTVs();
  void CreateDepthStencil();
  void FlushCommandQueue();
  void CalculateFrameStats();

  D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
  D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

  // D3D12 îáúåêòû
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
      mCbvHeap;  // ïåðâûå 2: object CBV, light CBV, çàòåì SRV òåêñòóð
  ComPtr<ID3D12DescriptorHeap> mSamplerHeap;  // îòäåëüíûé heap äëÿ ñýìïëåðà
  ComPtr<ID3D12RootSignature> mRootSignature;
  ComPtr<ID3D12PipelineState> mPSO;
  ComPtr<ID3D12Resource> mSwapChainBuffers[SwapChainBufferCount];
  ComPtr<ID3D12Resource> mDepthStencilBuffer;

  // Ðåñóðñû ãåîìåòðèè
  ComPtr<ID3D12Resource> mVertexBufferGPU;
  ComPtr<ID3D12Resource> mIndexBufferGPU;
  ComPtr<ID3D12Resource> mVertexBufferUploader;
  ComPtr<ID3D12Resource> mIndexBufferUploader;

  // Øåéäåðû
  ComPtr<ID3DBlob> mVSByteCode;
  ComPtr<ID3DBlob> mPSByteCode;

  // Êîíñòàíòíûå áóôåðû
  std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB;
  std::unique_ptr<UploadBuffer<LightConstants>> mLightCB;
  std::unique_ptr<UploadBuffer<MaterialConstants>> mMaterialCB = nullptr;

  // Âåêòîð âñåõ çàãðóæåííûõ òåêñòóð
  std::vector<std::unique_ptr<Texture>> mTextures;

  // Âõîäíîé ëåéàóò
  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  // Äåñêðèïòîðû
  UINT mRtvDescriptorSize = 0;
  UINT mDsvDescriptorSize = 0;
  UINT mCbvSrvDescriptorSize = 0;

  // Âèäîâûå ïàðàìåòðû
  D3D12_VIEWPORT mScreenViewport;
  D3D12_RECT mScissorRect;
  UINT64 mCurrentFence = 0;
  int mCurrBackBuffer = 0;


  std::unique_ptr<RenderingSystem> mRenderingSystem;
  // Îêíî è òàéìåð
  D3DWindow m_window;
  GameTimer mTimer;

  // Ìàòðèöû
  DirectX::SimpleMath::Matrix mWorld;
  DirectX::SimpleMath::Matrix mView;
  DirectX::SimpleMath::Matrix mProj;

  // ôðèêàì
  DirectX::SimpleMath::Vector3 mCamPos; 
  float mCamYaw;                        
  float mCamPitch;                    
  float mMoveSpeed;                   
  float mMouseSensitivity;             
  POINT mLastMousePos;                  

  // Ãåîìåòðèÿ ìîäåëè
  ModelGeometry mModelGeometry;
  UINT mVertexBufferByteSize = 0;
  UINT mIndexBufferByteSize = 0;
  D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
  D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
  UINT mIndexCount = 0;
};
