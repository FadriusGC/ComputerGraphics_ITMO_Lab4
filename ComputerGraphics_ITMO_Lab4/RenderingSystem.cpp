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
  BuildGeometryPSO(device);
  BuildComposePSO(device);

  mGBuffer.Initialize(device, width, height, rtvHeap, cbvSrvHeap,
                      rtvDescriptorSize, cbvSrvDescriptorSize, kGBufferRtvStart,
                      kGBufferSrvStart);
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
  mComposeVS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredComposeVS.hlsl",
      "VS", "vs_5_0");
  mComposePS = ShaderHelper::CompileShader(
      L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      L"ComputerGraphics_ITMO_Lab4/DeferredComposePS.hlsl",
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
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void RenderingSystem::BuildGeometryRootSignature(ID3D12Device* device) {
  CD3DX12_ROOT_PARAMETER params[4];

  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  params[0].InitAsDescriptorTable(1, &cbvTable0);

  CD3DX12_DESCRIPTOR_RANGE srvTable;
  srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  params[1].InitAsDescriptorTable(1, &srvTable);

  CD3DX12_DESCRIPTOR_RANGE samplerTable;
  samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
  params[2].InitAsDescriptorTable(1, &samplerTable);

  params[3].InitAsConstantBufferView(2);

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      4, params, 0, nullptr,
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
  CD3DX12_ROOT_PARAMETER params[2];

  CD3DX12_DESCRIPTOR_RANGE gbufferSrvTable;
  gbufferSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
  params[0].InitAsDescriptorTable(1, &gbufferSrvTable);

  CD3DX12_DESCRIPTOR_RANGE samplerTable;
  samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
  params[1].InitAsDescriptorTable(1, &samplerTable);

  CD3DX12_ROOT_SIGNATURE_DESC desc(
      2, params, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &serialized, &error));
  ThrowIfFailed(device->CreateRootSignature(
      0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
      IID_PPV_ARGS(&mComposeRootSignature)));
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

  CD3DX12_RASTERIZER_DESC rast(D3D12_DEFAULT);
  rast.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState = rast;
  pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  pso.SampleMask = UINT_MAX;
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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

void RenderingSystem::Render(
    ID3D12GraphicsCommandList* cmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv, ID3D12Resource* backBuffer,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, ID3D12DescriptorHeap* cbvSrvHeap,
    ID3D12DescriptorHeap* samplerHeap, UINT cbvSrvDescriptorSize,
    const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    const ModelGeometry& modelGeometry,
    UploadBuffer<MaterialConstants>* materialCB) {
  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissorRect);

  ID3D12DescriptorHeap* heaps[] = {cbvSrvHeap, samplerHeap};
  cmdList->SetDescriptorHeaps(2, heaps);

  cmdList->SetPipelineState(mGeometryPSO.Get());
  cmdList->SetGraphicsRootSignature(mGeometryRootSignature.Get());

  mGBuffer.BeginGeometryPass(cmdList, dsvHandle);

  cmdList->SetGraphicsRootDescriptorTable(
      0, cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
  cmdList->SetGraphicsRootDescriptorTable(
      2, samplerHeap->GetGPUDescriptorHandleForHeapStart());

  cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
  cmdList->IASetIndexBuffer(&indexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  const UINT cbMaterialSize = (sizeof(MaterialConstants) + 255) & ~255;

  for (const auto& submesh : modelGeometry.Submeshes) {
    if (submesh.MaterialIndex < modelGeometry.Materials.size()) {
      const auto& mat = modelGeometry.Materials[submesh.MaterialIndex];

      if (mat.DiffuseTextureIndex >= 0) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(kTextureSrvStart + mat.DiffuseTextureIndex),
            cbvSrvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(1, texHandle);
      }

      D3D12_GPU_VIRTUAL_ADDRESS matCBAddress =
          materialCB->Resource()->GetGPUVirtualAddress() +
          static_cast<UINT64>(mat.MatCBIndex) * cbMaterialSize;
      cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);
    }

    cmdList->DrawIndexedInstanced(submesh.IndexCount, 1,
                                  submesh.StartIndexLocation, 0, 0);
  }

  mGBuffer.EndGeometryPass(cmdList);

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
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);

  auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PRESENT);
  cmdList->ResourceBarrier(1, &toPresent);
}
