#pragma once
// ============================================================================
// ZufyUIDsl.h —— 链式构造 DSL（可选，零侵入）
//   目标：一行/一段完成“构造 → 设属性 → 组子树 → 连信号”。
//   不修改任何现有控件；Ref<T> 外层包装，通用 with(&T::Setter, args...) 转发。
//   用法：using namespace ZufyUI::dsl;  然后  Btn(L"OK").size(100,36).on<&Button::Clicked>([]{});
//   注意：不要同时 `using namespace ZufyUI;`（短工厂名与类名会歧义），用 ZufyUI:: 限定类型。
// ============================================================================
#include "ZufyUIWidgets.h"
#include "ZDataViewer.h"
#include "ZufyUICharts.h"
#include "ZufyUIIcons.h"   // FontIcon（Icon 枚举本身已在 ZufyUIWidgets.h）
#include <type_traits>

namespace ZufyUI {
namespace dsl {

    template<class T> class Ref;   // 前向

    // ---- 把 Ref<U> / shared_ptr<U> / nullptr 统一成 shared_ptr<UIElement> ----
    template<class U> std::shared_ptr<UIElement> toElem(const Ref<U>& r);
    template<class U, std::enable_if_t<std::is_base_of_v<UIElement, U>, int> = 0>
    inline std::shared_ptr<UIElement> toElem(const std::shared_ptr<U>& p) { return p; }
    inline std::shared_ptr<UIElement> toElem(std::nullptr_t) { return {}; }

    // ---- 编译期探测“容器加子”的能力（决定 add() 怎么分派、cell() 是否存在）----
    template<class T, class = void> struct HasAddChild1 : std::false_type {};          // ColumnBox / RowBox
    template<class T> struct HasAddChild1<T, std::void_t<decltype(
        std::declval<T&>().AddChild(std::declval<std::shared_ptr<UIElement>>()))>> : std::true_type {};

    template<class T, class = void> struct HasAddPage : std::false_type {};            // PageHost
    template<class T> struct HasAddPage<T, std::void_t<decltype(
        std::declval<T&>().AddPage(std::declval<std::shared_ptr<Page>>()))>> : std::true_type {};

    template<class T, class = void> struct HasGridAddChild : std::false_type {};       // GridLayout
    template<class T> struct HasGridAddChild<T, std::void_t<decltype(
        std::declval<T&>().AddChild(std::declval<std::shared_ptr<UIElement>>(), 0, 0, 1, 1))>> : std::true_type {};

    // GridLayout 的单元格说明（供 Grid(Cell(r,c,child), ...) 使用）
    struct CellSpec {
        int row = 0, col = 0, rowSpan = 1, colSpan = 1;
        std::shared_ptr<UIElement> child;
    };

    // 对齐枚举的短别名（GridLayout::Alignment），供 Ref::align() 使用
    using Align = GridLayout::Alignment;

    // ---- 元素包装 ----
    template<class T> class Ref {
    public:
        Ref() = default;
        Ref(std::shared_ptr<T> p) : p_(std::move(p)) {}

        T* operator->() const { return p_.get(); }
        T* ptr() const { return p_.get(); }
        std::shared_ptr<T> shared() const { return p_; }
        explicit operator bool() const { return (bool)p_; }
        operator std::shared_ptr<T>() const { return p_; }               // 兼容现有 API（如 AddChild/SetContent）

        // 通用 setter 转发：with(&Button::SetWidth, 100.0f)（重载 setter 需 static_cast 消歧）
        // 注意用 C（声明 setter 的类，通常是基类 UIElement）而非 T：继承来的 setter 的成员指针类型是基类的
        template<class C, class R, class... A0, class... A>
        Ref& with(R(C::*fn)(A0...), A&&... a) {
            if (p_) (p_.get()->*fn)(std::forward<A>(a)...);
            return *this;
        }
        // 信号：引用形式 on(elem->Clicked, slot) 或模板形式 on<&Button::Clicked>(slot)
        template<class Sig, class F> Ref& on(Sig& sig, F&& f) {
            if (p_) p_->Connect(sig, std::forward<F>(f));
            return *this;
        }
        template<auto SigMem, class F> Ref& on(F&& f) {
            if (p_) p_->Connect(p_.get()->*SigMem, std::forward<F>(f));
            return *this;
        }
        // 逃生口：with() 只接“成员函数指针”，多步 / 条件 / 需要迭代的操作走 apply(lambda)
        //   Lbl(L"x").apply([](Label& l){ l.SetText(L"y"); l.SetPadding(4.0f); });
        template<class F> Ref& apply(F&& f) { if (p_) f(*p_); return *this; }
        // 容器：add(child...)
        //   按容器的真实接口分派：ColumnBox/RowBox/Label → AddChild；PageHost → AddPage；
        //   GridLayout 需要坐标 → 用 cell(row,col,child) 或 dsl::Cell(...)。
        template<class... C> Ref& add(C&&... c) {
            if (!p_) return *this;
            if constexpr (HasAddChild1<T>::value) {
                (p_->AddChild(toElem(std::forward<C>(c))), ...);
            } else if constexpr (HasAddPage<T>::value) {
                // PageHost 只收 Page：动态转换，非 Page 的实参直接跳过（不塞空指针进去）
                (this->AddPageChecked(std::forward<C>(c)), ...);
            } else if constexpr (HasGridAddChild<T>::value) {
                static_assert(sizeof...(C) == 0,
                    "GridLayout 的子元素必须带坐标：用 .cell(row, col, child) 或 dsl::Grid(dsl::Cell(r,c,child), ...)");
            } else {
                static_assert(sizeof...(C) == 0,
                    "该类型没有可用的加子接口（支持 add 的：ColumnBox / RowBox / Label / PageHost）");
            }
            return *this;
        }
        // PageHost 专用：只接受能动态转换为 Page 的实参
        template<class C> void AddPageChecked(C&& c) {
            auto e = toElem(std::forward<C>(c));
            if (auto pg = std::dynamic_pointer_cast<Page>(e)) p_->AddPage(pg);
        }
        // GridLayout 专用：按 (行, 列) 放一个子元素；可跨行跨列
        template<class U = T, class = std::enable_if_t<HasGridAddChild<U>::value>, class C>
        Ref& cell(int row, int col, C&& c, int rowSpan = 1, int colSpan = 1) {
            if (p_) { auto e = toElem(std::forward<C>(c)); if (e) p_->AddChild(e, row, col, rowSpan, colSpan); }
            return *this;
        }

        // 通用布局/可视快捷（映射 UIElement 常用 setter）
        Ref& width(float w)             { return with(&T::SetWidth, w); }
        Ref& height(float h)            { return with(&T::SetHeight, h); }
        Ref& size(float w, float h)     { return width(w).height(h); }
        Ref& fillWidth(bool on = true)  { return with(&T::SetFillWidth, on); }
        Ref& fillHeight(bool on = true) { return with(&T::SetFillHeight, on); }
        Ref& fill(bool w = true, bool h = true) { return fillWidth(w).fillHeight(h); }
        Ref& margin(const Thickness& m) { return with(&T::SetMargin, m); }
        Ref& visible(bool on = true)    { return with(&T::SetVisible, on); }
        Ref& enabled(bool on = true)    { return with(&T::SetEnabled, on); }
        Ref& tooltip(const std::wstring& s) { return with(&T::SetToolTip, s); }

        // ---- 尺寸上下限 / 拉伸权重（都是 UIElement 的标准接口，无需门控）----
        Ref& minWidth(float w)          { return with(&T::SetMinWidth, w); }
        Ref& minHeight(float h)         { return with(&T::SetMinHeight, h); }
        Ref& maxWidth(float w)          { return with(&T::SetMaxWidth, w); }
        Ref& maxHeight(float h)         { return with(&T::SetMaxHeight, h); }
        Ref& minSize(float w, float h)  { return with(&T::SetMinSize, w, h); }
        Ref& maxSize(float w, float h)  { return with(&T::SetMaxSize, w, h); }
        Ref& weight(float h, float v)   { return with(&T::SetStretchWeights, h, v); }
        Ref& hWeight(float w)           { return with(&T::SetHorizontalStretchWeight, w); }
        Ref& vWeight(float w)           { return with(&T::SetVerticalStretchWeight, w); }

        // ---- 绘制 / 缓存 / 交互（UIElement 标准接口）----
        Ref& cache(bool on = true)      { return with(&T::SetUseCache, on); }
        Ref& bleed(float px)            { return with(&T::SetBleed, px); }
        Ref& clipRect(const Rect& r)    { return with(&T::SetClipRect, std::optional<Rect>(r)); }
        Ref& noClip()                   { return with(&T::SetClipRect, std::optional<Rect>()); }
        Ref& cursor(HCURSOR c)          { return with(&T::SetCursor, c); }
        Ref& dropTarget(bool on = true) { return with(&T::SetDropTargetEnabled, on); }
        Ref& contextMenuEnabled(bool on = true) { return with(&T::SetContextMenuEnabled, on); }
        Ref& accessible(const std::wstring& name) { return with(&T::SetAccessibleName, name); }

        // ---- 控件专属糖：SFINAE 门控 —— 仅当 T 有对应方法时才存在（不会误用/报错）----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetText(std::declval<const std::wstring&>()))>>
        Ref& text(const std::wstring& s) { if (p_) p_->SetText(s); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetChecked(true))>>
        Ref& checked(bool on = true) { if (p_) p_->SetChecked(on); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetValue(1.0f))>>
        Ref& value(float v) { if (p_) p_->SetValue(v); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetPlaceholder(std::declval<const std::wstring&>()))>>
        Ref& placeholder(const std::wstring& s) { if (p_) p_->SetPlaceholder(s); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetPadding(0.0f))>>
        Ref& padding(float v) { if (p_) p_->SetPadding(v); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddCategory(std::declval<const std::wstring&>()))>>
        Ref& category(const std::wstring& c) { if (p_) p_->AddCategory(c); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddSeries(std::declval<const std::wstring&>(), std::declval<const std::vector<double>&>()))>>
        Ref& series(const std::wstring& name, std::vector<double> v) { if (p_) p_->AddSeries(name, std::move(v)); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddItem(std::declval<const std::wstring&>(), std::declval<std::function<void()>>()))>>
        Ref& item(const std::wstring& t, std::function<void()> cb = nullptr) { if (p_) p_->AddItem(t, std::move(cb)); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddMenu(std::declval<const std::wstring&>(), std::declval<std::shared_ptr<Menu>>()))>>
        Ref& menu(const std::wstring& title, std::shared_ptr<Menu> m) { if (p_) p_->AddMenu(title, std::move(m)); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddSlice(std::declval<const std::wstring&>(), 0.0))>>
        Ref& slice(const std::wstring& label, double v) { if (p_) p_->AddSlice(label, v); return *this; }

        // ---- 容器间距（ColumnBox/RowBox 单参；GridLayout 双参）----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetSpacing(0.0f))>>
        Ref& spacing(float s) { if (p_) p_->SetSpacing(s); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetSpacing(0.0f, 0.0f))>>
        Ref& spacing(float h, float v) { if (p_) p_->SetSpacing(h, v); return *this; }

        // ---- 内边距：Thickness 版（float 版见上）----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetPadding(std::declval<const Thickness&>()))>>
        Ref& padding(const Thickness& t) { if (p_) p_->SetPadding(t); return *this; }

        // ---- 配色（各控件自带 SetBackgroundColor/SetTextColor，按存在与否门控）----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetBackgroundColor(std::declval<Color>()))>>
        Ref& background(Color c) { if (p_) p_->SetBackgroundColor(c); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetTextColor(std::declval<Color>()))>>
        Ref& textColor(Color c) { if (p_) p_->SetTextColor(c); return *this; }

        // ---- 对齐：仅 GridLayout 有（其嵌套枚举 Alignment）----
        template<class U = T, class A = typename U::Alignment>
        Ref& align(A h, A v) { if (p_) { p_->SetHorizontalAlignment(h); p_->SetVerticalAlignment(v); } return *this; }

        // ---- 数据视图：批量项 / 表格行列 / 树根 ----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddItem(std::declval<const std::wstring&>()))>>
        Ref& items(std::initializer_list<std::wstring> list) { if (p_) for (auto& s : list) p_->AddItem(s); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddItem(std::declval<const std::wstring&>()))>>
        Ref& items(const std::vector<std::wstring>& v) { if (p_) for (auto& s : v) p_->AddItem(s); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetRowCount(0))>>
        Ref& rows(int n) { if (p_) p_->SetRowCount(n); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetColumnCount(0))>>
        Ref& cols(int n) { if (p_) p_->SetColumnCount(n); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetHeaderLabel(0, std::declval<const std::wstring&>()))>>
        Ref& header(int col, const std::wstring& t) { if (p_) p_->SetHeaderLabel(col, t); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetItem(0, 0, std::declval<const std::wstring&>()))>>
        Ref& cell(int row, int col, const std::wstring& t) { if (p_) p_->SetItem(row, col, t); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddRoot(std::declval<const std::wstring&>()))>>
        Ref& root(const std::wstring& t) { if (p_) p_->AddRoot(t); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddRoot(std::declval<const std::wstring&>()))>>
        Ref& roots(std::initializer_list<std::wstring> list) { if (p_) for (auto& s : list) p_->AddRoot(s); return *this; }

        // ---- 菜单：分隔线 / 子菜单 ----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddSeparator())>>
        Ref& separator() { if (p_) p_->AddSeparator(); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddSubmenu(std::declval<const std::wstring&>(), std::declval<std::shared_ptr<Menu>>()))>>
        Ref& submenu(const std::wstring& title, std::shared_ptr<Menu> m) { if (p_) p_->AddSubmenu(title, std::move(m)); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().AddSubmenu(std::declval<const std::wstring&>(), std::declval<std::shared_ptr<Menu>>()))>>
        Ref& submenu(const std::wstring& title, const Ref<Menu>& m) { if (p_) p_->AddSubmenu(title, m.shared()); return *this; }

        // ---- 图片（Image 不是 UIElement，是资源对象；由控件侧 SetImage 接收）----
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetImage(std::declval<std::shared_ptr<Image>>()))>>
        Ref& image(std::shared_ptr<Image> img) { if (p_) p_->SetImage(std::move(img)); return *this; }
        template<class U = T, class = std::void_t<decltype(std::declval<U&>().SetImageFit(true))>>
        Ref& imageFit(bool on = true) { if (p_) p_->SetImageFit(on); return *this; }

    private:
        std::shared_ptr<T> p_;
    };

    template<class U> inline std::shared_ptr<UIElement> toElem(const Ref<U>& r) { return r.shared(); }

    // ---- 通用工厂 ----
    template<class T, class... A> Ref<T> Make(A&&... a) {
        return Ref<T>(std::make_shared<T>(std::forward<A>(a)...));
    }
    // 把已有的 shared_ptr 接进 DSL 链：auto r = Wrap(someExistingLabel);
    template<class T> inline Ref<T> Wrap(std::shared_ptr<T> p) { return Ref<T>(std::move(p)); }

    // ---- 容器（直接接子元素）----
    template<class... C> Ref<ColumnBox>  Col(C&&... c)  { auto p = std::make_shared<ColumnBox>();  (p->AddChild(toElem(std::forward<C>(c))), ...); return Ref<ColumnBox>(p); }
    template<class... C> Ref<RowBox>     Row(C&&... c)  { auto p = std::make_shared<RowBox>();     (p->AddChild(toElem(std::forward<C>(c))), ...); return Ref<RowBox>(p); }

    // GridLayout 的单元格：Grid(Cell(1, 2, Lbl(L"x")), Cell(0, 0, btn, 1, 2))
    template<class C> inline CellSpec Cell(int row, int col, C&& c, int rowSpan = 1, int colSpan = 1) {
        return CellSpec{ row, col, rowSpan, colSpan, toElem(std::forward<C>(c)) };
    }
    // Grid(...) 同时接受“带坐标的 Cell(...)”与“裸子元素”：
    //   裸元素沿用旧行为——按出现顺序依次放在 (i, 0)。
    template<class C> inline void GridAdd(GridLayout& g, int& autoRow, C&& c) {
        if constexpr (std::is_same_v<std::decay_t<C>, CellSpec>) {
            if (c.child) g.AddChild(c.child, c.row, c.col, c.rowSpan, c.colSpan);
        }
        else {
            auto e = toElem(std::forward<C>(c));
            if (e) g.AddChild(e, autoRow++, 0);
        }
    }
    template<class... C> Ref<GridLayout> Grid(C&&... c) {
        auto p = std::make_shared<GridLayout>();
        int i = 0;
        (GridAdd(*p, i, std::forward<C>(c)), ...);
        return Ref<GridLayout>(p);
    }

    // ---- 基本控件短工厂 ----
    inline Ref<Label>  Lbl(const std::wstring& t = L""){ return Make<Label>(t); }
    inline Ref<Button> Btn(const std::wstring& t = L""){ return Make<Button>(t); }
    inline Ref<TextBox> Txt(const std::wstring& t = L""){ auto r = Make<TextBox>(); if (!t.empty()) r->SetText(t); return r; }
    inline Ref<TextEdit> Edit(const std::wstring& t = L""){ auto r = Make<TextEdit>(); if (!t.empty()) r->SetPlainText(t); return r; }
    inline Ref<ComboBox> Combo(){ return Make<ComboBox>(); }
    inline Ref<CheckBox> Check(const std::wstring& label = L""){ auto r = Make<CheckBox>(); if (!label.empty()) r->SetLabel(label); return r; }
    inline Ref<RadioButton> Radio(const std::wstring& t){ return Make<RadioButton>(t); }
    inline Ref<ToggleSwitch> Toggle(bool on = false){ return Make<ToggleSwitch>(on); }
    inline Ref<ZufyUI::Slider> Slider(){ return Make<ZufyUI::Slider>(); }
    inline Ref<NumberBox> Num(){ return Make<NumberBox>(); }
    inline Ref<ProgressBar> Progress(){ return Make<ProgressBar>(); }
    inline Ref<ProgressRing> Ring(){ return Make<ProgressRing>(); }
    inline Ref<ScrollViewer> Scroll(){ return Make<ScrollViewer>(); }
    inline Ref<SplitView> Split(){ return Make<SplitView>(); }
    inline Ref<TabView> Tabs(){ return Make<TabView>(); }
    inline Ref<ListView> List(){ return Make<ListView>(); }
    inline Ref<TableView> Table(){ return Make<TableView>(); }
    inline Ref<TreeView> Tree(){ return Make<TreeView>(); }
    inline Ref<BarChart> Bar(){ return Make<BarChart>(); }
    inline Ref<LineChart> Line(){ return Make<LineChart>(); }
    inline Ref<PieChart> Pie(){ return Make<PieChart>(); }
    inline Ref<ZufyUI::Card> Card(){ return Make<ZufyUI::Card>(); }
    inline Ref<ZufyUI::Expander> Expander(const std::wstring& title = L"", const std::wstring& sub = L""){ return Make<ZufyUI::Expander>(title, sub); }
    inline Ref<PageHost> Pages(){ return Make<PageHost>(); }
    inline Ref<ZufyUI::Page> Page(){ return Make<ZufyUI::Page>(); }
    inline Ref<ZufyUI::Menu> Menu(){ return Make<ZufyUI::Menu>(); }
    inline Ref<SettingsList> Settings(){ return Make<SettingsList>(); }
    inline Ref<ZufyUI::MenuBar> MenuBar(){ return Make<ZufyUI::MenuBar>(); }
    inline Ref<ZufyUI::StatusBar> StatusBar(){ return Make<ZufyUI::StatusBar>(); }
    inline Ref<ZufyUI::RadioGroup> RadioGroup(){ return Make<ZufyUI::RadioGroup>(); }
    inline Ref<ZufyUI::ScrollBar> ScrollBar(bool vertical = true){ return Make<ZufyUI::ScrollBar>(vertical); }
    // 图标字形（ZufyUIIcons.h）：Glyph(Icon::Save) / Glyph(Icon::Save, 18.0f, Color::White)
    inline Ref<FontIcon> Glyph(Icon icon, float size = FontIcon::DefaultSize){ return Make<FontIcon>(icon, size); }
    inline Ref<FontIcon> Glyph(Icon icon, float size, Color color){ return Make<FontIcon>(icon, size, color); }

    // ---------- 窗口构建器（链式配置 + run）----------
    //   auto app = dsl::App();  app.title(...).size(...).backdrop(...).root(Col(...)).run();
    class WindowApp {
    public:
        WindowApp() : win_(std::make_unique<ZufyUI::Window>()) {}
        WindowApp(const WindowApp&) = delete;
        WindowApp& operator=(const WindowApp&) = delete;
        WindowApp(WindowApp&&) = default;                 // 可移动：能从容返回 / 放进容器
        WindowApp& operator=(WindowApp&&) = default;

        WindowApp& size(int w, int h) { w_ = w; h_ = h; return *this; }
        WindowApp& title(const std::wstring& t) { title_ = t; return *this; }
        WindowApp& resizable(bool on = true) { resizable_ = on; resizableSet_ = true; return *this; }
        WindowApp& corner(ZufyUI::Window::WindowCorner c) { corner_ = c; cornerSet_ = true; return *this; }
        // 不带 tint = 底色跟随主题；带 tint = 显式指定并锁定（含 0x00000000 全透明）
        WindowApp& backdrop(ZufyUI::Backdrop b) { backdrop_ = b; backdropSet_ = true; backdropTintSet_ = false; return *this; }
        WindowApp& backdrop(ZufyUI::Backdrop b, DWORD tint) { backdrop_ = b; tint_ = tint; backdropSet_ = true; backdropTintSet_ = true; return *this; }
        WindowApp& root(std::shared_ptr<Layout> l) { root_ = std::move(l); return *this; }
        // 任意 Layout 子类都能直接当根：Col / Row / Grid 一视同仁
        //   （原先只有 root(Ref<ColumnBox>)，Row/Grid 会因“需要两次用户定义转换”而编译失败）
        template<class L, class = std::enable_if_t<std::is_base_of_v<Layout, L>>>
        WindowApp& root(const Ref<L>& l) { root_ = l.shared(); return *this; }

        WindowApp& titleBar(std::shared_ptr<UIElement> b) { titleBar_ = std::move(b); return *this; }
        WindowApp& menuBar(std::shared_ptr<UIElement> b) { menuBar_ = std::move(b); return *this; }
        WindowApp& statusBar(std::shared_ptr<UIElement> b) { statusBar_ = std::move(b); return *this; }
        // 同上：Ref<T> 直接传，省掉 .shared()
        template<class E, class = std::enable_if_t<std::is_base_of_v<UIElement, E>>>
        WindowApp& titleBar(const Ref<E>& b) { titleBar_ = b.shared(); return *this; }
        template<class E, class = std::enable_if_t<std::is_base_of_v<UIElement, E>>>
        WindowApp& menuBar(const Ref<E>& b) { menuBar_ = b.shared(); return *this; }
        template<class E, class = std::enable_if_t<std::is_base_of_v<UIElement, E>>>
        WindowApp& statusBar(const Ref<E>& b) { statusBar_ = b.shared(); return *this; }

        // 不显示窗口（默认显示）：run() 会 Show()，需要后台/隐藏启动时打开这个开关
        WindowApp& hidden(bool on = true) { hidden_ = on; return *this; }

        // ---- 窗口级配置（都在 run() 里 Create 之后套用）----
        WindowApp& minSize(int w, int h) { minW_ = w; minH_ = h; minSizeSet_ = true; return *this; }
        WindowApp& position(int x, int y) { x_ = x; y_ = y; posSet_ = true; return *this; }
        WindowApp& frameRateLimit(int fps) { fps_ = fps; fpsSet_ = true; return *this; }
        WindowApp& theme(const ZufyUI::Theme& t) { theme_ = t; themeSet_ = true; return *this; }
        WindowApp& useAppTheme(bool on = true) { useAppTheme_ = on; useAppThemeSet_ = true; return *this; }
        WindowApp& dropTarget(bool on = true) { dropTarget_ = on; dropTargetSet_ = true; return *this; }
        WindowApp& icon(HICON bigIcon, HICON smallIcon) { bigIcon_ = bigIcon; smallIcon_ = smallIcon; iconSet_ = true; return *this; }
        WindowApp& titleBarVisible(bool on = true) { titleBarVisible_ = on; titleBarVisibleSet_ = true; return *this; }
        WindowApp& showTitleText(bool on = true) { showTitleText_ = on; showTitleTextSet_ = true; return *this; }
        WindowApp& menuBarInTitleBar(bool on = true) { menuBarInTitleBar_ = on; menuBarInTitleBarSet_ = true; return *this; }

        // 窗口信号（与 Ref::on 同形）：dsl::App().on<&ZufyUI::Window::Closed>([]{ ... })
        //   Window 没有 Connect，信号是裸成员，所以用成员指针形式。
        template<auto SigMem, class F> WindowApp& on(F&& f) {
            (win_.get()->*SigMem).connect(std::forward<F>(f));
            return *this;
        }

        ZufyUI::Window& window() { return *win_; }
        int run() {
            if (!win_) return 1;   // WindowApp 可移动：移动后源对象的 win_ 为 null，直接调 run() 会崩
            if (!win_->Create(w_, h_, title_)) return 1;
            if (resizableSet_) win_->SetResizable(resizable_);
            if (cornerSet_) win_->SetWindowCorner(corner_);
            if (backdropSet_) { if (backdropTintSet_) win_->SetBackdrop(backdrop_, tint_); else win_->SetBackdrop(backdrop_); }
            if (minSizeSet_) win_->SetMinSize(minW_, minH_);
            if (posSet_) win_->SetPosition(x_, y_);
            if (fpsSet_) win_->SetFrameRateLimit(fps_);
            if (useAppThemeSet_) win_->SetUseAppTheme(useAppTheme_);
            if (themeSet_) win_->SetTheme(theme_);
            if (dropTargetSet_) win_->SetDropTargetEnabled(dropTarget_);
            if (iconSet_) win_->SetIcon(bigIcon_, smallIcon_);
            if (titleBarVisibleSet_) win_->SetTitleBarVisible(titleBarVisible_);
            if (showTitleTextSet_) win_->SetShowTitleText(showTitleText_);
            if (menuBarInTitleBarSet_) win_->SetMenuBarInTitleBar(menuBarInTitleBar_);
            if (titleBar_) win_->SetCustomTitleBar(titleBar_);
            if (menuBar_) win_->SetMenuBar(menuBar_);
            if (statusBar_) win_->SetStatusBar(statusBar_);
            if (root_) win_->SetRootLayout(root_);
            // Window::Create 不显示窗口（何时显示由应用决定），所以这里必须补 Show()，
            // 否则 run() 会跑一个没有窗口的消息循环（表现为“卡死”）。
            if (!hidden_) win_->Show();
            win_->Run();
            return 0;
        }

    private:
        std::unique_ptr<ZufyUI::Window> win_;   // 堆上持有 → WindowApp 可移动（Window 自身既不可拷贝也不可移动）
        int w_ = 1000, h_ = 700;
        std::wstring title_ = L"ZufyUI";
        bool resizable_ = true, resizableSet_ = false;
        bool hidden_ = false;
        int minW_ = 0, minH_ = 0; bool minSizeSet_ = false;
        int x_ = 0, y_ = 0; bool posSet_ = false;
        int fps_ = 0; bool fpsSet_ = false;
        bool useAppTheme_ = true, useAppThemeSet_ = false;
        bool dropTarget_ = true, dropTargetSet_ = false;
        bool titleBarVisible_ = true, titleBarVisibleSet_ = false;
        bool showTitleText_ = true, showTitleTextSet_ = false;
        bool menuBarInTitleBar_ = true, menuBarInTitleBarSet_ = false;
        HICON bigIcon_ = nullptr, smallIcon_ = nullptr; bool iconSet_ = false;
        ZufyUI::Theme theme_; bool themeSet_ = false;
        ZufyUI::Window::WindowCorner corner_ = ZufyUI::Window::WindowCorner::Round; bool cornerSet_ = false;
        ZufyUI::Backdrop backdrop_ = ZufyUI::Backdrop::None; DWORD tint_ = 0; bool backdropSet_ = false; bool backdropTintSet_ = false;
        std::shared_ptr<Layout> root_;
        std::shared_ptr<UIElement> titleBar_, menuBar_, statusBar_;
    };
    inline WindowApp App() { return WindowApp{}; }

} // namespace dsl
} // namespace ZufyUI
