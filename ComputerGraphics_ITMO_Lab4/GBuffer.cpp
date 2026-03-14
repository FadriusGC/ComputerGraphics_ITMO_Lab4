#include "GBuffer.h"

void GBuffer::Initialize(ID3D12Device* device, UINT width, UINT height,
                         ID3D12DescriptorHeap* rtvHeap,
                         ID3D12DescriptorHeap* cbvSrvHeap,
                         UINT rtvDescriptorSize, UINT cbvSrvDescriptorSize,
                         UINT rtvStartIndex, UINT srvStartIndex) {
  mRtvHeap = rtvHeap;
  mCbvSrvHeap = cbvSrvHeap;
  mRtvDescriptorSize = rtvDescriptorSize;
  mCbvSrvDescriptorSize = cbvSrvDescriptorSize;
  mRtvStartIndex = rtvStartIndex;
  mSrvStartIndex = srvStartIndex;

  CreateResources(device, width, height);
  CreateDescriptors(device);
}

void GBuffer::Resize(ID3D12Device* device, UINT width, UINT height) {
  CreateResources(device, width, height);
  CreateDescriptors(device);
}

void GBuffer::CreateResources(ID3D12Device* device, UINT width, UINT height) {
  mWidth = width;
  mHeight = height;

  D3D12_CLEAR_VALUE albedoClear = {};
  albedoClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  albedoClear.Color[0] = 0.0f;
  albedoClear.Color[1] = 0.0f;
  albedoClear.Color[2] = 0.0f;
  albedoClear.Color[3] = 1.0f;

  D3D12_CLEAR_VALUE normalClear = {};
  normalClear.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  normalClear.Color[0] = 0.5f;
  normalClear.Color[1] = 0.5f;
  normalClear.Color[2] = 1.0f;
  normalClear.Color[3] = 1.0f;

  auto albedoDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  auto normalDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

  auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

  ThrowIfFailed(device->CreateCommittedResource(
      &defaultHeap, D3D12_HEAP_FLAG_NONE, &albedoDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &albedoClear,
      IID_PPV_ARGS(&mAlbedo)));

  ThrowIfFailed(device->CreateCommittedResource(
      &defaultHeap, D3D12_HEAP_FLAG_NONE, &normalDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &normalClear,
      IID_PPV_ARGS(&mNormal)));
}

void GBuffer::CreateDescriptors(ID3D12Device* device) {
  mRtvHandles[0] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mRtvStartIndex), mRtvDescriptorSize);
  mRtvHandles[1] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mRtvStartIndex + 1), mRtvDescriptorSize);

  mSrvHandles[0] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mSrvStartIndex), mCbvSrvDescriptorSize);
  mSrvHandles[1] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mSrvStartIndex + 1), mCbvSrvDescriptorSize);

  mSrvStartGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
      static_cast<INT>(mSrvStartIndex), mCbvSrvDescriptorSize);

  D3D12_RENDER_TARGET_VIEW_DESC albedoRtv = {};
  albedoRtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  albedoRtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  device->CreateRenderTargetView(mAlbedo.Get(), &albedoRtv, mRtvHandles[0]);

  D3D12_RENDER_TARGET_VIEW_DESC normalRtv = {};
  normalRtv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  normalRtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  device->CreateRenderTargetView(mNormal.Get(), &normalRtv, mRtvHandles[1]);

  D3D12_SHADER_RESOURCE_VIEW_DESC albedoSrv = {};
  albedoSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  albedoSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  albedoSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  albedoSrv.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(mAlbedo.Get(), &albedoSrv, mSrvHandles[0]);

  D3D12_SHADER_RESOURCE_VIEW_DESC normalSrv = {};
  normalSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  normalSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  normalSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  normalSrv.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(mNormal.Get(), &normalSrv, mSrvHandles[1]);
}

void GBuffer::BeginGeometryPass(ID3D12GraphicsCommandList* cmdList,
                                D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const {
  std::array<D3D12_RESOURCE_BARRIER, 2> toRt = {
      CD3DX12_RESOURCE_BARRIER::Transition(
          mAlbedo.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
          D3D12_RESOURCE_STATE_RENDER_TARGET),
      CD3DX12_RESOURCE_BARRIER::Transition(
          mNormal.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
          D3D12_RESOURCE_STATE_RENDER_TARGET)};
  cmdList->ResourceBarrier(static_cast<UINT>(toRt.size()), toRt.data());

  const float albedoClear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float normalClear[4] = {0.5f, 0.5f, 1.0f, 1.0f};

  cmdList->ClearRenderTargetView(mRtvHandles[0], albedoClear, 0, nullptr);
  cmdList->ClearRenderTargetView(mRtvHandles[1], normalClear, 0, nullptr);
  cmdList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);

  cmdList->OMSetRenderTargets(kRenderTargetCount, mRtvHandles, true,
                              &dsvHandle);
}

void GBuffer::EndGeometryPass(ID3D12GraphicsCommandList* cmdList) const {
  std::array<D3D12_RESOURCE_BARRIER, 2> toSrv = {
      CD3DX12_RESOURCE_BARRIER::Transition(
          mAlbedo.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
      CD3DX12_RESOURCE_BARRIER::Transition(
          mNormal.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
  cmdList->ResourceBarrier(static_cast<UINT>(toSrv.size()), toSrv.data());
}
