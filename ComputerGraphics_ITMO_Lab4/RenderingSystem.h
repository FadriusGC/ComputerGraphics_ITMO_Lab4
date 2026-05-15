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
  static constexpr UINT kObjectCbvStart = 5;
  static constexpr UINT kObjectCbvReservedCount = 128;
  static constexpr UINT kDepthSrvIndex =
      kObjectCbvStart + kObjectCbvReservedCount;
  static constexpr UINT kTextureSrvStart = kDepthSrvIndex + 1;
  static constexpr UINT kParticleSmokeSrvIndex = kTextureSrvStart + 255;
  static constexpr UINT kShadowMapSrvIndex = kTextureSrvStart + 257;
  static constexpr UINT kShadowCascadeCount = 4;
  static constexpr UINT kShadowMapResolution = 2048;

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
              const std::vector<SceneObject>& sceneObjects,
              const std::vector<SubmeshInstance>& submeshInstances,
              const std::vector<UINT>& visibleSubmeshInstanceIndices,
              UploadBuffer<MaterialConstants>* materialCB,
              ID3D12Resource* depthBuffer,
              D3D12_GPU_VIRTUAL_ADDRESS composeCBAddress, float deltaTime,
              const DirectX::SimpleMath::Matrix& view,
              const DirectX::SimpleMath::Matrix& proj,
              const DirectX::SimpleMath::Matrix& viewProj,
              const DirectX::SimpleMath::Vector3& cameraPosition);

  const DirectX::SimpleMath::Vector4& GetCascadeSplits() const {
    return mCascadeSplits;
  }

  const std::array<DirectX::SimpleMath::Matrix, kShadowCascadeCount>&
  GetShadowViewProjMatrices() const {
    return mShadowViewProj;
  }

  UINT GetShadowCascadeCount() const { return kShadowCascadeCount; }
  UINT GetShadowMapResolution() const { return kShadowMapResolution; }
  UINT GetShadowsEnabled() const { return mEnableShadows; }

 private:
  void BuildGeometryRootSignature(ID3D12Device* device);
  void BuildComposeRootSignature(ID3D12Device* device);
  void BuildParticlesComputeRootSignature(ID3D12Device* device);
  void BuildParticlesRenderRootSignature(ID3D12Device* device);
  void BuildGeometryPSO(ID3D12Device* device);
  void BuildComposePSO(ID3D12Device* device);
  void BuildParticlesEmitPSO(ID3D12Device* device);
  void BuildParticlesSimulatePSO(ID3D12Device* device);
  void BuildParticlesInitPSO(ID3D12Device* device);
  void BuildParticlesRenderPSO(ID3D12Device* device);
  void BuildInputLayout();
  void BuildShaders();
  void BuildParticleResources(ID3D12Device* device,
                              ID3D12DescriptorHeap* cbvSrvHeap,
                              UINT cbvSrvDescriptorSize);
  void SimulateParticles(ID3D12GraphicsCommandList* cmdList, float deltaTime,
                         const DirectX::SimpleMath::Vector3& cameraPosition);
  void RenderParticles(ID3D12GraphicsCommandList* cmdList);
  void BuildShadowPassResources(ID3D12Device* device,
                                ID3D12DescriptorHeap* cbvSrvHeap,
                                UINT cbvSrvDescriptorSize);
  void UpdateCascadedShadowMapsData(const DirectX::SimpleMath::Matrix& view,
                                    const DirectX::SimpleMath::Matrix& proj);
  void RenderShadowPass(ID3D12GraphicsCommandList* cmdList,
                        const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
                        const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
                        const ModelGeometry& modelGeometry,
                        const std::vector<SceneObject>& sceneObjects,
                        const std::vector<SubmeshInstance>& submeshInstances,
                        const std::vector<UINT>& visibleSubmeshInstanceIndices);

  ComPtr<ID3D12RootSignature> mGeometryRootSignature;
  ComPtr<ID3D12RootSignature> mComposeRootSignature;
  ComPtr<ID3D12RootSignature> mParticlesComputeRootSignature;
  ComPtr<ID3D12RootSignature> mParticlesRenderRootSignature;
  ComPtr<ID3D12PipelineState> mGeometryPSO;
  ComPtr<ID3D12PipelineState> mComposePSO;
  ComPtr<ID3D12PipelineState> mParticlesEmitPSO;
  ComPtr<ID3D12PipelineState> mParticlesSimulatePSO;
  ComPtr<ID3D12PipelineState> mParticlesInitPSO;
  ComPtr<ID3D12PipelineState> mParticlesRenderPSO;
  ComPtr<ID3D12PipelineState> mShadowPSO;

  ComPtr<ID3DBlob> mGeometryVS;
  ComPtr<ID3DBlob> mGeometryPS;
  ComPtr<ID3DBlob> mGeometryHS;
  ComPtr<ID3DBlob> mGeometryDS;
  ComPtr<ID3DBlob> mComposeVS;
  ComPtr<ID3DBlob> mComposePS;
  ComPtr<ID3DBlob> mParticlesEmitCS;
  ComPtr<ID3DBlob> mParticlesInitCS;
  ComPtr<ID3DBlob> mParticlesSimulateCS;
  ComPtr<ID3DBlob> mParticlesVS;
  ComPtr<ID3DBlob> mParticlesGS;
  ComPtr<ID3DBlob> mParticlesPS;
  ComPtr<ID3DBlob> mShadowVS;
  ComPtr<ID3DBlob> mShadowPS;

  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
  GBuffer mGBuffer;

  struct ParticleGpuData {
    DirectX::SimpleMath::Vector3 Position;
    float Age;
    DirectX::SimpleMath::Vector3 Velocity;
    float Lifetime;
    DirectX::SimpleMath::Vector4 Color;
    float Size;
    DirectX::SimpleMath::Vector3 Padding;
  };

  struct ParticleSimConstants {
    float DeltaTime = 0.0f;
    float TotalTime = 0.0f;
    UINT SpawnCount = 0;
    UINT MaxParticles = 0;
    DirectX::SimpleMath::Vector3 EmitterPosition;
    float EmitterSpread = 1.0f;
  };

  struct ParticleRenderConstants {
    DirectX::SimpleMath::Matrix ViewProj;
    DirectX::SimpleMath::Vector3 CameraPosition;
    float BillboardSize = 1.0f;
    UINT MaxParticles = 0;
    float TotalTime = 0.0f;
    DirectX::SimpleMath::Vector2 AtlasGrid = {8.0f, 8.0f};
    float AtlasFps = 24.0f;
    float Padding = 0.0f;
  };

  static constexpr UINT kParticleMaxCount = 16384;
  static constexpr UINT kParticlePoolSrvIndex = kTextureSrvStart + 256;
  static constexpr UINT kParticlePoolUavIndex = kParticlePoolSrvIndex + 1;
  static constexpr UINT kDeadListAUavIndex = kParticlePoolSrvIndex + 2;
  static constexpr UINT kDeadListBUavIndex = kParticlePoolSrvIndex + 3;

  ComPtr<ID3D12Resource> mParticlePoolBuffer;
  ComPtr<ID3D12Resource> mDeadListABuffer;
  ComPtr<ID3D12Resource> mDeadListBBuffer;
  ComPtr<ID3D12Resource> mDeadListBCounterBuffer;
  ComPtr<ID3D12Resource> mDeadListACounterBuffer;
  ComPtr<ID3D12Resource> mParticleSimConstantBuffer;
  ComPtr<ID3D12Resource> mParticleRenderConstantBuffer;
  ComPtr<ID3D12Resource> mParticleCounterResetBuffer;
  ComPtr<ID3D12RootSignature> mShadowRootSignature;
  ComPtr<ID3D12Resource> mShadowMap;
  ComPtr<ID3D12DescriptorHeap> mShadowDsvHeap;
  ComPtr<ID3D12Resource> mShadowFrameCB;
  ParticleSimConstants* mMappedParticleSimConstants = nullptr;
  ParticleRenderConstants* mMappedParticleRenderConstants = nullptr;
  bool mUseDeadListAAsConsume = true;
  bool mParticlesInitialized = false;
  float mParticlesTotalTime = 0.0f;
  UINT mCbvSrvDescriptorSize = 0;
  D3D12_RESOURCE_STATES mParticlePoolState = D3D12_RESOURCE_STATE_COMMON;
  D3D12_CPU_DESCRIPTOR_HANDLE mDeadListAUavCpuHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE mDeadListBUavCpuHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE mParticlePoolUavCpuHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mDeadListAUavGpuHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mDeadListBUavGpuHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mParticlePoolUavGpuHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE mCbvSrvHeapGpuStart = {};
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kShadowCascadeCount>
      mShadowDsvHandles{};
  std::array<DirectX::SimpleMath::Matrix, kShadowCascadeCount>
      mShadowViewProj{};
  DirectX::SimpleMath::Vector4 mCascadeSplits = {0, 0, 0, 0};
  UINT mShadowFrameCbStride = 0;
  UINT mEnableShadows = 1;
};
