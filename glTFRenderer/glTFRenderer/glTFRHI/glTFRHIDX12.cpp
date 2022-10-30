#include "glTFRHIDX12.h"
#include "d3dx12.h"
#include <d3dcompiler.h>

#define SAFE_RELEASE(x) if(x) { (x)->Release(); (x) = NULL; }

// direct3d stuff
bool glTFRHIDX12::Running = true;

UINT glTFRHIDX12::width = 0;
    
UINT glTFRHIDX12::height = 0;

HWND glTFRHIDX12::hwnd = 0;

bool glTFRHIDX12::fullScreen = false;

IDXGIFactory4* glTFRHIDX12::dxgiFactory = nullptr;

ID3D12Device* glTFRHIDX12::device = nullptr; // direct3d device

IDXGISwapChain3* glTFRHIDX12::swapChain = nullptr; // swapchain used to switch between render targets

ID3D12CommandQueue* glTFRHIDX12::commandQueue = nullptr; // container for command lists

ID3D12DescriptorHeap* glTFRHIDX12::rtvDescriptorHeap = nullptr; // a descriptor heap to hold resources like the render targets

ID3D12Resource* glTFRHIDX12::renderTargets[frameBufferCount] = {}; // number of render targets equal to buffer count

ID3D12CommandAllocator* glTFRHIDX12::commandAllocator[frameBufferCount] = {}; // we want enough allocators for each buffer * number of threads (we only have one thread)

ID3D12GraphicsCommandList* glTFRHIDX12::commandList = nullptr; // a command list we can record commands into, then execute them to render the frame

ID3D12Fence* glTFRHIDX12::fence[frameBufferCount] = {};    // an object that is locked while our command list is being executed by the gpu. We need as many 
//as we have allocators (more if we want to know when the gpu is finished with an asset)

HANDLE glTFRHIDX12::fenceEvent  = nullptr; // a handle to an event when our fence is unlocked by the gpu

UINT64 glTFRHIDX12::fenceValue[frameBufferCount] = {}; // this value is incremented each frame. each fence will have its own value

int glTFRHIDX12::frameIndex = 0; // current rtv we are on

int glTFRHIDX12::rtvDescriptorSize = 0; // size of the rtv descriptor on the device (all front and back buffers will be the same size)

ID3D12PipelineState* glTFRHIDX12::pipelineStateObject = nullptr; // pso containing a pipeline state

ID3D12RootSignature* glTFRHIDX12::rootSignature = nullptr; // root signature defines data shaders will access

D3D12_VIEWPORT glTFRHIDX12::viewport = {}; // area that output from rasterizer will be stretched to.

D3D12_RECT glTFRHIDX12::scissorRect = {}; // the area to draw in. pixels outside that area will not be drawn onto

ID3D12Resource* glTFRHIDX12::vertexBuffer = nullptr; // a default buffer in GPU memory that we will load vertex data for our triangle into

D3D12_VERTEX_BUFFER_VIEW glTFRHIDX12::vertexBufferView = {}; // a structure containing a pointer to the vertex data in gpu memory
// the total size of the buffer, and the size of each element (vertex)

DXGI_SAMPLE_DESC glTFRHIDX12::swapChainSampleDesc = {};

D3D12_SHADER_BYTECODE glTFRHIDX12::vertexShaderBytecode = {};
    
D3D12_SHADER_BYTECODE glTFRHIDX12::pixelShaderBytecode = {};

ID3D12Resource* glTFRHIDX12::indexBuffer = nullptr;

D3D12_INDEX_BUFFER_VIEW glTFRHIDX12::indexBufferView = {};

ID3D12DescriptorHeap* glTFRHIDX12::dsDescriptorHeap = nullptr;

ID3D12Resource* glTFRHIDX12::depthStencilBuffer = nullptr;

int glTFRHIDX12::dsDescriptorSize = 0;

struct Vertex
{
    float data[3];
    float color[4];
};

bool glTFRHIDX12::InitD3D(UINT Width, UINT Height, HWND Hwnd, bool FullScreen)
{
    // Setup window config parameters
    width = Width;
    height = Height;
    hwnd = Hwnd;
    fullScreen = FullScreen;
    
    if (!CreateFactory())
    {
        return false;
    }

    if (!CreateDevice())
    {
        return false;
    }

    if (!CreateCommandQueue())
    {
        return false;
    }

    if (!CreateSwapChain())
    {
        return false;
    }

    if (!CreateDescriptorHeap())
    {
        return false;
    }

    if (!CreateCommandAllocator())
    {
        return false;
    }

    if (!CreateRootSignature())
    {
        return false;
    }

    if (!CompileAndCreateShaderByteCode())
    {
        return false;
    }

    if (!CreateFenceEvents())
    {
        return false;
    }
    
    if (!CreatePipelineStateObject())
    {
        return false;
    }
    
    return true;
}

void glTFRHIDX12::Update()
{
    
}

void glTFRHIDX12::UpdatePipeline()
{
    HRESULT hr;

    // We have to wait for the gpu to finish with the command allocator before we reset it
    WaitForPreviousFrame();

    // we can only reset an allocator once the gpu is done with it
    // resetting an allocator frees the memory that the command list was stored in
    hr = commandAllocator[frameIndex]->Reset();
    if (FAILED(hr))
    {
        Running = false;
    }

    // reset the command list. by resetting the command list we are putting it into
    // a recording state so we can start recording commands into the command allocator.
    // the command allocator that we reference here may have multiple command lists
    // associated with it, but only one can be recording at any time. Make sure
    // that any other command lists associated to this command allocator are in
    // the closed state (not recording).
    // Here you will pass an initial pipeline state object as the second parameter,
    // but in this tutorial we are only clearing the rtv, and do not actually need
    // anything but an initial default pipeline, which is what we get by setting
    // the second parameter to NULL
    hr = commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
    if (FAILED(hr))
    {
        Running = false;
    }

    // here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)

    // transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
    auto transition_rt = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &transition_rt);

    // here we again get the handle to our current render target view so we can set it as the render target in the output merger stage of the pipeline
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

    // set the render target for the output merger stage (the output of the pipeline)
    // commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsHandle);
    
    // Clear the render target by using the ClearRenderTargetView command
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    
    // draw triangle
    commandList->SetGraphicsRootSignature(rootSignature); // set the root signature
    commandList->RSSetViewports(1, &viewport); // set the viewports
    commandList->RSSetScissorRects(1, &scissorRect); // set the scissor rects
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // set the vertex buffer (using the vertex buffer view)
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
    commandList->DrawIndexedInstanced(6, 1, 0, 4, 0);
    
    // transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
    // warning if present is called on the render target when it's not in the present state
    auto transition_present = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &transition_present);

    hr = commandList->Close();
    if (FAILED(hr))
    {
        Running = false;
    }
}

void glTFRHIDX12::Render()
{
    HRESULT hr;

    UpdatePipeline(); // update the pipeline by sending commands to the commandqueue

    // create an array of command lists (only one command list here)
    ID3D12CommandList* ppCommandLists[] = { commandList };

    // execute the array of command lists
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // this command goes in at the end of our command queue. we will know when our command queue 
    // has finished because the fence value will be set to "fenceValue" from the GPU since the command
    // queue is being executed on the GPU
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if (FAILED(hr))
    {
        Running = false;
    }

    // present the current backbuffer
    hr = swapChain->Present(0, 0);
    if (FAILED(hr))
    {
        Running = false;
    }
}

void glTFRHIDX12::Cleanup()
{
    // wait for the gpu to finish all frames
    for (int i = 0; i < frameBufferCount; ++i)
    {
        WaitForPreviousFrame(i);
    }
    
    // get swapchain out of full screen before exiting
    BOOL fs = false;
    if (swapChain->GetFullscreenState(&fs, NULL))
        swapChain->SetFullscreenState(false, NULL);

    SAFE_RELEASE(device);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(commandQueue);
    SAFE_RELEASE(rtvDescriptorHeap);
    SAFE_RELEASE(commandList);

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
}

void glTFRHIDX12::WaitForPreviousFrame(int previous_frame_index)
{
    HRESULT hr;

    // swap the current rtv buffer index so we draw on the correct buffer
    frameIndex = previous_frame_index;

    // if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
    // the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
    if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex])
    {
        // we have the fence create an event which is signaled once the fence's current value is "fenceValue"
        hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
        if (FAILED(hr))
        {
            Running = false;
        }

        // We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
        // has reached "fenceValue", we know the command queue has finished executing
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // increment fenceValue for next frame
    fenceValue[frameIndex]++;
}

bool glTFRHIDX12::CreateFactory()
{
    // -- Create the Device -- //
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}

bool glTFRHIDX12::CreateDevice()
{
    IDXGIAdapter1* adapter; // adapters are the graphics card (this includes the embedded graphics on the motherboard)

    int adapterIndex = 0; // we'll start looking for directx 12  compatible graphics devices starting at index 0

    bool adapterFound = false; // set this to true when a good one was found

    // find first hardware gpu that supports d3d 12
    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // we dont want a software device
            adapterIndex++; // add this line here. Its not currently in the downloadable project
            continue;
        }

        // we want a device that is compatible with direct3d 12 (feature level 11 or higher)
        HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
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
    HRESULT hr = D3D12CreateDevice(
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

bool glTFRHIDX12::CreateCommandQueue()
{
    // -- Create the Command Queue -- //
    D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // we will be using all the default values

    HRESULT hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue)); // create the command queue
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}

bool glTFRHIDX12::CreateSwapChain()
{
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC backBufferDesc = {}; // this is to describe our display mode
    backBufferDesc.Width = width; // buffer width
    backBufferDesc.Height = height; // buffer height
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    swapChainSampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameBufferCount; // number of buffers we have
    swapChainDesc.BufferDesc = backBufferDesc; // our back buffer description
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapChainDesc.OutputWindow = hwnd; // handle to our window
    swapChainDesc.SampleDesc = swapChainSampleDesc; // our multi-sampling description
    swapChainDesc.Windowed = !fullScreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps
    
    IDXGISwapChain* tempSwapChain;
    
    dxgiFactory->CreateSwapChain(
        commandQueue, // the queue will be flushed once the swap chain is created
        &swapChainDesc, // give it the swap chain description we created above
        &tempSwapChain // store the created swap chain in a temp IDXGISwapChain interface
        );
    
    swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);
    
    frameIndex = swapChain->GetCurrentBackBufferIndex();
    
    return true;
}

bool glTFRHIDX12::CreateDescriptorHeap()
{
    // -- Create the Back Buffers (render target views) Descriptor Heap -- //

    // describe an rtv descriptor heap and create
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = frameBufferCount; // number of descriptors for this heap.
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // this heap is a render target view heap

    // This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline
    // otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    if (FAILED(hr))
    {
        return false;
    }
    
    // get the size of a descriptor in this heap (this is a rtv heap, so only rtv descriptors should be stored in it.
    // descriptor sizes may vary from device to device, which is why there is no set size and we must ask the 
    // device to give us the size. we will use this size to increment a descriptor handle offset
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
    // but we cannot literally use it like a c++ pointer.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each buffer (double buffering is two buffers, tripple buffering is 3).
    for (int i = 0; i < frameBufferCount; i++)
    {
        // first we get the n'th buffer in the swap chain and store it in the n'th
        // position of our ID3D12Resource array
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr))
        {
            return false;
        }

        // the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
        device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);

        // we increment the rtv handle by the rtv descriptor size we got above
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    // create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
    if (FAILED(hr))
    {
        Running = false;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES default_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC dsResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    device->CreateCommittedResource(
        &default_heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &dsResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
        );
    dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");
    dsDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    
    device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    
    return true;
}

bool glTFRHIDX12::CreateCommandAllocator()
{
    // -- Create the Command Allocators -- //
    for (int i = 0; i < frameBufferCount; i++)
    {
        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
        if (FAILED(hr))
        {
            return false;
        }
    }

    // create the command list with the first allocator
    HRESULT hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[0], nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr))
    {
        return false;
    }

    // command lists are created in the recording state. our main loop will set it up for recording again so close it now
    commandList->Close();
    
    return true;
}

bool glTFRHIDX12::CreateRootSignature()
{
    // create root signature
    HRESULT hr;
    
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

bool glTFRHIDX12::CompileAndCreateShaderByteCode()
{
    // create vertex and pixel shaders
    HRESULT hr;
    
    // when debugging, we can compile the shader files at runtime.
    // but for release versions, we can compile the hlsl shaders
    // with fxc.exe to create .cso files, which contain the shader
    // bytecode. We can load the .cso files at runtime to get the
    // shader bytecode, which of course is faster than compiling
    // them at runtime

    // compile vertex shader
    ID3DBlob* vertexShader; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff; // a buffer holding the error data if any
    hr = D3DCompileFromFile(L"glTFShaders/vertexShader.hlsl",
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vertexShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }

    // fill out a shader bytecode structure, which is basically just a pointer
    // to the shader bytecode and the size of the shader bytecode
    vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
    vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

    // compile pixel shader
    ID3DBlob* pixelShader;
    hr = D3DCompileFromFile(L"glTFShaders/pixelShader.hlsl",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &pixelShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }

    // fill out shader bytecode structure for pixel shader
    pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
    pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

    return true;
}

bool glTFRHIDX12::CreatePipelineStateObject()
{
    // create a pipeline state object (PSO)
    HRESULT hr;
    
    // create input layout

    // The input layout is used by the Input Assembler so that it knows
    // how to read the vertex data bound to it.

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // fill out an input layout description structure
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    // we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
    inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
    inputLayoutDesc.pInputElementDescs = inputLayout;
    
    // In a real application, you will have many pso's. for each different shader
    // or different combinations of shaders, different blend states or different rasterizer states,
    // different topology types (point, line, triangle, patch), or a different number
    // of render targets you will need a pso

    // VS is the only required shader for a pso. You might be wondering when a case would be where
    // you only set the VS. It's possible that you have a pso that only outputs data with the stream
    // output, and not on a render target, which means you would not need anything after the stream
    // output.

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
    psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    psoDesc.pRootSignature = rootSignature; // the root signature that describes the input data this pso needs
    psoDesc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
    psoDesc.PS = pixelShaderBytecode; // same as VS but for pixel shader
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the render target
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    
    psoDesc.SampleDesc = swapChainSampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
    psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1; // we are only binding one render target

    // create the pso
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
    if (FAILED(hr))
    {
        return false;
    }

    // Create vertex buffer

    // a triangle
    Vertex vList[] = {
        // first quad (closer to camera, blue)
        { -0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },

        // second quad (further from camera, green)
        { -0.75f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        {   0.0f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.75f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        {   0.0f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f }
    };

    int vBufferSize = sizeof(vList);

    // create default heap
    // default heap is memory on the GPU. Only the GPU has access to this memory
    // To get data into this heap, we will have to upload the data using
    // an upload heap
    CD3DX12_HEAP_PROPERTIES default_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC buffer_heap_properties = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    device->CreateCommittedResource(
        &default_heap_properties, // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &buffer_heap_properties, // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
                                        // from the upload heap to this heap
        nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
        IID_PPV_ARGS(&vertexBuffer));

    // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
    vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    // create upload heap
    // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
    // We will upload the vertex buffer using this heap to the default heap
    ID3D12Resource* vBufferUploadHeap;
    CD3DX12_HEAP_PROPERTIES upload_heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    device->CreateCommittedResource(
        &upload_heap_properties, // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &buffer_heap_properties, // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(vList); // pointer to our vertex array
    vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
    vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

    commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
    
    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

    // transition the vertex buffer data from copy destination state to vertex buffer state
    CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER); 
    commandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    // Create index buffer

    // a quad (2 triangles)
    DWORD iList[] = {
        0, 1, 2, // first triangle
        0, 3, 1 // second triangle
    };

    int iBufferSize = sizeof(iList);

    // create default heap to hold index buffer
    CD3DX12_RESOURCE_DESC indexBufferSizeDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);
    device->CreateCommittedResource(
        &default_heap_properties, // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &indexBufferSizeDesc, // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
        nullptr, // optimized clear value must be null for this type of resource
        IID_PPV_ARGS(&indexBuffer));

    // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
    vertexBuffer->SetName(L"Index Buffer Resource Heap");

    // create upload heap to upload index buffer
    ID3D12Resource* iBufferUploadHeap;
    device->CreateCommittedResource(
        &upload_heap_properties, // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &indexBufferSizeDesc, // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&iBufferUploadHeap));
    iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = reinterpret_cast<BYTE*>(iList); // pointer to our index array
    indexData.RowPitch = iBufferSize; // size of all our index buffer
    indexData.SlicePitch = iBufferSize; // also the size of our index buffer

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(commandList, indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

    // transition the vertex buffer data from copy destination state to vertex buffer state
    CD3DX12_RESOURCE_BARRIER indexBufferTransitionFromCopyDestToIndexBuffer = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    commandList->ResourceBarrier(1, &indexBufferTransitionFromCopyDestToIndexBuffer);

    // Now we execute the command list to upload the initial assets (triangle data)
    commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
    fenceValue[frameIndex]++;
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if (FAILED(hr))
    {
        Running = false;
    }

    // create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vBufferSize;

    // create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
    indexBufferView.SizeInBytes = iBufferSize;

    
    // Fill out the Viewport
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Fill out a scissor rect
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = width;
    scissorRect.bottom = height;
    
    return true;
}

bool glTFRHIDX12::CreateFenceEvents()
{
    // -- Create a Fence & Fence Event -- //

    // create the fences
    for (int i = 0; i < frameBufferCount; i++)
    {
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
        if (FAILED(hr))
        {
            return false;
        }
        fenceValue[i] = 0; // set the initial fence value to 0
    }

    // create a handle to a fence event
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        return false;
    }
    
    return true;
}

