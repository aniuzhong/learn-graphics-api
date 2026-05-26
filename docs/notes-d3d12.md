# 📝

## 

1. 开启调试层 (Debug Layer)

在 D3D12 中，强烈建议在创建任何设备之前开启调试层。D3D12 极其严格，任何小错误都会直接导致崩溃，调试层会通过输出窗口（Output）给你非常详细的错误提示。

> 与 D3D11 的区别： D3D11 是在创建设备时传入 `D3D11_CREATE_DEVICE_DEBUG` 标志；而 D3D12 需要先显式获取调试接口并开启。

2. 创建 DXGI 工厂 (DXGI Factory)

DXGI（DirectX Graphics Infrastructure）独立于 D3D 版本，负责管理枚举显卡、交换链等。我们创建它是为了后面挑选显卡。

> 与 D3D11 的区别： 概念完全一致，只是 D3D12 通常配合更新的 IDXGIFactory4 或更高版本使用。

3. 创建逻辑设备 (Device)

有了工厂，我们就可以创建设备了。在 D3D12 中，设备主要用于创建资源（如缓冲、纹理、视图、管线状态等）。

> 与 D3D11 的区别： * D3D11 一个 D3D11CreateDevice 把设备和渲染上下文 (Context) 一起建好了。

D3D12 的 ID3D12Device 不再负责提交渲染命令（上下文的概念被拆分成了命令列表和命令队列）。它纯粹是一个"资源工厂"。

```c++
ComPtr<ID3D12Device> device;
    // 参数1: 显卡适配器 (nullptr 代表默认主显卡)
    // 参数2: 最低特性级别（D3D12 也可以在支持 11 的硬件上跑）
    // 参数3: 接收设备的接口 ID 和指针
    hr = D3D12CreateDevice(
        nullptr, 
        D3D_FEATURE_LEVEL_11_0, 
        IID_PPV_ARGS(&device)
    );
    
    if (FAILED(hr)) {
        // 如果失败，可以尝试创建 WARP 设备（软件模拟器），这里暂不展开
        return;
    }
}
```

## 创建命令队列 (Command Queue)

在 D3D11 中，调用 context->Draw()，驱动程序会默默地把这个命令翻译成显卡懂的指令并立刻发给显卡。

而在 D3D12 中，这个过程被显式地拆分成了两个部分：

- 命令记录：把渲染指令录制到一个"录像带"（Command List）里。
- 命令执行：把录像带扔进一个"播放机"（Command Queue）里让显卡去执行。

GPU 是一个异步的硬件，命令队列就像一个 FIFO（先进先出）的线程队列。CPU 把一堆渲染任务塞进队列，然后转头去做别的事，GPU 自己在后台慢慢消费这个队列。

> D3D11：ID3D11DeviceContext 内部自带了一个隐式的队列。

## 创建交换链 (Swap Chain)

概念完全一致，都是为了实现“双缓冲”或“三缓冲”（前端缓冲区负责显示，后端缓冲区负责渲染，画好后翻转）。

但区别巨大

- 在 D3D11 中，你把 D3D11 Device 传给交换链。
- 在 D3D12 中，你需要把 Command Queue (命令队列) 传给交换链。

因为 D3D12 的 Device 只是个资源工厂，它根本不知道怎么执行渲染。只有 Command Queue 才是真正负责把画面"泵"送给显卡的通道。交换链需要监听这个队列，知道什么时候一帧画面真正画完了。

## 描述符堆（Descriptor Heap）与 RTV

交换链成功创建后，它在底层为你隐式创建了 两张纹理（Texture2D 资源），作为交替渲染的画布。

但在 D3D12 中，流水线（Pipeline）不能直接绑定原始资源。你想把某张纹理当作"渲染目标（画布）"，就必须为它创建一个渲染目标视图（RenderTargetView, 简称 RTV）。

在 D3D11 中，显式声明 ID3D11RenderTargetView* 指针。

而在 D3D12 中，所有的"视图（View）"都被统称为描述符（Descriptor）。描述符是 GPU 能够理解的、指向资源的轻量级结构体。

因为 D3D12 追求极致性能，它不允许你零散地创建描述符，而是要求你必须在显存中开辟一块连续的数组，专门用来存放这些描述符。这个数组就叫描述符堆 (Descriptor Heap)。

## 创建命令分配器 (Command Allocator) 与命令列表 (Command List)

```c++
// 1. 创建命令分配器（存储卡）
// 类型必须是 DIRECT，与我们之前的命令队列（Command Queue）相匹配
hr = g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_command_allocator));
if (FAILED(hr)) {
    std::cerr << "Failed to create Command Allocator: " << std::hex << hr << std::endl;
    return;
}
std::cout << "Command Allocator created successfully." << std::endl;

// 2. 创建命令列表（录像机）
// 参数1: 节点掩码（NodeMask），单显卡设为 0
// 参数2: 命令列表的类型，同样是 DIRECT
// 参数3: 绑定的分配器（录像机必须插上存储卡才能工作）
// 参数4: 初始的流水线状态对象（PSO），目前我们还没建，先传 nullptr
hr = g_device->CreateCommandList(
    0, 
    D3D12_COMMAND_LIST_TYPE_DIRECT, 
    g_command_allocator.Get(), 
    nullptr, 
    IID_PPV_ARGS(&g_command_list)
);
if (FAILED(hr)) {
    std::cerr << "Failed to create Command List: " << std::hex << hr << std::endl;
    return;
}
std::cout << "Command List created successfully." << std::endl;

// 3. 关键细节：刚创建出来的命令列表默认是“录制中（Open）”状态。
// 在目前的初始化阶段我们不需要录制任何东西，所以立刻把它关闭（Close）。
g_command_list->Close();
```

## 创建同步围栏 (Fence)
