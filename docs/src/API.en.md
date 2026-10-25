# ZufyUI API Reference

> This document covers the entire public API of the ZufyUI framework, **organized by usage category** (not by code/header order).
> Companion reading: [Project README & build](../../README.en.md).

> **How to read**: this is not just a signature list — each class first explains overall behavior, then each method's side effects, defaults and pitfalls. **You can use it correctly without reading the source.**

> **Two markers used throughout**:
> - **`Pitfall`**: real traps / counter-intuitive behavior — read these first.
> - **`Version`**: the release in which the API or its semantics changed notably (`added`/`changed`/`deprecated`). Current release is **v{{ZUFYUI_VERSION}}**.

###chapter: Conventions | Namespaces, units, lifetime, version & pitfall notation

## Namespaces and headers

- Everything is in `namespace ZufyUI`.
- Headers are **layered by usage**:
  - `ZufyUI.h`: core — types / signals / fonts / element base / layout / menus / window.
  - `ZufyUIAcrylic.h`: hand-written DComp composition effects + the official acrylic/mica recipes (included by `ZufyUI.h`).
  - `ZufyUIWidgets.h`: basic controls (Label/Button/TextBox/TextEdit/ComboBox/…/ScrollViewer/TabView/icons), includes `ZufyUI.h`.
  - `ZDataViewer.h`: data views (ListView/TableView/TreeView), depends on the two above.
  - `ZufyUICharts.h`: **charts (BarChart/LineChart/PieChart)**, depends on `ZufyUI.h`.
  - `ZufyUIImages.h`: `Image` / `ImageManager` (image loading and device cache).
  - `ZufyUIIcons.h`: the `FontIcon` glyph control (the `Icon` enum itself lives in `ZufyUIWidgets.h`).
  - `ZufyUIWindowTool.h`: window extras (title bar / MessageBox / tray / system dialogs / accessibility provider / debug channel); **since v1.20.0 it includes `ZufyUIWidgets.h` itself**, so it can be included on its own; it includes `ZufyUIHwndView.h` at the end.
  - `ZufyUIHwndView.h`: native HWND host `HwndView` (WebView2 / camera / third-party / out-of-process windows).
  - `ZufyUIDragDrop.h`: drag & drop (included at the end of `ZufyUI.h`).
  - `ZufyUIDsl.h`: optional **fluent construction DSL** (purely additive; changes no existing control).
- All are **header-only**; `#pragma comment(lib, ...)` auto-links `d2d1 / dwrite / dwmapi / imm32 / winmm`.
- **Source encoding**: `.h` / `.cpp` files containing non-ASCII (e.g. Chinese) must be saved as **UTF-8 with BOM** (otherwise MSVC decodes them with the local code page and reports `C2143/C2259/C2888/C4819`); `.md` / `.tex` are saved as **UTF-8 without BOM**. For a BOM-free / ASCII-only copy, use `tools/export_ascii.py` (output `dist/ascii`, not committed; see the README's "ASCII-only export").

## Units

- All coordinates and sizes are **DIP** (device-independent pixels), not physical pixels.
- The framework snaps to the physical pixel grid via `Snap()` before drawing; you do **not** do DPI math yourself.
- Mouse event coordinates are DIP too. Screen coordinates only appear in system APIs (`Create`, `ShowAt`, `FileDialog`).
- **`Pitfall`**: the `Snap` DPI scale is **thread-local** (`GlobalDpiScaleRef`). The render thread sets it every frame; the UI thread now sets it too in `Window::Create`/`WM_DPICHANGED`. On your own threads it will be 1.0, not the window DPI.

## Object model & lifetime

- Controls are owned by `std::shared_ptr<T>`, created with `std::make_shared<T>()`.
- Mount to a parent via the container's `AddChild()`; the parent holds the child's `shared_ptr` — **the child survives as long as the parent**.
- Detached with no other `shared_ptr`, the element is destroyed; destruction auto-releases its offscreen cache and registered connections (internal `ConnectionGroup`).
- **`Pitfall`**: `UIElement::SetParent` stores a **raw** (non-owning) pointer; ownership always lives in the container/`Application` `shared_ptr`. An element records its window only as a **window id**, to avoid UAF if the window is destroyed first.

## Signals & events

- Bind with `Connect(signal, slot)`, returns a `Connection`, auto-disconnected when the host element is destroyed.
- The slot may be any callable (lambda / function pointer / `std::function`).
- **`Pitfall`**: do not connect a lambda that captures the host's own `shared_ptr` by value to the host's own signal — reference cycle, the whole subtree leaks. See the "Events & Signals" chapter.

## Defaults

- Every control has `inline static` `Default*` fields (e.g. `Button::DefaultSize`).
- `static SetDefault*()` affects **only instances created afterwards**; instance setters affect only that instance.
- **`Pitfall`**: static defaults are **process-global**; for different looks across windows, set before creating, or use per-instance setters.

## Three cross-cutting concepts (all controls)

- **Enabled/disabled**: `UIElement::SetEnabled(false)` makes the element and subtree non-interactive (mouse/keyboard blocked); controls decide graying via `IsEffectivelyEnabled()`. **Disabled does not change layout** (still occupies space). `SetEnabled(false)` ≠ `SetVisible(false)`.
- **Focus**: only elements whose `IsFocusable()` returns true can take keyboard focus. Tab traversal moves focus; the focus ring shows only when focus comes from Tab, not mouse.
- **Offscreen cache**: see the "Element base" chapter. On by default; turn it off with `SetUseCache(false)` for frequently-animating / many-instance controls to save VRAM.

###chapter: Quick start | Minimal program, structure, reading path

## Minimal program

```cpp
#include "ZufyUI.h"
#include "ZufyUIWidgets.h"
using namespace ZufyUI;

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {   // Windows subsystem (/SUBSYSTEM:WINDOWS)
    Application app = Application::Instance();
    auto win = app.CreateWindow(900, 600, L"Hello ZufyUI");
    auto root = win->GetRootColumnBox();          // default root: ColumnBox, margin(20), spacing 10
    auto btn  = std::make_shared<Button>(L"Click me");
    btn->Connect(btn->Clicked, [] { /* ... */ });
    root->AddChild(btn);
    win->Show();                                  // Create does NOT auto-Show (since v1.8.0)
    return app.Run();
}
```
> **`Pitfall`**: GUI apps use `wWinMain` + `/SUBSYSTEM:WINDOWS`. If you insist on `int main()`, set `/SUBSYSTEM:CONSOLE`, or add `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")` — otherwise the linker won't find an entry point.

## Event loop & threading

- `Application::Run()` runs the single message loop; **all windows/controls must be created and touched on the same UI thread**.
- To touch UI from a worker: use a signal's `ConnectionThread::UIThread`, or `detail::PostToUIThread`.
- The render thread (`ZUFYUI_RENDER_THREAD`) is **off by default** and documented as unstable / not recommended.

## Structure (outer → inner)

`Application` (process) → `Window` (top-level, optionally owned/modal) → root `Layout` (default `ColumnBox`) → containers (`Card`/`PageHost`/`TabView`/`ScrollViewer`/data views/…) → controls.
Overlays (menus/dropdowns/tooltips/system dialogs) use separate windows or a global overlay — not the root tree.

## Suggested reading path

1. This chapter + "Conventions" (DIP / shared_ptr / signals / defaults).
2. UI: "Layout" + "Element base" + "Basic controls".
3. Data UI: "Data views".
4. Charts: "Charts".
5. Window extras (title bar/tray/dialogs/accessibility/debug): the matching chapter.

###chapter: Basic types | Color, Rect, Thickness, Size

## Color

```cpp
struct Color {
    float r, g, b, a;                 // components in [0,1], NOT 0..255
    Color(float r=0, float g=0, float b=0, float a=1.0f);
    static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    D2D1_COLOR_F ToD2D() const;
    static Color Lerp(const Color& c1, const Color& c2, float t);
};
```
- **`Pitfall`**: components are floats in **[0,1]**; `Color(255,0,0)` is wrong (over-bright). For integer input use `Color::FromArgb(255,255,0,0)`.
- **`Note`**: `Color::Lerp(a, b, t)` does **not** clamp `t`; keep it in 0..1 (out of range extrapolates).

## Rect

```cpp
struct Rect {
    float x, y, width, height;
    Rect(float x=0, float y=0, float w=0, float h=0);
    bool Contains(float px, float py) const;   // half-open: x<=px<x+w
    D2D1_RECT_F ToD2D() const;
};
```
- `Contains` is **half-open [left,right)**; hit-testing relies on it.

## Thickness

```cpp
struct Thickness { float left, top, right, bottom; Thickness(float l=0,float t=0,float r=0,float b=0); };
```

## Size

```cpp
struct Size { float width, height; Size(float w=0, float h=0); };
```

###chapter: Events & Signals | ZSignal, Connection, cross-thread

## When to use signals

- Built-in `ZSignal` for control notifications (click, selection, value change).
- Custom controls: declare your own `ZSignal<...>` members and `Fire` them.

## ConnectionThread

```cpp
enum class ConnectionThread { CurrentThread, NewThread, UIThread };
```
- `CurrentThread` (default): slot runs synchronously on the firing thread.
- `NewThread`: **spawns a detached thread per Fire** — do not touch UI inside; a running slot cannot be cancelled midway; **not suitable for high-frequency signals** — use `UIThread` or your own pool instead.
- `UIThread`: posts the slot to the UI thread (the correct way to update UI from workers); queued tasks check liveness and skip after `disconnect` / receiver destruction.
- **`Pitfall`**: `Fire` should run on the UI thread by default; firing from a worker with `CurrentThread` slots runs UI slots on that worker — that is a caller contract violation. For cross-thread firing, use `UIThread` slots or guarantee subscriber lifetime yourself.
- **`Pitfall`**: `NewThread` + a raw `this` capture + a receiver that may die early = UAF; use `ConnectWeak` to keep it alive, or `UIThread`.
- **`Version`**: cross-thread/locking semantics hardened in `v1.15.0`; queued tasks gained liveness checks in `v1.24.0`.

## Connection (Qt-style passive handle)

```cpp
class Connection {
    void disconnect();
    bool isConnected() const;
    explicit operator bool() const;   // destructor does NOT auto-disconnect
};
```
- Disconnected by the receiver's `ConnectionGroup` on destruction; **`Connection`'s own destructor does not**.
- `disconnect()` logically invalidates pending `UIThread` / `NewThread` tasks, but **does not interrupt a slot already running**.
- **`Pitfall`**: `Connection` is **not an RAII guard**; a temporary handle's destructor does not disconnect. Call `disconnect()` explicitly or rely on the receiver's group.
- **`Pitfall`**: it is safe to ignore the return of `connect`; calling `disconnect` after the signal is destroyed is safe (`alive` is atomic).
- **`Version`**: passive-handle semantics since `v1.8.1`; the alive flag became atomic and pending tasks are cancelled since `v1.24.0`.

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    void disconnectAll(); size_t size() const;
};
```
- Used internally by `UIElement` / `Window` to disconnect all connections they own.
- `size()` counts only live connections; manually disconnected connections are not counted.
- **`Pitfall`**: `disconnectAll()` only removes connections; it does not invoke user slots.

## ZSignal

```cpp
template<typename... TArgs> class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    template<class Receiver, class Slot>
    Connection connect(Receiver* receiver, Slot&& slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread);
    void Fire(TArgs... targs) const;
    void operator()(TArgs... targs) const;
};
```
- Fire with `Fire` (**there is no `emit` macro**).
- `ZSignal` is **not copyable or movable**; it is a member of controls/windows — never put it in a movable container.
- **`Pitfall`**: `Fire` iterates a **snapshot**, so a slot may safely `disconnect`; re-firing the same signal inside a slot goes through the "reentrant" path (local copy) — avoid deep recursion there.
- **`Pitfall`**: `Fire` concurrent with `ZSignal` destruction is UB; fire and destroy a signal on the same thread.
- **`Pitfall`**: an exception thrown by a slot propagates out of `Fire`; later slots do not run. Avoid throwing from signal slots.
- Signal arguments are passed to slots as **lvalues**; do not declare `T&&` slot parameters, and prefer `const T&` for large objects.
- **`Version`**: `v1.18.0` adds a RAII guard so a throwing slot still restores reentrancy depth and releases the snapshot; non-movable since `v1.24.0`.

## Receiver context and ConnectWeak

- `elem->Connect(sig, slot)`, `Connect(elem, sig, slot)` and `sig.connect(elem, slot)` register the connection on `elem`'s `ConnectionGroup` and disconnect when `elem` is destroyed; `Window` supports the same.
- `ConnectWeak(self, sig, slot)` holds a weak reference; the slot's first parameter is `T*` or `T&`, keeps `self` alive during execution, and does nothing if the object is gone.
- Member-function pointers go through the free `Connect` / `ConnectWeak` only (compile-time type check); `elem->Connect(sig, &Class::Method)` is no longer supported.

```cpp
auto panel = std::make_shared<Panel>();

panel->Connect(btn->Clicked, [this] { Refresh(); });
Connect(panel, btn->Clicked, &Panel::OnClick);
btn->Clicked.connect(panel, [this] { Refresh(); });

ConnectWeak(panel, btn->Clicked, [](Panel* self) { self->Refresh(); });
ConnectWeak(panel, btn->Clicked, &Panel::OnClick);
```
- **`Pitfall`**: do not capture `self`'s `shared_ptr` by value inside a `ConnectWeak` slot — that creates a signal → slot → self → signal strong cycle.
- **`Note`**: `Menu` / `MenuItem` / `Timer` still use `signal.connect` and do **not** auto-disconnect.

## Global signals UIZSignals

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
- **`Pitfall`**: `DrawOverlay` carries a `Window*`; subscribers **must** filter by `GetWindow()`, or window A's overlay draws onto window B (classic multi-window cross-talk).
- `RepaintRequest`/`LayoutInvalidated`/capture signals only fall back for **unmounted** elements; mounted ones route directly via `Window`.
- `DeviceReset`: release all caches keyed to the render target/device (e.g. `ImageDeviceCache`).

###chapter: Fonts & Icons | FontSpec, FontManager, Icon

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

## FontManager (singleton, text-layout cache)

```cpp
static FontManager& Instance();
IDWriteFactory* GetFactory();
IDWriteTextFormat* GetFormat(const FontSpec&);        // never evicted
ComPtr<IDWriteTextLayout> GetRawLayout(text, fmt, maxW, maxH, noWrap);
ComPtr<IDWriteTextLayout> GetDisplayLayout(origText, fmt, maxW, maxH, noWrap);   // ellipsized
ComPtr<IDWriteTextLayout> GetStyledLayout(text, fmt, w, h, noWrap, hAlign, vAlign, lineSpacing, maxLines);
ComPtr<IDWriteTextLayout> GetStyledDisplayLayout(...);
void CacheDisplayLayout(...);  void CacheStyledDisplayLayout(...);
void SetGlobalFont(const FontSpec&);
ZSignal<> GlobalFontChanged;
```
- **`Pitfall`**: returned layouts are **shared read-only**; do not call `SetTextAlignment/SetLineSpacing/SetTrimming` on them (the cache key would no longer match).
- The layout cache is **bounded** (400 entries, FIFO evicts ¼) and returns strong refs (`ComPtr`), safe during use.
- **`Pitfall`**: `GetFormat`'s `IDWriteTextFormat*` is **never evicted** — do not mutate it.
- **`Version`**: `GetStyledLayout`/global layout cache hardened in `v1.10.1`.

## Icon (font glyphs)

```cpp
enum class Icon : unsigned short { None=0x0000, Add=0xE710, Remove=0xE738, ..., Pin=0xE718, View=0xE890, ... };
inline const std::wstring& IconFontFamily();   // Segoe Fluent Icons → fallback Segoe MDL2 Assets
inline std::wstring IconGlyph(Icon);
```
- **`Pitfall`**: the enum value **is the font codepoint**. Font fallback is probed **once, statically**; runtime font changes are not re-probed; a few Win11-only glyphs render as a box.
- **`Version`**: `Icon` sunk into `ZufyUIWidgets.h` in `v1.13.0`; all `Label`-derived controls gained `SetIcon`.

###chapter: Element base | UIElement (layout props, cache, events, accessibility, debug)

## Layout properties

```cpp
virtual Size MeasureOverride(const Size& avail) = 0;
Size Measure(const Size& avail);   Size GetDesiredSize() const;
virtual void ArrangeOverride(const Rect&);   void Arrange(const Rect&);
Rect GetArrangedRect() const;
void InvalidateLayout();        // bubbles to root
void InvalidateLayoutSelf();    // self only (pool/virtual rebind), no bubble
void SetWidth/SetHeight(float);   float GetWidth/GetHeight() const;
void SetFillWidth/SetFillHeight(bool);   bool GetFillWidth/GetFillHeight() const;
void SetMargin(const Thickness&);
void SetMinWidth/SetMinHeight/SetMaxWidth/SetMaxHeight(float);
void SetMinSize/SetMaxSize(float,float);
void SetStretchWeights(float h, float v);
void SetHorizontalStretchWeight(float);   void SetVerticalStretchWeight(float);
virtual float GetDefaultHorizontalStretchWeight() const;   // and Vertical
```
- **Stretch weights**: containers (`ColumnBox`/`RowBox`/`GridLayout`) distribute **leftover** space by child weight. `FillWidth/Height` = "weight 1 on that axis". `GetDefault*` is the type default; `Set*` overrides it.
- **`Pitfall`**: `SetWidth/SetHeight` is a **fixed size**; `SetFillWidth/Height` **fills the parent axis**; they compose with stretch weights.
- **`Pitfall`**: `GetDesiredSize()` is the result of the last `Measure`; may be stale right after changing content.
- **`Version`**: `SetStretchWeights`/`GetDefault*` existed since the first release; **actual leftover distribution by explicit weights = `v1.17.0`**.

## Drawing / children / clipping / cache

```cpp
virtual void Draw(ID2D1RenderTarget* rt) = 0;
virtual void RefreshChildren();                              // update phase (UI thread)
virtual const std::vector<UIElement*>& GetChildren() const;  // render phase: pure read
virtual bool UseCache() const;   void SetUseCache(bool);     // default true
virtual std::optional<D2D1_RECT_F> GetClipRect() const;
void SetClipRect(const std::optional<Rect>&);
void SetBleed(float);
virtual void ReleaseDeviceResources();       // self only
void ReleaseDeviceResourcesRecursive();      // whole subtree
virtual D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const;
```
- **Cache**: `UseCache()==true` renders into a per-element offscreen `ID2D1Bitmap1`; rebuilt only when `RequestRepaint()`-marked or size changes. Shadows add `bleed`.
- **`Pitfall`**: controls whose content **changes every frame** (animation, live text, spinners) or with **many instances** (thousands) should `SetUseCache(false)`, or VRAM/rebuild cost explodes.
- **`Pitfall`**: `GetChildren()` must be pure-read at render time; `RefreshChildren()` must have run first (the window does this in `AdvanceFrame`).
- **`Pitfall`**: `ReleaseDeviceResources()` releases **self only**; use the recursive variant for subtrees.
- **`Version`**: `ReleaseDeviceResourcesRecursive()` = `v1.18.0`; `SetUseCache(false)` now immediately frees an existing bitmap.

## Visibility / enabled / tooltip / cursor

```cpp
void SetVisible(bool);  void SetVisibleNoInvalidate(bool);  bool IsVisible() const;
void SetEnabled(bool);  bool IsEnabled() const;  bool IsEffectivelyEnabled() const;
void SetToolTip(const std::wstring&);   virtual std::wstring GetToolTip() const;
void SetCursor(HCURSOR);   HCURSOR GetDesiredCursor() const;
```
- Disabled elements are excluded from hit-test/hover. Tooltips **walk up** to parents.
- **`Pitfall`**: `SetCursor` does not call `::SetCursor` directly; it defers to `WM_SETCURSOR` to avoid flicker.

## Context menu / drag regions / layout participation

```cpp
void SetContextMenu(std::shared_ptr<Menu>);
void SetContextMenuFactory(std::function<std::shared_ptr<Menu>()>);
std::shared_ptr<Menu> BuildContextMenu() const;
enum class LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout };
void SetLayoutParticipation(LayoutParticipation);
void SetDraggable(bool);  void SetDragRegion(const Rect&);  void ClearDragRegion();
virtual int NonClientHitTest(float x, float y) const;
```
- **`Pitfall`**: `DrawBeforeLayout`/`DrawAfterLayout` are drawn by separate channels — do **not** also place them in the root tree (double draw). The custom title bar is `DrawBeforeLayout` and manages its own position.

## Event virtuals & element signals

```cpp
virtual UIElement* HitTest(float x, float y);
virtual void OnMouseEnter/Leave/Move/Down/Up(...);  virtual bool OnContextMenu(float,float);
virtual void OnKeyDown/Up(WPARAM,LPARAM);  virtual void OnChar(wchar_t);
virtual void OnFocus()/OnBlur();
virtual void UpdateAnimation(float dt);  virtual bool HasActiveAnimation() const;
virtual bool OnMouseWheel(float dx, float dy);
virtual bool IsTextInput() const;  virtual bool AcceptsTab() const;  virtual bool IsFocusable() const;
virtual Rect GetImeCandidateRect() const;  virtual void SetCompositionText(...);
// element signals
ZSignal<> MouseEnter, MouseLeave, Focused, Blurred;
ZSignal<float,float> MouseMove, MouseDown, MouseUp;
ZSignal<WPARAM,LPARAM> KeyDown, KeyUp;   ZSignal<wchar_t> Char;
```
- **`Pitfall`**: animated controls must make `HasActiveAnimation()` correct, or the frame loop stops and the animation freezes.
- **`Pitfall`**: a multi-line editor must return true from `AcceptsTab()` to consume Tab for indent; otherwise Tab moves focus.
- **`Version`**: `AcceptsTab()` = `v1.18.0`.

## Accessibility (UIA) — v1.17.0

```cpp
void SetAccessibleName(const std::wstring&);  std::wstring GetAccessibleName() const;
void SetAccessibleDescription(const std::wstring&);
void SetAutomationId(const std::wstring&);     std::wstring GetAutomationIdOrAuto() const;   // auto e{n}
void SetAccessibleRole(AccessibleRole);  virtual AccessibleRole GetAccessibleRole() const;
virtual std::wstring DefaultAccessibleName() const;  virtual bool IsAccessibilityIgnored() const;
std::vector<UIElement*> GetAccessibleChildren() const;
// Pattern virtuals (dispatched by role)
virtual void AccessibilityInvoke();
virtual std::wstring GetAccessibleValue() const;  virtual void SetAccessibleValue(const std::wstring&);
virtual bool IsAccessibleReadOnly() const;
virtual int  GetAccessibleToggleState() const;   virtual void AccessibilityToggle();
virtual int  GetAccessibleExpandState() const;   virtual void AccessibilityExpand/Collapse();
virtual double GetAccessibleRangeValue() const;  virtual void SetAccessibleRangeValue(double);
virtual double GetAccessibleRangeMin/Max/Step() const;
void AccessibilityNotifyFocus/PropertyChanged/StructureChanged();
```
- **`Pitfall`**: the role decides which UIA Patterns are exposed (Button/MenuItem→Invoke; Edit/Document→Value; Slider/ProgressBar→RangeValue; toggle state ≥0→Toggle; expand state ≥0→ExpandCollapse). Hidden nodes are not exposed by default.

## Fonts (element-level)

```cpp
static void SetGlobalFont(const FontSpec&);  void SetFont(const FontSpec&);
void SetFontFamily/SetFontSize/SetFontWeight(...);  void ClearFont();
virtual std::optional<FontSpec> GetTypeDefaultFont() const;  FontSpec GetEffectiveFontSpec() const;
```
- Resolution chain: **instance override → type default → global**.
- **`Pitfall`**: writing `fontOverride_` (contains `std::wstring`) must be serialized with rendering; `StoreFontOverride`/`InvalidateFontCache` take `renderLock_` for mounted elements.

## Shadow

```cpp
void SetShadow(bool);  bool HasShadow() const;
void SetShadowColor(Color); SetShadowBlur(float); SetShadowOffset(float,float); SetShadowCornerRadius(float);
float GetShadowExtent() const;
```
- Shadows only apply to **cached** elements (`Card` enables cache when a shadow is set).

## Debug counters (per element)

`DebugMeasureCount/ArrangeCount/RepaintCount/DrawCount/CacheValid/CacheBytes/MeasureMs/ArrangeMs/DrawMs/AnimationProgress/ResetCounters`, etc. — **`Version`**: `v1.17.0`, used by the debug channel Top-N.

###chapter: Layout system | Layout, ColumnBox, RowBox, GridLayout, Card, Page, PageHost

## Layout (base)

```cpp
class Layout : public UIElement { bool UseCache() const override { return false; } };
```
- Layout containers **never cache**.

## ColumnBox (vertical) / RowBox (horizontal)

```cpp
ColumnBox(); RowBox();
void AddChild(std::shared_ptr<UIElement>);
void ClearChildren();
void SetSpacing(float);
```
- `ColumnBox` fills width, no vertical stretch; `RowBox` the opposite.
- **`Pitfall`**: `AddChild` calls `SetParent(this)`; the container owns via `shared_ptr`. If a child sets an explicit `SetHeight`, leftover space is **added on top** by weight.
- **`Version`**: leftover distribution by child stretch weight since `v1.17.0`.

## GridLayout

```cpp
enum class Alignment { Start, Center, End };
void AddChild(std::shared_ptr<UIElement>, int row, int col, int rowSpan=1, int colSpan=1);
void SetSpacing(float h, float v);
void SetColumnStretch(int col, float weight);   void SetRowStretch(int row, float weight);
void SetHorizontalAlignment(Alignment);         void SetVerticalAlignment(Alignment);
```
- Without explicit row/col stretch, the cell weight is the **max** of spanned children's weights.

## LayoutHost / Card / Page

```cpp
class LayoutHost : public UIElement { std::shared_ptr<UIElement> GetLayout() const; void SetLayout(...); };
class Card : public LayoutHost { DefaultPadding=12; DefaultCornerRadius=8; ... };
class Page : public LayoutHost { DefaultPadding=10; ... };
void Page::SetScrollableContent(std::shared_ptr<UIElement> content);   // one line: wrap in ScrollViewer(fill)+8px
```
- `LayoutHost` is a single-slot container; `Card` is a rounded/bordered/hover card; `Page` is the page container for `TabView`/`PageHost`.
- `Card::UseCache()` = `HasShadow()`; `Page::UseCache()` = false.

## PageHost (page host / transitions)

```cpp
enum class TransitionDirection { Left, Right, Up, Down };
enum class TransitionEasing { Linear, EaseInOut, EaseOut };
void AddPage(std::shared_ptr<Page>);  void RemovePage(int);  void ClearPages();
void NavigateTo(int index);   void SetCurrentIndexInstant(int index);
void SetTransitionDirection(...);  void SetTransitionEasing(...);  void SetAnimationDuration(float);
int GetCurrentIndex() const;  std::shared_ptr<Page> GetCurrentPage() const;
```
- During a transition it returns **both** source and target pages as visible children; `UpdateAnimation` updates each page only once (otherwise double speed).
- **`Pitfall`**: `NavigateTo` is **asynchronous** (switches after the animation); `GetCurrentPage()`/`GetCurrentIndex()` may still be old during it.
- **`Version`**: easing `v1.8.1`; **delayed hidden-page cache release (~1.2 s, recursive)** = `v1.18.0`; `MeasureOverride` **measures every page** = `v1.23.1` — page content shown/hidden at runtime (e.g. an `Expander` card revealed after a combo-box selection) re-lays out and hit-tests immediately, with no need to resize the window.
- **`Pitfall`**: show/hide page content with `SetVisible()` (it calls `InvalidateLayout()`); do not poke the internal visibility flag, or it will be neither re-measured nor repainted.

## TabView

See the "Containers & Scrolling / Tabs" chapter.

###chapter: Window | Window (create, show, backdrop, title bar, taskbar, DPI, render thread)

## Nested public types

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

## Static configuration

```cpp
inline static Backdrop DefaultBackdrop = Backdrop::None;
inline static DWORD     DefaultBackdropColor = 0xFFFFFFFF;   // pure white default, v1.17.0
static void SetDefaultBackdrop(Backdrop, DWORD tint = 0);
static void SetBackgroundParams(Backdrop which, const BackgroundParams&);
static void SetBackgroundParams(Backdrop which, AcrylicPreset);
static void SetDefaultFrameRateLimit(int fps);   // v1.14.2
static void SetDefaultAppIcon(HICON big, HICON small = nullptr);
static void SetProcessAppUserModelID(const std::wstring& id);
```

## Creation / lifecycle

```cpp
bool Create(int w, int h, const std::wstring& title, int x=CW_USEDEFAULT, int y=CW_USEDEFAULT);
bool IsValid() const;  HWND GetHwnd() const;  int GetId() const;
void Show(); void ShowNoActivate(); void Hide(); void Raise(); void Close();
void Run();  int RunModal(Window* owner = nullptr);
void SetInputBlocked(bool);  bool IsInputBlocked() const;
std::unique_lock<std::recursive_mutex> LockRender();  void RenderNowSync();
```
- **`Pitfall`**: `Create` does **not** auto-`Show` (since v1.8.0). `RunModal` disables the owner (`EnableWindow(FALSE)`) and deliberately does **not** use `IsDialogMessage`.
- **`Version`**: multi-window `v1.6.0`; `SetInputBlocked`/modal `v1.6.2`; `LockRender`/`RenderNowSync` `v1.16.0`.

## Three ways to create a window (which to pick)

| Way | Code | Ownership | Message loop | Use for |
|---|---|---|---|---|
| Stack value | `Window w; w.Create(...); w.Show(); app.Run();` | You (scope) | shared `Run()` | single main window, simple lifetime |
| `shared_ptr` | `auto w = app.CreateWindow(...); w->Show(); app.Run();` | `shared_ptr` refcount | shared `Run()` | multiple windows, dynamic create/destroy |
| Modal | `int r = w->RunModal(owner);` | same as `shared_ptr` | **nested** loop, blocks until closed | dialogs / confirmations |

- **`Pitfall`**: `Window` is **non-copyable**; the stack form must **outlive `Run()`** (otherwise the loop still references a destroyed window).
- **`Pitfall`**: the `shared_ptr` from `Application::CreateWindow` **is the ownership** — dropping it destroys the window; `Create` also does not auto-`Show`.

## Ownership / multi-window / size

```cpp
void SetOwner(Window*);  Window* GetOwner() const;
std::vector<Window*> GetOwnedWindows() const;
void HideOwnedWindows(); void ShowOwnedWindows();
void SetPosition(int x, int y);  void SetSize(int w, int h);   // DIP
void SetMinSize(int w, int h);
void SetOwnedMinimizePolicy(OwnedMinimizePolicy);
```
- **`Pitfall`**: set `SetOwner` **before** `Create`, or the window gets its own taskbar button and does not minimize with the owner.
- **`Pitfall`**: `SetMinSize` **overrides** the content-based minimum. Without it, the minimum can be blown up by long text/charts (internally `rootElement->Measure(Size(0,0))`; with width 0 a wrapping Label reports its full single-line width). **Chart-bearing debugger windows set `SetMinSize` explicitly.**

## Root content / capture

```cpp
void SetRootLayout(std::shared_ptr<Layout>);
std::shared_ptr<ColumnBox> GetRootColumnBox() const;   // default root: ColumnBox margin(20) spacing 10
void SetContextMenu(std::shared_ptr<Menu>);  void CloseContextMenu();
UIElement* HitTestElement(float x, float y);
void CollectDragRegions();  bool PointInDragRegion(float,float) const;
void MarkRepaint(UIElement*);   // marks only; does NOT schedule a frame
float GetDpiScale() const;  float GetClientWidthDip/HeightDip() const;
```
- **`Pitfall`**: `RequestRepaint()` → `MarkRepaint` does **not** schedule a frame. Frames come from the 16 ms `WM_TIMER` (when busy) or animation continuation. Historical note: forcing a frame here once caused a continuous re-render loop (severe stutter) — **reverted**.

## Appearance / backdrop / corners

```cpp
void SetBackdrop(Backdrop, DWORD tint=0);  Backdrop GetBackdrop() const;
void SetBackgroundColor(Color);
void SetWindowCorner(WindowCorner);
void SetContentOpacity(float opacity);
void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border);
void SetResizable(bool);   void SetTitle(const std::wstring&);
```
- Backdrop is **hand-rolled** (Acrylic = host backdrop + effect chain; Mica = cached wallpaper layer). Change params via `SetBackgroundParams` or `UIZSignals::ReloadAcrylic`.
- **`Version`**: four-layer params `v1.8.1`; `SetContentOpacity` `v1.9.6`.

## Custom title bar

```cpp
void SetCustomTitleBar(std::shared_ptr<UIElement>);  std::shared_ptr<UIElement> GetCustomTitleBar() const;
bool HasCustomTitleBar() const;   void SetTitleBarVisible(bool);
CaptionMetrics QueryCaptionMetrics() const;
void Minimize(); void Maximize(); void Restore(); void MaximizeRestore();
bool IsMaximizedWindow() const;   void BeginSystemDrag();
// top menu bar / bottom status bar (window chrome, v1.19.2)
void SetMenuBar(std::shared_ptr<UIElement>);    std::shared_ptr<UIElement> GetMenuBar() const;
void SetStatusBar(std::shared_ptr<UIElement>);  std::shared_ptr<UIElement> GetStatusBar() const;
void SetMenuBarInTitleBar(bool on = true);      bool IsMenuBarInTitleBar() const;
void SetShowTitleText(bool on);                 bool IsShowTitleText() const;
```
- **`Pitfall`**: the custom title bar **does not participate in layout** — `Window` places it at `(0,0)` and shifts the root down by its measured height. Do not `AddChild` it manually.
- The menu bar is **overlaid on the custom title bar by default** (leaving room for the left icon and the right caption buttons); the title text is then **hidden by default** (they are mutually exclusive, to avoid overlap). `SetShowTitleText(true)` shows the title and moves the menu bar to the **top of the client area below the bar**; `SetMenuBarInTitleBar(false)` forces it to the client-area top. The status bar is pinned to the bottom and shrinks the root content height.
- **`Version`**: `v1.7.0`; `CaptionButton::Kind::Pin` = `v1.17.0`.

## Taskbar / thumbnails / jump list

```cpp
void SetTaskbarProgress(TaskbarProgress, ULONGLONG done=0, ULONGLONG total=0);  void ClearTaskbarProgress();
void SetTaskbarOverlayIcon(HICON, const std::wstring& = L"");  void ClearTaskbarOverlayIcon();
ZSignal<ThumbButtonId> ThumbButtonClicked;
void SetThumbButtons(...);  void UpdateThumbButton(ThumbButtonId, bool enabled);
void SetJumpList(...);  void Flash(int times=5, bool alsoTaskbar=true);  void FlashUntilForeground();
void SetAppUserModelID(const std::wstring&);
```
- These are COM calls; failures are **silent no-ops**. `FLASHW_CAPTION` does nothing for a custom title bar; use `FLASHW_ALL`.
- **`Version`**: all `v1.9.6`.

## DPI

- Per-Monitor-v2 awareness set at static init (fallback `SetProcessDPIAware`).
- `WM_DPICHANGED` rebuilds the whole render chain and clears all caches.
- **`Pitfall`**: element caches clear on DPI change, but child layout needs **no** manual re-measure.

## Render thread / frame rate

```cpp
void SetFrameRateLimit(int fps);   int GetFrameRateLimit() const;   // 0=follow vblank; >0=cap
```
- **`Pitfall`**: `ZUFYUI_RENDER_THREAD` is **0 by default** and documented as unstable/not recommended (blank popups, delayed hover/tooltip, occasional crashes with large virtual lists). Single-threaded `WM_PAINT` is the supported path.
- **`Version`**: render thread `v1.14.0`; vblank clock `v1.14.1`; frame cap `v1.14.2`; default-off documented `v1.16.0`.

## Accessibility / signals

```cpp
void SetAccessibilityEnabled(bool);   bool IsAccessibilityEnabled() const;   // on by default, v1.17.0
UIElement* GetFocusedElement/HoveredElement/PressedElement() const;
void FocusElement(UIElement*);
ZSignal<> Activated, Deactivated;   ZSignal<bool*> Closing;   ZSignal<> Closed;
ZSignal<> BackdropUnsupported, DeviceLost;   ZSignal<HRESULT> RenderingError;
std::shared_ptr<Timer> CreateTimer(int intervalMs = 1000);
```
- **`Pitfall`**: in `Closing`, set `*cancel = true` to veto the close. The parameter is `bool*` (not a reference).

###chapter: Basic controls | Label, Button, TextBox, TextEdit, ComboBox

> Shared pitfalls: text layout is globally cached by `(text, box, format)`; change internal strings via setters (or you bypass invalidation); `Color` components are [0,1]; `SetEnabled(false)` ≠ `SetVisible(false)`.

## Label

```cpp
enum class TextOverflow { Wrap, Ellipsis };
enum class HAlign { Left, Center, Right };   enum class VAlign { Top, Center, Bottom };
Label(const std::wstring& text = L"Label");
void SetText(const std::wstring&);   std::wstring GetText() const;
void SetTextFast(const std::wstring&);   // pool rebind only: self-dirty, no bubble, no repaint
void SetTextColor(Color);   void SetTextOverflow(TextOverflow);
void SetAlignment(HAlign, VAlign);   void SetPadding(const Thickness&);
void SetBackgroundColor(Color);  void SetBackgroundCornerRadius(float);
void SetLineSpacing(float);  void SetMaxLines(int);   // only meaningful with Wrap
void SetImage(std::shared_ptr<Image>);   void SetIcon(Icon, float size=0);
void AddChild(std::shared_ptr<UIElement>);
std::wstring GetToolTip() const override;   // auto full-text tooltip when ellipsized (v1.17.0)
```
- **`Pitfall`**: `SetTextFast` does not repaint — pooled containers (ListView/TableView) use it plus their own repaint. Direct use may look "not updated".
- **`Pitfall`**: when both image and glyph are set, **image wins**.
- **`Version`**: `SetIcon` family `v1.13.0`; auto tooltip `v1.17.0`.

## Button

```cpp
ZSignal<> Clicked;   ZSignal<bool> Toggled;     // Toggled needs SetCheckable(true)
ZSignal<int> DropSelected;   ZSignal<> DropMenuOpened;   // selected index / menu opened
Button(const std::wstring& text = L"Button");
void SetText/SetIcon/SetIconColor(...);
void SetColors(Color normal, Color hover, Color pressed);   void SetTextColor(Color);
void SetCornerRadius(float);   void SetHoverAnimationSpeed(float);
void SetCheckable(bool);  void SetChecked(bool);  bool IsChecked() const;
void SetAutoRepeat(bool enable, float intervalMs=400);
void SetAutoSizeToText(bool);   inline static bool AutoSizeToText;   // default true: grow to fit text
std::shared_ptr<Label> GetLabel();  void SetContent(std::shared_ptr<UIElement>);  void ClearContent();
void SetDropWriteBack(bool);  void SetDropOpenOnClick(bool);   // two independent switches (2x2)
void SetDropItems(const std::vector<std::wstring>&, int selected=-1);  void SetDropMenu(std::shared_ptr<Menu>);  void ShowDropMenu();
```
- Default `120×36`; `Clicked` fires whenever press+release are both inside the button (**click always calls back**).
- **Rich content**: the internal `Label` is a real child now, so `GetLabel()->AddChild(...)` children are laid out/composited/drawn; `SetContent` replaces it with any `UIElement`.
- **Drop 2x2**: `SetDropWriteBack` controls whether a menu selection writes its text back to the button; `SetDropOpenOnClick` controls whether the button opens the menu after `Clicked`. The menu is **single-select** (the previous choice is marked); selecting fires `DropSelected(index)`, readable via `GetDropSelectedIndex/GetDropSelectedText`. `SetDropItems` is the text-list shortcut; use `SetDropMenu` for custom menus.
- **Main area / expand zone**: clicking the main area always fires `Clicked`; the right expand zone (separated by a short, theme-coloured divider) **only opens the menu and never fires `Clicked`**; clicking a menu item fires only `DropSelected`, never `Clicked`. With `DropOpenOnClick=false` the main area only calls back; the expand zone still opens the menu.
- **`Pitfall`**: `IsChecked()` is meaningless unless checkable; `SetContent` lays out inside the rect left after `padding` and the right arrow.
- **`Version`**: since `v1.21.0` a Button **grows to fit its own text by default**; `v1.23.3` adds rich content and the drop 2x2 combinations.

## TextBox (single-line)

```cpp
ZSignal<const std::wstring&> TextChanged;   ZSignal<> ReturnPressed;
void SetText(const std::wstring&);   // strips \r\n, truncates to maxLength, clears undo
void SetPlaceholder(...);  void SetPasswordMode(bool);  void SetMaxLength(int);   // -1 = unlimited
void SetReadOnly(bool);    void SetInputFilter(std::function<bool(wchar_t)>);
void SetError(bool);   void SetImeEnabled(bool);        // v1.12.0
void SelectAll/Copy/Cut/Paste/Undo/Redo();
int GetSelectionStart/End/CursorPosition() const;  void SetSelection(int,int);
```
- **`Pitfall`**: `SetImeEnabled(false)` suppresses IME/on-screen keyboard (`IsTextInput()` returns `imeEnabled_`).
- **`Pitfall`**: programmatic `SetText` does **not** create an undo step.
- **`Version`**: error/indicator/IME `v1.12.0`; `v1.18.0` fixes shift+click semantics.

## TextEdit (multi-line editor / viewer) — v1.18.0

```cpp
enum class WrapMode { NoWrap, WidgetWidth };
ZSignal<const std::wstring&> TextChanged;   ZSignal<int,int> CursorPositionChanged;  ZSignal<> SelectionChanged;
std::wstring ToPlainText() const;  void SetPlainText(const std::wstring&);   // clears undo
void AppendPlainText(...);  void InsertPlainText(...);
void SetRichText(const std::vector<std::vector<TextRun>>&);   // forces read-only; TextRun{ text, font, color, underline }
void SetReadOnly(bool);  void SetShowLineNumbers(bool);  void SetShowNewlines(bool);
void SetWrapMode(WrapMode);  void SetTabSize(int);  void SetInsertSpaces(bool);
int GetCursorLine/Column() const;  void SetCursorLineColumn(int,int);   // 0-based
void SelectLine(int);  void SelectWordAt(int);  bool Find(const std::wstring&, bool fwd=true, bool matchCase=false);
void EnsureCursorVisible();  float GetScrollOffsetY() const;  void SetScrollOffsetY(float);
bool UseCache() const override { return false; }
bool AcceptsTab() const override { return !readOnly_; }
```
- **`Pitfall`**: changing wrap width **rebuilds every line layout**. Rich text is **read-only**. Line/column APIs are **0-based**. Because `AcceptsTab` is true, Tab **inserts an indent** (not focus navigation).
- Default `Consolas 14`, `320×160`, tabSize 4.

Minimal example:
```cpp
auto edit = std::make_shared<TextEdit>();
edit->SetPlainText(L"hello\nworld");
edit->SetShowLineNumbers(true);
root->AddChild(edit);                 // give it a size or let the layout fill it
```

## ComboBox

```cpp
ZSignal<int> SelectionChanged;   ZSignal<> DropDownOpened, DropDownClosed;
void AddItem/SetItems/InsertItem/RemoveItemAt/ClearItems(...);
void SetSelectedIndex(int);  int GetSelectedIndex() const;             // filtered/visible index
void SetSelectedSourceIndex(int);  int GetSelectedSourceIndex() const; // v1.15.0: source index
void SetItemDisabled(int visibleIndex, bool);   // persists by source index
void SetEditable(bool);  void SetFilterEnabled(bool);  void ApplyFilter();
```
- **`Pitfall`**: the dropdown draws on the global `DrawOverlay` and may exceed the control's bounds.
- **`Pitfall`**: `GetSelectedIndex()` is the **filtered/visible** index; `SetItemDisabled` persists by **source** index (v1.15.0).
- **`Version`**: dropdown geometry/state `v1.12.0`; source-index API `v1.15.0`; the dropdown arrow rotates with expansion progress (smoothstep, `v1.19.0`).

###chapter: Selection & input | CheckBox, RadioButton, RadioGroup, ToggleSwitch, Slider, NumberBox

> Several of these emit a "live" signal during the drag and a separate "commit" signal. The static `DrawBox` is shared with data views, so changing its look changes list/table/tree check rows too.

## CheckBox

```cpp
enum class State { Unchecked, PartiallyChecked, Checked };
ZSignal<bool> Toggled;   ZSignal<State> StateChanged;
void SetChecked(bool);  void SetState(State);  void Toggle();  void SetTriState(bool);
static void DrawBox(...);
```

## RadioButton / RadioGroup

```cpp
class RadioButton { ZSignal<bool> CheckedChanged; ZSignal<> Clicked; void SetChecked(bool); };
class RadioGroup  { enum class Orientation { Vertical, Horizontal };
                    ZSignal<int> SelectionChanged;
                    std::function<bool(int newIdx, int oldIdx)> SelectionChanging;   // false vetoes
                    int AddItem(const std::wstring&);   int AddItemSelected(const std::wstring&, bool selected);
                    int AddButton(std::shared_ptr<RadioButton>);   int AddButtonSelected(std::shared_ptr<RadioButton>, bool selected);
                    std::shared_ptr<RadioButton> GetButton(int index) const; int GetItemCount() const; void SetSelectedIndex(int); ... };
```
- **`Pitfall`**: a lone `RadioButton` does not enforce exclusion — `RadioGroup` does.
- **`Version`**: `v1.11.0`.

## ToggleSwitch

```cpp
ZSignal<bool> Toggled;   void SetOn(bool);  bool IsOn() const;
void SetIndeterminate(bool);   // half-selected appearance
inline static float DefaultAnimationDuration;   // full animation duration (s), default 0.25
```
- Default 50×24.
- **`Version`**: since `v1.21.0` the knob/track/hover animation is **duration-driven ease-in-out** (`DefaultAnimationDuration`, default 0.25s) and stops exactly on time instead of repainting forever; before it was linear (`animSpeed_=9.6` finished 0→1 in ~0.10s, which read as an instant snap). `SetAnimationSpeed` still means "larger = faster" (converted to a duration multiplier).

## Slider

```cpp
ZSignal<float> ValueChanged;   ZSignal<> SliderReleased;
void SetRange(float min,float max);  void SetValue(float);  void SetStep(float);  void SetSnapToStep(bool);
```
- **`Pitfall`**: `ValueChanged` fires **continuously** during a drag; commit with `SliderReleased`.
- **`Version`**: `v1.18.0` fixes the zero-width NaN path.

## NumberBox (with internal SpinButton)

```cpp
ZSignal<double> ValueChanged;   ZSignal<const std::wstring&> TextChanged;
void SetValue(double, bool fire=true);  void SetRange(double lo,double hi);
void SetStep(double);  void SetDecimals(int);  void SetWrap(bool);
void SetDefaultValue(double);  void ResetToDefault();
```
- **`Pitfall`**: **empty text is treated as an error** by the control; step buttons call `CommitText()` first, so an unfocused edited value is not lost.
- **`Version`**: `v1.12.0`; `ResetToDefault` error-clear `v1.12.2`.

###chapter: Progress | ProgressBar, ProgressRing

## ProgressBar

```cpp
ZSignal<float> ValueChanged;
void SetValue(float);          // normalized to [0,1]
void SetRange(float min,float max);  void SetRangeValue(float);   // [min,max]
void SetIndeterminate(bool);   void SetIndeterminateBlockWidth(float);  void SetIndeterminateSpeed(float);
inline static float DefaultValueAnimDuration;   // value animation duration (s), default 0.22
```
- **`Pitfall`**: `SetValue` is `[0,1]`, `SetRangeValue` is `[min,max]` — do not mix.
- **`Version`**: since `v1.21.0` value changes are **eased** (internal `displayValue_`, duration-driven; the fill and the `%` text move together and stop on time). The same version fixes the **inner fill radius not matching the outer frame after stretching** (track/clip/border used the member `height_`; now all use `arrangedRect_.height / 2`).

## ProgressRing

```cpp
void SetValue(float);  void SetIndeterminate(bool);  void SetThickness(float);
bool UseCache() const override { return false; }
```
- **`Version`**: `v1.12.0`; `UseCache=false` `v1.12.2`; NaN/Inf guard `v1.14.0`.

###chapter: Containers & scrolling | ScrollViewer, ScrollBar, SplitView, TabView

## ScrollViewer

```cpp
enum class ScrollBarVisibility { Auto, Always, Hidden };
ZSignal<float,float> ScrollChanged;
void SetContent(std::shared_ptr<UIElement>);
void SetVertical/HorizontalScrollEnabled(bool);
void SetVertical/HorizontalScrollBarVisibility(ScrollBarVisibility);
void SetScrollBarWidth(float);  void SetScrollWheelStep(float);  void SetAnimationSpeed(float);
void ScrollTo(float x,float y,bool animated=true);  void ScrollBy(...);
float GetScrollOffsetX/Y() const;
```
- **`Pitfall`**: `Hidden` still allows programmatic/wheel scrolling.
- **`Version`**: `v1.18.0` — `ScrollViewer::UseCache()==false`.

## ScrollBar (standalone, reusable)

```cpp
std::function<void(float value, bool animate)> ValueChanged;
explicit ScrollBar(bool vertical);
void SetRange(float value, float maxValue, float viewportSize);   // host pushes each frame
void SetBarWidth(float);  void SetMinLength(float);  void SetHitExtra(float);
void SetColors(D2D1_COLOR_F thumb, D2D1_COLOR_F hoverThumb, D2D1_COLOR_F track);
void SetIdleDelay(float);  void SetAutoShrink(bool);  void MarkActive();
inline static float ShrunkWidthRatio;   // default 0.30
```
- Drag → `animate=false`; track click → `animate=true`. Hover expands, idle shrinks; hosts reserve gutter space via `ShrunkWidthRatio`.
- **`Pitfall`**: `ValueChanged` is a `std::function` (not `ZSignal`), signature `(float, bool)`.
- **`Pitfall`**: the idle-shrink countdown is **not** counted as an active animation — otherwise it put the whole window into animation mode and stopped data repaints from landing. Cost: idle shrink is **lazy** (triggered by the next frame).
- **`Version`**: reusable since `v1.11.0`; `ShrunkWidthRatio` `v1.13.0`.

## SplitView

```cpp
enum class Orientation { Vertical, Horizontal };   // Vertical = left/right panes + vertical splitter
ZSignal<float> SplitChanged;
void SetFirst/SetSecond(std::shared_ptr<UIElement>);
void SetSplitRatio(float);  void SetSplitterWidth(float);  void SetMinFirst/MinSecond(float);
ZSignal<int> CollapsedChanged;   // 0=expanded 1=first pane 2=second pane
void SetCollapseEnabled(bool);   bool IsCollapseEnabled() const;   void SetCollapseThreshold(float);
void CollapseFirst/CollapseSecond();   void Expand();   bool IsFirstCollapsed/IsSecondCollapsed() const;
void SetExpandButtonColors(Color,Color,Color);   void SetExpandButtonSize(float w,float h);
bool UseCache() const override { return false; }
```
- **`Pitfall`**: `Orientation` naming is counter-intuitive (`Vertical` = vertical splitter, horizontal arrangement).
- **Collapse (`v1.21.0`)**: drag the splitter to within `CollapseThreshold` (default 28px) of one side's edge → a translucent accent **overlay plus an arrow pointing in the collapse direction fades in ON TOP OF that pane's content** (the content is *not* changed yet); the pane actually collapses **on mouse release** (its size eases from the current value to 0). Once collapsed, a small button appears at that edge (rounded light grey + arrow pointing in the expand direction + hover easing); clicking it eases that side from 0 to its **minimum limit** (`SetMinFirst/MinSecond`, or back to the pre-collapse ratio if no minimum is set). The whole feature can be turned off with `SetCollapseEnabled(false)`.
- **`Pitfall`**: the overlay is an internal `Overlay` **child element placed last** (the framework draws children in order), which is why it naturally ends up above both panes; do not try to paint such decorations from `SplitView::Draw` (the children would cover them).
- **`Version`**: `v1.12.0`; NaN/Inf guard `v1.14.0`; collapse + expand button `v1.21.0`.

## TabView

```cpp
struct Tab { std::shared_ptr<Label> label; std::shared_ptr<Page> page; bool closable; ... };
ZSignal<int> SelectionChanged;   ZSignal<int> TabCloseRequested;
int  AddTab(const std::wstring&, std::shared_ptr<UIElement> content=nullptr, bool closable=false);
int  AddTab(std::shared_ptr<Label>, ...);
void InsertTab(int, ...);  void RemoveTab(int);  void ClearTabs();
void SetTabTitle(int, const std::wstring&);  void SetTabLabel(int, std::shared_ptr<Label>);  void SetTabContent(int, ...);
void SetSelectedIndex(int);  int GetSelectedIndex() const;
void SetTransitionDirection(PageHost::TransitionDirection);  void SetAutoTransitionDirection(bool);
```
- **`Pitfall`**: content is hosted by an internal `PageHost`; `AddTab` auto-selects the first tab and raises `SelectionChanged`.
- **`Pitfall`**: `SetSelectedIndex`/switching is **asynchronous** (settles after the transition); `GetSelectedContent()`/index may be stale during it.
- **`Version`**: `v1.11.0`; Label-based titles + `SetTabLabel` `v1.13.0`; locking `v1.15.0`.

###chapter: Menus & Popups | Menu, MenuItem, MenuWindow, MessageBox

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
void AddCheckItem(const std::wstring& text, bool checked);
void AddCheckItemCb(const std::wstring& text, bool checked, std::function<void(bool)> onToggle);
void AddCheckItemRadio(const std::wstring& text, bool checked, std::function<void(bool)> onToggle);
void AddSeparator();   void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu, int id=0);
std::vector<std::shared_ptr<MenuItem>> items;
std::function<void(Menu&)> onOpening;   ZSignal<int> ItemSelected;
std::function<void()> onClosed;   // fired after the popup closes
void ShowAt(int screenX, int screenY);   void ShowAtCursor();   // standalone popup (tray / right-click)
```
- **`Pitfall`**: item callbacks are deferred via `PostToUIThread` (they often open modal dialogs; closing the menu on the current WndProc stack would cause UAF).

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
- Popups use `WS_EX_NOACTIVATE|TOOLWINDOW|TOPMOST`, no DWM frame, return `MA_NOACTIVATE` so they never steal focus.
- **`Pitfall`**: `CloseAll()` destroys HWNDs only; the object `shared_ptr`/`unique_ptr` destructs outside the message handler.

## MenuBar / StatusBar (v1.19.2)

```cpp
class MenuBar : public UIElement {
    int AddMenu(const std::wstring& title, std::shared_ptr<Menu> menu = nullptr);   // auto-created if omitted
    std::shared_ptr<Menu> GetMenu(int i) const;   int MenuCount() const;   void CloseMenu();
    void SetFont(const FontSpec&);   void SetTextColor(Color);   void SetHoverColor(Color);   void SetOpenColor(Color);
};
class StatusBar : public UIElement {
    int AddPanel(const std::wstring& text, bool rightAlign = false);
    int AddPanel(Icon, const std::wstring& text, bool rightAlign = false);
    void SetPanelText(int i, const std::wstring&);   void SetPanelIcon(int i, Icon);   int PanelCount() const;
    void SetFont(const FontSpec&);   void SetColors(Color bg, Color border, Color text);
};
```
- `MenuBar` opens the entry's `Menu` on click (reusing the menu popup); hovering only highlights, it does **not** auto-expand; the open entry stays "pressed" until the menu closes.
- Attach to a window with `Window::SetMenuBar` / `SetStatusBar` (see "Custom title bar").

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
- Enter = leftmost; ESC = Cancel/Close (else leftmost).
- **`Pitfall`**: `ZufyUIWindowTool.h` `#undef`s the Win32 `MessageBox` macro — call `MessageBoxW/A` for Win32.
- **`Pitfall`**: `blocking=true` runs the modal loop **inside the constructor**; after `SetUserContent` there are no preset buttons, so you must `EndDialog`.

###chapter: Data views | ListView, TableView, TreeView, TreeNode (virtualization, hide/filter/visible order)

> **Read this first.** Since **v1.15.0** all three views are **viewport-virtualized + Label-pooled**. There are **two coordinate systems** — **source row index** (stable; legacy APIs/signals) and **visible order** (post hide/filter/sort). **Mixing them is the #1 bug source.** In virtual mode rows have no persistent `Label`; `SetTextFast` is the pool rebind path. Serialize data mutation with rendering (`RenderGuard()`).

## ListView

```cpp
enum class SelectionMode { Single, Extended, Multi, None };
ZSignal<int> SelectionChanged, ItemClicked, ItemDoubleClicked, ItemRightClicked;
ZSignal<std::vector<int>> SelectionChangedMulti;   ZSignal<int,bool> ItemCheckStateChanged;
float ContentToLocalX/Y(float) const;   float LocalToContentX/Y(float) const;   // v1.10.0
void AddItem(const std::wstring&);   void AddItemWidget(std::shared_ptr<Label> rich);   void SetItem(int, const std::wstring&);
std::wstring GetItemText(int) const;   int GetItemCount() const;
void SortItems(std::function<bool(const std::wstring&,const std::wstring&)>);   void ScrollToItem(int);
void BeginUpdate();   void EndUpdate();
// virtualization (v1.15.0)
void SetItemCount(int n);
void SetItemHidden(int src, bool=true);   bool IsItemHidden(int src) const;   void ClearHidden();
void SetFilter(std::function<bool(int)>);  void ClearFilter();
void SetViewComparator(std::function<bool(int,int)>);  void SortView(bool ascending=true);  void ClearViewSort();
int  VisibleCount() const;   int SourceOfVisible(int vi) const;   int VisibleOfSource(int src) const;
std::wstring TextAtVisible(int vi) const;   void SetSelectedVisible(int vi);   int GetSelectedVisible() const;
// check / per-item metadata
void SetCheckable(bool);  void SetItemChecked(int src,bool);  std::vector<int> GetCheckedIndices() const;
void SetItemDisabled(int src,bool=true);  void SetItemTextColor(int src,Color);  void SetItemToolTip(int src,const std::wstring&);
```
- **`Pitfall`**: `SortItems()` rearranges `items_` and **clears multi-selection/checks**; `SortView` only changes **visible order**.
- **`Pitfall`**: pooled text uses `SetTextFast`, and the container must re-measure on width change (`v1.18.0` adds `poolW_` to re-measure on width change, fixing stale ellipsis).
- **`Version`**: `BeginUpdate` `v1.9.6`; coords `v1.10.0`; virtualization/hide/filter/visible order `v1.15.0`.

## TableView

```cpp
enum class SelectionMode { Cell, Row, Column, None };
static long long CellKey(int r,int c); static int CellKeyRow(long long); static int CellKeyCol(long long);
ZSignal<int,int> CellClicked, CellDoubleClicked, CurrentCellChanged, CellRightClicked;
ZSignal<int> HeaderClicked;  ZSignal<std::vector<std::pair<int,int>>> SelectionChangedCells;
void SetRowCount(int);  void SetColumnCount(int);  void AppendRow/Column(); InsertRow/Column(int); RemoveRow/Column(int);
void SetItem(int row,int col,const std::wstring&);  std::wstring GetItemText(int r,int c) const;
void SetHeaderLabel(int,const std::wstring&);  void SetColumnWidth(int,float);   // min 40
void SetColumnAlignment(int, TextHAlign);   void SetRowHeight(float);  void SetRowHeightAt(int,float);
void SetColumnVisible(int,bool);
// virtualization (v1.15.0; all by source row): SetItemHidden/SetFilter/SetViewComparator/SortView/VisibleCount/SourceOfVisible/VisibleOfSource
// selection / check / metadata
void SetSelectionMode(SelectionMode);  void SetCurrentCell(int,int);  void SelectRow(int);  void GetSelectedCells();
void SetCheckable(bool);  void SetRowChecked(int,bool);  void SetRowDisabled(int,bool);
void SetCellTextColor(int,int,Color);   void SetCellToolTip(int,int,const std::wstring&);
void SetColumnComparator(int, std::function<bool(const std::wstring&,const std::wstring&)>);  void SortByColumn(int,bool=true);
```
- **`Pitfall`**: default `Cell` selection; row/cell metadata **remaps** with insert/remove/sort; hidden columns don't hit-test.

## TreeNode (plain struct)

```cpp
enum class CheckState { Unchecked, PartiallyChecked, Checked };
std::vector<std::shared_ptr<Label>> columns;   // one real Label per column; col0 = node text
std::weak_ptr<TreeNode> parent;                // weak! use .lock()
std::vector<std::shared_ptr<TreeNode>> children;
bool expanded;  int depth;  void* userData;
std::wstring icon;  bool checkable;  CheckState checkState;  bool selected;  bool enabled;
std::wstring tooltip;   Color bgColor = Color(0,0,0,0);   // row background, v1.17.0
```
- **`Pitfall`**: `parent` is `weak_ptr`; row background is the **`node->bgColor`** field (**no** `SetNodeBackgroundColor`).
- **`Version`**: `bgColor` `v1.17.0`.

## TreeView

```cpp
enum class SelectionMode { Single, Extended, Multi };   enum class CheckMode { Linked, Independent };
ZSignal<std::shared_ptr<TreeNode>> SelectionChanged, NodeClicked, ItemDoubleClicked, ItemRightClicked;
ZSignal<std::shared_ptr<TreeNode>,bool> ExpandChanged;   ZSignal<int> HeaderClicked;
ZSignal<std::shared_ptr<TreeNode>, TreeNode::CheckState> ItemCheckStateChanged;
void SetColumnCount(int);  void SetHeaderLabels(const std::vector<std::wstring>&);  void SetColumnWidth(int,float);
std::shared_ptr<TreeNode> AddRoot(const std::wstring&);   std::shared_ptr<TreeNode> AddChild(parent, const std::wstring&);
void RemoveNode(node);  void RemoveChildren(node);  void MoveNode(node, newParent, int index);  void Clear();
void BeginUpdate();  void EndUpdate();                       // v1.15.0: defer BuildVisibleList (avoids O(N^2))
void SetNodeText(node,int col,const std::wstring&);   void SetNodeTooltip(node, ...);   void SetNodeIcon(node, ...);
void ExpandNode(node,bool);  void ExpandAll();  void CollapseAll();  void ExpandToDepth(int);
void SetDefaultExpandDepth(int);                             // affects only future inserts
void SetFilter(std::function<bool(const std::shared_ptr<TreeNode>&)>);   void Search(const std::wstring&);
void SetSelectionMode(SelectionMode);  void SetSelectedNode(node);  std::vector<std::shared_ptr<TreeNode>> GetSelectedNodes() const;
void SetCheckable(bool enable);  void SetCheckableShallow(bool enable);  void SetCheckMode(CheckMode);
void SetNodeCheckable(node, bool);   void SetNodeCheckableRecursive(node, bool);
void SetNodeEnabled(node, bool);     void SetNodeEnabledRecursive(node, bool);
void SetNodeCheckState(node, TreeNode::CheckState);
void SetNodeCheckStateFull(node, TreeNode::CheckState, bool updateChildren, bool updateParent);
float GetScrollOffsetY() const;   void SetScrollOffsetY(float);   void ScrollToNode(node);   // v1.17.0
```
- **`Pitfall`**: `Search` keeps a hit node **plus its ancestor chain**; `SetDefaultExpandDepth` affects only **future** inserts; `CheckMode::Linked` auto-computes `PartiallyChecked`.
- **`Version`**: `BeginUpdate`/`SetNodeText`/tooltip + concurrency `v1.15.0`; first-column ellipsis fix `v1.16.0`; `Get/SetScrollOffsetY` + `ScrollToNode` + `bgColor` `v1.17.0`.

###chapter: Charts | ChartBase, BarChart, LineChart, PieChart (v1.18.0)

> Header `ZufyUICharts.h`. `ChartBase` centralizes plot geometry, "nice" ticks, category axis, grid, legend, palette, internal scrollbars, sticky axes, hover/hit, Ctrl-zoom, drag-pan, entrance animation; `BarChart`/`LineChart` implement only `DrawData`.

Minimal example:
```cpp
#include "ZufyUICharts.h"
auto chart = std::make_shared<BarChart>();
chart->AddSeries(L"Sales");
chart->AddCategory(L"Q1"); chart->AddCategory(L"Q2"); chart->AddCategory(L"Q3");
chart->SetSeriesValues(0, { 12.0, 30.0, 22.0 });
chart->SetWidth(420); chart->SetHeight(260);   // size comes from the chart itself — set it or give a layout weight
root->AddChild(chart);
```

## ChartBase

```cpp
enum class AText { Left, Center, Right };   enum class AVert { Top, Center };
struct Series { std::wstring name; std::vector<double> values; Color color; };
inline static std::vector<Color> DefaultPalette;   // 8 colors
ZSignal<int,int> PointClicked;    // (series, category)
ZSignal<int> CategoryClicked;     // (category)

// data
void AddCategory(const std::wstring&);
void AddSeries(const std::wstring& name, const std::vector<double>& v = {});
void SetValue(int s, int c, double);
void SetSeriesValues(int s, const std::vector<double>&);      // resets + replays entrance animation
void UpdateSeriesValues(int s, const std::vector<double>&);   // repaint only, no animation (live)
void ClearData();
int CategoryCount() const;   int SeriesCount() const;

// config
void SetPalette(const std::vector<Color>&);   void SetSeriesColor(int s, Color);
void SetValueAxisRange(double min, double max);   void SetValueAxisAutoRange();
void SetValueTickCount(int n);   void SetValueTickStep(double);
void SetValueFormat(int decimals, const std::wstring& prefix=L"", const std::wstring& suffix=L"");
void SetShowGrid(bool);  void SetShowVGrid(bool);  void SetShowLegend(bool);  void SetShowValues(bool);
void SetShowCategoryLabels(bool);  void SetLabelFont(const FontSpec&);   void SetColors(Color axis, Color grid, Color label);
void SetStickyAxes(bool);   // sticky axes (on by default)
void SetAnimationEnabled(bool);   void SetAnimationDuration(float);
void SetCategoryWidth(float);   void SetPlotHeight(float);   void SetFitPlotSize(bool);
void SetReferenceLine(double value, const std::wstring& label=L"", Color c=...);   void ClearReferenceLine();
void SetVReferenceLine(int category, const std::wstring& label=L"", Color c=...);  void ClearVReferenceLine();

// extension points
virtual void DrawData(ID2D1RenderTarget*) = 0;
virtual void ComputeValueRange(double& lo, double& hi);
virtual void UpdateHover(float,float);   virtual void OnDataClick(float,float);
virtual std::wstring HoveredText() const;
```
- **`Version`**: `SetFillWidth/Height(true)` fills the parent axis when the parent gives a constraint; with no constraint (`avail=FLT_MAX`) and no explicit size it **falls back to the default `420×260`** (since v1.19.4; no longer collapses to 0). Charts default to weight 0 — use `SetFillWidth/Height(true)` or a stretch weight to fill the layout.
- **`Pitfall`**: a custom `ChartBase` subclass **must call `InitScrollBars()`** (as BarChart/LineChart do) or the internal scrollbars stay null.
- **`Pitfall`**: for live data use `UpdateSeriesValues` (no animation replay), not `SetSeriesValues`.
- **`Pitfall`**: wheel = Ctrl-zoom (0.4..3) / Shift-horizontal scroll / else vertical; drag pans only after > 4 DIP. Area-fill baseline is clamped into the visible axis range so auto-ranged charts don't flood the plot.

## BarChart

```cpp
enum class Orientation { Vertical, Horizontal };   // Horizontal implemented v1.18.0
enum class StackMode { Grouped, Stacked, PercentStacked, Overlapped };
void SetOrientation(Orientation);  void SetStackMode(StackMode);
void SetBarGap(float);  void SetStackBarRatio(float);  void SetGroupGap(float);
void SetCornerRadius(float);  void SetBarOutline(float w, Color=...);
void SetBarColor(int s, int c, Color);  void SetSelectedCategory(int c);
```

## LineChart

```cpp
enum class Marker { None, Circle, Square };
void SetLineWidth(float);  void SetSmooth(bool);  void SetDashed(bool);
void SetShowMarkers(bool);  void SetMarkerSize(float);  void SetMarker(Marker);
void SetAreaFill(bool on, float alpha=0.25f);
```

## PieChart (standalone UIElement)

```cpp
enum class LabelMode { None, Percent, Value, LabelAndPercent };
ZSignal<int> SliceClicked;
void AddSlice(const std::wstring& label, double value, Color = auto);
void Clear();   int SliceCount() const;
void SetDonut(float ratio);   // 0=pie, 0.5=donut
void SetStartAngle(float deg);   void SetClockwise(bool);   void SetSliceGap(float deg);
void SetLabelMode(LabelMode);   void SetShowLegend(bool);
void SetZoom(float);   float GetZoom() const;   // v1.18.0: wheel zoom 0.4..4
bool OnMouseWheel(float,float) override;         // draw radius and hit-test share zoom_
void SetAnimationEnabled(bool);
```
- **`Pitfall`**: `PieChart` default stretch weight is 0 — needs an explicit size or layout weight.
- **`Pitfall`**: labels use leader lines — on the right half, text is left-aligned just after the leader end; on the left half, right-aligned just before it.

###chapter: Window extras | Images, tray, system dialogs, app registration

## Image / ImageManager / ImageDeviceCache

```cpp
class Image { int Width() const; int Height() const; bool IsNull() const; ... };
class ImageDeviceCache { /* device bitmaps keyed by render target */ };
```
- **`Pitfall`**: `ImageDeviceCache::Get` does "GPU upload outside the lock + `emplace` inside". Only with `ZUFYUI_RENDER_THREAD=1` is there a tiny dangling window vs. `DeviceReset`; single-threaded is safe.

## TrayIcon

```cpp
bool Add(HICON icon, const std::wstring& tooltip, UINT id=1);
void SetIcon(HICON);  void SetToolTip(const std::wstring&);  void SetMenu(std::shared_ptr<Menu>);
void SetBadge(Color = ...);   void ClearBadge();
enum class BalloonIcon : DWORD { None, Info, Warning, Error, Custom };
void ShowBalloon(const std::wstring& title, const std::wstring& text, BalloonIcon = BalloonIcon::Custom, HICON = nullptr, ...);
ZSignal<> Clicked, DoubleClicked, RightClicked, HoverEnter, HoverLeave, BalloonClicked, ...;
```
- Same `id` re-`Add` becomes `NIM_MODIFY`; auto re-adds after Explorer restart.
- **`Pitfall`**: with NOTIFYICON_VERSION_4 the right-click arrives as `WM_CONTEXTMENU` inside the callback and **double-click must be timed manually**. The custom callback message is `WM_APP+0x400`.
- **`Version`**: `v1.16.0` fixed callback-message collision.

## System dialogs — FileDialog / ColorDialog (v1.16.0)

```cpp
struct FileFilter { std::wstring label; std::vector<std::wstring> patterns; };
struct FileDialogOptions {
    std::wstring title, initialDir, defaultFileName, defaultExtension;
    std::vector<FileFilter> filters; int filterIndex = 1;
    bool addAllFiles = true, pickFolders = false, multiSelect = false, save = false, forceFilesystem = true;
};
```
- Based on `IFileDialog`/`ChooseColor`; `forceFilesystem=true` forces a filesystem path.

## App registration — AppInfo / RegisterApp

```cpp
struct AppInfo { std::wstring displayName, aumid; std::shared_ptr<Image> icon; };
bool RegisterApp(const AppInfo&);    // or Application::Instance().RegisterApp(info)
```
- By default only sets the AUMID (zero file/registry side effects); defining `ZUFYUI_ALLOW_APP_REGISTRATION` writes `%LOCALAPPDATA%\ZufyUI\AppReg\...` and `HKCU\...\AppUserModelId` (so Win10/11 toasts show the app name/icon); auto-cleans on exit.

## Native window host — `HwndView` (v1.23.0)

> Header `ZufyUIHwndView.h` (auto-included at the end of `ZufyUIWindowTool.h`). Embeds a WebView2 HWND controller, camera/capture window, third-party native control, or an out-of-process window into the ZufyUI layout.

```cpp
class HwndView : public UIElement {
public:
    explicit HwndView(HWND owner, bool activate = false);
    HWND GetHostHwnd() const;
    void SetContentHwnd(HWND external, bool own = false);
    HWND GetContentHwnd() const;
    void SetObscured(bool on);
    bool IsObscured() const;
    void SetOnBoundsChanged(std::function<void(HWND, int, int)>);
};
```

- Implementation: creates a **normal Win32 top-level owned popup** as the host; `SetContentHwnd` re-parents the external HWND as its `WS_CHILD` (cross-thread/cross-process allowed) and shows it explicitly. Moving/resizing the host moves the child natively; the host client clips it, plus `SetWindowRgn` (element rect ∩ window client ∩ every ancestor `GetClipRect`, incl. the `ScrollViewer` viewport).
- **`Cross-process risk`**: cross-process `SetParent` is technically legal but not recommended: it couples the input queues (one stuck process can freeze the other), blocks some window messages, can be intermittently unstable, and the target process may not know its window was re-parented. **Prefer same-process**; use cross-process only as a last resort.
- **`Pitfall`**: the main window is `WS_EX_NOREDIRECTIONBITMAP + DComp`, so **child HWNDs are not composited**; do not use a normal child control. `HwndView` is an **airspace overlay** — always above the window content; in-window dropdowns/tooltips cannot cover it (menus are separate top-level windows and are unaffected).
- **`Convention`**: for "content goes into composition, no airspace" (web/video) use `CreateExternalVisual` below / WebView2 `CompositionController`.

## External visual attachment — `Window::CreateExternalVisual` (v1.23.0)

For WebView2 CompositionController, video/capture, or anything that **renders into DComp**.

```cpp
IUnknown* Window::CreateExternalVisual();
void Window::SetExternalVisualBounds(IUnknown* visual, float x, float y, float w, float h);
void Window::RemoveExternalVisual(IUnknown* visual);
int  Window::AddOverlaySync(std::function<void()>);
void Window::RemoveOverlaySync(int id);
void Window::SyncOverlays();
```

- Returns a WinRT `ContainerVisual` (inserted at the top of the root visual) to pass to `controller->put_RootVisualTarget(visual)`; set its position/size in `Arrange` via `SetExternalVisualBounds` (DIP, converted to physical pixels internally).
- `AddOverlaySync/RemoveOverlaySync/SyncOverlays` let external-window hosts (e.g. `HwndView`) register a sync callback; usually not called directly.
- **`Pitfall`**: on device loss/rebuild the visual tree is recreated → listen to `Window::DeviceLost`, call `CreateExternalVisual` again and re-attach.


###chapter: Drag & Drop, cards | Drag & Drop, Expander, SettingsList (v1.19.0)

> Drag & drop comes from `ZufyUIDragDrop.h` (already included at the end of `ZufyUI.h`). In-app default is MOVE, cross-app is COPY.

```cpp
// ---- drag & drop: payload / events ----
class DragData {                                   // read-only for the drop target
    bool HasText() const;   std::wstring GetText() const;
    bool HasFiles() const;  std::vector<std::wstring> GetFiles() const;
    bool HasFormat(const std::wstring& fmt) const;
    bool GetFormatData(const std::wstring& fmt, std::vector<BYTE>& out) const;
};
struct DragEventArgs { const DragData& data; float x, y; DWORD allowed, effect, keyState; };   // x/y are element-relative DIP

// ---- drag & drop: source side ----
class DragDataBuilder {
    DragDataBuilder& AddText(const std::wstring&);
    DragDataBuilder& AddFile(const std::wstring& path);
    DragDataBuilder& AddFiles(const std::vector<std::wstring>& paths);
    DragDataBuilder& AddCustom(const std::wstring& fmt, const void* bytes, size_t n);
    DragDataBuilder& SetPreferredEffect(DWORD);
};
DWORD Window::BeginDrag(DragDataBuilder&, DWORD allowed = COPY|MOVE|LINK, UIElement* source = nullptr);

// ---- drag & drop: target side (element) ----
void UIElement::SetDropTargetEnabled(bool);   bool IsDropTargetEnabled() const;
DWORD UIElement::GetAllowedDropEffects() const;                   // default COPY|MOVE|LINK
DWORD UIElement::OnDragEnter/OnDragOver(DragEventArgs&);         // returns effect
void  UIElement::OnDragLeave(DragEventArgs&);   DWORD UIElement::OnDrop(DragEventArgs&);
void UIElement::SetDragSource(std::function<void(DragDataBuilder&)>, DWORD allowed = COPY|MOVE|LINK);
ZSignal<DragEventArgs&> DragEnter, DragOver, DragLeave, Drop;

// ---- drag & drop: whole-window (when no drop-enabled element is under the cursor) ----
void Window::SetDropTargetEnabled(bool);   bool Window::IsDropTargetEnabled() const;
ZSignal<DragEventArgs&> Window::DragEnter, DragOver, DragLeave, Drop;   // x/y = client-area DIP

// ---- Expander: collapsible card ----
class Expander : public UIElement {
    Expander(const std::wstring& title = L"", const std::wstring& subtitle = L"");
    void SetTitle/SetSubtitle(const std::wstring&);   void SetContent(std::shared_ptr<UIElement>);
    std::shared_ptr<UIElement> GetContent() const;
    void SetExpanded(bool);   bool IsExpanded() const;   void Toggle();
    void SetIcon(Icon, float size = 22.0f);   // leading icon (None = none)
    ZSignal<bool> ExpandedChanged;
};

// ---- SettingsList: settings row list ----
class SettingsList : public UIElement {
    void AddRow(Icon, const std::wstring& text, const std::wstring& subtitle, std::function<void()> onClick, const std::wstring& id = {});
    ZSignal<int> RowClicked;
};
```
- **`Pitfall`**: `DragEventArgs.x/y` are **element-relative DIP** (not screen pixels).
- Same-control text drag: the source deletes the original selection first, then inserts at the drop point (with left-shift fix); the target detects the private `ZufyUI.TextSource` format and skips the duplicate insert.
- **`Version`**: collapse/expand uses the same **smoothstep easing** as page transitions; the arrow **rotates** with that easing (collapsed points down → expanded points up).
- **`Version`**: the header **highlights** on hover (rounded); the `ComboBox` dropdown arrow is likewise a chevron that **rotates** with expansion.
- **`Pitfall`**: `Expander` collapses via an explicit `SetHeight`, so the parent layout actually shrinks with it.
- Image display uses `Label`: `SetImage` + `SetImageFit(true)` (scale to fully visible) + `SetDragSource` (drag-out).
- **`Version`**: whole-window receive — `Window::SetDropTargetEnabled(true)` + window-level `DragEnter/DragOver/DragLeave/Drop` (`x/y` = client-area DIP).
- **`Version`**: `Expander::SetIcon(Icon, size)` adds a leading icon; `TextEdit` undo/redo uses the standard scheme (both restore the selection and cursor of that point).

###chapter: Theme system | Theme (v1.19.0+)

```cpp
// semantic color roles (look colors up by ROLE, not by control)
enum class ThemeRole { Text, TextSecondary, TextDisabled, TextOnAccent,
    Surface, SurfaceAlt, SurfaceRaised, Backdrop, Overlay,
    Control, ControlHover, ControlPressed, ControlDisabled,
    Accent, AccentHover, AccentPressed,
    Border, Divider, FocusRing, Selection,
    ScrollTrack, ScrollThumb, ScrollThumbHover,
    Danger, Warning, Success, Info, Shadow, Count };
struct Theme { Color colors[(int)ThemeRole::Count]; FontSpec font, fontCaption, fontMono;
    float cornerRadius; bool isDark; std::wstring name;
    Color Get(ThemeRole) const;   void Set(ThemeRole, Color);
    static Theme Light();   static Theme Dark();   static Theme HighContrast(); };

// app level
void ThemeManager::SetAppTheme(const Theme&);   void ThemeManager::SetLight/DarkAppTheme();
Theme& ThemeManager::AppTheme();   bool ThemeManager::IsDark();
// follow system (registry AppsUseLightTheme + DWM AccentColor; auto re-resolve on WM_SETTINGCHANGE etc.)
void ThemeManager::SetFollowSystem(bool);   bool ThemeManager::IsFollowingSystem();
bool ThemeManager::SystemPrefersDark();   Color ThemeManager::SystemAccent();   Theme ThemeManager::SystemTheme();
// test hook: emulate the system theme (bypass the registry)
void ThemeManager::DebugForceSystemTheme(bool enable, bool dark, bool highContrast);
// color remap: any hard-coded color → theme color (exact table; passthrough + keeps alpha on miss)
void ThemeManager::SetColorMap(std::unordered_map<uint32_t, Color>);   Color ThemeManager::Map(Color);
// Hook registration: returns a RAII HookHandle (auto-disconnect on destruction); own fixed hooks use RegisterHookPermanent.
HookHandle ThemeManager::RegisterHook(std::function<void(const Theme&)>);
HookHandle ThemeManager::RegisterSubtreeApplier(std::function<void(UIElement*, const Theme&)>);
// debug: theme name + each Role as AARRGGBB (debug channel cmd 23 GetTheme)
std::wstring ThemeManager::Dump();

// window level
void Window::SetTheme(const Theme&);   const Theme& Window::GetTheme();
void Window::SetUseAppTheme(bool = true);   bool Window::IsUsingAppTheme();   void Window::RefreshTheme();
ZSignal<Window*> UIZSignals::ThemeChanged;   // nullptr = app-wide
ZSignal<Window*> UIZSignals::ThemeReapply;   // re-apply to visible subtree after page/tab switch
```
- Color priority: **explicit instance color > window theme > app theme**. Once a control's `Set*Color` is called explicitly it is flagged "user-colored" and the theme **no longer overrides it**; all other controls follow the theme.
- **`Pitfall`**: `Backdrop` is the **window/background base color** (white in light, black in dark); after an explicit `SetBackdrop/SetBackgroundColor` tint, the theme no longer changes it.
- **`Version`**: introduced in `v1.19.0`; window periphery (caption buttons) follows the **system** theme, not the window theme; `WM_SETTINGCHANGE/WM_THEMECHANGED/WM_DWMCOLORIZATIONCOLORCHANGED/WM_SYSCOLORCHANGE` auto re-resolve.

###chapter: DSL | Fluent construction (ZufyUIDsl.h, v1.19.4; extended in v1.20.0)

```cpp
// header: ZufyUIDsl.h (optional, non-intrusive; changes no existing control)
namespace ZufyUI::dsl {
template<class T> class Ref {                      // wraps shared_ptr<T>
    T* operator->() const;   std::shared_ptr<T> shared() const;
    operator std::shared_ptr<T>() const;           // usable with AddChild/SetContent
    template<class C, class R, class...A0, class...A>
    Ref& with(R(C::*fn)(A0...), A&&... a);         // generic setter forwarding (incl. inherited setters)
    template<class F> Ref& apply(F&& f);           // escape hatch: multi-step/conditional logic via lambda
    template<class Sig, class F> Ref& on(Sig& sig, F&& f);
    template<auto SigMem, class F> Ref& on(F&& f);
    template<class...C> Ref& add(C&&... c);        // add children (dispatches on the container's real API)
    Ref& cell(int row, int col, child, int rowSpan=1, int colSpan=1);   // GridLayout only
    Ref& width(float); Ref& height(float); Ref& size(float,float); Ref& margin(Thickness);
    Ref& minWidth(float); Ref& minHeight(float); Ref& maxWidth(float); Ref& maxHeight(float);
    Ref& minSize(float,float); Ref& maxSize(float,float);
    Ref& weight(float h,float v); Ref& hWeight(float); Ref& vWeight(float);
    Ref& fill(bool w=true,bool h=true); Ref& visible(bool=true); Ref& enabled(bool=true);
    Ref& tooltip(const std::wstring&); Ref& cache(bool=true); Ref& bleed(float);
    Ref& clipRect(const Rect&); Ref& noClip(); Ref& cursor(HCURSOR);
    Ref& dropTarget(bool=true); Ref& contextMenuEnabled(bool=true); Ref& accessible(const std::wstring&);
    // SFINAE-gated sugar: exists only when the control has that method
    Ref& spacing(float); Ref& spacing(float,float); Ref& padding(const Thickness&);
    Ref& background(Color); Ref& textColor(Color); Ref& align(Alignment, Alignment);
    Ref& text(const std::wstring&); Ref& checked(bool=true); Ref& value(float);
    Ref& placeholder(const std::wstring&); Ref& image(std::shared_ptr<Image>); Ref& imageFit(bool=true);
    Ref& items({...}); Ref& rows(int); Ref& cols(int); Ref& header(int, const std::wstring&);
    Ref& cell(int row, int col, const std::wstring&);   // TableView cell
    Ref& root(const std::wstring&); Ref& separator(); Ref& submenu(const std::wstring&, Menu);
};
template<class T, class...A> Ref<T> ZMake(A&&... a);          // generic factory
template<class T> Ref<T> ZWrap(std::shared_ptr<T> p);         // wrap an existing shared_ptr into the chain
Ref<ColumnBox> ZCol(children...);   Ref<RowBox> ZRow(...);
Ref<GridLayout> ZGrid(children...);                          // also accepts ZCell(row,col,child)
CellSpec ZCell(int row, int col, child, int rowSpan=1, int colSpan=1);
using Align = GridLayout::Alignment;                        // used by Ref::align
// short factories all use a Z prefix: ZLbl ZBtn ZTxt ZEdit ZCombo ZCheck ZRadio ZToggle
//   ZSlider ZNum ZProgress ZRing ZScroll ZSplit ZTabs ZList ZTable ZTree
//   ZBar ZLine ZPie ZCard ZExpander ZPages ZPage ZMenu ZSettings
//   ZMenuBar ZStatusBar ZRadioGroup ZScrollBar ZGlyph
// This also avoids collisions with class names; no extra aliases are needed.
class WindowApp {                                  // dsl::ZApp()
    WindowApp& size(int,int);   WindowApp& title(const std::wstring&);
    WindowApp& resizable(bool=true);   WindowApp& corner(Window::WindowCorner);
    WindowApp& backdrop(Backdrop, DWORD tint=0);   WindowApp& hidden(bool=true);
    WindowApp& minSize(int,int);   WindowApp& position(int,int);   WindowApp& frameRateLimit(int);
    WindowApp& theme(const Theme&);   WindowApp& useAppTheme(bool=true);
    WindowApp& dropTarget(bool=true);   WindowApp& titleBarVisible(bool=true);
    WindowApp& showTitleText(bool=true);   WindowApp& menuBarInTitleBar(bool=true);
    WindowApp& icon(HICON, HICON);
    WindowApp& root(Ref<Layout>);   WindowApp& titleBar(Ref<T>);   WindowApp& menuBar(Ref<T>);
    WindowApp& statusBar(Ref<T>);
    template<auto SigMem, class F> WindowApp& on(F&& f);      // e.g. on<&Window::Closed>
    Window& window();   int run();                 // run(): Create -> apply config -> Show() -> Run()
};
WindowApp ZApp();
}
```
- Usage: `using namespace ZufyUI::dsl;` then `ZRow(ZBtn(L"OK").size(100,36).on<&Button::Clicked>([]{...}), ...)`. Every short factory uses a `Z` prefix, so combining it with `using namespace ZufyUI;` no longer collides with class names.
- **`Pitfall`**: `with(&T::Setter, args...)` takes a **member function pointer**; overloaded setters need `static_cast<...>(&T::SetX)`. A function with **default arguments** can't be used with `with` (the pointer carries every parameter → "too few arguments"); since **v1.22.0** such functions are split into **non-defaulted overloads** (e.g. `TreeView::SetCheckable(bool)` / `SetCheckableShallow(bool)`, `ListView::AddItem(text)` / `AddItemWidget(shared_ptr<Label>)`, `RadioGroup::AddItem` / `AddItemSelected`, `Menu::AddCheckItem` / `AddCheckItemCb` / `AddCheckItemRadio`).
- **`Pitfall`**: anything `with` can't express (multi-step, conditional, needs iteration) goes through `apply(lambda)` so the chain doesn't break. It also silences a conversion warning: `.with(&TextBox::SetWidth, 220)` passes an `int` and trips `C4244` — prefer `.width(220.0f)`.
- **`add()` dispatches on the container's real API**: `ColumnBox`/`RowBox`/`Label` → `AddChild`; `PageHost` → `AddPage` (non-`Page` arguments are skipped); `GridLayout` needs coordinates, so use `.cell(row,col,child[,rowSpan,colSpan])` or `dsl::ZCell(...)`. Null children (`nullptr` / empty `Ref`) are always ignored — no null dereference.
- **ZGrid coordinates**: `ZGrid(ZCell(1,2,ZLbl(L"x")), ZCell(0,0,btn,1,2))` places cells explicitly; bare children are still accepted (old behaviour: `(i,0)` in order), so existing code is unaffected.
- **Window builder**: `dsl::ZApp()` → `WindowApp` (chain `size/title/resizable/corner/backdrop/minSize/position/frameRateLimit/theme/useAppTheme/dropTarget/icon/titleBarVisible/showTitleText/menuBarInTitleBar/root/titleBar/menuBar/statusBar/hidden`; signals via `on<&Window::Closed>(...)`). `root()` accepts **any `Layout` subclass** (`ZCol`/`ZRow`/`ZGrid`), and `titleBar/menuBar/statusBar` take a `Ref<T>` directly.
- **`Pitfall`**: `Window::Create` **does not show the window** (the app decides when) — `WindowApp::run()` therefore calls `Show()`; use `.hidden()` to start hidden. `WindowApp` is movable but not copyable. `Window::Run()` returns `void`; `Window` has no public `SetContent`, so set the root via `root(...)`.
- **Per-control sugar** (**SFINAE-gated**: exists only when the control has the method): `text/checked/value/placeholder/padding(float and Thickness)/category/series/slice/item/menu/separator/submenu/items/rows/cols/header/cell/root(s)/background/textColor/spacing(f and h,v)/align/image/imageFit`; extra short factories: `ZPage()`/`ZMenu()`/`ZSettings()`/`ZRadioGroup()`/`ZScrollBar()`/`ZGlyph()`.
- **`Pitfall`**: `Image` is **not** a `UIElement` (it is a resource object), so there is no `Ref<Image>` factory — use `.image(Image::FromFile(...)).imageFit(true)` on a `Label`.


###chapter: Accessibility & Debug Channel | UIA + SetDebugEnabled (v1.17.0)

## Accessibility (UIA)

- The provider comes from `ZufyUIWindowTool.h`; element APIs are in the "Element base" chapter.
- Window APIs: `SetAccessibilityEnabled(bool)` (on by default), `FocusElement`, `GetFocused/Hovered/PressedElement`.
- **`Pitfall`**: the library stores no history and does no file I/O; the provider reflects only the current frame. `WM_GETOBJECT` is handled only when enabled.

## Debug / automation channel

```cpp
inline void SetDebugEnabled(bool);   inline bool IsDebugEnabled();
```
- Off by default. When on, the target window handles `WM_COPYDATA` (`dwData` = command id; reply `0x5A554631`) and returns current-frame info only (no file I/O).

| Cmd | Action | Cmd | Action |
|---|---|---|---|
| 1 | Ping | 12 | ClearHighlight |
| 2 | ListWindows | 13 | GetErrors (unified log E/W/I/D, ≤500) |
| 3 | GetFrameStats | 14 | ClearErrors |
| 4 | GetElementTree | 15/16/17 | TopN repaint/layout/cache (`all` = all) |
| 5 | ForceRepaint | 18 | TopN draw |
| 6 | SetElementText("id\ttext") | 19 | ResetCounters |
| 7 | InvokeElement | 20 | SetVisible("id\t0/1") |
| 8 | FocusElement | 21 | SetMargin("id\tx\ty") |
| 9 | GetElementInfo | 22 | SetHighlightColor("RRGGBB") |
| 10 | GetElementAt("x,y" screen→AutomationId) | | |
| 11 | Highlight(id) | 23 | GetTheme (theme name + AARRGGBB per role) |

- Unified log: `detail::Log(level, tag, msg)` / `LogWarning/LogInfo/DebugLog` / `RecordError` (→ E level + fires `UIZSignals::Error`).
- **`Pitfall`**: send commands with a timeout (`SendMessageTimeout` without `SMTO_BLOCK`), or the debugger hangs when the target is busy. `ZUFYUI_DEBUG` is a **compile-time** switch (logging); `SetDebugEnabled` is a **runtime** switch (channel/stats) — do not conflate.

###chapter: Version digest | v1.15.0 → v1.24.0

> Only the changes that affect your code are listed.

- **v1.15.0**: data-view virtualization (`SetItemCount` etc.) + hide/filter/visible-order sort (two coordinate systems); cross-thread/locking hardening; `ComboBox` source-index API; `TreeView::BeginUpdate/SetNodeText`.
- **v1.16.0**: system dialogs `FileDialog`/`ColorDialog`; `UIZSignals::Error`; render thread off by default + documented unstable; tray callback-message collision fix; optional `ZUFYUI_ENABLE_COMCTL_V6`.
- **v1.17.0**: **UIA accessibility** (on by default); **external debug/automation channel** (`SetDebugEnabled`, off by default, commands 1~22); **layout stretch weights** (`ColumnBox`/`RowBox`/`GridLayout` distribute leftover by weight); **automatic ellipsis tooltips** (Label/Button); `CaptionButton::Kind::Pin`; `TreeView` node `bgColor` / `Get/SetScrollOffsetY` / `ScrollToNode`; `Window::DefaultBackdropColor` pure white.
- **v1.18.0**:
  - New `ZufyUICharts.h`: `ChartBase` / `BarChart` / `LineChart` / `PieChart`; `BarChart` horizontal orientation; `PieChart` wheel zoom.
  - New `TextEdit` multi-line editor/viewer; `TextRun`; base `AcceptsTab()`.
  - `UIElement::ReleaseDeviceResourcesRecursive()`; `PageHost` delayed hidden-page cache release (~1.2 s); `ScrollViewer::UseCache()=false`; `SetUseCache(false)` frees immediately.
  - Library fixes: `Snap` DPI scale also set on the UI thread; `ZSignal::Fire` RAII exception safety; `TextBox` shift+click; `Slider` zero-width NaN guard; `ListView` pooled-label re-measure on width change (`poolW_`).
- **v1.19.0**:
  - New **drag & drop** (`ZufyUIDragDrop.h`): `Window::BeginDrag` + `DragDataBuilder`; element `SetDropTargetEnabled` / `OnDragEnter/Over/Leave/Drop` + signals; `Label`/`Button`/`TextBox`/`TextEdit`/`ComboBox` accept and start drags; drag files out to Explorer.
  - New `Expander` (collapsible card: smoothstep easing + rotating arrow + header hover highlight), `SettingsList` (settings rows); image display uses `Label::SetImage` + `SetImageFit` (scale to fully visible).
  - `ComboBox` dropdown arrow is now a chevron that rotates with expansion.
  - Text drag now has **move** semantics: the source deletes the original selection first, then inserts at the drop point (private format dedups same-control drags).
- **v1.19.1**:
  - `TextEdit` undo/redo switched to the **standard scheme** (stack top = current state): both undo and redo **restore the selection and cursor** of that point; Backspace/Delete/typing/paste are one step each; drop insert/move are undoable and redoable.
  - `ScrollViewer`: content is measured at the **viewport width** (fixes "clipped cards" and wrong scroll range from an infinite desired width for FillWidth/wrapping content); content is measured **twice** (re-measured at the narrower width once a vertical bar appears); the content element's own **`margin` is honored**; the clip gets a **1px bleed** so edge borders are not cut in half.
  - The drop caret now shows even when the target is **unfocused**; during a drag the **source selection is preserved** and a preview caret shows at the drop point; dropping inside the original selection is a no-op.
  - `Expander`: header hover highlight (no longer covering the card border), arrow rotates with the collapse easing, `SetIcon` leading icon.
  - Whole-window drag & drop: `Window::SetDropTargetEnabled` + window-level `DragEnter/DragOver/DragLeave/Drop`.
  - Image display merged into `Label` (new `SetImageFit`); the standalone `ImageView` is removed.
- **v1.19.2**:
  - New top **menu bar** `MenuBar` (VS-style: click opens the `Menu`; hovering only highlights, **no auto-expand**; the open entry stays **pressed** until the menu closes; overlaid on the custom title bar by default, empty areas pass through so the title bar stays draggable).
  - New bottom **status bar** `StatusBar` (left/right panels: optional icon + text).
  - `Window`: `SetMenuBar` / `SetStatusBar` / `SetMenuBarInTitleBar` / `SetShowTitleText` (title text and an on-title-bar menu are **mutually exclusive**).
  - `Menu` gains an `onClosed` callback; `MenuWindow::OnPopupClosed`.
- **v1.19.3**:
  - New **theme system** (`ThemeRole` / `Theme` / `ThemeManager`; `Light/Dark/HighContrast`; window-level `SetTheme`; `UIZSignals::ThemeChanged/ThemeReapply`) — one palette by role; re-applied to the visible subtree on page/tab switch.
  - **Follow system**: reads the registry light/dark + system accent, handles `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_DWMCOLORIZATIONCOLORCHANGED` / `WM_SYSCOLORCHANGE`; `ThemeManager::SetFollowSystem(bool)`.
  - **Explicit instance color wins**: once a control's `Set*Color` is called it is flagged "user-colored" and the theme no longer overrides it; ListView items / TableView cells+rich cells / TreeView cells / headers / alternating rows / Tab selected+hover / radio / NumberBox hover / CheckBox hover all themed.
  - `Backdrop` token (window base white/black); window periphery (caption buttons) follows the **system** theme; `ThemeManager::Map` color remap.
  - Fixes: `ThemeReapply` connection leak (UAF); `SyncThemeBackdrop` swallowing user tint; ListView/TableView/TreeView theme overriding per-item colors.
- **v1.19.4**:
  - New **DSL** (`ZufyUIDsl.h`): `Ref<T>` (`with/on/add` + layout shortcuts), `Make<T>`, `Col/Row/Grid` and per-control short factories — fluent construction / subtree assembly / signal wiring; demo gains a "DSL" page.
  - Fix: `CheckBox` hard-coded `width_/height_` in its ctor → treated as a **fixed width** in `RowBox/ColumnBox` and the **label was clipped**.
  - `ListView/TableView` `RefreshChildren` cache criteria collapsed into a **single signature** `ChildrenSig()` (criteria in one place, harder to forget); consistent with `TreeView`.
  - `ChartBase::MeasureOverride`: falls back to `420×260` when `fill` but unconstrained/no size (no longer collapses to 0).
  - `Window::SetMinSize` default content measure gets a **600 DIP** width floor; `pch.h` `ZufyUI_DEBUG` now `#ifdef _DEBUG`; `Theme::HighContrast` role colors distinguished (no longer all-yellow).
- **v1.19.5**:
  - DSL enhancements: `dsl::App()` / `WindowApp` window builder (fluent config + `run()`); `Page()/Menu()/Settings()` short factories; **SFINAE-gated** per-control sugar (`text/checked/value/placeholder/padding/category/series/slice/item/menu`).
  - The demo "DSL" page uses the sugar and chart composition (`Pie().slice(...)`, `Bar().category(...).series(...)`, `Progress().value(...)`).
- **v1.19.6**:
  - Fix: `ListView` button mode checkbox **hit test was 4px off** (now aligned with the draw / child left edge).
  - Fix: `Label::MeasureOverride` wrap branch **ignored inline children width** (wrapping Labels with `AddChild` squeezed/overflowed text).
  - Docs: version is now a **single source** (`API.md`/`README.md` headers reference the `ZufyUI.h` macro; the generator `mdapi2html.py` injects `{{ZUFYUI_VERSION}}`); cleaned stale items (`ImageView` default row, `Sort`→`SortItems`, `Menu::onClosed`, chart fill wording, README menu `+90MB` history entry); `mdapi2html.py` fixes `--theme auto` semantics and `c++`-style language names in the code-fence regex.
- **v1.19.7**:
  - Fixes: `ChartBase` swapped horizontal/vertical scrollbar allowance (`viewW` uses the vertical bar, `viewH` the horizontal) → rightmost/bottom content unreachable; `NumberBox` wheel focus check (focus is on the inner `TextBox`); `lastActiveAnimElements_` dangling (erased from the anim sets on destruction, prevents random crashes); `Menu::ShowAt` throwing `bad_weak_ptr` when not shared-owned; `ChildrenSig` missing `EnsureView`; `TreeView::SetDefaultExpandDepth` not applied by `AddRoot`/`AddChild`.
  - Fixes: `ColumnBox`/`RowBox` spacing counts only layout-participating children; `GridLayout` row/col count skips invisible; `Label` wrap ignored/duplicated spacing; `ListView`/`TableView` clear `childrenView_` before releasing labels and release pooled/rich/cell labels; `Window::defaultIMC_` via `ImmAssociateContext`; `DragData::GetFiles` buffer `len+1`; `TextEdit` fires `SelectionChanged` when clearing a selection; `MessageBox` default title localized.
  - Added `TODO.md` (merges the former `FEATURE_PLAN.md`/`OPTIMIZATION_PLAN.md`; the single authoritative backlog).
- **v1.20.0**:
  - **DSL**: `root()` now accepts **any `Layout` subclass** (previously only `Col` had a dedicated overload, so `Row`/`Grid` failed to compile because `Ref` → `shared_ptr<Layout>` needs **two user-defined conversions**); `titleBar/menuBar/statusBar` likewise take a `Ref<T>` directly.
  - **DSL**: `add()` dispatches on the container's real API (`ColumnBox`/`RowBox`/`Label` → `AddChild`; `PageHost` → `AddPage`, skipping non-`Page` arguments; `GridLayout` goes through the new coordinate API). Added `dsl::Cell(row,col,child[,rowSpan,colSpan])` and `Ref::cell`, the `apply()` escape hatch, and `Wrap()`.
  - **DSL**: more shortcuts — size limits (`minWidth/minHeight/maxWidth/maxHeight/minSize/maxSize`), stretch weights (`weight/hWeight/vWeight`), `cache/bleed/clipRect/noClip/cursor/dropTarget/contextMenuEnabled/accessible`, `spacing` (single-arg on `Col`/`Row`, two-arg on `GridLayout`), `padding(Thickness)`, `background/textColor`, `align` (GridLayout); per-control sugar gained `image/imageFit/items/rows/cols/header/cell/root(s)/separator/submenu`; new factories `RadioGroup()/ScrollBar()/Glyph()` (`Image` is a resource object, not a `UIElement`, so it has no factory — use the `Label::SetImage` sugar).
  - **DSL**: `WindowApp::run()` **now calls `Show()`** — `Window::Create` does not show the window, so the previous implementation ran a **message loop with no window** (looked like a hang); added `hidden/minSize/position/frameRateLimit/theme/useAppTheme/dropTarget/icon/titleBarVisible/showTitleText/menuBarInTitleBar` and the window signal hook `on<&Window::Closed>` (`Window`'s signals are plain members, there is no `Connect`); `WindowApp` is now **movable** (it holds a `unique_ptr<Window>`, because `Window` itself is neither copyable nor movable).
  - **Fix**: null guards in `ColumnBox`/`RowBox`/`GridLayout::AddChild` — the DSL's variadic forwarding can pass `nullptr`, which used to dereference a null pointer.
  - **Fix**: `ZufyUIWindowTool.h` was **not self-contained** — it used `Label`/`Button`/`ColumnBox::AddChild` without including `ZufyUIWidgets.h`, relying on the caller to include it first (`pch.h` happened to do so); the include is now present and the header can be included on its own.
  - **Tooling**: new `tools/build.ps1` (one shot: **auto-fix BOM before building** → kill processes holding files → build demo + `ZufyUIDebugger` → run self-tests), `tools/ensure_bom.ps1` (adds a UTF-8 BOM only to C++ sources that contain non-ASCII and lack one, preventing the GBK misparse behind `C2143/C2259/C2888/C4819`), and `tests/dsl_tests.cpp` + `tools/run_dsl_tests.ps1` (47 compile-time/runtime assertions, no window created, runnable from a shell or CI).
- **v1.21.0**:
  - **Fix**: `Card` flashed a **blue hover border** — the theme applier set `SetHoverBorderColor(Accent)` on cards, a behaviour cards never had; removed (hover still changes the background).
  - **Fix**: `ProgressBar` **inner fill radius no longer matched the outer frame after stretching** — the track, the clip geometry and the border still used the member `height_` (construction-time height) while only the fill used the arranged height; all three now use `arrangedRect_.height / 2`.
  - **Fix**: `Button` captions were **ellipsized even with room to spare** — `MeasureOverride` returned a constant `width_` (set to `DefaultWidth = 120` in the constructor); it now grows to fit the text (`max(width_, textWidth + 2*padding)`), with `Button::AutoSizeToText` (static, default `true`) and `SetAutoSizeToText(false)`. **This is a layout behaviour change.**
  - **New**: `ToggleSwitch` uses **duration-driven ease-in-out** (`DefaultAnimationDuration = 0.25s`, stops exactly on time); also fixes mouse-click/keyboard paths that bypassed `SetOn` and therefore never started the animation (the "instant snap").
  - **New**: `ProgressBar` value **easing** (`DefaultValueAnimDuration = 0.22s`; fill and percentage text move together).
  - **New**: `SplitView` **collapse**: dragging within the edge threshold first shows an "about to collapse" overlay (translucent accent + direction arrow, alpha fade, **content untouched**); the pane collapses on release (size eases to 0); afterwards a small button is drawn **on top of the content** at that edge (rounded light grey + arrow pointing in the expand direction + hover easing), and clicking it eases that side from 0 to its minimum limit. `SetCollapseEnabled(false)` disables the whole feature. Public API: `SetCollapseEnabled/IsCollapseEnabled/SetCollapseThreshold/CollapseFirst/CollapseSecond/Expand/IsFirstCollapsed/IsSecondCollapsed/GetCollapsedSide/IsCollapsing/GetCollapseProgress/SetExpandButtonColors/SetExpandButtonSize` plus the `CollapsedChanged(int)` signal.
  - **New**: `detail::EaseOutCubic` / `detail::EaseInOutCubic` (duration-driven easing, one shared convention for in-library animations).
  - **New**: `ThemeRole::Shadow` (**appended at the end of the enum** so existing values keep their numbers) with Light/Dark/HighContrast values; `UIElement::DefaultShadowColor`/`SetDefaultShadowColor`; `Window::SetBackdrop(Backdrop)` follows the theme (the `tint` overload means "locked") plus the `ApplyThemeToBackdropParams` hook.
  - **Fix**: `PieChart` **Ctrl+wheel zoom did nothing** — `PieChart`'s own `zoom_` shadows `ChartBase::zoom_`, so delegating to the base only changed the base's copy; the pie now handles Ctrl itself via `SetZoom`. Semantics are explicit: **Ctrl+wheel = zoom / plain wheel = pan only**.
  - **Fix**: chart wheel panning "did not move" — the wheel path was missing `SyncScrollPositions()` (it changed `scrollX_/scrollY_` but never recomputed `baseX_/baseY_`).
  - **Fix**: documented that `ColumnBox` arranges a child **without an explicit width** at the available width (horizontal stretch ignores the weight) — so "make a card hug its content" means wrapping it in a `Row`; and that a `SettingsList` placed in a `Card`'s built-in `GridLayout` (auto columns pass `FLT_MAX`) degenerates to zero width — use `SetLayout(Col(...))` + `SetFillWidth(true)`.
  - **Tooling**: `tests/dsl_tests.cpp` 62 → **85** assertions (regressions for this batch: button auto-size on/off, label width follows text, progress/toggle easing convergence, SplitView collapse and expand-to-minimum).
- **v1.22.0** (**contains breaking API changes**):
  - **Fix**: `MessageBox` (and any modal) **first click falling through to the window behind** — when a modal is opened from inside `Window::OnMouseUp`'s slot, that function's trailing `ReleaseCapture()` only runs after the modal loop ends, so the main window keeps `SetCapture` for the whole modal and Windows routes mouse input to the capture window; `Window::RunModal` now does `if (GetCapture()) ReleaseCapture();` before entering the loop.
  - **Fix**: `ScrollViewer` **scroll chaining** — it no longer swallows the wheel at the top/bottom (returns `false` to bubble up the parent chain when that axis can't scroll further), so the outer page keeps scrolling.
  - **Fix**: `SplitView` **expand had no easing + expand button jumped sides** — the old `Expand()` cleared the collapsed flags before starting the animation, but `PaneLengths` keyed off them; now a single `collapseSide_` drives both directions and is cleared only when the animation finishes.
  - **Breaking · P0-1**: member functions with **default arguments split into non-defaulted overloads** (so `.with()` works): `TreeView::SetCheckable(bool)` + `SetCheckableShallow(bool)`; `SetNodeCheckable(node,bool)` + `SetNodeCheckableRecursive`; `SetNodeEnabled(node,bool)` + `SetNodeEnabledRecursive`; `SetNodeCheckState(node,state)` + `SetNodeCheckStateFull(...,updateChildren,updateParent)`; `ListView::AddItem(text)` and rich-item **`AddItemWidget(shared_ptr<Label>)`** (old name dropped); `RadioGroup::AddItem` / **`AddItemSelected`** / `AddButton` / **`AddButtonSelected`**; `Menu::AddCheckItem(text,checked)` / **`AddCheckItemCb`** / **`AddCheckItemRadio`**.
  - **New · P0-2**: collision-free aliases `make_slider/make_card/make_page/make_menu/make_menu_bar/make_status_bar/make_radio_group/make_scroll_bar/make_expander`.
  - **New · P1-2**: `Page::SetScrollableContent(content)` — one line wraps the whole page in a `ScrollViewer` + margin.
  - **New · P1-4**: `ThemeManager::RegisterHook` / `RegisterSubtreeApplier` return a **RAII `HookHandle`** (auto-disconnect on destruction); the library's own fixed hooks use `RegisterHookPermanent` / `RegisterSubtreeApplierPermanent`.
  - **Docs · P3**: added `docs/pitfalls.md`; `AGENTS.md` gained a "chaining / API conventions" section.
- **v1.22.1**:
  - **Fix (layout)**: `RowBox` / `ColumnBox` `ArrangeOverride` did not clamp each child's `base` to its min/max before distributing leftover → a child with `SetMinWidth/Height` pushed its extra space onto a stretching sibling, **overflowing the row/column** (classic case: a `SetMinWidth` label + `SetFillWidth` slider → the slider grew too long and spilled outside its card). Now `base = clamp(base, min, max)` before computing leftover.
  - **Behaviour**: `ColumnBox` / `RowBox` / `GridLayout` **no longer shrink content** — insufficient space now **overflows** and is left to an outer `ScrollViewer` (desktop UI does not compress content). `GridLayout` dropped its proportional row/column compression on overflow.
  - **Fix (alignment)**: `GridLayout`'s `SetHorizontalAlignment/SetVerticalAlignment` never took effect because the test `colStretchWeights.empty()` was always false; now it checks "all zero", so `Center/End` work (only when no column/row stretch weight is set).
  - **Fix (text/IME)**: `TextBox` / `TextEdit` now merge **UTF-16 surrogate pairs** (emoji / beyond-BMP) and insert them as one unit; `←/→`, `Backspace`, `Delete` move/delete across the whole pair. **On losing focus** the control clears its own composition state and the `Window` cancels the IME composition (`ResetImeState()`) — both control and program IME state reset. `TextBox::SetImeEnabled(false)` also clears composition.
  - **Fix (accessibility)**: `GetAutomationIdOrAuto()` now uses a **per-window** counter (`Window::NextAccessibleAutoId()`), so multi-window/multi-root ids no longer collide; `ElementProviderFromPoint` descends **child-before-parent** to the deepest hit (e.g. a Button embedded in a Label).
  - **Fix**: data view item/cell placement `bleedY` is read from each element's `GetBleed()` (no longer a fixed `3.0f`), so shadows/bleed aren't clipped; `indicatorY_` is initialised to `-1` so the selection indicator snaps on the first frame.
  - **Fix (visual)**: the focus ring uses `ThemeRole::FocusRing` and `Theme::cornerRadius` (was a hard-coded blue + fixed 4); in `WM_NCHITTEST` the custom title-bar buttons take priority over the resize border (the maximize-button corner is no longer taken by `HTTOPRIGHT` → Win11 Snap Layouts can appear).
  - **Engineering**: `pch.h` no longer includes the library headers (editing a lib header no longer rebuilds the whole PCH); added `UIElement::MarkUserColor()` centralising 40+ "user colour" markers; removed dead code `ListView::FindEnabledFrom`.
- **v1.23.0**:
  - **New · `ZufyUIHwndView.h` (`HwndView`)**: native HWND content host. Because the main window is NRB+DComp (child HWNDs are not composited), it uses a **top-level owned popup** as the host; `SetContentHwnd` re-parents the external window as its `WS_CHILD` (cross-process allowed) and shows it. Moving the host moves the child natively; clipping = host client + `SetWindowRgn` (element rect ∩ window client ∩ ancestor `GetClipRect`, incl. the `ScrollViewer` viewport). Supports `SetObscured`, `SetOnBoundsChanged`, `SetFill*`.
  - **New · `Window` external-visual attachment**: `CreateExternalVisual()` (WinRT `ContainerVisual` inserted atop the root visual, for WebView2 `put_RootVisualTarget`), `SetExternalVisualBounds` (DIP→physical px), `RemoveExternalVisual`; `AddOverlaySync/RemoveOverlaySync/SyncOverlays` (unified overlay sync).
  - **Performance**: overlay sync is now a **single window-level point** (`WM_MOVE/WM_SIZE/WM_DPICHANGED` immediate + one shared 16ms fallback timer) instead of a per-instance timer; DPI/client cached; redundant `SetWindowPos` skipped and `SWP_NOSIZE/NOMOVE` used as appropriate; the demo's color pattern is cached into a DIB.
  - **Fix · `NumberBox`**: the empty-text error clears once the text becomes a **valid number**; `ResetToDefault` force-clears the error after notifying the app; step/clear buttons fire **on press** (no re-check against the moving rect; fixes staying red after the × button).
  - **Fix · data-view fill**: `ListView`/`TableView`/`TreeView` `MeasureOverride` honours `SetFillWidth/SetFillHeight` (like charts) instead of always returning the default 200/400.
  - **Demo**: new "Native HWND" page (GDI color pattern + Card/Grid/Column/ScrollViewer layouts + an out-of-process `--outproc` producer window hosted by `HwndView`).
- **v1.23.1**:
  - **Fix (layout / hit-testing)**: `PageHost::MeasureOverride` now **measures every page**. Previously `PageHost` only arranged pages while `Page::Arrange` re-measured solely on a size change, so page content made visible at runtime (e.g. an `Expander` card revealed after a combo-box selection) was never re-measured: the new element kept a zero `arrangedRect` (no border, `HitTest` failed) until the window was resized. Clean pages hit the `Measure` cache, so no duplicate work is added.
- **v1.23.2**:
  - **Build**: added `/bigobj` to the **Debug|Win32** / **Debug|x64** configurations of `ZufyUI.vcxproj` / `ZufyUIDebugger.vcxproj` (Release already had it); the full Release/Debug matrix passes.
- **v1.23.3**:
  - **Button**: the internal `Label` is a real child now (rich content via `GetLabel()->AddChild` / `SetContent`); added the drop 2x2 switches (`SetDropWriteBack` / `SetDropOpenOnClick`) and a single-select menu (`DropSelected` / `DropMenuOpened`).
  - **Fix ? drop-menu position**: `GetDpiScale()` returns 96/144 (not 1.0/1.5); the old multiplier sent the menu to the screen bottom-right. Dividing by 96 places it under the button, and the demo no longer shows a MessageBox.
  - **Fixes**: `SetBackdrop(Acrylic/Mica)` no longer covers the system material with opaque white when no tint is set; `ListView::SetItemHeight` is clamped to >=1px and Draw/hit-testing no longer divide by zero.
  - **Docs/cleanup**: documented `HwndView` cross-process risks; removed the redundant `WM_ACTIVATE` check, added `<cstring>`, removed the demo's empty callback.
- **v1.24.0** (**contains breaking changes**):
  - **Signal lifetime**: `ConnectionState::alive` is atomic; `Connection::disconnect` / receiver destruction logically cancels pending `UIThread` / `NewThread` tasks; `Fire` concurrent with signal destruction is explicitly UB.
  - **Receiver context**: `UIElement` / `Window` derive from `ConnectionOwner`; `elem->Connect(...)` / `Connect(elem, ...)` / `sig.connect(elem, ...)` disconnect with the receiver; added free `Connect` and `ConnectWeak`.
  - **Breaking**: `ZSignal` is neither copyable nor movable; member-function pointers use `Connect(elem, sig, &Class::Method)` / `ConnectWeak`; `elem->Connect(sig, &Class::Method)` is no longer supported.
  - **Notes**: `Menu` / `MenuItem` / `Timer` still use `signal.connect` and do not auto-disconnect; signal arguments are passed as lvalues.


###chapter: Pitfall index | Quick lookup by topic

- **Color**: components are [0,1]; use `Color::FromArgb` for integers.
- **Size/layout**: `SetWidth/Height` = fixed size; `SetFill*` = fill parent axis; stretch weights distribute **leftover** space (v1.17.0); charts default to weight 0, `SetFill*` takes effect when the parent gives a constraint and falls back to `420×260` otherwise (v1.19.4).
- **Window min size**: set `SetMinSize` explicitly for windows with long text/charts, or the content measure blows it up.
- **Repaint**: `RequestRepaint()` marks only, **does not schedule a frame**; don't force a frame there (continuous re-render stutter). `InvalidateLayout()` bubbles; `InvalidateLayoutSelf()` does not (pool/virtual rebind).
- **Cache**: on by default; `SetUseCache(false)` for per-frame/heavy controls; release subtrees with `ReleaseDeviceResourcesRecursive()`.
- **Text**: `SetTextFast` doesn't repaint (pool only); `FontManager` layouts/formats are shared read-only.
- **Signals**: `Fire` iterates a snapshot (safe disconnect); `Connection` destructor doesn't disconnect; `DrawOverlay` must filter by window; avoid self-capturing `shared_ptr` lambdas.
- **Data views**: source-index vs. visible-order coordinates; `Sort` rearranges + clears multi-select, `SortView` only reorders; `parent` is `weak_ptr`; row background via `node->bgColor`.
- **Multi-line editor**: Tab indents; rich text is read-only; wrap-width change rebuilds all line layouts.
- **Scrollbar**: hosts reserve a gutter via `ShrunkWidthRatio`; idle shrink is **lazy**.
- **Accessibility/debug**: role drives exposed UIA Patterns; send debug commands with a timeout.
- **DSL (`ZufyUIDsl.h`, optional)**: `root()` takes any `Layout` subclass; `add()` dispatches on the container's real API (`GridLayout` requires `.cell(row,col,child)` or `dsl::Cell(...)`); null children (`nullptr` / empty `Ref`) are always ignored; `with()` takes member function pointers only — use `apply(lambda)` for multi-step logic; `WindowApp::run()` calls `Show()` (`Window::Create` does not show the window), so use `.hidden()` to start hidden.

###chapter: Appendix | Defaults & signal index

## Common defaults

| Item | Value |
|---|---|
| Root layout | `ColumnBox`, margin 20, spacing 10 |
| `Color` default | `(0,0,0,1)` |
| `Button` size | 120×36 |
| `ToggleSwitch` | 50×24 |
| `ScrollBar` width / `ShrunkWidthRatio` | 8 / 0.30 |
| `TabView` tab height / indicator | 34 / 2.5 |
| `Card` padding / radius | 12 / 8 |
| `ChartBase` size / palette | 420×260 / 8 colors |
| `PieChart` size | 320×260 |
| `TextEdit` font / size / tab | Consolas 14 / 320×160 / 4 |
| `Expander` header height / radius | 66 / 8 |
| `Window::DefaultBackdropColor` | `0xFFFFFFFF` (white) |

## Signal index

- Element: `MouseEnter/MouseLeave/MouseMove/MouseDown/MouseUp/Focused/Blurred/KeyDown/KeyUp/Char`
- Button: `Clicked/Toggled`; TextBox: `TextChanged/ReturnPressed`; TextEdit: `TextChanged/CursorPositionChanged/SelectionChanged`
- ComboBox: `SelectionChanged/DropDownOpened/DropDownClosed`; CheckBox: `Toggled/StateChanged`
- RadioButton: `CheckedChanged/Clicked`; RadioGroup: `SelectionChanged`; ToggleSwitch: `Toggled`
- Slider: `ValueChanged/SliderReleased`; NumberBox: `ValueChanged/TextChanged`; ProgressBar: `ValueChanged`
- ScrollViewer: `ScrollChanged`; SplitView: `SplitChanged`; TabView: `SelectionChanged/TabCloseRequested`
- ListView/TableView/TreeView: `SelectionChanged/ItemClicked(CellClicked)/ItemDoubleClicked/ItemRightClicked/…CheckStateChanged`
- Chart: `PointClicked/CategoryClicked`; PieChart: `SliceClicked`
- Drag & drop: element `DragEnter/DragOver/DragLeave/Drop`; window-level `Window::DragEnter/DragOver/DragLeave/Drop`; Expander: `ExpandedChanged`; SettingsList: `RowClicked`
- Window: `Activated/Deactivated/Closing/Closed/DeviceLost/RenderingError`
- Global: `UIZSignals::DrawOverlay/GlobalMouseDown/WindowActivated/WindowDeactivated/DeviceReset/ReloadAcrylic/Error`