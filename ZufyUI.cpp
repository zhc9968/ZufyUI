// main.cpp - ZufyUI 综合自动化布局测试（使用 Connect 自动管理连接）
#include "pch.h"
#include "ZDataViewer.h"
#include "ZufyUIWindowTool.h"

using namespace ZufyUI;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 应用身份自注册（需在包含本库头文件前 #define ZUFYUI_ALLOW_APP_REGISTRATION 授权）：
    // 统一 AUMID；授权后自动写注册表 + 把图标缓存到本地，进程退出时清缓存。
    {
        auto appIcon = Image::FromFile(L"ZufyUI.ico");
        if (!appIcon || appIcon->IsNull()) appIcon = Image::FromFile(L"..\\..\\ZufyUI.ico");
        if (!appIcon || appIcon->IsNull()) appIcon = Image::FromFile(L"..\\..\\..\\ZufyUI.ico");
        RegisterApp(AppInfo{ L"ZufyUI Demo", L"ZufyUI.Demo", appIcon });
    }
    // 演示程序自己的默认背景：亚克力 + 半透明白色着色。
    // 库的默认是 Backdrop::None（不替应用决定），所以不透明/着色都由应用这里指定。
    Window::SetDefaultBackdrop(Backdrop::Acrylic, 0x80FFFFFF);
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
    auto grid1 = page1->GetLayoutAs<GridLayout>();
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

        auto numBox = std::make_shared<TextBox>();
        numBox->SetPlaceholder(L"数字");
        numBox->SetMaxLength(10);
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

        auto listButtons = std::make_shared<RowBox>();
        listButtons->SetSpacing(10);
        auto btnSortList = std::make_shared<Button>(L"排序");
        btnSortList->Connect(btnSortList->Clicked, [listView]() {
            static bool asc = true;
            listView->Sort(asc);
            asc = !asc;
            });
        listButtons->AddChild(btnAddItem);
        listButtons->AddChild(btnClearList);
        listButtons->AddChild(btnSortList);
        auto btnNoneMode = std::make_shared<Button>(L"无选择模式");
        btnNoneMode->Connect(btnNoneMode->Clicked, [listView]() { listView->SetSelectionMode(ListView::SelectionMode::None); });
        listButtons->AddChild(btnNoneMode);

        // 新 API 测试按钮
        auto btnBatch = std::make_shared<Button>(L"批量+50 (BeginUpdate)");
        btnBatch->Connect(btnBatch->Clicked, [listView]() {
            listView->BeginUpdate();
            for (int k = 0; k < 50; ++k) listView->AddItem(L"批量项 " + std::to_wstring(k + 1));
            listView->EndUpdate();
            });
        listButtons->AddChild(btnBatch);
        auto btnRich = std::make_shared<Button>(L"富项 (图标+内嵌 Label)");
        btnRich->Connect(btnRich->Clicked, [listView]() {
            auto rich = std::make_shared<Label>(L"富项 ");
            rich->SetTextColor(Color::FromArgb(255, 0, 90, 160));
            auto inner = std::make_shared<Label>(L"[内嵌]");
            inner->SetTextColor(Color::FromArgb(255, 200, 60, 60));
            rich->AddChild(inner);           // 内嵌 Label → 走 Window 合成递归画出来
            listView->AddItem(rich);
            });
        listButtons->AddChild(btnRich);

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
                return a->columns[0] < b->columns[0];
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
            "data:image/png;base64,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");

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
        // 用项目自带的图标（不是系统默认图标）
        auto loadAppImage = []() -> std::shared_ptr<Image> {
            const wchar_t* candidates[] = { L"ZufyUI.ico", L"..\\..\\ZufyUI.ico", L"..\\..\\..\\ZufyUI.ico" };
            for (auto p : candidates) {
                auto img = Image::FromFile(p);
                if (img && !img->IsNull()) return img;
            }
            return nullptr;
        };

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

    win.Show();            // 显示由应用决定
    win.SetMinSize(800, 600);
    win.Run();

    return 0;
}