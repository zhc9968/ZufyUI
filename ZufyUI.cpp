// main.cpp - ZufyUI 综合自动化布局测试（使用 Connect 自动管理连接）
#include "pch.h"
#include "resource.h"
#include "ZDataViewer.h"
#include "ZufyUIWindowTool.h"
#include "ZufyUICharts.h"
#include "ZufyUIDsl.h"

using namespace ZufyUI;

namespace {
    // 状态栏刷新：把"当前页 / 主题 / 背景 / DPI·客户区 / 窗口状态"写进 5 个面板。
    //   做成结构体而不是 lambda：主题钩子是全局的，捕获裸指针/状态对象更直观，也避免 lambda 相互捕获。
    inline const wchar_t* kPageNames[] = {
        L"首页", L"基础控件", L"输入与滚动", L"嵌套与联动", L"列表视图", L"表格视图",
        L"树形视图", L"树形增强", L"图像", L"多窗口", L"窗口属性", L"新控件A", L"新控件B",
        L"多行文本", L"条形图", L"折线图", L"饼图", L"拖放", L"设置卡片", L"主题", L"DSL", L"环境组合", L"原生HWND"
    };
    inline constexpr int kPageCount = (int)(sizeof(kPageNames) / sizeof(kPageNames[0]));

    // ---- 各页共用排版助手（原来每页各抄一份：section 14 份 / accent 7 份 / Scroll 包装 15 份）----
    inline void AddSection(const std::shared_ptr<ColumnBox>& col, const std::wstring& title) {
        if (!col) return;
        auto lbl = std::make_shared<Label>(title);
        lbl->SetMargin(Thickness(0, 10, 0, 0));
        col->AddChild(lbl);
    }
    inline Color ThemeColor(ThemeRole r) { return ThemeManager::AppTheme().Get(r); }
    // 整页纵向滚动包装：现在直接调用 Page::SetScrollableContent（一行）。
    inline void SetScrollablePage(const std::shared_ptr<Page>& page, const std::shared_ptr<UIElement>& content) {
        if (page) page->SetScrollableContent(content);
    }

    struct DemoStatus {
        StatusBar* sb = nullptr;
        Window* win = nullptr;
        std::wstring page = L"首页";
        std::wstring state = L"就绪";

        void Refresh() const {
            if (!sb || !win) return;
            const Theme& t = ThemeManager::AppTheme();
            std::wstring theme = t.isDark ? (t.cornerRadius <= 0.0f ? L"高对比度" : L"深色") : L"浅色";
            Backdrop bd = win->GetBackdrop();
            std::wstring bdName = (bd == Backdrop::Acrylic) ? L"亚克力" : (bd == Backdrop::Mica) ? L"云母" : L"无";
            std::wstring dpi = L"DPI -";
            if (HWND h = win->GetHwnd()) {
                int d = (int)GetDpiForWindow(h);
                RECT cr{}; GetClientRect(h, &cr);
                wchar_t b[96];
                swprintf(b, 96, L"DPI %d (%.0f%%) · %d×%d", d, d * 100.0 / 96.0, (int)(cr.right - cr.left), (int)(cr.bottom - cr.top));
                dpi = b;
            }
            sb->SetPanelText(0, L"页面: " + page);
            sb->SetPanelText(1, L"主题: " + theme);
            sb->SetPanelText(2, L"背景: " + bdName);
            sb->SetPanelText(3, dpi);
            sb->SetPanelText(4, state);
        }
    };

    // 库自带 MessageBox 的薄封装：统一替代系统 MessageBoxW（自带主题外观，按钮有点击反馈）。
    //   传 owner 时作为模态父窗口（更贴近原生观感）；不传则退化为无属主顶层弹窗。
    inline void ZuiInfo(const std::wstring& title, const std::wstring& text, Window* owner = nullptr) {
        if (owner) { MessageBox box(owner, title, text, MessageBox::Icon::None, FastButton::OK); }
        else       { MessageBox box(title, text, MessageBox::Icon::None, FastButton::OK); }
    }

    // ---- 原生 HWND 测试：GDI 画「全色条 + 彩虹渐变 + 移动扫描线」；静态部分缓存到 DIB，每帧只 BitBlt ----
    struct NativeHostPaint {
        int tick = 0; HFONT font = nullptr; int seed = 0; bool quitOnDestroy = false;
        HDC memDC = nullptr; HBITMAP memBmp = nullptr; HGDIOBJ oldBmp = nullptr;
        int bw = 0, bh = 0; bool patternValid = false;
    };

    inline void HsvToRgb(float h, int& r, int& g, int& b) {   // h in [0,1)
        float s = 1.0f, v = 1.0f;
        float c = v * s, x = c * (1 - fabsf(fmodf(h * 6.0f, 2.0f) - 1)), m = v - c;
        float rr = 0, gg = 0, bb = 0;
        int i = (int)(h * 6.0f) % 6;
        switch (i) {
        case 0: rr = c; gg = x; break;
        case 1: rr = x; gg = c; break;
        case 2: gg = c; bb = x; break;
        case 3: gg = x; bb = c; break;
        case 4: rr = x; bb = c; break;
        default: rr = c; bb = x; break;
        }
        r = (int)((rr + m) * 255); g = (int)((gg + m) * 255); b = (int)((bb + m) * 255);
    }

    inline LRESULT CALLBACK NativeHostProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* st = reinterpret_cast<NativeHostPaint*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            if (st && rc.right > 0 && rc.bottom > 0) {
                if (!st->memDC) { HDC sdc = GetDC(h); st->memDC = CreateCompatibleDC(sdc); ReleaseDC(h, sdc); }
                if (st->bw != rc.right || st->bh != rc.bottom) {   // 尺寸变 → 重建缓存位图
                    if (st->oldBmp) { SelectObject(st->memDC, st->oldBmp); st->oldBmp = nullptr; }
                    if (st->memBmp) { DeleteObject(st->memBmp); st->memBmp = nullptr; }
                    BITMAPINFO bmi{};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = rc.right;
                    bmi.bmiHeader.biHeight = -rc.bottom;   // top-down
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    void* bits = nullptr;
                    st->memBmp = CreateDIBSection(st->memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
                    if (st->memBmp) st->oldBmp = SelectObject(st->memDC, st->memBmp);
                    st->bw = rc.right; st->bh = rc.bottom; st->patternValid = false;
                }
                if (st->bw > 0 && st->bh > 0) {
                    if (!st->patternValid) {   // 静态图案只画一次（全色条 + 彩虹渐变）
                        static const COLORREF kCols[8] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(0,255,255),
                                                           RGB(255,0,255), RGB(255,255,0), RGB(255,255,255), RGB(0,0,0) };
                        int topH = st->bh * 2 / 3, barW = (st->bw + 7) / 8;
                        for (int i = 0; i < 8; ++i) {
                            HBRUSH b = CreateSolidBrush(kCols[(i + st->seed) & 7]);
                            RECT r{ i * barW, 0, min(st->bw, (i + 1) * barW), topH };
                            FillRect(st->memDC, &r, b); DeleteObject(b);
                        }
                        int gh = max(1, st->bh - topH);
                        for (int y = topH; y < st->bh; ++y) {
                            int rr, gg, bb; HsvToRgb((float)(y - topH) / gh, rr, gg, bb);
                            HBRUSH b = CreateSolidBrush(RGB(rr, gg, bb));
                            RECT r{ 0, y, st->bw, y + 1 };
                            FillRect(st->memDC, &r, b); DeleteObject(b);
                        }
                        st->patternValid = true;
                    }
                    if (st->memBmp) BitBlt(dc, 0, 0, st->bw, st->bh, st->memDC, 0, 0, SRCCOPY);
                }
                // 动态部分：移动扫描线 + tick 文本
                int sx = (st->tick * 5 + st->seed * 41) % max(1, rc.right);
                HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
                HGDIOBJ old = SelectObject(dc, pen);
                MoveToEx(dc, sx, 0, nullptr); LineTo(dc, sx, rc.bottom);
                SelectObject(dc, old); DeleteObject(pen);
                if (st->font) {
                    SelectObject(dc, st->font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(0, 0, 0));
                    wchar_t buf[160];
                    swprintf(buf, 160, L"HWND #%d   %d×%d px   tick=%d", st->seed, rc.right, rc.bottom, st->tick);
                    DrawTextW(dc, buf, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
                }
            }
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_TIMER: if (st) { st->tick++; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_DESTROY: if (st && st->quitOnDestroy) PostQuitMessage(0); break;
        case WM_NCDESTROY:
            if (st) {
                if (st->oldBmp) { SelectObject(st->memDC, st->oldBmp); st->oldBmp = nullptr; }
                if (st->memBmp) { DeleteObject(st->memBmp); st->memBmp = nullptr; }
                if (st->memDC) { DeleteDC(st->memDC); st->memDC = nullptr; }
                if (st->font) DeleteObject(st->font);
                delete st;
                SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            }
            break;
        }
        return DefWindowProcW(h, m, w, l);
    }

    // 建一个自绘的全色图窗口（顶层 popup），交给 HwndView 定位托管（不改 HwndView 的 WNDPROC）
    inline HWND CreateNativePaintWindow(HWND owner, int seed) {
        static const wchar_t* kClass = L"ZufyUI_NativePaint";
        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = NativeHostProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = kClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassExW(&wc);
            registered = true;
        }
        HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClass, L"", WS_POPUP,
            0, 0, 1, 1, owner, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!h) return nullptr;
        auto* st = new NativeHostPaint();
        st->seed = seed;
        st->font = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)st);
        SetTimer(h, 1, 100, nullptr);
        return h;
    }

    // 进程外生产者：由同一个 exe 的 --outproc 分支启动，创建独立进程里的窗口（自己画全色图）。
    inline int RunOutProcProducer() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = NativeHostProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"ZufyUI_OutProcProducer";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        // 先不显示：等主进程的 HwndView 定位/显示它
        HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"ZufyUI_OutProcProducer", L"", WS_POPUP,
            200, 200, 360, 240, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!h) return 1;
        auto* st = new NativeHostPaint();
        st->seed = 9;
        st->quitOnDestroy = true;
        st->font = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)st);
        SetTimer(h, 1, 100, nullptr);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        return 0;
    }


    // 主题色卡：按 ThemeRole 顺序画出当前应用主题的每个角色（色块 + 名称 + 十六进制），随主题变化刷新
    struct ThemeSwatch : public UIElement {
        Size MeasureOverride(const Size& avail) override {
            float w = (avail.width != FLT_MAX && avail.width > 0) ? avail.width : (width_ > 0 ? width_ : 600.0f);
            int per = max(1, (int)((w + 6.0f) / (118.0f + 6.0f)));
            int rows = ((int)ThemeRole::Count + per - 1) / per;
            return Size(w, rows * (44.0f + 6.0f));
        }
        void Draw(ID2D1RenderTarget* rt) override {
            const Theme& t = ThemeManager::AppTheme();
            static const wchar_t* names[] = { L"Text",L"TextSecondary",L"TextDisabled",L"TextOnAccent",L"Surface",L"SurfaceAlt",L"SurfaceRaised",L"Backdrop",L"Overlay",L"Control",L"ControlHover",L"ControlPressed",L"ControlDisabled",L"Accent",L"AccentHover",L"AccentPressed",L"Border",L"Divider",L"FocusRing",L"Selection",L"ScrollTrack",L"ScrollThumb",L"ScrollThumbHover",L"Danger",L"Warning",L"Success",L"Info",L"Shadow" };
            const float cw = 118.0f, ch = 44.0f, gap = 6.0f;
            int per = max(1, (int)((arrangedRect_.width + gap) / (cw + gap)));
            FontSpec fs; fs.size = 11.0f;
            IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(fs);
            for (int i = 0; i < (int)ThemeRole::Count; ++i) {
                int r = i / per, c = i % per;
                float x = arrangedRect_.x + c * (cw + gap), y = arrangedRect_.y + r * (ch + gap);
                Color col = t.Get((ThemeRole)i);
                ComPtr<ID2D1SolidColorBrush> b; rt->CreateSolidColorBrush(col.ToD2D(), b.GetAddressOf());
                if (b) rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + cw, y + ch), 4, 4), b.Get());
                float lum = 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
                Color fg = (lum > 0.55f) ? Color::FromArgb(255, 0, 0, 0) : Color::FromArgb(255, 255, 255, 255);
                wchar_t hex[24]; swprintf(hex, 24, L"#%02X%02X%02X", (int)(col.r * 255 + 0.5f), (int)(col.g * 255 + 0.5f), (int)(col.b * 255 + 0.5f));
                ComPtr<ID2D1SolidColorBrush> tb; rt->CreateSolidColorBrush(fg.ToD2D(), tb.GetAddressOf());
                if (tb && fmt) {
                    DrawTextWithEllipsis(rt, names[i], D2D1::RectF(x + 5, y + 4, x + cw - 4, y + ch * 0.5f), fg.ToD2D(), fs, tb, fmt, true, TextHAlign::Left);
                    DrawTextWithEllipsis(rt, hex, D2D1::RectF(x + 5, y + ch * 0.5f, x + cw - 4, y + ch - 3), fg.ToD2D(), fs, tb, fmt, true, TextHAlign::Left);
                }
            }
        }
    };
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 进程外测试：--outproc 分支只创建一个独立窗口（自己画全色图），给主进程的 HwndView 托管
    if (lpCmdLine && strstr(lpCmdLine, "--outproc")) return RunOutProcProducer();
    // 应用身份（名称 + 图标）统一在开头设置一次：
    //   图标打进 exe（资源 IDI_APPICON，见 ZufyUI.rc），运行时 Image::FromResource 从资源加载，不依赖外部 .ico 文件。
    //   RegisterApp 需在包含本库头文件前 #define ZUFYUI_ALLOW_APP_REGISTRATION 授权；授权后自动写注册表 + 缓存图标，退出清缓存。
    auto appIcon = Image::FromResource(IDI_APPICON, RT_GROUP_ICON);
    RegisterApp(AppInfo{ L"ZufyUI Demo", L"ZufyUI.Demo", appIcon });
    // 演示程序自己的默认背景：亚克力 + 半透明白色着色。
    // 库的默认是 Backdrop::None（不替应用决定），所以不透明/着色都由应用这里指定。
    Window::SetDefaultBackdrop(Backdrop::Acrylic, 0x80FFFFFF);
    ThemeManager::SetFollowSystem(true);   // 默认跟随系统深/浅色 + 系统强调色（启动时读一次 + 监听系统消息）
    const wchar_t* kTitle = L"ZufyUI 自动化布局综合测试 " ZufyUI_VERSION_STRING;
    Window win;
    if (!win.Create(1000, 700, kTitle)) {
    // 注意：这是"窗口创建失败"的错误路径 —— 库的 MessageBox 依赖 ZufyUI 框架（D2D/DComp/Window），
    //   此刻窗口都没起来，只能用系统 MessageBox；其余 15 处已全部换成库自带弹窗。
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_ICONERROR);
        return 1;
    }

    // 主窗口使用自定义标题栏（拖动标题栏移动窗口，右上角三件套与原生一致）
    win.SetWindowCorner(Window::WindowCorner::Round);
    win.SetResizable(true);
    auto titleBar = std::make_shared<DefaultTitleBar>();
    titleBar->SetTitle(kTitle);
    titleBar->SetIcon(appIcon);   // 自定义标题栏图标也来自同一个 appIcon
    titleBar->SetHeight(34);
    win.SetCustomTitleBar(titleBar);

    // 创建全局右键菜单（与信号无关）
    auto globalMenu = std::make_shared<Menu>();
    globalMenu->AddItem(L"显示信息", []() {
        ZuiInfo(L"提示", L"这是全局右键菜单");
        });
    globalMenu->AddSeparator();

    auto subMenu = std::make_shared<Menu>();
    subMenu->AddItem(L"子项1", []() { ZuiInfo(L"提示", L"点击了子项1"); });
    subMenu->AddItem(L"子项2", []() { ZuiInfo(L"提示", L"点击了子项2"); });
    globalMenu->AddSubmenu(L"更多操作", subMenu);
    globalMenu->AddSeparator();
    globalMenu->AddItem(L"关于", []() {
        ZuiInfo(L"关于", L"ZufyUI 综合测试程序 v1.0");
        });
    win.SetContextMenu(globalMenu);


    // 获取默认根布局（ColumnBox）
    auto root = win.GetRootColumnBox();
    if (!root) return 1;
    root->SetSpacing(10);

    // 主水平布局：左侧导航 + 右侧内容
    auto mainRow = std::make_shared<RowBox>();
    mainRow->SetSpacing(10);
    mainRow->SetFillHeight(true);
    mainRow->SetFillWidth(true);
    root->AddChild(mainRow);

    // 左侧导航：使用 ListView 按钮模式
    auto navList = std::make_shared<ListView>();
    navList->SetWidth(160);
    navList->SetFillHeight(true);
    navList->SetButtonMode(true);
    navList->SetButtonSpacing(4.0f);
    navList->SetItemHeight(36.0f);
    navList->AddItem(L"首页");
    navList->AddItem(L"基础控件");
    navList->AddItem(L"输入与滚动");
    navList->AddItem(L"嵌套与联动");
    navList->AddItem(L"列表视图");
    navList->AddItem(L"表格视图");
    navList->AddItem(L"树形视图");
    navList->AddItem(L"树形增强");
    navList->AddItem(L"图像");
    navList->AddItem(L"多窗口");
    navList->AddItem(L"窗口属性");
    navList->AddItem(L"新控件A");   // TabView + RadioGroup
    navList->AddItem(L"新控件B");   // ProgressRing + SplitView
    navList->AddItem(L"多行文本");
    navList->AddItem(L"条形图");
    navList->AddItem(L"折线图");
    navList->AddItem(L"饼图");
    navList->AddItem(L"拖放");
    navList->AddItem(L"设置卡片");
    navList->AddItem(L"主题");
    navList->AddItem(L"DSL");
    navList->AddItem(L"环境组合");
    navList->AddItem(L"原生HWND");
    navList->SetSelectedIndex(0);
    mainRow->AddChild(navList);

    // 右侧页面容器
    auto mainHost = std::make_shared<PageHost>();
    mainHost->SetTransitionDirection(PageHost::TransitionDirection::Left);
    mainHost->SetFillWidth(true);
    mainHost->SetFillHeight(true);
    mainRow->AddChild(mainHost);
    mainHost->SetUseCache(false);  // PageHost 本身也不应缓存
    // ---------- 底部状态栏：实时反映窗口/内容状态（切主题/背景/页面/激活/设备丢失都会变）----------
    auto status = std::make_shared<DemoStatus>();
    {
        auto statusBar = std::make_shared<StatusBar>();
        statusBar->AddPanel(L"页面: 首页");            // 0
        statusBar->AddPanel(L"主题: -");               // 1
        statusBar->AddPanel(L"背景: -");               // 2
        statusBar->AddPanel(L"DPI -");                 // 3
        statusBar->AddPanel(Icon::Home, L"就绪", true); // 4（右对齐）
        status->sb = statusBar.get();
        status->win = &win;
        win.SetStatusBar(statusBar);
        status->Refresh();

        // 主题变化（无论来自菜单、主题页还是跟随系统）→ 刷新。
        //   注册返回 RAII 句柄：存成 static 让它在整个程序生命周期内有效（局部丢弃 = 立即自动断开）。
        static auto demoStatusThemeHook = ThemeManager::RegisterHook([status](const Theme&) { status->Refresh(); });
        // 窗口状态信号 → 刷新第 5 个面板（"窗口某些状态变化时也跟着变"）
        win.Activated.connect([status]() { status->state = L"活动"; status->Refresh(); });
        win.Deactivated.connect([status]() { status->state = L"非活动"; status->Refresh(); });
        win.DeviceLost.connect([status]() { status->state = L"D3D 设备丢失"; status->Refresh(); });
        win.RenderingError.connect([status](HRESULT hr) {
            wchar_t b[64]; swprintf(b, 64, L"渲染错误 0x%08X", (unsigned)hr);
            status->state = b; status->Refresh(); });
        win.BackdropUnsupported.connect([status]() { status->state = L"当前系统不支持该背景层"; status->Refresh(); });
        win.ThumbButtonClicked.connect([status](Window::ThumbButtonId id) {
            status->state = L"缩略图按钮 #" + std::to_wstring((int)id); status->Refresh(); });
        // 页面切换（左栏 / 首页卡片 / 菜单共用同一条通道）→ 刷新"页面"面板
        navList->Connect(navList->SelectionChanged, [status](int idx) {
            if (idx >= 0 && idx < kPageCount) status->page = kPageNames[idx];
            status->Refresh(); });
    }

    // ---------- 顶部菜单栏：尽量"能干活"（切主题/背景/尺寸/帧率/页面跳转/窗口开关）----------
    auto menuBar = std::make_shared<MenuBar>();
    {
        auto mFile = std::make_shared<Menu>();
        mFile->AddItem(L"新建", []() { ZuiInfo(L"文件", L"新建（演示项）"); });
        mFile->AddItem(L"打开…", []() { ZuiInfo(L"文件", L"打开（演示项）"); });
        mFile->AddItem(L"保存", []() { ZuiInfo(L"文件", L"保存（演示项）"); });
        mFile->AddSeparator();
        mFile->AddItem(L"退出", [&win]() { if (win.GetHwnd()) PostMessageW(win.GetHwnd(), WM_CLOSE, 0, 0); });
        menuBar->AddMenu(L"文件(F)", mFile);

        auto mEdit = std::make_shared<Menu>();
        mEdit->AddItem(L"撤销", []() { ZuiInfo(L"编辑", L"撤销（未接编辑器，仅演示菜单项）"); });
        mEdit->AddItem(L"重做", []() { ZuiInfo(L"编辑", L"重做（未接编辑器，仅演示菜单项）"); });
        mEdit->AddSeparator();
        mEdit->AddItem(L"复制", []() { ZuiInfo(L"编辑", L"复制（演示项）"); });
        mEdit->AddItem(L"粘贴", []() { ZuiInfo(L"编辑", L"粘贴（演示项）"); });
        menuBar->AddMenu(L"编辑(E)", mEdit);

        // 视图：内容联动的核心菜单（页面跳转 / 背景层 / 尺寸 / 帧率 / 窗口开关）
        auto mView = std::make_shared<Menu>();
        mView->AddCheckItemCb(L"显示状态栏", true, [status](bool on) {
            if (auto sb = status->win->GetStatusBar()) sb->SetVisible(on);
            status->state = on ? L"状态栏: 显示" : L"状态栏: 隐藏"; status->Refresh(); });
        mView->AddCheckItemCb(L"自定义标题栏", true, [&win, titleBar, status](bool on) {
            win.SetCustomTitleBar(on ? std::static_pointer_cast<UIElement>(titleBar) : nullptr);
            status->state = on ? L"自定义标题栏: 开" : L"自定义标题栏: 关"; status->Refresh(); });
        mView->AddCheckItemCb(L"可调整大小", true, [&win, status](bool on) {
            win.SetResizable(on); status->state = on ? L"可调整大小: 开" : L"可调整大小: 关"; status->Refresh(); });
        mView->AddSeparator();
        {
            auto mPages = std::make_shared<Menu>();
            for (int i = 0; i < kPageCount; ++i)
                mPages->AddItem(kPageNames[i], [navList, i]() { navList->SetSelectedIndex(i); });
            mView->AddSubmenu(L"跳转到页面", mPages);
        }
        {
            auto mBd = std::make_shared<Menu>();
            mBd->AddItem(L"无背景", [status]() { status->win->SetBackdrop(Backdrop::None); status->Refresh(); });
            mBd->AddItem(L"亚克力", [status]() { status->win->SetBackdrop(Backdrop::Acrylic); status->Refresh(); });
            mBd->AddItem(L"云母", [status]() { status->win->SetBackdrop(Backdrop::Mica); status->Refresh(); });
            mView->AddSubmenu(L"背景层", mBd);
        }
        {
            auto mSize = std::make_shared<Menu>();
            mSize->AddItem(L"800 × 600（最小）", [&win]( ) { win.SetSize(800, 600); });
            mSize->AddItem(L"1024 × 768", [&win]() { win.SetSize(1024, 768); });
            mSize->AddItem(L"1280 × 720", [&win]() { win.SetSize(1280, 720); });
            mView->AddSubmenu(L"窗口尺寸", mSize);
        }
        {
            auto mFps = std::make_shared<Menu>();
            mFps->AddItem(L"不限制（跟随显示器）", [&win]() { win.SetFrameRateLimit(0); });
            mFps->AddItem(L"60 FPS", [&win]() { win.SetFrameRateLimit(60); });
            mFps->AddItem(L"120 FPS", [&win]() { win.SetFrameRateLimit(120); });
            mView->AddSubmenu(L"帧率上限", mFps);
        }
        menuBar->AddMenu(L"视图(V)", mView);

        // 主题
        auto mTheme = std::make_shared<Menu>();
        mTheme->AddCheckItemCb(L"跟随系统主题", true, [](bool on) { ThemeManager::SetFollowSystem(on); });
        mTheme->AddSeparator();
        mTheme->AddItem(L"浅色", []() { ThemeManager::SetLightAppTheme(); });
        mTheme->AddItem(L"深色", []() { ThemeManager::SetDarkAppTheme(); });
        mTheme->AddItem(L"高对比度", []() { ThemeManager::SetAppTheme(Theme::HighContrast()); });
        mTheme->AddSeparator();
        mTheme->AddItem(L"模拟系统: 深色", []() { ThemeManager::DebugForceSystemTheme(true, true, false); });
        mTheme->AddItem(L"模拟系统: 高对比度", []() { ThemeManager::DebugForceSystemTheme(true, false, true); });
        mTheme->AddItem(L"关闭模拟", []() { ThemeManager::DebugForceSystemTheme(false, false, false); });
        menuBar->AddMenu(L"主题(T)", mTheme);

        // 工具：调试通道（与"多窗口"页的调试按钮同源）
        auto mTools = std::make_shared<Menu>();
        mTools->AddCheckItemCb(L"调试通道", false, [](bool on) { ZufyUI::SetDebugEnabled(on); });
        mTools->AddItem(L"导出元素树", [&win]() {
            std::wstring t = detail::DumpWindowTree(&win);
            ZufyUI_DEBUG_LOG_W(t.c_str());
            ZuiInfo(L"工具", L"元素树已导出到调试输出（" + std::to_wstring(t.size()) + L" 字符）"); });
        mTools->AddItem(L"导出帧统计", [&win]() {
            ZufyUI::SetDebugEnabled(true);
            ZuiInfo(L"工具", detail::DumpFrameStats(&win)); });
        menuBar->AddMenu(L"工具(O)", mTools);

        auto mHelp = std::make_shared<Menu>();
        mHelp->AddItem(L"关于 ZufyUI", []() { ZuiInfo(L"关于", L"ZufyUI 综合测试程序 v1.0"); });
        mHelp->AddItem(L"菜单键盘操作", []() { ZuiInfo(L"帮助", L"Alt / F10 打开菜单，方向键选择，Enter 执行，Esc 关闭。"); });
        menuBar->AddMenu(L"帮助(H)", mHelp);
    }
    win.SetMenuBar(menuBar);


    // ---------- 首页：设置卡片式导航 ----------
    // 每张卡片 = 一组"设置行"，点任意一行 → 设置 navList 选中项；导航方向/跳转/左栏高亮都由既有的
    // SelectionChanged 连接统一处理（ListView::SetSelectedIndex 会触发该信号）。
    auto pageHome = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto go = [navList](int idx) { navList->SetSelectedIndex(idx); };

        struct HomeRow { Icon icon; const wchar_t* title; const wchar_t* sub; int page; };

        auto col = Col().spacing(10.0f).margin(Thickness(14, 14, 14, 14));
        col->AddChild(Lbl(L"ZufyUI 综合测试程序 —— 点击任意一行跳到对应页面").shared());
        col->AddChild(Lbl(L"提示：首页与左侧列表双向同步；下面按功能分组，方便快速定位。")
                          .margin(Thickness(0, 0, 0, 6)).shared());

        // 一个分组 = 一张带阴影的卡片 + 一个 SettingsList
        auto homeCard = [&col, go](const std::wstring& title, std::initializer_list<HomeRow> rows) {
            // 注意：不要用 Card 自带的 GridLayout —— 网格按自动列测量时给子元素 FLT_MAX，
            //   而 SettingsList::MeasureOverride 在 avail.width==FLT_MAX 时会退回 width_（=0），
            //   结果每行文字区被压成 0 宽，只剩图标和箭头（首页"看不到内容"就是这么来的）。
            //   改成 SetLayout(Col)：SettingsList 能拿到父级给的有限宽度并 fillWidth 撑满。
            auto card = make_card().padding(10.0f).with(&::ZufyUI::Card::SetShadow, true);
            auto list = Settings().fillWidth(true);
            for (auto& r : rows)   // AddRow 有默认参数 → 不能走 .with，直接调用
                list->AddRow(r.icon, r.title, r.sub, [go, idx = r.page]() { go(idx); });
            card->SetLayout(Col().spacing(6.0f).add(Lbl(title)).add(list.shared()).shared());
            col->AddChild(card.shared());
        };

        homeCard(L"入门与基础", {
            { Icon::View,     L"基础控件",   L"按钮 / 开关 / 勾选 / 滑块 / 进度 / 下拉 / 卡片阴影", 1 },
            { Icon::Edit,     L"输入与滚动", L"TextBox / NumberBox / ScrollViewer / 系统对话框",      2 },
            { Icon::File,     L"多行文本",   L"TextEdit：行号 / 换行 / Tab / 富文本",                 13 },
            { Icon::Folder,   L"嵌套与联动", L"PageHost 嵌套 + 控件联动 + 密集交叉",                  3 },
        });
        homeCard(L"数据视图", {
            { Icon::AllApps,  L"列表视图",   L"ListView：勾选 / 多选 / 富项 / 虚拟 20 万行",          4 },
            { Icon::View,     L"表格视图",   L"TableView：行·列·单元格选择 / 排序 / 隐藏列 / 万行",   5 },
            { Icon::Folder,   L"树形视图",   L"TreeView：多列 / 展开深度 / 节点路径 / 过滤搜索",      6 },
            { Icon::Settings, L"树形增强",   L"三态勾选 / 框选 / 图标 / 按名称排序",                  7 },
        });
        homeCard(L"图表", {
            { Icon::View,     L"条形图",     L"BarChart：分组 / 堆叠 / 百分比 / 参考线 / 拖拽平移",    14 },
            { Icon::View,     L"折线图",     L"LineChart：多系列 / 平滑 / 虚线 / 面积填充",           15 },
            { Icon::View,     L"饼图",       L"PieChart：环形 / 标签模式 / Ctrl\+滚轮 缩放 / 滚轮·拖拽 平移",      16 },
        });
        homeCard(L"窗口与系统", {
            { Icon::Settings, L"多窗口",     L"独立窗口 / 子窗口 / 模态 / 托盘 / 任务栏 / 跳转列表",  9 },
            { Icon::Settings, L"窗口属性",   L"背景层（亚克力·云母）/ 标题栏 / 尺寸 / 切换缓动",      10 },
            { Icon::Copy,     L"拖放",       L"接收文件与文本 / 拖出文本与文件",                      17 },
        });
        homeCard(L"主题 / DSL / 极限组合", {
            { Icon::Favorite, L"主题",       L"浅色 / 深色 / 高对比度 + ThemeRole 色卡",              19 },
            { Icon::Edit,     L"DSL",        L"DSL 综合演示：链式构造 / Grid / Expander / 图表",      20 },
            { Icon::Warning,  L"环境组合",   L"主题×背景 9 组合 / DPI 尺寸 / 深层嵌套 / 边界异常态",  21 },
        });

        col->AddChild(Lbl(L"「环境组合」页是新增的极端情况测试页：3 主题 × 3 背景 = 9 种组合、DPI 与尺寸边界、"
                          L"深层嵌套、控件墙，以及超长文本 / 空列表 / 极值进度 / 极窄容器 / 超长下拉项等边界态。")
                          .with(&Label::SetTextOverflow, Label::TextOverflow::Wrap).shared());

        SetScrollablePage(pageHome, col.shared());
    }
    mainHost->AddPage(pageHome);
    // ---------- 页面1：基础控件（DSL 重排）----------
    // 排版约定（后续页面沿用）：外层 Scroll 包一个 Col；页内按"功能小节"分组，
    // 每节一个标题 Label + 一行/多行控件；统一 spacing / margin；颜色一律交给主题，不显式设色。
    auto page1 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        // 回显标签先建（后面各处 Connect 捕获）
        auto lblToggleState = Lbl(L"开关状态：关").shared();
        auto lblCheckState = Lbl(L"勾选框：已选").shared();
        auto lblCombo = Lbl(L"当前选择：选项 A").shared();

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"按钮");
        col->AddChild(Row(
            Btn(L"普通按钮12345678901234567890123456789012345678901234567890").tooltip(L"超长文本：测试省略与裁剪")
                .on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"普通按钮被点击（超长文案按钮）"); }),
            Btn(L"彩色按钮").with(&Button::SetColors,
                Color::FromArgb(255, 180, 60, 60), Color::FromArgb(255, 200, 80, 80), Color::FromArgb(255, 160, 40, 40))
                .on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"按钮2被点击"); }),
            Btn(L"禁用按钮").enabled(false).tooltip(L"该按钮已被禁用"),
            Btn(L"可切换按钮").with(&Button::SetCheckable, true).tooltip(L"可切换按钮：点击切换选中状态")
        ).spacing(8.0f).shared());

        // 富文本按钮 + 右侧下拉（split button）
        {
            auto richRow = std::make_shared<RowBox>();
            richRow->SetSpacing(6.0f);
            richRow->AddChild(std::make_shared<Label>(L"富文本"));
            auto badge = std::make_shared<Label>(L"NEW");
            badge->SetTextColor(Color::FromArgb(255, 255, 255, 255));
            badge->SetBackgroundColor(Color::FromArgb(255, 210, 60, 60));
            badge->SetBackgroundCornerRadius(4.0f);
            badge->SetPadding(Thickness(5, 1, 5, 1));
            richRow->AddChild(badge);

            auto richBtn = std::make_shared<Button>(L"");
            richBtn->SetAutoSizeToText(false);
            richBtn->SetWidth(190.0f);
            richBtn->SetContent(richRow);

            auto writeOpen = std::make_shared<Button>(L"写回+开菜单");
            writeOpen->SetDropWriteBack(true);
            writeOpen->SetDropOpenOnClick(true);
            writeOpen->SetDropItems({ L"选项 A", L"选项 B" });

            auto noWriteOpen = std::make_shared<Button>(L"不写回+开菜单");
            noWriteOpen->SetDropWriteBack(false);
            noWriteOpen->SetDropOpenOnClick(true);
            noWriteOpen->SetDropItems({ L"选项 A", L"选项 B" });

            auto manualMenu = std::make_shared<Button>(L"只回调");
            manualMenu->SetDropWriteBack(true);
            manualMenu->SetDropOpenOnClick(false);
            manualMenu->SetDropItems({ L"选项 A", L"选项 B" });

            auto openMenuBtn = std::make_shared<Button>(L"手动打开菜单");
            openMenuBtn->Clicked.connect([manualMenu]() { manualMenu->ShowDropMenu(); });

            auto btnRow = std::make_shared<RowBox>();
            btnRow->SetSpacing(8.0f);
            btnRow->AddChild(richBtn);
            btnRow->AddChild(writeOpen);
            btnRow->AddChild(noWriteOpen);
            btnRow->AddChild(manualMenu);
            btnRow->AddChild(openMenuBtn);
            col->AddChild(btnRow);
        }

        AddSection(col, L"开关与勾选");
        col->AddChild(Row(
            Toggle(false).on<&ToggleSwitch::Toggled>([lblToggleState](bool on) {
                lblToggleState->SetText(on ? L"开关状态：开" : L"开关状态：关"); }),
            lblToggleState,
            Check(L"已选").checked(true).on<&CheckBox::Toggled>([lblCheckState](bool on) {
                lblCheckState->SetText(on ? L"勾选框：已选" : L"勾选框：未选"); }),
            Check(L"选项二"),
            lblCheckState
        ).spacing(10.0f).shared());

        AddSection(col, L"滑块与进度");
        auto progress = Progress().value(0.4f).with(&ProgressBar::SetShowText, true).shared();
        col->AddChild(Row(
            // 与类同名的短工厂（Slider/Card/Page/Expander…）在本文件（using namespace ZufyUI）
            //   下裸用会 C2872；统一改用无歧义别名 make_slider / make_card / make_page / make_expander…
            make_slider().width(240.0f).with(&::ZufyUI::Slider::SetRange, 0.0f, 1.0f).value(0.4f)
                     .with(&::ZufyUI::Slider::SetStep, 0.1f).with(&::ZufyUI::Slider::SetSnapToStep, true)
                     .on<&::ZufyUI::Slider::ValueChanged>([progress](float v) { progress->SetValue(v); }),
            progress
        ).spacing(12.0f).shared());

        AddSection(col, L"下拉框");
        auto combo1 = Combo().placeholder(L"请选择").with(&ComboBox::SetMaxVisibleItems, 5)
                            .items({ L"选项 A", L"选项 B", L"选项 C" }).shared();
        combo1->SetItemDisabled(1, true);
        {   // 捕获裸指针，避免 对象→信号→lambda→shared_ptr 的引用环
            ComboBox* raw = combo1.get();
            combo1->Connect(combo1->SelectionChanged, [lblCombo, raw](int) { lblCombo->SetText(L"当前选择：" + raw->GetSelectedText()); });
            combo1->Connect(combo1->DropDownOpened, []() {});
        }
        col->AddChild(Row(combo1, lblCombo).spacing(10.0f).shared());

        // 可编辑 + 输入过滤的下拉框
        col->AddChild(Combo().items({ L"Apple", L"Banana", L"Cherry", L"Avocado", L"Blueberry" })
                            .with(&ComboBox::SetEditable, true).with(&ComboBox::SetFilterEnabled, true)
                            .placeholder(L"输入过滤...").shared());

        AddSection(col, L"卡片与阴影");
        auto cardShadow = make_card().padding(12.0f)
                               .with(&::ZufyUI::Card::SetShadow, true)          // 阴影颜色不设 → 跟随主题
                               .with(&::ZufyUI::Card::SetShadowBlur, 12.0f)
                               .with(&::ZufyUI::Card::SetShadowOffset, 0.0f, 3.0f)
                               .with(&::ZufyUI::Card::SetShadowCornerRadius, 8.0f).shared();
        if (auto cardGrid = cardShadow->GetLayoutAs<GridLayout>()) cardGrid->AddChild(std::make_shared<Label>(L"带阴影的卡片"), 0, 0);
        // ColumnBox 会把没有显式宽度的子元素拉满可用宽度（横向拉伸不吃 hWeight），
        // 所以想让卡片按内容收窄，必须把它放进 Row。
        col->AddChild(Row(cardShadow).shared());

        SetScrollablePage(page1, col.shared());

        // （原页面1上的「新控件」——TabView / RadioGroup / ProgressRing / SplitView——已拆到
        //   单独一页「新控件」，便于二分定位"持续更新"来源；见下方 pageNew）
    }

    // ---------- 页面2：输入与滚动（DSL 重排）----------
    // 颜色一律取自主题角色（不要硬编码）：显式设色会让该实例退出主题管理，深色主题下就不会再变。
    auto page2 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;


        auto iconBtn = [](Icon ic, const std::wstring& t) {
            return Btn(t).apply([ic](Button& b) { b.SetIcon(ic); });   // apply：避开 SetIcon 重载导致的成员指针歧义
        };

        auto lblText = Lbl(L"你输入的内容将显示在这里").minWidth(160.0f)   // 固定宽度 → 超出即省略
                           .with(&Label::SetTextOverflow, Label::TextOverflow::Ellipsis).shared();
        auto dlgResult = Lbl(L"结果：（点下面按钮后显示在这里）").shared();
        auto dbgResult = Lbl(L"（调试关闭）").shared();

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"文本输入");
        auto textBox1 = Txt().placeholder(L"输入文本...").shared();
        textBox1->Connect(textBox1->TextChanged, [lblText](const std::wstring& t) {
            lblText->SetText(t.empty() ? L"你输入的内容将显示在这里" : L"输入：" + t); });
        auto numBox = Num().with(&NumberBox::SetRange, -100000000.0, 100000000.0)   // ±1 亿
                            .with(&NumberBox::SetStep, 100.0)                       // 每次上下调 100
                            .with(&NumberBox::SetDefaultValue, 5.0)                 // 非默认时右侧出现「清除 ×」
                            .placeholder(L"数字（仅数字；范围 ±1 亿；步进 100）").shared();
        {   // 实时校验（每次输入就判断，不用等失焦）：空 → 错误；114514 / 1919810 → 错误；其余正常
            NumberBox* raw = numBox.get();
            numBox->Connect(numBox->TextChanged, [raw](const std::wstring& s) {
                double v = 0.0; bool ok = !s.empty();
                if (ok) { wchar_t* end = nullptr; v = wcstod(s.c_str(), &end); ok = (end && *end == L'\0' && end != s.c_str()); }
                raw->SetError(!ok || v == 114514.0 || v == 1919810.0);
            });
        }
        col->AddChild(Row(textBox1, lblText).spacing(10.0f).shared());
        col->AddChild(Row(Txt().placeholder(L"密码").with(&TextBox::SetPasswordMode, true)
                                 .with(&TextBox::SetMaxLength, 16).width(200.0f),
                          numBox).spacing(10.0f).shared());

        AddSection(col, L"滚动容器（垂直滚动 30 项）");
        auto scrollContent = Col().spacing(6.0f);
        for (int i = 1; i <= 30; i++) scrollContent->AddChild(Lbl(L"滚动条目 " + std::to_wstring(i)).shared());
        auto scrollView = Scroll().height(150.0f)
                                 .with(&ScrollViewer::SetVerticalScrollEnabled, true)
                                 .with(&ScrollViewer::SetHorizontalScrollEnabled, false)
                                 .with(&ScrollViewer::SetVerticalScrollBarVisibility, ScrollViewer::ScrollBarVisibility::Always)
                                 .with(&ScrollViewer::SetContentMargin, Thickness(8, 8, 8, 8))
                                 .with(&ScrollViewer::SetContent, scrollContent.shared());

        col->AddChild(scrollView.shared());

        AddSection(col, L"TextBox 进阶：输入过滤 / 只读 / 密码显隐 / 占位色");
        auto filterBox = Txt().placeholder(L"仅数字").width(160.0f)
                             .apply([](TextBox& t) { t.SetInputFilter([](wchar_t c) { return c >= L'0' && c <= L'9'; }); })
                             .with(&TextBox::SetAutomationId, L"filterBox").shared();   // 供外部工具/自动化定位
        filterBox->Connect(filterBox->ReturnPressed, [raw = filterBox.get()]() {
            ZuiInfo(L"回车", raw->GetText()); });
        col->AddChild(Row(
            filterBox,
            Txt(L"只读内容").with(&TextBox::SetReadOnly, true),
            Txt().placeholder(L"密码(可显)").with(&TextBox::SetPasswordMode, true).with(&TextBox::SetRevealPassword, true),
            Txt().placeholder(L"彩色占位").with(&TextBox::SetPlaceholderColor, ThemeColor(ThemeRole::Danger))
        ).spacing(10.0f).shared());

        AddSection(col, L"系统对话框：打开(可多选) / 文件夹 / 另存为 / 颜色（新版 IFileDialog + ChooseColor）");
        col->AddChild(dlgResult);
        auto setRes = [dlgResult](const std::wstring& s) { dlgResult->SetText(s); };
        col->AddChild(Row(
            iconBtn(Icon::Open, L"打开文件(多选)").on<&Button::Clicked>([w = &win, setRes]() {
                FileDialogOptions o;
                o.title = L"选择文件（可多选）";
                o.filters = { { L"图片", { L"*.png", L"*.jpg", L"*.jpeg", L"*.bmp", L"*.gif" } },
                              { L"文本", { L"*.txt", L"*.log" } } };
                auto files = FileDialog::OpenFiles(w->GetHwnd(), o);
                if (files.empty()) { setRes(L"打开文件：已取消"); return; }
                std::wstring s = L"打开文件：共 " + std::to_wstring(files.size()) + L" 个 → " + files.front();
                if (files.size() > 1) s += L" …";
                setRes(s);
            }),
            iconBtn(Icon::Folder, L"选文件夹(多选)").on<&Button::Clicked>([w = &win, setRes]() {
                FileDialogOptions o; o.title = L"选择文件夹（可多选）"; o.multiSelect = true;
                auto folders = FileDialog::PickFolders(w->GetHwnd(), o);
                if (folders.empty()) { setRes(L"选文件夹：已取消"); return; }
                setRes(L"选文件夹：共 " + std::to_wstring(folders.size()) + L" 个 → " + folders.front());
            }),
            iconBtn(Icon::Save, L"另存为").on<&Button::Clicked>([w = &win, setRes]() {
                FileDialogOptions o;
                o.title = L"另存为"; o.defaultFileName = L"output"; o.defaultExtension = L"txt";
                o.filters = { { L"文本", { L"*.txt" } } };
                auto path = FileDialog::SaveOne(w->GetHwnd(), o);
                setRes(path ? (L"另存为：" + *path) : L"另存为：已取消");
            }),
            iconBtn(Icon::Edit, L"选择颜色").on<&Button::Clicked>([w = &win, setRes, dlgResult]() {
                ColorDialog::Options co;
                co.custom = { Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1) };
                auto c = ColorDialog::Pick(w->GetHwnd(), Color(0.2f, 0.6f, 1.0f, 1.0f), co);
                if (!c) { setRes(L"颜色：已取消"); return; }
                wchar_t buf[96];
                swprintf(buf, 96, L"颜色：R=%d G=%d B=%d", (int)(c->r * 255 + 0.5f), (int)(c->g * 255 + 0.5f), (int)(c->b * 255 + 0.5f));
                dlgResult->SetTextColor(*c);   // 文字直接用所选颜色，直观
                setRes(buf);
            })
        ).spacing(8.0f).shared());

        AddSection(col, L"调试 / 无障碍（调试默认关；外部工具向 ZufyUI_DispatcherWindow 发 WM_COPYDATA）");
        col->AddChild(dbgResult);
        col->AddChild(Row(
            iconBtn(Icon::Settings, L"开/关调试通道").on<&Button::Clicked>([dbgResult]() {
                bool on = !ZufyUI::IsDebugEnabled();
                ZufyUI::SetDebugEnabled(on);
                dbgResult->SetText(on ? L"调试通道：已开启" : L"调试通道：已关闭");
            }),
            iconBtn(Icon::AllApps, L"导出元素树").on<&Button::Clicked>([w = &win, dbgResult]() {
                std::wstring t = detail::DumpWindowTree(w);
                dbgResult->SetText(L"元素树：" + std::to_wstring(t.size()) + L" 字符（已 OutputDebugString）");
                ZufyUI_DEBUG_LOG_W(t.c_str());
            }),
            iconBtn(Icon::View, L"导出帧统计").on<&Button::Clicked>([w = &win, dbgResult]() {
                ZufyUI::SetDebugEnabled(true);   // 采样需要开
                dbgResult->SetText(detail::DumpFrameStats(w));
            }),
            iconBtn(Icon::Edit, L"自动化:聚焦 filterBox").on<&Button::Clicked>([dbgResult]() {
                bool ok = detail::DebugFocusElement(L"filterBox");
                dbgResult->SetText(ok ? L"已聚焦 filterBox" : L"未找到 filterBox");
            })
        ).spacing(8.0f).shared());

        SetScrollablePage(page2, col.shared());
    }

    // ---------- 页面3：嵌套与联动（DSL 重排）----------
    // 三个嵌套子页各自用 DSL 独立构建，子页内容用 SetLayout(Col(...)) 直接给，不再走 GetLayoutAs<GridLayout>()。
    auto page3 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;



        // ---- 子页 A：滑块 ↔ 进度条 ----
        auto sliderA = make_slider().with(&::ZufyUI::Slider::SetRange, 0.0f, 100.0f).value(50.0f).fillWidth(true);
        auto progressA = Progress().value(0.5f).fillWidth(true);
        sliderA.on<&::ZufyUI::Slider::ValueChanged>([progressA](float v) { progressA->SetValue(v / 100.0f); });
        auto nestedPageA = make_page();
        nestedPageA->SetLayout(Col().spacing(8.0f).margin(Thickness(12, 12, 12, 12))
            .add(Lbl(L"嵌套 A：滑块控制进度条"))
            .add(Row(sliderA, progressA).spacing(10.0f).shared())
            .add(Btn(L"重置为 50%").on<&Button::Clicked>([sliderA]() { sliderA->SetValue(50.0f); }))
            .shared());

        // ---- 子页 B：下拉框 ↔ 标签颜色（颜色取主题角色，不硬编码）----
        auto displayLabel = Lbl(L"当前颜色：红色").textColor(ThemeColor(ThemeRole::Danger)).shared();
        auto comboB = Combo().items({ L"红色", L"绿色", L"蓝色" }).shared();
        comboB->Connect(comboB->SelectionChanged, [displayLabel, raw = comboB.get()](int) {
            std::wstring name = raw->GetSelectedText();
            displayLabel->SetText(L"当前颜色：" + name);
            displayLabel->SetTextColor(name == L"红色" ? ThemeColor(ThemeRole::Danger)
                                     : name == L"绿色" ? ThemeColor(ThemeRole::Success)
                                                       : ThemeColor(ThemeRole::Accent));
        });
        auto nestedPageB = make_page();
        nestedPageB->SetLayout(Col().spacing(8.0f).margin(Thickness(12, 12, 12, 12))
            .add(Lbl(L"嵌套 B：下拉框控制标签"))
            .add(Row(comboB, displayLabel).spacing(10.0f).shared())
            .add(Btn(L"重置选择").on<&Button::Clicked>([comboB]() { comboB->SetSelectedIndex(0); }))
            .shared());

        // ---- 子页 C：密集交叉测试（4 列 × 5 行，下拉框/按钮/滑块/进度/开关混合）----
        auto clickA = []() { ZuiInfo(L"提示", L"按钮A被点击"); };
        auto clickB = []() { ZuiInfo(L"提示", L"按钮B被点击"); };
        auto clickC = []() { ZuiInfo(L"提示", L"按钮C被点击"); };
        auto clickD = []() { ZuiInfo(L"提示", L"按钮D被点击"); };
        auto nestedPageC = make_page();
        nestedPageC->SetLayout(Scroll().fill(true).margin(Thickness(8, 8, 8, 8)).with(&ScrollViewer::SetContent,
            Grid(
                Cell(0, 0, Lbl(L"密集交叉测试：多个下拉框与控件混合"), 1, 4),
                Cell(1, 0, Combo().items({ L"选项1-A", L"选项1-B", L"选项1-C" })),
                Cell(1, 1, Btn(L"按钮A").on<&Button::Clicked>(clickA)),
                Cell(1, 2, Combo().items({ L"选项2-A", L"选项2-B", L"选项2-C" })),
                Cell(1, 3, Lbl(L"状态1")),
                Cell(2, 0, Lbl(L"固定文本")),
                Cell(2, 1, Combo().items({ L"选项3-A", L"选项3-B", L"选项3-C" })),
                Cell(2, 2, make_slider().with(&::ZufyUI::Slider::SetRange, 0.0f, 100.0f).value(50.0f)),
                Cell(2, 3, Btn(L"按钮B").on<&Button::Clicked>(clickB)),
                Cell(3, 0, Progress().value(0.7f)),
                Cell(3, 1, Combo().items({ L"选项4-A", L"选项4-B", L"选项4-C" })),
                Cell(3, 2, Btn(L"按钮C").on<&Button::Clicked>(clickC)),
                Cell(3, 3, Combo().items({ L"选项5-A", L"选项5-B", L"选项5-C" })),
                Cell(4, 0, Toggle(false)),
                Cell(4, 1, Lbl(L"开关")),
                // 故意超长选项：测下拉展开后的滚动
                Cell(4, 2, Combo().items({ L"选项6-A", L"选项6-B", L"选项6-C", L"选项6-D", L"选项6-E", L"选项6-F",
                                           L"选项6-G", L"选项6-H", L"选项6-I", L"选项6-J", L"选项6-K", L"选项6-L" })),
                Cell(4, 3, Btn(L"按钮D").on<&Button::Clicked>(clickD))
            ).spacing(6.0f, 6.0f).fillWidth(true).shared()).shared());

        auto nestedHost = Pages()
            .with(&PageHost::SetTransitionDirection, PageHost::TransitionDirection::Up)
            .height(300.0f).fillWidth(true);
        nestedHost->AddPage(nestedPageA.shared());
        nestedHost->AddPage(nestedPageB.shared());
        nestedHost->AddPage(nestedPageC.shared());

        // 嵌套页切换：按相对位置设置左右方向
        auto navigateNested = [host = nestedHost.shared()](int target) {
            int cur = host->GetCurrentIndex();
            if (target > cur) host->SetTransitionDirection(PageHost::TransitionDirection::Left);
            else if (target < cur) host->SetTransitionDirection(PageHost::TransitionDirection::Right);
            host->NavigateTo(target);
        };

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"嵌套 PageHost 与控件联动");
        col->AddChild(Row(
            Btn(L"嵌套 A").on<&Button::Clicked>([navigateNested]() { navigateNested(0); }),
            Btn(L"嵌套 B").on<&Button::Clicked>([navigateNested]() { navigateNested(1); }),
            Btn(L"嵌套 C").on<&Button::Clicked>([navigateNested]() { navigateNested(2); })
        ).spacing(10.0f).shared());
        col->AddChild(nestedHost.shared());

        SetScrollablePage(page3, col.shared());
    }

    // ---------- 页面4：列表视图（DSL 重排）----------
    auto page4 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;



        auto listView = List().size(300.0f, 250.0f)
                            .with(&ListView::SetCheckable, true)                                    // 列表勾选框
                            .with(&ListView::SetSelectionMode, ListView::SelectionMode::Extended)
                            .cache(false);
        for (int i = 1; i <= 50; i++) listView->AddItem(L"列表项 " + std::to_wstring(i));
        listView->SetSelectedIndex(0);
        // 单项禁用 / 单项文字色（取主题色）/ ToolTip / 排序指示
        listView->SetItemDisabled(2, true);
        listView->SetItemTextColor(3, ThemeColor(ThemeRole::Danger));
        listView->SetItemToolTip(1, L"这是第 2 项的提示");
        listView->SetSortComparator([](const std::wstring& a, const std::wstring& b) { return a < b; });
        listView->SetShowSortIndicator(true);
        auto lv = listView.shared();

        auto lblListInfo = Lbl(L"当前选中：列表项 1").shared();
        lv->Connect(lv->SelectionChanged, [lblListInfo](int index) {
            if (index >= 0) lblListInfo->SetText(L"当前选中：列表项 " + std::to_wstring(index + 1));
            else            lblListInfo->SetText(L"无选中项");
        });

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"列表视图 (ListView)：勾选框 / Extended 多选 / 单项禁用 / 富项 / 虚拟化 / 视图筛选");
        col->AddChild(Row(lv, lblListInfo).spacing(12.0f).shared());
        // 3 行 × 4 列表格式排布（原来是 _lbCol 计数器手工排）
        col->AddChild(Grid(
            Cell(0, 0, Btn(L"添加一项").on<&Button::Clicked>([lv]() {
                static int n = 51; lv->AddItem(L"新增项 " + std::to_wstring(n++)); })),
            Cell(0, 1, Btn(L"清空列表").on<&Button::Clicked>([lv]() { lv->Clear(); })),
            Cell(0, 2, Btn(L"排序").on<&Button::Clicked>([lv]() {
                static bool asc = true; lv->Sort(asc); asc = !asc; })),
            Cell(0, 3, Btn(L"无选择模式").on<&Button::Clicked>([lv]() {
                lv->SetSelectionMode(ListView::SelectionMode::None); })),
            Cell(1, 0, Btn(L"批量+50 (BeginUpdate)").on<&Button::Clicked>([lv]() {
                lv->BeginUpdate();
                for (int k = 0; k < 50; ++k) lv->AddItem(L"批量项 " + std::to_wstring(k + 1));
                lv->EndUpdate(); })),
            Cell(1, 1, Btn(L"富项 (图标+内嵌 Label)").on<&Button::Clicked>([lv]() {
                auto rich = Lbl(L"富项 ").textColor(ThemeColor(ThemeRole::Accent));
                auto inner = Lbl(L"[内嵌]").textColor(ThemeColor(ThemeRole::Danger));
                rich->AddChild(inner.shared());          // 内嵌 Label → 走 Window 合成递归画出
                lv->AddItemWidget(rich.shared()); })),
            Cell(1, 2, Btn(L"虚拟 20万").on<&Button::Clicked>([lv]() {
                lv->SetItemCount(200000);
                for (int i = 0; i < 200000; ++i)
                    lv->SetItem(i, L"虚拟行 " + std::to_wstring(i) + ((i % 7 == 0) ? L" *" : L"")); })),
            Cell(1, 3, Btn(L"隐藏偶数行").on<&Button::Clicked>([lv]() {
                for (int i = 0; i < lv->GetItemCount(); i += 2) lv->SetItemHidden(i, true); })),
            // 注意：SetFilter / SetViewComparator 的 lambda 存在 ListView 内部，
            //   所以这里捕获裸指针而不是 shared_ptr —— 否则 对象→回调→shared_ptr→对象 形成引用环。
            Cell(2, 0, Btn(L"筛选含7").on<&Button::Clicked>([lv]() {
                ListView* raw = lv.get();
                lv->SetFilter([raw](int i) { return raw->GetItemText(i).find(L"7") != std::wstring::npos; }); })),
            Cell(2, 1, Btn(L"按文本排序").on<&Button::Clicked>([lv]() {
                // 注意：GetItemText 每次比较走 SourceText（虚拟模式下是哈希查找）；大表建议先快照文本再比较
                ListView* raw = lv.get();
                lv->SetViewComparator([raw](int a, int b) { return raw->GetItemText(a) < raw->GetItemText(b); });
                lv->SortView(true); })),
            Cell(2, 2, Btn(L"重置视图").on<&Button::Clicked>([lv]() {
                lv->ClearHidden(); lv->ClearFilter(); lv->ClearViewSort(); }))
        ).spacing(6.0f, 6.0f).shared());

        SetScrollablePage(page4, col.shared());
    }

    // ---------- 页面5：表格视图（DSL 重排）----------
    auto page5 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;


        auto info = Lbl(L"点击单元格查看信息").shared();

        auto tableView = Table().size(600.0f, 250.0f)
                               .with(&TableView::SetCheckable, true)          // 行勾选框
                               .rows(20).cols(4)
                               .header(0, L"姓名").header(1, L"年龄").header(2, L"城市").header(3, L"备注")
                               .with(&TableView::SetColumnWidth, 0, 120.0f)
                               .with(&TableView::SetColumnWidth, 1, 80.0f)
                               .with(&TableView::SetColumnWidth, 2, 120.0f)
                               .with(&TableView::SetColumnWidth, 3, 200.0f)
                               .cache(false);
        for (int row = 0; row < 20; row++) {
            tableView->SetItem(row, 0, L"用户 " + std::to_wstring(row + 1));
            tableView->SetItem(row, 1, std::to_wstring(20 + row % 30));
            tableView->SetItem(row, 2, (row % 3 == 0) ? L"北京" : (row % 3 == 1) ? L"上海" : L"广州");
            tableView->SetItem(row, 3, L"备注信息 " + std::to_wstring(row));
        }
        tableView->SetSelectionMode(TableView::SelectionMode::Row);
        tableView->SetCurrentCell(0, 0);
        // 列排序 / 排序指示 / 单元格颜色（取主题色）/ 单元格提示 / 列对齐 / 每行高度 / 行禁用
        tableView->SetColumnComparator(1, [](const std::wstring& a, const std::wstring& b) { return a < b; });
        tableView->SetShowSortIndicator(true);
        tableView->SetCellTextColor(0, 2, ThemeColor(ThemeRole::Success));
        tableView->SetCellToolTip(0, 0, L"第一行的提示");
        tableView->SetColumnAlignment(1, TextHAlign::Center);
        tableView->SetColumnAlignment(2, TextHAlign::Center);
        tableView->SetRowHeightAt(3, 44.0f);
        tableView->SetRowDisabled(5, true);
        auto tv = tableView.shared();

        tv->Connect(tv->CellClicked, [info](int row, int col) {
            info->SetText(L"选中：行 " + std::to_wstring(row) + L", 列 " + std::to_wstring(col)); });

        auto btnRowSel = Btn(L"行选择").on<&Button::Clicked>([tv]() { tv->SetSelectionMode(TableView::SelectionMode::Row); }).shared();
        auto btnColSel = Btn(L"列选择").on<&Button::Clicked>([tv]() { tv->SetSelectionMode(TableView::SelectionMode::Column); }).shared();
        auto btnCellSel = Btn(L"单元格选择").on<&Button::Clicked>([tv]() { tv->SetSelectionMode(TableView::SelectionMode::Cell); }).shared();
        auto btnSortAge = Btn(L"按年龄排序").on<&Button::Clicked>([tv]() {
            static bool asc = true; tv->SortByColumn(1, asc); asc = !asc; }).shared();
        auto btnHideRows = Btn(L"隐藏偶数行").on<&Button::Clicked>([tv]() {
            for (int i = 0; i < tv->GetRowCount(); i += 2) tv->SetItemHidden(i, true); }).shared();
        // SetFilter 的 lambda 存在 TableView 内部 → 捕获裸指针，避免引用环
        auto btnFilter = Btn(L"筛选含1").on<&Button::Clicked>([tv]() {
            TableView* raw = tv.get();
            tv->SetFilter([raw](int r) { return raw->GetItemText(r, 0).find(L"1") != std::wstring::npos; }); }).shared();
        auto btnReset = Btn(L"重置视图").on<&Button::Clicked>([tv]() {
            tv->ClearHidden(); tv->ClearFilter(); tv->ClearViewSort(); }).shared();
        auto btnBig = Btn(L"表格 1 万行").on<&Button::Clicked>([tv]() {
            tv->SetRowCount(10000);
            for (int r = 0; r < 10000; ++r) {
                tv->SetItem(r, 0, L"用户 " + std::to_wstring(r + 1));
                tv->SetItem(r, 1, std::to_wstring(18 + r % 40));
                tv->SetItem(r, 2, (r % 3 == 0) ? L"北京" : (r % 3 == 1) ? L"上海" : L"广州");
                tv->SetItem(r, 3, L"备注 " + std::to_wstring(r));
            } }).shared();
        auto btnHideCol = Btn(L"隐藏备注列").shared();
        btnHideCol->Connect(btnHideCol->Clicked, [tv, b = btnHideCol.get()]() {   // 自引用要裸指针（否则引用环）
            bool vis = tv->IsColumnVisible(3);
            tv->SetColumnVisible(3, !vis);
            b->SetText(!vis ? L"隐藏备注列" : L"显示备注列");
        });

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"表格视图 (TableView)：行勾选框 / 行·列·单元格三种选择 / 列排序 / 隐藏列 / 万行虚拟化");
        col->AddChild(tv);
        col->AddChild(info);
        col->AddChild(Grid(
            Cell(0, 0, btnRowSel), Cell(0, 1, btnColSel), Cell(0, 2, btnCellSel), Cell(0, 3, btnSortAge),
            Cell(1, 0, btnHideRows), Cell(1, 1, btnFilter), Cell(1, 2, btnReset), Cell(1, 3, btnBig),
            Cell(2, 0, btnHideCol)
        ).spacing(6.0f, 6.0f).shared());

        SetScrollablePage(page5, col.shared());
    }

    // ---------- 页面6：树形视图（DSL 重排）----------
    auto page6 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto treeView = Tree().size(500.0f, 300.0f).cache(false)
                             .with(&TreeView::SetColumnCount, 2)
                             .apply([](TreeView& t) { t.SetHeaderLabels({ L"名称", L"类型" }); })   // 花括号列表不能走 with 转发
                             .with(&TreeView::SetColumnWidth, 0, 250.0f)
                             .with(&TreeView::SetColumnWidth, 1, 150.0f);

        // 树结构（两列：名称 / 类型）
        auto root1 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点1", L"文件夹" });
        treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-1", L"文件" });
        auto child1_2 = treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-2", L"文件夹" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-1", L"文件" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-2", L"文件" });
        auto root2 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点2", L"文件夹" });
        treeView->AddChild(root2, std::vector<std::wstring>{ L"子节点2-1", L"文件" });

        treeView->SetSelectedNode(root1);
        treeView->ExpandNode(root1, true);
        treeView->ExpandNode(child1_2, true);
        treeView->SetDefaultExpandDepth(2);                                  // 默认展开深度
        child1_2->tooltip = L"这是一个可展开的文件夹节点";
        auto tv = treeView.shared();

        auto lblTreeInfo = Lbl(L"点击节点查看信息").shared();
        tv->Connect(tv->SelectionChanged, [lblTreeInfo](std::shared_ptr<TreeNode> node) {
            if (node) {
                std::wstring text = (!node->columns.empty() && node->columns[0]) ? node->columns[0]->GetText() : L"";
                lblTreeInfo->SetText(L"选中节点：" + text);
            }
            else {
                lblTreeInfo->SetText(L"无选中节点");
            }
        });
        tv->Connect(tv->NodeClicked, [lblTreeInfo, raw = tv.get()](std::shared_ptr<TreeNode> node) {
            if (node) lblTreeInfo->SetText(L"路径：" + raw->GetNodePath(node)); });

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"树形视图 (TreeView)：多列 / 展开深度 / 节点路径 / 过滤搜索 / 节点提示");
        col->AddChild(tv);
        col->AddChild(lblTreeInfo);
        col->AddChild(Grid(
            Cell(0, 0, Btn(L"展开全部").on<&Button::Clicked>([tv, root1, root2]() {
                tv->ExpandNodeRecursive(root1, true); tv->ExpandNodeRecursive(root2, true); })),
            Cell(0, 1, Btn(L"折叠全部").on<&Button::Clicked>([tv, root1, root2]() {
                tv->ExpandNodeRecursive(root1, false); tv->ExpandNodeRecursive(root2, false); })),
            Cell(0, 2, Btn(L"搜索 1-2").on<&Button::Clicked>([tv]() { tv->Search(L"1-2"); })),
            Cell(0, 3, Btn(L"清除过滤").on<&Button::Clicked>([tv]() { tv->ClearFilter(); }))
        ).spacing(6.0f, 6.0f).shared());

        SetScrollablePage(page6, col.shared());
    }

    // ---------- 页面7：树形视图增强（DSL 重排）----------
    auto page7 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto tree = Tree().size(560.0f, 320.0f).cache(false)
                         .with(&TreeView::SetColumnCount, 3)
                         .apply([](TreeView& t) { t.SetHeaderLabels({ L"名称", L"大小", L"类型" }); })
                         .with(&TreeView::SetColumnWidth, 0, 260.0f)
                         .with(&TreeView::SetColumnWidth, 1, 100.0f)
                         .with(&TreeView::SetColumnWidth, 2, 120.0f)
                         .with(&TreeView::SetSelectionMode, TreeView::SelectionMode::Extended)
                         .with(&TreeView::SetAlternatingRowColors, true)
                         // SetCheckable 已拆成单参（无默认参数）→ `.with` 直接可用；
                         //   旧的“只设视图级开关、不动节点”用 SetCheckableShallow。
                         .with(&TreeView::SetCheckable, true);

        auto proj = tree->AddRoot(std::vector<std::wstring>{ L"我的项目", L"", L"文件夹" });
        proj->icon = L"\u25A0";
        proj->checkable = true; proj->checkState = TreeNode::CheckState::PartiallyChecked;
        auto src = tree->AddChild(proj, std::vector<std::wstring>{ L"src", L"", L"文件夹" });
        src->icon = L"\u25A0"; src->checkable = true; src->checkState = TreeNode::CheckState::PartiallyChecked;
        auto mainCpp = tree->AddChild(src, std::vector<std::wstring>{ L"main.cpp", L"12 KB", L"源文件" });
        mainCpp->icon = L"\u2022"; mainCpp->checkable = true; mainCpp->checkState = TreeNode::CheckState::Checked;
        auto uiCpp = tree->AddChild(src, std::vector<std::wstring>{ L"ui.cpp", L"8 KB", L"源文件" });
        uiCpp->icon = L"\u2022"; uiCpp->checkable = true; uiCpp->checkState = TreeNode::CheckState::Unchecked;
        auto docs = tree->AddChild(proj, std::vector<std::wstring>{ L"docs", L"", L"文件夹" });
        docs->icon = L"\u25A0"; docs->checkable = true; docs->checkState = TreeNode::CheckState::Unchecked;
        tree->AddChild(docs, std::vector<std::wstring>{ L"README.md", L"4 KB", L"文本" })->icon = L"\u2022";
        tree->AddChild(docs, std::vector<std::wstring>{ L"API.md", L"20 KB", L"文本" })->icon = L"\u2022";
        auto buildDir = tree->AddRoot(std::vector<std::wstring>{ L"build", L"", L"文件夹" });
        buildDir->icon = L"\u25A0";
        tree->AddChild(buildDir, std::vector<std::wstring>{ L"ZufyUI.exe", L"600 KB", L"程序" })->icon = L"\u2022";
        tree->ExpandAll();
        tree->SetSelectedNode(proj);
        auto tv7 = tree.shared();

        auto info = Lbl(L"选中 1 个节点").shared();
        tv7->Connect(tv7->SelectionChangedMulti, [info](std::vector<std::shared_ptr<TreeNode>> nodes) {
            info->SetText(L"选中 " + std::to_wstring(nodes.size()) + L" 个节点"); });
        tv7->Connect(tv7->ItemCheckStateChanged, [info](std::shared_ptr<TreeNode> node, TreeNode::CheckState st) {
            info->SetText((node->columns.empty() || !node->columns[0] ? std::wstring() : node->columns[0]->GetText()) +
                          (st == TreeNode::CheckState::Checked ? L" 已勾选" :
                           (st == TreeNode::CheckState::PartiallyChecked ? L" 部分勾选" : L" 取消勾选"))); });
        tv7->Connect(tv7->ItemDoubleClicked, [](std::shared_ptr<TreeNode> node) {
            ZuiInfo(L"双击节点", (node->columns.empty() || !node->columns[0] ? std::wstring() : node->columns[0]->GetText())); });


        // 需要改自己文字的三个按钮：捕获裸指针，避免 对象→信号→lambda→shared_ptr→对象 引用环
        auto bCheckMode = Btn(L"勾选:联动").shared();
        bCheckMode->Connect(bCheckMode->Clicked, [tv7, b = bCheckMode.get()]() {
            bool indep = (tv7->GetCheckMode() == TreeView::CheckMode::Independent);
            tv7->SetCheckMode(indep ? TreeView::CheckMode::Linked : TreeView::CheckMode::Independent);
            b->SetText(indep ? L"勾选:联动" : L"勾选:独立"); });
        auto bMarquee = Btn(L"框选:开").shared();
        bMarquee->Connect(bMarquee->Clicked, [tv7, b = bMarquee.get()]() {
            bool on = tv7->IsMarqueeEnabled();
            tv7->SetMarqueeEnabled(!on);
            b->SetText(!on ? L"框选:开" : L"框选:关"); });
        auto bSync = Btn(L"框选同步勾选:关").shared();
        bSync->Connect(bSync->Clicked, [tv7, b = bSync.get()]() {
            bool on = tv7->IsMarqueeCheckSync();
            tv7->SetMarqueeCheckSync(!on);
            b->SetText(!on ? L"框选同步勾选:开" : L"框选同步勾选:关"); });

        auto btnCol = Col().spacing(8.0f)
            .add(Btn(L"展开全部").on<&Button::Clicked>([tv7]() { tv7->ExpandAll(); }))
            .add(Btn(L"折叠全部").on<&Button::Clicked>([tv7]() { tv7->CollapseAll(); }))
            .add(Btn(L"添加节点").on<&Button::Clicked>([tv7]() {
                auto sel = tv7->GetSelectedNode();
                if (sel) {
                    auto n = tv7->AddChild(sel, std::vector<std::wstring>{ L"新节点", L"-", L"新建" });
                    n->icon = L"\u2022";
                    tv7->ExpandNode(sel, true);
                }
                else {
                    auto n = tv7->AddRoot(std::vector<std::wstring>{ L"新节点", L"-", L"根" });
                    n->icon = L"\u25A0";
                } }))
            .add(Btn(L"删除选中").on<&Button::Clicked>([tv7]() {
                auto nodes = tv7->GetSelectedNodes();
                for (auto& n : nodes) tv7->RemoveNode(n); }))
            .add(Btn(L"按名称排序").on<&Button::Clicked>([tv7]() {
                tv7->SortChildren(nullptr, true, [](const std::shared_ptr<TreeNode>& a, const std::shared_ptr<TreeNode>& b) {
                    const std::wstring ta = (a->columns.empty() || !a->columns[0]) ? L"" : a->columns[0]->GetText();
                    const std::wstring tb = (b->columns.empty() || !b->columns[0]) ? L"" : b->columns[0]->GetText();
                    return ta < tb;   // 按首列文本排序（原来比较 shared_ptr 是指针地址）
                }); }))
            .add(Btn(L"全选").on<&Button::Clicked>([tv7]() { tv7->SelectAll(); }))
            .add(bCheckMode).add(bMarquee).add(bSync);

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"树形视图增强：勾选框（三态·联动/独立）/ 多选 / 图标 / 排序 / 框选");
        col->AddChild(Row(tv7, btnCol.shared()).spacing(12.0f).shared());
        col->AddChild(info);

        SetScrollablePage(page7, col.shared());
    }

    // ---------- 页面8：图像与图标（DSL 重排）----------
    // 图标色一律取主题角色（原来硬编码 → 切深色主题后不变）
    auto page8 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;


        const Color kIconText  = ThemeColor(ThemeRole::Text);
        const Color kIconBlue  = ThemeColor(ThemeRole::Accent);
        const Color kIconRed   = ThemeColor(ThemeRole::Danger);
        const Color kIconGreen = ThemeColor(ThemeRole::Success);
        const Color kIconAmber = ThemeColor(ThemeRole::Warning);

        auto iconLabel = [](const std::wstring& t, Icon ic, Color c) {
            return Lbl(t).apply([ic, c](Label& l) { l.SetIcon(ic); l.SetIconColor(c); });
        };
        auto iconButton = [](const std::wstring& t, Icon ic, float w = 0.0f) {
            auto r = Btn(t).apply([ic](Button& b) { b.SetIcon(ic); });
            if (w > 0.0f) r.width(w);
            return r;
        };
        auto mkImg = [](const std::wstring& caption, std::shared_ptr<Image> img) {
            auto l = Lbl(caption);
            if (img) { l->SetImage(img); l->SetIconSize(48, 48); }
            return l;
        };

        // 内嵌 base64（1x1 PNG），演示从 base64 加载
        std::shared_ptr<Image> base = Image::FromBase64(
            "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKIAAACqCAYAAAAuoZR2AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAHYcAAB2HAY/l8WUAAAhXSURBVHhe7d2xauNKFAbgs7fRiwRCMKRR5ycwpLC7dAvq3KW5uNxiS7GNOneB7dLFRUBPkC7NggiBeRFV9xaOk11ZnjPSzBmdHf8fqHGIxxn9njMajZ0vbdv+RwAT+6f7AMAUEERQAUEEFRBEUAFBBBUQRFABQQQVEERQAUEEFRBEUAFBBBUQRFABQQQVEERQAUEEFRBEUOGLto2x9Tqj5X330cDykprnO7roPg6TUTYi1vQoHUIiym9vEEJldAWxfiT5HOZ0e4MYaqMqiHWc4ZCQQ30UBRFl+ZzpCSLK8llTE0SU5fOmZPmmpnW2jDAiBsQtAdVrykKuQ2lrLzAdI6JTWc6pbFpq2xFHU1LefTpJpqJ5yFBwYrcnQEUQncqyR1k1Tw/00n3Q1/XlydEi9fYkKAii29Wyz9WueQ1+mii/Ov1qUm9PwvRBdCzL4692Db396j7my/Z6Um9PxuRBlC7LRIaCDxjFN7o7+XpSb0/GxEGUL8tk3sg2YORlc3xxwx3bRfdpPqXenpBpgyhelvmJ/PXl+Ofuk3p7UiYNonxZ5ibyBa0Cv/lTb0/KhEGMUJa5iXx+5fHcfVJvT850QYxQltmJfPC1stTbkzNZEGOUZXYiH3qtLPX2BE0URLeyTC8bmmUZZYOPOVUm/kQ+9fYkTRJEU313KMse3tfB7BP5nEIPGKm3J2mCIBp6erB1oL9itXCYyHuW/SOptycrfhDNE8nm8LBkEXsin3p7sqIHkZvXeCtWtM9h5Il86u0JixzEWGWZD/zLZtZzkWM55hWZ7pP8JvX2pMUNYrSyzE3kw0u9PWneQTx6Z9mO2cb6LvZ2KMvcRH4M65wr9fbkeQdRg+NdI8xEfgT7nCv19uQlEcQjTrcPh2BuNabeXgTeQTzay3biaEqXjy8VtOv5Xe7oMqHrFrNRNPX2Yoj0cVJD1XxGG66cFLtRmzKzLOs+BB763tzSvEdEJ45Xy4elFzg/UYLIrXnt/T2bOCG8CEF0XMT+WHqBcyQfRJRlcCAeRJRlcCEcxDhl+XMppyHrKlFeUtOz/DP+SLO9KcgGMXpZZu44BL+NlXp78YgGMXpZZu44BL+NlXp7EQkGMU5Z/h13xyH0ZzhSby8muSBGL8vc1qjwn+FIvb2YxIIYvSxzW6OCf4Yj9fbiEgpi/LIcfyKfentxyQRxgrIcfSKfenuRiQSx/uGyEztkWSaixYqK7mMHeUk/Q+9zSr29yCJtAwOwExkRAYZCEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBCJqF5nlGX8sa67v5mWKfshzr/JrdeULW3/9nqgvKTm+W78f2w3Fc1nLv9T2qLYUbsd+A+n0Q8nyY+IpqJ5yM73YKr5/l3t2/lERPfL/XPNKzLdn/VBP1iJB9E8Pfj/sV3Xl8NGAVPRPMtotgn+SoheNjTL5lQxZwH9YCcfxNfwf3R+5d799TrQO9/qhR6e7GcA/WAnHERDb7+6j/nK6fbG5QQYquYZ6aiG6AeOeBCDDwTFN7pj+7+mdTajIRWo2LXUtpZjV3R/5Q/Xl7YXhX7gyAbRvJFtIMjL5vgP5Q72Cs1QNV+S0wCQl9S8Py/7tIvtx2toyrzzw4JWtt9HP7BEg8hN0Me+e2zqtcsIkFPZtNSOXPq4uHven4z30SEv/yVb/6MfeKLriPXaNjcpaNduR7/wPqaa81eEvmtvI6AfeIIjIjNBz6/CdoKp6KvKzkc/uBANonWCPnQNzMpQ9ZVbmihoN0nnox9cyAWRm6APWANj1T/Y+VCxC1v+nKEfnIgFMd4E3VD1/eQEbK/Y8VeDQtAPbuSCaK1HOQUbCMwTPdiaooJ2E/Y++sGNUBC5CfotOd0UcFD/sM+JfJYU/KEfXIkF0ToQBJug1/RorUaut8GkoB9cyQQx1gS9frTfOXAYcT62RA0+HHaaoB+ciQSRm6C/bGY9f5DlOLHXzVjrHlF+e2MfcVzW3E7id5qgH9zJBNFaj8Kxt+NQjsyrNSi+7K8vHHs70/eDC4EgMhP0MXrnUty86Jq4lZHa/gQs+9IL+mEIkSBa36AjjJpLhb51doRbekE/DBE+iNzEebATpYW5EOgfPUJiRhr0wyDBg8hNnAdz2gA6zmLbs8/v42joaLvd75iRBv0wjPc2sCzLug/BGWvbtvuQk+AjIsAYCCKogCCCCggiqIAgggreQXS/zP/8yGKo4/jjjF3vn1Lr+V3rwXx21/7Z3/Puh7G8g/iJuZMgsLB6cXNL9lPwQpvZmoZ+i5p9DZC7k4B+GCNcEJk7CaNuT3EubujWfgaI6J6WQ77Tj92JwtxJQD+MEiyI9ndPmBvjxy7o7pu9fBzcL/dbqU6eiPdvymK/qKhYWXc6ox/GCRdEaz0KO4z/YbElZirzh8OJODq4jn/HjWjoh3ECBZHZ8uSwQ9jHYtsOOgk+7CMa+mGsYEG0DgQCE/SuOCeB+5Ih9MNYYYI4xQS9x2LLLzmMlpfUcN9Rg34YLUwQFys6+WfnJf2U2r/U5/C1aYFOxMdamcvXdKAfRvPeBvZ3qGmduX1XYF429BwzMFHp7YczCSJoF6Y0A3hCEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFU+B+chozQXr4A2QAAAABJRU5ErkJggg==");

        auto col = Col().spacing(10.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"图像（WIC 解码 + Direct2D GPU 绘制/变换）");
        col->AddChild(Row(
            mkImg(L"原图（放大）", base ? base->Scaled(48, 48) : nullptr),
            mkImg(L"旋转 45°", base ? base->Scaled(48, 48)->Rotated(45) : nullptr),
            mkImg(L"水平镜像", base ? base->Scaled(48, 48)->Mirrored(true, false) : nullptr)
        ).spacing(20.0f).shared());

        // 嵌套 Label：外层带图标，内层是子 Label（内联横排）
        auto outer = Lbl(L"嵌套 Label：");
        auto inner = Lbl(L"我是内层 Label（红色）").textColor(kIconRed);
        outer->AddChild(inner.shared());
        if (base) { outer->SetImage(base->Scaled(20, 20)); outer->SetIconSize(20, 20); }
        col->AddChild(outer.shared());
        col->AddChild(Lbl(L"提示：可从文件/资源/DLL/base64 加载，支持缩放、旋转、镜像、裁剪、编码保存。").shared());

        AddSection(col, L"图标系统：FontIcon（系统图标字体；Win11 = Segoe Fluent Icons / Win10 = Segoe MDL2 Assets，自动回退）");
        col->AddChild(Row(
            Glyph(Icon::Home, 16.0f, kIconText), Glyph(Icon::Search, 16.0f, kIconBlue),
            Glyph(Icon::Settings, 16.0f, kIconText), Glyph(Icon::Favorite, 16.0f, kIconRed),
            Glyph(Icon::Add, 16.0f, kIconGreen), Glyph(Icon::Delete, 16.0f, kIconRed),
            Glyph(Icon::Edit, 16.0f, kIconText), Glyph(Icon::Copy, 16.0f, kIconText),
            Glyph(Icon::Info, 16.0f, kIconBlue), Glyph(Icon::Warning, 16.0f, kIconAmber),
            Glyph(Icon::Error, 16.0f, kIconRed), Glyph(Icon::Success, 16.0f, kIconGreen),
            Glyph(Icon::Person, 16.0f, kIconText), Glyph(Icon::Folder, 16.0f, kIconAmber),
            Glyph(Icon::Image, 16.0f, kIconBlue), Glyph(Icon::ChevronRight, 16.0f, kIconText)
        ).spacing(14.0f).shared());

        AddSection(col, L"同一图标的不同大小（字号 = 图标大小）");
        col->AddChild(Row(
            Glyph(Icon::Settings, 14.0f, kIconBlue), Glyph(Icon::Settings, 20.0f, kIconBlue),
            Glyph(Icon::Settings, 28.0f, kIconBlue), Glyph(Icon::Settings, 40.0f, kIconBlue)
        ).spacing(18.0f).shared());

        AddSection(col, L"图标 + 文字 / Label·Button 内置字形图标（不需要外部 FontIcon）");
        col->AddChild(Row(Glyph(Icon::Folder, 18.0f, kIconAmber), Lbl(L"图标 + 文字（Row 自由组合）")).spacing(6.0f).shared());
        col->AddChild(Row(
            iconLabel(L"内置图标标签", Icon::Folder, kIconAmber),
            iconLabel(L"邮件", Icon::Mail, kIconBlue),
            iconLabel(L"成功", Icon::Success, kIconGreen)
        ).spacing(16.0f).shared());
        col->AddChild(Row(
            iconButton(L"新建", Icon::Add), iconButton(L"刷新", Icon::Refresh),
            iconButton(L"删除", Icon::Delete), iconButton(L"关闭", Icon::Close),
            iconButton(L"", Icon::More, 40.0f)          // 纯图标按钮
        ).spacing(8.0f).shared());
        col->AddChild(Lbl(L"图标用系统字体（Win11 Segoe Fluent Icons / Win10 Segoe MDL2 Assets）；Icon 枚举值即码点，字号决定大小。").shared());

        SetScrollablePage(page8, col.shared());
    }

    // ---------- 页面9：多窗口 / 托盘 / 任务栏（DSL 重排）----------
    auto page9 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto col = Col().spacing(10.0f).margin(Thickness(14, 14, 14, 14));


        auto ownedPolicy = std::make_shared<Window::OwnedMinimizePolicy>(Window::OwnedMinimizePolicy::Hide);
        auto tcombo = std::make_shared<ComboBox>();
        tcombo->AddItem(L"方案1：不处理");
        tcombo->AddItem(L"方案2：最小化则隐藏");
        tcombo->AddItem(L"方案3：禁用最小化");
        tcombo->SetPlaceholder(L"选择 owned 子窗口的最小化方案");
        tcombo->Connect(tcombo->SelectionChanged, [w = &win, ownedPolicy](int idx) {
            if (idx < 0) return;
            *ownedPolicy = (Window::OwnedMinimizePolicy)idx;
            for (auto* c : w->GetOwnedWindows()) c->SetOwnedMinimizePolicy(*ownedPolicy);
            });
        tcombo->SetSelectedIndex(1);

        auto tbtn = std::make_shared<Button>(L"新建独立窗口");
        tbtn->Connect(tbtn->Clicked, []() {
            static int n = 0;
            auto w = Application::Instance().CreateWindow(360, 240, L"动态窗口");
            if (w) {
                w->GetRootColumnBox()->AddChild(std::make_shared<Label>(L"动态创建的窗口 " + std::to_wstring(++n)));
                static std::vector<std::shared_ptr<Window>> keep;
                keep.push_back(w);
                w->Show();
            }
            });

        auto tchildBtn = std::make_shared<Button>(L"子窗口 (owned)");
        tchildBtn->Connect(tchildBtn->Clicked, [w = &win, ownedPolicy]() {
            auto c = Application::Instance().CreateWindow(360, 240, L"子窗口 (owned)", w);
            if (c) {
                c->SetOwnedMinimizePolicy(*ownedPolicy);
                c->GetRootColumnBox()->AddChild(std::make_shared<Label>(L"这是主窗口的 owned 子窗口"));
                static std::vector<std::shared_ptr<Window>> keep;
                keep.push_back(c);
                c->Show();
            }
            });

        auto tshowBtn = std::make_shared<Button>(L"显示并置顶所有子窗口");
        tshowBtn->Connect(tshowBtn->Clicked, [w = &win]() {
            for (auto* c : w->GetOwnedWindows()) { c->Show(); c->Raise(); }
            });

        auto tmodalBtn = std::make_shared<Button>(L"模态窗口");
        tmodalBtn->Connect(tmodalBtn->Clicked, [w = &win]() {
            auto m = Application::Instance().CreateWindow(320, 200, L"模态窗口", w);
            if (!m) return;
            auto r = m->GetRootColumnBox();
            r->AddChild(std::make_shared<Label>(L"模态窗口：所有者被禁用，关闭后恢复"));
            auto ok = std::make_shared<Button>(L"关闭");
            ok->Connect(ok->Clicked, [mp = m.get()]() { mp->Close(); });
            r->AddChild(ok);
            static std::vector<std::shared_ptr<Window>> keep;
            keep.push_back(m);
            m->Show();
            m->RunModal(w);
            });

        // 弹窗结果会显示在这里
        auto mbResult = std::make_shared<Label>(L"（弹窗结果会显示在这里）");

        // 按钮文本库默认英文；这里切成中文演示（SetLanguage / SetButtonText）
        MessageBox::SetLanguage(MessageBox::Lang::Chinese);

        auto tmsgBtn = std::make_shared<Button>(L"消息框 (预设 FastButton)");
        tmsgBtn->Connect(tmsgBtn->Clicked, [w = &win, mbResult]() {
            MessageBox box(w, L"提示",
                L"这是一个 ZufyUI 消息框：图标 + 文字 + 按钮。\n按钮用 FastButton 位标志组合，结果可按位判定。",
                MessageBox::Icon::Info, FastButton::Yes | FastButton::No | FastButton::Cancel);
            FastButton r = box.GetResult();
            if (mbResult) mbResult->SetText(L"预设弹窗：你点了「" + MessageBox::ButtonLabel(r) + L"」" +
                (HasFlag(r, FastButton::Yes) ? L"（Yes 位命中）" : L""));
            });

        // 自定义样式①：输入框（内容直接传控件；按钮仍由 MessageBox 提供）
        auto tinputBtn = std::make_shared<Button>(L"弹窗：输入框");
        tinputBtn->Connect(tinputBtn->Clicked, [w = &win, mbResult]() {
            auto edit = std::make_shared<TextBox>();
            edit->SetPlaceholder(L"请输入名称…");
            MessageBox box(w, L"输入", edit, MessageBox::Icon::None, FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(L"输入框弹窗：你点了「" + MessageBox::ButtonLabel(box.GetResult()) +
                L"」，输入内容「" + edit->GetText() + L"」");
            });

        // 自定义样式②：三个开关
        auto tswitchBtn = std::make_shared<Button>(L"弹窗：三个开关");
        tswitchBtn->Connect(tswitchBtn->Clicked, [w = &win, mbResult]() {
            auto col = std::make_shared<ColumnBox>();
            col->SetSpacing(8.0f);
            auto s1 = std::make_shared<ToggleSwitch>(true);
            auto s2 = std::make_shared<ToggleSwitch>(false);
            auto s3 = std::make_shared<ToggleSwitch>(true);
            col->AddChild(s1); col->AddChild(s2); col->AddChild(s3);
            MessageBox box(w, L"三个开关", col, MessageBox::Icon::None, FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(std::wstring(L"开关弹窗：") + (s1->IsOn() ? L"开" : L"关") + L"/" +
                (s2->IsOn() ? L"开" : L"关") + L"/" + (s3->IsOn() ? L"开" : L"关") +
                L"，你点了「" + MessageBox::ButtonLabel(box.GetResult()) + L"」");
            });

        // 自定义样式③：密集交叉测试（把嵌套页那段布局直接塞进弹窗）
        auto tdenseBtn = std::make_shared<Button>(L"弹窗：密集交叉测试");
        tdenseBtn->Connect(tdenseBtn->Clicked, [w = &win, mbResult]() {
            auto grid = std::make_shared<GridLayout>();
            grid->SetSpacing(6, 6);
            auto l1 = std::make_shared<Label>(L"密集交叉测试：多个下拉框与控件混合");
            grid->AddChild(l1, 0, 0, 1, 4);
            auto c1 = std::make_shared<ComboBox>();
            c1->AddItem(L"选项1-A"); c1->AddItem(L"选项1-B"); c1->AddItem(L"选项1-C");
            grid->AddChild(c1, 1, 0);
            auto bA = std::make_shared<Button>(L"按钮A"); grid->AddChild(bA, 1, 1);
            auto c2 = std::make_shared<ComboBox>();
            c2->AddItem(L"选项2-A"); c2->AddItem(L"选项2-B");
            grid->AddChild(c2, 1, 2);
            auto st1 = std::make_shared<Label>(L"状态1"); grid->AddChild(st1, 1, 3);
            auto l2 = std::make_shared<Label>(L"固定文本"); grid->AddChild(l2, 2, 0);
            auto c3 = std::make_shared<ComboBox>();
            c3->AddItem(L"选项3-A"); c3->AddItem(L"选项3-B");
            grid->AddChild(c3, 2, 1);
            auto sl = std::make_shared<::ZufyUI::Slider>(); grid->AddChild(sl, 2, 2);
            auto bB = std::make_shared<Button>(L"按钮B"); grid->AddChild(bB, 2, 3);
            MessageBox box(w, L"密集交叉测试", grid, MessageBox::Icon::None, FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(L"密集弹窗：你点了「" + MessageBox::ButtonLabel(box.GetResult()) + L"」");
            });

        // 独立右键菜单：不绑定任何窗口，ShowAtCursor 在鼠标处弹出（托盘菜单将复用这套）
        auto tmenuBtn = std::make_shared<Button>(L"独立弹出菜单 (ShowAtCursor)");
        tmenuBtn->Connect(tmenuBtn->Clicked, [mbResult]() {
            auto menu = std::make_shared<::ZufyUI::Menu>();
            static bool showIcon = true;
            menu->AddCheckItemCb(L"显示图标", showIcon, [mbResult](bool on) {
                showIcon = on;
                if (mbResult) mbResult->SetText(on ? L"菜单：显示图标 = 勾选" : L"菜单：显示图标 = 取消");
                });
            static int viewMode = 0;   // 0 列表 / 1 网格（单选）
            menu->AddCheckItemRadio(L"列表视图", viewMode == 0, [mbResult](bool) { viewMode = 0; if (mbResult) mbResult->SetText(L"菜单：列表视图"); });
            menu->AddCheckItemRadio(L"网格视图", viewMode == 1, [mbResult](bool) { viewMode = 1; if (mbResult) mbResult->SetText(L"菜单：网格视图"); });
            menu->AddSeparator();
            menu->AddItem(L"菜单项 A", [mbResult]() { if (mbResult) mbResult->SetText(L"独立菜单：菜单项 A"); }, 101);
            menu->AddItem(L"默认项(回车)", [mbResult]() { if (mbResult) mbResult->SetText(L"独立菜单：默认项"); }, 102);
            menu->items.back()->isDefault = true;
            menu->items.back()->shortcut = L"Enter";
            menu->AddItem(L"复制", nullptr, 103);
            menu->items.back()->shortcut = L"Ctrl+C";
            menu->AddItem(L"删除", nullptr, 104);
            menu->items.back()->danger = true;
            menu->AddItem(L"禁用项", nullptr, 105);
            menu->items.back()->enabled = false;
            menu->AddSeparator();
            auto sub = std::make_shared<::ZufyUI::Menu>();
            sub->AddItem(L"子项 1", nullptr, 201);
            sub->AddItem(L"子项 2", nullptr, 202);
            menu->AddSubmenu(L"子菜单", sub);
            // 演示：菜单项自定义背景色 / 文字色
            if (menu->items.size() > 6) {
                menu->items[4]->bgColor = Color(0.85f, 0.93f, 1.0f, 1.0f);      // 菜单项 A：浅蓝底
                menu->items[6]->textColor = Color(0.10f, 0.60f, 0.25f, 1.0f);    // 复制：绿字
            }
            // 另一条结果通道：id
            menu->ItemSelected.connect([mbResult](int id) {
                if (mbResult) mbResult->SetText(L"独立菜单：ItemSelected id=" + std::to_wstring(id));
                }, ConnectionThread::CurrentThread, nullptr);
            menu->ShowAtCursor();
            });

        // 托盘图标 / 任务栏状态 / 统一窗口图标
        static TrayIcon s_tray;
        // 托盘左键单击回调：弹一个 ZUI 消息框
        s_tray.Clicked.connect([w = &win, mbResult]() {
            MessageBox box(w, L"托盘", L"托盘图标被左键单击了。", MessageBox::Icon::Info,
                       FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(std::wstring(L"托盘点击 → 消息框，你点了「") +
                MessageBox::ButtonLabel(box.GetResult()) + L"」");
            }, ConnectionThread::CurrentThread, nullptr);
        // 气泡通知被点击 → 也弹消息框
        s_tray.BalloonClicked.connect([w = &win, mbResult]() {
            MessageBox box(w, L"通知", L"你点击了那条托盘通知（toast）。", MessageBox::Icon::Info,
                       FastButton::OK);
            if (mbResult) mbResult->SetText(std::wstring(L"通知点击 → 消息框，你点了「") +
                MessageBox::ButtonLabel(box.GetResult()) + L"」");
            }, ConnectionThread::CurrentThread, nullptr);
        // 应用图标（从 exe 资源加载，不依赖外部文件）
        auto loadAppImage = []() -> std::shared_ptr<Image> {
            return Image::FromResource(IDI_APPICON, RT_GROUP_ICON);
        };

        // 保证"无论从哪个按钮添加托盘，右键都有菜单"（否则 menu_ 为空 → 右键不弹菜单）
        {
            auto trayMenu = std::make_shared<::ZufyUI::Menu>();
            trayMenu->AddItem(L"显示并激活 (2→3→1)", [w = &win]() { w->ShowActivate(Window::ActivateMode::All); });
            trayMenu->AddItem(L"提升到 Z 序", [w = &win]() { w->ShowActivate(Window::ActivateMode::Raise); });
            trayMenu->AddItem(L"闪烁任务栏", [w = &win]() { w->Flash(5); });
            trayMenu->AddSeparator();
            trayMenu->AddItem(L"退出", [w = &win]() { w->Close(); });
            s_tray.SetMenu(trayMenu);
        }

        auto trayBtn = std::make_shared<Button>(L"托盘图标：添加/移除");
        trayBtn->Connect(trayBtn->Clicked, [mbResult, w = &win, loadAppImage]() {
            if (!s_tray.IsAdded()) {
                auto menu = std::make_shared<::ZufyUI::Menu>();
                menu->AddItem(L"显示并激活 (2→3→1)", [w]() { w->ShowActivate(Window::ActivateMode::All); });
                menu->AddItem(L"仅提升 Z 序", [w]() { w->ShowActivate(Window::ActivateMode::Raise); });
                menu->AddItem(L"闪烁任务栏", [w]() { w->Flash(5); });
                menu->AddSeparator();
                menu->AddItem(L"退出", [w]() { w->Close(); });
                s_tray.SetMenu(menu);
                bool ok = s_tray.Add(loadAppImage(), L"ZufyUI 托盘演示");   // 库自带 Image
                if (mbResult) mbResult->SetText(ok ? L"托盘：已添加（右键看菜单，悬停看提示）" : L"托盘：添加失败");
            }
            else {
                s_tray.Remove();
                if (mbResult) mbResult->SetText(L"托盘：已移除");
            }
            });

        auto trayToastBtn = std::make_shared<Button>(L"托盘气泡(→toast) 循环主图标");
        trayToastBtn->Connect(trayToastBtn->Clicked, [mbResult, loadAppImage]() {
            if (!s_tray.IsAdded()) s_tray.Add(loadAppImage(), L"ZufyUI");
            static const TrayIcon::BalloonIcon kinds[] = {
                TrayIcon::BalloonIcon::Custom, TrayIcon::BalloonIcon::Info,
                TrayIcon::BalloonIcon::Warning, TrayIcon::BalloonIcon::Error,
                TrayIcon::BalloonIcon::None };
            static const wchar_t* names[] = { L"自定义(应用图标)", L"信息", L"警告", L"错误", L"不显示" };
            static int k = 0;
            int cur = k % 5; ++k;
            s_tray.ShowBalloon(L"ZufyUI",
                std::wstring(L"主图标 = ") + names[cur] + L"，Win10/11 会显示成 toast。", kinds[cur]);
            if (mbResult) mbResult->SetText(std::wstring(L"已发送托盘气泡，主图标=") + names[cur]);
            });

        auto trayBadgeRow = std::make_shared<RowBox>();
        trayBadgeRow->SetSpacing(8.0f);
        trayBadgeRow->AddChild(std::make_shared<Label>(L"托盘徽章"));
        auto trayBadgeSw = std::make_shared<ToggleSwitch>(false);
        trayBadgeSw->Connect(trayBadgeSw->Toggled, [mbResult, loadAppImage](bool on) {
            if (!s_tray.IsAdded()) s_tray.Add(loadAppImage(), L"ZufyUI");
            if (on) s_tray.SetBadge(); else s_tray.ClearBadge();
            if (mbResult) mbResult->SetText(on ? L"托盘徽章：开（右下角红点）" : L"托盘徽章：关");
            });
        trayBadgeRow->AddChild(trayBadgeSw);

        auto progRow = std::make_shared<RowBox>();
        progRow->SetSpacing(8.0f);
        progRow->AddChild(std::make_shared<Label>(L"任务栏进度"));
        auto progSlider = std::make_shared<::ZufyUI::Slider>();
        progSlider->SetRange(0, 100);
        progSlider->SetWidth(200);
        progSlider->Connect(progSlider->ValueChanged, [mbResult, w = &win](float v) {
            int p = (int)(v + 0.5f);
            if (p <= 0) w->SetTaskbarProgress(Window::TaskbarProgress::None);
            else w->SetTaskbarProgress(Window::TaskbarProgress::Normal, (ULONGLONG)p, 100);
            if (mbResult) mbResult->SetText(L"任务栏进度：" + std::to_wstring(p) + L"%");
            });
        progRow->AddChild(progSlider);

        auto appIconBtn = std::make_shared<Button>(L"统一设置应用图标(项目自带图标)");
        appIconBtn->Connect(appIconBtn->Clicked, [mbResult, w = &win, loadAppImage]() {
            w->SetAppIcon(loadAppImage());   // 库自带 Image，直接传
            if (mbResult) mbResult->SetText(L"已用项目自带图标统一设置（原生 + 自定义标题栏）");
            });

        auto tbBadgeRow = std::make_shared<RowBox>();
        tbBadgeRow->SetSpacing(8.0f);
        tbBadgeRow->AddChild(std::make_shared<Label>(L"任务栏覆盖徽章"));
        auto tbBadgeSw = std::make_shared<ToggleSwitch>(false);
        tbBadgeSw->Connect(tbBadgeSw->Toggled, [mbResult, w = &win, loadAppImage](bool on) {
            if (on) w->SetTaskbarOverlayIcon(loadAppImage(), L"徽章");
            else w->ClearTaskbarOverlayIcon();
            if (mbResult) mbResult->SetText(on ? L"任务栏覆盖徽章：开（看任务栏按钮右下角）" : L"任务栏覆盖徽章：关");
            });
        tbBadgeRow->AddChild(tbBadgeSw);

        auto tbIconRow = std::make_shared<RowBox>();
        tbIconRow->SetSpacing(8.0f);
        tbIconRow->AddChild(std::make_shared<Label>(L"标题栏图标"));
        tbIconRow->AddChild(std::make_shared<Label>(L"徽章"));
        auto tbIconBadgeSw = std::make_shared<ToggleSwitch>(false);
        tbIconBadgeSw->Connect(tbIconBadgeSw->Toggled, [w = &win, loadAppImage](bool on) {
            w->SetAppIcon(loadAppImage());
            if (auto tb = std::dynamic_pointer_cast<TitleBar>(w->GetCustomTitleBar())) {
                if (on) tb->SetIconBadge(true); else tb->ClearIconBadge();
            }
            });
        tbIconRow->AddChild(tbIconBadgeSw);
        tbIconRow->AddChild(std::make_shared<Label>(L"进度"));
        auto tbIconProgSlider = std::make_shared<::ZufyUI::Slider>();
        tbIconProgSlider->SetRange(0, 100);
        tbIconProgSlider->SetWidth(160);
        tbIconProgSlider->Connect(tbIconProgSlider->ValueChanged, [w = &win, loadAppImage](float v) {
            w->SetAppIcon(loadAppImage());
            if (auto tb = std::dynamic_pointer_cast<TitleBar>(w->GetCustomTitleBar())) {
                float p = clamp(v / 100.0f, 0.0f, 1.0f);
                if (p <= 0.001f) tb->ClearIconProgress(); else tb->SetIconProgress(p);
            }
            });
        tbIconRow->AddChild(tbIconProgSlider);

        auto thumbBarBtn = std::make_shared<Button>(L"缩略图工具栏(库自带Image)");
        thumbBarBtn->Connect(thumbBarBtn->Clicked, [mbResult, w = &win, loadAppImage]() {
            std::vector<std::pair<Window::ThumbButtonId, std::wstring>> bs = {
                { Window::ThumbButtonId::Prev, L"上一个" },
                { Window::ThumbButtonId::Play, L"播放" },
                { Window::ThumbButtonId::Pause, L"暂停" },
                { Window::ThumbButtonId::Next, L"下一个" } };
            std::vector<std::shared_ptr<Image>> imgs = {
                loadAppImage(), loadAppImage(), loadAppImage(), loadAppImage() };
            w->SetThumbButtons(bs, imgs);
            static bool thumbHooked = false;
            if (!thumbHooked) {   // 每个缩略图按钮点击 → 弹窗提示是哪一个
                thumbHooked = true;
                w->ThumbButtonClicked.connect([w, mbResult](Window::ThumbButtonId id) {
                    const wchar_t* names[] = { L"播放", L"暂停", L"上一个", L"下一个", L"未知" };
                    int idx = (int)id; if (idx < 0 || idx > 3) idx = 4;
                    MessageBox box(w, L"缩略图按钮", std::wstring(L"点击了：") + names[idx],
                                   MessageBox::Icon::Info, FastButton::OK);
                    if (mbResult) mbResult->SetText(std::wstring(L"缩略图按钮「") + names[idx] + L"」被点击");
                    }, ConnectionThread::CurrentThread, nullptr);
            }
            if (mbResult) mbResult->SetText(L"已设置缩略图工具栏（鼠标悬停任务栏按钮查看）");
            });

        auto jumpListBtn = std::make_shared<Button>(L"跳转列表：自定义任务/分类");
        jumpListBtn->Connect(jumpListBtn->Clicked, [mbResult, w = &win]() {
            std::vector<Window::JumpListItem> tasks = {
                { L"新建窗口", L"--new-window" },
                { L"打开设置", L"--settings" },
                { L"关于",     L"--about" },
            };
            std::vector<std::pair<std::wstring, std::vector<Window::JumpListItem>>> cats = {
                { L"常用操作", {
                    { L"打开项目 A", L"--open A" },
                    { L"打开项目 B", L"--open B" },
                } },
            };
            w->SetJumpList(tasks, cats, true);
            if (mbResult) mbResult->SetText(L"已写入跳转列表（右键任务栏按钮查看 Tasks / 常用操作 / 最近使用）");
            });


        auto tclose = std::make_shared<Button>(L"关闭主窗口");
        tclose->Connect(tclose->Clicked, [w = &win]() { w->Close(); });

        // ---- 按功能分节改成多张可折叠卡片（原来是一大块平铺，看着很乱）----
        auto addCard = [&col](const std::wstring& title, const std::wstring& sub, bool expanded, const Ref<ColumnBox>& body) {
            col->AddChild(make_expander(title, sub).fillWidth(true)
                              .with(&Expander::SetExpanded, expanded)
                              .with(&Expander::SetContent, body.shared()).shared());
        };

        addCard(L"多窗口 / owned 子窗口 / 模态", L"独立窗口 / 子窗口 / 模态 / 关闭主窗口", true,
                Col().spacing(8.0f)
                     .add(tcombo)
                     .add(Row(tbtn, tchildBtn).spacing(8.0f).shared())
                     .add(Row(tshowBtn, tmodalBtn).spacing(8.0f).shared())
                     .add(tclose)
                     .add(mbResult));

        addCard(L"消息框与自定义内容弹窗", L"预设按钮 / 输入框 / 三个开关 / 密集交叉", true,
                Col().spacing(8.0f)
                     .add(tmsgBtn)
                     .add(Row(tinputBtn, tswitchBtn).spacing(8.0f).shared())
                     .add(tdenseBtn));

        addCard(L"独立弹出菜单", L"ShowAtCursor：不绑定任何窗口，在鼠标处弹出", false,
                Col().spacing(8.0f).add(tmenuBtn));

        addCard(L"托盘图标 / 徽章 / 气泡通知", L"添加移除 / 气泡 toast / 右上角徽章", false,
                Col().spacing(8.0f)
                     .add(Row(trayBtn, trayToastBtn).spacing(8.0f).shared())
                     .add(trayBadgeRow));

        addCard(L"任务栏：进度 / 覆盖徽章", L"任务栏按钮上的进度条与右下角覆盖图标", false,
                Col().spacing(8.0f).add(progRow).add(tbBadgeRow));

        addCard(L"自定义标题栏：图标徽章 / 图标进度", L"标题栏图标上的红点与进度环", false,
                Col().spacing(8.0f).add(tbIconRow));

        addCard(L"缩略图工具栏 / 跳转列表 / 应用图标", L"任务栏悬停按钮 / 右键跳转列表 / 统一应用图标", false,
                Col().spacing(8.0f).add(thumbBarBtn).add(jumpListBtn).add(appIconBtn));

        SetScrollablePage(page9, col.shared());
    }

    // ---------- 页面10：窗口属性（DSL 重排）----------
    auto page10 = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto backdropState = std::make_shared<Backdrop>(Backdrop::Acrylic);
        auto tintState = std::make_shared<DWORD>(0x80FFFFFFu);
        auto followTheme = std::make_shared<bool>(true);   // true = 底色跟随主题（不锁定为固定色）

        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));

        const float kLabelW = 220.0f;   // 左侧说明列统一宽度，右侧控件对齐

        AddSection(col, L"背景层与叠加色");
        // 背景模式（空 / 亚克力 / 云母）：颜色叠在这个背景之上
        auto backdropCombo = Combo().items({ L"空（透明）", L"亚克力", L"云母" }).fillWidth(true);
        backdropCombo->SetSelectedIndex(1);
        backdropCombo->Connect(backdropCombo->SelectionChanged, [w = &win, backdropState, tintState, followTheme](int idx) {
            if (idx < 0) return;
            *backdropState = (idx == 2) ? Backdrop::Mica : (idx == 1 ? Backdrop::Acrylic : Backdrop::None);
            if (*followTheme) w->SetBackdrop(*backdropState);                // 只换模式 → 底色继续跟主题
            else              w->SetBackdrop(*backdropState, *tintState);    // 已显式指定底色 → 保持
        });
        col->AddChild(Row(Lbl(L"背景层（亚克力 / 云母 / 无）").minWidth(kLabelW), backdropCombo).spacing(10.0f).shared());

        // 背景叠加层（ARGB）：A = 能透出多少背景（0 全透、255 不透），RGB = 叠加颜色
        auto colorCombo = Combo().items({
            L"跟随主题（推荐：浅色白 / 深色黑）",
            L"全透明 0x00000000（透出背景）",
            L"白色不透明 0xFFFFFFFF（纯白，盖住背景）",
            L"白色 50%  0x80FFFFFF（半透明白 + 背景）",
            L"红色 50%  0x80FF0000（半透明红 + 背景）",
            L"深灰不透明 0xFF202020" }).fillWidth(true);
        colorCombo->SetSelectedIndex(0);
        colorCombo->Connect(colorCombo->SelectionChanged, [w = &win, backdropState, tintState, followTheme](int idx) {
            static const DWORD kColors[] = { 0x00000000u, 0xFFFFFFFFu, 0x80FFFFFFu, 0x80FF0000u, 0xFF202020u };
            if (idx < 0) return;
            if (idx == 0) { *followTheme = true; w->SetBackdrop(*backdropState); return; }   // 交回主题
            int k = idx - 1;
            if (k > 4) return;
            *followTheme = false;
            *tintState = kColors[k];
            w->SetBackdrop(*backdropState, *tintState);
        });
        col->AddChild(Row(Lbl(L"背景叠加层（带 Alpha 的颜色）").minWidth(kLabelW), colorCombo).spacing(10.0f).shared());

        AddSection(col, L"窗口行为开关");
        col->AddChild(Row(Lbl(L"使用自定义标题栏").minWidth(kLabelW),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win, titleBar](bool on) {
                if (on) w->SetCustomTitleBar(titleBar);
                else w->SetCustomTitleBar(nullptr); })
        ).spacing(10.0f).shared());
        col->AddChild(Row(Lbl(L"标题栏可见 SetTitleBarVisible").minWidth(kLabelW),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win](bool on) { w->SetTitleBarVisible(on); })
        ).spacing(10.0f).shared());
        col->AddChild(Row(Lbl(L"可调整大小 SetResizable").minWidth(kLabelW),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win](bool on) { w->SetResizable(on); })
        ).spacing(10.0f).shared());

        // 页面切换缓动
        auto easingCombo = Combo().items({ L"Linear（匀速）", L"EaseInOut（平滑）", L"EaseOut（缓出）" }).fillWidth(true);
        easingCombo->SetSelectedIndex(1);
        easingCombo->Connect(easingCombo->SelectionChanged, [mainHost](int idx) {
            if (idx < 0) return;
            mainHost->SetTransitionEasing((PageHost::TransitionEasing)idx);
        });
        col->AddChild(Row(Lbl(L"页面切换缓动 SetTransitionEasing").minWidth(kLabelW), easingCombo).spacing(10.0f).shared());

        AddSection(col, L"亚克力 / 云母参数（选对应背景层才显示该卡片）");
        // 一个参数项 = 「名称 + 滑条」的 DSL Row
        auto addSlider = [](const std::wstring& name, float lo, float hi, float step, float init, std::function<void(float)> apply) {
            auto s = make_slider().fillWidth(true)
                           .with(&::ZufyUI::Slider::SetRange, lo, hi)
                           .with(&::ZufyUI::Slider::SetStep, step)
                           .with(&::ZufyUI::Slider::SetSnapToStep, true)
                           .value(init);
            s.on<&::ZufyUI::Slider::ValueChanged>([apply](float v) { apply(v); });
            return Row(Lbl(name).minWidth(90.0f), s).spacing(10.0f);   // 名字列 minWidth（原固定 width(90) 会把长名字省略号裁掉）
        };

        auto acrylicCard = make_card().padding(10.0f).with(&::ZufyUI::Card::SetShadow, true);
        if (auto g = acrylicCard->GetLayoutAs<GridLayout>()) {
            g->SetSpacing(8, 8);
            g->SetColumnStretch(1, 1.0f);   // 滑条列吃掉剩余宽度（原来只占自然宽 → 卡片右侧大片空白）
            g->AddChild(std::make_shared<Label>(L"亚克力参数"), 0, 0, 1, 2);
            int r = 1;
            g->AddChild(addSlider(L"模糊量", 0.0f, 200.0f, 1.0f, Window::AcrylicParams.blurAmount,
                [](float v) { Window::AcrylicParams.blurAmount = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"饱和度", 0.0f, 3.0f, 0.05f, Window::AcrylicParams.saturation,
                [](float v) { Window::AcrylicParams.saturation = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"色调 alpha", 0.0f, 1.0f, 0.05f, Window::AcrylicParams.tint.a,
                [](float v) { Window::AcrylicParams.tint.a = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"噪点", 0.0f, 0.1f, 0.005f, Window::AcrylicParams.noiseOpacity,
                [](float v) { Window::AcrylicParams.noiseOpacity = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); }).shared(), r++, 0, 1, 2);
        }
        col->AddChild(acrylicCard.shared());

        auto micaCard = make_card().padding(10.0f).with(&::ZufyUI::Card::SetShadow, true);
        if (auto g = micaCard->GetLayoutAs<GridLayout>()) {
            g->SetSpacing(8, 8);
            g->SetColumnStretch(1, 1.0f);   // 滑条列吃掉剩余宽度（原来只占自然宽 → 卡片右侧大片空白）
            g->AddChild(std::make_shared<Label>(L"云母参数"), 0, 0, 1, 2);
            int r = 1;
            g->AddChild(addSlider(L"模糊量", 0.0f, 400.0f, 1.0f, Window::MicaParams.blurAmount,
                [](float v) { Window::MicaParams.blurAmount = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"饱和度", 0.0f, 3.0f, 0.05f, Window::MicaParams.saturation,
                [](float v) { Window::MicaParams.saturation = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"色调 alpha", 0.0f, 1.0f, 0.05f, Window::MicaParams.tint.a,
                [](float v) { Window::MicaParams.tint.a = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); }).shared(), r++, 0, 1, 2);
            g->AddChild(addSlider(L"噪点", 0.0f, 0.1f, 0.005f, Window::MicaParams.noiseOpacity,
                [](float v) { Window::MicaParams.noiseOpacity = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); }).shared(), r++, 0, 1, 2);
        }
        col->AddChild(micaCard.shared());

        // 选亚克力显示亚克力卡；选云母显示云母卡；选无则都隐藏
        auto syncCards = [acrylicCard, micaCard, backdropCombo]() {
            int b = backdropCombo->GetSelectedIndex();
            acrylicCard->SetVisible(b == 1);
            micaCard->SetVisible(b == 2);
        };
        backdropCombo->Connect(backdropCombo->SelectionChanged, [syncCards](int) { syncCards(); });
        syncCards();

        SetScrollablePage(page10, col.shared());
    }

    // 所有页面加入 PageHost
    // ---------- 新控件页 A：TabView + RadioGroup（DSL 重排）----------
    auto pageNewA = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto col = Col().spacing(10.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"TabView：12 个页签（带图标，每 3 个可关闭）");
        static const Icon kTabIcons[] = { Icon::Home, Icon::Search, Icon::Settings, Icon::Folder,
                                          Icon::File, Icon::Person, Icon::Calendar, Icon::Mail,
                                          Icon::Image, Icon::Favorite, Icon::View, Icon::Info };
        auto tabs = Tabs().height(150.0f);
        for (int i = 1; i <= 12; ++i) {
            auto tabLabel = Lbl(L"页签 " + std::to_wstring(i)).apply([i](Label& l) { l.SetIcon(kTabIcons[i - 1]); });
            tabs->AddTab(tabLabel.shared(),
                         Lbl(L"  这里是「页签 " + std::to_wstring(i) + L"」的内容。").shared(),
                         (i % 3 == 0));
        }
        tabs->Connect(tabs->TabCloseRequested, [t = tabs.shared()](int idx) { t->RemoveTab(idx); });
        col->AddChild(tabs.shared());

        AddSection(col, L"RadioGroup：纵向（含禁用项） / 横向");
        // RadioGroup 与类同名 → 工厂必须全限定
        auto radios = make_radio_group();
        radios->AddItemSelected(L"单选 A（默认选中）", true);
        radios->AddItem(L"单选 B");
        radios->AddItem(L"单选 C");
        radios->AddItem(L"单选 D（禁用）");
        if (auto d = radios->GetButton(3)) d->SetEnabled(false);
        col->AddChild(radios.shared());

        auto radiosH = make_radio_group()
            .with(&::ZufyUI::RadioGroup::SetOrientation, ::ZufyUI::RadioGroup::Orientation::Horizontal);
        radiosH->AddItemSelected(L"甲", true);
        radiosH->AddItem(L"乙");
        radiosH->AddItem(L"丙");
        col->AddChild(radiosH.shared());

        SetScrollablePage(pageNewA, col.shared());
    }

    // ---------- 新控件页 B：ProgressRing + SplitView（DSL 重排）----------
    auto pageNewB = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto col = Col().spacing(10.0f).margin(Thickness(14, 14, 14, 14));


        AddSection(col, L"ProgressRing：定值 / 自定义色（主题色）/ 不定态（持续旋转）");
        // 注：DSL 的 Ring() 工厂是 Make<ProgressRing>() 无参，无法传尺寸；本页演示要 36px → 仍用类直接构造
        auto ring1 = std::make_shared<ProgressRing>(36.0f); ring1->SetValue(0.35f);
        auto ring2 = std::make_shared<ProgressRing>(36.0f); ring2->SetValue(0.72f);
        ring2->SetColor(ThemeManager::AppTheme().Get(ThemeRole::Success));   // 原来硬编码绿 → 主题色
        auto ring3 = std::make_shared<ProgressRing>(36.0f); ring3->SetIndeterminate(true);
        col->AddChild(Row(ring1, ring2, ring3).spacing(16.0f).shared());

        AddSection(col, L"SplitView：左 = Card（内嵌 PageHost + 导航按钮）/ 右 = ListView（按钮模式）");
        auto nested = Pages().height(120.0f);
        auto navRow = Row().spacing(6.0f);
        for (int i = 1; i <= 3; ++i) {
            auto pg = make_page();
            pg->SetLayout(Lbl(L"交叉页 " + std::to_wstring(i)).margin(Thickness(8, 8, 8, 8)).shared());
            nested->AddPage(pg.shared());
            navRow.add(Btn(L"页" + std::to_wstring(i)).size(44.0f, 26.0f)
                       .on<&Button::Clicked>([n = nested.shared(), i]() { n->NavigateTo(i - 1); }));
        }

        auto leftPane = make_card();
        if (auto lg = leftPane->GetLayoutAs<GridLayout>()) {
            lg->AddChild(navRow.shared(), 0, 0);
            lg->AddChild(nested.shared(), 1, 0);
        }
        auto rightPane = List().with(&ListView::SetButtonMode, true);
        for (int i = 1; i <= 8; ++i) rightPane->AddItem(L"列表项 " + std::to_wstring(i));

        auto split = Split().height(200.0f)
                            .with(&SplitView::SetSplitterWidth, 8.0f)
                            .with(&SplitView::SetMinFirst, 120.0f)
                            .with(&SplitView::SetMinSecond, 120.0f)
                            .with(&SplitView::SetFirst, leftPane.shared())
                            .with(&SplitView::SetSecond, rightPane.shared());
        col->AddChild(split.shared());

        // ---- SplitView 折叠功能：拖到距边缘阈值内 → 先出"即将折叠"提示（不改内容），松手才真折叠；
        //      折叠后该侧出现圆角浅灰小按钮（箭头指展开方向），点它展开到最小状态；整个功能可关闭。----
        AddSection(col, L"SplitView 折叠：拖到边缘 → 先提示、松手才折叠；折叠后点边上的小按钮展开（可关闭）");
        auto splitState = Lbl(L"状态：拖分隔条到距边缘 28px 内试试（松手折叠）").shared();
        auto splitRef = split.shared();
        col->AddChild(Row(
            Lbl(L"折叠功能").width(80.0f),
            Toggle(true).on<&ToggleSwitch::Toggled>([splitRef, splitState](bool on) {
                splitRef->SetCollapseEnabled(on);
                splitState->SetText(on ? L"状态：折叠功能已开启（拖到边缘会先出现提示）" : L"状态：折叠功能已关闭（拖到边缘不再提示/折叠）"); }),
            Lbl(L"阈值").width(44.0f).margin(Thickness(16, 0, 0, 0)),
            make_slider().width(140.0f)
                .with(&::ZufyUI::Slider::SetRange, 8.0f, 80.0f)
                .with(&::ZufyUI::Slider::SetStep, 4.0f)
                .with(&::ZufyUI::Slider::SetSnapToStep, true)
                .value(28.0f)
                .on<&::ZufyUI::Slider::ValueChanged>([splitRef, splitState](float v) {
                    splitRef->SetCollapseThreshold(v);
                    splitState->SetText(L"状态：折叠阈值 = " + std::to_wstring((int)v) + L" px（拖到该范围内松手即折叠）"); }),
            Btn(L"折叠左栏").on<&Button::Clicked>([splitRef, splitState]() { splitRef->CollapseFirst(); }),
            Btn(L"折叠右栏").on<&Button::Clicked>([splitRef, splitState]() { splitRef->CollapseSecond(); }),
            Btn(L"展开").on<&Button::Clicked>([splitRef, splitState]() { splitRef->Expand(); }),
            Btn(L"恢复 50%").on<&Button::Clicked>([splitRef, splitState]() { splitRef->Expand(); splitRef->SetSplitRatio(0.5f); splitState->SetText(L"状态：已恢复为 50% / 50%"); })
        ).spacing(8.0f).shared());
        col->AddChild(splitState);
        splitRef->CollapsedChanged.connect([splitState](int side) {
            if (side == 1) splitState->SetText(L"状态：左栏已折叠（点左侧小按钮展开）");
            else if (side == 2) splitState->SetText(L"状态：右栏已折叠（点右侧小按钮展开）");
            else splitState->SetText(L"状态：已展开到最小状态（min = 120px）"); });

        SetScrollablePage(pageNewB, col.shared());
    }

    // ---------- 多行文本 TextEdit 演示（DSL 重排）----------
    auto pageText = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;


        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));
        col->AddChild(Lbl(L"多行文本 TextEdit（行号 / 换行 / Tab / 跨行选择 / 只读富文本）").shared());

        auto codeRef = Edit()
            .with(&TextEdit::SetShowLineNumbers, false)          // 默认禁用行号（可按需开）
            .with(&TextEdit::SetWrapMode, TextEdit::WrapMode::NoWrap)
            .with(&TextEdit::SetTabSize, 4)
            .with(&TextEdit::SetBaseFont, [] { FontSpec f; f.familyName = L"Consolas"; f.size = 14.0f; return f; }())
            .height(240.0f).fillWidth(true);
        codeRef->SetPlainText(
            L"// ZufyUI TextEdit 演示（点这里编辑）\n"
            L"#include <vector>\n"
            L"int main() {\n"
            L"\tstd::vector<int> v{1, 2, 3};\n"
            L"\tfor (auto x : v) {\n"
            L"\t\tprintf(\"%d\\n\", x);\n"
            L"\t}\n"
            L"\treturn 0;\n"
            L"}\n");
        auto code = codeRef.shared();

        auto row = Row(
            Btn(L"切换只读").on<&Button::Clicked>([code]() { code->SetReadOnly(!code->IsReadOnly()); }),
            Btn(L"自动换行 开/关").on<&Button::Clicked>([code]() {
                code->SetWrapMode(code->GetWrapMode() == TextEdit::WrapMode::NoWrap ? TextEdit::WrapMode::WidgetWidth : TextEdit::WrapMode::NoWrap); }),
            Btn(L"追加一行").on<&Button::Clicked>([code]() { code->AppendPlainText(L"// 追加行"); }),
            Btn(L"查找 int").on<&Button::Clicked>([code]() { code->Find(L"int"); }),
            Btn(L"跳到 5 行 3 列").on<&Button::Clicked>([code]() { code->SetCursorLineColumn(4, 2); }),
            Btn(L"行号开关").on<&Button::Clicked>([code]() { code->SetShowLineNumbers(!code->IsShowLineNumbers()); })
        ).spacing(8.0f).height(32.0f);
        col->AddChild(code);
        col->AddChild(row.shared());

        // 只读富文本（颜色取主题角色，替代原硬编码蓝/灰/红）
        auto richRef = Edit().height(120.0f).fillWidth(true);
        {
            std::vector<std::vector<TextRun>> lines;
            TextRun h; h.text = L"富文本标题";
            h.font = [] { FontSpec f; f.familyName = L"Segoe UI"; f.size = 20.0f; f.weight = DWRITE_FONT_WEIGHT_BOLD; return f; }();
            h.color = ThemeColor(ThemeRole::Accent);
            lines.push_back({ h });
            TextRun a; a.text = L"普通  "; a.font = [] { FontSpec f; f.size = 14.0f; return f; }(); a.color = ThemeColor(ThemeRole::Text);
            TextRun b; b.text = L"红色"; b.font = [] { FontSpec f; f.size = 14.0f; return f; }(); b.color = ThemeColor(ThemeRole::Danger);
            TextRun c; c.text = L"  下划线"; c.font = [] { FontSpec f; f.size = 14.0f; return f; }();
            c.color = ThemeColor(ThemeRole::Text); c.underline = true;
            lines.push_back({ a, b, c });
            richRef.apply([&lines](TextEdit& t) { t.SetRichText(lines); });
        }
        col->AddChild(richRef.shared());

        pageText->SetLayout(Scroll().fill(true)
                                    .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                    .with(&ScrollViewer::SetContent, col.shared()).shared());
    }

    mainHost->AddPage(page1);
    mainHost->AddPage(page2);
    mainHost->AddPage(page3);
    mainHost->AddPage(page4);
    mainHost->AddPage(page5);
    mainHost->AddPage(page6);
    mainHost->AddPage(page7);
    mainHost->AddPage(page8);
    mainHost->AddPage(page9);
    mainHost->AddPage(page10);
    // ---------- 条形图 BarChart 演示（DSL 重排）----------
    auto pageChart = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto showVal = std::make_shared<bool>(false), showGrid = std::make_shared<bool>(true);

        auto chartRef = Bar().fillWidth(true).vWeight(1.0f);   // 填满=视口；内容超出由图表内部滚动条滚动
        const wchar_t* months[] = { L"一月", L"二月", L"三月", L"四月", L"五月", L"六月",
                                    L"七月", L"八月", L"九月", L"十月", L"十一月", L"十二月" };
        for (auto m : months) chartRef.category(m);            // 12 个月 → 测横向滚动
        chartRef.series(L"收入", { 120, 200, 150, 40, 170, 90, 210, 130, 60, 180, 140, 100 })
                .series(L"支出", { 60, 90, 120, 70, 110, 50, 160, 80, 40, 120, 90, 60 })
                .series(L"利润", { 60, 110, 30, -30, 60, 40, 50, 50, 20, 60, 50, 40 });
        auto chart = chartRef.shared();

        auto row = Row(
            Btn(L"分组").on<&Button::Clicked>([chart]() { chart->SetStackMode(BarChart::StackMode::Grouped); }),
            Btn(L"堆叠").on<&Button::Clicked>([chart]() { chart->SetStackMode(BarChart::StackMode::Stacked); }),
            Btn(L"百分比堆叠").on<&Button::Clicked>([chart]() { chart->SetStackMode(BarChart::StackMode::PercentStacked); }),
            Btn(L"重叠").on<&Button::Clicked>([chart]() { chart->SetStackMode(BarChart::StackMode::Overlapped); }),
            Btn(L"数值开关").on<&Button::Clicked>([chart, showVal]() { *showVal = !*showVal; chart->SetShowValues(*showVal); }),
            Btn(L"网格开关").on<&Button::Clicked>([chart, showGrid]() { *showGrid = !*showGrid; chart->SetShowGrid(*showGrid); }),
            Btn(L"描边").on<&Button::Clicked>([chart]() { chart->SetBarOutline(1.5f); }),
            Btn(L"参考线").on<&Button::Clicked>([chart]() { chart->SetReferenceLine(100.0, L"目标 100"); }),
            Btn(L"竖参考线").on<&Button::Clicked>([chart]() { chart->SetVReferenceLine(2, L"三月"); })
        ).spacing(8.0f);

        // 按钮多 → 横向滚动，别被裁
        auto rowScroll = Scroll().height(46.0f).fillWidth(true)
                                 .with(&ScrollViewer::SetHorizontalScrollEnabled, true)
                                 .with(&ScrollViewer::SetVerticalScrollEnabled, false)
                                 .with(&ScrollViewer::SetHorizontalScrollBarVisibility, ScrollViewer::ScrollBarVisibility::Always)
                                 .with(&ScrollViewer::SetContent, row.shared());

        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));
        col->AddChild(Lbl(L"条形图 BarChart（分组 / 堆叠 / 百分比堆叠 / 重叠；悬停看 tooltip）").shared());
        col->AddChild(chart);
        col->AddChild(rowScroll.shared());
        pageChart->SetLayout(Scroll().fill(true)
                                     .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                     .with(&ScrollViewer::SetContent, col.shared()).shared());
    }

    // ---------- 折线图 LineChart 演示（DSL 重排）----------
    auto pageLine = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto s1 = std::make_shared<bool>(false), s2 = std::make_shared<bool>(false), s3 = std::make_shared<bool>(true);
        auto s4 = std::make_shared<bool>(false), s5 = std::make_shared<bool>(false);

        auto lineRef = Line().fillWidth(true).vWeight(1.0f).with(&LineChart::SetShowVGrid, true);
        const wchar_t* lms[] = { L"一月", L"二月", L"三月", L"四月", L"五月", L"六月",
                                 L"七月", L"八月", L"九月", L"十月", L"十一月", L"十二月" };
        for (auto m : lms) lineRef.category(m);
        lineRef.series(L"访问", { 120, 160, 150, 190, 230, 210, 260, 240, 280, 300, 270, 320 })
               .series(L"下载", { 60, 80, 70, 100, 130, 110, 150, 140, 170, 180, 160, 200 })
               .series(L"注册", { 20, 30, 25, 40, 55, 45, 60, 58, 70, 80, 72, 95 });
        auto line = lineRef.shared();

        auto row = Row(
            Btn(L"平滑开关").on<&Button::Clicked>([line, s1]() { *s1 = !*s1; line->SetSmooth(*s1); }),
            Btn(L"虚线开关").on<&Button::Clicked>([line, s2]() { *s2 = !*s2; line->SetDashed(*s2); }),
            Btn(L"标记开关").on<&Button::Clicked>([line, s3]() { *s3 = !*s3; line->SetShowMarkers(*s3); }),
            Btn(L"数值开关").on<&Button::Clicked>([line, s4]() { *s4 = !*s4; line->SetShowValues(*s4); }),
            Btn(L"面积开关").on<&Button::Clicked>([line, s5]() { *s5 = !*s5; line->SetAreaFill(*s5); })
        ).spacing(8.0f);

        auto rowScroll = Scroll().height(46.0f).fillWidth(true)
                                 .with(&ScrollViewer::SetHorizontalScrollEnabled, true)
                                 .with(&ScrollViewer::SetVerticalScrollEnabled, false)
                                 .with(&ScrollViewer::SetHorizontalScrollBarVisibility, ScrollViewer::ScrollBarVisibility::Always)
                                 .with(&ScrollViewer::SetContent, row.shared());

        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));
        col->AddChild(Lbl(L"折线图 LineChart（多系列 / 平滑 / 虚线 / 标记 / 面积；悬停看 tooltip）").shared());
        col->AddChild(line);
        col->AddChild(rowScroll.shared());
        pageLine->SetLayout(Scroll().fill(true)
                                    .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                    .with(&ScrollViewer::SetContent, col.shared()).shared());
    }

    mainHost->AddPage(pageNewA);
    mainHost->AddPage(pageNewB);
    mainHost->AddPage(pageText);
    // ---------- 饼图 PieChart 演示（DSL 重排）----------
    // PieChart 已改为继承 ChartBase：滚轮缩放、拖拽平移、双向滚动条
    auto pagePie = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto d = std::make_shared<bool>(false), cw = std::make_shared<bool>(true), gp = std::make_shared<bool>(false);
        auto lm = std::make_shared<int>(0);

        auto pie = Pie().fillWidth(true).vWeight(1.0f)
                        .slice(L"桌面", 45).slice(L"移动", 35).slice(L"平板", 12).slice(L"其他", 8).shared();

        auto row = Row(
            Btn(L"环形开关").on<&Button::Clicked>([pie, d]() { *d = !*d; pie->SetDonut(*d ? 0.55f : 0.0f); }),
            Btn(L"标签模式").on<&Button::Clicked>([pie, lm]() {
                *lm = (*lm + 1) % 4;
                pie->SetLabelMode((PieChart::LabelMode)(*lm == 0 ? PieChart::LabelMode::LabelAndPercent
                                                   : *lm == 1 ? PieChart::LabelMode::Percent
                                                   : *lm == 2 ? PieChart::LabelMode::Value
                                                              : PieChart::LabelMode::None)); }),
            Btn(L"方向").on<&Button::Clicked>([pie, cw]() { *cw = !*cw; pie->SetClockwise(*cw); }),
            Btn(L"扇隙").on<&Button::Clicked>([pie, gp]() { *gp = !*gp; pie->SetSliceGap(*gp ? 2.0f : 0.0f); }),
            Btn(L"放大 1.6×").on<&Button::Clicked>([pie]() { pie->SetZoom(1.6f); }),   // 缩放后才能拖拽平移
            Btn(L"重置缩放").on<&Button::Clicked>([pie]() { pie->SetZoom(1.0f); })
        ).spacing(8.0f);

        auto rowScroll = Scroll().height(46.0f).fillWidth(true)
                                 .with(&ScrollViewer::SetHorizontalScrollEnabled, true)
                                 .with(&ScrollViewer::SetVerticalScrollEnabled, false)
                                 .with(&ScrollViewer::SetHorizontalScrollBarVisibility, ScrollViewer::ScrollBarVisibility::Always)
                                 .with(&ScrollViewer::SetContent, row.shared());

        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));
        col->AddChild(Lbl(L"饼图 PieChart（环形 / 标签模式 / 扇隙；放大后可用滚轮缩放 + 拖拽平移，悬停看 tooltip）").shared());
        col->AddChild(pie);
        col->AddChild(rowScroll.shared());
        pagePie->SetLayout(Scroll().fill(true)
                                   .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                   .with(&ScrollViewer::SetContent, col.shared()).shared());
    }

    mainHost->AddPage(pageChart);
    mainHost->AddPage(pageLine);
    mainHost->AddPage(pagePie);

    // ---------- 拖放（阶段 1：接收拖入）—— DSL 重排 ----------
    auto pageDragPage = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        // 拖放高亮色取主题角色（原为硬编码浅蓝灰 / 浅蓝）
        const Color kZoneBg = ThemeManager::AppTheme().Get(ThemeRole::SurfaceAlt);
        const Color kZoneHi = ThemeManager::AppTheme().Get(ThemeRole::Selection);

        auto lastImgPath = std::make_shared<std::wstring>();
        auto zonePtr = Lbl(L"拖放目标（拖到这里）").height(220.0f)
                           .background(kZoneBg)
                           .dropTarget(true)
                           .apply([](Label& l) {
                               l.SetAlignment(Label::HAlign::Center, Label::VAlign::Center);
                               l.SetBackgroundCornerRadius(10.0f); }).shared();
        auto statusPtr = Lbl(L"状态：等待拖入…").fillWidth(true).height(64.0f)
                             .apply([](Label& l) {
                                 l.SetTextOverflow(Label::TextOverflow::Wrap);   // 结果（含长路径）换行显示，不省略
                                 l.SetAlignment(Label::HAlign::Left, Label::VAlign::Top); }).shared();
        auto imgPtr = Lbl().fillWidth(true).height(220.0f).imageFit(true).shared();   // 等比缩放至完整可见，且可拖出
        imgPtr->SetDragSource([lastImgPath](DragDataBuilder& b) { if (!lastImgPath->empty()) b.AddFile(*lastImgPath); },
                              DROPEFFECT_COPY | DROPEFFECT_MOVE);

        zonePtr->Connect(zonePtr->DragEnter, [zonePtr, kZoneHi](DragEventArgs&) { zonePtr->SetBackgroundColor(kZoneHi); });
        zonePtr->Connect(zonePtr->DragLeave, [zonePtr, kZoneBg](DragEventArgs&) { zonePtr->SetBackgroundColor(kZoneBg); });
        zonePtr->Connect(zonePtr->Drop, [zonePtr, statusPtr, imgPtr, lastImgPath, kZoneBg](DragEventArgs& e) {
            if (e.data.HasFiles()) {
                auto fs = e.data.GetFiles();
                statusPtr->SetText(L"状态：收到 " + std::to_wstring(fs.size()) + L" 个文件，第一个：" + (fs.empty() ? std::wstring() : fs[0]));
                if (!fs.empty()) {   // 若是图片文件 → 加载显示
                    std::wstring ext; size_t d = fs[0].find_last_of(L'.'); if (d != std::wstring::npos) ext = fs[0].substr(d);
                    for (auto& c : ext) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
                    if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif" || ext == L".ico" || ext == L".webp") {
                        auto img = Image::FromFile(fs[0]);
                        if (img && !img->IsNull()) { imgPtr->SetImage(img); *lastImgPath = fs[0]; }   // 记住路径 → 可再拖出去
                    }
                }
            }
            else if (e.data.HasText()) {
                statusPtr->SetText(L"状态：收到文本：" + e.data.GetText());
            }
            else {
                statusPtr->SetText(L"状态：收到未知数据");
            }
            zonePtr->SetBackgroundColor(kZoneBg);
            e.effect = DROPEFFECT_COPY;
        });

        auto dragOut = Btn(L"按住并拖动：拖出文本（拖到别处 / 记事本）");
        dragOut->Connect(dragOut->Clicked, []() { ZuiInfo(L"提示", L"这是「拖出文本」拖源：按住并拖动可拖到别处或记事本"); });
        dragOut->SetDragSource([](DragDataBuilder& b) { b.AddText(L"ZufyUI 拖放示例文本"); }, DROPEFFECT_COPY | DROPEFFECT_MOVE);
        auto dragFile = Btn(L"按住并拖动：把本程序拖成文件（拖到资源管理器 / 桌面）");
        dragFile->Connect(dragFile->Clicked, []() { ZuiInfo(L"提示", L"这是「拖出文件」拖源：按住并拖到资源管理器或桌面"); });
        dragFile->SetDragSource([](DragDataBuilder& b) {
            wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
            b.AddFile(exe); b.SetPreferredEffect(DROPEFFECT_COPY);
        });

        auto col = Col().spacing(10.0f).margin(Thickness(6, 6, 6, 6));
        col->AddChild(Lbl(L"把「文件」（从资源管理器）或「文本」拖到下面的卡片上：").height(26.0f).shared());
        col->AddChild(zonePtr);
        col->AddChild(imgPtr);
        col->AddChild(statusPtr);
        col->AddChild(Txt().placeholder(L"把文本拖到这里（插入到光标处）").shared());
        col->AddChild(dragOut.shared());
        col->AddChild(dragFile.shared());
        pageDragPage->SetLayout(Scroll().fill(true).with(&ScrollViewer::SetContent, col.shared()).shared());
    }
    mainHost->AddPage(pageDragPage);

    // ---------- 设置卡片（折叠 / 展开）—— DSL 重排 ----------
    auto pageCardPage = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto list1 = Settings().fillWidth(true);
        list1->AddRow(Icon::View, L"屏幕", L"亮度、颜色、夜间模式", [] {});
        list1->AddRow(Icon::Settings, L"电源和电池", L"睡眠、节电模式、电池使用情况", [] {});
        list1->AddRow(Icon::Folder, L"安装的应用", L"卸载、默认应用、可选功能", [] {});
        auto card1 = make_expander(L"推荐设置", L"最近使用的和常用的设置")
                         .fillWidth(true).with(&Expander::SetContent, list1.shared());

        auto list2 = Settings().fillWidth(true);
        list2->AddRow(Icon::View, L"显示", L"监视器、亮度、缩放", [] {});
        list2->AddRow(Icon::Volume, L"声音", L"音量、输出、输入", [] {});
        list2->AddRow(Icon::Settings, L"通知", L"来自应用和系统的通知", [] {});
        auto card2 = make_expander(L"系统", L"显示、声音、通知、电源")
                         .fillWidth(true).with(&Expander::SetContent, list2.shared());

        // 任意内容的折叠卡片（内容本身也是 DSL 构建）
        auto content = Col().spacing(8.0f)
                            .add(Lbl(L"这是一个可放任意内容的折叠卡片："))
                            .add(Btn(L"按钮 A").on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"卡片内按钮 A"); }))
                            .add(Btn(L"按钮 B").on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"卡片内按钮 B"); }))
                            .add(Toggle(false));
        auto card3 = make_expander(L"任意内容", L"里面放按钮 / 开关等真实控件")
                         .fillWidth(true).with(&Expander::SetContent, content.shared());

        auto col = Col().spacing(12.0f).margin(Thickness(10, 10, 10, 10));
        col->AddChild(Lbl(L"点击卡片顶部（或右侧箭头）折叠 / 展开：").height(24.0f).shared());
        col->AddChild(card1.shared());
        col->AddChild(card2.shared());
        col->AddChild(card3.shared());
        pageCardPage->SetLayout(Scroll().fill(true).with(&ScrollViewer::SetContent, col.shared()).shared());
    }
    mainHost->AddPage(pageCardPage);

    // ---------- 主题测试页（DSL 重排）----------
    auto pageTheme = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;

        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));


        AddSection(col, L"主题系统：切换浅色 / 深色 / 高对比度（也可模拟系统主题变化）");
        col->AddChild(Row(
            Btn(L"浅色").on<&Button::Clicked>([]() { ThemeManager::SetLightAppTheme(); }),
            Btn(L"深色").on<&Button::Clicked>([]() { ThemeManager::SetDarkAppTheme(); }),
            Btn(L"高对比度").on<&Button::Clicked>([]() { ThemeManager::SetAppTheme(Theme::HighContrast()); }),
            Btn(L"模拟系统:深色").on<&Button::Clicked>([]() { ThemeManager::DebugForceSystemTheme(true, true, false); }),
            Btn(L"模拟系统:高对比度").on<&Button::Clicked>([]() { ThemeManager::DebugForceSystemTheme(true, false, true); }),
            Btn(L"关闭模拟").on<&Button::Clicked>([]() { ThemeManager::DebugForceSystemTheme(false, false, false); })
        ).spacing(8.0f).shared());

        AddSection(col, L"当前主题每个 ThemeRole 的取值（色卡）");
        auto swatch = std::make_shared<ThemeSwatch>();
        swatch->SetFillWidth(true);
        col->AddChild(swatch);

        AddSection(col, L"控件在主题下的实际外观");
        col->AddChild(Row(
            Txt(L"文本框").width(140.0f),
            Combo().items({ L"选项 A", L"选项 B" }).with(&ComboBox::SetSelectedIndex, 0),
            Toggle(true),
            Progress().value(0.6f).width(120.0f).height(22.0f)
        ).spacing(8.0f).height(34.0f).shared());

        col->AddChild(List().height(120.0f).fillWidth(true)
                            .items({ L"列表项 1", L"列表项 2", L"列表项 3", L"列表项 4", L"列表项 5", L"列表项 6" }).shared());

        AddSection(col, L"DSL 链式冒烟（链式构造 + 组子树 + 连信号）");
        col->AddChild(Row(
            Lbl(L"DSL 链式：").margin(Thickness(0, 0, 6, 0)),
            Btn(L"点击我").size(96.0f, 30.0f)
                .on<&Button::Clicked>([]() { ZuiInfo(L"DSL", L"DSL 按钮被点击"); }),
            Check(L"选项").margin(Thickness(12, 0, 6, 0)),
            Toggle(true)
        ).spacing(8.0f).shared());

        pageTheme->SetLayout(Scroll().fill(true)
                                     .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                     .with(&ScrollViewer::SetContent, col.shared()).shared());
    }
    mainHost->AddPage(pageTheme);

    // ---------- DSL 综合演示页（原"复杂构造测试页"）----------
    auto pageDsl = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto clicks = std::make_shared<int>(0);
        auto counter = Lbl(L"点击次数：0");

        auto makeBtn = [&](const std::wstring& t, int d) {
            return Btn(t).size(84.0f, 30.0f).on<&Button::Clicked>([clicks, counter, d]() {
                *clicks += d;
                counter->SetText(L"点击次数：" + std::to_wstring(*clicks));
            });
        };

        auto tint = [](ThemeRole r) { Color c = ThemeManager::AppTheme().Get(r); c.a = 0.16f; return c; };

        auto col = Col().spacing(10.0f).margin(Thickness(16, 16, 16, 16));


        AddSection(col, L"链式构造 + 连信号（多个按钮共用一个计数）");
        col->AddChild(
            Row(
                makeBtn(L"+1", 1), makeBtn(L"+10", 10), makeBtn(L"-1", -1),
                Check(L"复选").checked(true),
                Toggle(false).on<&ToggleSwitch::Toggled>([](bool) {})
            ).spacing(8.0f).shared());
        col.add(counter);                       // add() 的 ColumnBox 分支

        AddSection(col, L"items() 批量加项 / Grid 坐标（Cell）");
        col->AddChild(
            List().fillWidth(true).height(90.0f)
                  .items({ L"DSL 列表项 1", L"DSL 列表项 2", L"DSL 列表项 3", L"DSL 列表项 4" })
                  .shared());

        // Grid 坐标：Cell(行, 列, 子元素)；色块取主题色 + 低 alpha（原来硬编码三个固定色）
        col->AddChild(
            Grid(
                Cell(0, 0, Lbl(L"A").background(tint(ThemeRole::Accent))),
                Cell(1, 0, Lbl(L"B").background(tint(ThemeRole::Success))),
                Cell(2, 0, Lbl(L"C").background(tint(ThemeRole::Danger)))
            ).spacing(6.0f, 6.0f).shared());

        AddSection(col, L"Expander / 输入回显 / 进度");
        col->AddChild(
            make_expander(L"DSL 折叠卡片", L"内容也是 DSL 构建")
                .with(&Expander::SetExpanded, true)
                .with(&Expander::SetContent,
                      Col(Lbl(L"展开内容 1"), Btn(L"卡片内按钮").size(120.0f, 30.0f).on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"DSL 折叠卡片内的按钮"); }).shared()).shared())
                .shared());

        auto echo = Lbl(L"输入回显：");
        auto tbi = Txt(L"在此输入").width(220.0f);
        tbi.on<&TextBox::TextChanged>([echo](const std::wstring& s) { echo->SetText(L"输入回显：" + s); });
        col->AddChild(Row(tbi.shared(), echo.shared()).spacing(8.0f).shared());

        col->AddChild(Progress().value(0.65f).width(160.0f).height(22.0f).shared());

        AddSection(col, L"图表也能直接嵌进布局（Pie / Bar）");
        col->AddChild(
            Row(
                Pie().size(220.0f, 170.0f).slice(L"A", 3).slice(L"B", 5).slice(L"C", 2),
                Bar().size(360.0f, 170.0f).category(L"一").category(L"二").category(L"三")
                         .series(L"甲", { 3, 5, 2 }).series(L"乙", { 2, 4, 6 })
            ).spacing(8.0f).shared());

        pageDsl->SetLayout(Scroll().fill(true)
                                  .with(&ScrollViewer::SetContentMargin, Thickness(4, 4, 4, 4))
                                  .with(&ScrollViewer::SetContent, col.shared()).shared());
    }
    mainHost->AddPage(pageDsl);

    // ---------- 环境与组合矩阵（主题 × 背景 × 嵌套 × 尺寸边界）----------
    // 目的：把"单独看没问题、组合起来才暴露"的情况放进同一页，方便切换后一眼对比。
    auto pageEnv = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;


        auto col = Col().spacing(8.0f).margin(Thickness(14, 14, 14, 14));


        // ---- 1. 主题 × 背景：3×3 = 9 种组合，可逐个切换或循环遍历 ----
        AddSection(col, L"① 主题 × 背景 组合矩阵（3 主题 × 3 背景 = 9 种；「下一组」逐个走一遍）");
        auto comboIdx = std::make_shared<int>(0);
        auto comboLabel = Lbl(L"当前组合：1/9").shared();
        auto applyCombo = [comboIdx, comboLabel, w = &win](int i) {
            *comboIdx = ((i % 9) + 9) % 9;
            int ti = *comboIdx / 3, bi = *comboIdx % 3;
            if (ti == 0) ThemeManager::SetLightAppTheme();
            else if (ti == 1) ThemeManager::SetDarkAppTheme();
            else ThemeManager::SetAppTheme(Theme::HighContrast());
            // 不传 tint → 叠加底色跟随主题（这就是"跟随主题"语义）
            w->SetBackdrop(bi == 0 ? Backdrop::None : (bi == 1 ? Backdrop::Acrylic : Backdrop::Mica));
            static const wchar_t* tn[] = { L"浅色", L"深色", L"高对比度" };
            static const wchar_t* bn[] = { L"无背景", L"亚克力", L"云母" };
            comboLabel->SetText(std::wstring(L"当前组合：") + std::to_wstring(*comboIdx + 1) + L"/9　→　" + tn[ti] + L" + " + bn[bi]);
        };
        applyCombo(0);
        col->AddChild(Row(
            Btn(L"上一组").on<&Button::Clicked>([applyCombo, comboIdx]() { applyCombo(*comboIdx - 1); }),
            Btn(L"下一组（循环）").on<&Button::Clicked>([applyCombo, comboIdx]() { applyCombo(*comboIdx + 1); }),
            Btn(L"浅色").on<&Button::Clicked>([]() { ThemeManager::SetLightAppTheme(); }),
            Btn(L"深色").on<&Button::Clicked>([]() { ThemeManager::SetDarkAppTheme(); }),
            Btn(L"高对比度").on<&Button::Clicked>([]() { ThemeManager::SetAppTheme(Theme::HighContrast()); }),
            comboLabel
        ).spacing(8.0f).shared());
        col->AddChild(Row(
            Btn(L"背景：无").on<&Button::Clicked>([w = &win]() { w->SetBackdrop(Backdrop::None); }),
            Btn(L"背景：亚克力").on<&Button::Clicked>([w = &win]() { w->SetBackdrop(Backdrop::Acrylic); }),
            Btn(L"背景：云母").on<&Button::Clicked>([w = &win]() { w->SetBackdrop(Backdrop::Mica); }),
            Lbl(L"（都不传 tint → 底色跟随主题；显式 tint 才锁定）")
        ).spacing(8.0f).shared());

        // ---- 2. DPI / 尺寸边界：布局在不同尺寸与缩放下是否还成立 ----
        AddSection(col, L"② DPI / 尺寸边界（切到不同 DPI 的显示器后点「查询」，或直接改窗口尺寸看布局）");
        auto dpiLabel = Lbl(L"（点「查询」读取当前 DPI / 客户区尺寸）").shared();
        auto queryDpi = [dpiLabel, w = &win]() {
            HWND h = w->GetHwnd();
            int dpi = h ? (int)GetDpiForWindow(h) : 96;
            RECT cr{}; if (h) GetClientRect(h, &cr);
            wchar_t buf[200];
            swprintf(buf, 200, L"DPI=%d　缩放=%.0f%%　客户区=%d×%d",
                     dpi, dpi * 100.0 / 96.0, (int)(cr.right - cr.left), (int)(cr.bottom - cr.top));
            dpiLabel->SetText(buf);
        };
        col->AddChild(Row(
            Btn(L"查询").on<&Button::Clicked>(queryDpi),
            Btn(L"800×600（最小）").on<&Button::Clicked>([w = &win]() { w->SetSize(800, 600); }),
            Btn(L"1024×768").on<&Button::Clicked>([w = &win]() { w->SetSize(1024, 768); }),
            Btn(L"1280×720").on<&Button::Clicked>([w = &win]() { w->SetSize(1280, 720); }),
            dpiLabel
        ).spacing(8.0f).shared());

        // ---- 3. 窗口周边组合：自定义标题栏 × 标题栏可见 × 可调整大小 ----
        AddSection(col, L"③ 窗口周边组合（自定义标题栏 / 标题栏可见 / 可调整大小 任意组合）");
        col->AddChild(Row(
            Lbl(L"自定义标题栏").width(120.0f),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win, titleBar](bool on) {
                if (on) w->SetCustomTitleBar(titleBar); else w->SetCustomTitleBar(nullptr); }),
            Lbl(L"标题栏可见").width(100.0f).margin(Thickness(12, 0, 0, 0)),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win](bool on) { w->SetTitleBarVisible(on); }),
            Lbl(L"可调整大小").width(100.0f).margin(Thickness(12, 0, 0, 0)),
            Toggle(true).on<&ToggleSwitch::Toggled>([w = &win](bool on) { w->SetResizable(on); })
        ).spacing(8.0f).shared());

        // ---- 4. 深层嵌套：TabView → PageHost → Col/Row；TabView → Expander → ListView；TabView → TableView ----
        AddSection(col, L"④ 深层嵌套组合（页签里套页中页 / 套折叠卡片套列表 / 套表格）");
        auto deep = Tabs().height(210.0f).fillWidth(true);
        auto host2 = Pages().height(120.0f);
        for (int i = 1; i <= 2; ++i) {
            auto pg = make_page();
            pg->SetLayout(Col().spacing(6.0f).margin(Thickness(8, 8, 8, 8))
                .add(Lbl(L"页中页 " + std::to_wstring(i) + L"：再套一层 PageHost"))
                .add(Row(Btn(L"按钮").on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"页中页里的按钮"); }), Check(L"勾选"), Toggle(true), Progress().value(0.3f * i).width(120.0f)).spacing(10.0f).shared())
                .shared());
            host2->AddPage(pg.shared());
        }
        deep->AddTab(Lbl(L"页中页").shared(), host2.shared(), false);
        deep->AddTab(Lbl(L"卡片套列表").shared(),
            make_expander(L"卡片里的列表", L"Expander → ListView")
                .with(&Expander::SetExpanded, true)
                .with(&Expander::SetContent,
                      List().height(90.0f).fillWidth(true).items({ L"嵌套甲", L"嵌套乙", L"嵌套丙" }).shared())
                .shared(), false);
        auto tv3 = Table().rows(4).cols(2).header(0, L"列一").header(1, L"列二").height(130.0f).fillWidth(true);
        for (int r = 0; r < 4; ++r) tv3.cell(r, 0, L"行" + std::to_wstring(r + 1)).cell(r, 1, L"值" + std::to_wstring(r * 7));
        deep->AddTab(Lbl(L"表格").shared(), tv3.shared(), false);
        col->AddChild(deep.shared());

        // ---- 5. 控件墙：切主题后一屏目检所有控件 ----
        AddSection(col, L"⑤ 控件墙（切主题后一屏目检：文字 / 边框 / 悬停 / 选中 / 禁用 是否都跟着变）");
        col->AddChild(Grid(
            Cell(0, 0, Lbl(L"标签")), Cell(0, 1, Btn(L"按钮").on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"控件墙里的按钮"); })), Cell(0, 2, Txt(L"文本框").width(120.0f)),
            Cell(0, 3, Combo().items({ L"下拉 A", L"下拉 B" })),
            Cell(1, 0, Check(L"复选").checked(true)), Cell(1, 1, Radio(L"单选")),
            Cell(1, 2, Toggle(true)),
            Cell(1, 3, make_slider().width(120.0f).with(&::ZufyUI::Slider::SetRange, 0.0f, 1.0f).value(0.4f)),
            Cell(2, 0, Progress().value(0.6f).width(120.0f)), Cell(2, 1, Ring().value(0.7f)),
            Cell(2, 2, Btn(L"禁用按钮").enabled(false)), Cell(2, 3, Txt(L"只读").with(&TextBox::SetReadOnly, true).width(120.0f)),
            Cell(3, 0, make_card().padding(8.0f).with(&::ZufyUI::Card::SetShadow, true)
                           .apply([](::ZufyUI::Card& c) {
                               if (auto g = c.GetLayoutAs<GridLayout>()) g->AddChild(std::make_shared<Label>(L"带阴影卡片"), 0, 0); })),
            Cell(3, 1, List().height(70.0f).items({ L"列表 1", L"列表 2" })),
            Cell(3, 2, Tree().size(150.0f, 70.0f).roots({ L"根 1", L"根 2" })),
            Cell(3, 3, Pie().size(150.0f, 70.0f).slice(L"A", 3).slice(L"B", 2))
        ).spacing(8.0f, 8.0f).shared());

        // ---- 6. 边界 / 异常态 ----
        AddSection(col, L"⑥ 边界与异常态（超长文本省略 / 空列表 / 极值进度 / 极窄容器 / 超长下拉项）");
        col->AddChild(Row(
            Lbl(L"超长文本应显示省略号而不是撑破容器：这一行故意写得非常非常非常非常非常非常非常长").width(240.0f)
                .with(&Label::SetTextOverflow, Label::TextOverflow::Ellipsis),
            List().width(120.0f).height(60.0f),                       // 空列表
            Progress().value(0.0f).width(80.0f),
            Progress().value(1.0f).width(80.0f),
            Col().width(56.0f).spacing(4.0f)
                 .add(Btn(L"窄").on<&Button::Clicked>([]() { ZuiInfo(L"提示", L"极窄容器里的按钮"); })).add(Check(L"窄")).add(Toggle(false)).shared()   // 极窄容器
        ).spacing(10.0f).shared());
        col->AddChild(Combo().width(240.0f).items({
            L"这一项特别长，用来测试下拉列表里的省略与横向裁剪——第一项",
            L"第二项", L"第三项", L"第四项", L"第五项", L"第六项", L"第七项", L"第八项", L"第九项", L"第十项" }).shared());

        SetScrollablePage(pageEnv, col.shared());
    }
    mainHost->AddPage(pageEnv);

    // ---------- 页面：原生 HWND 子控件（HwndView）—— 全色图 + 复杂布局测试 ----------
    auto pageHwnd = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto views = std::make_shared<std::vector<std::shared_ptr<ZufyUI::HwndView>>>();

        auto mkView = [&win, views](int seed) {
            auto v = std::make_shared<ZufyUI::HwndView>(win.GetHwnd());
            v->SetFillWidth(true);
            v->SetFillHeight(true);
            if (HWND w = CreateNativePaintWindow(win.GetHwnd(), seed)) v->SetContentHwnd(w, true);
            views->push_back(v);
            return v;
        };

        auto col = Col().spacing(10.0f).margin(Thickness(12, 12, 12, 12));
        auto tip = Lbl(L"原生 HWND 子控件（HwndView）复杂布局测试：GDI 实时画「全色条 + 彩虹渐变 + 移动扫描线」。\n放入 Card / Grid / 嵌套 Column / ScrollViewer；注意 airspace：HWND 盖在 ZufyUI 内容之上、不随 ScrollViewer 裁剪。");
        tip->SetTextOverflow(Label::TextOverflow::Wrap);
        col->AddChild(tip.shared());

        // 左：Card 内 2 列 Grid（A 无 margin / B 带 margin）
        auto card = make_card().padding(10.0f).with(&ZufyUI::Card::SetShadow, true);
        if (auto g = card->GetLayoutAs<GridLayout>()) {
            g->SetSpacing(8.0f, 8.0f);
            g->SetColumnStretch(0, 1.0f); g->SetColumnStretch(1, 1.0f); g->SetRowStretch(1, 1.0f);
            g->AddChild(Lbl(L"Card / Grid：A（左） + B（右，带 margin）").shared(), 0, 0, 1, 2);
            auto a = mkView(0);
            auto b = mkView(1); b->SetMargin(Thickness(10, 10, 10, 10));
            g->AddChild(a, 1, 0, 1, 1);
            g->AddChild(b, 1, 1, 1, 1);
        }
        card->SetHorizontalStretchWeight(1.0f);
        card->SetVerticalStretchWeight(1.0f);

        // 右：嵌套 Column + 固定高 HWND + ScrollViewer（里面再放一个 HWND，滚出去看裁剪/位置）
        auto right = std::make_shared<ColumnBox>();
        right->SetSpacing(8.0f);
        right->SetHorizontalStretchWeight(1.0f);
        right->SetVerticalStretchWeight(1.0f);
        right->AddChild(std::make_shared<Label>(L"Column + ScrollViewer（滚出去观察裁剪/位置）"));
        auto c = mkView(2); c->SetHeight(110.0f); right->AddChild(c);
        auto sv = std::make_shared<ScrollViewer>();
        sv->SetFillWidth(true);
        sv->SetVerticalStretchWeight(1.0f);
        auto tall = std::make_shared<ColumnBox>();
        tall->SetSpacing(6.0f);
        for (int i = 0; i < 12; ++i) tall->AddChild(std::make_shared<Label>(L"滚动占位行 " + std::to_wstring(i + 1)));
        auto d = mkView(3); d->SetHeight(150.0f); tall->AddChild(d);
        for (int i = 0; i < 12; ++i) tall->AddChild(std::make_shared<Label>(L"滚动占位行 " + std::to_wstring(i + 13)));
        sv->SetContent(tall);
        right->AddChild(sv);

        auto mainRow = Row(card.shared(), right).spacing(12.0f);
        mainRow->SetVerticalStretchWeight(1.0f);
        col->AddChild(mainRow.shared());

        // 进程外：启动独立进程里的窗口（--outproc 生产者），由 HwndView 定位/托管
        col->AddChild(Lbl(L"进程外：独立进程的窗口（--outproc 生产者），由 HwndView 定位").shared());
        auto outView = std::make_shared<ZufyUI::HwndView>(win.GetHwnd());
        outView->SetFillWidth(true);
        outView->SetHeight(150.0f);
        {
            wchar_t exe[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --outproc";
            STARTUPINFOW si{}; si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            if (CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                CloseHandle(pi.hThread);
                HANDLE prodProc = pi.hProcess;   // 窗口关闭时兜底结束生产者进程
                win.Closed.connect([prodProc]() { TerminateProcess(prodProc, 0); CloseHandle(prodProc); });
                HWND prod = nullptr;
                for (int i = 0; i < 100 && !prod; ++i) { prod = FindWindowW(L"ZufyUI_OutProcProducer", nullptr); if (!prod) Sleep(20); }
                if (prod) outView->SetContentHwnd(prod, true);
            }
        }
        views->push_back(outView);
        col->AddChild(outView);

        col->AddChild(Row(
            Btn(L"隐藏全部").on<&Button::Clicked>([views]() { for (auto& v : *views) v->SetVisible(false); }),
            Btn(L"显示全部").on<&Button::Clicked>([views]() { for (auto& v : *views) v->SetVisible(true); }),
            Btn(L"遮挡 SetObscured(true)").on<&Button::Clicked>([views]() { for (auto& v : *views) v->SetObscured(true); }),
            Btn(L"取消遮挡").on<&Button::Clicked>([views]() { for (auto& v : *views) v->SetObscured(false); })
        ).spacing(8.0f).shared());

        pageHwnd->SetLayout(col.shared());
    }
    mainHost->AddPage(pageHwnd);

    // 主页面导航：记录当前索引，根据相对位置设置上下方向
    auto currentMainIndex = std::make_shared<int>(0);
    navList->Connect(navList->SelectionChanged, [mainHost, currentMainIndex](int index) {
        int oldIndex = *currentMainIndex;
        if (index > oldIndex) {
            mainHost->SetTransitionDirection(PageHost::TransitionDirection::Up);
        }
        else if (index < oldIndex) {
            mainHost->SetTransitionDirection(PageHost::TransitionDirection::Down);
        }
        *currentMainIndex = index;
        mainHost->NavigateTo(index);
        });

    // 多窗口相关演示（新建窗口 / owned / 模态）已移入主窗口"多窗口"页。

    // A3：帧率上限（0 = 不限/跟随显示器刷新）。改成 60 可限制动画平均出帧率以降低 GPU/CPU。
    win.SetFrameRateLimit(0);

    win.Show();            // 显示由应用决定
    win.SetMinSize(800, 600);
    win.Run();

    return 0;
}
