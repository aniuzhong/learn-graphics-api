# Renderer Abstraction Layer — Design Document

## Motivation

The current `hello_triangle.cpp` intermixes two concerns:

- **Scene logic** (what to draw): vertex data, shader source, draw calls
- **API boilerplate** (how to draw): device creation, swap chain setup, RTV/DSV creation,
  shader compilation, input layout, render loop state plumbing

Out of ~275 lines, only ~30 are scene-specific. The remaining ~240 are identical across
any D3D11 sample and would be duplicated into every new program.

This design extracts the boilerplate behind a thin, backend-agnostic interface so that:

1. Each sample file contains only scene logic
2. Adding a new graphics backend (OpenGL, Vulkan) means implementing one interface
3. CMake selects the backend at configure time — sample code does not change

---

## Architecture Overview

```
┌──────────────────────────────────────────────────┐
│  hello_triangle.cpp  (sample — scene logic only) │
├──────────────────────────────────────────────────┤
│  IGfxRenderer        (abstract interface)        │
├──────────────────────────────────────────────────┤
│  D3D11Renderer       (D3D11 implementation)      │
│  GLRenderer          (future OpenGL impl)        │
├──────────────────────────────────────────────────┤
│  gfx_common.h        (shared types & handles)    │
└──────────────────────────────────────────────────┘
```

The sample code sees only `IGfxRenderer` and opaque handle types. It never includes
`d3d11.h` or `gl.h` directly.

---

## Layer 1 — `gfx_common.h` (platform-agnostic types)

This header has zero dependencies on any graphics API. It defines the vocabulary shared
by all backends.

### Vertex attribute descriptors

```cpp
enum class Attrib {
    Position,
    Normal,
    Color,
    TexCoord0,
    TexCoord1,
};

enum class AttribFormat {
    Float,       // 1 x float
    Float2,      // 2 x float
    Float3,      // 3 x float
    Float4,      // 4 x float
    UByte4_Norm, // 4 x uint8_t, normalized to [0,1]
};

struct VertexElement {
    Attrib       attrib;
    AttribFormat format;
    uint32_t     offset;  // byte offset from start of vertex
};

struct VertexLayout {
    std::vector<VertexElement> elements;
    uint32_t                   stride;  // total bytes per vertex

    // Convenience builder — returns *this for chaining.
    VertexLayout& add(Attrib a, AttribFormat f);
};
```

### Primitive topology

```cpp
enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};
```

### Opaque resource handles

Typed integer handles isolate the sample from backend pointer types (`ID3D11Buffer*`,
`GLuint`, `VkBuffer`). The backend owns the actual resource; the handle is just an
index into an internal pool.

```cpp
template <typename Tag>
struct GfxHandle {
    uint32_t idx = UINT32_MAX;

    explicit operator bool() const { return idx != UINT32_MAX; }
    bool operator==(const GfxHandle&) const = default;
};

struct ShaderTag {};
struct BufferTag {};

using GfxShader = GfxHandle<ShaderTag>;   // vertex or pixel shader
using GfxBuffer = GfxHandle<BufferTag>;   // vertex or index buffer
```

A handle evaluates to `false` when default-constructed; `true` after successful creation.

---

## Layer 2 — `gfx_renderer.h` (abstract interface)

```cpp
#pragma once
#include "gfx_common.h"
#include <string_view>

struct GfxConfig {
    void*  native_window;   // HWND on Windows, X11 Window on Linux, etc.
    int    width;
    int    height;
    int    msaa_samples  = 4;
    int    buffer_count  = 2;
    bool   debug         = true;
};

class IGfxRenderer {
public:
    virtual ~IGfxRenderer() = default;

    // ---- lifecycle ----
    virtual bool init(const GfxConfig& cfg) = 0;
    virtual void shutdown()                = 0;
    virtual void resize(int width, int height) = 0;

    // ---- shaders ----
    virtual GfxShader createVertexShader(std::string_view source,
                                         std::string_view entry_point) = 0;
    virtual GfxShader createPixelShader (std::string_view source,
                                         std::string_view entry_point) = 0;
    virtual void      destroyShader(GfxShader s) = 0;

    // ---- buffers ----
    virtual GfxBuffer createVertexBuffer(const void* data,
                                         size_t      size,
                                         const VertexLayout& layout) = 0;
    virtual GfxBuffer createIndexBuffer (const void* data,
                                         size_t      size,
                                         bool        index32bit) = 0;
    virtual void      destroyBuffer(GfxBuffer b) = 0;

    // ---- per-frame ----
    virtual void beginFrame(const float clear_color[4]) = 0;
    virtual void bindShader(GfxShader vs, GfxShader ps) = 0;
    virtual void bindVertexBuffer(GfxBuffer vb) = 0;
    virtual void bindIndexBuffer (GfxBuffer ib) = 0;
    virtual void setTopology(PrimitiveTopology topo) = 0;
    virtual void draw(int vertex_count, int start_vertex = 0) = 0;
    virtual void drawIndexed(int index_count, int start_index = 0,
                             int base_vertex = 0) = 0;
    virtual void endFrame() = 0;  // calls Present / SwapBuffers internally
};
```

### Design decisions

| Decision | Rationale |
|---|---|
| Shader source passed as `string_view` | Learning project — avoids an offline shader compiler. The backend calls `D3DCompile` / `glCompileShader` internally. |
| `createVertexShader` returns a single handle | The D3D11 backend stores the compiled blob alongside the shader so `CreateInputLayout` can use it later. The GL backend ignores the blob. |
| No `setBlendState` / `setDepthStencil` yet | Premature for the current scope. Add these when samples need them. |
| No constant-buffer / uniform API yet | Likewise — add when shaders need per-draw parameters. |
| `void*` for native window | Avoids leaking Windows types into the interface. Each backend casts internally. |

---

## Layer 3 — `renderer_d3d11.h` / `renderer_d3d11.cpp` (D3D11 backend)

### Internal resource pools

Resources are stored in `std::vector` and referenced by handle index:

```cpp
class D3D11Renderer : public IGfxRenderer {
    // ...
private:
    struct ShaderEntry {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
        Microsoft::WRL::ComPtr<ID3DBlob>           blob;    // for input layout creation
        bool is_vertex;
    };

    struct BufferEntry {
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        uint32_t size;   // total bytes
        uint32_t stride; // vertex stride (vertex buffers only)
        bool     is_index;
        bool     index32;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
    };

    std::vector<ShaderEntry> m_shaders;
    std::vector<BufferEntry> m_buffers;

    // Core D3D11 objects
    Microsoft::WRL::ComPtr<ID3D11Device>        m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>      m_swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;
    D3D11_VIEWPORT m_viewport;

    // Dirty tracking for redundant-call elimination
    GfxShader m_cur_vs;
    GfxShader m_cur_ps;
    GfxBuffer m_cur_vb;
    GfxBuffer m_cur_ib;
    PrimitiveTopology m_cur_topo;
};
```

### `init()` flow

This is where all the boilerplate from the current `main()` moves to:

1. Create `ID3D11Device` and `ID3D11DeviceContext`
   - Feature level array: 11_1 → 11_0 → 10_1 → 10_0
   - `D3D11_CREATE_DEVICE_BGRA_SUPPORT` | `D3D11_CREATE_DEVICE_DEBUG` (if debug enabled)
   - Fallback: retry without `D3D11_CREATE_DEVICE_DEBUG` if the first attempt fails
2. Create `IDXGISwapChain` via `DXGI_SWAP_CHAIN_DESC`
3. Obtain back-buffer `ID3D11Texture2D` → `CreateRenderTargetView`
4. Create depth-stencil texture → `CreateDepthStencilView`
5. Set up `D3D11_VIEWPORT` (0, 0, width, height, 0.0f, 1.0f)

Error handling: return `false` on failure. Log diagnostic info via `OutputDebugStringA`.

### `createVertexShader()` / `createPixelShader()`

```cpp
GfxShader D3D11Renderer::createVertexShader(std::string_view source,
                                             std::string_view entry) {
    ComPtr<ID3DBlob> blob = compile_shader(source, entry, "vs_5_0");
    if (!blob) return {};

    ComPtr<ID3D11VertexShader> vs;
    HRESULT hr = m_device->CreateVertexShader(
        blob->GetBufferPointer(), blob->GetBufferSize(),
        nullptr, vs.GetAddressOf());
    if (FAILED(hr)) return {};

    uint32_t idx = static_cast<uint32_t>(m_shaders.size());
    m_shaders.push_back({nullptr, nullptr, blob, true});
    m_shaders[idx].vs = vs;
    m_shaders[idx].blob = blob;
    return {idx};
}
```

`createPixelShader` is analogous, using `"ps_5_0"` and storing into `.ps`.

### `createVertexBuffer()`

Creates both the `ID3D11Buffer` and the `ID3D11InputLayout` (from the stored shader
blob). The input layout is created eagerly so that `bindVertexBuffer` is a simple
`IASetInputLayout` call.

### `beginFrame()` / `endFrame()`

```cpp
void D3D11Renderer::beginFrame(const float clear_color[4]) {
    // Reset dirty tracking
    m_cur_vs = {}; m_cur_ps = {}; m_cur_vb = {}; m_cur_ib = {};
    m_cur_topo = static_cast<PrimitiveTopology>(-1);

    m_context->ClearRenderTargetView(m_rtv.Get(), clear_color);
    m_context->ClearDepthStencilView(m_dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    m_context->RSSetViewports(1, &m_viewport);
}

void D3D11Renderer::endFrame() {
    m_swap_chain->Present(1, 0);  // VSync on
}
```

### `bindShader()` / `bindVertexBuffer()` — dirty tracking

Each bind method checks whether the requested resource is already bound and skips
the D3D11 call if so:

```cpp
void D3D11Renderer::bindShader(GfxShader vs, GfxShader ps) {
    if (m_cur_vs != vs) {
        m_cur_vs = vs;
        auto& entry = m_shaders[vs.idx];
        m_context->VSSetShader(entry.vs.Get(), nullptr, 0);
    }
    if (m_cur_ps != ps) {
        m_cur_ps = ps;
        auto& entry = m_shaders[ps.idx];
        m_context->PSSetShader(entry.ps.Get(), nullptr, 0);
    }
}

void D3D11Renderer::bindVertexBuffer(GfxBuffer vb) {
    if (m_cur_vb == vb) return;
    m_cur_vb = vb;
    auto& entry = m_buffers[vb.idx];
    uint32_t stride = entry.stride;
    uint32_t offset = 0;
    m_context->IASetVertexBuffers(0, 1, entry.buffer.GetAddressOf(),
                                   &stride, &offset);
    m_context->IASetInputLayout(entry.input_layout.Get());
}
```

### `draw()`

```cpp
void D3D11Renderer::draw(int vertex_count, int start_vertex) {
    // Ensure topology is set at least once
    if (m_cur_topo == static_cast<PrimitiveTopology>(-1))
        setTopology(PrimitiveTopology::TriangleList);
    m_context->Draw(vertex_count, start_vertex);
}
```

### Private utility

```cpp
// Shared by createVertexShader and createPixelShader.
// Returns nullptr on failure (error logged to debug output).
static Microsoft::WRL::ComPtr<ID3DBlob>
compile_shader(std::string_view source, std::string_view entry,
               const char* target);
```

---

## Layer 4 — CMake integration

```cmake
# ---- Renderer backend selection ----
set(GFX_BACKEND "D3D11" CACHE STRING "Graphics backend: D3D11, OpenGL, Vulkan")

# ---- gfx-renderer library ----
add_library(gfx-renderer STATIC
    src/gfx_renderer.h
    src/gfx_common.h
)

if(GFX_BACKEND STREQUAL "D3D11")
    target_sources(gfx-renderer PRIVATE
        src/renderer_d3d11.h
        src/renderer_d3d11.cpp
    )
    target_link_libraries(gfx-renderer PUBLIC
        d3d11 dxgi d3dcompiler
    )
elseif(GFX_BACKEND STREQUAL "OpenGL")
    target_sources(gfx-renderer PRIVATE
        src/renderer_gl.h
        src/renderer_gl.cpp
    )
    target_link_libraries(gfx-renderer PUBLIC
        glad OpenGL::GL
    )
else()
    message(FATAL_ERROR "Unknown GFX_BACKEND: ${GFX_BACKEND}")
endif()

target_include_directories(gfx-renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---- Sample executables ----
add_executable(hello-triangle src/hello_triangle.cpp src/glfw_window.cpp)
target_link_libraries(hello-triangle PRIVATE gfx-renderer glfw)
```

Note: sample executables link only `gfx-renderer` and `glfw`. They do **not** link
`d3d11`, `dxgi`, `d3dcompiler`, `glad`, or `OpenGL::GL` directly. The backend library
pulls in the correct dependencies via its `PUBLIC` link interface.

---

## Refactored `hello_triangle.cpp` (target state)

```cpp
#include "gfx_renderer.h"
#include "glfw_window.h"

constexpr float vertices[] = {
     0.5f, -0.5f, 0.0f,
    -0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f,
};

const char* vs_source = R"(
    float4 main(float3 pos : POSITION) : SV_POSITION {
        return float4(pos, 1.0f);
    }
)";

const char* ps_source = R"(
    float4 main() : SV_TARGET {
        return float4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

int main() {
    glfwInit();
    glfw::Window window(640, 480, "D3D11 Triangle",
                         {.client_api = GLFW_NO_API});
    HWND hwnd = glfwGetWin32Window(window.Get());

    window.on_key = [&](int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            window.SetShouldClose(true);
    };

    // ---- setup (4 lines of boilerplate) ----
    D3D11Renderer renderer;
    renderer.init({hwnd, 640, 480});

    GfxShader vs = renderer.createVertexShader(vs_source, "main");
    GfxShader ps = renderer.createPixelShader(ps_source, "main");
    GfxBuffer vb = renderer.createVertexBuffer(vertices, sizeof(vertices),
        VertexLayout{}.add(Attrib::Position, AttribFormat::Float3));

    // ---- render loop ----
    while (!window.ShouldClose()) {
        glfwPollEvents();

        const float gray[] = {0.2f, 0.3f, 0.3f, 1.0f};
        renderer.beginFrame(gray);
        renderer.bindShader(vs, ps);
        renderer.bindVertexBuffer(vb);
        renderer.draw(3);
        renderer.endFrame();
    }

    renderer.shutdown();
    glfwTerminate();
    return 0;
}
```

---

## File layout after refactoring

```
src/
  gfx_common.h             ← shared types, handles, VertexLayout
  gfx_renderer.h           ← IGfxRenderer abstract interface
  renderer_d3d11.h         ← D3D11Renderer class declaration
  renderer_d3d11.cpp       ← D3D11Renderer implementation
  glfw_window.h            ← (existing) GLFW RAII wrapper
  glfw_window.cpp          ← (existing)
  hello_triangle.cpp       ← refactored sample
  hello_clear.cpp          ← future sample
  hello_textured_quad.cpp  ← future sample
```

---

## Adding a new backend (e.g. OpenGL)

1. Create `renderer_gl.h` / `renderer_gl.cpp` implementing `IGfxRenderer`.
2. Internally, `createVertexShader` calls `glCreateShader(GL_VERTEX_SHADER)` →
   `glShaderSource` → `glCompileShader`. Returns a `GfxShader` whose `idx` indexes
   into `std::vector<GLEntry>` storing the `GLuint` program name.
3. `beginFrame` calls `glClear`; `endFrame` calls `glfwSwapBuffers`.
4. Switch CMake: `-DGFX_BACKEND=OpenGL`.
5. `hello_triangle.cpp` compiles and runs without changes.

The interface deliberately hides the biggest backend difference — HLSL vs GLSL —
behind `createVertexShader`/`createPixelShader`. Each backend compiles its own
shading language. For a learning project this is acceptable; the sample author
writes the correct source for the active backend.

---

## What is intentionally NOT abstracted (yet)

| Item | Reason |
|---|---|
| Constant buffers / uniforms | No sample needs them yet. Add `createConstantBuffer` / `setUniform` when required. |
| Blend state, depth-stencil state | Defaults (alpha-blend off, depth-test on, back-face cull) are reasonable for early samples. Add `setBlendState` / `setDepthStencil` later. |
| Textures & samplers | Add when the first textured sample is written. |
| Shader binary pre-compilation | Runtime `D3DCompile` / `glCompileShader` is fine for a learning project. Offline compilation can be added later. |
| Multi-threaded command recording | Overkill for single-threaded samples. |
| Resource state caching (LRU) | Only a handful of resources exist at a time; no need for eviction. |
| Resource lifetime / reference counting | `destroyXxx` methods are provided but can be called at shutdown. Simple samples don't need fine-grained lifetime management. |

Each of these can be added to `IGfxRenderer` as new `virtual` methods when a concrete
sample requires them.

---

## Implementation order

1. Write `gfx_common.h` — the `VertexLayout`, `GfxHandle`, and enums.
2. Write `gfx_renderer.h` — the `IGfxRenderer` interface and `GfxConfig`.
3. Write `renderer_d3d11.h` — class declaration with private members.
4. Write `renderer_d3d11.cpp`:
   a. `init()` — move device/swap-chain/RTV/DSV/viewport creation out of `hello_triangle.cpp`.
   b. `compile_shader()` — move the existing `CompileShader` helper.
   c. `createVertexShader()` / `createPixelShader()` — wrap `CreateVertexShader` / `CreatePixelShader`.
   d. `createVertexBuffer()` — create buffer + input layout.
   e. `beginFrame()` / `endFrame()` — clear, set render targets, present.
   f. `bindShader()` / `bindVertexBuffer()` / `setTopology()` / `draw()` — thin wrappers with dirty tracking.
5. Update `CMakeLists.txt` to build `gfx-renderer` and link samples against it.
6. Rewrite `hello_triangle.cpp` against the new abstraction.
7. Build and verify the triangle still renders correctly.
