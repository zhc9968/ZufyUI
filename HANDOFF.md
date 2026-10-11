# ZufyUI 交接文档（HANDOFF）

> 给**下一位接手者 / AI 智能体**：读这一份即可上手。配套：`AGENTS.md`（硬规则）、`TODO.md`（待办）、`docs/API.md`（API 参考）、`docs/API.en.md`（英文）、`README.md`（构建/快速开始）。
> 版本基线：`1.20.0`。**先读「§1 硬规则」和「§6 坑与经验」，再看代码。**

---

## 1. 硬规则（务必遵守）

1. **任何 bug：先定位根因，再从根源修**。不许用“加个新机制把 bug 盖住”来冒充修复；找不到根因就如实说“未定位”，别猜。
2. **`git commit` / `git push`（含 Gitee/GitHub/gh-pages）必须先经用户明确同意**；默认只改工作区、不提交。
3. **不擅自 revert**（用户明确要求除外）。
4. **中文源文件编码**：`.h` / `.cpp` 必须 **UTF-8 带 BOM**（否则 MSVC 按 GBK 解析中文注释会报莫名 `C2143/C2259/C2888`）；`.md` / `.tex` 用 **UTF-8 无 BOM**。
5. **文档不自动更新**：API / README 只有用户明确要求时才改。
6. **双语文档逐行对齐**：改动 `docs/API.md` 或 `docs/API.en.md` 后，**同一章节的代码块必须行数一致、顺序一致**，并跑 `python tools/check_api_parity.py`（退出码 0 = 一致）。
7. **构建前先杀进程**（否则文件占用 / 链接失败）。
8. 库是公开的 → 行为默认对标系统 / 官方控件（别改约定俗成的默认）。

---

## 2. 项目是什么 / 仓库结构

**ZufyUI**：零依赖、**纯头文件**的 Windows 桌面 UI 框架（自动链接 `d2d1/dwrite/dwmapi/imm32/winmm`）。渲染走 **Direct2D 1.1 + DXGI flip swapchain + DirectComposition**（不是 `ID2D1HwndRenderTarget`）→ 支持逐像素透明、亚克力/云母、DComp 弹层。C++20。

| 文件 | 作用 |
|---|---|
| `ZufyUI.h` | 核心：类型/信号槽/字体/`UIElement` 基类/布局/`Window`/菜单/主题(`Theme`/`ThemeManager`)/`Application` |
| `ZufyUIWidgets.h` | 基础控件：Label/Button/TextBox/TextEdit/ComboBox/CheckBox/RadioButton/ToggleSwitch/Slider/NumberBox/ProgressBar/ProgressRing/ScrollViewer/ScrollBar/SplitView/TabView/MenuBar/StatusBar/Expander/SettingsList + 图标 |
| `ZDataViewer.h` | 数据视图：ListView/TableView/TreeView（虚拟化 + Label 池化） |
| `ZufyUICharts.h` | 图表：ChartBase/BarChart/LineChart/PieChart |
| `ZufyUIImages.h` | `Image`（加载/变换/编码）+ `ImageManager`/`ImageDeviceCache` |
| `ZufyUIWindowTool.h` | 标题栏(DefaultTitleBar/CaptionButton)/MessageBox/托盘/系统对话框/无障碍(UIA)提供程序/调试通道 |
| `ZufyUIDragDrop.h` | OLE 拖放（末尾被 `ZufyUI.h` `#include`） |
| `ZufyUIIcons.h` | 图标字形 |
| `ZufyUIDsl.h` | 可选**链式构造 DSL**（`dsl::`：`Ref<T>`/`Make`/`Col`/`Row`/`Grid`/短工厂/`App()` 窗口构建器） |
| `ZufyUI.cpp` | demo（左侧导航 + 多测试页），同时充当冒烟测试 |
| `docs/API.md` / `API.en.md` | API 参考（中/英，**按使用分类**，非按头文件） |
| `tools/` | `check_api_parity.py`（双语文档校验）、`colorscan.py`（硬编码色审计）、`color_inventory.md`（审计快照） |
| `TODO.md` | 唯一权威待办（合并了旧计划） |
| `AGENTS.md` | 硬规则（含“先找根源再修”） |

> 另有独立工程 **`C:\project\ZufyUIDebugger`**（用 ZufyUI 写的调试器；改库要一起构建验证）。

---

## 3. 构建 / 运行

**推荐：一键脚本**（构建前自动补 BOM，不会再因 BOM 被剥而编译失败）：
```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
#   流程：补 BOM → 杀占用进程 → 构建 ZufyUI(demo) → 构建 ZufyUIDebugger → 跑 DSL 自检
#   开关：-Configuration Debug | -SkipDebugger | -SkipTests | -SkipBomFix | -CheckBomOnly
```
- `tools/ensure_bom.ps1`：BOM 守卫。只给「含非 ASCII 且缺 BOM」的 `.h/.hpp/.cpp/.cxx/.ps1` 补 BOM
  （纯 ASCII 的不动，避免无谓 diff）；`-Check` 只检查、缺失返回 1（CI 用）。
  **编辑工具常会静默剥掉 BOM** → 它会以 GBK 解析中文，报一堆 `C2143/C2259/C2888/C4819/C2001`。
- `tools/run_dsl_tests.ps1`：编译并运行 `tests/dsl_tests.cpp`（不建窗口，可命令行 / CI 直接跑）。
- **⚠ `tools/` 下这三个 `.ps1` 故意写成「纯 ASCII、无 BOM」，请不要给它们加中文注释/中文输出。**
  原因：BOM 守卫不能自己依赖 BOM —— PowerShell 5.1 会把无 BOM 的含中文脚本按 GBK 读，
  于是守卫恰好在最需要它的时候解析失败（本项目实测踩过：`build.ps1` 被剥 BOM 后整脚本报假语法错，
  而 `ensure_bom.ps1` 自己丢了 BOM 时**无法自我修复**）。纯 ASCII 让这条修复链对 BOM 免疫。
  面向人的中文说明放在本文件（`.md` 本来就不该有 BOM）。

手动 MSBuild（VS 18 Community）：
```powershell
# 先杀进程
Get-Process ZufyUI,ZufyUIDebugger -ErrorAction SilentlyContinue | Stop-Process -Force
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "C:\project\ZufyUI\ZufyUI.vcxproj" /p:Configuration=Release /p:Platform=x64 `
  /p:DebugSymbols=false /p:DebugType=None /m /nologo /v:minimal
```
- 工程要点：`/bigobj`、pch（`pch.h`/`pch.cpp`）、`/SUBSYSTEM:WINDOWS` + `wWinMain`。
- 入口易错：坚持写 `int main()` 就要 `/SUBSYSTEM:CONSOLE`，或 `#pragma comment(linker,"/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")`。
- 反查错误：`<输出> | Select-String ': error'`。
- `pch.h` 里 `#ifdef _DEBUG` 才 `#define ZufyUI_DEBUG`（Release 不强制日志）。
- 命令行编译单独 TU（如测试）时记得显式链接 `user32.lib` 等，并加 `/utf-8`：
  MSVC 默认把**窄字面量**按系统 ANSI(GBK) 编码，中文输出会乱码。

BOM 重存（编辑含中文的 `.h/.cpp` 后；或直接跑 `tools/ensure_bom.ps1`）：
```powershell
$enc = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText($p, [System.IO.File]::ReadAllText($p,[System.Text.Encoding]::UTF8), $enc)  # $p 必须绝对路径
```

---

## 4. 架构速览

```
Application(单例) → Window → Layout(ColumnBox/RowBox/GridLayout) → Container → Control
                                    ↓
                         Overlay (Menu / ComboBox 下拉 / ToolTip)  ← 走 DComp 弹层
```

- **`UIElement` 基类**：`Measure`/`Arrange`（**非虚包装器**，子类只实现 `MeasureOverride`/`ArrangeOverride`，`MeasureOverride` 是**纯虚**——漏实现编译失败）、命中测试、事件、离屏缓存、无障碍、调试计数。
- **两级脏位**：`measureDirty_`（自身/子树需重测，**冒泡到根**）/ `selfArrangeDirty_`（自身要重排）/ `subtreeDirty_`（子树有脏，让父的 ArrangeOverride 跑起来到达）。`InvalidateLayout()` 冒泡；`InvalidateLayoutSelf()` 不冒泡（池化/virtual rebind 用）。
- **`LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout }`**：窗口级 chrome（标题栏/菜单栏/状态栏）不参与布局但要绘制。
- **元素身份用窗口 id（不是裸指针）**：窗口销毁后 `GetWindow()` 返回 `nullptr`。
- **并发契约（架构 B，`ZUFYUI_RENDER_THREAD=1`，默认关、实验）**：`renderLock_`（recursive）/ `live_` 生存令牌（`alive`+`inFlight`+`cv`，析构先置死再等 in-flight 归零）/ `gpuLock_`。数据变更 API 走 `RenderGuard()` 与 render 线程串行。
- **`Window`**：`Create` 自动 `RegisterDragDrop`；`OnPaint`/`AdvanceFrame` 驱动布局+动画+合成；`WM_DPICHANGED`/设备丢失重建。

---

## 5. 关键子系统

- **字体**：`FontManager`（单例）缓存 `IDWriteTextFormat`/`IDWriteTextLayout`，**永不淘汰**（`Measure` 缓存安全前提）。`DrawTextWithEllipsis(rt,text,rect,color,FontSpec,brush,fmt=nullptr,forceNoWrap=false,TextHAlign=Left)`。
- **数据视图**：**源行号（stable）** vs **可见序（vi）** 两套坐标；`SortItems` 重排源+清多选，`SortView` 只改可见序；Label **池化复用** + `SetTextFast`（不冒泡不重绘）；`BeginUpdate/EndUpdate` 把 N 次增删从 O(N²) 压到 O(N)；`ChildrenSig()` 是“可见子元素布局”的**单一版本签名**（判据集中一处）。
- **主题**：`ThemeRole` 语义取色 + `Theme`（`Light/Dark/HighContrast`）+ `ThemeManager`；**优先级：实例显式设色 > 窗口主题 > 应用主题**（控件被显式 `Set*Color` 就标 `themeManaged_=false`，主题不再覆盖它）；`UIZSignals::ThemeChanged`（nullptr=应用级）/`ThemeReapply`（切页/切标签后重套可见子树）；跟随系统（注册表 `AppsUseLightTheme`+`DWM\AccentColor`，`WM_SETTINGCHANGE/WM_THEMECHANGED/WM_DWMCOLORIZATIONCOLORCHANGED/WM_SYSCOLORCHANGE`）。
- **拖放**：`Window::BeginDrag` + `DragDataBuilder`；元素 `SetDropTargetEnabled`/`OnDragEnter/Over/Leave/Drop` + 同名信号；跨应用文件拖出靠 `IEnumFORMATETC`（`EnumFormatEtc` 返回 `E_NOTIMPL` 会被资源管理器拒绝）；同控件文本移动用私有格式 `ZufyUI.TextSource` 去重。
- **菜单**：`MenuWindowBase : Window`（`WS_EX_NOACTIVATE|TOOLWINDOW|TOPMOST`，走 DComp + 进程级共享设备）；`Menu::onClosed` 关闭回调（`MenuBar` 借它复位“按下”态）。
- **亚克力/云母**：自实现（`IGraphicsEffect`），Win10/Win11 一致；`SetBackgroundParams`/`AcrylicParams`/`MicaParams`。
- **调试通道**（默认关，`SetDebugEnabled`）：`WM_COPYDATA`，`dwData`=命令号（1~23），回包 `0x5A554631`；命令含 Ping/ListWindows/GetFrameStats/GetElementTree/ForceRepaint/SetElementText/InvokeElement/TopN/Highlight/GetTheme 等；发送侧用 `SendMessageTimeout`（**不加 `SMTO_BLOCK`**，否则目标忙会卡死调试器）。

---

## 6. 坑与经验（踩过的，务必按“对”的做）

### 绘制 / 布局
- `FontManager::GetFormat` 的 format 设了 `PARAGRAPH_ALIGNMENT_CENTER`：**多行文本必须显式 `SetParagraphAlignment(NEAR)`**，否则文本被居中到巨高盒里“看不见”。
- `D2D1::RectF(l,t,r,b)` 的 3/4 参是**右/下**（不是宽/高）；传反（`bottom<top`）直接不画（本轮“设置卡片行空白”就是这么来的）。
- `Snap()` 把绘制吸附物理像素；**用 Snap 后的偏移**算 firstVisible（与 itemRect 统一），否则错位一像素。
- CJK 行高：用**拉丁度量**取行高（拿“中文国”取 max 会把行高撑到 ~2 倍）。
- `SetLineSpacing(UNIFORM,…)` 后 `GetMetrics().height` 可能 ≈2×lh；行高按 `(m.lineCount>0?m.lineCount:1)*lh`。
- **缓存是逐控件的，不是整平**：父缓存只含父自身绘制，**子变化不动父缓存**；`Arrange` 里 `cacheValid_=false` 只由**尺寸变化**驱动（纯位置变化位图照 blit）。
- **默认缓存开**（`useCache_(true)`）→ **动画子元素的缓存要主动失效**（本轮“Expander 箭头不动”就是子元素缓存没失效）：改内容/属性后调 `RequestRepaint`；`RequestRepaint()` 只标脏、**不排帧**，别在标脏处强行出帧。
- 任何内容/size 变化的 setter **必须调 `RequestRepaint`/`InvalidateLayout`**，否则缓存陈旧。
- `childrenDirty_`：**所有改子元素列表的路径都要置脏**（现靠 `SetParent` 覆盖“增”；`ClearChildren`/`PageHost` 动画状态变化另置）。新增移除/替换 API 务必补。
- Windows 宏：`max/min` 是宏（用 `max(...)`，别 `std::max`）；`far` 是宏（用 `farEnd`）。

### 控件 / 窗口
- `ColumnBox`/`RowBox`：`childW/childH = 有显式尺寸用固定，`SetFillWidth/Height` 才铺满`；`ArrangeOverride` 用 `GetDesiredSize()`（不在 Arrange 里再 Measure）。
- `ScrollViewer`：内容按**视口宽度**测量（不是 `FLT_MAX`，否则 FillWidth/换行内容期望宽度无穷大 → “卡片被裁剪”）；内容**两遍测量**（竖条出现后按变窄宽度重量）；**尊重内容元素自身 `margin`**；裁剪留 **1px 出血**（否则内容边缘边框被切半）。
- 图表默认拉伸权值 0：`SetFill*` 在父级有约束时生效，无约束时**回退 420×260**（v1.19.4）。
- `CheckBox` 构造**不要写死 `width_/height_`**（否则带标签在 `RowBox/ColumnBox` 里被当固定宽度、标签被裁）。
- 窗口 `SetMinSize`：默认内容测量会撑出很宽的最小宽度（含长文本/图表），给可用宽度一个下限（600 DIP）。
- `Window::Run()` 返回 `void`；`Window` 无公开 `SetContent`，根内容走 `SetRootLayout(Layout)` 或 `GetRootColumnBox()`。
- 自定义标题栏**不参与布局**（`Window` 放 `(0,0)`，根内容整体下移其高度）；菜单栏/状态栏默认 chrome。
- 隐藏页/被池化的控件**不在 `GetChildren()` 里**：主题等“遍历可见子树”的操作覆盖不到它们 → 用 `UIZSignals::ThemeReapply`（切页时 `PageHost` 触发）懒套用。

### 信号 / 生命周期
- `UIElement::GetChildren()` 返回内部缓冲引用，**遍历期不得对同一元素再次调 `GetChildren()`**。
- `UIElement::Connect(signal, slot)` **不返回** `Connection`（登记在元素 `ConnectionGroup`，随元素析构断开）；需要手动断开才用 `signal.connect(...)`。
- 全局信号 `UIZSignals::*` 都带 `Window*`，订阅者**必须按 `GetWindow()` 过滤**，禁止跨窗口处理。
- `ZSignal::Fire` 的异步/重入分支要用**左值**（`std::apply(slot, tuple)` 右值绑不到 `T&`）。
- 池只增不减（`ListView/TableView` 数据量骤降不收缩），大量反复切换需注意显存。
- `~UIElement` 会从窗口的 `pendingRepaint_` / `lastActiveAnimElements_` / UIA 表**反向注销**——新增“窗口持有裸元素指针”的集合时，别忘了在析构里擦除，否则随机 UAF（本轮修过 `lastActiveAnimElements_`）。

### 平台硬事实
- DComp 桌面窗口目标视觉坐标是**物理像素**（该目标 DPI=96，不随窗口 DPI 走）。
- `CreateSwapChainForComposition` + `FLIP_SEQUENTIAL` 要求 `BufferCount>=2`（写 1 **静默失败** → 窗口全透明）。
- 菜单历史：早期“分层窗口 + 软件渲染目标 + 每菜单自建 D2D 工厂”每弹一次 **~+90MB**；`v1.8.0` 重构（继承 `Window` + DComp + 进程级共享设备）**已根治**。

### DSL（`ZufyUIDsl.h`）
- `with(&T::Setter, args...)` 传**成员函数指针**；继承来的 setter 的指针类型属于**基类**（已用 `template<class C,...>` 处理）；重载 setter 需 `static_cast`；**有默认参数的函数**（如 `ListView::AddItem`）成员指针调用**不支持默认值** → 用 `->AddItem(...)`。
- 短工厂 `Slider/Card/Expander/MenuBar/StatusBar/Page/Menu` 与类同名 → 不要同时 `using namespace ZufyUI;` 和 `using namespace ZufyUI::dsl;`（歧义），或 `dsl::` 限定。
- `GridLayout` 无单参 `AddChild`，`Grid(...)` 用 `(child,row,col)`。

### 主题
- 实例显式设色的控件**不会被主题覆盖**（故意）；想让它跟主题就别手动设色。
- `Backdrop` = 窗口基色（浅白/深黑）；`SetBackdrop/SetBackgroundColor` 显式 tint 后主题不再改它。

---

## 7. Git / 发布流程

- 远端：`origin`=gitee `zhc9968/ZufyUI`；`github`=github `zhc9968/ZufyUI`（github 偶发超时/需 VPN，重试若干次）。
- 发版（用户同意后）：改 `ZufyUI.h` 的 `PATCH`/`STRING` → 更新 `docs/API.md`/`API.en.md`（版本摘要 + 标题 `v… → vX.Y.Z`）+ `README.md`/`README.en.md`（changelog “当前版本”+ 一条）→ `python tools/check_api_parity.py` → 构建 → `git add -A && git commit` → `git tag vX.Y.Z` → `git push origin master` + `git push origin vX.Y.Z` + 同 github。
- **gh-pages 站点**：本地工作副本在 `C:\project\ZufyUI\ghpages`（分支 `gh-pages`；由旧路径 `%TEMP%\opencode\ghpages` 复制而来，**以后只改这里**，旧路径不再使用）。发布时：把 `docs/API*.md`/`README*.md` 拷过去 → `python C:\project\md2html\mdapi2html.py docs/API.md docs/API.html --toc-depth 3 --lang zh`（en 同理，README 同理）→ **手写 `index.html`** 的章节树/badge 版本号按当前章数更新 → commit + `git push origin gh-pages` + `git push github gh-pages`。
- 文档工具（`C:\project\md2html\mdapi2html.py`）：`###chapter:` 分章；`{{ZUFYUI_VERSION}}` 占位符会从附近的 `ZufyUI.h` 自动注入（**版本单一来源**）；`--theme auto` 会透传给页面 JS。

---

## 8. 常见任务怎么做

- **加一个控件**：在 `ZufyUIWidgets.h`（或对应头）加类（继承 `UIElement`，实现 `MeasureOverride`/`ArrangeOverride`/`Draw`）→ 需要就加静态默认色 + 实例 setter → 在 `ApplyThemeToWidgetTree` 里按类型 `dynamic_cast` 套主题（`if (e->IsThemeManaged())`）→ 需要就加 `dsl::` 短工厂 → demo 加演示页。
- **加一个主题角色**：改 `ThemeRole` 枚举 + `Light/Dark/HighContrast` 各加值 + `ThemeManager::Dump()` 与 demo `ThemeSwatch` 的 `names[]` 同步（顺序一致）→ 在控件 applier 用上。
- **改双语文档**：两个 `.md` 同章代码块**行数/顺序一致**，跑 parity 脚本。
- **修 bug**：先写最小复现（demo 某页/调试通道）→ 找根因 → 从根源修 → 构建 demo **和** debugger → 不提交（除非用户同意）。

---

## 9. 已知未做（详见 `TODO.md`）

高优先：`TreeView` 命中几何对齐 `GetFirstColDecor`；`TreeView`/`ListView` 数据 API 补 `RenderGuard`；数据视图 `dataVersion_`；CMake/模板/断言测试；逐控件主题令牌化长尾；图表 `PieChart` 命中角度归一化。
