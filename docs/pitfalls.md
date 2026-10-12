# ZufyUI 易错点总表（Pitfalls）

> 这份文件把散落在代码注释、`HANDOFF.md`、`TODO.md` 里的“坑”集中到一处。**接手/使用前先扫一遍**。
> 优先级：`易错` = 违反必踩；`约定` = 库的取向；`不要` = 明确不做。

---

## 1. 链式构造 DSL（`ZufyUIDsl.h`）

| # | 状态 | 说明 |
|---|---|---|
| 1.1 | 易错 | **`.with(&T::Method, args...)` 只适用于“无默认参数、无重载、非初始化列表”的成员函数**。带默认参数的成员函数，其成员指针类型包含全部形参 → `.with(&X::F, a)` 会报“参数太少”。这类函数已拆成**无默认参数的重载**（见下），直接单/多参调用即可；实在不行用 `.apply([](X& x){ x.F(...); })`。 |
| 1.2 | 易错 | 已拆分的历史函数（P0-1）：`TreeView::SetCheckable(bool)` + `SetCheckableShallow(bool)`；`SetNodeCheckable(node,bool)` + `SetNodeCheckableRecursive`；`SetNodeEnabled(node,bool)` + `SetNodeEnabledRecursive`；`SetNodeCheckState(node,state)` + `SetNodeCheckStateFull(...,updateChildren,updateParent)`；`ListView::AddItem(text)` 与 `AddItemWidget(shared_ptr<Label>)`；`RadioGroup::AddItem(text)` / `AddItemSelected(text,bool)` / `AddButton(rb)` / `AddButtonSelected(rb,bool)`；`Menu::AddCheckItem(text,checked)` / `AddCheckItemCb(text,checked,onToggle)` / `AddCheckItemRadio(...)`。 |
| 1.3 | 易错 | **与类同名的短工厂**（`Slider`/`Card`/`Page`/`Menu`/`MenuBar`/`StatusBar`/`RadioGroup`/`ScrollBar`/`Expander`）在同时 `using namespace ZufyUI;` + `using namespace ZufyUI::dsl;` 时触发 **C2872 二义**。用无歧义别名 `make_slider` / `make_card` / `make_page` / `make_menu` / `make_menu_bar` / `make_status_bar` / `make_radio_group` / `make_scroll_bar` / `make_expander`。 |
| 1.4 | 易错 | 短工厂既作**表达式**又要作**模板类型实参**时都要 `::ZufyUI::dsl::` 限定（如 `std::make_shared<::ZufyUI::dsl::Slider>()`）。 |
| 1.5 | 约定 | `GridLayout` 无单参 `AddChild`；用 `.cell(row,col,child)` 或 `Grid(Cell(r,c,child), ...)`。 |
| 1.6 | 易错 | `.with(&T::Method, ...)` 里方法有重载时需 `static_cast` 消歧。 |
| 1.7 | 约定 | `Ref::minWidth/maxWidth` 是**给父容器的夹取下限**，控件自身不夹；单独对控件 `Measure()` 后断言 `desired >= minWidth` 会失败（要断言就放进容器）。 |
| 1.8 | 约定 | `Ref::padding()` 需要 `T::SetPadding`；`ColumnBox`/`ScrollViewer` 没有 `SetPadding`（用 `.margin`）；`Card` 有。 |

## 2. 布局 / 测量

| # | 状态 | 说明 |
|---|---|---|
| 2.1 | 易错 | `ColumnBox`/`RowBox` 里**没有显式宽度的子元素按可用宽摆**（横向不吃拉伸权值）。“让卡片按内容收窄”不能靠 `hWeight(0)`，要放进 `Row` 或给显式宽。 |
| 2.2 | 易错 | `Row` 里的内容**不理会可用宽**（返回自然宽）→ 内容过宽时出现横向滚动。 |
| 2.3 | 易错 | `SettingsList::MeasureOverride` 被给 `FLT_MAX` 且自己无宽度时返回 0 宽（文字压成 0）。别把它直接塞进 `Card` 自带的 `GridLayout`（网格自动列传 `FLT_MAX`）；用 `card->SetLayout(Col().add(list))` + `list->SetFillWidth(true)`。 |
| 2.4 | 易错 | `ScrollViewer` 内容按**视口宽**测、**两遍测**（竖条出现后按变窄宽度重测）、尊重内容自身 `margin`、裁剪留 **1px 出血**。 |
| 2.5 | 易错 | `D2D1::RectF(l,t,r,b)` 的 3/4 参是**右/下**（不是宽/高）；传反直接不画。 |
| 2.6 | 约定 | `Measure`/`Arrange` 是**非虚包装器**；子类只实现 `MeasureOverride`（**纯虚**，漏实现编译失败）/`ArrangeOverride`。Arrange 里用 `GetDesiredSize()`，别在 Arrange 里再 Measure。 |
| 2.7 | 约定 | 两级脏位：`InvalidateLayout()` **冒泡到根**；`InvalidateLayoutSelf()` 不冒泡（池化/virtual rebind 用）。 |

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

## 4. 控件 / 窗口

| # | 状态 | 说明 |
|---|---|---|
| 4.1 | 易错 | `Window::Run()` 返回 `void`；无公开 `SetContent`，根内容走 `SetRootLayout(Layout)` 或 `GetRootColumnBox()`。 |
| 4.2 | 易错 | `Window` **没有 `Connect` 方法**、**没有尺寸/DPI 变化信号**；挂窗口信号用 `signal.connect(slot)`。 |
| 4.3 | 易错 | 自定义标题栏**不参与布局**；菜单栏/状态栏默认 chrome。 |
| 4.4 | 易错 | 隐藏页/池化控件**不在 `GetChildren()`**；主题等“遍历可见子树”覆盖不到 → 用 `UIZSignals::ThemeReapply`（切页触发）懒套用。 |
| 4.5 | 易错 | **动画 `deltaTime` 已被钳到 ≤0.033s**；自定义动画别用“指数收敛到阈值”（要爬 ~6τ），用**时长驱动** `detail::EaseOutCubic/EaseInOutCubic`（到点精确结束）。 |
| 4.6 | 易错 | 改 `isOn_`/`value_` 这类状态**别绕开 setter**（会漏掉动画启动/信号/上报）。 |
| 4.7 | 易错 | **模态框要在 `RunModal` 里先 `ReleaseCapture()`**：若模态是在 `Window::OnMouseUp` 的槽里弹出，函数末尾的 `ReleaseCapture()` 要等模态结束才执行 → 整个模态期间主窗口持捕获 → 弹窗**首次点击穿透**。（已在 `RunModal` 内处理。） |
| 4.8 | 约定 | `Button::AutoSizeToText`（static，默认 true）：按钮按文案撑开；要固定宽用 `SetAutoSizeToText(false)`。 |
| 4.9 | 约定 | 图表默认拉伸权值 0：`SetFill*` 有约束时生效、无约束回退 420×260。 |

## 5. 信号 / 生命周期

| # | 状态 | 说明 |
|---|---|---|
| 5.1 | 易错 | `GetChildren()` 返回内部缓冲引用，**遍历期不得对同一元素再次调 `GetChildren()`**。 |
| 5.2 | 易错 | `Connect(signal, slot)` **不返回** `Connection`；需要手动断开才用 `signal.connect(...)`。 |
| 5.3 | 易错 | 全局 `UIZSignals::*` 带 `Window*`，订阅者**必须按 `GetWindow()` 过滤**。 |
| 5.4 | 易错 | `ZSignal::Fire` 的异步/重入分支要用**左值**（`std::apply(slot, tuple)` 右值绑不到 `T&`）。 |
| 5.5 | 易错 | `~UIElement` 会从窗口的 `pendingRepaint_`/`lastActiveAnimElements_`/UIA 表**反向注销**；新增“窗口持裸元素指针”的集合时，析构里必须擦除，否则随机 UAF。 |
| 5.6 | 约定 | 回调捕获**裸指针**（而不是 `shared_ptr`）可避免“对象→回调→shared_ptr→对象”引用环；连接随元素析构自动断开，裸指针安全。 |
| 5.7 | 易错 | 池只增不减（`ListView/TableView` 数据骤降不收缩）。 |

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

## 8. 明确不做（不要提这些 feature request）

1. **页级“整平”缓存**（子树合成到一张位图）——单控件动画会整张重画、内存巨大、收益低。
2. **事件驱动 `CollectActiveAnimations`**（pull→push）——“是否在动”是派生量，条件触发的 bug 极难测，收益仅 1%~3%。
3. **合并 `UpdateAnimation` + `CollectActiveAnimations`**——容器 `UpdateAnimation` 自带递归，外部再遍历子元素会被调两次。
4. **数据控件里的 Label 走离屏缓存**——项多/滚动频繁，每项一张 GPU 位图是负优化。
5. **数据绑定 / MVVM**——信号槽已覆盖 90%，抽象税太高。
6. **NavigationView 做成控件**——做成示例即可，别把导航策略焊进库。
7. **放弃纯头文件**——单/少 TU 编译快；大规模 TU 会撞编译墙，属已知取舍。
