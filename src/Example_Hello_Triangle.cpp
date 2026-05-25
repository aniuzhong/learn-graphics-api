// hello_triangle_indexed — cross-API version
//
// Based on LearnOpenGL 2.2.hello_triangle_indexed (EBO / indexed draw).
//
// Usage:
//   hello-triangle.exe            → OpenGL 4.6 (default)
//   hello-triangle.exe --d3d11    → Direct3D 11
//   hello-triangle.exe --gl       → OpenGL 4.6 (explicit)
//
// Naming: Google C++ Style Guide  (PascalCase methods, snake_case locals,
//         kConstants, snake_case_ members)

#include <cstring>
#include <iostream>
#include <memory>

#include "glfw_window.h"
#include "renderer.h"

// -----------------------------------------------------------------------------
// Vertex / index data  (identical to LearnOpenGL 2.2.hello_triangle_indexed)
// -----------------------------------------------------------------------------
constexpr float kVertices[] = {
     0.5f,  0.5f, 0.0f,   // top right
     0.5f, -0.5f, 0.0f,   // bottom right
    -0.5f, -0.5f, 0.0f,   // bottom left
    -0.5f,  0.5f, 0.0f,   // top left
};

constexpr uint32_t kIndices[] = {
    0, 1, 3,   // first triangle
    1, 2, 3,   // second triangle
};

// -----------------------------------------------------------------------------
// OpenGL shader sources  (GLSL 4.60 — matches the GL 4.6 core profile)
// -----------------------------------------------------------------------------
constexpr const char* kGlVs = R"(#version 460 core
layout (location = 0) in vec3 aPos;
void main()
{
    gl_Position = vec4(aPos, 1.0);
})";

constexpr const char* kGlFs = R"(#version 460 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 0.5, 0.2, 1.0);
})";

// -----------------------------------------------------------------------------
// D3D11 shader sources  (HLSL SM 5.0)
// -----------------------------------------------------------------------------
constexpr const char* kD3D11Vs = R"(float4 main(float3 pos : POSITION) : SV_POSITION
{
    return float4(pos, 1.0);
})";

constexpr const char* kD3D11Fs = R"(float4 main() : SV_TARGET
{
    return float4(1.0, 0.5, 0.2, 1.0);
})";

// =============================================================================
int main(int argc, char* argv[]) {
    gfx::Backend backend = gfx::Backend::kOpenGL;
    // gfx::Backend backend = gfx::Backend::kD3D11;

    glfwInit();

    // create window (hints differ per backend)
    const char* vs_source = nullptr;
    const char* fs_source = nullptr;
    const char* title     = nullptr;

    std::unique_ptr<glfw::Window> window;
    if (backend == gfx::Backend::kOpenGL) {
        glfw::Window::Hints hints{};
        hints.client_api            = GLFW_OPENGL_API;
        hints.context_version_major = 4;
        hints.context_version_minor = 6;
        hints.context_profile       = GLFW_OPENGL_CORE_PROFILE;
#ifdef __APPLE__
        hints.context_forward_compat = true;
#endif
        window    = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — OpenGL 4.6 (Esc to quit)", hints);
        vs_source = kGlVs;
        fs_source = kGlFs;
    } else {
        glfw::Window::Hints hints{};
        hints.client_api = GLFW_NO_API;
        window    = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — D3D11 (Esc to quit)", hints);
        vs_source = kD3D11Vs;
        fs_source = kD3D11Fs;
    }

    if (!window || !window->Get()) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // init renderer
    std::unique_ptr<gfx::IRenderer> renderer(gfx::CreateRenderer(backend));
    if (!renderer) {
        std::cerr << "Unknown backend" << std::endl;
        glfwTerminate();
        return -1;
    }

    if (!renderer->Initialize(window->Get())) {
        std::cerr << "Failed to initialize " << renderer->GetName() << " renderer" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Renderer: " << renderer->GetName() << std::endl;

    // input
    window->on_key = [&](int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            window->SetShouldClose(true);
    };

    window->on_framebuffer_size = [&](int w, int h) {
        renderer->SetViewport(0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    };

    // compile shaders
    gfx::Handle vs = renderer->CreateShader(gfx::ShaderStage::kVertex, vs_source);
    gfx::Handle fs = renderer->CreateShader(gfx::ShaderStage::kFragment, fs_source);
    if (vs == gfx::kInvalidHandle || fs == gfx::kInvalidHandle) {
        std::cerr << "Shader compilation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    gfx::Handle program = renderer->CreateProgram(vs, fs);
    renderer->DestroyShader(vs);  // no longer needed after linking
    renderer->DestroyShader(fs);

    if (program == gfx::kInvalidHandle) {
        std::cerr << "Program linking failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // create buffers
    gfx::Handle vb = renderer->CreateBuffer(gfx::BufferType::kVertex, gfx::Usage::kStatic, sizeof(kVertices), kVertices);
    gfx::Handle ib = renderer->CreateBuffer(gfx::BufferType::kIndex, gfx::Usage::kStatic, sizeof(kIndices), kIndices);
    if (vb == gfx::kInvalidHandle || ib == gfx::kInvalidHandle) {
        std::cerr << "Buffer creation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // create input layout
    constexpr uint32_t kStride = 3 * sizeof(float); // position only
    constexpr gfx::VertexAttrib kAttribs[] = { {0, "POSITION", gfx::AttribFormat::kFloat3, 0}, };
    gfx::Handle layout = renderer->CreateInputLayout(program, kStride, kAttribs, 1);
    if (layout == gfx::kInvalidHandle) {
        std::cerr << "Input layout creation failed" << std::endl;
        renderer->Shutdown();
        glfwTerminate();
        return -1;
    }

    // render loop
    while (!window->ShouldClose()) {
        glfwPollEvents();

        int fb_width = 0, fb_height = 0;
        window->GetFramebufferSize(&fb_width, &fb_height);

        renderer->Clear(gfx::ClearFlags::kColor, 0.2f, 0.3f, 0.3f, 1.0f);
        renderer->SetViewport(0, 0, static_cast<uint32_t>(fb_width), static_cast<uint32_t>(fb_height));
        renderer->BindProgram(program);
        renderer->BindInputLayout(layout);
        renderer->BindVertexBuffer(0, vb, kStride, 0);
        renderer->BindIndexBuffer(ib, gfx::IndexFormat::kUint32);
        renderer->DrawIndexed(6, 0);

        renderer->Present();
    }

    // cleanup
    renderer->DestroyInputLayout(layout);
    renderer->DestroyBuffer(ib);
    renderer->DestroyBuffer(vb);
    renderer->DestroyProgram(program);
    renderer->Shutdown();

    glfwTerminate();
    return 0;
}
