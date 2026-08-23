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
ComPtr<ID3D12RootSignature>             g_root_signature;
ComPtr<ID3DBlob>                        g_vertex_shader_blob;
ComPtr<ID3DBlob>                        g_pixel_shader_blob;
ComPtr<ID3D12PipelineState>             g_pipeline_state;

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

void CreatePipelineState() {
    HRESULT hr;
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters             = 0;
    root_signature_desc.pParameters               = nullptr;
    root_signature_desc.NumStaticSamplers         = 0;
    root_signature_desc.pStaticSamplers           = nullptr;
    root_signature_desc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    hr = D3D12SerializeRootSignature(&root_signature_desc,
                                     D3D_ROOT_SIGNATURE_VERSION_1,
                                     &signature_blob,
                                     &error_blob);
    if (FAILED(hr)) {
        std::cerr << "Failed to serialize Root Signature: " << std::hex << hr << std::endl;
        if (error_blob) {
            std::cerr << "Error details: " << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return;
    }

    hr = g_device->CreateRootSignature(0,
                                       signature_blob->GetBufferPointer(),
                                       signature_blob->GetBufferSize(),
                                       IID_PPV_ARGS(&g_root_signature));
    if (FAILED(hr)) {
        std::cerr << "Failed to create Root Signature: " << std::hex << hr << std::endl;
        return;
    }

    std::cout << "Empty Root Signature created successfully." << std::endl;

    UINT compile_flags = 0;
#if defined(_DEBUG)
    compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    hr = D3DCompileFromFile(
        L"shaders.hlsl",
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        compile_flags,
        0,
        &g_vertex_shader_blob,
        &error_blob);

    if (FAILED(hr)) {
        std::cerr << "Failed to compile vertex shader: " << std::hex << hr << std::endl;
        if (error_blob) {
            std::cerr << "Error details: " << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return;
    }
    std::cout << "Vertex shader compiled successfully." << std::endl;

    hr = D3DCompileFromFile(
        L"shaders.hlsl",
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        compile_flags,
        0,
        &g_pixel_shader_blob,
        &error_blob);

    if (FAILED(hr)) {
        std::cerr << "Failed to compile pixel shader: " << std::hex << hr << std::endl;
        if (error_blob) {
            std::cerr << "Error details: " << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return;
    }
    std::cout << "Pixel shader compiled successfully." << std::endl;

    // Vertex layout is empty since vertex data is fully hardcoded in the shader for this simple example.
    D3D12_INPUT_LAYOUT_DESC input_layout_desc = {};
    input_layout_desc.pInputElementDescs = nullptr; // no vertex attributes for now
    input_layout_desc.NumElements        = 0;
    std::cout << "Empty Input Layout defined (Vertex data fully hardcoded in Shader)." << std::endl;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature                                   = g_root_signature.Get();
    pso_desc.VS                                               = { g_vertex_shader_blob->GetBufferPointer(), g_vertex_shader_blob->GetBufferSize() };
    pso_desc.PS                                               = { g_pixel_shader_blob->GetBufferPointer(), g_pixel_shader_blob->GetBufferSize() };
    pso_desc.InputLayout                                      = input_layout_desc;
    pso_desc.RasterizerState.FillMode                         = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode                         = D3D12_CULL_MODE_BACK;
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso_desc.DepthStencilState.DepthEnable                    = FALSE;
    pso_desc.DepthStencilState.StencilEnable                  = FALSE;
    pso_desc.PrimitiveTopologyType                            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets                                 = 1;
    pso_desc.RTVFormats[0]                                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count                                 = 1;
    pso_desc.SampleMask                                       = UINT_MAX;

    hr = g_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&g_pipeline_state));
    if (FAILED(hr)) {
        std::cerr << "Failed to create Pipeline State Object: " << std::hex << hr << std::endl;
        return;
    }
    std::cout << "Pipeline State Object created successfully." << std::endl;
}

void Render() {
    HRESULT hr;
    hr = g_command_allocator->Reset();
    if (FAILED(hr)) {
        std::cerr << "Failed to reset Command Allocator: " << std::hex << hr << std::endl;
        return;
    }
    hr = g_command_list->Reset(g_command_allocator.Get(), g_pipeline_state.Get());
    if (FAILED(hr)) {
        std::cerr << "Failed to reset Command List: " << std::hex << hr << std::endl;
        return;
    }

    UINT back_buffer_index = g_swap_chain->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = g_render_targets[back_buffer_index].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    g_command_list->ResourceBarrier(1, &barrier);
    // std::cout << "Command List reset and Barrier recorded for buffer " << back_buffer_index << "." << std::endl;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    UINT rtv_descriptor_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtv_handle.ptr += back_buffer_index * rtv_descriptor_size;

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(kWidth);
    viewport.Height = static_cast<float>(kHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor_rect = {};
    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = kWidth;
    scissor_rect.bottom = kHeight;

    const FLOAT clear_color[] = { 0.2f, 0.3f, 0.3f, 1.0f };
    g_command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    g_command_list->RSSetViewports(1, &viewport);
    g_command_list->RSSetScissorRects(1, &scissor_rect);
    g_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
    g_command_list->SetGraphicsRootSignature(g_root_signature.Get());
    g_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_command_list->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER present_barrier = {};
    present_barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    present_barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    present_barrier.Transition.pResource   = g_render_targets[back_buffer_index].Get();
    present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    present_barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    g_command_list->ResourceBarrier(1, &present_barrier);
    hr = g_command_list->Close();
    if (FAILED(hr)) {
        std::cerr << "Failed to close Command List: " << std::hex << hr << std::endl;
        return;
    }
    // std::cout << "DrawCall and Present Barrier recorded successfully. Command List Closed." << std::endl;

    ID3D12CommandList* command_lists[] = { g_command_list.Get() };
    g_command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

    hr = g_swap_chain->Present(1, 0);
    if (FAILED(hr)) {
        std::cerr << "Failed to present Swap Chain: " << std::hex << hr << std::endl;
        return;
    }

    g_fence_value++;

    const UINT64 current_fence_value = g_fence_value;
    hr = g_command_queue->Signal(g_fence.Get(), current_fence_value);
    if (FAILED(hr)) {
        std::cerr << "Failed to signal Command Queue: " << std::hex << hr << std::endl;
        return;
    }

    if (g_fence->GetCompletedValue() < current_fence_value) {
        hr = g_fence->SetEventOnCompletion(current_fence_value, g_fence_event);
        if (FAILED(hr)) {
            std::cerr << "Failed to set event on Fence completion: " << std::hex << hr << std::endl;
            return;
        }
        WaitForSingleObject(g_fence_event, INFINITE);
    }
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
    CreatePipelineState();

    while (!window.ShouldClose()) {
        glfwPollEvents();
        Render();
    }

    glfwTerminate();
    return 0;
}