// renderer.h — cross-API graphics abstraction
//
// Covers the full LearnOpenGL API surface (~55 topics, 8 chapters).
// Naming follows the Google C++ Style Guide:
//   Types   → PascalCase
//   Methods → PascalCase
//   Members → snake_case_  (trailing underscore)
//   Free vars → snake_case
//   Enums / constants → kPascalCase

#pragma once

#include <cstdint>

#include "platform.h"

namespace gfx {

// ===========================================================================
// Handle — opaque 32-bit resource identifier
// ===========================================================================
using Handle = uint32_t;
constexpr Handle kInvalidHandle = 0;

// ===========================================================================
// Backend
// ===========================================================================
enum class Backend {
    kOpenGL,
    kD3D11,
    kD3D12,
    kVulkan,
};

// ===========================================================================
// Buffer
// ===========================================================================
enum class BufferType {
    kVertex,
    kIndex,
    kUniform,
    kStorage,
};

enum class Usage {
    kStatic,
    kDynamic,
};

// ===========================================================================
// Primitive / index types
// ===========================================================================
enum class PrimitiveType {
    kTriangles,
    kTriangleStrip,
    kPoints,
    kLines,
};

enum class IndexFormat {
    kUint16,
    kUint32,
};

// ===========================================================================
// Vertex layout
// ===========================================================================
enum class AttribFormat {
    kFloat,
    kFloat2,
    kFloat3,
    kFloat4,
    kInt,
    kInt2,
    kInt3,
    kInt4,
    kUbyte4Norm,   // RGBA8 normalized unsigned byte  (color, bone weights)
};

inline uint32_t AttribSize(AttribFormat f) {
    constexpr uint32_t kSizes[] = {4, 8, 12, 16, 4, 8, 12, 16, 4};
    return kSizes[static_cast<uint8_t>(f)];
}

inline uint32_t AttribComponents(AttribFormat f) {
    constexpr uint32_t kComps[] = {1, 2, 3, 4, 1, 2, 3, 4, 4};
    return kComps[static_cast<uint8_t>(f)];
}

struct VertexAttrib {
    uint32_t     location;   // shader input location / semantic index
    const char*  semantic;   // "POSITION" / "NORMAL" / "TEXCOORD" / ...
    AttribFormat format;
    uint32_t     offset;     // byte offset within the vertex
};

// ===========================================================================
// Texture
// ===========================================================================
enum class TextureType {
    k2D,
    kCube,
    k2DMsaa,
    k3D,
};

enum class TextureFormat {
    kR8Unorm,
    kRg8Unorm,
    kRgba8Unorm,
    kRgba8Srgb,
    kR16F,
    kRg16F,
    kRgba16F,
    kR32F,
    kRg32F,
    kRgba32F,
    kR11G11B10F,
    kDepth16,
    kDepth24Stencil8,
    kDepth32F,
};

enum class CubeFace {
    kPositiveX,
    kNegativeX,
    kPositiveY,
    kNegativeY,
    kPositiveZ,
    kNegativeZ,
};

// ===========================================================================
// Sampler
// ===========================================================================
enum class FilterMode {
    kPoint,
    kLinear,
    kAnisotropic,
};

enum class WrapMode {
    kRepeat,
    kClampToEdge,
    kClampToBorder,
    kMirrorRepeat,
};

struct SamplerDesc {
    FilterMode min_filter      = FilterMode::kLinear;
    FilterMode mag_filter      = FilterMode::kLinear;
    FilterMode mip_filter      = FilterMode::kLinear;
    WrapMode   wrap_u          = WrapMode::kRepeat;
    WrapMode   wrap_v          = WrapMode::kRepeat;
    WrapMode   wrap_w          = WrapMode::kRepeat;
    float      border_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t    max_anisotropy  = 1;
    float      min_lod         = -1000.0f;
    float      max_lod         = 1000.0f;
};

// ===========================================================================
// Shader
// ===========================================================================
enum class ShaderStage {
    kVertex,
    kFragment,
    kGeometry,
    kCompute,
    kTessControl,
    kTessEval,
};

// ===========================================================================
// State descriptors  (maps to PSO in D3D12 / Vulkan, state blocks in D3D11,
//                     individual enables in OpenGL)
// ===========================================================================
enum class CompareFunc {
    kNever,
    kLess,
    kEqual,
    kLequal,
    kGreater,
    kNotequal,
    kGequal,
    kAlways,
};

enum class BlendFactor {
    kZero,
    kOne,
    kSrcColor,
    kInvSrcColor,
    kSrcAlpha,
    kInvSrcAlpha,
    kDstColor,
    kInvDstColor,
    kDstAlpha,
    kInvDstAlpha,
};

enum class BlendOp {
    kAdd,
    kSub,
    kRevSub,
    kMin,
    kMax,
};

enum class CullMode {
    kNone,
    kFront,
    kBack,
};

enum class FillMode {
    kSolid,
    kWireframe,
};

enum class StencilOp {
    kKeep,
    kZero,
    kReplace,
    kIncrClamp,
    kDecrClamp,
    kInvert,
    kIncrWrap,
    kDecrWrap,
};

struct RasterizerDesc {
    FillMode fill_mode        = FillMode::kSolid;
    CullMode cull_mode        = CullMode::kBack;
    bool     front_ccw        = true;    // GL convention: CCW = front
    bool     depth_clip       = true;
    bool     scissor_enabled  = false;
    int32_t  depth_bias       = 0;
    float    depth_bias_slope = 0.0f;
    float    depth_bias_clamp = 0.0f;
};

struct DepthStencilDesc {
    bool        depth_test   = true;
    bool        depth_write  = true;
    CompareFunc depth_func   = CompareFunc::kLess;
    bool        stencil_test = false;
    uint8_t     stencil_read_mask  = 0xFF;
    uint8_t     stencil_write_mask = 0xFF;
    int32_t     stencil_ref        = 0;

    struct StencilFace {
        StencilOp   fail_op       = StencilOp::kKeep;
        StencilOp   pass_op       = StencilOp::kKeep;
        StencilOp   depth_fail_op = StencilOp::kKeep;
        CompareFunc func          = CompareFunc::kAlways;
    };
    StencilFace front;
    StencilFace back;
};

struct BlendDesc {
    bool        enabled    = false;
    BlendFactor src_color  = BlendFactor::kSrcAlpha;
    BlendFactor dst_color  = BlendFactor::kInvSrcAlpha;
    BlendOp     color_op   = BlendOp::kAdd;
    BlendFactor src_alpha  = BlendFactor::kOne;
    BlendFactor dst_alpha  = BlendFactor::kInvSrcAlpha;
    BlendOp     alpha_op   = BlendOp::kAdd;
    uint8_t     write_mask = 0xF;   // RGBA
};

// ===========================================================================
// Framebuffer
// ===========================================================================
struct FramebufferAttachment {
    Handle   texture;
    uint8_t  mip_level = 0;
    CubeFace cube_face = CubeFace::kPositiveX;  // only used for cubemap
};

// ===========================================================================
// Compute
// ===========================================================================
enum class ImageAccess {
    kReadOnly,
    kWriteOnly,
    kReadWrite,
};

// ===========================================================================
// Clear
// ===========================================================================
enum class ClearFlags : uint8_t {
    kNone    = 0,
    kColor   = 1 << 0,
    kDepth   = 1 << 1,
    kStencil = 1 << 2,
};

inline ClearFlags operator|(ClearFlags a, ClearFlags b) {
    return static_cast<ClearFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(ClearFlags a, ClearFlags b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

// ===========================================================================
// IRenderer — the cross-API graphics interface
//
// Each backend (GL, D3D11, D3D12, Vulkan) implements this interface.
// Resource creation returns opaque Handle values; destruction is typed
// (DestroyBuffer, DestroyTexture, …) so the backend can dispatch correctly.
//
// State follows an incremental model (learn from GL style) — the backend
// caches state internally and creates PSOs lazily for D3D12 / Vulkan.
// ===========================================================================
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ---- lifecycle --------------------------------------------------------
    virtual const char* GetName() const = 0;
    virtual bool        Initialize(GLFWwindow* window) = 0;
    virtual void        Shutdown() = 0;
    virtual void        Resize(uint32_t width, uint32_t height) = 0;

    // ---- buffer -----------------------------------------------------------
    virtual Handle CreateBuffer(BufferType type, Usage usage,
                                 size_t size, const void* data = nullptr) = 0;
    virtual void   UpdateBuffer(Handle buffer, size_t offset, size_t size,
                                 const void* data) = 0;
    virtual void   DestroyBuffer(Handle buffer) = 0;

    // ---- texture ----------------------------------------------------------
    virtual Handle CreateTexture2D(uint32_t width, uint32_t height,
                                    uint32_t mip_levels, TextureFormat format,
                                    const void* data = nullptr) = 0;
    virtual Handle CreateTextureCube(uint32_t size, uint32_t mip_levels,
                                      TextureFormat format,
                                      const void* data[6] = nullptr) = 0;
    virtual Handle CreateTexture2DMsaa(uint32_t width, uint32_t height,
                                        uint32_t samples,
                                        TextureFormat format) = 0;
    virtual void   UpdateTexture(Handle texture, uint32_t mip_level,
                                  uint32_t x, uint32_t y, uint32_t z,
                                  uint32_t width, uint32_t height, uint32_t depth,
                                  const void* data) = 0;
    virtual void   GenerateMipmaps(Handle texture) = 0;
    virtual void   DestroyTexture(Handle texture) = 0;

    // ---- sampler ----------------------------------------------------------
    virtual Handle CreateSampler(const SamplerDesc& desc) = 0;
    virtual void   DestroySampler(Handle sampler) = 0;

    // ---- shader -----------------------------------------------------------
    virtual Handle  CreateShader(ShaderStage stage, const char* source) = 0;
    virtual Handle  CreateProgram(Handle vs, Handle fs,
                                   Handle gs = kInvalidHandle,
                                   Handle tcs = kInvalidHandle,
                                   Handle tes = kInvalidHandle) = 0;
    virtual Handle  CreateComputeProgram(Handle cs) = 0;
    virtual int32_t GetUniformLocation(Handle program, const char* name) = 0;
    virtual void    DestroyShader(Handle shader) = 0;
    virtual void    DestroyProgram(Handle program) = 0;

    // ---- input layout -----------------------------------------------------
    // program:  D3D11 needs VS bytecode; GL ignores it.
    // stride:   byte distance between consecutive vertices.
    // attribs:  array of per-attribute descriptors (semantic, format, offset).
    // Buffer binding is done separately via BindVertexBuffer / BindIndexBuffer.
    virtual Handle CreateInputLayout(Handle program,
                                      uint32_t stride,
                                      const VertexAttrib* attribs,
                                      uint32_t count) = 0;
    virtual void   DestroyInputLayout(Handle layout) = 0;

    // ---- framebuffer ------------------------------------------------------
    virtual Handle CreateFramebuffer(
        uint32_t num_color, const FramebufferAttachment* color,
        const FramebufferAttachment* depth_stencil = nullptr) = 0;
    virtual void   DestroyFramebuffer(Handle framebuffer) = 0;

    // ---- state ------------------------------------------------------------
    virtual void SetRasterizerState(const RasterizerDesc& desc) = 0;
    virtual void SetDepthStencilState(const DepthStencilDesc& desc) = 0;
    virtual void SetBlendState(const BlendDesc& desc) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height) = 0;
    virtual void SetScissor(uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height) = 0;
    virtual void SetPrimitiveType(PrimitiveType type) = 0;
    virtual void SetVertexLayout(const VertexAttrib* attribs,
                                  uint32_t count, uint32_t stride) = 0;

    // ---- resource binding -------------------------------------------------
    virtual void BindProgram(Handle program) = 0;
    virtual void BindInputLayout(Handle layout) = 0;
    virtual void BindVertexBuffer(uint32_t slot, Handle buffer,
                                   uint32_t stride, uint32_t offset = 0) = 0;
    virtual void BindIndexBuffer(Handle buffer, IndexFormat format) = 0;
    virtual void BindTexture(uint32_t slot, Handle texture) = 0;
    virtual void BindSampler(uint32_t slot, Handle sampler) = 0;
    virtual void BindUniformBuffer(uint32_t slot, Handle buffer,
                                    size_t offset = 0, size_t size = 0) = 0;
    virtual void BindStorageBuffer(uint32_t slot, Handle buffer) = 0;
    virtual void BindFramebuffer(Handle framebuffer) = 0;
    virtual void BindDefaultFramebuffer() = 0;

    // ---- uniforms  (program must already be bound) ------------------------
    virtual void SetUniformInt(int32_t location, int32_t value) = 0;
    virtual void SetUniformFloat(int32_t location, float value) = 0;
    virtual void SetUniformVec2(int32_t location, const float* value) = 0;
    virtual void SetUniformVec3(int32_t location, const float* value) = 0;
    virtual void SetUniformVec4(int32_t location, const float* value) = 0;
    virtual void SetUniformMat3(int32_t location, const float* value) = 0;
    virtual void SetUniformMat4(int32_t location, const float* value) = 0;

    // ---- clear & draw -----------------------------------------------------
    virtual void Clear(ClearFlags flags,
                        float r = 0.0f, float g = 0.0f,
                        float b = 0.0f, float a = 1.0f,
                        float depth = 1.0f, uint8_t stencil = 0) = 0;
    virtual void Draw(uint32_t vertex_count,
                       uint32_t first_vertex = 0) = 0;
    virtual void DrawIndexed(uint32_t index_count,
                              uint32_t first_index = 0,
                              int32_t vertex_offset = 0) = 0;
    virtual void DrawInstanced(uint32_t vertex_count_per_instance,
                                uint32_t instance_count,
                                uint32_t first_vertex = 0,
                                uint32_t first_instance = 0) = 0;
    virtual void DrawIndexedInstanced(uint32_t index_count_per_instance,
                                       uint32_t instance_count,
                                       uint32_t first_index = 0,
                                       int32_t vertex_offset = 0,
                                       uint32_t first_instance = 0) = 0;

    // ---- compute ----------------------------------------------------------
    virtual void BindImageTexture(uint32_t slot, Handle texture,
                                   uint32_t mip_level, ImageAccess access) = 0;
    virtual void Dispatch(uint32_t groups_x, uint32_t groups_y,
                           uint32_t groups_z) = 0;
    virtual void MemoryBarrier() = 0;

    // ---- debug ------------------------------------------------------------
    virtual void PushDebugGroup(const char* name) = 0;
    virtual void PopDebugGroup() = 0;

    // ---- backend query -----------------------------------------------------
    virtual Backend GetBackend() const = 0;

    // ---- present ----------------------------------------------------------
    virtual void Present() = 0;
};

// ===========================================================================
// Factory
// ===========================================================================
IRenderer* CreateRenderer(Backend backend);
void       DestroyRenderer(IRenderer* renderer);

}  // namespace gfx
