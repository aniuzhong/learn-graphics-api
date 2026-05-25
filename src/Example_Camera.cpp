// Example_Camera.cpp — cross-API port of LearnOpenGL 7.4.camera_class
//                        + 7.6 custom lookAt exercise.
//
// Usage:
//   camera.exe            → OpenGL 4.6 (default)
//   camera.exe --d3d11    → Direct3D 11
//
// Features: FPS camera (WASD + mouse + scroll), 10 textured cubes,
//           depth test, custom LookAt, multi-texture mixing.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stb_image.h>

#include "glfw_window.h"
#include "renderer.h"

// =============================================================================
// Camera — minimal inline FPS camera  (Google C++: PascalCase methods)
// =============================================================================
class Camera {
public:
    Camera(glm::vec3 pos, glm::vec3 up, float yaw, float pitch)
        : pos_(pos), world_up_(up), yaw_(yaw), pitch_(pitch) {
        UpdateVectors();
    }

    glm::mat4 GetViewMatrix() const {
        return LookAt(pos_, pos_ + front_, up_);
    }

    void ProcessKeyboard(glm::vec3 direction, float dt) {
        float speed = speed_ * dt;
        pos_ += direction * speed;
    }

    void ProcessMouse(float xoff, float yoff, bool constrain_pitch = true) {
        xoff *= sensitivity_;
        yoff *= sensitivity_;
        yaw_   += xoff;
        pitch_ += yoff;
        if (constrain_pitch) pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
        UpdateVectors();
    }

    void ProcessScroll(float yoff) {
        zoom_ = std::clamp(zoom_ - yoff, 1.0f, 45.0f);
    }

    float zoom() const { return zoom_; }
    glm::vec3 pos() const { return pos_; }
    glm::vec3 front() const { return front_; }
    glm::vec3 up() const { return up_; }

private:
    void UpdateVectors() {
        glm::vec3 f;
        f.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        f.y = std::sin(glm::radians(pitch_));
        f.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        front_ = glm::normalize(f);
        right_ = glm::normalize(glm::cross(front_, world_up_));
        up_    = glm::normalize(glm::cross(right_, front_));
    }

    // Custom LookAt — replaces glm::lookAt (the 7.6 exercise requirement).
    static glm::mat4 LookAt(glm::vec3 pos, glm::vec3 target, glm::vec3 up) {
        glm::vec3 z = glm::normalize(pos - target);       // camera direction
        glm::vec3 x = glm::normalize(glm::cross(up, z));  // right axis
        glm::vec3 y = glm::cross(z, x);                   // camera up

        glm::mat4 rot(1.0f);
        rot[0][0] = x.x;  rot[1][0] = x.y;  rot[2][0] = x.z;
        rot[0][1] = y.x;  rot[1][1] = y.y;  rot[2][1] = y.z;
        rot[0][2] = z.x;  rot[1][2] = z.y;  rot[2][2] = z.z;

        glm::mat4 trans(1.0f);
        trans[3][0] = -pos.x;
        trans[3][1] = -pos.y;
        trans[3][2] = -pos.z;

        return rot * trans;
    }

    glm::vec3 pos_{};
    glm::vec3 front_{};
    glm::vec3 up_{};
    glm::vec3 right_{};
    glm::vec3 world_up_;
    float yaw_   = -90.0f;
    float pitch_ = 0.0f;
    float speed_ = 2.5f;
    float sensitivity_ = 0.1f;
    float zoom_ = 45.0f;
};

// =============================================================================
// Vertex data — 36 unique vertices per cube (pos:3 + uv:2 = 5 floats)
// =============================================================================
constexpr float kCubeVertices[] = {
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
};

constexpr glm::vec3 kCubePositions[] = {
    { 0.0f,  0.0f,   0.0f},
    { 2.0f,  5.0f, -15.0f},
    {-1.5f, -2.2f,  -2.5f},
    {-3.8f, -2.0f, -12.3f},
    { 2.4f, -0.4f,  -3.5f},
    {-1.7f,  3.0f,  -7.5f},
    { 1.3f, -2.0f,  -2.5f},
    { 1.5f,  2.0f,  -2.5f},
    { 1.5f,  0.2f,  -1.5f},
    {-1.3f,  1.0f,  -1.5f},
};

// =============================================================================
// GLSL shaders
// =============================================================================
constexpr const char* kGlVs = R"(#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
})";

constexpr const char* kGlFs = R"(#version 460 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D texture1;
uniform sampler2D texture2;
void main()
{
    FragColor = mix(texture(texture1, TexCoord),
                    texture(texture2, TexCoord), 0.2);
})";

// =============================================================================
// HLSL shaders
// =============================================================================
constexpr const char* kD3D11Vs = R"(
cbuffer PerDraw : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};
struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = mul(projection, mul(view, mul(model, float4(input.pos, 1.0))));
    output.uv = input.uv;
    return output;
})";

constexpr const char* kD3D11Fs = R"(
Texture2D    texture1 : register(t0);
SamplerState sampler1 : register(s0);
Texture2D    texture2 : register(t1);
SamplerState sampler2 : register(s1);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};
float4 main(VS_OUTPUT input) : SV_TARGET
{
    float4 c1 = texture1.Sample(sampler1, input.uv);
    float4 c2 = texture2.Sample(sampler2, input.uv);
    return lerp(c1, c2, 0.2);
})";

// =============================================================================
int main(int argc, char* argv[]) {
    // ---- parse backend --------------------------------------------------
    gfx::Backend backend = gfx::Backend::kOpenGL;
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
        window    = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — Camera (OpenGL)", hints);
        vs_source = kGlVs;
        fs_source = kGlFs;
    } else {
        glfw::Window::Hints hints{};
        hints.client_api = GLFW_NO_API;
        window = std::make_unique<glfw::Window>(800, 600, "learn-graphics-api — Camera (D3D11)", hints);
        vs_source = kD3D11Vs;
        fs_source = kD3D11Fs;
    }

    if (!window || !window->Get()) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // ---- init renderer --------------------------------------------------
    std::unique_ptr<gfx::IRenderer> renderer(gfx::CreateRenderer(backend));
    if (!renderer || !renderer->Initialize(window->Get())) {
        std::cerr << "Failed to init renderer" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Renderer: " << renderer->GetName() << std::endl;

    // ---- camera ---------------------------------------------------------
    Camera camera({0.0f, 0.0f, 3.0f}, {0.0f, 1.0f, 0.0f}, -90.0f, 0.0f);

    float last_x = 400.0f, last_y = 300.0f;
    bool  first_mouse = true;
    float delta_time = 0.0f, last_frame_time = 0.0f;

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

    window->on_cursor_pos = [&](double x, double y) {
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        if (first_mouse) { last_x = fx; last_y = fy; first_mouse = false; }
        camera.ProcessMouse(fx - last_x, last_y - fy);
        last_x = fx;
        last_y = fy;
    };

    window->on_scroll = [&](double, double dy) {
        camera.ProcessScroll(static_cast<float>(dy));
    };

    glfwSetInputMode(window->Get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // ---- compile shaders ------------------------------------------------
    gfx::Handle vs = renderer->CreateShader(gfx::ShaderStage::kVertex, vs_source);
    gfx::Handle fs = renderer->CreateShader(gfx::ShaderStage::kFragment, fs_source);
    if (vs == gfx::kInvalidHandle || fs == gfx::kInvalidHandle) {
        std::cerr << "Shader compilation failed" << std::endl;
        return -1;
    }
    gfx::Handle program = renderer->CreateProgram(vs, fs);
    renderer->DestroyShader(vs);
    renderer->DestroyShader(fs);
    if (program == gfx::kInvalidHandle) { std::cerr << "Link failed" << std::endl; return -1; }

    // ---- get uniform locations ------------------------------------------
    int32_t loc_model      = renderer->GetUniformLocation(program, "model");
    int32_t loc_view       = renderer->GetUniformLocation(program, "view");
    int32_t loc_projection = renderer->GetUniformLocation(program, "projection");

    // ---- create buffers -------------------------------------------------
    gfx::Handle vb = renderer->CreateBuffer(gfx::BufferType::kVertex,
                                             gfx::Usage::kStatic,
                                             sizeof(kCubeVertices), kCubeVertices);
    if (vb == gfx::kInvalidHandle) { std::cerr << "VB failed" << std::endl; return -1; }

    // No index buffer — 36 unique vertices per cube, non-indexed draw.

    // ---- input layout (pos:3 + uv:2) -----------------------------------
    constexpr gfx::VertexAttrib kAttribs[] = {
        {0, "POSITION", gfx::AttribFormat::kFloat3, 0},
        {1, "TEXCOORD", gfx::AttribFormat::kFloat2, 3 * sizeof(float)},
    };
    constexpr uint32_t kStride = gfx::ComputeStride(kAttribs, 2);
    gfx::Handle layout = renderer->CreateInputLayout(
        program, kStride, kAttribs, 2);
    if (layout == gfx::kInvalidHandle) { std::cerr << "Layout failed" << std::endl; return -1; }

    // ---- load textures --------------------------------------------------
    stbi_set_flip_vertically_on_load(true);

    auto LoadTexture = [&](const char* path) -> gfx::Handle {
        int w = 0, h = 0, c = 0;
        unsigned char* data = stbi_load(path, &w, &h, &c, 4);
        if (!data) { std::cerr << "Failed: " << path << std::endl; return gfx::kInvalidHandle; }
        gfx::Handle t = renderer->CreateTexture2D(
            static_cast<uint32_t>(w), static_cast<uint32_t>(h),
            0, gfx::TextureFormat::kRgba8Unorm, data);
        stbi_image_free(data);
        if (t != gfx::kInvalidHandle) renderer->GenerateMipmaps(t);
        return t;
    };

    gfx::Handle texture1 = LoadTexture("container.jpg");
    gfx::Handle texture2 = LoadTexture("awesomeface.png");
    if (texture1 == gfx::kInvalidHandle || texture2 == gfx::kInvalidHandle) {
        std::cerr << "Texture load failed" << std::endl;
        return -1;
    }

    // ---- samplers -------------------------------------------------------
    gfx::SamplerDesc sd{};
    sd.min_filter = gfx::FilterMode::kLinear;
    sd.mag_filter = gfx::FilterMode::kLinear;
    sd.mip_filter = gfx::FilterMode::kLinear;
    gfx::Handle sampler = renderer->CreateSampler(sd);

    // ---- depth test ON --------------------------------------------------
    gfx::DepthStencilDesc ds{};
    ds.depth_test  = true;
    ds.depth_write = true;
    renderer->SetDepthStencilState(ds);

    // Tell GL which texture unit each sampler uses.
    // (D3D11 handles this in the HLSL register declarations; SetUniformInt
    //  is a no-op there.)
    renderer->BindProgram(program);
    renderer->SetUniformInt(renderer->GetUniformLocation(program, "texture1"), 0);
    renderer->SetUniformInt(renderer->GetUniformLocation(program, "texture2"), 1);

    // ---- render loop ----------------------------------------------------
    while (!window->ShouldClose()) {
        glfwPollEvents();

        // timing
        float now = static_cast<float>(glfwGetTime());
        delta_time = now - last_frame_time;
        last_frame_time = now;

        // keyboard movement
        float speed = 2.5f * delta_time;
        glm::vec3 dir(0.0f);
        if (window->GetKey(GLFW_KEY_W)) dir += camera.front();
        if (window->GetKey(GLFW_KEY_S)) dir -= camera.front();
        if (window->GetKey(GLFW_KEY_A)) dir -= glm::cross(camera.front(), camera.up());
        if (window->GetKey(GLFW_KEY_D)) dir += glm::cross(camera.front(), camera.up());
        if (glm::length(dir) > 0.0f)
            camera.ProcessKeyboard(glm::normalize(dir), delta_time);

        // render
        int fb_w = 0, fb_h = 0;
        window->GetFramebufferSize(&fb_w, &fb_h);
        float aspect = fb_h > 0 ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 1.0f;

        renderer->Clear(gfx::ClearFlags::kColor | gfx::ClearFlags::kDepth,
                         0.2f, 0.3f, 0.3f, 1.0f);
        renderer->SetViewport(0, 0,
                              static_cast<uint32_t>(fb_w),
                              static_cast<uint32_t>(fb_h));

        glm::mat4 proj = glm::perspective(
            glm::radians(camera.zoom()), aspect, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        renderer->BindProgram(program);
        renderer->BindInputLayout(layout);
        renderer->SetUniformMat4(loc_view, glm::value_ptr(view));
        renderer->SetUniformMat4(loc_projection, glm::value_ptr(proj));
        renderer->BindVertexBuffer(0, vb, kStride, 0);
        renderer->BindTexture(0, texture1);
        renderer->BindTexture(1, texture2);
        renderer->BindSampler(0, sampler);
        renderer->BindSampler(1, sampler);

        for (uint32_t i = 0; i < 10; ++i) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, kCubePositions[i]);
            model = glm::rotate(model, glm::radians(20.0f * i),
                                 glm::vec3(1.0f, 0.3f, 0.5f));
            renderer->SetUniformMat4(loc_model, glm::value_ptr(model));
            renderer->Draw(36, 0);
        }

        renderer->Present();
    }

    // ---- cleanup --------------------------------------------------------
    renderer->DestroySampler(sampler);
    renderer->DestroyTexture(texture2);
    renderer->DestroyTexture(texture1);
    renderer->DestroyInputLayout(layout);
    renderer->DestroyBuffer(vb);
    renderer->DestroyProgram(program);
    renderer->Shutdown();
    glfwTerminate();
    return 0;
}
