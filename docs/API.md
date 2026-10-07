# ZufyUI API 参考

> 本文档覆盖 ZufyUI 框架的全部公开 API，**按“使用分类”组织**（不是按代码/头文件顺序）。
> 配套阅读：[项目说明与构建](../README.md)。

> **阅读方式**：本文不是“签名清单”，而是按“它是什么 → 框架内部怎么处理 → 你需要注意什么”来写。
> 每个类先讲整体行为，再逐函数讲副作用、默认值与易混点。**没有读过源码也能据此正确使用。**

> **两个贯穿全文的标记**：
> - **`易错`**：真实踩过的坑 / 反直觉行为，优先看这些。
> - **`版本`**：该 API 或语义发生显著变化的版本（`新增`/`变更`/`废弃`）。当前发布为 **v1.18.0**（上一版 v1.17.0）。

###chapter: 约定 | 命名空间、单位、生命周期、版本与易错标注约定

## 命名空间与头文件

- 所有内容位于 `namespace ZufyUI`。
- 头文件**按使用分层**：
  - `ZufyUI.h`：核心——类型 / 信号槽 / 字体 / 元素基类 / 布局 / 菜单 / 窗口。
  - `ZufyUIWidgets.h`：基础控件（Label/Button/TextBox/TextEdit/ComboBox/…/ScrollViewer/TabView/图标…），内部 `#include "ZufyUI.h"`。
  - `ZDataViewer.h`：数据视图（ListView/TableView/TreeView），依赖前两者。
  - `ZufyUICharts.h`：**图表（BarChart/LineChart/PieChart）**，依赖 `ZufyUI.h`。
  - `ZufyUIWindowTool.h`：窗口周边（标题栏 / MessageBox / 托盘 / 系统对话框 / 无障碍提供程序 / 调试通道）。
- 全部是**纯头文件**；`#pragma comment(lib, ...)` 会自动链接 `d2d1 / dwrite / dwmapi / imm32 / winmm`。

## 单位

- 所有坐标与尺寸都是 **DIP**（设备无关像素），不是物理像素。
- 框架在绘制前用内部 `Snap()` 吸附到物理像素网格；你写代码时**不需要**自己做 DPI 换算。
- 鼠标事件坐标也是 DIP。屏幕坐标只在系统 API（`Create`、`ShowAt`、`FileDialog`）里出现。
- **`易错`**：`Snap` 用的 DPI 比例是 **线程本地**（`GlobalDpiScaleRef`）。渲染线程每帧设置；UI 线程现在也在 `Window::Create`/`WM_DPICHANGED` 设置。若你自建线程调用 `Snap`，它会是 1.0 而非窗口 DPI。

## 对象模型与生命周期

- 控件一律 `std::shared_ptr<T>` 拥有，用 `std::make_shared<T>()` 创建。
- 通过布局/容器的 `AddChild()` 挂到父容器；父容器保存子元素的 `shared_ptr`，**父容器存活期间子元素不会释放**。
- 脱离布局后若没有其它 `shared_ptr`，元素即回收；元素析构会自动释放自身离屏缓存与已注册连接（内部 `ConnectionGroup`）。
- **`易错`**：`UIElement::SetParent` 存的是**裸指针**（非拥有）；所有权永远在容器/`Application` 的 `shared_ptr` 上。窗口在元素里只以 **window id** 记录，避免窗口先销毁导致 UAF。

## 信号与事件

- 用 `Connect(signal, slot)` 绑定，返回 `Connection`，默认随宿主元素析构自动断开。
- 槽可以是任意可调用对象（lambda / 函数指针 / `std::function`）。
- **`易错`**：不要用“按值捕获宿主自身 `shared_ptr`”的 lambda 连接宿主自己的信号 → 引用环，整棵子树泄漏。详见「事件与信号」章。

## 默认值体系

- 每个控件有 `inline static` 的 `Default*` 静态字段（如 `Button::DefaultSize`）。
- `static SetDefault*()` 修改后**只影响之后新建的实例**；实例级 setter 只影响该实例。
- **`易错`**：静态默认字段是**进程全局**的；多窗口若需要不同观感，请在创建前设置或逐实例 setter。

## 禁用、焦点、缓存三个通用概念（贯穿所有控件）

- **启用/禁用**：`UIElement::SetEnabled(false)` 让该元素及子树不可交互（鼠标/键盘被拦截），控件需自己用 `IsEffectivelyEnabled()` 决定是否画灰。**禁用不改变布局**（仍占位）。`SetEnabled(false)` ≠ `SetVisible(false)`。
- **焦点**：只有 `IsFocusable()` 为 true 的元素能拿键盘焦点。窗口按 Tab 遍历聚焦元素；焦点环只在“通过 Tab 获得焦点”时显示，鼠标点击焦点不显示。
- **离屏缓存**：见「元素基类」的“缓存机制”。默认开启；动画频繁/实例极多的控件用 `SetUseCache(false)` 关掉以省显存。

###chapter: 快速开始 | 最小程序、组件结构与阅读路径

## 最小程序

```cpp
#include "ZufyUI.h"
#include "ZufyUIWidgets.h"
using namespace ZufyUI;

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {   // Windows 子系统（/SUBSYSTEM:WINDOWS）
    Application app = Application::Instance();
    auto win = app.CreateWindow(900, 600, L"Hello ZufyUI");
    auto root = win->GetRootColumnBox();          // 默认根是 ColumnBox，margin(20)，spacing 10
    auto btn  = std::make_shared<Button>(L"点我");
    btn->Connect(btn->Clicked, [] { /* ... */ });
    root->AddChild(btn);
    win->Show();                                  // Create 不会自动 Show（v1.8.0 起）
    return app.Run();
}
```
> **`易错`**：GUI 程序用 `wWinMain` + `/SUBSYSTEM:WINDOWS`。若坚持用 `int main()`，请把子系统设为控制台 `/SUBSYSTEM:CONSOLE`，或加 `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")`，否则链接会找不到入口。

## 事件循环与线程模型

- `Application::Run()` 跑唯一消息循环；**所有窗口/控件必须在同一 UI 线程创建与操作**。
- 后台线程改 UI：用信号的 `ConnectionThread::UIThread`，或 `detail::PostToUIThread`。
- 渲染线程（`ZUFYUI_RENDER_THREAD`）**默认关闭**，文档标注为不稳定、不建议开。

## 组件结构（从外到内）

`Application`（进程）→ `Window`（顶层窗口，可 owned/模态）→ 根 `Layout`（默认 `ColumnBox`）→ 容器（`Card`/`PageHost`/`TabView`/`ScrollViewer`/数据视图…）→ 具体控件。
浮层（菜单/下拉/提示/系统对话框）走独立窗口或全局覆盖层，不放进根树。

## 阅读路径建议

1. 先读本章与「约定」，建立 DIP / shared_ptr / 信号 / 默认值四个心智模型。
2. 写界面：读「布局系统」+「元素基类」+「基础控件」。
3. 写数据界面：读「数据视图」。
4. 画图表：读「图表」。
5. 窗口周边（标题栏/托盘/对话框/无障碍/调试）：读对应章节。

###chapter: 基础类型 | Color、Rect、Thickness、Size

## Color

```cpp
struct Color {
    float r, g, b, a;                 // 分量范围 [0,1]，不是 0..255
    Color(float r=0, float g=0, float b=0, float a=1.0f);
    static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    D2D1_COLOR_F ToD2D() const;
    static Color Lerp(const Color& c1, const Color& c2, float t);
};
```
- **`易错`**：分量是浮点 **[0,1]**；写 `Color(255,0,0)` 会得到“超亮”的错色。整数入参请用 `Color::FromArgb(255,255,0,0)`。
- **`注意`**：`Color::Lerp(a, b, t)` 的 `t` **不做 clamp**，请自行保证在 0..1（超出会外插）。

## Rect

```cpp
struct Rect {
    float x, y, width, height;
    Rect(float x=0, float y=0, float w=0, float h=0);
    bool Contains(float px, float py) const;   // 半开区间：x<=px<x+w
    D2D1_RECT_F ToD2D() const;
};
```
- `Contains` 是**左闭右开**。命中测试多处依赖它。

## Thickness

```cpp
struct Thickness { float left, top, right, bottom; Thickness(float l=0,float t=0,float r=0,float b=0); };
```

## Size

```cpp
struct Size { float width, height; Size(float w=0, float h=0); };
```

###chapter: 事件与信号 | ZSignal、Connection、跨线程

## 何时用信号

- 控件对外通知（点击、选中、值变化）用内置 `ZSignal`。
- 自定义控件对外事件：自己声明 `ZSignal<...>` 成员并 `Fire`。

## ConnectionThread

```cpp
enum class ConnectionThread { CurrentThread, NewThread, UIThread };
```
- `CurrentThread`（默认）：槽在 Fire 的线程同步执行。
- `NewThread`：**每次 Fire 起一个 detach 线程**执行槽——不要在槽里碰 UI。
- `UIThread`：把槽投递到 UI 线程（后台线程改 UI 的唯一正确方式）。
- **`版本`**：`v1.15.0` 起跨线程与锁语义收口。

## Connection（Qt 风格被动句柄）

```cpp
class Connection {
    void disconnect();
    bool isConnected() const;
    explicit operator bool() const;   // 析构不自动断开
};
```
- 默认随宿主元素的 `ConnectionGroup` 在其析构时断开；**`Connection` 自身析构不断开**。
- **`易错`**：可以忽略 `connect` 的返回值；但若手动保存 `Connection` 后又手动 `disconnect`，注意信号对象可能已销毁——`isConnected`/`disconnect` 由 `alive` 标志保护。
- **`版本`**：`v1.8.1` 起为被动句柄语义。

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    void disconnectAll(); size_t size() const;
};
```
- 元素内部用它批量断开自己注册的所有连接。

## ZSignal

```cpp
template<typename... TArgs> class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    void Fire(TArgs... targs) const;    // 触发
    void operator()(TArgs... targs) const;
};
```
- 触发用 `Fire`（**没有 `emit` 宏**）。
- **`易错`**：`Fire` 基于**快照**遍历，槽内可以安全 `disconnect`；槽内再次 `Fire` 同一信号会走“重入”分支（局部拷贝），**不要**在重入里做递归深链。
- **`版本`**：`v1.18.0` 加 RAII 守卫：槽抛异常也会正确还原重入深度并释放快照。

## 全局信号 UIZSignals

```cpp
namespace UIZSignals {
    inline ZSignal<Window*, ID2D1RenderTarget*> DrawOverlay;
    inline ZSignal<Window*, float, float> GlobalMouseDown;
    inline ZSignal<Window*> WindowActivated, WindowDeactivated;
    inline ZSignal<Window*, UIElement*> ElementCaptureRequest, ElementCaptureRelease, RepaintRequest;
    inline ZSignal<Window*> LayoutInvalidated;
    inline ZSignal<> DeviceReset, ReloadAcrylic;
    inline ZSignal<const std::wstring&> Error;         // v1.16.0
}
```
- **`易错`**：`DrawOverlay` 带 `Window*`，subscriber **必须**按 `GetWindow()` 过滤，否则窗口 A 的浮层会画到窗口 B 上（经典多窗口串扰）。
- `RepaintRequest`/`LayoutInvalidated`/capture 信号只对**未挂载**元素兜底；已挂载元素直接走 `Window`。
- `DeviceReset`：释放一切与渲染目标/设备绑定的缓存（如 `ImageDeviceCache`）。
- **`版本`**：`UIZSignals::Error` = `v1.16.0`。

###chapter: 字体与图标 | FontSpec、FontManager、Icon

## FontSpec

```cpp
struct FontSpec {
    std::wstring familyName = L"Segoe UI";
    float size = 14.0f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE  style  = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    std::wstring locale = L"en-us";
    bool operator==/!=(...) const;
};
```

## FontManager（单例，文本布局缓存）

```cpp
static FontManager& Instance();
IDWriteFactory* GetFactory();
IDWriteTextFormat* GetFormat(const FontSpec&);        // 格式缓存，永不淘汰
ComPtr<IDWriteTextLayout> GetRawLayout(text, fmt, maxW, maxH, noWrap);
ComPtr<IDWriteTextLayout> GetDisplayLayout(origText, fmt, maxW, maxH, noWrap);   // 带省略号
ComPtr<IDWriteTextLayout> GetStyledLayout(text, fmt, w, h, noWrap, hAlign, vAlign, lineSpacing, maxLines);
ComPtr<IDWriteTextLayout> GetStyledDisplayLayout(...);
void CacheDisplayLayout(...); void CacheStyledDisplayLayout(...);
void SetGlobalFont(const FontSpec&);
ZSignal<> GlobalFontChanged;
```
- **`易错`**：返回的 layout 是**共享只读**对象，**不要**对它 `SetTextAlignment/SetLineSpacing/SetTrimming`——缓存 key 会与对象不一致。
- 布局缓存**有上限**（400 条，FIFO 淘汰 ¼）；返回强引用（`ComPtr`），调用期间不会被淘汰。
- **`易错`**：`GetFormat` 返回的 `IDWriteTextFormat*` **永不淘汰**，请勿修改它。
- **`版本`**：`GetStyledLayout` 与全局布局缓存在 `v1.10.1` 强化；有界缓存/强引用契约在 `v1.15.0` 前后定型。

## 图标 Icon（字体字形）

```cpp
enum class Icon : unsigned short { None=0x0000, Add=0xE710, Remove=0xE738, ..., Pin=0xE718, View=0xE890, ... };
inline const std::wstring& IconFontFamily();   // Segoe Fluent Icons → 回退 Segoe MDL2 Assets
inline std::wstring IconGlyph(Icon);           // 码点 → 单字宽字符串
```
- **`易错`**：枚举值**就是字体码点**（无独立映射表）。字体回退**只在首次静态探测**，运行期换系统字体不会重探；个别 Win11 专有字形在两版字体里缺失会显示方框。
- **`版本`**：`v1.13.0` 起 `Icon` 下沉到 `ZufyUIWidgets.h`，所有 `Label` 派生控件获得 `SetIcon`。

###chapter: 元素基类 | UIElement（布局属性、缓存、事件、无障碍、调试）

## 布局属性

```cpp
virtual Size MeasureOverride(const Size& avail) = 0;
Size Measure(const Size& avail);      Size GetDesiredSize() const;
virtual void ArrangeOverride(const Rect&);   void Arrange(const Rect&);
Rect GetArrangedRect() const;
void InvalidateLayout();      // 冒泡：向根标脏
void InvalidateLayoutSelf();  // 只标自己（池化/虚拟 rebind 用，不冒泡）
void SetWidth/SetHeight(float);  float GetWidth/GetHeight() const;
void SetFillWidth/SetFillHeight(bool);   bool GetFillWidth/GetFillHeight() const;
void SetMargin(const Thickness&);
void SetMinWidth/SetMinHeight/SetMaxWidth/SetMaxHeight(float);
void SetMinSize/SetMaxSize(float,float);
void SetStretchWeights(float h, float v);
void SetHorizontalStretchWeight(float);   void SetVerticalStretchWeight(float);
virtual float GetDefaultHorizontalStretchWeight() const;   // 同理 Vertical
```
- **拉伸权值**：容器（`ColumnBox`/`RowBox`/`GridLayout`）把**剩余空间**按子元素权值分配。`FillWidth/Height` 等价“该轴权值 1”。`GetDefault*` 是类型默认，`Set*` 覆盖它。
- **`易错`**：`SetWidth/SetHeight` 是**固定尺寸**；`SetFillWidth/Height` 是**填满父轴**；两者与拉伸权值叠加，行为要分清。
- **`易错`**：`GetDesiredSize()` 是上一次 `Measure` 的结果；未测量过/刚改内容时可能是旧值。
- **`版本`**：`SetStretchWeights`/`GetDefault*` 自初版存在；**`ColumnBox`/`RowBox`/`GridLayout` 真正按显式权值分配剩余空间 = `v1.17.0`**。

## 绘制 / 子元素 / 裁剪 / 缓存

```cpp
virtual void Draw(ID2D1RenderTarget* rt) = 0;
virtual void RefreshChildren();                       // 更新阶段（UI 线程）重建可见子列表
virtual const std::vector<UIElement*>& GetChildren() const;   // 渲染阶段纯读
virtual bool UseCache() const;   void SetUseCache(bool);      // 默认 true
virtual std::optional<D2D1_RECT_F> GetClipRect() const;
void SetClipRect(const std::optional<Rect>&);
void SetBleed(float);            // 阴影/外发光留边
virtual void ReleaseDeviceResources();        // 只释放自身
void ReleaseDeviceResourcesRecursive();        // 释放整棵子树
virtual D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const;
```
- **缓存机制**：`UseCache()==true` 的元素渲染到自身 `ID2D1Bitmap1` 离屏位图；仅当被 `RequestRepaint()` 标记或尺寸变化时重建。出现阴影时自动加 `bleed`。
- **`易错`**：**每帧内容在变**（动画、实时文本、旋转）或**实例极多**（上千个）的控件应 `SetUseCache(false)`，否则显存与重建开销爆炸。
- **`易错`**：`GetChildren()` 在渲染阶段**必须纯读**；渲染前要保证 `RefreshChildren()` 已调用（窗口在 `AdvanceFrame` 里统一做）。
- **`易错`**：`ReleaseDeviceResources()` 只释放自身；释放整棵子树用 `ReleaseDeviceResourcesRecursive()`。
- **`版本`**：`ReleaseDeviceResourcesRecursive()` = `v1.18.0`（用于隐藏页缓存延迟释放）；`SetUseCache(false)` 现在会**立即释放**已有位图。

## 可见性 / 启用 / 提示 / 光标

```cpp
void SetVisible(bool);  void SetVisibleNoInvalidate(bool);  bool IsVisible() const;
void SetEnabled(bool);  bool IsEnabled() const;  bool IsEffectivelyEnabled() const;  // 沿父链
void SetToolTip(const std::wstring&);   virtual std::wstring GetToolTip() const;
void SetCursor(HCURSOR);   HCURSOR GetDesiredCursor() const;
```
- 禁用元素不参与命中/悬停。提示会**向上解析**父级提示（如 Button → Label）。
- **`易错`**：`SetCursor` 不直接调 `::SetCursor`，而是延迟到 `WM_SETCURSOR`，避免闪烁。

## 右键菜单 / 拖动区 / 布局参与

```cpp
void SetContextMenu(std::shared_ptr<Menu>);
void SetContextMenuFactory(std::function<std::shared_ptr<Menu>()>);
std::shared_ptr<Menu> BuildContextMenu() const;
enum class LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout };
void SetLayoutParticipation(LayoutParticipation);
void SetDraggable(bool); void SetDragRegion(const Rect&); void ClearDragRegion();
virtual int NonClientHitTest(float x, float y) const;
```
- **`易错`**：`DrawBeforeLayout`/`DrawAfterLayout` 由独立通道绘制，**不要再放进根树**，否则画两遍。自定义标题栏就是 `DrawBeforeLayout` 且自己管位置。

## 事件虚函数与元素信号

```cpp
virtual UIElement* HitTest(float x, float y);
virtual void OnMouseEnter/Leave/Move/Down/Up(...);  virtual bool OnContextMenu(float,float);
virtual void OnKeyDown/Up(WPARAM,LPARAM);  virtual void OnChar(wchar_t);
virtual void OnFocus()/OnBlur();
virtual void UpdateAnimation(float dt);  virtual bool HasActiveAnimation() const;
virtual bool OnMouseWheel(float dx, float dy);
virtual bool IsTextInput() const;  virtual bool AcceptsTab() const;  virtual bool IsFocusable() const;
virtual Rect GetImeCandidateRect() const;  virtual void SetCompositionText(...);
// 元素内置信号
ZSignal<> MouseEnter, MouseLeave, Focused, Blurred;
ZSignal<float,float> MouseMove, MouseDown, MouseUp;
ZSignal<WPARAM,LPARAM> KeyDown, KeyUp;   ZSignal<wchar_t> Char;
```
- **`易错`**：有动画的控件必须让 `HasActiveAnimation()` 正确返回，否则帧循环停了动画就不走。
- **`易错`**：多行编辑器要让 `AcceptsTab()` 返回 true 才会吞掉 Tab 做缩进，否则 Tab 切焦点。
- **`版本`**：`AcceptsTab()` = `v1.18.0`（配合 `TextEdit`）。

## 无障碍（UIA）— v1.17.0

```cpp
void SetAccessibleName(const std::wstring&);  std::wstring GetAccessibleName() const;
void SetAccessibleDescription(const std::wstring&);
void SetAutomationId(const std::wstring&);     std::wstring GetAutomationIdOrAuto() const;  // 自动 e{n}
void SetAccessibleRole(AccessibleRole);  virtual AccessibleRole GetAccessibleRole() const;
virtual std::wstring DefaultAccessibleName() const;  virtual bool IsAccessibilityIgnored() const;
std::vector<UIElement*> GetAccessibleChildren() const;
// Pattern 虚函数（按角色分派）
virtual void AccessibilityInvoke();
virtual std::wstring GetAccessibleValue() const;  virtual void SetAccessibleValue(const std::wstring&);
virtual bool IsAccessibleReadOnly() const;
virtual int  GetAccessibleToggleState() const;  virtual void AccessibilityToggle();
virtual int  GetAccessibleExpandState() const;  virtual void AccessibilityExpand/Collapse();
virtual double GetAccessibleRangeValue() const; virtual void SetAccessibleRangeValue(double);
virtual double GetAccessibleRangeMin/Max/Step() const;
void AccessibilityNotifyFocus/PropertyChanged/StructureChanged();
```
- **`易错`**：角色决定暴露哪些 UIA Pattern：Button/MenuItem→Invoke；Edit/Document→Value；Slider/ProgressBar→RangeValue；toggle 状态 ≥0→Toggle；expand 状态 ≥0→ExpandCollapse。隐藏节点默认不暴露。
- **`版本`**：整套 = `v1.17.0`（对应 `Window::SetAccessibilityEnabled`，默认开）。

## 字体（元素级）

```cpp
static void SetGlobalFont(const FontSpec&);  void SetFont(const FontSpec&);
void SetFontFamily/SetFontSize/SetFontWeight(...);  void ClearFont();
virtual std::optional<FontSpec> GetTypeDefaultFont() const;  FontSpec GetEffectiveFontSpec() const;
```
- 解析链：**实例覆盖 → 类型默认 → 全局**。
- **`易错`**：写 `fontOverride_`（含 `std::wstring`）必须与渲染串行；已挂载元素的 `StoreFontOverride`/`InvalidateFontCache` 内部会取 `renderLock_`。

## 阴影

```cpp
void SetShadow(bool);  bool HasShadow() const;
void SetShadowColor(Color); SetShadowBlur(float); SetShadowOffset(float,float); SetShadowCornerRadius(float);
float GetShadowExtent() const;
```
- 阴影仅对**开启缓存**的元素生效（`Card` 设阴影会自动开缓存）。

## 调试计数（按元素）

`DebugMeasureCount/ArrangeCount/RepaintCount/DrawCount/CacheValid/CacheBytes/MeasureMs/ArrangeMs/DrawMs/AnimationProgress/ResetCounters` 等。
- **`版本`**：`v1.17.0` 加入的逐元素调试计数，配合调试通道 Top-N 使用。

###chapter: 布局系统 | Layout、ColumnBox、RowBox、GridLayout、Card、Page、PageHost

## Layout（布局基类）

```cpp
class Layout : public UIElement { bool UseCache() const override { return false; } };
```
- 布局容器**一律不缓存**。

## ColumnBox（垂直）/ RowBox（水平）

```cpp
ColumnBox(); RowBox();
void AddChild(std::shared_ptr<UIElement>);
void ClearChildren();
void SetSpacing(float);
```
- `ColumnBox` 默认水平填满、垂直不拉伸；`RowBox` 相反。
- **`易错`**：`AddChild` 会 `SetParent(this)`；容器用 `shared_ptr` 拥有子元素。子元素设了显式 `SetHeight` 时，剩余空间按权值**叠加**到该高度上。
- **`版本`**：`v1.17.0` 起按子元素拉伸权值分配剩余空间。

## GridLayout（网格）

```cpp
enum class Alignment { Start, Center, End };
void AddChild(std::shared_ptr<UIElement>, int row, int col, int rowSpan=1, int colSpan=1);
void SetSpacing(float h, float v);
void SetColumnStretch(int col, float weight);  void SetRowStretch(int row, float weight);
void SetHorizontalAlignment(Alignment);        void SetVerticalAlignment(Alignment);
```
- 未显式设行/列拉伸时，单元格权重取**所跨子元素权值的最大者**。

## LayoutHost / Card / Page

```cpp
class LayoutHost : public UIElement { std::shared_ptr<UIElement> GetLayout() const; void SetLayout(...); };
class Card  : public LayoutHost { DefaultPadding=12; DefaultCornerRadius=8; ... };
class Page  : public LayoutHost { DefaultPadding=10; ... };
```
- `LayoutHost` 是“单槽容器”，内部装一个布局元素；`Card` 是带圆角/描边/悬停的卡片；`Page` 是 `TabView`/`PageHost` 的页面容器。
- `Card::UseCache()` = `HasShadow()`；`Page::UseCache()` = false。

## PageHost（页面宿主 / 过渡）

```cpp
enum class TransitionDirection { Left, Right, Up, Down };
enum class TransitionEasing { Linear, EaseInOut, EaseOut };
void AddPage(std::shared_ptr<Page>);  void RemovePage(int);  void ClearPages();
void NavigateTo(int index);   void SetCurrentIndexInstant(int index);
void SetTransitionDirection(...);  void SetTransitionEasing(...);  void SetAnimationDuration(float);
int GetCurrentIndex() const;  std::shared_ptr<Page> GetCurrentPage() const;
```
- 过渡期间**同时**把源页与目标页作为可见子元素返回；`UpdateAnimation` 每页只更新一次（否则动画速度翻倍）。
- **`易错`**：`NavigateTo` 是**异步**的（动画结束才真正切到目标），`GetCurrentPage()`/`GetCurrentIndex()` 在动画期间可能还是旧值。
- **`版本`**：缓动 `v1.8.1`；**隐藏页缓存延迟释放（约 1.2s，整棵子树递归）** = `v1.18.0`——解决快速切页内存抖动式增长。

## TabView（页签）

见「容器与滚动 / 页签」章。

###chapter: 窗口 | Window（创建、显示、背景、标题栏、任务栏、DPI、渲染线程）

## 嵌套公开类型

```cpp
struct BackgroundParams { float blurAmount=30, saturation=1; D2D1_COLOR_F tint, luminosity; float noiseOpacity=0.02f; };
enum class AcrylicPreset { Legacy, Luminosity, Base, Thin };
struct CaptionMetrics { bool valid; float rightMargin, top, height, width; };
enum class TaskbarProgress { None=0, Indeterminate=1, Normal=2, Error=4, Paused=8 };
enum class ThumbButtonId : UINT { Play=0, Pause=1, Prev=2, Next=3, MaxId=4 };
struct JumpListItem { std::wstring title, arguments, target, iconPath; int iconIndex=0; };
enum class WindowCorner { Default, Square, Round, RoundSmall };
enum class ActivateMode { Raise, Activate, Foreground, All };
enum class OwnedMinimizePolicy { None, Hide, DisableMinimize };
```

## 静态配置

```cpp
inline static Backdrop DefaultBackdrop = Backdrop::None;
inline static DWORD     DefaultBackdropColor = 0xFFFFFFFF;   // 默认纯白，v1.17.0
static void SetDefaultBackdrop(Backdrop, DWORD tint = 0);
static void SetBackgroundParams(Backdrop which, const BackgroundParams&);
static void SetBackgroundParams(Backdrop which, AcrylicPreset);
static void SetDefaultFrameRateLimit(int fps);   // v1.14.2
static void SetDefaultAppIcon(HICON big, HICON small = nullptr);
static void SetProcessAppUserModelID(const std::wstring& id);
```

## 创建 / 生命周期

```cpp
bool Create(int w, int h, const std::wstring& title, int x=CW_USEDEFAULT, int y=CW_USEDEFAULT);
bool IsValid() const;   HWND GetHwnd() const;   int GetId() const;
void Show(); void ShowNoActivate(); void Hide(); void Raise(); void Close();
void Run();  int RunModal(Window* owner = nullptr);
void SetInputBlocked(bool);  bool IsInputBlocked() const;
std::unique_lock<std::recursive_mutex> LockRender();  void RenderNowSync();
```
- **`易错`**：`Create` **不自动 `Show`**（v1.8.0 起）。`RunModal` 用 `EnableWindow(FALSE)` 禁用 owner、**不使用** `IsDialogMessage`。
- **`易错`**：`LockRender()` 仅在 `ZUFYUI_RENDER_THREAD=1` 且改元素树数据时需要；默认单线程无需。
- **`版本`**：多窗口 `v1.6.0`；`SetInputBlocked`/模态 `v1.6.2`；`LockRender`/`RenderNowSync` `v1.16.0`。

## 三种建窗方式（选哪个）

| 方式 | 代码 | 所有权 | 消息循环 | 适用场景 |
|---|---|---|---|---|
| 栈上值 | `Window w; w.Create(...); w.Show(); app.Run();` | 你自己（作用域） | 共享 `Run()` | 单一主窗口、生命周期简单 |
| `shared_ptr` | `auto w = app.CreateWindow(...); w->Show(); app.Run();` | `shared_ptr` 引用计数 | 共享 `Run()` | 多窗口、需动态创建/销毁 |
| 模态 | `int r = w->RunModal(owner);` | 同 `shared_ptr` | **嵌套**循环，阻塞至关闭 | 对话框 / 确认框 |

- **`易错`**：`Window` **不可拷贝**；栈上用法要保证它**活得比 `Run()` 久**（否则消息循环仍在引用已析构窗口）。
- **`易错`**：`Application::CreateWindow` 返回的 `shared_ptr` **就是所有权**——丢掉指针窗口就销毁；`Create` 也不会自动 `Show`。

## 所有权 / 多窗口 / 尺寸

```cpp
void SetOwner(Window*);  Window* GetOwner() const;
std::vector<Window*> GetOwnedWindows() const;
void HideOwnedWindows(); void ShowOwnedWindows();
void SetPosition(int x, int y);  void SetSize(int w, int h);   // DIP
void SetMinSize(int w, int h);
void SetOwnedMinimizePolicy(OwnedMinimizePolicy);
```
- **`易错`**：`SetOwner` 要在 `Create` **之前**，否则会自带任务栏按钮、且不随 owner 最小化。
- **`易错`**：`SetMinSize` 会**覆盖**基于内容的最小尺寸计算。含长文本/图表的窗口若不设，最小尺寸可能被内容撑大（内部用 `rootElement->Measure(Size(0,0))`，宽度 0 时换行 Label 会按整行宽度上报）。**含图表的调试器窗口已显式 `SetMinSize`。**

## 根内容 / 捕获

```cpp
void SetRootLayout(std::shared_ptr<Layout>);
std::shared_ptr<ColumnBox> GetRootColumnBox() const;   // 默认根：ColumnBox margin(20) spacing 10
void SetContextMenu(std::shared_ptr<Menu>);  void CloseContextMenu();
UIElement* HitTestElement(float x, float y);
void CollectDragRegions();  bool PointInDragRegion(float,float) const;
void MarkRepaint(UIElement*);   // 标脏；只记录，不定帧（帧由定时器/动画驱动）
float GetDpiScale() const;  float GetClientWidthDip/HeightDip() const;
```
- **`易错`**：`MarkRepaint` 存**裸指针**，元素析构会自动注销；不要跨销毁持有外部指针。
- **`易错`**：`RequestRepaint()` → `MarkRepaint` **不会**主动排一帧。帧由 16ms `WM_TIMER`（忙时）或动画续帧驱动。历史教训：曾在这里强制排帧，导致“每次重绘都出一帧”的持续重渲染（严重卡顿），**已回退**。

## 外观 / 背景 / 圆角

```cpp
void SetBackdrop(Backdrop, DWORD tint=0);  Backdrop GetBackdrop() const;
void SetBackgroundColor(Color);            // 叠加在背景（空/亚克力/云母）之上的颜色
void SetWindowCorner(WindowCorner);
void SetContentOpacity(float opacity);     // 弹窗淡入
void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border);
void SetResizable(bool);   void SetTitle(const std::wstring&);
```
- 背景是**自绘**：Acrylic = 宿主背景 + 自带效果链；Mica = 缓存壁纸层。改参数用 `SetBackgroundParams` 或 `UIZSignals::ReloadAcrylic`。
- `DefaultBackdropColor` 默认纯白（v1.17.0）。`SetContentOpacity` 需合成后端就绪。
- **`版本`**：四层参数 `v1.8.1`；`SetContentOpacity` `v1.9.6`。

## 自定义标题栏

```cpp
void SetCustomTitleBar(std::shared_ptr<UIElement>);  std::shared_ptr<UIElement> GetCustomTitleBar() const;
bool HasCustomTitleBar() const;   void SetTitleBarVisible(bool);
CaptionMetrics QueryCaptionMetrics() const;
void Minimize(); void Maximize(); void Restore(); void MaximizeRestore();
bool IsMaximizedWindow() const;   void BeginSystemDrag();
```
- **`易错`**：自定义标题栏**不参与布局**——`Window` 把它放 `(0,0)`，并把根内容下移它测量出的高度。别手动 `AddChild`。
- **`版本`**：`v1.7.0` 起；`CaptionButton::Kind::Pin` = `v1.17.0`。

## 任务栏 / 缩略图 / 跳转列表

```cpp
void SetTaskbarProgress(TaskbarProgress, ULONGLONG done=0, ULONGLONG total=0);  void ClearTaskbarProgress();
void SetTaskbarOverlayIcon(HICON, const std::wstring& = L"");  void ClearTaskbarOverlayIcon();
ZSignal<ThumbButtonId> ThumbButtonClicked;
void SetThumbButtons(...);   void UpdateThumbButton(ThumbButtonId, bool enabled);
void SetJumpList(...);  void Flash(int times=5, bool alsoTaskbar=true);  void FlashUntilForeground();
void SetAppUserModelID(const std::wstring&);
```
- 这些是 COM 调用，失败**静默 no-op**。`FLASHW_CAPTION` 对自绘标题栏无效，用 `FLASHW_ALL`。
- **`版本`**：均 `v1.9.6`。

## DPI

- 进程级 Per-Monitor-v2 在静态初始化设置（回退 `SetProcessDPIAware`）。
- `WM_DPICHANGED` 会重建整条渲染链并清空所有缓存。
- **`易错`**：元素缓存会在 DPI 变更时被清；但子布局**不需要**你手动重测。

## 渲染线程 / 帧率

```cpp
void SetFrameRateLimit(int fps);   int GetFrameRateLimit() const;   // 0=跟随刷新；>0=限帧
```
- **`易错`**：`ZUFYUI_RENDER_THREAD` **默认 0**，文档标注不稳定、不建议开（空白弹窗、悬停/提示延迟、大虚拟列表可能崩）。单线程 `WM_PAINT` 是受支持路径。
- **`版本`**：渲染线程 `v1.14.0`；vblank 节拍 `v1.14.1`；帧率上限 `v1.14.2`；默认关并标注 `v1.16.0`。

## 无障碍 / 信号

```cpp
void SetAccessibilityEnabled(bool);   bool IsAccessibilityEnabled() const;   // 默认开，v1.17.0
UIElement* GetFocusedElement/HoveredElement/PressedElement() const;
void FocusElement(UIElement*);
ZSignal<> Activated, Deactivated;   ZSignal<bool*> Closing;   ZSignal<> Closed;
ZSignal<> BackdropUnsupported, DeviceLost;   ZSignal<HRESULT> RenderingError;
std::shared_ptr<Timer> CreateTimer(int intervalMs = 1000);
```
- **`易错`**：`Closing` 里把 `*cancel = true` 可取消关闭。`Closing` 的参数是 `bool*`（不是引用）。

###chapter: 基础控件 | Label、Button、TextBox、TextEdit、ComboBox

> 本类控件共享的坑：文本布局按 `(文本, 盒, 格式)` 全局缓存；改内部字符串务必走 setter（否则绕过失效逻辑）；`Color` 分量是 [0,1]；`SetEnabled(false)` ≠ `SetVisible(false)`。

## Label

```cpp
enum class TextOverflow { Wrap, Ellipsis };
enum class HAlign { Left, Center, Right };   enum class VAlign { Top, Center, Bottom };
Label(const std::wstring& text = L"Label");
void SetText(const std::wstring&);   std::wstring GetText() const;      // 失效布局 + 重绘
void SetTextFast(const std::wstring&);   // 仅池化 rebind：自身脏、不冒泡、不重绘
void SetTextColor(Color);   void SetTextOverflow(TextOverflow);
void SetAlignment(HAlign, VAlign);   void SetPadding(const Thickness&);
void SetBackgroundColor(Color);  void SetBackgroundCornerRadius(float);
void SetLineSpacing(float);  void SetMaxLines(int);   // 仅在 Wrap 下有意义
void SetImage(std::shared_ptr<Image>);   void SetIcon(Icon, float size=0);
void AddChild(std::shared_ptr<UIElement>);   // 图标/文本之后的内联子元素
std::wstring GetToolTip() const override;   // 省略时自动全量 tooltip（v1.17.0）
```
- **`易错`**：`SetTextFast` 不重绘——池化容器（ListView/TableView）用它 + 容器自己重绘。直接用它设文本可能“看不到更新”。
- **`易错`**：同时设了 image 与 glyph 时 **image 优先**。
- **`易错`**：`GetDesiredSize()` 非 const 且用 `FLT_MAX` 测量，忽略父约束。
- **`版本`**：`SetIcon` 家族 `v1.13.0`；省略自动 tooltip `v1.17.0`。

## Button

```cpp
ZSignal<> Clicked;   ZSignal<bool> Toggled;     // Toggled 需先 SetCheckable(true)
Button(const std::wstring& text = L"Button");
void SetText/SetIcon/SetIconColor(...);
void SetColors(Color normal, Color hover, Color pressed);   void SetTextColor(Color);
void SetCornerRadius(float);   void SetHoverAnimationSpeed(float);
void SetCheckable(bool);  void SetChecked(bool);  bool IsChecked() const;
void SetAutoRepeat(bool enable, float intervalMs=400);   // 长按重复
```
- 默认 `120×36`；按下+抬起都在内部才 `Clicked`。
- **`易错`**：非 checkable 时 `IsChecked()` 无意义。内部 Label 不要手动 parent。

## TextBox（单行）

```cpp
ZSignal<const std::wstring&> TextChanged;   ZSignal<> ReturnPressed;
void SetText(const std::wstring&);   // 会去掉 \r\n、截断到 maxLength、清撤销
void SetPlaceholder(...);  void SetPasswordMode(bool);  void SetMaxLength(int);   // -1 不限
void SetReadOnly(bool);    void SetInputFilter(std::function<bool(wchar_t)>);
void SetError(bool);   void SetImeEnabled(bool);        // v1.12.0
void SelectAll/Copy/Cut/Paste/Undo/Redo();
int GetSelectionStart/End/CursorPosition() const;  void SetSelection(int,int);
```
- **`易错`**：`SetImeEnabled(false)` 会抑制 IME/屏幕键盘（`IsTextInput()` 返回 `imeEnabled_`）。
- **`易错`**：程序化 `SetText` **不产生撤销步**。
- **`版本`**：error/indicator/IME `v1.12.0`；`v1.18.0` 修 shift+点击语义（无选区时也自锚点扩展）。

## TextEdit（多行编辑器 / 只读查看器）— v1.18.0

> `QPlainTextEdit` 风格：行号、换行、Tab 缩进、跨行选择、剪贴板、撤销/重做、查找、只读富文本。

```cpp
enum class WrapMode { NoWrap, WidgetWidth };
ZSignal<const std::wstring&> TextChanged;   ZSignal<int,int> CursorPositionChanged;  ZSignal<> SelectionChanged;
std::wstring ToPlainText() const;  void SetPlainText(const std::wstring&);   // 清撤销
void AppendPlainText(const std::wstring&);  void InsertPlainText(const std::wstring&);
void SetRichText(const std::vector<std::vector<TextRun>>&);   // 强制只读；TextRun{ text, font, color, underline }
void SetReadOnly(bool);  void SetShowLineNumbers(bool);  void SetShowNewlines(bool);
void SetWrapMode(WrapMode);  void SetTabSize(int);  void SetInsertSpaces(bool);
int GetCursorLine/Column() const;  void SetCursorLineColumn(int,int);   // 0 基
void SelectLine(int);  void SelectWordAt(int);  bool Find(const std::wstring&, bool fwd=true, bool matchCase=false);
void EnsureCursorVisible();  float GetScrollOffsetY() const;  void SetScrollOffsetY(float);
bool UseCache() const override { return false; }
bool AcceptsTab() const override { return !readOnly_; }   // 依赖基类新虚函数
```
- **`易错`**：换行宽度变化会**重建每一行的 layout**（`ArrangeOverride` 检测 `wrapWidth_`）。
- **`易错`**：富文本**构造即只读**。行/列 API 是 **0 基**。
- **`易错`**：因 `AcceptsTab` 为真，Tab 是**插入缩进**而非切换焦点。
- 默认 `Consolas 14`、`320×160`、tabSize 4。

最小示例：
```cpp
auto edit = std::make_shared<TextEdit>();
edit->SetPlainText(L"hello\nworld");
edit->SetShowLineNumbers(true);
root->AddChild(edit);                 // 记得给尺寸或让它随布局铺满
```

## ComboBox

```cpp
ZSignal<int> SelectionChanged;   ZSignal<> DropDownOpened, DropDownClosed;
void AddItem/SetItems/InsertItem/RemoveItemAt/ClearItems(...);
void SetSelectedIndex(int);  int GetSelectedIndex() const;   // 过滤后“可见序”
void SetSelectedSourceIndex(int);  int GetSelectedSourceIndex() const;   // v1.15.0：源序
void SetItemDisabled(int visibleIndex, bool);   // 按源索引持久
void SetEditable(bool);  void SetFilterEnabled(bool);  void ApplyFilter();
```
- **`易错`**：下拉画在全局 `DrawOverlay`，可超出控件边界。
- **`易错`**：`GetSelectedIndex()` 是**过滤后可见序**；`SetItemDisabled` 按**源索引**持久（v1.15.0 修）。
- **`版本`**：下拉几何/状态 `v1.12.0`；源索引 API `v1.15.0`；下拉箭头随展开进度旋转（smoothstep，`v1.19.0`）。

###chapter: 选择与输入 | CheckBox、RadioButton、RadioGroup、ToggleSwitch、Slider、NumberBox

> 本类共性：多个控件会**同时**发“过程信号”和“提交信号”；静态 `DrawBox` 与数据视图共用，改外观会同时影响列表/表格/树的勾选行。

## CheckBox

```cpp
enum class State { Unchecked, PartiallyChecked, Checked };
ZSignal<bool> Toggled;   ZSignal<State> StateChanged;
void SetChecked(bool);  void SetState(State);  void Toggle();  void SetTriState(bool);
static void DrawBox(...);   // 可复用的勾选框绘制（数据视图也用）
```
- **`版本`**：早期加入（`1d886f5`，v1.6.0 之前）。

## RadioButton / RadioGroup

```cpp
class RadioButton { ZSignal<bool> CheckedChanged; ZSignal<> Clicked; void SetChecked(bool); ... };
class RadioGroup  { enum class Orientation { Vertical, Horizontal };
                    ZSignal<int> SelectionChanged;
                    std::function<bool(int newIdx, int oldIdx)> SelectionChanging;   // 返回 false 可否决
                    int AddItem(const std::wstring&, bool selected=false); std::shared_ptr<RadioButton> GetButton(int index) const; int GetItemCount() const; void SetSelectedIndex(int); ... };
```
- **`易错`**：单独一个 `RadioButton` 不做互斥；互斥由 `RadioGroup` 保证。
- **`版本`**：`v1.11.0`。

## ToggleSwitch

```cpp
ZSignal<bool> Toggled;   void SetOn(bool);  bool IsOn() const;
void SetIndeterminate(bool);   // 半选外观
```
- 默认 50×24。

## Slider

```cpp
ZSignal<float> ValueChanged;   ZSignal<> SliderReleased;
void SetRange(float min,float max);  void SetValue(float);  void SetStep(float);  void SetSnapToStep(bool);
```
- **`易错`**：`ValueChanged` 在拖动中**连续**触发；提交用 `SliderReleased`。
- **`版本`**：`v1.18.0` 修 0 宽时 0/0→NaN 静默把值设成 min/max。

## NumberBox（+ 内部 SpinButton）

```cpp
ZSignal<double> ValueChanged;   ZSignal<const std::wstring&> TextChanged;
void SetValue(double, bool fire=true);  void SetRange(double lo,double hi);
void SetStep(double);  void SetDecimals(int);  void SetWrap(bool);
void SetDefaultValue(double);  void ResetToDefault();
```
- **`易错`**：**空文本按“错误”处理**是控件行为（不是调用方责任）；步进按钮会先 `CommitText()`，所以未失焦的编辑值不会丢。
- **`版本`**：`v1.12.0`；`ResetToDefault` 清错误 `v1.12.2`。

###chapter: 进度 | ProgressBar、ProgressRing

## ProgressBar

```cpp
ZSignal<float> ValueChanged;
void SetValue(float);          // 归一化到 [0,1]
void SetRange(float min,float max);  void SetRangeValue(float);   // [min,max]
void SetIndeterminate(bool);   void SetIndeterminateBlockWidth(float);  void SetIndeterminateSpeed(float);
```
- **`易错`**：`SetValue` 是 `[0,1]`，`SetRangeValue` 是 `[min,max]`，**别混用**。

## ProgressRing

```cpp
void SetValue(float);  void SetIndeterminate(bool);  void SetThickness(float);
bool UseCache() const override { return false; }   // 每帧重绘
```
- **`版本`**：`v1.12.0`；`UseCache=false` `v1.12.2`；NaN/Inf 守卫 `v1.14.0`。

###chapter: 容器与滚动 | ScrollViewer、ScrollBar、SplitView、TabView

## ScrollViewer

```cpp
enum class ScrollBarVisibility { Auto, Always, Hidden };
ZSignal<float,float> ScrollChanged;   // (offsetX, offsetY)，动画中也发
void SetContent(std::shared_ptr<UIElement>);
void SetVertical/HorizontalScrollEnabled(bool);
void SetVertical/HorizontalScrollBarVisibility(ScrollBarVisibility);
void SetScrollBarWidth(float);  void SetScrollWheelStep(float);  void SetAnimationSpeed(float);
void ScrollTo(float x,float y,bool animated=true);  void ScrollBy(...);
float GetScrollOffsetX/Y() const;
```
- **`易错`**：`Hidden` 仍允许程序化/滚轮滚动（只是不显示条）。
- **`版本`**：`v1.18.0` 起 `ScrollViewer::UseCache()==false`（内容常变/可滚动，缓存无益且占大位图）。

## ScrollBar（独立可复用）

```cpp
std::function<void(float value, bool animate)> ValueChanged;
explicit ScrollBar(bool vertical);
void SetRange(float value, float maxValue, float viewportSize);   // 宿主每帧推入
void SetBarWidth(float);  void SetMinLength(float);  void SetHitExtra(float);
void SetColors(D2D1_COLOR_F thumb, D2D1_COLOR_F hoverThumb, D2D1_COLOR_F track);
void SetIdleDelay(float);  void SetAutoShrink(bool);  void MarkActive();
inline static float ShrunkWidthRatio;   // 默认 0.30
```
- 拖动 → `animate=false`（跟手）；点轨道 → `animate=true`。悬停扩张 + 空闲缩小；宿主用 `ShrunkWidthRatio` 预留槽位。
- **`易错`**：`ValueChanged` 是 `std::function`（不是 `ZSignal`），签名 `(float, bool)`。
- **`易错`**：`ShrinkTarget` 的“空闲等待缩小”倒计时**不再**算作活跃动画——否则会把整窗顶进动画模式导致数据重绘不落屏。代价：空闲缩小变**惰性**（到点后由下一帧触发）。
- **`版本`**：`v1.11.0` 起独立复用；`ShrunkWidthRatio` `v1.13.0`。

## SplitView

```cpp
enum class Orientation { Vertical, Horizontal };   // Vertical = 左右两栏 + 竖直分隔条
ZSignal<float> SplitChanged;   // 新比例 0..1
void SetFirst/SetSecond(std::shared_ptr<UIElement>);
void SetSplitRatio(float);  void SetSplitterWidth(float);  void SetMinFirst/MinSecond(float);
bool UseCache() const override { return false; }
```
- **`易错`**：`Orientation` 命名反直觉（`Vertical` 指竖直分隔条、水平排列两栏）。
- **`版本`**：`v1.12.0`；NaN/Inf 守卫 `v1.14.0`。

## TabView

```cpp
struct Tab { std::shared_ptr<Label> label; std::shared_ptr<Page> page; bool closable; ... };
ZSignal<int> SelectionChanged;   ZSignal<int> TabCloseRequested;
int  AddTab(const std::wstring&, std::shared_ptr<UIElement> content=nullptr, bool closable=false);
int  AddTab(std::shared_ptr<Label>, ...);
void InsertTab(int, ...);  void RemoveTab(int);  void ClearTabs();
void SetTabTitle(int, ...);  void SetTabLabel(int, std::shared_ptr<Label>);  void SetTabContent(int, ...);
void SetSelectedIndex(int);  int GetSelectedIndex() const;
void SetTransitionDirection(PageHost::TransitionDirection);  void SetAutoTransitionDirection(bool);
```
- **`易错`**：内容由内部 `PageHost` 承载；`AddTab` 首个页签会自动选中并触发 `SelectionChanged`。
- **`易错`**：`SetSelectedIndex`/切换是**异步**的（过渡动画结束才定），`GetSelectedContent()`/索引在动画期间可能仍是旧值。
- **`版本`**：`v1.11.0`；Label 化标题 + `SetTabLabel` `v1.13.0`；加锁 `v1.15.0`。

###chapter: 菜单与弹窗 | Menu、MenuItem、MenuWindow、MessageBox

## MenuItem

```cpp
enum class Type { Normal, Separator, Submenu };
std::wstring text;   ZSignal<> Clicked;   std::shared_ptr<Menu> submenu;
Type type = Normal;   bool enabled = true;   int id = 0;
bool checkable = false, checked = false, radio = false, isDefault = false, danger = false;
std::wstring shortcut;   std::optional<Color> bgColor, textColor;   std::optional<FontSpec> font;
std::shared_ptr<Label> icon;
```

## Menu

```cpp
void AddItem(const std::wstring& text, std::function<void()> cb = nullptr, int id = 0);
void AddCheckItem(const std::wstring& text, bool checked, std::function<void(bool)> onToggle = nullptr, int id=0, bool radio=false);
void AddSeparator();   void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu, int id=0);
std::vector<std::shared_ptr<MenuItem>> items;
std::function<void(Menu&)> onOpening;   ZSignal<int> ItemSelected;
void ShowAt(int screenX, int screenY);   void ShowAtCursor();   // 独立弹出（托盘/右键）
```
- **`易错`**：菜单项回调会 `PostToUIThread` **推迟到当前消息之后**执行（因为回调常开模态框，若在当前 WndProc 栈上关菜单会触发 UAF）。

## MenuWindowBase / MenuWindow

```cpp
class MenuWindowBase : public Window {
    bool CreatePopup(int widthDip, int heightDip, int screenX, int screenY);
    void ShowAtPoint(int x, int y);  void Hide();  void CloseAll();
    void SetStandalone(bool);  bool IsPointInPopupTree(POINT ptScreen);
    virtual bool OnMenuKeyDown(int vk);
};
class MenuWindow : public MenuWindowBase {
    MenuWindow(std::shared_ptr<Menu> menu, HWND ownerHwnd, int x, int y);
};
```
- 弹出窗口用 `WS_EX_NOACTIVATE|TOOLWINDOW|TOPMOST`、无 DWM 边框，`WM_MOUSEACTIVATE` 返回 `MA_NOACTIVATE` 不抢焦点。
- **`易错`**：`CloseAll()` 只销毁 HWND；对象的 `shared_ptr`/`unique_ptr` 在消息处理器**之外**析构。

## MessageBox / FastButton

```cpp
enum class FastButton : unsigned { None=0, OK=1<<0, Cancel=1<<1, Apply=1<<2, Close=1<<3, Yes=1<<4, No=1<<5, Help=1<<6 };
bool HasFlag(FastButton set, FastButton flag);
class MessageBox : public Window {
    enum class Icon { None, Info, Warning, Error, Question };
    enum class Lang { English, Chinese };
    MessageBox(Window* parent, const std::wstring& title, const std::wstring& content,
               Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true);
    FastButton GetResult() const;   ZSignal<FastButton> ButtonClicked;   void EndDialog(FastButton);
    void SetUserContent(std::shared_ptr<UIElement>);
};
```
- 按钮顺序 Yes/No/OK/Apply/Cancel/Close/Help；**Enter = 最左按钮**，**ESC = Cancel/Close**（否则最左）。
- **`易错`**：`ZufyUIWindowTool.h` 里 `#undef` 了 Win32 `MessageBox` 宏——调用 Win32 请用 `MessageBoxW/A`。
- **`易错`**：`blocking=true` 会在**构造函数内部**跑模态循环；`SetUserContent` 后没有预设按钮，必须自己 `EndDialog`。

###chapter: 数据视图 | ListView、TableView、TreeView、TreeNode（虚拟化、隐藏/筛选/可见序）

> **本章是易错重灾区，先读这段。**
> - 自 **v1.15.0** 起三个视图都是**视口虚拟化 + Label 池化**。
> - 存在**两套坐标**：**源行号**（稳定，旧 API/信号用它）与**可见序**（隐藏/筛选/排序之后）。**混用是头号 bug 来源。**
> - 虚拟模式下每行没有持久 `Label`；`SetTextFast` 是池 rebind 路径。
> - 所有数据改动应与渲染串行（视图提供 `RenderGuard()`）。

## ListView

```cpp
enum class SelectionMode { Single, Extended, Multi, None };
ZSignal<int> SelectionChanged, ItemClicked, ItemDoubleClicked, ItemRightClicked;
ZSignal<std::vector<int>> SelectionChangedMulti;   ZSignal<int,bool> ItemCheckStateChanged;
float ContentToLocalX/Y(float) const;   float LocalToContentX/Y(float) const;   // v1.10.0
void AddItem(const std::wstring&);   void SetItem(int, const std::wstring&);
std::wstring GetItemText(int) const;   int GetItemCount() const;
void SortItems(std::function<bool(const std::wstring&,const std::wstring&)>);   void ScrollToItem(int);
void BeginUpdate();   void EndUpdate();
// 虚拟化（v1.15.0）
void SetItemCount(int n);
void SetItemHidden(int src, bool=true);   bool IsItemHidden(int src) const;   void ClearHidden();
void SetFilter(std::function<bool(int)>);  void ClearFilter();
void SetViewComparator(std::function<bool(int,int)>);  void SortView(bool ascending=true);  void ClearViewSort();
int  VisibleCount() const;   int SourceOfVisible(int vi) const;   int VisibleOfSource(int src) const;
std::wstring TextAtVisible(int vi) const;   void SetSelectedVisible(int vi);   int GetSelectedVisible() const;
// 勾选 / 每项元数据
void SetCheckable(bool);  void SetItemChecked(int src,bool);  std::vector<int> GetCheckedIndices() const;
void SetItemDisabled(int src,bool=true);  void SetItemTextColor(int src,Color);  void SetItemToolTip(int src,const std::wstring&);
```
- **`易错`**：`Sort()` 会重排 `items_` 并**清空多选/勾选**；`SortView` 只改**可见序**。
- **`易错`**：池化下改行文本走 `SetTextFast`，且容器宽度变化必须重测（`v1.18.0` 修：新增 `poolW_`，宽度变也重测，修省略号错位）。
- **`版本`**：`BeginUpdate` `v1.9.6`；坐标转换 `v1.10.0`；虚拟化/隐藏/筛选/可见序 `v1.15.0`。

## TableView

```cpp
enum class SelectionMode { Cell, Row, Column, None };
static long long CellKey(int r,int c); static int CellKeyRow(long long); static int CellKeyCol(long long);
ZSignal<int,int> CellClicked, CellDoubleClicked, CurrentCellChanged, CellRightClicked;
ZSignal<int> HeaderClicked;  ZSignal<std::vector<std::pair<int,int>>> SelectionChangedCells;
void SetRowCount(int);  void SetColumnCount(int);  void AppendRow/Column(); InsertRow/Column(int); RemoveRow/Column(int);
void SetItem(int row,int col,const std::wstring&);  std::wstring GetItemText(int r,int c) const;
void SetHeaderLabel(int,const std::wstring&);  void SetColumnWidth(int,float);   // 最小 40
void SetColumnAlignment(int, TextHAlign);   void SetRowHeight(float);  void SetRowHeightAt(int,float);
void SetColumnVisible(int,bool);
// 虚拟化（v1.15.0，全部按“源行”）：SetItemHidden/SetFilter/SetViewComparator/SortView/VisibleCount/SourceOfVisible/VisibleOfSource
// 选中 / 勾选 / 元数据
void SetSelectionMode(SelectionMode);  void SetCurrentCell(int,int);  void SelectRow(int);  void GetSelectedCells();
void SetCheckable(bool);  void SetRowChecked(int,bool);  void SetRowDisabled(int,bool);
void SetCellTextColor(int,int,Color);   void SetCellToolTip(int,int,const std::wstring&);
void SetColumnComparator(int, std::function<bool(const std::wstring&,const std::wstring&)>);  void SortByColumn(int,bool=true);
```
- **`易错`**：默认 `Cell` 选择；行/单元格元数据随插入/删除/排序**重映射**；隐藏列不参与命中。

## TreeNode（普通结构体）

```cpp
enum class CheckState { Unchecked, PartiallyChecked, Checked };
std::vector<std::shared_ptr<Label>> columns;   // 每列一个真实 Label；col0 = 节点文本
std::weak_ptr<TreeNode> parent;                // 弱引用！用 .lock()
std::vector<std::shared_ptr<TreeNode>> children;
bool expanded;  int depth;  void* userData;
std::wstring icon;  bool checkable;  CheckState checkState;  bool selected;  bool enabled;
std::wstring tooltip;   Color bgColor = Color(0,0,0,0);   // 行背景，v1.17.0
```
- **`易错`**：`parent` 是 `weak_ptr`；行背景用 **`node->bgColor`** 直接字段（**没有** `SetNodeBackgroundColor`）。
- **`版本`**：`bgColor` `v1.17.0`。

## TreeView

```cpp
enum class SelectionMode { Single, Extended, Multi };   enum class CheckMode { Linked, Independent };
ZSignal<std::shared_ptr<TreeNode>> SelectionChanged, NodeClicked, ItemDoubleClicked, ItemRightClicked;
ZSignal<std::shared_ptr<TreeNode>,bool> ExpandChanged;   ZSignal<int> HeaderClicked;
ZSignal<std::shared_ptr<TreeNode>, TreeNode::CheckState> ItemCheckStateChanged;
void SetColumnCount(int);  void SetHeaderLabels(const std::vector<std::wstring>&);  void SetColumnWidth(int,float);
std::shared_ptr<TreeNode> AddRoot(const std::wstring&);   std::shared_ptr<TreeNode> AddChild(parent, const std::wstring&);
void RemoveNode(node);  void RemoveChildren(node);  void MoveNode(node, newParent, int index);  void Clear();
void BeginUpdate();  void EndUpdate();                       // v1.15.0：延后 BuildVisibleList（避免 O(N^2)）
void SetNodeText(node,int col,const std::wstring&);   void SetNodeTooltip(node, ...);   void SetNodeIcon(node, ...);
void ExpandNode(node,bool);  void ExpandAll();  void CollapseAll();  void ExpandToDepth(int);
void SetDefaultExpandDepth(int);                             // 只影响之后插入的节点
void SetFilter(std::function<bool(const std::shared_ptr<TreeNode>&)>);   void Search(const std::wstring&);
void SetSelectionMode(SelectionMode);  void SetSelectedNode(node);  std::vector<std::shared_ptr<TreeNode>> GetSelectedNodes() const;
void SetCheckable(bool enable, bool recursive=true);  void SetCheckMode(CheckMode);
void SetNodeCheckState(node, TreeNode::CheckState, bool updateChildren=true, bool updateParent=true);
float GetScrollOffsetY() const;   void SetScrollOffsetY(float);   void ScrollToNode(node);   // v1.17.0
```
- **`易错`**：`Search` 保留命中节点**及其祖先链**；`SetDefaultExpandDepth` 只影响**未来**插入；`CheckMode::Linked` 自动算 `PartiallyChecked`。
- **`易错`**：`GetVisibleNodeAt` 依赖**惰性重建**的可见列表，别跨树改动持有索引。
- **`版本`**：`BeginUpdate`/`SetNodeText`/tooltip + 并发 `v1.15.0`；首列省略号修 `v1.16.0`；`Get/SetScrollOffsetY` + `ScrollToNode` + 节点 `bgColor` `v1.17.0`。

###chapter: 图表 | ChartBase、BarChart、LineChart、PieChart（v1.18.0）

> 头文件 `ZufyUICharts.h`。`ChartBase` 集中处理绘图几何、Nice 刻度、类别轴、网格、图例、调色板、内部滚动条、吸附轴、悬停/命中、Ctrl 缩放、拖动平移、进场动画；`BarChart`/`LineChart` 只实现 `DrawData`。

最小示例：
```cpp
#include "ZufyUICharts.h"
auto chart = std::make_shared<BarChart>();
chart->AddSeries(L"销量");
chart->AddCategory(L"Q1"); chart->AddCategory(L"Q2"); chart->AddCategory(L"Q3");
chart->SetSeriesValues(0, { 12.0, 30.0, 22.0 });
chart->SetWidth(420); chart->SetHeight(260);   // 图表尺寸来自自身，必须显式给或给布局权值
root->AddChild(chart);
```

## ChartBase

```cpp
enum class AText { Left, Center, Right };   enum class AVert { Top, Center };
struct Series { std::wstring name; std::vector<double> values; Color color; };
inline static std::vector<Color> DefaultPalette;   // 8 色
ZSignal<int,int> PointClicked;    // (series, category)
ZSignal<int> CategoryClicked;     // (category)

// 数据
void AddCategory(const std::wstring&);
void AddSeries(const std::wstring& name, const std::vector<double>& v = {});
void SetValue(int s, int c, double);
void SetSeriesValues(int s, const std::vector<double>&);      // 重置并重播进场动画
void UpdateSeriesValues(int s, const std::vector<double>&);   // 只重绘，不重播动画（实时刷新用）
void ClearData();
int CategoryCount() const;   int SeriesCount() const;

// 配置
void SetPalette(const std::vector<Color>&);   void SetSeriesColor(int s, Color);
void SetValueAxisRange(double min, double max);   void SetValueAxisAutoRange();
void SetValueTickCount(int n);   void SetValueTickStep(double);
void SetValueFormat(int decimals, const std::wstring& prefix=L"", const std::wstring& suffix=L"");
void SetShowGrid(bool);  void SetShowVGrid(bool);  void SetShowLegend(bool);  void SetShowValues(bool);
void SetShowCategoryLabels(bool);  void SetLabelFont(const FontSpec&);   void SetColors(Color axis, Color grid, Color label);
void SetStickyAxes(bool);   // 吸附轴（默认开）
void SetAnimationEnabled(bool);   void SetAnimationDuration(float);
void SetCategoryWidth(float);   void SetPlotHeight(float);   void SetFitPlotSize(bool);
void SetReferenceLine(double value, const std::wstring& label=L"", Color c=...);   void ClearReferenceLine();
void SetVReferenceLine(int category, const std::wstring& label=L"", Color c=...);  void ClearVReferenceLine();

// 扩展点
virtual void DrawData(ID2D1RenderTarget*) = 0;
virtual void ComputeValueRange(double& lo, double& hi);
virtual void UpdateHover(float,float);   virtual void OnDataClick(float,float);
virtual std::wstring HoveredText() const;
```
- **`易错`**：图表尺寸来自自身的 `width_/height_`（默认 `420×260`）。**只 `SetFillWidth/Height` 不会改变图表尺寸**——要显式 `SetWidth/SetHeight`，或用布局的拉伸权值让其铺满。图表默认拉伸权值为 0。
- **`易错`**：自定义 `ChartBase` 子类**必须调用 `InitScrollBars()`**（`BarChart`/`LineChart` 构造里已调），否则内部滚动条为空。
- **`易错`**：实时数据用 `UpdateSeriesValues`（不重播动画），别用 `SetSeriesValues`（会重置进场动画，配合 250ms 刷新会一直闪）。
- **`易错`**：`SetValueAxisRange(min,max)` 仅当 `max>min` 生效；滚轮 `Ctrl`=缩放(0.4..3)、`Shift`=横向滚动、否则纵向；拖动位移 > 4 DIP 才判为平移。
- **`易错`**：折线**面积填充的基线夹在可见轴范围内**（`ValueY(clamp(0, axisMin, axisMax))`），自动量程下不会填满整块。`LineChart` 进场为左→右揭示。

## BarChart

```cpp
enum class Orientation { Vertical, Horizontal };   // v1.18.0补全 Horizontal 实现
enum class StackMode { Grouped, Stacked, PercentStacked, Overlapped };
void SetOrientation(Orientation);  void SetStackMode(StackMode);
void SetBarGap(float 0..0.9);  void SetStackBarRatio(float 0.1..1);  void SetGroupGap(float 0..0.9);
void SetCornerRadius(float);  void SetBarOutline(float w, Color c=...);
void SetBarColor(int s, int c, Color);  void SetSelectedCategory(int c);
```

## LineChart

```cpp
enum class Marker { None, Circle, Square };
void SetLineWidth(float);  void SetSmooth(bool);  void SetDashed(bool);
void SetShowMarkers(bool);  void SetMarkerSize(float);  void SetMarker(Marker);
void SetAreaFill(bool on, float alpha=0.25f);
```

## PieChart（独立 UIElement）

```cpp
enum class LabelMode { None, Percent, Value, LabelAndPercent };
ZSignal<int> SliceClicked;
void AddSlice(const std::wstring& label, double value, Color = auto);
void Clear();   int SliceCount() const;
void SetDonut(float ratio);   // 0=饼图，0.5=环形
void SetStartAngle(float deg);   void SetClockwise(bool);   void SetSliceGap(float deg);
void SetLabelMode(LabelMode);   void SetShowLegend(bool);   void SetPalette(...);
void SetZoom(float);   float GetZoom() const;   // v1.18.0：滚轮缩放 0.4..4
bool OnMouseWheel(float,float) override;   // 滚轮缩放；绘制半径与命中检测同用 zoom_
void SetAnimationEnabled(bool);
```
- **`易错`**：`PieChart` 默认拉伸权值为 0，需要显式尺寸或布局权值。
- **`易错`**：标签带引导线，**右半边文字左对齐接在引线末端右边、左半边右对齐贴在末端左边**。

###chapter: 窗口周边 | 图像、托盘、系统对话框、应用注册

## Image / ImageManager / ImageDeviceCache

```cpp
class Image { int Width() const; int Height() const; bool IsNull() const; ... };   // 从文件/资源/HICON/流加载
class ImageDeviceCache { /* 按渲染目标缓存的设备位图 */ };
```
- **`易错`**：`ImageDeviceCache::Get` 内部“锁外 GPU 上传 + 锁内 emplace”。**仅当 `ZUFYUI_RENDER_THREAD=1`** 才有与 `DeviceReset` 并发的极小悬垂窗口；单线程路径无风险。
- 设备丢失时（`UIZSignals::DeviceReset`）应释放与渲染目标绑定的位图缓存。

## TrayIcon

```cpp
bool Add(HICON icon, const std::wstring& tooltip, UINT id=1);
void SetIcon(HICON);  void SetToolTip(const std::wstring&);  void SetMenu(std::shared_ptr<Menu>);
void SetBadge(Color = ...);   void ClearBadge();
enum class BalloonIcon : DWORD { None, Info, Warning, Error, Custom };
void ShowBalloon(const std::wstring& title, const std::wstring& text, BalloonIcon = BalloonIcon::Custom, HICON = nullptr, ...);
ZSignal<> Clicked, DoubleClicked, RightClicked, HoverEnter, HoverLeave, BalloonClicked, ...;
```
- 相同 `id` 再次 `Add` 变 `NIM_MODIFY`；Explorer 重启后自动重加。
- **`易错`**：NOTIFYICON_VERSION_4 下右键以 `WM_CONTEXTMENU` 在回调里到达，**双击需手动计时**（没有 `WM_LBUTTONDBLCLK`）。自定义回调消息 `WM_APP+0x400`。
- **`版本`**：`v1.16.0` 修回调消息撞号。

## 系统对话框 — FileDialog / ColorDialog（v1.16.0）

```cpp
struct FileFilter { std::wstring label; std::vector<std::wstring> patterns; };
struct FileDialogOptions {
    std::wstring title, initialDir, defaultFileName, defaultExtension;
    std::vector<FileFilter> filters; int filterIndex = 1;
    bool addAllFiles = true, pickFolders = false, multiSelect = false, save = false, forceFilesystem = true;
};
```
- 基于 `IFileDialog`/`ChooseColor`；`forceFilesystem=true` 强制文件系统路径（否则可能返回虚拟项）。

## 应用注册 AppInfo / RegisterApp

```cpp
struct AppInfo { std::wstring displayName, aumid; std::shared_ptr<Image> icon; };
bool RegisterApp(const AppInfo&);     // 或 Application::Instance().RegisterApp(info)
```
- 默认只设 AUMID（零文件/注册表副作用）；定义 `ZUFYUI_ALLOW_APP_REGISTRATION` 才会写 `%LOCALAPPDATA%\ZufyUI\AppReg\...` 与 `HKCU\...\AppUserModelId`（让 Win10/11 toast 显示应用名/图标），退出自动清理。

###chapter: 拖放与卡片组件 | Drag & Drop、Expander、SettingsList（v1.19.0）

> 拖放由 `ZufyUIDragDrop.h` 提供（`ZufyUI.h` 末尾已包含）。应用内默认 MOVE，跨应用 COPY。

```cpp
// ---- 拖放：载荷 / 事件 ----
class DragData {                                   // 拖入方只读访问
    bool HasText() const;   std::wstring GetText() const;
    bool HasFiles() const;  std::vector<std::wstring> GetFiles() const;
    bool HasFormat(const std::wstring& fmt) const;
    bool GetFormatData(const std::wstring& fmt, std::vector<BYTE>& out) const;
};
struct DragEventArgs { const DragData& data; float x, y; DWORD allowed, effect, keyState; };   // x/y 为元素相对 DIP

// ---- 拖放：源端发起 ----
class DragDataBuilder {
    DragDataBuilder& AddText(const std::wstring&);
    DragDataBuilder& AddFile(const std::wstring& path);
    DragDataBuilder& AddFiles(const std::vector<std::wstring>& paths);
    DragDataBuilder& AddCustom(const std::wstring& fmt, const void* bytes, size_t n);
    DragDataBuilder& SetPreferredEffect(DWORD);
};
DWORD Window::BeginDrag(DragDataBuilder&, DWORD allowed = COPY|MOVE|LINK, UIElement* source = nullptr);

// ---- 拖放：目标端（元素）----
void UIElement::SetDropTargetEnabled(bool);   bool IsDropTargetEnabled() const;
DWORD UIElement::GetAllowedDropEffects() const;                   // 默认 COPY|MOVE|LINK
DWORD UIElement::OnDragEnter/OnDragOver(DragEventArgs&);         // 返回 effect
void  UIElement::OnDragLeave(DragEventArgs&);   DWORD UIElement::OnDrop(DragEventArgs&);
void UIElement::SetDragSource(std::function<void(DragDataBuilder&)>, DWORD allowed = COPY|MOVE|LINK);
ZSignal<DragEventArgs&> DragEnter, DragOver, DragLeave, Drop;

// ---- 拖放：整窗接收（光标未命中任何“启用落点”的元素时由窗口接收）----
void Window::SetDropTargetEnabled(bool);   bool Window::IsDropTargetEnabled() const;
ZSignal<DragEventArgs&> Window::DragEnter, DragOver, DragLeave, Drop;   // x/y = 客户区 DIP

// ---- Expander：可折叠卡片 ----
class Expander : public UIElement {
    Expander(const std::wstring& title = L"", const std::wstring& subtitle = L"");
    void SetTitle/SetSubtitle(const std::wstring&);   void SetContent(std::shared_ptr<UIElement>);
    std::shared_ptr<UIElement> GetContent() const;
    void SetExpanded(bool);   bool IsExpanded() const;   void Toggle();
    void SetIcon(Icon, float size = 22.0f);   // 标题前图标（None = 无）
    ZSignal<bool> ExpandedChanged;
};

// ---- SettingsList：设置行列表 ----
class SettingsList : public UIElement {
    void AddRow(Icon, const std::wstring& text, const std::wstring& subtitle, std::function<void()> onClick, const std::wstring& id = {});
    ZSignal<int> RowClicked;
};
```
- **`易错`**：`DragEventArgs.x/y` 是**元素相对 DIP**（不是屏幕像素）。
- 同控件内文本拖动：源端先删原选区、再按落点插入并左移修正；目标端检测私有格式 `ZufyUI.TextSource` 跳过重复插入。
- **`版本`**：折叠/展开用与页面切换相同的 **smoothstep 缓动**；箭头随同一缓动**旋转**（收起向下 → 展开向上）。
- **`版本`**：标题条悬停**高亮**（圆角）；`ComboBox` 下拉箭头同样改为随展开**旋转**的 chevron。
- **`易错`**：`Expander` 折叠时用显式 `SetHeight` 收拢，父布局才会跟随收缩。
- 图片展示用 `Label`：`SetImage` + `SetImageFit(true)`（等比缩放至完整可见）+ `SetDragSource`（拖出）。
- **`版本`**：整窗接收——`Window::SetDropTargetEnabled(true)` + 窗口级 `DragEnter/DragOver/DragLeave/Drop`（`x/y` 为客户区 DIP）。
- **`版本`**：`Expander::SetIcon(Icon, size)` 在标题前加图标；`TextEdit` 撤销/重做改为标准方案（撤销/重做都还原当时的选区与光标）。

###chapter: 无障碍与调试通道 | UIA + SetDebugEnabled（v1.17.0）

## 无障碍（UIA）

- 由 `ZufyUIWindowTool.h` 提供 UI Automation 提供程序；元素侧 API 见「元素基类」。
- 窗口侧：`SetAccessibilityEnabled(bool)`（默认开）、`FocusElement`、`GetFocused/Hovered/PressedElement`。
- **`易错`**：库**不存历史、不读写文件**，提供程序只反映当前帧。`WM_GETOBJECT` 仅在开启时处理。

## 调试 / 自动化通道

```cpp
inline void SetDebugEnabled(bool);   inline bool IsDebugEnabled();
```
- 默认**关闭**；开启后目标窗口处理 `WM_COPYDATA`（`dwData` = 命令号，回包 `0x5A554631`）。
- 命令集（1..22）：

| 命令 | 作用 | 命令 | 作用 |
|---|---|---|---|
| 1 | Ping | 12 | ClearHighlight |
| 2 | ListWindows | 13 | GetErrors（统一日志 E/W/I/D，≤500） |
| 3 | GetFrameStats | 14 | ClearErrors |
| 4 | GetElementTree | 15/16/17 | TopN 重绘/布局/缓存（`all`=全部） |
| 5 | ForceRepaint | 18 | TopN 绘制 |
| 6 | SetElementText("id\ttext") | 19 | ResetCounters |
| 7 | InvokeElement | 20 | SetVisible("id\t0/1") |
| 8 | FocusElement | 21 | SetMargin("id\tx\ty") |
| 9 | GetElementInfo | 22 | SetHighlightColor("RRGGBB") |
| 10 | GetElementAt("x,y" 屏幕→AutomationId) | — | — |
| 11 | Highlight(id) | — | — |

- 统一日志：`detail::Log(level, tag, msg)`、`LogWarning/LogInfo/DebugLog`、`RecordError`（→ E 级并触发 `UIZSignals::Error`）。
- **`易错`**：调试器侧发命令要带超时（`SendMessageTimeout`，不加 `SMTO_BLOCK`），否则目标忙时会卡死调试器。`ZUFYUI_DEBUG` 是**编译期**开关（日志），`SetDebugEnabled` 是**运行期**开关（通道/统计）——两套语义别混。

###chapter: 版本变化摘录 | v1.15.0 → v1.19.1

> 只列**对你写代码有影响**的显著变化。

- **v1.15.0**：数据视图虚拟化（`SetItemCount` 等）+ 隐藏/筛选/可见序排序（源行号 vs 可见序两套坐标）；跨线程/锁语义收口；`ComboBox` 源索引 API；`TreeView::BeginUpdate/SetNodeText`。
- **v1.16.0**：系统对话框 `FileDialog`/`ColorDialog`；`UIZSignals::Error`；渲染线程默认关并标注不稳定；托盘回调消息撞号修复；`ZUFYUI_ENABLE_COMCTL_V6` 可选宏。
- **v1.17.0**：**UIA 无障碍**（默认开）；**外部调试/自动化通道**（`SetDebugEnabled`，默认关，命令 1~22）；**布局拉伸权值**（`ColumnBox`/`RowBox`/`GridLayout` 按权值分配剩余空间）；**省略文本自动 ToolTip**（Label/Button）；`CaptionButton::Kind::Pin`；`TreeView` 节点 `bgColor`/`Get/SetScrollOffsetY`/`ScrollToNode`；`Window::DefaultBackdropColor` 默认纯白。
- **v1.18.0**：
  - 新增 `ZufyUICharts.h`：`ChartBase` / `BarChart` / `LineChart` / `PieChart`；`BarChart` 补全横向方向；`PieChart` 滚轮缩放。
  - 新增 `TextEdit` 多行编辑器 / 只读查看器；`TextRun`；基类新增 `AcceptsTab()`。
  - `UIElement::ReleaseDeviceResourcesRecursive()`；`PageHost` 隐藏页缓存**延迟释放**（约 1.2s）；`ScrollViewer::UseCache()=false`；`SetUseCache(false)` 立即释放位图。
  - 库层修复：`Snap` 的 DPI 上下文在 UI 线程也设置；`ZSignal::Fire` RAII 异常安全；`TextBox` shift+点击语义；`Slider` 0 宽 NaN 守卫；`ListView` 池宽度变化重测（`poolW_`）。
- **v1.19.0**：
  - 新增**拖放**（`ZufyUIDragDrop.h`）：`Window::BeginDrag` + `DragDataBuilder`；元素 `SetDropTargetEnabled`/`OnDragEnter/Over/Leave/Drop` + 信号；`Label`/`Button`/`TextBox`/`TextEdit`/`ComboBox` 支持拖入/拖出；跨应用文件拖出。
  - 新增 `Expander`（可折叠卡片：smoothstep 缓动 + 箭头旋转 + 标题悬停高亮）、`SettingsList`（设置行列表）；图片展示用 `Label::SetImage` + `SetImageFit`（等比缩放至完整可见）。
  - `ComboBox` 下拉箭头改为随展开旋转的 chevron。
  - 文本拖动为**移动**语义：源端先删原选区再按落点插入（同控件用私有格式去重）。
- **v1.19.1**：
  - `TextEdit` 撤销/重做改为**标准方案**（栈顶=当前态）：撤销/重做都**还原当时的选区与光标**；Backspace/Delete/输入/粘贴各一步；拖放插入/移动均可撤销、可重做。
  - `ScrollViewer`：内容改按**视口宽度**测量（修复 FillWidth/换行内容期望宽度无穷大导致的“卡片被裁剪”与滚动范围错乱）；内容**两遍测量**（出现竖滚动条后按变窄宽度重量）；**尊重内容元素自身的 `margin`**；裁剪四周留 **1px 出血**，避免内容边缘边框被切掉一半。
  - 拖放落点光标在目标**未聚焦**时也显示；拖动期间**保留源选区**并在落点显示预览光标；落点落在原选区内 → 不变更。
  - `Expander`：标题条悬停高亮（不再遮挡卡片边框）、箭头随折叠缓动旋转、`SetIcon` 标题图标。
  - 整窗拖放：`Window::SetDropTargetEnabled` + 窗口级 `DragEnter/DragOver/DragLeave/Drop`。
  - 图片展示并入 `Label`（新增 `SetImageFit`），移除独立的 `ImageView`。

###chapter: 易错点总表 | 按主题速查

- **颜色**：`Color` 分量是 [0,1]；整数用 `Color::FromArgb`。
- **尺寸/布局**：`SetWidth/Height` 是固定尺寸；`SetFill*` 是填满父轴；拉伸权值分配**剩余**空间（v1.17.0）；图表尺寸来自自身 `width_/height_`，且默认权值 0。
- **窗口最小尺寸**：含长文本/图表的窗口建议显式 `SetMinSize`，否则会被内容测量结果撑大。
- **重绘**：`RequestRepaint()` 只标脏、**不排帧**；不要试图在标脏处强制出帧（会持续重渲染卡顿）。`InvalidateLayout()` 冒泡，`InvalidateLayoutSelf()` 不冒泡（池化/虚拟 rebind 用）。
- **缓存**：默认开；每帧内容变化/实例极多的控件 `SetUseCache(false)`；释放整棵子树用 `ReleaseDeviceResourcesRecursive()`。
- **文本**：`SetTextFast` 不重绘（池化专用）；`FontManager` 返回的 layout/format 共享只读，勿改。
- **信号**：`Fire` 快照遍历、可安全 disconnect；`Connection` 析构不断开；`DrawOverlay` 必须按窗口过滤；别用捕获自身 `shared_ptr` 的 lambda 连自己。
- **数据视图**：源行号 vs 可见序两套坐标；`Sort` 重排并清多选，`SortView` 只改可见序；`parent` 是 `weak_ptr`；行背景用 `node->bgColor`。
- **多行编辑**：`AcceptsTab` 为真时 Tab 是缩进；富文本只读；换行宽度变化重建全部行 layout。
- **滚动条**：宿主用 `ShrunkWidthRatio` 预留槽位；空闲缩小是**惰性**的。
- **无障碍/调试**：角色决定暴露的 UIA Pattern；调试命令发送要带超时。

###chapter: 附录 | 默认值与信号索引

## 常见默认值速查

| 项 | 值 |
|---|---|
| 根布局 | `ColumnBox`，margin 20，spacing 10 |
| `Color` 默认 | `(0,0,0,1)` |
| `Button` 默认尺寸 | 120×36 |
| `ToggleSwitch` | 50×24 |
| `ScrollBar` 默认宽 / `ShrunkWidthRatio` | 8 / 0.30 |
| `TabView` 页签高 / 指示条高 | 34 / 2.5 |
| `Card` padding / 圆角 | 12 / 8 |
| `Page` padding | 10 |
| `ChartBase` 默认尺寸 / 调色板 | 420×260 / 8 色 |
| `PieChart` 默认尺寸 | 320×260 |
| `TextEdit` 默认字体 / 尺寸 / tab | Consolas 14 / 320×160 / 4 |
| `Expander` 标题高 / 圆角 | 66 / 8 |
| `ImageView` 默认尺寸 | 200×150 |
| `Window::DefaultBackdropColor` | `0xFFFFFFFF`（纯白） |

## 内置信号索引

- 元素：`MouseEnter/MouseLeave/MouseMove/MouseDown/MouseUp/Focused/Blurred/KeyDown/KeyUp/Char`
- Button：`Clicked/Toggled`；TextBox：`TextChanged/ReturnPressed`；TextEdit：`TextChanged/CursorPositionChanged/SelectionChanged`
- ComboBox：`SelectionChanged/DropDownOpened/DropDownClosed`；CheckBox：`Toggled/StateChanged`
- RadioButton：`CheckedChanged/Clicked`；RadioGroup：`SelectionChanged`；ToggleSwitch：`Toggled`
- Slider：`ValueChanged/SliderReleased`；NumberBox：`ValueChanged/TextChanged`；ProgressBar：`ValueChanged`
- ScrollViewer：`ScrollChanged`；SplitView：`SplitChanged`；TabView：`SelectionChanged/TabCloseRequested`
- ListView/TableView/TreeView：`SelectionChanged/ItemClicked(CellClicked)/ItemDoubleClicked/ItemRightClicked/…CheckStateChanged`
- Chart：`PointClicked/CategoryClicked`；PieChart：`SliceClicked`
- 拖放：元素 `DragEnter/DragOver/DragLeave/Drop`；窗口级 `Window::DragEnter/DragOver/DragLeave/Drop`；Expander：`ExpandedChanged`；SettingsList：`RowClicked`
- Window：`Activated/Deactivated/Closing/Closed/DeviceLost/RenderingError`
- 全局：`UIZSignals::DrawOverlay/GlobalMouseDown/WindowActivated/WindowDeactivated/DeviceReset/ReloadAcrylic/Error`
