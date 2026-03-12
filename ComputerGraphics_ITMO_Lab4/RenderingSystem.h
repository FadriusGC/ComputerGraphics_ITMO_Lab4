#pragma once

#include <memory>

#include "GBuffer.h"

class RenderingSystem {
 public:
  void Initialize(ID3D12Device* device, UINT width, UINT height,
                  DXGI_FORMAT depthStencilFormat);
  void Resize(ID3D12Device* device, UINT width, UINT height);

  void BeginGeometryPass(ID3D12GraphicsCommandList* commandList,
                         D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;
  void EndGeometryPass(ID3D12GraphicsCommandList* commandList) const;

  GBuffer* GetGBuffer() const { return mGBuffer.get(); }

 private:
  std::unique_ptr<GBuffer> mGBuffer;
};
