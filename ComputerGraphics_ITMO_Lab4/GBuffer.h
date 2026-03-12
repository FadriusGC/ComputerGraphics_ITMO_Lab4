#pragma once

#include <array>

#include "Common.h"

class GBuffer {
 public:
  enum class Target : UINT {
    AlbedoSpec = 0,
    Normal = 1,
    Material = 2,
    Count = 3,
  };

  void Initialize(ID3D12Device* device, UINT width, UINT height,
                  DXGI_FORMAT depthStencilFormat);
  void Resize(ID3D12Device* device, UINT width, UINT height);

  void BeginGeometryPass(ID3D12GraphicsCommandList* commandList,
                         D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;
  void EndGeometryPass(ID3D12GraphicsCommandList* commandList) const;

  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle(Target target) const;
  ID3D12DescriptorHeap* GetSrvHeap() const { return mSrvHeap.Get(); }

 private:
  struct TargetInfo {
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    std::array<float, 4> ClearColor{};
  };

  void CreateResources(ID3D12Device* device);
  D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(Target target) const;
  D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCpu(Target target) const;

 private:
  UINT mWidth = 0;
  UINT mHeight = 0;
  DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_UNKNOWN;

  UINT mRtvDescriptorSize = 0;
  UINT mSrvDescriptorSize = 0;

  ComPtr<ID3D12DescriptorHeap> mRtvHeap;
  ComPtr<ID3D12DescriptorHeap> mSrvHeap;

  std::array<TargetInfo, static_cast<size_t>(Target::Count)> mTargetsInfo = {
      TargetInfo{DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f, 0.0f, 0.0f, 1.0f}},
      TargetInfo{DXGI_FORMAT_R16G16B16A16_FLOAT, {0.5f, 0.5f, 1.0f, 0.0f}},
      TargetInfo{DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f, 0.0f, 0.0f, 0.0f}},
  };

  std::array<ComPtr<ID3D12Resource>, static_cast<size_t>(Target::Count)>
      mResources;
};
