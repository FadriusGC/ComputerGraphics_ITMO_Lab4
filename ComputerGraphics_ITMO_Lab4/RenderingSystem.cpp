#include "RenderingSystem.h"

#include <cfloat>
#include <cmath>

void RenderingSystem::Initialize(ID3D12Device* device, UINT width, UINT height,
                                 ID3D12DescriptorHeap* rtvHeap,
                                 ID3D12DescriptorHeap* cbvSrvHeap,
                                 UINT rtvDescriptorSize,
                                 UINT cbvSrvDescriptorSize) {
  mCbvSrvDescriptorSize = cbvSrvDescriptorSize;
  BuildShaders();
  BuildInputLayout();
  BuildGeometryRootSignature(device);
  BuildComposeRootSignature(device);
  BuildParticlesComputeRootSignature(device);
  BuildParticlesRenderRootSignature(device);
  BuildGeometryPSO(device);
  BuildComposePSO(device);
  BuildParticlesEmitPSO(device);
  BuildParticlesInitPSO(device);
  BuildParticlesSimulatePSO(device);
  BuildParticlesRenderPSO(device);

  BuildShadowPassResources(device, cbvSrvHeap, cbvSrvDescriptorSize);

  mGBuffer.Initialize(device, width, height, rtvHeap, cbvSrvHeap,
                      rtvDescriptorSize, cbvSrvDescriptorSize, kGBufferRtvStart,
                      kGBufferSrvStart);
  BuildParticleResources(device, cbvSrvHeap, cbvSrvDescriptorSize);
}

void RenderingSystem::BuildShaders() {
  mGeometryVS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredGeometryVS.hlsl",
      "VS", "vs_5_0");
  mGeometryPS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredGeometryPS.hlsl",
      "PS", "ps_5_0");
  mGeometryHS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredGeometryHS.hlsl",
      "HS", "hs_5_0");
  mGeometryDS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredGeometryDS.hlsl",
      "DS", "ds_5_0");
  mComposeVS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredComposeVS.hlsl",
      "VS", "vs_5_0");
  mComposePS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredComposePS.hlsl",
      "PS", "ps_5_0");
  mParticlesEmitCS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticleEmitCS.hlsl",
      "CS", "cs_5_0");
  mParticlesInitCS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticleInitCS.hlsl",
      "CS", "cs_5_0");
  mParticlesSimulateCS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticleSimulateCS.hlsl",
      "CS", "cs_5_0");
  mParticlesVS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticleVS.hlsl",
      "VS", "vs_5_0");
  mParticlesGS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticleGS.hlsl",
      "GS", "gs_5_0");
  mParticlesPS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ParticlePS.hlsl",
      "PS", "ps_5_0");
  mShadowVS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ShadowMap.hlsl",
      "VSMain", "vs_5_0");
  mShadowPS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/ShadowMap.hlsl",
      "PSMain", "ps_5_0");
}

void RenderingSystem::BuildInputLayout() {
  mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 60,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void RenderingSystem::BuildGeometryRootSignature(ID3D12Device* device) {
  CD3DX12_ROOT_PARAMETER params[7];

  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  params[0].InitAsDescriptorTable(1, &cbvTable0);

  CD3DX12_DESCRIPTOR_RANGE diffuseSrvTable;
  diffuseSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  params[1].InitAsDescriptorTable(1, &diffuseSrvTable);

  CD3DX12_DESCRIPTOR_RANGE normalSrvTable;
  normalSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  params[2].InitAsDescriptorTable(1, &normalSrvTable);

  CD3DX12_DESCRIPTOR_RANGE displacementSrvTable;
  displacementSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
  params[3].InitAsDescriptorTable(1, &displacementSrvTable);

  CD3DX12_DESCRIPTOR_RANGE roughnessSrvTable;
  roughnessSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
  params[4].InitAsDescriptorTable(1, &roughnessSrvTable);

  CD3DX12_DESCRIPTOR_RANGE samplerTable;
  samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
  params[5].InitAsDescriptorTable(1, &samplerTable);

  params[6].InitAsConstantBufferView(2);

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      7, params, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &error));
  ThrowIfFailed(device->CreateRootSignature(
      0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
      IID_PPV_ARGS(&mGeometryRootSignature)));
}

void RenderingSystem::BuildComposeRootSignature(ID3D12Device* device) {
  CD3DX12_ROOT_PARAMETER params[3];

  CD3DX12_DESCRIPTOR_RANGE gbufferSrvTable;
  gbufferSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
  params[0].InitAsDescriptorTable(1, &gbufferSrvTable);

  CD3DX12_DESCRIPTOR_RANGE shadowSrvTable;
  shadowSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
  params[1].InitAsDescriptorTable(1, &shadowSrvTable);

  params[2].InitAsConstantBufferView(0);

  CD3DX12_STATIC_SAMPLER_DESC samplers[2] = {
      CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT),
      CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f, 16,
                                  D3D12_COMPARISON_FUNC_LESS_EQUAL,
                                  D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)};

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      3, params, 2, samplers,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &error));
  ThrowIfFailed(device->CreateRootSignature(
      0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
      IID_PPV_ARGS(&mComposeRootSignature)));
}

void RenderingSystem::BuildParticlesComputeRootSignature(ID3D12Device* device) {
  CD3DX12_ROOT_PARAMETER params[4];
  CD3DX12_DESCRIPTOR_RANGE particlePoolUavRange;
  particlePoolUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
  params[0].InitAsDescriptorTable(1, &particlePoolUavRange);  // u0

  CD3DX12_DESCRIPTOR_RANGE deadAppendUavRange;
  deadAppendUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
  params[1].InitAsDescriptorTable(1, &deadAppendUavRange);  // u1

  CD3DX12_DESCRIPTOR_RANGE deadConsumeUavRange;
  deadConsumeUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
  params[2].InitAsDescriptorTable(1, &deadConsumeUavRange);  // u2

  params[3].InitAsConstantBufferView(0);  // b0

  CD3DX12_ROOT_SIGNATURE_DESC desc(4, params, 0, nullptr,
                                   D3D12_ROOT_SIGNATURE_FLAG_NONE);

  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &error));
  ThrowIfFailed(device->CreateRootSignature(
      0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
      IID_PPV_ARGS(&mParticlesComputeRootSignature)));
}

void RenderingSystem::BuildParticlesRenderRootSignature(ID3D12Device* device) {
  CD3DX12_ROOT_PARAMETER params[3];
  CD3DX12_DESCRIPTOR_RANGE smokeAtlasSrvRange;
  smokeAtlasSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  params[0].InitAsShaderResourceView(0);
  params[1].InitAsConstantBufferView(0);
  params[2].InitAsDescriptorTable(1, &smokeAtlasSrvRange);

  CD3DX12_STATIC_SAMPLER_DESC samplers[2] = {
      CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT),
      CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER,
                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f, 16,
                                  D3D12_COMPARISON_FUNC_LESS_EQUAL,
                                  D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)};

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      3, params, 2, samplers,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &error));
  ThrowIfFailed(device->CreateRootSignature(
      0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
      IID_PPV_ARGS(&mParticlesRenderRootSignature)));
}

void RenderingSystem::BuildGeometryPSO(ID3D12Device* device) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.InputLayout = {mInputLayout.data(),
                     static_cast<UINT>(mInputLayout.size())};
  pso.pRootSignature = mGeometryRootSignature.Get();
  pso.VS = {reinterpret_cast<BYTE*>(mGeometryVS->GetBufferPointer()),
            mGeometryVS->GetBufferSize()};
  pso.PS = {reinterpret_cast<BYTE*>(mGeometryPS->GetBufferPointer()),
            mGeometryPS->GetBufferSize()};
  pso.HS = {reinterpret_cast<BYTE*>(mGeometryHS->GetBufferPointer()),
            mGeometryHS->GetBufferSize()};
  pso.DS = {reinterpret_cast<BYTE*>(mGeometryDS->GetBufferPointer()),
            mGeometryDS->GetBufferSize()};

  CD3DX12_RASTERIZER_DESC rast(D3D12_DEFAULT);
  rast.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState = rast;
  pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  pso.SampleMask = UINT_MAX;
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
  pso.NumRenderTargets = 2;
  pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pso.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
  pso.DSVFormat = DepthStencilFormat;
  pso.SampleDesc.Count = 1;

  ThrowIfFailed(
      device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mGeometryPSO)));
}

void RenderingSystem::BuildComposePSO(ID3D12Device* device) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.InputLayout = {nullptr, 0};
  pso.pRootSignature = mComposeRootSignature.Get();
  pso.VS = {reinterpret_cast<BYTE*>(mComposeVS->GetBufferPointer()),
            mComposeVS->GetBufferSize()};
  pso.PS = {reinterpret_cast<BYTE*>(mComposePS->GetBufferPointer()),
            mComposePS->GetBufferSize()};

  CD3DX12_RASTERIZER_DESC rast(D3D12_DEFAULT);
  rast.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState = rast;
  pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

  auto depthState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  depthState.DepthEnable = false;
  depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  pso.DepthStencilState = depthState;

  pso.SampleMask = UINT_MAX;
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = BackBufferFormat;
  pso.DSVFormat = DepthStencilFormat;
  pso.SampleDesc.Count = 1;

  ThrowIfFailed(
      device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mComposePSO)));

  CD3DX12_ROOT_PARAMETER shadowParams[2];
  CD3DX12_DESCRIPTOR_RANGE shadowObjectCbvRange;
  shadowObjectCbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  shadowParams[0].InitAsDescriptorTable(1, &shadowObjectCbvRange);
  shadowParams[1].InitAsConstantBufferView(1);

  CD3DX12_ROOT_SIGNATURE_DESC shadowRsDesc(
      2, shadowParams, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> shadowSerialized;
  ComPtr<ID3DBlob> shadowErrors;
  ThrowIfFailed(D3D12SerializeRootSignature(&shadowRsDesc,
                                            D3D_ROOT_SIGNATURE_VERSION_1,
                                            &shadowSerialized, &shadowErrors));
  ThrowIfFailed(device->CreateRootSignature(
      0, shadowSerialized->GetBufferPointer(),
      shadowSerialized->GetBufferSize(), IID_PPV_ARGS(&mShadowRootSignature)));

  D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPso = {};
  shadowPso.InputLayout = {mInputLayout.data(),
                           static_cast<UINT>(mInputLayout.size())};
  shadowPso.pRootSignature = mShadowRootSignature.Get();
  shadowPso.VS = {reinterpret_cast<BYTE*>(mShadowVS->GetBufferPointer()),
                  mShadowVS->GetBufferSize()};
  shadowPso.PS = {reinterpret_cast<BYTE*>(mShadowPS->GetBufferPointer()),
                  mShadowPS->GetBufferSize()};
  shadowPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  shadowPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  shadowPso.RasterizerState.DepthBias = 100;
  shadowPso.RasterizerState.SlopeScaledDepthBias = 0.00001f;
  shadowPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  shadowPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  shadowPso.SampleMask = UINT_MAX;
  shadowPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  shadowPso.NumRenderTargets = 0;
  shadowPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  shadowPso.SampleDesc.Count = 1;
  ThrowIfFailed(device->CreateGraphicsPipelineState(&shadowPso,
                                                    IID_PPV_ARGS(&mShadowPSO)));
}

void RenderingSystem::BuildParticlesEmitPSO(ID3D12Device* device) {
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
  desc.pRootSignature = mParticlesComputeRootSignature.Get();
  desc.CS = {reinterpret_cast<BYTE*>(mParticlesEmitCS->GetBufferPointer()),
             mParticlesEmitCS->GetBufferSize()};
  ThrowIfFailed(device->CreateComputePipelineState(
      &desc, IID_PPV_ARGS(&mParticlesEmitPSO)));
}

void RenderingSystem::BuildParticlesSimulatePSO(ID3D12Device* device) {
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
  desc.pRootSignature = mParticlesComputeRootSignature.Get();
  desc.CS = {reinterpret_cast<BYTE*>(mParticlesSimulateCS->GetBufferPointer()),
             mParticlesSimulateCS->GetBufferSize()};
  ThrowIfFailed(device->CreateComputePipelineState(
      &desc, IID_PPV_ARGS(&mParticlesSimulatePSO)));
}

void RenderingSystem::BuildParticlesInitPSO(ID3D12Device* device) {
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
  desc.pRootSignature = mParticlesComputeRootSignature.Get();
  desc.CS = {reinterpret_cast<BYTE*>(mParticlesInitCS->GetBufferPointer()),
             mParticlesInitCS->GetBufferSize()};
  ThrowIfFailed(device->CreateComputePipelineState(
      &desc, IID_PPV_ARGS(&mParticlesInitPSO)));
}

void RenderingSystem::BuildParticlesRenderPSO(ID3D12Device* device) {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.InputLayout = {nullptr, 0};
  pso.pRootSignature = mParticlesRenderRootSignature.Get();
  pso.VS = {reinterpret_cast<BYTE*>(mParticlesVS->GetBufferPointer()),
            mParticlesVS->GetBufferSize()};
  pso.GS = {reinterpret_cast<BYTE*>(mParticlesGS->GetBufferPointer()),
            mParticlesGS->GetBufferSize()};
  pso.PS = {reinterpret_cast<BYTE*>(mParticlesPS->GetBufferPointer()),
            mParticlesPS->GetBufferSize()};

  pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  pso.DepthStencilState.DepthEnable = true;
  pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  pso.SampleMask = UINT_MAX;
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = BackBufferFormat;
  pso.DSVFormat = DepthStencilFormat;
  pso.SampleDesc.Count = 1;

  ThrowIfFailed(device->CreateGraphicsPipelineState(
      &pso, IID_PPV_ARGS(&mParticlesRenderPSO)));
}

void RenderingSystem::BuildParticleResources(ID3D12Device* device,
                                             ID3D12DescriptorHeap* cbvSrvHeap,
                                             UINT cbvSrvDescriptorSize) {
  const UINT particleStride = sizeof(ParticleGpuData);
  const UINT deadListStride = sizeof(UINT);
  const UINT particlePoolSize = kParticleMaxCount * particleStride;
  const UINT deadListSize = kParticleMaxCount * deadListStride;

  auto createDefaultBuffer =
      [&](UINT64 size, D3D12_RESOURCE_STATES initialState,
          D3D12_RESOURCE_FLAGS flags, ComPtr<ID3D12Resource>& buffer) {
        const CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        const CD3DX12_RESOURCE_DESC bufferDesc =
            CD3DX12_RESOURCE_DESC::Buffer(size, flags);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, initialState,
            nullptr, IID_PPV_ARGS(&buffer)));
      };

  createDefaultBuffer(particlePoolSize, D3D12_RESOURCE_STATE_COMMON,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      mParticlePoolBuffer);
  createDefaultBuffer(deadListSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      mDeadListABuffer);
  createDefaultBuffer(deadListSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      mDeadListBBuffer);
  createDefaultBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      mDeadListACounterBuffer);
  createDefaultBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      mDeadListBCounterBuffer);

  const CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC simCbDesc =
      CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleSimConstants));
  const CD3DX12_RESOURCE_DESC renderCbDesc =
      CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleRenderConstants));
  ThrowIfFailed(device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &simCbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mParticleSimConstantBuffer)));
  ThrowIfFailed(device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &renderCbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mParticleRenderConstantBuffer)));
  const CD3DX12_RESOURCE_DESC counterResetDesc =
      CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT));
  ThrowIfFailed(device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &counterResetDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mParticleCounterResetBuffer)));
  ThrowIfFailed(mParticleSimConstantBuffer->Map(
      0, nullptr, reinterpret_cast<void**>(&mMappedParticleSimConstants)));
  ThrowIfFailed(mParticleRenderConstantBuffer->Map(
      0, nullptr, reinterpret_cast<void**>(&mMappedParticleRenderConstants)));
  UINT* mappedCounterReset = nullptr;
  ThrowIfFailed(mParticleCounterResetBuffer->Map(
      0, nullptr, reinterpret_cast<void**>(&mappedCounterReset)));
  mappedCounterReset[0] = 0u;
  mParticleCounterResetBuffer->Unmap(0, nullptr);

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpuStart(
      cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

  D3D12_SHADER_RESOURCE_VIEW_DESC particleSrvDesc = {};
  particleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  particleSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  particleSrvDesc.Buffer.FirstElement = 0;
  particleSrvDesc.Buffer.NumElements = kParticleMaxCount;
  particleSrvDesc.Buffer.StructureByteStride = particleStride;
  particleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
  CD3DX12_CPU_DESCRIPTOR_HANDLE particleSrvCpu(cpuStart, kParticlePoolSrvIndex,
                                               cbvSrvDescriptorSize);
  device->CreateShaderResourceView(mParticlePoolBuffer.Get(), &particleSrvDesc,
                                   particleSrvCpu);

  D3D12_UNORDERED_ACCESS_VIEW_DESC particlePoolUavDesc = {};
  particlePoolUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  particlePoolUavDesc.Buffer.FirstElement = 0;
  particlePoolUavDesc.Buffer.NumElements = kParticleMaxCount;
  particlePoolUavDesc.Buffer.StructureByteStride = particleStride;
  particlePoolUavDesc.Format = DXGI_FORMAT_UNKNOWN;
  mParticlePoolUavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cpuStart, kParticlePoolUavIndex, cbvSrvDescriptorSize);
  mParticlePoolUavGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), kParticlePoolUavIndex,
      cbvSrvDescriptorSize);
  device->CreateUnorderedAccessView(mParticlePoolBuffer.Get(), nullptr,
                                    &particlePoolUavDesc,
                                    mParticlePoolUavCpuHandle);

  D3D12_UNORDERED_ACCESS_VIEW_DESC deadUavDesc = {};
  deadUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  deadUavDesc.Buffer.FirstElement = 0;
  deadUavDesc.Buffer.NumElements = kParticleMaxCount;
  deadUavDesc.Buffer.StructureByteStride = deadListStride;
  deadUavDesc.Format = DXGI_FORMAT_UNKNOWN;
  device->CreateUnorderedAccessView(
      mDeadListABuffer.Get(), mDeadListACounterBuffer.Get(), &deadUavDesc,
      CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, kDeadListAUavIndex,
                                    cbvSrvDescriptorSize));
  mDeadListAUavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cpuStart, kDeadListAUavIndex, cbvSrvDescriptorSize);
  mDeadListAUavGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), kDeadListAUavIndex,
      cbvSrvDescriptorSize);
  device->CreateUnorderedAccessView(
      mDeadListBBuffer.Get(), mDeadListBCounterBuffer.Get(), &deadUavDesc,
      CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, kDeadListBUavIndex,
                                    cbvSrvDescriptorSize));
  mDeadListBUavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      cpuStart, kDeadListBUavIndex, cbvSrvDescriptorSize);
  mDeadListBUavGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), kDeadListBUavIndex,
      cbvSrvDescriptorSize);
}

void RenderingSystem::SimulateParticles(
    ID3D12GraphicsCommandList* cmdList, float deltaTime,
    const DirectX::SimpleMath::Vector3& cameraPosition) {
  mParticlesTotalTime += deltaTime;
  mMappedParticleSimConstants->DeltaTime = deltaTime;
  mMappedParticleSimConstants->TotalTime = mParticlesTotalTime;
  mMappedParticleSimConstants->SpawnCount = 96;
  mMappedParticleSimConstants->MaxParticles = kParticleMaxCount;
  mMappedParticleSimConstants->EmitterPosition = cameraPosition;
  mMappedParticleSimConstants->EmitterSpread = 8.0f;

  auto appendDeadUavGpu =
      mUseDeadListAAsConsume ? mDeadListBUavGpuHandle : mDeadListAUavGpuHandle;

  auto toCompute = CD3DX12_RESOURCE_BARRIER::Transition(
      mParticlePoolBuffer.Get(), mParticlePoolState,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  cmdList->ResourceBarrier(1, &toCompute);
  mParticlePoolState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

  auto resetCounter = [&](ID3D12Resource* counterBuffer) {
    auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        counterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &toCopyDest);
    cmdList->CopyBufferRegion(
        counterBuffer, 0, mParticleCounterResetBuffer.Get(), 0, sizeof(UINT));
    auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
        counterBuffer, D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->ResourceBarrier(1, &toUav);
  };

  if (!mParticlesInitialized) {
    cmdList->SetPipelineState(mParticlesInitPSO.Get());
    cmdList->SetComputeRootSignature(mParticlesComputeRootSignature.Get());
    cmdList->SetComputeRootDescriptorTable(0, mParticlePoolUavGpuHandle);
    cmdList->SetComputeRootDescriptorTable(1, mDeadListAUavGpuHandle);
    cmdList->SetComputeRootDescriptorTable(2, mDeadListAUavGpuHandle);
    cmdList->SetComputeRootConstantBufferView(
        3, mParticleSimConstantBuffer->GetGPUVirtualAddress());
    resetCounter(mDeadListACounterBuffer.Get());
    resetCounter(mDeadListBCounterBuffer.Get());
    cmdList->Dispatch((kParticleMaxCount + 127) / 128, 1, 1);
    auto initBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    cmdList->ResourceBarrier(1, &initBarrier);
    mParticlesInitialized = true;
    mUseDeadListAAsConsume = true;
    appendDeadUavGpu = mDeadListBUavGpuHandle;
  }

  resetCounter(mUseDeadListAAsConsume ? mDeadListBCounterBuffer.Get()
                                      : mDeadListACounterBuffer.Get());

  cmdList->SetPipelineState(mParticlesEmitPSO.Get());
  cmdList->SetComputeRootSignature(mParticlesComputeRootSignature.Get());
  cmdList->SetComputeRootDescriptorTable(0, mParticlePoolUavGpuHandle);
  cmdList->SetComputeRootDescriptorTable(1, appendDeadUavGpu);
  cmdList->SetComputeRootDescriptorTable(2, mUseDeadListAAsConsume
                                                ? mDeadListAUavGpuHandle
                                                : mDeadListBUavGpuHandle);
  cmdList->SetComputeRootConstantBufferView(
      3, mParticleSimConstantBuffer->GetGPUVirtualAddress());
  cmdList->Dispatch((mMappedParticleSimConstants->SpawnCount + 63) / 64, 1, 1);

  auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  cmdList->ResourceBarrier(1, &uavBarrier);

  cmdList->SetPipelineState(mParticlesSimulatePSO.Get());
  cmdList->SetComputeRootDescriptorTable(0, mParticlePoolUavGpuHandle);
  cmdList->SetComputeRootDescriptorTable(1, appendDeadUavGpu);
  cmdList->SetComputeRootDescriptorTable(2, mUseDeadListAAsConsume
                                                ? mDeadListAUavGpuHandle
                                                : mDeadListBUavGpuHandle);
  cmdList->Dispatch((kParticleMaxCount + 127) / 128, 1, 1);
  cmdList->ResourceBarrier(1, &uavBarrier);

  auto toRender = CD3DX12_RESOURCE_BARRIER::Transition(
      mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  cmdList->ResourceBarrier(1, &toRender);
  mParticlePoolState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

  mUseDeadListAAsConsume = !mUseDeadListAAsConsume;
}

void RenderingSystem::RenderParticles(ID3D12GraphicsCommandList* cmdList) {
  cmdList->SetPipelineState(mParticlesRenderPSO.Get());
  cmdList->SetGraphicsRootSignature(mParticlesRenderRootSignature.Get());
  cmdList->SetGraphicsRootShaderResourceView(
      0, mParticlePoolBuffer->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, mParticleRenderConstantBuffer->GetGPUVirtualAddress());
  CD3DX12_GPU_DESCRIPTOR_HANDLE smokeTextureHandle(
      mCbvSrvHeapGpuStart, static_cast<INT>(kParticleSmokeSrvIndex),
      mCbvSrvDescriptorSize);
  cmdList->SetGraphicsRootDescriptorTable(2, smokeTextureHandle);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
  cmdList->DrawInstanced(kParticleMaxCount, 1, 0, 0);
}

void RenderingSystem::Render(
    ID3D12GraphicsCommandList* cmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv, ID3D12Resource* backBuffer,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, ID3D12DescriptorHeap* cbvSrvHeap,
    ID3D12DescriptorHeap* samplerHeap, UINT cbvSrvDescriptorSize,
    const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    const ModelGeometry& modelGeometry,
    const std::vector<SceneObject>& sceneObjects,
    const std::vector<SubmeshInstance>& submeshInstances,
    const std::vector<UINT>& visibleSubmeshInstanceIndices,
    UploadBuffer<MaterialConstants>* materialCB, ID3D12Resource* depthBuffer,
    D3D12_GPU_VIRTUAL_ADDRESS composeCBAddress, float deltaTime,
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj,
    const DirectX::SimpleMath::Matrix& viewProj,
    const DirectX::SimpleMath::Vector3& cameraPosition) {
  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissorRect);

  ID3D12DescriptorHeap* heaps[] = {cbvSrvHeap, samplerHeap};
  cmdList->SetDescriptorHeaps(2, heaps);
  mCbvSrvHeapGpuStart = cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
  mMappedParticleRenderConstants->CameraPosition = cameraPosition;
  mMappedParticleRenderConstants->BillboardSize = 0.55f;
  mMappedParticleRenderConstants->MaxParticles = kParticleMaxCount;
  mMappedParticleRenderConstants->TotalTime = mParticlesTotalTime;
  mMappedParticleRenderConstants->AtlasGrid = {8.0f, 8.0f};
  mMappedParticleRenderConstants->AtlasFps = 24.0f;
  mMappedParticleRenderConstants->ViewProj = viewProj.Transpose();

  SimulateParticles(cmdList, deltaTime, cameraPosition);

  UpdateCascadedShadowMapsData(view, proj);
  RenderShadowPass(cmdList, vertexBufferView, indexBufferView, modelGeometry,
                   sceneObjects, submeshInstances,
                   visibleSubmeshInstanceIndices);
  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissorRect);

  cmdList->SetPipelineState(mGeometryPSO.Get());
  cmdList->SetGraphicsRootSignature(mGeometryRootSignature.Get());

  mGBuffer.BeginGeometryPass(cmdList, dsvHandle);

  cmdList->SetGraphicsRootDescriptorTable(
      5, samplerHeap->GetGPUDescriptorHandleForHeapStart());

  cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
  cmdList->IASetIndexBuffer(&indexBufferView);
  cmdList->IASetPrimitiveTopology(
      D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

  const UINT cbMaterialSize = (sizeof(MaterialConstants) + 255) & ~255;

  if (!modelGeometry.Materials.empty() &&
      modelGeometry.Materials[0].DiffuseTextureIndex >= 0) {
    CD3DX12_GPU_DESCRIPTOR_HANDLE defaultTextureHandle(
        cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(kTextureSrvStart +
                         modelGeometry.Materials[0].DiffuseTextureIndex),
        cbvSrvDescriptorSize);
    cmdList->SetGraphicsRootDescriptorTable(1, defaultTextureHandle);
    cmdList->SetGraphicsRootDescriptorTable(2, defaultTextureHandle);
    cmdList->SetGraphicsRootDescriptorTable(3, defaultTextureHandle);
    cmdList->SetGraphicsRootDescriptorTable(4, defaultTextureHandle);
  }

  for (UINT visibleInstanceIndex : visibleSubmeshInstanceIndices) {
    if (visibleInstanceIndex >= submeshInstances.size()) {
      continue;
    }

    const SubmeshInstance& submeshInstance =
        submeshInstances[visibleInstanceIndex];
    const UINT objectIndex = submeshInstance.ObjectIndex;
    const UINT submeshIndex = submeshInstance.SubmeshIndex;
    if (objectIndex >= sceneObjects.size() ||
        submeshIndex >= modelGeometry.Submeshes.size()) {
      continue;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE objectCbHandle(
        cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(kObjectCbvStart + objectIndex), cbvSrvDescriptorSize);
    cmdList->SetGraphicsRootDescriptorTable(0, objectCbHandle);

    const auto& submesh = modelGeometry.Submeshes[submeshIndex];

    float distanceToCamera =
        (submeshInstance.WorldBounds.Center - cameraPosition).Length();
    const DirectX::SimpleMath::Vector4 lodDistances =
        sceneObjects[objectIndex].LodDistances;
    UINT lodLevel = 0;
    if (distanceToCamera >= lodDistances.y) {
      lodLevel = 2;
    } else if (distanceToCamera >= lodDistances.x) {
      lodLevel = 1;
    }

    const UINT lodIndexCount = submesh.LodIndexCount[lodLevel] > 0
                                   ? submesh.LodIndexCount[lodLevel]
                                   : submesh.IndexCount;
    const UINT lodStartIndexLocation =
        submesh.LodIndexCount[lodLevel] > 0
            ? submesh.LodStartIndexLocation[lodLevel]
            : submesh.StartIndexLocation;

    if (submesh.MaterialIndex < modelGeometry.Materials.size()) {
      const auto& mat = modelGeometry.Materials[submesh.MaterialIndex];

      if (mat.DiffuseTextureIndex >= 0) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseHandle(
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(kTextureSrvStart + mat.DiffuseTextureIndex),
            cbvSrvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(1, diffuseHandle);
      }

      if (mat.NormalTextureIndex >= 0) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle(
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(kTextureSrvStart + mat.NormalTextureIndex),
            cbvSrvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(2, normalHandle);
      }

      if (mat.DisplacementTextureIndex >= 0) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE displacementHandle(
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(kTextureSrvStart + mat.DisplacementTextureIndex),
            cbvSrvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(3, displacementHandle);
      }

      if (mat.RoughnessTextureIndex >= 0) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE roughnessHandle(
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(kTextureSrvStart + mat.RoughnessTextureIndex),
            cbvSrvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(4, roughnessHandle);
      }

      D3D12_GPU_VIRTUAL_ADDRESS matCBAddress =
          materialCB->Resource()->GetGPUVirtualAddress() +
          static_cast<UINT64>(mat.MatCBIndex) * cbMaterialSize;
      cmdList->SetGraphicsRootConstantBufferView(6, matCBAddress);
    }

    cmdList->DrawIndexedInstanced(lodIndexCount, 1, lodStartIndexLocation, 0,
                                  0);
  }

  mGBuffer.EndGeometryPass(cmdList);

  auto depthToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
      depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cmdList->ResourceBarrier(1, &depthToSrv);

  auto toBackBuffer = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  cmdList->ResourceBarrier(1, &toBackBuffer);

  const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
  cmdList->ClearRenderTargetView(backBufferRtv, clearColor, 0, nullptr);
  cmdList->OMSetRenderTargets(1, &backBufferRtv, true, nullptr);

  cmdList->SetPipelineState(mComposePSO.Get());
  cmdList->SetGraphicsRootSignature(mComposeRootSignature.Get());
  cmdList->SetGraphicsRootDescriptorTable(0, mGBuffer.GetSrvStartGpuHandle());
  CD3DX12_GPU_DESCRIPTOR_HANDLE shadowSrvGpu(
      mCbvSrvHeapGpuStart, static_cast<INT>(kShadowMapSrvIndex),
      mCbvSrvDescriptorSize);
  cmdList->SetGraphicsRootDescriptorTable(1, shadowSrvGpu);
  cmdList->SetGraphicsRootConstantBufferView(2, composeCBAddress);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);

  cmdList->OMSetRenderTargets(1, &backBufferRtv, true, &dsvHandle);
  RenderParticles(cmdList);

  auto depthToWrite = CD3DX12_RESOURCE_BARRIER::Transition(
      depthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);
  cmdList->ResourceBarrier(1, &depthToWrite);

  auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PRESENT);
  cmdList->ResourceBarrier(1, &toPresent);
}

void RenderingSystem::BuildShadowPassResources(ID3D12Device* device,
                                               ID3D12DescriptorHeap* cbvSrvHeap,
                                               UINT cbvSrvDescriptorSize) {
  mShadowFrameCbStride = (sizeof(DirectX::SimpleMath::Matrix) + 255u) & ~255u;
  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const auto cbDesc =
      CD3DX12_RESOURCE_DESC::Buffer(mShadowFrameCbStride * kShadowCascadeCount);
  ThrowIfFailed(device->CreateCommittedResource(
      &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mShadowFrameCB)));

  D3D12_RESOURCE_DESC shadowDesc = {};
  shadowDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  shadowDesc.Width = kShadowMapResolution;
  shadowDesc.Height = kShadowMapResolution;
  shadowDesc.DepthOrArraySize = kShadowCascadeCount;
  shadowDesc.MipLevels = 1;
  shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
  shadowDesc.SampleDesc.Count = 1;
  shadowDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  shadowDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = DXGI_FORMAT_D32_FLOAT;
  clearValue.DepthStencil.Depth = 1.0f;
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  ThrowIfFailed(device->CreateCommittedResource(
      &defaultHeap, D3D12_HEAP_FLAG_NONE, &shadowDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
      IID_PPV_ARGS(&mShadowMap)));

  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = kShadowCascadeCount;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc,
                                             IID_PPV_ARGS(&mShadowDsvHeap)));
  for (UINT cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.ArraySize = 1;
    dsvDesc.Texture2DArray.FirstArraySlice = cascade;
    mShadowDsvHandles[cascade] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mShadowDsvHeap->GetCPUDescriptorHandleForHeapStart(), cascade,
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
    device->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc,
                                   mShadowDsvHandles[cascade]);
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc.Texture2DArray.ArraySize = kShadowCascadeCount;
  srvDesc.Texture2DArray.MipLevels = 1;
  CD3DX12_CPU_DESCRIPTOR_HANDLE shadowSrvCpu(
      cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(kShadowMapSrvIndex), cbvSrvDescriptorSize);
  device->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, shadowSrvCpu);
}
void RenderingSystem::UpdateCascadedShadowMapsData(
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj) {
  using namespace DirectX;
  using namespace DirectX::SimpleMath;

  const float cameraNear = 0.1f;
  const float cameraFar = 1000.0f;
  const float lambda = 0.5f;

  std::array<float, kShadowCascadeCount> splits = {};
  for (UINT i = 1; i <= kShadowCascadeCount; ++i) {
    const float p =
        static_cast<float>(i) / static_cast<float>(kShadowCascadeCount);
    const float logSplit = cameraNear * std::pow(cameraFar / cameraNear, p);
    const float linearSplit = cameraNear + (cameraFar - cameraNear) * p;
    splits[i - 1] = lambda * logSplit + (1.0f - lambda) * linearSplit;
  }
  splits[kShadowCascadeCount - 1] = cameraFar;
  mCascadeSplits = Vector4(splits[0], splits[1], splits[2], splits[3]);

  Matrix invViewProj = (view * proj).Invert();
  Vector3 frustumNear[4];
  Vector3 frustumFar[4];
  const float ndcX[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
  const float ndcY[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
  for (int i = 0; i < 4; ++i) {
    frustumNear[i] =
        Vector3::Transform(Vector3(ndcX[i], ndcY[i], 0.0f), invViewProj);
    frustumFar[i] =
        Vector3::Transform(Vector3(ndcX[i], ndcY[i], 1.0f), invViewProj);
  }

  // Must match the directional light used in compose pass, otherwise
  // CSM is rendered from one direction and sampled/ lit from another.
  Vector3 lightDir = Vector3(0.0f, -1.0f, 0.0f);
  lightDir.Normalize();
  Vector3 up(0.0f, 1.0f, 0.0f);
  if (std::abs(lightDir.Dot(up)) > 0.95f) {
    up = Vector3(0.0f, 0.0f, 1.0f);
  }

  float previousSplit = cameraNear;
  for (UINT cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
    const float cascadeNear = previousSplit;
    const float cascadeFar = splits[cascade];
    previousSplit = cascadeFar;

    const float nearRatio =
        (cascadeNear - cameraNear) / (cameraFar - cameraNear);
    const float farRatio = (cascadeFar - cameraNear) / (cameraFar - cameraNear);

    Vector3 corners[8];
    Vector3 center = Vector3::Zero;
    for (int i = 0; i < 4; ++i) {
      const Vector3 ray = frustumFar[i] - frustumNear[i];
      corners[i] = frustumNear[i] + ray * nearRatio;
      corners[i + 4] = frustumNear[i] + ray * farRatio;
      center += corners[i] + corners[i + 4];
    }
    center /= 8.0f;

    float radius = 0.0f;
    for (const Vector3& corner : corners) {
      radius = std::max(radius, (corner - center).Length());
    }
    const float lightDistance = radius + 250.0f;
    const Vector3 eye = center - lightDir * lightDistance;
    Matrix lightView = Matrix::CreateLookAt(eye, center, up);

    Vector3 minB(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const Vector3& corner : corners) {
      Vector3 p = Vector3::Transform(corner, lightView);
      minB.x = std::min(minB.x, p.x);
      minB.y = std::min(minB.y, p.y);
      minB.z = std::min(minB.z, p.z);
      maxB.x = std::max(maxB.x, p.x);
      maxB.y = std::max(maxB.y, p.y);
      maxB.z = std::max(maxB.z, p.z);
    }

    const float width = maxB.x - minB.x;
    const float height = maxB.y - minB.y;
    const float texelX = width / static_cast<float>(kShadowMapResolution);
    const float texelY = height / static_cast<float>(kShadowMapResolution);
    float centerX = (minB.x + maxB.x) * 0.5f;
    float centerY = (minB.y + maxB.y) * 0.5f;
    centerX = std::floor(centerX / texelX) * texelX;
    centerY = std::floor(centerY / texelY) * texelY;
    minB.x = centerX - width * 0.5f;
    maxB.x = centerX + width * 0.5f;
    minB.y = centerY - height * 0.5f;
    maxB.y = centerY + height * 0.5f;
    minB.z -= 250.0f;
    maxB.z += 250.0f;

    Matrix lightProj = Matrix::CreateOrthographicOffCenter(
        minB.x, maxB.x, minB.y, maxB.y, minB.z, maxB.z);
    mShadowViewProj[cascade] = (lightView * lightProj).Transpose();
  }
}
void RenderingSystem::RenderShadowPass(
    ID3D12GraphicsCommandList* cmdList,
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    const ModelGeometry& modelGeometry,
    const std::vector<SceneObject>& sceneObjects,
    const std::vector<SubmeshInstance>& submeshInstances,
    const std::vector<UINT>& visIndices) {
  (void)visIndices;
  if (!mShadowMap || !mShadowPSO || !mShadowRootSignature || !mShadowFrameCB) {
    return;
  }

  auto toDepth = CD3DX12_RESOURCE_BARRIER::Transition(
      mShadowMap.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);
  cmdList->ResourceBarrier(1, &toDepth);

  D3D12_VIEWPORT shadowViewport = {0.0f,
                                   0.0f,
                                   static_cast<float>(kShadowMapResolution),
                                   static_cast<float>(kShadowMapResolution),
                                   0.0f,
                                   1.0f};
  D3D12_RECT shadowScissor = {0, 0, static_cast<LONG>(kShadowMapResolution),
                              static_cast<LONG>(kShadowMapResolution)};
  cmdList->RSSetViewports(1, &shadowViewport);
  cmdList->RSSetScissorRects(1, &shadowScissor);
  cmdList->SetPipelineState(mShadowPSO.Get());
  cmdList->SetGraphicsRootSignature(mShadowRootSignature.Get());
  cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
  cmdList->IASetIndexBuffer(&indexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  for (UINT cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
    cmdList->ClearDepthStencilView(mShadowDsvHandles[cascade],
                                   D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &mShadowDsvHandles[cascade]);
    void* shadowMapped = nullptr;
    mShadowFrameCB->Map(0, nullptr, &shadowMapped);
    memcpy(reinterpret_cast<std::uint8_t*>(shadowMapped) +
               cascade * mShadowFrameCbStride,
           &mShadowViewProj[cascade], sizeof(DirectX::SimpleMath::Matrix));
    mShadowFrameCB->Unmap(0, nullptr);
    cmdList->SetGraphicsRootConstantBufferView(
        1, mShadowFrameCB->GetGPUVirtualAddress() +
               cascade * mShadowFrameCbStride);

    for (UINT submeshInstanceIndex = 0;
         submeshInstanceIndex < static_cast<UINT>(submeshInstances.size());
         ++submeshInstanceIndex) {
      const SubmeshInstance& instance = submeshInstances[submeshInstanceIndex];
      if (instance.ObjectIndex >= sceneObjects.size() ||
          instance.SubmeshIndex >= modelGeometry.Submeshes.size()) {
        continue;
      }
      CD3DX12_GPU_DESCRIPTOR_HANDLE objectCbHandle(
          mCbvSrvHeapGpuStart,
          static_cast<INT>(kObjectCbvStart + instance.ObjectIndex),
          mCbvSrvDescriptorSize);
      cmdList->SetGraphicsRootDescriptorTable(0, objectCbHandle);

      const auto& submesh = modelGeometry.Submeshes[instance.SubmeshIndex];
      cmdList->DrawIndexedInstanced(submesh.IndexCount, 1,
                                    submesh.StartIndexLocation, 0, 0);
    }
  }

  auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
      mShadowMap.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cmdList->ResourceBarrier(1, &toSrv);
}
