// main.cpp - ZufyUI 综合自动化布局测试（使用 Connect 自动管理连接）
#include "pch.h"
#include "resource.h"
#include "ZDataViewer.h"
#include "ZufyUIWindowTool.h"
#include "ZufyUICharts.h"
#include "ZufyUIDsl.h"

using namespace ZufyUI;

namespace {
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
            static const wchar_t* names[] = { L"Text",L"TextSecondary",L"TextDisabled",L"TextOnAccent",L"Surface",L"SurfaceAlt",L"SurfaceRaised",L"Backdrop",L"Overlay",L"Control",L"ControlHover",L"ControlPressed",L"ControlDisabled",L"Accent",L"AccentHover",L"AccentPressed",L"Border",L"Divider",L"FocusRing",L"Selection",L"ScrollTrack",L"ScrollThumb",L"ScrollThumbHover",L"Danger",L"Warning",L"Success",L"Info" };
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
        MessageBoxW(nullptr, L"这是全局右键菜单", L"提示", MB_OK);
        });
    globalMenu->AddSeparator();

    auto subMenu = std::make_shared<Menu>();
    subMenu->AddItem(L"子项1", []() { MessageBoxW(nullptr, L"点击了子项1", L"提示", MB_OK); });
    subMenu->AddItem(L"子项2", []() { MessageBoxW(nullptr, L"点击了子项2", L"提示", MB_OK); });
    globalMenu->AddSubmenu(L"更多操作", subMenu);
    globalMenu->AddSeparator();
    globalMenu->AddItem(L"关于", []() {
        MessageBoxW(nullptr, L"ZufyUI 综合测试程序 v1.0", L"关于", MB_OK);
        });
    win.SetContextMenu(globalMenu);

    // 顶部菜单栏（默认直接叠在自定义标题栏上）+ 底部状态栏
    auto menuBar = std::make_shared<MenuBar>();
    {
        auto mFile = std::make_shared<Menu>();
        mFile->AddItem(L"新建", []() { MessageBoxW(nullptr, L"新建", L"文件", MB_OK); });
        mFile->AddItem(L"打开", []() { MessageBoxW(nullptr, L"打开", L"文件", MB_OK); });
        mFile->AddSeparator();
        mFile->AddItem(L"退出", [&win]() { if (win.GetHwnd()) PostMessageW(win.GetHwnd(), WM_CLOSE, 0, 0); });
        menuBar->AddMenu(L"文件(F)", mFile);
        auto mEdit = std::make_shared<Menu>();
        mEdit->AddItem(L"撤销", []() {});
        mEdit->AddItem(L"重做", []() {});
        menuBar->AddMenu(L"编辑(E)", mEdit);
        auto mView = std::make_shared<Menu>();
        mView->AddCheckItem(L"显示状态栏", true, [&win](bool on) { if (auto sb = win.GetStatusBar()) sb->SetVisible(on); });
        mView->AddCheckItem(L"跟随系统主题", true, [](bool on) { ThemeManager::SetFollowSystem(on); });
        auto darkOn = std::make_shared<bool>(false);
        mView->AddItem(L"切换深色 / 浅色", [darkOn]() { ThemeManager::SetFollowSystem(false); *darkOn = !*darkOn; if (*darkOn) ThemeManager::SetDarkAppTheme(); else ThemeManager::SetLightAppTheme(); });
        menuBar->AddMenu(L"视图(V)", mView);
        auto mHelp = std::make_shared<Menu>();
        mHelp->AddItem(L"关于", []() { MessageBoxW(nullptr, L"ZufyUI", L"关于", MB_OK); });
        menuBar->AddMenu(L"帮助(H)", mHelp);
    }
    win.SetMenuBar(menuBar);

    auto statusBar = std::make_shared<StatusBar>();
    statusBar->AddPanel(L"就绪");
    statusBar->AddPanel(L"行 1, 列 1");
    statusBar->AddPanel(Icon::Home, L"ZufyUI", true);
    win.SetStatusBar(statusBar);

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
    navList->SetSelectedIndex(0);
    mainRow->AddChild(navList);

    // 右侧页面容器
    auto mainHost = std::make_shared<PageHost>();
    mainHost->SetTransitionDirection(PageHost::TransitionDirection::Left);
    mainHost->SetFillWidth(true);
    mainHost->SetFillHeight(true);
    mainRow->AddChild(mainHost);
    mainHost->SetUseCache(false);  // PageHost 本身也不应缓存

    // ---------- 页面1：基础控件 ----------
    auto page1 = std::make_shared<Page>();
    auto page1Outer = page1->GetLayoutAs<GridLayout>();
    auto grid1 = std::make_shared<GridLayout>();
    if (page1Outer) {   // 内容较多：整页纵向滚动
        auto sv1 = std::make_shared<ScrollViewer>();
        sv1->SetContentMargin(Thickness(8, 8, 8, 8));
        sv1->SetContent(grid1);
        page1Outer->AddChild(sv1, 0, 0);
    }
    if (grid1) {
        grid1->SetSpacing(10, 10);

        auto title1 = std::make_shared<Label>(L"基础控件测试");
        title1->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid1->AddChild(title1, 0, 0, 1, 2);

        auto btn1 = std::make_shared<Button>(L"普通按钮12345678901234567890123456789012345678901234567890");
        auto btn2 = std::make_shared<Button>(L"彩色按钮");
        btn2->SetColors(Color::FromArgb(255, 180, 60, 60), Color::FromArgb(255, 200, 80, 80), Color::FromArgb(255, 160, 40, 40));
        btn1->Connect(btn1->Clicked, []() { MessageBoxW(nullptr, L"按钮1被点击", L"提示", MB_OK); });
        btn2->Connect(btn2->Clicked, []() { MessageBoxW(nullptr, L"按钮2被点击", L"提示", MB_OK); });
        grid1->AddChild(btn1, 1, 0);
        grid1->AddChild(btn2, 1, 1);

        auto toggle1 = std::make_shared<ToggleSwitch>(false);
        auto lblToggleState = std::make_shared<Label>(L"开关状态：关");
        toggle1->Connect(toggle1->Toggled, [lblToggleState](bool on) {
            lblToggleState->SetText(on ? L"开关状态：开" : L"开关状态：关");
            lblToggleState->SetTextColor(on ? Color::FromArgb(255, 0, 120, 0) : Color::FromArgb(255, 120, 0, 0));
            });
        grid1->AddChild(toggle1, 2, 0);
        grid1->AddChild(lblToggleState, 2, 1);

        auto slider1 = std::make_shared<Slider>();
        slider1->SetRange(0.0f, 1.0f);
        slider1->SetValue(0.4f);
        auto progress1 = std::make_shared<ProgressBar>();
        progress1->SetValue(0.4f);
        slider1->Connect(slider1->ValueChanged, [progress1](float val) {
            progress1->SetValue(val);
            });
        grid1->AddChild(slider1, 3, 0);
        grid1->AddChild(progress1, 3, 1);

        auto combo1 = std::make_shared<ComboBox>();
        combo1->AddItem(L"选项 A");
        combo1->AddItem(L"选项 B");
        combo1->AddItem(L"选项 C");
        auto lblCombo = std::make_shared<Label>(L"当前选择：选项 A");
        combo1->Connect(combo1->SelectionChanged, [lblCombo, c = combo1.get()](int index) {
            lblCombo->SetText(L"当前选择：" + c->GetSelectedText());
            });
        grid1->AddChild(combo1, 4, 0);
        grid1->AddChild(lblCombo, 4, 1);

        // 独立勾选框控件（含勾选动画）
        auto chk1 = std::make_shared<CheckBox>(true);
        chk1->SetSize(20.0f);
        auto chk2 = std::make_shared<CheckBox>(false);
        chk2->SetSize(20.0f);
        auto chkLabel = std::make_shared<Label>(L"勾选框：已选");
        chk1->Connect(chk1->Toggled, [chkLabel](bool on) {
            chkLabel->SetText(on ? L"勾选框：已选" : L"勾选框：未选");
            });
        grid1->AddChild(chk1, 5, 0);
        grid1->AddChild(chk2, 5, 1);
        grid1->AddChild(chkLabel, 6, 0, 1, 2);

        // 新特性：禁用态 / ToolTip / 阴影卡片 / 可切换按钮
        auto btnDisabled = std::make_shared<Button>(L"禁用按钮");
        btnDisabled->SetEnabled(false);
        btnDisabled->SetToolTip(L"该按钮已被禁用");
        auto btnToggle = std::make_shared<Button>(L"可切换按钮");
        btnToggle->SetCheckable(true);
        btnToggle->SetToolTip(L"可切换按钮：点击切换选中状态");
        btnToggle->Connect(btnToggle->Toggled, [](bool on) {});
        auto cardShadow = std::make_shared<Card>();
        cardShadow->SetShadow(true);
        cardShadow->SetShadowColor(Color::FromArgb(120, 0, 0, 0));
        cardShadow->SetShadowBlur(12.0f);
        cardShadow->SetShadowOffset(0.0f, 3.0f);
        cardShadow->SetShadowCornerRadius(8.0f);
        cardShadow->SetPadding(8.0f);
        if (auto cardGrid = cardShadow->GetLayoutAs<GridLayout>()) {
            cardGrid->AddChild(std::make_shared<Label>(L"带阴影的卡片"), 0, 0);
        }
        grid1->AddChild(btnDisabled, 7, 0);
        grid1->AddChild(btnToggle, 7, 1);
        grid1->AddChild(cardShadow, 8, 0, 1, 2);

        // 新特性：控件细节
        chk1->SetHoverBoxColor(Color::FromArgb(255, 0, 120, 215));
        chk2->SetLabel(L"选项二");
        progress1->SetShowText(true);
        slider1->SetStep(0.1f);
        slider1->SetSnapToStep(true);
        combo1->SetPlaceholder(L"请选择");
        combo1->SetMaxVisibleItems(5);
        combo1->SetItemDisabled(1, true);
        combo1->Connect(combo1->DropDownOpened, []() {});

        // 可编辑 + 输入过滤的下拉框
        auto comboEdit = std::make_shared<ComboBox>();
        comboEdit->SetItems({ L"Apple", L"Banana", L"Cherry", L"Avocado", L"Blueberry" });
        comboEdit->SetEditable(true);
        comboEdit->SetFilterEnabled(true);
        comboEdit->SetPlaceholder(L"输入过滤...");
        grid1->AddChild(comboEdit, 9, 0, 1, 2);

        // （原页面1上的「新控件」——TabView / RadioGroup / ProgressRing / SplitView——已拆到
        //   单独一页「新控件」，便于二分定位"持续更新"来源；见下方 pageNew）
    }

    // ---------- 页面2：输入与滚动 ----------
    auto page2 = std::make_shared<Page>();
    auto grid2 = page2->GetLayoutAs<GridLayout>();
    if (grid2) {
        grid2->SetSpacing(10, 10);

        auto title2 = std::make_shared<Label>(L"输入控件与滚动容器");
        title2->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid2->AddChild(title2, 0, 0, 1, 2);

        auto textBox1 = std::make_shared<TextBox>();
        textBox1->SetPlaceholder(L"输入文本...");
        auto lblText = std::make_shared<Label>(L"你输入的内容将显示在这里");
        textBox1->Connect(textBox1->TextChanged, [lblText](const std::wstring& text) {
            if (text.empty())
                lblText->SetText(L"你输入的内容将显示在这里");
            else
                lblText->SetText(L"输入：" + text);
            });
        lblText->SetWidth(100);  // 固定宽度，文本超出即省略
        lblText->SetTextOverflow(Label::TextOverflow::Ellipsis);
        grid2->AddChild(textBox1, 1, 0);
        grid2->AddChild(lblText, 1, 1);

        auto passwordBox = std::make_shared<TextBox>();
        passwordBox->SetPlaceholder(L"密码");
        passwordBox->SetPasswordMode(true);
        passwordBox->SetMaxLength(16);
        grid2->AddChild(passwordBox, 2, 0);

        auto numBox = std::make_shared<NumberBox>(5.0);
        numBox->SetRange(-100000000.0, 100000000.0);   // ±1 亿
        numBox->SetStep(100.0);                        // 每次上下调 100
        numBox->SetDefaultValue(5.0);                  // 默认值；非默认时右侧出现「清除 ×」
        numBox->SetPlaceholder(L"数字（仅数字；范围 ±1 亿；步进 100）");
        // 实时校验（每次输入就判断，不用等失焦）：空 → 错误；114514 / 1919810 → 错误；其余正常
        numBox->Connect(numBox->TextChanged, [nb = numBox.get()](const std::wstring& s) {
            double v = 0.0; bool ok = !s.empty();
            if (ok) { wchar_t* end = nullptr; v = wcstod(s.c_str(), &end); ok = (end && *end == L'\0' && end != s.c_str()); }
            nb->SetError(!ok || v == 114514.0 || v == 1919810.0);
        });
        grid2->AddChild(numBox, 2, 1);

        auto scrollView = std::make_shared<ScrollViewer>();
        scrollView->SetHeight(150);
        scrollView->SetVerticalScrollEnabled(true);
        scrollView->SetHorizontalScrollEnabled(false);

        auto scrollContent = std::make_shared<ColumnBox>();
        scrollContent->SetSpacing(6);
        for (int i = 1; i <= 30; i++) {
            auto line = std::make_shared<Label>(L"滚动条目 " + std::to_wstring(i));
            line->SetTextColor(Color::FromArgb(255, 60, 60, 60));
            scrollContent->AddChild(line);
        }
        scrollView->SetContent(scrollContent);
        grid2->AddChild(scrollView, 3, 0, 1, 2);

        // 新特性：只读 / 输入过滤 / 回车 / 密码显隐 / 占位色
        auto filterBox = std::make_shared<TextBox>();
        filterBox->SetPlaceholder(L"仅数字");
        filterBox->SetInputFilter([](wchar_t c) { return c >= L'0' && c <= L'9'; });
        filterBox->Connect(filterBox->ReturnPressed, [fb = filterBox.get()]() {
            MessageBoxW(nullptr, fb->GetText().c_str(), L"回车", MB_OK);
            });
        auto readonlyBox = std::make_shared<TextBox>();
        readonlyBox->SetText(L"只读内容");
        readonlyBox->SetReadOnly(true);
        auto revealBox = std::make_shared<TextBox>();
        revealBox->SetPlaceholder(L"密码(可显)");
        revealBox->SetPasswordMode(true);
        revealBox->SetRevealPassword(true);
        auto phColorBox = std::make_shared<TextBox>();
        phColorBox->SetPlaceholder(L"彩色占位");
        phColorBox->SetPlaceholderColor(Color::FromArgb(255, 200, 80, 80));
        grid2->AddChild(filterBox, 4, 0);
        grid2->AddChild(readonlyBox, 4, 1);
        grid2->AddChild(revealBox, 5, 0);
        grid2->AddChild(phColorBox, 5, 1);

        // ---------- 系统对话框测试：打开文件(多选) / 文件夹 / 另存为 / 颜色 ----------
        {
            auto dlgTitle = std::make_shared<Label>(L"系统对话框测试：打开(可多选) / 文件夹 / 另存为 / 颜色（新版 IFileDialog + ChooseColor）");
            dlgTitle->SetTextColor(Color::FromArgb(255, 40, 40, 40));
            grid2->AddChild(dlgTitle, 6, 0, 1, 2);

            auto dlgResult = std::make_shared<Label>(L"结果：（点下面按钮后显示在这里）");
            dlgResult->SetTextColor(Color::FromArgb(255, 0, 100, 180));
            grid2->AddChild(dlgResult, 8, 0, 1, 2);

            auto dlgRow = std::make_shared<RowBox>();
            dlgRow->SetSpacing(8.0f);
            auto setRes = [dlgResult](const std::wstring& s) { dlgResult->SetText(s); };
            auto mkBtn = [](Icon ic, const std::wstring& text) {
                auto b = std::make_shared<Button>(text);
                b->SetIcon(ic);
                return b;
            };

            auto bOpen = mkBtn(Icon::Open, L"打开文件(多选)");
            bOpen->Connect(bOpen->Clicked, [w = &win, setRes]() {
                FileDialogOptions o;
                o.title = L"选择文件（可多选）";
                o.filters = { { L"图片", { L"*.png", L"*.jpg", L"*.jpeg", L"*.bmp", L"*.gif" } },
                              { L"文本", { L"*.txt", L"*.log" } } };
                auto files = FileDialog::OpenFiles(w->GetHwnd(), o);
                if (files.empty()) { setRes(L"打开文件：已取消"); return; }
                std::wstring s = L"打开文件：共 " + std::to_wstring(files.size()) + L" 个 → " + files.front();
                if (files.size() > 1) s += L" …";
                setRes(s);
                });
            auto bFolder = mkBtn(Icon::Folder, L"选文件夹(多选)");
            bFolder->Connect(bFolder->Clicked, [w = &win, setRes]() {
                FileDialogOptions o; o.title = L"选择文件夹（可多选）"; o.multiSelect = true;
                auto folders = FileDialog::PickFolders(w->GetHwnd(), o);
                if (folders.empty()) { setRes(L"选文件夹：已取消"); return; }
                setRes(L"选文件夹：共 " + std::to_wstring(folders.size()) + L" 个 → " + folders.front());
                });
            auto bSave = mkBtn(Icon::Save, L"另存为");
            bSave->Connect(bSave->Clicked, [w = &win, setRes]() {
                FileDialogOptions o;
                o.title = L"另存为"; o.defaultFileName = L"output"; o.defaultExtension = L"txt";
                o.filters = { { L"文本", { L"*.txt" } } };
                auto path = FileDialog::SaveOne(w->GetHwnd(), o);
                setRes(path ? (L"另存为：" + *path) : L"另存为：已取消");
                });
            auto bColor = mkBtn(Icon::Edit, L"选择颜色");
            bColor->Connect(bColor->Clicked, [w = &win, setRes, dlgResult]() {
                ColorDialog::Options co;
                co.custom = { Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1) };
                auto c = ColorDialog::Pick(w->GetHwnd(), Color(0.2f, 0.6f, 1.0f, 1.0f), co);
                if (!c) { setRes(L"颜色：已取消"); return; }
                wchar_t buf[96];
                swprintf(buf, 96, L"颜色：R=%d G=%d B=%d", (int)(c->r * 255 + 0.5f), (int)(c->g * 255 + 0.5f), (int)(c->b * 255 + 0.5f));
                dlgResult->SetTextColor(*c);   // 文字直接用所选颜色，直观
                setRes(buf);
                });
            dlgRow->AddChild(bOpen);
            dlgRow->AddChild(bFolder);
            dlgRow->AddChild(bSave);
            dlgRow->AddChild(bColor);
            grid2->AddChild(dlgRow, 7, 0, 1, 2);
        }

        // 调试 / 无障碍 测试（调试默认关；只返回当前帧；库不写文件）
        {
            filterBox->SetAutomationId(L"filterBox");   // 供外部工具/自动化定位
            auto dbgTitle = std::make_shared<Label>(L"调试/无障碍测试（调试默认关；外部工具向 ZufyUI_DispatcherWindow 发 WM_COPYDATA）");
            dbgTitle->SetTextColor(Color::FromArgb(255, 40, 40, 40));
            grid2->AddChild(dbgTitle, 9, 0, 1, 2);
            auto dbgResult = std::make_shared<Label>(L"（调试关闭）");
            dbgResult->SetTextColor(Color::FromArgb(255, 0, 100, 180));
            grid2->AddChild(dbgResult, 11, 0, 1, 2);
            auto dbgRow = std::make_shared<RowBox>();
            dbgRow->SetSpacing(8.0f);
            auto bDbg = std::make_shared<Button>(L"开/关调试通道");
            bDbg->SetIcon(Icon::Settings);
            bDbg->Connect(bDbg->Clicked, [dbgResult]() {
                bool on = !ZufyUI::IsDebugEnabled();
                ZufyUI::SetDebugEnabled(on);
                dbgResult->SetText(on ? L"调试通道：已开启" : L"调试通道：已关闭");
                });
            auto bTree = std::make_shared<Button>(L"导出元素树");
            bTree->SetIcon(Icon::AllApps);
            bTree->Connect(bTree->Clicked, [w = &win, dbgResult]() {
                std::wstring t = detail::DumpWindowTree(w);
                dbgResult->SetText(L"元素树：" + std::to_wstring(t.size()) + L" 字符（已 OutputDebugString）");
                ZufyUI_DEBUG_LOG_W(t.c_str());
                });
            auto bStats = std::make_shared<Button>(L"导出帧统计");
            bStats->SetIcon(Icon::View);
            bStats->Connect(bStats->Clicked, [w = &win, dbgResult]() {
                ZufyUI::SetDebugEnabled(true);   // 采样需要开
                dbgResult->SetText(detail::DumpFrameStats(w));
                });
            auto bFocus = std::make_shared<Button>(L"自动化:聚焦 filterBox");
            bFocus->SetIcon(Icon::Edit);
            bFocus->Connect(bFocus->Clicked, [dbgResult]() {
                bool ok = detail::DebugFocusElement(L"filterBox");
                dbgResult->SetText(ok ? L"已聚焦 filterBox" : L"未找到 filterBox");
                });
            dbgRow->AddChild(bDbg);
            dbgRow->AddChild(bTree);
            dbgRow->AddChild(bStats);
            dbgRow->AddChild(bFocus);
            grid2->AddChild(dbgRow, 10, 0, 1, 2);
        }

        // 新特性：滚动条可见性 / 内边距 / 实例颜色 / 滚动事件
        scrollView->SetVerticalScrollBarVisibility(ScrollViewer::ScrollBarVisibility::Always);
        scrollView->SetContentMargin(Thickness(8, 8, 8, 8));
        scrollView->SetScrollBarColors(Color::FromArgb(40, 0, 0, 0), Color::FromArgb(120, 0, 0, 0), Color::FromArgb(200, 0, 0, 0));
        scrollView->Connect(scrollView->ScrollChanged, [](float x, float y) {});
    }

    // ---------- 页面3：嵌套与联动 ----------
    auto page3 = std::make_shared<Page>();
    auto grid3 = page3->GetLayoutAs<GridLayout>();
    if (grid3) {
        grid3->SetSpacing(10, 10);

        auto title3 = std::make_shared<Label>(L"嵌套 PageHost 与控件联动");
        title3->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid3->AddChild(title3, 0, 0, 1, 2);

        auto btnNestedA = std::make_shared<Button>(L"嵌套 A");
        auto btnNestedB = std::make_shared<Button>(L"嵌套 B");
        auto btnNestedC = std::make_shared<Button>(L"嵌套 C");
        auto nestedNav = std::make_shared<RowBox>();
        nestedNav->SetSpacing(10);
        nestedNav->AddChild(btnNestedA);
        nestedNav->AddChild(btnNestedB);
        nestedNav->AddChild(btnNestedC);
        grid3->AddChild(nestedNav, 1, 0, 1, 2);

        auto nestedHost = std::make_shared<PageHost>();
        nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Up);
        grid3->AddChild(nestedHost, 2, 0, 1, 2);

        auto nestedPageA = std::make_shared<Page>();
        auto nestedLayoutA = nestedPageA->GetLayoutAs<GridLayout>();
        if (nestedLayoutA) {
            nestedLayoutA->SetSpacing(8, 8);

            auto lblA = std::make_shared<Label>(L"嵌套 A：滑块控制进度条");
            nestedLayoutA->AddChild(lblA, 0, 0, 1, 2);

            auto sliderA = std::make_shared<Slider>();
            sliderA->SetRange(0.0f, 100.0f);
            sliderA->SetValue(50.0f);
            auto progressA = std::make_shared<ProgressBar>();
            progressA->SetValue(0.5f);
            sliderA->Connect(sliderA->ValueChanged, [progressA](float val) {
                progressA->SetValue(val / 100.0f);
                });
            nestedLayoutA->AddChild(sliderA, 1, 0);
            nestedLayoutA->AddChild(progressA, 1, 1);

            auto btnA = std::make_shared<Button>(L"重置为 50%");
            btnA->Connect(btnA->Clicked, [sliderA]() {
                sliderA->SetValue(50.0f);
                });
            nestedLayoutA->AddChild(btnA, 2, 0, 1, 2);
        }

        auto nestedPageB = std::make_shared<Page>();
        auto nestedLayoutB = nestedPageB->GetLayoutAs<GridLayout>();
        if (nestedLayoutB) {
            nestedLayoutB->SetSpacing(8, 8);

            auto lblB = std::make_shared<Label>(L"嵌套 B：下拉框控制标签");
            nestedLayoutB->AddChild(lblB, 0, 0, 1, 2);

            auto comboB = std::make_shared<ComboBox>();
            comboB->AddItem(L"红色");
            comboB->AddItem(L"绿色");
            comboB->AddItem(L"蓝色");
            auto displayLabel = std::make_shared<Label>(L"当前颜色：红色");
            displayLabel->SetTextColor(Color::FromArgb(255, 255, 0, 0));
            comboB->Connect(comboB->SelectionChanged, [displayLabel, c = comboB.get()](int index) {
                std::wstring colorName = c->GetSelectedText();
                displayLabel->SetText(L"当前颜色：" + colorName);
                if (colorName == L"红色")
                    displayLabel->SetTextColor(Color::FromArgb(255, 255, 0, 0));
                else if (colorName == L"绿色")
                    displayLabel->SetTextColor(Color::FromArgb(255, 0, 128, 0));
                else
                    displayLabel->SetTextColor(Color::FromArgb(255, 0, 0, 255));
                });
            nestedLayoutB->AddChild(comboB, 1, 0);
            nestedLayoutB->AddChild(displayLabel, 1, 1);

            auto btnB = std::make_shared<Button>(L"重置选择");
            btnB->Connect(btnB->Clicked, [comboB]() {
                comboB->SetSelectedIndex(0);
                });
            nestedLayoutB->AddChild(btnB, 2, 0, 1, 2);
        }
        // ---------- 新增：嵌套页面 C（密集交叉测试） ----------
        auto nestedPageC = std::make_shared<Page>();
        auto nestedLayoutC = nestedPageC->GetLayoutAs<GridLayout>();
        if (nestedLayoutC) {
            nestedLayoutC->SetSpacing(6, 6);

            auto lblC = std::make_shared<Label>(L"密集交叉测试：多个下拉框与控件混合");
            lblC->SetTextColor(Color::FromArgb(255, 40, 40, 40));
            nestedLayoutC->AddChild(lblC, 0, 0, 1, 4);

            // 第一行：下拉框1、按钮A、下拉框2、标签
            auto combo1 = std::make_shared<ComboBox>();
            combo1->AddItem(L"选项1-A");
            combo1->AddItem(L"选项1-B");
            combo1->AddItem(L"选项1-C");
            nestedLayoutC->AddChild(combo1, 1, 0);

            auto btnA = std::make_shared<Button>(L"按钮A");
            btnA->Connect(btnA->Clicked, []() { MessageBoxW(nullptr, L"按钮A被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnA, 1, 1);

            auto combo2 = std::make_shared<ComboBox>();
            combo2->AddItem(L"选项2-A");
            combo2->AddItem(L"选项2-B");
            combo2->AddItem(L"选项2-C");
            nestedLayoutC->AddChild(combo2, 1, 2);

            auto lblStatus1 = std::make_shared<Label>(L"状态1");
            nestedLayoutC->AddChild(lblStatus1, 1, 3);

            // 第二行：标签、下拉框3、滑块、按钮B
            auto lblStatic = std::make_shared<Label>(L"固定文本");
            nestedLayoutC->AddChild(lblStatic, 2, 0);

            auto combo3 = std::make_shared<ComboBox>();
            combo3->AddItem(L"选项3-A");
            combo3->AddItem(L"选项3-B");
            combo3->AddItem(L"选项3-C");
            nestedLayoutC->AddChild(combo3, 2, 1);

            auto sliderC = std::make_shared<Slider>();
            sliderC->SetRange(0.0f, 100.0f);
            sliderC->SetValue(50.0f);
            nestedLayoutC->AddChild(sliderC, 2, 2);

            auto btnB = std::make_shared<Button>(L"按钮B");
            btnB->Connect(btnB->Clicked, []() { MessageBoxW(nullptr, L"按钮B被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnB, 2, 3);

            // 第三行：进度条、下拉框4、按钮C、下拉框5
            auto progressC = std::make_shared<ProgressBar>();
            progressC->SetValue(0.7f);
            nestedLayoutC->AddChild(progressC, 3, 0);

            auto combo4 = std::make_shared<ComboBox>();
            combo4->AddItem(L"选项4-A");
            combo4->AddItem(L"选项4-B");
            combo4->AddItem(L"选项4-C");
            nestedLayoutC->AddChild(combo4, 3, 1);

            auto btnC = std::make_shared<Button>(L"按钮C");
            btnC->Connect(btnC->Clicked, []() { MessageBoxW(nullptr, L"按钮C被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnC, 3, 2);

            auto combo5 = std::make_shared<ComboBox>();
            combo5->AddItem(L"选项5-A");
            combo5->AddItem(L"选项5-B");
            combo5->AddItem(L"选项5-C");
            nestedLayoutC->AddChild(combo5, 3, 3);

            // 第四行：开关、标签、下拉框6、按钮D
            auto toggleC = std::make_shared<ToggleSwitch>(false);
            nestedLayoutC->AddChild(toggleC, 4, 0);

            auto lblToggle = std::make_shared<Label>(L"开关");
            nestedLayoutC->AddChild(lblToggle, 4, 1);

            auto combo6 = std::make_shared<ComboBox>();
            combo6->AddItem(L"选项6-A");
            combo6->AddItem(L"选项6-B");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            nestedLayoutC->AddChild(combo6, 4, 2);

            auto btnD = std::make_shared<Button>(L"按钮D");
            btnD->Connect(btnD->Clicked, []() { MessageBoxW(nullptr, L"按钮D被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnD, 4, 3);
        }

        nestedHost->AddPage(nestedPageA);
        nestedHost->AddPage(nestedPageB);
        nestedHost->AddPage(nestedPageC);   // 新增

        // 嵌套页面切换：根据目标索引设置左右方向
        auto navigateNested = [nestedHost](int target) {
            int current = nestedHost->GetCurrentIndex();
            if (target > current) {
                nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Left);
            }
            else if (target < current) {
                nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Right);
            }
            nestedHost->NavigateTo(target);
            };

        btnNestedA->Connect(btnNestedA->Clicked, [navigateNested]() { navigateNested(0); });
        btnNestedB->Connect(btnNestedB->Clicked, [navigateNested]() { navigateNested(1); });
        btnNestedC->Connect(btnNestedC->Clicked, [navigateNested]() { navigateNested(2); });
    }

    // ---------- 页面4：列表视图 ----------
    auto page4 = std::make_shared<Page>();
    auto grid4 = page4->GetLayoutAs<GridLayout>();
    if (grid4) {
        grid4->SetSpacing(10, 10);

        auto title4 = std::make_shared<Label>(L"列表视图 (ListView)");
        title4->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid4->AddChild(title4, 0, 0, 1, 2);

        auto listView = std::make_shared<ListView>();
        listView->SetHeight(250);
        listView->SetWidth(300);
        listView->SetCheckable(true);          // 演示：列表勾选框
        listView->SetSelectionMode(ListView::SelectionMode::Extended);
        for (int i = 1; i <= 50; i++) {
            listView->AddItem(L"列表项 " + std::to_wstring(i));
        }
        listView->SetSelectedIndex(0);

        // 新特性：单项禁用 / 单独文字色 / ToolTip / 排序指示
        listView->SetItemDisabled(2, true);
        listView->SetItemTextColor(3, Color::FromArgb(255, 200, 60, 60));
        listView->SetItemToolTip(1, L"这是第 2 项的提示");
        listView->SetSortComparator([](const std::wstring& a, const std::wstring& b) { return a < b; });
        listView->SetShowSortIndicator(true);
        // 提示：点击列表后直接键入字母可首字母定位（type-ahead）

        auto lblListInfo = std::make_shared<Label>(L"当前选中：列表项 1");
        listView->Connect(listView->SelectionChanged, [lblListInfo](int index) {
            if (index >= 0)
                lblListInfo->SetText(L"当前选中：列表项 " + std::to_wstring(index + 1));
            else
                lblListInfo->SetText(L"无选中项");
            });

        auto btnClearList = std::make_shared<Button>(L"清空列表");
        auto btnAddItem = std::make_shared<Button>(L"添加一项");
        btnClearList->Connect(btnClearList->Clicked, [listView]() { listView->Clear(); });
        btnAddItem->Connect(btnAddItem->Clicked, [listView]() {
            static int counter = 51;
            listView->AddItem(L"新增项 " + std::to_wstring(counter++));
            });

        auto listButtons = std::make_shared<GridLayout>();
        listButtons->SetSpacing(6, 6);
        int _lbCol = 0;
        auto addLB = [&listButtons, &_lbCol](const std::shared_ptr<Button>& b) { listButtons->AddChild(b, _lbCol % 4, _lbCol / 4); _lbCol++; };
        auto btnSortList = std::make_shared<Button>(L"排序");
        btnSortList->Connect(btnSortList->Clicked, [listView]() {
            static bool asc = true;
            listView->Sort(asc);
            asc = !asc;
            });
        addLB(btnAddItem);
        addLB(btnClearList);
        addLB(btnSortList);
        auto btnNoneMode = std::make_shared<Button>(L"无选择模式");
        btnNoneMode->Connect(btnNoneMode->Clicked, [listView]() { listView->SetSelectionMode(ListView::SelectionMode::None); });
        addLB(btnNoneMode);

        // 新 API 测试按钮
        auto btnBatch = std::make_shared<Button>(L"批量+50 (BeginUpdate)");
        btnBatch->Connect(btnBatch->Clicked, [listView]() {
            listView->BeginUpdate();
            for (int k = 0; k < 50; ++k) listView->AddItem(L"批量项 " + std::to_wstring(k + 1));
            listView->EndUpdate();
            });
        addLB(btnBatch);
        auto btnRich = std::make_shared<Button>(L"富项 (图标+内嵌 Label)");
        btnRich->Connect(btnRich->Clicked, [listView]() {
            auto rich = std::make_shared<Label>(L"富项 ");
            rich->SetTextColor(Color::FromArgb(255, 0, 90, 160));
            auto inner = std::make_shared<Label>(L"[内嵌]");
            inner->SetTextColor(Color::FromArgb(255, 200, 60, 60));
            rich->AddChild(inner);           // 内嵌 Label → 走 Window 合成递归画出来
            listView->AddItem(rich);
            });
        addLB(btnRich);
        auto btnVirt = std::make_shared<Button>(L"虚拟 20万");
        btnVirt->Connect(btnVirt->Clicked, [listView]() {
            listView->SetItemCount(200000);
            for (int i = 0; i < 200000; ++i)
                listView->SetItem(i, L"虚拟行 " + std::to_wstring(i) + ((i % 7 == 0) ? L" *" : L""));
            });
        addLB(btnVirt);
        auto btnHide = std::make_shared<Button>(L"隐藏偶数行");
        btnHide->Connect(btnHide->Clicked, [listView]() {
            for (int i = 0; i < listView->GetItemCount(); i += 2) listView->SetItemHidden(i, true);
            });
        addLB(btnHide);
        auto btnFilter = std::make_shared<Button>(L"筛选含7");
        btnFilter->Connect(btnFilter->Clicked, [listView]() {
            listView->SetFilter([listView](int i) { return listView->GetItemText(i).find(L"7") != std::wstring::npos; });
            });
        addLB(btnFilter);
        auto btnSortV = std::make_shared<Button>(L"按文本排序");
        btnSortV->Connect(btnSortV->Clicked, [listView]() {
            // 注意：GetItemText 每次比较走 SourceText（虚拟模式下是哈希查找）；大表建议先快照文本再比较
        listView->SetViewComparator([listView](int a, int b) { return listView->GetItemText(a) < listView->GetItemText(b); });
            listView->SortView(true);
            });
        addLB(btnSortV);
        auto btnResetV = std::make_shared<Button>(L"重置视图");
        btnResetV->Connect(btnResetV->Clicked, [listView]() {
            listView->ClearHidden(); listView->ClearFilter(); listView->ClearViewSort();
            });
        addLB(btnResetV);

        grid4->AddChild(listView, 1, 0);
        grid4->AddChild(lblListInfo, 1, 1);
        grid4->AddChild(listButtons, 2, 0, 1, 2);
        listView->SetUseCache(false);
    }

    // ---------- 页面5：表格视图 ----------
    auto page5 = std::make_shared<Page>();
    auto grid5 = page5->GetLayoutAs<GridLayout>();
    if (grid5) {
        grid5->SetSpacing(10, 10);

        auto title5 = std::make_shared<Label>(L"表格视图 (TableView)");
        title5->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid5->AddChild(title5, 0, 0, 1, 2);

        auto tableView = std::make_shared<TableView>();
        tableView->SetWidth(600);
        tableView->SetHeight(250);
        tableView->SetCheckable(true);          // 演示：表格行勾选框
        tableView->SetRowCount(20);
        tableView->SetColumnCount(4);
        tableView->SetHorizontalHeaderLabels({ L"姓名", L"年龄", L"城市", L"备注" });
        tableView->SetColumnWidth(0, 120);
        tableView->SetColumnWidth(1, 80);
        tableView->SetColumnWidth(2, 120);
        tableView->SetColumnWidth(3, 200);

        for (int row = 0; row < 20; row++) {
            tableView->SetItem(row, 0, L"用户 " + std::to_wstring(row + 1));
            tableView->SetItem(row, 1, std::to_wstring(20 + row % 30));
            tableView->SetItem(row, 2, (row % 3 == 0) ? L"北京" : (row % 3 == 1) ? L"上海" : L"广州");
            tableView->SetItem(row, 3, L"备注信息 " + std::to_wstring(row));
        }

        tableView->SetSelectionMode(TableView::SelectionMode::Row);
        tableView->SetCurrentCell(0, 0);

        // 新特性：列排序 / 排序指示 / 单元格颜色 / 单元格提示
        tableView->SetColumnComparator(1, [](const std::wstring& a, const std::wstring& b) { return a < b; });
        tableView->SetShowSortIndicator(true);
        tableView->SetCellTextColor(0, 2, Color::FromArgb(255, 0, 120, 0));
        tableView->SetCellToolTip(0, 0, L"第一行的提示");
        // 新特性：列对齐 / 每行高度 / 行禁用
        tableView->SetColumnAlignment(1, TextHAlign::Center);
        tableView->SetColumnAlignment(2, TextHAlign::Center);
        tableView->SetRowHeightAt(3, 44.0f);
        tableView->SetRowDisabled(5, true);

        auto lblTableInfo = std::make_shared<Label>(L"点击单元格查看信息");
        tableView->Connect(tableView->CellClicked, [lblTableInfo](int row, int col) {
            lblTableInfo->SetText(L"选中：行 " + std::to_wstring(row) + L", 列 " + std::to_wstring(col));
            });

        auto btnRowMode = std::make_shared<Button>(L"行选择");
        auto btnColMode = std::make_shared<Button>(L"列选择");
        auto btnCellMode = std::make_shared<Button>(L"单元格选择");
        btnRowMode->Connect(btnRowMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Row); });
        btnColMode->Connect(btnColMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Column); });
        btnCellMode->Connect(btnCellMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Cell); });

        auto modeButtons = std::make_shared<RowBox>();
        modeButtons->SetSpacing(10);
        modeButtons->AddChild(btnRowMode);
        modeButtons->AddChild(btnColMode);
        modeButtons->AddChild(btnCellMode);
        auto btnSortTable = std::make_shared<Button>(L"按年龄排序");
        btnSortTable->Connect(btnSortTable->Clicked, [tableView]() {
            static bool asc = true;
            tableView->SortByColumn(1, asc);
            asc = !asc;
            });
        modeButtons->AddChild(btnSortTable);
        auto btnHideRows = std::make_shared<Button>(L"隐藏偶数行");
        btnHideRows->Connect(btnHideRows->Clicked, [tableView]() {
            for (int i = 0; i < tableView->GetRowCount(); i += 2) tableView->SetItemHidden(i, true);
            });
        modeButtons->AddChild(btnHideRows);
        auto btnFilterRows = std::make_shared<Button>(L"筛选含1");
        btnFilterRows->Connect(btnFilterRows->Clicked, [tableView]() {
            tableView->SetFilter([tableView](int r) { return tableView->GetItemText(r, 0).find(L"1") != std::wstring::npos; });
            });
        modeButtons->AddChild(btnFilterRows);
        auto btnResetRows = std::make_shared<Button>(L"重置视图");
        btnResetRows->Connect(btnResetRows->Clicked, [tableView]() {
            tableView->ClearHidden(); tableView->ClearFilter(); tableView->ClearViewSort();
            });
        modeButtons->AddChild(btnResetRows);
        auto btnBigTable = std::make_shared<Button>(L"表格 1 万行");
        btnBigTable->Connect(btnBigTable->Clicked, [tableView]() {
            tableView->SetRowCount(10000);
            for (int r = 0; r < 10000; ++r) {
                tableView->SetItem(r, 0, L"用户 " + std::to_wstring(r + 1));
                tableView->SetItem(r, 1, std::to_wstring(18 + r % 40));
                tableView->SetItem(r, 2, (r % 3 == 0) ? L"北京" : (r % 3 == 1) ? L"上海" : L"广州");
                tableView->SetItem(r, 3, L"备注 " + std::to_wstring(r));
            }
            });
        modeButtons->AddChild(btnBigTable);
        auto btnHideCol = std::make_shared<Button>(L"隐藏备注列");
        btnHideCol->Connect(btnHideCol->Clicked, [tableView, b = btnHideCol.get()]() {
            bool vis = tableView->IsColumnVisible(3);
            tableView->SetColumnVisible(3, !vis);
            b->SetText(!vis ? L"隐藏备注列" : L"显示备注列");
            });
        modeButtons->AddChild(btnHideCol);

        grid5->AddChild(tableView, 1, 0, 1, 2);
        grid5->AddChild(lblTableInfo, 2, 0, 1, 2);
        grid5->AddChild(modeButtons, 3, 0, 1, 2);
        tableView->SetUseCache(false);
    }

    // ---------- 页面6：树形视图 ----------
    auto page6 = std::make_shared<Page>();
    auto grid6 = page6->GetLayoutAs<GridLayout>();
    if (grid6) {
        grid6->SetSpacing(10, 10);

        auto title6 = std::make_shared<Label>(L"树形视图 (TreeView)");
        title6->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid6->AddChild(title6, 0, 0, 1, 2);

        auto treeView = std::make_shared<TreeView>();
        treeView->SetWidth(500);
        treeView->SetHeight(300);
        treeView->SetColumnCount(2);
        treeView->SetHeaderLabels({ L"名称", L"类型" });
        treeView->SetColumnWidth(0, 250);
        treeView->SetColumnWidth(1, 150);

        // 构建树结构
        auto root1 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点1", L"文件夹" });
        auto child1_1 = treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-1", L"文件" });
        auto child1_2 = treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-2", L"文件夹" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-1", L"文件" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-2", L"文件" });

        auto root2 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点2", L"文件夹" });
        treeView->AddChild(root2, std::vector<std::wstring>{ L"子节点2-1", L"文件" });

        treeView->SetSelectedNode(root1);
        treeView->ExpandNode(root1, true);
        treeView->ExpandNode(child1_2, true);

        // 新特性：默认展开深度 / 节点路径 / 过滤搜索
        treeView->SetDefaultExpandDepth(2);
        child1_2->tooltip = L"这是一个可展开的文件夹节点";

        auto lblTreeInfo = std::make_shared<Label>(L"点击节点查看信息");
        treeView->Connect(treeView->SelectionChanged, [lblTreeInfo](std::shared_ptr<TreeNode> node) {
            if (node) {
                std::wstring text = (!node->columns.empty() && node->columns[0]) ? node->columns[0]->GetText() : L"";
                lblTreeInfo->SetText(L"选中节点：" + text);
            }
            else {
                lblTreeInfo->SetText(L"无选中节点");
            }
            });
        treeView->Connect(treeView->NodeClicked, [lblTreeInfo, tv = treeView.get()](std::shared_ptr<TreeNode> node) {
            if (node) {
                lblTreeInfo->SetText(L"路径：" + tv->GetNodePath(node));
            }
            });

        auto btnExpandAll = std::make_shared<Button>(L"展开全部");
        auto btnCollapseAll = std::make_shared<Button>(L"折叠全部");
        btnExpandAll->Connect(btnExpandAll->Clicked, [treeView, root1, root2]() {
            treeView->ExpandNodeRecursive(root1, true);
            treeView->ExpandNodeRecursive(root2, true);
            });
        btnCollapseAll->Connect(btnCollapseAll->Clicked, [treeView, root1, root2]() {
            treeView->ExpandNodeRecursive(root1, false);
            treeView->ExpandNodeRecursive(root2, false);
            });

        auto treeButtons = std::make_shared<RowBox>();
        treeButtons->SetSpacing(10);
        treeButtons->AddChild(btnExpandAll);
        treeButtons->AddChild(btnCollapseAll);
        auto btnSearchTree = std::make_shared<Button>(L"搜索 1-2");
        auto btnClearTree = std::make_shared<Button>(L"清除过滤");
        btnSearchTree->Connect(btnSearchTree->Clicked, [treeView]() { treeView->Search(L"1-2"); });
        btnClearTree->Connect(btnClearTree->Clicked, [treeView]() { treeView->ClearFilter(); });
        treeButtons->AddChild(btnSearchTree);
        treeButtons->AddChild(btnClearTree);

        grid6->AddChild(treeView, 1, 0, 1, 2);
        grid6->AddChild(lblTreeInfo, 2, 0, 1, 2);
        grid6->AddChild(treeButtons, 3, 0, 1, 2);
        treeView->SetUseCache(false);
    }

    // ---------- 页面7：树形视图增强 ----------
    auto page7 = std::make_shared<Page>();
    auto grid7 = page7->GetLayoutAs<GridLayout>();
    if (grid7) {
        grid7->SetSpacing(10, 10);

        auto title7 = std::make_shared<Label>(L"树形视图增强（勾选 / 多选 / 图标 / 排序）");
        title7->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid7->AddChild(title7, 0, 0, 1, 2);

        auto tree = std::make_shared<TreeView>();
        tree->SetWidth(560);
        tree->SetHeight(320);
        tree->SetColumnCount(3);
        tree->SetHeaderLabels({ L"名称", L"大小", L"类型" });
        tree->SetColumnWidth(0, 260);
        tree->SetColumnWidth(1, 100);
        tree->SetColumnWidth(2, 120);
        tree->SetSelectionMode(TreeView::SelectionMode::Extended);
        tree->SetAlternatingRowColors(true);
        tree->SetCheckable(true);
        tree->SetUseCache(false);

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

        auto info = std::make_shared<Label>(L"选中 1 个节点");
        tree->Connect(tree->SelectionChangedMulti, [info](std::vector<std::shared_ptr<TreeNode>> nodes) {
            info->SetText(L"选中 " + std::to_wstring(nodes.size()) + L" 个节点");
            });
        tree->Connect(tree->ItemCheckStateChanged, [info](std::shared_ptr<TreeNode> node, TreeNode::CheckState st) {
            info->SetText((node->columns.empty() || !node->columns[0] ? std::wstring() : node->columns[0]->GetText()) + (st == TreeNode::CheckState::Checked ? L" 已勾选" :
                (st == TreeNode::CheckState::PartiallyChecked ? L" 部分勾选" : L" 取消勾选")));
            });
        tree->Connect(tree->ItemDoubleClicked, [](std::shared_ptr<TreeNode> node) {
            MessageBoxW(nullptr, (node->columns.empty() || !node->columns[0] ? std::wstring() : node->columns[0]->GetText()).c_str(), L"双击节点", MB_OK);
            });

        grid7->AddChild(tree, 1, 0);

        auto btns = std::make_shared<ColumnBox>();
        btns->SetSpacing(8);
        auto bExpand = std::make_shared<Button>(L"展开全部");
        auto bCollapse = std::make_shared<Button>(L"折叠全部");
        auto bAdd = std::make_shared<Button>(L"添加节点");
        auto bDel = std::make_shared<Button>(L"删除选中");
        auto bSort = std::make_shared<Button>(L"按名称排序");
        auto bAll = std::make_shared<Button>(L"全选");
        auto bCheckMode = std::make_shared<Button>(L"勾选:联动");
        auto bMarquee = std::make_shared<Button>(L"框选:开");
        auto bSync = std::make_shared<Button>(L"框选同步勾选:关");
        bExpand->Connect(bExpand->Clicked, [tree]() { tree->ExpandAll(); });
        bCollapse->Connect(bCollapse->Clicked, [tree]() { tree->CollapseAll(); });
        bAdd->Connect(bAdd->Clicked, [tree]() {
            auto sel = tree->GetSelectedNode();
            if (sel) {
                auto n = tree->AddChild(sel, std::vector<std::wstring>{ L"新节点", L"-", L"新建" });
                n->icon = L"\u2022";
                tree->ExpandNode(sel, true);
            }
            else {
                auto n = tree->AddRoot(std::vector<std::wstring>{ L"新节点", L"-", L"根" });
                n->icon = L"\u25A0";
            }
            });
        bDel->Connect(bDel->Clicked, [tree]() {
            auto nodes = tree->GetSelectedNodes();
            for (auto& n : nodes) tree->RemoveNode(n);
            });
        bSort->Connect(bSort->Clicked, [tree]() {
            tree->SortChildren(nullptr, true, [](const std::shared_ptr<TreeNode>& a, const std::shared_ptr<TreeNode>& b) {
                const std::wstring ta = (a->columns.empty() || !a->columns[0]) ? L"" : a->columns[0]->GetText();
                const std::wstring tb = (b->columns.empty() || !b->columns[0]) ? L"" : b->columns[0]->GetText();
                return ta < tb;   // 按首列文本排序（原来比较 shared_ptr 是指针地址）
                });
            });
        bAll->Connect(bAll->Clicked, [tree]() { tree->SelectAll(); });
        bCheckMode->Connect(bCheckMode->Clicked, [tree, b = bCheckMode.get()]() {
            bool indep = (tree->GetCheckMode() == TreeView::CheckMode::Independent);
            tree->SetCheckMode(indep ? TreeView::CheckMode::Linked : TreeView::CheckMode::Independent);
            b->SetText(indep ? L"勾选:联动" : L"勾选:独立");
            });
        bMarquee->Connect(bMarquee->Clicked, [tree, b = bMarquee.get()]() {
            bool on = tree->IsMarqueeEnabled();
            tree->SetMarqueeEnabled(!on);
            b->SetText(!on ? L"框选:开" : L"框选:关");
            });
        bSync->Connect(bSync->Clicked, [tree, b = bSync.get()]() {
            bool on = tree->IsMarqueeCheckSync();
            tree->SetMarqueeCheckSync(!on);
            b->SetText(!on ? L"框选同步勾选:开" : L"框选同步勾选:关");
            });

        btns->AddChild(bExpand);
        btns->AddChild(bCollapse);
        btns->AddChild(bAdd);
        btns->AddChild(bDel);
        btns->AddChild(bSort);
        btns->AddChild(bAll);
        btns->AddChild(bCheckMode);
        btns->AddChild(bMarquee);
        btns->AddChild(bSync);
        grid7->AddChild(btns, 1, 1);
        grid7->AddChild(info, 2, 0, 1, 2);
    }

    // ---------- 页面8：图像 ----------
    auto page8 = std::make_shared<Page>();
    auto grid8 = page8->GetLayoutAs<GridLayout>();
    if (grid8) {
        grid8->SetSpacing(12, 12);
        auto title8 = std::make_shared<Label>(L"图像（WIC 解码 + Direct2D GPU 绘制/变换）");
        title8->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid8->AddChild(title8, 0, 0, 1, 4);

        // 内嵌 base64（1x1 PNG），演示从 base64 加载
        std::shared_ptr<Image> base = Image::FromBase64(
            "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAKIAAACqCAYAAAAuoZR2AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAHYcAAB2HAY/l8WUAAAhXSURBVHhe7d2xauNKFAbgs7fRiwRCMKRR5ycwpLC7dAvq3KW5uNxiS7GNOneB7dLFRUBPkC7NggiBeRFV9xaOk11ZnjPSzBmdHf8fqHGIxxn9njMajZ0vbdv+RwAT+6f7AMAUEERQAUEEFRBEUAFBBBUQRFABQQQVEERQAUEEFRBEUAFBBBUQRFABQQQVEERQAUEEFRBEUOGLto2x9Tqj5X330cDykprnO7roPg6TUTYi1vQoHUIiym9vEEJldAWxfiT5HOZ0e4MYaqMqiHWc4ZCQQ30UBRFl+ZzpCSLK8llTE0SU5fOmZPmmpnW2jDAiBsQtAdVrykKuQ2lrLzAdI6JTWc6pbFpq2xFHU1LefTpJpqJ5yFBwYrcnQEUQncqyR1k1Tw/00n3Q1/XlydEi9fYkKAii29Wyz9WueQ1+mii/Ov1qUm9PwvRBdCzL4692Db396j7my/Z6Um9PxuRBlC7LRIaCDxjFN7o7+XpSb0/GxEGUL8tk3sg2YORlc3xxwx3bRfdpPqXenpBpgyhelvmJ/PXl+Ofuk3p7UiYNonxZ5ibyBa0Cv/lTb0/KhEGMUJa5iXx+5fHcfVJvT850QYxQltmJfPC1stTbkzNZEGOUZXYiH3qtLPX2BE0URLeyTC8bmmUZZYOPOVUm/kQ+9fYkTRJEU313KMse3tfB7BP5nEIPGKm3J2mCIBp6erB1oL9itXCYyHuW/SOptycrfhDNE8nm8LBkEXsin3p7sqIHkZvXeCtWtM9h5Il86u0JixzEWGWZD/zLZtZzkWM55hWZ7pP8JvX2pMUNYrSyzE3kw0u9PWneQTx6Z9mO2cb6LvZ2KMvcRH4M65wr9fbkeQdRg+NdI8xEfgT7nCv19uQlEcQjTrcPh2BuNabeXgTeQTzay3biaEqXjy8VtOv5Xe7oMqHrFrNRNPX2Yoj0cVJD1XxGG66cFLtRmzKzLOs+BB763tzSvEdEJ45Xy4elFzg/UYLIrXnt/T2bOCG8CEF0XMT+WHqBcyQfRJRlcCAeRJRlcCEcxDhl+XMppyHrKlFeUtOz/DP+SLO9KcgGMXpZZu44BL+NlXp78YgGMXpZZu44BL+NlXp7EQkGMU5Z/h13xyH0ZzhSby8muSBGL8vc1qjwn+FIvb2YxIIYvSxzW6OCf4Yj9fbiEgpi/LIcfyKfentxyQRxgrIcfSKfenuRiQSx/uGyEztkWSaixYqK7mMHeUk/Q+9zSr29yCJtAwOwExkRAYZCEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBCJqF5nlGX8sa67v5mWKfshzr/JrdeULW3/9nqgvKTm+W78f2w3Fc1nLv9T2qLYUbsd+A+n0Q8nyY+IpqJ5yM73YKr5/l3t2/lERPfL/XPNKzLdn/VBP1iJB9E8Pfj/sV3Xl8NGAVPRPMtotgn+SoheNjTL5lQxZwH9YCcfxNfwf3R+5d799TrQO9/qhR6e7GcA/WAnHERDb7+6j/nK6fbG5QQYquYZ6aiG6AeOeBCDDwTFN7pj+7+mdTajIRWo2LXUtpZjV3R/5Q/Xl7YXhX7gyAbRvJFtIMjL5vgP5Q72Cs1QNV+S0wCQl9S8Py/7tIvtx2toyrzzw4JWtt9HP7BEg8hN0Me+e2zqtcsIkFPZtNSOXPq4uHven4z30SEv/yVb/6MfeKLriPXaNjcpaNduR7/wPqaa81eEvmtvI6AfeIIjIjNBz6/CdoKp6KvKzkc/uBANonWCPnQNzMpQ9ZVbmihoN0nnox9cyAWRm6APWANj1T/Y+VCxC1v+nKEfnIgFMd4E3VD1/eQEbK/Y8VeDQtAPbuSCaK1HOQUbCMwTPdiaooJ2E/Y++sGNUBC5CfotOd0UcFD/sM+JfJYU/KEfXIkF0ToQBJug1/RorUaut8GkoB9cyQQx1gS9frTfOXAYcT62RA0+HHaaoB+ciQSRm6C/bGY9f5DlOLHXzVjrHlF+e2MfcVzW3E7id5qgH9zJBNFaj8Kxt+NQjsyrNSi+7K8vHHs70/eDC4EgMhP0MXrnUty86Jq4lZHa/gQs+9IL+mEIkSBa36AjjJpLhb51doRbekE/DBE+iNzEebATpYW5EOgfPUJiRhr0wyDBg8hNnAdz2gA6zmLbs8/v42joaLvd75iRBv0wjPc2sCzLug/BGWvbtvuQk+AjIsAYCCKogCCCCggiqIAgggreQXS/zP/8yGKo4/jjjF3vn1Lr+V3rwXx21/7Z3/Puh7G8g/iJuZMgsLB6cXNL9lPwQpvZmoZ+i5p9DZC7k4B+GCNcEJk7CaNuT3EubujWfgaI6J6WQ77Tj92JwtxJQD+MEiyI9ndPmBvjxy7o7pu9fBzcL/dbqU6eiPdvymK/qKhYWXc6ox/GCRdEaz0KO4z/YbElZirzh8OJODq4jn/HjWjoh3ECBZHZ8uSwQ9jHYtsOOgk+7CMa+mGsYEG0DgQCE/SuOCeB+5Ih9MNYYYI4xQS9x2LLLzmMlpfUcN9Rg34YLUwQFys6+WfnJf2U2r/U5/C1aYFOxMdamcvXdKAfRvPeBvZ3qGmduX1XYF429BwzMFHp7YczCSJoF6Y0A3hCEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFUQBBBBQQRVEAQQQUEEVRAEEEFBBFU+B+chozQXr4A2QAAAABJRU5ErkJggg==");

        auto mk = [&](const std::wstring& caption, std::shared_ptr<Image> img) {
            auto l = std::make_shared<Label>(caption);
            if (img) { l->SetImage(img); l->SetIconSize(48, 48); }
            return l;
            };
        grid8->AddChild(mk(L"  原图（放大）", base ? base->Scaled(48, 48) : nullptr), 1, 0);
        grid8->AddChild(mk(L"  旋转 45°", base ? base->Scaled(48, 48)->Rotated(45) : nullptr), 1, 1);
        grid8->AddChild(mk(L"  水平镜像", base ? base->Scaled(48, 48)->Mirrored(true, false) : nullptr), 1, 2);

        // 嵌套 Label：外层带图标，内层是子 Label（内联横排）
        auto outer = std::make_shared<Label>(L"嵌套 Label：");
        auto inner = std::make_shared<Label>(L"我是内层 Label（红色）");
        inner->SetTextColor(Color::FromArgb(255, 200, 40, 40));
        outer->AddChild(inner);
        if (base) { outer->SetImage(base->Scaled(20, 20)); outer->SetIconSize(20, 20); }
        grid8->AddChild(outer, 2, 0, 1, 4);

        grid8->AddChild(std::make_shared<Label>(L"提示：可从文件/资源/DLL/base64 加载，支持缩放、旋转、镜像、裁剪、编码保存。"), 3, 0, 1, 4);

        // ---------- 图标系统：FontIcon ----------
        // 系统图标字体的字形当图标（Win11 = Segoe Fluent Icons / Win10 = Segoe MDL2 Assets，自动回退）。
        // Icon 枚举值即字体码点；字号决定图标大小；颜色可设；字形 layout 走 FontManager 全局缓存。
        const Color kIconText = Color::FromArgb(255, 40, 40, 40);
        const Color kIconBlue = Color::FromArgb(255, 0, 120, 212);
        const Color kIconRed = Color::FromArgb(255, 200, 60, 60);
        const Color kIconGreen = Color::FromArgb(255, 16, 124, 16);
        const Color kIconAmber = Color::FromArgb(255, 200, 150, 0);

        auto iconRow = std::make_shared<RowBox>();
        iconRow->SetSpacing(14.0f);
        iconRow->AddChild(MakeFontIcon(Icon::Home, 16, kIconText));
        iconRow->AddChild(MakeFontIcon(Icon::Search, 16, kIconBlue));
        iconRow->AddChild(MakeFontIcon(Icon::Settings, 16, kIconText));
        iconRow->AddChild(MakeFontIcon(Icon::Favorite, 16, kIconRed));
        iconRow->AddChild(MakeFontIcon(Icon::Add, 16, kIconGreen));
        iconRow->AddChild(MakeFontIcon(Icon::Delete, 16, kIconRed));
        iconRow->AddChild(MakeFontIcon(Icon::Edit, 16, kIconText));
        iconRow->AddChild(MakeFontIcon(Icon::Copy, 16, kIconText));
        iconRow->AddChild(MakeFontIcon(Icon::Info, 16, kIconBlue));
        iconRow->AddChild(MakeFontIcon(Icon::Warning, 16, kIconAmber));
        iconRow->AddChild(MakeFontIcon(Icon::Error, 16, kIconRed));
        iconRow->AddChild(MakeFontIcon(Icon::Success, 16, kIconGreen));
        iconRow->AddChild(MakeFontIcon(Icon::Person, 16, kIconText));
        iconRow->AddChild(MakeFontIcon(Icon::Folder, 16, kIconAmber));
        iconRow->AddChild(MakeFontIcon(Icon::Image, 16, kIconBlue));
        iconRow->AddChild(MakeFontIcon(Icon::ChevronRight, 16, kIconText));
        grid8->AddChild(iconRow, 4, 0, 1, 4);

        // 同一图标的不同大小（字号 = 图标大小）
        auto iconSizes = std::make_shared<RowBox>();
        iconSizes->SetSpacing(18.0f);
        for (float sz : { 14.0f, 20.0f, 28.0f, 40.0f })
            iconSizes->AddChild(MakeFontIcon(Icon::Settings, sz, kIconBlue));
        grid8->AddChild(iconSizes, 5, 0, 1, 4);

        // 图标 + 文字（FontIcon 与普通控件一样，可放进任意容器/与 Label 组合）
        auto iconWithText = std::make_shared<RowBox>();
        iconWithText->SetSpacing(6.0f);
        iconWithText->AddChild(MakeFontIcon(Icon::Folder, 18.0f, kIconAmber));
        iconWithText->AddChild(std::make_shared<Label>(L"图标 + 文字（RowBox 自由组合）"));
        grid8->AddChild(iconWithText, 6, 0, 1, 4);

        // 新：Label / Button 内置字体字形图标（不需要外部 FontIcon；凡基于 Label 的控件都通用）
        auto labelIcons = std::make_shared<RowBox>();
        labelIcons->SetSpacing(16.0f);
        { auto l = std::make_shared<Label>(L"内置图标标签"); l->SetIcon(Icon::Folder); l->SetIconColor(kIconAmber); labelIcons->AddChild(l); }
        { auto l = std::make_shared<Label>(L"邮件"); l->SetIcon(Icon::Mail); l->SetIconColor(kIconBlue); labelIcons->AddChild(l); }
        { auto l = std::make_shared<Label>(L"成功"); l->SetIcon(Icon::Success); l->SetIconColor(kIconGreen); labelIcons->AddChild(l); }
        grid8->AddChild(labelIcons, 7, 0, 1, 4);

        auto iconButtons = std::make_shared<RowBox>();
        iconButtons->SetSpacing(8.0f);
        { auto b = std::make_shared<Button>(L"新建"); b->SetIcon(Icon::Add); iconButtons->AddChild(b); }
        { auto b = std::make_shared<Button>(L"刷新"); b->SetIcon(Icon::Refresh); iconButtons->AddChild(b); }
        { auto b = std::make_shared<Button>(L"删除"); b->SetIcon(Icon::Delete); iconButtons->AddChild(b); }
        { auto b = std::make_shared<Button>(L"关闭"); b->SetIcon(Icon::Close); iconButtons->AddChild(b); }
        { auto b = std::make_shared<Button>(L""); b->SetIcon(Icon::More); b->SetWidth(40.0f); iconButtons->AddChild(b); }   // 纯图标按钮
        grid8->AddChild(iconButtons, 8, 0, 1, 4);

        grid8->AddChild(std::make_shared<Label>(
            L"图标用系统字体（Win11 Segoe Fluent Icons / Win10 Segoe MDL2 Assets）；Icon 枚举值即码点，字号决定大小。"), 9, 0, 1, 4);
    }

    // ---------- 页面9：多窗口（原独立工具窗口的内容） ----------
    auto page9 = std::make_shared<Page>();
    auto page9Outer = page9->GetLayoutAs<GridLayout>();
    auto grid9 = std::make_shared<GridLayout>();
    if (page9Outer) {   // 多窗口页内容较多：整体套一层纵向滚动
        auto sv9 = std::make_shared<ScrollViewer>();
        sv9->SetContentMargin(Thickness(8, 8, 8, 8));
        sv9->SetContent(grid9);
        page9Outer->AddChild(sv9, 0, 0);
    }
    if (grid9) {
        grid9->SetSpacing(10, 10);
        auto title9 = std::make_shared<Label>(L"多窗口 / owned 子窗口 / 模态");
        title9->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid9->AddChild(title9, 0, 0, 1, 2);

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
        grid9->AddChild(tcombo, 1, 0, 1, 2);

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
        grid9->AddChild(tbtn, 2, 0);

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
        grid9->AddChild(tchildBtn, 2, 1);

        auto tshowBtn = std::make_shared<Button>(L"显示并置顶所有子窗口");
        tshowBtn->Connect(tshowBtn->Clicked, [w = &win]() {
            for (auto* c : w->GetOwnedWindows()) { c->Show(); c->Raise(); }
            });
        grid9->AddChild(tshowBtn, 3, 0);

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
        grid9->AddChild(tmodalBtn, 3, 1);

        // 弹窗结果会显示在这里
        auto mbResult = std::make_shared<Label>(L"（弹窗结果会显示在这里）");
        grid9->AddChild(mbResult, 7, 0, 1, 2);

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
        grid9->AddChild(tmsgBtn, 5, 0, 1, 2);

        // 自定义样式①：输入框（内容直接传控件；按钮仍由 MessageBox 提供）
        auto tinputBtn = std::make_shared<Button>(L"弹窗：输入框");
        tinputBtn->Connect(tinputBtn->Clicked, [w = &win, mbResult]() {
            auto edit = std::make_shared<TextBox>();
            edit->SetPlaceholder(L"请输入名称…");
            MessageBox box(w, L"输入", edit, MessageBox::Icon::None, FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(L"输入框弹窗：你点了「" + MessageBox::ButtonLabel(box.GetResult()) +
                L"」，输入内容「" + edit->GetText() + L"」");
            });
        grid9->AddChild(tinputBtn, 8, 0);

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
        grid9->AddChild(tswitchBtn, 8, 1);

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
            auto sl = std::make_shared<Slider>(); grid->AddChild(sl, 2, 2);
            auto bB = std::make_shared<Button>(L"按钮B"); grid->AddChild(bB, 2, 3);
            MessageBox box(w, L"密集交叉测试", grid, MessageBox::Icon::None, FastButton::OK | FastButton::Cancel);
            if (mbResult) mbResult->SetText(L"密集弹窗：你点了「" + MessageBox::ButtonLabel(box.GetResult()) + L"」");
            });
        grid9->AddChild(tdenseBtn, 9, 0, 1, 2);

        // 独立右键菜单：不绑定任何窗口，ShowAtCursor 在鼠标处弹出（托盘菜单将复用这套）
        auto tmenuBtn = std::make_shared<Button>(L"独立弹出菜单 (ShowAtCursor)");
        tmenuBtn->Connect(tmenuBtn->Clicked, [mbResult]() {
            auto menu = std::make_shared<Menu>();
            static bool showIcon = true;
            menu->AddCheckItem(L"显示图标", showIcon, [mbResult](bool on) {
                showIcon = on;
                if (mbResult) mbResult->SetText(on ? L"菜单：显示图标 = 勾选" : L"菜单：显示图标 = 取消");
                });
            static int viewMode = 0;   // 0 列表 / 1 网格（单选）
            menu->AddCheckItem(L"列表视图", viewMode == 0, [mbResult](bool) { viewMode = 0; if (mbResult) mbResult->SetText(L"菜单：列表视图"); }, 0, true);
            menu->AddCheckItem(L"网格视图", viewMode == 1, [mbResult](bool) { viewMode = 1; if (mbResult) mbResult->SetText(L"菜单：网格视图"); }, 0, true);
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
            auto sub = std::make_shared<Menu>();
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
        grid9->AddChild(tmenuBtn, 10, 0, 1, 2);

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
            auto trayMenu = std::make_shared<Menu>();
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
                auto menu = std::make_shared<Menu>();
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
        grid9->AddChild(trayBtn, 11, 0);

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
        grid9->AddChild(trayToastBtn, 11, 1);

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
        grid9->AddChild(trayBadgeRow, 12, 0);

        auto progRow = std::make_shared<RowBox>();
        progRow->SetSpacing(8.0f);
        progRow->AddChild(std::make_shared<Label>(L"任务栏进度"));
        auto progSlider = std::make_shared<Slider>();
        progSlider->SetRange(0, 100);
        progSlider->SetWidth(200);
        progSlider->Connect(progSlider->ValueChanged, [mbResult, w = &win](float v) {
            int p = (int)(v + 0.5f);
            if (p <= 0) w->SetTaskbarProgress(Window::TaskbarProgress::None);
            else w->SetTaskbarProgress(Window::TaskbarProgress::Normal, (ULONGLONG)p, 100);
            if (mbResult) mbResult->SetText(L"任务栏进度：" + std::to_wstring(p) + L"%");
            });
        progRow->AddChild(progSlider);
        grid9->AddChild(progRow, 12, 1);

        auto appIconBtn = std::make_shared<Button>(L"统一设置应用图标(项目自带图标)");
        appIconBtn->Connect(appIconBtn->Clicked, [mbResult, w = &win, loadAppImage]() {
            w->SetAppIcon(loadAppImage());   // 库自带 Image，直接传
            if (mbResult) mbResult->SetText(L"已用项目自带图标统一设置（原生 + 自定义标题栏）");
            });
        grid9->AddChild(appIconBtn, 13, 0, 1, 2);

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
        grid9->AddChild(tbBadgeRow, 14, 0, 1, 2);

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
        auto tbIconProgSlider = std::make_shared<Slider>();
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
        grid9->AddChild(tbIconRow, 15, 0, 1, 2);

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
        grid9->AddChild(thumbBarBtn, 16, 0, 1, 2);

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
        grid9->AddChild(jumpListBtn, 17, 0, 1, 2);


        auto tclose = std::make_shared<Button>(L"关闭主窗口");
        tclose->Connect(tclose->Clicked, [w = &win]() { w->Close(); });
        grid9->AddChild(tclose, 4, 0, 1, 2);
    }

    // ---------- 页面10：窗口属性（运行时调用窗口 API） ----------
    auto page10 = std::make_shared<Page>();
    auto grid10 = page10->GetLayoutAs<GridLayout>();
    if (grid10) {
        grid10->SetSpacing(10, 10);
        auto title10 = std::make_shared<Label>(L"窗口属性（运行时调用 Window API）");
        title10->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid10->AddChild(title10, 0, 0, 1, 2);

        auto backdropState = std::make_shared<Backdrop>(Backdrop::Acrylic);
        auto tintState = std::make_shared<DWORD>(0x80FFFFFFu);

        // 背景模式（空 / 亚克力 / 云母）：颜色叠在这个背景之上
        auto backdropCombo = std::make_shared<ComboBox>();
        backdropCombo->AddItem(L"空（透明）");
        backdropCombo->AddItem(L"亚克力");
        backdropCombo->AddItem(L"云母");
        backdropCombo->SetSelectedIndex(1);
        backdropCombo->Connect(backdropCombo->SelectionChanged, [w = &win, backdropState, tintState](int idx) {
            if (idx < 0) return;
            *backdropState = (idx == 2) ? Backdrop::Mica : (idx == 1 ? Backdrop::Acrylic : Backdrop::None);
            w->SetBackdrop(*backdropState, *tintState);
            });
        grid10->AddChild(std::make_shared<Label>(L"背景层（亚克力 / 云母 / 无）"), 1, 0);
        grid10->AddChild(backdropCombo, 1, 1);

        // 背景色（ARGB）：A = 能透出多少背景（0 全透、255 不透），RGB = 叠加颜色
        auto colorCombo = std::make_shared<ComboBox>();
        colorCombo->AddItem(L"全透明 0x00000000（透出背景）");
        colorCombo->AddItem(L"白色不透明 0xFFFFFFFF（纯白，盖住背景）");
        colorCombo->AddItem(L"白色 50%  0x80FFFFFF（半透明白 + 背景）");
        colorCombo->AddItem(L"红色 50%  0x80FF0000（半透明红 + 背景）");
        colorCombo->AddItem(L"深灰不透明 0xFF202020");
        colorCombo->SetSelectedIndex(2);
        colorCombo->Connect(colorCombo->SelectionChanged, [w = &win, backdropState, tintState](int idx) {
            static const DWORD kColors[] = { 0x00000000u, 0xFFFFFFFFu, 0x80FFFFFFu, 0x80FF0000u, 0xFF202020u };
            if (idx < 0 || idx > 4) return;
            *tintState = kColors[idx];
            w->SetBackdrop(*backdropState, *tintState);
            });
        grid10->AddChild(std::make_shared<Label>(L"背景叠加层（带 Alpha 的颜色）"), 3, 0);
        grid10->AddChild(colorCombo, 3, 1);

        // 自定义标题栏开关
        auto customTitleSwitch = std::make_shared<ToggleSwitch>(true);
        customTitleSwitch->Connect(customTitleSwitch->Toggled, [w = &win, titleBar](bool on) {
            if (on) w->SetCustomTitleBar(titleBar);
            else w->SetCustomTitleBar(nullptr);
            });
        grid10->AddChild(std::make_shared<Label>(L"使用自定义标题栏"), 4, 0);
        grid10->AddChild(customTitleSwitch, 4, 1);

        // 标题栏可见
        auto barVisibleSwitch = std::make_shared<ToggleSwitch>(true);
        barVisibleSwitch->Connect(barVisibleSwitch->Toggled, [w = &win](bool on) { w->SetTitleBarVisible(on); });
        grid10->AddChild(std::make_shared<Label>(L"标题栏可见 SetTitleBarVisible"), 5, 0);
        grid10->AddChild(barVisibleSwitch, 5, 1);

        // 可调整大小
        auto resizableSwitch = std::make_shared<ToggleSwitch>(true);
        resizableSwitch->Connect(resizableSwitch->Toggled, [w = &win](bool on) { w->SetResizable(on); });
        grid10->AddChild(std::make_shared<Label>(L"可调整大小 SetResizable"), 6, 0);
        grid10->AddChild(resizableSwitch, 6, 1);

        // 页面切换缓动
        auto easingCombo = std::make_shared<ComboBox>();
        easingCombo->AddItem(L"Linear（匀速）"); easingCombo->AddItem(L"EaseInOut（平滑）"); easingCombo->AddItem(L"EaseOut（缓出）");
        easingCombo->SetSelectedIndex(1);
        easingCombo->Connect(easingCombo->SelectionChanged, [mainHost](int idx) {
            if (idx < 0) return;
            mainHost->SetTransitionEasing((PageHost::TransitionEasing)idx);
            });
        grid10->AddChild(std::make_shared<Label>(L"页面切换缓动 SetTransitionEasing"), 7, 0);
        grid10->AddChild(easingCombo, 7, 1);

        // ---- 参数卡片：亚克力 / 云母（仅"手动模式 + 对应背景层"时显示）----
        auto addSlider = [](std::shared_ptr<GridLayout> g, int r, const wchar_t* name, float lo, float hi, float step,
                            float init, std::function<void(float)> apply) {
            auto lbl = std::make_shared<Label>(name);
            lbl->SetTextColor(Color::FromArgb(255, 40, 40, 40));
            auto s = std::make_shared<Slider>();
            s->SetRange(lo, hi);
            s->SetStep(step);
            s->SetSnapToStep(true);
            s->SetValue(init);
            s->Connect(s->ValueChanged, [apply](float v) { apply(v); });
            g->AddChild(lbl, r, 0);
            g->AddChild(s, r, 1);
            };

        auto acrylicCard = std::make_shared<Card>();
        acrylicCard->SetShadow(true);
        acrylicCard->SetPadding(10.0f);
        if (auto g = acrylicCard->GetLayoutAs<GridLayout>()) {
            g->AddChild(std::make_shared<Label>(L"亚克力参数"), 0, 0, 1, 2);
            addSlider(g, 1, L"模糊量", 0.0f, 200.0f, 1.0f, Window::AcrylicParams.blurAmount,
                [](float v) { Window::AcrylicParams.blurAmount = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); });
            addSlider(g, 2, L"饱和度", 0.0f, 3.0f, 0.05f, Window::AcrylicParams.saturation,
                [](float v) { Window::AcrylicParams.saturation = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); });
            addSlider(g, 3, L"色调 alpha", 0.0f, 1.0f, 0.05f, Window::AcrylicParams.tint.a,
                [](float v) { Window::AcrylicParams.tint.a = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); });
            addSlider(g, 4, L"噪点", 0.0f, 0.1f, 0.005f, Window::AcrylicParams.noiseOpacity,
                [](float v) { Window::AcrylicParams.noiseOpacity = v; Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams); });
        }
        grid10->AddChild(acrylicCard, 8, 0, 1, 2);

        auto micaCard = std::make_shared<Card>();
        micaCard->SetShadow(true);
        micaCard->SetPadding(10.0f);
        if (auto g = micaCard->GetLayoutAs<GridLayout>()) {
            g->AddChild(std::make_shared<Label>(L"云母参数"), 0, 0, 1, 2);
            addSlider(g, 1, L"模糊量", 0.0f, 400.0f, 1.0f, Window::MicaParams.blurAmount,
                [](float v) { Window::MicaParams.blurAmount = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); });
            addSlider(g, 2, L"饱和度", 0.0f, 3.0f, 0.05f, Window::MicaParams.saturation,
                [](float v) { Window::MicaParams.saturation = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); });
            addSlider(g, 3, L"色调 alpha", 0.0f, 1.0f, 0.05f, Window::MicaParams.tint.a,
                [](float v) { Window::MicaParams.tint.a = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); });
            addSlider(g, 4, L"噪点", 0.0f, 0.1f, 0.005f, Window::MicaParams.noiseOpacity,
                [](float v) { Window::MicaParams.noiseOpacity = v; Window::SetBackgroundParams(Backdrop::Mica, Window::MicaParams); });
        }
        grid10->AddChild(micaCard, 9, 0, 1, 2);

        // 选亚克力显示亚克力卡；选云母显示云母卡；选无则都隐藏
        auto syncCards = [acrylicCard, micaCard, backdropCombo]() {
            int b = backdropCombo->GetSelectedIndex();
            acrylicCard->SetVisible(b == 1);
            micaCard->SetVisible(b == 2);
            };
        backdropCombo->Connect(backdropCombo->SelectionChanged, [syncCards](int) { syncCards(); });
        syncCards();
    }

    // 所有页面加入 PageHost
    // ---------- 新控件页 A：TabView + RadioGroup ----------
    auto pageNewA = std::make_shared<Page>();
    auto pageNewAOuter = pageNewA->GetLayoutAs<GridLayout>();
    auto ga = std::make_shared<GridLayout>();
    if (pageNewAOuter) {   // 和「基础控件」页一样：整页纵向滚动（否则直接网格会缩到自然宽 → 内容挤左）
        auto sv = std::make_shared<ScrollViewer>();
        sv->SetContentMargin(Thickness(8, 8, 8, 8));
        sv->SetContent(ga);
        pageNewAOuter->AddChild(sv, 0, 0);
    }
    {
        if (ga) {
            ga->SetSpacing(10, 10);
            ga->SetColumnStretch(0, 1.0f); ga->SetColumnStretch(1, 1.0f);   // 列撑满（而不是靠控件权重把控件拉变形）
            auto tabs = std::make_shared<TabView>();
            tabs->SetHeight(150.0f);
            static const Icon kTabIcons[] = { Icon::Home, Icon::Search, Icon::Settings, Icon::Folder,
                                              Icon::File, Icon::Person, Icon::Calendar, Icon::Mail,
                                              Icon::Image, Icon::Favorite, Icon::View, Icon::Info };
            for (int i = 1; i <= 12; ++i) {
                auto content = std::make_shared<Label>(L"  这里是「页签 " + std::to_wstring(i) + L"」的内容。");
                auto tabLabel = std::make_shared<Label>(L"页签 " + std::to_wstring(i));
                tabLabel->SetIcon(kTabIcons[i - 1]);
                tabs->AddTab(tabLabel, content, (i % 3 == 0));
            }
            tabs->Connect(tabs->TabCloseRequested, [tabs](int idx) { tabs->RemoveTab(idx); });
            ga->AddChild(tabs, 0, 0, 1, 2);

            auto radios = std::make_shared<RadioGroup>();
            radios->AddItem(L"单选 A（默认选中）", true);
            radios->AddItem(L"单选 B");
            radios->AddItem(L"单选 C");
            radios->AddItem(L"单选 D（禁用）");
            if (auto d = radios->GetButton(3)) d->SetEnabled(false);
            ga->AddChild(radios, 1, 0, 1, 2);

            auto radiosH = std::make_shared<RadioGroup>();
            radiosH->SetOrientation(RadioGroup::Orientation::Horizontal);
            radiosH->AddItem(L"甲", true);
            radiosH->AddItem(L"乙");
            radiosH->AddItem(L"丙");
            ga->AddChild(radiosH, 2, 0, 1, 2);
        }
    }

    // ---------- 新控件页 B：ProgressRing + SplitView ----------
    auto pageNewB = std::make_shared<Page>();
    auto pageNewBOuter = pageNewB->GetLayoutAs<GridLayout>();
    auto gb = std::make_shared<GridLayout>();
    if (pageNewBOuter) {
        auto sv = std::make_shared<ScrollViewer>();
        sv->SetContentMargin(Thickness(8, 8, 8, 8));
        sv->SetContent(gb);
        pageNewBOuter->AddChild(sv, 0, 0);
    }
    {
        if (gb) {
            gb->SetSpacing(10, 10);
            gb->SetColumnStretch(0, 1.0f); gb->SetColumnStretch(1, 1.0f);
            auto ringRow = std::make_shared<RowBox>();
            ringRow->SetSpacing(16.0f);
            auto ring1 = std::make_shared<ProgressRing>(36.0f); ring1->SetValue(0.35f);
            auto ring2 = std::make_shared<ProgressRing>(36.0f); ring2->SetValue(0.72f);
            ring2->SetColor(Color::FromArgb(255, 16, 124, 16));
            auto ring3 = std::make_shared<ProgressRing>(36.0f); ring3->SetIndeterminate(true);   // 不定态：持续旋转
            ringRow->AddChild(ring1);
            ringRow->AddChild(ring2);
            ringRow->AddChild(ring3);
            gb->AddChild(ringRow, 0, 0, 1, 2);

            auto split = std::make_shared<SplitView>();
            split->SetHeight(200.0f);
            split->SetSplitterWidth(8.0f);
            split->SetMinFirst(120.0f);
            split->SetMinSecond(120.0f);
            {
                auto leftPane = std::make_shared<Card>();
                if (auto lg = leftPane->GetLayoutAs<GridLayout>()) {
                    auto nested = std::make_shared<PageHost>();
                    auto btns = std::make_shared<RowBox>();
                    btns->SetSpacing(6.0f);
                    for (int i = 1; i <= 3; ++i) {
                        auto pg = std::make_shared<Page>();
                        if (auto g = pg->GetLayoutAs<GridLayout>()) g->AddChild(std::make_shared<Label>(L"交叉页 " + std::to_wstring(i)), 0, 0);
                        nested->AddPage(pg);
                        auto b = std::make_shared<Button>(L"页" + std::to_wstring(i));
                        b->SetWidth(44.0f);
                        b->SetHeight(26.0f);
                        b->Connect(b->Clicked, [nested, i]() { nested->NavigateTo(i - 1); });
                        btns->AddChild(b);
                    }
                    nested->SetHeight(120.0f);
                    lg->AddChild(btns, 0, 0);
                    lg->AddChild(nested, 1, 0);
                }
                auto rightPane = std::make_shared<ListView>();
                rightPane->SetButtonMode(true);
                for (int i = 1; i <= 8; ++i) rightPane->AddItem(L"列表项 " + std::to_wstring(i));
                split->SetFirst(leftPane);
                split->SetSecond(rightPane);
            }
            gb->AddChild(split, 1, 0, 1, 2);
        }
    }

    // ---------- 多行文本 TextEdit 演示 ----------
    auto pageText = std::make_shared<Page>();
    {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(10);
        col->SetMargin(Thickness(16, 16, 16, 16));
        col->AddChild(std::make_shared<Label>(L"多行文本 TextEdit（行号 / 换行 / Tab / 跨行选择 / 只读富文本）"));

        auto code = std::make_shared<TextEdit>();
        code->SetShowLineNumbers(false);   // 默认禁用行号（可按需开）
        code->SetWrapMode(TextEdit::WrapMode::NoWrap);
        code->SetTabSize(4);
        code->SetBaseFont([] { FontSpec f; f.familyName = L"Consolas"; f.size = 14.0f; return f; }());
        code->SetHeight(240); code->SetFillWidth(true);
        code->SetPlainText(
            L"// ZufyUI TextEdit 演示（点这里编辑）\n"
            L"#include <vector>\n"
            L"int main() {\n"
            L"\tstd::vector<int> v{1, 2, 3};\n"
            L"\tfor (auto x : v) {\n"
            L"\t\tprintf(\"%d\\n\", x);\n"
            L"\t}\n"
            L"\treturn 0;\n"
            L"}\n");
        col->AddChild(code);

        auto row = std::make_shared<RowBox>(); row->SetSpacing(8); row->SetHeight(32);
        auto bRo = std::make_shared<Button>(L"切换只读");
        auto bWrap = std::make_shared<Button>(L"自动换行 开/关");
        auto bAppend = std::make_shared<Button>(L"追加一行");
        auto bFind = std::make_shared<Button>(L"查找 int");
        auto bGoto = std::make_shared<Button>(L"跳到 5 行 3 列");
        auto bNum = std::make_shared<Button>(L"行号开关");
        row->AddChild(bRo); row->AddChild(bWrap); row->AddChild(bAppend); row->AddChild(bFind); row->AddChild(bGoto); row->AddChild(bNum);
        col->AddChild(row);
        bNum->Connect(bNum->Clicked, [code]() { code->SetShowLineNumbers(!code->IsShowLineNumbers()); });
        bRo->Connect(bRo->Clicked, [code]() { code->SetReadOnly(!code->IsReadOnly()); });
        bWrap->Connect(bWrap->Clicked, [code]() {
            code->SetWrapMode(code->GetWrapMode() == TextEdit::WrapMode::NoWrap ? TextEdit::WrapMode::WidgetWidth : TextEdit::WrapMode::NoWrap);
            });
        bAppend->Connect(bAppend->Clicked, [code]() { code->AppendPlainText(L"// 追加行"); });
        bFind->Connect(bFind->Clicked, [code]() { code->Find(L"int"); });
        bGoto->Connect(bGoto->Clicked, [code]() { code->SetCursorLineColumn(4, 2); });

        // 只读富文本
        auto rich = std::make_shared<TextEdit>();
        rich->SetHeight(120); rich->SetFillWidth(true);
        {
            std::vector<std::vector<TextRun>> lines;
            TextRun h; h.text = L"富文本标题"; h.font = [] { FontSpec f; f.familyName = L"Segoe UI"; f.size = 20.0f; f.weight = DWRITE_FONT_WEIGHT_BOLD; return f; }(); h.color = Color::FromArgb(255, 0, 102, 204);
            lines.push_back({ h });
            TextRun a; a.text = L"普通  "; a.font = [] { FontSpec f; f.size = 14.0f; return f; }(); a.color = Color::FromArgb(255, 40, 40, 40);
            TextRun b; b.text = L"红色"; b.font = [] { FontSpec f; f.size = 14.0f; return f; }(); b.color = Color::FromArgb(255, 200, 40, 40);
            TextRun c; c.text = L"  下划线"; c.font = [] { FontSpec f; f.size = 14.0f; return f; }(); c.color = Color::FromArgb(255, 40, 40, 40); c.underline = true;
            lines.push_back({ a, b, c });
            rich->SetRichText(lines);
        }
        col->AddChild(rich);

        auto sv = std::make_shared<ScrollViewer>();
        sv->SetFillWidth(true); sv->SetFillHeight(true);
        sv->SetContentMargin(Thickness(4, 4, 4, 4));
        sv->SetContent(col);
        pageText->SetLayout(sv);
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
    // ---------- 条形图 BarChart 演示 ----------
    auto pageChart = std::make_shared<Page>();
    {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(10); col->SetMargin(Thickness(16, 16, 16, 16));
        col->AddChild(std::make_shared<Label>(L"条形图 BarChart（分组 / 堆叠 / 百分比堆叠；悬停看 tooltip）"));
        auto chart = std::make_shared<BarChart>();
        chart->SetFillWidth(true); chart->SetVerticalStretchWeight(1.0f);   // 填满=视口；内容超出由图表内部滚动条滚动
        const wchar_t* months[] = { L"一月", L"二月", L"三月", L"四月", L"五月", L"六月", L"七月", L"八月", L"九月", L"十月", L"十一月", L"十二月" };
        for (auto m : months) chart->AddCategory(m);   // 12 个月 → 测横向滚动
        chart->AddSeries(L"收入", { 120, 200, 150, 40, 170, 90, 210, 130, 60, 180, 140, 100 });
        chart->AddSeries(L"支出", { 60, 90, 120, 70, 110, 50, 160, 80, 40, 120, 90, 60 });
        chart->AddSeries(L"利润", { 60, 110, 30, -30, 60, 40, 50, 50, 20, 60, 50, 40 });
        col->AddChild(chart);
        auto row = std::make_shared<RowBox>(); row->SetSpacing(8); row->SetHeight(32);
        auto bG = std::make_shared<Button>(L"分组"); auto bS = std::make_shared<Button>(L"堆叠"); auto bP = std::make_shared<Button>(L"百分比堆叠");
        auto bV = std::make_shared<Button>(L"数值开关"); auto bGrid = std::make_shared<Button>(L"网格开关");
        auto bO = std::make_shared<Button>(L"重叠"); auto bOut = std::make_shared<Button>(L"描边"); auto bRef = std::make_shared<Button>(L"参考线"); auto bVRef = std::make_shared<Button>(L"竖参考线");
        row->AddChild(bG); row->AddChild(bS); row->AddChild(bP); row->AddChild(bO); row->AddChild(bV); row->AddChild(bGrid); row->AddChild(bOut); row->AddChild(bRef); row->AddChild(bVRef);
        auto rowScroll = std::make_shared<ScrollViewer>();   // 按钮多 → 横向滚动，别被裁
        rowScroll->SetHeight(46); rowScroll->SetFillWidth(true);
        rowScroll->SetHorizontalScrollEnabled(true); rowScroll->SetVerticalScrollEnabled(false);
        rowScroll->SetHorizontalScrollBarVisibility(ScrollViewer::ScrollBarVisibility::Always);
        rowScroll->SetContent(row);
        col->AddChild(rowScroll);
        auto showVal = std::make_shared<bool>(false), showGrid = std::make_shared<bool>(true);
        bG->Connect(bG->Clicked, [chart]() { chart->SetStackMode(BarChart::StackMode::Grouped); });
        bS->Connect(bS->Clicked, [chart]() { chart->SetStackMode(BarChart::StackMode::Stacked); });
        bP->Connect(bP->Clicked, [chart]() { chart->SetStackMode(BarChart::StackMode::PercentStacked); });
        bV->Connect(bV->Clicked, [chart, showVal]() { *showVal = !*showVal; chart->SetShowValues(*showVal); });
        bGrid->Connect(bGrid->Clicked, [chart, showGrid]() { *showGrid = !*showGrid; chart->SetShowGrid(*showGrid); });
        bO->Connect(bO->Clicked, [chart]() { chart->SetStackMode(BarChart::StackMode::Overlapped); });
        bOut->Connect(bOut->Clicked, [chart]() { chart->SetBarOutline(1.5f); });
        bRef->Connect(bRef->Clicked, [chart]() { chart->SetReferenceLine(100.0, L"目标 100"); });
        bVRef->Connect(bVRef->Clicked, [chart]() { chart->SetVReferenceLine(2, L"三月"); });
        auto sv = std::make_shared<ScrollViewer>(); sv->SetFillWidth(true); sv->SetFillHeight(true);
        sv->SetContentMargin(Thickness(4, 4, 4, 4)); sv->SetContent(col);
        pageChart->SetLayout(sv);
    }

    // ---------- 折线图 LineChart 演示 ----------
    auto pageLine = std::make_shared<Page>();
    {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(10); col->SetMargin(Thickness(16, 16, 16, 16));
        col->AddChild(std::make_shared<Label>(L"折线图 LineChart（多系列 / 平滑 / 标记；悬停看 tooltip）"));
        auto line = std::make_shared<LineChart>();
        line->SetFillWidth(true); line->SetVerticalStretchWeight(1.0f); line->SetShowVGrid(true);
        const wchar_t* lms[] = { L"一月", L"二月", L"三月", L"四月", L"五月", L"六月", L"七月", L"八月", L"九月", L"十月", L"十一月", L"十二月" };
        for (auto m : lms) line->AddCategory(m);
        line->AddSeries(L"访问", { 120, 160, 150, 190, 230, 210, 260, 240, 280, 300, 270, 320 });
        line->AddSeries(L"下载", { 60, 80, 70, 100, 130, 110, 150, 140, 170, 180, 160, 200 });
        line->AddSeries(L"注册", { 20, 30, 25, 40, 55, 45, 60, 58, 70, 80, 72, 95 });
        col->AddChild(line);
        auto row = std::make_shared<RowBox>(); row->SetSpacing(8); row->SetHeight(32);
        auto bSm = std::make_shared<Button>(L"平滑开关"); auto bDash = std::make_shared<Button>(L"虚线开关");
        auto bMk = std::make_shared<Button>(L"标记开关"); auto bVal = std::make_shared<Button>(L"数值开关"); auto bArea = std::make_shared<Button>(L"面积开关");
        row->AddChild(bSm); row->AddChild(bDash); row->AddChild(bMk); row->AddChild(bVal); row->AddChild(bArea);
        auto rowScroll = std::make_shared<ScrollViewer>(); rowScroll->SetHeight(46); rowScroll->SetFillWidth(true);
        rowScroll->SetHorizontalScrollEnabled(true); rowScroll->SetVerticalScrollEnabled(false);
        rowScroll->SetHorizontalScrollBarVisibility(ScrollViewer::ScrollBarVisibility::Always);
        rowScroll->SetContent(row); col->AddChild(rowScroll);
        auto s1 = std::make_shared<bool>(false), s2 = std::make_shared<bool>(false), s3 = std::make_shared<bool>(true), s4 = std::make_shared<bool>(false), s5 = std::make_shared<bool>(false);
        bSm->Connect(bSm->Clicked, [line, s1]() { *s1 = !*s1; line->SetSmooth(*s1); });
        bDash->Connect(bDash->Clicked, [line, s2]() { *s2 = !*s2; line->SetDashed(*s2); });
        bMk->Connect(bMk->Clicked, [line, s3]() { *s3 = !*s3; line->SetShowMarkers(*s3); });
        bVal->Connect(bVal->Clicked, [line, s4]() { *s4 = !*s4; line->SetShowValues(*s4); });
        bArea->Connect(bArea->Clicked, [line, s5]() { *s5 = !*s5; line->SetAreaFill(*s5); });
        auto sv = std::make_shared<ScrollViewer>(); sv->SetFillWidth(true); sv->SetFillHeight(true);
        sv->SetContentMargin(Thickness(4, 4, 4, 4)); sv->SetContent(col);
        pageLine->SetLayout(sv);
    }

    mainHost->AddPage(pageNewA);
    mainHost->AddPage(pageNewB);
    mainHost->AddPage(pageText);
    // ---------- 饼图 PieChart 演示 ----------
    auto pagePie = std::make_shared<Page>();
    {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(10); col->SetMargin(Thickness(16, 16, 16, 16));
        col->AddChild(std::make_shared<Label>(L"饼图 PieChart（环形 / 标签模式；悬停看 tooltip）"));
        auto pie = std::make_shared<PieChart>();
        pie->SetFillWidth(true); pie->SetVerticalStretchWeight(1.0f);
        pie->AddSlice(L"桌面", 45); pie->AddSlice(L"移动", 35); pie->AddSlice(L"平板", 12); pie->AddSlice(L"其他", 8);
        col->AddChild(pie);
        auto row = std::make_shared<RowBox>(); row->SetSpacing(8); row->SetHeight(32);
        auto bDonut = std::make_shared<Button>(L"环形开关"); auto bLbl = std::make_shared<Button>(L"标签模式");
        auto bCw = std::make_shared<Button>(L"方向"); auto bGap = std::make_shared<Button>(L"扇隙");
        row->AddChild(bDonut); row->AddChild(bLbl); row->AddChild(bCw); row->AddChild(bGap);
        auto rowScroll = std::make_shared<ScrollViewer>(); rowScroll->SetHeight(46); rowScroll->SetFillWidth(true);
        rowScroll->SetHorizontalScrollEnabled(true); rowScroll->SetVerticalScrollEnabled(false);
        rowScroll->SetHorizontalScrollBarVisibility(ScrollViewer::ScrollBarVisibility::Always);
        rowScroll->SetContent(row); col->AddChild(rowScroll);
        auto d = std::make_shared<bool>(false), cw = std::make_shared<bool>(true), gp = std::make_shared<bool>(false);
        auto lm = std::make_shared<int>(0);
        bDonut->Connect(bDonut->Clicked, [pie, d]() { *d = !*d; pie->SetDonut(*d ? 0.55f : 0.0f); });
        bCw->Connect(bCw->Clicked, [pie, cw]() { *cw = !*cw; pie->SetClockwise(*cw); });
        bGap->Connect(bGap->Clicked, [pie, gp]() { *gp = !*gp; pie->SetSliceGap(*gp ? 2.0f : 0.0f); });
        bLbl->Connect(bLbl->Clicked, [pie, lm]() {
            *lm = (*lm + 1) % 4;
            pie->SetLabelMode((PieChart::LabelMode)(*lm == 0 ? PieChart::LabelMode::LabelAndPercent : *lm == 1 ? PieChart::LabelMode::Percent : *lm == 2 ? PieChart::LabelMode::Value : PieChart::LabelMode::None));
            });
        auto sv = std::make_shared<ScrollViewer>(); sv->SetFillWidth(true); sv->SetFillHeight(true);
        sv->SetContentMargin(Thickness(4, 4, 4, 4)); sv->SetContent(col);
        pagePie->SetLayout(sv);
    }

    mainHost->AddPage(pageChart);
    mainHost->AddPage(pageLine);
    mainHost->AddPage(pagePie);

    // ---------- 拖放（阶段 1：接收拖入）----------
    auto pageDrag = std::make_shared<ColumnBox>(); pageDrag->SetSpacing(10); pageDrag->SetMargin(Thickness(6, 6, 6, 6));
    {
        auto tip = std::make_shared<Label>(L"把「文件」（从资源管理器）或「文本」拖到下面的卡片上：");
        tip->SetHeight(26);
        auto zone = std::make_shared<Label>(L"拖放目标（拖到这里）");
        zone->SetAlignment(Label::HAlign::Center, Label::VAlign::Center);
        zone->SetBackgroundColor(Color::FromArgb(255, 240, 245, 250));
        zone->SetBackgroundCornerRadius(10.0f);
        zone->SetHeight(220);
        zone->SetDropTargetEnabled(true);
        auto status = std::make_shared<Label>(L"状态：等待拖入…");
        status->SetFillWidth(true);
        status->SetTextOverflow(Label::TextOverflow::Wrap);       // 结果（含长路径）换行显示，不省略
        status->SetAlignment(Label::HAlign::Left, Label::VAlign::Top);
        status->SetHeight(64);
        auto imgView = std::make_shared<Label>(); imgView->SetFillWidth(true); imgView->SetHeight(220);
        imgView->SetImageFit(true);   // 图片等比缩放至完整可见（Label 自带 + 可拖出）
        auto lastImgPath = std::make_shared<std::wstring>();
        imgView->SetDragSource([lastImgPath](DragDataBuilder& b) { if (!lastImgPath->empty()) b.AddFile(*lastImgPath); }, DROPEFFECT_COPY | DROPEFFECT_MOVE);
        zone->Connect(zone->DragEnter, [zone](DragEventArgs&) {
            zone->SetBackgroundColor(Color::FromArgb(255, 205, 228, 250));
        });
        zone->Connect(zone->DragLeave, [zone](DragEventArgs&) {
            zone->SetBackgroundColor(Color::FromArgb(255, 240, 245, 250));
        });
        zone->Connect(zone->Drop, [zone, status, imgView, lastImgPath](DragEventArgs& e) {
            if (e.data.HasFiles()) {
                auto fs = e.data.GetFiles();
                status->SetText(L"状态：收到 " + std::to_wstring(fs.size()) + L" 个文件，第一个：" + (fs.empty() ? std::wstring() : fs[0]));
                if (!fs.empty()) {   // 若是图片文件 → 加载显示
                    std::wstring ext; size_t d = fs[0].find_last_of(L'.'); if (d != std::wstring::npos) ext = fs[0].substr(d);
                    for (auto& c : ext) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
                    if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif" || ext == L".ico" || ext == L".webp") {
                        auto img = Image::FromFile(fs[0]);
                        if (img && !img->IsNull()) { imgView->SetImage(img); *lastImgPath = fs[0]; }   // 记住路径 → 可再拖出去
                    }
                }
            }
            else if (e.data.HasText()) {
                status->SetText(L"状态：收到文本：" + e.data.GetText());
            }
            else {
                status->SetText(L"状态：收到未知数据");
            }
            zone->SetBackgroundColor(Color::FromArgb(255, 240, 245, 250));
            e.effect = DROPEFFECT_COPY;
        });
        pageDrag->AddChild(tip);
        pageDrag->AddChild(zone);
        pageDrag->AddChild(imgView);
        pageDrag->AddChild(status);
        auto tb = std::make_shared<TextBox>(); tb->SetPlaceholder(L"把文本拖到这里（插入到光标处）");
        pageDrag->AddChild(tb);
        auto dragOut = std::make_shared<Button>(L"按住并拖动：拖出文本（拖到别处 / 记事本）");
        dragOut->SetDragSource([](DragDataBuilder& b) { b.AddText(L"ZufyUI 拖放示例文本"); }, DROPEFFECT_COPY | DROPEFFECT_MOVE);
        pageDrag->AddChild(dragOut);
        auto dragFile = std::make_shared<Button>(L"按住并拖动：把本程序拖成文件（拖到资源管理器 / 桌面）");
        dragFile->SetDragSource([](DragDataBuilder& b) {
            wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
            b.AddFile(exe); b.SetPreferredEffect(DROPEFFECT_COPY);
        });
        pageDrag->AddChild(dragFile);
    }
    auto svDrag = std::make_shared<ScrollViewer>(); svDrag->SetFillWidth(true); svDrag->SetFillHeight(true); svDrag->SetContent(pageDrag);
    auto pageDragPage = std::make_shared<Page>(); pageDragPage->SetLayout(svDrag);
    mainHost->AddPage(pageDragPage);

    // ---------- 设置卡片（折叠 / 展开）----------
    auto pageCard = std::make_shared<ColumnBox>(); pageCard->SetSpacing(12); pageCard->SetMargin(Thickness(10, 10, 10, 10));
    {
        auto tip = std::make_shared<Label>(L"点击卡片顶部（或右侧箭头）折叠 / 展开：");
        tip->SetHeight(24);
        auto list1 = std::make_shared<SettingsList>();
        list1->AddRow(Icon::View, L"屏幕", L"亮度、颜色、夜间模式", [] {});
        list1->AddRow(Icon::Settings, L"电源和电池", L"睡眠、节电模式、电池使用情况", [] {});
        list1->AddRow(Icon::Folder, L"安装的应用", L"卸载、默认应用、可选功能", [] {});
        auto card1 = std::make_shared<Expander>(L"推荐设置", L"最近使用的和常用的设置");
        card1->SetFillWidth(true); card1->SetContent(list1);
        auto list2 = std::make_shared<SettingsList>();
        list2->AddRow(Icon::View, L"显示", L"监视器、亮度、缩放", [] {});
        list2->AddRow(Icon::Volume, L"声音", L"音量、输出、输入", [] {});
        list2->AddRow(Icon::Settings, L"通知", L"来自应用和系统的通知", [] {});
        auto card2 = std::make_shared<Expander>(L"系统", L"显示、声音、通知、电源");
        card2->SetFillWidth(true); card2->SetContent(list2);
        pageCard->AddChild(tip);
        pageCard->AddChild(card1);
        pageCard->AddChild(card2);
        auto contentCol = std::make_shared<ColumnBox>(); contentCol->SetSpacing(8);
        contentCol->AddChild(std::make_shared<Label>(L"这是一个可放任意内容的折叠卡片："));
        contentCol->AddChild(std::make_shared<Button>(L"按钮 A"));
        contentCol->AddChild(std::make_shared<Button>(L"按钮 B"));
        contentCol->AddChild(std::make_shared<ToggleSwitch>(false));
        auto card3 = std::make_shared<Expander>(L"任意内容", L"里面放按钮 / 开关等真实控件");
        card3->SetFillWidth(true); card3->SetContent(contentCol);
        pageCard->AddChild(card3);
    }
    auto svCard = std::make_shared<ScrollViewer>(); svCard->SetFillWidth(true); svCard->SetFillHeight(true); svCard->SetContent(pageCard);
    auto pageCardPage = std::make_shared<Page>(); pageCardPage->SetLayout(svCard);
    mainHost->AddPage(pageCardPage);

    // ---------- 主题测试页 ----------
    auto pageTheme = std::make_shared<Page>();
    {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(10); col->SetMargin(Thickness(16, 16, 16, 16));
        col->AddChild(std::make_shared<Label>(L"主题系统：切换浅色 / 深色 / 高对比度；下方色卡展示每个 ThemeRole 的当前值"));
        auto row = std::make_shared<RowBox>(); row->SetSpacing(8);
        auto bL = std::make_shared<Button>(L"浅色"); bL->Connect(bL->Clicked, []() { ThemeManager::SetLightAppTheme(); });
        auto bD = std::make_shared<Button>(L"深色"); bD->Connect(bD->Clicked, []() { ThemeManager::SetDarkAppTheme(); });
        auto bH = std::make_shared<Button>(L"高对比度"); bH->Connect(bH->Clicked, []() { ThemeManager::SetAppTheme(Theme::HighContrast()); });
        auto bF = std::make_shared<Button>(L"模拟系统:深色"); bF->Connect(bF->Clicked, []() { ThemeManager::DebugForceSystemTheme(true, true, false); });
        auto bF2 = std::make_shared<Button>(L"模拟系统:高对比度"); bF2->Connect(bF2->Clicked, []() { ThemeManager::DebugForceSystemTheme(true, false, true); });
        auto bF3 = std::make_shared<Button>(L"关闭模拟"); bF3->Connect(bF3->Clicked, []() { ThemeManager::DebugForceSystemTheme(false, false, false); });
        row->AddChild(bL); row->AddChild(bD); row->AddChild(bH); row->AddChild(bF); row->AddChild(bF2); row->AddChild(bF3);
        col->AddChild(row);
        auto swatch = std::make_shared<ThemeSwatch>(); swatch->SetFillWidth(true);
        col->AddChild(swatch);
        auto ctrl = std::make_shared<RowBox>(); ctrl->SetSpacing(8); ctrl->SetHeight(34);
        auto tb = std::make_shared<TextBox>(); tb->SetText(L"文本框"); tb->SetWidth(140);
        auto cb = std::make_shared<ComboBox>(); cb->AddItem(L"选项 A"); cb->AddItem(L"选项 B"); cb->SetSelectedIndex(0);
        auto sw = std::make_shared<ToggleSwitch>(true);
        auto pb = std::make_shared<ProgressBar>(); pb->SetValue(0.6f); pb->SetWidth(120);
        ctrl->AddChild(tb); ctrl->AddChild(cb); ctrl->AddChild(sw); ctrl->AddChild(pb);
        col->AddChild(ctrl);
        auto lv = std::make_shared<ListView>(); lv->SetHeight(120); lv->SetFillWidth(true);
        for (int i = 0; i < 6; ++i) lv->AddItem(L"列表项 " + std::to_wstring(i + 1));
        col->AddChild(lv);
        {
            // DSL 冒烟：链式构造 + 组子树 + 连信号
            using namespace ::ZufyUI::dsl;
            auto lastPath = std::make_shared<std::wstring>();
            col->AddChild(Row(
                Lbl(L"DSL 链式：").margin(Thickness(0, 0, 6, 0)),
                Btn(L"点击我").size(96, 30).on<&Button::Clicked>([]() { MessageBoxW(nullptr, L"DSL 按钮被点击", L"DSL", MB_OK); }),
                Check(L"选项").margin(Thickness(12, 0, 6, 0)),
                Toggle(true)
            ).with(&RowBox::SetSpacing, 8.0f).shared());
        }
        auto sv = std::make_shared<ScrollViewer>(); sv->SetFillWidth(true); sv->SetFillHeight(true); sv->SetContent(col);
        pageTheme->SetLayout(sv);
    }
    mainHost->AddPage(pageTheme);

    // ---------- DSL 复杂构造测试页 ----------
    auto pageDsl = std::make_shared<Page>();
    {
        using namespace ::ZufyUI::dsl;
        auto clicks = std::make_shared<int>(0);
        auto counter = Lbl(L"点击次数：0");

        auto makeBtn = [&](const std::wstring& t, int d) {
            return Btn(t).size(84, 30).on<&Button::Clicked>([clicks, counter, d]() {
                *clicks += d;
                counter->SetText(L"点击次数：" + std::to_wstring(*clicks));
            });
        };

        auto col = Col().with(&ColumnBox::SetSpacing, 10.0f).margin(Thickness(16, 16, 16, 16));

        col->AddChild(
            Row(
                makeBtn(L"+1", 1), makeBtn(L"+10", 10), makeBtn(L"-1", -1),
                Check(L"复选").checked(true),
                Toggle(false).on<&ToggleSwitch::Toggled>([](bool) {})
            ).with(&RowBox::SetSpacing, 8.0f).shared());
        col->AddChild(counter.shared());

        auto lv = List().fillWidth(true).height(90);
        for (int i = 0; i < 4; ++i) lv->AddItem(L"DSL 列表项 " + std::to_wstring(i + 1));
        col->AddChild(lv.shared());

        col->AddChild(
            Grid(
                Lbl(L"A").with(&Label::SetBackgroundColor, Color::FromArgb(40, 0, 120, 212)),
                Lbl(L"B").with(&Label::SetBackgroundColor, Color::FromArgb(40, 16, 124, 16)),
                Lbl(L"C").with(&Label::SetBackgroundColor, Color::FromArgb(40, 196, 43, 28))
            ).with(&GridLayout::SetSpacing, 6.0f, 6.0f).shared());

        col->AddChild(
            ::ZufyUI::dsl::Expander(L"DSL 折叠卡片", L"内容也是 DSL 构建")
                .with(&Expander::SetExpanded, true)
                .with(&Expander::SetContent,
                      Col(Lbl(L"展开内容 1"), Btn(L"卡片内按钮").size(120, 30).shared()).shared())
                .shared());

        auto echo = Lbl(L"输入回显：");
        auto tbi = Txt(L"在此输入").with(&TextBox::SetWidth, 220);
        tbi.on<&TextBox::TextChanged>([echo](const std::wstring& s) { echo->SetText(L"输入回显：" + s); });
        col->AddChild(Row(tbi.shared(), echo.shared()).with(&RowBox::SetSpacing, 8.0f).shared());

        col->AddChild(Progress().value(0.65f).width(160).height(22).shared());
        col->AddChild(
            Row(
                Pie().size(220, 170).slice(L"A", 3).slice(L"B", 5).slice(L"C", 2),
                Bar().size(360, 170).category(L"一").category(L"二").category(L"三")
                         .series(L"甲", { 3, 5, 2 }).series(L"乙", { 2, 4, 6 })
            ).with(&RowBox::SetSpacing, 8.0f).shared());
        pageDsl->SetLayout(Scroll().fill(true).with(&ScrollViewer::SetContent, col.shared()).shared());
    }
    mainHost->AddPage(pageDsl);

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