# ZufyUI

> **Renamed: `ZUI` → `ZufyUI`** (to avoid clashes with other projects of the same name). Old repository URLs are redirected by the platform to the new address (see links below).

> A C++ native Win32 custom-drawn UI framework based on Direct2D, with layout, controls, signals/slots, and high-DPI adaptation.

> Language: **English** · [简体中文](README.md)

ZufyUI is a **header-only** Windows desktop UI framework built directly on Direct2D / DirectWrite / DWM. It does not depend on Qt, MFC, or any third-party library; it packs controls, layout, animation, fonts, data views, and signals/slots into a handful of `.h` files, making it suitable for native C++ applications that want a lightweight, controllable, modern look.

- Repository: GitHub <https://github.com/zhc9968/ZufyUI> · Gitee <https://gitee.com/zhc9968/ZufyUI>
- License: MIT
- Online docs: <https://zhc9968.github.io/ZufyUI/>
- API Reference (English): <https://zhc9968.github.io/ZufyUI/docs/API.en.html>

> ⚠️ **The independent render thread (compile switch `ZUFYUI_RENDER_THREAD`) is experimental: off by default, unstable, buggy, and not recommended.** See the "Independent render thread" section in the API reference.

## Features

- **Direct2D custom drawing**: hardware-accelerated rendering, Acrylic background, rounded corners, and shadows for a modern look.
- **Complete layout system**: `ColumnBox` / `RowBox` / `GridLayout`, supporting spacing, stretch weight, fill, alignment, and row/column spanning.
- **Signals and slots**: built-in `ZSignal` / `Connection`, supporting `Connect` with automatic lifetime management, plus three dispatch strategies: current thread / new thread / UI thread.
- **Rich controls**: labels, buttons, text boxes, combo boxes, toggle switches, scroll containers, progress bars, sliders, and three data views: list / table / tree.
- **Charts**: `BarChart` / `LineChart` / `PieChart` (horizontal orientation, area fill, wheel zoom, live refresh) from `ZufyUICharts.h`.
- **Drag & drop**: built-in OLE drag & drop (`ZufyUIDragDrop.h`) — drop text / files / custom formats in, and drag text / images / files out to Explorer; text boxes, combo boxes, images, buttons, and labels are all wired up; supports element-level and **whole-window** reception (`Window::SetDropTargetEnabled`).
- **Card components**: collapsible card `Expander` (easing + rotating arrow + header hover highlight + `SetIcon`), settings row list `SettingsList`; image display via `Label::SetImage` + `SetImageFit` (fit to fully visible, drag-out).
- **Window chrome**: top **menu bar** `MenuBar` (VS-style; overlays the custom title bar by default, switchable) and bottom **status bar** `StatusBar` (left/right panels: icon + text).
- **Theme system**: semantic `Theme` palette (light / dark / high-contrast), app- and window-level switching, **follows the system** light/dark + accent (auto re-resolve on `WM_SETTINGCHANGE` etc.), `ThemeManager::Map` color remap; **explicit instance colors win** (once set, the theme won't override them).
- **Fluent construction DSL** (`ZufyUIDsl.h`, optional): `Ref<T>` with `with/on/add/apply`, `Make`/`Wrap`/`Col`/`Row`/`Grid` (including `Cell(row,col,...)` coordinates) and per-control short factories — assemble controls / subtrees / signals in one expression; a `dsl::App()` window builder (config + `hidden` + window signals + automatic `Show()`); SFINAE-gated sugar covering layout, size, weights, colours, spacing, alignment and data-view / menu builders.
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
├── ZufyUI.h              # Core: types / signals & slots / fonts / elements / layout / menus / window / theme
├── ZufyUIWidgets.h       # Basic controls: Label / Button / TextBox / TextEdit / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider / MenuBar / StatusBar …
├── ZDataViewer.h         # Data views: ListView / TableView / TreeView
├── ZufyUICharts.h        # Charts: ChartBase / BarChart / LineChart / PieChart
├── ZufyUIAcrylic.h       # DComp composition effects + acrylic / mica recipes (included by ZufyUI.h)
├── ZufyUIImages.h        # Image / ImageManager (image loading and device cache)
├── ZufyUIIcons.h         # FontIcon glyph control
├── ZufyUIWindowTool.h    # Tray / custom title bar / accessibility (UIA) / system dialogs / debug channel
├── ZufyUIDragDrop.h      # Drag & drop: DragData / DragDataBuilder / Window::BeginDrag / element hooks
├── ZufyUIDsl.h           # Optional: fluent construction DSL (Ref / Col / Row / Grid / short factories / dsl::App)
├── ZufyUI.cpp            # Demo program entry point (wWinMain)
├── ZufyUI.slnx           # Solution
├── ZufyUI.vcxproj        # Project file
├── AGENTS.md             # Development principles (incl. "find the root cause before fixing any bug")
├── HANDOFF.md            # Handoff notes (hard rules / architecture / pitfalls / release flow)
├── TODO.md               # Single authoritative backlog
├── docs/
│   ├── API.md            # Full API documentation (Chinese)
│   ├── API.en.md         # Full API documentation (English)
│   └── pitfalls.md       # Pitfall index
├── tests/
│   └── dsl_tests.cpp     # DSL self-test (compile-time + runtime assertions, no window)
└── tools/
    ├── build.ps1         # One-shot build: fix BOM → build demo + Debugger → run self-tests
    ├── ensure_bom.ps1    # BOM guard (restores UTF-8 BOM before building)
    ├── run_dsl_tests.ps1 # Compile and run the DSL self-test
    ├── check_api_parity.py # zh/en API code-block parity check
    └── colorscan.py      # Hard-coded colour audit
```

## Quick start

> Goal: **get your first window on screen within 10 minutes** and know which chapter to read next. Follow the steps below.

**Step 0: setup (header-only, zero build config)**
- Add the directory containing `ZufyUI.h`, `ZufyUIWidgets.h`, `ZDataViewer.h`, `ZufyUICharts.h`, `ZufyUIWindowTool.h`, `ZufyUIDragDrop.h` to your **include path**; **nothing to compile**, no third-party deps.
- Required system libs are auto-linked via `#pragma comment(lib, ...)`: `d2d1 / dwrite / dwmapi / imm32 / winmm`.
- GUI entry point: **`wWinMain` + `/SUBSYSTEM:WINDOWS`**.
- **⚠ Entry point pitfall**: if you insist on `int main()`, set `/SUBSYSTEM:CONSOLE`, or add `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")`, otherwise the linker won't find an entry point.

**Step 1: minimal window + a button + a toggle**

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

Key points (what each step does):

- `Window::Create` creates the window, initializes Direct2D, applies the background, and generates a default root `ColumnBox` (margin 20, spacing 10);
- Create controls with `std::make_shared<T>()` and attach them with `AddChild`; **a parent owns its children via `shared_ptr`**;
- Bind events with `Connect(signal, slot)`; connections auto-disconnect when the control is destroyed;
- `win.Run()` enters the message loop; all windows/controls live on the **same UI thread**.
- **⚠ Units**: coordinates/sizes are **DIP** (not physical pixels); the framework handles DPI snapping, so you don't.

Result (that exact code above):

![Quick start: a window + a button + a toggle](docs/images/quickstart.png)

**Steps 2 / 3: change the background, change the title bar**

**Step 2: change the background.** Only three backdrop layers exist — `None / Acrylic / Mica` — with an ARGB tint composited on top:

```cpp
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // Mica + fully transparent tint
```

![Backdrop: manual Mica](docs/images/backdrop-mica.png)

**Step 3: a custom title bar.** Swap the title bar (plus switch to Mica so it stands out):

```cpp
#include "ZufyUIWindowTool.h"   // DefaultTitleBar lives here

auto bar = std::make_shared<DefaultTitleBar>();
win.SetCustomTitleBar(bar);
win.SetBackdrop(Backdrop::Mica, 0x00000000);   // also switch to Mica
```

![Custom title bar](docs/images/custom-titlebar.png)

A tour of the widgets (first page of the demo):

![ZufyUI demo](docs/images/demo-main.png)

**Step 4: what to read next**
- Layout & controls overview: [API Reference · Layout / Basic controls](docs/API.en.md)
- Data UI (list / table / tree): the "Data views" chapter
- Charts: the "Charts" chapter (`BarChart` / `LineChart` / `PieChart`)
- Window extras (title bar / tray / system dialogs): the "Window extras" chapter
- Accessibility & debug/automation: the "Accessibility & Debug Channel" chapter

**Three things to always remember**
1. **DIP**: all sizes/coordinates are DIP — no manual DPI math.
2. **`shared_ptr` = ownership**: own controls with `make_shared`, attach with `AddChild`; dropping the only `shared_ptr` destroys it.
3. **Entry point / subsystem**: `wWinMain` + `/SUBSYSTEM:WINDOWS` (or the "Entry point pitfall" above).


## Version history

> Current version: **1.23.0** (source of truth: `ZUFYUI_VERSION_STRING` in `ZufyUI.h`).
> Only "roughly what changed/fixed" (noun level); full per-item details are in [CHANGELOG.en.md](CHANGELOG.en.md).

- **1.23.0**: `HwndView` native window host; `Window` external visual attachment; overlay sync/performance; `NumberBox` error state; data-view fill.
- **1.22.1**: layout overflow/alignment; IME/surrogate pairs; accessibility id/hit; focus-ring theme; pch/MarkUserColor.
- **1.22.0**: API debt (default-arg overloads, `make_*`, `SetScrollableContent`, `HookHandle`); MessageBox first click; ScrollViewer scroll chaining; SplitView expand easing.
- **1.21.0**: SplitView collapse; render/layout/chart fixes; demo home/menu/status bar; unified animation.
- **1.20.0**: DSL `root`/`add`/`Cell`/`apply`/`Wrap`; `WindowApp`; `build.ps1`; DSL tests.
- **1.19.7**: chart scrollbars; NumberBox wheel; anim-set dangling; `Menu::ShowAt`; layout spacing; data-view release; IME; `TODO.md`.
- **1.19.6**: ListView checkbox hit; Label wrap width; single-source version.
- **1.19.5**: DSL `App`/`WindowApp` + SFINAE sugar + chart composition.
- **1.19.4**: DSL chaining; CheckBox label clipping; data-view cache signature; chart fill fallback.
- **1.19.3**: theme system + follow system + explicit instance colours.
- **1.19.2**: MenuBar / StatusBar; Menu `onClosed`.
- **1.19.1**: TextEdit undo/redo; drag & drop; ScrollViewer viewport measure; `Label::SetImageFit`.
- **1.19.0**: drag & drop; `Expander`/`SettingsList`; ComboBox arrow; charts.
- **1.18.1**: docs/screenshots update.
- **1.18.0**: charts `ZufyUICharts.h`; `TextEdit`; `PageHost` deferred release; docs rewrite.
- **1.17.0**: UIA; debug channel; layout weights; Label/Button auto ToolTip; TreeView bg/scroll; title-bar Pin.
- **1.16.0**: render thread off by default; system dialogs; `UIZSignals::Error`; cursor/hover/first-column fixes.
- **1.15.0**: data-view virtualization; cross-thread cleanup; ComboBox source index.

> Older versions (≤ 1.14) are in [CHANGELOG.en.md](CHANGELOG.en.md).

## Documentation

- Online documentation: <https://zhc9968.github.io/ZufyUI/>
- API Reference (English, 15 chapters): <https://zhc9968.github.io/ZufyUI/docs/API.en.html>
- API 参考（中文）: <https://zhc9968.github.io/ZufyUI/docs/API.html>

## License

This project is licensed under the **MIT** License; see [LICENSE](LICENSE) for details.
