# 🖊

- [🖊](#)
  - [Viewport 是公有概念吗？](#viewport-是公有概念吗)
  - [Viewport 和 GLFW 的 Framebuffer 是什么关系？](#viewport-和-glfw-的-framebuffer-是什么关系)
  - ["framebuffer"](#framebuffer)
  - [为什么 GL 用 glfwSwapBuffers 而 D3D11 用 SwapChain::Present？](#为什么-gl-用-glfwswapbuffers-而-d3d11-用-swapchainpresent)
    - [GLFW 封装的 "交换链" 就是 IDXGISwapChain 吗？](#glfw-封装的-交换链-就是-idxgiswapchain-吗)
  - [D3D11 实现的 Initialize 为什么代码量明显多于 OpenGL？](#d3d11-实现的-initialize-为什么代码量明显多于-opengl)
  - [D3D11 和 OpenGL 的 Shader Stage](#d3d11-和-opengl-的-shader-stage)
  - [为什么选了 CreateProgram 方案？](#为什么选了-createprogram-方案)
  - ["顶点数据怎么解释"](#顶点数据怎么解释)
  - [属性槽是 GL 内部自动创建的吗？](#属性槽是-gl-内部自动创建的吗)

## Viewport 是公有概念吗？

是的。

Viewport 是 GPU 固定管线中的一个独立阶段——将 NDC（归一化设备坐标，范围 [-1, 1]）映射到渲染目标的像素坐标。所有图形 API 都有它：

- OpenGL glViewport(x, y, w, h)
- D3D11  RSSetViewports(1, &vp)
- D3D12  RSSetViewports(1, &vp) 或 PSO 内
- Vulkan VkViewport 在 pipeline 中
- Metal  MTLViewport

它是每个 draw call 之前必须设置的。

## Viewport 和 GLFW 的 Framebuffer 是什么关系？

两个**不同**的东西，但是尺寸通常一致。

```
窗口尺寸 (逻辑像素)
    ↓ 高 DPI 下可能不等
Framebuffer 尺寸 (物理像素)    ← GLFW 的 framebuffer_size_callback 告诉你这个
    ↓ 作为参数传给
Viewport                     ← GPU 用这个把 NDC → 像素坐标
```

- GLFW framebuffer = 窗口可绘制区域的物理像素尺寸。Retina 屏上一个 800×600 的窗口，framebuffer 可能是 1600×1200。
- Viewport = 告诉 GPU "把 NDC 空间映射到这个矩形区域内"。通常设成和 framebuffer 一样的尺寸，但也可以更小（分屏渲染、小地图等）。

GLFW 只是在回调里告诉你 "可绘制区域变大了"，你拿这个值去设 viewport。换 Win32 CreateWindow 也一样——窗口大小变了，你会收到 WM_SIZE，然后更新 viewport。

## "framebuffer"

1. 默认帧缓冲（Swap Chain / Back Buffer）

"往屏幕上画的那个最终目标"。每个 API 都有，叫法不同：

- OpenGL 叫 Default framebuffer (FBO 0)， 绑定了 OpenGL context 就会自动存在
- D3D11  叫 Swap chain back buffer     ， IDXGISwapChain::GetBuffer(0, ...)
- D3D12  叫 Swap chain back buffer     ， 同 D3D11，但要手动管理
- Vulkan 叫 Swap chain image           ， vkGetSwapchainImagesKHR

2. 离屏帧缓冲（FBO / Render Target）

"渲染到纹理"的那种。同样所有 API 都有：

- OpenGL  Framebuffer Object (FBO) + attachment
- D3D11   RTV + DSV 绑定到纹理
- D3D12   Render target descriptor
- Vulkan  VkFramebuffer + render pass

3. GLFW 中的 FrameBuffer

> 三个概念共享 "framebuffer" 这个词，但分别在**三个不同层级**

```
层级 1: GLFW Framebuffer
"窗口可以画多少像素" — 一个整数值对 (w, h)
用于：告诉 GPU viewport 该设多大
来源：OS 窗口系统通知
                │ 作为尺寸参数传入
                ▼
层级 2: 默认帧缓冲 (Swap Chain Back Buffer)
GPU 里的一块内存，直接对应屏幕上的像素
来源：CreateWindow → CreateSwapChain → GetBuffer
FBO 编号：GL=0, D3D11=OMSetRenderTargets(rtv)
                │ 如果不想画到屏幕，而是画到纹理
                ▼
层级 3: 离屏帧缓冲 (FBO / Render Target)
GPU 里的一块内存，存纹理数据，不直接显示
用途：shadow map, G-buffer, 后处理, 反射
FBO 编号：GL=glGenFramebuffers(), D3D11=自建 RTV
```

层级 1：GLFW Framebuffer — 这不是 GPU 概念

framebuffer_size_callback(GLFWwindow* w, int width, int height)

GLFW 的 "framebuffer" 根本不是 GPU 资源。它只是一个回调通知："窗口的可绘制区域现在是 width × height 像素了"。你收到这个数字后要做的是：调 glViewport(0, 0, width, height) 告诉 GPU 往哪画。

换 Win32 不用 GLFW，等价物是 WM_SIZE 消息里 LOWORD(lParam) / HIWORD(lParam)。

GLFW 的命名是个不幸的历史遗留——它应该叫 `drawable_size_callback` 而不是 `framebuffer_size_callback`。

层级 2：默认帧缓冲 — GPU 硬件概念

这是 GPU 里对应屏幕的一块内存。操作系统显示子系统（Windows 上是 DWM 合成器）直接读这块内存来更新屏幕。

| | OpenGL | D3D11 |
|---|---|---|
│ 谁创建 | EGL/WGL/GLX 绑定 context 时 | D3D11CreateDeviceAndSwapChain    |
| 怎么引用 | glBindFramebuffer(GL_FRAMEBUFFER, 0) | OMSetRenderTargets(1, &rtv, dsv) |
| 包含什么 | 颜色缓冲 + 深度缓冲 + 模板缓冲 | 同 |
| 可绘制区域 | 由 GLFW/Win32 窗口尺寸决定 | 同 |
| 能删吗 | 不能，生命周期跟 context/swapchain 绑定 | 不能 |

它的尺寸 = 层级 1 的 GLFW framebuffer 尺寸。但这只是数值相等，不是同一个东西。

层级 3：离屏帧缓冲 — 渲染到纹理

跟层级 2 本质上是同一类 GPU 对象（都是 FBO / Render Target），区别在于：不显示在屏幕上，而是作为后续渲染的输入。

创建离屏 FBO 后，把几何体画到它上面，然后把它的颜色附件当纹理绑定到另一个 shader ——这就是 shadow map、延迟着色、后处理的基础。

## 为什么 GL 用 glfwSwapBuffers 而 D3D11 用 SwapChain::Present？

**因为 OpenGL 故意不暴露 Swap Chain 这个概念。**

OpenGL 的设计哲学

OpenGL 规范里没有 "SwapChain" 这个对象

交换行为由窗口系统 (WGL/GLX/EGL) 处理，对开发者透明

glfwSwapBuffers() → SwapBuffers(hdc) → WGL交换链
   App 代码              GLFW            驱动/OS

D3D11 的设计哲学

DXGI 把 Swap Chain 作为一等公民暴露给开发者

SwapChain->Present(1, 0)   →      IDXGISwapChain
   直接调用 COM 接口              (谁创建、谁持有、谁调用)


### GLFW 封装的 "交换链" 就是 IDXGISwapChain 吗？

不是。至少在使用 OpenGL 时不是。

GLFW 在 Windows 上的 glfwSwapBuffers 实际调用路径：

```
  glfwSwapBuffers(window)
    └─ _glfwPlatformSwapBuffers(window)
         └─ SwapBuffers(hdc)          // GDI 函数，不是 DXGI
              └─ 驱动内部可能用 DXGI，可能不用
```

SwapBuffers(hdc) 是 GDI 函数（自 Windows 3.1 就有了），它操作的是 WGL 为你创建的隐式交换链——你永远拿不到指向它的 IDXGISwapChain* 指针。

当我们在 D3D11 后端里写 swap_chain_->Present(1, 0) 时，这个 swap_chain_ 是我们自己在 CreateDeviceAndSwapChain 里创建的——代码路径完全不同：

```
  GL 路径 (你的代码根本碰不到 SwapChain):
    glfwCreateWindow(..., GLFW_OPENGL_API)
    → GLFW 创建 WGL context
    → WGL 在驱动内部创建了隐式 swap chain
    → glfwSwapBuffers → GDI SwapBuffers → 驱动交换
```

```
  D3D11 路径 (SwapChain 是你的私有成员):
    glfwCreateWindow(..., GLFW_NO_API)   // GLFW 只管窗口，不管渲染
    → glfwGetWin32Window(window)         // 拿 HWND
    → D3D11CreateDeviceAndSwapChain(..., hwnd, &swapChain, ...)
    → swapChain->Present(1, 0)           // 你自己调
```

## D3D11 实现的 Initialize 为什么代码量明显多于 OpenGL？

**OpenGL 有一套隐式默认值体系，D3D11 没有。**

D3D11 多出来的代码在做什么：

1. 交换链管理（OpenGL 没有这个概念）

```c++
// D3D11: 你必须显式创建 swap chain
DXGI_SWAP_CHAIN_DESC sd = {};
sd.BufferDesc.Width  = width_;
sd.BufferDesc.Height = height_;
sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
sd.BufferCount       = 2;
sd.OutputWindow      = hwnd_;
// ... 
D3D11CreateDeviceAndSwapChain(..., &sd, &swapChain_, &device_, ...);
```

OpenGL 这边，swap chain 是在 glfwCreateWindow + glfwMakeContextCurrent 时由 WGL 在驱动内部静默创建的。你永远看不到它，也拿不到它的指针。

2. 渲染目标（OpenGL 有 FBO 0，D3D11 什么都没有）

```c++
// D3D11: 手动从 swap chain 拿 back buffer，手动创建 RTV
swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);

// D3D11: 深度缓冲也要手动创建
D3D11_TEXTURE2D_DESC dsd = {};
dsd.Width = width_; dsd.Height = height_;
dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
device_->CreateTexture2D(&dsd, nullptr, &depthTexture_);
device_->CreateDepthStencilView(depthTexture_.Get(), nullptr, &dsv_);

// D3D11: 手动绑定
context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
```

OpenGL 这边：glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) 工作在 FBO 0（默认帧缓冲）上。颜色缓冲、深度缓冲都已经由 context 创建好了，你不需要手动创建纹理、RTV、DSV。

3. 管线状态（OpenGL 有默认值，D3D11 需要显式创建）

```c++
// D3D11: 必须创建状态对象
device_->CreateRasterizerState(&rd, &rasterizerState_);
device_->CreateDepthStencilState(&dsd, &depthStencilState_);
device_->CreateBlendState(&bd, &blendState_);
context_->RSSetState(rasterizerState_.Get());
context_->OMSetDepthStencilState(depthStencilState_.Get(), 0);
context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);
```

OpenGL 这边：glEnable(GL_DEPTH_TEST) 就够。光栅化、混合的默认值就是合理的，不调也行。

## D3D11 和 OpenGL 的 Shader Stage

```
ShaderStage        GL                  D3D11
─────────────────────────────────────────────────
kVertex        →  GL_VERTEX_SHADER    → vs_5_0
kFragment      →  GL_FRAGMENT_SHADER  → ps_5_0
kGeometry      →  GL_GEOMETRY_SHADER  → gs_5_0
kCompute       →  GL_COMPUTE_SHADER   → cs_5_0
kTessControl   →  GL_TESS_CONTROL_    → hs_5_0  (Hull Shader)
kTessEval      →  GL_TESS_EVALUATION_ → ds_5_0  (Domain Shader)
```

## 为什么选了 CreateProgram 方案？

CreateShader = 编译单个阶段源码，各 API 都有对应。

CreateProgram = 把多个阶段组装成一个可用的管线。GL 的链接是一种组装方式，D3D11 的"存到一起等待绑定时用"也是一种组装方式，D3D12/Vulkan 的 PSO 创建也是。接口暴露这个步骤，是让组装产物在整个系统中有一个统一的标识（Handle），上层代码不关心后端怎么实现组装。

```
OpenGL:                           D3D11:
───────                           ──────
glCreateShader(VS)                D3DCompile(HLSL, "vs_5_0") → ID3DBlob
glCreateShader(FS)                D3DCompile(HLSL, "ps_5_0") → ID3DBlob
        ↓ 链接                                ↓ 无链接
glCreateProgram()                （VS+PS 各自独立，不存在"合并"步骤）
glAttachShader + glLinkProgram
        ↓                                    ↓
glUseProgram(prog)                VSSetShader(vs) + PSSetShader(ps) ← 分开绑
```

1. 存储中间产物。 D3D11 的 CreateInputLayout 需要 VS 字节码。如果 vs/fs 各自独立，你得额外暴露一个 GetShaderBytecode() 来拿 VS blob。把 vs+fs 打包成 Program 之后，VS blob 随 Program 一起存着，CreateInputLayout 只须传 program handle。

2. 提供一个自然的位置管理 per-pipeline 资源。 D3D11 的 Constant Buffer、PSO 缓存、shader resource binding layout——这些东西的粒度是"整套管线"而非单个 shader stage。Program handle 成了挂这些资源的锚点：

```c++
struct ProgramData {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    ComPtr<ID3DBlob>           vs_blob;    // CreateInputLayout 要用
    ComPtr<ID3D11Buffer>       cb;         // Constant buffer 跟 program 走
    uint8_t  cb_shadow[1024];              // Uniform 缓存
    // ...
};
```

3. 给以后的 D3D12/Vulkan 后端留接口。 这两个 API 的 PSO (Pipeline State Object) 就是把所有 shader stage 绑在一起，而且创建 PSO 时必须一次性传入所有 stage：

```c++
// D3D12: PSO 创建时就要全套 shader
D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
pso.VS = vs_bytecode;
pso.PS = ps_bytecode;
// ...
CreateGraphicsPipelineState(&pso, ...);  // ← 这就是 D3D12 的"CreateProgram"
```

CreateProgram(vs, fs) 在 GL 后端做 glLinkProgram，在 D3D11 后端做 CreateVS + CreatePS + 存 VS blob，在 D3D12/Vulkan 后端对应 PSO 创建——同一个接口，各后端语义不同但结果对齐。

## "顶点数据怎么解释"

```
GL:        VAO = 独立对象              ← glGenVertexArrays
D3D11:     InputLayout = 独立对象      ← CreateInputLayout
D3D12:     输入布局 = PSO 的一部分      ← D3D12_INPUT_LAYOUT_DESC 嵌在 PSO 里
Vulkan:    输入布局 = Pipeline 的一部分 ← VkPipelineVertexInputStateCreateInfo 嵌在 pipeline 里
Metal:     顶点描述 = Pipeline 的一部分 ← MTLVertexDescriptor 嵌在 pipeline state 里
```

location 和 semantic 是"同一个位置"在两种语言里的不同叫法

location = 0   ←→   semantic = "POSITION"     ← 指同一个着色器输入口
location = 1   ←→   semantic = "COLOR"        ← 同上
location = 2   ←→   semantic = "TEXCOORD"     ← 同上

GL 通过数字 location=N 区分输入口——"第 0 号口叫 aPos"。

D3D11 通过字符串 : SEMANTIC 区分输入口 —— "名叫 POSITION 的那个口叫 pos"。

C++ 侧的 VertexAttrib 负责把两边对接上

```c++

//       GL 用这个找口          D3D11 用这个找口         CPU 端的数据类型    内存偏移
//       ──────────            ─────────────           ────────────────    ──────
        {0,                    "POSITION",             Float3,             0},       // 对着色器输入口 #0  aka POSITION
        {1,                    "COLOR",                Float3,             12},      // 对着色器输入口 #1  aka COLOR
        {2,                    "TEXCOORD",             Float2,             24},      // 对着色器输入口 #2  aka TEXCOORD
```

## 属性槽是 GL 内部自动创建的吗？

是的。属性槽的数量和编号不由你创建——它们是 GPU 硬件固定的，GL 规范规定的。你只能开启或关闭某个编号，不能新建或删除。

硬件层面的属性槽

```
GPU 规范：
┌───────┬───────┬───────┬───────┬───────┬───────┬─────┬───────┐
│ slot0 │ slot1 │ slot2 │ slot3 │ slot4 │ slot5 │ ... │ slot15│
│ on/off│on/off │on/off │on/off │on/off │on/off │ ... │on/off │
└───────┴───────┴───────┴───────┴───────┴───────┴─────┴───────┘
        ↑
    最少 16 个（GL 保证），实际可能更多
    GL_MAX_VERTEX_ATTRIBS 查询（通常 ≥ 16）
```

代码里做的事就是激活已有的、配置空闲的：

```c++
glEnableVertexAttribArray(0);   // slot0: 从 OFF 切换到 ON
glEnableVertexAttribArray(1);   // slot1: 从 OFF 切换到 ON
glEnableVertexAttribArray(2);   // slot2: 从 OFF 切换到 ON
// slot3 ~ slot15: 保持 OFF，GPU 忽略
```

D3D11 这边完全不同

没有"自动存在的属性槽"这个概念。它通过 CreateInputLayout 创建的是一个独立的 COM 对象，里面明确写了"有几个属性、每个属性叫什么、什么格式"：

```c++
D3D11_INPUT_ELEMENT_DESC descs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, ...},
    {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, ...},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    ...},
};
// → 创建 ID3D11InputLayout 对象
//   这个对象描述了"有 3 个属性，分别叫这些名字"
```

匹配方式也不同：

GL:    location = 0 → 直接往硬件 slot 0 写 → VS 里 layout(location=0) 来拿
D3D11: SemanticName = "POSITION" → InputLayout 对象记录 → VS 里 :POSITION 来匹配

GL 的属性槽是预分配好的硬件寄存器——你能做的只是 enabled/disabled + 配置格式。D3D11 的 InputLayout 是你手动创建的软件对象——包含了完整的属性描述。CreateInputLayout 要同时兼容这两种模型，对外表现为同一个接口。

```
    CreateBuffer(type=kVertex, data, size)
      → glGenBuffers + glBufferData
      → returns Handle h₁
      → stored in buffers_[h₁] = {id=17, type=kVertex}

    CreateBuffer(type=kIndex, data, size)
      → glGenBuffers + glBufferData (GL_ELEMENT_ARRAY_BUFFER)
      → returns Handle h₂
      → stored in buffers_[h₂] = {id=42, type=kIndex}

    CreateInputLayout(program, stride=32, attribs[], count=3)
      → glGenVertexArrays + glVertexAttribFormat × N + glVertexAttribBinding × N
      → returns Handle h₃
      → stored in input_layouts_[h₃] = {vao=7}
      → VAO slot routing: all N attribs → VBO binding point 0

    BindInputLayout(h₃)
      → glBindVertexArray(7)                                   // activate VAO

    BindVertexBuffer(slot=0, h₁, stride=32, offset=0)
      → glBindVertexBuffer(0, 17, 0, 32)                       // VBO → point[0]

    BindIndexBuffer(h₂, kUint32)
      → glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 42)              // EBO → VAO

    DrawIndexed(count=6, firstIndex=0)
      → glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0)   // GPU renders
```
```c++
// ============================================================================
// GPU-side resource: a block of video memory holding raw byte data.
// No type, no layout — just size + bytes.
// ============================================================================
typedef struct GpuBuffer {
    uint32_t    id;           // driver-internal handle (GLuint)
    size_t      size_bytes;   // total allocation size
    void*       gpu_address;  // GPU virtual address (opaque to us)
} GpuBuffer;

// ============================================================================
// EBO — an index buffer.  No "layout" concept.
// It is literally just a GpuBuffer filled with uint16_t[] or uint32_t[].
// The only extra information is whether indices are 16- or 32-bit,
// which is NOT stored in the buffer itself — the caller provides it
// at draw time:  glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, ptr)
// ============================================================================
typedef GpuBuffer IndexBuffer;

// ============================================================================
// One attribute slot — describes a single field within a vertex.
// "From byte `offset` in each vertex, read `components` values of `type`."
// ============================================================================
typedef struct AttribSlot {
    uint32_t  location;        // which shader input this feeds (layout=N)
    uint32_t  components;      // 1, 2, 3, or 4
    uint32_t  type;            // GL_FLOAT, GL_INT, GL_UNSIGNED_BYTE, etc.
    bool      normalized;      // GL_TRUE or GL_FALSE
    uint32_t  offset_bytes;    // byte offset from the start of each vertex
    uint32_t  vbo_binding;     // which VBO binding point feeds this slot
    bool      enabled;         // active or ignored
} AttribSlot;

// ============================================================================
// One VBO binding point — "this VBO, starting here, stepping by this much."
// Multiple AttribSlots can point to the same VBO binding point (interleaved).
// ============================================================================
typedef struct VboBindingPoint {
    GpuBuffer* vbo;            // pointer to the actual GPU buffer (NULL if unset)
    uint32_t   stride_bytes;   // distance between consecutive vertices
    uint32_t   start_offset;   // where to start reading within the VBO
} VboBindingPoint;

// ============================================================================
// VAO — the master descriptor object.
// Describes HOW to interpret raw VBO bytes into vertex shader inputs,
// and optionally holds an EBO reference for indexed draws.
// ============================================================================
#define MAX_ATTRIB_SLOTS    16      // GL guarantees at least 16
#define MAX_VBO_BINDINGS    16      // GL guarantees at least 16

typedef struct VertexArrayObject {
    uint32_t         id;                     // driver-internal handle (GLuint)

    // Attribute descriptors: "what fields exist in each vertex"
    AttribSlot       attribs[MAX_ATTRIB_SLOTS];

    // VBO routing table:  "where does each binding point get its data"
    //   attrib[K].vbo_binding = N   →   use binding[N] to fetch bytes
    VboBindingPoint  bindings[MAX_VBO_BINDINGS];

    // Index buffer (optional — NULL for non-indexed draws)
    IndexBuffer*     element_buffer;         // stored directly in the VAO
} VertexArrayObject;


// ============================================================================
// Resource manager (RendererGl internal)
// ============================================================================
typedef struct {
    // ... (programs, textures, samplers, framebuffers omitted) ...

    // All GPU buffers (VBOs and EBOs live in the same pool)
    GpuBuffer*         buffers[MAX_HANDLES];      // indexed by Handle

    // All VAOs
    VertexArrayObject*  vaos[MAX_HANDLES];         // indexed by Handle
} GlState;


// ============================================================================
// How the pieces connect — example: interleaved pos+color+uv, 3 vertices
// ============================================================================

void example(void) {
    GlState state = {0};

    // ── Create VBO (GPU memory) ──────────────────────────────────────────
    GpuBuffer* vbo = glGenBuffers + glBufferData(
        /* data */  { pos0xyz|clr0rgb|uv0st | pos1xyz|clr1rgb|uv1st | pos2xyz|clr2rgb|uv2st },
        /* size */  3 * 32
    );
    state.buffers[handle_vb] = vbo;

    // ── Create EBO (GPU memory) ──────────────────────────────────────────
    IndexBuffer* ebo = glGenBuffers + glBufferData(
        /* data */  { 0, 1, 2 },
        /* size */  3 * sizeof(uint32_t)
    );
    state.buffers[handle_ib] = ebo;

    // ── Create VAO (empty — no buffers attached yet) ─────────────────────
    VertexArrayObject* vao = glGenVertexArrays;
    state.vaos[handle_vao] = vao;

    // ── Configure VAO attribute slots ────────────────────────────────────
    vao->attribs[0] = (AttribSlot){
        .location      = 0,
        .components    = 3,
        .type          = GL_FLOAT,
        .normalized    = false,
        .offset_bytes  = 0,
        .vbo_binding   = 0,           // "data comes from binding point 0"
        .enabled       = true,
    };
    vao->attribs[1] = (AttribSlot){
        .location      = 1,
        .components    = 3,
        .type          = GL_FLOAT,
        .offset_bytes  = 12,
        .vbo_binding   = 0,           // same VBO, interleaved
        .enabled       = true,
    };
    vao->attribs[2] = (AttribSlot){
        .location      = 2,
        .components    = 2,
        .type          = GL_FLOAT,
        .offset_bytes  = 24,
        .vbo_binding   = 0,
        .enabled       = true,
    };
    // slots 3..15 remain { .enabled = false }

    // ── Bind VBO into VAO ────────────────────────────────────────────────
    vao->bindings[0] = (VboBindingPoint){
        .vbo          = vbo,
        .stride_bytes = 32,
        .start_offset = 0,
    };

    // ── Bind EBO into VAO ────────────────────────────────────────────────
    vao->element_buffer = ebo;

    // ── Final picture ────────────────────────────────────────────────────
    //
    //   vbo { pos0|clr0|uv0 | pos1|clr1|uv1 | pos2|clr2|uv2 }
    //          ↑──────────────────────────────────────────────┐
    //   vao.bindings[0] { .vbo = vbo, .stride = 32 }          │
    //          │                                              │
    //          ├── vao.attribs[0] { offset=0,  Float×3, binding=0 }
    //          ├── vao.attribs[1] { offset=12, Float×3, binding=0 }
    //          └── vao.attribs[2] { offset=24, Float×2, binding=0 }
    //
    //   ebo { 0, 1, 2 }
    //   vao.element_buffer = ebo
    //
    //   glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0)
    //
    //   GPU fetches:
    //     for i in 0..2:                    // index from ebo
    //       addr = vbo + 0 + i * 32         // binding[0]
    //       slot0 ← *(addr+0..12)           // attrib[0]
    //       slot1 ← *(addr+12..24)          // attrib[1]
    //       slot2 ← *(addr+24..32)          // attrib[2]
    //       run vertex shader with {slot0, slot1, slot2}
}
```

D3D11 没有 VAO 这样的统一结构体。三个东西各自独立——InputLayout、VertexBuffer、IndexBuffer 是三个不相关的 COM 对象。

```c++
  // ============================================================================
  // D3D11 side: NO equivalent of VAO.  Three independent objects.
  // ============================================================================

  // ── Vertex Buffer — same concept as GL VBO ──────────────────────────────
  typedef struct D3D11_Buffer {
      ID3D11Buffer*    ptr;          // COM interface
      size_t           size_bytes;
      UINT             bind_flags;   // D3D11_BIND_VERTEX_BUFFER
  } D3D11_Buffer;

  // ── Index Buffer — same concept as GL EBO ────────────────────────────────
  // Just a D3D11_Buffer with bind_flags = D3D11_BIND_INDEX_BUFFER
  typedef D3D11_Buffer D3D11_IndexBuffer;

  // ── Input Layout — describes HOW to interpret bytes, NO reference to
  //    any specific VBO or EBO.  Pure format descriptor, always immutable.
  //    Created from VS bytecode + D3D11_INPUT_ELEMENT_DESC[].
  // ============================================================================
  typedef struct {
      LPCSTR            SemanticName;        // "POSITION", "COLOR", "TEXCOORD"
      UINT              SemanticIndex;       // 0 (unless TEXCOORD0 vs TEXCOORD1)
      DXGI_FORMAT       Format;              // DXGI_FORMAT_R32G32B32_FLOAT
      UINT              InputSlot;           // which vertex buffer slot (0..15)
      UINT              AlignedByteOffset;   // byte offset within each vertex
      D3D11_INPUT_CLASSIFICATION InputSlotClass;   // PER_VERTEX or PER_INSTANCE
      UINT              InstanceDataStepRate;       // 0 for per-vertex
  } InputElementDesc;
  //        ↑  This is NOT an object — it is just an array of descriptors
  //        ↑  that you pass to CreateInputLayout() to manufacture the object:

  typedef struct D3D11_InputLayout {
      ID3D11InputLayout* ptr;        // COM interface (immutable after creation)
  } D3D11_InputLayout;


  // ============================================================================
  // D3D11 resource manager internals
  // ============================================================================
  typedef struct {
      // ... device_, context_, swap_chain_, rtv_, dsv_ ...

      D3D11_Buffer*        buffers[MAX_HANDLES];       // VBOs and EBOs
      D3D11_InputLayout*   layouts[MAX_HANDLES];       // pure format descriptors
      // ... programs, textures, samplers, framebuffers ...
  } D3D11State;


  // ============================================================================
  // How the pieces connect — same interleaved pos+color+uv example
  // ============================================================================

  void example_d3d11(void) {
      D3D11State state = {0};

      // ── Create VBO (GPU memory) ──────────────────────────────────────────
      D3D11_BUFFER_DESC vbo_desc = {
          .ByteWidth = 3 * 32,
          .Usage     = D3D11_USAGE_DEFAULT,
          .BindFlags = D3D11_BIND_VERTEX_BUFFER,
      };
      D3D11_Buffer* vbo = device->CreateBuffer(&vbo_desc, init_data);
      state.buffers[handle_vb] = vbo;

      // ── Create EBO (GPU memory) ──────────────────────────────────────────
      D3D11_BUFFER_DESC ebo_desc = {
          .ByteWidth = 3 * sizeof(uint32_t),
          .Usage     = D3D11_USAGE_DEFAULT,
          .BindFlags = D3D11_BIND_INDEX_BUFFER,
      };
      D3D11_IndexBuffer* ebo = device->CreateBuffer(&ebo_desc, index_data);
      state.buffers[handle_ib] = ebo;

      // ── Create InputLayout (pure format — no buffer references) ──────────
      InputElementDesc descs[3] = {
          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  PER_VERTEX, 0 },
          { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, PER_VERTEX, 0 },
          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, PER_VERTEX, 0 },
          //                  InputSlot=0 means "all come from IASetVertexBuffers slot 0"
      };
      D3D11_InputLayout* layout = device->CreateInputLayout(
          descs, 3, vs_bytecode, vs_bytecode_len);
      state.layouts[handle_layout] = layout;

      // ── Per-frame binding (EVERY frame, no persistent state) ─────────────
      //     Three completely independent Set* calls.  No object links them.
      context->IASetInputLayout(layout->ptr);
      context->IASetVertexBuffers(0, 1, &vbo->ptr, &stride, &offset);
      context->IASetIndexBuffer(ebo->ptr, DXGI_FORMAT_R32_UINT, 0);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      context->DrawIndexed(3, 0, 0);


      // ── Final picture ────────────────────────────────────────────────────
      //
      //        D3D11_InputLayout                  D3D11_Buffer (VBO)
      //   ┌──────────────────────────┐     ┌──────────────────────────┐
      //   │ POSITION Float3 @0  slot0│     │ pos0|clr0|uv0|pos1|...   │
      //   │ COLOR    Float3 @12 slot0│     └───────────┬──────────────┘
      //   │ TEXCOORD Float2 @24 slot0│                 │
      //   └────────────┬─────────────┘                 │
      //                │                               │
      //                │  IASetInputLayout             │  IASetVertexBuffers(slot=0)
      //                │  (format only, no buffer)     │  (buffer only, format from layout)
      //                │                               │
      //                ▼                               ▼
      //   ┌─────────────────────────────────────────────────────────────────┐
      //   │                    D3D11 Device Context                         │
      //   │  Stores 16 slots of { ID3D11Buffer*, stride, offset }           │
      //   │  + one active ID3D11InputLayout*                                │
      //   │                                                                 │
      //   │  DrawIndexed() → GPU assembles:  layout.slot_K  +  VBO[slot_K]  │
      //   └─────────────────────────────────────────────────────────────────┘
      //
      //                             D3D11_IndexBuffer (EBO)
      //                        ┌──────────────────────────┐
      //                        │ { 0, 1, 2 }              │
      //                        └───────────┬──────────────┘
      //                                    │
      //                                    │  IASetIndexBuffer
      //                                    ▼
      //                        DrawIndexed → reads indices from EBO, fetches
      //                        vertices from VBOs according to InputLayout
  }


  // ============================================================================
  // Side-by-side: GL VAO vs D3D11's three independent objects
  // ============================================================================
  //
  //                    GL                              D3D11
  //                    ──                              ─────
  //
  //   Object model    VAO bundles:                 Three independent objects:
  //                   - attrib descriptors         - ID3D11InputLayout (format)
  //                   - VBO pointers               - ID3D11Buffer (VBO)
  //                   - EBO pointer                - ID3D11Buffer (EBO)
  //                   All in ONE GPU object.       No object links them.
  //
  //   Binding         glBindVertexArray(vao)       IASetInputLayout(layout)
  //                   → restores ALL state          IASetVertexBuffers(0,1,&vb,...)
  //                   in ONE call.                  IASetIndexBuffer(eb,...)
  //                                                 THREE separate calls.
  //
  //   Persistence     VAO state survives             Device context state survives
  //                   between frames.                 between frames, but best
  //                   Bind once → draw many.          practice is to re-bind
  //                                                   every frame.
  //
  //   Attrib setup    glVertexAttribFormat           D3D11_INPUT_ELEMENT_DESC[]
  //                   glVertexAttribBinding          → baked into InputLayout
  //                   → baked into VAO              (immutable after creation)
  //
  //   VBO binding     glBindVertexBuffer            IASetVertexBuffers(slot, ...)
  //                   → writes into VAO's            → writes into device context's
  //                      binding-point table            slot table
  //
  //   EBO binding     glBindBuffer(ELEMENT_ARRAY)    IASetIndexBuffer(eb, fmt)
  //                   → writes EBO into VAO          → writes EBO into context
```