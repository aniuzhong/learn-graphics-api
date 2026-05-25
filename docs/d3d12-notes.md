# D3D12 Notes

1. 开启调试层 (Debug Layer)

在 D3D12 中，强烈建议在创建任何设备之前开启调试层。D3D12 极其严格，任何小错误都会直接导致崩溃，调试层会通过输出窗口（Output）给你非常详细的错误提示。

与 D3D11 的区别： D3D11 是在创建设备时传入 D3D11_CREATE_DEVICE_DEBUG 标志；而 D3D12 需要先显式获取调试接口并开启。

2. 创建 DXGI 工厂（DXGI Factory）

DXGI (DirectX Graphics Infrastructure) 独立于 D3D 版本，负责管理枚举显卡、交换链等。我们创建它是为了后面挑选显卡。

与 D3D11 的区别： 概念完全一致，只是 D3D12 通常配合更新的 IDXGIFactory4 或更高版本使用。