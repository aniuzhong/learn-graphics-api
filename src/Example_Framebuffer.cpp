// Example_Framebuffer.cpp — cross-API port of LearnOpenGL 4.5.1.framebuffers
//
// Usage:
//   framebuffer.exe            → OpenGL 4.6 (default)
//   framebuffer.exe --d3d11    → Direct3D 11
//
// Features: offscreen rendering (FBO), multi-pass (scene → texture → screen),
//           camera, depth test toggle, two shader programs.

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
// Camera — minimal inline FPS camera
// =============================================================================
class Camera {
public:
    Camera(glm::vec3 pos, glm::vec3 up, float yaw, float pitch)
        : pos_(pos), world_up_(up), yaw_(yaw), pitch_(pitch) { UpdateVectors(); }

    glm::mat4 GetViewMatrix() const { return LookAt(pos_, pos_ + front_, up_); }
    void ProcessKeyboard(glm::vec3 dir, float dt) { pos_ += dir * (speed_ * dt); }
    void ProcessMouse(float xoff, float yoff) {
        xoff *= sensitivity_; yoff *= sensitivity_;
        yaw_ += xoff; pitch_ = std::clamp(pitch_ + yoff, -89.0f, 89.0f);
        UpdateVectors();
    }
    void ProcessScroll(float dy) { zoom_ = std::clamp(zoom_ - dy, 1.0f, 45.0f); }
    float zoom() const { return zoom_; }
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
    static glm::mat4 LookAt(glm::vec3 pos, glm::vec3 target, glm::vec3 up) {
        glm::vec3 z = glm::normalize(pos - target);
        glm::vec3 x = glm::normalize(glm::cross(up, z));
        glm::vec3 y = glm::cross(z, x);
        glm::mat4 rot(1.0f);
        rot[0][0]=x.x; rot[1][0]=x.y; rot[2][0]=x.z;
        rot[0][1]=y.x; rot[1][1]=y.y; rot[2][1]=y.z;
        rot[0][2]=z.x; rot[1][2]=z.y; rot[2][2]=z.z;
        glm::mat4 tr(1.0f);
        tr[3][0]=-pos.x; tr[3][1]=-pos.y; tr[3][2]=-pos.z;
        return rot * tr;
    }
    glm::vec3 pos_{}, front_{}, up_{}, right_{}, world_up_;
    float yaw_=-90.0f, pitch_=0.0f, speed_=2.5f, sensitivity_=0.1f, zoom_=45.0f;
};

// =============================================================================
// Vertex data
// =============================================================================
constexpr float kCubeVerts[] = {
    -0.5f,-0.5f,-0.5f, 0.0f,0.0f,  0.5f,-0.5f,-0.5f, 1.0f,0.0f,
     0.5f, 0.5f,-0.5f, 1.0f,1.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,
    -0.5f, 0.5f,-0.5f, 0.0f,1.0f, -0.5f,-0.5f,-0.5f, 0.0f,0.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f,  0.5f,-0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,1.0f,  0.5f, 0.5f, 0.5f, 1.0f,1.0f,
    -0.5f, 0.5f, 0.5f, 0.0f,1.0f, -0.5f,-0.5f, 0.5f, 0.0f,0.0f,
    -0.5f, 0.5f, 0.5f, 1.0f,0.0f, -0.5f, 0.5f,-0.5f, 1.0f,1.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f, -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f, -0.5f, 0.5f, 0.5f, 1.0f,0.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f,-0.5f,-0.5f, 0.0f,1.0f,  0.5f,-0.5f,-0.5f, 0.0f,1.0f,
     0.5f,-0.5f, 0.5f, 0.0f,0.0f,  0.5f, 0.5f, 0.5f, 1.0f,0.0f,
    -0.5f,-0.5f,-0.5f, 0.0f,1.0f,  0.5f,-0.5f,-0.5f, 1.0f,1.0f,
     0.5f,-0.5f, 0.5f, 1.0f,0.0f,  0.5f,-0.5f, 0.5f, 1.0f,0.0f,
    -0.5f,-0.5f, 0.5f, 0.0f,0.0f, -0.5f,-0.5f,-0.5f, 0.0f,1.0f,
    -0.5f, 0.5f,-0.5f, 0.0f,1.0f,  0.5f, 0.5f,-0.5f, 1.0f,1.0f,
     0.5f, 0.5f, 0.5f, 1.0f,0.0f,  0.5f, 0.5f, 0.5f, 1.0f,0.0f,
    -0.5f, 0.5f, 0.5f, 0.0f,0.0f, -0.5f, 0.5f,-0.5f, 0.0f,1.0f,
};
constexpr float kPlaneVerts[] = {
     5.0f,-0.5f, 5.0f, 2.0f,0.0f, -5.0f,-0.5f, 5.0f, 0.0f,0.0f,
    -5.0f,-0.5f,-5.0f, 0.0f,2.0f,  5.0f,-0.5f, 5.0f, 2.0f,0.0f,
    -5.0f,-0.5f,-5.0f, 0.0f,2.0f,  5.0f,-0.5f,-5.0f, 2.0f,2.0f,
};
constexpr float kQuadVerts[] = {
    -1.0f, 1.0f, 0.0f,1.0f, -1.0f,-1.0f, 0.0f,0.0f,
     1.0f,-1.0f, 1.0f,0.0f, -1.0f, 1.0f, 0.0f,1.0f,
     1.0f,-1.0f, 1.0f,0.0f,  1.0f, 1.0f, 1.0f,1.0f,
};

// =============================================================================
// GLSL shaders
// =============================================================================
constexpr const char* kGlSceneVs = R"(#version 460 core
layout(location=0) in vec3 aPos; layout(location=1) in vec2 aTexCoords;
out vec2 TexCoords;
uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() {
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos,1.0);
})";
constexpr const char* kGlSceneFs = R"(#version 460 core
out vec4 FragColor; in vec2 TexCoords;
uniform sampler2D texture1;
void main() { FragColor = texture(texture1, TexCoords); }
)";
constexpr const char* kGlScreenVs = R"(#version 460 core
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aTexCoords;
out vec2 TexCoords;
void main() {
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
})";
constexpr const char* kGlScreenFs = R"(#version 460 core
out vec4 FragColor; in vec2 TexCoords;
uniform sampler2D screenTexture;
void main() { FragColor = vec4(texture(screenTexture, TexCoords).rgb, 1.0); }
)";

// =============================================================================
// HLSL shaders
// =============================================================================
constexpr const char* kD3D11SceneVs = R"(
cbuffer PerDraw : register(b0) { float4x4 model; float4x4 view; float4x4 projection; };
struct VS_IN  { float3 pos : POSITION; float2 uv : TEXCOORD; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
VS_OUT main(VS_IN i) {
    VS_OUT o; o.pos = mul(projection, mul(view, mul(model, float4(i.pos,1.0))));
    o.uv = i.uv; return o;
})";
constexpr const char* kD3D11SceneFs = R"(
Texture2D texture1 : register(t0); SamplerState sampler1 : register(s0);
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main(VS_OUT i) : SV_TARGET { return texture1.Sample(sampler1, i.uv); }
)";
constexpr const char* kD3D11ScreenVs = R"(
struct VS_IN  { float2 pos : POSITION; float2 uv : TEXCOORD; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
VS_OUT main(VS_IN i) {
    VS_OUT o; o.pos = float4(i.pos, 0.0, 1.0);
    o.uv = float2(i.uv.x, 1.0 - i.uv.y);  // flip Y: D3D texture origin top-left
    return o;
})";
constexpr const char* kD3D11ScreenFs = R"(
Texture2D screenTexture : register(t0); SamplerState sampler1 : register(s0);
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main(VS_OUT i) : SV_TARGET { return float4(screenTexture.Sample(sampler1, i.uv).rgb, 1.0); }
)";

// =============================================================================
int main(int argc, char* argv[]) {
    gfx::Backend backend = gfx::Backend::kOpenGL;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--d3d11") == 0) backend = gfx::Backend::kD3D11;
        else if (std::strcmp(argv[i], "--gl") == 0) backend = gfx::Backend::kOpenGL;
    }
    glfwInit();

    const char *sceneVs, *sceneFs, *screenVs, *screenFs;
    std::unique_ptr<glfw::Window> window;
    if (backend == gfx::Backend::kOpenGL) {
        glfw::Window::Hints h{}; h.client_api=GLFW_OPENGL_API;
        h.context_version_major=4; h.context_version_minor=6;
        h.context_profile=GLFW_OPENGL_CORE_PROFILE;
        window = std::make_unique<glfw::Window>(800,600,"Framebuffer (GL)",h);
        sceneVs=kGlSceneVs; sceneFs=kGlSceneFs;
        screenVs=kGlScreenVs; screenFs=kGlScreenFs;
    } else {
        glfw::Window::Hints h{}; h.client_api=GLFW_NO_API;
        window = std::make_unique<glfw::Window>(800,600,"Framebuffer (D3D11)",h);
        sceneVs=kD3D11SceneVs; sceneFs=kD3D11SceneFs;
        screenVs=kD3D11ScreenVs; screenFs=kD3D11ScreenFs;
    }
    if (!window||!window->Get()) { glfwTerminate(); return -1; }

    auto renderer = std::unique_ptr<gfx::IRenderer>(gfx::CreateRenderer(backend));
    if (!renderer||!renderer->Initialize(window->Get())) { glfwTerminate(); return -1; }
    std::cout << "Renderer: " << renderer->GetName() << std::endl;

    // ---- camera -----------------------------------------------------------
    Camera cam({0,0,3},{0,1,0},-90,0);
    float lx=400,ly=300,dt=0,lt=0; bool fm=true;
    window->on_key = [&](int k,int,int a,int){ if(k==GLFW_KEY_ESCAPE&&a==GLFW_PRESS)window->SetShouldClose(true); };
    window->on_framebuffer_size = [&](int w,int h){ renderer->SetViewport(0,0,(uint32_t)w,(uint32_t)h); };
    window->on_cursor_pos = [&](double x,double y){
        float fx=(float)x,fy=(float)y;
        if(fm){lx=fx;ly=fy;fm=false;}
        cam.ProcessMouse(fx-lx,ly-fy); lx=fx;ly=fy;
    };
    window->on_scroll = [&](double,double dy){ cam.ProcessScroll((float)dy); };
    glfwSetInputMode(window->Get(),GLFW_CURSOR,GLFW_CURSOR_DISABLED);

    // ---- compile shaders (two programs) -----------------------------------
    auto CompileProg = [&](const char* vsSrc, const char* fsSrc) -> gfx::Handle {
        gfx::Handle vs=renderer->CreateShader(gfx::ShaderStage::kVertex,vsSrc);
        gfx::Handle fs=renderer->CreateShader(gfx::ShaderStage::kFragment,fsSrc);
        if(vs==gfx::kInvalidHandle||fs==gfx::kInvalidHandle) return gfx::kInvalidHandle;
        gfx::Handle p=renderer->CreateProgram(vs,fs);
        renderer->DestroyShader(vs); renderer->DestroyShader(fs);
        return p;
    };
    gfx::Handle sceneProg  = CompileProg(sceneVs, sceneFs);
    gfx::Handle screenProg = CompileProg(screenVs, screenFs);
    if(sceneProg==gfx::kInvalidHandle||screenProg==gfx::kInvalidHandle){return -1;}

    int32_t locModel=renderer->GetUniformLocation(sceneProg,"model");
    int32_t locView=renderer->GetUniformLocation(sceneProg,"view");
    int32_t locProj=renderer->GetUniformLocation(sceneProg,"projection");

    // ---- buffers & layouts ------------------------------------------------
    constexpr uint32_t kStride5 = 5*sizeof(float);
    constexpr gfx::VertexAttrib kAttr5[] = {
        {0,"POSITION",gfx::AttribFormat::kFloat3,0},
        {1,"TEXCOORD",gfx::AttribFormat::kFloat2,3*sizeof(float)},
    };
    constexpr uint32_t kStride4 = 4*sizeof(float);
    constexpr gfx::VertexAttrib kAttr4[] = {
        {0,"POSITION",gfx::AttribFormat::kFloat2,0},
        {1,"TEXCOORD",gfx::AttribFormat::kFloat2,2*sizeof(float)},
    };

    gfx::Handle cubeVB  = renderer->CreateBuffer(gfx::BufferType::kVertex,gfx::Usage::kStatic,sizeof(kCubeVerts),kCubeVerts);
    gfx::Handle planeVB = renderer->CreateBuffer(gfx::BufferType::kVertex,gfx::Usage::kStatic,sizeof(kPlaneVerts),kPlaneVerts);
    gfx::Handle quadVB  = renderer->CreateBuffer(gfx::BufferType::kVertex,gfx::Usage::kStatic,sizeof(kQuadVerts),kQuadVerts);
    gfx::Handle cubeLay  = renderer->CreateInputLayout(sceneProg,kStride5,kAttr5,2);
    gfx::Handle planeLay = renderer->CreateInputLayout(sceneProg,kStride5,kAttr5,2);
    gfx::Handle quadLay  = renderer->CreateInputLayout(screenProg,kStride4,kAttr4,2);

    // ---- textures ---------------------------------------------------------
    stbi_set_flip_vertically_on_load(true);
    auto LoadTex = [&](const char* path)->gfx::Handle{
        int w,h,c; auto* d=stbi_load(path,&w,&h,&c,4);
        if(!d)return gfx::kInvalidHandle;
        gfx::Handle t=renderer->CreateTexture2D((uint32_t)w,(uint32_t)h,0,gfx::TextureFormat::kRgba8Unorm,d);
        stbi_image_free(d); if(t!=gfx::kInvalidHandle)renderer->GenerateMipmaps(t); return t;
    };
    gfx::Handle texCube = LoadTex("container.jpg");
    gfx::Handle texFloor = LoadTex("metal.png");

    gfx::SamplerDesc sd{}; sd.min_filter=sd.mag_filter=sd.mip_filter=gfx::FilterMode::kLinear;
    gfx::Handle samp = renderer->CreateSampler(sd);

    // ---- framebuffer ------------------------------------------------------
    int fbW=800,fbH=600; window->GetFramebufferSize(&fbW,&fbH);
    gfx::Handle fbColor = renderer->CreateTexture2D((uint32_t)fbW,(uint32_t)fbH,1,gfx::TextureFormat::kRgba8Unorm,nullptr);
    gfx::Handle fbDepth = renderer->CreateTexture2D((uint32_t)fbW,(uint32_t)fbH,1,gfx::TextureFormat::kDepth24Stencil8,nullptr);

    gfx::FramebufferAttachment colorAttach{fbColor,0};
    gfx::FramebufferAttachment depthAttach{fbDepth,0};
    gfx::Handle fbo = renderer->CreateFramebuffer(1,&colorAttach,&depthAttach);
    if(fbo==gfx::kInvalidHandle){std::cerr<<"FBO failed"<<std::endl;return -1;}

    // ---- render loop ------------------------------------------------------
    while(!window->ShouldClose()){
        glfwPollEvents();
        float now=(float)glfwGetTime(); dt=now-lt; lt=now;
        glm::vec3 dir(0); float s=2.5f*dt;
        if(window->GetKey(GLFW_KEY_W))dir+=cam.front();
        if(window->GetKey(GLFW_KEY_S))dir-=cam.front();
        if(window->GetKey(GLFW_KEY_A))dir-=glm::cross(cam.front(),cam.up());
        if(window->GetKey(GLFW_KEY_D))dir+=glm::cross(cam.front(),cam.up());
        if(glm::length(dir)>0)cam.ProcessKeyboard(glm::normalize(dir),dt);

        int fw,fh; window->GetFramebufferSize(&fw,&fh);
        float asp=fh>0?(float)fw/(float)fh:1;
        glm::mat4 proj=glm::perspective(glm::radians(cam.zoom()),asp,0.1f,100.0f);
        glm::mat4 view=cam.GetViewMatrix();

        // -- Pass 1: render scene to offscreen framebuffer --
        renderer->BindFramebuffer(fbo);
        renderer->SetViewport(0,0,(uint32_t)fw,(uint32_t)fh);
        gfx::DepthStencilDesc dsOn{}; dsOn.depth_test=dsOn.depth_write=true;
        renderer->SetDepthStencilState(dsOn);
        renderer->Clear(gfx::ClearFlags::kColor|gfx::ClearFlags::kDepth,0.1f,0.1f,0.1f);

        renderer->BindProgram(sceneProg);
        renderer->SetUniformMat4(locView,glm::value_ptr(view));
        renderer->SetUniformMat4(locProj,glm::value_ptr(proj));
        renderer->BindTexture(0,texCube);
        renderer->BindTexture(1,texFloor);
        renderer->BindSampler(0,samp);

        // cubes
        renderer->BindInputLayout(cubeLay);
        renderer->BindVertexBuffer(0,cubeVB,kStride5,0);
        glm::mat4 m(1); m=glm::translate(m,{-1,0,-1}); renderer->SetUniformMat4(locModel,glm::value_ptr(m)); renderer->Draw(36);
        m=glm::mat4(1); m=glm::translate(m,{2,0,0});  renderer->SetUniformMat4(locModel,glm::value_ptr(m)); renderer->Draw(36);
        // floor
        renderer->BindInputLayout(planeLay);
        renderer->BindVertexBuffer(0,planeVB,kStride5,0);
        renderer->BindTexture(0,texFloor);
        m=glm::mat4(1); renderer->SetUniformMat4(locModel,glm::value_ptr(m)); renderer->Draw(6);

        // -- Pass 2: render screen quad with FBO color texture --
        renderer->BindDefaultFramebuffer();
        gfx::DepthStencilDesc dsOff{}; dsOff.depth_test=false;
        renderer->SetDepthStencilState(dsOff);
        renderer->Clear(gfx::ClearFlags::kColor,1,1,1);

        renderer->BindProgram(screenProg);
        renderer->BindInputLayout(quadLay);
        renderer->BindVertexBuffer(0,quadVB,kStride4,0);
        renderer->BindTexture(0,fbColor);
        renderer->BindSampler(0,samp);
        renderer->Draw(6);

        renderer->Present();
    }

    // ---- cleanup ----------------------------------------------------------
    renderer->DestroyFramebuffer(fbo);
    renderer->DestroyTexture(fbDepth);
    renderer->DestroyTexture(fbColor);
    renderer->DestroySampler(samp);
    renderer->DestroyTexture(texFloor);
    renderer->DestroyTexture(texCube);
    renderer->DestroyInputLayout(quadLay); renderer->DestroyInputLayout(planeLay); renderer->DestroyInputLayout(cubeLay);
    renderer->DestroyBuffer(quadVB); renderer->DestroyBuffer(planeVB); renderer->DestroyBuffer(cubeVB);
    renderer->DestroyProgram(screenProg); renderer->DestroyProgram(sceneProg);
    renderer->Shutdown(); glfwTerminate(); return 0;
}
