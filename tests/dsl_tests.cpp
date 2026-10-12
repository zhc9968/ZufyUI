// ============================================================================
// tests/dsl_tests.cpp —— ZufyUI DSL 自检（编译期断言 + 运行期断言）
//
//   构建并运行：
//       powershell -ExecutionPolicy Bypass -File tools/run_dsl_tests.ps1
//   退出码 0 = 全部通过；非 0 = 有断言失败（失败项会打印到 stdout）。
//
//   本测试**不创建窗口**（不需要 D2D / DComp / 消息循环），所以能在命令行 / CI 里直接跑。
//   覆盖：容器加子分派、空值过滤、布局快捷、Grid 坐标、数据视图与菜单构建糖、新工厂、Wrap。
// ============================================================================
// 包含顺序刻意保持“WindowTool 在前”：它自己会 include ZufyUIWidgets.h，
// 所以这里不再手动包含 —— 这个测试同时验证 WindowTool 是否自洽。
#include "ZufyUIWindowTool.h"   // 无障碍提供程序：UIElement 析构要用其中的定义（ForgetAccessibleElement 等）
#include "ZufyUIDsl.h"
#include <cstdio>

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

    auto lbl = d::Lbl(L"a");
    auto col = d::Col(lbl);
    col->RefreshChildren();   // GetChildren() 返回 RefreshChildren() 维护的视图缓冲
    CHECK(col->GetChildren().size() == 1);
    CHECK(col->GetChildren()[0] == lbl.ptr());
    CHECK(lbl->GetParent() == col.ptr());

    // A4：空子元素被容器过滤，不再解引用空指针
    auto col2 = d::Col(d::Lbl(L"b"), nullptr, d::Ref<Label>{});
    col2->RefreshChildren();
    CHECK(col2->GetChildren().size() == 1);

    // A3：add() 按容器分派（ColumnBox → AddChild）
    auto col3 = d::Col().add(d::Lbl(L"x")).add(d::Lbl(L"y"));
    col3->RefreshChildren();
    CHECK(col3->GetChildren().size() == 2);

    // A3：Label 也是 AddChild 分支（内联子控件）——只验父子关系，Label 不维护 GetChildren 视图
    auto host = d::Lbl(L"host");
    auto inlineChild = d::Lbl(L"inline");
    host.add(inlineChild);
    CHECK(inlineChild->GetParent() == host.ptr());
    // Label::AddChild 自身也拒绝空值
    host.add(nullptr);
    CHECK(inlineChild->GetParent() == host.ptr());

    // A3：PageHost → AddPage
    auto ph = d::Pages().add(d::Page()).add(d::Page());
    CHECK(ph->GetPageCount() == 2);
    // 非 Page 实参被跳过，不会塞空指针进去
    ph.add(d::Lbl(L"不是页"));
    CHECK(ph->GetPageCount() == 2);

    // A3：add() 之后仍可继续链式调用
    auto row = d::Row().add(d::Lbl(L"1")).spacing(6.0f);
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
    auto l = d::Lbl(L"before").apply([&calls](Label& x) {
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
    auto l2 = d::Lbl(L"x").apply([](Label& x) { x.SetMinWidth(10.0f); }).minWidth(20.0f);
    CHECK(l2->GetMinWidth() == 20.0f);
}

// ---------------------------------------------------------------------------
// 4) 运行期：布局 / 尺寸 / 外观快捷（B2）
// ---------------------------------------------------------------------------
static void test_layout_sugar() {
    std::printf("布局 / 尺寸 / 外观快捷\n");

    auto col = d::Col().spacing(12.0f);
    CHECK(col->GetSpacing() == 12.0f);

    auto l = d::Lbl(L"x").minWidth(40.0f).maxWidth(120.0f).bleed(2.0f).visible(false);
    CHECK(l->GetMinWidth() == 40.0f);
    CHECK(l->GetMaxWidth() == 120.0f);
    CHECK(l->GetBleed() == 2.0f);
    CHECK(!l->IsVisible());

    auto l2 = d::Lbl(L"y").margin(Thickness(1, 2, 3, 4)).size(50.0f, 20.0f);
    CHECK(l2->GetMargin().left == 1.0f);
    CHECK(l2->GetWidth() == 50.0f);
    CHECK(l2->GetHeight() == 20.0f);

    auto l3 = d::Lbl(L"z").enabled(false).hWeight(2.0f).vWeight(0.5f).weight(3.0f, 1.0f);
    CHECK(!l3->IsEnabled());

    // GridLayout：双参 spacing + align（对齐只有 GridLayout 有）
    auto g = d::Grid().spacing(4.0f, 6.0f).align(d::Align::Center, d::Align::End);
    CHECK(g.ptr() != nullptr);

    // UIElement 标准接口的其余快捷：只验“可调用、不崩、返回自身”
    auto l4 = d::Lbl(L"w").cache(true).dropTarget(true).contextMenuEnabled(false).accessible(L"名字");
    CHECK(l4.ptr() != nullptr);
    l4.clipRect(Rect(0, 0, 10, 10)).noClip();
    CHECK(l4.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 5) 运行期：Grid 坐标（A3 / B1）
// ---------------------------------------------------------------------------
static void test_grid_coords() {
    std::printf("Grid 坐标\n");

    auto a = d::Lbl(L"A");
    auto b = d::Lbl(L"B");
    auto g = d::Grid(d::Cell(0, 0, a), d::Cell(1, 2, b));
    g->RefreshChildren();
    CHECK(g->GetChildren().size() == 2);

    auto c = d::Lbl(L"C");
    auto g2 = d::Grid().cell(0, 0, c).cell(1, 1, d::Lbl(L"D"));
    g2->RefreshChildren();
    CHECK(g2->GetChildren().size() == 2);

    // 裸元素沿用旧行为（按出现顺序 (i,0)），不破坏既有代码
    auto g3 = d::Grid(d::Lbl(L"x"), d::Lbl(L"y"));
    g3->RefreshChildren();
    CHECK(g3->GetChildren().size() == 2);

    // Cell 的跨行跨列
    auto g4 = d::Grid(d::Cell(0, 0, d::Lbl(L"span"), 1, 2));
    g4->RefreshChildren();
    CHECK(g4->GetChildren().size() == 1);
}

// ---------------------------------------------------------------------------
// 6) 运行期：数据视图与菜单构建糖（B4）
// ---------------------------------------------------------------------------
static void test_data_views_and_menu() {
    std::printf("数据视图 / 菜单构建糖\n");

    auto lv = d::List().items({ L"a", L"b", L"c" });
    CHECK(lv->GetItemCount() == 3);

    std::vector<std::wstring> v{ L"d", L"e" };
    auto lv2 = d::List().items(v);
    CHECK(lv2->GetItemCount() == 2);

    auto tv = d::Table().rows(3).cols(2).header(0, L"列0").cell(0, 0, L"r0c0");
    CHECK(tv->GetRowCount() == 3);
    CHECK(tv->GetColumnCount() == 2);

    auto tree = d::Tree().roots({ L"根1", L"根2" });
    CHECK(tree.ptr() != nullptr);

    auto m = d::Menu().item(L"打开").separator().item(L"退出");
    CHECK(m.ptr() != nullptr);

    auto sub = d::Menu().item(L"子项");
    auto m2 = d::Menu().item(L"顶层").submenu(L"更多", sub);
    CHECK(m2.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 7) 运行期：新工厂 / Wrap / 图片糖（B3）
// ---------------------------------------------------------------------------
static void test_factories_and_wrap() {
    std::printf("新工厂 / Wrap / 图片糖\n");

    auto rg = d::RadioGroup();
    CHECK(rg.ptr() != nullptr);

    auto sb = d::ScrollBar(false);
    CHECK(sb.ptr() != nullptr);

    auto g = d::Glyph(Icon::Save, 18.0f);
    CHECK(g.ptr() != nullptr);
    CHECK(g->GetIcon() == Icon::Save);
    CHECK(g->GetIconSize() == 18.0f);

    auto rg2 = d::Make<RadioGroup>();
    CHECK(rg2.ptr() != nullptr);

    auto existing = std::make_shared<Label>(L"已有");
    auto w = d::Wrap(existing);
    CHECK(w.ptr() == existing.get());
    CHECK(w->GetText() == L"已有");

    // Image 是资源对象（不是 UIElement），只能由控件侧接收
    auto withImg = d::Lbl(L"pic").image(nullptr).imageFit(true);
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
    auto cardPad = d::Card().padding(6.0f);
    CHECK(cardPad.ptr() != nullptr);
    auto colPad = d::Col().margin(Thickness(4, 4, 4, 4));
    CHECK(colPad.ptr() != nullptr);

    // b) 单参、无默认参数的成员函数可安全用于 .with()。
    //    历史坑：带默认参数的成员函数（如旧的 SetCheckable(bool,bool)）成员指针含全部形参，
    //    .with 会按推导参数包调用 → 报"参数太少"。P0-1 已把这类函数拆成无默认参数的重载。
    auto treeCk = d::Tree().with(&TreeView::SetCheckable, true);
    CHECK(treeCk.ptr() != nullptr);

    // c) 同名工厂当"表达式 / 类型实参"用必须全限定（::ZufyUI::dsl::Slider）；
    //    而 .with(&Slider::SetRange, ...) 这种"嵌套名限定符"位置不歧义。
    auto sl = ::ZufyUI::dsl::Slider()
                  .with(&Slider::SetRange, 0.0f, 100.0f)
                  .with(&Slider::SetStep, 5.0f)
                  .with(&Slider::SetSnapToStep, true)
                  .value(40.0f);
    CHECK(sl.ptr() != nullptr);
    CHECK(sl->GetValue() == 40.0f);

    // d) 花括号初始化列表过不了 .with()（转发引用推不出 braced-init-list）→ 用 .apply(...)
    auto tv = d::Tree().with(&TreeView::SetColumnCount, 2)
                       .apply([](TreeView& t) { t.SetHeaderLabels({ L"名称", L"类型" }); });
    CHECK(tv.ptr() != nullptr);

    // e) Grid(Cell(...)) 声明式坐标
    auto grid = d::Grid(d::Cell(0, 0, d::Lbl(L"A")), d::Cell(1, 1, d::Lbl(L"B"))).spacing(4.0f, 4.0f);
    grid->RefreshChildren();
    CHECK(grid->GetChildren().size() == 2);

    // f) items() 批量加项
    auto combo = d::Combo().items({ L"一", L"二", L"三" });
    CHECK(combo->GetItemCount() == 3);

    // g) Table 行列 / 表头 / cell 快捷
    auto tbl = d::Table().rows(3).cols(2).header(0, L"列一").header(1, L"列二").cell(0, 0, L"x").cell(0, 1, L"y");
    CHECK(tbl->GetRowCount() == 3);
    CHECK(tbl->GetColumnCount() == 2);

    // h) Ring() 走默认尺寸（工厂无法传尺寸）；要自定义尺寸只能直接构造 ProgressRing(size)
    auto ring = d::Ring();
    CHECK(ring.ptr() != nullptr);
    auto ring36 = std::make_shared<ProgressRing>(36.0f);
    CHECK(ring36 != nullptr);

    // i) 页面重排的固定写法：Col + add() 分派 + Scroll 包一层
    auto col = d::Col();
    col.add(d::Lbl(L"1"));
    col->RefreshChildren();
    CHECK(col->GetChildren().size() == 1);
    col.add(d::Row(d::Lbl(L"a"), d::Lbl(L"b")).shared());
    col->RefreshChildren();
    CHECK(col->GetChildren().size() == 2);
    auto sc = d::Scroll().fill(true).margin(Thickness(8, 8, 8, 8))
                         .with(&ScrollViewer::SetContent, col.shared());
    CHECK(sc.ptr() != nullptr);
}

// ---------------------------------------------------------------------------
// 10) 运行期：本批修复的行为回归（按钮自动撑宽 / 进度条与开关缓动 / SplitView 折叠）
// ---------------------------------------------------------------------------
static void test_fix_regressions() {
    std::printf("修复回归（Button 撑宽 / 进度条·开关缓动 / SplitView 折叠）\n");

    // ⑧ Button：文案超出默认宽度时应自动撑开，不再被省略号裁掉
    auto longBtn = d::Btn(L"这是一个非常非常非常非常非常非常非常长的按钮文案，远超默认宽度");
    longBtn->Measure(Size(1000.0f, 100.0f));
    CHECK(longBtn->GetDesiredSize().width > Button::DefaultWidth);

    // 关掉自动撑宽 → 回到固定宽度（给需要精确控制布局的场景留后路）
    auto fixedBtn = d::Btn(L"这是一个非常非常非常非常非常非常非常长的按钮文案，远超默认宽度")
                        .with(&Button::SetAutoSizeToText, false);
    fixedBtn->Measure(Size(1000.0f, 100.0f));
    CHECK(fixedBtn->GetDesiredSize().width == Button::DefaultWidth);

    // ⑥ Label：不设固定宽度时，宽度跟随文本 —— 这正是"后面还有空间却提前省略号"的修复点
    //    （旧代码是 .width(100.0f)，文本超过 100px 就被省略号裁掉；minWidth 只是给父容器的夹取下限）
    auto echoLbl = d::Lbl(L"输入：一段不算短的回显文本内容");
    echoLbl->Measure(Size(1000.0f, 100.0f));
    CHECK(echoLbl->GetWidth() <= 0.0f);                            // 没有固定宽度
    CHECK(echoLbl->GetDesiredSize().width > 100.0f);               // 宽度跟随文本
    CHECK(d::Lbl(L"短").minWidth(160.0f)->GetMinWidth() >= 160.0f); // 快捷方法写入 MinWidth（夹取由父容器负责）

    // ④ ProgressBar：SetValue 后进入"缓动中"，跑约 1 秒后收敛
    auto pb = d::Progress().value(0.0f);
    pb->SetValue(1.0f);
    CHECK(pb->HasActiveAnimation());
    for (int i = 0; i < 70; ++i) pb->UpdateAnimation(1.0f / 60.0f);
    CHECK(!pb->HasActiveAnimation());
    CHECK(pb->GetValue() == 1.0f);

    // ③ ToggleSwitch：一帧走不完（缓动，不是瞬变），足够多帧后收敛到 1
    auto tg = d::Toggle(false);
    tg->SetOn(true);
    tg->UpdateAnimation(1.0f / 60.0f);
    CHECK(tg->DebugAnimationProgress() > 0.0f);
    CHECK(tg->DebugAnimationProgress() < 0.5f);          // 若为线性瞬变这里会是 1.0
    for (int i = 0; i < 70; ++i) tg->UpdateAnimation(1.0f / 60.0f);
    CHECK(tg->DebugAnimationProgress() >= 0.999f);

    // ⑮⑯ SplitView：折叠后该栏长度为 0；展开回到"最小状态"；信号与开关都生效
    auto sv = d::Split().size(400.0f, 200.0f);
    sv->SetMinFirst(100.0f);
    sv->SetMinSecond(100.0f);
    sv->SetFirst(d::Lbl(L"左").shared());
    sv->SetSecond(d::Lbl(L"右").shared());
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
}

int main() {
    SetConsoleOutputCP(CP_UTF8);   // 源码是 UTF-8：不改代码页的话中文断言输出在 cmd 里是乱码
    std::printf("=== ZufyUI DSL 自检 ===\n");
    test_containers();
    test_apply();
    test_layout_sugar();
    test_grid_coords();
    test_data_views_and_menu();
    test_factories_and_wrap();
    test_dsl_pitfalls();
    test_fix_regressions();
    std::printf("\n通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    if (g_fail) { std::printf("结果：FAILED\n"); return 1; }
    std::printf("结果：OK\n");
    return 0;
}
