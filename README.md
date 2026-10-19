# ZufyUI

> **项目已更名：原名 `ZUI` → 现名 `ZufyUI`**（避免与其它同名项目冲突）。旧的仓库地址会由平台自动跳转到新地址（见下方链接）。

> 基于 Direct2D 的 C++ 原生 Win32 自绘 UI 框架，含布局、控件、信号槽与高 DPI 适配。

> 语言：**简体中文** · [English](README.en.md)

ZufyUI 是一个**纯头文件**的 Windows 桌面 UI 框架，直接建立在 Direct2D / DirectWrite / DWM 之上。它不依赖 Qt、MFC 或任何第三方库，把控件、布局、动画、字体、数据视图和信号槽都装进几个 `.h` 里，适合想要轻量、可控、现代观感的原生 C++ 应用。

- 仓库：GitHub <https://github.com/zhc9968/ZufyUI> · Gitee <https://gitee.com/zhc9968/ZufyUI>
- 许可证：MIT
- 在线文档：<https://zhc9968.github.io/ZufyUI/>
- API 参考：<https://zhc9968.github.io/ZufyUI/docs/API.html>

> ⚠️ **独立渲染线程（编译开关 `ZUFYUI_RENDER_THREAD`）为实验特性，默认关闭、不稳定、bug 较多、不建议开启。** 详见 API 参考「独立渲染线程」一节。

## 特性

- **Direct2D 自绘**：硬件加速渲染，亚克力（Acrylic）背景、圆角、阴影，观感现代。
- **完整布局系统**：`ColumnBox` / `RowBox` / `GridLayout`，支持间距、拉伸权重、填充、对齐与跨行跨列。
- **信号槽**：内置 `ZSignal` / `Connection`，支持 `Connect` 自动管理生命周期，以及当前线程 / 新线程 / UI 线程三种分发策略。
- **丰富控件**：标签、按钮、文本框、下拉框、开关、滚动容器、进度条、滑块，以及列表 / 表格 / 树三种数据视图。
- **图表**：`BarChart` / `LineChart` / `PieChart`（横向方向、面积填充、滚轮缩放、实时刷新），来自 `ZufyUICharts.h`。
- **拖放**：内置 OLE 拖放（`ZufyUIDragDrop.h`），支持拖入文本 / 文件 / 自定义格式，并可把文本 / 图片 / 文件拖出到资源管理器；文本框、下拉框、图片、按钮、标签均已接入；支持元素级与**整窗级**接收（`Window::SetDropTargetEnabled`）。
- **卡片组件**：可折叠卡片 `Expander`（缓动 + 箭头旋转 + 标题悬停高亮 + `SetIcon` 标题图标）、设置行列表 `SettingsList`；图片展示用 `Label::SetImage` + `SetImageFit`（等比缩放完整可见，可拖出）。
- **窗口装饰**：顶部**菜单栏** `MenuBar`（VS 风格；默认叠在自定义标题栏上、可切换）、底部**状态栏** `StatusBar`（左右面板：图标 + 文本）。
- **主题系统**：语义色板 `Theme`（浅/深/高对比度），应用级 / 窗口级切换，**跟随系统**深浅色与强调色（`WM_SETTINGCHANGE` 等自动重解析），`ThemeManager::Map` 色差重映射；**实例显式设色优先**（设过色就不被主题覆盖）。
- **链式构造 DSL**（`ZufyUIDsl.h`，可选）：`Ref<T>` 的 `with/on/add/apply` + `Make`/`Wrap`/`Col`/`Row`/`Grid`（含 `Cell(row,col,...)` 坐标）+ 各控件短工厂，一行组合控件 / 子树 / 信号；`dsl::App()` 窗口构建器（配置 + `hidden` + 窗口信号 + 自动 `Show()`）；SFINAE 门控的控件糖覆盖布局、尺寸、权重、配色、间距、对齐与数据视图 / 菜单构建。
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
├── ZufyUI.h              # 核心：类型 / 信号槽 / 字体 / 元素 / 布局 / 菜单 / 窗口 / 主题
├── ZufyUIWidgets.h       # 基础控件：Label / Button / TextBox / TextEdit / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider / MenuBar / StatusBar …
├── ZDataViewer.h         # 数据视图：ListView / TableView / TreeView
├── ZufyUICharts.h        # 图表：ChartBase / BarChart / LineChart / PieChart
├── ZufyUIAcrylic.h       # DComp 合成效果 + 亚克力 / 云母配方（由 ZufyUI.h 包含）
├── ZufyUIImages.h        # Image / ImageManager（图片加载与设备缓存）
├── ZufyUIIcons.h         # FontIcon 字形控件
├── ZufyUIWindowTool.h    # 托盘 / 自定义标题栏 / 无障碍(UIA) / 系统对话框 / 调试通道
├── ZufyUIDragDrop.h      # 拖放：DragData / DragDataBuilder / Window::BeginDrag / 元素拖放钩子
├── ZufyUIDsl.h           # 可选：链式构造 DSL（Ref / Col / Row / Grid / 短工厂 / dsl::App）
├── ZufyUI.cpp            # 综合演示程序入口（wWinMain）
├── ZufyUI.slnx           # 解决方案
├── ZufyUI.vcxproj        # 工程文件
├── AGENTS.md             # 开发原则（含“任何 bug 先找根源再修”的规则）
├── HANDOFF.md            # 交接文档（硬规则 / 架构 / 坑与经验 / 发版流程）
├── TODO.md               # 唯一权威待办清单
├── docs/
│   ├── API.md            # 完整 API 文档（中文）
│   ├── API.en.md         # 完整 API 文档（English）
│   └── pitfalls.md       # 易错点总表
├── tests/
│   └── dsl_tests.cpp     # DSL 自检（编译期 + 运行期断言，不建窗口）
└── tools/
    ├── build.ps1         # 一键构建：补 BOM → 构建 demo + Debugger → 跑自检
    ├── ensure_bom.ps1    # BOM 守卫（构建前自动补 UTF-8 BOM）
    ├── run_dsl_tests.ps1 # 编译并运行 DSL 自检
    ├── check_api_parity.py # 双语文档代码块一致性校验
    └── colorscan.py      # 硬编码色审计
```

## 快速开始

> 目标：**10 分钟内跑出第一个窗口**，并清楚接下来该看哪一章。按下面的步骤走即可。

**第 0 步：准备（纯头文件，零构建配置）**
- 把 `ZufyUI.h`、`ZufyUIWidgets.h`、`ZDataViewer.h`、`ZufyUICharts.h`、`ZufyUIWindowTool.h`、`ZufyUIDragDrop.h` 所在目录加入**包含路径**即可；**不需要编译任何 .cpp**，也不依赖第三方库。
- 依赖的系统库由库内 `#pragma comment(lib, ...)` 自动链接：`d2d1 / dwrite / dwmapi / imm32 / winmm`。
- GUI 程序入口用 **`wWinMain` + `/SUBSYSTEM:WINDOWS`**。
- **⚠ 入口易错**：若坚持写 `int main()`，必须把子系统设成 `/SUBSYSTEM:CONSOLE`，或加 `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")`，否则链接器找不到入口。

**第 1 步：最小窗口 + 一个按钮 + 一个开关**

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

要点（每一步都在做什么）：

- `Window::Create` 会创建窗口、初始化 Direct2D、应用背景并生成一个默认的根 `ColumnBox`（margin 20、spacing 10）；
- 控件用 `std::make_shared<T>()` 创建，`AddChild` 挂到布局上；**父容器用 `shared_ptr` 拥有子元素**；
- 用 `Connect(信号, 槽)` 绑定事件，连接会随控件析构自动断开；
- 最后 `win.Run()` 进入消息循环；所有窗口/控件都在**同一 UI 线程**创建与操作。
- **⚠ 单位**：坐标与尺寸都是 **DIP**（不是物理像素），框架自动做 DPI 吸附，你不必自己换算。

运行效果（就是上面这段代码）：

![快速开始：一个窗口 + 按钮 + 开关](docs/images/quickstart.png)

**第 2 / 3 步：换个背景、换个标题栏**

**第 2 步：换个背景。** 只有三个背景层可选——`无 / 亚克力 / 云母`，背景层之上再叠一个带 Alpha 的颜色：

```cpp
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // 云母 + 全透明叠加色
```

![背景层：手动云母](docs/images/backdrop-mica.png)

**第 3 步：自定义标题栏。** 换个标题栏（顺带换成云母背景，标题栏更明显）：

```cpp
#include "ZufyUIWindowTool.h"   // DefaultTitleBar 在这里

auto bar = std::make_shared<DefaultTitleBar>();
win.SetCustomTitleBar(bar);
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // 顺带换成云母
```

![自定义标题栏](docs/images/custom-titlebar.png)

更多控件的总览（主示例程序首屏）：

![ZufyUI 示例程序](docs/images/demo-main.png)

**第 4 步：接下来看什么**
- 布局与控件总览：[API 参考 · 布局系统 / 基础控件](docs/API.md)
- 数据界面（列表 / 表格 / 树）：API 参考「数据视图」章
- 画图表：API 参考「图表」章（`BarChart` / `LineChart` / `PieChart`）
- 窗口周边（标题栏 / 托盘 / 系统对话框）：API 参考「窗口周边」章
- 无障碍与调试/自动化：API 参考「无障碍与调试通道」章

**务必记住的三件事**
1. **DIP**：所有尺寸/坐标都是 DIP，不用管 DPI。
2. **`shared_ptr` 即所有权**：控件用 `make_shared` 拥有、`AddChild` 挂父；丢掉唯一的 `shared_ptr` 就等于销毁。
3. **入口/子系统**：`wWinMain` + `/SUBSYSTEM:WINDOWS`（或按上面的“入口易错”处理）。


## 版本历史

> 当前版本：**1.23.0**（以 `ZufyUI.h` 的 `ZUFYUI_VERSION_STRING` 为准）。
> 只列“大概更新/修复了什么”（名词级）；完整逐项说明见 [CHANGELOG.md](CHANGELOG.md)。

- **1.23.0**：`HwndView` 原生窗口宿主；`Window` 外部视觉挂载；浮窗同步/性能；`NumberBox` 错误态；数据视图填充。
- **1.22.1**：布局溢出/对齐；IME/代理对；无障碍 id/命中；焦点框主题；pch/MarkUserColor。
- **1.22.0**：API 债（默认参数拆重载、`make_*`、`SetScrollableContent`、`HookHandle`）；MessageBox 首击；ScrollViewer 滚动链；SplitView 展开缓动。
- **1.21.0**：SplitView 折叠；渲染/布局/图表修复；demo 首页/菜单/状态栏；动画口径统一。
- **1.20.0**：DSL `root`/`add`/`Cell`/`apply`/`Wrap`；`WindowApp` 扩面；`build.ps1`；DSL 测试。
- **1.19.7**：图表滚动条；NumberBox 滚轮；动画集合悬垂；`Menu::ShowAt`；布局 spacing；数据视图释放；IME；`TODO.md`。
- **1.19.6**：ListView 勾选框命中；Label 换行宽度；版本单一来源。
- **1.19.5**：DSL `App`/`WindowApp` + SFINAE 糖 + 图表组合。
- **1.19.4**：DSL 链式构造；CheckBox 标签裁切；数据视图缓存签名；图表 fill 回退。
- **1.19.3**：主题系统 + 跟随系统 + 实例显式色优先。
- **1.19.2**：MenuBar / StatusBar；Menu `onClosed`。
- **1.19.1**：TextEdit 撤销/重做；拖放；ScrollViewer 视口测量；`Label::SetImageFit`。
- **1.19.0**：拖放；`Expander`/`SettingsList`；ComboBox 箭头；图表。
- **1.18.1**：文档/配图更新。
- **1.18.0**：图表 `ZufyUICharts.h`；`TextEdit`；`PageHost` 延迟释放；文档重整。
- **1.17.0**：UIA；调试通道；布局权值；Label/Button 自动 ToolTip；TreeView 背景/滚动；标题栏 Pin。
- **1.16.0**：渲染线程默认关；系统对话框；`UIZSignals::Error`；光标/悬停/首列修复。
- **1.15.0**：数据视图虚拟化；跨线程收口；ComboBox 源索引。

> 更早版本（≤ 1.14）见 [CHANGELOG.md](CHANGELOG.md)。

## 文档

- 在线文档站点：<https://zhc9968.github.io/ZufyUI/>
- API 参考（中文，15 章）：<https://zhc9968.github.io/ZufyUI/docs/API.html>
- API Reference (English)：<https://zhc9968.github.io/ZufyUI/docs/API.en.html>

## 许可证

本项目采用 **MIT** 许可证，详见 [LICENSE](LICENSE)。
