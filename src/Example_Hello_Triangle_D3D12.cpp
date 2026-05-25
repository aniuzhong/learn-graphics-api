#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <iostream>

#include "glfw_window.h"
#include "platform.h"

using Microsoft::WRL::ComPtr;

void InitializeD3D12() {
    // 在 Debug 模式下开启 D3D12 的调试层
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
        debug_controller->EnableDebugLayer();
    }
#endif
}

int main(int argc, char* argv[])
{
    glfwInit();

    auto window = glfw::Window(800, 600, "learn-graphics-api — Hello Triangle D3D12", glfw::Window::Hints{ .client_api = GLFW_NO_API, });
    if (!window.Get()) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    InitializeD3D12();

    while (!window.ShouldClose()) {
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}