#pragma once

#include <SimpleMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include "Common.h"
#include "D3DWindow.h"
#include "DDSTextureLoader.h"
#include "GameTimer.h"
#include "RenderingSystem.h"
#include "Structures.h"
#include "UploadBuffer.h"
#include "d3dx12.h"

class ShadowMap {
 public:
  static constexpr UINT kCascadeCount = 3;

  void Initialize(ID3D12Device* device, UINT width, UINT height,
                  ID3D12DescriptorHeap* cbvSrvHeap, UINT srvIndex,
                  UINT cbvSrvDescriptorSize);

  D3D12_VIEWPORT Viewport() const { return mViewport; }
  D3D12_RECT ScissorRect() const { return mScissorRect; }

  ID3D12Resource* Resource() const { return mShadowMap.Get(); }

  D3D12_CPU_DESCRIPTOR_HANDLE Dsv(UINT cascade) const {
    return mDsvHandles[cascade];
  }

  D3D12_GPU_DESCRIPTOR_HANDLE SrvGpuHandle() const { return mSrvGpuHandle; }

 private:
  ComPtr<ID3D12Resource> mShadowMap;
  ComPtr<ID3D12DescriptorHeap> mDsvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE mDsvHandles[kCascadeCount] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mSrvGpuHandle = {};
  D3D12_VIEWPORT mViewport = {};
  D3D12_RECT mScissorRect = {};
};
