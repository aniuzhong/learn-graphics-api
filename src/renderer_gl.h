// renderer_gl.h — OpenGL 4.6 backend
//   Google C++ Style: PascalCase methods, snake_case members_

#pragma once

#include <unordered_map>
#include <vector>

#include "renderer.h"


namespace gfx {



// ============================================================================
// RendererGl — maps IRenderer to OpenGL 4.6 core
// ============================================================================
class RendererGl final : public IRenderer {
public:
    const char* GetName() const override;
    Backend GetBackend() const override;

    bool Initialize(GLFWwindow* window) override;

    void Shutdown() override;

    void Resize(uint32_t width, uint32_t height) override;

    // -------------------------------------------------------------------
    // buffer
    // -------------------------------------------------------------------
    Handle CreateBuffer(BufferType type, Usage /*usage*/, size_t size, const void* data) override;

    void UpdateBuffer(Handle buffer, size_t offset, size_t size, const void* data) override;

    void DestroyBuffer(Handle buffer) override;

    // -------------------------------------------------------------------
    // texture
    // -------------------------------------------------------------------
    Handle CreateTexture2D(uint32_t width, uint32_t height,
                            uint32_t mip_levels, TextureFormat format,
                            const void* data) override;

    Handle CreateTextureCube(uint32_t size, uint32_t mip_levels, TextureFormat format, const void* data[6]) override;

    Handle CreateTexture2DMsaa(uint32_t width, uint32_t height, uint32_t samples, TextureFormat format) override;

    void UpdateTexture(Handle texture, uint32_t mip_level,
                       uint32_t x, uint32_t y, uint32_t z,
                       uint32_t width, uint32_t height, uint32_t /*depth*/,
                       const void* data) override;

    void GenerateMipmaps(Handle texture) override;

    void DestroyTexture(Handle texture) override;

    // -------------------------------------------------------------------
    // sampler
    // -------------------------------------------------------------------
    Handle CreateSampler(const SamplerDesc& desc) override;

    void DestroySampler(Handle sampler) override;

    // -------------------------------------------------------------------
    // shader
    // -------------------------------------------------------------------
    Handle CreateShader(ShaderStage stage, const char* source) override;

    Handle CreateProgram(Handle vs, Handle fs, Handle gs,
                          Handle tcs, Handle tes) override;

    Handle CreateComputeProgram(Handle cs) override;

    int32_t GetUniformLocation(Handle program, const char* name) override;

    void DestroyShader(Handle shader) override;

    void DestroyProgram(Handle program) override;

    // -------------------------------------------------------------------
    // input layout
    //
    // Creates a VAO describing the vertex format.  Actual buffer binding
    // happens later via BindVertexBuffer / BindIndexBuffer (GL 4.3+ DSA).
    // -------------------------------------------------------------------
    Handle CreateInputLayout(Handle /*program*/, uint32_t stride, const VertexAttrib* attribs, uint32_t count) override;

    void DestroyInputLayout(Handle layout) override;

    // -------------------------------------------------------------------
    // framebuffer
    // -------------------------------------------------------------------
    Handle CreateFramebuffer(
        uint32_t num_color, const FramebufferAttachment* color,
        const FramebufferAttachment* depth_stencil) override;

    void DestroyFramebuffer(Handle framebuffer) override;

    // -------------------------------------------------------------------
    // state
    // -------------------------------------------------------------------
    void SetRasterizerState(const RasterizerDesc& desc) override;

    void SetDepthStencilState(const DepthStencilDesc& desc) override;

    void SetBlendState(const BlendDesc& desc) override;

    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void SetPrimitiveType(PrimitiveType type) override;

    void SetVertexLayout(const VertexAttrib* attribs, uint32_t count,
                          uint32_t stride) override;

    // -------------------------------------------------------------------
    // resource binding
    // -------------------------------------------------------------------
    void BindProgram(Handle program) override;

    void BindInputLayout(Handle layout) override;

    void BindVertexBuffer(uint32_t slot, Handle buffer, uint32_t stride, uint32_t offset) override;

    void BindIndexBuffer(Handle buffer, IndexFormat format) override;

    void BindTexture(uint32_t slot, Handle texture) override;

    void BindSampler(uint32_t slot, Handle sampler) override;

    void BindUniformBuffer(uint32_t slot, Handle buffer,
                            size_t offset, size_t size) override;

    void BindStorageBuffer(uint32_t slot, Handle buffer) override;

    void BindFramebuffer(Handle framebuffer) override;

    void BindDefaultFramebuffer() override;

    // -------------------------------------------------------------------
    // uniforms
    // -------------------------------------------------------------------
    void SetUniformInt(int32_t location, int32_t value) override;
    void SetUniformFloat(int32_t location, float value) override;
    void SetUniformVec2(int32_t location, const float* value) override;
    void SetUniformVec3(int32_t location, const float* value) override;
    void SetUniformVec4(int32_t location, const float* value) override;
    void SetUniformMat3(int32_t location, const float* value) override;
    void SetUniformMat4(int32_t location, const float* value) override;

    // -------------------------------------------------------------------
    // clear & draw
    // -------------------------------------------------------------------
    void Clear(ClearFlags flags,
                float r, float g, float b, float a,
                float depth, uint8_t stencil) override;

    void Draw(uint32_t vertex_count, uint32_t first_vertex) override;

    void DrawIndexed(uint32_t index_count, uint32_t first_index,
                      int32_t vertex_offset) override;

    void DrawInstanced(uint32_t vertex_count_per_instance,
                        uint32_t instance_count,
                        uint32_t first_vertex,
                        uint32_t first_instance) override;

    void DrawIndexedInstanced(uint32_t index_count_per_instance,
                               uint32_t instance_count,
                               uint32_t first_index,
                               int32_t vertex_offset,
                               uint32_t first_instance) override;

    // -------------------------------------------------------------------
    // compute
    // -------------------------------------------------------------------
    void BindImageTexture(uint32_t slot, Handle texture, uint32_t mip_level, ImageAccess access) override;

    void Dispatch(uint32_t groups_x, uint32_t groups_y,
                   uint32_t groups_z) override;

    void MemoryBarrier() override;

    // -------------------------------------------------------------------
    // debug
    // -------------------------------------------------------------------
    void PushDebugGroup(const char* name) override;

    void PopDebugGroup() override;

    // -------------------------------------------------------------------
    // present
    // -------------------------------------------------------------------
    void Present() override;

    // -------------------------------------------------------------------
    // native-handle accessors  (non-virtual — cast to RendererGl* to use)
    // -------------------------------------------------------------------
    GLuint GetNativeTexture(Handle h) const;
    GLuint GetNativeBuffer(Handle h)  const;
    GLuint GetNativeProgram(Handle h) const;

private:
    // ================================================================
    // GL enum helpers
    // ================================================================
    static GLenum ToGlTarget(BufferType type);

    static GLenum ToGlTarget(TextureType type);

    static GLenum ToGlShaderStage(ShaderStage s);

    static GLenum ToGlInternalFormat(TextureFormat f);

    static GLenum ToGlPixelFormat(TextureFormat f);

    static GLenum ToGlPixelType(TextureFormat f);

    static GLenum ToGlAttribType(AttribFormat f);

    static bool IsNormalized(AttribFormat f);

    static GLenum ToGlTopology(PrimitiveType t);

    static GLenum ToGlCompareFunc(CompareFunc f);

    static GLenum ToGlBlendFactor(BlendFactor f);

    static GLenum ToGlBlendOp(BlendOp op);

    static GLenum ToGlCullFace(CullMode m);

    static GLenum ToGlStencilOp(StencilOp op);

    static GLenum ToGlMinFilter(const SamplerDesc& d);

    static GLenum ToGlMagFilter(const SamplerDesc& d);

    static GLenum ToGlWrap(WrapMode w);

    static const char* ToString(ShaderStage s);

    // ================================================================
    // helpers
    // ================================================================
    void AttachShader(GLuint program, Handle shader);

    void DetachShader(GLuint program, Handle shader);

    // ================================================================
    // resource records
    // ================================================================
    struct BufferData {
        GLuint     id    = 0;
        BufferType type  = BufferType::kVertex;
        size_t     size  = 0;  // filled in CreateBuffer
    };

    struct TextureData {
        GLuint        id = 0;
        TextureType   type = TextureType::k2D;
        TextureFormat format = TextureFormat::kRgba8Unorm;
        uint32_t      width = 0, height = 0, depth = 1;
        uint32_t      mip_levels = 1;
    };

    struct InputLayoutData {
        GLuint vao = 0;
    };

    struct ShaderData {
        GLuint      id    = 0;
        ShaderStage stage = ShaderStage::kVertex;
    };

    // ================================================================
    // state
    // ================================================================
    GLFWwindow* window_ = nullptr;
    uint32_t    width_  = 0;
    uint32_t    height_ = 0;
    Handle      next_handle_ = 1;

    GLuint      current_program_ = 0;
    GLuint      current_fbo_ = 0;
    GLuint      current_vb_  = 0;
    GLuint      current_ib_  = 0;
    GLenum      current_topology_ = GL_TRIANGLES;
    GLenum      current_index_format_ = GL_UNSIGNED_INT;

    RasterizerDesc            rasterizer_;
    DepthStencilDesc          depth_stencil_;
    std::vector<VertexAttrib> vertex_attribs_;
    uint32_t                  attrib_stride_ = 0;

    std::unordered_map<Handle, BufferData>      buffers_;
    std::unordered_map<Handle, TextureData>     textures_;
    std::unordered_map<Handle, GLuint>          samplers_;
    std::unordered_map<Handle, ShaderData>      shaders_;
    std::unordered_map<Handle, GLuint>          programs_;
    std::unordered_map<Handle, InputLayoutData> input_layouts_;
    std::unordered_map<Handle, GLuint>          framebuffers_;
};


}  // namespace gfx
