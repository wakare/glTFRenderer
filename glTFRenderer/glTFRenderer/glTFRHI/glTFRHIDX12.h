#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>

class glTFRHIDX12
{
public:
    // function declarations
    static bool InitD3D(UINT Width, UINT Height, HWND hwnd, bool FullScreen); // initializes direct3d 12

    static void Update(); // update the game logic

    static void UpdatePipeline(); // update the direct3d pipeline (update command lists)

    static void Render(); // execute the command list

    static void Cleanup(); // release com ojects and clean up memory

    static void WaitForPreviousFrame(); // wait until gpu is finished with command list

    static bool Running;
    
private:
    static bool CreateFactory();
    
    static bool CreateDeviceDX12();
    
    static bool CreateCommandQueue();
    
    static bool CreateSwapChain();
    
    static bool CreateDescriptorHeap();
    
    static bool CreateCommandAllocator();
    
    static bool CreateRootSignature();
    
    static bool CompileAndCreateShaderByteCode();
    
    static bool CreatePipelineStateObject();
    
    static bool CreateFenceEvents();

    // direct3d stuff
    static constexpr int frameBufferCount = 3; // number of buffers we want, 2 for double buffering, 3 for tripple buffering

    static UINT width;
    
    static UINT height;

    static HWND hwnd;

    static bool fullScreen;
    
    static IDXGIFactory4* dxgiFactory;
    
    static ID3D12Device* device; // direct3d device

    static IDXGISwapChain3* swapChain; // swapchain used to switch between render targets

    static ID3D12CommandQueue* commandQueue; // container for command lists

    static ID3D12DescriptorHeap* rtvDescriptorHeap; // a descriptor heap to hold resources like the render targets

    static ID3D12Resource* renderTargets[frameBufferCount]; // number of render targets equal to buffer count

    static ID3D12CommandAllocator* commandAllocator[frameBufferCount]; // we want enough allocators for each buffer * number of threads (we only have one thread)

    static ID3D12GraphicsCommandList* commandList; // a command list we can record commands into, then execute them to render the frame

    static ID3D12Fence* fence[frameBufferCount];    // an object that is locked while our command list is being executed by the gpu. We need as many 
    //as we have allocators (more if we want to know when the gpu is finished with an asset)

    static HANDLE fenceEvent; // a handle to an event when our fence is unlocked by the gpu

    static UINT64 fenceValue[frameBufferCount]; // this value is incremented each frame. each fence will have its own value

    static int frameIndex; // current rtv we are on

    static int rtvDescriptorSize; // size of the rtv descriptor on the device (all front and back buffers will be the same size)
};
