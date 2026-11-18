// ============================================================================
// tests/zufyui_tests.cpp —— ZufyUI 自检（编译期断言 + 运行期断言）
//
//   构建并运行：
//       powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1
//   退出码 0 = 全部通过；非 0 = 有断言失败（失败项会打印到 stdout）。
//
//   本测试默认**不创建可见窗口**（不需要 D2D / DComp / 消息循环），所以能在命令行 / CI 里直接跑。
//   UIThread 取消测试会创建一个**隐藏 dispatcher 窗口**；没有桌面会话时自动跳过。
//   覆盖：容器加子分派、空值过滤、布局快捷、Grid 坐标、数据视图与菜单构建糖、新工厂、Wrap、信号生命周期。
// ============================================================================
// 包含顺序刻意保持“WindowTool 在前”：它自己会 include ZufyUIWidgets.h，
// 所以这里不再手动包含 —— 这个测试同时验证 WindowTool 是否自洽。
#include "ZufyUIWindowTool.h"   // 无障碍提供程序：UIElement 析构要用其中的定义（ForgetAccessibleElement 等）
#include "ZufyUIDsl.h"
#include <chrono>
#include <cstdio>
#include <future>
#include <latch>
#include <random>
#include <stdexcept>
#include <thread>

namespace d = ZufyUI::dsl;
using namespace ZufyUI;

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (expr) { ++g_pass; }                                                  \
        else { ++g_fail; std::printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
    } while (0)

// ---------------------------------------------------------------------------
// 1) 编译期：容器能力探测（决定 Ref::add() / Ref::cell() 怎么分派）
// ---------------------------------------------------------------------------
static_assert(d::HasAddChild1<ColumnBox>::value,     "ColumnBox 应有单参 AddChild");
static_assert(d::HasAddChild1<RowBox>::value,        "RowBox 应有单参 AddChild");
static_assert(!d::HasAddChild1<GridLayout>::value,   "GridLayout 没有单参 AddChild");
static_assert(d::HasGridAddChild<GridLayout>::value, "GridLayout 应有带坐标 AddChild");
static_assert(!d::HasGridAddChild<ColumnBox>::value, "ColumnBox 不是网格容器");
static_assert(d::HasAddPage<PageHost>::value,        "PageHost 应有 AddPage");
static_assert(!d::HasAddChild1<PageHost>::value,     "PageHost 没有 AddChild");
static_assert(d::HasAddChild1<Label>::value,         "Label 有单参 AddChild（内联子控件）");
static_assert(!d::HasAddChild1<TableView>::value,    "TableView 没有单参 AddChild");

// ---------------------------------------------------------------------------
// 2) 编译期：WindowApp 的 root() 泛化（A1）与 chrome / hidden（A2）
// ---------------------------------------------------------------------------
template<class T> using RootRet = decltype(std::declval<d::WindowApp&>().root(std::declval<d::Ref<T>>()));
static_assert(std::is_same_v<RootRet<ColumnBox>,  d::WindowApp&>, "root(Col) 应可用");
static_assert(std::is_same_v<RootRet<RowBox>,     d::WindowApp&>, "root(Row) 应可用");
static_assert(std::is_same_v<RootRet<GridLayout>, d::WindowApp&>, "root(Grid) 应可用");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().hidden()), d::WindowApp&>,
              "hidden() 应存在");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().titleBar(std::declval<d::Ref<Label>>())), d::WindowApp&>,
              "titleBar(Ref<T>) 应可用");

// Ref<Derived> 不能一步隐式转成 shared_ptr<Base> —— 这正是 A1 的根因，锁死它防止回归。
static_assert(!std::is_convertible_v<d::Ref<RowBox>, std::shared_ptr<Layout>>,
              "Ref<Derived> → shared_ptr<Base> 需要两次用户定义转换，必须由专门重载解决");

// ---------------------------------------------------------------------------
// 2b) 编译期：WindowApp 扩面（B6）与可移动（A5）、Ref::apply（B5）
// ---------------------------------------------------------------------------
static_assert(std::is_move_constructible_v<d::WindowApp>,    "WindowApp 应可移动（A5）");
static_assert(std::is_move_assignable_v<d::WindowApp>,       "WindowApp 应可移动赋值（A5）");
static_assert(!std::is_copy_constructible_v<d::WindowApp>,   "WindowApp 仍不可拷贝");

static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().minSize(1, 1)), d::WindowApp&>,     "minSize");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().position(1, 1)), d::WindowApp&>,    "position");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().frameRateLimit(60)), d::WindowApp&>, "frameRateLimit");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().useAppTheme(true)), d::WindowApp&>, "useAppTheme");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().dropTarget(true)), d::WindowApp&>,  "dropTarget");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().titleBarVisible(true)), d::WindowApp&>, "titleBarVisible");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().showTitleText(true)), d::WindowApp&>, "showTitleText");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().menuBarInTitleBar(true)), d::WindowApp&>, "menuBarInTitleBar");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().icon(nullptr, nullptr)), d::WindowApp&>, "icon");
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().theme(std::declval<const Theme&>())), d::WindowApp&>, "theme");
// 窗口信号（成员指针形式，覆盖 Window::Closed 等 8 个裸信号成员）
static_assert(std::is_same_v<decltype(std::declval<d::WindowApp&>().on<&Window::Closed>([]{})), d::WindowApp&>, "on<&Window::Closed>");

// ---------------------------------------------------------------------------
// 3) 运行期：容器 / 加子 / 空值过滤
// ---------------------------------------------------------------------------
static void test_containers() {
    std::printf("容器 / 加子 / 空值过滤\n");

    auto lbl = d::ZLbl(L"a");
    auto col = d::ZCol(lbl);
    col->RefreshChildren();   // GetChildren() 返回 RefreshChildren() 维护的视图缓冲
    CHECK(col->GetChildren().size() == 1);
    CHECK(col->GetChildren()[0] == lbl.ptr());
    CHECK(lbl->GetParent() == col.ptr());

    // A4：空子元素被容器过滤，不再解引用空指针
    auto col2 = d::ZCol(d::ZLbl(L"b"), nullptr, d::Ref<Label>{});
    col2->RefreshChildren();
    CHECK(col2->GetChildren().size() == 1);

    // A3：add() 按容器分派（ColumnBox → AddChild）
    auto col3 = d::ZCol().add(d::ZLbl(L"x")).add(d::ZLbl(L"y"));
    col3->RefreshChildren();
    CHECK(col3->GetChildren().size() == 2);

    // A3：Label 也是 AddChild 分支（内联子控件）——只验父子关系，Label 不维护 GetChildren 视图
    auto host = d::ZLbl(L"host");
    auto inlineChild = d::ZLbl(L"inline");
    host.add(inlineChild);
    CHECK(inlineChild->GetParent() == host.ptr());
    // Label::AddChild 自身也拒绝空值
    host.add(nullptr);
    CHECK(inlineChild->GetParent() == host.ptr());

    // A3：PageHost → AddPage
    auto ph = d::ZPages().add(d::ZPage()).add(d::ZPage());
    CHECK(ph->GetPageCount() == 2);
    // 非 Page 实参被跳过，不会塞空指针进去
    ph.add(d::ZLbl(L"不是页"));
    CHECK(ph->GetPageCount() == 2);

    // A3：add() 之后仍可继续链式调用
    auto row = d::ZRow().add(d::ZLbl(L"1")).spacing(6.0f);
    row->RefreshChildren();
    CHECK(row->GetChildren().size() == 1);
    CHECK(row->GetSpacing() == 6.0f);
}

// ---------------------------------------------------------------------------
// 3b) 运行期：Ref::apply 逃生口（B5）
// ---------------------------------------------------------------------------
static void test_apply() {
    std::printf("apply() 逃生口\n");

    int calls = 0;
    auto l = d::ZLbl(L"before").apply([&calls](Label& x) {
        ++calls;
        x.SetText(L"after");
        x.SetPadding(6.0f);
    });
    CHECK(calls == 1);
    CHECK(l->GetText() == L"after");

    // 空 Ref 不应调用回调（也不崩）
    int calls2 = 0;
    d::Ref<Label>{}.apply([&calls2](Label&) { ++calls2; });
    CHECK(calls2 == 0);

    // apply 之后仍可继续链式
    auto l2 = d::ZLbl(L"x").apply([](Label& x) { x.SetMinWidth(10.0f); }).minWidth(20.0f);
    CHECK(l2->GetMinWidth() == 20.0f);
}

// ---------------------------------------------------------------------------
// 4) 运行期：布局 / 尺寸 / 外观快捷（B2）
// ---------------------------------------------------------------------------
static void test_layout_sugar() {
    std::printf("布局 / 尺寸 / 外观快捷\n");

    auto col = d::ZCol().spacing(12.0f);
    CHECK(col->GetSpacing() == 12.0f);

    auto l = d::ZLbl(L"x").minWidth(40.0f).maxWidth(120.0f).bleed(2.0f).visible(false);
    CHECK(l->GetMinWidth() == 40.0f);
    CHECK(l->GetMaxWidth() == 120.0f);
    CHECK(l->GetBleed() == 2.0f);
    CHECK(!l->IsVisible());

    auto l2 = d::ZLbl(L"y").margin(Thickness(1, 2, 3, 4)).size(50.0f, 20.0f);
    CHECK(l2->GetMargin().left == 1.0f);
    CHECK(l2->GetWidth() == 50.0f);
    CHECK(l2->GetHeight() == 20.0f);

    auto l3 = d::ZLbl(L"z").enabled(false).hWeight(2.0f).vWeight(0.5f).weight(3.0f, 1.0f);
    CHECK(!l3->IsEnabled());

    // GridLayout：双参 spacing + align（对齐只有 GridLayout 有）
    auto g = d::ZGrid().spacing(4.0f, 6.0f).align(d::Align::Center, d::Align::End);
    CHECK(g.ptr() != nullptr);

    // UIElement 标准接口的其余快捷：只验“可调用、不崩、返回自身”
    auto l4 = d::ZLbl(L"w").cache(true).dropTarget(true).contextMenuEnabled(false).accessible(L"名字");
    CHECK(l4.ptr() != nullptr);
    l4.clipRect(Rect(0, 0, 10, 10)).noClip();
    CHECK(l4.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 5) 运行期：Grid 坐标（A3 / B1）
// ---------------------------------------------------------------------------
static void test_grid_coords() {
    std::printf("Grid 坐标\n");

    auto a = d::ZLbl(L"A");
    auto b = d::ZLbl(L"B");
    auto g = d::ZGrid(d::ZCell(0, 0, a), d::ZCell(1, 2, b));
    g->RefreshChildren();
    CHECK(g->GetChildren().size() == 2);

    auto c = d::ZLbl(L"C");
    auto g2 = d::ZGrid().cell(0, 0, c).cell(1, 1, d::ZLbl(L"D"));
    g2->RefreshChildren();
    CHECK(g2->GetChildren().size() == 2);

    // 裸元素沿用旧行为（按出现顺序 (i,0)），不破坏既有代码
    auto g3 = d::ZGrid(d::ZLbl(L"x"), d::ZLbl(L"y"));
    g3->RefreshChildren();
    CHECK(g3->GetChildren().size() == 2);

    // Cell 的跨行跨列
    auto g4 = d::ZGrid(d::ZCell(0, 0, d::ZLbl(L"span"), 1, 2));
    g4->RefreshChildren();
    CHECK(g4->GetChildren().size() == 1);
}

// ---------------------------------------------------------------------------
// 6) 运行期：数据视图与菜单构建糖（B4）
// ---------------------------------------------------------------------------
static void test_data_views_and_menu() {
    std::printf("数据视图 / 菜单构建糖\n");

    auto lv = d::ZList().items({ L"a", L"b", L"c" });
    CHECK(lv->GetItemCount() == 3);

    std::vector<std::wstring> v{ L"d", L"e" };
    auto lv2 = d::ZList().items(v);
    CHECK(lv2->GetItemCount() == 2);

    auto tv = d::ZTable().rows(3).cols(2).header(0, L"列0").cell(0, 0, L"r0c0");
    CHECK(tv->GetRowCount() == 3);
    CHECK(tv->GetColumnCount() == 2);

    auto tree = d::ZTree().roots({ L"根1", L"根2" });
    CHECK(tree.ptr() != nullptr);

    auto m = d::ZMenu().item(L"打开").separator().item(L"退出");
    CHECK(m.ptr() != nullptr);

    auto sub = d::ZMenu().item(L"子项");
    auto m2 = d::ZMenu().item(L"顶层").submenu(L"更多", sub);
    CHECK(m2.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 7) 运行期：新工厂 / Wrap / 图片糖（B3）
// ---------------------------------------------------------------------------
static void test_factories_and_wrap() {
    std::printf("新工厂 / Wrap / 图片糖\n");

    auto rg = d::ZRadioGroup();
    CHECK(rg.ptr() != nullptr);

    auto sb = d::ZScrollBar(false);
    CHECK(sb.ptr() != nullptr);

    auto g = d::ZGlyph(Icon::Save, 18.0f);
    CHECK(g.ptr() != nullptr);
    CHECK(g->GetIcon() == Icon::Save);
    CHECK(g->GetIconSize() == 18.0f);

    auto rg2 = d::ZMake<RadioGroup>();
    CHECK(rg2.ptr() != nullptr);

    auto existing = std::make_shared<Label>(L"已有");
    auto w = d::ZWrap(existing);
    CHECK(w.ptr() == existing.get());
    CHECK(w->GetText() == L"已有");

    // Image 是资源对象（不是 UIElement），只能由控件侧接收
    auto withImg = d::ZLbl(L"pic").image(nullptr).imageFit(true);
    CHECK(withImg.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 8) 编译期：padding() 的适用范围（重排 demo 时踩到）
//    Ref::padding(float) 与 padding(Thickness) 都要求 T 有 SetPadding ——
//    ColumnBox / ScrollViewer 根本没有 SetPadding，所以两者都不可用，只能用 .margin(Thickness)。
// ---------------------------------------------------------------------------
template<class T, class = void> struct HasSetPaddingF : std::false_type {};
template<class T> struct HasSetPaddingF<T, std::void_t<decltype(std::declval<T&>().SetPadding(0.0f))>> : std::true_type {};
static_assert(HasSetPaddingF<Card>::value,          "Card 有 SetPadding(float) → .padding(float) 可用");
static_assert(!HasSetPaddingF<ColumnBox>::value,    "ColumnBox 没有 SetPadding → .padding(...) 不可用，请用 .margin(Thickness)");
static_assert(!HasSetPaddingF<ScrollViewer>::value, "ScrollViewer 没有 SetPadding → 同上");

// ---------------------------------------------------------------------------
// 9) 运行期：重排 demo 时踩到的 DSL 边界（固化防回归）
// ---------------------------------------------------------------------------
static void test_dsl_pitfalls() {
    std::printf("DSL 边界（默认参数 / 同名歧义 / padding / 尺寸工厂）\n");

    // a) Card 可用 padding(float)；Col/Scroll 只能用 margin(Thickness)（见上面的 static_assert）
    auto cardPad = d::ZCard().padding(6.0f);
    CHECK(cardPad.ptr() != nullptr);
    auto colPad = d::ZCol().margin(Thickness(4, 4, 4, 4));
    CHECK(colPad.ptr() != nullptr);

    // b) 单参、无默认参数的成员函数可安全用于 .with()。
    //    历史坑：带默认参数的成员函数（如旧的 SetCheckable(bool,bool)）成员指针含全部形参，
    //    .with 会按推导参数包调用 → 报"参数太少"。P0-1 已把这类函数拆成无默认参数的重载。
    auto treeCk = d::ZTree().with(&TreeView::SetCheckable, true);
    CHECK(treeCk.ptr() != nullptr);

    // c) 同名工厂当"表达式 / 类型实参"用必须全限定（::ZufyUI::dsl::ZSlider）；
    //    而 .with(&Slider::SetRange, ...) 这种"嵌套名限定符"位置不歧义。
    auto sl = ::ZufyUI::dsl::ZSlider()
                  .with(&Slider::SetRange, 0.0f, 100.0f)
                  .with(&Slider::SetStep, 5.0f)
                  .with(&Slider::SetSnapToStep, true)
                  .value(40.0f);
    CHECK(sl.ptr() != nullptr);
    CHECK(sl->GetValue() == 40.0f);

    // d) 花括号初始化列表过不了 .with()（转发引用推不出 braced-init-list）→ 用 .apply(...)
    auto tv = d::ZTree().with(&TreeView::SetColumnCount, 2)
                       .apply([](TreeView& t) { t.SetHeaderLabels({ L"名称", L"类型" }); });
    CHECK(tv.ptr() != nullptr);

    // e) Grid(Cell(...)) 声明式坐标
    auto grid = d::ZGrid(d::ZCell(0, 0, d::ZLbl(L"A")), d::ZCell(1, 1, d::ZLbl(L"B"))).spacing(4.0f, 4.0f);
    grid->RefreshChildren();
    CHECK(grid->GetChildren().size() == 2);

    // f) items() 批量加项
    auto combo = d::ZCombo().items({ L"一", L"二", L"三" });
    CHECK(combo->GetItemCount() == 3);

    // g) Table 行列 / 表头 / cell 快捷
    auto tbl = d::ZTable().rows(3).cols(2).header(0, L"列一").header(1, L"列二").cell(0, 0, L"x").cell(0, 1, L"y");
    CHECK(tbl->GetRowCount() == 3);
    CHECK(tbl->GetColumnCount() == 2);

    // h) Ring() 走默认尺寸（工厂无法传尺寸）；要自定义尺寸只能直接构造 ProgressRing(size)
    auto ring = d::ZRing();
    CHECK(ring.ptr() != nullptr);
    auto ring36 = std::make_shared<ProgressRing>(36.0f);
    CHECK(ring36 != nullptr);

    // i) 页面重排的固定写法：Col + add() 分派 + Scroll 包一层
    auto col = d::ZCol();
    col.add(d::ZLbl(L"1"));
    col->RefreshChildren();
    CHECK(col->GetChildren().size() == 1);
    col.add(d::ZRow(d::ZLbl(L"a"), d::ZLbl(L"b")).shared());
    col->RefreshChildren();
    CHECK(col->GetChildren().size() == 2);
    auto sc = d::ZScroll().fill(true).margin(Thickness(8, 8, 8, 8))
                         .with(&ScrollViewer::SetContent, col.shared());
    CHECK(sc.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 10) 运行期：本批修复的行为回归（按钮自动撑宽 / 进度条与开关缓动 / SplitView 折叠）
// ---------------------------------------------------------------------------
static void test_fix_regressions() {
    std::printf("修复回归（Button 撑宽 / 进度条·开关缓动 / SplitView 折叠）\n");

    // ⑧ Button：文案超出默认宽度时应自动撑开，不再被省略号裁掉
    auto longBtn = d::ZBtn(L"这是一个非常非常非常非常非常非常非常长的按钮文案，远超默认宽度");
    longBtn->Measure(Size(1000.0f, 100.0f));
    CHECK(longBtn->GetDesiredSize().width > Button::DefaultWidth);

    // 关掉自动撑宽 → 回到固定宽度（给需要精确控制布局的场景留后路）
    auto fixedBtn = d::ZBtn(L"这是一个非常非常非常非常非常非常非常长的按钮文案，远超默认宽度")
                        .with(&Button::SetAutoSizeToText, false);
    fixedBtn->Measure(Size(1000.0f, 100.0f));
    CHECK(fixedBtn->GetDesiredSize().width == Button::DefaultWidth);

    // ⑥ Label：不设固定宽度时，宽度跟随文本 —— 这正是"后面还有空间却提前省略号"的修复点
    //    （旧代码是 .width(100.0f)，文本超过 100px 就被省略号裁掉；minWidth 只是给父容器的夹取下限）
    auto echoLbl = d::ZLbl(L"输入：一段不算短的回显文本内容");
    echoLbl->Measure(Size(1000.0f, 100.0f));
    CHECK(echoLbl->GetWidth() <= 0.0f);                            // 没有固定宽度
    CHECK(echoLbl->GetDesiredSize().width > 100.0f);               // 宽度跟随文本
    CHECK(d::ZLbl(L"短").minWidth(160.0f)->GetMinWidth() >= 160.0f); // 快捷方法写入 MinWidth（夹取由父容器负责）

    // ④ ProgressBar：SetValue 后进入"缓动中"，跑约 1 秒后收敛
    auto pb = d::ZProgress().value(0.0f);
    pb->SetValue(1.0f);
    CHECK(pb->HasActiveAnimation());
    for (int i = 0; i < 70; ++i) pb->UpdateAnimation(1.0f / 60.0f);
    CHECK(!pb->HasActiveAnimation());
    CHECK(pb->GetValue() == 1.0f);

    // ③ ToggleSwitch：一帧走不完（缓动，不是瞬变），足够多帧后收敛到 1
    auto tg = d::ZToggle(false);
    tg->SetOn(true);
    tg->UpdateAnimation(1.0f / 60.0f);
    CHECK(tg->DebugAnimationProgress() > 0.0f);
    CHECK(tg->DebugAnimationProgress() < 0.5f);          // 若为线性瞬变这里会是 1.0
    for (int i = 0; i < 70; ++i) tg->UpdateAnimation(1.0f / 60.0f);
    CHECK(tg->DebugAnimationProgress() >= 0.999f);

    // ⑮⑯ SplitView：折叠后该栏长度为 0；展开回到"最小状态"；信号与开关都生效
    auto sv = d::ZSplit().size(400.0f, 200.0f);
    sv->SetMinFirst(100.0f);
    sv->SetMinSecond(100.0f);
    sv->SetFirst(d::ZLbl(L"左").shared());
    sv->SetSecond(d::ZLbl(L"右").shared());
    sv->Arrange(Rect(0.0f, 0.0f, 400.0f, 200.0f));
    CHECK(sv->GetFirst()->GetArrangedRect().width > 1.0f);      // 折叠前左栏有宽度

    int collapsedSignal = -1;
    sv->CollapsedChanged.connect([&collapsedSignal](int side) { collapsedSignal = side; });
    sv->CollapseFirst();
    CHECK(sv->IsFirstCollapsed());                       // 逻辑状态立即生效
    CHECK(collapsedSignal == 1);
    for (int i = 0; i < 30; ++i) sv->UpdateAnimation(1.0f / 60.0f);   // 折叠是**动画**的：先跑完 0.2s
    sv->Arrange(Rect(0.0f, 0.0f, 400.0f, 200.0f));
    CHECK(sv->GetFirst()->GetArrangedRect().width == 0.0f);     // 折叠后左栏占 0
    CHECK(sv->GetSecond()->GetArrangedRect().width > 300.0f);   // 右栏吃掉全部

    sv->Expand();                                                // 展开到"最小状态"（min = 100px）
    CHECK(!sv->IsFirstCollapsed());
    CHECK(collapsedSignal == 0);
    for (int i = 0; i < 30; ++i) sv->UpdateAnimation(1.0f / 60.0f);   // 展开同样是动画的
    sv->Arrange(Rect(0.0f, 0.0f, 400.0f, 200.0f));
    CHECK(sv->GetFirst()->GetArrangedRect().width >= 99.0f);
    CHECK(sv->GetFirst()->GetArrangedRect().width <= 101.0f);

    // 折叠功能可整体关闭（⑮ 的"这个东西可以关闭"）
    sv->SetCollapseEnabled(false);
    CHECK(!sv->IsCollapseEnabled());
    sv->SetCollapseEnabled(true);
    CHECK(sv->IsCollapseEnabled());
    CHECK(sv->GetCollapseThreshold() > 0.0f);

    {
        auto sw = d::ZSplit().size(400.0f, 200.0f);
        sw->SetMinFirst(300.0f);
        sw->SetMinSecond(300.0f);
        sw->SetFirst(d::ZLbl(L"first").shared());
        sw->SetSecond(d::ZLbl(L"second").shared());
        sw->Arrange(Rect(0.0f, 0.0f, 400.0f, 200.0f));
        sw->CollapseSecond();
        for (int i = 0; i < 30; ++i) sw->UpdateAnimation(1.0f / 60.0f);
        sw->Arrange(Rect(0.0f, 0.0f, 400.0f, 200.0f));
        CHECK(sw->GetActivePane() == 1);

        int active = 0;
        sw->ActivePaneChanged.connect([&active](int side) { active = side; });
        sw->SwitchToSecond();
        CHECK(sw->IsSwitching());
        CHECK(sw->GetSwitchProgress() == 0.0f);
        for (int i = 0; i < 30; ++i) sw->UpdateAnimation(1.0f / 60.0f);
        CHECK(!sw->IsSwitching());
        CHECK(sw->GetActivePane() == 2);
        CHECK(active == 2);
        CHECK(sw->IsFirstCollapsed());
        CHECK(!sw->IsSecondCollapsed());
    }
}

template<class Pred>
static bool PumpUntil(Pred pred, int timeoutMs = 3000) {
    const DWORD start = GetTickCount();
    while ((int)(GetTickCount() - start) < timeoutMs) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return pred();
}

static void test_async_edge_cases() {
    std::printf("Async edge cases: no dispatcher / throwing callbacks\n");
    const HWND savedWindow = detail::g_uiDispatcherWindow;
    const std::thread::id savedThread = detail::g_uiThreadId;
    detail::g_uiDispatcherWindow = nullptr;
    detail::g_uiThreadId = std::thread::id();

    {
        std::atomic<int> progressCalls{ 0 };
        std::atomic<bool> completed{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress progress) {
                for (int i = 0; i < 100; ++i) progress.Report((float)i / 100.0f);
                return 1;
            },
            [&](int) { completed = true; },
            {},
            [&](float, const std::wstring&) { progressCalls.fetch_add(1); });
        CHECK(task->Wait());
        CHECK(!completed.load());
        CHECK(progressCalls.load() == 0);
    }

    detail::g_uiDispatcherWindow = savedWindow;
    detail::g_uiThreadId = savedThread;
    detail::InitializeUIThread();

    {
        std::atomic<bool> callbackEntered{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress) { return 2; },
            [&](int) {
                callbackEntered = true;
                throw std::runtime_error("callback");
            });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return callbackEntered.load(); }));
        CHECK(task->GetStatus() == AsyncTaskStatus::Completed);
    }

    {
        std::atomic<bool> progressEntered{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress progress) { progress.Report(0.5f); return 3; },
            [](int) {},
            {},
            [&](float, const std::wstring&) {
                progressEntered = true;
                throw std::runtime_error("progress");
            });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return progressEntered.load(); }));
        CHECK(task->GetStatus() == AsyncTaskStatus::Completed);
    }
}

static void test_viewcore_list_example() {
    std::printf("ViewCore: scroll axis / recycle pool / fetch flow\n");

    {
        viewcore::ScrollAxis axis;
        axis.SetMetrics(400.0f, 1000.0f * 20.0f);
        axis.ScrollTo(2000.0f);
        axis.offset = axis.target;
        auto range = axis.VisibleRange(20.0f, 20.0f, 1000);
        CHECK(!range.empty());
        CHECK(range.first == 99);
        CHECK(range.last == 121);
        axis.EnsureVisible(0.0f, 20.0f);
        CHECK(axis.target == 0.0f);
        axis.SetMetrics(400.0f, 100.0f);
        CHECK(axis.maxOffset == 0.0f);
        CHECK(axis.offset == 0.0f);
    }

    {
        int created = 0, released = 0;
        viewcore::RecyclePool<Label> pool(
            [&]() { ++created; return std::make_shared<Label>(L"row"); },
            [&](Label* label) { ++released; label->SetParent(nullptr); },
            2);
        auto a = pool.Acquire();
        auto b = pool.Acquire();
        CHECK(created == 2);
        pool.Release(a);
        pool.Release(b);
        CHECK(released == 2);
        auto c = pool.Acquire();
        CHECK(created == 2);
        pool.Release(c);
        pool.Trim(0);
        CHECK(pool.IdleCount() == 0);
        CHECK(released == 3);
    }

    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }
    {
        viewcore::FetchFlow<std::vector<int>> flow;
        std::atomic<bool> ready{ false };
        flow.Start(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress) {
                return std::vector<int>{ 10, 20, 30 };
            },
            [&](std::vector<int> rows) {
                CHECK(rows.size() == 3);
                CHECK(rows[1] == 20);
                ready = true;
            });
        CHECK(flow.Wait());
        CHECK(PumpUntil([&]() { return ready.load(); }));
        CHECK(flow.GetState() == viewcore::FetchState::Ready);
    }

    {
        ListView lv;
        lv.SetItemCount(10);
        lv.AddItem(L"virtual-add");
        CHECK(lv.GetItemCount() == 11);
        CHECK(lv.GetItemText(10) == L"virtual-add");
        lv.RemoveItem(10);
        CHECK(lv.GetItemCount() == 10);

        lv.SetItemProvider(200000, [](int i) { return L"??? " + std::to_wstring(i); });
        CHECK(lv.GetItemCount() == 200000);
        CHECK(lv.GetItemText(1234) == L"??? 1234");
        detail::InitializeUIThread();
        ListView asyncList;
        asyncList.SetWidth(300.0f); asyncList.SetHeight(200.0f);
        asyncList.Arrange(Rect(0, 0, 300.0f, 200.0f));
        asyncList.SetAsyncItemProvider(1000, [](int first, int count, AsyncCancelToken, AsyncProgress) {
            ListView::AsyncRowBatch batch;
            batch.first = first;
            for (int i = first; i < first + count; ++i) {
                wchar_t buf[64]; swprintf(buf, 64, L"async %d", i); batch.rows.push_back(buf);
            }
            return batch;
        });
        asyncList.RefreshChildren();
        CHECK(PumpUntil([&]() { return asyncList.GetItemText(0) == L"async 0"; }));

        ListView asyncRich;
        asyncRich.SetWidth(300.0f); asyncRich.SetHeight(200.0f);
        asyncRich.Arrange(Rect(0, 0, 300.0f, 200.0f));
        asyncRich.SetAsyncLabelProvider(1000, [](int first, int count, AsyncCancelToken, AsyncProgress) {
            AsyncLabelBatch batch;
            batch.first = first;
            for (int i = first; i < first + count; ++i) {
                ViewLabelData d;
                LabelRun r; r.text = L"rich " + std::to_wstring(i);
                r.color = Color::FromArgb(255, 255, 0, 0);
                d.runs.push_back(r);
                d.icon = Icon::Close;
                batch.rows.push_back(std::move(d));
            }
            return batch;
        });
        asyncRich.RefreshChildren();
        CHECK(PumpUntil([&]() { return asyncRich.GetItemText(0) == L"rich 0"; }));
        asyncRich.RefreshChildren();
        bool asyncRichFound = false;
        for (UIElement* e : asyncRich.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"rich 0" && lb->HasRichText() && lb->GetIcon() == Icon::Close)
                asyncRichFound = lb->GetDesiredSize().width > 0.0f && lb->GetDesiredSize().height > 0.0f;
        }
        CHECK(asyncRichFound);

        TableView asyncTable;
        asyncTable.SetWidth(400.0f); asyncTable.SetHeight(200.0f);
        asyncTable.SetAsyncCellLabelProvider(1000, 2, [](int first, int count, AsyncCancelToken, AsyncProgress) {
            AsyncCellBatch batch;
            batch.first = first; batch.cols = 2;
            for (int r = first; r < first + count; ++r) {
                std::vector<ViewLabelData> row;
                for (int c = 0; c < 2; ++c) {
                    ViewLabelData d;
                    LabelRun run; run.text = L"cell " + std::to_wstring(r) + L"," + std::to_wstring(c);
                    d.runs.push_back(run);
                    d.icon = Icon::Close;
                    row.push_back(std::move(d));
                }
                batch.rows.push_back(std::move(row));
            }
            return batch;
        });
        asyncTable.Arrange(Rect(0, 0, 400.0f, 200.0f));
        asyncTable.RefreshChildren();
        CHECK(PumpUntil([&]() { return asyncTable.GetItemText(0, 0) == L"cell 0,0"; }));
        asyncTable.RefreshChildren();
        bool asyncCellFound = false;
        for (UIElement* e : asyncTable.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"cell 0,0" && lb->HasRichText() && lb->GetIcon() == Icon::Close)
                asyncCellFound = lb->GetDesiredSize().width > 0.0f && lb->GetDesiredSize().height > 0.0f;
        }
        CHECK(asyncCellFound);

        TreeView asyncTree;
        asyncTree.SetColumnCount(1);
        asyncTree.AddRoot(L"raw");
        asyncTree.SetAsyncNodeLabelProvider([](int first, int count, AsyncCancelToken, AsyncProgress) {
            AsyncCellBatch batch;
            batch.first = first; batch.cols = 1;
            for (int i = first; i < first + count; ++i) {
                ViewLabelData d;
                LabelRun run; run.text = L"node " + std::to_wstring(i);
                d.runs.push_back(run);
                d.icon = Icon::Close;
                batch.rows.push_back({ std::move(d) });
            }
            return batch;
        });
        asyncTree.Arrange(Rect(0, 0, 300.0f, 200.0f));
        asyncTree.RefreshChildren();
        bool asyncNodeFound = false;
        CHECK(PumpUntil([&]() {
            asyncTree.RefreshChildren();
            for (UIElement* e : asyncTree.GetChildren()) {
                auto* lb = dynamic_cast<Label*>(e);
                if (lb && lb->GetText() == L"node 0" && lb->HasRichText() && lb->GetIcon() == Icon::Close) {
                    asyncNodeFound = lb->GetDesiredSize().width > 0.0f && lb->GetDesiredSize().height > 0.0f;
                    return true;
                }
            }
            return false;
        }));
        CHECK(asyncNodeFound);

        lv.SetItemCount(1000);
        CHECK(lv.GetItemCount() == 1000);
        CHECK(lv.GetItemCount() == 1000);
    }
}

static void test_virtual_label_stress() {
    std::printf("Virtual label providers: sync / async stress\n");
    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }
    const int rounds = 24;

    {
        ListView lv;
        lv.SetWidth(360.0f); lv.SetHeight(240.0f);
        lv.Arrange(Rect(0, 0, 360.0f, 240.0f));
        lv.SetItemLabelProvider(200000, [](int i, Label& lb) {
            LabelRun a; a.text = L"row "; a.color = Color::FromArgb(255, 0, 120, 212);
            LabelRun b; b.text = std::to_wstring(i); b.color = Color::FromArgb(255, 220, 40, 40);
            lb.SetRichText({ a, b });
            lb.SetIcon((i & 1) ? Icon::Folder : Icon::Save);
        });
        for (int i = 0; i < rounds; ++i) {
            lv.ScrollToItem((i * 7919) % 200000);
            lv.UpdateAnimation(1.0f);
            lv.RefreshChildren();
            CHECK(!lv.GetChildren().empty());
        }
    }

    {
        ListView lv;
        lv.SetWidth(360.0f); lv.SetHeight(240.0f);
        lv.Arrange(Rect(0, 0, 360.0f, 240.0f));
        lv.SetAsyncLabelProvider(50000, [](int first, int count, AsyncCancelToken token, AsyncProgress) {
            AsyncLabelBatch batch;
            batch.first = first;
            for (int i = first; i < first + count; ++i) {
                if (token.IsCancellationRequested()) break;
                ViewLabelData d;
                LabelRun a; a.text = L"async "; a.color = Color::FromArgb(255, 0, 120, 212);
                LabelRun b; b.text = std::to_wstring(i);
                d.runs = { a, b };
                d.icon = (i & 1) ? Icon::Folder : Icon::Save;
                batch.rows.push_back(std::move(d));
            }
            return batch;
        });
        for (int i = 0; i < rounds; ++i) {
            const int target = (i * 997) % 50000;
            lv.ScrollToItem(target);
            lv.UpdateAnimation(1.0f);
            lv.RefreshChildren();
            CHECK(PumpUntil([&]() {
                lv.RefreshChildren();
                return lv.GetItemText(target).rfind(L"async ", 0) == 0;
            }));
        }
    }

    {
        TableView tv;
        tv.SetWidth(420.0f); tv.SetHeight(220.0f);
        tv.SetCellLabelProvider(100000, 4, [](int r, int c, Label& lb) {
            LabelRun a; a.text = L"c" + std::to_wstring(c) + L" ";
            a.color = Color::FromArgb(255, 0, 120, 212);
            LabelRun b; b.text = std::to_wstring(r);
            b.color = Color::FromArgb(255, 220, 40, 40);
            lb.SetRichText({ a, b });
            lb.SetIcon((c & 1) ? Icon::Folder : Icon::Save);
        });
        tv.Arrange(Rect(0, 0, 420.0f, 220.0f));
        for (int i = 0; i < rounds; ++i) {
            tv.ScrollToCell((i * 7919) % 100000, i & 3);
            tv.UpdateAnimation(1.0f);
            tv.RefreshChildren();
            CHECK(!tv.GetChildren().empty());
        }
    }

    {
        TableView tv;
        tv.SetWidth(420.0f); tv.SetHeight(220.0f);
        tv.SetAsyncCellLabelProvider(50000, 4, [](int first, int count, AsyncCancelToken token, AsyncProgress) {
            AsyncCellBatch batch;
            batch.first = first; batch.cols = 4;
            for (int r = first; r < first + count; ++r) {
                if (token.IsCancellationRequested()) break;
                std::vector<ViewLabelData> row;
                for (int c = 0; c < 4; ++c) {
                    ViewLabelData d;
                    LabelRun a; a.text = L"a" + std::to_wstring(c) + L" ";
                    LabelRun b; b.text = std::to_wstring(r);
                    d.runs = { a, b };
                    d.icon = (c & 1) ? Icon::Folder : Icon::Save;
                    row.push_back(std::move(d));
                }
                batch.rows.push_back(std::move(row));
            }
            return batch;
        });
        tv.Arrange(Rect(0, 0, 420.0f, 220.0f));
        for (int i = 0; i < rounds; ++i) {
            const int target = (i * 997) % 50000;
            tv.ScrollToCell(target, 0);
            tv.UpdateAnimation(1.0f);
            tv.RefreshChildren();
            CHECK(PumpUntil([&]() {
                tv.RefreshChildren();
                return tv.GetItemText(target, 0).rfind(L"a0 ", 0) == 0;
            }));
        }
    }

    {
        TreeView tv;
        tv.SetColumnCount(2);
        for (int i = 0; i < 400; ++i) tv.AddRoot(L"root " + std::to_wstring(i));
        tv.SetNodeLabelProvider([](const std::shared_ptr<TreeNode>& node, int c, Label& lb) {
            if (c == 0 && node && !node->children.empty()) lb.SetIcon(Icon::Folder);
            else lb.SetIcon(Icon::Save);
        });
        tv.SetWidth(420.0f); tv.SetHeight(220.0f);
        tv.Arrange(Rect(0, 0, 420.0f, 220.0f));
        for (int i = 0; i < rounds; ++i) {
            tv.ScrollToNode(tv.GetRootAt(i % 400));
            tv.UpdateAnimation(1.0f);
            tv.RefreshNodeLabels();
            tv.RefreshChildren();
            CHECK(!tv.GetChildren().empty());
        }
    }

    {
        TreeView tv;
        tv.SetColumnCount(1);
        for (int i = 0; i < 400; ++i) tv.AddRoot(L"root " + std::to_wstring(i));
        tv.SetAsyncNodeLabelProvider([](int first, int count, AsyncCancelToken token, AsyncProgress) {
            AsyncCellBatch batch;
            batch.first = first; batch.cols = 1;
            for (int i = first; i < first + count; ++i) {
                if (token.IsCancellationRequested()) break;
                ViewLabelData d;
                LabelRun a; a.text = L"n"; a.color = Color::FromArgb(255, 0, 120, 212);
                LabelRun b; b.text = std::to_wstring(i);
                d.runs = { a, b };
                d.icon = Icon::Save;
                batch.rows.push_back({ std::move(d) });
            }
            return batch;
        });
        tv.SetWidth(420.0f); tv.SetHeight(220.0f);
        tv.Arrange(Rect(0, 0, 420.0f, 220.0f));
        for (int i = 0; i < rounds; ++i) {
            tv.RefreshChildren();
            CHECK(PumpUntil([&]() {
                tv.RefreshChildren();
                for (UIElement* e : tv.GetChildren()) {
                    auto* lb = dynamic_cast<Label*>(e);
                    if (lb && lb->HasRichText()) return true;
                }
                return false;
            }));
        }
    }
}

static void test_async_runtime() {
    std::printf("Async runtime / cancel / progress / lifetime\n");
    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }
    const auto uiThread = std::this_thread::get_id();

    {
        std::promise<int> result;
        auto done = result.get_future();
        std::mutex orderMutex;
        std::vector<int> order;
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress progress) {
                progress.Report(0.25f, L"quarter");
                progress.Report(0.75f, L"three quarters");
                return 42;
            },
            [&](int value) {
                std::lock_guard<std::mutex> lock(orderMutex);
                order.push_back(2);
                CHECK(std::this_thread::get_id() == uiThread);
                result.set_value(value);
            },
            [](std::exception_ptr) { CHECK(false); },
            [&](float, const std::wstring&) {
                std::lock_guard<std::mutex> lock(orderMutex);
                order.push_back(1);
                CHECK(std::this_thread::get_id() == uiThread);
            });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return done.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }));
        CHECK(done.get() == 42);
        CHECK(task->GetStatus() == AsyncTaskStatus::Completed);
        std::lock_guard<std::mutex> lock(orderMutex);
        CHECK(!order.empty() && order.back() == 2);
        CHECK(std::is_sorted(order.begin(), order.end()));
    }

    {
        std::shared_ptr<int> owner = std::make_shared<int>(1);
        std::atomic<bool> progressSeen{ false };
        std::atomic<bool> completed{ false };
        auto task = RunAsync<int>(AsyncOwner(owner),
            [](AsyncCancelToken, AsyncProgress progress) {
                progress.Report(0.5f, L"owner-drop");
                return 5;
            },
            [&](int) { completed = true; },
            {},
            [&](float, const std::wstring&) {
                progressSeen = true;
                owner.reset();
            });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return progressSeen.load(); }));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        CHECK(progressSeen.load());
        CHECK(!completed.load());
    }

    {
        std::atomic<bool> called{ false };
        auto task = RunAsync<int>(
            [](AsyncCancelToken, AsyncProgress) { return 3; },
            [&](int value) { called = (value == 3); });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return called.load(); }));
        CHECK(task->GetStatus() == AsyncTaskStatus::Completed);
    }

    {
        std::latch started(1);
        std::atomic<bool> success{ false }, failed{ false }, canceled{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [&](AsyncCancelToken token, AsyncProgress) {
                started.count_down();
                const DWORD begin = GetTickCount();
                while ((GetTickCount() - begin) < 2000) {
                    if (token.IsCancellationRequested()) return 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return 7;
            },
            [&](int) { success = true; },
            [&](std::exception_ptr) { failed = true; },
            {},
            [&]() { canceled = true; });
        started.wait();
        CHECK(task->Cancel());
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return canceled.load() || failed.load() || success.load(); }));
        CHECK(canceled.load());
        CHECK(!success.load());
        CHECK(!failed.load());
        CHECK(task->GetStatus() == AsyncTaskStatus::Canceled);
    }

    {
        std::atomic<bool> failed{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress) -> int { throw std::runtime_error("boom"); },
            [](int) { CHECK(false); },
            [&](std::exception_ptr error) {
                CHECK(error != nullptr);
                failed = true;
            });
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return failed.load(); }));
        CHECK(task->GetStatus() == AsyncTaskStatus::Failed);
    }

    {
        std::latch started(1);
        std::latch release(1);
        std::atomic<bool> completed{ false };
        auto owner = std::make_shared<int>(1);
        auto task = RunAsync<int>(AsyncOwner(owner),
            [&](AsyncCancelToken, AsyncProgress) {
                started.count_down();
                release.wait();
                return 99;
            },
            [&](int) { completed = true; });
        started.wait();
        owner.reset();
        release.count_down();
        CHECK(task->Wait());
        PumpUntil([&]() { return task->GetStatus() != AsyncTaskStatus::Running; });
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        CHECK(!completed.load());
        CHECK(task->GetStatus() == AsyncTaskStatus::Completed);
    }

    {
        std::latch gate(1);
        std::mt19937 rng((unsigned)GetTickCount64() ^ (unsigned)(uintptr_t)&rng);
        std::vector<std::shared_ptr<AsyncTask<int>>> tasks;
        std::array<std::atomic<int>, 20> calls{};
        std::vector<bool> canceled(20);
        for (int i = 0; i < 20; ++i) {
            calls[i].store(0);
            canceled[i] = (rng() & 1u) != 0;
            tasks.push_back(RunAsync<int>(AsyncOwner{},
                [&, i](AsyncCancelToken token, AsyncProgress progress) {
                    gate.wait();
                    for (int step = 0; step < 20; ++step) {
                        if (token.IsCancellationRequested()) return i;
                        progress.Report((float)step / 20.0f);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return i;
                },
                [&, i](int value) { CHECK(value == i); calls[i].fetch_add(1); },
                [&](std::exception_ptr) { CHECK(false); },
                {},
                [&, i]() { calls[i].fetch_add(100); }));
        }
        for (int i = 0; i < 20; ++i) if (canceled[i]) tasks[i]->Cancel();
        gate.count_down();
        for (auto& task : tasks) CHECK(task->Wait());
        CHECK(PumpUntil([&]() {
            for (int i = 0; i < 20; ++i) {
                const int expected = canceled[i] ? 100 : 1;
                if (calls[i].load() != expected) return false;
            }
            return true;
        }));
        for (int i = 0; i < 20; ++i) {
            CHECK(tasks[i]->GetStatus() == (canceled[i] ? AsyncTaskStatus::Canceled : AsyncTaskStatus::Completed));
            CHECK(calls[i].load() == (canceled[i] ? 100 : 1));
        }
    }
}

static void test_async_randomized() {
    std::printf("Async randomized callbacks / cancellation / owners\n");
    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }

    constexpr int kTasks = 40;
    std::latch gate(1);
    std::mt19937 rng((unsigned)GetTickCount64() ^ (unsigned)(uintptr_t)&rng ^ 0xA5A5F00Du);
    std::array<std::shared_ptr<AsyncTask<int>>, kTasks> tasks;
    std::array<std::shared_ptr<int>, kTasks> owners;
    std::array<std::atomic<int>, kTasks> success{};
    std::array<std::atomic<int>, kTasks> canceled{};
    std::array<std::atomic<int>, kTasks> failed{};
    std::array<bool, kTasks> wasCanceled{};
    std::array<bool, kTasks> ownerAlive{};

    for (int i = 0; i < kTasks; ++i) {
        success[i].store(0);
        canceled[i].store(0);
        failed[i].store(0);
        wasCanceled[i] = (rng() & 1u) != 0;
        ownerAlive[i] = (rng() % 4u) != 0;
        owners[i] = std::make_shared<int>(i);
        tasks[i] = RunAsync<int>(AsyncOwner(owners[i]),
            [&, i](AsyncCancelToken token, AsyncProgress progress) {
                gate.wait();
                for (int step = 0; step < 8; ++step) {
                    if (token.IsCancellationRequested()) return i;
                    progress.Report((float)step / 8.0f, std::to_wstring(i));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return i;
            },
            [&, i](int value) { CHECK(value == i); success[i].fetch_add(1); },
            [&, i](std::exception_ptr) { failed[i].fetch_add(1); CHECK(false); },
            {},
            [&, i]() { canceled[i].fetch_add(1); });
        if (wasCanceled[i]) tasks[i]->Cancel();
        if (!ownerAlive[i]) owners[i].reset();
    }
    gate.count_down();
    for (auto& task : tasks) CHECK(task->Wait());

    CHECK(PumpUntil([&]() {
        for (int i = 0; i < kTasks; ++i) {
            const int expectedSuccess = (!wasCanceled[i] && ownerAlive[i]) ? 1 : 0;
            const int expectedCanceled = (wasCanceled[i] && ownerAlive[i]) ? 1 : 0;
            if (success[i].load() != expectedSuccess) return false;
            if (canceled[i].load() != expectedCanceled) return false;
            if (failed[i].load() != 0) return false;
        }
        return true;
    }));
    for (int i = 0; i < kTasks; ++i) {
        CHECK(tasks[i]->GetStatus() == (wasCanceled[i] ? AsyncTaskStatus::Canceled : AsyncTaskStatus::Completed));
        CHECK(success[i].load() == ((!wasCanceled[i] && ownerAlive[i]) ? 1 : 0));
        CHECK(canceled[i].load() == ((wasCanceled[i] && ownerAlive[i]) ? 1 : 0));
        CHECK(failed[i].load() == 0);
    }
}

static void test_async_stress() {
    std::printf("Async stress: 1000 randomized tasks / 20 waves\n");
    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }

    constexpr int kBatch = 50;
    constexpr int kWaves = 20;
    std::mt19937 rng((unsigned)GetTickCount64() ^ (unsigned)(uintptr_t)&rng ^ 0xC0FFEEu);
    for (int wave = 0; wave < kWaves; ++wave) {
        std::latch gate(1);
        std::array<std::shared_ptr<AsyncTask<int>>, kBatch> tasks;
        std::array<std::shared_ptr<int>, kBatch> owners;
        std::array<std::atomic<int>, kBatch> success{};
        std::array<std::atomic<int>, kBatch> canceled{};
        std::array<std::atomic<int>, kBatch> failed{};
        std::array<bool, kBatch> cancelIt{};
        std::array<bool, kBatch> throwIt{};
        std::array<bool, kBatch> ownerAlive{};
        std::array<int, kBatch> steps{};

        for (int i = 0; i < kBatch; ++i) {
            success[i].store(0);
            canceled[i].store(0);
            failed[i].store(0);
            cancelIt[i] = (rng() % 4u) == 0;
            throwIt[i] = (rng() % 7u) == 0;
            ownerAlive[i] = (rng() % 5u) != 0;
            steps[i] = 1 + (int)(rng() % 8u);
            owners[i] = std::make_shared<int>(wave * kBatch + i);
            tasks[i] = RunAsync<int>(AsyncOwner(owners[i]),
                [&, i](AsyncCancelToken token, AsyncProgress progress) {
                    gate.wait();
                    if (throwIt[i]) throw std::runtime_error("stress");
                    for (int step = 0; step < steps[i]; ++step) {
                        if (token.IsCancellationRequested()) return i;
                        progress.Report((float)step / (float)steps[i], std::to_wstring(i));
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return i;
                },
                [&, i](int value) { CHECK(value == i); success[i].fetch_add(1); },
                [&, i](std::exception_ptr) { failed[i].fetch_add(1); },
                {},
                [&, i]() { canceled[i].fetch_add(1); });
            if (cancelIt[i]) tasks[i]->Cancel();
            if (!ownerAlive[i]) owners[i].reset();
        }
        gate.count_down();
        for (auto& task : tasks) CHECK(task->Wait());
        CHECK(PumpUntil([&]() {
            for (int i = 0; i < kBatch; ++i) {
                const int expectedSuccess = (!cancelIt[i] && !throwIt[i] && ownerAlive[i]) ? 1 : 0;
                const int expectedCanceled = (cancelIt[i] && ownerAlive[i]) ? 1 : 0;
                const int expectedFailed = (!cancelIt[i] && throwIt[i] && ownerAlive[i]) ? 1 : 0;
                if (success[i].load() != expectedSuccess) return false;
                if (canceled[i].load() != expectedCanceled) return false;
                if (failed[i].load() != expectedFailed) return false;
            }
            return true;
        }));
        for (int i = 0; i < kBatch; ++i) {
            const AsyncTaskStatus expected = cancelIt[i] ? AsyncTaskStatus::Canceled
                : (throwIt[i] ? AsyncTaskStatus::Failed : AsyncTaskStatus::Completed);
            CHECK(tasks[i]->GetStatus() == expected);
        }
    }
}

static void test_async_cancel_race() {
    std::printf("Async cancel/completion race (1000)\n");
    detail::InitializeUIThread();
    if (!detail::g_uiDispatcherWindow) {
        std::printf("  [skip] UI dispatcher unavailable\n");
        return;
    }
    int races = 0;
    for (int i = 0; i < 1000; ++i) {
        std::atomic<bool> success{ false };
        std::atomic<bool> canceled{ false };
        auto task = RunAsync<int>(AsyncOwner{},
            [](AsyncCancelToken, AsyncProgress) { return 1; },
            [&](int) { success = true; },
            [](std::exception_ptr) { CHECK(false); },
            {},
            [&]() { canceled = true; });
        const bool requested = task->Cancel();
        CHECK(task->Wait());
        CHECK(PumpUntil([&]() { return success.load() || canceled.load(); }));
        if (requested && success.load()) ++races;
        CHECK(!(requested && success.load()));
        CHECK(success.load() != canceled.load());
    }
    CHECK(races == 0);
}

static void test_animation_primitives() {
    FloatAnimation to;
    to.Snap(0.0f);
    CHECK(to.To(10.0f, 1.0f, Easing::EaseOut));
    CHECK(to.Active());
    CHECK(to.Update(0.5f));
    CHECK(to.Value() > 0.0f && to.Value() < 10.0f);
    CHECK(to.Update(0.5f));                  // final frame still reports moved
    CHECK(to.Value() == 10.0f);
    CHECK(!to.Active());
    CHECK(!to.Update(0.016f));

    FloatAnimation same;
    same.Snap(0.0f);
    same.To(1.0f, 1.0f);
    same.Update(0.2f);
    CHECK(!same.To(1.0f, 1.0f));             // same target while active -> no restart

    FloatAnimation spring;
    spring.Snap(0.0f);
    spring.Spring(1.0f, 12.0f, 0.001f);
    float prev = 0.0f;
    bool monotonic = true;
    for (int i = 0; i < 300 && spring.Active(); ++i) {
        spring.Update(1.0f / 60.0f);
        if (spring.Value() < prev) monotonic = false;
        prev = spring.Value();
    }
    CHECK(!spring.Active());
    CHECK(spring.Value() == 1.0f);
    CHECK(monotonic);

    FloatAnimation retarget;
    retarget.Snap(0.0f);
    retarget.Spring(1.0f, 12.0f, 0.001f);
    for (int i = 0; i < 5; ++i) retarget.Update(1.0f / 60.0f);
    float before = retarget.Value();
    retarget.Spring(0.0f, 12.0f, 0.001f);
    retarget.Update(1.0f / 60.0f);
    CHECK(retarget.Value() <= before + 1e-5f);
    CHECK(retarget.Value() >= 0.0f);

    ColorAnimation color;
    color.Snap(Color(0, 0, 0, 1));
    CHECK(color.To(Color(1, 1, 1, 1), 0.1f, Easing::Linear));
    color.Update(0.05f);
    CHECK(color.Value().r > 0.4f && color.Value().r < 0.6f);
    color.Update(0.05f);
    CHECK(color.Value().r == 1.0f);

    AnimationGroup group;
    FloatAnimation a, b;
    group.Add(&a); group.Add(&b); group.Add(&a);
    CHECK(group.Size() == 2);
    a.To(1.0f, 1.0f);
    b.Spring(1.0f);
    CHECK(group.Active());
    CHECK(group.Update(0.5f));
    group.Update(2.0f);
    CHECK(!group.Active());
}

static void test_drawer() {
    std::printf("Drawer 抽屉\n");

    {
        Drawer d;
        auto content = std::make_shared<Label>(L"content");
        auto panel = std::make_shared<Label>(L"panel");
        d.SetContent(content);
        d.SetPanel(panel);
        d.SetPanelSize(200.0f);
        d.Arrange(Rect(0, 0, 800, 600));
        CHECK(content->GetArrangedRect().width == 800.0f);

        int opened = 0, closed = 0, changes = 0;
        d.Opened.connect([&]() { ++opened; });
        d.Closed.connect([&]() { ++closed; });
        d.OpenChanged.connect([&](bool) { ++changes; });

        d.Open();
        CHECK(d.IsOpen());
        CHECK(d.IsAnimating());
        CHECK(changes == 1);
        CHECK(opened == 0);
        d.UpdateAnimation(1.0f);
        CHECK(d.GetOpenProgress() == 1.0f);
        CHECK(!d.IsAnimating());
        CHECK(opened == 1);
        CHECK(content->GetArrangedRect().width == 800.0f);   // Overlay keeps content full
        CHECK(panel->GetArrangedRect().width == 168.0f);     // default 16px panel padding
        CHECK(panel->GetArrangedRect().x == 16.0f);

        d.Close();
        CHECK(!d.IsOpen());
        d.UpdateAnimation(1.0f);
        CHECK(closed == 1);
        CHECK(content->GetArrangedRect().width == 800.0f);
        CHECK(changes == 2);
    }

    {
        Drawer d;
        auto content = std::make_shared<Label>(L"content");
        auto panel = std::make_shared<Label>(L"panel");
        d.SetContent(content);
        d.SetPanel(panel);
        d.SetMode(DrawerMode::Push);
        d.SetPanelSize(200.0f);
        d.Arrange(Rect(0, 0, 800, 600));
        d.SetOpen(true, false);
        CHECK(content->GetArrangedRect().x == 200.0f);        // left Push shifts content right
        CHECK(content->GetArrangedRect().width == 600.0f);
        CHECK(panel->GetArrangedRect().width == 168.0f);

        d.SetSide(DrawerSide::Right);
        CHECK(content->GetArrangedRect().x == 0.0f);
        CHECK(panel->GetArrangedRect().x == 616.0f);
        d.SetSide(DrawerSide::Top);
        CHECK(content->GetArrangedRect().y == 200.0f);        // top Push shifts content down
        CHECK(content->GetArrangedRect().height == 400.0f);
        CHECK(panel->GetArrangedRect().height == 168.0f);
        d.SetSide(DrawerSide::Bottom);
        CHECK(content->GetArrangedRect().y == 0.0f);
        CHECK(content->GetArrangedRect().height == 400.0f);
        CHECK(panel->GetArrangedRect().height == 168.0f);
    }

    {
        Drawer d;
        auto content = std::make_shared<ColumnBox>();
        auto child = std::make_shared<Label>(L"child");
        content->AddChild(child);
        d.SetContent(content);
        d.SetMode(DrawerMode::Push);
        d.SetPanelSize(200.0f);
        d.Measure(Size(800, 600));
        d.Arrange(Rect(0, 0, 800, 600));
        d.SetOpen(true, false);
        d.Measure(Size(800, 600));
        d.Arrange(Rect(0, 0, 800, 600));

        d.Close();
        d.UpdateAnimation(1.0f);
        CHECK(content->NeedsLayout());
        CHECK(child->NeedsLayout());
        d.Measure(Size(800, 600));
        d.Arrange(Rect(0, 0, 800, 600));
        CHECK(child->GetArrangedRect().width == 800.0f);
    }

    {
        Drawer d;
        auto content = std::make_shared<Label>(L"content");
        d.SetContent(content);
        d.SetMode(DrawerMode::Permanent);
        d.SetPanelSize(200.0f);
        d.Arrange(Rect(0, 0, 800, 600));
        CHECK(d.IsOpen());
        CHECK(d.GetOpenProgress() == 1.0f);
        CHECK(content->GetArrangedRect().x == 200.0f);
        CHECK(content->GetArrangedRect().width == 600.0f);    // permanent reserves panel width
        d.SetSide(DrawerSide::Left);
        d.SetPanel(std::make_shared<Label>(L"side"));
        d.Arrange(Rect(0, 0, 800, 600));
        CHECK(content->GetArrangedRect().width == 600.0f);
        d.Close();
        CHECK(d.IsOpen());
    }

    {
        Drawer d;
        auto content = std::make_shared<Label>(L"content");
        auto panel = std::make_shared<Label>(L"panel");
        d.SetContent(content);
        d.SetPanel(panel);
        d.SetPanelSize(200.0f);
        d.Arrange(Rect(0, 0, 800, 600));
        d.Open();
        d.UpdateAnimation(1.0f);
        UIElement* scrim = d.HitTest(500.0f, 100.0f);
        CHECK(scrim != nullptr);
        CHECK(scrim != content.get());
        scrim->OnMouseDown(500.0f, 100.0f);
        CHECK(!d.IsOpen());
        d.UpdateAnimation(1.0f);

        d.SetModal(false);
        d.Open();
        d.UpdateAnimation(1.0f);
        CHECK(d.HitTest(500.0f, 100.0f) == content.get());
    }

    {
        auto drawer = d::ZDrawer(d::ZLbl(L"content"), d::ZLbl(L"panel"));
        CHECK(drawer->GetContent() != nullptr);
        CHECK(drawer->GetPanel() != nullptr);
    }
}

static void test_button_animations() {
    Button b;
    b.OnMouseEnter();
    b.UpdateAnimation(0.2f);
    CHECK(b.DebugAnimationProgress() == 1.0f);
    b.OnMouseLeave();
    b.UpdateAnimation(0.2f);
    CHECK(b.DebugAnimationProgress() == 0.0f);
    CHECK(!b.HasActiveAnimation());
}

static void test_backdrop_defaults() {
    Window w;
    w.SetBackdrop(Backdrop::Acrylic);
    CHECK(((w.GetBackdropColor() >> 24) & 0xFF) == 0);
    w.SetBackdrop(Backdrop::Mica);
    CHECK(((w.GetBackdropColor() >> 24) & 0xFF) == 0);
    w.SetBackdrop(Backdrop::None);
    CHECK(((w.GetBackdropColor() >> 24) & 0xFF) == 0xFF);
    w.SetBackdrop(Backdrop::Acrylic, 0x80FFFFFF);
    CHECK(((w.GetBackdropColor() >> 24) & 0xFF) == 0x80);
    w.SetBackdrop(Backdrop::Mica);
    CHECK(((w.GetBackdropColor() >> 24) & 0xFF) == 0x80);
}

struct SignalProbe : Label {
    int total = 0;
    SignalProbe() : Label(L"signal-probe") {}
    void Add(int v) { total += v; }
};

static void test_signal_lifetime() {
    std::printf("信号生命周期 / ConnectWeak\n");

    static_assert(!std::is_copy_constructible_v<ZSignal<>>, "ZSignal must not be copyable");
    static_assert(!std::is_move_constructible_v<ZSignal<>>, "ZSignal must not be movable");
    static_assert(std::is_base_of_v<ConnectionOwner, Button>, "Button must be a ConnectionOwner");
    static_assert(std::is_base_of_v<ConnectionOwner, Window>, "Window must be a ConnectionOwner");

    {
        ZSignal<> sig;
        int calls = 0;
        auto b = std::make_shared<Button>();
        b->Connect(sig, [&calls]() { ++calls; });
        sig.Fire();
        CHECK(calls == 1);
        b.reset();
        sig.Fire();
        CHECK(calls == 1);
    }

    {
        ZSignal<int> sig;
        auto p = std::make_shared<SignalProbe>();
        auto weakConn = ConnectWeak(p, sig, &SignalProbe::Add);
        sig.Fire(2);
        CHECK(p->total == 2);

        auto freeConn = Connect(p.get(), sig, &SignalProbe::Add);
        sig.Fire(3);
        CHECK(p->total == 8);
        freeConn.disconnect();
        CHECK(!freeConn.isConnected());
        sig.Fire(4);
        CHECK(p->total == 12);

        weakConn.disconnect();
        CHECK(!weakConn.isConnected());
        sig.Fire(5);
        CHECK(p->total == 12);

        auto weakRef = ConnectWeak(p, sig, [](SignalProbe& self, int v) { self.total += v; });
        sig.Fire(6);
        CHECK(p->total == 18);
        auto moved = std::move(weakRef);
        CHECK(!weakRef.isConnected());
        CHECK(moved.isConnected());
        moved.disconnect();
        sig.Fire(7);
        CHECK(p->total == 18);

        p.reset();
        sig.Fire(8);
        CHECK(!p);
    }

    {
        ZSignal<int> sig;
        auto p = std::make_shared<SignalProbe>();
        int observed = 0;
        auto conn = Connect(p.get(), sig, [&observed](int v) { observed += v; });
        sig.Fire(2);
        CHECK(observed == 2);
        auto copy = conn;
        conn.disconnect();
        CHECK(!copy.isConnected());
        sig.Fire(3);
        CHECK(observed == 2);
        p.reset();
    }

    {
        Connection dangling;
        {
            ZSignal<int> sig;
            dangling = sig.connect([](int) {});
            CHECK(dangling.isConnected());
        }
        CHECK(!dangling.isConnected());
        dangling.disconnect();
    }

    {
        Window w;
        ZSignal<> sig;
        int calls = 0;
        w.Connect(sig, [&calls]() { ++calls; });
        sig.Fire();
        CHECK(calls == 1);
    }

    {
        auto btn = d::ZBtn(L"dsl-signal");
        int calls = 0;
        btn.on(btn->Clicked, [&calls]() { ++calls; });
        btn->Clicked.Fire();
        CHECK(calls == 1);
    }

    {
        auto app = d::ZApp();
        int closed = 0;
        app.on<&Window::Closed>([&closed]() { ++closed; });
        app.window().Closed.Fire();
        CHECK(closed == 1);
    }
}

static void test_signal_reentrancy_and_exception() {
    std::printf("信号重入 / 槽异常\n");

    {
        ZSignal<int> sig;
        std::vector<int> order;
        sig.connect([&](int v) {
            order.push_back(v);
            if (v == 1) sig.Fire(2);
        });
        sig.connect([&](int v) {
            order.push_back(v * 10);
        });
        sig.Fire(1);
        CHECK(order.size() == 4);
        CHECK(order[0] == 1);
        CHECK(order[1] == 2);
        CHECK(order[2] == 20);
        CHECK(order[3] == 10);
    }

    {
        ZSignal<> sig;
        int after = 0;
        bool caught = false;
        auto throwing = sig.connect([]() { throw std::runtime_error("slot"); });
        sig.connect([&]() { ++after; });
        try {
            sig.Fire();
        } catch (const std::runtime_error&) {
            caught = true;
        }
        CHECK(caught);
        CHECK(after == 0);                 // exception stops later slots
        throwing.disconnect();
        sig.Fire();                        // depth guard restored
        CHECK(after == 1);
    }
}

static void test_signal_threading() {
    std::printf("信号线程 / 取消 / 弱引用保活\n");

    {
        ZSignal<int> sig;
        auto p = std::make_shared<SignalProbe>();
        std::promise<int> got;
        auto future = got.get_future();
        ConnectWeak(p, sig, [&](SignalProbe*, int v) {
            got.set_value(v);
        }, ConnectionThread::NewThread);
        sig.Fire(5);
        CHECK(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        CHECK(future.get() == 5);

        p.reset();
        sig.Fire(6);   // weak receiver gone; detached task must no-op
    }

    {
        ZSignal<int> sig;
        auto p = std::make_shared<SignalProbe>();
        std::latch started(1);
        std::latch release(1);
        std::promise<void> finished;
        auto done = finished.get_future();
        ConnectWeak(p, sig, [&](SignalProbe*, int) {
            started.count_down();
            release.wait();
            finished.set_value();
        }, ConnectionThread::NewThread);
        sig.Fire(1);
        started.wait();

        std::weak_ptr<SignalProbe> weak = p;
        p.reset();
        CHECK(!weak.expired());            // slot holds a shared_ptr while running
        release.count_down();
        CHECK(done.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        for (int i = 0; i < 200 && !weak.expired(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CHECK(weak.expired());
    }

    {
        detail::InitializeUIThread();
        if (!detail::g_uiDispatcherWindow) {
            std::printf("  [skip] UI dispatcher unavailable\n");
            return;
        }

        ZSignal<> sig;
        auto p = std::make_shared<Button>();
        int calls = 0;
        auto conn = p->Connect(sig, [&]() { ++calls; }, ConnectionThread::UIThread);

        std::promise<void> posted;
        auto postedFuture = posted.get_future();
        std::thread worker([&]() {
            sig.Fire();
            posted.set_value();
        });
        postedFuture.wait();
        worker.join();

        conn.disconnect();
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        CHECK(calls == 0);                 // queued task sees alive == false
    }
}

static void test_button_and_listview_regressions() {
    Button b(L"x");
    b.RefreshChildren();
    CHECK(b.GetChildren().size() == 1);
    CHECK(b.GetChildren()[0] == b.GetLabel().get());

    auto inlineChild = std::make_shared<Label>(L"inline");
    b.GetLabel()->AddChild(inlineChild);
    b.GetLabel()->RefreshChildren();
    CHECK(b.GetLabel()->GetChildren().size() == 1);
    CHECK(b.GetLabel()->GetChildren()[0] == inlineChild.get());

    auto content = std::make_shared<Label>(L"content");
    b.SetContent(content);
    b.RefreshChildren();
    CHECK(b.GetChildren().size() == 1);
    CHECK(b.GetChildren()[0] == content.get());

    b.ClearContent();
    b.RefreshChildren();
    CHECK(b.GetChildren().size() == 1);
    CHECK(b.GetChildren()[0] == b.GetLabel().get());

    b.SetDropWriteBack(true);
    CHECK(b.GetDropWriteBack());
    b.SetDropOpenOnClick(true);
    CHECK(b.GetDropOpenOnClick());
    b.SetDropItems({ L"one", L"two" }, 1);
    CHECK(b.GetDropSelectedIndex() == 1);
    CHECK(b.GetText() == L"two");
    CHECK(b.GetDropSelectedText() == L"two");

    b.SetDropWriteBack(false);
    b.SetDropOpenOnClick(false);
    CHECK(!b.GetDropWriteBack());
    CHECK(!b.GetDropOpenOnClick());
    b.SetDropItems({ L"alpha", L"beta" }, 0);
    CHECK(b.GetDropSelectedIndex() == 0);
    CHECK(b.GetText() == L"two");   // no write-back
    CHECK(b.GetDropSelectedText() == L"alpha");
    b.SetDropWriteBack(true);
    CHECK(b.GetText() == L"alpha");

    Button split;
    split.Arrange(Rect(0, 0, 120, 36));
    split.SetDropItems({ L"A", L"B" });
    int clicks = 0;
    split.Clicked.connect([&clicks]() { ++clicks; });
    split.OnMouseDown(10, 18);
    split.OnMouseUp(10, 18);
    CHECK(clicks == 1);                       // main button -> Clicked
    split.OnMouseDown(110, 18);
    split.OnMouseUp(110, 18);
    CHECK(clicks == 1);                       // expand arrow -> no Clicked
    split.SetDropOpenOnClick(false);
    split.OnMouseDown(10, 18);
    split.OnMouseUp(10, 18);
    CHECK(clicks == 2);                       // main button still Clicked
    split.OnMouseDown(110, 18);
    split.OnMouseUp(110, 18);
    CHECK(clicks == 2);                       // arrow never Clicked

    ListView lv;
    lv.SetItemHeight(0.0f);
    CHECK(lv.GetItemHeight() >= 1.0f);
    lv.SetItemHeight(-5.0f);
    CHECK(lv.GetItemHeight() >= 1.0f);

    SettingsList settings;
    CHECK(settings.Measure(Size(FLT_MAX, FLT_MAX)).width >= SettingsList::DefaultWidth);
}

// ---------------------------------------------------------------------------
// 布局 RFC：显式轨道 / 对齐 / justify / 不重测
// ---------------------------------------------------------------------------
class CountingElement : public UIElement {
public:
    int measures = 0;
    Size MeasureOverride(const Size&) override { ++measures; return Size(20.0f, 20.0f); }
    void Draw(ID2D1RenderTarget*) override {}
};

class MeasureProbe : public UIElement {
public:
    float lastWidth = -1.0f;
    float lastHeight = -1.0f;
    Size MeasureOverride(const Size& availableSize) override {
        lastWidth = availableSize.width;
        lastHeight = availableSize.height;
        return Size(10.0f, 10.0f);
    }
    void Draw(ID2D1RenderTarget*) override {}
};

class ResponsiveProbe : public UIElement {
public:
    Size MeasureOverride(const Size& availableSize) override {
        float h = (availableSize.height != FLT_MAX && availableSize.height > 0.0f)
            ? min(availableSize.height, 200.0f) : 200.0f;
        return Size(20.0f, h);
    }
    void Draw(ID2D1RenderTarget*) override {}
};

static void test_api_feedback() {
    std::printf("API feedback: self-delete / incremental / ids / Card\n");

    {
        auto button = std::make_shared<Button>(L"self-delete");
        Button* raw = button.get();
        button->Clicked.connect([&button]() { button.reset(); });
        raw->OnMouseDown(1.0f, 1.0f);
        raw->OnMouseUp(1.0f, 1.0f);
        CHECK(button == nullptr);
    }

    {
        ColumnBox col;
        auto a = std::make_shared<Label>(L"a");
        auto b = std::make_shared<Label>(L"b");
        col.AddChild(a);
        col.AddChild(b);
        col.RemoveChild(a);
        CHECK(a->GetParent() == nullptr);
        CHECK(b->GetParent() == &col);
        col.InsertChild(0, a);
        CHECK(a->GetParent() == &col);
        col.RemoveChildAt(0);
        CHECK(a->GetParent() == nullptr);
    }

    {
        TableView tv;
        tv.BeginUpdate();
        tv.SetRowCount(4);
        tv.SetColumnCount(2);
        tv.SetItem(1, 1, L"updated");
        tv.EndUpdate();
        CHECK(tv.GetRowCount() == 4);
        CHECK(tv.GetItemText(1, 1) == L"updated");
    }

    {
        TabView tv;
        int first = tv.AddTab(L"first", nullptr, true, 101);
        int second = tv.AddTab(L"second", nullptr, true, 202);
        CHECK(first == 0 && second == 1);
        CHECK(tv.FindTabById(202) == 1);
        CHECK(tv.GetTabId(0) == 101);
        CHECK(tv.RemoveTabById(101));
        CHECK(tv.GetTabCount() == 1);
        CHECK(tv.FindTabById(101) == -1);
    }

    {
        Card card;
        auto action = std::make_shared<Button>(L"action");
        card.SetContent(L"Title", L"Description", action);
        CHECK(card.GetContent() != nullptr);
    }

    {
        ListView lv;
        lv.SetItemCount(5);
        lv.SetCheckable(true);
        lv.SetMarqueeCheckSync(true);
        lv.SetSelectionMode(ListView::SelectionMode::Multi);
        lv.SetSelectedIndex(1);
        CHECK(lv.GetCheckedIndices() == std::vector<int>{ 1 });
        lv.SetSelectedIndex(2);
        CHECK(lv.GetCheckedIndices() == std::vector<int>{ 2 });
        lv.SelectAll();
        CHECK(lv.GetCheckedIndices().size() == 5);
        lv.ClearSelection();
        CHECK(lv.GetCheckedIndices().empty());
    }

    {
        TableView tv;
        tv.SetRowCount(5);
        tv.SetColumnCount(2);
        tv.SetCheckable(true);
        tv.SetMarqueeCheckSync(true);
        tv.SetSelectionMode(TableView::SelectionMode::Row);
        tv.SelectRow(1);
        CHECK(tv.GetCheckedRows() == std::vector<int>{ 1 });
        tv.SelectAll();
        CHECK(tv.GetCheckedRows().size() == 5);
        tv.ClearSelection();
        CHECK(tv.GetCheckedRows().empty());
    }

    {
        TreeView tv;
        tv.AddRoot(L"a");
        auto b = tv.AddRoot(L"b");
        tv.SetCheckable(true);
        tv.SetMarqueeCheckSync(true);
        tv.SetSelectedNode(b);
        CHECK(tv.GetNodeCheckState(b) == TreeNode::CheckState::Checked);
        tv.ClearSelection();
        CHECK(tv.GetNodeCheckState(b) == TreeNode::CheckState::Unchecked);
    }
}

static void test_layout_rfc() {
    std::printf("布局 RFC：轨道 / 对齐 / justify\n");

    // 3x3 只放中间：空轨道仍存在，中间元素居中
    {
        GridLayout grid;
        grid.SetFillWidth(true);
        grid.SetFillHeight(true);
        grid.SetColumnStretch(0, 1.0f);
        grid.SetColumnStretch(2, 1.0f);
        grid.SetRowStretch(0, 1.0f);
        grid.SetRowStretch(2, 1.0f);
        CHECK(grid.GetColumnCount() == 3);
        CHECK(grid.GetRowCount() == 3);

        auto center = std::make_shared<ColumnBox>();
        center->SetWidth(100.0f);
        center->SetHeight(50.0f);
        grid.AddChild(center, 1, 1);
        grid.Measure(Size(300.0f, 200.0f));
        grid.Arrange(Rect(0, 0, 300.0f, 200.0f));
        CHECK(fabs(center->GetArrangedRect().x - 100.0f) < 0.01f);
        CHECK(fabs(center->GetArrangedRect().y - 75.0f) < 0.01f);
    }

    // 1x1 + 内容对齐
    {
        GridLayout grid;
        grid.SetFillWidth(true);
        grid.SetFillHeight(true);
        grid.SetHorizontalAlignment(GridLayout::Alignment::Center);
        grid.SetVerticalAlignment(GridLayout::Alignment::Center);
        auto child = std::make_shared<ColumnBox>();
        child->SetWidth(100.0f);
        child->SetHeight(50.0f);
        child->SetHorizontalStretchWeight(0.0f);
        child->SetVerticalStretchWeight(0.0f);
        grid.AddChild(child, 0, 0);
        grid.Measure(Size(300.0f, 200.0f));
        grid.Arrange(Rect(0, 0, 300.0f, 200.0f));
        CHECK(fabs(child->GetArrangedRect().x - 100.0f) < 0.01f);
        CHECK(fabs(child->GetArrangedRect().y - 75.0f) < 0.01f);
    }

    // Grid cell 内对齐：ProgressRing 之类不被强制拉满
    {
        GridLayout grid;
        grid.SetFillWidth(true);
        grid.SetFillHeight(true);
        grid.SetColumnStretch(0, 1.0f);
        grid.SetRowStretch(0, 1.0f);
        auto child = std::make_shared<ColumnBox>();
        child->SetWidth(100.0f);
        child->SetHeight(50.0f);
        grid.AddChild(child, 0, 0);
        grid.SetCellAlignment(0, 0, GridLayout::Alignment::Center, GridLayout::Alignment::Center);
        grid.Measure(Size(300.0f, 200.0f));
        grid.Arrange(Rect(0, 0, 300.0f, 200.0f));
        CHECK(fabs(child->GetArrangedRect().x - 100.0f) < 0.01f);
        CHECK(fabs(child->GetArrangedRect().y - 75.0f) < 0.01f);
    }

    // Arrange 不再触发子元素 Measure（含 cell 内对齐）
    {
        GridLayout grid;
        grid.SetColumnStretch(0, 1.0f);
        grid.SetRowStretch(0, 1.0f);
        auto child = std::make_shared<CountingElement>();
        grid.AddChild(child, 0, 0);
        grid.SetCellAlignment(0, 0, GridLayout::Alignment::Center, GridLayout::Alignment::Center);
        grid.Measure(Size(100.0f, 100.0f));
        const int before = child->measures;
        grid.Arrange(Rect(0, 0, 100.0f, 100.0f));
        CHECK(child->measures == before);
        CHECK(fabs(child->GetArrangedRect().x - 40.0f) < 0.01f);
        CHECK(fabs(child->GetArrangedRect().y - 40.0f) < 0.01f);
    }

    // Grid cell 约束下 wrap 文本按 cell 宽度折行
    {
        GridLayout grid;
        grid.SetColumnStretch(0, 1.0f);
        auto label = std::make_shared<Label>(L"a very long line that must wrap inside the cell");
        label->SetTextOverflow(Label::TextOverflow::Wrap);
        grid.AddChild(label, 0, 0);
        grid.Measure(Size(100.0f, 200.0f));
        CHECK(label->GetDesiredSize().height > 20.0f);
    }

    // FillWidth 的数据控件放在等宽 Grid 里，不能把整张 Grid 撑到两倍宽
    {
        GridLayout grid;
        grid.SetFillWidth(true);
        grid.SetColumnCount(2);
        grid.SetColumnStretch(0, 1.0f);
        grid.SetColumnStretch(1, 1.0f);
        auto left = std::make_shared<TableView>();
        left->SetFillWidth(true);
        left->SetHeight(120.0f);
        auto right = std::make_shared<TreeView>();
        right->SetFillWidth(true);
        right->SetHeight(120.0f);
        grid.AddChild(left, 0, 0);
        grid.AddChild(right, 0, 1);
        grid.Measure(Size(400.0f, 300.0f));
        CHECK(grid.GetDesiredSize().width <= 400.01f);
        CHECK(left->GetDesiredSize().width <= 200.01f);
        CHECK(right->GetDesiredSize().width <= 200.01f);
    }

    // 4×4 + 跨列的数据控件混排：Grid 必须按 656 的上限收敛，而不是让数据控件默认宽撑开
    {
        GridLayout grid;
        grid.SetFillWidth(true);
        grid.SetColumnCount(4);
        grid.SetRowCount(4);
        for (int c = 0; c < 4; ++c) grid.SetColumnStretch(c, 1.0f);
        for (int r = 0; r < 4; ++r) grid.SetRowStretch(r, 1.0f);
        auto table = std::make_shared<TableView>();
        table->SetFillWidth(true);
        table->SetHeight(120.0f);
        auto tree = std::make_shared<TreeView>();
        tree->SetFillWidth(true);
        tree->SetHeight(120.0f);
        auto bar = std::make_shared<BarChart>();
        bar->SetWidth(180.0f);
        bar->SetHeight(100.0f);
        grid.AddChild(bar, 0, 0, 1, 2);
        grid.AddChild(table, 1, 0, 1, 3);
        grid.AddChild(tree, 1, 3, 2, 1);
        grid.Measure(Size(656.0f, 380.0f));
        CHECK(grid.GetDesiredSize().width <= 656.01f);
    }

    // RowBox 主轴 justify + 交叉轴对齐
    {
        RowBox row;
        auto child = std::make_shared<ColumnBox>();
        child->SetWidth(100.0f);
        child->SetHeight(50.0f);
        row.AddChild(child);
        row.SetJustify(Justify::End);
        row.SetCrossAlign(LayoutAlignment::Center);
        row.Measure(Size(300.0f, 100.0f));
        row.Arrange(Rect(0, 0, 300.0f, 100.0f));
        CHECK(fabs(child->GetArrangedRect().x - 200.0f) < 0.01f);
        CHECK(fabs(child->GetArrangedRect().y - 25.0f) < 0.01f);
    }

    // ColumnBox 交叉轴对齐
    {
        ColumnBox col;
        auto child = std::make_shared<ColumnBox>();
        child->SetWidth(100.0f);
        child->SetHeight(50.0f);
        col.AddChild(child);
        col.SetCrossAlign(LayoutAlignment::Center);
        col.Measure(Size(300.0f, 100.0f));
        col.Arrange(Rect(0, 0, 300.0f, 100.0f));
        CHECK(fabs(child->GetArrangedRect().x - 100.0f) < 0.01f);
    }

    // Page 内容居中
    {
        Page page;
        page.SetPadding(10.0f);
        page.SetContentAlignment(LayoutAlignment::Center, LayoutAlignment::Center);
        auto child = std::make_shared<ColumnBox>();
        child->SetWidth(100.0f);
        child->SetHeight(50.0f);
        page.SetContent(child);
        page.Measure(Size(300.0f, 200.0f));
        page.Arrange(Rect(0, 0, 300.0f, 200.0f));
        CHECK(fabs(child->GetArrangedRect().x - 100.0f) < 0.01f);
        CHECK(fabs(child->GetArrangedRect().y - 75.0f) < 0.01f);
    }

    // Label 不再默认显示 "Label"
    {
        Label label;
        CHECK(label.GetText().empty());
    }

    // Label 的显式宽度也必须参与 Wrap 折行（Row/Column 交叉轴可能传 FLT_MAX）
    {
        Label label(L"这是一个必须按显式宽度换行的长文本，不能只在父级给有限宽度时才折行。");
        label.SetTextOverflow(Label::TextOverflow::Wrap);
        label.SetWidth(220.0f);
        const Size s = label.Measure(Size(FLT_MAX, FLT_MAX));
        CHECK(fabs(s.width - 220.0f) < 0.01f);
        CHECK(s.height > 30.0f);
    }

    // 富内容按钮不开 autoSize 时也必须先测量内容，否则子元素尺寸为 0
    {
        Button button;
        button.SetAutoSizeToText(false);
        button.SetWidth(190.0f);
        auto rich = std::make_shared<RowBox>();
        auto badge = std::make_shared<Label>(L"富文本");
        rich->AddChild(badge);
        button.SetContent(rich);
        button.Measure(Size(300.0f, 36.0f));
        button.Arrange(Rect(0, 0, 190.0f, 36.0f));
        CHECK(badge->GetArrangedRect().width > 1.0f);
    }

    // 有界 Row/Column 必须把有限主轴尺寸传给 Fill 子元素
    {
        RowBox row;
        auto rowChild = std::make_shared<MeasureProbe>();
        rowChild->SetFillWidth(true);
        row.AddChild(rowChild);
        row.Measure(Size(300.0f, 100.0f));
        CHECK(fabs(rowChild->lastWidth - 300.0f) < 0.01f);

        ColumnBox col;
        auto colChild = std::make_shared<MeasureProbe>();
        colChild->SetFillHeight(true);
        col.AddChild(colChild);
        col.Measure(Size(300.0f, 100.0f));
        CHECK(fabs(colChild->lastHeight - 100.0f) < 0.01f);
    }

    // PageHost 放在 Row 里时，必须用剩余宽度测量页面；否则 Wrap 文本会按极窄宽度折行
    {
        RowBox row;
        auto host = std::make_shared<PageHost>();
        auto page = std::make_shared<Page>();
        auto text = std::make_shared<Label>(L"这是一个很长很长的段落，用来检查 PageHost 在 Row 中是否拿到有限宽度，而不是按最小宽度把每个字都拆成一行。");
        text->SetTextOverflow(Label::TextOverflow::Wrap);
        page->SetContent(text);
        host->AddPage(page);
        row.AddChild(host);
        row.Measure(Size(800.0f, 600.0f));
        CHECK(text->GetDesiredSize().height < 100.0f);
        row.Arrange(Rect(0, 0, 800.0f, 600.0f));
        CHECK(host->GetArrangedRect().width > 700.0f);
        CHECK(text->GetArrangedRect().width > 700.0f);

        ColumnBox col;
        auto host2 = std::make_shared<PageHost>();
        auto page2 = std::make_shared<Page>();
        auto text2 = std::make_shared<Label>(L"这是另一个很长很长的段落，用来检查 PageHost 在 Column 中是否拿到有限高度。");
        text2->SetTextOverflow(Label::TextOverflow::Wrap);
        page2->SetContent(text2);
        host2->AddPage(page2);
        col.AddChild(host2);
        col.Measure(Size(600.0f, 500.0f));
        CHECK(text2->GetDesiredSize().height < 100.0f);
        col.Arrange(Rect(0, 0, 600.0f, 500.0f));
        CHECK(host2->GetArrangedRect().height > 400.0f);
    }

    // 空 Grid 也必须尊重显式宽高（不能因为没有子元素就塌成 0）
    {
        GridLayout grid;
        grid.SetWidth(123.0f);
        grid.SetHeight(45.0f);
        const Size s = grid.Measure(Size(300.0f, 300.0f));
        CHECK(fabs(s.width - 123.0f) < 0.01f);
        CHECK(fabs(s.height - 45.0f) < 0.01f);
    }

    // Grid 显式高度必须作为 cell 测量范围，不能“返回 100、子项按 200 排”
    {
        GridLayout grid;
        grid.SetHeight(100.0f);
        auto child = std::make_shared<ResponsiveProbe>();
        grid.AddChild(child, 0, 0);
        const Size s = grid.Measure(Size(200.0f, 600.0f));
        CHECK(fabs(s.height - 100.0f) < 0.01f);
        grid.Arrange(Rect(0, 0, 200.0f, 100.0f));
        CHECK(child->GetArrangedRect().height <= 100.01f);
    }

    // ScrollViewer：Arrange 不得重测内容（滚动条可见性在 Measure 已算好）
    {
        ScrollViewer sv;
        auto content = std::make_shared<CountingElement>();
        sv.SetContent(content);
        sv.Measure(Size(200.0f, 100.0f));
        const int before = content->measures;
        sv.Arrange(Rect(0, 0, 200.0f, 100.0f));
        CHECK(content->measures == before);
    }

    // 空内容容器也必须尊重显式尺寸
    {
        Card card;
        card.SetWidth(120.0f);
        card.SetHeight(80.0f);
        const Size cs = card.Measure(Size(300.0f, 300.0f));
        CHECK(fabs(cs.width - 120.0f) < 0.01f);
        CHECK(fabs(cs.height - 80.0f) < 0.01f);

        ScrollViewer sv;
        sv.SetWidth(130.0f);
        sv.SetHeight(90.0f);
        const Size ss = sv.Measure(Size(300.0f, 300.0f));
        CHECK(fabs(ss.width - 130.0f) < 0.01f);
        CHECK(fabs(ss.height - 90.0f) < 0.01f);
    }

    {
        Card card;
        card.SetWidth(200.0f);
        card.SetHeight(300.0f);
        auto content = std::make_shared<ColumnBox>();
        content->SetMargin(Thickness(0.0f, 40.0f, 0.0f, 40.0f));
        card.SetContent(content);
        card.Measure(Size(200.0f, 300.0f));
        card.Arrange(Rect(0, 0, 200.0f, 300.0f));
        CHECK(fabs(content->GetArrangedRect().y - 52.0f) < 0.01f);
        CHECK(fabs(content->GetArrangedRect().height - 196.0f) < 0.01f);
    }

    {
        TextBox box;
        box.SetPasswordMode(true);
        box.SetText(L"secret");
        box.SetWidth(200.0f);
        box.SetHeight(30.0f);
        box.Arrange(Rect(0, 0, 200.0f, 30.0f));
        CHECK(!box.IsPasswordRevealed());
        box.OnMouseDown(180.0f, 15.0f);
        box.OnMouseUp(180.0f, 15.0f);
        CHECK(box.IsPasswordRevealed());
        box.OnMouseDown(180.0f, 15.0f);
        box.OnMouseUp(180.0f, 15.0f);
        CHECK(!box.IsPasswordRevealed());
        box.SetPasswordRevealButtonVisible(false);
        box.OnMouseDown(180.0f, 15.0f);
        box.OnMouseUp(180.0f, 15.0f);
        CHECK(!box.IsPasswordRevealed());
    }

    // RadioGroup / SettingsList 的显式宽高必须进入 Measure
    {
        RadioGroup rg;
        rg.AddItem(L"A");
        rg.SetWidth(180.0f);
        rg.SetHeight(90.0f);
        const Size rs = rg.Measure(Size(400.0f, 300.0f));
        CHECK(fabs(rs.width - 180.0f) < 0.01f);
        CHECK(fabs(rs.height - 90.0f) < 0.01f);

        SettingsList settings;
        settings.SetWidth(170.0f);
        settings.SetHeight(80.0f);
        const Size ss = settings.Measure(Size(400.0f, 300.0f));
        CHECK(fabs(ss.width - 170.0f) < 0.01f);
        CHECK(fabs(ss.height - 80.0f) < 0.01f);
    }

    // TabView 的页签 Label 在 Arrange 阶段就完成布局，不能等 Draw 才摆
    {
        TabView tv;
        tv.AddTab(L"Tab", nullptr);
        tv.Measure(Size(300.0f, 200.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 200.0f));
        auto label = tv.GetTabLabel(0);
        CHECK(label && label->GetArrangedRect().width > 0.0f);
    }

    // 可关闭 Tab 的 × 命中区必须能触发 TabCloseRequested
    {
        TabView tv;
        int closed = -1;
        tv.TabCloseRequested.connect([&closed](int index) { closed = index; });
        tv.AddTab(L"A", nullptr, true);
        tv.Measure(Size(300.0f, 200.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 200.0f));
        tv.OnMouseDown(42.0f, 17.0f);
        CHECK(closed == 0);
    }

    // Expander 的显式宽高必须优先（动画期间 height_ 由折叠动画驱动）
    {
        Expander ex;
        ex.SetWidth(150.0f);
        ex.SetHeight(210.0f);
        const Size s = ex.Measure(Size(300.0f, 300.0f));
        CHECK(fabs(s.width - 150.0f) < 0.01f);
        CHECK(fabs(s.height - 210.0f) < 0.01f);
    }

    // Expander 初始折叠时也要测量一次内容，避免首次展开使用未测量的子控件尺寸
    {
        Expander collapsed(L"Test");
        auto content = std::make_shared<Label>(L"collapsed content");
        content->SetHeight(60.0f);
        collapsed.SetExpanded(false);
        collapsed.SetContent(content);
        for (int i = 0; i < 30; ++i) collapsed.UpdateAnimation(0.016f);
        collapsed.Measure(Size(300.0f, FLT_MAX));
        CHECK(content->GetDesiredSize().height > 0.0f);
        CHECK(collapsed.GetDesiredSize().height < 100.0f);
    }

    // Expander 动画只改变动画进度，不再把当前高度写进用户 height_
    {
        Expander ex(L"Test");
        auto content = std::make_shared<Label>(L"content");
        content->SetHeight(80.0f);
        ex.SetContent(content);
        ex.Measure(Size(300.0f, FLT_MAX));
        const float expandedH = ex.GetDesiredSize().height;
        CHECK(expandedH > 80.0f);
        CHECK(fabs(ex.GetHeight()) < 0.01f);

        ex.SetExpanded(false);
        for (int i = 0; i < 30; ++i) { ex.UpdateAnimation(0.016f); ex.Measure(Size(300.0f, FLT_MAX)); }
        CHECK(ex.GetDesiredSize().height < expandedH);
        CHECK(fabs(ex.GetHeight()) < 0.01f);

        ex.SetExpanded(true);
        for (int i = 0; i < 30; ++i) { ex.UpdateAnimation(0.016f); ex.Measure(Size(300.0f, FLT_MAX)); }
        CHECK(fabs(ex.GetDesiredSize().height - expandedH) < 0.01f);
        CHECK(fabs(ex.GetHeight()) < 0.01f);
        ex.RefreshChildren();
        bool hasContent = false;
        for (UIElement* child : ex.GetChildren()) if (child == content.get()) hasContent = true;
        CHECK(hasContent);
    }

    {
        const Color oldBg = Expander::Bg, oldBorder = Expander::Border;
        Expander a(L"A");
        a.SetColors(Color::FromArgb(255, 10, 20, 30), Color::FromArgb(255, 40, 50, 60),
            Color::FromArgb(255, 70, 80, 90), Color::FromArgb(255, 100, 110, 120),
            Color::FromArgb(255, 130, 140, 150));
        CHECK(Expander::Bg.ToD2D().r == oldBg.ToD2D().r &&
            Expander::Bg.ToD2D().g == oldBg.ToD2D().g &&
            Expander::Border.ToD2D().b == oldBorder.ToD2D().b);
    }

    {
        Button b(L"Font");
        FontSpec f;
        f.size = 27.0f;
        b.SetFont(f);
        auto lb = b.GetLabel();
        CHECK(lb != nullptr);
        CHECK(!lb->HasFontOverride());
        CHECK(fabs(lb->GetEffectiveFontSpec().size - 27.0f) < 0.01f);
        lb->ClearFont();
        CHECK(!lb->HasFontOverride());
        CHECK(fabs(lb->GetEffectiveFontSpec().size - 27.0f) < 0.01f);
    }

    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = 10;
        bi.bmiHeader.biHeight = 10;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC dc = GetDC(nullptr);
        HBITMAP hbmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dc) ReleaseDC(nullptr, dc);
        CHECK(hbmp != nullptr && bits != nullptr);
        if (hbmp && bits) {
            auto img = Image::FromHBITMAP(hbmp, false);
            Label lb(L"Image");
            lb.SetImage(img);
            lb.SetImageFit(true);
            const Size noIcon = lb.Measure(Size(500.0f, 500.0f));
            lb.SetIcon(Icon::Close);
            const Size withIcon = lb.Measure(Size(500.0f, 500.0f));
            CHECK(fabs(noIcon.width - withIcon.width) < 0.01f);
            DeleteObject(hbmp);
        }
    }

    // ToggleSwitch：点击切换；拖动时 knob 跟随鼠标，松开按阈值切换
    {
        ToggleSwitch sw(false);
        sw.SetSize(50.0f, 24.0f);
        sw.Arrange(Rect(0, 0, 50.0f, 24.0f));
        sw.OnMouseDown(10.0f, 12.0f);
        sw.OnMouseUp(10.0f, 12.0f);
        CHECK(sw.IsOn());

        sw.OnMouseDown(40.0f, 12.0f);
        sw.OnMouseMove(-20.0f, 12.0f);
        sw.OnMouseUp(-20.0f, 12.0f);
        CHECK(!sw.IsOn());
    }

    // Slider：普通拖动跟手；点击轨道缓动；开启吸附时缓动吸附
    {
        Slider s;
        s.SetRange(0.0f, 100.0f);
        s.SetWidth(100.0f);
        s.Arrange(Rect(0, 0, 100.0f, 24.0f));
        s.SetValue(20.0f);
        for (int i = 0; i < 30; ++i) s.UpdateAnimation(1.0f / 60.0f);

        s.OnMouseDown(50.0f, 12.0f);
        CHECK(fabs(s.GetValue() - 50.0f) < 0.01f);
        CHECK(fabs(s.GetDisplayValue() - 50.0f) > 0.1f);   // 点击不是瞬移
        s.OnMouseMove(80.0f, 12.0f);
        CHECK(fabs(s.GetValue() - 80.0f) < 0.01f);
        CHECK(fabs(s.GetDisplayValue() - 80.0f) < 0.01f);  // 普通拖动跟手

        Slider snapped;
        snapped.SetRange(0.0f, 100.0f);
        snapped.SetWidth(100.0f);
        snapped.SetStep(25.0f);
        snapped.SetSnapToStep(true);
        snapped.Arrange(Rect(0, 0, 100.0f, 24.0f));
        snapped.OnMouseDown(60.0f, 12.0f);
        CHECK(fabs(snapped.GetValue() - 50.0f) < 0.01f);   // 吸附到 50
        CHECK(fabs(snapped.GetDisplayValue() - 50.0f) > 0.1f);  // 吸附不是瞬移
    }

    // 显式 Card/Page 高度必须进入 Measure，不能只按内容高度排
    {
        Card card;
        card.SetHeight(200.0f);
        auto content = std::make_shared<ColumnBox>();
        content->SetHeight(50.0f);
        card.SetContent(content);
        card.Measure(Size(400.0f, 400.0f));
        CHECK(fabs(card.GetDesiredSize().height - 200.0f) < 0.01f);
    }
}

static void test_quick_patches() {
    std::printf("Quick patches: selection / HBITMAP alpha\n");

    {
        ListView lv;
        lv.AddItem(L"A");
        lv.AddItem(L"B");
        lv.AddItem(L"C");
        lv.SetSelectionMode(ListView::SelectionMode::Multi);
        lv.ToggleIndexSelection(0);
        lv.ToggleIndexSelection(2);
        auto sel = lv.GetSelectedIndices();
        CHECK(sel.size() == 2);
        CHECK(sel[0] == 0 && sel[1] == 2);
        lv.SelectAll();
        CHECK(lv.GetSelectedIndices().size() == 3);
        lv.ClearMultiSelection();
        CHECK(lv.GetSelectedIndices().empty());
        CHECK(lv.IsMarqueeEnabled());
        lv.SetMarqueeEnabled(false);
        CHECK(!lv.IsMarqueeEnabled());
    {
        ListView lv;
        lv.AddItem(L"A"); lv.AddItem(L"B"); lv.AddItem(L"C");
        lv.SetSelectionMode(ListView::SelectionMode::Extended);
        lv.SetSelectedIndex(1);
        auto selected = lv.GetSelectedIndices();
        CHECK(selected.size() == 1 && selected[0] == 1);
        int multiSignals = 0;
        lv.SelectionChangedMulti.connect([&](const std::vector<int>&) { ++multiSignals; });
        lv.ToggleIndexSelection(2);
        CHECK(multiSignals > 0);
        lv.ClearSelection();
        CHECK(lv.GetSelectedIndices().empty());
    }

    {
        TableView tv;
        tv.SetRowCount(2);
        tv.SetColumnCount(2);
        int cellSignals = 0;
        tv.SelectionChangedCells.connect([&](const std::vector<std::pair<int, int>>&) { ++cellSignals; });
        tv.SetSelectionMode(TableView::SelectionMode::Cell);
        tv.SetCurrentCell(1, 1);
        auto cells = tv.GetSelectedCells();
        CHECK(cells.size() == 1 && cells[0].first == 1 && cells[0].second == 1);
        tv.SelectAll();
        CHECK(tv.GetSelectedCells().size() == 4);
        CHECK(cellSignals > 0);
        tv.ClearSelection();
        CHECK(tv.GetSelectedCells().empty());
        CHECK(cellSignals > 1);
        tv.SelectRow(0);
        auto rows = tv.GetSelectedRows();
        CHECK(rows.size() == 1 && rows[0] == 0);
    }

    {
        TreeView tv;
        auto a = tv.AddRoot(L"A");
        auto b = tv.AddRoot(L"B");
        tv.SetSelectionMode(TreeView::SelectionMode::Multi);
        tv.SetSelectedNode(a);
        tv.SetSelectedNode(b);
        auto nodes = tv.GetSelectedNodes();
        CHECK(nodes.size() == 1);
        CHECK(nodes[0] == b);
    }

    {
        ListView lv;
        lv.SetItemLabelProvider(1000, [](int i, Label& lb) {
            lb.SetText(L"row " + std::to_wstring(i));
            lb.SetIcon(Icon::Close);
        });
        lv.Measure(Size(300.0f, 120.0f));
        lv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        lv.RefreshChildren();
        bool listRich = false;
        for (UIElement* e : lv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText().rfind(L"row ", 0) == 0) {
                listRich = lb->GetIcon() == Icon::Close;
                break;
            }
        }
        CHECK(listRich);
    }

    {
        TableView tv;
        tv.SetCellLabelProvider(1000, 2, [](int r, int c, Label& lb) {
            lb.SetText(L"cell " + std::to_wstring(r) + L"," + std::to_wstring(c));
            lb.SetIcon(Icon::Close);
        });
        tv.Measure(Size(300.0f, 120.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        tv.RefreshChildren();
        bool tableRich = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText().rfind(L"cell ", 0) == 0) {
                tableRich = lb->GetIcon() == Icon::Close;
                break;
            }
        }
        CHECK(tableRich);
    }

    {
        TreeView tv;
        tv.SetColumnCount(1);
        tv.AddRoot(L"raw");
        tv.SetNodeLabelProvider([](const std::shared_ptr<TreeNode>&, int, Label& lb) {
            lb.SetText(L"rich node");
            lb.SetIcon(Icon::Close);
        });
        tv.Measure(Size(300.0f, 120.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        tv.RefreshChildren();
        bool treeRich = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"rich node") {
                treeRich = lb->GetIcon() == Icon::Close;
                break;
            }
        }
        CHECK(treeRich);
    }

    {
        ListView lv;
        int mode = 0;
        lv.SetItemLabelProvider(100, [&mode](int i, Label& lb) {
            lb.SetText(L"row " + std::to_wstring(i));
            if (mode == 0) lb.SetIcon(Icon::Close);
        });
        lv.Measure(Size(300.0f, 120.0f));
        lv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        lv.RefreshChildren();
        bool icon0 = false;
        for (UIElement* e : lv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"row 0") icon0 = lb->GetIcon() == Icon::Close;
        }
        CHECK(icon0);
        mode = 1;
        lv.NotifyChanged(0, 1);
        lv.RefreshChildren();
        bool iconCleared = false;
        for (UIElement* e : lv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"row 0") iconCleared = lb->GetIcon() == Icon::None;
        }
        CHECK(iconCleared);
    }

    {
        TableView tv;
        int mode = 0;
        tv.SetCellLabelProvider(20, 2, [&mode](int r, int c, Label& lb) {
            lb.SetText(L"cell " + std::to_wstring(r) + L"," + std::to_wstring(c));
            if (mode == 0) lb.SetIcon(Icon::Close);
        });
        tv.Measure(Size(300.0f, 120.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        tv.RefreshChildren();
        bool icon0 = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"cell 0,0") icon0 = lb->GetIcon() == Icon::Close;
        }
        CHECK(icon0);
        mode = 1;
        tv.SetRowCount(20);
        tv.RefreshChildren();
        bool iconCleared = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"cell 0,0") iconCleared = lb->GetIcon() == Icon::None;
        }
        CHECK(iconCleared);
    }

    {
        TreeView tv;
        tv.SetColumnCount(1);
        tv.AddRoot(L"raw");
        int mode = 0;
        tv.SetNodeLabelProvider([&mode](const std::shared_ptr<TreeNode>&, int, Label& lb) {
            if (mode == 0) lb.SetIcon(Icon::Close);
        });
        tv.Measure(Size(300.0f, 120.0f));
        tv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        tv.RefreshChildren();
        bool icon0 = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"raw") icon0 = lb->GetIcon() == Icon::Close;
        }
        CHECK(icon0);
        mode = 1;
        tv.RefreshNodeLabels();
        tv.RefreshChildren();
        bool iconCleared = false;
        for (UIElement* e : tv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->GetText() == L"raw") iconCleared = lb->GetIcon() == Icon::None;
        }
        CHECK(iconCleared);
    }

    {
        TreeView tv;
        auto root = tv.AddRoot(L"lazy-root");
        int loads = 0;
        tv.SetLazyLoader([&](std::shared_ptr<TreeNode> node) {
            if (++loads == 1) tv.AddChild(node, L"lazy-child");
        });
        tv.ExpandNode(root, true);
        CHECK(loads == 1);
        CHECK(tv.GetVisibleNodes().size() == 2);
    }

    {
        Label lb(L"0123456789");
        const Size natural = lb.Measure(Size(FLT_MAX, FLT_MAX));
        lb.SetWidth(natural.width - 0.2f);
        lb.Measure(Size(natural.width, natural.height));
        lb.Arrange(Rect(0.0f, 0.0f, natural.width - 0.2f, natural.height));
        CHECK(lb.GetToolTip().empty());
    }

    {
        Label lb;
        LabelRun r1; r1.text = L"red"; r1.color = Color::FromArgb(255, 255, 0, 0);
        LabelRun r2; r2.text = L"blue"; r2.color = Color::FromArgb(255, 0, 0, 255);
        lb.SetRichText({ r1, r2 });
        CHECK(lb.HasRichText());
        CHECK(lb.GetText() == L"redblue");
        CHECK(lb.GetRichTextClip() == Label::RichTextClip::Clip);
        lb.SetRichTextClip(Label::RichTextClip::Ellipsis);
        CHECK(lb.GetRichTextClip() == Label::RichTextClip::Ellipsis);
        lb.SetRichTextClip(Label::RichTextClip::None);
        CHECK(lb.GetRichTextClip() == Label::RichTextClip::None);
        CHECK(lb.Measure(Size(500.0f, 100.0f)).width > 0.0f);
        lb.SetText(L"plain");
        CHECK(!lb.HasRichText());
        CHECK(lb.GetText() == L"plain");
    }

    {
        ListView lv;
        lv.SetItemLabelProvider(10, [](int i, Label& lb) {
            LabelRun a; a.text = L"row "; a.color = Color::FromArgb(255, 255, 0, 0);
            LabelRun b; b.text = std::to_wstring(i); b.color = Color::FromArgb(255, 0, 0, 255);
            lb.SetRichText({ a, b });
            lb.SetIcon(Icon::Close);
        });
        lv.Measure(Size(300.0f, 120.0f));
        lv.Arrange(Rect(0, 0, 300.0f, 120.0f));
        lv.RefreshChildren();
        bool rich = false;
        for (UIElement* e : lv.GetChildren()) {
            auto* lb = dynamic_cast<Label*>(e);
            if (lb && lb->HasRichText() && lb->GetText() == L"row 0") rich = lb->GetIcon() == Icon::Close;
        }
        CHECK(rich);
    }

    }

    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = 2;
        bi.bmiHeader.biHeight = -1;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC dc = GetDC(nullptr);
        HBITMAP hbmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dc) ReleaseDC(nullptr, dc);
        CHECK(hbmp != nullptr && bits != nullptr);
        if (hbmp && bits) {
            BYTE* px = static_cast<BYTE*>(bits);
            px[0] = 50; px[1] = 100; px[2] = 200; px[3] = 128;
            px[4] = 10; px[5] = 20;  px[6] = 30;  px[7] = 255;
            auto img = Image::FromHBITMAP(hbmp, false);
            CHECK(img && !img->IsNull());
            std::vector<BYTE> out;
            int w = 0, h = 0;
            CHECK(img && img->CopyPixelsBgra(out, w, h));
            CHECK(w == 2 && h == 1);
            if (out.size() >= 8) {
                CHECK(out[3] == 128);
                CHECK(out[7] == 255);
                CHECK(abs((int)out[0] - 50) <= 1);
                CHECK(abs((int)out[1] - 100) <= 1);
                CHECK(abs((int)out[2] - 200) <= 1);
            }
            DeleteObject(hbmp);
        }
    }
}




int main() {
    SetConsoleOutputCP(CP_UTF8);   // 源码是 UTF-8：不改代码页的话中文断言输出在 cmd 里是乱码
    std::printf("=== ZufyUI 自检 ===\n");
    test_containers();
    test_apply();
    test_layout_sugar();
    test_grid_coords();
    test_data_views_and_menu();
    test_factories_and_wrap();
    test_dsl_pitfalls();
    test_fix_regressions();
    test_button_and_listview_regressions();
    test_quick_patches();
    test_async_edge_cases();
    test_viewcore_list_example();
    test_virtual_label_stress();
    test_async_runtime();
    test_async_randomized();
    test_async_cancel_race();
    test_async_stress();
    test_animation_primitives();
    test_button_animations();
    test_drawer();
    test_api_feedback();
    test_layout_rfc();
    test_backdrop_defaults();
    test_signal_lifetime();
    test_signal_reentrancy_and_exception();
    test_signal_threading();
    std::printf("\n通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    if (g_fail) { std::printf("结果：FAILED\n"); return 1; }
    std::printf("结果：OK\n");
    return 0;
}
