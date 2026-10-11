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

namespace ZufyUI {
namespace dsl {

    template<class T> class Ref;   // 前向

    // ---- 把 Ref<U> / shared_ptr<U> / nullptr 统一成 shared_ptr<UIElement> ----
    template<class U> std::shared_ptr<UIElement> toElem(const Ref<U>& r);
    template<class U, std::enable_if_t<std::is_base_of_v<UIElement, U>, int> = 0>
    inline std::shared_ptr<UIElement> toElem(const std::shared_ptr<U>& p) { return p; }
    inline std::shared_ptr<UIElement> toElem(std::nullptr_t) { return {}; }

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
        // 容器：add(child...)（Label/Button 的内联子控件也可用）
        template<class... C> Ref& add(C&&... c) {
            if (p_) (p_->AddChild(toElem(std::forward<C>(c))), ...);
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

    private:
        std::shared_ptr<T> p_;
    };

    template<class U> inline std::shared_ptr<UIElement> toElem(const Ref<U>& r) { return r.shared(); }

    // ---- 通用工厂 ----
    template<class T, class... A> Ref<T> Make(A&&... a) {
        return Ref<T>(std::make_shared<T>(std::forward<A>(a)...));
    }

    // ---- 容器（直接接子元素）----
    template<class... C> Ref<ColumnBox>  Col(C&&... c)  { auto p = std::make_shared<ColumnBox>();  (p->AddChild(toElem(std::forward<C>(c))), ...); return Ref<ColumnBox>(p); }
    template<class... C> Ref<RowBox>     Row(C&&... c)  { auto p = std::make_shared<RowBox>();     (p->AddChild(toElem(std::forward<C>(c))), ...); return Ref<RowBox>(p); }
    template<class... C> Ref<GridLayout> Grid(C&&... c) { auto p = std::make_shared<GridLayout>(); int i = 0; (p->AddChild(toElem(std::forward<C>(c)), i++, 0), ...); return Ref<GridLayout>(p); }

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
    inline Ref<ZufyUI::MenuBar> MenuBar(){ return Make<ZufyUI::MenuBar>(); }
    inline Ref<ZufyUI::StatusBar> StatusBar(){ return Make<ZufyUI::StatusBar>(); }

} // namespace dsl
} // namespace ZufyUI
