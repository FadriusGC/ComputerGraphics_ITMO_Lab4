#define NOMINMAX
#include "BoxApp.h"

#include "DDSTextureLoader.h"
#include "Material.h"
#include "ModelLoader.h"
#include "ShaderHelper.h"

BoxApp::BoxApp(HINSTANCE hInstance)
    : m_window(),
      mTimer(),
      mCamPos(0.0f, 2.0f, -5.0f),  // начальная позиция камеры
      mCamYaw(0.0f),
      mCamPitch(0.0f),
      mMoveSpeed(25.0f),
      mMouseSensitivity(0.002f),
      mRandomEngine(std::random_device{}()) {
  mWorld = DirectX::SimpleMath::Matrix::Identity;
  mView = DirectX::SimpleMath::Matrix::Identity;
  mProj = DirectX::SimpleMath::Matrix::Identity;

  mFallingLights[0].Color = DirectX::SimpleMath::Vector3(1.0f, 0.35f, 0.25f);
  mFallingLights[0].Intensity = 13.0f;
  mFallingLights[0].Range = 100.0f;
  mFallingLights[0].FallSpeed = 45.0f;

  mFallingLights[1].Color = DirectX::SimpleMath::Vector3(0.25f, 0.45f, 1.0f);
  mFallingLights[1].Intensity = 12.0f;
  mFallingLights[1].Range = 120.0f;
  mFallingLights[1].FallSpeed = 42.0f;

  mFallingLights[2].Color = DirectX::SimpleMath::Vector3(0.25f, 0.65f, 0.10f);
  mFallingLights[2].Intensity = 11.0f;
  mFallingLights[2].Range = 110.0f;
  mFallingLights[2].FallSpeed = 40.0f;

  const std::array<DirectX::SimpleMath::Vector3, 5> extraLightPalette = {
      DirectX::SimpleMath::Vector3(1.0f, 0.80f, 0.30f),
      DirectX::SimpleMath::Vector3(0.80f, 0.20f, 1.0f),
      DirectX::SimpleMath::Vector3(0.20f, 1.0f, 0.80f),
      DirectX::SimpleMath::Vector3(1.0f, 0.40f, 0.70f),
      DirectX::SimpleMath::Vector3(0.40f, 0.90f, 1.0f)};

  for (size_t i = 3; i < mFallingLights.size(); ++i) {
    auto& light = mFallingLights[i];
    light.Color = extraLightPalette[(i - 3) % extraLightPalette.size()];
    light.Intensity = 9.0f + static_cast<float>((i - 3) % 4);
    light.Range = 95.0f + 7.0f * static_cast<float>((i - 3) % 5);
    light.FallSpeed = 32.0f + 3.5f * static_cast<float>((i - 3) % 4);
  }
  for (auto& light : mFallingLights) {
    ResetFallingLight(light);
    light.CooldownAfterLanding = 0.0f;
  }
}

BoxApp::~BoxApp() { FlushCommandQueue(); }

bool BoxApp::Initialize() {
  if (!m_window.Initialize(GetModuleHandle(nullptr), WIDTH, HEIGHT,
                           L"Direct3D 12 with Assimp Model UBEITE MENYA PZH")) {
    return false;
  }

  CreateDevice();
  CreateCommandObjects();
  CreateSwapChain();
  BuildDescriptorHeaps();
  CreateRTVs();
  CreateDepthStencil();

  mScreenViewport.TopLeftX = 0;
  mScreenViewport.TopLeftY = 0;
  mScreenViewport.Width = static_cast<float>(WIDTH);
  mScreenViewport.Height = static_cast<float>(HEIGHT);
  mScreenViewport.MinDepth = 0.0f;
  mScreenViewport.MaxDepth = 1.0f;

  mScissorRect = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};

  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

  BuildConstantBuffers();
  BuildBoxGeometry();  // загружает модель, создаёт буферы и текстуры

  // Создаём сэмплер
  CreateSamplerHeap();
  mRenderingSystem.Initialize(mDevice.Get(), WIDTH, HEIGHT, mRtvHeap.Get(),
                              mCbvHeap.Get(), mRtvDescriptorSize,
                              mCbvSrvDescriptorSize);
  // Закрываем и выполняем все накопленные команды (геометрия + текстуры)
  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);

  FlushCommandQueue();
  OnResize();

  return true;
}

void BoxApp::OnResize() {
  mProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
      0.25f * DirectX::XM_PI,
      static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 1000.0f);
}

void BoxApp::ResetFallingLight(FallingPointLight& light) {
  std::uniform_real_distribution<float> xzDistribution(-140.0f, 140.0f);
  std::uniform_real_distribution<float> heightDistribution(95.0f, 145.0f);
  std::uniform_real_distribution<float> cooldownDistribution(0.7f, 2.1f);

  light.Position.x = xzDistribution(mRandomEngine);
  light.Position.z = xzDistribution(mRandomEngine);
  light.Position.y = heightDistribution(mRandomEngine);
  light.CooldownAfterLanding = cooldownDistribution(mRandomEngine);
}

void BoxApp::BuildDescriptorHeaps() {
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors =
      SwapChainBufferCount + GBuffer::kRenderTargetCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

  D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
  cbvHeapDesc.NumDescriptors =
      128;  // достаточно для object CBV, light CBV и множества текстур
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));

  D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
  samplerHeapDesc.NumDescriptors = 1;
  samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(mDevice->CreateDescriptorHeap(&samplerHeapDesc,
                                              IID_PPV_ARGS(&mSamplerHeap)));
}

void BoxApp::BuildConstantBuffers() {
  mObjectCB = std::unique_ptr<UploadBuffer<ObjectConstants>>(
      new UploadBuffer<ObjectConstants>(mDevice.Get(), 1, true));
  mLightCB = std::unique_ptr<UploadBuffer<LightConstants>>(
      new UploadBuffer<LightConstants>(mDevice.Get(), 1, true));
  mComposeCB = std::unique_ptr<UploadBuffer<ComposeConstants>>(
      new UploadBuffer<ComposeConstants>(mDevice.Get(), 1, true));

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescObject = {};
  cbvDescObject.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress();
  cbvDescObject.SizeInBytes = (sizeof(ObjectConstants) + 255) & ~255;

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescLight = {};
  cbvDescLight.BufferLocation = mLightCB->Resource()->GetGPUVirtualAddress();
  cbvDescLight.SizeInBytes = (sizeof(LightConstants) + 255) & ~255;

  // Первый дескриптор – object CBV
  mDevice->CreateConstantBufferView(
      &cbvDescObject, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

  // Второй дескриптор – light CBV
  CD3DX12_CPU_DESCRIPTOR_HANDLE lightCbvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvDescriptorSize);
  mDevice->CreateConstantBufferView(&cbvDescLight, lightCbvHandle);
}

void BoxApp::BuildRootSignature() {
  CD3DX12_ROOT_PARAMETER slotRootParameter[5];

  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

  CD3DX12_DESCRIPTOR_RANGE cbvTable1;
  cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

  CD3DX12_DESCRIPTOR_RANGE srvTable;
  srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  slotRootParameter[2].InitAsDescriptorTable(1, &srvTable);

  CD3DX12_DESCRIPTOR_RANGE samplerTable;
  samplerTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
  slotRootParameter[3].InitAsDescriptorTable(1, &samplerTable);

  slotRootParameter[4].InitAsConstantBufferView(2);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
      5, slotRootParameter, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;

  ThrowIfFailed(D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
      serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

  if (errorBlob != nullptr) {
    OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  }

  ThrowIfFailed(mDevice->CreateRootSignature(
      0, serializedRootSig->GetBufferPointer(),
      serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout() {
  try {
    mVSByteCode = ShaderHelper::CompileShader(
        L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
        L"ComputerGraphics_ITMO_Lab4/BoxVertexShader.hlsl",
        "VS", "vs_5_0");
    mPSByteCode = ShaderHelper::CompileShader(
        L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
        L"ComputerGraphics_ITMO_Lab4/BoxPixelShader.hlsl",
        "PS", "ps_5_0");
  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "Shader Error", MB_OK | MB_ICONERROR);
    throw;
  }

  mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void BoxApp::BuildBoxGeometry() {
  std::string modelPath =
      "C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
      "ComputerGraphics_ITMO_Lab4/sponza.obj";

  if (!ModelLoader::LoadModel(modelPath, mModelGeometry)) {
    MessageBoxA(nullptr, "Failed to load model. Using fallback cube.",
                "Warning", MB_OK);
    CreateFallbackCube();
  } else {
    char buf[256];
    sprintf_s(
        buf, "Loaded %zu vertices, %zu indices, %zu submeshes, %zu materials\n",
        mModelGeometry.Vertices.size(), mModelGeometry.Indices.size(),
        mModelGeometry.Submeshes.size(), mModelGeometry.Materials.size());
    OutputDebugStringA(buf);
  }

  if (mModelGeometry.Materials.empty()) {
    Material defaultMat;
    defaultMat.Name = "Default";
    defaultMat.Data.DiffuseAlbedo = {0.8f, 0.8f, 0.8f, 1.0f};
    defaultMat.Data.FresnelR0 = {0.01f, 0.01f, 0.01f};
    defaultMat.Data.Roughness = 0.25f;
    mModelGeometry.Materials.push_back(defaultMat);
  }

  // Создаём буфер материалов
  UINT numMaterials = (UINT)mModelGeometry.Materials.size();
  mMaterialCB = std::unique_ptr<UploadBuffer<MaterialConstants>>(
      new UploadBuffer<MaterialConstants>(mDevice.Get(), numMaterials, true));

  for (UINT i = 0; i < numMaterials; ++i) {
    mMaterialCB->CopyData(i, mModelGeometry.Materials[i].Data);
    mModelGeometry.Materials[i].MatCBIndex = i;
  }

  mVertexBufferByteSize =
      (UINT)(mModelGeometry.Vertices.size() * sizeof(Vertex));
  mIndexBufferByteSize =
      (UINT)(mModelGeometry.Indices.size() * sizeof(uint32_t));
  mIndexCount = (UINT)mModelGeometry.Indices.size();

  // Вершинный буфер
  {
    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mVertexBufferByteSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mVertexBufferGPU)));

    D3D12_HEAP_PROPERTIES uploadHeapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mVertexBufferUploader)));

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = mModelGeometry.Vertices.data();
    vertexData.RowPitch = mVertexBufferByteSize;
    vertexData.SlicePitch = mVertexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mVertexBufferGPU.Get(),
                       mVertexBufferUploader.Get(), 0, 0, 1, &vertexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mVertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = mVertexBufferByteSize;
    mVertexBufferView.StrideInBytes = sizeof(Vertex);
  }

  // Индексный буфер
  {
    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mIndexBufferByteSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mIndexBufferGPU)));

    D3D12_HEAP_PROPERTIES uploadHeapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mIndexBufferUploader)));

    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = mModelGeometry.Indices.data();
    indexData.RowPitch = mIndexBufferByteSize;
    indexData.SlicePitch = mIndexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mIndexBufferGPU.Get(),
                       mIndexBufferUploader.Get(), 0, 0, 1, &indexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mIndexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = mIndexBufferByteSize;
    mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
  }

  // Загружаем все текстуры, связанные с материалами
  LoadAllTextures();
}

void BoxApp::CreateFallbackCube() {
  std::array<Vertex, 24> vertices = {
      // Front (z = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 1.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),

      // Back (z = 1)
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 1.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),

      // Left (x = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{-1.0f, 1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),
      Vertex({{-1.0f, 1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),
      Vertex({{-1.0f, -1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),

      // Right (x = 1)
      Vertex({{1.0f, -1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{1.0f, -1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),

      // Top (y = 1)
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),

      // Bottom (y = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 0.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 1.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 1.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),
  };

  std::array<uint32_t, 36> indices = {
      0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
      12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

  mModelGeometry.Vertices.assign(vertices.begin(), vertices.end());
  mModelGeometry.Indices.assign(indices.begin(), indices.end());

  // Создаём один сабмеш
  Submesh submesh;
  submesh.MaterialIndex = 0;
  submesh.IndexCount = 36;
  submesh.StartIndexLocation = 0;
  mModelGeometry.Submeshes.push_back(submesh);

  Material defaultMat;
  defaultMat.Name = "CubeMaterial";
  defaultMat.Data.DiffuseAlbedo = {0.8f, 0.8f, 0.8f, 1.0f};
  defaultMat.Data.FresnelR0 = {0.01f, 0.01f, 0.01f};
  defaultMat.Data.Roughness = 0.25f;
  defaultMat.Data.TexTransform = DirectX::SimpleMath::Matrix::Identity;
  mModelGeometry.Materials.push_back(defaultMat);
}

// Загрузка текстур для всех материалов

void BoxApp::LoadAllTextures() {
  // Собираем уникальные пути текстур из материалов
  std::unordered_map<std::string, int>
      textureNameToIndex;  // имя файла -> индекс в mTextures
  std::vector<std::string> uniqueTexturePaths;

  for (auto& mat : mModelGeometry.Materials) {
    if (!mat.DiffuseTexture.empty()) {
      if (textureNameToIndex.find(mat.DiffuseTexture) ==
          textureNameToIndex.end()) {
        int index = (int)uniqueTexturePaths.size();
        textureNameToIndex[mat.DiffuseTexture] = index;
        uniqueTexturePaths.push_back(mat.DiffuseTexture);
      }
    }
  }

  if (uniqueTexturePaths.empty()) {
    OutputDebugStringA("No diffuse textures found.\n");
    return;
  }

  // Загружаем текстуры
  for (const auto& texName : uniqueTexturePaths) {
    // Формируем полный путь к текстуре
    std::wstring fullPath =
        L"C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/"
        L"ComputerGraphics_ITMO_Lab4/textures/" +
        std::wstring(texName.begin(), texName.end());

    auto texture = std::make_unique<Texture>();
    texture->name = texName;
    texture->filepath = fullPath;

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
        mDevice.Get(), mCommandList.Get(), fullPath.c_str(), texture->Resource,
        texture->UploadHeap));

    // Сохраняем текстуру в вектор
    int textureIndex = (int)mTextures.size();
    mTextures.push_back(std::move(texture));

    // Создаём SRV для этой текстуры в куче по индексу
    // (kTextureSrvHeapStart + textureIndex)
    CreateSRV(mTextures[textureIndex]->Resource,
              kTextureSrvHeapStart + textureIndex);
  }

  // После загрузки всех текстур связываем материалы с индексами текстур
  for (auto& mat : mModelGeometry.Materials) {
    if (!mat.DiffuseTexture.empty()) {
      auto it = textureNameToIndex.find(mat.DiffuseTexture);
      if (it != textureNameToIndex.end()) {
        mat.DiffuseTextureIndex = it->second;
      }
    }
  }

  OutputDebugStringA(
      ("Loaded " + std::to_string(mTextures.size()) + " textures.\n").c_str());
}

void BoxApp::CreateSRV(ComPtr<ID3D12Resource> textureResource, int heapIndex) {
  CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(), heapIndex,
      mCbvSrvDescriptorSize);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = textureResource->GetDesc().Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.MipLevels = textureResource->GetDesc().MipLevels;
  srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

  mDevice->CreateShaderResourceView(textureResource.Get(), &srvDesc, srvHandle);
}

void BoxApp::CreateSamplerHeap() {
  D3D12_SAMPLER_DESC samplerDesc = {};
  samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.MinLOD = 0;
  samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
  samplerDesc.MipLODBias = 0.0f;
  samplerDesc.MaxAnisotropy = 1;
  samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

  mDevice->CreateSampler(&samplerDesc,
                         mSamplerHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildPSO() {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

  psoDesc.InputLayout = {mInputLayout.data(), (UINT)mInputLayout.size()};
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.VS = {reinterpret_cast<BYTE*>(mVSByteCode->GetBufferPointer()),
                mVSByteCode->GetBufferSize()};
  psoDesc.PS = {reinterpret_cast<BYTE*>(mPSByteCode->GetBufferPointer()),
                mPSByteCode->GetBufferSize()};

  CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

  psoDesc.RasterizerState = rasterizerDesc;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = BackBufferFormat;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;
  psoDesc.DSVFormat = DepthStencilFormat;

  ThrowIfFailed(
      mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void BoxApp::CreateDevice() {
  ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

#if defined(DEBUG) || defined(_DEBUG)
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
  }
#endif

  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&mDevice));

  if (FAILED(hr)) {
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(mFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&mDevice)));
  }

  ThrowIfFailed(
      mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

  mRtvDescriptorSize =
      mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  mDsvDescriptorSize =
      mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  mCbvSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void BoxApp::CreateCommandObjects() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

  ThrowIfFailed(mDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));

  ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           mCommandAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&mCommandList)));

  mCommandList->Close();
}

void BoxApp::CreateSwapChain() {
  mSwapChain.Reset();

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferDesc.Width = WIDTH;
  sd.BufferDesc.Height = HEIGHT;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = BackBufferFormat;
  sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = SwapChainBufferCount;
  sd.OutputWindow = m_window.GetHWND();
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  ThrowIfFailed(
      mFactory->CreateSwapChain(mCommandQueue.Get(), &sd, &mSwapChain));
}

void BoxApp::CreateRTVs() {
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart());

  for (UINT i = 0; i < SwapChainBufferCount; i++) {
    ThrowIfFailed(
        mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffers[i])));
    mDevice->CreateRenderTargetView(mSwapChainBuffers[i].Get(), nullptr,
                                    rtvHeapHandle);
    rtvHeapHandle.Offset(1, mRtvDescriptorSize);
  }
}

void BoxApp::CreateDepthStencil() {
  D3D12_RESOURCE_DESC depthStencilDesc = {};
  depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthStencilDesc.Width = WIDTH;
  depthStencilDesc.Height = HEIGHT;
  depthStencilDesc.DepthOrArraySize = 1;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
  depthStencilDesc.SampleDesc.Count = 1;
  depthStencilDesc.SampleDesc.Quality = 0;
  depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE optClear = {};
  optClear.Format = DepthStencilFormat;
  optClear.DepthStencil.Depth = 1.0f;
  optClear.DepthStencil.Stencil = 0;

  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

  ThrowIfFailed(mDevice->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc,
      D3D12_RESOURCE_STATE_COMMON, &optClear,
      IID_PPV_ARGS(&mDepthStencilBuffer)));

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DepthStencilFormat;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  mDevice->CreateDepthStencilView(
      mDepthStencilBuffer.Get(), &dsvDesc,
      mDsvHeap->GetCPUDescriptorHandleForHeapStart());

  D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
  depthSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
  depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  depthSrvDesc.Texture2D.MipLevels = 1;
  CD3DX12_CPU_DESCRIPTOR_HANDLE depthSrvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<INT>(RenderingSystem::kDepthSrvIndex), mCbvSrvDescriptorSize);
  mDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &depthSrvDesc,
                                    depthSrvHandle);

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
  mCommandList->ResourceBarrier(1, &barrier);
  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);
  FlushCommandQueue();
}

void BoxApp::Update(const GameTimer& gt) {
  // фрикам
  if (GetActiveWindow() == m_window.GetHWND()) {
    DirectX::SimpleMath::Vector3 lookDir(cosf(mCamPitch) * sinf(mCamYaw),
                                         sinf(mCamPitch),
                                         cosf(mCamPitch) * cosf(mCamYaw));
    lookDir.Normalize();

    DirectX::SimpleMath::Vector3 up(0.0f, 1.0f, 0.0f);
    DirectX::SimpleMath::Vector3 right = lookDir.Cross(up);
    right.Normalize();

    // Перемещение
    float speed = mMoveSpeed * gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000) mCamPos += lookDir * speed;
    if (GetAsyncKeyState('S') & 0x8000) mCamPos -= lookDir * speed;
    if (GetAsyncKeyState('A') & 0x8000) mCamPos -= right * speed;
    if (GetAsyncKeyState('D') & 0x8000) mCamPos += right * speed;
    if (GetAsyncKeyState('Q') & 0x8000)  // вверх
      mCamPos.y += speed;
    if (GetAsyncKeyState('E') & 0x8000)  // вниз
      mCamPos.y -= speed;
  }

  // Строим матрицу вида
  DirectX::SimpleMath::Vector3 lookDir(cosf(mCamPitch) * sinf(mCamYaw),
                                       sinf(mCamPitch),
                                       cosf(mCamPitch) * cosf(mCamYaw));
  lookDir.Normalize();
  DirectX::SimpleMath::Vector3 target = mCamPos + lookDir;
  mView = DirectX::SimpleMath::Matrix::CreateLookAt(
      mCamPos, target, DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f));

  // Матрица мира
  float scale = 0.5f;
  mWorld = DirectX::SimpleMath::Matrix::CreateScale(scale);

  DirectX::SimpleMath::Matrix worldViewProj = mWorld * mView * mProj;

  ObjectConstants objConstants;
  objConstants.World = mWorld;
  objConstants.WorldViewProj = worldViewProj.Transpose();
  mObjectCB->CopyData(0, objConstants);

  LightConstants lightConstants;
  lightConstants.LightPosition =
      DirectX::SimpleMath::Vector4(3.0f, 3.0f, 3.0f, 1.0f);
  lightConstants.LightColor =
      DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  lightConstants.CameraPosition =
      DirectX::SimpleMath::Vector4(mCamPos.x, mCamPos.y, mCamPos.z, 1.0f);
  mLightCB->CopyData(0, lightConstants);

  ComposeConstants composeConstants = {};
  DirectX::SimpleMath::Matrix viewProj = mView * mProj;
  composeConstants.InvViewProj = viewProj.Invert().Transpose();
  composeConstants.CameraPosition =
      DirectX::SimpleMath::Vector4(mCamPos.x, mCamPos.y, mCamPos.z, 1.0f);
  composeConstants.ScreenSize = DirectX::SimpleMath::Vector4(
      static_cast<float>(WIDTH), static_cast<float>(HEIGHT),
      1.0f / static_cast<float>(WIDTH), 1.0f / static_cast<float>(HEIGHT));
  constexpr size_t kStaticLightCount = 3;
  static_assert(
      kFallingLightCount + kStaticLightCount <= ComposeConstants::kMaxLights,
      "слишком много источников для ComposeConstants::Lights array");
  composeConstants.LightCount = DirectX::SimpleMath::Vector4(
      static_cast<float>(mFallingLights.size() + kStaticLightCount), 0.0f, 0.0f,
      0.0f);

  // Падающие point lights с приземлением на пол. занимают lights[0-3]
  const float deltaTime = gt.DeltaTime();
  for (size_t i = 0; i < mFallingLights.size(); ++i) {
    auto& fallingLight = mFallingLights[i];
    if (fallingLight.Position.y > fallingLight.GroundY) {
      fallingLight.Position.y = std::max(
          fallingLight.GroundY,
          fallingLight.Position.y - fallingLight.FallSpeed * deltaTime);
    } else {
      fallingLight.CooldownAfterLanding -= deltaTime;
      if (fallingLight.CooldownAfterLanding <= 0.0f) {
        ResetFallingLight(fallingLight);
      }
    }
    composeConstants.Lights[i].PositionWorldAndRange =
        DirectX::SimpleMath::Vector4(
            fallingLight.Position.x, fallingLight.Position.y,
            fallingLight.Position.z, fallingLight.Range);
    composeConstants.Lights[i].DirectionAndType =
        DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    composeConstants.Lights[i].ColorAndIntensity = DirectX::SimpleMath::Vector4(
        fallingLight.Color.x, fallingLight.Color.y, fallingLight.Color.z,
        fallingLight.Intensity);
  }

  const size_t directionalLightIndex = 0;
  const size_t firstSpotLightIndex = directionalLightIndex + 1;
  const size_t secondSpotLightIndex = directionalLightIndex + 2;
  // Directional: солнце типо
  composeConstants.Lights[directionalLightIndex].PositionWorldAndRange =
      DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
  composeConstants.Lights[directionalLightIndex].DirectionAndType =
      DirectX::SimpleMath::Vector4(-0.35f, -1.0f, 0.1f, 1.0f);
  composeConstants.Lights[directionalLightIndex].ColorAndIntensity =
      DirectX::SimpleMath::Vector4(1.0f, 0.95f, 0.82f, 1.6f);

  // Spot #1: спот щеленый
  composeConstants.Lights[firstSpotLightIndex].PositionWorldAndRange =
      DirectX::SimpleMath::Vector4(0.0f, 12.0f, -5.0f, 14445.0f);
  composeConstants.Lights[firstSpotLightIndex].DirectionAndType =
      DirectX::SimpleMath::Vector4(0.0f, 0.5f, -1.0f, 2.0f);
  composeConstants.Lights[firstSpotLightIndex].ColorAndIntensity =
      DirectX::SimpleMath::Vector4(1.0f, 1.0f, 0.0f, 5.0f);
  composeConstants.Lights[firstSpotLightIndex].Params =
      DirectX::SimpleMath::Vector4(0.96f, 0.82f, 0.0f, 0.0f);

  // Spot #2: красный
  composeConstants.Lights[secondSpotLightIndex].PositionWorldAndRange =
      DirectX::SimpleMath::Vector4(0.0f, 12.0f, -5.0f, 155500.0f);
  composeConstants.Lights[secondSpotLightIndex].DirectionAndType =
      DirectX::SimpleMath::Vector4(0.0f, 0.0f, 1.0f, 2.0f);
  composeConstants.Lights[secondSpotLightIndex].ColorAndIntensity =
      DirectX::SimpleMath::Vector4(1.0f, 0.0f, 0.0f, 111.8f);
  composeConstants.Lights[secondSpotLightIndex].Params =
      DirectX::SimpleMath::Vector4(0.96f, 0.82f, 0.0f, 0.0f);

  mComposeCB->CopyData(0, composeConstants);

  static float time = 0.0f;
  time += gt.DeltaTime();

  for (size_t i = 0; i < mModelGeometry.Materials.size(); ++i) {
    auto& mat = mModelGeometry.Materials[i];
    auto& matData = mat.Data;

    if (mat.Name == "bricks") {
      // Тайлинг и вращение только для стены
      float scaleU = 15.0f;
      float scaleV = 15.0f;
      float angle = time * 0.8f;

      DirectX::SimpleMath::Matrix rotation =
          DirectX::SimpleMath::Matrix::CreateRotationZ(angle);
      DirectX::SimpleMath::Matrix transform =
          DirectX::SimpleMath::Matrix::CreateTranslation(-0.5f, -0.5f, 0.0f) *
          rotation *
          DirectX::SimpleMath::Matrix::CreateTranslation(0.5f, 0.5f, 0.0f) *
          DirectX::SimpleMath::Matrix::CreateScale(scaleU, scaleV, 1.0f);

      matData.TexTransform = transform;
    } else {
      // Все остальные материалы без трансформации
      matData.TexTransform = DirectX::SimpleMath::Matrix::Identity;
    }

    mMaterialCB->CopyData(static_cast<int>(i), matData);
  }
}

void BoxApp::Draw(const GameTimer& gt) {
  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
  mRenderingSystem.Render(
      mCommandList.Get(), CurrentBackBufferView(),
      mSwapChainBuffers[mCurrBackBuffer].Get(), DepthStencilView(),
      mCbvHeap.Get(), mSamplerHeap.Get(), mCbvSrvDescriptorSize,
      mScreenViewport, mScissorRect, mVertexBufferView, mIndexBufferView,
      mModelGeometry, mMaterialCB.get(), mDepthStencilBuffer.Get(),
      mComposeCB->Resource()->GetGPUVirtualAddress());

  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);

  ThrowIfFailed(mSwapChain->Present(1, 0));
  mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

  FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y) {
  mLastMousePos.x = x;
  mLastMousePos.y = y;
  SetCapture(m_window.GetHWND());
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y) {
  if ((btnState & MK_LBUTTON) != 0) {
    float dx = static_cast<float>(x - mLastMousePos.x) * mMouseSensitivity;
    float dy = static_cast<float>(y - mLastMousePos.y) * mMouseSensitivity;

    mCamYaw += dx;
    mCamPitch += dy;

    const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
    if (mCamPitch > pitchLimit) mCamPitch = pitchLimit;
    if (mCamPitch < -pitchLimit) mCamPitch = -pitchLimit;

    mLastMousePos.x = x;
    mLastMousePos.y = y;
  }
}

int BoxApp::Run() {
  MSG msg = {0};
  mTimer.Reset();

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      switch (msg.message) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
          OnMouseDown(msg.wParam, GET_X_LPARAM(msg.lParam),
                      GET_Y_LPARAM(msg.lParam));
          break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
          OnMouseUp(msg.wParam, GET_X_LPARAM(msg.lParam),
                    GET_Y_LPARAM(msg.lParam));
          break;
        case WM_MOUSEMOVE:
          OnMouseMove(msg.wParam, GET_X_LPARAM(msg.lParam),
                      GET_Y_LPARAM(msg.lParam));
          break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      mTimer.Tick();

      if (!m_window.IsPaused()) {
        Update(mTimer);
        Draw(mTimer);
      } else {
        Sleep(100);
      }
    }
  }

  return (int)msg.wParam;
}

void BoxApp::CalculateFrameStats() {
  static int frameCnt = 0;
  static float timeElapsed = 0.0f;

  frameCnt++;

  if ((mTimer.TotalTime() - timeElapsed) >= 1.0f) {
    float fps = (float)frameCnt;
    float mspf = 1000.0f / fps;

    wstring fpsStr = std::to_wstring(fps);
    wstring mspfStr = std::to_wstring(mspf);

    wstring windowText =
        L"Direct3D 12 with Assimp    fps: " + fpsStr + L"   mspf: " + mspfStr;
    SetWindowText(m_window.GetHWND(), windowText.c_str());

    frameCnt = 0;
    timeElapsed += 1.0f;
  }
}

void BoxApp::FlushCommandQueue() {
  mCurrentFence++;

  ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

  if (mFence->GetCompletedValue() < mCurrentFence) {
    HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);

    if (eventHandle) {
      ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
    }
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE BoxApp::CurrentBackBufferView() const {
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
      mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer,
      mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE BoxApp::DepthStencilView() const {
  return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
