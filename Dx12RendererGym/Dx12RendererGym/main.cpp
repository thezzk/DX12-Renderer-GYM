#include "stdafx.h"

using namespace DirectX;



int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd)
{
    // create the window
    if (!InitializeWindow(hInstance, nShowCmd, FullScreen))
    {
        MessageBox(0, L"Window Init Failed!", L"Error", MB_OK);
        return 0;
    }
    // init d3d
    if (!InitD3D())
    {
        MessageBox(0, L"Failed to initialize d3d 12",
            L"Error", MB_OK);
        Cleanup();
        return 1;
    }

    mainloop();

    // wait for gpu to finish executing the command list before starting releasing everything
    WaitForPreviousFrame();

    // close the fence event
    CloseHandle(fenceEvent);

    Cleanup();

    return 0;
}

bool InitializeWindow(HINSTANCE hInstance,
    int ShowWnd,
    bool fullscreen)
{
    if (fullscreen)
    {
        HMONITOR hmon = MonitorFromWindow(hwnd,
            MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hmon, &mi);

        Width = mi.rcMonitor.right - mi.rcMonitor.left;
        Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }

    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WindowName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"Error registering class", L"Error", MB_OK|MB_ICONERROR);
        return false;
    }

    hwnd = CreateWindowEx(NULL,
        WindowName,
        WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        Width, Height,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hwnd)
    {
        MessageBox(NULL, L"Error creating window", L"Error", MB_OK|MB_ICONERROR);
        return false;
    }

    if (fullscreen)
    {
        SetWindowLong(hwnd, GWL_STYLE, 0);
    }

    ShowWindow(hwnd, ShowWnd);
    UpdateWindow(hwnd);

    return true;
}

void mainloop() {
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);

        }
        else
        {
            Update();
            Render();
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (MessageBox(0, L"Are you sure you want to exit?",
                L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
                DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd,
        msg,
        wParam,
        lParam);
}

bool InitD3DDevice()
{
    HRESULT hr;

    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        return false;
    }

    IDXGIAdapter1* adapter;

    int adapterIndex = 0;

    bool adapterFound = false;

    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // dont want a software device
            adapterIndex++;
            continue;
        }

        hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {
            adapterFound = true;
            break;
        }

        adapterIndex++;
    }
    if (!adapterFound)
    {
        return false;
    }

    // Create the device
    hr = D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    );
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitCommandQueue()
{
    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitCommandAllocators()
{
    HRESULT hr;

    for (int i = 0; i < frameBufferCount; ++i)
    {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
        if (FAILED(hr))
        {
            return false;
        }
    }

    return true;
}

bool InitCommandList()
{
    HRESULT hr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[0], NULL, IID_PPV_ARGS(&commandList));
    if (FAILED(hr))
    {
        return false;
    }
    return true;
}

bool InitSwapChain()
{
    HRESULT hr;

    // --  Create the Swap Chain --
    DXGI_MODE_DESC backBufferDesc = {};
    backBufferDesc.Width = Width;
    backBufferDesc.Height = Height;
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // multi-sampling
    sampleDesc = {};
    sampleDesc.Count = 1;

    // Describe and create the swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameBufferCount;
    swapChainDesc.BufferDesc = backBufferDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc = sampleDesc;
    swapChainDesc.Windowed = !FullScreen;

    IDXGISwapChain* tempSwapChain;

    hr = dxgiFactory->CreateSwapChain(commandQueue,
        &swapChainDesc,
        &tempSwapChain);

    swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    if (FAILED(hr))
    {
        return false;
    }
    return true;
}

bool InitDescriptorHeaps()
{
    HRESULT hr;
    // RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = frameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    if (FAILED(hr))
    {
        return false;
    }
    // depth stencil descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
    dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");
    if (FAILED(hr))
    {
        return false;
    }
    //Cerate descriptor heap refer to SRV
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap));
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitFenceAndEvent()
{
    HRESULT hr;

    for (int i = 0; i < frameBufferCount; ++i)
    {
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
        if (FAILED(hr))
        {
            return false;
        }
        fenceValue[i] = 0;
    }

    // create a handle to a fence event
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        return false;
    }

    return true;
}

bool InitRootSignature()
{
    HRESULT hr;

    // root descriptor
    D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
    rootCBVDescriptor.RegisterSpace = 0;
    rootCBVDescriptor.ShaderRegister = 0;

    // descriptor range
    D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
    descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorTableRanges[0].NumDescriptors = 1;
    descriptorTableRanges[0].BaseShaderRegister = 0;
    descriptorTableRanges[0].RegisterSpace = 0;
    descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // create a descriptor table
    D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
    descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges);
    descriptorTable.pDescriptorRanges = &descriptorTableRanges[0];

    // create a root parameter and fill it out
    D3D12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor = rootCBVDescriptor;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2nd root param
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable = descriptorTable;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // create a static sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters),
        rootParameters,
        1,
        &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ID3DBlob* signature;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    if (FAILED(hr))
    {
        return false;
    }

    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitResources()
{
    HRESULT hr;

    std::vector<Vertex> vList;
    std::vector<DWORD> iList;

    if (!loadMesh("teapot.obj", vList, iList))
    {
        return false;
    }

    // **Vertex Buffer**
    vBufferSize = vList.size() * sizeof(Vertex);
    CD3DX12_HEAP_PROPERTIES vertextHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC vertexHeapResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    device->CreateCommittedResource(
        &vertextHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &vertexHeapResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));
    vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    CD3DX12_HEAP_PROPERTIES uploadVHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // upload heap
    CD3DX12_RESOURCE_DESC uploadVHeapResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    ID3D12Resource* vBufferUploadHeap;
    device->CreateCommittedResource(
        &uploadVHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadVHeapResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(vList.data());
    vertexData.RowPitch = vBufferSize;
    vertexData.SlicePitch = vBufferSize;

    UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

    CD3DX12_RESOURCE_BARRIER vBufferBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    commandList->ResourceBarrier(1, &vBufferBarrier);

    // **Index Buffer**
    numCubeIndices = iList.size();
    iBufferSize = iList.size() * sizeof(DWORD);
    
    CD3DX12_HEAP_PROPERTIES indexHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC indexHeapResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);
    device->CreateCommittedResource(
        &indexHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &indexHeapResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&indexBuffer));
    indexBuffer->SetName(L"Index Buffer Resource Heap");

    ID3D12Resource* iBufferUploadHeap;
    CD3DX12_HEAP_PROPERTIES uploadIHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadIHeapResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);
    device->CreateCommittedResource(
        &uploadIHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadIHeapResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&iBufferUploadHeap));
    iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = reinterpret_cast<BYTE*>(iList.data());
    indexData.RowPitch = iBufferSize;
    indexData.SlicePitch = iBufferSize;

    UpdateSubresources(commandList, indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

    CD3DX12_RESOURCE_BARRIER iBufferBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    commandList->ResourceBarrier(1, &iBufferBarrier);

    // **Depth Buffer**
    depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES depthStencilHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC depthStencilHeapResourceDesc
        = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, Width, Height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    device->CreateCommittedResource(
        &depthStencilHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilHeapResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );

    // **Constant Buffer**
    for (int i = 0; i < frameBufferCount; ++i)
    {
        CD3DX12_HEAP_PROPERTIES constantBufferUploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC constantBufferUploadHeapResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
        hr = device->CreateCommittedResource(
            &constantBufferUploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &constantBufferUploadHeapResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBufferUploadHeaps[i]));
        constantBufferUploadHeaps[i]->SetName(L"Constant buffer Upload Resource Haep");

        ZeroMemory(&cbPerObject, sizeof(cbPerObject));

        CD3DX12_RANGE readRange(0, 0);

        hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbvGPUAddress[i]));

        memcpy(cbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject));
        memcpy(cbvGPUAddress[i] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));
    }

    // **Texture buffer**
    // Load image from file
    int imageBytesPerRow;
    BYTE* imageData;
    int imageSize = LoadImageDataFromFile(&imageData, textureDesc, L"img.jpg", imageBytesPerRow);
    if (imageSize <= 0)
    {
        Running = false;
        return false;
    }
    // create a heap for texture
    CD3DX12_HEAP_PROPERTIES textureBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    hr = device->CreateCommittedResource(
        &textureBufferHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureBuffer));
    if (FAILED(hr))
    {
        Running = false;
        return false;
    }
    textureBuffer->SetName(L"Texture Buffer Resource Heap");

    // create a heap for upload texture
    UINT64 textureUploadBufferSize;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    CD3DX12_HEAP_PROPERTIES textureBufferUploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC textureBufferUploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize);
    hr = device->CreateCommittedResource(
        &textureBufferUploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureBufferUploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureBufferUploadHeap));
    if (FAILED(hr))
    {
        Running = false;
        return false;
    }
    textureBufferUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

    // upload texture
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &imageData[0];
    textureData.RowPitch = imageBytesPerRow;
    textureData.SlicePitch = imageBytesPerRow * textureDesc.Height;

    UpdateSubresources(commandList, textureBuffer, textureBufferUploadHeap, 0, 0, 1, &textureData);

    CD3DX12_RESOURCE_BARRIER textureBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &textureBufferBarrier);
    delete imageData;


    return true;
}

bool InitViews()
{
    HRESULT hr;

    // ** RTV **
    // The length of RTV descriptor size, this may vary from device to device
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // Get the CPU handle of the first descriptor in the descriptro heap
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    // Create RTV for each buffer
    for (int i = 0; i < frameBufferCount; ++i)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr))
        {
            return false;
        }

        device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);

        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    // ** DSV **
    device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // ** SRV **
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(textureBuffer, &srvDesc, mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // vertex buffer view
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vBufferSize;
    // index buffer view
    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = iBufferSize;

    return true;
}

bool InitVSPS()
{
    HRESULT hr;
    // vertx shader
    ID3DBlob* vertexShader;
    ID3DBlob* errorBuff;
    hr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }
    vertexShaderBytecode = {};
    vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
    vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

    // pixel shader
    ID3DBlob* pixelShader;
    hr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }
    pixelShaderBytecode = {};
    pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
    pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

    return true;
}

bool InitPSO()
{
    HRESULT hr;

    // Input Layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
    inputLayoutDesc.pInputElementDescs = inputLayout;

    // PSO Desc
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS = vertexShaderBytecode;
    psoDesc.PS = pixelShaderBytecode;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc = sampleDesc; // must be the same as the swapchain and depth/stencil buffer
    psoDesc.SampleMask = 0xffffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // create the pso
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitD3D()
{
    HRESULT hr;

    if (!InitD3DDevice())
    {
        Running = false;
        return false;
    }
    if (!InitCommandQueue())
    {
        Running = false;
        return false;
    }
    if (!InitCommandAllocators())
    {
        Running = false;
        return false;
    }
    if (!InitCommandList())
    {
        Running = false;
        return false;
    }
    if (!InitSwapChain())
    {
        Running = false;
        return false;
    }
    if (!InitDescriptorHeaps())
    {
        Running = false;
        return false;
    }
    if (!InitFenceAndEvent())
    {
        Running = false;
        return false;
    }
    if (!InitRootSignature())
    {
        Running = false;
        return false;
    }
    if (!InitResources())
    {
        Running = false;
        return false;
    }
    if (!InitVSPS())
    {
        Running = false;
        return false;
    }
    if (!InitPSO())
    {
        Running = false;
        return false;
    }

    // Close Command List 
    commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    fenceValue[frameIndex]++;
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if (FAILED(hr))
    {
        Running = false;
        return false;
    }

    if (!InitViews())
    {
        Running = false;
        return false;
    }
    


    // -- Viewport and Scissor rect --
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = Width;
    viewport.Height = Height;
    viewport.MinDepth = 0.0;
    viewport.MaxDepth = 1.0f;

    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = Width;
    scissorRect.bottom = Height;
    
    // matrix!!!!
    XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(3.14f* (45.f / 180.f), (float)Width / float(Height), 0.1f, 1000.f);
    XMStoreFloat4x4(&cameraProjMat, tmpMat);
    
    cameraPosition = XMFLOAT4(0.0f, 2.0f, -40.0f, 0.0f);
    cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
    XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
    XMVECTOR cUp = XMLoadFloat4(&cameraUp);
    tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);
    XMStoreFloat4x4(&cameraViewMat, tmpMat);

    // cubes start pos
    // Mesh
    cube1Position = XMFLOAT4(0.0f, -7.0f, 0.0f, 0.0f);
    XMVECTOR posVec = XMLoadFloat4(&cube1Position);
    tmpMat = XMMatrixTranslationFromVector(posVec);
    XMStoreFloat4x4(&cube1RotMat, XMMatrixIdentity() * XMMatrixRotationX(3.14 * (270.0f/180.f)));
    XMStoreFloat4x4(&cube1WorldMat, tmpMat);
    //Cube2
    cube2PositionOffset = XMFLOAT4(1.5f, 0.0f, 0.0f, 0.0f);
    posVec = XMLoadFloat4(&cube2PositionOffset) + XMLoadFloat4(&cube1Position);
    tmpMat = XMMatrixTranslationFromVector(posVec);
    XMStoreFloat4x4(&cube2RotMat, XMMatrixIdentity());
    XMStoreFloat4x4(&cube2WorldMat, tmpMat);

    return true;
}


void Update()
{
    // cube1
    //XMMATRIX rotXMat = XMMatrixRotationX(0.003f);
    XMMATRIX rotYMat = XMMatrixRotationY(0.003f);
    //XMMATRIX rotZMat = XMMatrixRotationZ(0.003f);

    XMMATRIX rotMat = XMLoadFloat4x4(&cube1RotMat) * rotYMat;
    XMStoreFloat4x4(&cube1RotMat, rotMat);

    XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube1Position));

    XMMATRIX worldMat = rotMat * translationMat;

    XMStoreFloat4x4(&cube1WorldMat, worldMat);

    XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat);
    XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat);
    XMMATRIX wvpMat = XMLoadFloat4x4(&cube1WorldMat) * viewMat * projMat;
    XMMATRIX transposed = XMMatrixTranspose(wvpMat);
    XMStoreFloat4x4(&cbPerObject.wvpMat, transposed);

    memcpy(cbvGPUAddress[frameIndex], &cbPerObject, sizeof(cbPerObject));

    // cube2
    //rotXMat = XMMatrixRotationX(0.0003f);
    //rotYMat = XMMatrixRotationY(0.0002f);
    //rotZMat = XMMatrixRotationZ(0.0001f);

    ////rotMat = rotZMat * (XMLoadFloat4x4(&cube2RotMat) * (rotXMat * rotYMat));
    ////XMStoreFloat4x4(&cube2RotMat, rotMat);

    //XMMATRIX translateOffsetMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube2PositionOffset));
    //
    //XMMATRIX scaleMat = XMMatrixScaling(0.5f, 0.5f, 0.5f);

    //worldMat = scaleMat * translateOffsetMat * rotMat * translationMat;
    //XMStoreFloat4x4(&cube2WorldMat, worldMat);

    //wvpMat = XMLoadFloat4x4(&cube2WorldMat) * viewMat * projMat;
    //transposed = XMMatrixTranspose(wvpMat);
    //XMStoreFloat4x4(&cbPerObject.wvpMat, transposed);

    //memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));

}

void UpdatePipeline()
{
    HRESULT hr;

    WaitForPreviousFrame();

    hr = commandAllocator[frameIndex]->Reset();
    if (FAILED(hr))
    {
        Running = false;
    }

    hr = commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
    if (FALSE(hr))
    {
        Running = false;
    }


    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    // Get a handle to the depth/stencil buffer
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    
    commandList->SetGraphicsRootSignature(rootSignature);
  
    ID3D12DescriptorHeap* descriptorHeaps[] = { mainDescriptorHeap };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    commandList->SetGraphicsRootDescriptorTable(1, mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // draw triangle
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    
    // cube 1
    commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress());
    commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

    // cube 2
   // commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize);
    //commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);


    barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (FAILED(hr))
    {
        Running = false;
    }
}

void Render()
{
    HRESULT hr;

    UpdatePipeline();

    ID3D12CommandList* ppCommandLists[] = {commandList};

    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if (FAILED(hr))
    {
        Running = false;
    }

    hr = swapChain->Present(1, 0);
    if (FAILED(hr))
    {
        Running = false;
    }
}

void Cleanup()
{
    for (int i = 0; i < frameBufferCount; ++i)
    {
        frameIndex = i;
        WaitForPreviousFrame();
    }

    BOOL fs = false;
    if (swapChain->GetFullscreenState(&fs, NULL))
        swapChain->SetFullscreenState(false, NULL);

    SAFE_RELEASE(device);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(commandQueue);
    SAFE_RELEASE(rtvDescriptorHeap);
    SAFE_RELEASE(commandList);
    SAFE_RELEASE(dxgiFactory)
    for (int i = 0; i < frameBufferCount; ++i)
    {
        SAFE_RELEASE(renderTargets[i]);
        SAFE_RELEASE(commandAllocator[i]);
        SAFE_RELEASE(fence[i]);
    }

    SAFE_RELEASE(pipelineStateObject);
    SAFE_RELEASE(rootSignature);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(indexBuffer);
    SAFE_RELEASE(depthStencilBuffer);
    SAFE_RELEASE(dsDescriptorHeap);
    for (int i = 0; i < frameBufferCount; ++i)
    {
        SAFE_RELEASE(constantBufferUploadHeaps[i]);
    }
}

void WaitForPreviousFrame()
{
    HRESULT hr;

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex])
    {
        hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
        if (FAILED(hr))
        {
            Running = false;
        }

        WaitForSingleObject(fenceEvent, INFINITE);
    }

    fenceValue[frameIndex]++;
}

int  LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int& bytesPerRow)
{
    HRESULT hr;

    static IWICImagingFactory* wicFactory = NULL;

    IWICBitmapDecoder* wicDecoder = NULL;
    IWICBitmapFrameDecode* wicFrame = NULL;
    IWICFormatConverter* wicConverter = NULL;

    bool imageCoverted = false;

    if (wicFactory == NULL)
    {
        CoInitialize(NULL);

        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wicFactory));
        
        if (FAILED(hr)) return 0;
    }

    hr = wicFactory->CreateDecoderFromFilename(
            filename,
            NULL,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &wicDecoder);
    if (FAILED(hr)) return 0;

    hr = wicDecoder->GetFrame(0, &wicFrame);
    if (FAILED(hr)) return 0;

    WICPixelFormatGUID pixelFormat;
    hr = wicFrame->GetPixelFormat(&pixelFormat);
    if (FAILED(hr)) return 0;

    UINT textureWidth, textureHeight;
    hr = wicFrame->GetSize(&textureWidth, &textureHeight);
    if (FAILED(hr)) return 0;

    DXGI_FORMAT dxgiFormat = GetDXGIFormatFromWICFormat(pixelFormat);

    if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
    {
        WICPixelFormatGUID covertToPixelFormat = GetConvertToWICFormat(pixelFormat);

        if (covertToPixelFormat == GUID_WICPixelFormatDontCare) return 0;

        dxgiFormat = GetDXGIFormatFromWICFormat(covertToPixelFormat);

        hr = wicFactory->CreateFormatConverter(&wicConverter);
        if (FAILED(hr)) return 0;

        BOOL canCovert = FALSE;
        hr = wicConverter->CanConvert(pixelFormat, covertToPixelFormat, &canCovert);
        if (FAILED(hr) || !canCovert) return 0;

        hr = wicConverter->Initialize(wicFrame, covertToPixelFormat, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return 0;

        imageCoverted = true;
    }

    int bitsPerPixel = GetDXGIFormatBitsPerPixel(dxgiFormat);
    bytesPerRow = (textureWidth * bitsPerPixel) / 8;
    int imageSize = bytesPerRow * textureHeight;

    *imageData = (BYTE*)malloc(imageSize);
    
    if (imageCoverted)
    {
        hr = wicConverter->CopyPixels(0, bytesPerRow, imageSize, *imageData);
        if (FAILED(hr)) return 0;
    }
    else
    {
        hr = wicFrame->CopyPixels(0, bytesPerRow, imageSize, *imageData);
        if (FAILED(hr)) return 0;
    }

    resourceDescription = {};
    resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDescription.Alignment = 0;
    resourceDescription.Width = textureWidth;
    resourceDescription.Height = textureHeight;
    resourceDescription.DepthOrArraySize = 1;
    resourceDescription.MipLevels = 1;
    resourceDescription.Format = dxgiFormat;
    resourceDescription.SampleDesc.Count = 1;
    resourceDescription.SampleDesc.Quality = 0;
    resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    return imageSize;
}

bool loadMesh(std::string objfileName, std::vector<Vertex>& vertexList, std::vector<DWORD>& indexList)
{
    objl::Loader loader;
    bool loadout = loader.LoadFile(objfileName);

    if (!loadout || loader.LoadedMeshes.size() == 0)
    {
        return false;
    }
    objl::Mesh mesh = loader.LoadedMeshes[0];
        
    for (int i = 0; i < mesh.Vertices.size(); ++i)
    {
        Vertex v = {mesh.Vertices[i].Position.X, mesh.Vertices[i].Position.Y, mesh.Vertices[i].Position.Z, 
            mesh.Vertices[i].TextureCoordinate.X, mesh.Vertices[i].TextureCoordinate.Y};
        vertexList.push_back(v);
    }

    for (int i = 0; i < mesh.Indices.size(); ++i)
    {
        indexList.push_back(mesh.Indices[i]);
    }


    return true;
}