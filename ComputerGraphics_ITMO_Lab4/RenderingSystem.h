#pragma once

#include "Common.h"
#include "GBuffer.h"
#include "Material.h"
#include "ShaderHelper.h"
#include "Structures.h"
#include "UploadBuffer.h"

class RenderingSystem {
 public:
  static constexpr UINT kGBufferRtvStart = SwapChainBufferCount;
  static constexpr UINT kGBufferSrvStart = 2;
  static constexpr UINT kTextureSrvStart = 4;

  void Initialize(ID3D12Device* device, UINT width, UINT height,
                  ID3D12DescriptorHeap* rtvHeap,
                  ID3D12DescriptorHeap* cbvSrvHeap, UINT rtvDescriptorSize,
                  UINT cbvSrvDescriptorSize);

  void Render(ID3D12GraphicsCommandList* cmdList,
              D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
              ID3D12Resource* backBuffer, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
              ID3D12DescriptorHeap* cbvSrvHeap,
              ID3D12DescriptorHeap* samplerHeap, UINT cbvSrvDescriptorSize,
              const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
              const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
              const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
              const ModelGeometry& modelGeometry,
              UploadBuffer<MaterialConstants>* materialCB);

 private:
  void BuildGeometryRootSignature(ID3D12Device* device);
  void BuildComposeRootSignature(ID3D12Device* device);
  void BuildGeometryPSO(ID3D12Device* device);
  void BuildComposePSO(ID3D12Device* device);
  void BuildInputLayout();
  void BuildShaders();

  ComPtr<ID3D12RootSignature> mGeometryRootSignature;
  ComPtr<ID3D12RootSignature> mComposeRootSignature;
  ComPtr<ID3D12PipelineState> mGeometryPSO;
  ComPtr<ID3D12PipelineState> mComposePSO;

  ComPtr<ID3DBlob> mGeometryVS;
  ComPtr<ID3DBlob> mGeometryPS;
  ComPtr<ID3DBlob> mComposeVS;
  ComPtr<ID3DBlob> mComposePS;

  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
  GBuffer mGBuffer;
};
