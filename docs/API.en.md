# ZufyUI API Reference

> This document covers the entire public API of the ZufyUI framework, **organized by usage category** (not by code/header order).
> Companion reading: [Project README & build](../README.en.md).

> **How to read**: this is not just a signature list — each class first explains overall behavior, then each method's side effects, defaults and pitfalls. **You can use it correctly without reading the source.**

> **Two markers used throughout**:
> - **`Pitfall`**: real traps / counter-intuitive behavior — read these first.
> - **`Version`**: the release in which the API or its semantics changed notably (`added`/`changed`/`deprecated`). Current release is **v1.18.0** (previous: v1.17.0).

###chapter: Conventions | Namespaces, units, lifetime, version & pitfall notation

## Namespaces and headers

- Everything is in `namespace ZufyUI`.
- Headers are **layered by usage**:
  - `ZufyUI.h`: core — types / signals / fonts / element base / layout / menus / window.
  - `ZufyUIWidgets.h`: basic controls (Label/Button/TextBox/TextEdit/ComboBox/…/ScrollViewer/TabView/icons), includes `ZufyUI.h`.
  - `ZDataViewer.h`: data views (ListView/TableView/TreeView), depends on the two above.
  - `ZufyUICharts.h`: **charts (BarChart/LineChart/PieChart)**, depends on `ZufyUI.h`.
  - `ZufyUIWindowTool.h`: window extras (title bar / MessageBox / tray / system dialogs / accessibility provider / debug channel).
- All are **header-only**; `#pragma comment(lib, ...)` auto-links `d2d1 / dwrite / dwmapi / imm32 / winmm`.

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
- `NewThread`: **spawns a detached thread per Fire** — do not touch UI inside.
- `UIThread`: posts the slot to the UI thread (the correct way to update UI from workers).
- **`Version`**: cross-thread/locking semantics hardened in `v1.15.0`.

## Connection (Qt-style passive handle)

```cpp
class Connection {
    void disconnect();
    bool isConnected() const;
    explicit operator bool() const;   // destructor does NOT auto-disconnect
};
```
- Disconnected by the host element's `ConnectionGroup` on destruction; **`Connection`'s own destructor does not**.
- **`Pitfall`**: it is safe to ignore the return of `connect`; but if you keep a `Connection` and `disconnect` manually, the signal may already be gone — `isConnected`/`disconnect` are guarded by an `alive` flag.
- **`Version`**: passive-handle semantics since `v1.8.1`.

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    void disconnectAll(); size_t size() const;
};
```

## ZSignal

```cpp
template<typename... TArgs> class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    void Fire(TArgs... targs) const;
    void operator()(TArgs... targs) const;
};
```
- Fire with `Fire` (**there is no `emit` macro**).
- **`Pitfall`**: `Fire` iterates a **snapshot**, so a slot may safely `disconnect`; re-firing the same signal inside a slot goes through the "reentrant" path (local copy) — avoid deep recursion there.
- **`Version`**: `v1.18.0` adds a RAII guard so a throwing slot still restores reentrancy depth and releases the snapshot.

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
- **`Version`**: easing `v1.8.1`; **delayed hidden-page cache release (~1.2 s, recursive)** = `v1.18.0`.

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
```
- **`Pitfall`**: the custom title bar **does not participate in layout** — `Window` places it at `(0,0)` and shifts the root down by its measured height. Do not `AddChild` it manually.
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
Button(const std::wstring& text = L"Button");
void SetText/SetIcon/SetIconColor(...);
void SetColors(Color normal, Color hover, Color pressed);   void SetTextColor(Color);
void SetCornerRadius(float);   void SetHoverAnimationSpeed(float);
void SetCheckable(bool);  void SetChecked(bool);  bool IsChecked() const;
void SetAutoRepeat(bool enable, float intervalMs=400);
```
- Default `120×36`; `Clicked` only if press+release are both inside.
- **`Pitfall`**: `IsChecked()` is meaningless unless checkable.

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
- **`Version`**: dropdown geometry/state `v1.12.0`; source-index API `v1.15.0`.

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
                    int AddItem(const std::wstring&, bool selected=false); std::shared_ptr<RadioButton> GetButton(int index) const; int GetItemCount() const; void SetSelectedIndex(int); ... };
```
- **`Pitfall`**: a lone `RadioButton` does not enforce exclusion — `RadioGroup` does.
- **`Version`**: `v1.11.0`.

## ToggleSwitch

```cpp
ZSignal<bool> Toggled;   void SetOn(bool);  bool IsOn() const;
void SetIndeterminate(bool);   // half-selected appearance
```
- Default 50×24.

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
```
- **`Pitfall`**: `SetValue` is `[0,1]`, `SetRangeValue` is `[min,max]` — do not mix.

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
bool UseCache() const override { return false; }
```
- **`Pitfall`**: `Orientation` naming is counter-intuitive (`Vertical` = vertical splitter, horizontal arrangement).
- **`Version`**: `v1.12.0`; NaN/Inf guard `v1.14.0`.

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
void AddCheckItem(const std::wstring& text, bool checked, std::function<void(bool)> onToggle = nullptr, int id=0, bool radio=false);
void AddSeparator();   void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu, int id=0);
std::vector<std::shared_ptr<MenuItem>> items;
std::function<void(Menu&)> onOpening;   ZSignal<int> ItemSelected;
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
void AddItem(const std::wstring&);   void SetItem(int, const std::wstring&);
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
- **`Pitfall`**: `Sort()` rearranges `items_` and **clears multi-selection/checks**; `SortView` only changes **visible order**.
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
void SetCheckable(bool enable, bool recursive=true);  void SetCheckMode(CheckMode);
void SetNodeCheckState(node, TreeNode::CheckState, bool updateChildren=true, bool updateParent=true);
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
- **`Pitfall`**: size comes from the chart's own `width_/height_` (default `420×260`). **`SetFillWidth/Height` alone won't size the chart** — set explicit `SetWidth/SetHeight`, or give it a layout stretch weight. Charts default to weight 0.
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
| 11 | Highlight(id) | | |

- Unified log: `detail::Log(level, tag, msg)` / `LogWarning/LogInfo/DebugLog` / `RecordError` (→ E level + fires `UIZSignals::Error`).
- **`Pitfall`**: send commands with a timeout (`SendMessageTimeout` without `SMTO_BLOCK`), or the debugger hangs when the target is busy. `ZUFYUI_DEBUG` is a **compile-time** switch (logging); `SetDebugEnabled` is a **runtime** switch (channel/stats) — do not conflate.

###chapter: Version digest | v1.15.0 → v1.18.0

> Only the changes that affect your code are listed.

- **v1.15.0**: data-view virtualization (`SetItemCount` etc.) + hide/filter/visible-order sort (two coordinate systems); cross-thread/locking hardening; `ComboBox` source-index API; `TreeView::BeginUpdate/SetNodeText`.
- **v1.16.0**: system dialogs `FileDialog`/`ColorDialog`; `UIZSignals::Error`; render thread off by default + documented unstable; tray callback-message collision fix; optional `ZUFYUI_ENABLE_COMCTL_V6`.
- **v1.17.0**: **UIA accessibility** (on by default); **external debug/automation channel** (`SetDebugEnabled`, off by default, commands 1~22); **layout stretch weights** (`ColumnBox`/`RowBox`/`GridLayout` distribute leftover by weight); **automatic ellipsis tooltips** (Label/Button); `CaptionButton::Kind::Pin`; `TreeView` node `bgColor` / `Get/SetScrollOffsetY` / `ScrollToNode`; `Window::DefaultBackdropColor` pure white.
- **v1.18.0**:
  - New `ZufyUICharts.h`: `ChartBase` / `BarChart` / `LineChart` / `PieChart`; `BarChart` horizontal orientation; `PieChart` wheel zoom.
  - New `TextEdit` multi-line editor/viewer; `TextRun`; base `AcceptsTab()`.
  - `UIElement::ReleaseDeviceResourcesRecursive()`; `PageHost` delayed hidden-page cache release (~1.2 s); `ScrollViewer::UseCache()=false`; `SetUseCache(false)` frees immediately.
  - Library fixes: `Snap` DPI scale also set on the UI thread; `ZSignal::Fire` RAII exception safety; `TextBox` shift+click; `Slider` zero-width NaN guard; `ListView` pooled-label re-measure on width change (`poolW_`).

###chapter: Pitfall index | Quick lookup by topic

- **Color**: components are [0,1]; use `Color::FromArgb` for integers.
- **Size/layout**: `SetWidth/Height` = fixed size; `SetFill*` = fill parent axis; stretch weights distribute **leftover** space (v1.17.0); charts size from their own `width_/height_` and default to weight 0.
- **Window min size**: set `SetMinSize` explicitly for windows with long text/charts, or the content measure blows it up.
- **Repaint**: `RequestRepaint()` marks only, **does not schedule a frame**; don't force a frame there (continuous re-render stutter). `InvalidateLayout()` bubbles; `InvalidateLayoutSelf()` does not (pool/virtual rebind).
- **Cache**: on by default; `SetUseCache(false)` for per-frame/heavy controls; release subtrees with `ReleaseDeviceResourcesRecursive()`.
- **Text**: `SetTextFast` doesn't repaint (pool only); `FontManager` layouts/formats are shared read-only.
- **Signals**: `Fire` iterates a snapshot (safe disconnect); `Connection` destructor doesn't disconnect; `DrawOverlay` must filter by window; avoid self-capturing `shared_ptr` lambdas.
- **Data views**: source-index vs. visible-order coordinates; `Sort` rearranges + clears multi-select, `SortView` only reorders; `parent` is `weak_ptr`; row background via `node->bgColor`.
- **Multi-line editor**: Tab indents; rich text is read-only; wrap-width change rebuilds all line layouts.
- **Scrollbar**: hosts reserve a gutter via `ShrunkWidthRatio`; idle shrink is **lazy**.
- **Accessibility/debug**: role drives exposed UIA Patterns; send debug commands with a timeout.

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
- Window: `Activated/Deactivated/Closing/Closed/DeviceLost/RenderingError`
- Global: `UIZSignals::DrawOverlay/GlobalMouseDown/WindowActivated/WindowDeactivated/DeviceReset/ReloadAcrylic/Error`