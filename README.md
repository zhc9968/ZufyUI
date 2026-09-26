# ZufyUI

> **项目已更名：原名 `ZUI` → 现名 `ZufyUI`**（避免与其它同名项目冲突）。旧的仓库地址会由平台自动跳转到新地址（见下方链接）。

> 基于 Direct2D 的 C++ 原生 Win32 自绘 UI 框架，含布局、控件、信号槽与高 DPI 适配。

> 语言：**简体中文** · [English](README.en.md)

ZufyUI 是一个**纯头文件**的 Windows 桌面 UI 框架，直接建立在 Direct2D / DirectWrite / DWM 之上。它不依赖 Qt、MFC 或任何第三方库，把控件、布局、动画、字体、数据视图和信号槽都装进几个 `.h` 里，适合想要轻量、可控、现代观感的原生 C++ 应用。

- 仓库：GitHub <https://github.com/zhc9968/ZufyUI> · Gitee <https://gitee.com/zhc9968/ZufyUI>
- 许可证：MIT
- 在线文档：<https://zhc9968.github.io/ZufyUI/>
- API 参考：<https://zhc9968.github.io/ZufyUI/docs/API.html>

## 特性

- **Direct2D 自绘**：硬件加速渲染，亚克力（Acrylic）背景、圆角、阴影，观感现代。
- **完整布局系统**：`ColumnBox` / `RowBox` / `GridLayout`，支持间距、拉伸权重、填充、对齐与跨行跨列。
- **信号槽**：内置 `ZSignal` / `Connection`，支持 `Connect` 自动管理生命周期，以及当前线程 / 新线程 / UI 线程三种分发策略。
- **丰富控件**：标签、按钮、文本框、下拉框、开关、滚动容器、进度条、滑块，以及列表 / 表格 / 树三种数据视图。
- **完整动画与转场**：悬停、展开、指示条、页面切换（`PageHost`）都有内置动画，滚动支持平滑滚动。
- **高 DPI 适配**：自动感知 DPI，`Snap()` 把绘制吸附到物理像素，避免模糊。
- **IME 兼容**：文本框支持中文输入法组合输入与候选框定位。
- **离屏缓存**：元素可自动缓存绘制结果，减少重复绘制开销。
- **系统集成**：托盘图标（悬停/点击/右键/徽章/气泡通知）、任务栏（进度 / 覆盖徽章 / 缩略图工具栏）、跳转列表，以及**应用身份（AUMID）自注册**（统一 toast / 分组归属；需显式授权宏）。

## 环境要求

| 项 | 要求 |
| --- | --- |
| 操作系统 | Windows 10 / 11 |
| 编译器 | Visual Studio（**任意工具集均可**，需 **C++20** 及以上） |
| Windows SDK | 10.0 及以上 |
| 源文件编码 | **UTF-8 带 BOM**（不然 MSVC 会按本地代码页解析中文注释导致编译报错） |
| 运行库 | 系统自带 `d2d1` / `dwrite` / `dwmapi` / `imm32`（无需额外安装） |
| 第三方依赖 | 无 |

## 构建

1. 用 Visual Studio 打开解决方案 `ZufyUI.slnx`；
2. 选择配置 **Release | x64**；
3. 生成解决方案（Build Solution）；
4. 产物位于 `x64\Release\ZufyUI.exe`，直接运行即可。

命令行构建（需要 MSBuild 在 PATH 中）：

```bash
msbuild ZufyUI.slnx /p:Configuration=Release /p:Platform=x64
```

> `ZufyUI.cpp` 是一个综合演示程序（左侧导航 + 多个测试页），同时也充当框架的“冒烟测试”。真正要做自己的应用时，只需要包含头文件并写自己的 `WinMain`。

## 项目结构

```text
ZufyUI/
├── ZufyUI.h              # 核心：类型 / 信号槽 / 字体 / 元素 / 布局 / 菜单 / 窗口
├── ZufyUIWidgets.h       # 基础控件：Label / Button / TextBox / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider
├── ZDataViewer.h      # 数据视图：ListView / TableView / TreeView
├── ZufyUI.cpp            # 综合演示程序入口（WinMain）
├── ZufyUI.slnx           # 解决方案
├── ZufyUI.vcxproj        # 工程文件
├── AGENTS.md          # 开发原则（含“任何 bug 先找根源再修”的规则）
└── docs/
    └── API.md         # 完整 API 文档
```

## 快速开始

```cpp
#include "ZufyUIWidgets.h"
using namespace ZufyUI;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    Window win;
    if (!win.Create(1000, 700, L"ZufyUI Demo"))
        return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto btn = std::make_shared<Button>(L"点我");
    btn->Connect(btn->Clicked, []() {
        MessageBoxW(nullptr, L"Hello ZufyUI!", L"提示", MB_OK);
    });

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto state = std::make_shared<Label>(L"开关：关");
    toggle->Connect(toggle->Toggled, [state](bool on) {
        state->SetText(on ? L"开关：开" : L"开关：关");
    });

    row->AddChild(btn);
    row->AddChild(toggle);
    row->AddChild(state);
    root->AddChild(row);

    win.Run();
    return 0;
}
```

要点：

- `Window::Create` 会创建窗口、初始化 Direct2D、应用亚克力背景并生成一个默认的根 `ColumnBox`；
- 用 `std::make_shared<T>()` 创建控件，`AddChild` 挂到布局上；
- 用 `Connect(信号, 槽)` 绑定事件，连接会随控件析构自动断开；
- 最后调用 `win.Run()` 进入消息循环。

运行效果（就是上面这段代码）：

![快速开始：一个窗口 + 按钮 + 开关](docs/images/quickstart.png)

### 再走两步

**第 2 步：换个背景。** 只有三个背景层可选——`无 / 亚克力 / 云母`，背景层之上再叠一个带 Alpha 的颜色：

```cpp
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // 云母 + 全透明叠加色
```

![背景层：手动云母](docs/images/backdrop-mica.png)

**第 3 步：自定义标题栏。** 换个标题栏，窗口立刻不一样：

```cpp
#include "ZufyUIWindowTool.h"   // DefaultTitleBar 在这里

auto bar = std::make_shared<DefaultTitleBar>();
win.SetCustomTitleBar(bar);
```

![自定义标题栏](docs/images/custom-titlebar.png)

更多控件的总览（主示例程序首屏）：

![ZufyUI 示例程序](docs/images/demo-main.png)

## 更新日志

### 2026-09-26 — 托盘 / 任务栏 / 菜单增强 + 应用身份自注册

**菜单**
- 重做右键菜单为**分层窗口 + 自绘柔阴影 + 圆角 + 渐显**；修复「第二次弹不出」（根因：勾选对勾的 `ID2D1StrokeStyle` 静态跨工厂复用 → `EndDraw` 报 `D2DERR_WRONG_FACTORY`，改为随实例工厂创建/释放）。
- 菜单项增强：`id` / 勾选 / 单选 / 默认项（加粗 + 回车）/ 危险项（红字）/ 快捷键提示 / 禁用；逐项 `bgColor` / `textColor` / `font`；`Menu::font` / `SetDefaultFont`。
- **动态菜单工厂** `UIElement::SetContextMenuFactory`（每次右键现搭）+ `SetContextMenuEnabled`。
- 独立弹出 `Menu::ShowAt` / `ShowAtCursor`（不绑定窗口，托盘菜单用）；开关菜单互斥。
- 悬停高亮改为半透明叠加（自定义底色上仍可见）。

**托盘 / 任务栏 / 图标**
- `TrayIcon`（`Shell_NotifyIcon` v4）：悬停 / 点击 / 右键 / 双击、徽章（自绘合成）、气泡通知（`ShowBalloon`，`BalloonIcon` 可选 None/Info/Warning/Error/Custom，支持实时 / 静音）；`explorer` 重启自动重加。
- 任务栏：进度（`SetTaskbarProgress`）、覆盖徽章（`SetTaskbarOverlayIcon`）、缩略图工具栏（`SetThumbButtons`，`Image` 版自动建 `HIMAGELIST`）、跳转列表（`SetJumpList`，Tasks + 分类 + 最近）。
- `Window::SetAppIcon`（统一原生大/小 + 类图标 + 自定义标题栏）、`ShowActivate`（四模式）、`Flash`（`FLASHW_ALL`）；`TitleBar` 图标徽章 / 进度。
- `Image::ToHICON` / `CopyPixelsBgra`。

**数据视图**
- `ListView::ItemRightClicked(row)` + `GetContextRow()`；`TableView::CellRightClicked(row,col)` + `GetContextRow()/GetContextColumn()`，支持列表/表格右键菜单。

**应用身份自注册（新）**
- 声明 `AppInfo{ displayName, aumid, icon }` + `RegisterApp(...)`（自由函数 / `Application::RegisterApp` 转发）：统一进程 AUMID，让 toast / 跳转列表 / 任务栏分组归属同一身份。
- **需显式授权宏 `ZUFYUI_ALLOW_APP_REGISTRATION`**；授权后写注册表 `HKCU\...\AppUserModelId\<AUMID>`（DisplayName + IconUri）+ 把图标缓存到 `%LOCALAPPDATA%\ZufyUI\AppReg\<AUMID>\app.ico`，**进程退出清理缓存与注册表项**；未授权则只设 AUMID、零副作用。

**构建**
- 引入预编译头 `pch.h` / `pch.cpp`（改 demo 的编译 ~1.1s）；`Release` 关闭调试信息；`Debug` 补 `/bigobj`。

> 已知问题：菜单窗口走「分层窗口 + 软件渲染目标」路径且每个菜单自建 D2D 工厂 → 每弹一次菜单约 +90MB（关闭后数秒回收）。计划改为复用共享设备（菜单走 DComp 路径），修复前请留意。

### 2026-09-20 — 性能优化 + 背景 API 收敛（v1.9.6）

- **性能**：切页内存峰值 300–400MB → **~30MB 封顶**；布局机制重构（DesiredSize 缓存 + 两级脏位，去掉"布局失效即全清缓存"、切页不再全量重排）；拖动/缩放期间不再每帧整树命中检测；**全局文本布局缓存**（跨控件共享）+ `Label` 省略号改二分；去每元素 `GetDpi()`。
- **背景 API 收敛为"背景层三选一 + 叠加色"**：`Backdrop` 只有 `None / Acrylic / Mica`；**删除 `BackdropMode`**；删除 `ReloadAcrylic/ReloadMica`；新增 `SetBackgroundParams(层, BackgroundParams/AcrylicPreset)` 与 `AcrylicParams`/`MicaParams`。亚克力/云母一律由 ZufyUI 自己实现，**Win10 与 Win11 效果一致**。
- **数据控件**：`ListView` 新增 `BeginUpdate/EndUpdate` 批量增删。
- **一批修复**：排序补发 `SelectionChanged`、`ComboBox` 过滤保留选中、`TextBox` 撤销栈、`CaptionButton` 按下行为、滚动条除零守卫、图像缓存加设备代次等。

### 2026-09-19 — 1.8.0 后续修复与完善（v1.8.1）

**背景**
- DComp 亚克力背景层改为**可运行时重建**（新增 `UpdateDCompBackdrop()`）：`SetBackdrop` / `SetBackdropMode` 在窗口创建后再切换也会生效；`Acrylic → Mica/其它` 时会正确清除旧背景层（不再露底/发黑）；设备丢失重建后也会重新挂上。

**窗口**
- 新增 `Window::SetTitle(const std::wstring&)` 与 `Window::SetIcon(HICON bigIcon, HICON smallIcon)`（原生标题栏；自定义标题栏请用 `TitleBar::SetTitle`）。
- `SetCustomTitleBar` 会套用当前 `SetTitleBarVisible` 状态，消除"先隐藏再装栏"的不一致。

**信号（连接）**
- `Connection` 改为 **Qt `QMetaObject::Connection` 风格的被动句柄**：可拷贝、**析构不再自动断连**；`UIElement::Connect(...)` 现在**返回 `Connection`**，忽略返回值安全，需要单独断开时 `auto c = elem->Connect(sig, slot); … c.disconnect();`。自动断连仍由元素的 `ConnectionGroup` 负责（`ImageManager` 的 DeviceReset 订阅同样改走 group）。移除冗余的 `autoConnections_`。

**数据视图 / 控件**
- `TableView` 单元格键由 `(long long)row*10000+col` 改为 `(uint64)row<<32 | col`，修列数 ≥ 10000 时键冲突（`cellSel_` / `cellTextColors_` / `cellTips_`）。
- `ListView::MoveItem` 现在会触发 `SelectionChanged`（并 `EnsureVisible`）。
- `ComboBox` 选项宽度加缓存（仅数据变化重算），`SetToolTip` 移出 `Measure`（去每帧测量与副作用）。
- `ScrollViewer` 内容裁到视口（新增 `UIElement::SetClipRect`），不再画到滚动条下面；滚动条本身不受影响。

**其它**
- `PageHost` 新增过渡缓动 `TransitionEasing { Linear, EaseInOut, EaseOut }`，默认 `EaseInOut`（缓入缓出，平滑）；`SetTransitionEasing` 切换。
- 进程 DPI 感知只设置一次；删除 `D2DERR_RECREATE_TARGET` 死判断。
- **修复内存泄漏**：`AppCore::dqController_` 改用 `ComPtr` 持有并释放。
- demo：移除独立的"自定义标题栏窗口"（主窗口已是自定义标题栏）。

### 2026-09-19 — 渲染迁移到 DirectComposition + 背景/信号 API 重整（v1.8.0）

**渲染：迁移到 Direct2D 1.1 + DXGI flip SwapChain + DirectComposition**

- `Window` 不再使用 `ID2D1HwndRenderTarget`，改为 `ID2D1DeviceContext` + `IDXGISwapChain1`（`CreateSwapChainForComposition`）+ DComp 视觉树，窗口加 `WS_EX_NOREDIRECTIONBITMAP`，内容经 DComp 提交、支持逐像素透明。
- `AppCore` 进程级共享 `ID3D11Device → IDXGIDevice → ID2D1Device` 与 WinRT `ICompositor`，多窗口复用。
- 设备丢失/DPI 变化/尺寸变化走统一的资源丢弃与重建（`DiscardDeviceResources` / `ResizeSwapChain`），并新增 `DeviceLost` / `RenderingError` 信号。

**亚克力：改用 DComp `HostBackdropBrush` + 高斯模糊**

- 亚克力由 DComp 背景层（`ICompositor3::CreateHostBackdropBrush`，回退 `ICompositor2::CreateBackdropBrush`，外包高斯模糊效果）采样宿主背景绘制，配合 `DWMWA_USE_HOSTBACKDROPBRUSH` + `AccentState(HOSTBACKDROP)`；不再依赖会随窗口框架失效的 `DWMWA_SYSTEMBACKDROP_TYPE`。
- 附带手写的 `IGraphicsEffect` / `IGraphicsEffectD2D1Interop` 封装（无需 Win2D），依赖新增 `d2d1effects_2.h` 与 `dxguid.lib`。

**背景 API 重整（表示“要什么” vs “用什么 API”）**

- 新增 `Backdrop { None, Normal, Blur, Acrylic, Mica, MicaAlt }`：表示**要什么效果**。
- `BackdropMode { Auto, System, Accent }`：表示**用什么 API 实现**。
- `SetBackdrop(Backdrop, tint = 0)` / `GetBackdrop()`、`SetBackdropMode` / `GetBackdropMode()`。
- 删除旧的 `WindowBackdrop` / `SystemBackdropMaterial`（含义与命名混淆）。

**回调 → 信号（统一用信号槽）**

- `UIElement` 的事件由裸 `std::function` 成员改为信号：`MouseEnter / MouseLeave / MouseMove / MouseDown / MouseUp / KeyDown / KeyUp / Char / Focused / Blurred`。
- `MenuItem::Clicked`、`CaptionButton::Clicked`（默认行为由 `DefaultTitleBar` 接线，自定义标题栏可自行连接/拦截）。
- 新增 `Window::Closing`（`ZSignal<bool*>`，槽可置 `*cancel=true` 取消关闭，用于“关闭前询问保存”）、`Window::BackdropUnsupported` / `Window::DeviceLost` / `Window::RenderingError`。
- 全局 `UIZSignals::DeviceReset`：渲染设备资源被丢弃/重建时触发，供订阅者清理按设备缓存（`ImageManager` 据此清图像位图缓存，修复旧渲染目标指针悬垂与位图泄漏）。
- 删除 `SetBackdropUnsupportedHandler` / `SetDeviceLostHandler` / `SetRenderingErrorHandler`。

**窗口**

- `Window::Create` **不再自动显示**，何时显示由应用 `Show()` 决定。
- 自定义标题栏新增**每个按钮**的启用/可见控制：`TitleBar::GetButton / SetButtonEnabled / IsButtonEnabled / SetButtonVisible / IsButtonVisible`；`DefaultTitleBar` 便捷封装 `SetMinimizeEnabled / SetMaximizeEnabled / SetCloseEnabled` 及对应 `...Visible`。
- `DWMWA_BORDER_COLOR` 默认改为系统默认（不再硬编码灰色）。

**修复与清理**

- 组合框弹出列表的 hover/点击判定改用与绘制一致的宽度（`ListWidth()`），修右侧超出区域点不到。
- `TableView::SortByColumn` 排序后同步重映射 `checkedRows_`。
- 双击判定改用系统 `GetDoubleClickTime()`（三处）。
- `ScrollViewer::Arrange` 用 `SetVisibleNoInvalidate()` 避免布局循环；`DrawTextWithEllipsis` 截断由 O(n²) 改为二分。
- `MenuWindow` 多显示器定位改用 `MonitorFromPoint` + `GetMonitorInfo`。
- 清理多项兜底/临时逻辑（`PageHost` 调试特例、无用的 `animationIdleFrames_` 等）。

**测试程序（`ZufyUI.cpp`）**

- 主窗口改为**自定义标题栏**。
- 移除独立的小工具窗口，多窗口 / owned / 模态演示移入主窗口的**“多窗口”页**。
- 版本号提升到 **1.8.0**（新增 `ZufyUI_VERSION_STRING` 等宏，demo 标题引用）。

### 2026-09-16 — 自定义标题栏（ZufyUIWindowTool）与背景三模式

**新增：自定义标题栏（新文件 `ZufyUIWindowTool.h`）**

- `TitleBar` / `CaptionButton` / `DefaultTitleBar`：窗口级控件，作为“不参与布局”的覆盖控件由 `Window::SetCustomTitleBar` 安装；传 `nullptr` 恢复原生标题栏。
- 三件套用系统图标字体绘制（`Segoe Fluent Icons` / `Segoe MDL2 Assets`，码位 `E921/E922/E923/E8BB`），带 **hover/pressed 渐变**，关闭悬停红 `#C42B1C`；按钮在 `WM_NCHITTEST` 报告为 `HTMINBUTTON/HTMAXBUTTON/HTCLOSE`，从而启用系统 **Snap Layouts**；点击在 `WM_NCLBUTTONUP` 处理；标题栏区域为**拖动区域**（拖动 / Aero Snap / 双击最大化交给系统）。
- 标题栏与按钮**不参与 Tab 焦点**；默认 `DrawAfterLayout`、`UseCache`（仅状态变化重绘）。
- 便捷设置：`SetTitleBarVisible`、`SetButtonsEnabled`、`SetButtonWidth/Height`、`SetRightMargin`、`CaptionButton::SetAnimationSpeed` 等。

**新增：布局参与 / 拖动机制（`UIElement`）**

- `LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout }`：不参与布局的元素由 Window 在正常布局树**之前/之后**单独绘制；布局与 `ComposeImpl` 子递归都会跳过它们。
- 拖动区域：`SetDraggable / SetDragRegion / CollectDragRegions`；Window 每帧收集并交 `WM_NCHITTEST`（返回 `HTCAPTION`）。

**新增：背景三模式**

- `BackdropMode { Auto, Acrylic, SystemBackdrop }` + `SystemBackdropMaterial { Auto, Mica, MicaAlt, Acrylic }` + `SetBackdropUnsupportedHandler`（运行时不受支持的回调）。
- `SystemBackdrop` 走 Win11 `DWMWA_SYSTEMBACKDROP_TYPE`；不支持时回调并回退 `AccentState`。

**窗口 / 边框修复**

- 分屏（Snap）状态保留 DWM 边框与阴影：`WM_NCCALCSIZE` 只内缩**被吸附到工作区边缘**的那几条边（内缩量取 `DWMWA_VISIBLE_FRAME_BORDER_THICKNESS` 的实际厚度），最大化不内缩；最大化/还原/分屏状态变化时发 `SWP_FRAMECHANGED` 重算框架。
- 最大化时贴顶下拖可还原窗口；关闭键位置与原生一致。

**演示**：`ZufyUI.cpp` 新增“自定义标题栏窗口”。

**已知限制（重要）**

- ZufyUI 当前渲染在 `ID2D1HwndRenderTarget`（不透明重定向表面）上，**无法显示 DWM 系统材质**：`BackdropMode::Auto` 目前在 Win10/Win11 都走 `AccentState`；`SystemBackdrop` 模式在渲染目标迁移到 **DirectComposition** 之前**不可见**（会发灰/发黑）。
- `AccentState` 亚克力是未公开 API，会让 DWM **跳过最小化/最大化等窗口过渡动画**（Win10/Win11 均如此）。要同时获得“亚克力 + 原生动画 + Mica”，需把渲染迁移到 DirectComposition（SwapChain / CompositionSurface），属后续规划。

### 2026-09-15 — 图像系统、窗口机制增强与多项根源修复

**图像（新增 `ZufyUIImages.h`）**

- 新增 `Image`：WIC 解码 + Direct2D GPU 绘制/变换。加载：`FromFile / FromMemory / FromBase64 / FromResource(HMODULE,name,type) / FromResource(id,type) / FromHBITMAP / FromHICON`；变换：`Scaled / ScaledToWidth / ScaledToHeight / Rotated / Mirrored / Cropped`（轻量描述符，绘制时 GPU 施加）；绘制：`Draw`（opacity / 插值）、`Bake`；编码：`Save / Encode`（`CreateStreamOnHGlobal` 内存流，**无临时文件**）。
- `ImageManager`：共享 `IWICImagingFactory` 并登记设备缓存；`ImageDeviceCache`：每个渲染目标一张 `ID2D1Bitmap` 缓存，设备丢失统一重建。同一图像同一窗口只上传一次 GPU 位图。
- **根源修复**：`IWICStream::InitializeFromMemory` 不复制内存，而解码/转换是惰性的；改为在解码时用 `WICBitmapCacheOnLoad` 立即把像素拷进独立 WIC 位图，避免 `FromMemory/FromBase64/RT_BITMAP` 在绘制阶段读到已释放内存（此前表现为**图像完全不显示**）。
- **根源修复**：`DrawWithTransform` 的变换矩阵合成顺序写反，导致旋转/镜像后的图像被推出目标矩形（表现为旋转/镜像**什么都看不到**）；改用带 `center` 的 `Rotation/Scale` 重载。
- `Label` 支持**图标与嵌套子控件**：`SetImage/GetImage`、`SetIconSize/GetIconSize`、`SetIconSpacing/GetIconSpacing`、`AddChild/ClearChildren/GetChildCount`（图标 + 文本 + 子控件内联横排）。

**窗口**

- `RunModal` 改为官方机制：`EnableWindow(owner, FALSE)` + 激活模态窗 + `IsDialogMessage` 嵌套循环；**移除了全局低级鼠标钩子**（之前会吞掉其它进程的点击，模态一开整个桌面点击失灵）。
- owned 子窗口：**创建时**即通过 `CreateWindowEx` 的父窗口参数建立所有者关系，因此不再有独立任务栏按钮；父窗口最小化时**无条件**隐藏其 owned 子窗口、还原/激活时恢复（不再依赖系统“最小化分组”，避免父窗口在后台时子窗口不跟随）。
- 新增便捷 API：`Flash(times=5, captionOnly=true)` / `StopFlash()`（`FlashWindowEx`）、`GetOwnedWindows()`、`GetStyle()/GetExStyle()`、`SetWindowStyleFlag/SetWindowExStyleFlag`（如 `WS_EX_TOOLWINDOW`）、`Show()/ShowNoActivate()/Hide()/Raise()`。
- 新增 `OwnedMinimizePolicy { None, Hide, DisableMinimize }` + `SetOwnedMinimizePolicy`：处理 owned 子窗口被单独最小化的方式。
- **修复**：窗口销毁时清除其它窗口对它的裸引用（`owner_` / 隐藏列表），避免地址被复用后牵连不相干的窗口。
- **修复**：`SIZE_RESTORED` 在拖拽改变窗口大小时也会触发，之前会无条件恢复已隐藏的 owned 子窗口（拖一下大小子窗口就冒出来）；改为只在“最小化 → 还原”时恢复。

**控件 / 渲染**

- **根源修复**：元素缓存位图合成到主渲染目标时，目标像素尺寸必须与位图像素尺寸**完全一致**，否则任何插值都会把缓存整体重采样，导致文字发糊；改为用位图实际像素数反推目标尺寸。
- `ComboBox`：折叠框宽度按选项文本**平均宽度**自适应（放不下时省略号截断 + 自动 `ToolTip` 显示完整文本）；下拉列表宽度按**最宽选项**自适应，保证每个选项完整显示；修复“列表比框体宽时滚动条画到中间”的问题。

**文档**

- API 文档新增“**图像**”章节（中英），并补充 `Window`、`Label`、`ComboBox` 的新 API 与要点。

### 2026-09-13 — 大优化：交互状态、阴影 / ToolTip、控件与数据视图增强

**核心框架**

- `UIElement` 新增：启用/禁用（`SetEnabled/IsEnabled/IsEffectivelyEnabled`，禁用状态沿父链继承、拦截鼠标键盘、控件自行置灰）、通用 ToolTip（`SetToolTip/GetToolTip`）、阴影（`SetShadow/SetShadowColor/SetShadowBlur/SetShadowOffset/SetShadowCornerRadius/GetShadowExtent`）、右键菜单钩子 `OnContextMenu`。
- `Window`：元素阴影合成进离屏缓存；统一 ToolTip 浮层（锚定“显示时鼠标位置”上方固定偏移、悬停 0.5s 渐显、白底黑字带柔和阴影、鼠标移动即关闭）；Tab 焦点遍历（焦点环仅在 Tab 导航时显示，鼠标点击不显示）。
- 阴影重写为 **高斯 CDF 分层**（`DrawSoftShadow`）：按高斯分布分配每层 alpha，使叠加结果逼近 `targetA·(1-Φ(d/σ))`；`SetShadowColor` 的 alpha 语义为“边缘可见透明度”，内部约为其 2 倍。
- 帧时间钳制：`OnPaint` 的 `deltaTime` 上限 `0.033s`，修复空闲 / 最小化恢复后“动画一帧跳到终点”的问题。
- `PageHost` 修复切页动画期间同一页面每帧被 `UpdateAnimation` 两次、导致页面内嵌动画速度翻倍的问题。
- 定时器精度：窗口创建 `timeBeginPeriod(1)`、销毁 `timeEndPeriod(1)`，降低动画抖动。
- 调试输出统一由宏 `ZufyUI_DEBUG` 控制（默认关闭，定义后启用），Release 热路径不再有调试字符串构造。
- 性能 / 内存：`GetChildren()` 改为返回 `const&`（复用缓冲，消除每帧每节点分配）；`ZSignal::Fire` 用线程本地快照；`Compose` 增加裁剪剔除（完全在裁剪区外的子树直接跳过）；活跃动画集合复用缓冲；`DrawSoftShadow` 用定长数组避免每帧堆分配。

**基础控件**

- `Button`：禁用态、Enter/Space 键盘触发、可切换（`SetCheckable/SetChecked/IsChecked/Toggled`）、文字对齐 / 内边距、自动重复。
- `CheckBox`：悬停光晕动画、文字标签与颜色、悬停框色、键盘、禁用。
- `ToggleSwitch`：禁用、文字标签、键盘、`SetSize`、不确定态、自定义颜色。
- `Label`：内边距、行距、最大行数（超出省略）、`GetDesiredSize`、禁用色。
- `ProgressBar`：`ValueChanged`、范围、显示百分比文本与文字颜色、禁用。
- `Slider`：`SliderReleased`、步进 / 吸附（`SetStep/SetSnapToStep`）、方向键 / Home / End、禁用。
- `ScrollViewer`：滚动条可见性策略（`Auto/Always/Hidden`）、`ScrollChanged`、`GetScrollOffset`、实例颜色、`ContentMargin`。
- `TextBox`：只读（`SetReadOnly`）、输入过滤（`SetInputFilter`）、`ReturnPressed`、公开选区 / 撤销 / 复制粘贴 / 全选、占位符颜色、密码显隐（`SetRevealPassword`）；修复 Shift 与鼠标拖选“选区不累积”的问题（引入独立锚点）。
- `ComboBox`：数据增删查、占位符、每项禁用、最大可见项、开合信号（`DropDownOpened/DropDownClosed`）、**可编辑 + 输入过滤**（`SetEditable/SetFilterEnabled/SetEditText`，带闪烁光标与点击定位）。

**数据视图**

- `ListView`：选择模式新增 `None`；首字母定位（type-ahead）；排序回调 + 排序指示；每项禁用 / 单独文字色 / ToolTip（**按项目绑定，排序后仍然跟随原项目**）；键盘上下键跳过禁用项。
- `TableView`：按列排序 + 排序指示（排序 / 增删行列时行级元数据随行重映射）；单元格文字色 / ToolTip；按行禁用；列隐藏；列对齐；按行高度；键盘上下键跳过禁用行。
- `TreeView`：过滤 / 搜索、`GetNodePath`、默认展开深度；节点 `tooltip` 接入基础类的统一 ToolTip。

**文档**

- 新增本更新日志；API 文档从“定义罗列”改为更详细的“实现要点 + 易混点”风格；补充信号与对象生命周期的引用环警示。

### 2026-09-13（续）— 多窗口支持（Application）

- 新增 `ZufyUI::Application`：`app.CreateWindow(...)` 创建窗口、`app.Run()` 运行单一共享消息循环，Qt 风格；窗口之间真正独立。
- 重绘 / 布局按所属窗口路由（`UIElement::GetWindow()`）；DPI 缩放改为**线程本地**（每个窗口渲染前设置自己的缩放）；`Window` 新增实例信号 `Activated` / `Deactivated` / `Closed`，全局 `UIZSignals::WindowDeactivated` 与 `GlobalMouseDown` 增加 `Window*` 参数。
- 关闭单个窗口不再退出应用，**最后一个窗口关闭才退出**；`ID2D1Factory` 与 `timeBeginPeriod` 由应用核心共享。
- 所有窗口相关全局信号统一带 `Window*`（含 `DrawOverlay` / `GlobalMouseDown` / `WindowDeactivated` 及捕获、重绘兜底），订阅者用 `GetWindow()` 过滤；修复“A 窗口的下拉弹层被画到 B 窗口 / 点击 B 导致 A 误收起”的串扰。新增 `Window::SetPosition/SetSize/IsValid`。
- 修复“ComboBox 展开后一直持有系统鼠标捕获、导致其它窗口无法使用”：Win32 捕获只在按住鼠标期间持有，松开立即释放（元素级逻辑捕获不受影响）；另处理 `WM_CAPTURECHANGED`。
- 新增模态与 owned 父子窗口：`Window::SetOwner` / `GetOwner`、`Window::RunModal(owner)`、`Application::CreateWindow(..., owner)`；模态期间点击被禁用的父窗口会让模态窗口**闪烁**提示。
- 稳健性：元素改用**窗口 id** 记录所属窗口（替代裸指针），窗口销毁后 `GetWindow()` 返回 nullptr，消除“悬垂窗口指针”导致的崩溃。
- 修复 `PageHost` 过渡结束后**每帧重复**对非当前页 `ReleaseDeviceResources()` 的问题（改为仅在过渡完成的那一帧释放一次）；非当前页会被设为不可见，其中的展开控件（ComboBox）随之自动收起。
- 修复 `UIElement::Connect` 返回被移动过的空 `Connection` 的隐患（改为不返回）；修复 `TreeView::RemoveChildren` 未清理 `checkAnim_` 造成的悬垂节点键。
- 新增 `AGENTS.md`，固化“任何 bug 先定位根源、从根源修复，不用新机制掩盖”的开发原则。
- 向后兼容：单窗口 `Window win; win.Create(...); win.Run();` 仍可用。`#undef CreateWindow` 以避免与 Win32 宏冲突。

## 文档

- 在线文档站点：<https://zhc9968.github.io/ZufyUI/>
- API 参考（中文，15 章）：<https://zhc9968.github.io/ZufyUI/docs/API.html>
- API Reference (English)：<https://zhc9968.github.io/ZufyUI/docs/API.en.html>

## 许可证

本项目采用 **MIT** 许可证，详见 [LICENSE](LICENSE)。
