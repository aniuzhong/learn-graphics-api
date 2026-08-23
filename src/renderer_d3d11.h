// renderer_d3d11.h — Direct3D 11 backend
//   Google C++ Style: PascalCase methods, snake_case members_

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "renderer.h"


namespace gfx {



using Microsoft::WRL::ComPtr;

class RendererD3D11 final : public IRenderer {
public:
    const char* GetName() const override;
    Backend     GetBackend() const override;

    bool Initialize(GLFWwindow* window) override;

    void Shutdown() override;

    void Resize(uint32_t width, uint32_t height) override;

    // ===================================================================
    // buffer
    // ===================================================================
    Handle CreateBuffer(BufferType type, Usage /*usage*/, size_t size, const void* data) override;

    void UpdateBuffer(Handle buffer, size_t offset, size_t size,
                       const void* data) override;

    void DestroyBuffer(Handle buffer) override;

    // ===================================================================
    // texture
    // ===================================================================
    Handle CreateTexture2D(uint32_t width, uint32_t height,
                            uint32_t mip_levels, TextureFormat format,
                            const void* data) override;

    Handle CreateTextureCube(uint32_t size, uint32_t mip_levels,
                              TextureFormat format,
                              const void* data[6]) override;

    Handle CreateTexture2DMsaa(uint32_t width, uint32_t height,
                                uint32_t samples,
                                TextureFormat format) override;

    void UpdateTexture(Handle texture, uint32_t mip_level,
                       uint32_t x, uint32_t y, uint32_t z,
                       uint32_t width, uint32_t height, uint32_t /*depth*/,
                       const void* data) override;

    void GenerateMipmaps(Handle texture) override;

    void DestroyTexture(Handle texture) override;

    // ===================================================================
    // sampler
    // ===================================================================
    Handle CreateSampler(const SamplerDesc& desc) override;

    void DestroySampler(Handle sampler) override;

    // ===================================================================
    // shader
    // ===================================================================
    Handle CreateShader(ShaderStage stage, const char* source) override;

    Handle CreateProgram(Handle vs, Handle fs, Handle /*gs*/, Handle /*tcs*/, Handle /*tes*/) override;

    Handle CreateComputeProgram(Handle) override;

    int32_t GetUniformLocation(Handle program, const char* name) override;

    void DestroyShader(Handle shader) override;

    void DestroyProgram(Handle program) override;

    // ===================================================================
    // input layout
    // ===================================================================
    Handle CreateInputLayout(Handle program, uint32_t /*stride*/, const VertexAttrib* attribs, uint32_t count) override;

    void DestroyInputLayout(Handle layout) override;

    // ===================================================================
    // framebuffer
    // ===================================================================
    Handle CreateFramebuffer(uint32_t num_color,
                             const FramebufferAttachment* color,
                             const FramebufferAttachment* depth_stencil) override;

    void DestroyFramebuffer(Handle framebuffer) override;

    // ===================================================================
    // state
    // ===================================================================
    void SetRasterizerState(const RasterizerDesc& desc) override;

    void SetDepthStencilState(const DepthStencilDesc& desc) override;

    void SetBlendState(const BlendDesc& desc) override;

    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void SetScissor(uint32_t x, uint32_t y,
                     uint32_t width, uint32_t height) override;

    void SetPrimitiveType(PrimitiveType type) override;

    void SetVertexLayout(const VertexAttrib*, uint32_t, uint32_t) override;

    void UploadConstantBuffer();

    // ===================================================================
    // resource binding
    // ===================================================================
    void BindProgram(Handle program) override;

    void BindInputLayout(Handle layout) override;

    void BindVertexBuffer(uint32_t slot, Handle buffer,
                           uint32_t stride, uint32_t offset) override;

    void BindIndexBuffer(Handle buffer, IndexFormat format) override;

    void BindTexture(uint32_t slot, Handle texture) override;
    void BindSampler(uint32_t slot, Handle sampler) override;
    void BindUniformBuffer(uint32_t, Handle, size_t, size_t) override;
    void BindStorageBuffer(uint32_t, Handle) override;
    void BindFramebuffer(Handle framebuffer) override;

    void BindDefaultFramebuffer() override;

    // ===================================================================
    // uniforms  (stubs — hello_triangle doesn't use uniforms)
    // ===================================================================
    void SetUniformInt(int32_t location, int32_t value) override;
    void SetUniformFloat(int32_t location, float value) override;
    void SetUniformVec2(int32_t location, const float* value) override;
    void SetUniformVec3(int32_t location, const float* value) override;
    void SetUniformVec4(int32_t location, const float* value) override;
    void SetUniformMat3(int32_t location, const float* value) override;
    void SetUniformMat4(int32_t location, const float* value) override;

    // ===================================================================
    // clear & draw
    // ===================================================================
    void Clear(ClearFlags flags,
                float r, float g, float b, float a,
                float depth, uint8_t stencil) override;

    void Draw(uint32_t vertex_count, uint32_t first_vertex) override;

    void DrawIndexed(uint32_t index_count, uint32_t first_index,
                      int32_t /*vertex_offset*/) override;

    void DrawInstanced(uint32_t vertex_count_per_instance,
                        uint32_t instance_count,
                        uint32_t first_vertex,
                        uint32_t first_instance) override;

    void DrawIndexedInstanced(uint32_t index_count_per_instance,
                               uint32_t instance_count,
                               uint32_t first_index,
                               int32_t /*vertex_offset*/,
                               uint32_t first_instance) override;

    // ===================================================================
    // compute  (stubs)
    // ===================================================================
    void BindImageTexture(uint32_t, Handle, uint32_t, ImageAccess) override;
    void Dispatch(uint32_t, uint32_t, uint32_t) override;
    void MemoryBarrier() override;

    // ===================================================================
    // debug
    // ===================================================================
    void PushDebugGroup(const char*) override;
    void PopDebugGroup() override;

    // ===================================================================
    // present
    // ===================================================================
    void Present() override;

    // ===================================================================
    // native-handle accessors  (cast to RendererD3D11* to use)
    // ===================================================================
    ID3D11Device*        GetDevice()  const;
    ID3D11DeviceContext* GetContext() const;

private:
    // ================================================================
    // D3D11 enum helpers
    // ================================================================
    static const char* ToD3D11ShaderTarget(ShaderStage s);

    static UINT ToD3D11BindFlags(BufferType t);

    static DXGI_FORMAT ToDxgiFormat(AttribFormat f);

    static DXGI_FORMAT ToDxgiIndexFormat(IndexFormat f);

    static D3D11_CULL_MODE ToD3D11CullMode(CullMode m);

    static D3D11_COMPARISON_FUNC ToD3D11Comparison(CompareFunc f);

    static D3D11_STENCIL_OP ToD3D11StencilOp(StencilOp op);

    static D3D11_BLEND ToD3D11Blend(BlendFactor f);

    static D3D11_BLEND_OP ToD3D11BlendOp(BlendOp op);

    static D3D11_PRIMITIVE_TOPOLOGY ToD3D11Topology(PrimitiveType t);

    static DXGI_FORMAT ToDxgiTextureFormat(TextureFormat f);

    static UINT BytesPerPixel(TextureFormat f);

    static bool IsDepthFormat(TextureFormat f);

    static D3D11_FILTER ToD3D11Filter(const SamplerDesc& d);

    static D3D11_TEXTURE_ADDRESS_MODE ToD3D11AddressMode(WrapMode w);

    // ================================================================
    // device / swap-chain creation
    // ================================================================
    bool CreateDeviceAndSwapChain();

    bool CreateBackBufferRtv();

    bool CreateDepthStencil();

    void CreateDefaultStates();

    // ================================================================
    // internal types
    // ================================================================
    struct ShaderData {
        ComPtr<ID3DBlob> blob;
        ShaderStage      stage = ShaderStage::kVertex;
    };

    static constexpr size_t kMaxConstantBufferSize = 1024;

    struct ProgramData {
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader>  ps;
        ComPtr<ID3DBlob>           vs_blob;
        ComPtr<ID3D11Buffer>       cb;
        uint8_t                    cb_shadow[kMaxConstantBufferSize] = {};
        bool                       cb_dirty = false;
        std::unordered_map<std::string, int32_t> uniform_locations;
    };

    struct BufferData {
        ComPtr<ID3D11Buffer> buffer;
        BufferType type   = BufferType::kVertex;
        size_t     size   = 0;
    };

    struct InputLayoutData {
        ComPtr<ID3D11InputLayout> layout;
    };

    struct TextureData {
        ComPtr<ID3D11Texture2D>          texture;
        ComPtr<ID3D11ShaderResourceView> srv;
        TextureType  type       = TextureType::k2D;
        TextureFormat format    = TextureFormat::kRgba8Unorm;
        uint32_t     width      = 0;
        uint32_t     height     = 0;
        uint32_t     depth      = 1;
        uint32_t     mip_levels = 1;
    };

    struct FramebufferData {
        std::vector<ComPtr<ID3D11RenderTargetView>> color_rtvs;
        ComPtr<ID3D11DepthStencilView> ds_view;
    };

    // ================================================================
    // state
    // ================================================================
    GLFWwindow* window_ = nullptr;
    HWND        hwnd_   = nullptr;
    uint32_t    width_  = 0;
    uint32_t    height_ = 0;
    Handle      next_handle_ = 1;
    Handle      current_program_ = 0;

    static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    ComPtr<ID3D11Device>            device_;
    ComPtr<ID3D11DeviceContext>     context_;
    ComPtr<IDXGISwapChain>          swap_chain_;
    ComPtr<ID3D11RenderTargetView>  rtv_;
    ComPtr<ID3D11DepthStencilView>  dsv_;
    ComPtr<ID3D11Texture2D>         depth_texture_;
    ComPtr<ID3D11RasterizerState>   rasterizer_state_;
    ComPtr<ID3D11DepthStencilState> depth_stencil_state_;
    ComPtr<ID3D11BlendState>        blend_state_;

    ID3D11RenderTargetView* current_rtv_ptrs_[8] = {};
    UINT                     current_rtv_count_   = 0;
    ID3D11DepthStencilView*  current_dsv_ptr_     = nullptr;

    std::unordered_map<Handle, ShaderData>      shaders_;
    std::unordered_map<Handle, ProgramData>     programs_;
    std::unordered_map<Handle, BufferData>      buffers_;
    std::unordered_map<Handle, InputLayoutData> input_layouts_;
    std::unordered_map<Handle, TextureData>     textures_;
    std::unordered_map<Handle, ComPtr<ID3D11SamplerState>> samplers_;
    std::unordered_map<Handle, FramebufferData> framebuffers_;
};


}  // namespace gfx
