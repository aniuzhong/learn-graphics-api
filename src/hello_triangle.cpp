#include <iostream>

#include "platform.h"
#include "glfw_window.h"

constexpr int         kDefaultWidth     = 640;
constexpr int         kDefaultHeight    = 480;
constexpr const char* kWindowTitle      = "D3D11 Triangle";
constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr int         kMsaaSamples      = 4;
constexpr int         kBufferCount      = 2;
constexpr bool        debug_device      = true;

constexpr float vertices[] = {
    0.5f, -0.5f, 0.0f,   // right bottom
   -0.5f, -0.5f, 0.0f,   // left bottom
    0.0f,  0.5f, 0.0f,   // top
};

const char* vertex_shader_source = R"(
    float4 main(float3 pos : POSITION) : SV_POSITION
    {
        return float4(pos, 1.0f);
    }
)";

const char* pixel_shader_source = R"(
    float4 main() : SV_TARGET
    {
        return float4(1.0f, 0.5f, 0.2f, 1.0f); // warm orange
    }
)";

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const char* source, const char* entry_point, const char* target) {
    Microsoft::WRL::ComPtr<ID3DBlob> shader_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    HRESULT hr = D3DCompile(
        source,
        strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entry_point,
        target,
        0,
        0,
        shader_blob.GetAddressOf(),
        error_blob.GetAddressOf()
    );

    if (FAILED(hr)) {
        if (error_blob) {
            std::cerr << "Shader compilation error:\n" << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        } else {
            std::cerr << "Shader compilation failed with unknown error." << std::endl;
        }
        return nullptr;
    }

    return shader_blob;
}

int main()
{
    glfwInit();
    glfw::Window window(kDefaultWidth, kDefaultHeight, kWindowTitle, { .client_api = GLFW_NO_API });
    HWND hwnd = glfwGetWin32Window(window.Get());

    window.on_key = [&](int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            window.SetShouldClose(true);
        };
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    D3D11_VIEWPORT viewport;

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };

    D3D_FEATURE_LEVEL chosen_level;

    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (debug_device)
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width                   = kDefaultWidth;
    sd.BufferDesc.Height                  = kDefaultHeight;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format                  = kBackBufferFormat;
    sd.SampleDesc.Count                   = std::max(1, kMsaaSamples);
    sd.SampleDesc.Quality                 = kMsaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = kBufferCount;
    sd.OutputWindow                       = hwnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags                              = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &sd,
        swap_chain.GetAddressOf(),
        device.GetAddressOf(),
        &chosen_level,
        context.GetAddressOf()
    );

    if (FAILED(hr) && debug_device) {
        create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            create_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &sd,
            swap_chain.GetAddressOf(),
            device.GetAddressOf(),
            &chosen_level,
            context.GetAddressOf()
        );
    }

    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create D3D11 device and swap chain", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to get back buffer", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    hr = device->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create render target view", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    D3D11_TEXTURE2D_DESC dsd = {};
    dsd.Width              = kDefaultWidth;
    dsd.Height             = kDefaultHeight;
    dsd.MipLevels          = 1;
    dsd.ArraySize          = 1;
    dsd.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count   = std::max(1, kMsaaSamples);
    dsd.SampleDesc.Quality = kMsaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    dsd.Usage              = D3D11_USAGE_DEFAULT;
    dsd.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture;
    hr = device->CreateTexture2D(&dsd, nullptr, depth_texture.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create depth texture", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    hr = device->CreateDepthStencilView(depth_texture.Get(), nullptr, dsv.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create depth stencil view", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = static_cast<float>(kDefaultWidth);
    viewport.Height   = static_cast<float>(kDefaultHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob = CompileShader(vertex_shader_source, "main", "vs_5_0");
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob = CompileShader(pixel_shader_source, "main", "ps_5_0");
    if (!vs_blob || !ps_blob) {
        MessageBoxW(hwnd, L"Failed to compile shaders", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixel_shader;
    hr = device->CreateVertexShader(vs_blob->GetBufferPointer(),
                                    vs_blob->GetBufferSize(),
                                    nullptr,
                                    vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create vertex shader", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(),
                                   ps_blob->GetBufferSize(),
                                   nullptr,
                                   pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create pixel shader", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
        {
            "POSITION",                  // SemanticName
            0,                           // SemanticIndex
            DXGI_FORMAT_R32G32B32_FLOAT, // Format
            0,                           // InputSlot
            0,                           // AlignedByteOffset
            D3D11_INPUT_PER_VERTEX_DATA, // InputSlotClass
        }
    };

    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
    hr = device->CreateInputLayout(layout_desc,
                                   ARRAYSIZE(layout_desc),
                                   vs_blob->GetBufferPointer(),
                                   vs_blob->GetBufferSize(),
                                   input_layout.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create input layout", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.ByteWidth      = sizeof(vertices);
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = vertices;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
    hr = device->CreateBuffer(&bd, &init_data, vertex_buffer.GetAddressOf());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Failed to create vertex buffer", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    while (!window.ShouldClose()) {
        glfwPollEvents();
        float clear_color[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
        context->ClearRenderTargetView(rtv.Get(), clear_color);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        context->RSSetViewports(1, &viewport);
        context->IASetInputLayout(input_layout.Get());
        UINT stride = sizeof(float) * 3;
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->Draw(3, 0);
        swap_chain->Present(1, 0);
    }

    glfwTerminate();
    return 0;
}
