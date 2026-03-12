#include "RenderingSystem.h"

void RenderingSystem::Initialize(ID3D12Device* device, UINT width, UINT height,
                                 DXGI_FORMAT depthStencilFormat) {
  mGBuffer = std::make_unique<GBuffer>();
  mGBuffer->Initialize(device, width, height, depthStencilFormat);
}

void RenderingSystem::Resize(ID3D12Device* device, UINT width, UINT height) {
  if (mGBuffer) {
    mGBuffer->Resize(device, width, height);
  }
}

void RenderingSystem::BeginGeometryPass(
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const {
  if (mGBuffer) {
    mGBuffer->BeginGeometryPass(commandList, dsvHandle);
  }
}

void RenderingSystem::EndGeometryPass(
    ID3D12GraphicsCommandList* commandList) const {
  if (mGBuffer) {
    mGBuffer->EndGeometryPass(commandList);
  }
}
