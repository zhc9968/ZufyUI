# ZufyUI API Reference

> This document covers the entire public API of the ZufyUI framework: core types, signals/slots, fonts, elements, layout, windows, basic controls, and data views.
> Companion reading: [Project overview and build](../README.en.md).

> **How to read this**: this is not a list of signatures. Each class is described as "what it is → what the framework does internally → what you must watch out for". Behavior, side effects, defaults, and pitfalls are spelled out so that you can use the API **correctly without ever having read the source code**.

###chapter: Conventions | Namespace, units, lifetime, and defaults

## Namespace and headers

- Everything lives in `namespace ZufyUI`.
- Files: `ZufyUI.h` (core: types / signals / fonts / elements / layout / menus / window), `ZufyUIWidgets.h` (basic controls), `ZDataViewer.h` (data views). `ZufyUIWidgets.h` includes `ZufyUI.h`; `ZDataViewer.h` depends on both.
- All three are header-only; `#pragma comment(lib, ...)` links `d2d1 / dwrite / dwmapi / imm32 / winmm` automatically.

## Units

- All coordinates and sizes are **DIP** (device-independent pixels), not physical pixels.
- Before drawing, the framework uses the internal `Snap()` to snap coordinates to the physical-pixel grid and avoid half-pixel blur. You do **not** need to do DPI conversion yourself.
- Mouse-event coordinates are also DIP.

## Object model and lifetime

- Controls are owned through `std::shared_ptr<T>` and created with `std::make_shared<T>()`.
- Attach them to a parent with the layout's `AddChild()`; the parent keeps the child's `shared_ptr`, so **the child stays alive as long as the parent does**.
- Once detached (e.g. `RemoveChild` / `Clear`) and with no other `shared_ptr`, the element is freed.
- When an element is destroyed it automatically releases its offscreen cache and its connections (via an internal `ConnectionGroup`).

## Signals and events

- Bind events with `Connect(signal, slot)`; it returns a `Connection` that disconnects automatically when the host element is destroyed.
- A slot can be any callable (lambda / function pointer / `std::function`).
- **Important:** do not connect a signal of an object to a lambda that captures the object's own `shared_ptr` by value — this creates a reference cycle and leaks the whole subtree. See Chapter 4.

## Default-value system

- Every control has `inline static` `Default*` fields (e.g. `Button::DefaultSize`).
- Changing them via `static SetDefault*()` affects **only instances created afterwards**; existing instances are unaffected.
- Instance setters (e.g. `SetColors`) affect only that instance.

## Three cross-cutting concepts (apply to all controls)

- **Enabled/disabled**: `UIElement::SetEnabled(false)` makes an element and its subtree non-interactive (mouse/keyboard are intercepted). The control itself should look at `IsEffectivelyEnabled()` and grey itself out. Disabling does **not** change layout.
- **Focus**: only elements whose `IsFocusable()` returns true can hold keyboard focus. The window cycles focusable elements with Tab; the focus ring is drawn only when focus was obtained via Tab, not via a mouse click.
- **Offscreen cache**: see the "Cache mechanism" section in Chapter 6. Enabled by default; turn it off with `SetUseCache(false)`.

###chapter: Basic types | Color, Rect, Thickness, Size

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

- Components are **floats in `[0,1]`**, not 0–255. Mixing this up is the most common bug: `Color(255,0,0)` does not give red but an out-of-range value with unpredictable output. Use `Color(1,0,0)` or `FromArgb(255,255,0,0)`.
- `FromArgb` takes **`(a, r, g, b)`** — alpha first, unlike `Color(r,g,b,a)`. Watch the order.
- `ToD2D()` converts to `D2D1_COLOR_F`. Brushes are cached internally; after changing a color call `RequestRepaint()` (the control setters do this for you).
- `Lerp` interpolates each component; `t` is not clamped.

## Rect

```cpp
struct Rect {
    float x, y, width, height;
    Rect(float x = 0, float y = 0, float w = 0, float h = 0);
    bool Contains(float px, float py) const;
    D2D1_RECT_F ToD2D() const;
};
```

- Position + size (not left/top/right/bottom).
- `Contains` includes the boundary.
- `ToD2D()` converts to `D2D1_RECT_F`.

## Thickness

```cpp
struct Thickness {
    float left, top, right, bottom;
    Thickness(float l = 0, float t = 0, float r = 0, float b = 0);
};
```

- Used for `SetMargin()` and `ScrollViewer::SetContentMargin()`.
- There is no single-argument "all sides equal" constructor; write `Thickness(8,8,8,8)`.

## Size

```cpp
struct Size {
    float width, height;
    Size(float w = 0, float h = 0);
};
```

- Used as the argument to and return value of `Measure()`.

###chapter: Global functions and DPI | clamp, DPI scale, pixel snapping

```cpp
template<typename T> T clamp(T value, T low, T high);
inline float& GlobalDpiScaleRef();
inline void SetGlobalDpiScale(float scale);
inline float GetGlobalDpiScale();
inline float Snap(float dip);
```

| Function | Implementation notes |
| --- | --- |
| `clamp(v, low, high)` | Order is **`(value, low, high)`** (same as `std::clamp`), **not** `(value, high, low)`. Returns the boundary on overflow. |
| `SetGlobalDpiScale(s)` | Set automatically by `Window` to `dpi/96` before rendering. `s<=0` falls back to `1.0f`. Normally you never call it. |
| `GetGlobalDpiScale()` | Reads the current scale, default `1.0f`. |
| `Snap(dip)` | Snaps a DIP coordinate to the physical-pixel grid (`round(dip*scale)/scale`). The framework calls it before drawing. |

> **Why `Snap` exists**: Direct2D antialiases on non-integer pixels, making text and 1px lines look fuzzy. Snapping to physical pixels keeps them sharp.

###chapter: Signals and connections | ZSignal, Connection, ConnectionGroup, and global signals

## What this chapter solves

ZufyUI's signals are not just "connect and forget" — they also handle **automatic disconnection on object destruction**. Understand the three actors to avoid dangling callbacks and leaks:

- `ZSignal`: owns the slots (a list of `std::function`).
- `Connection`: the handle of one connection; disconnects on destruction.
- `ConnectionGroup`: a set of connections that are all disconnected together on destruction. Every `UIElement` owns one.

## ConnectionThread

```cpp
enum class ConnectionThread {
    CurrentThread,   // run on the firing thread (default)
    NewThread,       // spawn a detached thread per fire
    UIThread         // post to the UI thread message loop
};
```

- `CurrentThread`: synchronous; safe to mutate control state from the slot. Use this in almost all cases.
- `NewThread`: detaches a thread per fire; **do not touch the UI from the slot**.
- `UIThread`: posts to the UI thread; use it when a worker thread produces data and the UI must be updated.

## Connection

```cpp
class Connection {                  // Qt QMetaObject::Connection-style passive handle
    Connection();
    ~Connection();                  // note: destruction does NOT disconnect
    Connection(const Connection&) = default;   // copyable
    void disconnect();              // disconnect this one explicitly
    bool isConnected() const;
    explicit operator bool() const;
};
```

- **Passive handle**: destroying it does **not** disconnect, so ignoring the return value (`elem->Connect(sig, slot);`) is safe; to disconnect a single connection call `conn.disconnect()`.
- Auto-disconnect is handled by the element's `ConnectionGroup` (all connections are dropped when the element is destroyed).
- `isConnected()` / `operator bool` query whether it is still alive.

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    ConnectionGroup();
    ~ConnectionGroup();                    // disconnect all
    void disconnectAll();
    size_t size() const;
    void addState(const std::shared_ptr<detail::ConnectionState>& state);
};
```

- Every `UIElement` has one. Destroying a control destroys its group, which disconnects everything attached to the control.
- You can create your own group and pass it to `signal.connect(slot, ConnectionThread::CurrentThread, group)` for batch management.

## ZSignal

```cpp
template <typename... TArgs>
class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    void Fire(TArgs... targs) const;
    void operator()(TArgs... targs) const;   // sugar for Fire
};
```

**Implementation notes that affect usage:**

- Firing takes a snapshot of the current connections, so **connecting/disconnecting slots from within a slot is safe**.
- The snapshot reuses a thread-local buffer (no heap allocation when non-reentrant); reentrant firing falls back to a local copy.
- `Fire` is `const`.
- Thread-safe: the connection list is guarded by a mutex.

###block_green_start
**Recommended usage**: controls provide `UIElement::Connect(signal, slot)`, which attaches the connection to that control's `ConnectionGroup` so it disconnects automatically when the control is destroyed. When using `signal.connect(...)` manually, mind the connection's lifetime.
###block_green_end

###block_orange_start
**Note (signals and object lifetime)**: if a slot (lambda) passed to `Connect` **captures by value the `shared_ptr` of the object that owns the signal** (e.g. `obj->Connect(obj->SomeSignal, [obj](){...})`), it forms a reference cycle and the object and its whole subtree will never be released (`ConnectionGroup` cannot break it either, because it is owned by that object). Capture a **raw pointer** or a `weak_ptr` instead:

```cpp
auto b = btn.get();   // raw pointer; the signal is a member of btn, so the connection dies with btn
btn->Connect(btn->Clicked, [b]() { b->SetText(L"..."); });
```
###block_orange_end

## Global signals `UIZSignals`

| Signal | Args | Fired when | Notes |
| --- | --- | --- | --- |
| `DrawOverlay` | `ID2D1RenderTarget*` | after all UI is drawn | used for overlays such as open combo-box lists and tooltips |
| `GlobalMouseDown` | `float, float` | any mouse-down (DIP) | controls use it to close-on-outside-click |
| `WindowDeactivated` | — | window deactivated/minimized | collapse expanded state |
| `ElementCaptureRequest` | `UIElement*` | a control requests mouse capture | framework-internal |
| `ElementCaptureRelease` | `UIElement*` | a control releases capture | framework-internal |
| `RepaintRequest` | `UIElement*` | a control requests repaint | backs `RequestRepaint()` |
| `LayoutInvalidated` | — | global layout invalidation | backs `InvalidateLayout()` |
| `DeviceReset` | — | render device resources discarded/recreated (device loss, DPI change, window destroy) | subscribers should clear device/render-target-keyed caches (e.g. `ImageManager` bitmap caches) |

> These are global singleton signals, not members of a control. When connecting them, register the connection in a well-defined `ConnectionGroup` or disconnect it yourself at the right time.

###chapter: Fonts | FontSpec and FontManager

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

- A comparable value object, usable as a hash key (`FontSpecHash`), so it can live in an `unordered_map`.
- `size` is in points under DIP.
- `familyName` is a system font name (e.g. `Microsoft YaHei`, `Segoe UI`).

## FontManager (singleton)

| Method | Notes |
| --- | --- |
| `static FontManager& Instance()` | Meyers singleton, constructed on first use. |
| `IDWriteFactory* GetFactory()` | Lazily creates and shares `IDWriteFactory`. |
| `IDWriteTextFormat* GetFormat(const FontSpec&)` | Caches a shared `IDWriteTextFormat` per spec. Returns a **shared** object — do not `Release` or mutate it. |
| `void SetGlobalFont(const FontSpec&)` | Sets the global font and fires `GlobalFontChanged`. |
| `const FontSpec& GetGlobalFont() const` | Reads the global font. |
| `ZSignal<> GlobalFontChanged` | Every element without an instance font override subscribes to it and rebuilds its text layout. |

## Font resolution chain (important)

An element's effective font resolves as:

1. instance override via `SetFont(...)` / `SetFontFamily(...)` / `SetFontSize(...)` / `SetFontWeight(...)`;
2. type default (a control overriding `GetTypeDefaultFont()`, e.g. `Label` defaults to 16, `TextBox` to 14);
3. global default (`FontManager::GetGlobalFont()`).

`GetEffectiveFontSpec()` returns the resolved spec; `GetFontFormat()` turns it into a shared `IDWriteTextFormat` (with a per-instance fast cache).

- After calling an instance `SetFont*`, `HasFontOverride()` is true and the element **no longer** follows global font changes.
- `ClearFont()` clears the override and returns to the chain.

###chapter: Core element UIElement | Size, layout, drawing, events, and fonts

The base class of all visual elements. A custom control derives from it and implements the pure virtual `Measure` and `Draw`.

## Layout properties

| Method | Description |
| --- | --- |
| `SetMinWidth/SetMinHeight(float)` | Minimum size; measured size is never smaller |
| `SetMaxWidth/SetMaxHeight(float)` | Maximum size; default `FLT_MAX` |
| `SetMinSize(float w, float h)` / `SetMaxSize(...)` | Set both |
| `GetMinWidth/GetMinHeight/GetMaxWidth/GetMaxHeight()` | Read |
| `SetFillWidth(bool)` / `SetFillHeight(bool)` | Whether the element stretches to fill the parent axis |
| `SetWidth(float)` / `SetHeight(float)` | Fixed size (`0` means "use measured size") |
| `SetMargin(const Thickness&)` / `GetMargin()` | Outer margin |
| `SetStretchWeights(float h, float v)` / `SetHorizontalStretchWeight` / `SetVerticalStretchWeight` | Stretch weights |
| `InvalidateLayout()` / `IsLayoutDirty()` / `ClearLayoutDirty()` | Mark dirty; the layout pass re-measures/re-arranges |

**Pitfalls:** `SetWidth/SetHeight` is a *fixed* size, while `SetFillWidth/SetFillHeight` *fills* the parent axis; both interact with the parent's stretch weights. Every property setter above already calls `InvalidateLayout()`.

## Measure, arrange, draw (must-read for custom controls)

| Method | Description |
| --- | --- |
| `virtual Size Measure(const Size& availableSize) = 0` | **Must implement.** Return the desired size; `availableSize` may be `FLT_MAX` (unconstrained). |
| `virtual void Arrange(const Rect& finalRect)` | Arrange; the default records `arrangedRect_`; containers must arrange their children here. |
| `Rect GetArrangedRect() const` | Result of the last arrange; used by drawing and hit-testing. |
| `virtual void Draw(ID2D1RenderTarget* rt) = 0` | **Must implement.** Draw using absolute coordinates from `GetArrangedRect()`. |
| `virtual const std::vector<UIElement*>& GetChildren() const` | Child list (by reference; containers keep their own view buffer). **The returned reference points at an internal buffer**: valid until the element's child list/visibility changes or its `GetChildren()` is called again. During traversal **do not call `GetChildren()` on the same element again** (recursing into children uses their own buffers, which is safe). |
| `virtual bool UseCache() const` / `SetUseCache(bool)` | Offscreen cache (default true). |
| `virtual std::optional<D2D1_RECT_F> GetClipRect() const` | Child clip rect (absolute); `nullopt` means no clip. Returns the value set by `SetClipRect(...)`. |
| `void SetClipRect(const std::optional<Rect>&)` | Set the clip rect for this element's subtree (e.g. `ScrollViewer` clips content to the viewport). |
| `void RequestRepaint()` | Request a repaint (fires the global `RepaintRequest`). |

> **Measure/Arrange contract**: `Measure(available)` only says how big you *want* to be; do not mutate parent state there. `Arrange(finalRect)` says where you *were placed*; drawing is always based on `arrangedRect_`.

## Cache mechanism (affects performance and visuals)

- `UseCache()` is true by default: the framework draws the element into an offscreen bitmap of `(w + 2·bleed) × (h + 2·bleed)` and just blits it on following frames. `bleed` defaults to `4.0f` (room for antialiasing/shadow).
- For most controls the cache greatly reduces redundant drawing; but many small elements each holding a bitmap can cost noticeable VRAM — call `SetUseCache(false)` when needed.
- The cache is rebuilt only when the element is marked for repaint or its size changes. After changing colors/text call `RequestRepaint()`.
- Shadows are drawn into the cache (shadowed elements get an extra `GetShadowExtent()` on each side).
- During composition, subtrees **entirely outside the clip** are skipped (scrolled-out content allocates no cache).
- `SetVisible(false)` releases the cache immediately.

## Enabled / disabled

| Method | Description |
| --- | --- |
| `SetEnabled(bool)` | Set this element's own enabled state (default enabled) |
| `IsEnabled() const` | Own state |
| `IsEffectivelyEnabled() const` | Combines self and all ancestors: false if any ancestor is disabled |

- When disabled, the window does not dispatch mouse/keyboard to it; controls should grey themselves based on `IsEffectivelyEnabled()`. Disabling does **not** change layout.
- Typical use: `btn->SetEnabled(false);`. Most basic controls already grey themselves.

## ToolTip

| Method | Description |
| --- | --- |
| `SetToolTip(const std::wstring&)` / `const std::wstring& GetToolTip() const` | Set/read the hover text |

- The tooltip is a **framework-level overlay** drawn by `Window`; you write no drawing code.
- Behavior: after the mouse rests on an element for **0.5s**, a white, black-text, softly shadowed tooltip fades in at a fixed offset above the mouse position captured when it was shown; **any mouse movement dismisses it** and restarts the timer.
- Disabled elements do not show tooltips.
- The position is based on the mouse position at show time and does not follow the mouse.
- Data views use the same mechanism for per-item/cell/node tooltips (see Chapter 11).

## Shadows

| Method | Description |
| --- | --- |
| `SetShadow(bool)` / `HasShadow() const` | Toggle (default off) |
| `SetShadowColor(Color)` | Shadow color; **`a` is the visible edge opacity** (interior ≈ 2×) |
| `SetShadowBlur(float)` | Blur radius ≈ Gaussian `2σ`; spread ≈ `1.5×blur` (default 10) |
| `SetShadowOffset(float x, float y)` | Offset (default `0,3`) |
| `SetShadowCornerRadius(float)` | Corner radius; `<0` means 8 |
| `GetShadowExtent() const` | Max outward extent, used to enlarge the cache bleed |

- Shadows apply only to cached elements (base `UIElement` caches by default; `Card` auto-enables caching when a shadow is set).
- Implemented as **layered Gaussian CDF**: per-layer alpha follows the Gaussian, approximating a real Gaussian blur (close to DWM) without D2D effects, so per-pixel-alpha windows stay compatible.

## Focus and keyboard

| Method | Description |
| --- | --- |
| `virtual bool IsFocusable() const` | Can it take keyboard focus (default false) |
| `bool IsFocused() const` | Whether it currently holds focus |
| `void Focus()` / `Blur()` | Request/clear focus |

- Tab cycles focusable elements. The focus ring is drawn **only** when focus was obtained via Tab; mouse-click focus does not draw it.
- Only the focused element receives `OnKeyDown/OnKeyUp/OnChar`.

## Event virtuals (overridable)

| Virtual | When called / default |
| --- | --- |
| `HitTest(x,y)` | Hit-testing; returns the innermost hit element or `nullptr` |
| `OnMouseEnter/Leave/Move/Down/Up` | Mouse events; call `RequestRepaint()` yourself |
| `OnMouseWheel(dx,dy)` | Return true to consume (do not bubble to parent) |
| `OnKeyDown/Up(key,lParam)` | Only while focused |
| `OnChar(ch)` | Character input (including after IME) |
| `OnFocus` / `OnBlur` | Focus gained/lost |
| `UpdateAnimation(dt)` | Per-frame animation update; `dt` in seconds, clamped to `0.033s` |
| `HasActiveAnimation()` | Returning true keeps the 16ms timer redrawing |
| `IsTextInput()` | Show an I-beam cursor on hover |
| `ReleaseDeviceResources()` | Release brushes on device loss |
| `GetImeCandidateRect()` | IME candidate window positioning |
| `SetCompositionText(...)` | IME composition callback |
| `GetChildRenderTransform(child)` | Render transform for a child (page transitions) |
| `OnFontChanged()` | Font changed; default rebuilds layout |

## Event signals (`ZSignal`)

`MouseEnter`, `MouseLeave`, `MouseMove(float,float)`, `MouseDown(float,float)`, `MouseUp(float,float)`, `KeyDown(WPARAM,LPARAM)`, `KeyUp(WPARAM,LPARAM)`, `Char(wchar_t)`, `Focused`, `Blurred`.

> Since 1.8.0 these events are `ZSignal`s instead of raw `std::function` callbacks; connect with `Connect(elem->MouseDown, ...)` (connections auto-disconnect when the element is destroyed). They are still invoked alongside the `OnXxx` virtuals.

## Parent, visibility, context menu, connections

| Method | Description |
| --- | --- |
| `SetParent(UIElement*)` / `GetParent()` | Parent (usually set by the container) |
| `Window* GetWindow() const` | Owning window; `nullptr` if not attached or the window was destroyed (looked up by window id, no raw pointer held) |
| `SetVisible(bool)` / `IsVisible() const` | Visibility; false releases the cache and triggers `OnVisibilityChanged(false)` |
| `virtual void OnVisibilityChanged(bool visible)` | Visibility hook; e.g. `ComboBox` auto-collapses its popup when hidden |
| `SetContextMenu(std::shared_ptr<Menu>)` / `GetContextMenu()` | Fixed right-click menu |
| `SetContextMenuFactory(std::function<std::shared_ptr<Menu>()>)` / `BuildContextMenu()` | **Dynamic menu factory**: built on each right-click (can depend on current state); takes precedence over a fixed menu |
| `SetContextMenuEnabled(bool)` / `IsContextMenuEnabled() const` | Whether a right-click menu is allowed (default yes) |
| `virtual bool OnContextMenu(float,float)` | Right-click hook; return true to suppress the default menu |
| `SetBleed(float)` / `GetBleed() const` | Cache bleed (default `4.0f`) |
| `template<typename Signal, typename Slot> Connection Connect(Signal&, Slot&&)` | Connect a signal: registered in the element's `ConnectionGroup` (auto-disconnected when the element dies) and returns a Qt-style **passive `Connection` handle** (destruction does not disconnect). Ignoring the return is safe; to disconnect one, `auto c = Connect(...); c.disconnect();` |

## Font interface

| Method | Description |
| --- | --- |
| `static SetGlobalFont(const FontSpec&)` | Global font |
| `static SetGlobalFontFamily(const std::wstring&)` | Change only the global family |
| `static SetGlobalFontSize(float)` | Change only the global size |
| `static GetGlobalFont()` | Read the global font |
| `SetFont/SetFontFamily/SetFontSize/SetFontWeight` | Instance override (stops following the global font) |
| `ClearFont()` / `HasFontOverride()` | Clear/query the override |
| `GetEffectiveFontSpec()` | Resolved spec (override → type default → global) |
| `GetFontFormat()` | Shared text format (per-instance fast cache) |

## Minimal custom control

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

## Layout participation / draw timing

```cpp
enum class LayoutParticipation {
    Normal,            // participates in layout (default)
    DrawBeforeLayout,  // does not participate: drawn separately before the normal layout tree
    DrawAfterLayout    // does not participate: drawn after the normal layout tree, before overlays
};
void SetLayoutParticipation(LayoutParticipation);
LayoutParticipation GetLayoutParticipation() const;
bool ParticipatesInLayout() const;
```

- Layout containers (ColumnBox/RowBox/GridLayout) **skip** `!ParticipatesInLayout()` elements in Measure/Arrange and in the `ComposeImpl` child recursion.
- A non-participating element placed in a layout must manage its own position/size; `Window`'s custom title bar uses exactly this (the window places it at `(0,0)`).

## Drag regions (any control)

```cpp
void SetDraggable(bool on);                 // the whole arrangedRect is a drag region
void SetDragRegion(const Rect& localRect);  // a local sub-region
void ClearDragRegion();
bool HasDragRegion() const;
Rect GetDragRegion() const;
bool IsPointInDragRegion(float x, float y) const;
virtual void CollectDragRegions(std::vector<Rect>& out) const;   // Window collects recursively
```

- Each frame `Window` collects the tree's drag regions; on a hit `WM_NCHITTEST` returns `HTCAPTION`, so dragging / Aero Snap / double-click maximize are all handled by the system.

###chapter: Layout | Layout, ColumnBox, RowBox, GridLayout, LayoutHost, Card, Page, PageHost

## Layout (base)

```cpp
class Layout : public UIElement {
    virtual ~Layout() = default;
    bool UseCache() const override;   // layout containers do not cache
};
```

- Layout containers disable caching (`UseCache()==false`) because caching them is low-value and memory-heavy.

## ColumnBox (vertical)

```cpp
class ColumnBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
};
```

- Children are stacked top-to-bottom; `spacing` is the gap between adjacent children.
- Default stretch weights: horizontal `1.0` (children fill width), vertical `0.0`.

## RowBox (horizontal)

```cpp
class RowBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
};
```

- Children are laid out left-to-right; default weights horizontal `0.0`, vertical `1.0`.

## GridLayout (grid)

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

- Defaults: spacing `0`, alignment `Start`, both stretch weights `1.0`.
- `SetColumnStretch/SetRowStretch` control how extra space is distributed; `0` means "do not participate".
- `rowSpan/colSpan` span rows/columns. `LayoutHost` holds a `GridLayout` by default, so `Card`/`Page` usually add children via `GetLayoutAs<GridLayout>()`.

## LayoutHost (swappable layout container)

```cpp
class LayoutHost : public UIElement {
    std::shared_ptr<UIElement> GetLayout() const;
    void SetLayout(std::shared_ptr<UIElement> layout);
    template<typename T> std::shared_ptr<T> GetLayoutAs() const;
};
```

- Holds a `GridLayout` by default. `GetLayoutAs<GridLayout>()` is the usual way to reach it.
- `SetLayout` replaces the inner layout and reparents it; `nullptr` or `this` is ignored.

## Card

```cpp
class Card : public LayoutHost {
    static float DefaultPadding;           // 12.0f
    static float DefaultCornerRadius;      // 8.0f
    static Color DefaultBgColor;           // white
    static Color DefaultBorderColor;       // 200,200,200

    void SetPadding(float);
    void SetCornerRadius(float);
    void SetBackgroundColor(Color);
    void SetBorderColor(Color);
    void SetHoverBackgroundColor(Color);   // hover background
    void SetHoverBorderColor(Color);       // hover border
    void SetHoverAnimationSpeed(float);
};
```

- A card is a container with background/border/corner-radius/padding. Add children to its inner layout (`GetLayoutAs<GridLayout>()`).
- Supports hover colors with a smooth transition; enabling a shadow auto-enables caching.
- When disabled it is drawn grey.

## Page

```cpp
class Page : public LayoutHost {
    static float DefaultPadding;     // 10.0f
    void SetPadding(float);
    void SetBackgroundColor(Color);
};
```

- A `Page` is typically one screen of a `PageHost`.

## PageHost (page host / transition animation)

```cpp
class PageHost : public UIElement {
    enum class TransitionDirection { Left, Right, Up, Down };
    enum class TransitionEasing { Linear, EaseInOut, EaseOut };   // transition easing

    void AddPage(std::shared_ptr<Page> page);
    void NavigateTo(int index);
    void SetTransitionDirection(TransitionDirection dir);
    void SetTransitionEasing(TransitionEasing e);   // default EaseInOut (smooth)
    TransitionEasing GetTransitionEasing() const;
    void SetAnimationDuration(float seconds);   // min 0.01s, default 0.3s
    int GetCurrentIndex() const;
    std::shared_ptr<Page> GetCurrentPage() const;
};
```

- `NavigateTo` starts a slide transition. During the animation only the source and target pages are updated; after it finishes only the current page is (so a page is never updated twice per frame, which previously doubled nested-animation speed).
- Non-current page caches are released after the transition to save memory.
- **Note**: `GetCurrentIndex()` changes only after the animation completes — do not assume it already equals the target during the transition.

###chapter: Menus | MenuItem, Menu, MenuWindow

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
    // ---- enhanced fields ----
    int id = 0;                      // command id (paired with Menu::ItemSelected)
    bool checkable = false;          // show the check column
    bool checked = false;            // checked state
    bool radio = false;              // radio (mutually exclusive within a menu)
    bool isDefault = false;          // default item: bold + Enter triggers it
    bool danger = false;             // danger item: red text
    std::wstring shortcut;           // right-side shortcut hint (display only)
    std::optional<Color> bgColor;    // per-item custom background (rounded)
    std::optional<Color> textColor;  // per-item custom text color
    std::optional<FontSpec> font;    // per-item font (defaults to Menu::font)
};
```

## Menu

```cpp
class Menu : public std::enable_shared_from_this<Menu> {
    inline static FontSpec DefaultFont{};              // global default menu font
    void AddItem(const std::wstring& text, std::function<void()> callback = nullptr, int id = 0);
    void AddCheckItem(const std::wstring& text, bool checked,
                      std::function<void(bool)> onToggle = nullptr, int id = 0, bool radio = false);
    void AddSeparator();
    void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu, int id = 0);

    std::vector<std::shared_ptr<MenuItem>> items;
    std::function<void(Menu&)> onOpening;   // pre-open callback (can adjust checked/enabled/text live)
    ZSignal<int> ItemSelected;              // any normal item clicked (carries id; root menu receives submenu clicks)
    FontSpec font = DefaultFont;            // menu font
    static void SetDefaultFont(const FontSpec&);

    void ShowAt(int screenX, int screenY);  // standalone popup (bound to no window; typical: tray menu)
    void ShowAtCursor();                    // pop up at the mouse cursor
};
```

- `AddItem` adds a clickable item, `AddSeparator` a separator, `AddSubmenu` a submenu, `AddCheckItem` a check/radio item.
- Attach a menu to an element with `element->SetContextMenu(menu);` (or `SetContextMenuFactory` to build on demand), or to the window with `window.SetContextMenu(menu);`.
- **Standalone popup**: `menu->ShowAtCursor()` pops up at the mouse without binding to a window (great for tray right-click menus); it first closes any other open menu (mutual exclusion).
- **Signal-slot**: items may carry an `id`; collect them with `menu->ItemSelected.connect([](int id){...})`. Per-item `Clicked` / `AddItem` callbacks still work. `onOpening` runs before each popup.
- **Appearance**: `MenuItem::bgColor` / `textColor` / `font` per item; `Menu::font` overall; the hover highlight is a translucent overlay (still visible over a custom background). Supports `isDefault` (bold + Enter), `danger` (red), `shortcut` (right hint), `enabled=false` (disabled).

## MenuWindow (popup, framework-internal)

```cpp
class MenuWindow {
    MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y);
    void Show(int x, int y);
    void Hide();
    void CloseAll();
};
```

- Usually not used directly; created automatically on right-click.

###chapter: Window | Creation, backdrop, title bar, and root layout

```cpp
class Window {
    static Backdrop DefaultBackdrop;              // None
    static DWORD DefaultBackdropColor;            // 0
    static Color DefaultBackgroundColor;          // transparent

    bool Create(int width, int height, const std::wstring& title);   // does NOT auto-show
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
    void SetPosition(int x, int y);            // move the window, in DIP (useful for multiple windows)
    void SetSize(int width, int height);       // resize the window, in DIP
    bool IsValid() const;                      // still valid (not destroyed)
    int GetId() const;                         // unique window id (elements store ownership by id, avoiding dangling)
    void Close();                              // close this window (others keep running)

    void SetOwner(Window* owner);              // owned window: stays above the owner, minimizes with it
    Window* GetOwner() const;
    std::vector<Window*> GetOwnedWindows() const;   // all owned child windows of this window
    int RunModal(Window* owner = nullptr);     // run modally: disables the owner, nested loop, restores on close

    // Flash hint (5 consecutive flashes by default; captionOnly flashes only the caption, not the taskbar)
    void Flash(int times = 5, bool captionOnly = true);
    void StopFlash();

    // Convenient style / extended-style helpers (e.g. SetWindowExStyleFlag(WS_EX_TOOLWINDOW, true))
    DWORD GetStyle() const;
    DWORD GetExStyle() const;
    void SetWindowStyleFlag(DWORD flag, bool on);
    void SetWindowExStyleFlag(DWORD flag, bool on);

    // Show / hide / raise (bring to top of z-order)
    void Show();
    void ShowNoActivate();
    void Hide();
    void Raise();

    // How an owned child handles being minimized on its own
    enum class OwnedMinimizePolicy {
        None,            // do nothing (an old-style "small tile" may appear)
        Hide,            // intercept minimize -> hide; restore when the owner restores/activates
        DisableMinimize  // gray out the minimize button and ignore minimize-related messages
    };
    void SetOwnedMinimizePolicy(OwnedMinimizePolicy p);
    OwnedMinimizePolicy GetOwnedMinimizePolicy() const;

    // Backdrop layer (only three options): None = transparent / Acrylic / Mica
    // enum class Backdrop { None, Acrylic, Mica };
    // SetBackdrop(layer, overlayArgb): the overlay is an ARGB colour composited on top of the layer
    void SetBackdrop(Backdrop layer, DWORD overlayArgb = 0x00000000);
    Backdrop GetBackdrop() const;

    // Background "4-layer parameter model" (all tunable — see below)
    struct BackgroundParams {
        float        blurAmount;    // Blur layer: Gaussian sigma
        float        saturation;    // Luminosity layer: saturation (used by the Legacy/RS2 recipe)
        D2D1_COLOR_F tint;          // Tint layer: colour (with alpha, i.e. the veil)
        D2D1_COLOR_F luminosity;    // Luminosity layer: brightness colour
        float        noiseOpacity;  // Noise layer
    };
    inline static BackgroundParams AcrylicParams;   // Acrylic: source = host backdrop (window behind)
    inline static BackgroundParams MicaParams;      // Mica: source = cached desktop wallpaper
    enum class AcrylicPreset { Legacy, Luminosity, Base, Thin };   // official acrylic presets
    static void SetBackgroundParams(Backdrop layer, const BackgroundParams& p);  // set all params
    static void SetBackgroundParams(Backdrop layer, AcrylicPreset preset);       // use a preset

    // Event signals
    ZSignal<bool*> Closing;           // close requested; set *cancel = true to cancel
    ZSignal<> BackdropUnsupported;    // requested implementation unsupported on this system
    ZSignal<> DeviceLost;             // device lost
    ZSignal<HRESULT> RenderingError;  // fatal rendering error

    // Custom title bar (control in the "Window tools" chapter)
    void SetCustomTitleBar(std::shared_ptr<UIElement> bar);   // pass nullptr to restore the native one
    std::shared_ptr<UIElement> GetCustomTitleBar() const;
    bool HasCustomTitleBar() const;
    float GetCustomTitleBarHeight() const;
    void SetTitleBarVisible(bool on);
    bool IsTitleBarVisible() const;
    void SetTitle(const std::wstring& title);                 // native title text (use TitleBar::SetTitle with a custom title bar)
    std::wstring GetTitle() const;                            // current window title (GetWindowTextW)
    HWND GetHwnd() const;                                     // native window handle
    void SetIcon(HICON bigIcon, HICON smallIcon);             // native title-bar icons (WM_SETICON)

    // Border / resizing
    void SetResizable(bool on);
    bool IsResizable() const;

    // Corner preference
    enum class WindowCorner { Default, Square, Round, RoundSmall };
    void SetWindowCorner(WindowCorner c);
    WindowCorner GetWindowCorner() const;

    // Window actions (for custom title bar buttons)
    void Minimize();
    void Maximize();
    void Restore();
    void MaximizeRestore();
    bool IsMaximizedWindow() const;
    void BeginSystemDrag();

    void SetMouseCapture(UIElement* elem);
    void ReleaseMouseCapture(UIElement* elem);
};
```

`Backdrop` (**backdrop layer**, only three options): `None` (fully transparent), `Acrylic`, `Mica`. The second argument of `SetBackdrop` is the **overlay**: an ARGB colour composited on top of the layer (`A` = how much is revealed/covered, `RGB` = the colour). **`BackdropMode` no longer exists** — backdrops are always rendered by ZufyUI itself (no system material), so Win10 and Win11 look the same.

**Behavior and pitfalls:**

- `Create` creates the window, initializes Direct2D, applies the backdrop, and builds a default root layout: a `ColumnBox` with `margin 20` and `spacing 10`. `GetRootColumnBox()` returns it.
- `Run()` enters the message loop and blocks until the window closes.
- An internal 16ms timer drives animations; `timeBeginPeriod(1)` / `timeEndPeriod(1)` reduce jitter.
- `deltaTime` is clamped to `0.033s`, so animations never jump to their end after idle/minimize.
- Title-bar colors use Win11 DWM attributes; ignored on unsupported systems.
- Shadows: the window composites element shadows into their caches; tooltips are drawn by the window too.
- **Owned windows**: the owner relationship set at creation (via the `CreateWindowEx` parent parameter) means owned windows have **no separate taskbar button**; when the owner is minimized their owned children are hidden **unconditionally**, restored when the owner is restored/activated (not relying on the shell's minimize grouping, which does not apply when the owner is in the background).
- **Modal**: `RunModal` uses the official mechanism — `EnableWindow(owner, FALSE)` + activating the modal window + an `IsDialogMessage` nested loop. **No global hooks are used**, so other processes are unaffected. Clicking the disabled owner produces the standard system hint.
- **`OwnedMinimizePolicy`**: how an owned child handles being minimized on its own. `None` does nothing; `Hide` intercepts minimize and hides, restoring when the owner restores/activates; `DisableMinimize` grays out the minimize button and ignores minimize-related messages.
- **Dangling cleanup**: on destroy a window clears every reference other windows hold to it (`owner_` and hidden lists), so address reuse can never affect unrelated windows.
- **Custom title bar**: `SetCustomTitleBar(bar)` installs a non-layout title bar control (see "Window tools"); the window places it at `(0,0)` and shifts the root layout down by its height. Pass `nullptr` to restore the native title bar (sends `SWP_FRAMECHANGED`). `SetTitleBarVisible(false)` hides it while keeping the custom frame.
- **Backdrop (`Backdrop` is a 3-way layer + overlay colour)**: the layer is only `None / Acrylic / Mica`, and it is **always rendered by ZufyUI itself** — there is no "system vs manual" split and no system material (so Win10 and Win11 look the same).
  - `Acrylic`: our own DirectComposition implementation of the official acrylic recipe (Border noise tiling → Opacity → Gaussian blur → Luminosity/Color blends → noise multiply, plus a Saturation step and a `CompositeStep` for the tint so its alpha takes effect); source = **host backdrop** (the content behind the window).
  - `Mica`: reads the desktop wallpaper (`SPI_GETDESKWALLPAPER` + WIC) → blur / desaturate / white veil / noise, baked once into a bitmap placed on a **separate compositor layer** (below the content layer); moving the window only changes that layer's `Offset` (a pure translation, no resampling).
  - Parameters follow the "**4-layer model**" (Blur / Luminosity / Tint / Noise): `SetBackgroundParams(layer, BackgroundParams)` to set all of them, or `SetBackgroundParams(layer, AcrylicPreset)` for an official preset (`Legacy / Luminosity / Base / Thin`). `AcrylicParams` / `MicaParams` hold the defaults (the latter is a tuned look).
  - If the requested effect cannot be realized on the current system, `BackdropUnsupported` fires and it does **not** substitute another effect.
- **Rendering (1.8.0)**: Direct2D 1.1 + DXGI flip SwapChain + DirectComposition (`WS_EX_NOREDIRECTIONBITMAP`), per-pixel transparent; a process-wide shared D3D11/D2D device and WinRT `ICompositor`. `Create` **no longer shows the window**; call `Show()` explicitly.
- **Border / resize / corners**: `SetResizable` controls edge resizing; when snapped, the DWM border/shadow is preserved (`WM_NCCALCSIZE` only insets the snapped edges), and maximizing does not inset. `SetWindowCorner` maps to `DWMWA_WINDOW_CORNER_PREFERENCE`.

###chapter: Application and multiple windows | Application and multi-window

## Application

`Application` is the process / UI-thread-level object that manages one shared message loop and all top-level windows. It is decoupled from window lifetime: `Application` is just a handle to an internal singleton, so constructing/destroying it does **not** destroy existing windows (no "app destroyed, windows dangling" pitfalls).

```cpp
class Application {
    std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title);
    std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title, Window* owner); // owned child
    void AddWindow(const std::shared_ptr<Window>& w);
    int Run();
    void Quit(int code = 0);
    void CloseAllWindows();
    size_t WindowCount() const;
    bool RegisterApp(const AppInfo& info);   // app identity self-registration (see "App identity self-registration")
    static Application& Instance();
};
```

**Recommended usage (Qt style):**

```cpp
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ZufyUI::Application app;
    auto w1 = app.CreateWindow(1000, 700, L"Main");
    auto w2 = app.CreateWindow(640, 480, L"Tool");
    // build each UI: w1->GetRootColumnBox()->AddChild(...)
    return app.Run();                    // one shared message loop
}
```

**Behavior:**

- `CreateWindow` creates and registers a window; you **must hold** the returned `shared_ptr<Window>`, otherwise the window is destroyed when the pointer dies.
- `Run()` runs **one** message loop; messages for all windows on the thread are dispatched by it.
- Calling `CreateWindow` while the loop is running also works.
- Closing one window leaves the others running; the loop exits when the **last** window closes. You can also call `Quit()`.
- A process should run windows on a single UI thread (same as Win32).

## App identity self-registration (RegisterApp / AppInfo)

Declare "app name + icon" once; the library sets the **process-level AppUserModelID** for you and (once authorized) writes the registry + caches the icon locally, so the **toast top-left shows the app name and icon**, and the jump list / taskbar grouping belong to that identity.

```cpp
struct AppInfo {
    std::wstring displayName;      // display name (toast / jump list / taskbar grouping)
    std::wstring aumid;            // unique id; empty = derived from displayName
    std::shared_ptr<Image> icon;   // icon (optional)
};

bool RegisterApp(const AppInfo& info);                 // free function
bool Application::RegisterApp(const AppInfo& info);    // forwards to the above (equivalent)
```

**Authorization macro (important)**: this library is not a system library, so touching the registry/files on the app's behalf is out of scope — it therefore requires **explicit authorization**: define the macro **before including the library headers**:

```cpp
#define ZUFYUI_ALLOW_APP_REGISTRATION
```

Without it, `RegisterApp` only sets the process AUMID (**no side effects**) and writes no registry / files.

**What the library does when authorized**:

1. Set the process AUMID: `SetCurrentProcessExplicitAppUserModelID`
2. Write the registry key `HKCU\Software\Classes\AppUserModelId\<AUMID>`: `DisplayName` + `IconUri`
3. Cache the icon at `%LOCALAPPDATA%\ZufyUI\AppReg\<AUMID>\app.ico` (exported from `Image` as a single-image `.ico`)
4. **On process exit**: delete that cache directory and the registry key above (so `IconUri` never points at a deleted file)

**Usage** (call once, preferably before creating windows):

```cpp
#define ZUFYUI_ALLOW_APP_REGISTRATION   // must be before the include
...
int WINAPI WinMain(...) {
    RegisterApp(AppInfo{ L"MyApp", L"ZufyUI.MyApp", myIcon });   // aumid may be empty = derived
    auto w = Application::Instance().CreateWindow(/*...*/);
    // ...
    return Application::Instance().Run();
}
```

**Notes:**

- The AUMID is per-process and unique; **call once**. When `aumid` is empty it is derived from `displayName` (spaces/illegal chars stripped; a `ZufyUI.` prefix is added if there is no dot; max 128 chars).
- The authorization macro is a **compile-time** switch; when absent, nothing is left on the user's machine.
- On Win10/11 a legacy tray balloon is upgraded to a toast; its top-left icon/app name come from the registry keyed by the AUMID — exactly what this registration solves (otherwise it is blank).
- Exit cleanup is handled inside the library; the app writes no code for it.

## Independence and compatibility

- **Per-window repaint/layout**: each element records its owning window (`UIElement::GetWindow()`); `RequestRepaint()` / `InvalidateLayout()` only affect that window — windows do not repaint each other.
- **Per-window DPI**: the current DPI scale is **thread-local** and set per window before drawing, so mixed-DPI windows snap correctly.
- **Per-window activation**: `Window` exposes instance signals `Activated` / `Deactivated` / `Closed`; global `UIZSignals::WindowDeactivated` now carries a `Window*`, and `GlobalMouseDown` carries a `Window*`. Controls such as ComboBox only react to their own window's events.
- **Per-window overlay drawing**: global `UIZSignals::DrawOverlay` now carries a `Window*` (plus the render target); subscribers **must** filter with `GetWindow()`, otherwise window A's popup will be drawn onto window B — the classic multi-window cross-talk. ZufyUI's own controls already do this.
- **Mouse capture**: Win32 `SetCapture` is used only while the mouse button is held (dragging) and is released on mouse-up (element-level logical capture is unaffected). This fixes ComboBoxes holding the thread-wide capture after expanding, which made other windows unusable.
- **Modal / owned windows**: `Window::SetOwner(owner)` creates an owned child (stays above and minimizes with the owner); `Window::RunModal(owner)` runs a window modally (disables the owner, nested loop, restores on close). While modal, clicking the disabled owner **flashes** the modal window.
- **Window handle & dangling**: elements store the owning window as an **id** (not a raw pointer); after the window is destroyed `GetWindow()` returns `nullptr`, eliminating crashes from elements holding a dangling window pointer.
- **Shared resources**: the `ID2D1Factory` and the system timer period (`timeBeginPeriod`) are managed by the application core and shared by all windows.
- **Backward compatible**: the single-window style still works — `Window win; win.Create(...); win.Show(); win.Run();` (since 1.8.0 `Create` no longer auto-shows; call `Show()`); `Run()` forwards to the app-level loop.

## Multi-window pitfalls

- Keep the `shared_ptr` returned by `CreateWindow` alive (e.g. store it in a container).
- Window-scoped global signals (`GlobalMouseDown`, `WindowDeactivated`) carry a `Window*`; filter with `GetWindow()`.
- Create and run all windows on the same UI thread.

###chapter: Basic controls | Label, Button, TextBox, ComboBox, ToggleSwitch, CheckBox, ScrollViewer, ProgressBar, Slider

## Helper

```cpp
enum class TextHAlign { Left, Center, Right };

inline void DrawTextWithEllipsis(ID2D1RenderTarget* rt,
    const std::wstring& text, const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color, const FontSpec& spec,
    ComPtr<ID2D1SolidColorBrush>& textBrush,
    IDWriteTextFormat* textFormat = nullptr, bool forceNoWrap = false,
    TextHAlign align = TextHAlign::Left);
```

- Draws a single line and appends an ellipsis when truncated; `align` controls horizontal alignment (used for table column alignment).

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
    void SetAlignment(HAlign hAlign, VAlign vAlign);
    HAlign GetHorizontalAlignment() const;
    VAlign GetVerticalAlignment() const;

    void SetPadding(const Thickness&);   // or a float for all sides
    void SetLineSpacing(float);
    void SetMaxLines(int);               // extras get an ellipsis
    Size GetDesiredSize() const;

    static void SetDefaultTextColor(Color);
    static void SetDefaultFontSize(float);
    static void SetDefaultOverflow(TextOverflow);
    static void SetDefaultAlignment(HAlign, VAlign);

    // Icon / image
    void SetImage(std::shared_ptr<Image> image);
    std::shared_ptr<Image> GetImage() const;
    void SetIconSize(float w, float h);      // 0 means the image's natural size
    Size GetIconSize() const;
    void SetIconSpacing(float spacing);      // gap between icon and text/children
    float GetIconSpacing() const;

    // Nested child elements (inline row: icon + text + children)
    void AddChild(std::shared_ptr<UIElement> child);
    void ClearChildren();
    size_t GetChildCount() const;
};
```

- Defaults: black text, `Ellipsis`, left-aligned, vertically centered, size `16`.
- `SetMaxLines` only matters in `Wrap` mode; `Ellipsis` is inherently single-line.
- When disabled the text turns grey (`DefaultDisabledColor`).
- **Icon**: after `SetImage` the Label draws the icon to the left of its text; `SetIconSize(0,0)` uses the image's natural size. An icon can coexist with text and children (it is drawn even with no text).
- **Nested children**: elements added via `AddChild` are laid out **inline** with the icon and text, and participate in `GetChildren()` recursion (window ownership, repaint, and layout all treat them as child elements).

## Button

```cpp
class Button : public UIElement {
    ZSignal<> Clicked;
    ZSignal<bool> Toggled;               // only meaningful after SetCheckable(true)

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

    void SetCheckable(bool);
    void SetChecked(bool);
    bool IsChecked() const;              // meaningful only with SetCheckable(true)
    void SetTextAlignment(...);
    void SetPadding(...);
    void SetAutoRepeat(bool);            // hold to fire Clicked repeatedly

    static void SetDefaultColors(Color, Color, Color);
    static void SetDefaultSize(float width, float height);
};
```

- Defaults: `120×36`, radius `4`, blue background / white text (`0,120,212`).
- `Clicked` fires when the mouse is pressed and released inside the button (not if released outside).
- Keyboard: when focused, `Enter` or `Space` fires `Clicked`.
- Disabled: no events and drawn grey.
- Checkable: after `SetCheckable(true)`, clicking toggles the checked state and fires `Toggled(bool)`.

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
    void SetRevealPassword(bool reveal);
    bool IsPasswordRevealed() const;
    void SetMaxLength(int maxLength);        // -1 = unlimited
    int  GetMaxLength() const;
    void SetReadOnly(bool);
    void SetInputFilter(std::function<bool(wchar_t)>);  // accept only when true

    bool IsFocused() const;
    void Focus(); void Blur();

    int GetSelectionStart() const;
    int GetSelectionEnd() const;
    int GetCursorPosition() const;
    void SetSelection(int start, int end);
    void SelectAll();
    void Undo(); void Redo();
    void Copy(); void Cut(); void Paste();

    // colors: SetTextColor / SetBackgroundColor / SetBorderColor / SetSelectionColor /
    //         SetHoverBackgroundColor / SetHoverBorderColor / SetCursorBlinkInterval
};
```

- Defaults: `160×30`, size `14`, caret blink `0.5s`.
- Supports `Ctrl+C/X/V/A/Z/Y`, arrow keys, `Home/End`, `Shift+arrows` selection, mouse drag, and IME composition.
- **Selection accumulation**: holding Shift and pressing an arrow extends the highlight character by character (an independent anchor is used internally, so it never collapses to one character per press).
- `SetInputFilter` is the character-level filter, e.g. digits only: `[](wchar_t c){ return c >= L'0' && c <= L'9'; }`.
- `SetReadOnly(true)`: you can still select/copy but not edit.
- `TextChanged` fires on every text change; `ReturnPressed` fires on Enter.

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
    void Collapse(); void Expand(); void SetOpen(bool);

    void SetPlaceholder(const std::wstring&);
    void SetItemDisabled(int index, bool disabled = true);
    bool IsItemDisabled(int index) const;
    void SetMaxVisibleItems(int count);      // 0 = unlimited

    void SetEditable(bool); bool IsEditable() const;
    void SetFilterEnabled(bool); bool IsFilterEnabled() const;
    void SetEditText(const std::wstring&); std::wstring GetEditText() const;

    // styling: SetNormalBgColor / SetHoverBgColor / SetBorderColor / SetIndicatorColor / SetListItemHeight ...
};
```

- Defaults: `160×30`, item height `24`, indicator width `3`, height ratio `0.6`.
- The open list is drawn on the `DrawOverlay` layer, so it can extend beyond the control bounds.
- `SetMaxVisibleItems(n)` limits the visible rows; extras scroll.
- **Editable + filtering**: `SetEditable(true)` enables typing; `SetFilterEnabled(true)` filters items live (case-insensitive substring); `GetSelectedIndex()` refers to the **filtered** list. It has a blinking caret and click-to-position (text-range selection is not supported in this version).
- Disabled items cannot be selected; keyboard navigation skips them.
- **Adaptive width**: the collapsed box is sized to the **average item text width** (text that does not fit is ellipsized and gets an automatic `ToolTip` with the full text); the **drop-down list is sized to the widest item** so every option is fully visible (hit-testing, background, and scrollbar all use that width).

## ToggleSwitch

```cpp
class ToggleSwitch : public UIElement {
    ZSignal<bool> Toggled;
    ToggleSwitch(bool initialState = false);
    void SetOn(bool on); bool IsOn() const;
    void SetColors(Color on, Color off, Color knob);
    void SetSize(float width, float height);
    void SetIndeterminate(bool indeterminate);
    void SetLabel(const std::wstring&); void SetLabelColor(Color);
    void SetAnimationSpeed(float speed);
};
```

- Defaults: `50×24`, on `0,120,212`, off `200,200,200`, white knob.
- Keyboard: when focused, `Space`/`Enter` toggles.

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

    static void DrawBox(ID2D1RenderTarget*, const D2D1_RECT_F& rect, float fill,
                        State state, D2D1_COLOR_F boxColor, D2D1_COLOR_F checkColor,
                        D2D1_COLOR_F borderColor, float cornerRadius,
                        ComPtr<ID2D1SolidColorBrush>& brush, bool enabled = true);
};
```

- Check has a progress animation; hover has a fading light-blue halo.
- With `SetTriState(true)`, `Toggle()` cycles Unchecked → PartiallyChecked → Checked.
- Keyboard `Space`/`Enter` toggles; disabled is greyed.
- `DrawBox` is static so `ListView`/`TableView`/`TreeView` row checkboxes reuse it for a consistent look.

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

- Scrollbar width `8`, min length `20`, wheel step `30`.
- `Auto` shows the bar only when content overflows; `Always` always shows it; `Hidden` never shows it (scrolling via wheel/code still works).
- `ScrollChanged(x,y)` fires whenever the offset changes (including during animation).
- `SetContentMargin` adds padding around the content.
- The internal `ScrollBar` is normally not used directly.

## ProgressBar

```cpp
class ProgressBar : public UIElement {
    ZSignal<float> ValueChanged;

    ProgressBar();
    void SetValue(float value);        // [0,1]; turns off indeterminate
    float GetValue() const;
    void SetRange(float min, float max);
    float GetMin() const; float GetMax() const;
    void SetRangeValue(float value);
    float GetRangeValue() const;
    void SetShowText(bool); bool IsShowText() const;
    void SetTextColor(Color);
    void SetIndeterminate(bool); bool IsIndeterminate() const;
    void SetTrackColor(Color); void SetFillColor(Color); void SetBorderColor(Color);
    void SetIndeterminateBlockWidth(float); void SetIndeterminateSpeed(float);
};
```

- Defaults: `200×20`, indeterminate block width `40`, speed `100`.
- `SetValue` normalizes to `[0,1]`; `SetRangeValue` uses `[min,max]`.
- `SetShowText(true)` draws a percentage; disabled greys the fill.

## Slider

```cpp
class Slider : public UIElement {
    ZSignal<float> ValueChanged;
    ZSignal<> SliderReleased;          // drag ended (or keyboard adjustment ended)

    Slider();
    void SetRange(float min, float max);
    void SetValue(float value); float GetValue() const;
    void SetStep(float step); float GetStep() const;
    void SetSnapToStep(bool); bool IsSnapToStep() const;
    void SetTrackColor(Color); void SetFillColor(Color);
    void SetThumbColor(Color); void SetHoverThumbColor(Color);
    void SetThumbSize(float); void SetTrackHeight(float);
};
```

- Defaults: `160×24`, range `[0,100]`, track height `4`, thumb diameter `14`.
- Arrow keys adjust by `step`; `Home/End` jump to the range ends.
- `SetSnapToStep(true)` snaps the live value to multiples of `step`.

###chapter: Data views | ListView, TableView, TreeNode, TreeView

> Data views have many details around **selection, checking, sorting and disabling**, and the framework treats per-item/per-row metadata specially. Read the "Key points" of each section.

## ListView

```cpp
enum class SelectionMode { Single, Extended, Multi, None };

class ListView : public UIElement {
    ZSignal<int> SelectionChanged;
    ZSignal<int> ItemClicked;
    ZSignal<int> ItemDoubleClicked;
    ZSignal<std::vector<int>> SelectionChangedMulti;
    ZSignal<int, bool> ItemCheckStateChanged;
    ZSignal<int> ItemRightClicked;   // right-click an item (passes the row)

    ListView();
    // data
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
    void BeginUpdate();   // batched add/remove: suspend refresh (v1.9.6)
    void EndUpdate();     // refresh once at the end
    void MoveItem(int from, int to);
    void SwapItems(int a, int b);
    void Sort(bool ascending = true);
    void SortItems(std::function<bool(const std::wstring&, const std::wstring&)>);
    void ScrollToItem(int index);
    void SetEmptyText(const std::wstring&);

    // selection
    void SetSelectionMode(SelectionMode); SelectionMode GetSelectionMode() const;
    void SetSelectedIndex(int); int GetSelectedIndex() const;
    std::wstring GetSelectedText() const;
    std::vector<int> GetSelectedIndices() const;
    std::vector<std::wstring> GetSelectedTexts() const;
    void SelectAll();

    // right-click: row of the last right-click (for SetContextMenuFactory; -1 = none)
    int GetContextRow() const;

    // checking
    void SetCheckable(bool);
    void SetItemChecked(int, bool); bool IsItemChecked(int) const;
    std::vector<bool> GetSelectionStates() const;
    std::vector<bool> GetCheckStates() const;
    std::vector<int> GetCheckedIndices() const;

    // per-item metadata (key)
    void SetItemDisabled(int index, bool disabled = true);
    bool IsItemDisabled(int index) const;
    void SetItemTextColor(int index, Color); void ClearItemTextColor(int index);
    void SetItemToolTip(int index, const std::wstring&);
    std::wstring GetItemToolTip(int index) const;

    // sort indicator
    void SetSortComparator(std::function<bool(const std::wstring&, const std::wstring&)>);
    bool IsSortAscending() const;
    void SetShowSortIndicator(bool);
    void SetSortIndicatorColor(Color);

    // styling: SetItemHeight / SetSelectedColor / SetHoverColor / SetAlternatingRowColors /
    //   SetAlternatingRowColor / SetMarqueeEnabled / SetButtonMode / SetButtonSpacing ...
};
```

**Key points:**

- **Per-item metadata is bound to the item object itself**: `SetItemDisabled/SetItemTextColor/SetItemToolTip` are internally tied to the `Label` item, so after `Sort()` / `MoveItem()` / `SwapItems()` the metadata still follows its item and never misaligns.
- Selection modes: `Single`, `Extended` (Ctrl-click / Shift-range), `Multi` (click toggles), `None` (no selection, but `ItemClicked` still fires).
- **Keyboard**: `↑/↓/Home/End/PageUp/PageDown` **skip disabled items**; a focused list also supports **type-ahead** (typing letters jumps to the matching prefix).
- `SetCheckable(true)` shows a checkbox per row; `ItemCheckStateChanged(index, checked)` reports changes.
- Sorting: set a comparator then call `Sort(asc)` (or `SortItems`); `SetShowSortIndicator(true)` shows the arrow at the top-right.
- Hovering an item with `SetItemToolTip` uses the shared tooltip.
- **Right-click**: `ItemRightClicked(row)` passes the clicked row; you can also read `lv->GetContextRow()` inside `SetContextMenuFactory([...]{ ... })` to build the menu on demand.

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
    ZSignal<int,int> CellRightClicked;   // right-click a cell (passes row, col)

    TableView();
    void SetRowCount(int); int GetRowCount() const;
    void SetColumnCount(int); int GetColumnCount() const;
    void AppendRow(); void InsertRow(int); void RemoveRow(int);
    void AppendColumn(); void InsertColumn(int); void RemoveColumn(int);

    void SetItem(int row, int col, const std::wstring& text);
    void SetItem(int row, int col, std::shared_ptr<Label> label);
    std::wstring GetItemText(int row, int col) const;
    std::shared_ptr<Label> GetItemLabel(int row, int col) const;

    void SetHeaderLabel(int col, const std::wstring&);
    std::wstring GetHeaderLabel(int col) const;
    void SetHorizontalHeaderLabels(const std::vector<std::wstring>&);
    void SetColumnWidth(int col, float); float GetColumnWidth(int col) const;
    void SetRowHeight(float);
    void SetRowHeightAt(int row, float);
    float GetRowHeightAt(int row) const;
    void ClearRowHeightAt(int row);
    void SetHeaderHeight(float);

    void SetHeaderVisible(bool); void SetGridVisible(bool);
    void SetAlternatingRowColors(bool); void SetAlternatingRowColor(Color);
    void SetColumnVisible(int col, bool visible); bool IsColumnVisible(int col) const;
    void SetColumnAlignment(int col, TextHAlign); TextHAlign GetColumnAlignment(int col) const;

    void SetSelectionMode(SelectionMode); SelectionMode GetSelectionMode() const;
    void SetCurrentCell(int row, int col);
    int GetCurrentRow() const; int GetCurrentColumn() const;
    void SelectRow(int row); void SelectColumn(int col); void ClearSelection();
    void ScrollToCell(int row, int col);
    std::vector<std::pair<int,int>> GetSelectedCells() const;
    std::vector<int> GetSelectedRows() const;
    std::vector<int> GetSelectedColumns() const;
    std::vector<bool> GetSelectionStates() const;

    // right-click: row/col of the last right-click (for SetContextMenuFactory; -1 = none)
    int GetContextRow() const; int GetContextColumn() const;

    void SetCheckable(bool);
    void SetRowChecked(int row, bool); bool IsRowChecked(int row) const;
    std::vector<bool> GetRowCheckStates() const;
    std::vector<int> GetCheckedRows() const;
    void SetRowDisabled(int row, bool disabled = true); bool IsRowDisabled(int row) const;

    void SetCellTextColor(int row, int col, Color); void ClearCellTextColor(int row, int col);
    void SetCellToolTip(int row, int col, const std::wstring&);

    void SetColumnComparator(int col, std::function<bool(const std::wstring&, const std::wstring&)>);
    void SortByColumn(int col, bool ascending = true);
    int GetSortColumn() const; bool IsSortAscending() const;
    void SetShowSortIndicator(bool); void SetSortIndicatorColor(Color);
};
```

**Key points:**

- Defaults: `400×300`, header height `26`, default row height `24`, min column width `40`, default selection mode `Cell`.
- Row-level metadata (cell text color / tooltip / per-row disabled / per-row height) is **remapped together with rows/columns** in `SortByColumn`, row/column insert/remove — so it still follows the original row/column after sorting.
- `SetColumnVisible(false)` hides a column: it is excluded from layout, drawing, and hit-testing.
- `SetColumnAlignment` takes `TextHAlign::{Left,Center,Right}`.
- `SetRowHeightAt(row, h)` changes one row; `SetRowHeight` changes the default. Row-height changes affect scrolling and hit-testing.
- Keyboard: `↑/↓` skip disabled rows; `←/→` and `Home/End` move the current cell.
- `SelectionMode::None` produces no selection but still fires `CellClicked`.
- **Right-click**: `CellRightClicked(row, col)` passes the clicked cell; you can also read `GetContextRow()/GetContextColumn()` inside `SetContextMenuFactory`.

## TreeNode

```cpp
struct TreeNode {
    std::vector<std::shared_ptr<Label>> columns;       // Label-ized: one real Label per column (col 0 = node text)
    TreeNode* parent = nullptr;                        // non-owning
    std::vector<std::shared_ptr<TreeNode>> children;
    bool expanded = false;
    int depth = 0;
    void* userData = nullptr;

    std::wstring icon;                                 // leading icon (one char/emoji, may be empty)
    bool checkable = false;
    CheckState checkState = CheckState::Unchecked;     // tri-state
    bool selected = false;
    bool enabled = true;
    std::wstring tooltip;                              // shown via the shared tooltip

    TreeNode(const std::wstring& text);
    TreeNode(const std::vector<std::wstring>& cols);
};
```

- `parent` is a **non-owning** raw pointer (ownership is via `children`); `depth` is maintained by the framework.
- `icon` is a single leading char/emoji; `tooltip` uses the base-class tooltip.
- `columns` holds **real Labels** (Label-ized): read text via `node->columns[c]->GetText()`; you can also set per-cell font/color/embedded content directly. Position and clipping are owned by `TreeView::GetChildren` (column 0 starts after indent, expander, selection indicator, checkbox and icon).

## TreeView

```cpp
enum class SelectionMode { Single, Extended, Multi };
enum class CheckMode { Linked, Independent };

class TreeView : public UIElement {
    // signals: SelectionChanged / NodeClicked / ItemDoubleClicked / ItemRightClicked /
    //   HeaderClicked / SelectionChangedMulti / ExpandChanged / ItemCheckStateChanged

    TreeView();
    void SetColumnCount(int count);
    void SetHeaderLabels(const std::vector<std::wstring>&);
    void SetColumnWidth(int col, float); float GetColumnWidth(int col) const;
    void SetHeaderVisible(bool);

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

    void ExpandNode(std::shared_ptr<TreeNode>, bool);
    void ToggleNode(std::shared_ptr<TreeNode>);
    void ExpandNodeRecursive(std::shared_ptr<TreeNode>, bool);
    bool IsExpanded(std::shared_ptr<TreeNode>) const;
    void ExpandAll(); void CollapseAll();
    void ExpandToDepth(int depth);
    void SetDefaultExpandDepth(int depth); int GetDefaultExpandDepth() const;

    void SetFilter(std::function<bool(const std::shared_ptr<TreeNode>&)>);
    void ClearFilter(); bool HasFilter() const;
    void Search(const std::wstring& keyword);
    std::wstring GetNodePath(const std::shared_ptr<TreeNode>&, const std::wstring& separator = L" / ") const;

    void SetSelectionMode(SelectionMode);
    void SetSelectedNode(std::shared_ptr<TreeNode>);
    std::shared_ptr<TreeNode> GetSelectedNode() const;
    std::vector<std::shared_ptr<TreeNode>> GetSelectedNodes() const;
    void SelectAll();
    void SetCheckable(bool enable, bool recursive = true);
    void SetCheckMode(CheckMode); CheckMode GetCheckMode() const;
    std::vector<std::shared_ptr<TreeNode>> GetCheckedNodes() const;
    std::vector<bool> GetSelectionStates() const;
    std::vector<TreeNode::CheckState> GetCheckStates() const;

    void SetRowHeight(float); void SetIndent(float);
    void SetAlternatingRowColors(bool); void SetGridVisible(bool);
};
```

**Key points:**

- Defaults: `400×300`, row height `24`, indent `16`, header height `24`; one column, width `40`, header `Name`.
- Tri-state checking: with `CheckMode::Linked`, parent/child checks propagate and `PartiallyChecked` is computed automatically; with `Independent` they are independent.
- `Search(keyword)` wraps `SetFilter`: keeps matching nodes plus their ancestor chain, hides the rest.
- `GetNodePath(node)` returns the root-to-node path joined from column 0 (customizable separator).
- `SetDefaultExpandDepth` affects **newly inserted** nodes only; use `ExpandToDepth` for existing ones.
- `SortChildren` sorts only the given node's children; `recursive=true` sorts the whole subtree.
- Node `tooltip` uses the shared tooltip; `enabled=false` greys the node and makes it unselectable.

###chapter: Images | Image, ImageManager, ImageDeviceCache

The image system lives in `ZufyUIImages.h`: WIC decoding plus Direct2D (GPU) drawing and transforms.

## ImageManager / ImageDeviceCache

```cpp
class ImageManager {
    static ImageManager& Instance();
    IWICImagingFactory* Factory();                        // shared WIC factory
    void RegisterCache(const std::shared_ptr<ImageDeviceCache>&);
    void ClearAllDeviceCaches();                          // clears every image's D2D bitmap cache on device loss
};

class ImageDeviceCache {                                  // per-render-target ID2D1Bitmap cache for one Image
    ID2D1Bitmap* Get(ID2D1RenderTarget*, IWICBitmapSource*);
    void Clear();
    void Clear(ID2D1RenderTarget*);
};
```

- Decoded pixels (`IWICBitmapSource`, 32bppPBGRA) are **device-independent**, one per `Image`; `ImageDeviceCache` caches one `ID2D1Bitmap` per render target (D2D bitmaps belong to the render target that created them).
- On device loss the framework calls `ClearAllDeviceCaches()` to rebuild them.

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

    // Loading
    static std::shared_ptr<Image> FromFile(const std::wstring& path);
    static std::shared_ptr<Image> FromMemory(const void* data, size_t size);
    static std::shared_ptr<Image> FromBase64(const std::string& base64);            // supports data: URI prefix and URL-safe
    static std::shared_ptr<Image> FromResource(HMODULE mod, const wchar_t* name, const wchar_t* type);
    static std::shared_ptr<Image> FromResource(int id, const wchar_t* type);        // current module
    static std::shared_ptr<Image> FromHBITMAP(HBITMAP);
    static std::shared_ptr<Image> FromHICON(HICON);
    HICON ToHICON() const;                              // to a system HICON (new object; DestroyIcon when done)
    bool CopyPixelsBgra(std::vector<uint8_t>& out, int& w, int& h) const;  // export 32bpp BGRA (straight alpha)

    // Transforms (lightweight descriptors applied by the GPU at draw time; share one decoded source)
    std::shared_ptr<Image> Scaled(float w, float h) const;
    std::shared_ptr<Image> ScaledToWidth(float w) const;
    std::shared_ptr<Image> ScaledToHeight(float h) const;
    std::shared_ptr<Image> Rotated(float degrees) const;
    std::shared_ptr<Image> Mirrored(bool horizontal = true, bool vertical = false) const;
    std::shared_ptr<Image> Cropped(const Rect& srcRect) const;

    // Drawing (GPU)
    void Draw(ID2D1RenderTarget* rt, const Rect& dst, const DrawOptions& = {}) const;
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& = {}) const;
    void Draw(ID2D1RenderTarget* rt, float x, float y, const DrawOptions& = {}) const;
    ComPtr<ID2D1Bitmap> Bake(ID2D1RenderTarget* rt) const;

    // Encoding / saving
    bool Save(const std::wstring& path, Format = Format::Png, float quality = 0.9f) const;
    std::vector<uint8_t> Encode(Format = Format::Png, float quality = 0.9f) const;
};
```

**Notes and pitfalls:**

- **Decode once, share**: `Scaled/Rotated/Mirrored/Cropped` return new `Image` objects sharing the same decoded source (lightweight descriptors); transforms are applied by the GPU at `Draw` time with no re-decoding and no per-pixel CPU work.
- **Device bitmap cache**: drawing the same `Image` repeatedly in one window uploads its GPU bitmap once; each window has its own.
- **Lifetime (root cause)**: memory-backed inputs (`FromMemory/FromBase64/FromResource(RT_BITMAP)`) use `IWICStream::InitializeFromMemory`, which does **not copy** the buffer, and frame decoding / format conversion are **lazy**; if pixels are not fetched at decode time, drawing reads freed memory (the image simply never appears). The implementation therefore uses `WICBitmapCacheOnLoad` inside `detail_img_MakeFromSource` to **copy the pixels into an independent WIC bitmap immediately**, fully decoupling from the source buffer.
- **Resources**: `type` may be `L"PNG"`/`L"IMAGE"`/`RT_RCDATA`/`RT_BITMAP`, etc.; `RT_BITMAP` (DIB) gets a BMP file header prepended automatically; the `HMODULE` overload can load **from a DLL's resources**.
- **Encoding**: `Encode` uses `CreateStreamOnHGlobal` (a growable memory stream, **no temp files**) and returns the encoded bytes; `Save` writes a file directly.
- **With `Label`**: use `Label::SetImage` + `SetIconSize` to show an icon (see "Basic controls -> Label").
- **Interop with system icons**: `ToHICON()` returns a system `HICON` (for `SetAppIcon` / `TrayIcon`); `CopyPixelsBgra()` exports 32bpp BGRA straight-alpha pixels (e.g. to write a `.ico`).
- **Transform matrix (root cause)**: `Rotated/Mirrored` pivot around the destination rect center. D2D matrix multiplication applies the **left operand first**, so the `Rotation/Scale` overloads that take a `center` must be used; otherwise the image is pushed out of the target rect (rotated/mirrored images appear missing).

###chapter: Window tools | TitleBar / MessageBox / Tray / Taskbar / Icons and activation

Window-level controls live in `ZufyUIWindowTool.h` (a collection that will later also host a built-in MessageBox, etc.).

```cpp
class TitleBar : public UIElement {
    void SetTitle(const std::wstring&);
    const std::wstring& GetTitle() const;
    void SetIcon(std::shared_ptr<Image>);        // reuses the image system
    void SetIconSize(float w, float h);
    void SetShowIcon(bool);
    void SetShowTitle(bool);
    void SetContentPadding(float left, float right = 8.0f);
    void SetBackgroundColor(Color);  void SetActiveBackgroundColor(Color);
    void SetTitleColor(Color);       void SetActiveTitleColor(Color);
    void SetButtonWidth(float);      // also SetButtonHeight / SetRightMargin
    void SetButtonHeight(float);
    void SetRightMargin(float);
    void ClearRightMargin();
    void SetButtonsEnabled(bool);
    bool AreButtonsEnabled() const;
    // Per-button control (detailed disable API)
    std::shared_ptr<CaptionButton> GetButton(CaptionButton::Kind) const;
    void SetButtonEnabled(CaptionButton::Kind, bool);   // gray out, ignore input, don't report the NC button code
    bool IsButtonEnabled(CaptionButton::Kind) const;
    void SetButtonVisible(CaptionButton::Kind, bool);   // hidden buttons take no space
    bool IsButtonVisible(CaptionButton::Kind) const;
};

class CaptionButton : public UIElement {
    enum class Kind { Minimize, MaximizeRestore, Close };
    explicit CaptionButton(Kind kind);
    ZSignal<> Clicked;              // default behaviour wired by DefaultTitleBar
    void SetHoverColor(Color);      void SetPressedColor(Color);
    void SetCloseHoverColor(Color); void SetClosePressedColor(Color);
    void SetGlyphColor(Color);
    void SetButtonWidth(float);
    void SetAnimationSpeed(float);
};

class DefaultTitleBar : public TitleBar {
    DefaultTitleBar();               // built-in minimize / maximize-restore / close
    std::shared_ptr<CaptionButton> GetMinButton() const;
    std::shared_ptr<CaptionButton> GetMaxButton() const;
    std::shared_ptr<CaptionButton> GetCloseButton() const;
    // convenience: enable/disable, show/hide the three buttons individually
    void SetMinimizeEnabled(bool);  void SetMaximizeEnabled(bool);  void SetCloseEnabled(bool);
    void SetMinimizeVisible(bool);  void SetMaximizeVisible(bool);  void SetCloseVisible(bool);
};
```

**Usage**:

```cpp
auto bar = std::make_shared<DefaultTitleBar>();
bar->SetTitle(L"My Window");
bar->SetIcon(myImage);                 // optional
win.SetCustomTitleBar(bar);            // install; win.SetCustomTitleBar(nullptr) restores the native one
```

**Notes and pitfalls**:

- The title bar does **not participate in layout** (Window places it at `(0,0)`), defaults to `DrawAfterLayout`, is **not focusable** (skipped by Tab) and has `UseCache()==true` (repaints only on state change).
- Buttons are drawn with the system icon font `Segoe Fluent Icons` (Win11) / `Segoe MDL2 Assets` (Win10), matching the system glyphs (minimize `E921`, maximize `E922`, restore `E923`, close `E8BB`); a vector fallback is used when the font is unavailable.
- Hover/press are animated; minimize/maximize hover light gray, **close hover red** (`#C42B1C`).
- Buttons report system hit codes (`HTMINBUTTON` / `HTMAXBUTTON` / `HTCLOSE`) from `WM_NCHITTEST`, so **hovering the maximize button shows the system Snap Layouts**; clicks are handled in `WM_NCLBUTTONUP`.
- The title bar's own rect (excluding buttons) is the window **drag region**; dragging / Aero Snap / double-click maximize are handled by the system.
- It is **not recommended** to `AddChild` the title bar into a layout (it does not participate); use `Window::SetCustomTitleBar`.

## MessageBox / FastButton

Preset message box built on `Window` / `DefaultTitleBar` / `Window::RunModal`. It *is* a `Window`, so you can add any controls to it.

```cpp
enum class FastButton : unsigned {   // bit flags, combine with |
    None = 0, OK = 1 << 0, Cancel = 1 << 1, Apply = 1 << 2,
    Close = 1 << 3, Yes = 1 << 4, No = 1 << 5, Help = 1 << 6
};
FastButton operator|(FastButton, FastButton);
FastButton operator&(FastButton, FastButton);
bool HasFlag(FastButton set, FastButton flag);      // bit test

class MessageBox : public Window {
    enum class Icon { None, Info, Warning, Error, Question };
    enum class Lang { English, Chinese };

    // Quick call: creates the window in the ctor; blocking=true runs the modal loop inside the ctor
    MessageBox(Window* parent, const std::wstring& title, const std::wstring& content,
               Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true);
    MessageBox(HWND parent, const std::wstring& title, const std::wstring& content, ...);
    MessageBox(const std::wstring& title, const std::wstring& content, ...);   // no parent
    // content can also be a control (Label / TextBox / GridLayout ...)
    MessageBox(Window* parent, const std::wstring& title, std::shared_ptr<UIElement> content, ...);

    static void SetLanguage(Lang);                          // preset EN/ZH button text, default English
    static Lang GetLanguage();
    static std::wstring ButtonLabel(FastButton);
    void SetButtonText(FastButton, const std::wstring&);    // override one button's text
    void SetIconImage(std::shared_ptr<Image>);              // custom icon (default: built-in system icon)
    void SetUserContent(std::shared_ptr<UIElement>);        // afterwards no preset buttons; fully user-controlled
    void EndDialog(FastButton);                             // end manually with custom content

    FastButton GetResult() const;
    ZSignal<FastButton> ButtonClicked;
};
```

```cpp
ZufyUI::MessageBox box(owner, L"Title", L"Body", MessageBox::Icon::Info,
                    FastButton::Yes | FastButton::No | FastButton::Cancel);
if (HasFlag(box.GetResult(), FastButton::Yes)) { /* ... */ }
```

- Button order (left to right): `Yes No OK Apply Cancel Close Help`; **Enter = leftmost button, ESC = Cancel/Close (leftmost if absent)**; buttons are right-aligned.
- `parent` may be a `Window*` / `HWND` / omitted; it is an owned window (no taskbar button), not resizable, close button only.
- Height auto-fits the content; a **truncated button shows its full text as a tooltip on hover** (a user `SetToolTip` wins).
- Customize with `SetUserContent` (no preset buttons afterwards) or the `shared_ptr<UIElement>` ctor; for full flow control build with `blocking=false` and call `RunModal` yourself.
- Note: `windows.h` defines `MessageBox` as a macro for `MessageBoxW`; this library `#undef`s it in `ZufyUIWindowTool.h`. Use `MessageBoxW/A` explicitly for Win32.

## Window icon / activation / flash

```cpp
class Window {
    // Unified app icon (native big/small + class icons + custom title bar)
    void SetAppIcon(HICON bigIcon, HICON smallIcon);
    void SetAppIcon(std::shared_ptr<Image> big, std::shared_ptr<Image> small = nullptr);  // auto-converts
    void SetAppIconFromResource(int bigId, int smallId = 0);

    enum class ActivateMode { Raise, Activate, Foreground, All };
    void ShowActivate(ActivateMode mode = ActivateMode::All);   // restores if minimized; Foreground/All call SetForegroundWindow
    void Raise();                                               // raise Z-order only

    void Flash(int times = 5, bool alsoTaskbar = true);         // default FLASHW_ALL
    void FlashUntilForeground();
    void StopFlash();
};
```

- `SetAppIcon(Image)` uses `Image::ToHICON()`; after setting, the taskbar / Alt-Tab / title bar icons are consistent (class icons included).
- `ShowActivate(All)` = restore (if minimized) + raise + `SetForegroundWindow` (uses `AttachThreadInput` for reliability).
- `Flash()` defaults to `FLASHW_ALL` (taskbar button + window caption).

## Tray icon (TrayIcon)

`Shell_NotifyIcon` wrapper (`NOTIFYICON_VERSION_4`): hover / click / right-click menu / badge / balloon.

```cpp
class TrayIcon {
    bool Add(HICON icon, const std::wstring& tooltip, UINT id = 1);
    bool Add(std::shared_ptr<Image> icon, const std::wstring& tooltip, UINT id = 1);   // auto-converts
    bool AddFromResource(int resId, const std::wstring& tooltip, UINT id = 1);
    bool AddFromFile(const std::wstring& icoPath, const std::wstring& tooltip, UINT id = 1);
    void Remove();
    bool IsAdded() const;

    void SetIcon(HICON);   void SetIcon(std::shared_ptr<Image>);
    void SetToolTip(const std::wstring&);
    void SetMenu(std::shared_ptr<Menu>);

    void SetBadge(Color color = Color::FromArgb(255, 220, 40, 40));   // bottom-right red dot
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

**Notes:**

- v4 callbacks: right-click arrives as `WM_CONTEXTMENU`; hover as `NIN_POPUPOPEN/CLOSE`; double-click is detected in `NIN_SELECT` via `GetDoubleClickTime()`.
- `Add` with the same id does `NIM_MODIFY`; re-added automatically after `explorer` restarts (`TaskbarCreated`).
- `BalloonIcon`: `None/Info/Warning/Error` (system icons) / `Custom` (uses `customIcon`, default = current tray icon); `realtime=true` (`NIF_REALTIME`); `noSound=true` mutes.
- ⚠️ When Win10/11 upgrades a balloon to a toast, the **top-left "app icon" comes from the registry keyed by the AUMID** (`hBalloonIcon` is ignored) → use `RegisterApp` to register the app identity so the app name and icon show.

## Taskbar (progress / overlay badge / thumbnail toolbar / jump list)

```cpp
class Window {
    enum class TaskbarProgress { None = 0, Indeterminate = 1, Normal = 2, Error = 4, Paused = 8 };
    void SetTaskbarProgress(TaskbarProgress state, ULONGLONG completed = 0, ULONGLONG total = 0);
    void SetTaskbarProgressValue(ULONGLONG completed, ULONGLONG total);
    void ClearTaskbarProgress();

    void SetTaskbarOverlayIcon(HICON, const std::wstring& description = L"");
    void SetTaskbarOverlayIcon(std::shared_ptr<Image>, const std::wstring& description = L"");
    void ClearTaskbarOverlayIcon();

    // Thumbnail toolbar (buttons shown when hovering the taskbar thumbnail); ThumbButtonId is a command id
    ZSignal<ThumbButtonId> ThumbButtonClicked;
    void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons, HIMAGELIST images);
    void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons,
                         const std::vector<std::shared_ptr<Image>>& images);   // auto-builds HIMAGELIST
    void UpdateThumbButton(ThumbButtonId id, bool enabled);

    // Jump list
    struct JumpListItem { std::wstring title, arguments, target, iconPath; int iconIndex = 0; };
    void SetJumpList(const std::vector<JumpListItem>& tasks = {},
                     const std::vector<std::pair<std::wstring, std::vector<JumpListItem>>>& categories = {},
                     bool includeRecent = false);
    void SetAppUserModelID(const std::wstring&);                 // this window's AUMID
    static void SetProcessAppUserModelID(const std::wstring&);   // process-level
};
```

**Notes:**

- Taskbar features depend on the **process/window AUMID**; prefer `RegisterApp` to register the identity once (jump list / grouping / taskbar all belong to it).
- The `Image` overload of `SetThumbButtons` auto-converts `Image -> HICON -> HIMAGELIST`; clicks are reported via `ThumbButtonClicked(id)`.
- `SetJumpList` supports `tasks` (user tasks) and `categories` (custom categories) + `includeRecent` (system "Recent").

## Window-level hooks (overridable)

```cpp
virtual bool OnWindowKeyDown(int vk);   // called before dispatching to the focused element; true = handled
virtual bool OnWindowTimer(int id);     // WM_TIMER
virtual void OnWindowSize();            // WM_SIZE (final layout that depends on client width)
void SetInputBlocked(bool on);          // block mouse/keyboard input for this window
```

###chapter: Appendix | Signal list, default values, and common pitfalls

## Signal list (updated)

| Class | Signals | Args |
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
| `TreeView` | `SelectionChanged` / `NodeClicked` / `ItemDoubleClicked` / `ItemRightClicked` / `HeaderClicked` / `SelectionChangedMulti` / `ExpandChanged` / `ItemCheckStateChanged` | see above |
| `FontManager` | `GlobalFontChanged` | — |
| `UIZSignals` | `DrawOverlay` / `GlobalMouseDown` / `WindowDeactivated` / `ElementCaptureRequest` / `ElementCaptureRelease` / `RepaintRequest` / `LayoutInvalidated` / `DeviceReset` | see Chapter 4 |

## Default values

| Item | Value |
| --- | --- |
| Global font | `Segoe UI` / `14.0f` |
| Default window backdrop | `Backdrop::None` / tint `0` |
| Root layout margin / spacing | `20` / `10` |
| `deltaTime` cap | `0.033s` |
| Window timer | `16ms`, `timeBeginPeriod(1)` |
| Shadow defaults | color `(0,0,0.02,0.42)`, `blur 10`, `offset (0,3)` |
| ToolTip hover delay | `0.5s` |
| Label | size `16`, `Ellipsis`, vertically centered |
| Button | `120×36`, radius `4`, blue/white |
| TextBox | `160×30`, size `14`, caret blink `0.5s` |
| ComboBox | `160×30`, item height `24` |
| ToggleSwitch | `50×24` |
| CheckBox | size `16`, radius `4` |
| ScrollViewer | bar width `8`, min length `20`, wheel step `30` |
| ProgressBar | `200×20` |
| Slider | `160×24`, range `[0,100]`, track `4`, thumb `14` |
| ListView | `200×200`, row height `28` |
| TableView | `400×300`, header `26`, default row `24`, min column `40` |
| TreeView | `400×300`, row height `24`, indent `16` |

## Common pitfalls

1. **Color components are `[0,1]`**: `Color(255,0,0)` is wrong; use `Color(1,0,0)` or `FromArgb(255,255,0,0)`.
2. **Capturing the host's `shared_ptr` in a signal slot** creates a reference cycle that leaks the whole subtree. Capture a raw pointer or `weak_ptr` (Chapter 4).
3. **Forgetting to repaint after changing state**: framework setters call `RequestRepaint()` for you; if you mutate internal data directly, call it yourself.
4. **Invisible elements**: `SetVisible(false)` releases the cache but keeps layout; the cache is rebuilt when shown again.
5. **Disabled ≠ hidden**: `SetEnabled(false)` keeps layout and draws grey; use `SetVisible(false)` to hide.
6. **Focus ring only on Tab**: mouse-click focus deliberately does not draw the ring.
7. **`deltaTime` is already clamped**: compute animations in seconds; you do not need to guard against huge `dt`.
8. **Data-view metadata after sorting**: this version binds metadata to items / remaps it, so it survives sorting; if you **reorder internal containers yourself**, maintain it manually.
9. **`PageHost::NavigateTo` is asynchronous**: `GetCurrentIndex()` changes only after the transition.
10. **`GetChildren()` returns a reference**: do not hold it while mutating the element's child list; calling it during traversal is safe.

## Complete example

```cpp
#include "ZDataViewer.h"
using namespace ZufyUI;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Window win;
    if (!win.Create(900, 600, L"ZufyUI Example")) return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

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

    auto list = std::make_shared<ListView>();
    list->SetFillHeight(true);
    list->SetSelectionMode(ListView::SelectionMode::Extended);
    list->SetCheckable(true);
    for (int i = 1; i <= 20; ++i)
        list->AddItem(L"Item " + std::to_wstring(i));
    list->SetItemDisabled(2, true);
    list->SetItemTextColor(3, Color::FromArgb(255, 200, 60, 60));
    list->SetItemToolTip(1, L"Tooltip for item 2");
    list->SetSortComparator([](const std::wstring& a, const std::wstring& b) { return a < b; });
    list->SetShowSortIndicator(true);
    list->SetSelectedIndex(0);
    root->AddChild(list);

    win.Show();          // since 1.8.0 Create no longer auto-shows; call Show()
    win.Run();
    return 0;
}
```
