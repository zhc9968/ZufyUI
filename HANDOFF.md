# ZufyUI 交接文档（HANDOFF）

> 给**下一位接手者 / AI 智能体**：读这一份即可上手。配套：`AGENTS.md`（硬规则）、`TODO.md`（待办）、`docs/API.md`（API 参考）、`docs/API.en.md`（英文）、`README.md`（构建/快速开始）。
> 版本基线：`1.20.0`。**先读「§1 硬规则」和「§6 坑与经验」，再看代码。**
> ⚠️ **有一批工作全部停在「工作区未提交」状态**（13 个文件，+2411/−1590）：见 **§10**。接手前必须先读 §10，否则你会以为代码就是 v1.20.0 的样子。

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
- **父元素先画、子元素后画**（`Window` 按 `GetChildren()` **顺序**递归）。所以「想画在子内容之上」的自绘装饰（覆盖层/角标/按钮）**必须做成一个排在最后的子元素**，不能指望父元素 `Draw` 里画 —— 父里画的会被子内容盖住（本轮 SplitView 的折叠提示与小按钮就踩过）。
- **动画 `deltaTime` 在窗口 tick 里已被钳到 `≤0.033s`**（空闲/最小化/断点恢复后首帧会很大）。自定义动画**不要**再用"指数收敛到阈值"的写法：它要爬好几个时间常数才停，期间每帧重绘 → 用户会觉得"卡"。改用**时长驱动** `detail::EaseOutCubic/EaseInOutCubic`（`u = t/dur`），到点精确结束、不再请求重绘。
- **改 `isOn_`/`value_` 这类状态时，别绕开 setter**：`ToggleSwitch` 的鼠标/键盘路径原来直接写 `isOn_ = !isOn_;` → 漏掉动画启动 → 表现为"瞬间开关"。新增动画就必须保证**所有**改状态的路径都启动动画。
- **`Card::UseCache()` 返回 `HasShadow()`** → 带阴影的卡片会走离屏缓存；放在里面的控件做动画时，要确认缓存会被失效，否则动画看不见。

### 控件 / 窗口
- `ColumnBox`/`RowBox`：`childW/childH = 有显式尺寸用固定，`SetFillWidth/Height` 才铺满`；`ArrangeOverride` 用 `GetDesiredSize()`（不在 Arrange 里再 Measure）。
- `ScrollViewer`：内容按**视口宽度**测量（不是 `FLT_MAX`，否则 FillWidth/换行内容期望宽度无穷大 → “卡片被裁剪”）；内容**两遍测量**（竖条出现后按变窄宽度重量）；**尊重内容元素自身 `margin`**；裁剪留 **1px 出血**（否则内容边缘边框被切半）。
- 图表默认拉伸权值 0：`SetFill*` 在父级有约束时生效，无约束时**回退 420×260**（v1.19.4）。
- `CheckBox` 构造**不要写死 `width_/height_`**（否则带标签在 `RowBox/ColumnBox` 里被当固定宽度、标签被裁）。
- 窗口 `SetMinSize`：默认内容测量会撑出很宽的最小宽度（含长文本/图表），给可用宽度一个下限（600 DIP）。
- `Window::Run()` 返回 `void`；`Window` 无公开 `SetContent`，根内容走 `SetRootLayout(Layout)` 或 `GetRootColumnBox()`。
- 自定义标题栏**不参与布局**（`Window` 放 `(0,0)`，根内容整体下移其高度）；菜单栏/状态栏默认 chrome。
- 隐藏页/被池化的控件**不在 `GetChildren()` 里**：主题等“遍历可见子树”的操作覆盖不到它们 → 用 `UIZSignals::ThemeReapply`（切页时 `PageHost` 触发）懒套用。
- **`ColumnBox`/`RowBox` 会把"没有显式宽度"的子元素按可用宽度摆**（`childW = GetWidth() > 0 ? GetWidth() : availW`），**横向不看 `hWeight`/拉伸权值**。所以「让卡片按内容收窄」不能靠 `.hWeight(0)`，要么给显式宽度，要么**把它放进一个 `Row`**（`Row` 按子元素自然宽度摆）。反过来，`Row` 里的 Row 内容**不理会可用宽度**（返回自然宽）→ 内容过宽时会出现横向滚动。
- **`SettingsList::MeasureOverride`**：`w = (avail.width != FLT_MAX && avail.width > 0) ? avail.width : width_`。**若被给到 `FLT_MAX` 且自己没设宽度，它会返回 0 宽** → 行内文字被压成 0（只剩图标和箭头）。所以**别把 `SettingsList` 直接塞进 `Card` 自带的 `GridLayout`**（网格自动列会传 `FLT_MAX`）；用 `card->SetLayout(Col().add(list))` + `list->SetFillWidth(true)`。
- **`Window` 没有 `Connect` 方法**，挂窗口信号用 `signal.connect(slot)`（`ZSignal` 自带）。
- **`Window` 没有尺寸/DPI 变化信号**（现有信号只有 `BackdropUnsupported/DeviceLost/RenderingError/ThumbButtonClicked/Activated/Deactivated/Closing/Closed`）→ 状态栏之类"实时反映窗口尺寸"的面板只能在激活/切页/切主题时刷新，做不到每次拖边框都变（要实时得先加信号）。
- **`StatusBar::AddPanel` 没有点击回调**（`Panel{text,icon,right}`）→ 状态栏只能"反映状态"，不能"点某一格触发动作"（要做得给 Panel 加 `onClick`）。
- **`WM_MOUSEACTIVATE → MA_NOACTIVATE` 只对弹出层生效**：条件是 `!WantDwmChrome()`，而只有 `MenuWindowBase` 把 `WantDwmChrome()` 覆写成 `false`；`MessageBox` 继承 `true`，走**正常激活**路径。排查"弹窗首次点击穿透"时别一上来就怀疑这条。
- `Window` 的 `WantDwmChrome()` 默认 `true`（系统边框/阴影/圆角）；`Layout`/`LayoutHost`/`Card`(无阴影时)/`Page` 等 `UseCache()` 为 `false`。
- **`ConvergeValue(value,target,eps)` 不产生动画** —— 它只在"已经在 eps 内时吸附"，别拿它当缓动（历史代码里有误导性用法，已清理）。

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
- `Ref::padding()` 需要 `T::SetPadding`：**`ColumnBox`/`ScrollViewer` 没有 `SetPadding`**（用 `.margin(Thickness)`）；`Card` 有 `SetPadding(float)`。
- `Ref::minWidth/maxWidth/width/height` 是布局期望；**`minWidth` 只是给父容器的夹取下限，控件自己不会夹** → 单独对控件 `Measure()` 后断言 `desired >= minWidth` 会失败（要断言就放进容器里）。
- `.with(&T::Method, ...)` 不能用于**有默认参数**的方法（成员指针带全部形参，如 `SettingsList::AddRow`、`ListView::AddItem` → 直接 `->AddRow(...)`）；也不能用于花括号初始化列表（用 `.apply`）。同名 `dsl::` 短工厂既要在**表达式**里限定，也要在**模板类型实参**里限定（如 `std::make_shared<::ZufyUI::dsl::Slider>()`）。

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

---

## 10. 当前未提交的工作（用户反馈批次，基线 `v1.20.0`）

> **状态：全部停在「工作区未提交」，未 commit、未发版、未改版本号。**
> `git diff --stat`：**13 个文件，+2411 / −1590**（`ZufyUI.cpp` 2316→2321 行属正常改动，**不是**换行符翻搅：该文件全 CRLF）。
> 最近一次全量验证：`tools/build.ps1` **exit 0**；DSL 自检 **85/85**（起点 62）；`python tools/check_api_parity.py` OK；`tools/ensure_bom.ps1 -Check` OK。
> **§10.2 是已改的，§10.4 是没解决的 —— 接手请直接从 §10.4 开始。**

### 10.1 两批工作概览

| 批次 | 内容 |
|---|---|
| **A（已完成）** | demo `ZufyUI.cpp` 的 **21 个页面全部改用 `ZufyUIDsl.h` 的 DSL 重排**（`Scroll(fill) → Col(spacing/margin) → 小节标题 + Row/Grid`）；新增**「环境组合」极端情况页**（3 主题 × 3 背景 = 9 组合、DPI/尺寸边界、深层嵌套、控件墙、超长文本/空列表/极值进度/极窄容器/超长下拉项）；DSL 自检 **47 → 62**。顺手修了 3 个缺陷：卡片阴影不跟主题、ComboBox 下拉无阴影、弹窗按钮纵向只显示一半。 |
| **B（本轮 16 项反馈）** | 见 §10.2；DSL 自检 **62 → 85**（新增本批修复的回归断言，见 `tests/dsl_tests.cpp::test_fix_regressions`）。 |

### 10.2 B 批逐项：现象 → 根因 → 修法

| # | 现象 | 根因（已定位） | 修法 / 文件 |
|---|---|---|---|
| ① | Card 悬停闪出蓝色边框 | 主题 applier 给 `Card` 设了 `SetHoverBorderColor(Accent)` —— 卡片本无此行为 | 删掉该调用（悬停仍改背景）。`ZufyUIWidgets.h` 主题套用器 |
| ② | ProgressBar 拉伸后内填充圆角与外框对不上 | `Draw`/裁剪几何/`DrawBorder` 三处用的仍是成员 `height_`（初始高度），只有填充用了 `arrangedRect_.height/2` | 三处统一 `arrangedRect_.height / 2`。`ProgressBar::Draw/DrawBorder` |
| ③ | ToggleSwitch 没缓动 / 后来又"瞬间开关" | (a) 原为线性推进（`animSpeed_=9.6` → 0→1 仅 ~0.10s）；(b) **鼠标点击与键盘路径直接写 `isOn_ = !isOn_;`，绕过了 `SetOn`**，所以新加的"启动动画"从没执行 | 时长驱动 `detail::EaseInOutCubic`（0.25s），并在**三处**改状态的地方都启动动画 |
| ④ | ProgressBar 数值变化没缓动 | 完全没做，`SetValue` 直接跳变 | 新增 `displayValue_` + 时长驱动缓动（0.22s），填充条与 `%` 文字同步；**动画启动放在"值确实变了"之后**（否则空调用也会重启动画→持续重绘） |
| ⑤ | page1 两个勾选框挤在一起 | A 批我误加了 `.with(&CheckBox::SetSize, 20.0f)` → 把**整个控件**（含标签）压成 20px | 去掉该设置 |
| ⑥ | Label 后面有空间却提前省略号 | 回显 Label 沿用了固定 `width(100)` | 改 `minWidth(160)`，宽度跟随文本 |
| ⑦ | 多窗口页标题重复出现 | A 批重排时旧 `title9` 与新卡片标题并存 | 删旧的 |
| ⑧ | 多窗口页按钮文字被折叠 | **`Button::MeasureOverride` 直接返回 `Size(width_,height_)`，而构造函数把 `width_` 设成 `DefaultWidth=120`** → 每个按钮恒定 120px，文案长了必被省略 | 按文案测量撑开 `max(width_, 文案宽 + 2*padding)`；加 `Button::AutoSizeToText`（static，默认 true）与 `SetAutoSizeToText(false)` 留后路。⚠️ **全库布局行为变更** |
| ⑨ | 全部 `MessageBoxW` 换库自带 `MessageBox` | — | 16 处 API 调用，**15 处**改为库 `MessageBox`（封装成匿名命名空间自由函数 `ZuiInfo`，便于内层 lambda 免改捕获）；**刻意保留 1 处**：窗口创建失败路径（此刻框架还没起来）。另给 **9 个无 handler 的按钮**补了点击反馈（禁用/可切换/拖源按钮按语义豁免） |
| ⑩ | 多窗口页改用可折叠卡片 | — | 拆成 **7 张 `Expander` 卡片**；只改**布局尾部**，342 行对象创建与事件处理**一行未动** |
| ⑪ | 顶部菜单栏可交互/多样/与内容联动 | 原先 4 个菜单几乎全是 `ZuiInfo` 占位 | 扩到 **6 个菜单**：视图里含**真改状态**的子菜单（跳转到页面 22 项 / 背景层 / 窗口尺寸 / 帧率上限）+ 勾选项（状态栏、自定义标题栏、可调整大小）；主题菜单含跟随系统/浅色/深色/高对比度/模拟系统 |
| ⑫ | 状态栏可交互/反映窗口状态 | `StatusBar::AddPanel` **没有点击回调** → "可点击"这条路走不通 | 走"反映状态"：**5 个实时面板**（页面/主题/背景/DPI·客户区/窗口状态）；刷新源 = 主题全局钩子 + 页面切换信号 + `Activated/Deactivated/DeviceLost/RenderingError/BackdropUnsupported/ThumbButtonClicked`。刷新逻辑提成匿名命名空间 `DemoStatus` 结构体 |
| ⑬ | 新增首页（设置卡片点击跳转） | — | `navList`/`AddPage` 各**在最前插 1 项**（首页成为索引 0，其余整体 +1，跳转索引逐个核对过）；5 张 `Card` + `SettingsList`，17 行；点行走 `navList->SetSelectedIndex`（复用既有 `SelectionChanged` 连接做跳转与左栏高亮） |
| ⑭ | 整理各页 / 提取重复 / 补极端测试 | 大量复制粘贴 | 抽成 3 个共用助手：`AddSection`（**原来 14 份**）、`ThemeColor`（**7 份**）、`SetScrollablePage`（**14 处** Scroll 包装）→ 全部收敛；测试 62 → **85** |
| ⑮ | SplitView：拖到边缘极限先出"即将折叠"提示（不改内容），松手才折叠，可关闭 | 原无此功能 | 新增 `SetCollapseEnabled/SetCollapseThreshold`；提示判据 = **指针到该侧边缘的距离** ≤ 阈值；覆盖层半透明强调色 + 指向折叠方向的箭头，alpha 渐入渐出；**松手**才折叠 |
| ⑯ | 折叠后该侧显示小突出按钮（圆角浅灰、箭头指展开方向、悬停动画，点击展开到最小状态） | 原无此功能 | 覆盖层里画小按钮（圆角 + 描边 + 箭头 + 悬停缓动）；点击 → `Expand()` → 该侧尺寸带缓动从 0 到**最小极限值**（min；按**被折叠那一侧**换算，右侧用 `(usable - minSecond_)/usable`） |

**⑮⑯ 的实现方式（用户明确要求"画在内容上面"，这是关键）**：`SplitView` 内部有一个嵌套类 `Overlay : public UIElement`，它被作为**最后一个子元素**放进 `childrenView_`。框架按子元素顺序绘制 → 它天然画在两栏内容**之上**，不需要任何"预留条"或层级 API。用户的 5 条要求就是按这个模型落地的。

### 10.2b 本轮追加修复（B 批之后的用户反馈）

| 现象 | 根因 | 修法 |
|---|---|---|
| 饼图 **Ctrl+滚轮不能缩放** | `PieChart` 自己有一个 `zoom_` 成员，**遮蔽**了 `ChartBase::zoom_` → 委派基类只改了基类那份，饼图不读 | `PieChart::OnMouseWheel` 自己处理 Ctrl：`SetZoom(zoom_ * (1+dy*0.12))`；普通滚轮才委派基类 |
| 饼图 **普通滚轮不改变位置** | `ChartBase` 的滚轮平移只改 `scrollX_/scrollY_`，**没重算 `baseX_/baseY_`**（`SyncScrollPositions()` 只在拖动平移路径里调） | 滚轮平移路径补 `SyncScrollPositions()` |
| 页面被撑出横向滚动 / "显示出去了却没有滚动条" | `ColumnBox::ArrangeOverride`：`childW = GetWidth()>0 ? GetWidth() : availW` → **无显式宽度的子元素一律按可用宽摆，横向不看 `hWeight`**；我一度用 `SetHorizontalScrollEnabled(false)` 去压，结果从"显示滚动条"变成"直接裁掉" | 卡片按内容收窄要**包进 `Row`**；`SetHorizontalScrollEnabled(false)` 与"`ScrollViewer` 无条件消费滚轮"**两处已回退**（见 §10.4-4） |
| 首页卡片里**文字全不显示** | `SettingsList` 被放进 `Card` 自带 `GridLayout`，网格自动列给它 `FLT_MAX`，而 `SettingsList::MeasureOverride` 在 `avail.width==FLT_MAX` 时退回 `width_`（=0）→ 行内文字压成 0 宽 | `card->SetLayout(Col().add(标题).add(list))` + `list->SetFillWidth(true)` |
| 开关"卡"（每帧重绘 0.65s） | 我一度用"指数收敛 + 阈值吸附"，要爬 ~6.2τ 才停，期间每帧重绘 | 改时长驱动 ease-out（到点精确结束） |

### 10.3 本两批累计新增/变更的公开 API

- `Button`：`AutoSizeToText`（static，默认 `true`）、`SetAutoSizeToText(bool)`
- `ToggleSwitch`：`DefaultAnimationDuration`（0.25s）
- `ProgressBar`：`DefaultValueAnimDuration`（0.22s）
- `SplitView`：`SetCollapseEnabled/IsCollapseEnabled`、`SetCollapseThreshold/GetCollapseThreshold`、`CollapseFirst/CollapseSecond/Expand`、`IsFirstCollapsed/IsSecondCollapsed/GetCollapsedSide`、`IsCollapsing/GetCollapseProgress`、`SetExpandButtonColors/SetExpandButtonSize`、静态 `SetDefaultColors/SetDefaultExpandButtonColors`、信号 `CollapsedChanged(int)`、`Default*Duration/Threshold/ExpandBtn*` 常量
- `detail::EaseOutCubic(float)` / `detail::EaseInOutCubic(float)`
- `ThemeRole::Shadow`（**追加在枚举末尾**，避免改动已有枚举值）+ Light/Dark/HC 取值
- `Window::SetBackdrop(Backdrop)`（跟随主题）与 `SetBackdrop(Backdrop, DWORD tint)`（锁定）；`ApplyThemeToBackdropParams` 钩子
- `UIElement::DefaultShadowColor` / `SetDefaultShadowColor`
- `ZufyUICharts.h`：`FormalComputeContent` 改虚、`ChartBase::SyncScrollPositions()`
- `tools/ensure_bom.ps1` 的 BOM 守卫修正

### 10.4 未解决 / 需用户拍板（**接手请从这里继续**）

1. **MessageBox 首次点击穿透：未定位**（用户描述：**每次必现**，第一次点击**落到弹窗后面的主窗口**；弹窗的系统标题栏同样）。
   已排除：全库无 `HTTRANSPARENT`；`WM_MOUSEACTIVATE → MA_NOACTIVATE` **只对 `MenuWindowBase`**（`MessageBox` 继承 `WantDwmChrome()==true`，走正常激活）。已加但仍未验证有效：`RunModal` 进入模态循环前 `if (GetCapture()) ReleaseCapture();`。
   **下一步要问用户**：弹窗出现时它的标题栏是不是**激活态外观**？从**菜单**打开弹窗（"帮助 → 关于"）是否正常？——这两个答案能把方向锁死在"激活时序"或"输入路由/捕获"。
2. **SplitView 需目视验收**：用户对上一版（"预留条"方案）明确不满意并要求重写，本轮已按他的 5 条重写（覆盖层做最后一个子元素、画在内容之上）。**行为只能靠用户看**，尤其：提示覆盖层的颜色/透明度/箭头方向、松手后收缩到 0 的缓动、折叠后小按钮的位置与悬停、点击后展开到"最小极限值"（不是折叠前比例）。
3. **page10（窗口属性）的"有空间却省略号 / 不铺满"**：已把 6 处固定 `width(220)` 改成 `minWidth`，参数卡内层网格加 `SetColumnStretch(1,1)`，两个下拉框 `fillWidth(true)`。**若仍省略**，说明是父级 `Row` 把子元素压到自然宽以下 → 下一步把那几行改成 `Grid`（标签列按内容 + 控件列拉伸）。
4. **`ScrollViewer` 滚轮归属**：用户提过"鼠标放在一个滚动上，无论如何都只滚动这一个"。我实现成"允许该轴滚动就无条件消费" → 用户**测不出变化**，且它把外层页面滚动也堵住了 → **已回退**。要重做必须先跟用户确认场景（哪一层滚动容器、期望内层不能滚时是否真的禁止外层滚）。
5. **需产品决策（我没擅自动 API）**：
   - `StatusBar::AddPanel` 加 `onClick`（否则状态栏永远不能"点击触发动作"）；
   - `Window` 加尺寸/DPI 变化信号（否则状态栏 DPI/客户区格无法实时刷新）；
   - 9 个与类同名的 `dsl::` 短工厂是否加显式别名（现在必须在表达式与模板实参里 `::ZufyUI::dsl::` 限定）；
   - 是否给 `ProgressRing` 加 `Ring(float size)` 短工厂；
   - `PieChart::zoom_` 遮蔽 `ChartBase::zoom_` 是**代码味道**（建议后续统一成一个成员 + 统一 0.4~4 的缩放区间，当前基类钳 0.4~3、饼图 `SetZoom` 钳 0.4~4）。
6. **A 批评审报告里"真问题"但本轮未修的**（用户当时只让修确认过的）：`N3` TextBox undo 栈污染且无上限、`N4` TreeView 勾选命中 vs `GetFirstColDecor`、`N7` `IconFormat` DPI 缓存、`N9` MessageBox HWND owner 时序、`N10` `SetAppIcon`/`RegisterApp` HICON 泄漏、`N12` RenderHost 设备丢失时丢批次、`N13/N14` TreeView/TableView 虚拟化、`N16` ComboBox 过滤 O(N)。

### 10.5 提交/发版前的建议顺序

1. 先让用户**目视验收 §10.4 的 1~3**（尤其 SplitView 与弹窗穿透）；
2. 跑 `tools/build.ps1`（含 DSL 自检）+ `python tools/check_api_parity.py` + `tools/ensure_bom.ps1 -Check`；
3. 再走 §7 的发版流程（版本号、`docs/API*.md`、`README*.md` changelog）；
4. **必须获得用户明确同意**才 `git commit` / `git push`（含 gh-pages）。

> 注：本轮**没有**更新 `TODO.md`（§9 指向它）；如果要继续，把 §10.4 的条目搬进 `TODO.md` 更符合本仓库约定。
