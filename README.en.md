# ZufyUI

> **Renamed: `ZUI` → `ZufyUI`** (to avoid clashes with other projects of the same name). Old repository URLs are redirected by the platform to the new address (see links below).

> A C++ native Win32 custom-drawn UI framework based on Direct2D, with layout, controls, signals/slots, and high-DPI adaptation.

> Language: **English** · [简体中文](README.md)

ZufyUI is a **header-only** Windows desktop UI framework built directly on Direct2D / DirectWrite / DWM. It does not depend on Qt, MFC, or any third-party library; it packs controls, layout, animation, fonts, data views, and signals/slots into a handful of `.h` files, making it suitable for native C++ applications that want a lightweight, controllable, modern look.

- Repository: GitHub <https://github.com/zhc9968/ZufyUI> · Gitee <https://gitee.com/zhc9968/ZufyUI>
- License: MIT
- Online docs: <https://zhc9968.github.io/ZufyUI/>
- API Reference (English): <https://zhc9968.github.io/ZufyUI/docs/API.en.html>

## Features

- **Direct2D custom drawing**: hardware-accelerated rendering, Acrylic background, rounded corners, and shadows for a modern look.
- **Complete layout system**: `ColumnBox` / `RowBox` / `GridLayout`, supporting spacing, stretch weight, fill, alignment, and row/column spanning.
- **Signals and slots**: built-in `ZSignal` / `Connection`, supporting `Connect` with automatic lifetime management, plus three dispatch strategies: current thread / new thread / UI thread.
- **Rich controls**: labels, buttons, text boxes, combo boxes, toggle switches, scroll containers, progress bars, sliders, and three data views: list / table / tree.
- **Complete animation and transitions**: hover, expand, indicator bar, and page switching (`PageHost`) all have built-in animations, and scrolling supports smooth scrolling.
- **High-DPI adaptation**: automatically aware of DPI; `Snap()` snaps drawing to physical pixels to avoid blurriness.
- **IME compatibility**: text boxes support Chinese IME composition input and candidate window positioning.
- **Offscreen caching**: elements can automatically cache their drawing results to reduce repeated drawing overhead.
- **System integration**: tray icon (hover/click/right-click/badge/balloon), taskbar (progress / overlay badge / thumbnail toolbar), jump lists, and **app identity (AUMID) self-registration** (unifies toast / grouping; requires an explicit authorization macro).

## Requirements

| Item | Requirement |
| --- | --- |
| Operating system | Windows 10 / 11 |
| Compiler | Visual Studio (**any toolset works**; requires **C++20** or later) |
| Windows SDK | 10.0 or later |
| Source encoding | **UTF-8 with BOM** (otherwise MSVC parses Chinese comments in the local code page and fails to compile) |
| Runtime libraries | System-provided `d2d1` / `dwrite` / `dwmapi` / `imm32` (no additional installation required) |
| Third-party dependencies | None |

## Build

1. Open the solution `ZufyUI.slnx` with Visual Studio;
2. Select the **Release | x64** configuration;
3. Build the solution (Build Solution);
4. The output is located at `x64\Release\ZufyUI.exe`; run it directly.

Command-line build (requires MSBuild on PATH):

```bash
msbuild ZufyUI.slnx /p:Configuration=Release /p:Platform=x64
```

> `ZufyUI.cpp` is a comprehensive demo program (left-side navigation + multiple test pages) that also serves as the framework's "smoke test". When building your own application, you only need to include the headers and write your own `WinMain`.

## Project structure

```text
ZufyUI/
├── ZufyUI.h              # Core: types / signals & slots / fonts / elements / layout / menus / window
├── ZufyUIWidgets.h       # Basic controls: Label / Button / TextBox / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider
├── ZDataViewer.h      # Data views: ListView / TableView / TreeView
├── ZufyUI.cpp            # Demo program entry point (WinMain)
├── ZufyUI.slnx           # Solution
├── ZufyUI.vcxproj        # Project file
├── AGENTS.md          # Development principles (incl. "find the root cause before fixing any bug")
└── docs/
    └── API.en.md      # Full API documentation
```

## Quick start

```cpp
#include "ZufyUIWidgets.h"
using namespace ZufyUI;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    Window win;
    if (!win.Create(1000, 700, L"ZufyUI Demo"))
        return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto btn = std::make_shared<Button>(L"Click me");
    btn->Connect(btn->Clicked, []() {
        MessageBoxW(nullptr, L"Hello ZufyUI!", L"Info", MB_OK);
    });

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto state = std::make_shared<Label>(L"Switch: off");
    toggle->Connect(toggle->Toggled, [state](bool on) {
        state->SetText(on ? L"Switch: on" : L"Switch: off");
    });

    row->AddChild(btn);
    row->AddChild(toggle);
    row->AddChild(state);
    root->AddChild(row);

    win.Run();
    return 0;
}
```

Key points:

- `Window::Create` creates the window, initializes Direct2D, applies the Acrylic background, and generates a default root `ColumnBox`;
- Create controls with `std::make_shared<T>()` and attach them to the layout with `AddChild`;
- Bind events with `Connect(signal, slot)`; connections are automatically disconnected when the control is destroyed;
- Finally, call `win.Run()` to enter the message loop.

Result (that exact code above):

![Quick start: a window + a button + a toggle](docs/images/quickstart.png)

### Two more steps

**Step 2: change the background.** Only three backdrop layers exist — `None / Acrylic / Mica` — with an ARGB tint composited on top:

```cpp
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // Mica + fully transparent tint
```

![Backdrop: manual Mica](docs/images/backdrop-mica.png)

**Step 3: a custom title bar.**

```cpp
#include "ZufyUIWindowTool.h"   // DefaultTitleBar lives here

auto bar = std::make_shared<DefaultTitleBar>();
win.SetCustomTitleBar(bar);
```

![Custom title bar](docs/images/custom-titlebar.png)

A tour of the widgets (first page of the demo):

![ZufyUI demo](docs/images/demo-main.png)

## Changelog

### 2026-09-26 — Tray / taskbar / menu enhancements + app identity self-registration

**Menus**
- Rebuilt the right-click menu as a **layered window with self-drawn soft shadow, rounded corners, and fade-in**; fixed "menu won't open a second time" (root cause: the checkmark's `ID2D1StrokeStyle` was a `static` reused across factories → `EndDraw` returned `D2DERR_WRONG_FACTORY`; now created/released per instance).
- Menu item enhancements: `id` / check / radio / default item (bold + Enter) / danger (red) / shortcut hint / disabled; per-item `bgColor` / `textColor` / `font`; `Menu::font` / `SetDefaultFont`.
- **Dynamic menu factory** `UIElement::SetContextMenuFactory` (built on each right-click) + `SetContextMenuEnabled`.
- Standalone popup `Menu::ShowAt` / `ShowAtCursor` (bound to no window; for tray menus); open menus are mutually exclusive.
- Hover highlight is now a translucent overlay (still visible over a custom background).

**Tray / taskbar / icons**
- `TrayIcon` (`Shell_NotifyIcon` v4): hover / click / right-click / double-click, badge (self-drawn composite), balloon (`ShowBalloon`, `BalloonIcon` = None/Info/Warning/Error/Custom, realtime / silent); auto re-added after `explorer` restarts.
- Taskbar: progress (`SetTaskbarProgress`), overlay badge (`SetTaskbarOverlayIcon`), thumbnail toolbar (`SetThumbButtons`, the `Image` overload auto-builds an `HIMAGELIST`), jump lists (`SetJumpList`, tasks + categories + recent).
- `Window::SetAppIcon` (unifies native big/small + class icons + custom title bar), `ShowActivate` (four modes), `Flash` (`FLASHW_ALL`); `TitleBar` icon badge / progress.
- `Image::ToHICON` / `CopyPixelsBgra`.

**Data views**
- `ListView::ItemRightClicked(row)` + `GetContextRow()`; `TableView::CellRightClicked(row,col)` + `GetContextRow()/GetContextColumn()` for list/table right-click menus.

**App identity self-registration (new)**
- Declare `AppInfo{ displayName, aumid, icon }` + `RegisterApp(...)` (free function / `Application::RegisterApp` forwarding): sets the process AUMID so toast / jump list / taskbar grouping share one identity.
- **Requires the explicit authorization macro `ZUFYUI_ALLOW_APP_REGISTRATION`**; when authorized it writes `HKCU\...\AppUserModelId\<AUMID>` (DisplayName + IconUri) and caches the icon at `%LOCALAPPDATA%\ZufyUI\AppReg\<AUMID>\app.ico`, **cleaning both up on process exit**; without the macro it only sets the AUMID (zero side effects).

**Build**
- Added a precompiled header `pch.h` / `pch.cpp` (demo compile ~1.1s); `Release` drops debug info; `Debug` adds `/bigobj`.

> Known issue: menu windows use the "layered window + software render target" path and each menu creates its own D2D factory → every menu open costs ~+90MB (released a few seconds after closing). Planned fix: reuse the shared device (move menus to the DComp path).

### 2026-09-20 — Performance work + backdrop API consolidation (v1.9.6)

- **Performance**: page-switch peak memory 300–400MB → **capped at ~30MB**; layout rework (DesiredSize cache + two-level dirty flags; no more "clear all caches on any layout invalidation"; page switches no longer trigger a full relayout); no per-frame full-tree hit-testing while dragging/resizing; **global cross-widget text-layout cache** + binary-search ellipsis for `Label`; no per-element `GetDpi()`.
- **Backdrop API consolidated to "a 3-way layer + an overlay colour"**: `Backdrop` is only `None / Acrylic / Mica`; **`BackdropMode` removed**; `ReloadAcrylic/ReloadMica` removed; new `SetBackgroundParams(layer, BackgroundParams/AcrylicPreset)` plus `AcrylicParams`/`MicaParams`. Acrylic/Mica are rendered by ZufyUI itself, **identical on Win10 and Win11**.
- **Data views**: `ListView` gains `BeginUpdate/EndUpdate` for batched add/remove.
- **Fixes**: sort now emits `SelectionChanged`; `ComboBox` filtering keeps the selection; `TextBox` undo stack; `CaptionButton` press behaviour; scrollbar divide-by-zero guards; image cache device epoch.

### 2026-09-19 — Post-1.8.0 fixes and polish (v1.8.1)

**Backdrop**
- The DComp acrylic background layer is now **rebuilt at runtime** (new `UpdateDCompBackdrop()`): `SetBackdrop` / `SetBackdropMode` work even after the window is created; switching `Acrylic → Mica/other` properly clears the old layer (no more see-through/black); it is also re-attached after device-loss rebuilds.

**Window**
- Added `Window::SetTitle(const std::wstring&)` and `Window::SetIcon(HICON bigIcon, HICON smallIcon)` (native title bar; use `TitleBar::SetTitle` with a custom title bar).
- `SetCustomTitleBar` now applies the current `SetTitleBarVisible` state, removing the "hide then install" inconsistency.

**Signals (connections)**
- `Connection` is now a **Qt `QMetaObject::Connection`-style passive handle**: copyable, and **destroying it no longer disconnects**. `UIElement::Connect(...)` now **returns a `Connection`**; ignoring the result is safe, and to disconnect a single connection use `auto c = elem->Connect(sig, slot); … c.disconnect();`. Auto-disconnect is still handled by the element's `ConnectionGroup` (`ImageManager`'s DeviceReset subscription also goes through a group). Removed the redundant `autoConnections_`.

**Data views / controls**
- `TableView` cell keys changed from `(long long)row*10000+col` to `(uint64)row<<32 | col`, fixing key collisions when a column index ≥ 10000 (`cellSel_` / `cellTextColors_` / `cellTips_`).
- `ListView::MoveItem` now fires `SelectionChanged` (and `EnsureVisible`).
- `ComboBox` item widths are cached (recomputed only when data changes) and `SetToolTip` was moved out of `Measure` (no per-frame measuring / side effects).
- `ScrollViewer` clips content to the viewport (new `UIElement::SetClipRect`), so it no longer draws under the scrollbars; the scrollbars themselves are unaffected.

**Misc**
- `PageHost` gained transition easing `TransitionEasing { Linear, EaseInOut, EaseOut }`, default `EaseInOut` (smooth); switch via `SetTransitionEasing`.
- Process DPI awareness is set once; removed the dead `D2DERR_RECREATE_TARGET` branch.
- **Memory leak fixed**: `AppCore::dqController_` is held/released via `ComPtr`.
- Demo: removed the standalone "custom title bar window" (the main window is already custom).

### 2026-09-19 — Rendering migrated to DirectComposition + backdrop/signal API overhaul (v1.8.0)

**Rendering: migrated to Direct2D 1.1 + DXGI flip SwapChain + DirectComposition**

- `Window` no longer uses `ID2D1HwndRenderTarget`; it now uses an `ID2D1DeviceContext` + `IDXGISwapChain1` (`CreateSwapChainForComposition`) + a DComp visual tree, with `WS_EX_NOREDIRECTIONBITMAP`, so content is submitted via DComp and per-pixel transparency is supported.
- `AppCore` shares a process-wide `ID3D11Device → IDXGIDevice → ID2D1Device` and WinRT `ICompositor` across windows.
- Device loss / DPI change / resize go through a unified discard-and-recreate path (`DiscardDeviceResources` / `ResizeSwapChain`); new `DeviceLost` / `RenderingError` signals.

**Acrylic: switched to the DComp `HostBackdropBrush` + Gaussian blur**

- Acrylic is now drawn by a DComp background layer (`ICompositor3::CreateHostBackdropBrush`, falling back to `ICompositor2::CreateBackdropBrush`, wrapped in a Gaussian blur effect) that samples the host backdrop, together with `DWMWA_USE_HOSTBACKDROPBRUSH` + `AccentState(HOSTBACKDROP)`; it no longer relies on `DWMWA_SYSTEMBACKDROP_TYPE`, which breaks as the window frame changes.
- Includes a hand-written `IGraphicsEffect` / `IGraphicsEffectD2D1Interop` wrapper (no Win2D); new dependency on `d2d1effects_2.h` and `dxguid.lib`.

**Backdrop API overhaul ("what" vs "which API")**

- New `Backdrop { None, Normal, Blur, Acrylic, Mica, MicaAlt }`: **what effect you want**.
- `BackdropMode { Auto, System, Accent }`: **which API implements it**.
- `SetBackdrop(Backdrop, tint = 0)` / `GetBackdrop()`, `SetBackdropMode` / `GetBackdropMode()`.
- Removed the old `WindowBackdrop` / `SystemBackdropMaterial` (confusing names/semantics).

**Callbacks → signals**

- `UIElement` events changed from raw `std::function` members to signals: `MouseEnter / MouseLeave / MouseMove / MouseDown / MouseUp / KeyDown / KeyUp / Char / Focused / Blurred`.
- `MenuItem::Clicked`, `CaptionButton::Clicked` (default behaviour wired by `DefaultTitleBar`; custom title bars can connect/intercept themselves).
- New `Window::Closing` (`ZSignal<bool*>`, set `*cancel=true` to cancel, e.g. "confirm before close"), `Window::BackdropUnsupported` / `Window::DeviceLost` / `Window::RenderingError`.
- Global `UIZSignals::DeviceReset`: fired when render device resources are discarded/recreated, so subscribers can clear device-keyed caches (`ImageManager` clears image bitmap caches, fixing dangling render-target keys and bitmap leaks).
- Removed `SetBackdropUnsupportedHandler` / `SetDeviceLostHandler` / `SetRenderingErrorHandler`.

**Window**

- `Window::Create` **no longer shows the window automatically**; the app decides when to `Show()`.
- Custom title bar: added **per-button** enable/visible control: `TitleBar::GetButton / SetButtonEnabled / IsButtonEnabled / SetButtonVisible / IsButtonVisible`; `DefaultTitleBar` convenience `SetMinimizeEnabled / SetMaximizeEnabled / SetCloseEnabled` and `...Visible`.
- `DWMWA_BORDER_COLOR` now defaults to the system default (no hard-coded grey).

**Fixes and cleanup**

- ComboBox popup hover/click hit-testing now uses the same width as drawing (`ListWidth()`), fixing the unreachable overhang on the right.
- `TableView::SortByColumn` remaps `checkedRows_` after sorting.
- Double-click detection now uses the system `GetDoubleClickTime()` (three places).
- `ScrollViewer::Arrange` uses `SetVisibleNoInvalidate()` to avoid a layout loop; `DrawTextWithEllipsis` truncation changed from O(n²) to binary search.
- `MenuWindow` positioning now uses `MonitorFromPoint` + `GetMonitorInfo`.
- Removed several fallback/temporary bits (debug-only `PageHost` special case, unused `animationIdleFrames_`, etc.).

**Demo (`ZufyUI.cpp`)**

- The main window now uses a **custom title bar**.
- The separate small tool window was removed; the multi-window / owned / modal demos moved into a new **"Multi-window" page** of the main window.
- Version bumped to **1.8.0** (new `ZufyUI_VERSION_STRING` etc. macros, referenced by the demo title).

### 2026-09-16 — Custom title bar (ZufyUIWindowTool) and backdrop modes

**New: custom title bar (new file `ZufyUIWindowTool.h`)**

- `TitleBar` / `CaptionButton` / `DefaultTitleBar`: window-level controls installed as a non-layout overlay via `Window::SetCustomTitleBar`; pass `nullptr` to restore the native title bar.
- The three caption buttons are drawn with the system icon font (`Segoe Fluent Icons` / `Segoe MDL2 Assets`, glyphs `E921/E922/E923/E8BB`), with **hover/press gradients** and red close hover `#C42B1C`; buttons report `HTMINBUTTON/HTMAXBUTTON/HTCLOSE` from `WM_NCHITTEST`, enabling the system **Snap Layouts**; clicks are handled in `WM_NCLBUTTONUP`; the title bar area is the **drag region** (drag / Aero Snap / double-click maximize handled by the system).
- The title bar and buttons are **not Tab-focusable**; default `DrawAfterLayout`, `UseCache`.
- Helpers: `SetTitleBarVisible`, `SetButtonsEnabled`, `SetButtonWidth/Height`, `SetRightMargin`, `CaptionButton::SetAnimationSpeed`, etc.

**New: layout participation / drag mechanism (`UIElement`)**

- `LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout }`: non-participating elements are drawn by the window before/after the normal layout tree; layouts and the `ComposeImpl` child recursion skip them.
- Drag regions: `SetDraggable / SetDragRegion / CollectDragRegions`; the window collects them each frame and feeds `WM_NCHITTEST` (returns `HTCAPTION`).

**New: three backdrop modes**

- `BackdropMode { Auto, Acrylic, SystemBackdrop }` + `SystemBackdropMaterial { Auto, Mica, MicaAlt, Acrylic }` + `SetBackdropUnsupportedHandler`.
- `SystemBackdrop` uses Win11 `DWMWA_SYSTEMBACKDROP_TYPE`; if unsupported the handler is called and it falls back to `AccentState`.

**Window / frame fixes**

- Snapped windows keep the DWM border and shadow: `WM_NCCALCSIZE` insets only the edges snapped to the work area (using the actual `DWMWA_VISIBLE_FRAME_BORDER_THICKNESS`), and maximizing does not inset; `SWP_FRAMECHANGED` is sent when the maximize/snap state changes.
- From maximized, dragging down from the top edge restores the window; the close button aligns with the native position.

**Demo**: `ZufyUI.cpp` gained a "custom title bar window".

**Known limitations (important)**

- ZufyUI currently renders through `ID2D1HwndRenderTarget` (an opaque redirection surface) and **cannot display DWM system materials**: `BackdropMode::Auto` uses AccentState on both Win10 and Win11, and `SystemBackdrop` is **not visible** until the render target is migrated to **DirectComposition** (it looks gray/black).
- AccentState acrylic is an undocumented API and makes DWM **skip window transition animations** (minimize/maximize) on both Win10 and Win11. Getting "acrylic + native animations + Mica" together requires migrating rendering to DirectComposition (SwapChain / CompositionSurface), which is planned.

### 2026-09-15 — Image system, window mechanisms, and root-cause fixes

**Images (new `ZufyUIImages.h`)**

- New `Image`: WIC decoding + Direct2D (GPU) drawing/transforms. Loading: `FromFile / FromMemory / FromBase64 / FromResource(HMODULE,name,type) / FromResource(id,type) / FromHBITMAP / FromHICON`; transforms: `Scaled / ScaledToWidth / ScaledToHeight / Rotated / Mirrored / Cropped` (lightweight descriptors applied by the GPU at draw time); drawing: `Draw` (opacity / interpolation), `Bake`; encoding: `Save / Encode` (`CreateStreamOnHGlobal` memory stream, **no temp files**).
- `ImageManager`: shared `IWICImagingFactory` and device-cache registry; `ImageDeviceCache`: one `ID2D1Bitmap` per render target, rebuilt on device loss. The same image is uploaded to the GPU once per window.
- **Root-cause fix**: `IWICStream::InitializeFromMemory` does not copy the buffer and decoding/conversion are lazy; pixels are now copied into an independent WIC bitmap immediately at decode time via `WICBitmapCacheOnLoad`, so `FromMemory/FromBase64/RT_BITMAP` no longer read freed memory while drawing (previously the image **never appeared**).
- **Root-cause fix**: the transform matrix composition in `DrawWithTransform` was reversed, pushing rotated/mirrored images out of the target rect (they appeared **missing**); fixed by using the `Rotation/Scale` overloads that take a `center`.
- `Label` now supports an **icon and nested children**: `SetImage/GetImage`, `SetIconSize/GetIconSize`, `SetIconSpacing/GetIconSpacing`, `AddChild/ClearChildren/GetChildCount` (icon + text + children laid out inline).

**Windows**

- `RunModal` now uses the official mechanism: `EnableWindow(owner, FALSE)` + activating the modal window + an `IsDialogMessage` nested loop; the **global low-level mouse hook was removed** (it used to swallow clicks for other processes, breaking the whole desktop's input while modal).
- Owned child windows now establish the owner relationship **at creation** (via the `CreateWindowEx` parent parameter), so they have no separate taskbar button; when the owner is minimized their owned children are hidden **unconditionally** and restored on restore/activate (no longer relying on the shell's minimize grouping, which fails when the owner is in the background).
- New convenience APIs: `Flash(times=5, captionOnly=true)` / `StopFlash()` (`FlashWindowEx`), `GetOwnedWindows()`, `GetStyle()/GetExStyle()`, `SetWindowStyleFlag/SetWindowExStyleFlag` (e.g. `WS_EX_TOOLWINDOW`), `Show()/ShowNoActivate()/Hide()/Raise()`.
- New `OwnedMinimizePolicy { None, Hide, DisableMinimize }` + `SetOwnedMinimizePolicy` for how an owned child handles being minimized on its own.
- **Fix**: on destroy a window clears every reference other windows hold to it (`owner_` / hidden lists), so address reuse can never affect unrelated windows.
- **Fix**: `SIZE_RESTORED` also fires while dragging to resize, which used to unconditionally restore hidden owned children (resizing made them pop back); now they are restored only on a real "minimize -> restore".

**Controls / rendering**

- **Root-cause fix**: when an element cache bitmap is composited onto the main render target, the destination pixel size must exactly match the bitmap's pixel size; otherwise any interpolation resamples the whole cache and text looks blurry. Now the destination size is derived from the bitmap's actual pixel size.
- `ComboBox`: the collapsed box sizes to the **average item text width** (overflow is ellipsized with an automatic `ToolTip` of the full text); the drop-down list sizes to the **widest item** so every option is fully visible; fixed the scrollbar being drawn in the middle when the list is wider than the box.

**Docs**

- API docs gained an **Images** chapter (zh/en), plus new `Window`, `Label`, and `ComboBox` APIs and notes.

### 2026-09-13 — Major update: interaction states, shadows / tooltips, controls and data views

**Core**

- `UIElement` gained: enabled/disabled (`SetEnabled/IsEnabled/IsEffectivelyEnabled` — disabled state inherits from the parent chain, blocks mouse/keyboard input, and controls grey themselves), generic tooltips (`SetToolTip/GetToolTip`), shadows (`SetShadow/SetShadowColor/SetShadowBlur/SetShadowOffset/SetShadowCornerRadius/GetShadowExtent`), and a context-menu hook `OnContextMenu`.
- `Window`: element shadows are composited into the offscreen cache; a shared tooltip overlay (anchored at a fixed offset above the mouse position when shown, fades in after a 0.5s hover, white background with black text and a soft shadow, dismissed as soon as the mouse moves); Tab focus traversal (the focus ring is shown only for Tab navigation, not for mouse clicks).
- Shadows rewritten as **layered Gaussian CDF** (`DrawSoftShadow`): per-layer alpha is derived from the Gaussian distribution so the composite approximates `targetA·(1-Φ(d/σ))`; the alpha of `SetShadowColor` now means the *visible edge* opacity (the interior is roughly 2×).
- Frame-time clamping: `deltaTime` in `OnPaint` is capped at `0.033s`, fixing animations that jumped straight to their end after an idle period or a restore from minimize.
- `PageHost`: fixed a bug where, during a page transition, the same page was updated twice per frame, doubling the speed of animations nested inside it.
- Timer precision: `timeBeginPeriod(1)` on window creation and `timeEndPeriod(1)` on destruction reduce animation jitter.
- Debug output is now controlled by the `ZufyUI_DEBUG` macro (off by default; define it to enable); Release hot paths no longer build debug strings.
- Performance / memory: `GetChildren()` now returns `const&` (reused buffer, no per-frame allocation); `ZSignal::Fire` uses a thread-local snapshot; `Compose` culls subtrees entirely outside the clip; the active-animation set reuses a buffer; `DrawSoftShadow` uses fixed-size arrays to avoid per-frame heap allocation.

**Basic controls**

- `Button`: disabled state, Enter/Space activation, checkable mode (`SetCheckable/SetChecked/IsChecked/Toggled`), text alignment / padding, auto-repeat.
- `CheckBox`: hover halo animation, text label and color, hover box color, keyboard, disabled.
- `ToggleSwitch`: disabled, text label, keyboard, `SetSize`, indeterminate state, custom colors.
- `Label`: padding, line spacing, max lines (with ellipsis), `GetDesiredSize`, disabled color.
- `ProgressBar`: `ValueChanged`, range, percentage text with text color, disabled.
- `Slider`: `SliderReleased`, stepping / snapping (`SetStep/SetSnapToStep`), arrow keys / Home / End, disabled.
- `ScrollViewer`: scrollbar visibility policy (`Auto/Always/Hidden`), `ScrollChanged`, `GetScrollOffset`, per-instance colors, `ContentMargin`.
- `TextBox`: read-only (`SetReadOnly`), input filter (`SetInputFilter`), `ReturnPressed`, public selection / undo / copy-paste / select-all, placeholder color, password reveal (`SetRevealPassword`); fixed Shift and mouse-drag selection not accumulating (independent anchor).
- `ComboBox`: data add/remove/query, placeholder, per-item disabled, max visible items, open/close signals (`DropDownOpened/DropDownClosed`), **editable + input filtering** (`SetEditable/SetFilterEnabled/SetEditText`, with a blinking caret and click positioning).

**Data views**

- `ListView`: new `None` selection mode; type-ahead; sort comparator + indicator; per-item disabled / text color / tooltip (**bound to the item itself, so they survive sorting**); keyboard navigation skips disabled items.
- `TableView`: per-column sorting + indicator (row-level metadata is remapped when sorting or inserting/removing rows/columns); cell text color / tooltip; per-row disabled; column hiding; column alignment; per-row height; keyboard navigation skips disabled rows.
- `TreeView`: filter / search, `GetNodePath`, default expand depth; per-node `tooltip` wired to the shared tooltip in the base class.

**Docs**

- Added this changelog; the API docs were rewritten from a "list of signatures" into a more detailed "implementation notes + pitfalls" style; added a warning about signal/object lifetime reference cycles.

### 2026-09-13 (cont.) — Multi-window support (`Application`)

- New `ZufyUI::Application`: `app.CreateWindow(...)` to create windows and `app.Run()` for a single shared message loop, Qt style; windows are truly independent.
- Repaint/layout are routed per owning window (`UIElement::GetWindow()`); the DPI scale is now **thread-local** (set per window before drawing); `Window` gained instance signals `Activated` / `Deactivated` / `Closed`, and `UIZSignals::WindowDeactivated` / `GlobalMouseDown` now carry a `Window*`.
- Closing one window no longer quits the app; it quits when the **last** window closes. The `ID2D1Factory` and `timeBeginPeriod` are shared by the app core.
- All window-related global signals now carry a `Window*` (`DrawOverlay` / `GlobalMouseDown` / `WindowDeactivated`, plus capture and repaint fallbacks); subscribers filter with `GetWindow()`. This fixes the cross-talk where window A's popup was drawn onto window B or clicking B wrongly collapsed A. Added `Window::SetPosition/SetSize/IsValid`.
- Fixed ComboBoxes holding the thread-wide system mouse capture after expanding (which made other windows unusable): Win32 capture is now held only while the mouse button is down and released on mouse-up (element-level logical capture is unaffected); also handled `WM_CAPTURECHANGED`.
- Added modal and owned windows: `Window::SetOwner` / `GetOwner`, `Window::RunModal(owner)`, `Application::CreateWindow(..., owner)`; while modal, clicking the disabled owner **flashes** the modal window.
- Robustness: elements store the owning window as an **id** (instead of a raw pointer); after the window is destroyed `GetWindow()` returns nullptr, removing crashes from dangling window pointers.
- Fixed `PageHost` calling `ReleaseDeviceResources()` on non-current pages **every frame** after a transition finished (now released once, on the completing frame); non-current pages are made invisible, so expanded controls (ComboBox) auto-collapse.
- Fixed `UIElement::Connect` returning a moved-from empty `Connection` (now returns nothing); fixed `TreeView::RemoveChildren` not clearing `checkAnim_` (dangling node keys).
- Added `AGENTS.md` codifying the principle: for any bug, find and fix the root cause first — never mask it with a new mechanism.
- Backward compatible: `Window win; win.Create(...); win.Run();` still works. `#undef CreateWindow` avoids the Win32 macro clash.

## Documentation

- Online documentation: <https://zhc9968.github.io/ZufyUI/>
- API Reference (English, 15 chapters): <https://zhc9968.github.io/ZufyUI/docs/API.en.html>
- API 参考（中文）: <https://zhc9968.github.io/ZufyUI/docs/API.html>

## License

This project is licensed under the **MIT** License; see [LICENSE](LICENSE) for details.
