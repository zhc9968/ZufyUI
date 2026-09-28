# ZufyUI 功能补齐计划：三批次

> 状态日期：2026-09-27 · 当前提交 `035bd01`（v1.10.3）· 本文件为新增（未提交）
> 说明：本文件是**功能补齐**的权威清单，级别：🔴刚需 / 🟡重要 / 🟢可选。
> 依据：对"功能缺口盘点"逐条核对后的结论，并已按你的决定调整（TabView 提前；多行 TextBox 延后；主题延后；数据绑定不做）。

---

## 0. 结论

- 库"能演示一切"，真正卡业务的是**"同一件事要用户手搓三遍"**：命令要同步三处、加载态要自己写、图标/配置各自维护。
- **先补"常用控件 + 命令/快捷键/图标"，收益最大**；虚拟化 / 数据绑定 / 主题属于"大块 + 易过度设计"，推迟到有真实约束时再做。
- **不做**：数据绑定（信号槽已覆盖 90%，全量抽象税太高）。
- **延后**：主题（复用现有手工配色，**绝不重生成**）、OLE 拖放、UIA、DatePicker/Calendar、RichText、Skeleton。

---

## 一、批次 1：常用控件 + 命令 / 快捷键 / 图标

目标：消掉日常最大重复劳动。TabView **提前**到此批。

| # | 项 | 级别 | 要点 | 落点 |
|---|---|---|---|---|
| 1-1 | **图标系统 Icon** | 🔴 | `enum class Icon`（Fluent 码点表）+ 尺寸/颜色变体 + 按尺寸缓存 `IDWriteTextLayout`（复用 FontManager 缓存） | 新增 `ZufyUIIcons.h` |
| 1-2 | **Command 命令** | 🔴 | `Command{Execute()/CanExecute()/CanExecuteChanged/text/icon/hotkey}`；Button/MenuItem/快捷键绑同一 Command，`enabled` 自动同步 | 新增 `ZufyUICommands.h` |
| 1-3 | **应用级快捷键** | 🔴 | `Application::RegisterCommand(cmd)` + "焦点→窗口→应用"冒泡链；`Ctrl+S`/`Ctrl+O` 集中注册 | `ZufyUI.h`（Application / Window 按键链） |
| 1-4 | **TabView / TabControl** | 🔴（提前） | 顶部横向页签；键盘导航（←/→/Home/End）、可关闭按钮、溢出折叠；可与 PageHost 共用过渡 | 新增 `ZufyUINavigation.h` |
| 1-5 | **RadioButton + RadioGroup** | 🔴 | 圆点样式；**显式 `GroupId`** 组内互斥（不靠父容器推断） | `ZufyUIWidgets.h` |
| 1-6 | **Spinner / NumberBox** | 🟡 | TextBox + 两个小 Button + `min/max/clamp` + 滚轮调值 + 输入过滤 | `ZufyUIWidgets.h` |
| 1-7 | **ProgressRing** | 🟡 | 环形进度；与 ProgressBar 共用取值/动画模型 | `ZufyUIWidgets.h` |
| 1-8 | **Splitter** | 🟡 | 可拖动分隔条子元素 + 比例回调（配合批次 2 的持久化） | `ZufyUIWidgets.h` |
| 1-9 | **空状态允许任意元素** | 🟢 | 保留 `SetEmptyText(string)`，新增 `SetEmptyContent(shared_ptr<UIElement>)` | `ZDataViewer.h` |

---

## 二、批次 2：应用骨架（配置 / 异步 / 动画）

| # | 项 | 级别 | 要点 | 落点 |
|---|---|---|---|---|
| 2-1 | **Settings / LayoutStore** | 🔴 | JSON 存 `%APPDATA%`：窗口位置尺寸、SplitView 比例、列宽/列可见性、页面选中项；读写 API + 自动保存 | 新增 `ZufyUISettings.h` |
| 2-2 | **RunAsync<T>** | 🔴 | `work` 跑后台、`onSuccess/onError` 回 UI（`PostToUIThread`）、取消令牌、异常兜底 | `ZufyUI.h` 或 `ZufyUIAsync.h` |
| 2-3 | **动画助手 + 统一缓动** | 🟡 | `AnimateProperty(target, get/set, from, to, ms, Easing)` + 统一缓动表；**先收敛现有散落插值**（Button/PageHost/…） | 新增 `ZufyUIAnimation.h` |
| 2-4 | **Expander / Accordion** | 🟡 | 折叠面板；高度过渡走 2-3 | `ZufyUIWidgets.h` |

---

## 三、批次 3：数据 / 文本能力（较大块）

| # | 项 | 级别 | 要点 | 落点 |
|---|---|---|---|---|
| 3-1 | **数据视图"伪虚拟化"** | 🟡 | 轻量数据源回调（`RowCount()` + `GetCellText(r,c)`），可见 Label 进**回收池**；**不做**全量 MVVM `IDataSource` | `ZDataViewer.h` |
| 3-2 | **多行 TextBox** | 🟡 | 重构 `UpdateDisplayLayout` 的换行/滚动模型 | `ZufyUIWidgets.h` |

> 3-1 现状：`ListView::items_` 是 `vector<shared_ptr<Label>>`，`SetRowCount(N)` 就 N 个 Label；TableView 已有"可见行范围 + 就地摆放"的地基，离"回收复用可见 Label"只差一步。

---

## 四、延后 / 不做

| 项 | 结论 | 理由 |
|---|---|---|
| **主题系统** | 延后 | 只做**方向**：把现有颜色抽成命名 token，**默认值 = 你现在手工调好的原值**；暗色只是"token→值 覆盖表"，**由人来定色**。框架只提供容器/切换，配色这活儿留给人。 |
| **数据绑定** | **不做** | 信号槽已覆盖 90%；全量 `Bind()` 要 model 反射/属性系统，抽象税太高。 |
| **OLE 拖放** | 延后 | 另一套 COM（`IDataObject`/`IDropTarget`）。**控件间拖拽（重排/跨列表）小**，可随时补。 |
| **UIA / Accessibility** | 延后 | 合规需要时再动。 |
| **DatePicker / Calendar / TimePicker** | 延后 | 自绘日历 + 本地化，大工程；先用受限文本输入顶。 |
| **RichText Label（Run 列表）** | 延后 | 段落级样式是独立子系统。 |
| **Skeleton / Loading** | 延后 | Skeleton = **骨架屏**：加载完成前，用灰色占位块**模拟内容的布局形状**（图/行/卡片），让界面不空白。非刚需；Loading 微件随批次 2 动画顺带。 |
| **NavigationView** | 不做成控件 | 它 = `NavList + PageHost` 的组合，做**示例**即可，别把导航策略焊进库。 |

---

## 五、一句话

先做**批次 1（9 个小件）**，立刻消掉大部分重复劳动；**批次 2** 补"应用骨架"；**批次 3** 才碰虚拟化/多行文本。主题与数据绑定按上述处理。
