# ZufyUI 待办清单（TODO）

> **单一权威待办清单**。合并自已删除的 `FEATURE_PLAN.md` / `OPTIMIZATION_PLAN.md`，汇总历次代码/文档审计中**尚未处理**的项，并收录 `HANDOFF.md §10.4` 的未决项。
> 级别：🔴 重要 / 🟡 中等 / 🟢 轻微 / ⛔ 明确不做（保留结论，避免反复讨论）。
> 版本基线：`1.23.0`。完成一项就删一行（或标 ✅）。

---

## A. 功能补齐（尚未做）

| 级别 | 项 | 要点 | 落点 |
|---|---|---|---|
| 🟡 | **Command 命令系统** | `Command{ Execute/CanExecute/CanExecuteChanged, text, icon, hotkey }`；Button/MenuItem/快捷键绑同一 Command，`enabled` 自动同步 | 新增 `ZufyUICommands.h` |
| 🟡 | **应用级快捷键** | 焦点→窗口→应用 的按键冒泡链；`RegisterCommand` 集中注册 `Ctrl+S` 等 | `ZufyUI.h` |
| 🟡 | **Settings / LayoutStore** | JSON 存 `%APPDATA%`：窗口位置尺寸、SplitView 比例、列宽/列可见性、页面选中项；自动保存 | 新增 `ZufyUISettings.h` |
| 🟡 | **RunAsync\<T\>** | 后台 `work` + UI 线程 `onSuccess/onError`、取消令牌、异常兜底 | `ZufyUI.h` |
| 🟢 | **动画助手 `AnimateProperty`** | `AnimateProperty(target,get/set,from,to,ms,Easing)`。缓动本体的收敛**已部分完成**（`detail::EaseOutCubic/EaseInOutCubic`，v1.21.0；`ToggleSwitch`/`ProgressBar`/`Slider` 已改时长驱动）→ 仍缺统一助手与其余控件迁移 | 新增 `ZufyUIAnimation.h` |
| 🟢 | **空状态任意元素** | 数据视图保留 `SetEmptyText`，新增 `SetEmptyContent(shared_ptr<UIElement>)` | `ZDataViewer.h` |
| 🟢 | DatePicker / Calendar / TimePicker | 自绘日历 + 本地化，大工程 | — |
| 🟢 | RichText Label（Run 列表） | 段落级样式是独立子系统 | — |
| 🟢 | Skeleton / Loading | 骨架屏占位块 | — |

## B. 数据视图

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **D5：`Label` 自身布局缓存** | `Label::Draw` 会 `SetTextAlignment/WordWrapping/LineSpacing/Trimming` 修改 layout，不能共用全局缓存 → 需按 `text+宽高+fmt+对齐+换行+overflow+maxLines` 建独立 keyed cache |
| 🟡 | **D2b：ListView 富项动画递归** | `UpdateAnimation`/`HasActiveAnimation` 递归可见项 Label（富项里的动画子控件需要） |
| 🟡 | **D3：数据控件接口收敛** | `SetItemTextColor` 等转发给 Label，删 `itemTextColors_`/`cellTextColors_`（破坏源码兼容） |
| 🟡 | **D4：TreeView 重构** | `TreeNode.columns` 由 `std::wstring` 改 `std::shared_ptr<Label>`（破坏性；目前单元格 Label 已存在但仍是弱约束） |
| 🟡 | **N13/N14：TreeView / TableView 视口虚拟化** | 目前只有 `ListView` 做了按视口虚拟化；`TreeView`/`TableView` 大数据量仍全量建 Label（与 ListView 对齐） |
| 🟢 | **D6 注释止血** | 给 `SetItem(shared_ptr<Label>)` 系列加注释：“仅 `GetText()` 被读取，其余属性被忽略；单项样式用 `SetItemTextColor`” |
| 🟢 | **ListView ChildrenSig 补 `dataVersion_`** | 签名对“行文字色/勾选/隐藏”等仅靠 `viewVersion_` 隐式兜底；建议加统一 `dataVersion_` 计数器在每次数据变更自增 |
| 🟢 | **P2/P4：TreeView 增量** | `FindNode` 建 `id→node` 映射；`BuildVisibleList` 增量 |
| 🟢 | **N16：ComboBox 过滤 O(N)** | 过滤时逐项扫描；已选保留已是 O(1)，未选命中的构建仍是 O(N) |

## C. 多线程 / 生命周期

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **TreeView 数据 API 补 `RenderGuard`** | `AddRoot/AddChild/RemoveNode/Clear/ExpandNode/SetNodeText/SetFilter/Search/SetNodeCheckState/SetCheckable/SortChildren/BeginUpdate/EndUpdate` 等在 `ZUFYUI_RENDER_THREAD=1` 下与 render 线程竞态（与自身注释矛盾） |
| 🟡 | **ListView `SortItems`/`SwapItems` 补 `RenderGuard`** | 直接改 `items_`，无 `RenderGuard`（其余数据修改都有）。⚠ `SortItems` 现已加虚拟模式守卫（v1.21.0） |
| 🟡 | **N12：RenderHost 设备丢失时丢批次** | 设备丢失重建期间在途批次可能被丢，需确认/补偿 |
| 🟢 | **M7：离屏缓存预算 / LRU** | 给缓存加“总像素预算 + 淘汰”，大容器才不爆 |
| 🟢 | **R3/R4/L13** | `TreeNode::parent` 弱引用；`checkAnim_` key 改 nodeId；`SetParent` 幽灵节点（基类虚 `RemoveChild`） |
| 🟢 | **M8/M9/M10** | 单元素缓存像素上限；`ReleaseDeviceResources` 是否递归；多页懒构建/卸载 |
| 🟢 | **N7：`IconFormat` / 图标字形 DPI 缓存** | 高 DPI 下字形/图标格式缓存未随 DPI 变化失效 |
| 🟢 | **N10：`SetAppIcon` / `RegisterApp` HICON 泄漏** | 传入的 `HICON` 生命周期/释放路径需理清 |
| 🟢 | **UIA provider 引用约定** | `ZufyUIElementProvider::Release` 不清理 `ctx_->elements` 映射；当前靠“map 不额外 AddRef”的紧绷约定，建议注释或改 `WeakPtr` |
| 🟢 | **`Window` 消息观察项** | `OnMouseDown` 转发给 capture 元素时 `pressedElement_` 收不到 `OnMouseUp`；`WM_DESTROY` 里 `Closed()` 回调时 `hwnd_` 已空（文档标注） |

## D. 主题系统

| 级别 | 项 | 要点 |
|---|---|---|
| 🟢 | 逐控件令牌化收尾 | `MenuBar` 背景、`CheckBox` 悬停、图表轴/网格之外的长尾硬编码色（见 `tools/color_inventory.md`）；`ThemeManager::Map` 色差重映射接入绘制点（v1.21.0 已补一批默认色 setter） |
| 🟢 | `Theme::HighContrast` 精化 | 目前语义色已区分；可进一步对齐系统 HC 经典色（`SPI_GETCOLOR_*`） |
| 🟢 | “跟随系统层”开关 | 目前用 `AppsUseLightTheme`（应用层）；可选加 `SystemUsesLightTheme`（系统层） |
| 🟢 | 单隐藏 watcher 窗口 | 由 `AppCore` 挂一个隐藏消息窗统一听 `WM_SETTINGCHANGE` 等，多窗口时只解析一次 |

## E. 拖放

| 级别 | 项 | 要点 |
|---|---|---|
| 🟢 | 元素级拖拽影像 | `BeginDrag` 目前用 `InitializeFromWindow`（整窗缩略图）；可选元素级位图 |
| 🟢 | `BeginDrag` 影像对齐鼠标 | `InitializeFromWindow(hwnd_, &pt, ...)` 的 `pt` 固定 `(0,0)`，可传光标位置 |
| 🟢 | `DragData::x/y` 语义文档化 | 元素级=元素相对 DIP、窗口级=客户区 DIP；运行期无法区分，文档已写但建议在类型上再标注 |

## F. 图表

| 级别 | 项 | 要点 |
|---|---|---|
| 🟢 | `PieChart::HitSlice` 角度归一化 | 跨 `-π/π` 边界时 `lo/hi` 可能落在不同周期，极端情况命中错扇区；建议统一映射到 `[0,2π)`（v1.21.0 已抽 `PieGeometry` 统一几何） |
| 🟢 | 百分比堆叠 0/NaN | `ComputeValueRange` 在 `posSum==0`/`negSum==0` 时 `v/sum*100` 可能静默 0 或 NaN |
| ✅ | 滚动条尺寸互换 | 已修（v1.19.7）：`viewW` 用竖条、`viewH` 用横条 |
| ✅ | fill 回退默认尺寸 | 已修（v1.19.4） |
| ✅ | `PieChart` 遮蔽 `ChartBase::zoom_` / 滚轮不平移 / 继承 `ChartBase` 获得平移+滚动条 | 已修（v1.21.0） |

## G. 布局 / 控件 bug

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **TreeView 命中未对齐 `GetFirstColDecor`** | 窄列隐藏勾选框/三角时，点击隐藏区域仍会误触；命中前应调用 `GetFirstColDecor` 只对 `dec.check/dec.triangle` 处理（`HANDOFF §10.4-6 / N4`） |
| 🟢 | **N3：TextBox undo 栈污染且无上限** | 每次小改动都压栈、无上限；需合并/限长 |
| ✅ | ColumnBox/RowBox spacing 统计 | 已修：只按“参与布局”的子元素数 |
| ✅ | GridLayout maxRow/maxCol | 已修：过滤不可见元素 |
| ✅ | Label 换行漏算子控件宽度 / 多算一个 spacing | 已修 |
| ✅ | ListView/TreeView 释放子 Label 未清 `childrenView_` / 漏释放池化/富项/单元格 Label | 已修 |
| ✅ | NumberBox 滚轮焦点判断 | 已修（焦点在内部 TextBox） |
| ✅ | `Window::defaultIMC_` IME 关联 | 已修（`ImmAssociateContext` 取默认） |
| ✅ | `lastActiveAnimElements_` 悬垂 | 已修（析构时从 anim 集合擦除） |
| ✅ | `Menu::ShowAt` bad_weak_ptr | 已修（`weak_from_this`） |
| ✅ | `DragData::GetFiles` len==0 越界 | 已修（`len+1`） |
| ✅ | `Button` 定宽 120 把长文案裁成省略号 | 已修（v1.21.0 `AutoSizeToText`） |
| ✅ | ProgressBar/Slider 圆角用构造高度、拉伸后错位 | 已修（v1.21.0 用 `arrangedRect_.height/2`） |
| ✅ | `ToggleSwitch` 鼠标/键盘绕过 `SetOn` → 瞬间开关 | 已修（v1.21.0） |

## H. 工程化 / 文档

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **CMakeLists.txt / 模板工程** | 目前只有 `.slnx`（只吃最新 VS）；补 CMake + 最小模板 |
| 🟡 | **断言式单元测试** | 数据视图虚拟化坐标、主题切换幂等、拖放自环等核心路径；README 说 demo 充当冒烟测试（现有 `tests/dsl_tests.cpp` 85 项 DSL 断言） |
| 🟢 | **CI** | 至少构建 + parity 校验（`tools/check_api_parity.py`）+ `tools/ensure_bom.ps1 -Check` |
| 🟢 | `MessageBox(HWND)` owner | 传裸 `HWND` 时窗口先以 owner=nullptr 创建再改，任务栏按钮会闪一下；应支持“创建前设 owner”（`N9`） |
| 🟢 | `TabView::ReleaseDeviceResources` 补子元素 | 漏 `contentHost_`/`hBar_` |
| 🟢 | `TreeView::OnMouseDown` 重复表头判断 | 第二次 `if (headerVisible_ && ...)` 不可达，重构遗留 |
| 🟢 | `NumberBox::DrawButtons` 死代码 | 从未调用（实际走 `DrawSpinButton`） |
| 🟢 | `FileDialog` 硬编码中文“所有文件” | 无语言参数，英文用户也拿中文 |
| 🟢 | `TrayIcon` HICON/Image 所有权注释 | 只销毁通过 `Image`/`AddFromFile` 传入的图标；直接传 `HICON` 由调用方管理 |
| 🟢 | 滚动条拖动除零守卫 | `ListView`/`TableView` 的 `trackHeight - thumbLength` 极端下为 0 → 统一加 `if (trackHeight <= thumbLength + 0.5f) return;` |
| 🟢 | `Window::OnPaint` 单线程 present 里 `PeekMessage` 派发 | 复杂场景可能递归；有 guard 但可评估 |
| 🟢 | `Fire` 重入锁内 `tlSnapshot.clear()` 注释 | 极低风险，补注释 |

## I. DSL（链式构造）

| 级别 | 项 | 要点 |
|---|---|---|
| 🟢 | P3 组合运算符（可选） | `operator,` / `operator|` 让 `Col({...})` 更简洁；可能牺牲报错可读性 |
| 🟢 | 更多控件专属糖 | 用宏集中生成，避免逐手写漂移 |

## J. 待定位 / 待用户验收（来自 `HANDOFF §10.4`）

| 级别 | 项 | 状态 / 下一步 |
|---|---|---|
| ✅ | **MessageBox 首次点击穿透** | **已修**（待用户复测）。根因：模态是在 `Window::OnMouseUp` 里按钮 `Clicked` 槽中弹的，而该函数末尾的 `ReleaseCapture()` 要等模态循环结束才执行 → 整段模态期间主窗口仍持有 `SetCapture`，系统把鼠标消息优先路由给捕获窗口（主窗口）→ 首击“穿透”；那次点击让主窗口自己释放了捕获，故只有第一次坏。修法：`Window::RunModal` 进入循环前 `if (GetCapture()) ReleaseCapture();`。 |
| 🟡 | **SplitView 折叠（已修 2 个真 bug，待目视验收）** | 已修：① `Expand()` 在 `StartCollapseAnim()` 前就清了 `collapsedFirst_/Second_`，而 `PaneLengths` 按这两个标志决定折叠插值 → **展开时尺寸瞬跳、无缓动**；② 展开期间小按钮按 `collapsedFirst_` 判断 → **跳侧**。改用贯穿折叠/展开的 `collapseSide_`，动画收尾才清零。**仍需用户看**：提示覆盖层颜色/透明度/箭头方向、松手收缩缓动、折叠后小按钮位置与悬停、展开到“最小极限值”。 |
| 🟡 | **demo page10“有空间却省略号/不铺满”** | 主行代码已到位（6 处 `minWidth(220)` + 网格 `SetColumnStretch(1,1)` + 下拉 `fillWidth`）。**本轮再补**：参数卡里滑条名字列由固定 `width(90)` 改 `minWidth(90)`（长名字“色调 alpha”类会被省略号裁）。**无法本地复现（不代跑 GUI），待用户确认是否仍省略**；若仍省略 → 那几行改 `Grid`（标签列按内容 + 控件列拉伸）。 |
| ✅ | **`ScrollViewer` 滚轮归属** | **已修**（待用户复测）。根因：滚轮路由（`Window::OnMouseWheel`）本就逐级冒泡，但 `ScrollViewer::OnMouseWheel` 只要“能滚该轴”就无条件消费 → **滚到顶/底后仍吞掉滚轮**，外层页面再也滚不动。改为“该方向还能滚才消费，到边界返回 false 冒泡”，即标准**滚动链**。 |

## K. 待产品决策（不放擅自改 API，`HANDOFF §10.4-5`）

| 级别 | 项 | 说明 |
|---|---|---|
| 🟡 | `StatusBar::AddPanel` 加 `onClick` | 否则状态栏永远不能“点击触发动作” |
| 🟡 | `Window` 加尺寸/DPI 变化信号 | 否则状态栏“DPI/客户区”格无法实时刷新（现有信号无尺寸变化） |
| 🟢 | 9 个与类同名的 `dsl::` 短工厂是否加显式别名 | 现在必须在表达式与模板实参里 `::ZufyUI::dsl::` 限定 |
| 🟢 | `ProgressRing` 是否加 `Ring(float size)` 短工厂 | — |
| 🟢 | `PieChart::zoom_` 遮蔽 `ChartBase::zoom_`（代码味道） | v1.21.0 已让饼图自处理 Ctrl 缩放；建议后续统一成一个成员 + 统一 0.4~4 缩放区间（基类现钳 0.4~3） |

## L. API 债整理（P0→P3，来自外部评审；本轮进度）

| 状态 | 级别 | 项 | 说明 |
|---|---|---|---|
| ✅ | P0-1 | **带默认参数的成员函数拆成无默认参数重载** | 已拆：`TreeView::SetCheckable/SetNodeCheckable/SetNodeEnabled/SetNodeCheckState`、`ListView::AddItem`↔`AddItemWidget`、`RadioGroup::AddItem`↔`AddItemSelected`/`AddButton`↔`AddButtonSelected`、`Menu::AddCheckItem`↔`AddCheckItemCb`/`AddCheckItemRadio`。`.with` 可用率大增。 |
| ✅ | P0-2 | **同名 dsl 工厂无歧义别名** | 新增 `make_slider/make_card/make_page/make_menu/make_menu_bar/make_status_bar/make_radio_group/make_scroll_bar/make_expander`（非破坏）；demo 已改用。 |
| ✅ | P0-3 | **`.with`/`.apply`/`->` 收敛 + 约定文档** | 约定写入 `AGENTS.md`；易错点集中到 `docs/pitfalls.md`。 |
| ✅ | P1-2 | **`Page::SetScrollableContent(content)`** | 一行套 ScrollViewer + 边距，替代 demo 里 15 份手写样板。 |
| ✅ | P1-4 | **`ThemeManager::RegisterHook/RegisterSubtreeApplier` 返回 RAII `HookHandle`** | 析构自动断开；库内部固定钩子走 `RegisterHookPermanent`。 |
| ✅ | P3 | **`docs/pitfalls.md`** | 把散落的坑抽成一表。 |
| 🟡 | P1-1 | **拆 demo `main.cpp`（2300+ 行）→ `pages/*.cpp`** | 每页成为“这库怎么用”的活文档；未做（改动大，建议单独一轮）。 |
| 🟡 | P1-3 | **窗口 / 托盘 / 主题钩子归 `Application` 管** | `CreateWindow` 所有权契约模糊、`std::shared_ptr<枚举>` 当状态；未做。 |
| 🟢 | P2-1 | **`Connect` 弱引用便利接口（`ConnectWeak`）** | 让用户不必自己理解引用环；未做（需逐个 setter 或加通用弱连接）。 |
| 🟢 | P2-2 | **`Ref<T>` 与 `shared_ptr<T>` 统一** | 减少 `.shared()` / 隐式转换来回切；未做。 |

---

## ⛔ 明确不做（保留结论，避免反复讨论）

### 设计取舍
1. **页级“整平”缓存**（把子树合成到一张位图）——❌ 不做：单控件动画会导致整张位图更新、内存巨大、收益低。
2. **事件驱动 `CollectActiveAnimations`**（pull→push）——❌ 不做：“是否在动”是派生量；条件触发的 bug 极难测，收益仅 1%~3%。
3. **合并 `UpdateAnimation` + `CollectActiveAnimations`**——❌ 不做：容器 `UpdateAnimation` 自带递归，外部再遍历会让子元素被调两次、速度翻倍。
4. **数据控件里的 Label 走缓存（`cacheRT_`）**——❌ 负优化：项多、滚动频繁，每项一张 GPU 位图成本 > 收益。
5. **数据绑定 / MVVM**——❌ 不做：信号槽已覆盖 90%，抽象税太高。
6. **NavigationView 做成控件**——❌ 做成示例即可，别把导航策略焊进库。
7. **头文件规模 / 纯头文件卖点**——保留：单/少 TU 编译快；大规模 TU 会撞编译墙，属已知取舍。

### 已踩过的认知坑（务必按“对”的做）
| 错误做法 | 正确认知 |
|---|---|
| 子变化 → 作废父链缓存 | 缓存是**逐控件**的；父缓存只含父自身绘制 |
| `Arrange` 里无条件 `cacheValid_=false` | 只由**尺寸变化**驱动；纯位置变化不清缓存 |
| 用一个脏位承担“自身/子树/缓存失效”三义 | 拆 `selfArrangeDirty_` + `subtreeDirty_`；缓存失效独立 |
| `InvalidateLayout` “遇脏即停” | **冒泡到根** |
| 拖动时照常做 hover 命中 | 拖动/缩放循环里 `WM_NCHITTEST→HTCAPTION`、`WM_NCMOUSEMOVE→return` |
| 用未 Snap 的偏移算 firstVisible | 与 itemRect 统一用 **Snap 后**的值 |
| 用“指数收敛 + 阈值吸附”做动画 | 要爬 ~6τ 才停、期间每帧重绘 → 用户觉得卡；改**时长驱动** `EaseOutCubic/EaseInOutCubic`（到点精确结束） |
| 改状态（`isOn_`/`value_`…）绕开 setter | 会漏掉动画启动/信号/上报 → **所有**改状态的路径必须走 setter 或显式启动动画 |

### 平台 / 工程硬事实
- DComp 桌面窗口目标视觉坐标是**物理像素**（DPI=96，不随窗口 DPI）。
- `CreateSwapChainForComposition` + `FLIP_SEQUENTIAL` 要求 `BufferCount>=2`（写 1 静默失败 → 全透明）。
- 含中文的 `.h/.cpp` **必须 UTF-8 带 BOM**；`.md/.tex` 无 BOM；`tools/*.ps1` 故意纯 ASCII 无 BOM。
- `ID2D1BitmapRenderTarget` 缓存不是整平：父缓存不含子树。
- `Measure` 缓存安全前提：`FontManager::formatCache_` 永不淘汰。
- 缓存作废的唯一可靠来源：`RequestRepaint()`；任何内容/size 变化的 setter 必须调 `RequestRepaint`/`InvalidateLayout`。
- `childrenDirty_`：所有改子元素列表的路径都要置脏。
- 父元素先画、子元素后画（按 `GetChildren()` 顺序）→ “画在内容之上”的覆盖层必须做成**排在最后的子元素**（`SplitView::Overlay` 即此模型）。
