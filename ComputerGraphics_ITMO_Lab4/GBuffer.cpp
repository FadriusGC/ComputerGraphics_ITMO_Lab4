#include "GBuffer.h"

namespace {
constexpr UINT kTargetCount = static_cast<UINT>(GBuffer::Target::Count);
}

void GBuffer::Initialize(ID3D12Device* device, UINT width, UINT height,
                         DXGI_FORMAT depthStencilFormat) {
  mWidth = width;
  mHeight = height;
  mDepthStencilFormat = depthStencilFormat;

  mRtvDescriptorSize =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  mSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = kTargetCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = kTargetCount;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(
      device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

  CreateResources(device);
}

void GBuffer::Resize(ID3D12Device* device, UINT width, UINT height) {
  if (mWidth == width && mHeight == height) {
    return;
  }

  mWidth = width;
  mHeight = height;

  for (auto& resource : mResources) {
    resource.Reset();
  }

  CreateResources(device);
}

void GBuffer::CreateResources(ID3D12Device* device) {
  for (UINT i = 0; i < kTargetCount; ++i) {
    const auto& targetInfo = mTargetsInfo[i];

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = mWidth;
    textureDesc.Height = mHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = targetInfo.Format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = targetInfo.Format;
    std::copy(targetInfo.ClearColor.begin(), targetInfo.ClearColor.end(),
              clearValue.Color);

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
        IID_PPV_ARGS(&mResources[i])));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = targetInfo.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    device->CreateRenderTargetView(mResources[i].Get(), &rtvDesc,
                                   GetRtvHandle(static_cast<Target>(i)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = targetInfo.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(mResources[i].Get(), &srvDesc,
                                     GetSrvHandleCpu(static_cast<Target>(i)));
  }
}

void GBuffer::BeginGeometryPass(ID3D12GraphicsCommandList* commandList,
                                D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const {
  std::array<D3D12_RESOURCE_BARRIER, kTargetCount> barriers{};
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kTargetCount> rtvs{};

  for (UINT i = 0; i < kTargetCount; ++i) {
    barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
        mResources[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    rtvs[i] = GetRtvHandle(static_cast<Target>(i));
  }

  commandList->ResourceBarrier(kTargetCount, barriers.data());

  for (UINT i = 0; i < kTargetCount; ++i) {
    commandList->ClearRenderTargetView(rtvs[i], mTargetsInfo[i].ClearColor.data(),
                                       0, nullptr);
  }

  commandList->OMSetRenderTargets(kTargetCount, rtvs.data(), false, &dsvHandle);

  commandList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);
}

void GBuffer::EndGeometryPass(ID3D12GraphicsCommandList* commandList) const {
  std::array<D3D12_RESOURCE_BARRIER, kTargetCount> barriers{};

  for (UINT i = 0; i < kTargetCount; ++i) {
    barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
        mResources[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }

  commandList->ResourceBarrier(kTargetCount, barriers.data());
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetSrvHandle(Target target) const {
  CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
      mSrvHeap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(target),
      mSrvDescriptorSize);

  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetRtvHandle(Target target) const {
  CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(target),
      mRtvDescriptorSize);

  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::GetSrvHandleCpu(Target target) const {
  CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
      mSrvHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(target),
      mSrvDescriptorSize);

  return handle;
}
