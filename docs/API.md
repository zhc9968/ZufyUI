# ZufyUI API 参考

> 本文档覆盖 ZufyUI 框架的全部公开 API：核心类型、信号槽、字体、元素、布局、窗口、基础控件与数据视图。
> 配套阅读：[项目说明与构建](../README.md)。

> **阅读方式**：本文不是“签名清单”，而是按“它是什么 → 框架内部怎么处理 → 你需要注意什么”来写。每个类都会先讲整体行为，再逐函数讲清楚副作用、默认值和易混点。**没有读过源码也能据此正确使用。**

###chapter: 约定 | 命名空间、单位、生命周期与默认值约定

## 命名空间与头文件

- 所有内容位于 `namespace ZufyUI`。
- 头文件分工：`ZufyUI.h`（核心：类型 / 信号槽 / 字体 / 元素 / 布局 / 菜单 / 窗口）、`ZufyUIWidgets.h`（基础控件）、`ZDataViewer.h`（数据视图）。`ZufyUIWidgets.h` 会包含 `ZufyUI.h`，`ZDataViewer.h` 依赖两者。
- 三者都是纯头文件；`#pragma comment(lib, ...)` 会自动链接 `d2d1 / dwrite / dwmapi / imm32 / winmm`。

## 单位

- 所有坐标与尺寸都是 **DIP**（设备无关像素），不是物理像素。
- 框架在绘制前用内部 `Snap()` 把坐标吸附到物理像素网格，避免半像素模糊；你写代码时**不需要**自己做 DPI 换算。
- 鼠标事件的坐标也是 DIP；如需屏幕/物理像素，用 `Window` 内部已处理的转换或自行 `MulDiv`。

## 对象模型与生命周期

- 控件一律用 `std::shared_ptr<T>` 拥有，用 `std::make_shared<T>()` 创建。
- 通过布局的 `AddChild()` 挂到父容器；父容器保存子元素的 `shared_ptr`，**父容器存活期间子元素不会被释放**。
- 脱离布局（如 `RemoveChild` / `Clear`）后若没有其它 `shared_ptr`，元素即被回收。
- 元素析构时会自动释放它自己的离屏缓存与已注册的连接（内部 `ConnectionGroup`）。

## 信号与事件

- 事件用 `Connect(signal, slot)` 绑定；返回 `Connection`，默认随宿主元素析构自动断开。
- 槽可以是任意可调用对象（lambda / 函数指针 / `std::function`）。
- **重要**：不要用“按值捕获宿主自身 `shared_ptr`”的 lambda 连接宿主自己的信号，会形成引用环导致整棵子树泄漏。详见第 4 章。

## 默认值体系

- 每个控件都有 `inline static` 的 `Default*` 静态字段（如 `Button::DefaultSize`）。
- 通过对应的 `static SetDefault*()` 修改后，**只影响之后新建的实例**，已存在的实例不变。
- 实例级 setter（如 `SetColors`）只影响该实例。

## 禁用、焦点与缓存三个通用概念（贯穿所有控件）

- **启用/禁用**：`UIElement::SetEnabled(false)` 会让该元素及子树“不可交互”（鼠标/键盘事件被拦截），控件需自己根据 `IsEffectivelyEnabled()` 决定是否绘制为灰色。禁用不会改变布局（仍占位）。
- **焦点**：只有 `IsFocusable()` 返回 true 的元素能获得键盘焦点。窗口按 Tab 遍历可聚焦元素；焦点环只在该元素通过 Tab 获得焦点时显示，鼠标点击获得焦点不显示。
- **离屏缓存**：见第 6 章 `UIElement` 的“缓存机制”小节。默认开启，可用 `SetUseCache(false)` 关闭。

###chapter: 基础类型 | Color、Rect、Thickness、Size

## Color

```cpp
struct Color {
    float r, g, b, a;
    Color(float r = 0, float g = 0, float b = 0, float a = 1.0f);
    static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    D2D1_COLOR_F ToD2D() const;
    static Color Lerp(const Color& c1, const Color& c2, float t);
};
```

- **分量范围 `[0,1]`（float）**，不是 0–255。混用是最常见的错误来源：`Color(255,0,0)` 不会得到红色，而是“极大的越界值”，最终颜色不可预期。要么用 `Color(1,0,0)`，要么用 `FromArgb(255, 255, 0, 0)`。
- `FromArgb` 的参数顺序是 **`(a, r, g, b)`**：alpha 在最前，和 `Color(r,g,b,a)` 的顺序不同，注意别混。
- `ToD2D()` 返回 Direct2D 的 `D2D1_COLOR_F`。内部会缓存画刷，颜色改变后需要 `RequestRepaint()`（控件 setter 已自动调用）。
- `Lerp` 对四个分量分别线性插值，`t` 未做 clamp；动画里通常先自行 clamp。

## Rect

```cpp
struct Rect {
    float x, y, width, height;
    Rect(float x = 0, float y = 0, float w = 0, float h = 0);
    bool Contains(float px, float py) const;
    D2D1_RECT_F ToD2D() const;
};
```

- 表示**位置 + 尺寸**（不是 left/top/right/bottom）。
- `Contains` 判定是否在矩形内（含边界）。
- `ToD2D()` 转成 Direct2D 的 `D2D1_RECT_F`（left/top/right/bottom）。

## Thickness

```cpp
struct Thickness {
    float left, top, right, bottom;
    Thickness(float l = 0, float t = 0, float r = 0, float b = 0);
};
```

- 用于 `SetMargin()`、`ScrollViewer::SetContentMargin()` 的四周间距。
- 没有 `Thickness(all)` 单参构造的“四边相同”语义：写 `Thickness(8,8,8,8)`。

## Size

```cpp
struct Size {
    float width, height;
    Size(float w = 0, float h = 0);
};
```

- 用于 `Measure()` 的可用空间与返回值。

###chapter: 全局函数与 DPI | clamp、DPI 缩放与像素吸附

```cpp
template<typename T> T clamp(T value, T low, T high);
inline float& GlobalDpiScaleRef();
inline void SetGlobalDpiScale(float scale);
inline float GetGlobalDpiScale();
inline float Snap(float dip);
```

| 函数 | 实现要点与注意 |
| --- | --- |
| `clamp(v, low, high)` | 参数顺序是 **`(值, 下界, 上界)`**，和 `std::clamp` 一致，但**不是** `(值, 上界, 下界)`。越界时返回边界值。 |
| `SetGlobalDpiScale(s)` | 渲染前由 `Window` 自动设置为 `dpi/96`。传 `s<=0` 会回退到 `1.0f`。一般不需要手动调用。 |
| `GetGlobalDpiScale()` | 读取当前缩放，默认 `1.0f`。 |
| `Snap(dip)` | 把 DIP 坐标吸附到当前物理像素网格（`round(dip*scale)/scale`）。框架在绘制前自动用，用户通常无需调用。 |

> **为什么需要 `Snap`**：Direct2D 在非整数像素上绘制会做抗锯齿，文本和 1px 线条会发虚。把坐标吸附到物理像素即可保持锐利。

###chapter: 信号与连接 | ZSignal、Connection、ConnectionGroup 与全局信号

## 这一章要解决什么

ZufyUI 的信号槽不是“连上就完事”，它还要处理**对象销毁时自动断开**这件事。理解下面三者的分工，才能避免悬垂回调与内存泄漏：

- `ZSignal`：信号的持有者，内部保存若干个槽（`std::function`）。
- `Connection`：一次连接的句柄，析构时断开。
- `ConnectionGroup`：一组连接的集合，析构时把组内所有连接一次性断开。每个 `UIElement` 自带一个。

## ConnectionThread

```cpp
enum class ConnectionThread {
    CurrentThread,   // 在触发线程直接执行（默认）
    NewThread,       // 每次触发新建分离线程执行
    UIThread         // 投递到 UI 线程消息循环执行
};
```

- `CurrentThread`：同步执行，槽里改控件状态是安全的，也是绝大多数场景应使用的模式。
- `NewThread`：每次触发 `detach` 一个线程；**槽里不要直接操作 UI**，否则有线程安全问题。
- `UIThread`：通过内部消息投递到 UI 线程，适合“后台线程产生数据、回主线程更新界面”。

## Connection

```cpp
class Connection {                  // Qt QMetaObject::Connection 风格的“被动句柄”
    Connection();
    ~Connection();                  // 注意：析构【不】自动断开
    Connection(const Connection&) = default;   // 可拷贝
    void disconnect();              // 显式断开这一条
    bool isConnected() const;
    explicit operator bool() const;
};
```

- **被动句柄**：析构**不会**自动断开，所以忽略返回值（`elem->Connect(sig, slot);`）是安全的；要单独断开就 `conn.disconnect()`。
- 自动断连由元素的 `ConnectionGroup` 负责（元素析构时统一断开）。
- `isConnected()` / `operator bool` 查询是否仍有效。

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    ConnectionGroup();
    ~ConnectionGroup();                    // 析构时断开全部
    void disconnectAll();
    size_t size() const;
    void addState(const std::shared_ptr<detail::ConnectionState>& state);
};
```

- 每个 `UIElement` 构造时自带一个。控件析构 → 组析构 → `disconnectAll()`，因此挂在控件上的连接会随控件一起消失。
- 你也可以自己 `std::make_shared<ConnectionGroup>()`，用 `signal.connect(slot, ConnectionThread::CurrentThread, group)` 把连接归组，方便批量管理。

## ZSignal

```cpp
template <typename... TArgs>
class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    void Fire(TArgs... targs) const;
    void operator()(TArgs... targs) const;   // Fire 的语法糖
};
```

**实现要点**（影响你的使用方式）：

- 触发时对当前连接做一次快照再回调，因此**在槽里连接/断开别的槽是安全的**（不会导致迭代器失效）。
- 快照使用线程本地缓冲复用，非重入时不产生堆分配；重入（槽内再次触发同一信号）会退回局部拷贝。
- `Fire` 是 `const` 的，可在 const 成员函数里触发。
- 线程安全：内部用互斥锁保护连接列表。

###block_green_start
**推荐用法**：控件内部已提供 `UIElement::Connect(signal, slot)`，会把连接挂到该控件的 `ConnectionGroup`，控件销毁时自动断开。手动用 `signal.connect(...)` 时要注意连接的生存期。
###block_green_end

###block_orange_start
**注意（信号与对象生命周期）**：`Connect` 的槽（lambda）若**按值捕获了拥有该信号的对象的 `shared_ptr`**（例如 `obj->Connect(obj->SomeSignal, [obj](){...})`），会形成引用环，导致该对象及其整棵子树永远不会被释放（`ConnectionGroup` 也无法解开，因为它本身归该对象所有）。建议改捕获**原始指针**或 `weak_ptr`：

```cpp
auto b = btn.get();   // 原始指针；信号是 btn 的成员，btn 析构时连接自动失效
btn->Connect(btn->Clicked, [b]() { b->SetText(L"..."); });
```
###block_orange_end

## 全局信号 `UIZSignals`

| 信号 | 参数 | 触发时机 | 备注 |
| --- | --- | --- | --- |
| `DrawOverlay` | `ID2D1RenderTarget*` | 所有 UI 绘制完成后叠加绘制 | 下拉框展开列表、ToolTip 等就挂在这里 |
| `GlobalMouseDown` | `float, float` | 全局鼠标按下（DIP） | 控件用它实现“点击外部收起” |
| `WindowDeactivated` | — | 窗口失活/最小化 | 用于收起展开态 |
| `ElementCaptureRequest` | `UIElement*` | 控件请求鼠标捕获 | 框架内部使用 |
| `ElementCaptureRelease` | `UIElement*` | 控件释放鼠标捕获 | 框架内部使用 |
| `RepaintRequest` | `UIElement*` | 控件请求重绘 | `RequestRepaint()` 的底层 |
| `LayoutInvalidated` | — | 全局布局失效 | `InvalidateLayout()` 的底层 |
| `DeviceReset` | — | 渲染设备资源被丢弃/重建（设备丢失、DPI 变化、窗口销毁等） | 订阅者应清理按渲染目标/设备缓存的东西（如 `ImageManager` 的图像位图缓存） |

> 这些是“全局单例信号”，不是某个控件的成员。连接它们时同样建议登记到一个明确的 `ConnectionGroup`，否则需要自行在合适时机断开。

###chapter: 字体 | FontSpec 与 FontManager

## FontSpec

```cpp
struct FontSpec {
    std::wstring familyName = L"Segoe UI";
    float size = 14.0f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    std::wstring locale = L"en-us";
    bool operator==(const FontSpec& other) const;
    bool operator!=(const FontSpec& other) const;
};
```

- 是“字体规格值对象”，可比较、可作哈希键（`FontSpecHash`），因此可放入 `unordered_map`。
- `size` 单位是 DIP 下的点数。
- `familyName` 用系统字体名（如 `Microsoft YaHei`、`Segoe UI`）。

## FontManager（单例）

| 方法 | 实现要点与注意 |
| --- | --- |
| `static FontManager& Instance()` | Meyers 单例，首次访问才构造。 |
| `IDWriteFactory* GetFactory()` | 懒加载共享 `IDWriteFactory`；首次调用创建。 |
| `IDWriteTextFormat* GetFormat(const FontSpec& spec)` | 按 spec 缓存共享 `IDWriteTextFormat`。相同的 spec 只创建一次；返回的是**共享对象**，不要 `Release`、也不要修改其属性。 |
| `void SetGlobalFont(const FontSpec& spec)` | 设置全局字体并触发 `GlobalFontChanged`。 |
| `const FontSpec& GetGlobalFont() const` | 读取全局字体。 |
| `ZSignal<> GlobalFontChanged` | 每个未做实例字体覆盖的元素都订阅了它；触发时它们会重建文本布局。 |

## 字体解析链（很重要）

元素的最终字体的解析顺序是：

1. 实例级覆盖 `SetFont(...)` / `SetFontFamily(...)` / `SetFontSize(...)` / `SetFontWeight(...)`；
2. 类型默认（控件重写 `GetTypeDefaultFont()`，例如 `Label` 默认 16 号、`TextBox` 默认 14 号）；
3. 全局默认（`FontManager::GetGlobalFont()`）。

`GetEffectiveFontSpec()` 返回上述解析结果；`GetFontFormat()` 再把它换成共享的 `IDWriteTextFormat`（带每个实例的快速缓存）。

- 实例调用了 `SetFont*` 后 `HasFontOverride()` 为 true，此时**不再**响应全局字体变更（符合“我显式指定了”的直觉）。
- `ClearFont()` 清除实例覆盖，回到解析链的后两级。

###chapter: 核心元素 UIElement | 尺寸、布局、绘制、事件与字体接口

所有可视元素的基类。自定义控件需继承它并实现纯虚的 `Measure` 与 `Draw`。

## 布局属性

| 方法 | 说明 |
| --- | --- |
| `void SetMinWidth(float)` / `SetMinHeight(float)` | 最小尺寸；测量时不会被压到更小 |
| `void SetMaxWidth(float)` / `SetMaxHeight(float)` | 最大尺寸；默认 `FLT_MAX` |
| `void SetMinSize(float w, float h)` / `SetMaxSize(float w, float h)` | 同时设置 |
| `float GetMinWidth()/GetMinHeight()/GetMaxWidth()/GetMaxHeight() const` | 读取 |
| `void SetFillWidth(bool)` / `SetFillHeight(bool)` | 是否在父容器方向被拉伸填满 |
| `bool GetFillWidth()/GetFillHeight() const` | 读取 |
| `void SetWidth(float)` / `SetHeight(float)` | 固定尺寸（`0` 表示按测量结果） |
| `float GetWidth()/GetHeight() const` | 读取 |
| `void SetMargin(const Thickness&)` / `Thickness GetMargin() const` | 外边距 |
| `void SetStretchWeights(float h, float v)` | 同时设置横/纵拉伸权重 |
| `void SetHorizontalStretchWeight(float)` / `SetVerticalStretchWeight(float)` | 单独设置 |
| `float GetHorizontalStretchWeight()/GetVerticalStretchWeight() const` | 读取 |
| `void InvalidateLayout()` / `bool IsLayoutDirty() const` / `ClearLayoutDirty()` | 标脏；下次绘制前的布局阶段会重新测量排布 |

**易混点**：

- `SetWidth/SetHeight` 是“固定尺寸”，`SetFillWidth/SetFillHeight` 是“跟随父容器拉伸”。两者都可能与父布局的拉伸权重共同作用。
- 改任何布局属性都要 `InvalidateLayout()`；上面这些 setter 都已自动调用。

## 测量、排布与绘制（自定义控件必读）

| 方法 | 说明 |
| --- | --- |
| `virtual Size Measure(const Size& availableSize) = 0` | 测量（**必须实现**）。返回你期望的尺寸；`availableSize` 可能是 `FLT_MAX`，表示“不约束”。 |
| `virtual void Arrange(const Rect& finalRect)` | 排布。默认实现记录 `arrangedRect_`；有子元素的容器要在这里给子元素 `Arrange`。 |
| `Rect GetArrangedRect() const` | 上一轮排布结果，绘制与命中测试都用它。 |
| `virtual void Draw(ID2D1RenderTarget* rt) = 0` | 绘制（**必须实现**）。用 `GetArrangedRect()` 里的绝对坐标绘制。 |
| `virtual const std::vector<UIElement*>& GetChildren() const` | 子元素列表（返回引用，容器需维护自己的视图缓冲）。**返回的是元素内部缓冲的引用**：有效期到“该元素的子列表/可见性变化或再次调用其 `GetChildren()`”；遍历期间**不要对同一个元素再次调用 `GetChildren()`**（递归子元素用的是各自的缓冲，安全）。 |
| `virtual bool UseCache() const` / `SetUseCache(bool)` | 是否使用离屏缓存（默认 true）。 |
| `virtual std::optional<D2D1_RECT_F> GetClipRect() const` | 返回子元素裁剪区（绝对坐标）；返回 `nullopt` 表示不裁剪。默认返回 `SetClipRect(...)` 设置的值。 |
| `void SetClipRect(const std::optional<Rect>&)` | 设置该元素子树的裁剪矩形（如 `ScrollViewer` 用它把内容裁到视口）。 |
| `void RequestRepaint()` | 请求重绘；底层触发全局 `RepaintRequest`。 |

> **Measure/Arrange 的契约**：`Measure(可用空间)` 只描述“我想要多大”，不要在里面改父布局状态；`Arrange(finalRect)` 才是“我被放到了哪里”，绘制永远基于 `arrangedRect_`。

## 缓存机制（影响性能与显示，务必理解）

- 默认 `UseCache()` 为 true：框架把元素绘制到一张尺寸为 `(宽+2·bleed) × (高+2·bleed)` 的离屏位图，之后每帧只贴图。`bleed` 默认 `4.0f`，用于给抗锯齿/阴影留边。
- 对大多数控件，缓存能显著减少重复绘制；但**大量小元素**每个都建一张位图会占用可观显存，必要时 `SetUseCache(false)`。
- 缓存只在“标记为待重绘”或“尺寸变化”时重建。改颜色/文本后需要 `RequestRepaint()`。
- 阴影会被画进缓存（带阴影的元素缓存会额外扩大 `GetShadowExtent()`）。
- 窗口合成时会把**完全落在裁剪区外**的子树整棵跳过（滚动出视区的内容不会建缓存）。
- `SetVisible(false)` 会立刻释放该元素的缓存。

## 启用 / 禁用

| 方法 | 说明 |
| --- | --- |
| `void SetEnabled(bool)` | 设置自身启用状态（默认为启用） |
| `bool IsEnabled() const` | 自身启用状态 |
| `bool IsEffectivelyEnabled() const` | 综合自身与所有祖先：任一祖先被禁用则返回 false |

- 禁用后：窗口不会把鼠标/键盘事件派发给它；控件应依据 `IsEffectivelyEnabled()` 把自身绘制成灰色。禁用**不改变布局**。
- 典型用法：`btn->SetEnabled(false);`。绝大多数基础控件的 `Draw` 已经处理了灰色显示。

## ToolTip

| 方法 | 说明 |
| --- | --- |
| `void SetToolTip(const std::wstring&)` / `const std::wstring& GetToolTip() const` | 设置/读取悬停提示文本 |

- ToolTip 是**框架级统一浮层**，由 `Window` 绘制，不需要你写任何绘制代码。
- 行为：鼠标在某元素上**静止悬停 0.5 秒**后，在“显示时的鼠标位置”上方固定偏移处渐显一个白底黑字、带柔和阴影的提示；**鼠标一移动就关闭**，并重新计时。
- 禁用元素不显示提示。
- 提示位置基于“触发那一刻的鼠标坐标”，显示后不会跟随鼠标移动。
- 数据视图（列表/表格/树）的每项/单元格/节点提示也走这套机制（详见第 11 章）。

## 阴影

| 方法 | 说明 |
| --- | --- |
| `void SetShadow(bool)` / `bool HasShadow() const` | 开关阴影（默认关） |
| `void SetShadowColor(Color)` | 阴影颜色；**`a` 表示“边缘可见透明度”**，内部约为其 2 倍 |
| `void SetShadowBlur(float)` | 模糊半径；约等于高斯 `2σ`，扩散范围 ≈ `1.5×blur`（默认 10） |
| `void SetShadowOffset(float x, float y)` | 偏移（默认 `0,3`） |
| `void SetShadowCornerRadius(float)` | 圆角；小于 0 时按 8 处理 |
| `float GetShadowExtent() const` | 阴影需要向外扩张的最大像素，用于扩大缓存出血 |

- 阴影只对**使用缓存的元素**生效（基础 `UIElement` 默认缓存，所以大多数元素设置后可见；`Card` 会自动在开启阴影时启用缓存）。
- 实现是“高斯 CDF 分层”：按高斯分布分配多层 alpha，叠加后逼近真实高斯模糊，观感接近 DWM 阴影，但不使用 D2D Effect（因此兼容逐像素 Alpha 的窗口）。

## 焦点与键盘

| 方法 | 说明 |
| --- | --- |
| `virtual bool IsFocusable() const` | 是否可获取键盘焦点（默认 false） |
| `bool IsFocused() const` | 是否当前持有焦点 |
| `void Focus()` / `void Blur()` | 请求/取消焦点 |

- 窗口按 Tab 在可聚焦元素间遍历。焦点环**仅在通过 Tab 获得焦点时**显示；鼠标点击获得焦点不显示环。
- 只有持有焦点的元素才接收 `OnKeyDown/OnKeyUp/OnChar`。

## 事件虚函数（可重写）

| 虚函数 | 何时被调用 / 默认行为 |
| --- | --- |
| `HitTest(x,y)` | 命中测试；返回命中的最内层元素或 `nullptr` |
| `OnMouseEnter/Leave/Move/Down/Up` | 鼠标事件；需要自行 `RequestRepaint()` |
| `OnMouseWheel(dx,dy)` | 返回 true 表示已消费，事件不再冒泡给父级 |
| `OnKeyDown/Up(key,lParam)` | 仅在持有焦点时 |
| `OnChar(ch)` | 字符输入（含 IME 后） |
| `OnFocus` / `OnBlur` | 获得/失去焦点 |
| `UpdateAnimation(dt)` | 每帧动画更新；`dt` 单位秒，已被钳制在 `0.033s` 内 |
| `HasActiveAnimation()` | 返回 true 时窗口保持 16ms 定时器持续重绘 |
| `IsTextInput()` | 返回 true 时鼠标悬停显示文本光标 |
| `ReleaseDeviceResources()` | 设备丢失/重建时释放画刷等 |
| `GetImeCandidateRect()` | 返回 IME 候选框定位矩形 |
| `SetCompositionText(...)` | IME 组合输入回调 |
| `GetChildRenderTransform(child)` | 子元素的渲染变换（用于页面切换等） |
| `OnFontChanged()` | 字体变化；默认重建布局 |

## 事件信号（`ZSignal`）

`MouseEnter`、`MouseLeave`、`MouseMove(float,float)`、`MouseDown(float,float)`、`MouseUp(float,float)`、`KeyDown(WPARAM,LPARAM)`、`KeyUp(WPARAM,LPARAM)`、`Char(wchar_t)`、`Focused`、`Blurred`。

> 1.8.0 起这些事件由裸 `std::function` 回调改为 `ZSignal`，用 `Connect(elem->MouseDown, ...)` 连接（连接随元素析构自动断开）。它们与 `OnXxx` 虚函数都会被内部触发，互不冲突。

## 父子、可见性、右键菜单与连接

| 方法 | 说明 |
| --- | --- |
| `void SetParent(UIElement*)` / `UIElement* GetParent() const` | 父元素（通常由容器自动设置） |
| `Window* GetWindow() const` | 所属窗口；未挂载或窗口已销毁时返回 `nullptr`（内部按窗口 id 查找，不持有裸指针） |
| `void SetVisible(bool)` / `bool IsVisible() const` | 可见性；设为 false 会释放缓存，并触发 `OnVisibilityChanged(false)` |
| `virtual void OnVisibilityChanged(bool visible)` | 可见性变化钩子；例如 `ComboBox` 在隐藏时会自动收起下拉弹层 |
| `void SetContextMenu(std::shared_ptr<Menu>)` / `GetContextMenu()` | 固定右键菜单 |
| `void SetContextMenuFactory(std::function<std::shared_ptr<Menu>()>)` / `BuildContextMenu()` | **动态菜单工厂**：每次右键现搭（可依据当前状态）；优先于固定菜单 |
| `void SetContextMenuEnabled(bool)` / `bool IsContextMenuEnabled() const` | 是否允许右键出菜单（默认允许） |
| `virtual bool OnContextMenu(float,float)` | 右键钩子；返回 true 表示已处理，不再弹默认菜单 |
| `void SetBleed(float)` / `float GetBleed() const` | 缓存出血（默认 `4.0f`） |
| `template<typename Signal, typename Slot> Connection Connect(Signal&, Slot&&)` | 连接信号：连接登记进本元素的 `ConnectionGroup`（元素析构时自动断开），并返回 Qt 风格的**被动 `Connection` 句柄**（析构不断连）。忽略返回值安全；要单独断开写 `auto c = Connect(...); c.disconnect();` |

## 字体接口

| 方法 | 说明 |
| --- | --- |
| `static void SetGlobalFont(const FontSpec&)` | 全局字体 |
| `static void SetGlobalFontFamily(const std::wstring&)` | 只改全局字体族（保留其余字段） |
| `static void SetGlobalFontSize(float)` | 只改全局字号 |
| `static FontSpec GetGlobalFont()` | 读取全局字体 |
| `void SetFont(...)` / `SetFontFamily(...)` / `SetFontSize(...)` / `SetFontWeight(...)` | 实例级覆盖（覆盖后不再跟随全局字体） |
| `void ClearFont()` / `bool HasFontOverride() const` | 清除/查询覆盖 |
| `FontSpec GetEffectiveFontSpec() const` | 解析链结果（实例覆盖 → 类型默认 → 全局） |
| `IDWriteTextFormat* GetFontFormat() const` | 共享文本格式（内部有实例级快速缓存） |

## 自定义控件最小示例

```cpp
class Dot : public UIElement {
public:
    Dot(float r) : r_(r) {}
    Size Measure(const Size&) override { return Size(r_ * 2, r_ * 2); }
    void Draw(ID2D1RenderTarget* rt) override {
        ComPtr<ID2D1SolidColorBrush> b;
        rt->CreateSolidColorBrush(D2D1::ColorF(0, 0.47f, 0.84f), &b);
        D2D1_ELLIPSE e{ (arrangedRect_.x + r_), (arrangedRect_.y + r_), r_, r_ };
        rt->FillEllipse(e, b.Get());
    }
private:
    float r_;
};
```

## 布局参与 / 绘制时机

```cpp
enum class LayoutParticipation {
    Normal,            // 参与布局（默认）
    DrawBeforeLayout,  // 不参与布局：在正常布局树绘制之前单独绘制
    DrawAfterLayout    // 不参与布局：在正常布局树绘制之后、覆盖层之前单独绘制
};
void SetLayoutParticipation(LayoutParticipation);
LayoutParticipation GetLayoutParticipation() const;
bool ParticipatesInLayout() const;
```

- 布局容器（ColumnBox/RowBox/GridLayout）在 Measure/Arrange 以及 `ComposeImpl` 的子递归里都会**跳过** `!ParticipatesInLayout()` 的元素。
- 不参与布局的元素若被放进布局，其位置/尺寸需由设置者自理；`Window` 的自定义标题栏就是用它实现的（`Window` 直接把它放在 `(0,0)`）。

## 拖动区域（所有控件可用）

```cpp
void SetDraggable(bool on);                 // 整个 arrangedRect 作为拖动区
void SetDragRegion(const Rect& localRect);  // 指定本地子区域
void ClearDragRegion();
bool HasDragRegion() const;
Rect GetDragRegion() const;
bool IsPointInDragRegion(float x, float y) const;
virtual void CollectDragRegions(std::vector<Rect>& out) const;   // Window 递归收集
```

- `Window` 每帧把整棵树的拖动区域收集起来，命中时 `WM_NCHITTEST` 返回 `HTCAPTION`，于是拖动 / Aero Snap / 双击最大化全部交给系统。

###chapter: 布局 | Layout、ColumnBox、RowBox、GridLayout、LayoutHost、Card、Page、PageHost

## Layout（基类）

```cpp
class Layout : public UIElement {
    virtual ~Layout() = default;
    bool UseCache() const override;   // 布局容器默认不用缓存
};
```

- 布局容器**默认关闭缓存**（`UseCache()==false`），因为它们通常直接绘制/组织子元素，缓存收益低而显存占用高。

## ColumnBox（垂直布局）

```cpp
class ColumnBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
};
```

- 子元素自上而下排列；`spacing` 是相邻元素间距。
- 默认横向拉伸权重 `1.0`（子元素默认横向填满），纵向 `0.0`。

## RowBox（水平布局）

```cpp
class RowBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
};
```

- 子元素自左向右排列；默认横向 `0.0`、纵向 `1.0`。

## GridLayout（网格布局）

```cpp
class GridLayout : public Layout {
    enum class Alignment { Start, Center, End };
    void AddChild(std::shared_ptr<UIElement> child, int row, int col, int rowSpan = 1, int colSpan = 1);
    void SetSpacing(float horizontal, float vertical);
    void SetColumnStretch(int col, float weight);
    void SetRowStretch(int row, float weight);
    void SetHorizontalAlignment(Alignment align);
    void SetVerticalAlignment(Alignment align);
};
```

- 默认横纵间距 `0`、对齐 `Start`、默认横纵拉伸权重均 `1.0`。
- `SetColumnStretch/SetRowStretch` 控制某列/行在多余空间中的分配比例；`0` 表示不参与分配。
- `rowSpan/colSpan` 用于跨行跨列。`LayoutHost` 默认持有 `GridLayout`，因此 `Card` / `Page` 通常用 `GetLayoutAs<GridLayout>()` 直接加子元素。

## LayoutHost（可替换布局容器）

```cpp
class LayoutHost : public UIElement {
    std::shared_ptr<UIElement> GetLayout() const;
    void SetLayout(std::shared_ptr<UIElement> layout);
    template<typename T> std::shared_ptr<T> GetLayoutAs() const;
};
```

- 构造时默认持有一个 `GridLayout`。`GetLayoutAs<GridLayout>()` 是拿布局的常用方式。
- `SetLayout` 会替换内部布局并把其父元素设为自己；传入 `nullptr` 或自己会被忽略。

## Card（卡片）

```cpp
class Card : public LayoutHost {
    static float DefaultPadding;           // 12.0f
    static float DefaultCornerRadius;      // 8.0f
    static Color DefaultBgColor;           // 白
    static Color DefaultBorderColor;       // 200,200,200

    void SetPadding(float);
    void SetCornerRadius(float);
    void SetBackgroundColor(Color);
    void SetBorderColor(Color);
    void SetHoverBackgroundColor(Color);   // 悬停背景
    void SetHoverBorderColor(Color);       // 悬停边框
    void SetHoverAnimationSpeed(float);
    // 另有对应的 static SetDefault*
};
```

- 卡片是“带背景/边框/圆角/内边距的容器”。子元素加到它的内部布局（`GetLayoutAs<GridLayout>()`）。
- 支持悬停配色与平滑过渡动画；开启阴影时自动启用缓存。
- 禁用时整体绘制为灰色。

## Page（页面）

```cpp
class Page : public LayoutHost {
    static float DefaultPadding;     // 10.0f
    void SetPadding(float);
    void SetBackgroundColor(Color);
};
```

- 一个 `Page` 通常代表 `PageHost` 中的一屏内容。

## PageHost（页面宿主 / 切换动画）

```cpp
class PageHost : public UIElement {
    enum class TransitionDirection { Left, Right, Up, Down };
    enum class TransitionEasing { Linear, EaseInOut, EaseOut };   // 过渡缓动

    void AddPage(std::shared_ptr<Page> page);
    void NavigateTo(int index);
    void SetTransitionDirection(TransitionDirection dir);
    void SetTransitionEasing(TransitionEasing e);   // 默认 EaseInOut（缓入缓出，平滑）
    TransitionEasing GetTransitionEasing() const;
    void SetAnimationDuration(float seconds);   // 下限 0.01s，默认 0.3s
    int GetCurrentIndex() const;
    std::shared_ptr<Page> GetCurrentPage() const;
};
```

- `NavigateTo` 触发**滑动**切换，并可按 `TransitionEasing` 做缓动：默认 `EaseInOut`（Smoothstep，平滑）；`Linear` 为旧的匀速行为；`EaseOut` 为缓出。
- 动画期间只更新“源页”和“目标页”，动画结束后只更新当前页（避免同一页每帧被更新两次导致内嵌动画变快）。
- 非当前页的缓存会在切换结束后释放，以节省显存。
- **注意**：切换过程中不要假设 `GetCurrentIndex()` 已变为目标值——它在动画结束后才更新。

###chapter: 菜单 | MenuItem、Menu 与 MenuWindow

## MenuItem

```cpp
class MenuItem {
    enum class Type { Normal, Separator, Submenu };
    std::wstring text;
    std::function<void()> callback;
    std::shared_ptr<Menu> submenu;
    Type type = Type::Normal;
    bool enabled = true;
    std::shared_ptr<Label> icon;
    // ---- 增强字段 ----
    int id = 0;                      // 命令 id（配 Menu::ItemSelected）
    bool checkable = false;          // 显示勾选列
    bool checked = false;            // 勾选状态
    bool radio = false;              // 单选（同菜单内互斥）
    bool isDefault = false;          // 默认项：加粗 + 回车触发
    bool danger = false;             // 危险项：红字
    std::wstring shortcut;           // 右侧快捷键提示（仅展示）
    std::optional<Color> bgColor;    // 该项自定义背景色（带圆角）
    std::optional<Color> textColor;  // 该项自定义文字色
    std::optional<FontSpec> font;    // 该项自定义字体（缺省=Menu::font）
};
```

## Menu

```cpp
class Menu : public std::enable_shared_from_this<Menu> {
    inline static FontSpec DefaultFont{};              // 全局默认菜单字体
    void AddItem(const std::wstring& text, std::function<void()> callback = nullptr, int id = 0);
    void AddCheckItem(const std::wstring& text, bool checked,
                      std::function<void(bool)> onToggle = nullptr, int id = 0, bool radio = false);
    void AddSeparator();
    void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu, int id = 0);

    std::vector<std::shared_ptr<MenuItem>> items;
    std::function<void(Menu&)> onOpening;   // 弹出前回调（可现场改勾选/启用/文字）
    ZSignal<int> ItemSelected;              // 任一普通项被点（带 id；根菜单接收子菜单的点击）
    FontSpec font = DefaultFont;            // 菜单字体
    static void SetDefaultFont(const FontSpec&);

    void ShowAt(int screenX, int screenY);  // 独立弹出（不绑定任何窗口；典型用途=托盘菜单）
    void ShowAtCursor();                    // 在鼠标当前位置弹出
};
```

- 用 `AddItem` 添加可点击项，`AddSeparator` 添加分隔线，`AddSubmenu` 添加子菜单，`AddCheckItem` 添加勾选/单选项。
- 把菜单设置到元素：`element->SetContextMenu(menu);`（或用 `SetContextMenuFactory` 动态现搭），或设置到窗口：`window.SetContextMenu(menu);`。
- **独立弹出**：`menu->ShowAtCursor()` 在鼠标处弹出，不绑定窗口，适合托盘右键菜单；会先关掉其它已打开的菜单（互斥）。
- **信号槽**：项可传 `id`，用 `menu->ItemSelected.connect([](int id){...})` 统一收；`菜单项->Clicked` / `AddItem` 的回调仍可用。`onOpening` 在每次弹出前调用，可现场改勾选/启用/文字。
- **外观**：`MenuItem::bgColor` / `textColor` / `font` 逐项覆盖；`Menu::font` 整体字体；悬停高亮为半透明叠加（自定义底色上仍可见）。支持 `isDefault`（加粗 + 回车触发）、`danger`（红字）、`shortcut`（右侧快捷键提示）、`enabled=false`（禁用）。

## MenuWindow（弹出菜单窗口，框架内部使用）

```cpp
class MenuWindow {
    MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y);
    void Show(int x, int y);
    void Hide();
    void CloseAll();
};
```

- 一般不需要直接使用；右键时由框架自动创建。

###chapter: 窗口 Window | 创建、背景、标题栏与根布局

```cpp
class Window {
    static Backdrop DefaultBackdrop;              // None
    static DWORD DefaultBackdropColor;            // 0
    static Color DefaultBackgroundColor;          // 透明

    bool Create(int width, int height, const std::wstring& title);   // 创建后不自动显示，见 Show()
    void Run();

    void SetRootLayout(std::shared_ptr<Layout> layout);
    std::shared_ptr<Layout> GetRootLayout() const;
    std::shared_ptr<ColumnBox> GetRootColumnBox() const;

    void SetBackdrop(Backdrop backdrop, DWORD tint = 0x00000000);
    Backdrop GetBackdrop() const;
    void SetBackgroundColor(Color color);
    void SetContextMenu(std::shared_ptr<Menu> menu);

    void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border);
    void SetCaptionColor(COLORREF color);
    void SetTitleTextColor(COLORREF color);
    void SetBorderColor(COLORREF color);

    void SetMinSize(int width, int height);
    void SetPosition(int x, int y);            // 按 DIP 移动窗口（多窗口常用）
    void SetSize(int width, int height);       // 按 DIP 设置窗口尺寸
    bool IsValid() const;                      // 窗口是否仍然有效（未销毁）
    int GetId() const;                         // 窗口唯一 id（元素按 id 记录归属，避免悬垂）
    void Close();                              // 关闭本窗口（其余窗口不受影响）

    void SetOwner(Window* owner);              // 设置所有者：owned 子窗口，始终在所有者之上、随其最小化
    Window* GetOwner() const;
    std::vector<Window*> GetOwnedWindows() const;   // 本窗口拥有的全部 owned 子窗口
    int RunModal(Window* owner = nullptr);     // 模态运行：禁用 owner，嵌套消息循环，关闭后恢复

    // 闪烁提示（默认连闪 5 次；captionOnly=true 只闪标题栏，不闪任务栏）
    void Flash(int times = 5, bool captionOnly = true);
    void StopFlash();

    // 样式 / 扩展样式便捷设置（如 SetWindowExStyleFlag(WS_EX_TOOLWINDOW, true)）
    DWORD GetStyle() const;
    DWORD GetExStyle() const;
    void SetWindowStyleFlag(DWORD flag, bool on);
    void SetWindowExStyleFlag(DWORD flag, bool on);

    // 显示 / 隐藏 / 置顶（z 序最上）
    void Show();
    void ShowNoActivate();
    void Hide();
    void Raise();

    // owned 子窗口被“单独最小化”的处理策略
    enum class OwnedMinimizePolicy {
        None,            // 不处理（会出现老式“小瓷砖”）
        Hide,            // 拦截最小化 → 隐藏；父窗口还原/激活时恢复
        DisableMinimize  // 禁用最小化按钮，并忽略最小化相关消息
    };
    void SetOwnedMinimizePolicy(OwnedMinimizePolicy p);
    OwnedMinimizePolicy GetOwnedMinimizePolicy() const;

    // 背景层（只有三个选项）：None=无（完全透明）/ Acrylic=亚克力 / Mica=云母
    // enum class Backdrop { None, Acrylic, Mica };
    // SetBackdrop(背景层, 背景叠加层)：叠加层是带 Alpha 的颜色，直接叠在背景层之上
    void SetBackdrop(Backdrop layer, DWORD overlayArgb = 0x00000000);
    Backdrop GetBackdrop() const;

    // 背景“4 层参数模型”（全部可调，见下方说明）
    struct BackgroundParams {
        float        blurAmount;    // Blur 层：高斯模糊 σ
        float        saturation;    // Luminosity 层：饱和度（Legacy/RS2 配方用；Luminosity 配方无此步）
        D2D1_COLOR_F tint;          // Tint 层：颜色（含 alpha，即“叠色/白纱”）
        D2D1_COLOR_F luminosity;    // Luminosity 层：亮度色
        float        noiseOpacity;  // Noise 层：噪点不透明度
    };
    inline static BackgroundParams AcrylicParams;   // 亚克力：源 = 宿主背景（后面窗口的内容）
    inline static BackgroundParams MicaParams;      // 云母：源 = 缓存的桌面壁纸
    enum class AcrylicPreset { Legacy, Luminosity, Base, Thin };   // 官方亚克力配方预设
    static void SetBackgroundParams(Backdrop layer, const BackgroundParams& p);  // 用一套参数
    static void SetBackgroundParams(Backdrop layer, AcrylicPreset preset);       // 用预设

    // 事件信号
    ZSignal<bool*> Closing;           // 请求关闭；槽置 *cancel = true 可取消（如关闭前询问保存）
    ZSignal<> BackdropUnsupported;    // 所选实现当前系统不支持
    ZSignal<> DeviceLost;             // 设备丢失（GPU 移除/重置/交换链失效）
    ZSignal<HRESULT> RenderingError;  // 渲染致命错误

    // 自定义标题栏（控件见“窗口工具”章节）
    void SetCustomTitleBar(std::shared_ptr<UIElement> bar);   // 传 nullptr 恢复原生标题栏
    std::shared_ptr<UIElement> GetCustomTitleBar() const;
    bool HasCustomTitleBar() const;
    float GetCustomTitleBarHeight() const;
    void SetTitleBarVisible(bool on);                         // 隐藏/显示（保留自定义边框）
    bool IsTitleBarVisible() const;
    void SetTitle(const std::wstring& title);                 // 原生标题栏文字（自定义标题栏用 TitleBar::SetTitle）
    std::wstring GetTitle() const;                            // 取当前窗口标题（GetWindowTextW）
    HWND GetHwnd() const;                                     // 原生窗口句柄
    void SetIcon(HICON bigIcon, HICON smallIcon);             // 原生标题栏图标（WM_SETICON）

    // 边框 / 调整
    void SetResizable(bool on);
    bool IsResizable() const;

    // 圆角偏好（仅一次 DWM 调用；不支持的系统会忽略）
    enum class WindowCorner { Default, Square, Round, RoundSmall };
    void SetWindowCorner(WindowCorner c);
    WindowCorner GetWindowCorner() const;

    // 窗口动作（供自定义标题栏按钮调用）
    void Minimize();
    void Maximize();
    void Restore();
    void MaximizeRestore();
    bool IsMaximizedWindow() const;
    void BeginSystemDrag();                    // ReleaseCapture + WM_NCLBUTTONDOWN(HTCAPTION)

    void SetMouseCapture(UIElement* elem);
    void ReleaseMouseCapture(UIElement* elem);
};
```

`Backdrop`（**背景层**，只有三选一）：`None`（无 —— 背景完全透明）、`Acrylic`（亚克力）、`Mica`（云母）。`SetBackdrop` 第二个参数是**背景叠加层**：一个带 Alpha 的颜色，直接叠在背景层之上（`A` = 透出/覆盖面，`RGB` = 叠加色）。**不再有 `BackdropMode`** —— 背景一律由 ZufyUI 自己实现（不依赖系统材质），Win10/Win11 效果一致。

**行为与易混点**：

- `Create` 创建窗口、初始化渲染资源、应用背景，并生成一个默认根布局：带 `margin 20`、`spacing 10` 的 `ColumnBox`（`GetRootColumnBox()` 即它）。**1.8.0 起 `Create` 不再自动显示窗口**，需要显示时由应用调用 `Show()`。
- `Run()` 进入消息循环，阻塞直到窗口关闭。
- 窗口内部维护 16ms 定时器驱动动画；`timeBeginPeriod(1)` / `timeEndPeriod(1)` 用于降低定时器抖动。
- 帧时间 `deltaTime` 被钳制在 `0.033s`，空闲或最小化恢复后不会让动画一帧跳到终点。
- 标题栏颜色是 Win11 的 DWM 属性；在不支持的系统上会被忽略。
- 阴影：窗口负责把元素的阴影合成进其离屏缓存；ToolTip 也由窗口统一绘制。
- **owned 子窗口**：用 `CreateWindow(...)` / `SetOwner` 建立的所有者关系在**创建时**即生效（`CreateWindowEx` 的父窗口参数），因此 owned 窗口**不会有独立任务栏按钮**；父窗口最小化时会**无条件**隐藏其 owned 子窗口、还原/激活时恢复（不依赖系统“最小化分组”，避免父窗口在后台时子窗口不跟随）。
- **模态**：`RunModal` 用官方机制实现——`EnableWindow(owner, FALSE)` + 激活模态窗 + `IsDialogMessage` 嵌套消息循环；**不使用任何全局钩子**，不会影响其它进程。点击被禁用的所有者时由系统给出标准提示。
- **`OwnedMinimizePolicy`**：处理 owned 子窗口被“单独最小化”的情况。`None` 不处理（会出现老式小瓷砖）；`Hide` 拦截最小化改为隐藏、父窗口还原/激活时恢复；`DisableMinimize` 置灰最小化按钮并忽略最小化相关消息。
- **裸引用清理**：窗口销毁时会清除其它窗口对它的引用（`owner_` 与隐藏列表），避免地址被复用后牵连不相干的窗口。
- **自定义标题栏**：`SetCustomTitleBar(bar)` 安装一个"不参与布局"的标题栏控件（见"窗口工具"章节），Window 把它放在 `(0,0)`、根布局整体下移其高度；传入 `nullptr` 恢复原生标题栏（会发 `SWP_FRAMECHANGED` 全量刷新）。`SetTitleBarVisible(false)` 可隐藏标题栏但保留自定义边框。
- **背景（`Backdrop` 三选一 + 叠加色）**：背景层只有 `None / Acrylic / Mica`，且**一律由 ZufyUI 自己实现**——不再分"系统/手动"、不依赖系统材质（Win10/Win11 效果一致）。
  - `Acrylic`：我们自己的 DirectComposition 官方亚克力配方（Border 噪点平铺 → Opacity → 高斯模糊 → Luminosity/Color 混合 → 噪点叠乘，另加 Saturation 与 tint 的 `CompositeStep` 让 alpha 生效）；源 = **宿主背景**（窗口后面的内容）。
  - `Mica`：读取桌面壁纸（`SPI_GETDESKWALLPAPER` + WIC 解码）→ 模糊 / 降饱和 / 叠白 / 噪点，一次性烘焙成位图，放进**独立合成器图层**（位于内容层下方）；窗口移动时只改该图层 `Offset`（只是平移变换，不重采样）。
  - 参数走"**4 层模型**"（Blur / Luminosity / Tint / Noise）：`SetBackgroundParams(背景层, BackgroundParams)` 传整套参数，或 `SetBackgroundParams(背景层, AcrylicPreset)` 传官方预设（`Legacy / Luminosity / Base / Thin`）。`AcrylicParams` / `MicaParams` 是默认值（`MicaParams` 为调好的观感）。
  - 所请求效果在当前系统无法实现时触发 `BackdropUnsupported` 信号，**不会**用别的效果凑合。
- **渲染架构（1.8.0）**：Direct2D 1.1 + DXGI flip SwapChain + DirectComposition（`WS_EX_NOREDIRECTIONBITMAP`），支持逐像素透明；进程级共享 D3D11/D2D 设备与 WinRT `ICompositor`。`Create` **不再自动显示窗口**，需应用显式 `Show()`。
- **边框/调整/圆角**：`SetResizable` 控制拖边缩放；分屏时会保留 DWM 边框/阴影（`WM_NCCALCSIZE` 只内缩被吸附的边），最大化不做内缩。`SetWindowCorner` 映射到 `DWMWA_WINDOW_CORNER_PREFERENCE`。

###chapter: 应用与多窗口 | Application 与多窗口

## Application

`Application` 是进程 / UI 线程级的应用对象，管理一个共享消息循环与所有顶层窗口。它与窗口的生命周期解耦：`Application` 只是内部单例的句柄，构造 / 析构它**不会**销毁已创建的窗口（因此不会出现“app 先析构、窗口悬垂”这类问题）。

```cpp
class Application {
    std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title);
    std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title, Window* owner); // owned 子窗口
    void AddWindow(const std::shared_ptr<Window>& w);
    int Run();
    void Quit(int code = 0);
    void CloseAllWindows();
    size_t WindowCount() const;
    bool RegisterApp(const AppInfo& info);   // 应用身份自注册（见「应用身份自注册」）
    static Application& Instance();
};
```

**推荐用法（Qt 风格）**：

```cpp
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ZufyUI::Application app;
    auto w1 = app.CreateWindow(1000, 700, L"主窗口");
    auto w2 = app.CreateWindow(640, 480, L"工具窗口");
    // 各自搭 UI：w1->GetRootColumnBox()->AddChild(...)
    return app.Run();                    // 单一消息循环，所有窗口共用
}
```

**行为与要点**：

- `CreateWindow` 创建窗口并登记进应用；返回的 `shared_ptr<Window>` **必须由你持有**，否则 `shared_ptr` 析构会让窗口立即被销毁。
- `Run()` 只运行**一个**消息循环；同线程创建的所有窗口消息都由它分发。
- 在 `Run()` 之后（消息循环运行期间）调用 `CreateWindow` 也能正常显示新窗口。
- 关闭任意窗口不影响其它窗口；**最后一个窗口关闭时**循环自动退出。也可用 `Quit()` 主动退出。
- 单进程应只有一个 UI 线程在跑窗口（与 Win32 一致）。

## 应用身份自注册（RegisterApp / AppInfo）

把「应用名 + 图标」声明一次，库统一替你设好**进程级 AppUserModelID**，并（在授权后）写注册表 + 把图标缓存到本地，让 **toast 左上角显示应用名与图标**、跳转列表 / 任务栏分组也统一归属该身份。

```cpp
struct AppInfo {
    std::wstring displayName;      // 显示名称（toast / 跳转列表 / 任务栏分组）
    std::wstring aumid;            // 唯一 ID；留空=由 displayName 自动派生
    std::shared_ptr<Image> icon;   // 图标（可空）
};

bool RegisterApp(const AppInfo& info);                 // 自由函数
bool Application::RegisterApp(const AppInfo& info);    // 转发到上面（两者等价）
```

**授权宏（重要）**：本库不是系统库，替应用动注册表/文件属于越界行为，因此**必须显式授权**——在包含本库头文件**之前**定义宏：

```cpp
#define ZUFYUI_ALLOW_APP_REGISTRATION
```

未定义该宏时：`RegisterApp` 只设置进程 AUMID（**无副作用**），不写注册表、不落文件。

**授权后库会做什么**：

1. 设置进程级 AUMID：`SetCurrentProcessExplicitAppUserModelID`
2. 写注册表 `HKCU\Software\Classes\AppUserModelId\<AUMID>`：`DisplayName` + `IconUri`
3. 把图标缓存到 `%LOCALAPPDATA%\ZufyUI\AppReg\<AUMID>\app.ico`（由 `Image` 导出为单图 `.ico`）
4. **进程退出时**清空该缓存目录，并删除上面写的注册表项（避免 `IconUri` 指向已删除文件）

**用法**（建议创建窗口前调用一次）：

```cpp
#define ZUFYUI_ALLOW_APP_REGISTRATION   // 需在 include 之前
...
int WINAPI WinMain(...) {
    RegisterApp(AppInfo{ L"我的应用", L"ZufyUI.MyApp", myIcon });   // aumid 可留空自动派生
    auto w = Application::Instance().CreateWindow(/*...*/);
    // ...
    return Application::Instance().Run();
}
```

**要点**：

- AUMID 每进程唯一；**只需调用一次**。`aumid` 留空时由 `displayName` 派生（去空格/非法字符；无点号则自动加 `ZufyUI.` 前缀，最长 128 字符）。
- 授权宏是**编译期**开关；未授权不会在用户机器上留下任何痕迹。
- Win10/11 里老式托盘气球会升级成 toast，其左上角图标/应用名由系统按 AUMID 查注册表得到——这一步正是「库替应用注册」解决的事（否则为空白）。
- 退出清理在库内部完成，无需应用写任何代码。

## 多窗口的独立性与兼容

- **重绘 / 布局按窗口路由**：每个元素记录所属窗口（`UIElement::GetWindow()`）；`RequestRepaint()` / `InvalidateLayout()` 只作用于所属窗口，窗口之间不会互相触发重绘。
- **DPI 每窗口**：当前 DPI 缩放是**线程本地**的，渲染每个窗口前会设置该窗口自己的缩放，所以混合 DPI 的多窗口也能正确 `Snap`。
- **激活 / 失活按窗口**：`Window` 提供实例信号 `Activated` / `Deactivated` / `Closed`，以及 `Closing`（`bool*`，置 `*cancel=true` 可取消关闭）、`BackdropUnsupported` / `DeviceLost` / `RenderingError`；全局 `UIZSignals::WindowDeactivated` 带 `Window*` 参数、`GlobalMouseDown` 带 `Window*`。ComboBox 等控件据此只响应“本窗口”的事件，不会因别的窗口而误收起。
- **叠加绘制按窗口**：全局 `UIZSignals::DrawOverlay` 现在带 `Window*`（以及渲染目标）参数；订阅者**必须**用 `GetWindow()` 过滤，否则会把 A 窗口的下拉/弹层画到 B 窗口上（这是多窗口下最典型的串扰）。ZufyUI 自带控件已按此处理。
- **鼠标捕获**：Win32 `SetCapture` 只用于“按住鼠标”的拖拽，松开立即释放（元素级逻辑捕获不受影响）。修复了“ComboBox 展开后一直持有系统鼠标捕获、导致其它窗口无法使用”的问题。
- **模态 / 父子窗口**：`Window::SetOwner(owner)` 建立 owned 子窗口（始终在所有者之上、随其最小化）；`Window::RunModal(owner)` 以模态运行（禁用所有者、嵌套消息循环、关闭后恢复）。模态期间点击被禁用的所有者窗口，模态窗口会**闪烁**提示。
- **窗口句柄与悬垂**：元素内部用**窗口 id** 记录所属窗口（而不是裸指针），窗口销毁后 `GetWindow()` 返回 `nullptr`，从根本上避免“元素持有已销毁窗口指针”导致的崩溃。
- **共享资源**：`ID2D1Factory` 与系统计时器精度（`timeBeginPeriod`）由应用核心统一管理，多窗口共享。
- **向后兼容**：单窗口写法仍然有效——`Window win; win.Create(...); win.Show(); win.Run();`（1.8.0 起 `Create` 不再自动显示，需显式 `Show()`）；`Run()` 会转发到应用级消息循环。

## 多窗口常见坑

- `CreateWindow` 返回的 `shared_ptr` 不要丢；丢了窗口即被销毁（可像示例那样放进容器长期持有）。
- 需要窗口归属的全局信号（`GlobalMouseDown`、`WindowDeactivated`）都带上了 `Window*`；订阅时用 `GetWindow()` 过滤，只处理本窗口。
- 所有窗口必须在同一 UI 线程创建与运行。

###chapter: 基础控件 | Label、Button、TextBox、ComboBox、ToggleSwitch、CheckBox、ScrollViewer、ProgressBar、Slider

## 辅助函数

```cpp
enum class TextHAlign { Left, Center, Right };

inline void DrawTextWithEllipsis(ID2D1RenderTarget* rt,
    const std::wstring& text, const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color, const FontSpec& spec,
    ComPtr<ID2D1SolidColorBrush>& textBrush,
    IDWriteTextFormat* textFormat = nullptr, bool forceNoWrap = false,
    TextHAlign align = TextHAlign::Left);
```

- 单行绘制并按需截断加省略号；`align` 控制水平对齐（用于表格列对齐等）。

## Label

```cpp
enum class TextOverflow { Wrap, Ellipsis };

class Label : public UIElement {
    Label(const std::wstring& text = L"Label");

    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    void SetTextColor(Color color);
    Color GetTextColor() const;
    void SetTextOverflow(TextOverflow mode);
    TextOverflow GetTextOverflow() const;
    void SetAlignment(HAlign hAlign, VAlign vAlign);   // HAlign/VAlign 见下
    HAlign GetHorizontalAlignment() const;
    VAlign GetVerticalAlignment() const;

    void SetPadding(const Thickness&);   // 也可传 float 表示四边相同
    void SetLineSpacing(float);
    void SetMaxLines(int);               // 超出以省略号结尾
    Size GetDesiredSize() const;         // 当前内容期望尺寸

    static void SetDefaultTextColor(Color);
    static void SetDefaultFontSize(float);
    static void SetDefaultOverflow(TextOverflow);
    static void SetDefaultAlignment(HAlign, VAlign);

    // 图标 / 图片
    void SetImage(std::shared_ptr<Image> image);
    std::shared_ptr<Image> GetImage() const;
    void SetIconSize(float w, float h);      // 0 表示按图像原尺寸
    Size GetIconSize() const;
    void SetIconSpacing(float spacing);      // 图标与文本/子控件的间距
    float GetIconSpacing() const;

    // 嵌套子控件（内联横排：图标 + 文本 + 子控件）
    void AddChild(std::shared_ptr<UIElement> child);
    void ClearChildren();
    size_t GetChildCount() const;
};
```

- 默认：黑色文字、`Ellipsis`、左对齐、垂直居中、字号 `16`。
- `SetMaxLines` 只在 `Wrap` 模式下有意义；`Ellipsis` 本身就是单行。
- 禁用时文字自动变为灰色（`DefaultDisabledColor`）。
- **图标**：`SetImage` 后 Label 在文本左侧绘制图标；`SetIconSize(0,0)` 表示用图像原尺寸。图标可与文字、子控件共存（即使没有文字也会绘制）。
- **嵌套子控件**：`AddChild` 的子控件与图标、文本**内联横排**，并参与 `GetChildren()` 递归（窗口归属、重绘、布局都按子控件处理）。

## Button

```cpp
class Button : public UIElement {
    ZSignal<> Clicked;
    ZSignal<bool> Toggled;               // 仅 SetCheckable(true) 后有意义

    Button(const std::wstring& text = L"Button");
    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    void SetColors(Color normal, Color hover, Color pressed);
    void SetTextColor(Color color);
    void SetCornerRadius(float radius);
    Color GetTextColor() const;
    Color GetNormalColor() const;
    Color GetHoverColor() const;
    Color GetPressedColor() const;
    float GetCornerRadius() const;
    void SetHoverAnimationSpeed(float speed);

    void SetCheckable(bool);             // 可切换按钮
    void SetChecked(bool);
    bool IsChecked() const;              // 依赖 SetCheckable(true) 才有意义
    void SetTextAlignment(...);          // 文字对齐
    void SetPadding(...);                // 内边距
    void SetAutoRepeat(bool);            // 按住自动重复触发 Clicked

    static void SetDefaultColors(Color, Color, Color);
    static void SetDefaultSize(float width, float height);
};
```

- 默认尺寸 `120×36`，圆角 `4`，蓝底白字（常态 `0,120,212`）。
- 鼠标在按钮内按下、抬起时触发 `Clicked`（在别处抬起不触发）。
- 键盘：获得焦点后按 `Enter` 或 `Space` 触发 `Clicked`。
- 禁用时不响应事件并绘制为灰色。
- 可切换按钮：`SetCheckable(true)` 后点击切换选中态并触发 `Toggled(bool)`。

## TextBox

```cpp
class TextBox : public UIElement {
    ZSignal<const std::wstring&> TextChanged;
    ZSignal<> ReturnPressed;

    TextBox();
    std::wstring GetText() const;
    void SetText(const std::wstring& text);
    void SetPlaceholder(const std::wstring& placeholder);
    void SetPlaceholderColor(Color);
    void SetPasswordMode(bool mode);
    void SetRevealPassword(bool reveal);     // 密码模式下临时明文显示
    bool IsPasswordRevealed() const;
    void SetMaxLength(int maxLength);        // -1 不限
    int  GetMaxLength() const;

    void SetReadOnly(bool);

    void SetInputFilter(std::function<bool(wchar_t)>);  // 返回 true 才接受该字符

    bool IsFocused() const;
    void Focus();
    void Blur();

    // 选区 / 编辑（公开）
    int GetSelectionStart() const;
    int GetSelectionEnd() const;
    int GetCursorPosition() const;
    void SetSelection(int start, int end);
    void SelectAll();
    void Undo(); void Redo();
    void Copy(); void Cut(); void Paste();

    void SetTextColor(Color); SetBackgroundColor(Color); SetBorderColor(Color);
    void SetSelectionColor(Color); SetHoverBackgroundColor(Color); SetHoverBorderColor(Color);
    void SetCursorBlinkInterval(float);
};
```

- 默认尺寸 `160×30`，字号 `14`，光标闪烁 `0.5s`。
- 支持 `Ctrl+C/X/V/A/Z/Y`、方向键、`Home/End`、`Shift+方向键` 选区、鼠标拖选，以及 IME 组合输入。
- **选区累积**：按住 Shift 连按方向键会逐字符累积高亮（内部用独立锚点，不会每按一次塌缩成一个字符）。
- `SetInputFilter` 是唯一字符级过滤入口，例如“仅数字”：`[](wchar_t c){ return c >= L'0' && c <= L'1'; }` 风格。
- `SetReadOnly(true)` 后仍可选择/复制，但不能编辑。
- `TextChanged` 在每次文本变化时触发；`ReturnPressed` 在按回车时触发。

## ComboBox

```cpp
class ComboBox : public UIElement {
    ZSignal<int> SelectionChanged;
    ZSignal<> DropDownOpened;
    ZSignal<> DropDownClosed;

    ComboBox();
    void AddItem(const std::wstring& item);
    void SetItems(const std::vector<std::wstring>& items);
    void InsertItem(int index, const std::wstring& item);
    void RemoveItemAt(int index);
    void RemoveItem(const std::wstring& item);
    void ClearItems();
    int GetItemCount() const;
    std::wstring GetItemAt(int index) const;
    const std::vector<std::wstring>& GetItems() const;

    void SetSelectedIndex(int index);
    int GetSelectedIndex() const;
    std::wstring GetSelectedText() const;
    bool IsExpanded() const;
    void Collapse();
    void Expand();
    void SetOpen(bool);

    void SetPlaceholder(const std::wstring&);

    void SetItemDisabled(int index, bool disabled = true);
    bool IsItemDisabled(int index) const;

    void SetMaxVisibleItems(int count);      // 0 表示不限制

    // 可编辑 + 输入过滤
    void SetEditable(bool);
    bool IsEditable() const;
    void SetFilterEnabled(bool);
    bool IsFilterEnabled() const;
    void SetEditText(const std::wstring&);
    std::wstring GetEditText() const;

    // 样式：SetNormalBgColor / SetHoverBgColor / SetBorderColor / SetIndicatorColor / SetListItemHeight 等
};
```

- 默认尺寸 `160×30`，列表项高 `24`，指示条宽 `3`、高占比 `0.6`。
- 展开时列表绘制在 `DrawOverlay` 浮层上，所以可以超出自身边界。
- `SetMaxVisibleItems(n)` 限制下拉最多显示 `n` 行，超出部分滚动。
- **可编辑 + 过滤**：`SetEditable(true)` 后可输入；`SetFilterEnabled(true)` 时输入内容会即时筛选下拉项（大小写不敏感的子串匹配）；`GetSelectedIndex()` 对应**筛选后**的列表。带闪烁光标，点击可定位光标位置（当前版本不支持文本区间选区）。
- `SetItemDisabled` 的禁用项不可被选择，键盘上下键会跳过。
- **宽度自适应**：折叠框宽度取“选项文本平均宽度”（放不下当前文本时用省略号截断，并自动挂 `ToolTip` 显示完整文本）；**下拉列表宽度取最宽选项**，保证每个选项都完整显示（列表的命中检测、背景、滚动条都按该宽度对齐）。

## ToggleSwitch

```cpp
class ToggleSwitch : public UIElement {
    ZSignal<bool> Toggled;

    ToggleSwitch(bool initialState = false);
    void SetOn(bool on);
    bool IsOn() const;
    void SetColors(Color on, Color off, Color knob);
    void SetSize(float width, float height);
    void SetIndeterminate(bool indeterminate);   // 不确定态（半选观感）
    void SetLabel(const std::wstring&);
    void SetLabelColor(Color);
    void SetAnimationSpeed(float speed);
};
```

- 默认尺寸 `50×24`，开 `0,120,212`、关 `200,200,200`、滑块白。
- 键盘：获得焦点后 `Space`/`Enter` 切换。

## CheckBox

```cpp
enum class State { Unchecked, PartiallyChecked, Checked };

class CheckBox : public UIElement {
    ZSignal<bool> Toggled;
    ZSignal<State> StateChanged;

    CheckBox(bool checked = false);
    void SetChecked(bool); bool IsChecked() const;
    State GetState() const; void SetState(State);
    void Toggle();

    void SetTriState(bool);
    void SetSize(float);
    void SetCornerRadius(float);
    void SetBoxColor(Color); void SetBorderColor(Color); void SetCheckColor(Color);
    void SetHoverBoxColor(Color);
    void SetAnimationSpeed(float);
    void SetLabel(const std::wstring&); void SetLabelColor(Color);

    // 静态绘制（供列表/表格行复选框复用）
    static void DrawBox(ID2D1RenderTarget*, const D2D1_RECT_F& rect, float fill,
                        State state, D2D1_COLOR_F boxColor, D2D1_COLOR_F checkColor,
                        D2D1_COLOR_F borderColor, float cornerRadius,
                        ComPtr<ID2D1SolidColorBrush>& brush, bool enabled = true);
};
```

- 勾选有进度动画；悬停有淡蓝光晕渐变动画。
- `SetTriState(true)` 后 `Toggle()` 在 未选 → 半选 → 全选 之间循环。
- 键盘 `Space`/`Enter` 切换；禁用时置灰。
- `DrawBox` 是静态函数，`ListView`/`TableView`/`TreeView` 的行勾选框复用它，因此观感一致。

## ScrollViewer

```cpp
enum class ScrollBarVisibility { Auto, Always, Hidden };

class ScrollViewer : public UIElement {
    ZSignal<float, float> ScrollChanged;     // (offsetX, offsetY)

    ScrollViewer();
    void SetContent(std::shared_ptr<UIElement> content);
    std::shared_ptr<UIElement> GetContent() const;

    void SetVerticalScrollEnabled(bool enabled);
    void SetHorizontalScrollEnabled(bool enabled);
    void SetVerticalScrollBarVisibility(ScrollBarVisibility);
    void SetHorizontalScrollBarVisibility(ScrollBarVisibility);
    ScrollBarVisibility GetVerticalScrollBarVisibility() const;
    ScrollBarVisibility GetHorizontalScrollBarVisibility() const;

    void SetScrollBarWidth(float width);
    void SetScrollWheelStep(float step);
    void SetAnimationSpeed(float speed);
    void SetContentMargin(const Thickness&);
    Thickness GetContentMargin() const;
    void SetScrollBarColors(Color track, Color thumb, Color hoverThumb);
    float GetScrollOffsetX() const;
    float GetScrollOffsetY() const;

    void ScrollTo(float offsetX, float offsetY, bool animated = true);
    void ScrollBy(float deltaX, float deltaY, bool animated = true);
};
```

- 滚动条宽 `8`、最小长度 `20`、滚轮步长 `30`。
- `Auto`：内容超出才显示；`Always`：始终显示；`Hidden`：不显示滚动条（仍可用滚轮/代码滚动）。
- `ScrollChanged(x,y)` 在偏移变化时触发（含动画过程中）。
- `SetContentMargin` 给内容加内边距。
- 内部 `ScrollBar` 类一般无需直接使用。

## ProgressBar

```cpp
class ProgressBar : public UIElement {
    ZSignal<float> ValueChanged;

    ProgressBar();
    void SetValue(float value);        // [0,1]，会关闭不确定模式
    float GetValue() const;
    void SetRange(float min, float max);
    float GetMin() const; float GetMax() const;
    void SetRangeValue(float value);   // 在 [min,max] 内取值
    float GetRangeValue() const;
    void SetShowText(bool); bool IsShowText() const;
    void SetTextColor(Color);
    void SetIndeterminate(bool); bool IsIndeterminate() const;
    void SetTrackColor(Color); void SetFillColor(Color); void SetBorderColor(Color);
    void SetIndeterminateBlockWidth(float); void SetIndeterminateSpeed(float);
    // ... 其余默认值/颜色 setter
};
```

- 默认尺寸 `200×20`，不确定块宽 `40`、速度 `100`。
- `SetValue` 归一化到 `[0,1]`；`SetRangeValue` 用 `[min,max]`。
- `SetShowText(true)` 在进度条上显示百分比文本；禁用时填充色置灰。

## Slider

```cpp
class Slider : public UIElement {
    ZSignal<float> ValueChanged;
    ZSignal<> SliderReleased;           // 拖拽结束（或键盘调整结束）

    Slider();
    void SetRange(float min, float max);
    void SetValue(float value);
    float GetValue() const;
    void SetStep(float step);
    float GetStep() const;
    void SetSnapToStep(bool); bool IsSnapToStep() const;
    void SetTrackColor(Color); void SetFillColor(Color);
    void SetThumbColor(Color); void SetHoverThumbColor(Color);
    void SetThumbSize(float); void SetTrackHeight(float);
};
```

- 默认尺寸 `160×24`，默认范围 `[0,100]`，轨道高 `4`，滑块直径 `14`。
- 方向键按 `step` 调整；`Home/End` 到范围两端。
- `SetSnapToStep(true)` 会把实时值吸附到 step 的整数倍。

###chapter: 数据视图 | ListView、TableView、TreeNode、TreeView

> 数据视图的**选择、勾选、排序、禁用**有大量细节，且框架内部对“每项/每行的附加信息”做了特殊处理，请仔细阅读每节的“要点”。

## ListView

```cpp
enum class SelectionMode { Single, Extended, Multi, None };

class ListView : public UIElement {
    ZSignal<int> SelectionChanged;
    ZSignal<int> ItemClicked;
    ZSignal<int> ItemDoubleClicked;
    ZSignal<std::vector<int>> SelectionChangedMulti;
    ZSignal<int, bool> ItemCheckStateChanged;
    ZSignal<int> ItemRightClicked;   // 右键点击项（传行号）

    ListView();
    // 数据
    void AddItem(std::shared_ptr<Label>);
    void AddItem(const std::wstring&);
    void InsertItem(int index, const std::wstring&);
    void RemoveItem(int index);
    void Clear();
    void SetItem(int index, std::shared_ptr<Label>);
    void SetItem(int index, const std::wstring&);
    std::shared_ptr<Label> GetItemLabel(int index) const;
    std::wstring GetItemText(int index) const;
    int GetItemCount() const;
    void InsertItems(int index, const std::vector<std::wstring>&);
    void BeginUpdate();   // 批量增删：挂起刷新（v1.9.6）
    void EndUpdate();     // 结束时统一刷新一次
    void MoveItem(int from, int to);
    void SwapItems(int a, int b);
    void Sort(bool ascending = true);                 // 使用 SetSortComparator
    void SortItems(std::function<bool(const std::wstring&, const std::wstring&)>);
    void ScrollToItem(int index);
    void SetEmptyText(const std::wstring&);

    // 选择
    void SetSelectionMode(SelectionMode);
    SelectionMode GetSelectionMode() const;
    void SetSelectedIndex(int); int GetSelectedIndex() const;
    std::wstring GetSelectedText() const;
    std::vector<int> GetSelectedIndices() const;
    std::vector<std::wstring> GetSelectedTexts() const;
    void SelectAll();

    // 右键：最近一次右键所在行（配 SetContextMenuFactory 用；-1=没点到有效行）
    int GetContextRow() const;

    // 勾选
    void SetCheckable(bool);
    void SetItemChecked(int, bool);
    bool IsItemChecked(int) const;
    std::vector<bool> GetSelectionStates() const;
    std::vector<bool> GetCheckStates() const;
    std::vector<int> GetCheckedIndices() const;

    // 每项附加信息（重点）
    void SetItemDisabled(int index, bool disabled = true);
    bool IsItemDisabled(int index) const;
    void SetItemTextColor(int index, Color);
    void ClearItemTextColor(int index);
    void SetItemToolTip(int index, const std::wstring&);
    std::wstring GetItemToolTip(int index) const;

    // 排序指示
    void SetSortComparator(std::function<bool(const std::wstring&, const std::wstring&)>);
    bool IsSortAscending() const;
    void SetShowSortIndicator(bool);
    void SetSortIndicatorColor(Color);

    // 样式：SetItemHeight / SetSelectedColor / SetHoverColor / SetAlternatingRowColors /
    //       SetAlternatingRowColor / SetMarqueeEnabled / SetButtonMode / SetButtonSpacing 等
};
```

**要点**：

- **每项附加信息按“项目本身”绑定**：`SetItemDisabled/SetItemTextColor/SetItemToolTip` 在内部与 `Label` 项目对象绑定，因此 `Sort()` / `MoveItem()` / `SwapItems()` 之后，这些信息仍然跟随各自的项目，不会错位。
- 选择模式：`Single`（单选）、`Extended`（Ctrl 点选 / Shift 连选）、`Multi`（点击即切换）、`None`（不可选择，仍可触发 `ItemClicked`）。
- **键盘**：`↑/↓/Home/End/PageUp/PageDown` 会**跳过禁用项**；获得焦点的列表还支持**首字母定位**（连续键入字母快速定位到以该前缀开头的项）。
- `SetCheckable(true)` 后每行出现复选框，`ItemCheckStateChanged(index, checked)` 通知变化。
- 排序：设置比较器后调用 `Sort(asc)`（或 `SortItems`）；`SetShowSortIndicator(true)` 时右上角显示排序箭头。
- 悬停带 `SetItemToolTip` 的项时使用框架统一 ToolTip。
- **右键**：`ItemRightClicked(row)` 传点击行号；也可在 `SetContextMenuFactory([...]{ int row = lv->GetContextRow(); ... })` 里现搭菜单。

## TableView

```cpp
enum class SelectionMode { Cell, Row, Column, None };

class TableView : public UIElement {
    ZSignal<int,int> CellClicked;
    ZSignal<int,int> CellDoubleClicked;
    ZSignal<int> HeaderClicked;
    ZSignal<int,int> CurrentCellChanged;
    ZSignal<std::vector<std::pair<int,int>>> SelectionChangedCells;
    ZSignal<int,bool> ItemCheckStateChanged;
    ZSignal<int,int> CellRightClicked;   // 右键点击单元格（传行、列）

    TableView();
    // 行列
    void SetRowCount(int rows); int GetRowCount() const;
    void SetColumnCount(int cols); int GetColumnCount() const;
    void AppendRow(); void InsertRow(int index); void RemoveRow(int index);
    void AppendColumn(); void InsertColumn(int index); void RemoveColumn(int index);

    // 单元格
    void SetItem(int row, int col, const std::wstring& text);
    void SetItem(int row, int col, std::shared_ptr<Label> label);
    std::wstring GetItemText(int row, int col) const;
    std::shared_ptr<Label> GetItemLabel(int row, int col) const;

    // 表头 / 尺寸
    void SetHeaderLabel(int col, const std::wstring&);
    std::wstring GetHeaderLabel(int col) const;
    void SetHorizontalHeaderLabels(const std::vector<std::wstring>&);
    void SetColumnWidth(int col, float); float GetColumnWidth(int col) const;
    void SetRowHeight(float);            // 默认行高
    void SetRowHeightAt(int row, float); // 单行高度
    float GetRowHeightAt(int row) const;
    void ClearRowHeightAt(int row);
    void SetHeaderHeight(float);

    // 显示
    void SetHeaderVisible(bool);
    void SetGridVisible(bool);
    void SetAlternatingRowColors(bool);
    void SetAlternatingRowColor(Color);
    void SetColumnVisible(int col, bool visible);   // 列隐藏
    bool IsColumnVisible(int col) const;
    void SetColumnAlignment(int col, TextHAlign);   // 列对齐（Left/Center/Right）
    TextHAlign GetColumnAlignment(int col) const;

    // 选择
    void SetSelectionMode(SelectionMode);
    SelectionMode GetSelectionMode() const;
    void SetCurrentCell(int row, int col);
    int GetCurrentRow() const; int GetCurrentColumn() const;
    void SelectRow(int row); void SelectColumn(int col); void ClearSelection();
    void ScrollToCell(int row, int col);
    std::vector<std::pair<int,int>> GetSelectedCells() const;
    std::vector<int> GetSelectedRows() const;
    std::vector<int> GetSelectedColumns() const;
    std::vector<bool> GetSelectionStates() const;

    // 右键：最近一次右键所在行/列（配 SetContextMenuFactory 用；-1=没点到有效单元格）
    int GetContextRow() const;
    int GetContextColumn() const;

    // 勾选 / 每行禁用
    void SetCheckable(bool);
    void SetRowChecked(int row, bool); bool IsRowChecked(int row) const;
    std::vector<bool> GetRowCheckStates() const;
    std::vector<int> GetCheckedRows() const;
    void SetRowDisabled(int row, bool disabled = true);
    bool IsRowDisabled(int row) const;

    // 单元格颜色 / ToolTip
    void SetCellTextColor(int row, int col, Color);
    void ClearCellTextColor(int row, int col);
    void SetCellToolTip(int row, int col, const std::wstring&);

    // 排序
    void SetColumnComparator(int col, std::function<bool(const std::wstring&, const std::wstring&)>);
    void SortByColumn(int col, bool ascending = true);
    int GetSortColumn() const; bool IsSortAscending() const;
    void SetShowSortIndicator(bool);
    void SetSortIndicatorColor(Color);

    // 框选 / 样式：SetMarqueeEnabled / SetMarqueeCheckSync / SetBackgroundColor /
    //   SetHeaderBackgroundColor / SetTextColor / SetGridLineColor / SetScrollBarColors 等
};
```

**要点**：

- 默认尺寸 `400×300`，表头高 `26`、默认行高 `24`、最小列宽 `40`，选择模式默认 `Cell`。
- **单元格颜色 / ToolTip / 每行禁用 / 每行高度**等行级元数据在 `SortByColumn` 以及插入/删除行、插入/删除列时会**随行列一起重映射**，排序后仍然跟随原来的行/列（不会错位）。
- `SetColumnVisible(false)` 隐藏列：该列不参与布局、绘制与命中测试。
- `SetColumnAlignment` 用 `TextHAlign::{Left,Center,Right}`。
- `SetRowHeightAt(row, h)` 只改某一行；`SetRowHeight` 改默认行高。行高变化会影响滚动与命中测试。
- 键盘：`↑/↓` 跳过禁用行；`←/→`、`Home/End` 移动当前单元格。
- `SelectionMode::None` 下不产生选择，但仍触发 `CellClicked`。
- **右键**：`CellRightClicked(row, col)` 传点击的行列；也可在 `SetContextMenuFactory` 里用 `GetContextRow()/GetContextColumn()` 读。
- **注意**：不同版本曾缺少 `GetRowCount/GetColumnCount`，当前已提供。

## TreeNode

```cpp
struct TreeNode {
    std::vector<std::shared_ptr<Label>> columns;       // Label 化：每列一个真 Label（第 0 列为节点文本）
    TreeNode* parent = nullptr;                        // 非拥有
    std::vector<std::shared_ptr<TreeNode>> children;
    bool expanded = false;
    int depth = 0;
    void* userData = nullptr;

    std::wstring icon;                                 // 第一列前置图标（一个字符/emoji，可空）
    bool checkable = false;
    CheckState checkState = CheckState::Unchecked;     // 三态
    bool selected = false;
    bool enabled = true;
    std::wstring tooltip;                              // 悬停提示（接入框架统一 ToolTip）

    TreeNode(const std::wstring& text);
    TreeNode(const std::vector<std::wstring>& cols);
};
```

- `parent` 是**非拥有**裸指针（由 `children` 的所有权决定生命周期）；`depth` 由框架维护。
- `icon` 是绘制在第一列文本前的单个字符/emoji；`tooltip` 交给基础类统一显示。
- `columns` 是**真 Label**（Label 化）：读文本用 `node->columns[c]->GetText()`；也可直接对该单元格 Label 设字体/颜色/内嵌内容。位置与裁剪由 TreeView 的 `GetChildren` 统一接管（第一列会在缩进、展开箭头、选中指示条、勾选框、图标之后起排）。

## TreeView

```cpp
enum class SelectionMode { Single, Extended, Multi };
enum class CheckMode { Linked, Independent };          // 勾选父子联动/独立

class TreeView : public UIElement {
    ZSignal<std::shared_ptr<TreeNode>> SelectionChanged;
    ZSignal<std::shared_ptr<TreeNode>> NodeClicked;
    ZSignal<std::shared_ptr<TreeNode>> ItemDoubleClicked;
    ZSignal<std::shared_ptr<TreeNode>> ItemRightClicked;
    ZSignal<int> HeaderClicked;
    ZSignal<std::vector<std::shared_ptr<TreeNode>>> SelectionChangedMulti;
    ZSignal<std::shared_ptr<TreeNode>, bool> ExpandChanged;
    ZSignal<std::shared_ptr<TreeNode>, TreeNode::CheckState> ItemCheckStateChanged;

    TreeView();
    // 列 / 表头
    void SetColumnCount(int count);
    void SetHeaderLabels(const std::vector<std::wstring>&);
    void SetColumnWidth(int col, float); float GetColumnWidth(int col) const;
    void SetHeaderVisible(bool);

    // 结构
    std::shared_ptr<TreeNode> AddRoot(const std::wstring&);
    std::shared_ptr<TreeNode> AddRoot(const std::vector<std::wstring>&);
    std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode>, const std::wstring&);
    std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode>, const std::vector<std::wstring>&);
    std::shared_ptr<TreeNode> InsertRoot(int index, const std::vector<std::wstring>&);
    std::shared_ptr<TreeNode> InsertChild(std::shared_ptr<TreeNode>, int index, const std::vector<std::wstring>&);
    void RemoveNode(std::shared_ptr<TreeNode>);
    void RemoveChildren(std::shared_ptr<TreeNode>);
    void MoveNode(std::shared_ptr<TreeNode>, std::shared_ptr<TreeNode> newParent);
    void SortChildren(std::shared_ptr<TreeNode> parent, bool recursive,
                      std::function<bool(const std::shared_ptr<TreeNode>&, const std::shared_ptr<TreeNode>&)>);
    void Clear();

    // 展开
    void ExpandNode(std::shared_ptr<TreeNode>, bool);
    void ToggleNode(std::shared_ptr<TreeNode>);
    void ExpandNodeRecursive(std::shared_ptr<TreeNode>, bool);
    bool IsExpanded(std::shared_ptr<TreeNode>) const;
    void ExpandAll(); void CollapseAll();
    void ExpandToDepth(int depth);
    void SetDefaultExpandDepth(int depth);   // 之后插入的节点自动展开到该深度
    int GetDefaultExpandDepth() const;

    // 过滤 / 搜索
    void SetFilter(std::function<bool(const std::shared_ptr<TreeNode>&)>);
    void ClearFilter();
    bool HasFilter() const;
    void Search(const std::wstring& keyword);
    std::wstring GetNodePath(const std::shared_ptr<TreeNode>&, const std::wstring& separator = L" / ") const;

    // 选择 / 勾选
    void SetSelectionMode(SelectionMode);
    void SetSelectedNode(std::shared_ptr<TreeNode>);
    std::shared_ptr<TreeNode> GetSelectedNode() const;
    std::vector<std::shared_ptr<TreeNode>> GetSelectedNodes() const;
    void SelectAll();
    void SetCheckable(bool enable, bool recursive = true);
    void SetCheckMode(CheckMode);
    CheckMode GetCheckMode() const;
    std::vector<std::shared_ptr<TreeNode>> GetCheckedNodes() const;
    std::vector<bool> GetSelectionStates() const;
    std::vector<TreeNode::CheckState> GetCheckStates() const;

    // 尺寸 / 外观
    void SetRowHeight(float);
    void SetIndent(float);
    void SetAlternatingRowColors(bool);
    void SetGridVisible(bool);
    // 框选：SetMarqueeEnabled / SetMarqueeCheckSync
};
```

**要点**：

- 默认尺寸 `400×300`，行高 `24`、缩进 `16`、表头高 `24`；默认一列、列宽 `40`、表头 `名称`。
- 三态勾选：`CheckMode::Linked` 时父子勾选联动并自动计算 `PartiallyChecked`；`Independent` 时各自独立。
- `Search(keyword)` 是 `SetFilter` 的便捷封装：保留“命中节点 + 其祖先链”，其余隐藏。
- `GetNodePath(node)` 返回从根到该节点、以第一列文本拼接的路径（可自定义分隔符）。
- `SetDefaultExpandDepth` 只影响**之后插入**的节点；对已存在节点用 `ExpandToDepth`。
- `SortChildren` 只对子节点排序，`recursive=true` 时递归排序整棵子树。
- 节点的 `tooltip` 通过基础类统一 ToolTip 显示；`enabled=false` 的节点置灰且不可选。

###chapter: 图像 | Image、ImageManager、ImageDeviceCache

图像系统在 `ZufyUIImages.h`，用 WIC 解码、Direct2D（GPU）绘制与变换。

## ImageManager / ImageDeviceCache

```cpp
class ImageManager {
    static ImageManager& Instance();
    IWICImagingFactory* Factory();                        // 共享的 WIC 工厂
    void RegisterCache(const std::shared_ptr<ImageDeviceCache>&);
    void ClearAllDeviceCaches();                          // 设备丢失/重建时清理所有图像的 D2D 位图缓存
};

class ImageDeviceCache {                                  // 某个 Image 在“每个渲染目标”上的 ID2D1Bitmap 缓存
    ID2D1Bitmap* Get(ID2D1RenderTarget*, IWICBitmapSource*);
    void Clear();
    void Clear(ID2D1RenderTarget*);
};
```

- 解码结果（`IWICBitmapSource`，32bppPBGRA）是**设备无关**的，每个 `Image` 一份；`ImageDeviceCache` 按渲染目标缓存一张 `ID2D1Bitmap`（D2D 位图属于创建它的渲染目标）。
- 设备丢失时由框架调用 `ClearAllDeviceCaches()` 统一重建。

## Image

```cpp
class Image {
    enum class Format { Png, Jpeg, Bmp, Gif, Tiff };
    enum class Interpolation { Nearest, Linear };
    struct DrawOptions { float opacity = 1.0f; Interpolation interpolation = Interpolation::Linear; };

    bool IsNull() const;
    int Width() const; int Height() const;
    Rect Bounds() const;
    bool HasAlpha() const;

    // 加载
    static std::shared_ptr<Image> FromFile(const std::wstring& path);
    static std::shared_ptr<Image> FromMemory(const void* data, size_t size);
    static std::shared_ptr<Image> FromBase64(const std::string& base64);            // 支持 data: URI 前缀与 URL-safe
    static std::shared_ptr<Image> FromResource(HMODULE mod, const wchar_t* name, const wchar_t* type);
    static std::shared_ptr<Image> FromResource(int id, const wchar_t* type);        // 当前模块
    static std::shared_ptr<Image> FromHBITMAP(HBITMAP);
    static std::shared_ptr<Image> FromHICON(HICON);
    HICON ToHICON() const;                              // 转回系统 HICON（新建对象，用完 DestroyIcon）
    bool CopyPixelsBgra(std::vector<uint8_t>& out, int& w, int& h) const;  // 导出 32bpp BGRA（直通 alpha）

    // 变换（轻量描述符，绘制时 GPU 施加，共享同一份解码数据）
    std::shared_ptr<Image> Scaled(float w, float h) const;
    std::shared_ptr<Image> ScaledToWidth(float w) const;
    std::shared_ptr<Image> ScaledToHeight(float h) const;
    std::shared_ptr<Image> Rotated(float degrees) const;
    std::shared_ptr<Image> Mirrored(bool horizontal = true, bool vertical = false) const;
    std::shared_ptr<Image> Cropped(const Rect& srcRect) const;

    // 绘制（GPU）
    void Draw(ID2D1RenderTarget* rt, const Rect& dst, const DrawOptions& = {}) const;
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& = {}) const;
    void Draw(ID2D1RenderTarget* rt, float x, float y, const DrawOptions& = {}) const;
    ComPtr<ID2D1Bitmap> Bake(ID2D1RenderTarget* rt) const;

    // 编码 / 保存
    bool Save(const std::wstring& path, Format = Format::Png, float quality = 0.9f) const;
    std::vector<uint8_t> Encode(Format = Format::Png, float quality = 0.9f) const;
};
```

**要点与易混点**：

- **解码一次、共享**：`Scaled/Rotated/Mirrored/Cropped` 返回的是共享同一份解码数据的新 `Image`（轻量描述符），变换在 `Draw` 时由 GPU 施加，不重复解码，也不做 CPU 逐像素处理。
- **设备位图缓存**：同一 `Image` 在同一窗口反复绘制只上传一次 GPU 位图；多窗口各自一张。
- **元数据缓存（根源）**：内存型输入（`FromMemory/FromBase64/FromResource(RT_BITMAP)`）用的 `IWICStream::InitializeFromMemory` **不复制**缓冲区，而帧解码 / 格式转换是**惰性**的；若不在解码时立即取像素，绘制阶段会读到已释放内存（表现为图像完全不显示）。实现里用 `WICBitmapCacheOnLoad` 在 `detail_img_MakeFromSource` 内**立即把像素拷进独立 WIC 位图**，与来源内存彻底解耦。
- **资源**：`FromResource` 的 `type` 可传 `L"PNG"`/`L"IMAGE"`/`RT_RCDATA`/`RT_BITMAP` 等；`RT_BITMAP`（DIB）会自动补 BMP 文件头再解码；`HMODULE` 版本可用于**从 DLL 资源**加载。
- **编码**：`Encode` 用 `CreateStreamOnHGlobal` 得到可增长内存流（**无临时文件**），返回编码字节；`Save` 直接写文件。
- **配合 `Label`**：`Label::SetImage` + `SetIconSize` 即可显示图标（见“基础控件 → Label”）。
- **与系统图标互转**：`ToHICON()` 得到系统 `HICON`（用于 `SetAppIcon` / `TrayIcon` 等）；`CopyPixelsBgra()` 导出 32bpp BGRA 直通像素（用于写 `.ico` 等）。
- **变换矩阵（根源）**：`Rotated/Mirrored` 以目标矩形中心为轴。D2D 矩阵乘法是“左侧先应用”，必须用带 `center` 的 `Rotation/Scale` 重载，否则会把图像推出目标矩形（表现为旋转/镜像后什么都看不到）。

###chapter: 窗口工具 | TitleBar / MessageBox / 托盘 / 任务栏 / 图标与激活

窗口工具控件在 `ZufyUIWindowTool.h`（窗口级控件集合，后续还会放内置 MessageBox 等）。

```cpp
class TitleBar : public UIElement {
    void SetTitle(const std::wstring&);
    const std::wstring& GetTitle() const;
    void SetIcon(std::shared_ptr<Image>);        // 复用图像系统
    void SetIconSize(float w, float h);
    void SetShowIcon(bool);
    void SetShowTitle(bool);
    void SetContentPadding(float left, float right = 8.0f);
    void SetBackgroundColor(Color);  void SetActiveBackgroundColor(Color);
    void SetTitleColor(Color);       void SetActiveTitleColor(Color);
    void SetButtonWidth(float);      // 也可 SetButtonHeight / SetRightMargin
    void SetButtonHeight(float);
    void SetRightMargin(float);
    void ClearRightMargin();         // 恢复默认（右上角齐平，默认右边距 1px）
    void SetButtonsEnabled(bool);    // 三件套是否可点击（总开关）
    bool AreButtonsEnabled() const;
    // 按按钮类型单独控制（详细禁用 API）
    std::shared_ptr<CaptionButton> GetButton(CaptionButton::Kind) const;
    void SetButtonEnabled(CaptionButton::Kind, bool);   // 置灰、不响应、不报告系统按钮码
    bool IsButtonEnabled(CaptionButton::Kind) const;
    void SetButtonVisible(CaptionButton::Kind, bool);   // 隐藏后不占位
    bool IsButtonVisible(CaptionButton::Kind) const;
};

class CaptionButton : public UIElement {
    enum class Kind { Minimize, MaximizeRestore, Close };
    explicit CaptionButton(Kind kind);
    ZSignal<> Clicked;                // 点击信号；默认行为由 DefaultTitleBar 接线
    void SetHoverColor(Color);        void SetPressedColor(Color);
    void SetCloseHoverColor(Color);   void SetClosePressedColor(Color);
    void SetGlyphColor(Color);
    void SetButtonWidth(float);
    void SetAnimationSpeed(float);
};

class DefaultTitleBar : public TitleBar {
    DefaultTitleBar();                // 内置 最小化 / 最大化还原 / 关闭
    std::shared_ptr<CaptionButton> GetMinButton() const;
    std::shared_ptr<CaptionButton> GetMaxButton() const;
    std::shared_ptr<CaptionButton> GetCloseButton() const;
    // 便捷：单独启用/禁用、显示/隐藏三件套
    void SetMinimizeEnabled(bool);  void SetMaximizeEnabled(bool);  void SetCloseEnabled(bool);
    void SetMinimizeVisible(bool);  void SetMaximizeVisible(bool);  void SetCloseVisible(bool);
};
```

**用法**：

```cpp
auto bar = std::make_shared<DefaultTitleBar>();
bar->SetTitle(L"我的窗口");
bar->SetIcon(myImage);                 // 可选
win.SetCustomTitleBar(bar);            // 安装；win.SetCustomTitleBar(nullptr) 恢复原生
```

**要点与易混点**：

- 标题栏**不参与布局**（由 Window 放到 `(0,0)`），默认 `DrawAfterLayout`；**不参与 Tab 焦点**（`IsFocusable()==false`）；`UseCache()==true`，仅状态变化重绘。
- 按钮用系统图标字体 `Segoe Fluent Icons`（Win11）/ `Segoe MDL2 Assets`（Win10）绘制，字形与系统一致（最小化 `E921`、最大化 `E922`、还原 `E923`、关闭 `E8BB`）；找不到图标字体时用矢量兜底。
- 悬停/按下带**渐变**；最小化/最大化悬停浅灰，**关闭悬停红**（`#C42B1C`）。
- 按钮在 `WM_NCHITTEST` 里被报告为系统按钮码（`HTMINBUTTON` / `HTMAXBUTTON` / `HTCLOSE`），因此**最大化按钮悬浮会出现系统 Snap Layouts**；点击动作在 `WM_NCLBUTTONUP` 处理。
- 标题栏自身矩形（按钮以外）是窗口的**拖动区域**，拖动 / Aero Snap / 双击最大化都交给系统。
- **不建议**把标题栏手动 `AddChild` 进布局（它不参与布局，由 `Window::SetCustomTitleBar` 统一管理）；文档不禁止，但行为由使用者自负。

## MessageBox / FastButton

预设消息框，复用 `Window` / `DefaultTitleBar` / `Window::RunModal`。本体就是 `Window`，可直接往里加控件。

```cpp
enum class FastButton : unsigned {   // 位标志，可 | 组合
    None = 0, OK = 1 << 0, Cancel = 1 << 1, Apply = 1 << 2,
    Close = 1 << 3, Yes = 1 << 4, No = 1 << 5, Help = 1 << 6
};
FastButton operator|(FastButton, FastButton);
FastButton operator&(FastButton, FastButton);
bool HasFlag(FastButton set, FastButton flag);      // 按位判定

class MessageBox : public Window {
    enum class Icon { None, Info, Warning, Error, Question };
    enum class Lang { English, Chinese };

    // 快速调用：构造即建窗；blocking=true 时构造内直接跑完模态循环
    MessageBox(Window* parent, const std::wstring& title, const std::wstring& content,
               Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true);
    MessageBox(HWND parent, const std::wstring& title, const std::wstring& content, ...);
    MessageBox(const std::wstring& title, const std::wstring& content, ...);   // 无父
    // content 也可直接传控件（Label / TextBox / GridLayout …）
    MessageBox(Window* parent, const std::wstring& title, std::shared_ptr<UIElement> content, ...);

    static void SetLanguage(Lang);                          // 按钮文本预置中英，默认 English
    static Lang GetLanguage();
    static std::wstring ButtonLabel(FastButton);
    void SetButtonText(FastButton, const std::wstring&);    // 单独改某个按钮文本
    void SetIconImage(std::shared_ptr<Image>);              // 自定义图标（默认内置系统图标）
    void SetUserContent(std::shared_ptr<UIElement>);        // 调用后不放任何按钮，全交给用户
    void EndDialog(FastButton);                             // 自定义内容时用户主动结束

    FastButton GetResult() const;
    ZSignal<FastButton> ButtonClicked;
};
```

```cpp
ZufyUI::MessageBox box(owner, L"标题", L"正文", MessageBox::Icon::Info,
                    FastButton::Yes | FastButton::No | FastButton::Cancel);
if (HasFlag(box.GetResult(), FastButton::Yes)) { /* ... */ }
```

- 按钮显示顺序（左→右）：`Yes No OK Apply Cancel Close Help`；**Enter = 最左侧按钮，ESC = Cancel/Close（没有则最左）**；按钮整体靠右。
- `parent` 可为 `Window*` / `HWND` / 省略（不继承、无父）；是 owned 窗口（不进任务栏）、不可调整大小、只留关闭按钮。
- 按内容自适应高度；文本被截断的按钮**悬停会自动浮出完整文本**（`SetToolTip` 设置过的优先）。
- 自定义：`SetUserContent`（之后没有预设按钮）或直接用带 `shared_ptr<UIElement>` 的构造；需要自己控制流程时用 `blocking=false` 建窗后自行 `RunModal`。
- 注意：`windows.h` 里 `MessageBox` 是 `MessageBoxW` 的宏，本库在 `ZufyUIWindowTool.h` 中 `#undef` 掉它；要用 Win32 的请显式写 `MessageBoxW/A`。

## 窗口图标 / 激活 / 闪烁

```cpp
class Window {
    // 统一设置应用图标（原生大/小图标 + 类图标 + 自定义标题栏）
    void SetAppIcon(HICON bigIcon, HICON smallIcon);
    void SetAppIcon(std::shared_ptr<Image> big, std::shared_ptr<Image> small = nullptr);  // 自动转 HICON
    void SetAppIconFromResource(int bigId, int smallId = 0);

    enum class ActivateMode { Raise, Activate, Foreground, All };
    void ShowActivate(ActivateMode mode = ActivateMode::All);   // 最小化则还原；Foreground/All 调 SetForegroundWindow
    void Raise();                                               // 仅提升 Z 序

    void Flash(int times = 5, bool alsoTaskbar = true);         // 默认 FLASHW_ALL
    void FlashUntilForeground();
    void StopFlash();
};
```

- `SetAppIcon(Image)` 走 `Image::ToHICON()`；设置后任务栏 / Alt-Tab / 标题栏图标一致（另含类图标）。
- `ShowActivate(All)` = 还原（若最小化）+ 置顶 + `SetForegroundWindow`（内部 `AttachThreadInput` 提高成功率）。
- `Flash()` 默认 `FLASHW_ALL`（任务栏按钮 + 窗口标题）。

## 托盘图标（TrayIcon）

`Shell_NotifyIcon` 封装（`NOTIFYICON_VERSION_4`）：悬停 / 点击 / 右键菜单 / 徽章 / 气泡通知。

```cpp
class TrayIcon {
    bool Add(HICON icon, const std::wstring& tooltip, UINT id = 1);
    bool Add(std::shared_ptr<Image> icon, const std::wstring& tooltip, UINT id = 1);   // 自动转
    bool AddFromResource(int resId, const std::wstring& tooltip, UINT id = 1);
    bool AddFromFile(const std::wstring& icoPath, const std::wstring& tooltip, UINT id = 1);
    void Remove();
    bool IsAdded() const;

    void SetIcon(HICON);   void SetIcon(std::shared_ptr<Image>);
    void SetToolTip(const std::wstring&);
    void SetMenu(std::shared_ptr<Menu>);

    void SetBadge(Color color = Color::FromArgb(255, 220, 40, 40));   // 右下角小红点
    void ClearBadge();

    enum class BalloonIcon : DWORD { None, Info, Warning, Error, Custom };
    void ShowBalloon(const std::wstring& title, const std::wstring& text,
                     BalloonIcon icon = BalloonIcon::Custom,
                     HICON customIcon = nullptr, bool realtime = false, bool noSound = false);

    bool GetRect(RECT& out) const;
    HWND GetHwnd() const;

    ZSignal<> Clicked; ZSignal<> DoubleClicked; ZSignal<> RightClicked;
    ZSignal<> HoverEnter; ZSignal<> HoverLeave; ZSignal<> Selected;
    ZSignal<> BalloonClicked; ZSignal<> BalloonDismissed; ZSignal<> BalloonTimeout;
};
```

**要点**：

- v4 回调：右键以 `WM_CONTEXTMENU` 到达；悬停 `NIN_POPUPOPEN/CLOSE`；双击在 `NIN_SELECT` 里按 `GetDoubleClickTime()` 判定。
- 重复 `Add` 同 ID 自动 `NIM_MODIFY`；`explorer` 重启后自动重加（`TaskbarCreated`）。
- `BalloonIcon`：`None/Info/Warning/Error`（系统图标）/`Custom`（用 `customIcon`，缺省=当前托盘图标）；`realtime=true`（`NIF_REALTIME`）；`noSound=true` 静音。
- ⚠️ Win10/11 把气球升级成 toast 时，**左上角“应用图标”由系统按 AUMID 查注册表**（`hBalloonIcon` 被忽略）→ 用 `RegisterApp` 登记应用身份即可显示应用名与图标。

## 任务栏（进度 / 覆盖徽章 / 缩略图工具栏 / 跳转列表）

```cpp
class Window {
    enum class TaskbarProgress { None = 0, Indeterminate = 1, Normal = 2, Error = 4, Paused = 8 };
    void SetTaskbarProgress(TaskbarProgress state, ULONGLONG completed = 0, ULONGLONG total = 0);
    void SetTaskbarProgressValue(ULONGLONG completed, ULONGLONG total);
    void ClearTaskbarProgress();

    void SetTaskbarOverlayIcon(HICON, const std::wstring& description = L"");
    void SetTaskbarOverlayIcon(std::shared_ptr<Image>, const std::wstring& description = L"");
    void ClearTaskbarOverlayIcon();

    // 缩略图工具栏（缩略图悬停时的一排按钮）；ThumbButtonId 为命令 id
    ZSignal<ThumbButtonId> ThumbButtonClicked;
    void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons, HIMAGELIST images);
    void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons,
                         const std::vector<std::shared_ptr<Image>>& images);   // 自动建 HIMAGELIST
    void UpdateThumbButton(ThumbButtonId id, bool enabled);

    // 跳转列表
    struct JumpListItem { std::wstring title, arguments, target, iconPath; int iconIndex = 0; };
    void SetJumpList(const std::vector<JumpListItem>& tasks = {},
                     const std::vector<std::pair<std::wstring, std::vector<JumpListItem>>>& categories = {},
                     bool includeRecent = false);
    void SetAppUserModelID(const std::wstring&);                 // 本窗口 AUMID
    static void SetProcessAppUserModelID(const std::wstring&);   // 进程级
};
```

**要点**：

- 任务栏能力依赖**进程/窗口 AUMID**；建议用 `RegisterApp` 统一登记（跳转列表 / 分组 / taskbar 都归属该身份）。
- `SetThumbButtons` 的 `Image` 版会自动 `Image → HICON → HIMAGELIST`；点击经 `ThumbButtonClicked(id)` 通知。
- `SetJumpList` 支持 `tasks`（用户任务）与 `categories`（自定义分类）+ `includeRecent`（系统“最近”）。

## 窗口级钩子（可重写）

```cpp
virtual bool OnWindowKeyDown(int vk);   // 在派发给焦点元素前调用，返回 true 表示已处理
virtual bool OnWindowTimer(int id);     // WM_TIMER
virtual void OnWindowSize();            // WM_SIZE（依赖客户区宽度的收尾布局）
void SetInputBlocked(bool on);          // 屏蔽本窗口鼠标/键盘输入
```

###chapter: 附录 | 信号一览、默认值速查与常见坑

## 信号一览（更新后）

| 类 | 信号 | 参数 |
| --- | --- | --- |
| `Button` | `Clicked` / `Toggled` | — / `bool` |
| `CheckBox` | `Toggled` / `StateChanged` | `bool` / `State` |
| `TextBox` | `TextChanged` / `ReturnPressed` | `const std::wstring&` / — |
| `ComboBox` | `SelectionChanged` / `DropDownOpened` / `DropDownClosed` | `int` / — / — |
| `ToggleSwitch` | `Toggled` | `bool` |
| `ProgressBar` | `ValueChanged` | `float` |
| `Slider` | `ValueChanged` / `SliderReleased` | `float` / — |
| `ScrollViewer` | `ScrollChanged` | `float, float` |
| `ListView` | `SelectionChanged` / `ItemClicked` / `ItemDoubleClicked` / `SelectionChangedMulti` / `ItemCheckStateChanged` / `ItemRightClicked` | `int` / `int` / `int` / `std::vector<int>` / `int,bool` / `int` |
| `TableView` | `CellClicked` / `CellDoubleClicked` / `HeaderClicked` / `CurrentCellChanged` / `SelectionChangedCells` / `ItemCheckStateChanged` / `CellRightClicked` | `int,int` / `int,int` / `int` / `int,int` / `vector<pair<int,int>>` / `int,bool` / `int,int` |
| `TrayIcon` | `Clicked` / `DoubleClicked` / `RightClicked` / `HoverEnter` / `HoverLeave` / `Selected` / `BalloonClicked` / `BalloonDismissed` / `BalloonTimeout` | — |
| `Menu` | `ItemSelected` | `int` |
| `TreeView` | `SelectionChanged` / `NodeClicked` / `ItemDoubleClicked` / `ItemRightClicked` / `HeaderClicked` / `SelectionChangedMulti` / `ExpandChanged` / `ItemCheckStateChanged` | 见上 |
| `FontManager` | `GlobalFontChanged` | — |
| `UIZSignals` | `DrawOverlay` / `GlobalMouseDown` / `WindowDeactivated` / `ElementCaptureRequest` / `ElementCaptureRelease` / `RepaintRequest` / `LayoutInvalidated` / `DeviceReset` | 见第 4 章 |

## 常用默认值速查

| 项 | 值 |
| --- | --- |
| 全局字体 | `Segoe UI` / `14.0f` |
| 窗口默认背景 | `Backdrop::None` / tint `0` |
| 根布局 margin / spacing | `20` / `10` |
| 帧时间上限 `deltaTime` | `0.033s` |
| 窗口定时器 | `16ms`，`timeBeginPeriod(1)` |
| 阴影默认 | 颜色 `(0,0,0.02,0.42)`，`blur 10`，`offset (0,3)` |
| ToolTip 悬停延迟 | `0.5s` |
| Label | 字号 `16`，`Ellipsis`，垂直居中 |
| Button | `120×36`，圆角 `4`，蓝底白字 |
| TextBox | `160×30`，字号 `14`，光标闪烁 `0.5s` |
| ComboBox | `160×30`，列表项高 `24` |
| ToggleSwitch | `50×24` |
| CheckBox | 尺寸 `16`，圆角 `4` |
| ScrollViewer | 滚动条宽 `8`，最小长度 `20`，滚轮步长 `30` |
| ProgressBar | `200×20` |
| Slider | `160×24`，范围 `[0,100]`，轨道高 `4`，滑块 `14` |
| ListView | `200×200`，行高 `28` |
| TableView | `400×300`，表头 `26`，默认行高 `24`，最小列宽 `40` |
| TreeView | `400×300`，行高 `24`，缩进 `16` |

## 常见坑（实战最容易踩）

1. **颜色分量是 `[0,1]`**：`Color(255,0,0)` 是错的；用 `Color(1,0,0)` 或 `FromArgb(255,255,0,0)`。
2. **信号里捕获了宿主的 `shared_ptr`**：形成引用环导致整棵子树泄漏。改捕获原始指针或 `weak_ptr`（见第 4 章）。
3. **改属性后忘记重绘**：框架提供的 setter 通常已 `RequestRepaint()`；若你直接改内部数据，要自己调。
4. **元素不可见后仍想更新其状态**：`SetVisible(false)` 会释放缓存，但布局仍保留；重新显示时会重建。
5. **禁用不等于隐藏**：`SetEnabled(false)` 仍占布局、仍显示（灰色）；隐藏用 `SetVisible(false)`。
6. **焦点环只在 Tab 时出现**：鼠标点击获得焦点不画环，这是刻意行为，不是 bug。
7. **`deltaTime` 已被钳制**：写动画时按“秒”计算即可，不用再自己防超大 `dt`。
8. **数据视图排序后附加信息错位**：当前版本已通过“按项目绑定 / 元数据重映射”修复；但如果你**自己重排**内部容器，需要自行维护这些信息。
9. **`PageHost::NavigateTo` 是异步的**：动画结束后 `GetCurrentIndex()` 才变化。
10. **`GetChildren()` 返回引用**：不要缓存它在手的同时去修改元素的子列表；遍历中调用是安全的。

## 完整示例

```cpp
#include "ZDataViewer.h"
using namespace ZufyUI;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Window win;
    if (!win.Create(900, 600, L"ZufyUI 示例")) return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    // 顶部：开关 + 滑块 + 进度条
    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto slider = std::make_shared<Slider>();
    slider->SetRange(0.0f, 1.0f);
    slider->SetStep(0.1f);
    slider->SetSnapToStep(true);
    auto bar = std::make_shared<ProgressBar>();
    bar->SetShowText(true);

    toggle->Connect(toggle->Toggled, [bar](bool on) { bar->SetIndeterminate(on); });
    slider->Connect(slider->ValueChanged, [bar](float v) { bar->SetValue(v); });

    row->AddChild(toggle);
    row->AddChild(slider);
    row->AddChild(bar);
    root->AddChild(row);

    // 中间：列表（含禁用项、颜色、提示、排序）
    auto list = std::make_shared<ListView>();
    list->SetFillHeight(true);
    list->SetSelectionMode(ListView::SelectionMode::Extended);
    list->SetCheckable(true);
    for (int i = 1; i <= 20; ++i)
        list->AddItem(L"项目 " + std::to_wstring(i));
    list->SetItemDisabled(2, true);                                  // 第 3 项禁用
    list->SetItemTextColor(3, Color::FromArgb(255, 200, 60, 60));    // 第 4 项红字
    list->SetItemToolTip(1, L"这是第 2 项的提示");
    list->SetSortComparator([](const std::wstring& a, const std::wstring& b) { return a < b; });
    list->SetShowSortIndicator(true);
    list->SetSelectedIndex(0);
    root->AddChild(list);

    win.Show();          // 1.8.0 起 Create 不再自动显示，由应用调用 Show()
    win.Run();
    return 0;
}
```
