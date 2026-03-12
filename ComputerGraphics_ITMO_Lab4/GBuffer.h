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

class GBuffer {
 public:
  static constexpr UINT kRenderTargetCount = 2;

  void Initialize(ID3D12Device* device, UINT width, UINT height,
                  ID3D12DescriptorHeap* rtvHeap,
                  ID3D12DescriptorHeap* cbvSrvHeap, UINT rtvDescriptorSize,
                  UINT cbvSrvDescriptorSize, UINT rtvStartIndex,
                  UINT srvStartIndex);

  void Resize(ID3D12Device* device, UINT width, UINT height);

  void BeginGeometryPass(ID3D12GraphicsCommandList* cmdList,
                         D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;
  void EndGeometryPass(ID3D12GraphicsCommandList* cmdList) const;

  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvStartGpuHandle() const {
    return mSrvStartGpuHandle;
  }

 private:
  void CreateResources(ID3D12Device* device, UINT width, UINT height);
  void CreateDescriptors(ID3D12Device* device);

  UINT mWidth = 0;
  UINT mHeight = 0;

  ID3D12DescriptorHeap* mRtvHeap = nullptr;
  ID3D12DescriptorHeap* mCbvSrvHeap = nullptr;
  UINT mRtvDescriptorSize = 0;
  UINT mCbvSrvDescriptorSize = 0;
  UINT mRtvStartIndex = 0;
  UINT mSrvStartIndex = 0;

  ComPtr<ID3D12Resource> mAlbedo;
  ComPtr<ID3D12Resource> mNormal;

  D3D12_CPU_DESCRIPTOR_HANDLE mRtvHandles[kRenderTargetCount] = {};
  D3D12_CPU_DESCRIPTOR_HANDLE mSrvHandles[kRenderTargetCount] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mSrvStartGpuHandle = {};
};
