// textures.cpp — cross-API version of LearnOpenGL 4.1.textures
//
// Usage:
//   textures.exe            → OpenGL 4.6 (default)
//   textures.exe --d3d11    → Direct3D 11
//
// Naming: Google C++ Style Guide

#include <cstring>
#include <iostream>
#include <memory>

#include <stb_image.h>

#include "glfw_window.h"
#include "renderer.h"

// -----------------------------------------------------------------------------
// Vertex data  (pos:3 + color:3 + uv:2 = 8 floats, interleaved)
// -----------------------------------------------------------------------------
constexpr float kVertices[] = {
    // positions          // colors           // texture coords
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
     0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
    -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f,   // top left
};

constexpr uint32_t kIndices[] = {
    0, 1, 3,   // first triangle
    1, 2, 3,   // second triangle
};

// -----------------------------------------------------------------------------
// OpenGL shader sources  (GLSL 4.60)
// -----------------------------------------------------------------------------
constexpr const char* kGlVs = R"(#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;
out vec3 ourColor;
out vec2 TexCoord;
void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
})";

constexpr const char* kGlFs = R"(#version 460 core
out vec4 FragColor;
in vec3 ourColor;
in vec2 TexCoord;
uniform sampler2D texture1;
void main()
{
    FragColor = texture(texture1, TexCoord);
})";

// -----------------------------------------------------------------------------
// D3D11 shader sources  (HLSL SM 5.0)
// -----------------------------------------------------------------------------
constexpr const char* kD3D11Vs = R"(
struct VS_INPUT
{
    float3 pos   : POSITION;
    float3 color : COLOR;
    float2 uv    : TEXCOORD;
};
struct VS_OUTPUT
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR;
    float2 uv    : TEXCOORD;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos   = float4(input.pos, 1.0);
    output.color = input.color;
    output.uv    = input.uv;
    return output;
})";

constexpr const char* kD3D11Fs = R"(
Texture2D    texture1 : register(t0);
SamplerState sampler1 : register(s0);

struct VS_OUTPUT
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR;
    float2 uv    : TEXCOORD;
};
float4 main(VS_OUTPUT input) : SV_TARGET
{
    return texture1.Sample(sampler1, input.uv);
})";

// =============================================================================
int main(int argc, char* argv[]) {
    // ---- parse backend --------------------------------------------------
    gfx::Backend backend = gfx::Backend::kD3D11;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--d3d11") == 0)
            backend = gfx::Backend::kD3D11;
        else if (std::strcmp(argv[i], "--gl") == 0)
            backend = gfx::Backend::kOpenGL;
    }

    glfwInit();

    // ---- create window --------------------------------------------------
    const char* vs_source = nullptr;
    const char* fs_source = nullptr;
    std::unique_ptr<glfw::Window> window;

    if (backend == gfx::Backend::kOpenGL) {
        glfw::Window::Hints hints{};
        hints.client_api            = GLFW_OPENGL_API;
        hints.context_version_major = 4;
        hints.context_version_minor = 6;
        hints.context_profile       = GLFW_OPENGL_CORE_PROFILE;
        window    = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — textures (OpenGL)", hints);
        vs_source = kGlVs;
        fs_source = kGlFs;
    } else {
        glfw::Window::Hints hints{};
        hints.client_api = GLFW_NO_API;
        window = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — textures (D3D11)", hints);
        vs_source = kD3D11Vs;
        fs_source = kD3D11Fs;
    }

    if (!window || !window->Get()) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // ---- init renderer --------------------------------------------------
    std::unique_ptr<gfx::IRenderer> renderer(gfx::CreateRenderer(backend));
    if (!renderer || !renderer->Initialize(window->Get())) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Renderer: " << renderer->GetName() << std::endl;

    // ---- input ----------------------------------------------------------
    window->on_key = [&](int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            window->SetShouldClose(true);
    };
    window->on_framebuffer_size = [&](int w, int h) {
        renderer->SetViewport(0, 0,
                              static_cast<uint32_t>(w),
                              static_cast<uint32_t>(h));
    };

    // ---- compile shaders ------------------------------------------------
    gfx::Handle vs = renderer->CreateShader(gfx::ShaderStage::kVertex, vs_source);
    gfx::Handle fs = renderer->CreateShader(gfx::ShaderStage::kFragment, fs_source);
    if (vs == gfx::kInvalidHandle || fs == gfx::kInvalidHandle) {
        std::cerr << "Shader compilation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    gfx::Handle program = renderer->CreateProgram(vs, fs);
    renderer->DestroyShader(vs);
    renderer->DestroyShader(fs);
    if (program == gfx::kInvalidHandle) {
        std::cerr << "Program linking failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // ---- create buffers -------------------------------------------------
    gfx::Handle vb = renderer->CreateBuffer(gfx::BufferType::kVertex,
                                             gfx::Usage::kStatic,
                                             sizeof(kVertices), kVertices);
    gfx::Handle ib = renderer->CreateBuffer(gfx::BufferType::kIndex,
                                             gfx::Usage::kStatic,
                                             sizeof(kIndices), kIndices);
    if (vb == gfx::kInvalidHandle || ib == gfx::kInvalidHandle) {
        std::cerr << "Buffer creation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // ---- create input layout  (3 attribs: pos + color + texcoord) -------
    constexpr gfx::VertexAttrib kAttribs[] = {
        {0, "POSITION", gfx::AttribFormat::kFloat3,  0},
        {1, "COLOR",    gfx::AttribFormat::kFloat3,  3 * sizeof(float)},
        {2, "TEXCOORD", gfx::AttribFormat::kFloat2,  6 * sizeof(float)},
    };
    constexpr uint32_t kStride = gfx::ComputeStride(kAttribs, 3);

    gfx::Handle layout = renderer->CreateInputLayout(program, kStride, kAttribs, 3);
    if (layout == gfx::kInvalidHandle) {
        std::cerr << "Input layout creation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // ---- load texture ---------------------------------------------------
    // Force 4 channels (RGBA) — stbi_load(..., 0) would return the native
    // channel count (3 for JPEG), which would mismatch kRgba8Unorm (4 Bpp).
    int tex_w = 0, tex_h = 0, channels = 0;
    unsigned char* image_data = stbi_load("resources/textures/container.jpg", &tex_w, &tex_h, &channels, 4);
    if (!image_data) {
        std::cerr << "Failed to load texture.  Make sure "
                     "resources/textures/container.jpg is in the working "
                     "directory." << std::endl;
        return -1;
    }

    gfx::Handle texture = renderer->CreateTexture2D(
        static_cast<uint32_t>(tex_w > 0 ? tex_w : 1),
        static_cast<uint32_t>(tex_h > 0 ? tex_h : 1),
        0,  // auto-generate full mip chain
        gfx::TextureFormat::kRgba8Unorm,
        image_data);
    if (image_data) stbi_image_free(image_data);

    if (texture != gfx::kInvalidHandle) {
        renderer->GenerateMipmaps(texture);
    }

    // ---- create sampler -------------------------------------------------
    gfx::SamplerDesc sampler_desc{};
    sampler_desc.min_filter = gfx::FilterMode::kLinear;
    sampler_desc.mag_filter = gfx::FilterMode::kLinear;
    sampler_desc.mip_filter = gfx::FilterMode::kLinear;
    sampler_desc.wrap_u     = gfx::WrapMode::kRepeat;
    sampler_desc.wrap_v     = gfx::WrapMode::kRepeat;
    gfx::Handle sampler = renderer->CreateSampler(sampler_desc);

    // ---- render loop ----------------------------------------------------
    while (!window->ShouldClose()) {
        glfwPollEvents();

        int fb_w = 0, fb_h = 0;
        window->GetFramebufferSize(&fb_w, &fb_h);

        renderer->Clear(gfx::ClearFlags::kColor, 0.2f, 0.3f, 0.3f, 1.0f);
        renderer->SetViewport(0, 0,
                              static_cast<uint32_t>(fb_w),
                              static_cast<uint32_t>(fb_h));

        renderer->BindProgram(program);
        renderer->BindInputLayout(layout);
        renderer->BindVertexBuffer(0, vb, kStride, 0);
        renderer->BindIndexBuffer(ib, gfx::IndexFormat::kUint32);
        renderer->BindTexture(0, texture);
        renderer->BindSampler(0, sampler);
        renderer->DrawIndexed(6, 0);

        renderer->Present();
    }

    // ---- cleanup --------------------------------------------------------
    renderer->DestroySampler(sampler);
    renderer->DestroyTexture(texture);
    renderer->DestroyInputLayout(layout);
    renderer->DestroyBuffer(ib);
    renderer->DestroyBuffer(vb);
    renderer->DestroyProgram(program);
    renderer->Shutdown();

    glfwTerminate();
    return 0;
}
