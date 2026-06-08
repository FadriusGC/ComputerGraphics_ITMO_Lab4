#include "ShadowMap.h"

void ShadowMap::Initialize(ID3D12Device* device, UINT width, UINT height,
                           ID3D12DescriptorHeap* cbvSrvHeap, UINT srvIndex,
                           UINT cbvSrvDescriptorSize) {
  D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R24G8_TYPELESS, width, height, kCascadeCount, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  clearValue.DepthStencil.Depth = 1.0f;
  clearValue.DepthStencil.Stencil = 0;

  auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  ThrowIfFailed(device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
      IID_PPV_ARGS(&mShadowMap)));

  D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
  dsvDesc.NumDescriptors = kCascadeCount;
  dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&mDsvHeap)));

  const UINT dsvSize =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  for (UINT i = 0; i < kCascadeCount; ++i) {
    mDsvHandles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mDsvHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(i),
        dsvSize);
    D3D12_DEPTH_STENCIL_VIEW_DESC v = {};
    v.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    v.Texture2DArray.FirstArraySlice = i;
    v.Texture2DArray.ArraySize = 1;
    v.Texture2DArray.MipSlice = 0;
    device->CreateDepthStencilView(mShadowMap.Get(), &v, mDsvHandles[i]);
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc.Texture2DArray.ArraySize = kCascadeCount;
  srvDesc.Texture2DArray.FirstArraySlice = 0;
  srvDesc.Texture2DArray.MipLevels = 1;

  auto cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(srvIndex), cbvSrvDescriptorSize);
  device->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, cpu);
  mSrvGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
      static_cast<INT>(srvIndex), cbvSrvDescriptorSize);

  mViewport = {
      0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
      0.0f, 1.0f};
  mScissorRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
}
