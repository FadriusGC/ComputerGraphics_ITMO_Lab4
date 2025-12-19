#include "BoxApp.h"

BoxApp::BoxApp(HINSTANCE hInstance) : m_window(), mTimer() {
  mWorld = DirectX::SimpleMath::Matrix::Identity;
  mView = DirectX::SimpleMath::Matrix::Identity;
  mProj = DirectX::SimpleMath::Matrix::Identity;
}

BoxApp::~BoxApp() { FlushCommandQueue(); }

bool BoxApp::Initialize() {
  if (!m_window.Initialize(GetModuleHandle(nullptr), WIDTH, HEIGHT,
                           L"Direct3D 12 Box with Phong Lighting")) {
    return false;
  }

  CreateDevice();
  CreateCommandObjects();
  CreateSwapChain();
  BuildDescriptorHeaps();
  CreateRTVs();
  CreateDepthStencil();

  // Создаем вьюпорт и ножницы
  mScreenViewport.TopLeftX = 0;
  mScreenViewport.TopLeftY = 0;
  mScreenViewport.Width = static_cast<float>(WIDTH);
  mScreenViewport.Height = static_cast<float>(HEIGHT);
  mScreenViewport.MinDepth = 0.0f;
  mScreenViewport.MaxDepth = 1.0f;

  mScissorRect = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};

  // Сброс командного списка для инициализации
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

  BuildRootSignature();
  BuildShadersAndInputLayout();
  BuildBoxGeometry();
  BuildConstantBuffers();
  BuildPSO();

  // Закрываем список команд
  ThrowIfFailed(mCommandList->Close());

  // Выполняем команды инициализации
  ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
  mCommandQueue->ExecuteCommandLists(1, cmdLists);

  FlushCommandQueue();

  // Проекционная матрица
  OnResize();

  return true;
}

void BoxApp::OnResize() {
  // Используем проекционную матрицу для левосторонней системы координат
  mProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
      0.25f * DirectX::XM_PI,
      static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 1000.0f);
}

void BoxApp::BuildDescriptorHeaps() {
  // Куча RTV
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

  // Куча DSV
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

  // Куча CBV (для константных буферов)
  D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
  cbvHeapDesc.NumDescriptors = 2;  // Два буфера: Object и Light
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(
      mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
  // Константный буфер для объекта (World и WorldViewProj)
  mObjectCB = std::unique_ptr<UploadBuffer<ObjectConstants>>(
      new UploadBuffer<ObjectConstants>(mDevice.Get(), 1, true));

  // Константный буфер для параметров света
  mLightCB = std::unique_ptr<UploadBuffer<LightConstants>>(
      new UploadBuffer<LightConstants>(mDevice.Get(), 1, true));

  // Описание CBV для ObjectConstants (register(b0))
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescObject = {};
  cbvDescObject.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress();
  cbvDescObject.SizeInBytes =
      (sizeof(ObjectConstants) + 255) & ~255;  // Выравнивание до 256 байт

  // Описание CBV для LightConstants (register(b1))
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescLight = {};
  cbvDescLight.BufferLocation = mLightCB->Resource()->GetGPUVirtualAddress();
  cbvDescLight.SizeInBytes =
      (sizeof(LightConstants) + 255) & ~255;  // Выравнивание до 256 байт

  // Создаем CBV для ObjectConstants в начале кучи
  mDevice->CreateConstantBufferView(
      &cbvDescObject, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

  // Создаем CBV для LightConstants со смещением
  CD3DX12_CPU_DESCRIPTOR_HANDLE lightCbvHandle(
      mCbvHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvDescriptorSize);
  mDevice->CreateConstantBufferView(&cbvDescLight, lightCbvHandle);
}

void BoxApp::BuildRootSignature() {
  // Создаем два слота: один для ObjectConstants (b0), другой для LightConstants
  // (b1)
  CD3DX12_ROOT_PARAMETER slotRootParameter[2];

  // Слот 0: ObjectConstants (b0)
  CD3DX12_DESCRIPTOR_RANGE cbvTable0;
  cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);

  // Слот 1: LightConstants (b1)
  CD3DX12_DESCRIPTOR_RANGE cbvTable1;
  cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
  slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

  // Описание корневой сигнатуры
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
  // Вершинный шейдер
  const char* vsCode =
      "struct VS_INPUT {\n"
      "    float3 Pos : POSITION;\n"
      "    float3 Normal : NORMAL;\n"
      "    float4 Color : COLOR;\n"
      "};\n"
      "struct VS_OUTPUT {\n"
      "    float4 Pos : SV_POSITION;\n"
      "    float3 WorldPos : WORLDPOS;\n"  // Позиция в мировом пространстве
      "    float3 Normal : NORMAL;\n"      // Нормаль в мировом пространстве
      "    float4 Color : COLOR;\n"
      "};\n"
      "cbuffer cbPerObject : register(b0) {\n"
      "    float4x4 gWorld;\n"          // Матрица мира
      "    float4x4 gWorldViewProj;\n"  // Матрица проекции
      "};\n"
      "cbuffer cbLight : register(b1) {\n"
      "    float4 gLightPosition;\n"
      "    float4 gLightColor;\n"
      "    float4 gCameraPosition;\n"
      "};\n"
      "VS_OUTPUT VS(VS_INPUT input) {\n"
      "    VS_OUTPUT output;\n"
      "    output.Pos = mul(float4(input.Pos, 1.0f), gWorldViewProj);\n"
      "    output.WorldPos = mul(float4(input.Pos, 1.0f), gWorld).xyz;\n"
      "    // Трансформируем нормаль: используем матрицу мира (без "
      "транспонирования, так как нет масштабирования)\n"
      "    output.Normal = mul(float4(input.Normal, 0.0f), gWorld).xyz;\n"
      "    output.Color = input.Color;\n"
      "    return output;\n"
      "}";

  // Пиксельный шейдер
  const char* psCode =
      "struct PS_INPUT {\n"
      "    float4 Pos : SV_POSITION;\n"
      "    float3 WorldPos : WORLDPOS;\n"
      "    float3 Normal : NORMAL;\n"
      "    float4 Color : COLOR;\n"
      "};\n"
      "cbuffer cbLight : register(b1) {\n"
      "    float4 gLightPosition;\n"
      "    float4 gLightColor;\n"
      "    float4 gCameraPosition;\n"
      "};\n"
      "float4 PS(PS_INPUT input) : SV_Target {\n"
      "    // Нормализуем нормаль\n"
      "    float3 normal = normalize(input.Normal);\n"
      "    \n"
      "    // Направление света\n"
      "    float3 lightDir = normalize(gLightPosition.xyz - input.WorldPos);\n"
      "    \n"
      "    // Диффузное освещение\n"
      "    float diffuse = max(dot(normal, lightDir), 0.0f);\n"
      "    float3 diffuseColor = diffuse * gLightColor.rgb * input.Color.rgb;\n"
      "    \n"
      "    // Амбиентное освещение\n"
      "    float3 ambient = 0.1f * gLightColor.rgb * input.Color.rgb;\n"
      "    \n"
      "    // Специфическое освещение\n"
      "    float3 viewDir = normalize(gCameraPosition.xyz - input.WorldPos);\n"
      "    float3 reflectDir = reflect(-lightDir, normal);\n"
      "    float specular = pow(max(dot(reflectDir, viewDir), 0.0f), 32.0f);\n"
      "    float3 specularColor = specular * gLightColor.rgb * 0.5f;\n"
      "    \n"
      "    float3 finalColor = ambient + diffuseColor + specularColor;\n"
      "    return float4(finalColor, 1.0f);\n"
      "}";

  ComPtr<ID3DBlob> errorBlob;

  // Компилируем шейдеры
  HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
                          "VS", "vs_5_0", 0, 0, &mVSByteCode, &errorBlob);

  if (FAILED(hr)) {
    if (errorBlob != nullptr) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
  }

  hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "PS",
                  "ps_5_0", 0, 0, &mPSByteCode, &errorBlob);

  if (FAILED(hr)) {
    if (errorBlob != nullptr) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
  }

  // Входной лейаут
  mInputLayout = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                  {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24,
                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void BoxApp::BuildBoxGeometry() {
  // Создаем 24 вершины (по 4 на грань) для корректных нормалей
  std::array<Vertex, 24> vertices = {
      // Front (z = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 0: белый
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 1: черный
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 2: красный
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, 0.0f, -1.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 3: зеленый

      // Back (z = 1)
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 4: синий
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 5: желтый
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 6: голубой
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, 0.0f, 1.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 7: пурпурный

      // Left (x = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 8
      Vertex({{-1.0f, 1.0f, -1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 9
      Vertex({{-1.0f, 1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 10
      Vertex({{-1.0f, -1.0f, 1.0f},
              {-1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 11

      // Right (x = 1)
      Vertex({{1.0f, -1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 12
      Vertex({{1.0f, 1.0f, -1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 13
      Vertex({{1.0f, 1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 14
      Vertex({{1.0f, -1.0f, 1.0f},
              {1.0f, 0.0f, 0.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 15

      // Top (y = 1)
      Vertex({{-1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}}),  // 16
      Vertex({{-1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 1.0f, 0.0f, 1.0f}}),  // 17
      Vertex({{1.0f, 1.0f, 1.0f},
              {0.0f, 1.0f, 0.0f},
              {0.0f, 1.0f, 1.0f, 1.0f}}),  // 18
      Vertex({{1.0f, 1.0f, -1.0f},
              {0.0f, 1.0f, 0.0f},
              {1.0f, 0.0f, 0.0f, 1.0f}}),  // 19

      // Bottom (y = -1)
      Vertex({{-1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 1.0f, 1.0f, 1.0f}}),  // 20
      Vertex({{-1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 1.0f}}),  // 21
      Vertex({{1.0f, -1.0f, 1.0f},
              {0.0f, -1.0f, 0.0f},
              {1.0f, 0.0f, 1.0f, 1.0f}}),  // 22
      Vertex({{1.0f, -1.0f, -1.0f},
              {0.0f, -1.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 1.0f}}),  // 23
  };

  // Индексы: по 6 индексов на грань (36 индексов всего)
  std::array<uint16_t, 36> indices = {// Front face
                                      0, 1, 2, 0, 2, 3,
                                      // Back face
                                      4, 5, 6, 4, 6, 7,
                                      // Left face
                                      8, 9, 10, 8, 10, 11,
                                      // Right face
                                      12, 13, 14, 12, 14, 15,
                                      // Top face
                                      16, 17, 18, 16, 18, 19,
                                      // Bottom face
                                      20, 21, 22, 20, 22, 23};

  mVertexBufferByteSize = (UINT)(vertices.size() * sizeof(Vertex));
  mIndexBufferByteSize = (UINT)(indices.size() * sizeof(uint16_t));
  mIndexCount = (UINT)indices.size();

  // Создаем вершинный буфер в GPU
  {
    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mVertexBufferByteSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mVertexBufferGPU)));

    // Создаем промежуточный буфер для загрузки
    D3D12_HEAP_PROPERTIES uploadHeapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mVertexBufferUploader)));

    // Копируем данные
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.data();
    vertexData.RowPitch = mVertexBufferByteSize;
    vertexData.SlicePitch = mVertexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mVertexBufferGPU.Get(),
                       mVertexBufferUploader.Get(), 0, 0, 1, &vertexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mVertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    // Создаем вью вершинного буфера
    mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = mVertexBufferByteSize;
    mVertexBufferView.StrideInBytes = sizeof(Vertex);
  }

  // Создаем индексный буфер в GPU
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
    indexData.pData = indices.data();
    indexData.RowPitch = mIndexBufferByteSize;
    indexData.SlicePitch = mIndexBufferByteSize;

    UpdateSubresources(mCommandList.Get(), mIndexBufferGPU.Get(),
                       mIndexBufferUploader.Get(), 0, 0, 1, &indexData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mIndexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER);
    mCommandList->ResourceBarrier(1, &barrier);

    // Создаем вью индексного буфера
    mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = mIndexBufferByteSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
  }
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
  rasterizerDesc.FrontCounterClockwise = FALSE;

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
  // Преобразуем сферические координаты в декартовы
  float x = mRadius * sinf(mPhi) * cosf(mTheta);
  float z = mRadius * sinf(mPhi) * sinf(mTheta);
  float y = mRadius * cosf(mPhi);

  // Строим матрицу вида
  DirectX::SimpleMath::Vector3 pos(x, y, z);
  DirectX::SimpleMath::Vector3 target(0.0f, 0.0f, 0.0f);
  DirectX::SimpleMath::Vector3 up(0.0f, 1.0f, 0.0f);

  mView = DirectX::SimpleMath::Matrix::CreateLookAt(pos, target, up);

  // Комбинируем матрицы
  DirectX::SimpleMath::Matrix worldViewProj = mWorld * mView * mProj;

  // Обновляем константный буфер объекта
  ObjectConstants objConstants;
  objConstants.World = mWorld;
  objConstants.WorldViewProj = worldViewProj.Transpose();
  mObjectCB->CopyData(0, objConstants);

  // Обновляем константный буфер света
  LightConstants lightConstants;
  // Позиция света (вверху справа)
  lightConstants.LightPosition =
      DirectX::SimpleMath::Vector4(3.0f, 3.0f, 3.0f, 1.0f);
  // Белый свет
  lightConstants.LightColor =
      DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  // Позиция камеры для вычисления viewDir
  lightConstants.CameraPosition = DirectX::SimpleMath::Vector4(x, y, z, 1.0f);
  mLightCB->CopyData(0, lightConstants);
}

void BoxApp::Draw(const GameTimer& gt) {
  ThrowIfFailed(mCommandAllocator->Reset());
  ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  // Переход заднего буфера в состояние рендер-таргета
  auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
      mSwapChainBuffers[mCurrBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  mCommandList->ResourceBarrier(1, &barrier1);

  // Очистка буферов
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();

  const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
  mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  mCommandList->ClearDepthStencilView(
      dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);

  // Устанавливаем рендер-таргеты
  mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

  // Устанавливаем дескрипторные кучи
  ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvHeap.Get()};
  mCommandList->SetDescriptorHeaps(1, descriptorHeaps);

  // Устанавливаем корневую сигнатуру
  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  // Привязываем константные буферы: Object (b0) и Light (b1)
  mCommandList->SetGraphicsRootDescriptorTable(
      0,
      mCbvHeap->GetGPUDescriptorHandleForHeapStart());  // b0: ObjectConstants
  mCommandList->SetGraphicsRootDescriptorTable(
      1, CD3DX12_GPU_DESCRIPTOR_HANDLE(
             mCbvHeap->GetGPUDescriptorHandleForHeapStart(), 1,
             mCbvSrvDescriptorSize));  // b1: LightConstants

  // Устанавливаем вершинный и индексный буферы
  mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
  mCommandList->IASetIndexBuffer(&mIndexBufferView);
  mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Рисуем куб
  mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);

  // Переход обратно в PRESENT
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
    // Вращение камеры
    float dx = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(x - mLastMousePos.x));
    float dy = DirectX::XMConvertToRadians(
        0.25f * static_cast<float>(y - mLastMousePos.y));

    mTheta += dx;
    mPhi += dy;

    // Ограничиваем угол phi
    mPhi = clamp_val(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
  } else if ((btnState & MK_RBUTTON) != 0) {
    // Приближение/отдаление
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
      // Обработка сообщений мыши
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
        L"Direct3D 12 Box    fps: " + fpsStr + L"   mspf: " + mspfStr;
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
