# ZufyUI 易错点总表（Pitfalls）

> 这份文件把散落在代码注释、`HANDOFF.md`、`TODO.md` 里的“坑”集中到一处。**接手/使用前先扫一遍**。
> 优先级：`易错` = 违反必踩；`注意` = 契约/限制，违反易出问题；`约定` = 库的取向；`不要` = 明确不做。

---

## 1. 链式构造 DSL（`ZufyUIDsl.h`）

| # | 状态 | 说明 |
|---|---|---|
| 1.1 | 易错 | **`.with(&T::Method, args...)` 只适用于“无默认参数、无重载、非初始化列表”的成员函数**。带默认参数的成员函数，其成员指针类型包含全部形参 → `.with(&X::F, a)` 会报“参数太少”。这类函数已拆成**无默认参数的重载**（见下），直接单/多参调用即可；实在不行用 `.apply([](X& x){ x.F(...); })`。 |
| 1.2 | 易错 | 已拆分的历史函数（P0-1）：`TreeView::SetCheckable(bool)` + `SetCheckableShallow(bool)`；`SetNodeCheckable(node,bool)` + `SetNodeCheckableRecursive`；`SetNodeEnabled(node,bool)` + `SetNodeEnabledRecursive`；`SetNodeCheckState(node,state)` + `SetNodeCheckStateFull(...,updateChildren,updateParent)`；`ListView::AddItem(text)` 与 `AddItemWidget(shared_ptr<Label>)`；`RadioGroup::AddItem(text)` / `AddItemSelected(text,bool)` / `AddButton(rb)` / `AddButtonSelected(rb,bool)`；`Menu::AddCheckItem(text,checked)` / `AddCheckItemCb(text,checked,onToggle)` / `AddCheckItemRadio(...)`。 |
| 1.3 | 约定 | DSL 短工厂统一 `Z` 前缀（`ZSlider`/`ZCard`/`ZPage`/`ZMenu`…），同时 `using namespace ZufyUI;` + `using namespace ZufyUI::dsl;` 不再和类名冲突。 |
| 1.4 | 约定 | 短工厂作模板实参时写 `::ZufyUI::dsl::ZSlider` 等限定名。 |
| 1.5 | 约定 | `GridLayout` 无单参 `AddChild`；用 `.cell(row,col,child)` 或 `ZGrid(ZCell(r,c,child), ...)`。 |
| 1.6 | 易错 | `.with(&T::Method, ...)` 里方法有重载时需 `static_cast` 消歧。 |
| 1.7 | 约定 | `Ref::minWidth/maxWidth` 是**给父容器的夹取下限**，控件自身不夹；单独对控件 `Measure()` 后断言 `desired >= minWidth` 会失败（要断言就放进容器）。 |
| 1.8 | 约定 | `Ref::padding()` 需要 `T::SetPadding`；`ColumnBox`/`ScrollViewer` 没有 `SetPadding`（用 `.margin`）；`Card` 有。 |
| 1.9 | 易错 | `Ref<T>()` 是**空句柄，不是工厂**：不会创建控件；`ZRow/ZCol` 会忽略它，`->` 得到空指针。普通 UI 用 `ZTxt()` / `ZLbl()` / `ZBtn()` / `ZMake<T>()`；空 Ref 只用于“类成员先声明、后赋值”或显式可空引用。 |
| 1.10 | 易错 | 事件绑定是信号槽：DSL 用 `.on<&Button::Clicked>(...)`，核心用 `btn->Connect(btn->Clicked, ...)`。不存在 `Button::OnClicked`、`ToggleSwitch::OnToggled` 或链式 `.On(...)`；`Button::Clicked` 只作为成员指针时合法。 |
| 1.11 | 易错 | `WindowApp` 的链式方法返回 `WindowApp&`，且拷贝构造被删除：`auto app = ZApp().title(...)` **编译失败**。直接链到 `.run()`，或先 `WindowApp app = ZApp();` 再分步调用；不要写 `app = app.title(...)`。 |
| 1.12 | 易错 | DSL 命名空间是小写 `ZufyUI::dsl`，短工厂是 `ZCol/ZRow/ZLbl/ZBtn/ZTxt`，不是 `ZufyUI::Dsl` / `Col` / `Row`。同时 `using namespace ZufyUI;` 和 `using namespace ZufyUI::dsl;` 是允许的。 |
| 1.13 | 约定 | 常用 API 名不要凭 Qt 猜：占位符是 `SetPlaceholder` / `.placeholder()`，不是 `SetPlaceholderText`；`ZToggle(bool)` 只接收开关状态，不带文字标签（标签另用 `ZLbl`）。 |

## 2. 布局 / 测量

| # | 状态 | 说明 |
|---|---|---|
| 2.1 | 易错 | `ColumnBox`/`RowBox` 里**没有显式宽度的子元素按可用宽摆**（横向不吃拉伸权值）。“让卡片按内容收窄”不能靠 `hWeight(0)`，要放进 `Row` 或给显式宽。 |
| 2.2 | 易错 | `Row` 里的内容**不理会可用宽**（返回自然宽）→ 内容过宽时出现横向滚动。 |
| 2.3 | 约定 | `SettingsList` 在无父约束且自身未设宽时回退到 `DefaultWidth`（240），不会再把 `Card` 的自动列压成 0；要横向填满仍可用 `SetFillWidth(true)`。 |
| 2.4 | 易错 | `ScrollViewer` 内容按**视口宽**测、**两遍测**（竖条出现后按变窄宽度重测）、尊重内容自身 `margin`、裁剪留 **1px 出血**。 |
| 2.5 | 易错 | `D2D1::RectF(l,t,r,b)` 的 3/4 参是**右/下**（不是宽/高）；传反直接不画。 |
| 2.6 | 约定 | `Measure`/`Arrange` 是**非虚包装器**；子类只实现 `MeasureOverride`（**纯虚**，漏实现编译失败）/`ArrangeOverride`。Arrange 里用 `GetDesiredSize()`，别在 Arrange 里再 Measure。 |
| 2.7 | 易错 | `GridLayout` 的行/列数现在由 `SetColumnCount/SetRowCount` 或 `SetColumnStretch/SetRowStretch` 明确建立；空轨道仍然存在，**不要再用 Label/ColumnBox 占位**。`SetColumnStretch(2, ...)` 会建立第 3 列。 |
| 2.8 | 约定 | `GridLayout::SetCellAlignment` 管“子元素在 cell 内”；`SetHorizontalAlignment/SetVerticalAlignment` 管“整个 Grid 内容在剩余空间里”。两者不是一回事。 |
| 2.9 | 约定 | `ColumnBox/RowBox::SetJustify` 分配主轴剩余空间；`SetCrossAlign` 默认 `Stretch` 保持旧行为，设成 `Center/Start/End` 时按子元素 desiredSize 摆放，不再强制拉满。 |
| 2.10 | 约定 | `Page/Card::SetContentAlignment` 默认 `Stretch`；需要整页内容居中时设 `Center/Center`，不要再用外层 Grid 占位。 |
| 2.11 | 约定 | `UIElement::Arrange` 只在最终矩形小于 `desiredSize`（父级压缩）时重测；容器不要在对齐后的 Arrange 里再次 Measure 子元素。 |
| 2.12 | 约定 | 两级脏位：`InvalidateLayout()` **冒泡到根**；`InvalidateLayoutSelf()` 不冒泡（池化/virtual rebind 用）。 |

## 3. 绘制 / 缓存

| # | 状态 | 说明 |
|---|---|---|
| 3.1 | 易错 | `FontManager::GetFormat` 的 format 是 `PARAGRAPH_ALIGNMENT_CENTER`：**多行文本必须显式 `SetParagraphAlignment(NEAR)`**，否则被居中到巨高盒“看不见”。 |
| 3.2 | 易错 | **缓存是逐控件的，不是整平**：父缓存只含父自身绘制，子变化不动父缓存。 |
| 3.3 | 易错 | `Arrange` 里 `cacheValid_=false` **只由尺寸变化驱动**（纯位置变化位图照 blit）。 |
| 3.4 | 易错 | 默认缓存开 → 动画子元素的缓存要主动失效（改内容/属性后 `RequestRepaint`）；`RequestRepaint()` **只标脏、不排帧**。 |
| 3.5 | 易错 | 任何内容/size 变化的 setter 必须调 `RequestRepaint`/`InvalidateLayout`；改子元素列表的路径都要置 `childrenDirty_`。 |
| 3.6 | 易错 | **父元素先画、子元素后画**（按 `GetChildren()` 顺序）→ “画在内容之上”的覆盖层必须做成**排在最后的子元素**（如 `SplitView::Overlay`）。 |
| 3.7 | 约定 | CJK 行高用**拉丁度量**（拿“中文国”取 max 会把行高撑到 ~2 倍）。`Snap()` 后偏移算 firstVisible。 |
| 3.8 | 约定 | Windows 宏：`max/min` 是宏（用 `max(...)`，别 `std::max`）；`far` 是宏（用 `farEnd`）。 |
| 3.9 | 约定 | `Label()` 默认文本已改为空字符串；不要再用无参 Label 当占位/Spacer，布局空白请用空容器或专用 Spacer。 |

## 4. 控件 / 窗口

| # | 状态 | 说明 |
|---|---|---|
| 4.1 | 易错 | `Window::Run()` 返回 `void`；无公开 `SetContent`，根内容走 `SetRootLayout(Layout)` 或 `GetRootColumnBox()`。 |
| 4.2 | 易错 | `Window` 现在继承 `ConnectionOwner`，有 `Connect`；仍**没有尺寸/DPI 变化信号**。成员函数指针用自由 `Connect` / `ConnectWeak`。 |
| 4.3 | 易错 | 自定义标题栏**不参与布局**；菜单栏/状态栏默认 chrome。 |
| 4.4 | 易错 | 隐藏页/池化控件**不在 `GetChildren()`**；主题等“遍历可见子树”覆盖不到 → 用 `UIZSignals::ThemeReapply`（切页触发）懒套用。 |
| 4.5 | 易错 | **动画 `deltaTime` 已被钳到 ≤0.033s**；自定义动画别用“指数收敛到阈值”（要爬 ~6τ），用**时长驱动** `detail::EaseOutCubic/EaseInOutCubic`（到点精确结束）。 |
| 4.6 | 易错 | 改 `isOn_`/`value_` 这类状态**别绕开 setter**（会漏掉动画启动/信号/上报）。 |
| 4.7 | 易错 | **模态框要在 `RunModal` 里先 `ReleaseCapture()`**：若模态是在 `Window::OnMouseUp` 的槽里弹出，函数末尾的 `ReleaseCapture()` 要等模态结束才执行 → 整个模态期间主窗口持捕获 → 弹窗**首次点击穿透**。（已在 `RunModal` 内处理。） |
| 4.8 | 约定 | `Button::AutoSizeToText`（static，默认 true）：按钮按文案撑开；要固定宽用 `SetAutoSizeToText(false)`。 |
| 4.9 | 约定 | 图表默认拉伸权值 0：`SetFill*` 有约束时生效、无约束回退 420×260。 |
| 4.10 | 易错 | `AddKeyHandler` 每次按键先复制 handler 快照；回调里 `RemoveKeyHandler` 不取消当前轮，刚移除的 handler 仍可能执行一次。返回 true 会吞键；handler 捕获裸 `this` 时必须在宿主析构前移除（`Drawer` 已自动做）。 |
| 4.11 | 约定 | `Window` **没有尺寸/DPI 变化信号**；StatusBar 等依赖实时客户区的面板只能在 Activated/主题/切页时刷新，拖边框不会自动更新。 |
| 4.12 | 约定 | `StatusBar::AddPanel` 只有文本/图标，**没有点击回调**；要点某一段触发动作请用按钮/自定义行。 |
| 4.13 | 注意 | 任务栏 COM 调用失败**静默 no-op**；`FLASHW_CAPTION` 对自绘标题栏无效，要连任务栏一起闪用 `FLASHW_ALL`。 |

## 5. 信号 / 生命周期

| # | 状态 | 说明 |
|---|---|---|
| 5.1 | 易错 | `GetChildren()` 返回内部缓冲引用，**遍历期不得对同一元素再次调 `GetChildren()`**。 |
| 5.2 | 易错 | `Connect(signal, slot)` 返回 `Connection`；连接随**接收者**析构断开。可忽略返回值；手动断开用 `Connection::disconnect()`。 |
| 5.3 | 易错 | 全局 `UIZSignals::*` 带 `Window*`，订阅者**必须按 `GetWindow()` 过滤**。 |
| 5.4 | 易错 | `ZSignal::Fire` 的异步/重入分支要用**左值**（`std::apply(slot, tuple)` 右值绑不到 `T&`）；信号参数不要写 `T&&`。 |
| 5.5 | 易错 | `~UIElement` 会从窗口的 `pendingRepaint_`/`lastActiveAnimElements_`/UIA 表**反向注销**；新增“窗口持裸元素指针”的集合时，析构里必须擦除，否则随机 UAF。 |
| 5.6 | 约定 | 裸 `this` 捕获安全的前提是连接登记在**正确的接收者** `ConnectionGroup`；直接 `signal.connect(slot)` 不自动断开。 |
| 5.7 | 易错 | 池只增不减（`ListView/TableView` 数据骤降不收缩）。 |
| 5.8 | 约定 | 成员函数指针用自由 `Connect(receiver, signal, &Class::Method)` 或 `ConnectWeak`；`elem->Connect(sig, &Class::Method)` 已不支持。 |
| 5.9 | 易错 | `disconnect()` 只逻辑取消**已排队**的 `UIThread`/`NewThread` 任务；**不中断已开始执行**的槽。 |
| 5.10 | 易错 | `NewThread` 槽里不能碰 UI；裸 `this` + 接收者可能死亡 = UAF。用 `ConnectWeak` 保活，改 UI 用 `UIThread`。 |
| 5.11 | 易错 | `Fire` 与 `ZSignal` 析构并发是 **UB**；`ZSignal` 不可拷贝/移动，不要放进可移动容器。 |
| 5.12 | 易错 | `Connection` **不是 RAII 守卫**；临时句柄析构不会断连，必须显式 `disconnect()` 或依赖接收者 group。 |
| 5.13 | 易错 | `Fire` 默认应在 UI 线程；后台 Fire + `CurrentThread` 槽会直接在后台执行 UI 槽，属于调用方违约。 |
| 5.14 | 易错 | 槽抛异常会直接穿出 `Fire`，后续槽不再执行；信号槽内不建议抛异常。 |
| 5.15 | 约定 | `NewThread` 每次 Fire 起一个 detached 线程，不适合高频信号；高频用 `UIThread` 或线程池。 |
| 5.16 | 易错 | `ListView/TableView/TreeView` 行高必须 > 0。`ListView::SetItemHeight` 会夹到 ≥1，但 `TableView::SetRowHeight` / `TreeView::SetRowHeight` **不夹**；传 0 会破坏行高计算/命中测试。`SetRowHeightAt` 夹到 ≥8。 |
| 5.17 | 易错 | `SetFilter` / `SetViewComparator` 把 `std::function` 存在控件里；按引用捕获外部变量会悬空。虚拟模式比较器每次调 `GetItemText`，大表先快照文本；批量改动用 `BeginUpdate/EndUpdate`。 |
| 5.18 | 易错 | `TreeView::MoveNode` 不能移到自己的后代（返回 `false`，树不变）；忽略返回值会误以为成功。 |

## 6. 主题

| # | 状态 | 说明 |
|---|---|---|
| 6.1 | 约定 | **实例显式设色优先**：控件被 `Set*Color` 后标 `themeManaged_=false`，主题不再覆盖它。想跟主题就别手动设色。 |
| 6.2 | 约定 | `ThemeManager::RegisterHook/RegisterSubtreeApplier` 返回 **RAII `HookHandle`**：析构自动断开。要长期有效就**持有**它（库内部用 `RegisterHookPermanent`）。 |
| 6.3 | 约定 | `Backdrop` = 窗口基色；`SetBackdrop(Backdrop)` 跟随主题、`SetBackdrop(Backdrop,tint)` 锁定。 |

## 7. 平台 / 工程

| # | 状态 | 说明 |
|---|---|---|
| 7.1 | 易错 | 含中文的 `.h/.cpp` **必须 UTF-8 带 BOM**（否则 MSVC 按 GBK 解析报一堆 `C2143/C2259/C2888`）；`.md/.tex` 无 BOM；`tools/*.ps1` 故意纯 ASCII 无 BOM。 |
| 7.2 | 约定 | DComp 桌面窗口目标视觉坐标是**物理像素**（DPI=96）。 |
| 7.3 | 约定 | `CreateSwapChainForComposition` + `FLIP_SEQUENTIAL` 要求 `BufferCount>=2`（写 1 静默失败→全透明）。 |
| 7.4 | 约定 | `Measure` 缓存安全前提：`FontManager::formatCache_` 永不淘汰。 |

## 8. Drawer / 浮层（v1.25.0）

| # | 状态 | 说明 |
|---|---|---|
| 8.1 | 易错 | `SetContent` 是主内容（会被遮住/让位），`SetPanel` 才是抽屉面板。面板内容塞进 `SetContent` 不会得到面板背景、内边距、圆角和阴影。 |
| 8.2 | 易错 | `SetPanelSize` 是面板**外框**尺寸；默认 16px 内边距先从面板内容扣除（外宽 200 → 内容 168）。需要更宽内容时调大外框或减小 `SetPanelPadding`。 |
| 8.3 | 易错 | `Drawer` 是布局控件，只覆盖自己的 arranged rect（根布局或 `GridLayout` cell），**不覆盖**自定义标题栏/菜单栏；要压暗整个内容区就放在该区域最外层。 |
| 8.4 | 易错 | `Open()` 立即改目标状态并触发 `OpenChanged`；`Opened/Closed` 等动画结束才触发。`IsOpen()` 返回目标状态，不是进度；动画中用 `IsAnimating()/GetOpenProgress()`。 |
| 8.5 | 易错 | `Permanent` 强制打开、进度 1、无动画/遮罩，`Close()/SetOpen(false)` 无效；从 Permanent 切回 Overlay/Push **不会自动关闭**，要再调 `Close()`。 |
| 8.6 | 易错 | Scrim 只在 Overlay 存在；`SetDismissOnScrimClick`/`ScrimClicked` 仅在 `SetModal(true)` 且进度 >0 时生效。`SetModal(false)` 后点击落到内容上，`ScrimClicked` 不触发。 |
| 8.7 | 易错 | `SetScrimOpacity(0)` 只把遮罩画成透明，**模态命中仍拦截点击**；要穿透必须 `SetModal(false)`。 |
| 8.8 | 约定 | `SetContentBorder` 只在 Overlay 绘制；Push/Permanent 没有内容边框。`SetPanelShadow` 的内层被不透明背景覆盖，只有外侧阴影可见，边框最后画。 |
| 8.9 | 易错 | Esc 拦截和 `SetFocusOnOpen` 要求 `Drawer` 已挂到 `Window`；未 attach 时不会注册 Esc，也不会执行聚焦。 |

## 9. 窗口边框 / DWM / 渲染驱动（v1.27.0）

| # | 状态 | 说明 |
|---|---|---|
| 9.1 | 易错 | Win10 自定义标题栏**不要设置** `DWMWA_WINDOW_CORNER_PREFERENCE`，也不要用 `SetWindowRgn` 裁圆角；会丢 DWM 边框/阴影，`SetWindowRgn` 还会产生硬裁剪锯齿。 |
| 9.2 | 约定 | Win10 和 Win11 的自定义边框都保留 `DwmExtendFrameIntoClientArea({1,1,1,1})`；Win11 在此之外仍由 DWM 处理圆角。 |
| 9.3 | 易错 | Snap 会重新计算 DWM frame；必须在 `WM_SIZE`、`WM_WINDOWPOSCHANGED` 和背景重载后重新挂 frame 状态，否则 Snap 1/2、1/4 会丢边框/阴影。 |
| 9.4 | 破坏性 | `WindowCorner` / `SetWindowCorner()` / `GetWindowCorner()` / DSL `corner()` 已删除；需要自定义圆角请自行调用 Win32 DWM API，并承担 Win10/Snap 风险。 |
| 9.5 | 易错 | `SetRenderDriver()` 必须在第一次 `Window::Create` / D3D 设备创建之前调用；设备已创建后不会热切换。 |
| 9.6 | 约定 | `Auto` = 硬件优先、失败回退 WARP；WARP 只改变应用自己的 D3D11/D2D 内容渲染，WinRT Compositor、DWM 合成和 Acrylic 效果仍可能使用 GPU；“省内存”不等于“零 GPU”。 |
| 9.7 | 易错 | 任务管理器的“内存”列不是 `PrivateMemorySize64`；比较 WARP/硬件时应同时看 Private Bytes、Working Set、Commit Size，并在 Win10/Win11 分开记录。 |
| 9.8 | 易错 | `DWMWA_VISIBLE_FRAME_BORDER_THICKNESS` 只支持 Win11 22000+；Win10 上应使用 `DWMWA_EXTENDED_FRAME_BOUNDS` + `GetWindowRect/GetClientRect` 诊断 DWM 几何。 |

## 10. 明确不做（不要提这些 feature request）

1. **页级“整平”缓存**（子树合成到一张位图）——单控件动画会整张重画、内存巨大、收益低。
2. **事件驱动 `CollectActiveAnimations`**（pull→push）——“是否在动”是派生量，条件触发的 bug 极难测，收益仅 1%~3%。
3. **合并 `UpdateAnimation` + `CollectActiveAnimations`**——容器 `UpdateAnimation` 自带递归，外部再遍历子元素会被调两次。
4. **数据控件里的 Label 走离屏缓存**——项多/滚动频繁，每项一张 GPU 位图是负优化。
5. **数据绑定 / MVVM**——信号槽已覆盖 90%，抽象税太高。
6. **NavigationView 做成控件**——做成示例即可，别把导航策略焊进库。
7. **放弃纯头文件**——单/少 TU 编译快；大规模 TU 会撞编译墙，属已知取舍。
