#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <iostream>
#include <memory>

#include "glfw_window.h"
#include "platform.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

const uint32_t                          kFrameCount                     = 2;
const uint32_t                          kWidth                          = 1920;
const uint32_t                          kHeight                         = 1080;
HWND                                    g_hwnd                          = nullptr;
ComPtr<ID3D12Device>                    g_device;
ComPtr<ID3D12CommandQueue>              g_command_queue;
ComPtr<IDXGISwapChain3>                 g_swap_chain;
ComPtr<ID3D12DescriptorHeap>            g_rtv_heap;
ComPtr<ID3D12Resource>                  g_render_targets[kFrameCount];
uint32_t                                g_rtv_descriptor_size           = 0;
ComPtr<ID3D12CommandAllocator>          g_command_allocator;
ComPtr<ID3D12GraphicsCommandList>       g_command_list;
ComPtr<ID3D12Fence>                     g_fence;
UINT64                                  g_fence_value                   = 0;
HANDLE                                  g_fence_event                   = nullptr;

void InitializeD3D12() {
    HRESULT hr;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
        debug_controller->EnableDebugLayer();
    }
    std::cout << "D3D12 Debug Layer enabled." << std::endl;
#endif

    ComPtr<IDXGIFactory4> factory;
    UINT create_factory_flags = 0;
#if defined(_DEBUG)
    create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        std::cerr << "Failed to create DXGI Factory: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "DXGI Factory created successfully." << std::endl;

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 Device: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "D3D12 Device created successfully." << std::endl;

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT; // for graphics and compute
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue_desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 0; // single GPU
    hr = g_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_command_queue));
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 Command Queue: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "D3D12 Command Queue created successfully." << std::endl;

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width                 = kWidth;
    swap_chain_desc.Height                = kHeight;
    swap_chain_desc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount           = kFrameCount;
    swap_chain_desc.SampleDesc.Count      = 1; // no MSAA
    // swap_chain_desc.SampleDesc.Quality    = 0;
    swap_chain_desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // swap_chain_desc.Flags                 = 0;
    // swap_chain_desc.Scaling               = DXGI_SCALING_STRETCH;
    // swap_chain_desc.AlphaMode             = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swap_chain1;

    std::cout << "Native HWND: " << g_hwnd << std::endl;

    hr = factory->CreateSwapChainForHwnd(
        g_command_queue.Get(),
        g_hwnd,
        &swap_chain_desc,
        nullptr, // full-screen desc
        nullptr, // restrict output
        &swap_chain1);
    if (FAILED(hr)) {
        std::cerr << "Failed to create Swap Chain: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Swap Chain created successfully." << std::endl;

    hr = swap_chain1.As(&g_swap_chain);
    if (FAILED(hr)) {
        std::cerr << "Failed to query IDXGISwapChain3: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Queried IDXGISwapChain3 successfully." << std::endl;

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = kFrameCount;
    rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = g_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&g_rtv_heap));
    if (FAILED(hr)) {
        std::cerr << "Failed to create RTV Descriptor Heap: " << std::hex << hr << std::endl;
        return;
    }

    std::cout << "RTV Descriptor Heap created successfully." << std::endl;

    g_rtv_descriptor_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        hr = g_swap_chain->GetBuffer(i, IID_PPV_ARGS(&g_render_targets[i]));
        if (FAILED(hr)) {
            std::cerr << "Failed to get Swap Chain buffer " << i << ": " << std::hex << hr << std::endl;
            return;
        }
        g_device->CreateRenderTargetView(g_render_targets[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += g_rtv_descriptor_size;
    }
    std::cout << "Render targets created successfully." << std::endl;

    hr = g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_command_allocator));
    if (FAILED(hr)) {
        std::cerr << "Failed to create Command Allocator: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Command Allocator created successfully." << std::endl;

    hr = g_device->CreateCommandList(
        0, // node mask for multi-GPU, 0 for single GPU
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_command_allocator.Get(),
        nullptr, // initial PSO, can be set later
        IID_PPV_ARGS(&g_command_list));
    if (FAILED(hr)) {
        std::cerr << "Failed to create Command List: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Command List created successfully." << std::endl;

    g_command_list->Close(); // start in closed state

    hr = g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    if (FAILED(hr)) {
        std::cerr << "Failed to create Fence: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Fence created successfully." << std::endl;
    g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fence_event) {
        std::cerr << "Failed to create Fence event: " << GetLastError() << std::endl;
        return;
    }
    std::cout << "Fence event created successfully." << std::endl;
}

int main()
{
    glfwInit();

    auto window = glfw::Window(kWidth, kHeight, "Hello Triangle D3D12", {
        .client_api = GLFW_NO_API,
    });

    g_hwnd = glfwGetWin32Window(window.Get());

    window.on_key = [&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            window.SetShouldClose(true);
        }
    };

    InitializeD3D12();

    while (!window.ShouldClose()) {
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}