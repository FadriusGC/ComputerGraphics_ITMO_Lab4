#include "BoxApp.h"

#include "ModelLoader.h"
#include "ShaderHelper.h"

BoxApp::BoxApp(HINSTANCE hInstance) : m_window(), mTimer() {
  mWorld = DirectX::SimpleMath::Matrix::Identity;
  mView = DirectX::SimpleMath::Matrix::Identity;
  mProj = DirectX::SimpleMath::Matrix::Identity;
}

BoxApp::~BoxApp() { FlushCommandQueue(); }

bool BoxApp::Initialize() {
  if (!m_window.Initialize(GetModuleHandle(nullptr), WIDTH, HEIGHT,
                           L"Direct3D 12 with Assimp Model Loading")) {
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

  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

  BuildRootSignature();
  BuildShadersAndInputLayout();
  BuildConstantBuffers();
  BuildPSO();
  BuildBoxGeometry();  // загружаем модель

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

void BoxApp::BuildDescriptorHeaps() {
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
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
  cbvHeapDesc.NumDescriptors = 2;
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
  mObjectCB = std::unique_ptr<UploadBuffer<ObjectConstants>>(
      new UploadBuffer<ObjectConstants>(mDevice.Get(), 1, true));
  mLightCB = std::unique_ptr<UploadBuffer<LightConstants>>(
      new UploadBuffer<LightConstants>(mDevice.Get(), 1, true));

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescObject = {};
  cbvDescObject.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress();
  cbvDescObject.SizeInBytes = (sizeof(ObjectConstants) + 255) & ~255;

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescLight = {};
  cbvDescLight.BufferLocation = mLightCB->Resource()->GetGPUVirtualAddress();
  cbvDescLight.SizeInBytes = (sizeof(LightConstants) + 255) & ~255;

  mDevice->CreateConstantBufferView(
      &cbvDescObject, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

  CD3DX12_CPU_DESCRIPTOR_HANDLE lightCbvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvDescriptorSize);
  mDevice->CreateConstantBufferView(&cbvDescLight, lightCbvHandle);
}

void BoxApp::BuildRootSignature() {
  CD3DX12_ROOT_PARAMETER slotRootParameter[2];

  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

  CD3DX12_DESCRIPTOR_RANGE cbvTable1;
  cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
      2, slotRootParameter, 0, nullptr,
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
    mVSByteCode =
        ShaderHelper::CompileShader(L"BoxVertexShader.hlsl", "VS", "vs_5_0");
    mPSByteCode =
        ShaderHelper::CompileShader(L"BoxPixelShader.hlsl", "PS", "ps_5_0");
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
  // Укажите правильный путь к вашей модели
  std::string modelPath =
      "C:/Users/grish/source/repos/ComputerGraphics_ITMO_Lab4/sponza.obj";

  if (!ModelLoader::LoadModel(modelPath, mModelGeometry)) {
    MessageBoxA(nullptr, "Failed to load model. Using fallback cube.",
                "Warning", MB_OK);
    CreateFallbackCube();
  } else {
    char buf[256];
    sprintf_s(buf, "Loaded %zu vertices, %zu indices\n",
              mModelGeometry.Vertices.size(), mModelGeometry.Indices.size());
    OutputDebugStringA(buf);
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
  depthStencilDesc.Format = DepthStencilFormat;
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

  mDevice->CreateDepthStencilView(
      mDepthStencilBuffer.Get(), nullptr,
      mDsvHeap->GetCPUDescriptorHandleForHeapStart());

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
  mCommandList->ResourceBarrier(1, &barrier);
  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);
  FlushCommandQueue();
}

void BoxApp::Update(const GameTimer& gt) {
  float x = mRadius * sinf(mPhi) * cosf(mTheta);
  float z = mRadius * sinf(mPhi) * sinf(mTheta);
  float y = mRadius * cosf(mPhi);

  DirectX::SimpleMath::Vector3 pos(x, y, z);
  DirectX::SimpleMath::Vector3 target(0.0f, 0.0f, 0.0f);
  DirectX::SimpleMath::Vector3 up(0.0f, 1.0f, 0.0f);

  mView = DirectX::SimpleMath::Matrix::CreateLookAt(pos, target, up);

  float scale = 0.1f;  // подобрать надо
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
  lightConstants.CameraPosition = DirectX::SimpleMath::Vector4(x, y, z, 1.0f);
  mLightCB->CopyData(0, lightConstants);
}

void BoxApp::Draw(const GameTimer& gt) {
  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
      mSwapChainBuffers[mCurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  mCommandList->ResourceBarrier(1, &barrier1);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();

  const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
  mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  mCommandList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);

  mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

  ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvHeap.Get()};
  mCommandList->SetDescriptorHeaps(1, descriptorHeaps);

  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  mCommandList->SetGraphicsRootDescriptorTable(
      0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
  mCommandList->SetGraphicsRootDescriptorTable(
      1, CD3DX12_GPU_DESCRIPTOR_HANDLE(
             mCbvHeap->GetGPUDescriptorHandleForHeapStart(), 1,
             mCbvSrvDescriptorSize));

  mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
  mCommandList->IASetIndexBuffer(&mIndexBufferView);
  mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);

  auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      mSwapChainBuffers[mCurrBackBuffer].Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  mCommandList->ResourceBarrier(1, &barrier2);

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
    float dx = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(x - mLastMousePos.x));
    float dy = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(y - mLastMousePos.y));

    mTheta += dx;
    mPhi += dy;
    mPhi = clamp_val(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
  } else if ((btnState & MK_RBUTTON) != 0) {
    float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
    float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

    mRadius += dx - dy;
    mRadius = clamp_val(mRadius, 3.0f, 15.0f);
  }

  mLastMousePos.x = x;
  mLastMousePos.y = y;
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
