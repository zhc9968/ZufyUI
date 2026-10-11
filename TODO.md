# ZufyUI 待办清单（TODO）

> **单一权威待办清单**。合并自已删除的 `FEATURE_PLAN.md` / `OPTIMIZATION_PLAN.md`，并汇总历次代码/文档审计中**尚未处理**的项。
> 级别：🔴 重要 / 🟡 中等 / 🟢 轻微 / ⛔ 明确不做（保留结论，避免反复讨论）。
> 版本基线：`1.19.6`。完成一项就删一行（或标 ✅）。

---

## A. 功能补齐（尚未做）

| 级别 | 项 | 要点 | 落点 |
|---|---|---|---|
| 🟡 | **Command 命令系统** | `Command{ Execute/CanExecute/CanExecuteChanged, text, icon, hotkey }`；Button/MenuItem/快捷键绑同一 Command，`enabled` 自动同步 | 新增 `ZufyUICommands.h` |
| 🟡 | **应用级快捷键** | 焦点→窗口→应用 的按键冒泡链；`RegisterCommand` 集中注册 `Ctrl+S` 等 | `ZufyUI.h` |
| 🟡 | **Settings / LayoutStore** | JSON 存 `%APPDATA%`：窗口位置尺寸、SplitView 比例、列宽/列可见性、页面选中项；自动保存 | 新增 `ZufyUISettings.h` |
| 🟡 | **RunAsync\<T\>** | 后台 `work` + UI 线程 `onSuccess/onError`、取消令牌、异常兜底 | `ZufyUI.h` |
| 🟡 | **动画助手 + 统一缓动** | `AnimateProperty(target,get/set,from,to,ms,Easing)`；先收敛现有散落插值 | 新增 `ZufyUIAnimation.h` |
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
| 🟢 | **D6 注释止血** | 给 `SetItem(shared_ptr<Label>)` 系列加注释：“仅 `GetText()` 被读取，其余属性被忽略；单项样式用 `SetItemTextColor`” |
| 🟢 | **ListView ChildrenSig 补 `dataVersion_`** | 签名对“行文字色/勾选/隐藏”等仅靠 `viewVersion_` 隐式兜底；建议加统一 `dataVersion_` 计数器在每次数据变更自增 |
| 🟢 | **P2/P4：TreeView 增量** | `FindNode` 建 `id→node` 映射；`BuildVisibleList` 增量 |

## C. 多线程 / 生命周期

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **TreeView 数据 API 补 `RenderGuard`** | `AddRoot/AddChild/RemoveNode/Clear/ExpandNode/SetNodeText/SetFilter/Search/SetNodeCheckState/SetCheckable/SortChildren/BeginUpdate/EndUpdate` 等在 `ZUFYUI_RENDER_THREAD=1` 下与 render 线程竞态（与自身注释矛盾） |
| 🟡 | **ListView `SortItems`/`SwapItems` 补 `RenderGuard`** | 直接改 `items_`，无 `RenderGuard`（其余数据修改都有） |
| 🟢 | **M7：离屏缓存预算 / LRU** | 给缓存加“总像素预算 + 淘汰”，大容器才不爆 |
| 🟢 | **R3/R4/L13** | `TreeNode::parent` 弱引用；`checkAnim_` key 改 nodeId；`SetParent` 幽灵节点（基类虚 `RemoveChild`） |
| 🟢 | **M8/M9/M10** | 单元素缓存像素上限；`ReleaseDeviceResources` 是否递归；多页懒构建/卸载 |
| 🟢 | **UIA provider 引用约定** | `ZufyUIElementProvider::Release` 不清理 `ctx_->elements` 映射；当前靠“map 不额外 AddRef”的紧绷约定，建议注释或改 `WeakPtr` |
| 🟢 | **`Window` 消息观察项** | `OnMouseDown` 转发给 capture 元素时 `pressedElement_` 收不到 `OnMouseUp`；`WM_DESTROY` 里 `Closed()` 回调时 `hwnd_` 已空（文档标注） |

## D. 主题系统

| 级别 | 项 | 要点 |
|---|---|---|
| 🟢 | 逐控件令牌化收尾 | `MenuBar` 背景、`CheckBox` 悬停、图表轴/网格之外的长尾硬编码色（见 `tools/color_inventory.md`）；`ThemeManager::Map` 色差重映射接入绘制点 |
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
| 🟢 | `PieChart::HitSlice` 角度归一化 | 跨 `-π/π` 边界时 `lo/hi` 可能落在不同周期，极端情况命中错扇区；建议统一映射到 `[0,2π)` |
| 🟢 | 百分比堆叠 0/NaN | `ComputeValueRange` 在 `posSum==0`/`negSum==0` 时 `v/sum*100` 可能静默 0 或 NaN |
| ✅ | 滚动条尺寸互换 | 已修（v1.19.7 待发版）：`viewW` 用竖条、`viewH` 用横条 |
| ✅ | fill 回退默认尺寸 | 已修（v1.19.4） |

## G. 布局 / 控件 bug

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **TreeView 命中未对齐 `GetFirstColDecor`** | 窄列隐藏勾选框/三角时，点击隐藏区域仍会误触；命中前应调用 `GetFirstColDecor` 只对 `dec.check/dec.triangle` 处理 |
| ✅ | ColumnBox/RowBox spacing 统计 | 已修：只按“参与布局”的子元素数 |
| ✅ | GridLayout maxRow/maxCol | 已修：过滤不可见元素 |
| ✅ | Label 换行漏算子控件宽度 / 多算一个 spacing | 已修 |
| ✅ | ListView/TreeView 释放子 Label 未清 `childrenView_` / 漏释放池化/富项/单元格 Label | 已修 |
| ✅ | NumberBox 滚轮焦点判断 | 已修（焦点在内部 TextBox） |
| ✅ | `Window::defaultIMC_` IME 关联 | 已修（`ImmAssociateContext` 取默认） |
| ✅ | `lastActiveAnimElements_` 悬垂 | 已修（析构时从 anim 集合擦除） |
| ✅ | `Menu::ShowAt` bad_weak_ptr | 已修（`weak_from_this`） |
| ✅ | `DragData::GetFiles` len==0 越界 | 已修（`len+1`） |

## H. 工程化 / 文档

| 级别 | 项 | 要点 |
|---|---|---|
| 🟡 | **CMakeLists.txt / 模板工程** | 目前只有 `.slnx`（只吃最新 VS）；补 CMake + 最小模板 |
| 🟡 | **断言式单元测试** | 数据视图虚拟化坐标、主题切换幂等、拖放自环等核心路径；README 说 demo 充当冒烟测试 |
| 🟢 | **CI** | 至少构建 + parity 校验（`tools/check_api_parity.py`） |
| 🟢 | `MessageBox(HWND)` owner | 传裸 `HWND` 时窗口先以 owner=nullptr 创建再改，任务栏按钮会闪一下；应支持“创建前设 owner” |
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

### 平台 / 工程硬事实
- DComp 桌面窗口目标视觉坐标是**物理像素**（DPI=96，不随窗口 DPI）。
- `CreateSwapChainForComposition` + `FLIP_SEQUENTIAL` 要求 `BufferCount>=2`（写 1 静默失败 → 全透明）。
- 含中文的 `.h/.cpp` **必须 UTF-8 带 BOM**；`.md/.tex` 无 BOM。
- `ID2D1BitmapRenderTarget` 缓存不是整平：父缓存不含子树。
- `Measure` 缓存安全前提：`FontManager::formatCache_` 永不淘汰。
- 缓存作废的唯一可靠来源：`RequestRepaint()`；任何内容/size 变化的 setter 必须调 `RequestRepaint`/`InvalidateLayout`。
- `childrenDirty_`：所有改子元素列表的路径都要置脏。
