#include "RenderingSystem.h"

void RenderingSystem::Initialize(ID3D12Device* device, UINT width, UINT height,
                                 ID3D12DescriptorHeap* rtvHeap,
                                 ID3D12DescriptorHeap* cbvSrvHeap,
                                 UINT rtvDescriptorSize,
                                 UINT cbvSrvDescriptorSize) {
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

  CD3DX12_DESCRIPTOR_RANGE samplerTable;
  samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
  params[1].InitAsDescriptorTable(1, &samplerTable);

  params[2].InitAsConstantBufferView(0);

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      3, params, 0, nullptr,
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
  CD3DX12_ROOT_PARAMETER params[5];
  params[0].InitAsUnorderedAccessView(2);  // deadList consume
  params[1].InitAsUnorderedAccessView(0);  // Particle pool
  params[2].InitAsUnorderedAccessView(1);  // DeadList append
  params[3].InitAsConstantBufferView(0);   // Sim constants
  params[4].InitAsShaderResourceView(1);   // Particle pool SRV

  CD3DX12_ROOT_SIGNATURE_DESC desc(5, params, 0, nullptr,
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
  CD3DX12_ROOT_PARAMETER params[2];
  params[0].InitAsShaderResourceView(0);
  params[1].InitAsConstantBufferView(0);

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      2, params, 0, nullptr,
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

  auto createDefaultBuffer = [&](UINT64 size, ComPtr<ID3D12Resource>& buffer) {
    const CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    const CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(size);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));
  };

  createDefaultBuffer(particlePoolSize, mParticlePoolBuffer);
  createDefaultBuffer(deadListSize, mDeadListABuffer);
  createDefaultBuffer(deadListSize, mDeadListBBuffer);
  createDefaultBuffer(sizeof(UINT), mDeadListACounterBuffer);
  createDefaultBuffer(sizeof(UINT), mDeadListBCounterBuffer);

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

  D3D12_SHADER_RESOURCE_VIEW_DESC deadSrvDesc = {};
  deadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  deadSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  deadSrvDesc.Buffer.FirstElement = 0;
  deadSrvDesc.Buffer.NumElements = kParticleMaxCount;
  deadSrvDesc.Buffer.StructureByteStride = deadListStride;
  deadSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
  device->CreateShaderResourceView(
      mDeadListABuffer.Get(), &deadSrvDesc,
      CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, kDeadListAConsumeSrvIndex,
                                    cbvSrvDescriptorSize));
  device->CreateShaderResourceView(
      mDeadListBBuffer.Get(), &deadSrvDesc,
      CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, kDeadListBConsumeSrvIndex,
                                    cbvSrvDescriptorSize));

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

  ID3D12Resource* consumeDeadList =
      mUseDeadListAAsConsume ? mDeadListABuffer.Get() : mDeadListBBuffer.Get();
  ID3D12Resource* appendDeadList =
      mUseDeadListAAsConsume ? mDeadListBBuffer.Get() : mDeadListABuffer.Get();
  auto appendDeadUavCpu =
      mUseDeadListAAsConsume ? mDeadListBUavCpuHandle : mDeadListAUavCpuHandle;
  auto appendDeadUavGpu =
      mUseDeadListAAsConsume ? mDeadListBUavGpuHandle : mDeadListAUavGpuHandle;

  auto toCompute = CD3DX12_RESOURCE_BARRIER::Transition(
      mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  cmdList->ResourceBarrier(1, &toCompute);

  if (!mParticlesInitialized) {
    cmdList->SetPipelineState(mParticlesInitPSO.Get());
    cmdList->SetComputeRootSignature(mParticlesComputeRootSignature.Get());
    cmdList->SetComputeRootUnorderedAccessView(
        1, mParticlePoolBuffer->GetGPUVirtualAddress());
    cmdList->SetComputeRootUnorderedAccessView(
        2, mDeadListABuffer->GetGPUVirtualAddress());
    cmdList->SetComputeRootConstantBufferView(
        3, mParticleSimConstantBuffer->GetGPUVirtualAddress());
    cmdList->Dispatch((kParticleMaxCount + 127) / 128, 1, 1);
    auto initBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    cmdList->ResourceBarrier(1, &initBarrier);
    mParticlesInitialized = true;
    mUseDeadListAAsConsume = true;
    consumeDeadList = mDeadListABuffer.Get();
    appendDeadList = mDeadListBBuffer.Get();
    appendDeadUavCpu = mDeadListBUavCpuHandle;
    appendDeadUavGpu = mDeadListBUavGpuHandle;
  }

  const UINT clearValues[4] = {0, 0, 0, 0};
  cmdList->ClearUnorderedAccessViewUint(appendDeadUavGpu, appendDeadUavCpu,
                                        appendDeadList, clearValues, 0,
                                        nullptr);

  cmdList->SetPipelineState(mParticlesEmitPSO.Get());
  cmdList->SetComputeRootSignature(mParticlesComputeRootSignature.Get());
  cmdList->SetComputeRootUnorderedAccessView(
      0, consumeDeadList->GetGPUVirtualAddress());
  cmdList->SetComputeRootUnorderedAccessView(
      1, mParticlePoolBuffer->GetGPUVirtualAddress());
  cmdList->SetComputeRootUnorderedAccessView(
      2, appendDeadList->GetGPUVirtualAddress());
  cmdList->SetComputeRootConstantBufferView(
      3, mParticleSimConstantBuffer->GetGPUVirtualAddress());
  cmdList->SetComputeRootShaderResourceView(
      4, mParticlePoolBuffer->GetGPUVirtualAddress());
  cmdList->Dispatch((mMappedParticleSimConstants->SpawnCount + 63) / 64, 1, 1);

  auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  cmdList->ResourceBarrier(1, &uavBarrier);

  cmdList->SetPipelineState(mParticlesSimulatePSO.Get());
  cmdList->Dispatch((kParticleMaxCount + 127) / 128, 1, 1);
  cmdList->ResourceBarrier(1, &uavBarrier);

  auto toCommon = CD3DX12_RESOURCE_BARRIER::Transition(
      mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COMMON);
  cmdList->ResourceBarrier(1, &toCommon);

  mUseDeadListAAsConsume = !mUseDeadListAAsConsume;
}

void RenderingSystem::RenderParticles(ID3D12GraphicsCommandList* cmdList) {
  cmdList->SetPipelineState(mParticlesRenderPSO.Get());
  cmdList->SetGraphicsRootSignature(mParticlesRenderRootSignature.Get());
  cmdList->SetGraphicsRootShaderResourceView(
      0, mParticlePoolBuffer->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, mParticleRenderConstantBuffer->GetGPUVirtualAddress());
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
    const DirectX::SimpleMath::Matrix& viewProj,
    const DirectX::SimpleMath::Vector3& cameraPosition) {
  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissorRect);

  ID3D12DescriptorHeap* heaps[] = {cbvSrvHeap, samplerHeap};
  cmdList->SetDescriptorHeaps(2, heaps);
  mMappedParticleRenderConstants->CameraPosition = cameraPosition;
  mMappedParticleRenderConstants->BillboardSize = 0.55f;
  mMappedParticleRenderConstants->MaxParticles = kParticleMaxCount;
  mMappedParticleRenderConstants->ViewProj = viewProj.Transpose();

  SimulateParticles(cmdList, deltaTime, cameraPosition);

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
  cmdList->SetGraphicsRootDescriptorTable(
      1, samplerHeap->GetGPUDescriptorHandleForHeapStart());
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
