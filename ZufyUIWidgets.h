#pragma once
#include "ZufyUI.h"
#include "ZufyUIImages.h"

namespace ZufyUI {

    enum class TextHAlign { Left, Center, Right };

    inline void DrawTextWithEllipsis(ID2D1RenderTarget* rt,
        const std::wstring& text,
        const D2D1_RECT_F& rect,
        const D2D1_COLOR_F& color,
        const FontSpec& spec,
        ComPtr<ID2D1SolidColorBrush>& textBrush,
        IDWriteTextFormat* textFormat = nullptr,
        bool forceNoWrap = false,
        TextHAlign align = TextHAlign::Left) {
        if (text.empty() || !rt) return;

        IDWriteTextFormat* fmt = textFormat;
        if (!fmt) fmt = FontManager::Instance().GetFormat(spec);
        if (!fmt) return;

        IDWriteFactory* dwriteFactory = FontManager::Instance().GetFactory();
        if (!dwriteFactory) return;

        float maxWidth = rect.right - rect.left;
        float maxHeight = rect.bottom - rect.top;
        if (maxWidth <= 0.0f || maxHeight <= 0.0f) return;

        FontManager& fm = FontManager::Instance();
        // 热路径：显示布局缓存命中（key=原文本+宽高+格式）→ 直接画，跳过整段"测量 + 二分截断"
        ComPtr<IDWriteTextLayout> finalLayout = fm.GetDisplayLayout(text, fmt, maxWidth, maxHeight, forceNoWrap);
        if (!finalLayout) {
            // 未命中：原始布局（也走缓存）测宽 → 超宽二分截断（中间 layout 不缓存）
            ComPtr<IDWriteTextLayout> measureLayout = fm.GetRawLayout(text, fmt, maxWidth, maxHeight, forceNoWrap);
            if (!measureLayout) return;
            DWRITE_TEXT_METRICS metrics;
            measureLayout->GetMetrics(&metrics);
            std::wstring displayText = text;
            if (metrics.width > maxWidth && displayText.length() > 3) {
                const std::wstring suffix = L"...";
                int len = (int)displayText.length();
                int lo = 0, hi = len - 1, best = -1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    std::wstring test;                       // reserve+assign 消掉 substr/+ 的临时分配
                    test.reserve((size_t)mid + suffix.size());
                    test.assign(displayText, 0, mid);
                    test += suffix;
                    ComPtr<IDWriteTextLayout> testLayout;
                    dwriteFactory->CreateTextLayout(test.c_str(), (UINT32)test.length(),
                        fmt, maxWidth, maxHeight, &testLayout);
                    if (!testLayout) break;
                    if (forceNoWrap) testLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    DWRITE_TEXT_METRICS tm;
                    testLayout->GetMetrics(&tm);
                    if (tm.width <= maxWidth) { best = mid; lo = mid + 1; }
                    else hi = mid - 1;
                }
                displayText = (best >= 0) ? (displayText.substr(0, best) + suffix) : suffix;
                finalLayout = fm.GetRawLayout(displayText, fmt, maxWidth, maxHeight, forceNoWrap);
            }
            else {
                finalLayout = measureLayout;   // 未截断 → 复用原始布局
            }
            if (!finalLayout) return;
            fm.CacheDisplayLayout(text, fmt, maxWidth, maxHeight, forceNoWrap, finalLayout.Get());
        }

        if (!textBrush) rt->CreateSolidColorBrush(color, textBrush.GetAddressOf());
        else textBrush->SetColor(color);

        DWRITE_TEXT_METRICS fm2{};
        finalLayout->GetMetrics(&fm2);
        float drawX = rect.left;
        if (align == TextHAlign::Center) drawX = rect.left + (maxWidth - fm2.width) / 2.0f;
        else if (align == TextHAlign::Right) drawX = rect.left + (maxWidth - fm2.width);
        if (drawX < rect.left) drawX = rect.left;
        rt->DrawTextLayout(D2D1::Point2F(Snap(drawX), Snap(rect.top)), finalLayout.Get(), textBrush.Get());
    }

    // ============================================================================
    // 图标系统（字形图标）—— 下沉到核心，任何控件（尤其基于 Label 的）都能用
    // ----------------------------------------------------------------------------
    // 用系统图标字体的字形当图标：Win11 = "Segoe Fluent Icons"，Win10 = "Segoe MDL2 Assets"，
    // 两者码点基本一致。运行期探测可用性，缺前者自动回退后者。
    //   - Icon 枚举值**就是字体码点本身**（`Icon::Add = 0xE710`），不维护平行码点表 → 不会漂移。
    //   - FontIcon 只画一个字形；Label::SetIcon 给"带文本的标签"加前置字形图标。
    // ============================================================================

    // ---- 图标字体族（运行期探测一次）----
    inline const std::wstring& IconFontFamily() {
        static const std::wstring family = []() -> std::wstring {
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            if (factory) {
                ComPtr<IDWriteFontCollection> coll;
                if (SUCCEEDED(factory->GetSystemFontCollection(&coll)) && coll) {
                    UINT32 index = 0; BOOL exists = FALSE;
                    if (SUCCEEDED(coll->FindFamilyName(L"Segoe Fluent Icons", &index, &exists)) && exists)
                        return L"Segoe Fluent Icons";
                }
            }
            return L"Segoe MDL2 Assets";
        }();
        return family;
    }

    // ---- 图标枚举：枚举值 = 字体码点（括号内为 MDL2 Assets 名称，便于对照/查图）----
    enum class Icon : unsigned short {
        None = 0x0000,

        // 常规操作
        Add = 0xE710,          // Add
        Remove = 0xE738,       // Remove
        Delete = 0xE74D,       // Delete
        Edit = 0xE70F,         // Edit
        Save = 0xE74E,         // Save
        Open = 0xE8E5,         // OpenFile
        Copy = 0xE8C8,         // Copy
        Cut = 0xE8C6,          // Cut
        Paste = 0xE77F,        // Paste
        Undo = 0xE7A7,         // Undo
        Redo = 0xE7A6,         // Redo
        Refresh = 0xE72C,      // Refresh
        Search = 0xE721,       // Search
        Filter = 0xE71C,       // Filter
        Settings = 0xE713,     // Settings
        More = 0xE712,         // More
        Close = 0xE8BB,        // ChromeClose
        Cancel = 0xE711,       // Cancel
        Check = 0xE73E,        // CheckMark
        Share = 0xE72D,        // Share
        Download = 0xE896,     // Download
        Upload = 0xE898,       // Upload
        Link = 0xE71B,         // Link
        Attach = 0xE723,       // Attach
        Send = 0xE724,         // Send
        Pin = 0xE718,          // Pin
        Sort = 0xE8CB,         // Sort
        Sync = 0xE895,         // Sync

        // 导航
        Home = 0xE80F,         // Home
        Back = 0xE72B,         // Back
        Forward = 0xE72A,      // Forward
        ChevronDown = 0xE70D,  // ChevronDown
        ChevronUp = 0xE70E,    // ChevronUp
        ChevronLeft = 0xE76B,  // ChevronLeft
        ChevronRight = 0xE76C, // ChevronRight
        GlobalNav = 0xE700,    // GlobalNavButton（汉堡菜单）
        AllApps = 0xE71D,      // AllApps
        Zoom = 0xE71E,         // Zoom

        // 状态 / 提示
        Info = 0xE946,         // Info
        Warning = 0xE7BA,      // Warning
        Error = 0xE783,        // ErrorBadge
        Success = 0xE930,      // Completed
        Help = 0xE897,         // Help
        Lock = 0xE72E,         // Lock
        Unlock = 0xE785,       // Unlock
        View = 0xE890,         // View（眼睛）

        // 对象
        Person = 0xE77B,       // Contact
        Mail = 0xE715,         // Mail
        Phone = 0xE717,        // Phone
        Calendar = 0xE787,     // Calendar
        Clock = 0xE823,        // Clock
        Folder = 0xE8B7,       // Folder
        File = 0xE7C3,         // Page
        Image = 0xE8B9,        // Picture
        Favorite = 0xE734,     // FavoriteStar
        FavoriteFill = 0xE735, // FavoriteStarFill
        Play = 0xE768,         // Play
        Pause = 0xE769,        // Pause
        Stop = 0xE71A,         // Stop
        Volume = 0xE767,       // Volume
    };

    // 取字形字符串（单码点）
    inline std::wstring IconGlyph(Icon icon) {
        unsigned short code = (unsigned short)icon;
        if (code == 0) return std::wstring();
        return std::wstring(1, (wchar_t)code);
    }

    // ---------- 标签（支持对齐、换行/省略号，最终修正版） ----------
    class Label : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Text; }
        std::wstring DefaultAccessibleName() const override { return text_; }
        enum class TextOverflow {
            Wrap,      // 自动换行
            Ellipsis   // 单行，超出显示省略号
        };

        enum class HAlign { Left, Center, Right };
        enum class VAlign { Top, Center, Bottom };

        inline static Color DefaultTextColor = Color::FromArgb(255, 0, 0, 0);
        inline static TextOverflow DefaultOverflow = TextOverflow::Ellipsis;
        inline static HAlign DefaultHAlign = HAlign::Left;
        inline static VAlign DefaultVAlign = VAlign::Center;   // 默认垂直居中
        inline static float DefaultHorizontalStretchWeight = 0.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;
        inline static Color DefaultDisabledColor = Color::FromArgb(255, 150, 150, 150);
        inline static FontSpec DefaultFontSpec = []() {
            FontSpec s;
            s.size = 16.0f;
            return s;
            }();

        Label(const std::wstring& text = L"Label") : text_(text), textColor_(DefaultTextColor),
            overflow_(DefaultOverflow), hAlign_(DefaultHAlign), vAlign_(DefaultVAlign) {}

        void SetText(const std::wstring& text) {
            text_ = text;
            InvalidateLayout();
            RequestRepaint();
        }
        // 虚拟列表 / 元素池"重绑"专用：只置自身布局脏（不冒泡到根），且不 RequestRepaint（重绘由容器驱动）。
        void SetTextFast(const std::wstring& text) { text_ = text; InvalidateLayoutSelf(); }
        std::wstring GetText() const { return text_; }
        void SetTextColor(Color color) {
            if (textColor_.r == color.r && textColor_.g == color.g &&
                textColor_.b == color.b && textColor_.a == color.a) return;   // 颜色未变短路（池化每帧调用不重建刷子）
            textColor_ = color; textBrush_.Reset(); RequestRepaint();
        }
        Color GetTextColor() const { return textColor_; }
        void SetTextOverflow(TextOverflow mode) { overflow_ = mode; InvalidateLayout(); RequestRepaint(); }
        TextOverflow GetTextOverflow() const { return overflow_; }
        // 悬停时：若文本被省略（Ellipsis 且放不下），自动用完整文本当 tooltip；用户设过的 tooltip 优先
        std::wstring GetToolTip() const override {
            std::wstring custom = UIElement::GetToolTip();
            if (!custom.empty()) return custom;
            return truncated_ ? text_ : std::wstring();
        }
        void SetAlignment(HAlign hAlign, VAlign vAlign) {
            if (hAlign_ == hAlign && vAlign_ == vAlign) return;   // 未变短路（表格/树每帧调用不再触发布局）
            hAlign_ = hAlign;
            vAlign_ = vAlign;
            InvalidateLayout();
            RequestRepaint();
        }
        HAlign GetHorizontalAlignment() const { return hAlign_; }
        VAlign GetVerticalAlignment() const { return vAlign_; }
        void SetPadding(const Thickness& p) {
            if (padding_.left == p.left && padding_.top == p.top &&
                padding_.right == p.right && padding_.bottom == p.bottom) return;   // 未变短路
            padding_ = p; InvalidateLayout(); RequestRepaint();
        }
        // 背景色（a=0 表示不画）+ 圆角半径
        void SetBackgroundColor(Color c) { bgColor_ = c; bgBrush_.Reset(); RequestRepaint(); }
        Color GetBackgroundColor() const { return bgColor_; }
        void SetBackgroundCornerRadius(float r) { bgCornerRadius_ = r; RequestRepaint(); }
        float GetBackgroundCornerRadius() const { return bgCornerRadius_; }
        void SetPadding(float all) { padding_ = Thickness(all, all, all, all); InvalidateLayout(); RequestRepaint(); }
        Thickness GetPadding() const { return padding_; }
        void SetLineSpacing(float spacing) { lineSpacing_ = spacing; InvalidateLayout(); RequestRepaint(); }
        float GetLineSpacing() const { return lineSpacing_; }
        void SetMaxLines(int n) { maxLines_ = n; InvalidateLayout(); RequestRepaint(); }
        int GetMaxLines() const { return maxLines_; }
        Size GetDesiredSize() { return Measure(Size(FLT_MAX, FLT_MAX)); }

        // ---------------- 图标 / 图片 ----------------
        void SetImage(std::shared_ptr<Image> image) { image_ = std::move(image); InvalidateLayout(); RequestRepaint(); }
        std::shared_ptr<Image> GetImage() const { return image_; }
        void SetIconSize(float w, float h) { iconSize_ = Size(w, h); InvalidateLayout(); RequestRepaint(); }
        Size GetIconSize() const { return iconSize_; }
        void SetIconSpacing(float s) { iconSpacing_ = max(0.0f, s); InvalidateLayout(); RequestRepaint(); }
        float GetIconSpacing() const { return iconSpacing_; }

        // 字体字形图标（前置）：与图片图标共用同一槽位（若同时设了图片则图片优先）。
        // size<=0 表示跟随本标签字号。图标颜色默认取文字色。
        void SetIcon(Icon icon, float size = 0.0f) {
            if (glyphIcon_ != icon || glyphIconSize_ != size) {
                glyphIcon_ = icon; glyphIconSize_ = size;
                InvalidateLayout(); RequestRepaint();
            }
        }
        Icon GetIcon() const { return glyphIcon_; }
        void SetIconColor(Color c) { glyphIconColor_ = c; RequestRepaint(); }
        Color GetIconColor() const { return glyphIconColor_; }

        // ---------------- 嵌套子控件（内联横排：图标 + 文本 + 子控件） ----------------
        void AddChild(std::shared_ptr<UIElement> child) {
            if (!child || child.get() == this) return;
            child->SetParent(this);
            children_.push_back(std::move(child));
            InvalidateLayout(); RequestRepaint();
        }
        void ClearChildren() {
            for (auto& c : children_) if (c) { c->SetParent(nullptr); }
            children_.clear();
            MarkChildrenDirty();   // 移除子元素：本地标记（SetParent(nullptr) 不会标到旧父）
            InvalidateLayout(); RequestRepaint();
        }
        size_t GetChildCount() const { return children_.size(); }

        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& c : children_) if (c) childrenView_.push_back(c.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }
        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& c : children_) if (c) c->AttachWindowRecursive(w);
        }

        static void SetDefaultTextColor(Color color) { DefaultTextColor = color; }
        static void SetDefaultFontSize(float size) { Label::DefaultFontSpec.size = size; }
        static void SetDefaultOverflow(TextOverflow mode) { DefaultOverflow = mode; }
        static void SetDefaultAlignment(HAlign hAlign, VAlign vAlign) { DefaultHAlign = hAlign; DefaultVAlign = vAlign; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        std::optional<FontSpec> GetTypeDefaultFont() const override {
            return Label::DefaultFontSpec;
        }

        Size MeasureOverride(const Size& availableSize) override {
            float padW = padding_.left + padding_.right;
            float padH = padding_.top + padding_.bottom;

            // 图标尺寸（图片优先；否则用字体字形图标）
            float iw = 0, ih = 0;
            if (image_ && !image_->IsNull()) {
                iw = iconSize_.width > 0 ? iconSize_.width : (float)image_->Width();
                ih = iconSize_.height > 0 ? iconSize_.height : (float)image_->Height();
            }
            else if (glyphIcon_ != Icon::None) {
                float fsz = GlyphSize();
                std::wstring glyph = IconGlyph(glyphIcon_);
                IDWriteTextFormat* ifmt = GetGlyphFormat(fsz);
                if (!glyph.empty() && ifmt) {
                    auto gl = FontManager::Instance().GetStyledLayout(glyph, ifmt, 10000.0f, 10000.0f, true, 0, 0, 0.0f, 0);
                    if (gl) { DWRITE_TEXT_METRICS m{}; gl->GetMetrics(&m); iw = m.width; ih = m.height; }
                }
                if (iw <= 0.0f) { iw = fsz; ih = fsz; }
            }

            // 文本尺寸
            float textW = 0, textH = 0;
            if (!text_.empty()) {
                IDWriteFactory* factory = FontManager::Instance().GetFactory();
                IDWriteTextFormat* fmt = GetFontFormat();
                if (factory && fmt) {
                    if (overflow_ == TextOverflow::Wrap && availableSize.width != FLT_MAX && availableSize.width > 0) {
                        float availW = max(0.0f, availableSize.width - padW - (iw > 0 ? iw + iconSpacing_ : 0.0f));
                        // 走全局布局缓存（wrap）；测量只取 metrics，装饰保持旧行为（不加行距/多行）
                        ComPtr<IDWriteTextLayout> tempLayout = FontManager::Instance().GetStyledLayout(
                            text_, fmt, availW, 10000.0f, false, (int)hAlign_, (int)vAlign_, 0.0f, 0);
                        if (tempLayout) {
                            DWRITE_TEXT_METRICS metrics;
                            tempLayout->GetMetrics(&metrics);
                            textW = min(availW, metrics.width);
                            textH = metrics.height;
                        }
                    }
                    else {
                        // 走全局布局缓存（no-wrap）
                        ComPtr<IDWriteTextLayout> layout = FontManager::Instance().GetStyledLayout(
                            text_, fmt, 10000.0f, 10000.0f, true, (int)hAlign_, (int)vAlign_, 0.0f, 0);
                        if (layout) {
                            DWRITE_TEXT_METRICS metrics;
                            layout->GetMetrics(&metrics);
                            textW = metrics.width;
                            textH = metrics.height;
                        }
                    }
                }
            }

            // 子控件尺寸
            float cw = 0, ch = 0;
            childSizes_.clear();
            for (auto& c : children_) {
                if (!c) { childSizes_.push_back(Size(0, 0)); continue; }
                Size s = c->Measure(Size(FLT_MAX, FLT_MAX));
                childSizes_.push_back(s);
                cw += s.width;
                ch = max(ch, s.height);
            }
            if (!children_.empty()) cw += iconSpacing_ * (float)children_.size();

            float gapIconText = (iw > 0 && textW > 0) ? iconSpacing_ : 0.0f;
            float gapTextChild = (textW > 0 && cw > 0) ? iconSpacing_ : ((iw > 0 && cw > 0 && textW <= 0) ? iconSpacing_ : 0.0f);

            measuredIconW_ = iw; measuredIconH_ = ih;
            measuredTextW_ = textW; measuredTextH_ = textH;

            return Size(padW + iw + gapIconText + textW + gapTextChild + cw,
                padH + max(max(ih, textH), ch));
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            const float padW = padding_.left + padding_.right;
            const float availW = max(0.0f, finalRect.width - padW);
            const float iconW = measuredIconW_;
            const bool hasChildren = !children_.empty();

            float childrenW = 0.0f;
            for (auto& s : childSizes_) childrenW += s.width;
            if (hasChildren) childrenW += iconSpacing_ * (float)children_.size();

            // 非文本内容宽度（图标 + 子控件 + 它们与文本之间的间隔）
            const float otherW = iconW
                + ((iconW > 0.0f && (measuredTextW_ > 0.0f || hasChildren)) ? iconSpacing_ : 0.0f)
                + childrenW;
            const float textAvail = max(0.0f, availW - otherW);

            // 文本实际宽度 / 交给文本布局的盒宽
            float textW = 0.0f, textBoxW = 0.0f;
            if (measuredTextW_ > 0.0f) {
                textW = min(measuredTextW_, textAvail);
                textBoxW = (overflow_ == TextOverflow::Wrap) ? textAvail : textW;
            }
            truncated_ = (overflow_ == TextOverflow::Ellipsis) && (measuredTextW_ > textAvail + 0.5f);

            // 把「图标 + 文本 + 子控件」当作一个整体做水平对齐（这样按钮里图标会和居中文字贴在一起）
            const float contentW = otherW + textW;
            float startX = finalRect.x + padding_.left;
            if (hAlign_ == HAlign::Center) startX += max(0.0f, (availW - contentW) * 0.5f);
            else if (hAlign_ == HAlign::Right) startX += max(0.0f, availW - contentW);

            float x = startX;
            const float cy = finalRect.y + finalRect.height * 0.5f;
            iconRect_ = D2D1::RectF(0, 0, 0, 0);
            if (iconW > 0.0f) {
                float iy = cy - measuredIconH_ * 0.5f;
                iconRect_ = D2D1::RectF(x, iy, x + iconW, iy + measuredIconH_);
                x += iconW;
                if (measuredTextW_ > 0.0f || hasChildren) x += iconSpacing_;
            }
            textLeft_ = x;
            textBoxW_ = textBoxW;
            x += textW;
            for (size_t i = 0; i < children_.size(); ++i) {
                auto& c = children_[i];
                if (!c) continue;
                x += iconSpacing_;
                Size s = (i < childSizes_.size()) ? childSizes_[i] : Size(0, 0);
                c->Arrange(Rect(x, cy - s.height * 0.5f, s.width, s.height));
                x += s.width;
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            // 背景（带圆角）
            if (bgColor_.a > 0.0f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(bgColor_.ToD2D(), bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(bgColor_.ToD2D());
                if (bgBrush_) {
                    D2D1_RECT_F br = arrangedRect_.ToD2D();
                    if (bgCornerRadius_ > 0.0f)
                        rt->FillRoundedRectangle(D2D1::RoundedRect(br, bgCornerRadius_, bgCornerRadius_), bgBrush_.Get());
                    else
                        rt->FillRectangle(br, bgBrush_.Get());
                }
            }

            // 图标（可与文字/子控件共存；即使没有文字也绘制）
            if (image_ && !image_->IsNull() && iconRect_.right > iconRect_.left) {
                Image::DrawOptions o;
                image_->Draw(rt, iconRect_, o);
            }
            else if (rt && glyphIcon_ != Icon::None && iconRect_.right > iconRect_.left) {
                // 字体字形图标：居中画进图标槽（颜色默认取文字色）
                float fsz = GlyphSize();
                std::wstring glyph = IconGlyph(glyphIcon_);
                IDWriteTextFormat* ifmt = GetGlyphFormat(fsz);
                if (!glyph.empty() && ifmt) {
                    float gw = iconRect_.right - iconRect_.left;
                    float gh = iconRect_.bottom - iconRect_.top;
                    auto gl = FontManager::Instance().GetStyledLayout(glyph, ifmt, gw, gh, true, 1, 1, 0.0f, 0);
                    if (gl) {
                        Color ic = (glyphIconColor_.a > 0.0f) ? glyphIconColor_ : textColor_;
                        if (!glyphBrush_) rt->CreateSolidColorBrush(ic.ToD2D(), glyphBrush_.GetAddressOf());
                        else glyphBrush_->SetColor(ic.ToD2D());
                        if (!IsEffectivelyEnabled() && glyphBrush_) glyphBrush_->SetColor(DefaultDisabledColor.ToD2D());
                        if (glyphBrush_) rt->DrawTextLayout(D2D1::Point2F(Snap(iconRect_.left), Snap(iconRect_.top)), gl.Get(), glyphBrush_.Get());
                    }
                }
            }
            if (text_.empty()) return;

            IDWriteTextFormat* fmt = GetFontFormat();
            if (!fmt) return;
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            if (!factory) return;

            D2D1_RECT_F rect = arrangedRect_.ToD2D();
            rect.top = arrangedRect_.y + padding_.top;
            rect.bottom = arrangedRect_.y + arrangedRect_.height - padding_.bottom;
            rect.left = (textLeft_ > 0.0f) ? textLeft_ : (arrangedRect_.x + padding_.left);
            rect.right = arrangedRect_.x + arrangedRect_.width - padding_.right;
            // 有图标/子控件时，文本盒按"实际文本宽度"收窄，让文字紧贴图标（整体对齐已在 Arrange 里算好）
            if (textBoxW_ > 0.0f && rect.left + textBoxW_ < rect.right) rect.right = rect.left + textBoxW_;
            if (rect.right < rect.left) rect.right = rect.left;
            if (rect.bottom < rect.top) rect.bottom = rect.top;
            FontManager& fm = FontManager::Instance();
            const float boxW = rect.right - rect.left;
            const float boxH = rect.bottom - rect.top;
            const int ha = (int)hAlign_, va = (int)vAlign_;
            ComPtr<IDWriteTextLayout> layout = nullptr;

            if (overflow_ == TextOverflow::Ellipsis) {
                // ① 命中“显示布局”缓存 → 跳过整段截断计算（key 含文本/尺寸/对齐/行距/多行）
                layout = fm.GetStyledDisplayLayout(text_, fmt, boxW, boxH, true, ha, va, lineSpacing_, maxLines_);
                if (!layout) {
                    // ② 未命中：先量自然宽度判断是否需要截断，再二分找最长可显示前缀
                    std::wstring displayText = text_;
                    ComPtr<IDWriteTextLayout> measureLayout = fm.GetStyledLayout(
                        text_, fmt, boxW, boxH, true, ha, va, 0.0f, 0);   // 只判宽：不加行距/多行装饰
                    if (measureLayout) {
                        DWRITE_TEXT_METRICS metrics;
                        measureLayout->GetMetrics(&metrics);
                        if (metrics.width > boxW && displayText.length() > 3) {
                            const std::wstring suffix = L"...";
                            int len = (int)displayText.length();
                            int lo = 0, hi = len - 1, best = -1;
                            while (lo <= hi) {                       // 二分（与 DrawTextWithEllipsis 对齐）
                                int mid = (lo + hi) / 2;
                                std::wstring test;
                                test.reserve((size_t)mid + suffix.size());
                                test.assign(displayText, 0, mid);
                                test += suffix;
                                ComPtr<IDWriteTextLayout> testLayout;   // 中间结果不入全局缓存，避免污染
                                factory->CreateTextLayout(test.c_str(), (UINT32)test.length(), fmt, boxW, boxH, &testLayout);
                                if (!testLayout) break;
                                testLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                                DWRITE_TEXT_METRICS tm;
                                testLayout->GetMetrics(&tm);
                                if (tm.width <= boxW) { best = mid; lo = mid + 1; }
                                else hi = mid - 1;
                            }
                            displayText = (best >= 0) ? (displayText.substr(0, best) + suffix) : suffix;
                        }
                    }
                    layout = fm.GetStyledLayout(displayText, fmt, boxW, boxH, true, ha, va, lineSpacing_, maxLines_);
                    if (layout) fm.CacheStyledDisplayLayout(text_, fmt, boxW, boxH, true, ha, va, lineSpacing_, maxLines_, layout.Get());
                }
            }
            else {
                layout = fm.GetStyledLayout(text_, fmt, boxW, boxH, overflow_ != TextOverflow::Wrap,
                                            ha, va, lineSpacing_, maxLines_);
            }
            if (!layout) return;

            if (!textBrush_) rt->CreateSolidColorBrush(textColor_.ToD2D(), textBrush_.GetAddressOf());
            else textBrush_->SetColor(textColor_.ToD2D());
            if (!IsEffectivelyEnabled() && textBrush_) textBrush_->SetColor(DefaultDisabledColor.ToD2D());

            rt->DrawTextLayout(D2D1::Point2F(Snap(rect.left), Snap(rect.top)), layout.Get(), textBrush_.Get());
        }

        void ReleaseDeviceResources() override {
            textBrush_.Reset();
            bgBrush_.Reset();
            glyphBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        // 字形图标：字号（<=0 跟随本标签字体）与取格式
        float GlyphSize() {
            if (glyphIconSize_ > 0.0f) return glyphIconSize_;
            IDWriteTextFormat* fmt = GetFontFormat();
            if (fmt) { float s = fmt->GetFontSize(); if (s > 0.0f) return s; }
            return 16.0f;
        }
        IDWriteTextFormat* GetGlyphFormat(float size) {
            FontSpec spec; spec.familyName = IconFontFamily(); spec.size = size;
            return FontManager::Instance().GetFormat(spec);
        }

        std::wstring text_;
        Color textColor_;
        TextOverflow overflow_;
        HAlign hAlign_;
        VAlign vAlign_;
        Thickness padding_;
        Color bgColor_ = Color(0.0f, 0.0f, 0.0f, 0.0f);
        float bgCornerRadius_ = 0.0f;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        float lineSpacing_ = 0.0f;
        int maxLines_ = 0;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        // 图标 / 子控件
        std::shared_ptr<Image> image_;
        Icon  glyphIcon_ = Icon::None;                 // 字体字形图标（图片优先）
        float glyphIconSize_ = 0.0f;                   // <=0 跟随字号
        Color glyphIconColor_ = Color(0.0f, 0.0f, 0.0f, 0.0f);   // a==0 用文字色
        ComPtr<ID2D1SolidColorBrush> glyphBrush_;
        Size iconSize_{ 0, 0 };
        float iconSpacing_ = 6.0f;
        std::vector<std::shared_ptr<UIElement>> children_;
        std::vector<Size> childSizes_;
        float measuredIconW_ = 0, measuredIconH_ = 0;
        float measuredTextW_ = 0, measuredTextH_ = 0;
        float textLeft_ = 0;
        float textBoxW_ = 0;
        bool truncated_ = false;                        // Arrange 时记录：Ellipsis 且放不下 → 供 tooltip
        D2D1_RECT_F iconRect_ = D2D1::RectF(0, 0, 0, 0);
    };

    // ---------- 按钮（内部使用 Label 渲染文本） ----------
    class Button : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Button; }
        std::wstring DefaultAccessibleName() const override { return text_; }
        void AccessibilityInvoke() override { Clicked.Fire(); }   // UIA Invoke
        float DebugAnimationProgress() const override { return hoverProgress_; }
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static Color DefaultNormalColor = Color::FromArgb(255, 0, 120, 212);
        inline static Color DefaultHoverColor = Color::FromArgb(255, 0, 105, 190);
        inline static Color DefaultPressedColor = Color::FromArgb(255, 0, 90, 170);
        inline static Color DefaultTextColor = Color::FromArgb(255, 255, 255, 255);
        inline static float DefaultCornerRadius = 4.0f;
        inline static float DefaultWidth = 120.0f;
        inline static float DefaultHeight = 36.0f;
        inline static float DefaultFontSize = 16.0f;
        inline static float DefaultHorizontalStretchWeight = 0.2f;
        inline static float DefaultVerticalStretchWeight = 0.0f;
        inline static Color DefaultDisabledColor = Color::FromArgb(255, 210, 210, 210);

        // 类型默认字体
        inline static FontSpec DefaultFontSpec = []() {
            FontSpec s;
            s.size = 16.0f;
            return s;
            }();

        ZSignal<> Clicked;
        ZSignal<bool> Toggled;   // 开关式按钮

        Button(const std::wstring& text = L"Button") : text_(text), hovered_(false), isPressed_(false), hoverProgress_(0.0f),
            normalColor_(DefaultNormalColor), hoverColor_(DefaultHoverColor), pressedColor_(DefaultPressedColor),
            textColor_(DefaultTextColor), cornerRadius_(DefaultCornerRadius),
            hoverAnimSpeed_(DefaultHoverAnimationSpeed) {
            width_ = DefaultWidth; height_ = DefaultHeight;
            label_ = std::make_shared<Label>(text_);
            label_->SetTextColor(textColor_);
            label_->SetTextOverflow(Label::TextOverflow::Ellipsis);
            label_->SetAlignment(Label::HAlign::Center, Label::VAlign::Center);
            // Button 的字体跟随自己，标签作为子元素继承显示
            SetFont(Button::DefaultFontSpec);
        }

        void SetText(const std::wstring& text) {
            text_ = text;
            if (label_) label_->SetText(text_);
            InvalidateLayout();
            RequestRepaint();
        }
        std::wstring GetText() const { return text_; }

        // 图标（转发给内部 Label）：Button 等基于 Label 的控件因此也支持字体字形图标。
        // 只设图标、不设文本时即为「图标按钮」。
        void SetIcon(Icon icon, float size = 0.0f) { if (label_) label_->SetIcon(icon, size); }
        Icon GetIcon() const { return label_ ? label_->GetIcon() : Icon::None; }
        void SetIconColor(Color c) { if (label_) label_->SetIconColor(c); }
        // 悬停时：若内部 Label 的文本被省略，自动带上完整文本当 tooltip；用户设过的优先
        std::wstring GetToolTip() const override {
            std::wstring custom = UIElement::GetToolTip();
            if (!custom.empty()) return custom;
            return label_ ? label_->GetToolTip() : std::wstring();
        }
        void SetColors(Color normal, Color hover, Color pressed) {
            normalColor_ = normal; hoverColor_ = hover; pressedColor_ = pressed; bgBrush_.Reset(); RequestRepaint();
        }
        void SetTextColor(Color color) {
            textColor_ = color;
            if (label_) label_->SetTextColor(textColor_);
            RequestRepaint();
        }
        Color GetTextColor() const { return textColor_; }
        Color GetNormalColor() const { return normalColor_; }
        Color GetHoverColor() const { return hoverColor_; }
        Color GetPressedColor() const { return pressedColor_; }
        float GetCornerRadius() const { return cornerRadius_; }
        void SetCornerRadius(float radius) { cornerRadius_ = radius; RequestRepaint(); }
        void SetHoverAnimationSpeed(float speed) { hoverAnimSpeed_ = speed; }
        void SetCheckable(bool checkable) { checkable_ = checkable; if (!checkable) checked_ = false; RequestRepaint(); }
        bool IsCheckable() const { return checkable_; }
        void SetChecked(bool checked) { if (checkable_ && checked_ != checked) { checked_ = checked; Toggled(checked_); RequestRepaint(); } }
        bool IsChecked() const { return checked_; }
        void SetTextAlignment(Label::HAlign align) { hAlign_ = align; if (label_) label_->SetAlignment(align, Label::VAlign::Center); RequestRepaint(); }
        void SetPadding(float p) { padding_ = max(0.0f, p); InvalidateLayout(); RequestRepaint(); }
        void SetAutoRepeat(bool enable, float intervalMs = 400.0f) { autoRepeat_ = enable; autoRepeatInterval_ = intervalMs; }

        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultColors(Color normal, Color hover, Color pressed) { DefaultNormalColor = normal; DefaultHoverColor = hover; DefaultPressedColor = pressed; }
        static void SetDefaultTextColor(Color color) { DefaultTextColor = color; }
        static void SetDefaultCornerRadius(float radius) { DefaultCornerRadius = radius; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultFontSize(float size) { Button::DefaultFontSpec.size = size; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        std::optional<FontSpec> GetTypeDefaultFont() const override {
            return Button::DefaultFontSpec;
        }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (label_) {
                float padding = padding_;
                Rect labelRect(finalRect.x + padding, finalRect.y, finalRect.width - padding * 2, finalRect.height);
                label_->Arrange(labelRect);
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            bool enabled = IsEffectivelyEnabled();
            Color bgColor;
            if (!enabled) bgColor = DefaultDisabledColor;
            else if (checkable_ && checked_) bgColor = pressedColor_;
            else bgColor = isPressed_ ? pressedColor_ : Color::Lerp(normalColor_, hoverColor_, hoverProgress_);
            if (!bgBrush_) rt->CreateSolidColorBrush(bgColor.ToD2D(), bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bgColor.ToD2D());

            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), cornerRadius_, cornerRadius_), bgBrush_.Get());

            if (label_) {
                label_->SetTextColor(enabled ? textColor_ : Color::FromArgb(255, 120, 120, 120));
                label_->Draw(rt);
            }
        }

        void UpdateAnimation(float deltaTime) override {
            float target = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < target) { hoverProgress_ += hoverAnimSpeed_ * deltaTime; if (hoverProgress_ > target) hoverProgress_ = target; }
            else if (hoverProgress_ > target) { hoverProgress_ -= hoverAnimSpeed_ * deltaTime; if (hoverProgress_ < target) hoverProgress_ = target; }
            if (HasActiveAnimation()) RequestRepaint();
            ConvergeValue(hoverProgress_, hovered_ ? 1.0f : 0.0f, 0.001f);
            if (autoRepeat_ && isPressed_ && autoRepeatInterval_ > 0.0f) {
                DWORD now = GetTickCount();
                if (now - lastRepeatTick_ >= (DWORD)autoRepeatInterval_) { lastRepeatTick_ = now; Clicked(); }
            }
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.001f;
            return hovered_ ? (hoverProgress_ < 1.0f - epsilon) : (hoverProgress_ > epsilon);
        }

        void OnMouseEnter() override { if (!IsEffectivelyEnabled()) return; hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; isPressed_ = false; RequestRepaint(); MouseLeave.Fire(); }
        void OnMouseDown(float x, float y) override { if (!IsEffectivelyEnabled()) return; isPressed_ = true; lastRepeatTick_ = GetTickCount(); RequestRepaint(); MouseDown.Fire(x, y); }
        void OnMouseUp(float x, float y) override {
            if (!IsEffectivelyEnabled()) return;
            if (isPressed_) {
                if (checkable_) SetChecked(!checked_);
                Clicked();
            }
            isPressed_ = false;
            RequestRepaint();
            MouseUp.Fire(x, y);
        }
        bool IsFocusable() const override { return true; }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            if ((key == VK_SPACE || key == VK_RETURN) && !isPressed_) { isPressed_ = true; lastRepeatTick_ = GetTickCount(); RequestRepaint(); }
            KeyDown.Fire(key, lParam);
        }
        void OnKeyUp(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            if ((key == VK_SPACE || key == VK_RETURN) && isPressed_) {
                if (checkable_) SetChecked(!checked_);
                Clicked();
                isPressed_ = false;
                RequestRepaint();
            }
            KeyUp.Fire(key, lParam);
        }

        // 字体变化时，label 也要跟着变
        void OnFontChanged() override {
            if (label_) label_->SetFont(GetEffectiveFontSpec());
            UIElement::OnFontChanged();
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            if (label_) label_->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        std::wstring text_;
        bool hovered_, isPressed_;
        float hoverProgress_;
        Color normalColor_, hoverColor_, pressedColor_, textColor_;
        float cornerRadius_;
        float hoverAnimSpeed_;
        std::shared_ptr<Label> label_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        bool checkable_ = false;
        bool checked_ = false;
        float padding_ = 5.0f;
        Label::HAlign hAlign_ = Label::HAlign::Center;
        bool autoRepeat_ = false;
        float autoRepeatInterval_ = 400.0f;
        DWORD lastRepeatTick_ = 0;
    };

    // ---------- 文本框（支持自动滚动、边框正常、文本裁剪） ----------
    class TextBox : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Edit; }
        std::wstring DefaultAccessibleName() const override { return text_.empty() ? placeholder_ : text_; }
        std::wstring GetAccessibleValue() const override { return text_; }                 // UIA Value
        void SetAccessibleValue(const std::wstring& s) override { if (!readOnly_) SetText(s); }
        bool IsAccessibleReadOnly() const override { return readOnly_; }
        inline static float DefaultWidth = 160.0f;
        inline static float DefaultHeight = 30.0f;
        inline static D2D1_COLOR_F DefaultBgColor = D2D1::ColorF(0.98f, 0.98f, 0.98f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.80f, 0.80f, 0.80f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0, 0, 0, 1);
        inline static D2D1_COLOR_F DefaultSelectionColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 0.5f);
        inline static D2D1_COLOR_F DefaultHoverBgColor = D2D1::ColorF(0.93f, 0.93f, 0.93f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverBorderColor = D2D1::ColorF(0.4f, 0.4f, 0.4f, 1.0f);
        inline static float DefaultCursorBlinkInterval = 0.5f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;

        // 类型默认字体
        inline static FontSpec DefaultFontSpec = []() {
            FontSpec s;
            s.size = 14.0f;
            return s;
            }();

        ZSignal<const std::wstring&> TextChanged;

        TextBox() : text_(), placeholder_(), cursorPos_(0), selectionStart_(0), selectionEnd_(0), selectionAnchor_(0),
            focused_(false), showCursor_(true), cursorBlinkTime_(0.0f),
            hovered_(false), hoverProgress_(0.0f),
            bgColor_(DefaultBgColor), borderColor_(DefaultBorderColor), textColor_(DefaultTextColor),
            selectionColor_(DefaultSelectionColor), hoverBgColor_(DefaultHoverBgColor), hoverBorderColor_(DefaultHoverBorderColor),
            cursorBlinkInterval_(DefaultCursorBlinkInterval), hoverAnimSpeed_(DefaultHoverAnimationSpeed),
            undoStack_(), undoIndex_(-1),
            passwordMode_(false),
            maxLength_(-1),
            scrollX_(0.0f),
            compositionCursorPos_(0),
            hasComposition_(false) {
            width_ = DefaultWidth; height_ = DefaultHeight;
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            PushUndoState();
        }

        std::wstring GetText() const { return text_; }
        void SetText(const std::wstring& text) {
            text_ = text;
            text_.erase(std::remove_if(text_.begin(), text_.end(),
                [](wchar_t c) { return c == L'\r' || c == L'\n'; }), text_.end());
            if (maxLength_ >= 0 && text_.size() > (size_t)maxLength_) text_.resize(maxLength_);
            cursorPos_ = min(cursorPos_, (int)text_.size());
            selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            ClearUndoHistory();   // 编程式 SetText 不产生撤销点（对齐 QTextEdit）
            TextChanged(text_);
            EnsureCursorVisible();
            InvalidateLayout();
            RequestRepaint();
            AccessibilityNotifyPropertyChanged();   // UIA Value 变化
        }
        void SetPlaceholder(const std::wstring& placeholder) { placeholder_ = placeholder; RequestRepaint(); }
        void SetTextColor(Color color) { textColor_ = color.ToD2D(); textBrush_.Reset(); RequestRepaint(); }
        void SetBackgroundColor(Color color) { bgColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetSelectionColor(Color color) { selectionColor_ = color.ToD2D(); selectionBrush_.Reset(); RequestRepaint(); }
        void SetHoverBackgroundColor(Color color) { hoverBgColor_ = color.ToD2D(); RequestRepaint(); }
        void SetHoverBorderColor(Color color) { hoverBorderColor_ = color.ToD2D(); RequestRepaint(); }
        void SetPasswordMode(bool mode) { passwordMode_ = mode; UpdateDisplayLayout(); UpdateFullDisplayLayout(); EnsureCursorVisible(); RequestRepaint(); }
        bool IsPasswordMode() const { return passwordMode_; }
        void SetMaxLength(int maxLength) {
            maxLength_ = maxLength;
            if (maxLength_ >= 0 && text_.size() > (size_t)maxLength_) {
                text_.resize(maxLength_);
                if (cursorPos_ > maxLength_) cursorPos_ = maxLength_;
                    if (selectionStart_ > maxLength_) selectionStart_ = maxLength_;
                    if (selectionEnd_ > maxLength_) selectionEnd_ = maxLength_;
                    if (selectionAnchor_ > maxLength_) selectionAnchor_ = maxLength_;
                UpdateDisplayLayout();
                UpdateFullDisplayLayout();
                EnsureCursorVisible();
                TextChanged(text_);
                InvalidateLayout();
                RequestRepaint();
            }
        }
        int GetMaxLength() const { return maxLength_; }
        bool IsFocused() const { return focused_; }
        void Focus() { focused_ = true; OnFocus(); }
        void Blur() { focused_ = false; OnBlur(); }
        void SetCursorBlinkInterval(float interval) { cursorBlinkInterval_ = interval; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimSpeed_ = speed; }

        // ---------- 只读 / 输入过滤 / 回车 / 占位色 / 密码显隐 / 选区编辑 ----------
        void SetReadOnly(bool readOnly) { readOnly_ = readOnly; RequestRepaint(); }
        // 错误态：整体（含边框、底部指示器）变红
        void SetError(bool on) { if (on != error_) { error_ = on; RequestRepaint(); } }
        bool IsError() const { return error_; }
        // 关掉 IME 关联（数字框等不希望弹出输入法）
        void SetImeEnabled(bool on) { imeEnabled_ = on; }
        bool IsImeEnabled() const { return imeEnabled_; }
        void SetAccentColor(Color c) { accentColor_ = c.ToD2D(); RequestRepaint(); }
        void SetErrorColor(Color c) { errorColor_ = c.ToD2D(); RequestRepaint(); }
        void SetIndicatorColor(Color c) { accentColor_ = c.ToD2D(); RequestRepaint(); }
        void SetIndicatorThickness(float t) { indicatorThickness_ = max(1.0f, t); RequestRepaint(); }
        bool IsReadOnly() const { return readOnly_; }
        void SetInputFilter(std::function<bool(wchar_t)> filter) { inputFilter_ = std::move(filter); }
        void ClearInputFilter() { inputFilter_ = nullptr; }
        ZSignal<> ReturnPressed;
        void SetPlaceholderColor(Color color) { placeholderColor_ = color.ToD2D(); placeholderBrush_.Reset(); RequestRepaint(); }
        void SetRevealPassword(bool reveal) { revealPassword_ = reveal; UpdateDisplayLayout(); UpdateFullDisplayLayout(); EnsureCursorVisible(); RequestRepaint(); }
        bool IsPasswordRevealed() const { return revealPassword_; }
        int GetSelectionStart() const { return selectionStart_; }
        int GetSelectionEnd() const { return selectionEnd_; }
        int GetCursorPosition() const { return cursorPos_; }
        void SetSelection(int start, int end) {
            int n = (int)text_.size();
            selectionStart_ = max(0, min(n, start));
            selectionEnd_ = max(0, min(n, end));
            if (selectionStart_ > selectionEnd_) std::swap(selectionStart_, selectionEnd_);
            cursorPos_ = selectionEnd_;
            selectionAnchor_ = selectionStart_;
            EnsureCursorVisible(); RequestRepaint();
        }

        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultColors(D2D1_COLOR_F bg, D2D1_COLOR_F border, D2D1_COLOR_F text, D2D1_COLOR_F selection) { DefaultBgColor = bg; DefaultBorderColor = border; DefaultTextColor = text; DefaultSelectionColor = selection; }
        static void SetDefaultHoverColors(D2D1_COLOR_F bg, D2D1_COLOR_F border) { DefaultHoverBgColor = bg; DefaultHoverBorderColor = border; }
        static void SetDefaultCursorBlinkInterval(float interval) { DefaultCursorBlinkInterval = interval; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultFontSize(float size) { TextBox::DefaultFontSpec.size = size; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        std::optional<FontSpec> GetTypeDefaultFont() const override {
            return TextBox::DefaultFontSpec;
        }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }
        UIElement* HitTest(float x, float y) override {
            if (visible_ && arrangedRect_.Contains(x, y)) return this;
            return nullptr;
        }
        bool IsFocusable() const override { return true; }
        bool IsTextInput() const override { return imeEnabled_; }

        Rect GetImeCandidateRect() const override {
            float cursorX = GetTextPositionX(cursorPos_ + (hasComposition_ ? max(0, min(compositionCursorPos_, (int)compositionText_.size())) : 0));
            return Rect(arrangedRect_.x + 5 + cursorX - scrollX_,
                arrangedRect_.y,
                1, arrangedRect_.height);
        }

        void SetCompositionText(const std::wstring& text, bool has, int cursorPos = -1) override {
            compositionText_ = text;
            hasComposition_ = has;
            if (cursorPos >= 0) compositionCursorPos_ = cursorPos;
            UpdateFullDisplayLayout();
            EnsureCursorVisible();
            RequestRepaint();
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            IDWriteTextFormat* fmt = GetFontFormat();
            if (!fmt) return;

            // 1. 背景和边框（不裁剪；错误态整体变红）
            D2D1_COLOR_F bgCol = error_
                ? D2D1::ColorF(1.0f, 0.97f, 0.97f, 1.0f)
                : D2D1::ColorF(
                    bgColor_.r + (hoverBgColor_.r - bgColor_.r) * hoverProgress_,
                    bgColor_.g + (hoverBgColor_.g - bgColor_.g) * hoverProgress_,
                    bgColor_.b + (hoverBgColor_.b - bgColor_.b) * hoverProgress_, 1.0f);
            if (!bgBrush_) rt->CreateSolidColorBrush(bgCol, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bgCol);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            D2D1_COLOR_F borderCol = error_ ? errorColor_ : D2D1::ColorF(
                borderColor_.r + (hoverBorderColor_.r - borderColor_.r) * hoverProgress_,
                borderColor_.g + (hoverBorderColor_.g - borderColor_.g) * hoverProgress_,
                borderColor_.b + (hoverBorderColor_.b - borderColor_.b) * hoverProgress_, 1.0f);
            if (!borderBrush_) rt->CreateSolidColorBrush(borderCol, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderCol);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);

            // 底部指示器：与边框同圆角的"加粗描边"，裁到框底部 → 替换下边框并与圆角融合
            {
                D2D1_COLOR_F ind = error_ ? errorColor_ : accentColor_;
                if (!indicatorBrush_) rt->CreateSolidColorBrush(ind, indicatorBrush_.GetAddressOf());
                else indicatorBrush_->SetColor(ind);
                if (indicatorBrush_) {
                    float th2 = indicatorThickness_;
                    float ins = th2 * 0.5f;
                    D2D1_RECT_F box = arrangedRect_.ToD2D();
                    D2D1_RECT_F ip = D2D1::RectF(box.left + ins, box.top + ins, box.right - ins, box.bottom - ins);
                    float irad = max(0.0f, 4.0f - ins);
                    rt->PushAxisAlignedClip(D2D1::RectF(box.left - 1.0f, box.bottom - th2, box.right + 1.0f, box.bottom + 1.0f),
                        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(ip, irad, irad), indicatorBrush_.Get(), th2);
                    rt->PopAxisAlignedClip();
                }
            }

            // 2. 文本内容裁剪（内部区域）
            D2D1_RECT_F clipRect = D2D1::RectF(
                arrangedRect_.x + 5.0f,
                arrangedRect_.y + 2.0f,
                arrangedRect_.x + arrangedRect_.width - 5.0f,
                arrangedRect_.y + arrangedRect_.height - 2.0f);
            rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            // 3. 绘制选择高亮（使用原始文本布局，不包含组合文本）
            if (selectionStart_ != selectionEnd_) {
                float selLeft = GetTextPositionX(selectionStart_) - scrollX_;
                float selRight = GetTextPositionX(selectionEnd_) - scrollX_;
                D2D1_RECT_F selRect = D2D1::RectF(
                    arrangedRect_.x + 5 + selLeft,
                    arrangedRect_.y + 2,
                    arrangedRect_.x + 5 + selRight,
                    arrangedRect_.y + arrangedRect_.height - 2);
                if (!selectionBrush_) rt->CreateSolidColorBrush(selectionColor_, selectionBrush_.GetAddressOf());
                else selectionBrush_->SetColor(selectionColor_);
                if (selectionBrush_) rt->FillRectangle(selRect, selectionBrush_.Get());
            }

            // 4. 绘制文本（包括组合文本）或占位符
            if (text_.empty() && !hasComposition_ && !placeholder_.empty() && !focused_) {
                // 占位符
                if (!placeholderBrush_) rt->CreateSolidColorBrush(placeholderColor_, placeholderBrush_.GetAddressOf());
                else placeholderBrush_->SetColor(placeholderColor_);
                if (placeholderBrush_) {
                    D2D1_RECT_F textRect = D2D1::RectF(
                        arrangedRect_.x + 5,
                        arrangedRect_.y,
                        arrangedRect_.x + arrangedRect_.width - 5,
                        arrangedRect_.y + arrangedRect_.height);
                    rt->DrawText(placeholder_.c_str(), (UINT32)placeholder_.length(), fmt, textRect, placeholderBrush_.Get());
                }
            }
            else if (!text_.empty() || hasComposition_) {
                // 使用完整布局绘制（包括组合文本，以及下划线）
                if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                else textBrush_->SetColor(textColor_);
                if (textBrush_ && fullDisplayLayout_) {
                    DWRITE_TEXT_METRICS metrics;
                    fullDisplayLayout_->GetMetrics(&metrics);
                    float textHeight = metrics.height;
                    float yOffset = (arrangedRect_.height - textHeight) / 2.0f; // 垂直居中
                    D2D1_POINT_2F origin = D2D1::Point2F(
                        Snap(arrangedRect_.x + 5 - scrollX_),
                        Snap(arrangedRect_.y + yOffset));
                    rt->DrawTextLayout(origin, fullDisplayLayout_.Get(), textBrush_.Get());
                }
            }

            // 5. 绘制光标
            if (focused_ && showCursor_ && !readOnly_ && selectionStart_ == selectionEnd_) {
                int globalCursorPos = cursorPos_;
                if (hasComposition_) {
                    globalCursorPos += max(0, min(compositionCursorPos_, (int)compositionText_.size()));
                }
                float cursorX = GetTextPositionX(globalCursorPos) - scrollX_;
                D2D1_POINT_2F pt1 = D2D1::Point2F(arrangedRect_.x + 5 + cursorX, arrangedRect_.y + 4);
                D2D1_POINT_2F pt2 = D2D1::Point2F(arrangedRect_.x + 5 + cursorX, arrangedRect_.y + arrangedRect_.height - 4);
                if (!cursorBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), cursorBrush_.GetAddressOf());
                else cursorBrush_->SetColor(D2D1::ColorF(0, 0, 0, 1));
                if (cursorBrush_) rt->DrawLine(pt1, pt2, cursorBrush_.Get(), 1.0f);
            }

            rt->PopAxisAlignedClip();
        }

        void OnMouseDown(float x, float y) override {
            int pos = GetCharIndexFromX(x - arrangedRect_.x - 5 + scrollX_);
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            cursorPos_ = pos;
            if (shift && selectionStart_ != selectionEnd_) {
                selectionStart_ = min(selectionAnchor_, pos);
                selectionEnd_ = max(selectionAnchor_, pos);
            }
            else {
                selectionAnchor_ = pos;
                selectionStart_ = selectionEnd_ = pos;
            }
            ResetCursorBlink();
            EnsureCursorVisible();
            RequestRepaint();
            MouseDown.Fire(x, y);
        }
        void OnMouseMove(float x, float y) override {
            if (focused_ && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                int pos = GetCharIndexFromX(x - arrangedRect_.x - 5 + scrollX_);
                cursorPos_ = pos;
                selectionStart_ = min(selectionAnchor_, pos);
                selectionEnd_ = max(selectionAnchor_, pos);
                ResetCursorBlink();
                EnsureCursorVisible();
                RequestRepaint();
            }
            MouseMove.Fire(x, y);
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!focused_) return;
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (ctrl) {
                switch (key) {
                case 'C': Copy(); return;
                case 'X': if (!readOnly_) Cut(); return;
                case 'V': if (!readOnly_) Paste(); return;
                case 'A': SelectAll(); return;
                case 'Z': if (!readOnly_) Undo(); return;
                case 'Y': if (!readOnly_) Redo(); return;
                default: return;
                }
            }

            if (readOnly_ && (key == VK_BACK || key == VK_DELETE)) {
                KeyDown.Fire(key, lParam);
                return;
            }

            switch (key) {
            case VK_LEFT:
                if (shift) {
                    if (selectionStart_ == selectionEnd_) selectionAnchor_ = cursorPos_;
                    cursorPos_ = max(0, cursorPos_ - 1);
                    selectionStart_ = min(selectionAnchor_, cursorPos_);
                    selectionEnd_ = max(selectionAnchor_, cursorPos_);
                }
                else { cursorPos_ = max(0, cursorPos_ - 1); selectionStart_ = selectionEnd_ = cursorPos_; }
                ResetCursorBlink(); EnsureCursorVisible(); RequestRepaint(); break;
            case VK_RIGHT:
                if (shift) {
                    if (selectionStart_ == selectionEnd_) selectionAnchor_ = cursorPos_;
                    cursorPos_ = min((int)text_.size(), cursorPos_ + 1);
                    selectionStart_ = min(selectionAnchor_, cursorPos_);
                    selectionEnd_ = max(selectionAnchor_, cursorPos_);
                }
                else { cursorPos_ = min((int)text_.size(), cursorPos_ + 1); selectionStart_ = selectionEnd_ = cursorPos_; }
                ResetCursorBlink(); EnsureCursorVisible(); RequestRepaint(); break;
            case VK_HOME:
                if (shift) {
                    if (selectionStart_ == selectionEnd_) selectionAnchor_ = cursorPos_;
                    cursorPos_ = 0;
                    selectionStart_ = min(selectionAnchor_, cursorPos_);
                    selectionEnd_ = max(selectionAnchor_, cursorPos_);
                }
                else { cursorPos_ = 0; selectionStart_ = selectionEnd_ = 0; }
                ResetCursorBlink(); EnsureCursorVisible(); RequestRepaint(); break;
            case VK_END:
                if (shift) {
                    if (selectionStart_ == selectionEnd_) selectionAnchor_ = cursorPos_;
                    cursorPos_ = (int)text_.size();
                    selectionStart_ = min(selectionAnchor_, cursorPos_);
                    selectionEnd_ = max(selectionAnchor_, cursorPos_);
                }
                else { cursorPos_ = (int)text_.size(); selectionStart_ = selectionEnd_ = cursorPos_; }
                ResetCursorBlink(); EnsureCursorVisible(); RequestRepaint(); break;
            case VK_BACK:
                if (selectionStart_ != selectionEnd_) DeleteSelection();
                else if (cursorPos_ > 0) {
                    text_.erase(cursorPos_ - 1, 1);
                    cursorPos_--; selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
                }
                UpdateDisplayLayout();
                UpdateFullDisplayLayout();
                PushUndoState();
                TextChanged(text_);
                ResetCursorBlink(); EnsureCursorVisible(); InvalidateLayout(); RequestRepaint(); break;
            case VK_DELETE:
                if (selectionStart_ != selectionEnd_) DeleteSelection();
                else if (cursorPos_ < (int)text_.size()) {
                    text_.erase(cursorPos_, 1);
                    selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
                }
                UpdateDisplayLayout();
                UpdateFullDisplayLayout();
                PushUndoState();
                TextChanged(text_);
                ResetCursorBlink(); EnsureCursorVisible(); InvalidateLayout(); RequestRepaint(); break;
            case VK_RETURN: ReturnPressed(); break;
            default: break;
            }
            KeyDown.Fire(key, lParam);
        }
        void OnChar(wchar_t ch) override {
            if (!focused_ || readOnly_) return;
            if (ch < 32 || ch == L'\r' || ch == L'\n') return;
            if (inputFilter_ && !inputFilter_(ch)) return;
            if (selectionStart_ != selectionEnd_) DeleteSelection();
            if (maxLength_ >= 0 && (int)text_.size() >= maxLength_) return;
            text_.insert(cursorPos_, 1, ch);
            cursorPos_++; selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            PushUndoState();
            TextChanged(text_);
            ResetCursorBlink(); EnsureCursorVisible(); InvalidateLayout(); RequestRepaint();
            Char.Fire(ch);
        }
        void OnFocus() override {
            focused_ = true; showCursor_ = true; cursorBlinkTime_ = 0.0f;
            EnsureCursorVisible();
            RequestRepaint();
            Focused.Fire();
        }
        void OnBlur() override {
            focused_ = false; showCursor_ = false; cursorBlinkTime_ = 0.0f;
            selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
            RequestRepaint();
            Blurred.Fire();
        }
        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); MouseLeave.Fire(); }
        void UpdateAnimation(float deltaTime) override {
            if (focused_) {
                cursorBlinkTime_ += deltaTime;
                if (cursorBlinkTime_ >= cursorBlinkInterval_) {
                    cursorBlinkTime_ = 0.0f;
                    showCursor_ = !showCursor_;
                }
            }
            else { showCursor_ = false; cursorBlinkTime_ = 0.0f; }

            float target = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < target) { hoverProgress_ += hoverAnimSpeed_ * deltaTime; if (hoverProgress_ > target) hoverProgress_ = target; }
            else if (hoverProgress_ > target) { hoverProgress_ -= hoverAnimSpeed_ * deltaTime; if (hoverProgress_ < target) hoverProgress_ = target; }

            if (HasActiveAnimation()) RequestRepaint();
        }
        bool HasActiveAnimation() const override {
            const float epsilon = 0.001f;
            bool cursorAnim = focused_ && cursorBlinkInterval_ > 0;
            bool hoverAnim = (hovered_ ? (hoverProgress_ < 1.0f - epsilon) : (hoverProgress_ > epsilon));
            return cursorAnim || hoverAnim;
        }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); borderBrush_.Reset(); textBrush_.Reset();
            placeholderBrush_.Reset(); selectionBrush_.Reset(); cursorBrush_.Reset();
            compositionBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

        // 字体变化时重建文本布局
        void OnFontChanged() override {
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            EnsureCursorVisible();
            UIElement::OnFontChanged();
        }

    private:
        float GetCompositionTextWidth() const {
            if (hasComposition_ && !compositionText_.empty()) {
                IDWriteFactory* dwriteFactory = FontManager::Instance().GetFactory();
                IDWriteTextFormat* fmt = GetFontFormat();
                if (dwriteFactory && fmt) {
                    ComPtr<IDWriteTextLayout> layout;
                    dwriteFactory->CreateTextLayout(
                        compositionText_.c_str(), (UINT32)compositionText_.length(),
                        fmt, 10000.0f, 10000.0f, &layout);
                    if (layout) {
                        DWRITE_TEXT_METRICS metrics;
                        layout->GetMetrics(&metrics);
                        return metrics.width;
                    }
                }
            }
            return 0.0f;
        }

        void UpdateDisplayLayout() {
            displayTextLayout_.Reset();
            if (text_.empty()) return;
            IDWriteFactory* dwriteFactory = FontManager::Instance().GetFactory();
            IDWriteTextFormat* fmt = GetFontFormat();
            if (!dwriteFactory || !fmt) return;
            std::wstring displayText = (passwordMode_ && !revealPassword_) ? std::wstring(text_.size(), L'\u2022') : text_;
            dwriteFactory->CreateTextLayout(displayText.c_str(), (UINT32)displayText.length(),
                fmt, 10000.0f, 10000.0f, &displayTextLayout_);
        }

        void UpdateFullDisplayLayout() {
            fullDisplayLayout_.Reset();
            IDWriteFactory* dwriteFactory = FontManager::Instance().GetFactory();
            IDWriteTextFormat* fmt = GetFontFormat();
            if (!dwriteFactory || !fmt) return;
            std::wstring fullText = text_;
            if (hasComposition_ && !compositionText_.empty()) {
                int insertPos = min(cursorPos_, (int)fullText.size());
                fullText.insert(insertPos, compositionText_);
            }
            if (passwordMode_ && !revealPassword_) {
                fullText = std::wstring(fullText.size(), L'\u2022');
            }
            if (fullText.empty()) return;
            dwriteFactory->CreateTextLayout(fullText.c_str(), (UINT32)fullText.length(),
                fmt, 10000.0f, 10000.0f, &fullDisplayLayout_);
            if (fullDisplayLayout_) {
                fullDisplayLayout_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                if (hasComposition_ && !compositionText_.empty()) {
                    int start = min(cursorPos_, (int)text_.size());
                    int length = (int)compositionText_.size();
                    fullDisplayLayout_->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ (UINT32)start, (UINT32)length });
                }
            }
        }

        void ResetCursorBlink() { showCursor_ = true; cursorBlinkTime_ = 0.0f; }

        float GetTextPositionX(int charIndex) const {
            if (fullDisplayLayout_) {
                DWRITE_HIT_TEST_METRICS metrics; float x, y;
                fullDisplayLayout_->HitTestTextPosition(charIndex, false, &x, &y, &metrics);
                return x;
            }
            if (displayTextLayout_) {
                DWRITE_HIT_TEST_METRICS metrics; float x, y;
                displayTextLayout_->HitTestTextPosition(charIndex, false, &x, &y, &metrics);
                return x;
            }
            if (text_.empty()) return 0.0f;
            return charIndex * 7.0f;
        }
        int GetCharIndexFromX(float x) const {
            if (fullDisplayLayout_) {
                BOOL isTrailingHit = FALSE; BOOL isInside = FALSE;
                DWRITE_HIT_TEST_METRICS metrics;
                fullDisplayLayout_->HitTestPoint(x, 0.0f, &isTrailingHit, &isInside, &metrics);
                int pos = metrics.textPosition;
                if (isTrailingHit) pos += metrics.length;
                return max(0, min((int)text_.size(), pos));
            }
            if (displayTextLayout_) {
                BOOL isTrailingHit = FALSE; BOOL isInside = FALSE;
                DWRITE_HIT_TEST_METRICS metrics;
                displayTextLayout_->HitTestPoint(x, 0.0f, &isTrailingHit, &isInside, &metrics);
                int pos = metrics.textPosition;
                if (isTrailingHit) pos += metrics.length;
                return max(0, min((int)text_.size(), pos));
            }
            if (text_.empty()) return 0;
            int pos = (int)(x / 7.0f);
            return max(0, min((int)text_.size(), pos));
        }
        void DeleteSelection() {
            if (selectionStart_ == selectionEnd_) return;
            text_.erase(selectionStart_, selectionEnd_ - selectionStart_);
            cursorPos_ = selectionStart_; selectionEnd_ = selectionStart_; selectionAnchor_ = cursorPos_;
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            EnsureCursorVisible(); InvalidateLayout(); RequestRepaint();
        }
        void PushUndoState() {
            if (undoIndex_ >= 0 && undoIndex_ < (int)undoStack_.size() - 1)
                undoStack_.erase(undoStack_.begin() + undoIndex_ + 1, undoStack_.end());
            undoStack_.push_back(text_);
            undoIndex_ = (int)undoStack_.size() - 1;
        }
        public:
        void Undo() {
            if (undoIndex_ > 0) {
                undoIndex_--;
                SetTextWithoutHistory(undoStack_[undoIndex_]);
                TextChanged(text_);
                EnsureCursorVisible();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        void Redo() {
            if (undoIndex_ >= 0 && undoIndex_ < (int)undoStack_.size() - 1) {
                undoIndex_++;
                SetTextWithoutHistory(undoStack_[undoIndex_]);
                TextChanged(text_);
                EnsureCursorVisible();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        void SetTextWithoutHistory(const std::wstring& text) {
            text_ = text;
            if (maxLength_ >= 0 && text_.size() > (size_t)maxLength_) text_.resize(maxLength_);
            cursorPos_ = (int)text_.size(); selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_;
            UpdateDisplayLayout();
            UpdateFullDisplayLayout();
            EnsureCursorVisible();
        }
        void ClearUndoHistory() { undoStack_.clear(); undoIndex_ = -1; }
        void Copy() {
            if (selectionStart_ == selectionEnd_) return;
            std::wstring sel = text_.substr(selectionStart_, selectionEnd_ - selectionStart_);
            HWND owner = GetWindow() ? GetWindow()->GetHwnd() : GetActiveWindow();
            if (OpenClipboard(owner)) {
                EmptyClipboard();
                size_t size = (sel.size() + 1) * sizeof(wchar_t);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hMem) {
                    memcpy(GlobalLock(hMem), sel.c_str(), size);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
        }
        void Paste() {
            HWND owner = GetWindow() ? GetWindow()->GetHwnd() : GetActiveWindow();
            if (!OpenClipboard(owner)) return;
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pData = (wchar_t*)GlobalLock(hData);
                if (pData) {
                    std::wstring pasteText(pData);
                    GlobalUnlock(hData);
                    pasteText.erase(std::remove_if(pasteText.begin(), pasteText.end(),
                        [](wchar_t c) { return c == L'\r' || c == L'\n'; }), pasteText.end());
                    if (selectionStart_ != selectionEnd_) DeleteSelection();
                    if (maxLength_ >= 0) {
                        int remain = maxLength_ - (int)text_.size();
                        if (remain <= 0) { CloseClipboard(); return; }
                        if (pasteText.size() > (size_t)remain) pasteText.resize(remain);
                    }
                    text_.insert(cursorPos_, pasteText);
                    cursorPos_ += (int)pasteText.size();
                    selectionStart_ = selectionEnd_ = cursorPos_; selectionAnchor_ = cursorPos_; selectionAnchor_ = cursorPos_;
                    UpdateDisplayLayout();
                    UpdateFullDisplayLayout();
                    PushUndoState();
                    TextChanged(text_);
                    ResetCursorBlink(); EnsureCursorVisible(); InvalidateLayout(); RequestRepaint();
                }
            }
            CloseClipboard();
        }
        void Cut() {
            Copy();
            if (selectionStart_ != selectionEnd_) {
                DeleteSelection();
                UpdateDisplayLayout();
                UpdateFullDisplayLayout();
                PushUndoState();
                TextChanged(text_);
                ResetCursorBlink(); EnsureCursorVisible(); InvalidateLayout(); RequestRepaint();
            }
        }
        void SelectAll() {
            selectionStart_ = 0; selectionEnd_ = (int)text_.size(); cursorPos_ = selectionEnd_; selectionAnchor_ = 0;
            EnsureCursorVisible();
            InvalidateLayout();
            RequestRepaint();
        }

        private:
        void EnsureCursorVisible() {
            int globalCursorPos = cursorPos_;
            if (hasComposition_ && !compositionText_.empty()) {
                globalCursorPos += max(0, min(compositionCursorPos_, (int)compositionText_.size()));
            }
            float cursorX = GetTextPositionX(globalCursorPos);
            float leftPadding = 5.0f;
            float rightPadding = 5.0f;
            float viewWidth = arrangedRect_.width - leftPadding - rightPadding;

            if (cursorX < scrollX_ + leftPadding) {
                scrollX_ = cursorX - leftPadding;
                if (scrollX_ < 0) scrollX_ = 0;
            }
            else if (cursorX > scrollX_ + viewWidth - rightPadding) {
                scrollX_ = cursorX - viewWidth + rightPadding;
            }
            float maxScroll = max(0.0f, GetTextWidth() - viewWidth);
            if (scrollX_ > maxScroll) scrollX_ = maxScroll;
            if (scrollX_ < 0) scrollX_ = 0;
        }

        float GetTextWidth() const {
            if (fullDisplayLayout_) {
                DWRITE_TEXT_METRICS metrics;
                fullDisplayLayout_->GetMetrics(&metrics);
                return metrics.width;
            }
            if (displayTextLayout_) {
                DWRITE_TEXT_METRICS metrics;
                displayTextLayout_->GetMetrics(&metrics);
                return metrics.width;
            }
            return 0.0f;
        }

        std::wstring text_;
        std::wstring placeholder_;
        bool readOnly_ = false;
        bool revealPassword_ = false;
        std::function<bool(wchar_t)> inputFilter_;
        D2D1_COLOR_F placeholderColor_ = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        int cursorPos_;
        int selectionStart_, selectionEnd_;
        int selectionAnchor_;
        bool focused_;
        bool showCursor_;
        float cursorBlinkTime_;
        bool hovered_;
        float hoverProgress_;
        ComPtr<IDWriteTextLayout> displayTextLayout_;
        ComPtr<IDWriteTextLayout> fullDisplayLayout_;
        D2D1_COLOR_F bgColor_, borderColor_, textColor_, selectionColor_;
        bool error_ = false;
        bool imeEnabled_ = true;                                            // 关掉后不关联 IME（输入法）
        D2D1_COLOR_F accentColor_ = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        D2D1_COLOR_F errorColor_ = D2D1::ColorF(0.80f, 0.13f, 0.13f, 1.0f);
        float indicatorThickness_ = 2.5f;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
        D2D1_COLOR_F hoverBgColor_, hoverBorderColor_;
        std::vector<std::wstring> undoStack_;
        int undoIndex_;
        bool passwordMode_;
        int maxLength_;
        float cursorBlinkInterval_;
        float hoverAnimSpeed_;
        float scrollX_;
        std::wstring compositionText_;
        bool hasComposition_;
        int compositionCursorPos_;

        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> placeholderBrush_;
        ComPtr<ID2D1SolidColorBrush> selectionBrush_;
        ComPtr<ID2D1SolidColorBrush> cursorBrush_;
        ComPtr<ID2D1SolidColorBrush> compositionBrush_;
    };

    // ---------- 下拉框（最终增强版） ----------
    class ComboBox : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::ComboBox; }
        std::wstring DefaultAccessibleName() const override { return editable_ ? editText_ : GetSelectedText(); }
        float DebugAnimationProgress() const override { return expandProgress_; }
        inline static float DefaultExpandAnimationSpeed = 7.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultIndicatorAnimationSpeed = 12.0f;
        inline static D2D1_COLOR_F DefaultNormalBgColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverBgColor = D2D1::ColorF(0.88f, 0.88f, 0.88f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverItemColor = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultListItemHeight = 24.0f;
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultWidth = 160.0f;
        inline static float DefaultHeight = 30.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;

        // 类型默认字体
        inline static FontSpec DefaultFontSpec = []() {
            FontSpec s;
            s.size = 14.0f;
            return s;
            }();

        ZSignal<int> SelectionChanged;

        ComboBox() : selectedIndex_(-1), expanded_(false), expandProgress_(0.0f),
            hoveredItemIndex_(-1),
            pressedItemIndex_(-1),
            pressedOnSelf_(false),
            justExpanded_(false),
            normalBgColor_(DefaultNormalBgColor),
            hoverItemColor_(DefaultHoverItemColor),
            hovered_(false), hoverProgress_(0.0f),
            hoverBgColor_(DefaultHoverBgColor),
            borderColor_(DefaultBorderColor),
            listScrollOffset_(0.0f),
            listMaxScroll_(0.0f),
            listViewHeight_(0.0f),
            listItemHeight_(DefaultListItemHeight),
            indicatorY_(0.0f),
            targetIndicatorY_(0.0f),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            expandSpeed_(DefaultExpandAnimationSpeed),
            hoverSpeed_(DefaultHoverAnimationSpeed),
            indicatorSpeed_(DefaultIndicatorAnimationSpeed),
            controlCaptureActive_(false),
            expandUp_(false) {
            width_ = DefaultWidth; height_ = DefaultHeight;
            UpdateIndicatorPosition();

            // 展开列表的叠加绘制改为“按需连接”（见 EnsureOverlayConnected）：未展开的 ComboBox 不挂全局 DrawOverlay，
            // 避免每个 ComboBox 每帧都跑一遍空 lambda。

            UIZSignals::GlobalMouseDown.connect(
                [this](Window* w, float x, float y) {
                    if (w && w != GetWindow()) return;   // 只处理本窗口的点击
                    auto _rg = RenderGuard();
                    if ((expanded_ || expandProgress_ > 0.01f) && !controlCaptureActive_) {
                        bool insideSelf = arrangedRect_.Contains(x, y) || IsPointInExpandedList(x, y);
                        if (!insideSelf) {
                            CollapseInternal();
                        }
                    }
                },
                ConnectionThread::CurrentThread,
                connectionGroup_
            );

            UIZSignals::WindowDeactivated.connect(
                [this](Window* w) {
                    if (w && w != GetWindow()) return;   // 只处理本窗口失活
                    auto _rg = RenderGuard();
                    if (expanded_ || expandProgress_ > 0.01f) CollapseInternal();
                },
                ConnectionThread::CurrentThread,
                connectionGroup_
            );
        }

        ~ComboBox() {
            ReleaseControlCapture();
        }

        void SetExpandAnimationSpeed(float speed) { expandSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverSpeed_ = speed; }
        void SetIndicatorAnimationSpeed(float speed) { indicatorSpeed_ = speed; }
        void SetNormalBgColor(D2D1_COLOR_F color) { normalBgColor_ = color; RequestRepaint(); }
        void SetHoverBgColor(D2D1_COLOR_F color) { hoverBgColor_ = color; RequestRepaint(); }
        void SetHoverItemColor(D2D1_COLOR_F color) { hoverItemColor_ = color; RequestRepaint(); }
        void SetBorderColor(D2D1_COLOR_F color) { borderColor_ = color; RequestRepaint(); }
        void SetIndicatorColor(D2D1_COLOR_F color) { indicatorColor_ = color; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetListItemHeight(float height) { listItemHeight_ = height; UpdateIndicatorPosition(); InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }

        static void SetDefaultExpandAnimationSpeed(float speed) { DefaultExpandAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultIndicatorAnimationSpeed(float speed) { DefaultIndicatorAnimationSpeed = speed; }
        static void SetDefaultNormalBgColor(D2D1_COLOR_F color) { DefaultNormalBgColor = color; }
        static void SetDefaultHoverBgColor(D2D1_COLOR_F color) { DefaultHoverBgColor = color; }
        static void SetDefaultHoverItemColor(D2D1_COLOR_F color) { DefaultHoverItemColor = color; }
        static void SetDefaultBorderColor(D2D1_COLOR_F color) { DefaultBorderColor = color; }
        static void SetDefaultIndicatorColor(D2D1_COLOR_F color) { DefaultIndicatorColor = color; }
        static void SetDefaultIndicatorHeightRatio(float ratio) { DefaultIndicatorHeightRatio = clamp(ratio, 0.1f, 1.0f); }
        static void SetDefaultListItemHeight(float height) { DefaultListItemHeight = height; }
        static void SetDefaultIndicatorWidth(float width) { DefaultIndicatorWidth = width; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultFontSize(float size) { ComboBox::DefaultFontSpec.size = size; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        std::optional<FontSpec> GetTypeDefaultFont() const override {
            return ComboBox::DefaultFontSpec;
        }

        void AddItem(const std::wstring& item) {
            auto _rg = RenderGuard();
            allItems_.push_back(item);
            ApplyFilter();
        }
        std::unique_lock<std::recursive_mutex> RenderGuard() {   // 数据变更与 render 线程（DrawOverlay 读 items_）串行
            Window* w = GetWindow();
            return w ? w->LockRender() : std::unique_lock<std::recursive_mutex>();
        }
        void SetItems(const std::vector<std::wstring>& items) {
            auto _rg = RenderGuard();
            allItems_ = items;
            disabledItems_.clear();   // 新数据集：禁用状态一并重置
            editText_.clear();
            caretPos_ = 0;
            ApplyFilter();
        }
        void SetSelectedIndex(int index) {
            auto _rg = RenderGuard();
            if (index >= 0 && index < (int)items_.size()) {
                if (selectedIndex_ != index) {
                    selectedIndex_ = index;
                    UpdateIndicatorPosition();
                    SelectionChanged(selectedIndex_);
                    RequestRepaint();
                }
                if (editable_) { editText_ = items_[index]; caretPos_ = (int)editText_.size(); }
                if (expanded_) CollapseInternal();
                UpdateToolTip();
            }
        }
        int GetSelectedIndex() const { return selectedIndex_; }
        std::wstring GetSelectedText() const { return selectedIndex_ >= 0 ? items_[selectedIndex_] : L""; }
        // 源索引（allItems_）与可见索引（items_）互转：过滤开启时二者不同
        void SetSelectedSourceIndex(int srcIndex) {
            auto _rg = RenderGuard();
            if (srcIndex < 0 || srcIndex >= (int)allItems_.size()) return;
            for (int i = 0; i < (int)filteredToSource_.size(); ++i)
                if (filteredToSource_[i] == srcIndex) { SetSelectedIndex(i); return; }
        }
        int GetSelectedSourceIndex() const {
            return (selectedIndex_ >= 0 && selectedIndex_ < (int)filteredToSource_.size()) ? filteredToSource_[selectedIndex_] : -1;
        }
        int SourceIndexOf(int visibleIndex) const {
            return (visibleIndex >= 0 && visibleIndex < (int)filteredToSource_.size()) ? filteredToSource_[visibleIndex] : -1;
        }

        bool IsExpanded() const { return expanded_; }
        // 所属页面/元素被隐藏时自动收起下拉（避免隐藏页里的展开弹层继续通过全局 DrawOverlay 绘制）
        void OnVisibilityChanged(bool visible) override {
            auto _rg = RenderGuard();
            if (!visible && (expanded_ || expandProgress_ > 0.01f)) CollapseInternal();
        }
        float GetExpandProgress() const { return expandProgress_; }
        void Collapse() { auto _rg = RenderGuard(); CollapseInternal(); }

        // ---------- 可编辑 / 输入过滤 ----------
        void SetEditable(bool editable) { editable_ = editable; RequestRepaint(); }
        bool IsEditable() const { return editable_; }
        void SetFilterEnabled(bool enable) { filterEnabled_ = enable; ApplyFilter(); }
        bool IsFilterEnabled() const { return filterEnabled_; }
        void SetEditText(const std::wstring& text) {
            auto _rg = RenderGuard();
            editText_ = text;
            caretPos_ = (int)editText_.size();
            if (filterEnabled_) ApplyFilter(); else { UpdateToolTip(); RequestRepaint(); }
        }
        std::wstring GetEditText() const { return editText_; }
        bool IsFocusable() const override { return editable_; }
        static std::wstring ToLower(std::wstring s) { for (auto& c : s) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32); return s; }
        void ApplyFilter() {
            auto _rg = RenderGuard();
            // 保留原选中：优先按"源索引"（重复文本也稳），退回按文本
            int prevSrc = (selectedIndex_ >= 0 && selectedIndex_ < (int)filteredToSource_.size()) ? filteredToSource_[selectedIndex_] : -1;
            std::wstring prevSel = (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) ? items_[selectedIndex_] : L"";
            items_.clear();
            filteredToSource_.clear();
            if (!filterEnabled_ || editText_.empty()) {
                items_ = allItems_;
                filteredToSource_.resize(allItems_.size());
                for (int i = 0; i < (int)allItems_.size(); ++i) filteredToSource_[i] = i;
            }
            else {
                std::wstring key = ToLower(editText_);
                for (int i = 0; i < (int)allItems_.size(); ++i)
                    if (ToLower(allItems_[i]).find(key) != std::wstring::npos) {
                        items_.push_back(allItems_[i]);
                        filteredToSource_.push_back(i);
                    }
            }
            if ((int)disabledItems_.size() < (int)allItems_.size()) disabledItems_.resize(allItems_.size(), false);   // 禁用状态按源索引保留，不再被过滤清空
            selectedIndex_ = -1;
            for (int i = 0; i < (int)items_.size(); ++i)
                if ((prevSrc >= 0 && filteredToSource_[i] == prevSrc) || (!prevSel.empty() && items_[i] == prevSel)) { selectedIndex_ = i; break; }
            if (selectedIndex_ < 0 && !items_.empty()) selectedIndex_ = 0;
            itemWidthsDirty_ = true;
            UpdateIndicatorPosition();
            UpdateToolTip();
            InvalidateLayout();
            RequestRepaint();
        }

        // ---------- 数据操作 / 占位符 / 每项禁用 / 开合信号 / 最大可见项 ----------
        void InsertItem(int index, const std::wstring& item) {
            auto _rg = RenderGuard();
            int oldSize = (int)allItems_.size();
            index = max(0, min(oldSize, index));
            allItems_.insert(allItems_.begin() + index, item);
            if ((int)disabledItems_.size() < oldSize) disabledItems_.resize(oldSize, false);
            disabledItems_.insert(disabledItems_.begin() + index, false);   // 禁用状态随源索引一起插入
            ApplyFilter();
        }
        void RemoveItemAt(int index) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)allItems_.size()) return;
            allItems_.erase(allItems_.begin() + index);
            if (index < (int)disabledItems_.size()) disabledItems_.erase(disabledItems_.begin() + index);
            ApplyFilter();
        }
        void RemoveItem(const std::wstring& item) {
            auto _rg = RenderGuard();
            for (int i = 0; i < (int)allItems_.size(); ++i) if (allItems_[i] == item) { RemoveItemAt(i); return; }
        }
        void ClearItems() { auto _rg = RenderGuard(); allItems_.clear(); items_.clear(); filteredToSource_.clear(); disabledItems_.clear(); selectedIndex_ = -1; UpdateIndicatorPosition(); InvalidateLayout(); RequestRepaint(); }
        int GetItemCount() const { return (int)items_.size(); }
        std::wstring GetItemAt(int index) const { return (index >= 0 && index < (int)items_.size()) ? items_[index] : L""; }
        const std::vector<std::wstring>& GetItems() const { return items_; }
        // index 为“可见索引”（items_）；内部按源索引（allItems_）存储，过滤后不丢禁用状态
        void SetItemDisabled(int index, bool disabled = true) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)filteredToSource_.size()) return;
            int src = filteredToSource_[index];
            if (src < 0 || src >= (int)allItems_.size()) return;
            if ((int)disabledItems_.size() < (int)allItems_.size()) disabledItems_.resize(allItems_.size(), false);
            disabledItems_[src] = disabled;
            RequestRepaint();
        }
        bool IsItemDisabled(int index) const {
            if (index < 0 || index >= (int)filteredToSource_.size()) return false;
            int src = filteredToSource_[index];
            return src >= 0 && src < (int)disabledItems_.size() && disabledItems_[src];
        }
        void SetPlaceholder(const std::wstring& placeholder) { placeholder_ = placeholder; RequestRepaint(); }
        const std::wstring& GetPlaceholder() const { return placeholder_; }
        void SetMaxVisibleItems(int count) { maxVisibleItems_ = max(0, count); InvalidateLayout(); RequestRepaint(); }
        int GetMaxVisibleItems() const { return maxVisibleItems_; }
        void Expand() { auto _rg = RenderGuard(); ExpandInternal(); }
        void SetOpen(bool open) { auto _rg = RenderGuard(); if (open) ExpandInternal(); else CollapseInternal(); }
        ZSignal<> DropDownOpened;
        ZSignal<> DropDownClosed;

        bool IsPointInExpandedList(float x, float y) const {
            if (!expanded_ && expandProgress_ <= 0.01f) return false;
            float listY = expandUp_ ? arrangedRect_.y - listViewHeight_ : arrangedRect_.y + arrangedRect_.height;
            Rect listRect(arrangedRect_.x, listY, ListWidth(), listViewHeight_);
            return listRect.Contains(x, y);
        }

        // 测量一段文本的像素宽度
        float MeasureStringWidth(const std::wstring& s) {
            if (s.empty()) return 0.0f;
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            IDWriteTextFormat* fmt = GetFontFormat();
            if (!factory || !fmt) return 0.0f;
            ComPtr<IDWriteTextLayout> layout;
            factory->CreateTextLayout(s.c_str(), (UINT32)s.length(), fmt, 10000.0f, 1000.0f, &layout);
            if (!layout) return 0.0f;
            DWRITE_TEXT_METRICS m;
            layout->GetMetrics(&m);
            return m.width;
        }
        void RecalcItemWidths() {
            itemWidthsDirty_ = false;
            avgItemWidth_ = 0.0f; widestItemWidth_ = 0.0f;
            const std::vector<std::wstring>& src = (filterEnabled_ && !editText_.empty()) ? items_ : allItems_;
            if (src.empty()) return;
            float sum = 0.0f;
            for (auto& s : src) {
                float w = MeasureStringWidth(s);
                sum += w;
                if (w > widestItemWidth_) widestItemWidth_ = w;
            }
            avgItemWidth_ = sum / (float)src.size();
        }
        // ToolTip 依赖文本与目标宽度，数据/选中/文本变化时更新（不放在 Measure 里，避免测量副作用）
        void UpdateToolTip() {
            if (itemWidthsDirty_) RecalcItemWidths();
            float w = width_;
            if (avgItemWidth_ > 0.0f) w = max(w, avgItemWidth_ + 36.0f);
            std::wstring disp = editable_
                ? editText_
                : (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size() ? items_[selectedIndex_] : std::wstring());
            if (!disp.empty() && MeasureStringWidth(disp) > w - 36.0f) SetToolTip(disp);
            else SetToolTip(std::wstring());
        }
        // 下拉框宽度：至少能完整显示最宽的选项
        float ListWidth() const {
            float w = arrangedRect_.width;
            if (widestItemWidth_ > 0.0f) w = max(w, widestItemWidth_ + 24.0f);
            return w;
        }

        Size MeasureOverride(const Size& availableSize) override {
            if (itemWidthsDirty_) RecalcItemWidths();   // 仅数据变化时重算，避免每帧测量所有选项
            // 折叠框宽度取“选项平均宽度”（放不下时再靠 tooltip 显示完整文本）
            float w = width_;
            if (avgItemWidth_ > 0.0f) w = max(w, avgItemWidth_ + 36.0f);   // 8 左内边距 + 箭头/右内边距
            return Size(w, height_);
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) return this;
            if (expanded_ || expandProgress_ > 0.01f) {
                float listY = expandUp_ ? arrangedRect_.y - listViewHeight_ : arrangedRect_.y + arrangedRect_.height;
                Rect listRect(arrangedRect_.x, listY, ListWidth(), listViewHeight_);
                if (listRect.Contains(x, y)) return this;
            }
            return nullptr;
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            IDWriteTextFormat* fmt = GetFontFormat();
            if (!fmt) return;

            D2D1_COLOR_F bgCol;
            if (pressedOnSelf_) {
                bgCol = D2D1::ColorF(hoverBgColor_.r * 0.9f, hoverBgColor_.g * 0.9f, hoverBgColor_.b * 0.9f, 1.0f);
            }
            else {
                bgCol = D2D1::ColorF(
                    normalBgColor_.r + (hoverBgColor_.r - normalBgColor_.r) * hoverProgress_,
                    normalBgColor_.g + (hoverBgColor_.g - normalBgColor_.g) * hoverProgress_,
                    normalBgColor_.b + (hoverBgColor_.b - normalBgColor_.b) * hoverProgress_,
                    1.0f);
            }
            if (!bgBrush_) rt->CreateSolidColorBrush(bgCol, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bgCol);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);

            if (fmt) {
                bool hasSel = selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size();
                if (!textBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), textBrush_.GetAddressOf());
                else textBrush_->SetColor(D2D1::ColorF(0, 0, 0, 1));
                if (textBrush_) {
                    D2D1_RECT_F txtRect = D2D1::RectF(arrangedRect_.x + 8, arrangedRect_.y,
                        arrangedRect_.x + arrangedRect_.width - 28, arrangedRect_.y + arrangedRect_.height);
                    std::wstring disp = editable_ ? editText_ : (hasSel ? items_[selectedIndex_] : L"");
                    if (!disp.empty()) {
                        IDWriteFactory* factory = FontManager::Instance().GetFactory();
                        ComPtr<IDWriteTextLayout> layout;
                        if (factory) {
                            factory->CreateTextLayout(disp.c_str(), (UINT32)disp.length(), fmt,
                                txtRect.right - txtRect.left, txtRect.bottom - txtRect.top, &layout);
                        }
                        if (layout) {
                            layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                            layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                            layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                            ComPtr<IDWriteInlineObject> ellipsis;
                            if (factory && SUCCEEDED(factory->CreateEllipsisTrimmingSign(fmt, &ellipsis))) {
                                DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                                layout->SetTrimming(&tr, ellipsis.Get());
                            }
                            rt->DrawTextLayout(D2D1::Point2F(txtRect.left, txtRect.top), layout.Get(), textBrush_.Get());
                        }
                    }
                    else if (!placeholder_.empty()) {
                        textBrush_->SetColor(D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f));
                        rt->DrawText(placeholder_.c_str(), (UINT32)placeholder_.length(), fmt, txtRect, textBrush_.Get());
                    }
                    if (editable_ && focused_ && showCursor_) {
                        float cx = arrangedRect_.x + 8 + MeasureEditX(caretPos_);
                        float maxX = arrangedRect_.x + arrangedRect_.width - 30.0f;
                        if (cx > maxX) cx = maxX;
                        if (cx < arrangedRect_.x + 8) cx = arrangedRect_.x + 8;
                        if (!caretBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f, 1), caretBrush_.GetAddressOf());
                        if (caretBrush_)
                            rt->DrawLine(D2D1::Point2F(cx, arrangedRect_.y + 4),
                                D2D1::Point2F(cx, arrangedRect_.y + arrangedRect_.height - 4), caretBrush_.Get(), 1.5f);
                    }
                }
            }

            std::wstring arrow = expanded_ ^ expandUp_ ? L"\u25B2" : L"\u25BC";
            if (!arrowBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.3f, 1), arrowBrush_.GetAddressOf());
            else arrowBrush_->SetColor(D2D1::ColorF(0.3f, 0.3f, 0.3f, 1));
            if (arrowBrush_) {
                D2D1_RECT_F arrowRect = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - 24, arrangedRect_.y,
                    arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + arrangedRect_.height);
                rt->DrawText(arrow.c_str(), (UINT32)arrow.length(), fmt, arrowRect, arrowBrush_.Get());
            }
        }

        void DrawExpandedList(ID2D1RenderTarget* rt) {
            IDWriteTextFormat* fmt = GetFontFormat();
            if (expandProgress_ <= 0.01f || items_.empty() || !fmt) return;

            D2D1_SIZE_F rtSize = rt->GetSize();
            UpdateListGeometry(rtSize.width, rtSize.height);   // 目标几何：UI 命中与渲染共用（不再各算各的）
            float currentListHeight = listViewHeight_ * expandProgress_;
            if (currentListHeight <= 0.0f) return;

            float listY = expandUp_ ? (arrangedRect_.y - currentListHeight)
                                    : (arrangedRect_.y + arrangedRect_.height);
            if (expandUp_ && listY < 0.0f) listY = 0.0f;

            D2D1_RECT_F listRect = D2D1::RectF(arrangedRect_.x, listY, arrangedRect_.x + ListWidth(), listY + currentListHeight);

            ComPtr<ID2D1RoundedRectangleGeometry> clipGeometry;
            ID2D1Factory* factory = nullptr;
            rt->GetFactory(&factory);
            if (factory) {
                factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(listRect, 4.0f, 4.0f), &clipGeometry);
                factory->Release();
            }

            if (clipGeometry) {
                D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
                    D2D1::InfiniteRect(),
                    clipGeometry.Get(),
                    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                    D2D1::IdentityMatrix(),
                    1.0f,
                    nullptr,
                    D2D1_LAYER_OPTIONS_NONE
                );
                rt->PushLayer(&layerParams, nullptr);
            }
            else {
                rt->PushAxisAlignedClip(listRect, D2D1_ANTIALIAS_MODE_ALIASED);
            }

            if (!listBgBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &listBgBrush_);
            else listBgBrush_->SetColor(D2D1::ColorF(1, 1, 1, 1));
            rt->FillRoundedRectangle(D2D1::RoundedRect(listRect, 4, 4), listBgBrush_.Get());

            if (!listBorderBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.6f, 0.6f, 1), &listBorderBrush_);
            else listBorderBrush_->SetColor(D2D1::ColorF(0.6f, 0.6f, 0.6f, 1));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(listRect, 4, 4), listBorderBrush_.Get(), 1.0f);

            int firstVisibleIndex = (int)(listScrollOffset_ / listItemHeight_);
            int lastVisibleIndex = min((int)items_.size() - 1, (int)((listScrollOffset_ + currentListHeight) / listItemHeight_));

            for (int i = firstVisibleIndex; i <= lastVisibleIndex; ++i) {
                float itemTop = listY + i * listItemHeight_ - listScrollOffset_;
                D2D1_RECT_F itemRect = D2D1::RectF(arrangedRect_.x, itemTop, arrangedRect_.x + ListWidth(), itemTop + listItemHeight_);
                bool isSelected = (i == selectedIndex_);
                bool isHovered = (i == hoveredItemIndex_);
                bool isPressed = (i == pressedItemIndex_);
                bool isDisabled = IsItemDisabled(i);
                D2D1_COLOR_F itemBgCol = D2D1::ColorF(1, 1, 1, 1);
                if (isSelected) itemBgCol = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1);
                if (isHovered && !isDisabled) itemBgCol = hoverItemColor_;
                if (isPressed && !isDisabled) itemBgCol = D2D1::ColorF(0.7f, 0.7f, 0.7f, 1);
                if (isSelected && isHovered && !isPressed && !isDisabled) itemBgCol = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1);

                if (!itemBgBrush_) rt->CreateSolidColorBrush(itemBgCol, &itemBgBrush_);
                else itemBgBrush_->SetColor(itemBgCol);
                rt->FillRectangle(itemRect, itemBgBrush_.Get());

                D2D1_COLOR_F itemTextCol = isDisabled ? D2D1::ColorF(0.65f, 0.65f, 0.65f, 1) : D2D1::ColorF(0, 0, 0, 1);
                if (!itemTextBrush_) rt->CreateSolidColorBrush(itemTextCol, &itemTextBrush_);
                else itemTextBrush_->SetColor(itemTextCol);
                D2D1_RECT_F textRect = D2D1::RectF(itemRect.left + 12, itemRect.top, itemRect.right - 4, itemRect.bottom);
                rt->DrawText(items_[i].c_str(), (UINT32)items_[i].length(), fmt, textRect, itemTextBrush_.Get());
            }

            if (selectedIndex_ >= 0) {
                float indicatorX = arrangedRect_.x + 4.0f;
                float indicatorTop = listY + indicatorY_ - listScrollOffset_;
                float indicatorHeight = listItemHeight_ * indicatorHeightRatio_;
                float indicatorOffset = (listItemHeight_ - indicatorHeight) / 2.0f;
                float indicatorDrawTop = indicatorTop + indicatorOffset;
                float indicatorBottom = indicatorDrawTop + indicatorHeight;
                if (indicatorBottom > listY && indicatorDrawTop < listY + currentListHeight) {
                    if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, &indicatorBrush_);
                    else indicatorBrush_->SetColor(indicatorColor_);
                    D2D1_RECT_F indicatorRect = D2D1::RectF(indicatorX, indicatorDrawTop, indicatorX + indicatorWidth_, indicatorBottom);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, 1.5f, 1.5f), indicatorBrush_.Get());
                }
            }

            if (listMaxScroll_ > 0.0f) {
                float trackWidth = 6.0f;
                float trackX = arrangedRect_.x + ListWidth() - trackWidth - 2.0f;
                float trackY = listY + 2.0f;
                float trackHeight = currentListHeight - 4.0f;
                if (!scrollTrackBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f), &scrollTrackBrush_);
                else scrollTrackBrush_->SetColor(D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f));
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight), trackWidth / 2, trackWidth / 2), scrollTrackBrush_.Get());

                float thumbHeight = max(20.0f, trackHeight * (trackHeight / (trackHeight + listMaxScroll_)));
                float thumbY = trackY + (trackHeight - thumbHeight) * (listScrollOffset_ / listMaxScroll_);
                if (!scrollThumbBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f), &scrollThumbBrush_);
                else scrollThumbBrush_->SetColor(D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f));
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, thumbY, trackX + trackWidth, thumbY + thumbHeight), trackWidth / 2, trackWidth / 2), scrollThumbBrush_.Get());
            }

            if (clipGeometry) rt->PopLayer();
            else rt->PopAxisAlignedClip();
        }

        void UpdateAnimation(float deltaTime) override {
            float targetExpand = expanded_ ? 1.0f : 0.0f;
            if (expandProgress_ < targetExpand) { expandProgress_ += expandSpeed_ * deltaTime; if (expandProgress_ > targetExpand) expandProgress_ = targetExpand; }
            else if (expandProgress_ > targetExpand) { expandProgress_ -= expandSpeed_ * deltaTime; if (expandProgress_ < targetExpand) expandProgress_ = targetExpand; }

            float targetHover = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < targetHover) { hoverProgress_ += hoverSpeed_ * deltaTime; if (hoverProgress_ > targetHover) hoverProgress_ = targetHover; }
            else if (hoverProgress_ > targetHover) { hoverProgress_ -= hoverSpeed_ * deltaTime; if (hoverProgress_ < targetHover) hoverProgress_ = targetHover; }

            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;

            if (editable_ && focused_) {
                cursorBlinkTime_ += deltaTime;
                if (cursorBlinkTime_ >= cursorBlinkInterval_) { cursorBlinkTime_ = 0.0f; showCursor_ = !showCursor_; RequestRepaint(); }
            }

            if (HasActiveAnimation()) RequestRepaint();
            ConvergeValue(expandProgress_, expanded_ ? 1.0f : 0.0f, 0.001f);
            ConvergeValue(hoverProgress_, hovered_ ? 1.0f : 0.0f, 0.001f);
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.1f);
            // 收起动画已收敛到 0：在 UI 线程断连 DrawOverlay（render 线程只负责画，不碰 Connection）
            if (!expanded_ && expandProgress_ <= 0.001f)
                overlayConn_.disconnect();
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.001f;
            return (expanded_ ? (expandProgress_ < 1.0f - epsilon) : (expandProgress_ > epsilon)) ||
                (hovered_ ? (hoverProgress_ < 1.0f - epsilon) : (hoverProgress_ > epsilon)) ||
                (fabs(indicatorY_ - targetIndicatorY_) > 0.01f) ||
                (editable_ && focused_ && cursorBlinkInterval_ > 0.0f);
        }

        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; hoveredItemIndex_ = -1; RequestRepaint(); MouseLeave.Fire(); }

        void OnMouseMove(float x, float y) override {
            auto _rg = RenderGuard();
            if (!expanded_) {
                hoveredItemIndex_ = -1;
                MouseMove.Fire(x, y);
                return;
            }
            int prevHover = hoveredItemIndex_;

            float listY = expandUp_ ? arrangedRect_.y - listViewHeight_ : arrangedRect_.y + arrangedRect_.height;
            if (x >= arrangedRect_.x && x < arrangedRect_.x + ListWidth() &&
                y >= listY && y < listY + listViewHeight_) {
                float adjustedY = y - listY + listScrollOffset_;
                int idx = (int)(adjustedY / listItemHeight_);
                if (idx >= 0 && idx < (int)items_.size()) hoveredItemIndex_ = idx;
                else hoveredItemIndex_ = -1;
            }
            else {
                hoveredItemIndex_ = -1;
            }
            if (hoveredItemIndex_ != prevHover) RequestRepaint();
            MouseMove.Fire(x, y);
        }

        void OnMouseDown(float x, float y) override {
            auto _rg = RenderGuard();
            if (expanded_) {
                bool insideList = IsPointInExpandedList(x, y);
                bool insideSelf = arrangedRect_.Contains(x, y);

                if (insideList) {
                    int idx = GetItemIndexFromPoint(x, y);
                    pressedItemIndex_ = idx;
                    pressedOnSelf_ = false;
                    RequestRepaint();
                }
                else if (insideSelf) {
                    pressedOnSelf_ = true;
                    pressedItemIndex_ = -1;
                    PlaceCaretFromX(x);
                    RequestRepaint();
                }
                else {
                    CollapseInternal();
                }
                MouseDown.Fire(x, y);
                return;
            }

            ExpandInternal();
            if (!justExpanded_) PlaceCaretFromX(x);   // 刚展开这一次点击不重定位光标（justExpanded_ 生效）
            MouseDown.Fire(x, y);
        }

        void OnMouseUp(float x, float y) override {
            auto _rg = RenderGuard();
            if (justExpanded_) {
                justExpanded_ = false;
                MouseUp.Fire(x, y);
                return;
            }

            if (expanded_) {
                if (pressedItemIndex_ >= 0) {
                    int idx = GetItemIndexFromPoint(x, y);
                    if (idx == pressedItemIndex_ && !IsItemDisabled(idx)) {
                        if (selectedIndex_ != idx) {
                            selectedIndex_ = idx;
                            UpdateIndicatorPosition();
                            SelectionChanged(selectedIndex_);
                        }
                        if (editable_) { editText_ = items_[idx]; caretPos_ = (int)editText_.size(); }
                    }
                }
                CollapseInternal();
            }
            RequestRepaint();
            MouseUp.Fire(x, y);
        }

        bool OnMouseWheel(float deltaX, float deltaY) override {
            auto _rg = RenderGuard();
            if (expanded_ && listMaxScroll_ > 0) {
                listScrollOffset_ = clamp(listScrollOffset_ - deltaY * 30.0f, 0.0f, listMaxScroll_);
                RequestRepaint();
                return true;
            }
            return false;
        }

        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            auto _rg = RenderGuard();
            if (editable_) {
                switch (key) {
                case VK_BACK:
                    if (caretPos_ > 0 && !editText_.empty()) {
                        editText_.erase(caretPos_ - 1, 1);
                        caretPos_--;
                        ResetCursorBlink();
                        if (filterEnabled_) ApplyFilter(); else { UpdateToolTip(); RequestRepaint(); }
                    }
                    KeyDown.Fire(key, lParam);
                    return;
                case VK_DELETE:
                    if (caretPos_ >= 0 && caretPos_ < (int)editText_.size()) {
                        editText_.erase(caretPos_, 1);
                        ResetCursorBlink();
                        if (filterEnabled_) ApplyFilter(); else { UpdateToolTip(); RequestRepaint(); }
                    }
                    return;
                case VK_LEFT: caretPos_ = max(0, caretPos_ - 1); ResetCursorBlink(); RequestRepaint(); return;
                case VK_RIGHT: caretPos_ = min((int)editText_.size(), caretPos_ + 1); ResetCursorBlink(); RequestRepaint(); return;
                case VK_HOME: caretPos_ = 0; ResetCursorBlink(); RequestRepaint(); return;
                case VK_END: caretPos_ = (int)editText_.size(); ResetCursorBlink(); RequestRepaint(); return;
                case VK_RETURN:
                    if (expanded_ && selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                        editText_ = items_[selectedIndex_];
                        caretPos_ = (int)editText_.size();
                    }
                    CollapseInternal();
                    RequestRepaint();
                    return;
                case VK_ESCAPE: CollapseInternal(); return;
                default: break;
                }
            }
            if (expanded_) {
                if (key == VK_ESCAPE) { CollapseInternal(); return; }
                if (key == VK_UP || key == VK_DOWN) {
                    int step = (key == VK_UP) ? -1 : 1;
                    int newIdx = selectedIndex_ + step;
                    while (newIdx >= 0 && newIdx < (int)items_.size() && IsItemDisabled(newIdx)) newIdx += step;
                    if (newIdx >= 0 && newIdx < (int)items_.size()) SetSelectedIndex(newIdx);
                    return;
                }
                if (key == VK_RETURN) { CollapseInternal(); return; }
            }
            KeyDown.Fire(key, lParam);
        }

        void OnFocus() override { focused_ = true; ResetCursorBlink(); RequestRepaint(); Focused.Fire(); }
        void OnBlur() override {
            focused_ = false;
            if (expanded_) CollapseInternal();
            RequestRepaint();
            Blurred.Fire();
        }
        void OnChar(wchar_t ch) override {
            auto _rg = RenderGuard();
            if (!editable_ || !focused_) return;
            if (ch < 32 || ch == L'\r' || ch == L'\n') return;
            if (caretPos_ < 0) caretPos_ = 0;
            if (caretPos_ > (int)editText_.size()) caretPos_ = (int)editText_.size();
            editText_.insert(caretPos_, 1, ch);
            caretPos_++;
            ResetCursorBlink();
            if (filterEnabled_) {
                ApplyFilter();
                if (!expanded_ && !items_.empty()) ExpandInternal();
            }
            else { UpdateToolTip(); RequestRepaint(); }
            Char.Fire(ch);
        }

        void CollectExpandedComboBoxes(std::vector<ComboBox*>& list) override {
            if (expanded_ || expandProgress_ > 0.01f) list.push_back(this);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); borderBrush_.Reset(); textBrush_.Reset(); arrowBrush_.Reset();
            listBgBrush_.Reset(); listBorderBrush_.Reset(); itemBgBrush_.Reset(); itemTextBrush_.Reset();
            scrollTrackBrush_.Reset(); scrollThumbBrush_.Reset(); indicatorBrush_.Reset(); caretBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        float MeasureEditX(int index) const {
            if (editText_.empty()) return 0.0f;
            if (index < 0) index = 0;
            if (index > (int)editText_.size()) index = (int)editText_.size();
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            IDWriteTextFormat* efmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            if (!factory || !efmt) return index * 7.0f;
            ComPtr<IDWriteTextLayout> layout;
            factory->CreateTextLayout(editText_.c_str(), (UINT32)editText_.length(), efmt, 10000.0f, 100.0f, &layout);
            if (!layout) return index * 7.0f;
            DWRITE_HIT_TEST_METRICS m{};
            float x = 0.0f, y = 0.0f;
            layout->HitTestTextPosition((UINT32)index, false, &x, &y, &m);
            return x;
        }
        int EditIndexFromX(float localX) const {
            if (editText_.empty()) return 0;
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            IDWriteTextFormat* efmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            if (!factory || !efmt) return (int)(localX / 7.0f);
            ComPtr<IDWriteTextLayout> layout;
            factory->CreateTextLayout(editText_.c_str(), (UINT32)editText_.length(), efmt, 10000.0f, 100.0f, &layout);
            if (!layout) return (int)(localX / 7.0f);
            BOOL trailing = FALSE, inside = FALSE;
            DWRITE_HIT_TEST_METRICS m{};
            layout->HitTestPoint(localX, 5.0f, &trailing, &inside, &m);
            int pos = (int)m.textPosition;
            if (trailing) pos += (int)m.length;
            if (pos < 0) pos = 0;
            if (pos > (int)editText_.size()) pos = (int)editText_.size();
            return pos;
        }
        void ResetCursorBlink() { showCursor_ = true; cursorBlinkTime_ = 0.0f; }
        void PlaceCaretFromX(float x) {
            if (!editable_) return;
            caretPos_ = EditIndexFromX(x - (arrangedRect_.x + 8));
            ResetCursorBlink();
        }

        void UpdateIndicatorPosition() {
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                targetIndicatorY_ = selectedIndex_ * listItemHeight_;
            }
            else {
                targetIndicatorY_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
        }

        int GetItemIndexFromPoint(float x, float y) const {
            if (!expanded_) return -1;
            float listY = expandUp_ ? arrangedRect_.y - listViewHeight_ : arrangedRect_.y + arrangedRect_.height;
            if (x < arrangedRect_.x || x > arrangedRect_.x + ListWidth() ||
                y < listY || y > listY + listViewHeight_) return -1;
            float adjustedY = y - listY + listScrollOffset_;
            int idx = (int)(adjustedY / listItemHeight_);
            if (idx >= 0 && idx < (int)items_.size()) return idx;
            return -1;
        }

        void AcquireControlCapture() {
            if (!controlCaptureActive_) {
                if (GetWindow()) GetWindow()->RequestElementCapture(this);
                else UIZSignals::ElementCaptureRequest(GetWindow(), this);
                controlCaptureActive_ = true;
            }
        }

        void ReleaseControlCapture() {
            if (controlCaptureActive_) {
                if (GetWindow()) GetWindow()->ReleaseElementCapture(this);
                else UIZSignals::ElementCaptureRelease(GetWindow(), this);
                controlCaptureActive_ = false;
            }
        }

        // 展开时才挂全局 DrawOverlay；收起且动画结束即断连（懒连接，避免未展开的 ComboBox 每帧空跑）
        void EnsureOverlayConnected() {
            if (overlayConn_) return;
            overlayConn_ = UIZSignals::DrawOverlay.connect(
                [this](Window* w, ID2D1RenderTarget* rt) {
                    if (w != GetWindow()) return;                 // 只画在自己所属窗口上
                    if (expanded_ || expandProgress_ > 0.01f) DrawExpandedList(rt);
                    // 不再在 render 线程 disconnect（Connection 引用计数跨线程竞态）；
                    // 收起动画结束后的断连改由 UI 线程的 UpdateAnimation 完成。
                },
                ConnectionThread::CurrentThread,
                connectionGroup_);
        }

        // 下拉目标几何（动画结束态）：UI 命中/箭头方向与 render 共用同一份，避免"UI 命中用上一帧几何"
        void UpdateListGeometry() {
            UpdateListGeometry(0.0f, GetWindow() ? GetWindow()->GetClientHeightDip() : 0.0f);
        }
        void UpdateListGeometry(float /*windowW*/, float windowH) {
            float fullListHeight = (float)items_.size() * listItemHeight_;
            float visibleListHeight = fullListHeight;
            if (maxVisibleItems_ > 0) visibleListHeight = min(fullListHeight, maxVisibleItems_ * listItemHeight_);
            if (windowH <= 0.0f) {   // 窗口未就绪：不做屏幕 clamp（保证命中区非空）
                listViewHeight_ = visibleListHeight;
                listMaxScroll_ = max(0.0f, fullListHeight - listViewHeight_);
                return;
            }
            float belowSpace = windowH - (arrangedRect_.y + arrangedRect_.height);
            float aboveSpace = arrangedRect_.y;
            expandUp_ = (belowSpace < fullListHeight && aboveSpace > belowSpace);
            float avail = expandUp_ ? aboveSpace : belowSpace;
            if (avail < 0.0f) avail = 0.0f;
            if (avail < visibleListHeight) visibleListHeight = avail;
            listViewHeight_ = visibleListHeight;                 // 目标高度（不再被 render 的动画值覆盖）
            listMaxScroll_ = max(0.0f, fullListHeight - listViewHeight_);
            listScrollOffset_ = clamp(listScrollOffset_, 0.0f, listMaxScroll_);
        }

        void ExpandInternal() {
            if (expanded_ || items_.empty()) return;
            EnsureOverlayConnected();   // 按需连接全局 DrawOverlay
            expanded_ = true;
            justExpanded_ = true;
            AcquireControlCapture();
            hoveredItemIndex_ = -1;
            pressedItemIndex_ = -1;
            pressedOnSelf_ = false;
            listScrollOffset_ = 0.0f;
            UpdateListGeometry();   // 立即算出目标几何（命中/箭头不再等于第一帧渲染后）
            InvalidateLayout();
            RequestRepaint();
            DropDownOpened();
        }

        void CollapseInternal() {
            bool wasExpanded = expanded_;
            expanded_ = false;
            if (controlCaptureActive_) {
                ReleaseControlCapture();
            }
            hoveredItemIndex_ = -1;
            pressedItemIndex_ = -1;
            pressedOnSelf_ = false;
            justExpanded_ = false;
            expandUp_ = false;
            if (wasExpanded) {
                InvalidateLayout();
                RequestRepaint();
                DropDownClosed();
            }
        }

        std::vector<std::wstring> items_;
        std::vector<std::wstring> allItems_;
        std::vector<bool> disabledItems_;      // 按源索引（allItems_）存储
        std::vector<int> filteredToSource_;    // items_[i] 对应的 allItems_ 源索引
        std::wstring placeholder_;
        int maxVisibleItems_ = 0;
        bool editable_ = false;
        bool filterEnabled_ = false;
        std::wstring editText_;
        int caretPos_ = 0;
        bool focused_ = false;
        bool showCursor_ = true;
        float cursorBlinkTime_ = 0.0f;
        float cursorBlinkInterval_ = 0.5f;
        ComPtr<ID2D1SolidColorBrush> caretBrush_;
        int selectedIndex_;
        bool expanded_;
        float expandProgress_;
        Connection overlayConn_;   // 懒连接的 DrawOverlay 句柄
        int hoveredItemIndex_;
        int pressedItemIndex_;
        bool pressedOnSelf_;
        bool justExpanded_;

        D2D1_COLOR_F normalBgColor_;
        D2D1_COLOR_F hoverItemColor_;
        D2D1_COLOR_F hoverBgColor_;
        D2D1_COLOR_F borderColor_;
        bool hovered_;
        float hoverProgress_;

        float listScrollOffset_;
        float listMaxScroll_;
        float listViewHeight_;
        float listItemHeight_;

        float avgItemWidth_ = 0.0f;      // 选项文本平均宽度 → 折叠框据此定宽
        float widestItemWidth_ = 0.0f;   // 最宽选项文本 → 下拉框据此定宽（保证完整显示）
        bool itemWidthsDirty_ = true;    // 宽度缓存失效标志（数据变化才重算，避免每帧测量）

        float indicatorY_;
        float targetIndicatorY_;
        float indicatorWidth_;
        float indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;

        float expandSpeed_;
        float hoverSpeed_;
        float indicatorSpeed_;

        bool controlCaptureActive_;
        bool expandUp_;

        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> arrowBrush_;
        ComPtr<ID2D1SolidColorBrush> listBgBrush_;
        ComPtr<ID2D1SolidColorBrush> listBorderBrush_;
        ComPtr<ID2D1SolidColorBrush> itemBgBrush_;
        ComPtr<ID2D1SolidColorBrush> itemTextBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollThumbBrush_;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
    };

    // ---------- 开关 ----------
    class ToggleSwitch : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::CheckBox; }
        int  GetAccessibleToggleState() const override { return IsOn() ? 1 : 0; }   // UIA Toggle
        void AccessibilityToggle() override { SetOn(!IsOn()); }
        float DebugAnimationProgress() const override { return toggleProgress_; }
        inline static float DefaultAnimationSpeed = 9.6f;
        inline static Color DefaultOnColor = Color::FromArgb(255, 0, 120, 212);
        inline static Color DefaultOffColor = Color::FromArgb(255, 200, 200, 200);
        inline static Color DefaultKnobColor = Color::FromArgb(255, 255, 255, 255);
        inline static float DefaultWidth = 50.0f;
        inline static float DefaultHeight = 24.0f;
        inline static float DefaultHorizontalStretchWeight = 0.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;
        inline static Color DefaultDisabledColor = Color::FromArgb(255, 224, 224, 224);

        ZSignal<bool> Toggled;

        ToggleSwitch(bool initialState = false) : isOn_(initialState), hovered_(false), hoverProgress_(0.0f), toggleProgress_(initialState ? 1.0f : 0.0f),
            onColor_(DefaultOnColor), offColor_(DefaultOffColor), knobColor_(DefaultKnobColor),
            animSpeed_(DefaultAnimationSpeed) {
            width_ = DefaultWidth; height_ = DefaultHeight;
        }

        void SetOn(bool on) {
            if (isOn_ != on) {
                isOn_ = on;
                Toggled(isOn_);
                RequestRepaint();
                AccessibilityNotifyPropertyChanged();   // UIA Toggle 变化
            }
        }
        bool IsOn() const { return isOn_; }
        void SetColors(Color on, Color off, Color knob) { onColor_ = on; offColor_ = off; knobColor_ = knob; trackBrush_.Reset(); knobBrush_.Reset(); RequestRepaint(); }
        void SetAnimationSpeed(float speed) { animSpeed_ = speed; }
        void SetSize(float w, float h) { width_ = w; height_ = h; InvalidateLayout(); RequestRepaint(); }
        void SetIndeterminate(bool ind) { indeterminate_ = ind; RequestRepaint(); }
        bool IsIndeterminate() const { return indeterminate_; }
        void SetLabel(const std::wstring& text) { label_ = text; InvalidateLayout(); RequestRepaint(); }
        std::wstring GetLabel() const { return label_; }
        void SetLabelColor(Color c) { labelColor_ = c.ToD2D(); labelBrush_.Reset(); RequestRepaint(); }

        static void SetDefaultAnimationSpeed(float speed) { DefaultAnimationSpeed = speed; }
        static void SetDefaultColors(Color on, Color off, Color knob) { DefaultOnColor = on; DefaultOffColor = off; DefaultKnobColor = knob; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        void ArrangeOverride(const Rect& finalRect) override {
            float tw = width_, th = height_;
            float w = label_.empty() ? tw : finalRect.width;
            float h = max(th, finalRect.height);
            UIElement::ArrangeOverride(Rect(finalRect.x, finalRect.y, w, h));
            float ty = finalRect.y + (h - th) / 2.0f;
            trackRect_ = Rect(finalRect.x, ty, tw, th);
        }

        Size MeasureOverride(const Size& availableSize) override {
            float tw = width_, th = height_;
            if (label_.empty()) return Size(tw, th);
            auto fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            ComPtr<IDWriteTextLayout> layout;
            if (fmt) FontManager::Instance().GetFactory()->CreateTextLayout(label_.c_str(), (UINT32)label_.length(), fmt, 10000.0f, 100.0f, &layout);
            DWRITE_TEXT_METRICS tm{};
            if (layout) layout->GetMetrics(&tm);
            return Size(tw + 6.0f + tm.width, max(th, tm.height));
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            bool en = IsEffectivelyEnabled();
            Rect& tr = trackRect_;
            Color trackColor = en ? Color::Lerp(offColor_, onColor_, toggleProgress_) : DefaultDisabledColor;
            if (!trackBrush_) rt->CreateSolidColorBrush(trackColor.ToD2D(), trackBrush_.GetAddressOf());
            else trackBrush_->SetColor(trackColor.ToD2D());
            if (trackBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(tr.ToD2D(), tr.height / 2, tr.height / 2), trackBrush_.Get());

            float knobSize = tr.height - 4 + hoverProgress_ * 2.0f;
            float knobX = tr.x + 2 + (tr.width - 4 - knobSize) * toggleProgress_;
            Rect knobRect(knobX, tr.y + (tr.height - knobSize) / 2, knobSize, knobSize);

            if (!knobBrush_) rt->CreateSolidColorBrush(knobColor_.ToD2D(), knobBrush_.GetAddressOf());
            else knobBrush_->SetColor(knobColor_.ToD2D());
            if (knobBrush_) {
                D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(knobRect.x + knobRect.width / 2, knobRect.y + knobRect.height / 2), knobSize / 2, knobSize / 2);
                rt->FillEllipse(ellipse, knobBrush_.Get());
            }

            if (!label_.empty()) {
                auto fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
                D2D1_COLOR_F lc = en ? labelColor_ : D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
                if (!labelBrush_) rt->CreateSolidColorBrush(lc, labelBrush_.GetAddressOf());
                else labelBrush_->SetColor(lc);
                D2D1_RECT_F lr = D2D1::RectF(tr.x + tr.width + 6.0f, arrangedRect_.y, arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + arrangedRect_.height);
                if (fmt && labelBrush_) rt->DrawText(label_.c_str(), (UINT32)label_.length(), fmt, lr, labelBrush_.Get());
            }
        }

        void UpdateAnimation(float deltaTime) override {
            float target = indeterminate_ ? 0.5f : (isOn_ ? 1.0f : 0.0f);
            if (toggleProgress_ < target) { toggleProgress_ += animSpeed_ * deltaTime; if (toggleProgress_ > target) toggleProgress_ = target; }
            else if (toggleProgress_ > target) { toggleProgress_ -= animSpeed_ * deltaTime; if (toggleProgress_ < target) toggleProgress_ = target; }
            float hoverTarget = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < hoverTarget) { hoverProgress_ += animSpeed_ * deltaTime; if (hoverProgress_ > hoverTarget) hoverProgress_ = hoverTarget; }
            else if (hoverProgress_ > hoverTarget) { hoverProgress_ -= animSpeed_ * deltaTime; if (hoverProgress_ < hoverTarget) hoverProgress_ = hoverTarget; }

            if (HasActiveAnimation()) RequestRepaint();
            ConvergeValue(toggleProgress_, indeterminate_ ? 0.5f : (isOn_ ? 1.0f : 0.0f), 0.001f);
            ConvergeValue(hoverProgress_, hovered_ ? 1.0f : 0.0f, 0.001f);
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.001f;
            float tk = indeterminate_ ? 0.5f : (isOn_ ? 1.0f : 0.0f);
            return (fabs(toggleProgress_ - tk) > epsilon) ||
                (hovered_ ? (hoverProgress_ < 1.0f - epsilon) : (hoverProgress_ > epsilon));
        }

        void OnMouseEnter() override { if (!IsEffectivelyEnabled()) return; hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); MouseLeave.Fire(); }
        void OnMouseDown(float x, float y) override {
            if (!IsEffectivelyEnabled()) return;
            indeterminate_ = false;
            isOn_ = !isOn_;
            Toggled(isOn_);
            RequestRepaint();
            MouseDown.Fire(x, y);
        }
        bool IsFocusable() const override { return true; }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            if (key == VK_SPACE || key == VK_RETURN) { indeterminate_ = false; isOn_ = !isOn_; Toggled(isOn_); RequestRepaint(); }
            KeyDown.Fire(key, lParam);
        }

        void ReleaseDeviceResources() override {
            trackBrush_.Reset();
            knobBrush_.Reset();
            labelBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        bool isOn_, hovered_;
        float hoverProgress_, toggleProgress_;
        Color onColor_, offColor_, knobColor_;
        float animSpeed_;
        bool indeterminate_ = false;
        std::wstring label_;
        D2D1_COLOR_F labelColor_ = D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f);
        D2D1_RECT_F trackRectD2D_ = D2D1::RectF(0, 0, 0, 0);
        Rect trackRect_;
        ComPtr<ID2D1SolidColorBrush> trackBrush_;
        ComPtr<ID2D1SolidColorBrush> knobBrush_;
        ComPtr<ID2D1SolidColorBrush> labelBrush_;
    };

    // ============================================================================
    // ScrollBar：可复用的滚动条控件（纵向/横向）。
    //   - 自带 hover 扩张动画 + 空闲缩小动画（不依赖宿主）
    //   - 数值变化通过 ValueChanged 回调交给宿主（ScrollViewer / TabView ...）
    //   - 宿主每帧用 SetRange(value, maxValue, viewportSize) 推入最新范围
    // ============================================================================
    class ScrollBar : public UIElement {
    public:
        inline static float DefaultWidth = 8.0f;
        inline static float DefaultMinLength = 20.0f;
        inline static float DefaultHitExtra = 6.0f;
        inline static float DefaultIdleDelay = 2.0f;
        inline static float DefaultAnimationSpeed = 14.0f;
        inline static float ShrunkWidthRatio = 0.30f;   // 完全收缩时的粗细比例（Draw 用；宿主可据此留边）
        inline static D2D1_COLOR_F DefaultThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static D2D1_COLOR_F DefaultTrackColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f);

        std::function<void(float, bool animate)> ValueChanged;   // 用户拖动/点轨道 → (新值, 是否平滑)

        explicit ScrollBar(bool vertical) : vertical_(vertical) {
            visible_ = false;
            bleed_ = 4.0f;
            width_ = 0; height_ = 0;
            trackColor_ = DefaultTrackColor;
            thumbColor_ = DefaultThumbColor;
            hoverThumbColor_ = DefaultHoverThumbColor;
            lastActive_ = (double)GetTickCount64();   // 初始视为刚活跃（不缩小）
        }

        bool UseCache() const override { return false; }

        // ---- 宿主每帧推入范围 ----
        void SetRange(float value, float maxValue, float viewportSize) {
            if (value != value_) MarkActive();        // 值变了（滚动/动画）→ 保持不缩小
            value_ = value; maxValue_ = maxValue; viewportSize_ = viewportSize;
        }
        void SetValue(float v) { value_ = v; }
        float GetValue() const { return value_; }
        bool IsVertical() const { return vertical_; }

        // ---- 样式 ----
        void SetBarWidth(float w) { barWidth_ = max(1.0f, w); }
        float GetBarWidth() const { return barWidth_; }
        void SetMinLength(float l) { minLength_ = max(4.0f, l); }
        void SetHitExtra(float e) { hitExtra_ = max(0.0f, e); }
        void SetColors(D2D1_COLOR_F thumb, D2D1_COLOR_F hoverThumb, D2D1_COLOR_F track) {
            thumbColor_ = thumb; hoverThumbColor_ = hoverThumb; trackColor_ = track; RequestRepaint();
        }
        void SetIdleDelay(float s) { idleDelay_ = max(0.0f, s); }
        void SetAutoShrink(bool on) { autoShrink_ = on; RequestRepaint(); }
        void MarkActive() { lastActive_ = (double)GetTickCount64(); }

        // ---- UIElement ----
        Size MeasureOverride(const Size&) override {
            return vertical_ ? Size(barWidth_, 100.0f) : Size(100.0f, barWidth_);
        }
        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            Rect r = GetArrangedRect();
            Rect hot(r.x - hitExtra_, r.y - hitExtra_, r.width + hitExtra_ * 2, r.height + hitExtra_ * 2);
            return hot.Contains(x, y) ? this : nullptr;
        }
        void OnMouseEnter() override { hovering_ = true; MarkActive(); RequestRepaint(); }
        void OnMouseLeave() override { hovering_ = false; MarkActive(); if (!dragging_) RequestRepaint(); }
        void OnMouseDown(float x, float y) override {
            MarkActive();
            Rect r = GetArrangedRect();
            float pos, len; GetThumbInfo(r, pos, len);
            if (vertical_) {
                float start = r.y + pos, end = start + len;
                if (y >= start && y <= end) { dragging_ = true; dragStartMouse_ = y; dragStartValue_ = value_; }
                else { float ratio = (r.height > len) ? (y - r.y - len * 0.5f) / (r.height - len) : 0.0f; Fire(clamp(ratio * maxValue_, 0.0f, maxValue_), true); }
            }
            else {
                float start = r.x + pos, end = start + len;
                if (x >= start && x <= end) { dragging_ = true; dragStartMouse_ = x; dragStartValue_ = value_; }
                else { float ratio = (r.width > len) ? (x - r.x - len * 0.5f) / (r.width - len) : 0.0f; Fire(clamp(ratio * maxValue_, 0.0f, maxValue_), true); }
            }
        }
        void OnMouseMove(float x, float y) override {
            if (!dragging_) return;
            MarkActive();
            Rect r = GetArrangedRect();
            if (vertical_) {
                float len = ThumbLength(r.height);
                if (r.height > len) { float ratio = (y - dragStartMouse_) / (r.height - len); Fire(clamp(dragStartValue_ + ratio * maxValue_, 0.0f, maxValue_), false); }
            }
            else {
                float len = ThumbLength(r.width);
                if (r.width > len) { float ratio = (x - dragStartMouse_) / (r.width - len); Fire(clamp(dragStartValue_ + ratio * maxValue_, 0.0f, maxValue_), false); }
            }
        }
        void OnMouseUp(float, float) override { if (dragging_) { dragging_ = false; RequestRepaint(); } }

        void UpdateAnimation(float dt) override {
            float ht = hovering_ ? 1.0f : 0.0f;
            if (fabs(ht - hoverProgress_) > 0.001f) {
                hoverProgress_ += (ht - hoverProgress_) * min(1.0f, animSpeed_ * dt);
                if (fabs(ht - hoverProgress_) < 0.001f) hoverProgress_ = ht;
                RequestRepaint();
            }
            float st = ShrinkTarget();
            if (fabs(st - shrinkProgress_) > 0.001f) {
                shrinkProgress_ += (st - shrinkProgress_) * min(1.0f, animSpeed_ * dt);
                if (fabs(st - shrinkProgress_) < 0.001f) shrinkProgress_ = st;
                RequestRepaint();
            }
        }
        bool HasActiveAnimation() const override {
            if (hovering_ ? hoverProgress_ < 0.999f : hoverProgress_ > 0.001f) return true;
            if (fabs(ShrinkTarget() - shrinkProgress_) > 0.001f) return true;
            if (autoShrink_ && !hovering_ && shrinkProgress_ < 0.999f) {
                double now = (double)GetTickCount64();
                if (now - lastActive_ < (double)idleDelay_ * 1000.0) return true;   // 空闲倒计时期间保持活跃
            }
            return false;
        }
        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || !rt) return;
            Rect r = GetArrangedRect();
            float shrink = 1.0f - shrinkProgress_;
            float trackWidth = barWidth_ * (ShrunkWidthRatio + (1.0f - ShrunkWidthRatio) * shrink) * (1.0f + 0.25f * hoverProgress_);

            D2D1_COLOR_F trackCol = trackColor_;
            if (shrink < 0.999f) trackCol.a *= shrink;
            D2D1_COLOR_F thumbCol = thumbColor_;
            if (hoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    thumbColor_.r + (hoverThumbColor_.r - thumbColor_.r) * hoverProgress_,
                    thumbColor_.g + (hoverThumbColor_.g - thumbColor_.g) * hoverProgress_,
                    thumbColor_.b + (hoverThumbColor_.b - thumbColor_.b) * hoverProgress_,
                    thumbColor_.a + (hoverThumbColor_.a - thumbColor_.a) * hoverProgress_);
            }

            D2D1_ROUNDED_RECT trackRect;
            if (vertical_) {
                float tx = r.x + r.width - trackWidth;
                trackRect = D2D1::RoundedRect(D2D1::RectF(tx, r.y, tx + trackWidth, r.y + r.height), trackWidth * 0.5f, trackWidth * 0.5f);
            }
            else {
                float ty = r.y + r.height - trackWidth;
                trackRect = D2D1::RoundedRect(D2D1::RectF(r.x, ty, r.x + r.width, ty + trackWidth), trackWidth * 0.5f, trackWidth * 0.5f);
            }
            if (trackCol.a > 0.001f) {
                if (!trackBrush_) rt->CreateSolidColorBrush(trackCol, trackBrush_.GetAddressOf());
                else trackBrush_->SetColor(trackCol);
                if (trackBrush_) rt->FillRoundedRectangle(trackRect, trackBrush_.Get());
            }

            float pos, len; GetThumbInfo(r, pos, len);
            D2D1_ROUNDED_RECT thumbRect;
            if (vertical_) {
                float tw = max(1.5f, trackWidth - 2.0f);
                float tx = trackRect.rect.left + (trackWidth - tw) * 0.5f;
                thumbRect = D2D1::RoundedRect(D2D1::RectF(tx, r.y + pos, tx + tw, r.y + pos + len), tw * 0.5f, tw * 0.5f);
            }
            else {
                float th = max(1.5f, trackWidth - 2.0f);
                float ty = trackRect.rect.top + (trackWidth - th) * 0.5f;
                thumbRect = D2D1::RoundedRect(D2D1::RectF(r.x + pos, ty, r.x + pos + len, ty + th), th * 0.5f, th * 0.5f);
            }
            if (!thumbBrush_) rt->CreateSolidColorBrush(thumbCol, thumbBrush_.GetAddressOf());
            else thumbBrush_->SetColor(thumbCol);
            if (thumbBrush_) rt->FillRoundedRectangle(thumbRect, thumbBrush_.Get());
        }
        void ReleaseDeviceResources() override { trackBrush_.Reset(); thumbBrush_.Reset(); UIElement::ReleaseDeviceResources(); }

    private:
        void Fire(float v, bool animate) { value_ = v; if (ValueChanged) ValueChanged(v, animate); RequestRepaint(); }
        float ThumbLength(float total) const {
            if (maxValue_ <= 0.0f) return total;
            return max(minLength_, total * (total / (maxValue_ + total)));
        }
        void GetThumbInfo(const Rect& r, float& pos, float& len) const {
            float total = vertical_ ? r.height : r.width;
            len = ThumbLength(total);
            float ratio = (maxValue_ > 0.0f) ? clamp(value_ / maxValue_, 0.0f, 1.0f) : 0.0f;
            pos = (total - len) * ratio;
        }
        float ShrinkTarget() const {
            if (!autoShrink_ || hovering_) return 0.0f;
            double now = (double)GetTickCount64();
            return (now - lastActive_ >= (double)idleDelay_ * 1000.0) ? 1.0f : 0.0f;
        }

        bool vertical_ = true;
        float value_ = 0.0f, maxValue_ = 0.0f, viewportSize_ = 0.0f;
        float barWidth_ = DefaultWidth, minLength_ = DefaultMinLength, hitExtra_ = DefaultHitExtra;
        bool dragging_ = false, hovering_ = false;
        float dragStartMouse_ = 0.0f, dragStartValue_ = 0.0f;
        float hoverProgress_ = 0.0f, shrinkProgress_ = 0.0f;
        float animSpeed_ = DefaultAnimationSpeed, idleDelay_ = DefaultIdleDelay;
        bool autoShrink_ = true;
        double lastActive_ = 0.0;
        D2D1_COLOR_F trackColor_, thumbColor_, hoverThumbColor_;
        ComPtr<ID2D1SolidColorBrush> trackBrush_, thumbBrush_;
    };

    // ==================== 滚动容器（ScrollViewer） ====================
    class ScrollViewer : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Group; }
        // 默认样式（与稳定版一致）
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 12.0f;  // 滚动条悬停扩张动画速度
        inline static D2D1_COLOR_F DefaultTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultScrollBarHitExtra = 6.0f;
        inline static float DefaultScrollBarIdleDelay = 2.0f;   // 鼠标离开后多久开始缩小（秒）

        // 空闲缩小：鼠标离开 idleDelay 秒后滚动条缩成细线；悬停/滚动时恢复正常并播放悬停动画
        void SetScrollBarIdleDelay(float s) {
            scrollBarIdleDelay_ = max(0.0f, s);
            if (vScrollBar_) vScrollBar_->SetIdleDelay(scrollBarIdleDelay_);
            if (hScrollBar_) hScrollBar_->SetIdleDelay(scrollBarIdleDelay_);
            RequestRepaint();
        }
        float GetScrollBarIdleDelay() const { return scrollBarIdleDelay_; }
        void SetScrollBarAutoShrink(bool on) {
            autoShrinkScrollBar_ = on;
            if (vScrollBar_) vScrollBar_->SetAutoShrink(on);
            if (hScrollBar_) hScrollBar_->SetAutoShrink(on);
            RequestRepaint();
        }
        static void SetDefaultScrollBarIdleDelay(float s) { DefaultScrollBarIdleDelay = max(0.0f, s); }
        void MarkScrollBarActive(bool vertical) {
            if (vertical) { if (vScrollBar_) vScrollBar_->MarkActive(); }
            else { if (hScrollBar_) hScrollBar_->MarkActive(); }
        }
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;


        // ---------- ScrollViewer 构造函数 ----------
        ScrollViewer()
            : content_(nullptr),
            scrollOffsetX_(0.0f), scrollOffsetY_(0.0f),
            targetScrollOffsetX_(0.0f), targetScrollOffsetY_(0.0f),
            maxScrollX_(0.0f), maxScrollY_(0.0f),
            isDraggingVertical_(false), isDraggingHorizontal_(false),
            isTrackScrolling_(false),
            dragStartMouseX_(0), dragStartMouseY_(0),
            dragStartScrollX_(0), dragStartScrollY_(0),
            showVerticalScrollBar_(false), showHorizontalScrollBar_(false),
            allowVerticalScroll_(true), allowHorizontalScroll_(true),
            scrollBarWidth_(DefaultScrollBarWidth), scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep), animationSpeed_(DefaultAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            trackColor_(DefaultTrackColor), thumbColor_(DefaultThumbColor), hoverThumbColor_(DefaultHoverThumbColor),
            lastMouseX_(0.0f), lastMouseY_(0.0f),
            isVerticalHovered_(false), isHorizontalHovered_(false),
            verticalHoverProgress_(0.0f), horizontalHoverProgress_(0.0f),
            scrollBarHitExtra_(DefaultScrollBarHitExtra),
            lastArrangeRect_(0, 0, 0, 0) {
            width_ = 0; height_ = 0;

            // 创建垂直滚动条子元素
            vScrollBar_ = std::make_shared<ScrollBar>(true);
            vScrollBar_->SetParent(this);
            vScrollBar_->SetBarWidth(scrollBarWidth_);
            vScrollBar_->SetMinLength(scrollBarMinLength_);
            vScrollBar_->SetHitExtra(scrollBarHitExtra_);
            vScrollBar_->SetColors(thumbColor_, hoverThumbColor_, trackColor_);
            vScrollBar_->SetIdleDelay(scrollBarIdleDelay_);
            vScrollBar_->ValueChanged = [this](float v, bool animate) {
                if (animate) { ScrollTo(scrollOffsetX_, v, true); }
                else { scrollOffsetY_ = v; targetScrollOffsetY_ = v; ArrangeContent(); RequestRepaint(); }
                if (vScrollBar_) vScrollBar_->RequestRepaint();
            };

            // 创建水平滚动条子元素
            hScrollBar_ = std::make_shared<ScrollBar>(false);
            hScrollBar_->SetParent(this);
            hScrollBar_->SetBarWidth(scrollBarWidth_);
            hScrollBar_->SetMinLength(scrollBarMinLength_);
            hScrollBar_->SetHitExtra(scrollBarHitExtra_);
            hScrollBar_->SetColors(thumbColor_, hoverThumbColor_, trackColor_);
            hScrollBar_->SetIdleDelay(scrollBarIdleDelay_);
            hScrollBar_->ValueChanged = [this](float v, bool animate) {
                if (animate) { ScrollTo(v, scrollOffsetY_, true); }
                else { scrollOffsetX_ = v; targetScrollOffsetX_ = v; ArrangeContent(); RequestRepaint(); }
                if (hScrollBar_) hScrollBar_->RequestRepaint();
            };
        }

        // ---------- 公共接口 ----------
        void SetContent(std::shared_ptr<UIElement> content) {
            if (content_) content_->SetParent(nullptr);
            content_ = content;
            if (content_) content_->SetParent(this);
            InvalidateLayout();
            RequestRepaint();
        }
        std::shared_ptr<UIElement> GetContent() const { return content_; }
        void SetVerticalScrollEnabled(bool enabled) { allowVerticalScroll_ = enabled; InvalidateLayout(); RequestRepaint(); }
        void SetHorizontalScrollEnabled(bool enabled) { allowHorizontalScroll_ = enabled; InvalidateLayout(); RequestRepaint(); }
        bool IsVerticalScrollEnabled() const { return allowVerticalScroll_; }
        bool IsHorizontalScrollEnabled() const { return allowHorizontalScroll_; }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetAnimationSpeed(float speed) { animationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = max(0.1f, speed); }
        void SetScrollBarHitExtra(float extra) { scrollBarHitExtra_ = extra; }
        float GetScrollBarHitExtra() const { return scrollBarHitExtra_; }

        // ---------- 可见性策略 / 事件 / 偏移 / 实例颜色 / 内边距 ----------
        enum class ScrollBarVisibility { Auto, Always, Hidden };
        void SetVerticalScrollBarVisibility(ScrollBarVisibility v) { vVisibility_ = v; allowVerticalScroll_ = (v != ScrollBarVisibility::Hidden); InvalidateLayout(); RequestRepaint(); }
        void SetHorizontalScrollBarVisibility(ScrollBarVisibility v) { hVisibility_ = v; allowHorizontalScroll_ = (v != ScrollBarVisibility::Hidden); InvalidateLayout(); RequestRepaint(); }
        ScrollBarVisibility GetVerticalScrollBarVisibility() const { return vVisibility_; }
        ScrollBarVisibility GetHorizontalScrollBarVisibility() const { return hVisibility_; }
        float GetScrollOffsetX() const { return scrollOffsetX_; }
        float GetScrollOffsetY() const { return scrollOffsetY_; }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            trackColor_ = track.ToD2D(); thumbColor_ = thumb.ToD2D(); hoverThumbColor_ = hoverThumb.ToD2D();
            trackBrush_.Reset(); thumbBrush_.Reset(); RequestRepaint();
            if (vScrollBar_) vScrollBar_->RequestRepaint();
            if (hScrollBar_) hScrollBar_->RequestRepaint();
        }
        void SetContentMargin(const Thickness& m) { contentMargin_ = m; InvalidateLayout(); RequestRepaint(); }
        Thickness GetContentMargin() const { return contentMargin_; }
        ZSignal<float, float> ScrollChanged;   // 偏移变化 (x, y)

        static void SetDefaultScrollBarWidth(float width) { DefaultScrollBarWidth = width; }
        static void SetDefaultScrollBarMinLength(float length) { DefaultScrollBarMinLength = length; }
        static void SetDefaultScrollWheelStep(float step) { DefaultScrollWheelStep = step; }
        static void SetDefaultAnimationSpeed(float speed) { DefaultAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultColors(D2D1_COLOR_F track, D2D1_COLOR_F thumb, D2D1_COLOR_F hoverThumb) {
            DefaultTrackColor = track; DefaultThumbColor = thumb; DefaultHoverThumbColor = hoverThumb;
        }
        static void SetDefaultScrollBarHitExtra(float extra) { DefaultScrollBarHitExtra = extra; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        void ScrollTo(float offsetX, float offsetY, bool animated = true) {
            if (!allowHorizontalScroll_) offsetX = 0;
            if (!allowVerticalScroll_) offsetY = 0;
            targetScrollOffsetX_ = clamp(offsetX, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(offsetY, 0.0f, maxScrollY_);
            if (!animated) {
                scrollOffsetX_ = targetScrollOffsetX_;
                scrollOffsetY_ = targetScrollOffsetY_;
                ArrangeContent();
                RequestRepaint();
                // 显式请求滚动条重绘，确保滑块立即更新
                if (vScrollBar_) vScrollBar_->RequestRepaint();
                if (hScrollBar_) hScrollBar_->RequestRepaint();
            }
            else {
                // 即使动画，也提前触发重绘，让动画驱动
                RequestRepaint();
                if (vScrollBar_) vScrollBar_->RequestRepaint();
                if (hScrollBar_) hScrollBar_->RequestRepaint();
            }
        }
        void ScrollBy(float deltaX, float deltaY, bool animated = true) {
            ScrollTo(targetScrollOffsetX_ + deltaX, targetScrollOffsetY_ + deltaY, animated);
        }

        // ---------- UIElement 重写 ----------
        Size MeasureOverride(const Size& availableSize) override {
            if (!content_) return Size(0, 0);
            contentDesiredSize_ = content_->Measure(Size(FLT_MAX, FLT_MAX));
            Size result;
            if (width_ > 0) result.width = width_;
            else if (availableSize.width != FLT_MAX) result.width = availableSize.width;
            else result.width = max(minWidth_, 0.0f);
            if (height_ > 0) result.height = height_;
            else if (availableSize.height != FLT_MAX) result.height = availableSize.height;
            else result.height = max(minHeight_, 0.0f);
            return result;
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            lastArrangeRect_ = finalRect;
            if (!content_) return;

            UpdateScrollBarVisibility(finalRect.width, finalRect.height);
            float viewportWidth = finalRect.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = finalRect.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);

            // 扣掉内容内边距才是内容可用区，滚动范围也要把内边距算进去
            float availW = max(0.0f, viewportWidth - contentMargin_.left - contentMargin_.right);
            float availH = max(0.0f, viewportHeight - contentMargin_.top - contentMargin_.bottom);
            float contentWidth = max(contentDesiredSize_.width, availW);
            float contentHeight = max(contentDesiredSize_.height, availH);

            maxScrollX_ = max(0.0f, contentWidth + contentMargin_.left + contentMargin_.right - viewportWidth);
            maxScrollY_ = max(0.0f, contentHeight + contentMargin_.top + contentMargin_.bottom - viewportHeight);

            scrollOffsetX_ = clamp(scrollOffsetX_, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);

            ArrangeContent();

            // 把内容裁到视口（扣掉滚动条占用），避免内容（含内容内部再溢出的子控件）画到滚动条下面
            if (vScrollBar_) vScrollBar_->SetRange(scrollOffsetY_, maxScrollY_, viewportHeight);
            if (hScrollBar_) hScrollBar_->SetRange(scrollOffsetX_, maxScrollX_, viewportWidth);
            content_->SetClipRect(Rect(finalRect.x, finalRect.y, viewportWidth, viewportHeight));

            // 更新滚动条子元素布局和可见性
            if (showVerticalScrollBar_) {
                Rect vRect(finalRect.x + finalRect.width - scrollBarWidth_, finalRect.y,
                    scrollBarWidth_, viewportHeight);
                vScrollBar_->Arrange(vRect);
                vScrollBar_->SetVisibleNoInvalidate(true);
            }
            else {
                vScrollBar_->SetVisibleNoInvalidate(false);
                isVerticalHovered_ = false; // 不可见时确保无悬停
            }
            if (showHorizontalScrollBar_) {
                Rect hRect(finalRect.x, finalRect.y + finalRect.height - scrollBarWidth_,
                    viewportWidth, scrollBarWidth_);
                hScrollBar_->Arrange(hRect);
                hScrollBar_->SetVisibleNoInvalidate(true);
            }
            else {
                hScrollBar_->SetVisibleNoInvalidate(false);
                isHorizontalHovered_ = false;
            }

            RequestRepaint();
            // 显式请求滚动条重绘，确保布局改变后滚动条显示正确
            if (vScrollBar_) vScrollBar_->RequestRepaint();
            if (hScrollBar_) hScrollBar_->RequestRepaint();
        }

        // 注意：ScrollViewer 自身不绘制滚动条，滚动条是子元素，会被 Window 自动合成
        void Draw(ID2D1RenderTarget* rt) override {
            // 无需绘制任何内容，子元素（内容、滚动条）会被 Window 自动绘制
        }

        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            if (content_)
                childrenView_.push_back(content_.get());
            if (vScrollBar_)
                childrenView_.push_back(vScrollBar_.get());
            if (hScrollBar_)
                childrenView_.push_back(hScrollBar_.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            if (content_) content_->AttachWindowRecursive(w);
            if (vScrollBar_) vScrollBar_->AttachWindowRecursive(w);
            if (hScrollBar_) hScrollBar_->AttachWindowRecursive(w);
        }

        // 修改：裁剪区域返回整个控件区域，避免滚动条被裁剪
        std::optional<D2D1_RECT_F> GetClipRect() const override {
            return arrangedRect_.ToD2D();
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            lastMouseX_ = x; lastMouseY_ = y;

            // 先检测滚动条
            if (vScrollBar_ && vScrollBar_->IsVisible() && vScrollBar_->HitTest(x, y))
                return vScrollBar_.get();
            if (hScrollBar_ && hScrollBar_->IsVisible() && hScrollBar_->HitTest(x, y))
                return hScrollBar_.get();

            // 再检测内容
            if (content_) {
                UIElement* hit = content_->HitTest(x, y);
                if (hit) return hit;
            }
            return this;
        }

        void OnMouseMove(float x, float y) override {
            lastMouseX_ = x; lastMouseY_ = y;
            // 滚动条 hover 状态更新（同时也会在 ScrollBar::OnMouseEnter/Leave 中设置，这里确保一致）
            bool oldV = isVerticalHovered_, oldH = isHorizontalHovered_;
            isVerticalHovered_ = (vScrollBar_ && vScrollBar_->IsVisible() && vScrollBar_->HitTest(x, y));
            isHorizontalHovered_ = (hScrollBar_ && hScrollBar_->IsVisible() && hScrollBar_->HitTest(x, y));
            if (oldV != isVerticalHovered_ || oldH != isHorizontalHovered_)
                RequestRepaint();

            // 传递给内容
            if (content_ && !isVerticalHovered_ && !isHorizontalHovered_) {
                UIElement* hit = content_->HitTest(x, y);
                if (hit) hit->OnMouseMove(x, y);
            }
            MouseMove.Fire(x, y);
        }

        void OnMouseDown(float x, float y) override {
            lastMouseX_ = x; lastMouseY_ = y;
            MouseDown.Fire(x, y);
            if (vScrollBar_ && vScrollBar_->IsVisible() && vScrollBar_->HitTest(x, y)) {
                vScrollBar_->OnMouseDown(x, y);
                return;
            }
            if (hScrollBar_ && hScrollBar_->IsVisible() && hScrollBar_->HitTest(x, y)) {
                hScrollBar_->OnMouseDown(x, y);
                return;
            }
            if (content_) {
                UIElement* hit = content_->HitTest(x, y);
                if (hit) hit->OnMouseDown(x, y);
            }
        }

        void OnMouseUp(float x, float y) override {
            MouseUp.Fire(x, y);
            if (isDraggingVertical_ || isDraggingHorizontal_ || isTrackScrolling_) {
                isDraggingVertical_ = false;
                isDraggingHorizontal_ = false;
                isTrackScrolling_ = false;
                RequestRepaint();
                return;
            }
            if (content_) {
                UIElement* hit = content_->HitTest(x, y);
                if (hit) hit->OnMouseUp(x, y);
            }
        }

        void OnMouseLeave() override {
            isVerticalHovered_ = false;
            isHorizontalHovered_ = false;
            if (content_) content_->OnMouseLeave();
            MouseLeave.Fire();
            RequestRepaint();
        }

        bool OnMouseWheel(float deltaX, float deltaY) override {
            bool overVertical = (vScrollBar_ && vScrollBar_->IsVisible() && vScrollBar_->HitTest(lastMouseX_, lastMouseY_));
            bool overHorizontal = (hScrollBar_ && hScrollBar_->IsVisible() && hScrollBar_->HitTest(lastMouseX_, lastMouseY_));
            bool handled = false;
            if (deltaY != 0 && allowVerticalScroll_ && maxScrollY_ > 0 && !overHorizontal) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                handled = true;
            }
            if (deltaX != 0 && allowHorizontalScroll_ && maxScrollX_ > 0 && !overVertical) {
                targetScrollOffsetX_ = clamp(targetScrollOffsetX_ - deltaX * scrollWheelStep_, 0.0f, maxScrollX_);
                handled = true;
            }
            if (handled) {
                RequestRepaint();
                if (vScrollBar_) vScrollBar_->RequestRepaint();
                if (hScrollBar_) hScrollBar_->RequestRepaint();
            }
            return handled;
        }

        void UpdateAnimation(float deltaTime) override {
            if (vScrollBar_) vScrollBar_->UpdateAnimation(deltaTime);   // 滚动条自身动画（悬停/缩小）由父级转发
            if (hScrollBar_) hScrollBar_->UpdateAnimation(deltaTime);
            if (!content_) return;
            bool moved = false;
            if (fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f) {
                scrollOffsetX_ += (targetScrollOffsetX_ - scrollOffsetX_) * min(1.0f, animationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetX_ - scrollOffsetX_) <= 0.1f) scrollOffsetX_ = targetScrollOffsetX_;
                moved = true;
            }
            else scrollOffsetX_ = targetScrollOffsetX_;

            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, animationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                moved = true;
            }
            else scrollOffsetY_ = targetScrollOffsetY_;

            if (moved) {
                ArrangeContent();
                RequestRepaint(); // 关键：让滚动条子元素也重绘
                // 显式请求滚动条重绘，确保滑块同步
                if (vScrollBar_) vScrollBar_->RequestRepaint();
                if (hScrollBar_) hScrollBar_->RequestRepaint();
                lastScrollX_ = scrollOffsetX_; lastScrollY_ = scrollOffsetY_;
                ScrollChanged(scrollOffsetX_, scrollOffsetY_);
            }

            // 把最新范围推给滚动条（滚动条自身负责 hover / 空闲缩小的动画）
            if (vScrollBar_) vScrollBar_->SetRange(scrollOffsetY_, maxScrollY_,
                arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0.0f));
            if (hScrollBar_) hScrollBar_->SetRange(scrollOffsetX_, maxScrollX_,
                arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0.0f));

            // 更新子元素动画
            content_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.1f;
            bool scrollAnim = (fabs(targetScrollOffsetX_ - scrollOffsetX_) > epsilon) ||
                (fabs(targetScrollOffsetY_ - scrollOffsetY_) > epsilon);
            bool contentAnim = content_ ? content_->HasActiveAnimation() : false;
            bool barAnim = (vScrollBar_ && vScrollBar_->HasActiveAnimation()) ||
                           (hScrollBar_ && hScrollBar_->HasActiveAnimation());
            return scrollAnim || contentAnim || barAnim;
        }

        void ReleaseDeviceResources() override {
            trackBrush_.Reset(); thumbBrush_.Reset();
            if (vScrollBar_) vScrollBar_->ReleaseDeviceResources();
            if (hScrollBar_) hScrollBar_->ReleaseDeviceResources();
            if (content_) content_->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        // 内部辅助函数
        void ArrangeContent() {
            if (!content_ || lastArrangeRect_.width <= 0 || lastArrangeRect_.height <= 0) return;
            float viewportWidth = lastArrangeRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = lastArrangeRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float availW = max(0.0f, viewportWidth - contentMargin_.left - contentMargin_.right);
            float availH = max(0.0f, viewportHeight - contentMargin_.top - contentMargin_.bottom);
            float contentWidth = max(contentDesiredSize_.width, availW);
            float contentHeight = max(contentDesiredSize_.height, availH);
            Rect contentRect(lastArrangeRect_.x - Snap(scrollOffsetX_) + contentMargin_.left,
                lastArrangeRect_.y - Snap(scrollOffsetY_) + contentMargin_.top, contentWidth, contentHeight);
            content_->Arrange(contentRect);
        }

        void UpdateScrollBarVisibility(float availWidth, float availHeight) {
            showVerticalScrollBar_ = false;
            showHorizontalScrollBar_ = false;
            if (!content_) return;
            float viewportWidth = availWidth - scrollBarWidth_;
            float viewportHeight = availHeight - scrollBarWidth_;
            bool wantV = (vVisibility_ == ScrollBarVisibility::Always) ? true
                : (allowVerticalScroll_ && contentDesiredSize_.height > viewportHeight);
            bool wantH = (hVisibility_ == ScrollBarVisibility::Always) ? true
                : (allowHorizontalScroll_ && contentDesiredSize_.width > viewportWidth);
            if (wantV) { showVerticalScrollBar_ = true; viewportWidth = availWidth - scrollBarWidth_; }
            if (wantH) { showHorizontalScrollBar_ = true; viewportHeight = availHeight - scrollBarWidth_; }
            if (!wantV && allowVerticalScroll_ && contentDesiredSize_.height > viewportHeight) showVerticalScrollBar_ = true;
            if (!wantH && allowHorizontalScroll_ && contentDesiredSize_.width > viewportWidth) showHorizontalScrollBar_ = true;
        }

        // 成员变量
        std::shared_ptr<UIElement> content_;
        std::shared_ptr<ScrollBar> vScrollBar_;
        std::shared_ptr<ScrollBar> hScrollBar_;
        Size contentDesiredSize_;
        float scrollOffsetX_, scrollOffsetY_;
        float targetScrollOffsetX_, targetScrollOffsetY_;
        float maxScrollX_, maxScrollY_;
        bool isDraggingVertical_, isDraggingHorizontal_;
        bool isTrackScrolling_;
        float dragStartMouseX_, dragStartMouseY_;
        float dragStartScrollX_, dragStartScrollY_;
        bool showVerticalScrollBar_, showHorizontalScrollBar_;
        bool allowVerticalScroll_, allowHorizontalScroll_;
        float scrollBarWidth_;
        float scrollBarMinLength_;
        float scrollWheelStep_;
        float animationSpeed_;
        float hoverAnimationSpeed_;
        D2D1_COLOR_F trackColor_, thumbColor_, hoverThumbColor_;
        float lastMouseX_, lastMouseY_;
        bool isVerticalHovered_, isHorizontalHovered_;
        float verticalHoverProgress_, horizontalHoverProgress_;
        // 空闲缩小
        float scrollBarIdleDelay_ = DefaultScrollBarIdleDelay;
        bool  autoShrinkScrollBar_ = true;
        float verticalShrinkProgress_ = 0.0f, horizontalShrinkProgress_ = 0.0f;   // 0=正常, 1=缩成细线
        double verticalLastActive_ = 0.0, horizontalLastActive_ = 0.0;
        float scrollBarHitExtra_;
        ScrollBarVisibility vVisibility_ = ScrollBarVisibility::Auto;
        ScrollBarVisibility hVisibility_ = ScrollBarVisibility::Auto;
        Thickness contentMargin_;
        float lastScrollX_ = -1.0f, lastScrollY_ = -1.0f;
        Rect lastArrangeRect_;
        ComPtr<ID2D1SolidColorBrush> trackBrush_;
        ComPtr<ID2D1SolidColorBrush> thumbBrush_;

        // 友元声明，允许 ScrollBar 访问私有成员
        friend class ScrollBar;
    };

    // ---------- 进度条 ----------
    class ProgressBar : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::ProgressBar; }
        double GetAccessibleRangeValue() const override { return GetRangeValue(); }     // UIA RangeValue
        void   SetAccessibleRangeValue(double v) override { SetRangeValue((float)v); }
        double GetAccessibleRangeMin() const override { return min_; }
        double GetAccessibleRangeMax() const override { return max_; }
        inline static float DefaultWidth = 200.0f;
        inline static float DefaultHeight = 20.0f;
        inline static D2D1_COLOR_F DefaultTrackColor = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
        inline static D2D1_COLOR_F DefaultFillColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static float DefaultIndeterminateSpeed = 100.0f;
        inline static float DefaultIndeterminateBlockWidth = 40.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;

        ZSignal<float> ValueChanged;

        ProgressBar() : value_(0.0f), indeterminate_(false),
            indeterminatePos_(-40.0f),
            indeterminateSpeed_(DefaultIndeterminateSpeed),
            trackColor_(DefaultTrackColor), fillColor_(DefaultFillColor), borderColor_(DefaultBorderColor),
            indeterminateBlockWidth_(DefaultIndeterminateBlockWidth) {
            width_ = DefaultWidth; height_ = DefaultHeight;
        }

        void SetValue(float value) {
            float v = clamp(value, 0.0f, 1.0f);
            if (v == value_ && !indeterminate_) return;   // 值未变且非 indeterminate，不重复触发
            value_ = v; indeterminate_ = false; ValueChanged(GetRangeValue()); RequestRepaint();
        }
        float GetValue() const { return value_; }
        void SetRange(float min, float max) { if (max > min) { min_ = min; max_ = max; } else { min_ = min; max_ = min + 1.0f; } RequestRepaint(); }
        float GetMin() const { return min_; }
        float GetMax() const { return max_; }
        void SetRangeValue(float v) { value_ = clamp((v - min_) / (max_ - min_), 0.0f, 1.0f); indeterminate_ = false; ValueChanged(GetRangeValue()); RequestRepaint(); }
        float GetRangeValue() const { return min_ + value_ * (max_ - min_); }
        void SetShowText(bool show) { showText_ = show; RequestRepaint(); }
        bool IsShowText() const { return showText_; }
        void SetTextColor(Color c) { textColor_ = c.ToD2D(); textBrush_.Reset(); RequestRepaint(); }
        void SetIndeterminate(bool indeterminate) { indeterminate_ = indeterminate; if (indeterminate_) { value_ = 0.0f; indeterminatePos_ = -indeterminateBlockWidth_; } RequestRepaint(); }
        bool IsIndeterminate() const { return indeterminate_; }
        void SetTrackColor(Color color) { trackColor_ = color.ToD2D(); trackBrush_.Reset(); RequestRepaint(); }
        void SetFillColor(Color color) { fillColor_ = color.ToD2D(); fillBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetIndeterminateBlockWidth(float width) { indeterminateBlockWidth_ = width; RequestRepaint(); }
        void SetIndeterminateSpeed(float speed) { indeterminateSpeed_ = speed; }

        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultColors(D2D1_COLOR_F track, D2D1_COLOR_F fill, D2D1_COLOR_F border) { DefaultTrackColor = track; DefaultFillColor = fill; DefaultBorderColor = border; }
        static void SetDefaultIndeterminate(float speed, float blockWidth) { DefaultIndeterminateSpeed = speed; DefaultIndeterminateBlockWidth = blockWidth; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (indeterminate_ && arrangedRect_.width > 0) {
                if (indeterminatePos_ > arrangedRect_.width) {
                    indeterminatePos_ = -indeterminateBlockWidth_;
                }
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            if (!trackBrush_) rt->CreateSolidColorBrush(trackColor_, trackBrush_.GetAddressOf());
            else trackBrush_->SetColor(trackColor_);
            if (trackBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), height_ / 2, height_ / 2), trackBrush_.Get());

            ID2D1Factory* factory = nullptr;
            rt->GetFactory(&factory);
            if (!factory) { DrawFillContent(rt); DrawBorder(rt); return; }

            ComPtr<ID2D1RoundedRectangleGeometry> clipGeometry;
            HRESULT hr = factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(arrangedRect_.ToD2D(), height_ / 2, height_ / 2), clipGeometry.GetAddressOf());
            factory->Release();

            if (FAILED(hr) || !clipGeometry) { DrawFillContent(rt); DrawBorder(rt); return; }

            D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(D2D1::InfiniteRect(), clipGeometry.Get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);
            rt->PushLayer(&layerParams, nullptr);
            DrawFillContent(rt);
            rt->PopLayer();
            DrawBorder(rt);

            if (showText_ && !indeterminate_) {
                auto fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
                if (fmt) {
                    wchar_t buf[32];
                    int pct = (int)std::round(value_ * 100.0f);
                    swprintf(buf, 32, L"%d%%", pct);
                    if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                    else textBrush_->SetColor(textColor_);
                    float tw = 40.0f;
                    D2D1_RECT_F tr = D2D1::RectF(arrangedRect_.x + arrangedRect_.width / 2 - tw, arrangedRect_.y,
                        arrangedRect_.x + arrangedRect_.width / 2 + tw, arrangedRect_.y + arrangedRect_.height);
                    if (textBrush_) rt->DrawText(buf, (UINT32)wcslen(buf), fmt, tr, textBrush_.Get());
                }
            }
        }

        void UpdateAnimation(float deltaTime) override {
            if (indeterminate_) {
                float trackWidth = arrangedRect_.width;
                if (trackWidth <= 0) return;
                indeterminatePos_ += indeterminateSpeed_ * deltaTime;
                if (indeterminatePos_ > trackWidth) {
                    indeterminatePos_ = -indeterminateBlockWidth_;
                }
                RequestRepaint();
            }
        }

        bool HasActiveAnimation() const override {
            return indeterminate_;
        }

        void ReleaseDeviceResources() override {
            trackBrush_.Reset(); fillBrush_.Reset(); borderBrush_.Reset(); textBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        void DrawFillContent(ID2D1RenderTarget* rt) {
            if (indeterminate_) {
                float blockX = arrangedRect_.x + indeterminatePos_;
                float blockWidth = indeterminateBlockWidth_;
                float blockY = arrangedRect_.y + 1.0f;
                float blockHeight = arrangedRect_.height - 2.0f;
                float radius = blockHeight / 2.0f;
                D2D1_RECT_F blockRect = D2D1::RectF(blockX, blockY, blockX + blockWidth, blockY + blockHeight);
                if (!fillBrush_) rt->CreateSolidColorBrush(fillColor_, fillBrush_.GetAddressOf());
                else fillBrush_->SetColor(fillColor_);
                if (fillBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(blockRect, radius, radius), fillBrush_.Get());
            }
            else {
                float fillWidth = arrangedRect_.width * value_;
                if (fillWidth > 0.0f) {
                    float radius = height_ / 2.0f;
                    D2D1_RECT_F fillRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y, arrangedRect_.x + fillWidth, arrangedRect_.y + arrangedRect_.height);
                    D2D1_COLOR_F fc = IsEffectivelyEnabled() ? fillColor_ : D2D1::ColorF(0.68f, 0.68f, 0.68f, 1.0f);
                    if (!fillBrush_) rt->CreateSolidColorBrush(fc, fillBrush_.GetAddressOf());
                    else fillBrush_->SetColor(fc);
                    if (fillBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(fillRect, radius, radius), fillBrush_.Get());
                }
            }
        }
        void DrawBorder(ID2D1RenderTarget* rt) {
            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), height_ / 2, height_ / 2), borderBrush_.Get(), 1.0f);
        }

        float value_;
        bool indeterminate_;
        float min_ = 0.0f, max_ = 1.0f;
        bool showText_ = false;
        D2D1_COLOR_F textColor_ = D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f);
        float indeterminatePos_;
        float indeterminateSpeed_;
        D2D1_COLOR_F trackColor_, fillColor_, borderColor_;
        float indeterminateBlockWidth_;
        ComPtr<ID2D1SolidColorBrush> trackBrush_;
        ComPtr<ID2D1SolidColorBrush> fillBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
    };

    // ---------- 滑块 ----------
    class Slider : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Slider; }
        double GetAccessibleRangeValue() const override { return value_; }              // UIA RangeValue
        void   SetAccessibleRangeValue(double v) override { SetValue((float)v); }
        double GetAccessibleRangeMin() const override { return min_; }
        double GetAccessibleRangeMax() const override { return max_; }
        double GetAccessibleRangeStep() const override { return step_; }
        inline static float DefaultWidth = 160.0f;
        inline static float DefaultHeight = 24.0f;
        inline static float DefaultTrackHeight = 4.0f;
        inline static float DefaultThumbSize = 14.0f;
        inline static D2D1_COLOR_F DefaultTrackColor = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
        inline static D2D1_COLOR_F DefaultFillColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultThumbColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 1.0f);
        inline static D2D1_COLOR_F DefaultThumbBorderColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverThumbBorderColor = D2D1::ColorF(0.1f, 0.1f, 0.1f, 1.0f);
        inline static float DefaultAnimationSpeed = 10.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 0.0f;

        ZSignal<float> ValueChanged;
        ZSignal<float> SliderReleased;   // 拖动结束

        Slider() : min_(0.0f), max_(100.0f), value_(0.0f),
            isDragging_(false), hovered_(false), hoverProgress_(0.0f),
            trackHeight_(DefaultTrackHeight), thumbSize_(DefaultThumbSize),
            trackColor_(DefaultTrackColor), fillColor_(DefaultFillColor),
            thumbColor_(DefaultThumbColor), hoverThumbColor_(DefaultHoverThumbColor),
            thumbBorderColor_(DefaultThumbBorderColor), hoverThumbBorderColor_(DefaultHoverThumbBorderColor),
            animSpeed_(DefaultAnimationSpeed) {
            width_ = DefaultWidth; height_ = DefaultHeight;
            bleed_ = 15.0f;
        }

        void SetRange(float min, float max) { if (max > min) { min_ = min; max_ = max; } value_ = clamp(value_, min_, max_); RequestRepaint(); }
        void SetValue(float value) {
            float snapped = SnapValue(clamp(value, min_, max_));
            if (snapped == value_) return;   // 值未变不触发 ValueChanged
            value_ = snapped;
            ValueChanged(value_);
            RequestRepaint();
        }
        void SetStep(float step) { step_ = max(0.0f, step); }
        float GetStep() const { return step_; }
        void SetSnapToStep(bool snap) { snapToStep_ = snap; }
        bool IsSnapToStep() const { return snapToStep_; }
        float GetValue() const { return value_; }
        void SetTrackColor(Color color) { trackColor_ = color.ToD2D(); trackBrush_.Reset(); RequestRepaint(); }
        void SetFillColor(Color color) { fillColor_ = color.ToD2D(); fillBrush_.Reset(); RequestRepaint(); }
        void SetThumbColor(Color color) { thumbColor_ = color.ToD2D(); thumbBrush_.Reset(); RequestRepaint(); }
        void SetHoverThumbColor(Color color) { hoverThumbColor_ = color.ToD2D(); RequestRepaint(); }
        void SetThumbBorderColor(Color color) { thumbBorderColor_ = color.ToD2D(); thumbBorderBrush_.Reset(); RequestRepaint(); }
        void SetHoverThumbBorderColor(Color color) { hoverThumbBorderColor_ = color.ToD2D(); RequestRepaint(); }
        void SetThumbSize(float size) { thumbSize_ = size; RequestRepaint(); }
        void SetTrackHeight(float height) { trackHeight_ = height; RequestRepaint(); }
        void SetAnimationSpeed(float speed) { animSpeed_ = speed; }

        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultColors(D2D1_COLOR_F track, D2D1_COLOR_F fill) { DefaultTrackColor = track; DefaultFillColor = fill; }
        static void SetDefaultThumbColors(D2D1_COLOR_F normal, D2D1_COLOR_F hover) { DefaultThumbColor = normal; DefaultHoverThumbColor = hover; }
        static void SetDefaultThumbBorderColors(D2D1_COLOR_F normal, D2D1_COLOR_F hover) { DefaultThumbBorderColor = normal; DefaultHoverThumbBorderColor = hover; }
        static void SetDefaultAnimationSpeed(float speed) { DefaultAnimationSpeed = speed; }
        static void SetDefaultTrackHeight(float height) { DefaultTrackHeight = height; }
        static void SetDefaultThumbSize(float size) { DefaultThumbSize = size; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            float trackY = arrangedRect_.y + (arrangedRect_.height - trackHeight_) / 2;
            float trackWidth = arrangedRect_.width;
            bool en = IsEffectivelyEnabled();

            D2D1_COLOR_F tcol = en ? trackColor_ : D2D1::ColorF(0.90f, 0.90f, 0.90f, 1.0f);
            if (!trackBrush_) rt->CreateSolidColorBrush(tcol, trackBrush_.GetAddressOf());
            else trackBrush_->SetColor(tcol);
            if (trackBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(arrangedRect_.x, trackY, arrangedRect_.x + trackWidth, trackY + trackHeight_), trackHeight_ / 2, trackHeight_ / 2), trackBrush_.Get());

            float fillRatio = (value_ - min_) / (max_ - min_);
            float fillWidth = trackWidth * fillRatio;
            if (fillWidth > 0) {
                float radius = min(trackHeight_ / 2.0f, fillWidth / 2.0f);
                D2D1_ROUNDED_RECT fillRect = D2D1::RoundedRect(D2D1::RectF(arrangedRect_.x, trackY, arrangedRect_.x + fillWidth, trackY + trackHeight_), radius, radius);
                D2D1_COLOR_F fcol = en ? fillColor_ : D2D1::ColorF(0.74f, 0.74f, 0.74f, 1.0f);
                if (!fillBrush_) rt->CreateSolidColorBrush(fcol, fillBrush_.GetAddressOf());
                else fillBrush_->SetColor(fcol);
                if (fillBrush_) rt->FillRoundedRectangle(fillRect, fillBrush_.Get());
            }

            float thumbSize = thumbSize_ + hoverProgress_ * 2.0f;
            float thumbX = arrangedRect_.x + fillWidth - thumbSize / 2;
            float thumbY = arrangedRect_.y + (arrangedRect_.height - thumbSize) / 2;
            D2D1_ELLIPSE thumbEllipse = D2D1::Ellipse(D2D1::Point2F(thumbX + thumbSize / 2, thumbY + thumbSize / 2), thumbSize / 2, thumbSize / 2);

            D2D1_COLOR_F fillCol = D2D1::ColorF(
                thumbColor_.r + (hoverThumbColor_.r - thumbColor_.r) * hoverProgress_,
                thumbColor_.g + (hoverThumbColor_.g - thumbColor_.g) * hoverProgress_,
                thumbColor_.b + (hoverThumbColor_.b - thumbColor_.b) * hoverProgress_,
                thumbColor_.a + (hoverThumbColor_.a - thumbColor_.a) * hoverProgress_);
            if (!thumbBrush_) rt->CreateSolidColorBrush(fillCol, thumbBrush_.GetAddressOf());
            else thumbBrush_->SetColor(fillCol);
            if (thumbBrush_) rt->FillEllipse(thumbEllipse, thumbBrush_.Get());

            D2D1_COLOR_F borderCol = D2D1::ColorF(
                thumbBorderColor_.r + (hoverThumbBorderColor_.r - thumbBorderColor_.r) * hoverProgress_,
                thumbBorderColor_.g + (hoverThumbBorderColor_.g - thumbBorderColor_.g) * hoverProgress_,
                thumbBorderColor_.b + (hoverThumbBorderColor_.b - thumbBorderColor_.b) * hoverProgress_,
                thumbBorderColor_.a + (hoverThumbBorderColor_.a - thumbBorderColor_.a) * hoverProgress_);
            if (!thumbBorderBrush_) rt->CreateSolidColorBrush(borderCol, thumbBorderBrush_.GetAddressOf());
            else thumbBorderBrush_->SetColor(borderCol);
            if (thumbBorderBrush_) rt->DrawEllipse(thumbEllipse, thumbBorderBrush_.Get(), 1.0f);
        }

        void UpdateAnimation(float deltaTime) override {
            float target = (hovered_ || isDragging_) ? 1.0f : 0.0f;
            if (hoverProgress_ < target) { hoverProgress_ += animSpeed_ * deltaTime; if (hoverProgress_ > target) hoverProgress_ = target; }
            else if (hoverProgress_ > target) { hoverProgress_ -= animSpeed_ * deltaTime; if (hoverProgress_ < target) hoverProgress_ = target; }
            if (HasActiveAnimation()) RequestRepaint();
            ConvergeValue(hoverProgress_, (hovered_ || isDragging_) ? 1.0f : 0.0f, 0.001f);
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.001f;
            return (hovered_ ? (hoverProgress_ < 1.0f - epsilon) : (hoverProgress_ > epsilon));
        }

        void OnMouseEnter() override { if (!IsEffectivelyEnabled()) return; hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; if (!isDragging_) { RequestRepaint(); MouseLeave.Fire(); } }
        void OnMouseDown(float x, float y) override {
            if (!IsEffectivelyEnabled()) return;
            isDragging_ = true;
            UpdateValueFromMouse(x);
            RequestRepaint();
            MouseDown.Fire(x, y);
        }
        void OnMouseMove(float x, float y) override {
            if (isDragging_) { UpdateValueFromMouse(x); RequestRepaint(); }
            MouseMove.Fire(x, y);
        }
        void OnMouseUp(float x, float y) override {
            if (isDragging_) {
                isDragging_ = false;
                SliderReleased(value_);
                RequestRepaint();
                MouseUp.Fire(x, y);
            }
        }
        bool IsFocusable() const override { return true; }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            float d = (step_ > 0.0f ? step_ : (max_ - min_) / 100.0f);
            if (key == VK_LEFT || key == VK_DOWN) SetValue(value_ - d);
            else if (key == VK_RIGHT || key == VK_UP) SetValue(value_ + d);
            else if (key == VK_HOME) SetValue(min_);
            else if (key == VK_END) SetValue(max_);
            KeyDown.Fire(key, lParam);
        }

        void ReleaseDeviceResources() override {
            trackBrush_.Reset(); fillBrush_.Reset(); thumbBrush_.Reset(); thumbBorderBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        void UpdateValueFromMouse(float mouseX) {
            float ratio = clamp((mouseX - arrangedRect_.x) / arrangedRect_.width, 0.0f, 1.0f);
            float newValue = SnapValue(min_ + (max_ - min_) * ratio);
            if (newValue != value_) {
                value_ = newValue;
                ValueChanged(value_);
            }
        }
        float SnapValue(float v) const {
            if (!snapToStep_ || step_ <= 0.0f) return clamp(v, min_, max_);
            return clamp(min_ + std::round((v - min_) / step_) * step_, min_, max_);
        }

        float min_, max_, value_;
        float step_ = 0.0f;
        bool snapToStep_ = false;
        bool isDragging_;
        bool hovered_;
        float hoverProgress_;
        float trackHeight_;
        float thumbSize_;
        D2D1_COLOR_F trackColor_, fillColor_;
        D2D1_COLOR_F thumbColor_, hoverThumbColor_;
        D2D1_COLOR_F thumbBorderColor_, hoverThumbBorderColor_;
        float animSpeed_;
        ComPtr<ID2D1SolidColorBrush> trackBrush_;
        ComPtr<ID2D1SolidColorBrush> fillBrush_;
        ComPtr<ID2D1SolidColorBrush> thumbBrush_;
        ComPtr<ID2D1SolidColorBrush> thumbBorderBrush_;
    };

    // ==================== 勾选框 CheckBox ====================
    // 独立控件：圆角蓝底 + 白色对勾。勾选时蓝底渐显，蓝底过半后对勾从左到右绘制。
    // 同时提供静态 DrawBox()，供列表/表格/树等控件复用同一套视觉与动画。
    class CheckBox : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::CheckBox; }
        int  GetAccessibleToggleState() const override { return IsChecked() ? 1 : 0; }   // UIA Toggle
        void AccessibilityToggle() override { SetChecked(!IsChecked()); }
        enum class State { Unchecked, PartiallyChecked, Checked };

        inline static float DefaultSize = 16.0f;
        inline static float DefaultCornerRadius = 4.0f;
        inline static D2D1_COLOR_F DefaultBoxColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);      // 蓝
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.62f, 0.62f, 0.62f, 1.0f);
        inline static D2D1_COLOR_F DefaultCheckColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static float DefaultAnimationSpeed = 9.0f;

        ZSignal<bool> Toggled;            // 选中/取消（Checked 为 true）
        ZSignal<State> StateChanged;

        CheckBox(bool checked = false)
            : state_(checked ? State::Checked : State::Unchecked),
            progress_(checked ? 1.0f : 0.0f), target_(checked ? 1.0f : 0.0f),
            animSpeed_(DefaultAnimationSpeed),
            cornerRadius_(DefaultCornerRadius),
            boxColor_(DefaultBoxColor), borderColor_(DefaultBorderColor), checkColor_(DefaultCheckColor) {
            width_ = DefaultSize;
            height_ = DefaultSize;
            bleed_ = 4.0f;
        }

        void SetChecked(bool checked) { SetState(checked ? State::Checked : State::Unchecked); AccessibilityNotifyPropertyChanged(); }
        bool IsChecked() const { return state_ == State::Checked; }
        State GetState() const { return state_; }
        void SetState(State s) {
            if (state_ == s) return;
            bool wasChecked = (state_ == State::Checked);
            state_ = s;
            target_ = (s == State::Unchecked) ? 0.0f : 1.0f;
            if ((s == State::Checked) != wasChecked) Toggled(s == State::Checked);
            StateChanged(s);
            RequestRepaint();
        }
        void Toggle() {
            if (triState_) {
                if (state_ == State::Unchecked) SetState(State::PartiallyChecked);
                else if (state_ == State::PartiallyChecked) SetState(State::Checked);
                else SetState(State::Unchecked);
            }
            else {
                SetChecked(!IsChecked());
            }
        }

        void SetTriState(bool tri) { triState_ = tri; }
        bool IsTriState() const { return triState_; }
        void SetSize(float size) { width_ = size; height_ = size; InvalidateLayout(); RequestRepaint(); }
        void SetCornerRadius(float r) { cornerRadius_ = r; RequestRepaint(); }
        void SetBoxColor(Color c) { boxColor_ = c.ToD2D(); RequestRepaint(); }
        void SetBorderColor(Color c) { borderColor_ = c.ToD2D(); RequestRepaint(); }
        void SetCheckColor(Color c) { checkColor_ = c.ToD2D(); RequestRepaint(); }
        void SetAnimationSpeed(float s) { animSpeed_ = s; }
        void SetHoverBoxColor(Color c) { hoverBoxColor_ = c.ToD2D(); RequestRepaint(); }
        void SetLabel(const std::wstring& text) { label_ = text; InvalidateLayout(); RequestRepaint(); }
        std::wstring GetLabel() const { return label_; }
        void SetLabelColor(Color c) { labelColor_ = c.ToD2D(); labelBrush_.Reset(); RequestRepaint(); }

        // ---- 静态绘制：fillProgress 0..1（蓝底渐显）；对勾在 fill>0.5 后从左到右出现 ----
        static void DrawBox(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float fill,
                            State state, D2D1_COLOR_F boxColor, D2D1_COLOR_F checkColor,
                            D2D1_COLOR_F borderColor, float cornerRadius,
                            ComPtr<ID2D1SolidColorBrush>& brush, bool enabled = true) {
            float w = rect.right - rect.left, h = rect.bottom - rect.top;
            if (w <= 1.0f || h <= 1.0f) return;
            fill = clamp(fill, 0.0f, 1.0f);
            if (!enabled) {
                boxColor = D2D1::ColorF(0.80f, 0.80f, 0.80f, 1.0f);
                borderColor = D2D1::ColorF(0.82f, 0.82f, 0.82f, 1.0f);
                checkColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            }

            if (fill < 0.999f) {
                D2D1_COLOR_F bc = borderColor;
                bc.a = borderColor.a * (1.0f - fill);
                if (!brush) rt->CreateSolidColorBrush(bc, brush.GetAddressOf());
                else brush->SetColor(bc);
                if (brush) rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, cornerRadius, cornerRadius), brush.Get(), 1.4f);
            }
            if (fill > 0.001f) {
                D2D1_COLOR_F fc = boxColor;
                fc.a = boxColor.a * fill;
                if (!brush) rt->CreateSolidColorBrush(fc, brush.GetAddressOf());
                else brush->SetColor(fc);
                if (brush) rt->FillRoundedRectangle(D2D1::RoundedRect(rect, cornerRadius, cornerRadius), brush.Get());
            }

            float reveal = clamp((fill - 0.5f) * 2.0f, 0.0f, 1.0f);
            if (reveal <= 0.001f) return;
            if (!brush) rt->CreateSolidColorBrush(checkColor, brush.GetAddressOf());
            else brush->SetColor(checkColor);
            if (!brush) return;

            ID2D1StrokeStyle* ss = GetRoundStroke(rt);
            float lw = max(1.6f, h * 0.15f);
            if (state == State::Checked) {
                D2D1_POINT_2F p0 = D2D1::Point2F(rect.left + w * 0.26f, rect.top + h * 0.52f);
                D2D1_POINT_2F p1 = D2D1::Point2F(rect.left + w * 0.43f, rect.top + h * 0.72f);
                D2D1_POINT_2F p2 = D2D1::Point2F(rect.left + w * 0.76f, rect.top + h * 0.30f);
                DrawPartialPolyline(rt, p0, p1, p2, lw, reveal, brush.Get(), ss);
            }
            else if (state == State::PartiallyChecked) {
                float y = (rect.top + rect.bottom) / 2.0f;
                D2D1_POINT_2F a = D2D1::Point2F(rect.left + w * 0.26f, y);
                D2D1_POINT_2F b = D2D1::Point2F(rect.left + w * 0.74f, y);
                D2D1_POINT_2F m = D2D1::Point2F(a.x + (b.x - a.x) * reveal, y);
                rt->DrawLine(a, m, brush.Get(), lw, ss);
            }
        }

        Size MeasureOverride(const Size&) override {
            float box = (width_ > 0.0f ? width_ : DefaultSize);
            if (label_.empty()) return Size(box, box);
            auto fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            ComPtr<IDWriteTextLayout> layout;
            if (fmt) FontManager::Instance().GetFactory()->CreateTextLayout(label_.c_str(), (UINT32)label_.length(), fmt, 10000.0f, 100.0f, &layout);
            DWRITE_TEXT_METRICS tm{};
            if (layout) layout->GetMetrics(&tm);
            return Size(box + 6.0f + tm.width, max(box, tm.height));
        }
        void ArrangeOverride(const Rect& finalRect) override {
            float box = (width_ > 0.0f && height_ > 0.0f) ? (width_ < height_ ? width_ : height_) : DefaultSize;
            float w = label_.empty() ? box : finalRect.width;
            float h = max(box, finalRect.height);
            UIElement::ArrangeOverride(Rect(finalRect.x, finalRect.y, w, h));
            float by = finalRect.y + (h - box) / 2.0f;
            boxRect_ = D2D1::RectF(finalRect.x, by, finalRect.x + box, by + box);
        }
        void Draw(ID2D1RenderTarget* rt) override {
            bool en = IsEffectivelyEnabled();
            if (en && hoverProgress_ > 0.001f) {
                D2D1_COLOR_F hc = hoverBoxColor_;
                hc.a = hoverBoxColor_.a * hoverProgress_;
                if (!hoverBrush_) rt->CreateSolidColorBrush(hc, hoverBrush_.GetAddressOf());
                else hoverBrush_->SetColor(hc);
                if (hoverBrush_) {
                    D2D1_RECT_F hr = D2D1::RectF(boxRect_.left - 3.0f, boxRect_.top - 3.0f,
                        boxRect_.right + 3.0f, boxRect_.bottom + 3.0f);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(hr, cornerRadius_ + 2.0f, cornerRadius_ + 2.0f), hoverBrush_.Get());
                }
            }
            DrawBox(rt, boxRect_, progress_, state_, boxColor_, checkColor_, borderColor_, cornerRadius_, brush_, en);
            if (!label_.empty()) {
                auto fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
                D2D1_COLOR_F lc = en ? labelColor_ : D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
                if (!labelBrush_) rt->CreateSolidColorBrush(lc, labelBrush_.GetAddressOf());
                else labelBrush_->SetColor(lc);
                D2D1_RECT_F tr = D2D1::RectF(boxRect_.right + 6.0f, arrangedRect_.y, arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + arrangedRect_.height);
                if (fmt && labelBrush_) rt->DrawText(label_.c_str(), (UINT32)label_.length(), fmt, tr, labelBrush_.Get());
            }
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            if (key == VK_SPACE || key == VK_RETURN) Toggle();
            KeyDown.Fire(key, lParam);
        }
        bool IsFocusable() const override { return true; }
        void OnMouseDown(float x, float y) override { Toggle(); MouseDown.Fire(x, y); }
        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); MouseLeave.Fire(); }

        void UpdateAnimation(float dt) override {
            if (fabs(target_ - progress_) > 0.001f) {
                progress_ += (target_ - progress_) * min(1.0f, animSpeed_ * dt);
                if (fabs(target_ - progress_) <= 0.001f) progress_ = target_;
                RequestRepaint();
            }
            float ht = hovered_ ? 1.0f : 0.0f;
            if (fabs(ht - hoverProgress_) > 0.001f) {
                hoverProgress_ += (ht - hoverProgress_) * min(1.0f, hoverSpeed_ * dt);
                if (fabs(ht - hoverProgress_) <= 0.001f) hoverProgress_ = ht;
                RequestRepaint();
            }
        }
        bool HasActiveAnimation() const override {
            return fabs(target_ - progress_) > 0.001f ||
                (hovered_ ? hoverProgress_ < 0.999f : hoverProgress_ > 0.001f);
        }
        void ReleaseDeviceResources() override { brush_.Reset(); labelBrush_.Reset(); hoverBrush_.Reset(); UIElement::ReleaseDeviceResources(); }

    private:
        static ID2D1StrokeStyle* GetRoundStroke(ID2D1RenderTarget* rt) {
            static ComPtr<ID2D1StrokeStyle> ss;
            if (!ss && rt) {
                ComPtr<ID2D1Factory> factory;
                rt->GetFactory(&factory);
                if (factory)
                    factory->CreateStrokeStyle(
                        D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                            D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &ss);
            }
            return ss.Get();
        }
        static void DrawPartialPolyline(ID2D1RenderTarget* rt, D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_POINT_2F p2,
                                        float width, float reveal, ID2D1Brush* brush, ID2D1StrokeStyle* ss) {
            auto len = [](D2D1_POINT_2F a, D2D1_POINT_2F b) { float dx = b.x - a.x, dy = b.y - a.y; return sqrtf(dx * dx + dy * dy); };
            auto lerp = [](D2D1_POINT_2F a, D2D1_POINT_2F b, float t) { return D2D1::Point2F(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); };
            float l1 = len(p0, p1), l2 = len(p1, p2), total = l1 + l2;
            float d = clamp(reveal, 0.0f, 1.0f) * total;
            if (d <= 0.01f) return;
            if (d <= l1) {
                rt->DrawLine(p0, lerp(p0, p1, d / l1), brush, width, ss);
            }
            else {
                rt->DrawLine(p0, p1, brush, width, ss);
                float d2 = d - l1;
                rt->DrawLine(p1, lerp(p1, p2, clamp(d2 / l2, 0.0f, 1.0f)), brush, width, ss);
            }
        }

        State state_;
        float progress_, target_;
        float animSpeed_;
        float cornerRadius_;
        D2D1_COLOR_F boxColor_, borderColor_, checkColor_;
        bool hovered_ = false;
        bool triState_ = false;
        ComPtr<ID2D1SolidColorBrush> brush_;
        D2D1_RECT_F boxRect_ = D2D1::RectF(0, 0, 0, 0);
        std::wstring label_;
        D2D1_COLOR_F labelColor_ = D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f);
        D2D1_COLOR_F hoverBoxColor_ = D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.15f);
        float hoverProgress_ = 0.0f;
        float hoverSpeed_ = 10.0f;
        ComPtr<ID2D1SolidColorBrush> hoverBrush_;
        ComPtr<ID2D1SolidColorBrush> labelBrush_;
    };

    class RadioGroup;   // 前置声明（RadioButton 内部回调它）

    // ============================================================================
    // RadioButton：圆形单选按钮（圆点 + 文字）。自身只负责勾选自己的状态；
    //   **组内互斥由 RadioGroup 负责**（不靠遍历父容器推断）。
    // ============================================================================
    class RadioButton : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::RadioButton; }
        inline static float DefaultSize = 16.0f;
        inline static float DefaultAnimationSpeed = 9.0f;
        inline static float DefaultHoverSpeed = 10.0f;
        inline static D2D1_COLOR_F DefaultAccentColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.62f, 0.62f, 0.62f, 1.0f);
        inline static D2D1_COLOR_F DefaultLabelColor = D2D1::ColorF(0.15f, 0.15f, 0.15f, 1.0f);

        ZSignal<bool> CheckedChanged;   // 自身勾选态变化
        ZSignal<> Clicked;              // 被点击（无论是否已是选中）

        RadioButton(bool checked = false)
            : checked_(checked), progress_(checked ? 1.0f : 0.0f), target_(checked ? 1.0f : 0.0f),
            animSpeed_(DefaultAnimationSpeed), hoverSpeed_(DefaultHoverSpeed),
            accentColor_(DefaultAccentColor), borderColor_(DefaultBorderColor), labelColor_(DefaultLabelColor) {
            size_ = DefaultSize; bleed_ = 4.0f;
            hoverColor_ = D2D1::ColorF(accentColor_.r, accentColor_.g, accentColor_.b, 0.15f);
            highlightColor_ = D2D1::ColorF(0.839f, 0.910f, 0.984f, 1.0f);   // 选中整行浅蓝 #D6E8FB
        }
        explicit RadioButton(const std::wstring& text, bool checked = false) : RadioButton(checked) { label_ = text; }

        void SetChecked(bool checked) {
            if (checked_ == checked) return;
            checked_ = checked;
            target_ = checked ? 1.0f : 0.0f;
            CheckedChanged(checked_);
            RequestRepaint();
        }
        bool IsChecked() const { return checked_; }

        void SetLabel(const std::wstring& text) { label_ = text; InvalidateLayout(); RequestRepaint(); }
        std::wstring GetLabel() const { return label_; }
        void SetLabelColor(Color c) { labelColor_ = c.ToD2D(); labelBrush_.Reset(); RequestRepaint(); }
        void SetAccentColor(Color c) { accentColor_ = c.ToD2D(); hoverColor_ = D2D1::ColorF(accentColor_.r, accentColor_.g, accentColor_.b, 0.15f); RequestRepaint(); }
        void SetBorderColor(Color c) { borderColor_ = c.ToD2D(); RequestRepaint(); }
        void SetSize(float size) { size_ = max(8.0f, size); InvalidateLayout(); RequestRepaint(); }
        void SetAnimationSpeed(float s) { animSpeed_ = s; }
        void SetHoverSpeed(float s) { hoverSpeed_ = s; }
        // 选中整行高亮（浅蓝底 + 可选左侧竖条）；由 RadioGroup 自动开启
        void SetRowHighlight(bool on) { rowHighlight_ = on; RequestRepaint(); }
        bool GetRowHighlight() const { return rowHighlight_; }
        void SetRowHighlightColor(Color c) { highlightColor_ = c.ToD2D(); RequestRepaint(); }
        void SetAccentBar(bool on) { accentBar_ = on; RequestRepaint(); }

        // 由 RadioGroup 注入（组内互斥）
        void SetGroup(RadioGroup* g) { group_ = g; }
        RadioGroup* GetGroup() const { return group_; }
        void NotifyGroup();              // 定义在 RadioGroup 之后
        void NotifyGroupKey(WPARAM key); // 定义在 RadioGroup 之后

        bool IsFocusable() const override { return true; }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsEffectivelyEnabled()) return;
            if (key == VK_SPACE || key == VK_RETURN) { if (!checked_) Activate(); }
            else if (key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT) {
                if (group_) NotifyGroupKey(key);   // 组整体键盘导航（定义在 RadioGroup 之后）
            }
            KeyDown.Fire(key, lParam);
        }
        void OnMouseDown(float x, float y) override {
            if (!IsEffectivelyEnabled()) return;
            if (!checked_) Activate();
            Clicked.Fire();
            MouseDown.Fire(x, y);
        }
        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); MouseLeave.Fire(); }

        Size MeasureOverride(const Size&) override {
            float circle = size_;
            if (label_.empty()) return Size(circle, circle);
            IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            ComPtr<IDWriteTextLayout> layout;
            if (fmt) FontManager::Instance().GetFactory()->CreateTextLayout(label_.c_str(), (UINT32)label_.length(), fmt, 10000.0f, 100.0f, &layout);
            DWRITE_TEXT_METRICS tm{};
            if (layout) layout->GetMetrics(&tm);
            return Size(circle + 6.0f + tm.width, max(circle, tm.height));
        }
        void ArrangeOverride(const Rect& finalRect) override {
            float circle = size_;
            float w = label_.empty() ? circle : finalRect.width;
            float h = max(circle, finalRect.height);
            UIElement::ArrangeOverride(Rect(finalRect.x, finalRect.y, w, h));
            float by = finalRect.y + (h - circle) * 0.5f;
            circleRect_ = D2D1::RectF(finalRect.x, by, finalRect.x + circle, by + circle);
        }
        void Draw(ID2D1RenderTarget* rt) override {
            bool en = IsEffectivelyEnabled();
            // 选中整行高亮（浅蓝底 + 可选左侧竖条），在最底层
            if (rowHighlight_ && progress_ > 0.01f) {
                float a = clamp(progress_, 0.0f, 1.0f);
                if (!highlightBrush_) {
                    D2D1_COLOR_F hc = highlightColor_; hc.a *= a;
                    rt->CreateSolidColorBrush(hc, highlightBrush_.GetAddressOf());
                }
                else { D2D1_COLOR_F hc = highlightColor_; hc.a *= a; highlightBrush_->SetColor(hc); }
                if (highlightBrush_) {
                    D2D1_RECT_F rr = D2D1::RectF(arrangedRect_.x - 3.0f, arrangedRect_.y - 1.0f,
                        arrangedRect_.x + arrangedRect_.width + 3.0f, arrangedRect_.y + arrangedRect_.height + 1.0f);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(rr, 6.0f, 6.0f), highlightBrush_.Get());
                }
                if (accentBar_) {
                    D2D1_COLOR_F ac = accentColor_; ac.a *= a;
                    if (highlightBrush_) highlightBrush_->SetColor(ac);
                    if (highlightBrush_) {
                        D2D1_RECT_F bar = D2D1::RectF(arrangedRect_.x - 3.0f, arrangedRect_.y + 2.0f,
                            arrangedRect_.x - 0.5f, arrangedRect_.y + arrangedRect_.height - 2.0f);
                        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.5f, 1.5f), highlightBrush_.Get());
                    }
                }
            }
            float cx = (circleRect_.left + circleRect_.right) * 0.5f;
            float cy = (circleRect_.top + circleRect_.bottom) * 0.5f;
            float r = (circleRect_.right - circleRect_.left) * 0.5f;
            if (r <= 0.5f) return;

            // hover 光晕
            if (en && hoverProgress_ > 0.001f) {
                D2D1_COLOR_F hc = hoverColor_; hc.a *= hoverProgress_;
                if (!hoverBrush_) rt->CreateSolidColorBrush(hc, hoverBrush_.GetAddressOf());
                else hoverBrush_->SetColor(hc);
                if (hoverBrush_) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r + 3.0f, r + 3.0f), hoverBrush_.Get());
            }

            D2D1_COLOR_F accent = en ? accentColor_ : D2D1::ColorF(0.80f, 0.80f, 0.80f, 1.0f);
            D2D1_COLOR_F border = en ? borderColor_ : D2D1::ColorF(0.82f, 0.82f, 0.82f, 1.0f);
            float p = clamp(progress_, 0.0f, 1.0f);

            // 外圈：未选=灰描边 → 选中=主题色（颜色/粗细随进度）
            D2D1_COLOR_F ring = D2D1::ColorF(
                border.r + (accent.r - border.r) * p,
                border.g + (accent.g - border.g) * p,
                border.b + (accent.b - border.b) * p, 1.0f);
            if (!brush_) rt->CreateSolidColorBrush(ring, brush_.GetAddressOf());
            else brush_->SetColor(ring);
            if (brush_) rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r - 0.7f, r - 0.7f), brush_.Get(), 1.4f + p * 0.6f);

            // 内点
            float dr = (r - 4.0f) * p;
            if (dr > 0.4f) {
                if (brush_) brush_->SetColor(accent); else rt->CreateSolidColorBrush(accent, brush_.GetAddressOf());
                if (brush_) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dr, dr), brush_.Get());
            }

            // 文字
            if (!label_.empty()) {
                IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
                D2D1_COLOR_F lc = en ? labelColor_ : D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
                if (!labelBrush_) rt->CreateSolidColorBrush(lc, labelBrush_.GetAddressOf());
                else labelBrush_->SetColor(lc);
                D2D1_RECT_F tr = D2D1::RectF(circleRect_.right + 6.0f, arrangedRect_.y,
                    arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + arrangedRect_.height);
                if (fmt && labelBrush_) rt->DrawText(label_.c_str(), (UINT32)label_.length(), fmt, tr, labelBrush_.Get());
            }
        }
        void UpdateAnimation(float dt) override {
            if (fabs(target_ - progress_) > 0.001f) {
                progress_ += (target_ - progress_) * min(1.0f, animSpeed_ * dt);
                if (fabs(target_ - progress_) <= 0.001f) progress_ = target_;
                RequestRepaint();
            }
            float ht = hovered_ ? 1.0f : 0.0f;
            if (fabs(ht - hoverProgress_) > 0.001f) {
                hoverProgress_ += (ht - hoverProgress_) * min(1.0f, hoverSpeed_ * dt);
                if (fabs(ht - hoverProgress_) <= 0.001f) hoverProgress_ = ht;
                RequestRepaint();
            }
        }
        bool HasActiveAnimation() const override {
            return fabs(target_ - progress_) > 0.001f ||
                (hovered_ ? hoverProgress_ < 0.999f : hoverProgress_ > 0.001f);
        }
        void ReleaseDeviceResources() override { brush_.Reset(); labelBrush_.Reset(); hoverBrush_.Reset(); highlightBrush_.Reset(); UIElement::ReleaseDeviceResources(); }

    private:
        void Activate() {
            if (group_) NotifyGroup();   // 交给组做互斥（会 SetChecked(true)）
            else SetChecked(true);
        }

        bool checked_ = false;
        float progress_ = 0.0f, target_ = 0.0f;
        float animSpeed_, hoverSpeed_;
        bool hovered_ = false;
        float hoverProgress_ = 0.0f;
        std::wstring label_;
        float size_ = DefaultSize;
        bool rowHighlight_ = false;
        bool accentBar_ = true;
        D2D1_COLOR_F accentColor_, borderColor_, labelColor_, hoverColor_, highlightColor_;
        D2D1_RECT_F circleRect_ = D2D1::RectF(0, 0, 0, 0);
        RadioGroup* group_ = nullptr;
        ComPtr<ID2D1SolidColorBrush> brush_, labelBrush_, hoverBrush_, highlightBrush_;
    };

    // ============================================================================
    // RadioGroup：单选组的"组"容器。纵向/横向堆叠；默认组内互斥；
    //   可 SetMutualExclusion(false) 并用 SelectionChanging/SelectionChanged 处理特殊互斥关系。
    //   组整体键盘导航：纵向 ↑/↓，横向 ←/→。
    // ============================================================================
    class RadioGroup : public LayoutHost {
    public:
        enum class Orientation { Vertical, Horizontal };
        inline static float DefaultItemSpacing = 6.0f;
        inline static bool  DefaultMutualExclusion = true;

        ZSignal<int> SelectionChanged;                                  // 选中项索引变化
        std::function<bool(int newIndex, int oldIndex)> SelectionChanging;   // 返回 false 取消本次选择

        RadioGroup() { BuildStack(Orientation::Vertical); }

        void SetOrientation(Orientation o) { if (o == orientation_) return; RebuildStack(o); }
        Orientation GetOrientation() const { return orientation_; }
        void SetItemSpacing(float s) { spacing_ = max(0.0f, s); RebuildStack(orientation_); }

        void SetMutualExclusion(bool on) { mutualExclusion_ = on; }
        bool GetMutualExclusion() const { return mutualExclusion_; }

        int AddItem(const std::wstring& text, bool selected = false) {
            return AddButton(std::make_shared<RadioButton>(text), selected);
        }
        int AddButton(std::shared_ptr<RadioButton> rb, bool selected = false) {
            if (!rb) return -1;
            rb->SetGroup(this);
            rb->SetRowHighlight(true);   // 选中整行浅蓝
            buttons_.push_back(rb);
            StackAddChild(rb);
            int idx = (int)buttons_.size() - 1;
            if (selected || selectedIndex_ < 0) SelectOnly(rb.get());
            return idx;
        }
        int GetItemCount() const { return (int)buttons_.size(); }
        std::shared_ptr<RadioButton> GetButton(int index) const {
            return (index >= 0 && index < (int)buttons_.size()) ? buttons_[index] : nullptr;
        }
        void SetSelectedIndex(int index) { if (index >= 0 && index < (int)buttons_.size()) SelectOnly(buttons_[index].get()); }
        int GetSelectedIndex() const { return selectedIndex_; }

        // 组整体键盘导航（由 RadioButton 的 OnKeyDown 转发过来）；自动跳过禁用项
        void NavigateKey(WPARAM key) {
            int n = (int)buttons_.size();
            if (n == 0) return;
            int dir = 0;
            if (orientation_ == Orientation::Vertical) { if (key == VK_UP) dir = -1; else if (key == VK_DOWN) dir = +1; }
            else { if (key == VK_LEFT) dir = -1; else if (key == VK_RIGHT) dir = +1; }
            if (dir == 0) return;
            int cur = (selectedIndex_ < 0) ? 0 : selectedIndex_;
            for (int step = 1; step <= n; ++step) {
                int idx = (((cur + dir * step) % n) + n) % n;
                if (buttons_[idx]->IsEffectivelyEnabled()) { SelectOnly(buttons_[idx].get()); return; }
            }
        }

        // 只选中 b（RadioButton 勾选时回调这里）
        void SelectOnly(RadioButton* b) {
            int idx = -1;
            for (int i = 0; i < (int)buttons_.size(); ++i) if (buttons_[i].get() == b) { idx = i; break; }
            if (idx < 0) return;
            if (SelectionChanging && !SelectionChanging(idx, selectedIndex_)) return;   // 用户否决
            if (mutualExclusion_) for (int i = 0; i < (int)buttons_.size(); ++i) buttons_[i]->SetChecked(i == idx);
            else buttons_[idx]->SetChecked(true);
            bool changed = (idx != selectedIndex_);
            selectedIndex_ = idx;
            if (changed) SelectionChanged(idx);
        }

        // ---- UIElement / 容器 ----
        Size MeasureOverride(const Size& availableSize) override {
            return layout_ ? layout_->Measure(availableSize) : Size(0, 0);
        }
        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (layout_) layout_->Arrange(finalRect);
        }
        void Draw(ID2D1RenderTarget*) override {}   // 自身无视觉内容，子元素由合成通道递归绘制
        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            if (layout_) { if (UIElement* h = layout_->HitTest(x, y)) return h; }
            return this;
        }
        void UpdateAnimation(float dt) override { if (layout_) layout_->UpdateAnimation(dt); }
        bool HasActiveAnimation() const override { return layout_ ? layout_->HasActiveAnimation() : false; }
        bool UseCache() const override { return false; }

    private:
        void BuildStack(Orientation o) {
            orientation_ = o;
            std::shared_ptr<UIElement> stack;
            if (o == Orientation::Vertical) { auto c = std::make_shared<ColumnBox>(); c->SetSpacing(spacing_); stack = c; }
            else { auto r = std::make_shared<RowBox>(); r->SetSpacing(spacing_); stack = r; }
            layout_ = stack;   // LayoutHost::layout_
            layout_->SetParent(this);
            MarkChildrenDirty();
            InvalidateLayout();
            RequestRepaint();
        }
        void RebuildStack(Orientation o) {
            BuildStack(o);
            for (auto& b : buttons_) StackAddChild(b);
        }
        void StackAddChild(std::shared_ptr<UIElement> child) {
            if (!layout_ || !child) return;
            if (auto c = std::dynamic_pointer_cast<ColumnBox>(layout_)) c->AddChild(child);
            else if (auto r = std::dynamic_pointer_cast<RowBox>(layout_)) r->AddChild(child);
        }

        Orientation orientation_ = Orientation::Vertical;
        float spacing_ = DefaultItemSpacing;
        bool mutualExclusion_ = DefaultMutualExclusion;
        std::vector<std::shared_ptr<RadioButton>> buttons_;
        int selectedIndex_ = -1;
    };

    inline void RadioButton::NotifyGroup() { if (group_) group_->SelectOnly(this); }
    inline void RadioButton::NotifyGroupKey(WPARAM key) { if (group_) group_->NavigateKey(key); }

    // ============================================================================
    // TabView / TabControl：顶部横向紧凑页签 + 内容区
    //   - 页签条自绘：文字 + 选中下划线指示器（带过渡）+ hover + 可选关闭 ×
    //   - 键盘：←/→/Home/End 切换，Delete 关闭当前；点击页签即聚焦
    //   - 选中页的内容作为子元素，由合成通道递归绘制（同 PageHost 的托管模式）
    //   - 页签溢出：裁剪 + 滚轮横向滚动
    // ============================================================================
    class TabView : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Tab; }
        struct Tab {
            std::shared_ptr<Label> label; // 页签标题就是一个 Label（自带图标、整体对齐等）
            std::shared_ptr<Page> page;   // 内容用 Page 承载（复用 PageHost 的过渡动画）
            bool closable = false;
            float x = 0.0f;               // 动画中的当前左坐标（关闭/增删时平滑移动）
            bool  xInit = false;
        };

        // ---- 默认样式（可实例覆盖）----
        inline static float DefaultTabHeight = 34.0f;
        inline static float DefaultTabMinWidth = 64.0f;
        inline static float DefaultTabTextPad = 14.0f;
        inline static float DefaultIndicatorHeight = 2.5f;
        inline static float DefaultCloseBox = 16.0f;
        inline static float DefaultScrollWheelStep = 80.0f;    // 滚轮一格滚动的 DIP（deltaY 是"格数"）
        inline static float DefaultScrollBarThickness = 6.0f;  // 页签条底部横向滚动条厚度
        inline static Color DefaultSelectedTabColor = Color::FromArgb(255, 0xD6, 0xE8, 0xFB);   // 选中页签浅蓝 #D6E8FB
        inline static Color DefaultBackgroundColor = Color(1, 1, 1, 1);
        inline static Color DefaultContentColor = Color(1, 1, 1, 1);
        inline static Color DefaultTextColor = Color(0.36f, 0.36f, 0.36f, 1);
        inline static Color DefaultSelectedTextColor = Color(0.13f, 0.13f, 0.13f, 1);
        inline static Color DefaultHoverColor = Color(0, 0, 0, 0.05f);
        inline static Color DefaultIndicatorColor = Color(0.0f, 0.47f, 0.84f, 1);
        inline static Color DefaultBorderColor = Color(0, 0, 0, 0.10f);

        ZSignal<int> SelectionChanged;    // 选中页签索引
        ZSignal<int> TabCloseRequested;   // 用户点了某页签的 ×（由应用决定是否 RemoveTab）

        TabView() {
            width_ = 0; height_ = 0;
            minWidth_ = 120.0f; minHeight_ = 80.0f;
            fillWidth_ = true;
            bleed_ = 8.0f;   // 出血：给整体边框/圆角留余量，避免被裁
            // 内容区用内建 PageHost 托管（复用它的过渡动画）
            contentHost_ = std::make_shared<PageHost>();
            contentHost_->SetParent(this);
            // 页签条底部横向滚动条：复用库自身的 ScrollBar 子类
            hBar_ = std::make_shared<ScrollBar>(false);
            hBar_->SetParent(this);
            hBar_->SetVisibleNoInvalidate(false);
            hBar_->SetBarWidth(scrollBarThickness_);
            hBar_->SetMinLength(24.0f);
            hBar_->SetHitExtra(4.0f);
            hBar_->SetColors(scrollThumbColor_.ToD2D(), scrollThumbHoverColor_.ToD2D(), D2D1::ColorF(0, 0, 0, 0.0f));
            hBar_->ValueChanged = [this](float v, bool animate) {
                stripScrollTarget_ = v;
                if (!animate) stripScroll_ = v;
                stripDirty_ = true; InvalidateLayout(); RequestRepaint();
            };
        }

        // 过渡动画参数（内容切换）
        void SetTransitionDirection(PageHost::TransitionDirection dir) { if (contentHost_) contentHost_->SetTransitionDirection(dir); }
        void SetTransitionEasing(PageHost::TransitionEasing e) { if (contentHost_) contentHost_->SetTransitionEasing(e); }
        void SetAnimationDuration(float seconds) { if (contentHost_) contentHost_->SetAnimationDuration(seconds); }

        // 把任意内容包成一个 Page（若本身就是 Page 直接用）
        static std::shared_ptr<Page> MakePage(std::shared_ptr<UIElement> content) {
            if (auto p = std::dynamic_pointer_cast<Page>(content)) return p;
            auto page = std::make_shared<Page>();
            page->SetPadding(0.0f);
            if (content) page->SetLayout(content);
            return page;
        }

        // ---------- 页签增删改 ----------
        std::unique_lock<std::recursive_mutex> RenderGuard() {
            Window* w = GetWindow();
            return w ? w->LockRender() : std::unique_lock<std::recursive_mutex>();
        }
        // 页签标题就是一个 Label（所以可直接 SetIcon 等）；Title 重载是便捷写法（内部包一个 Label）
        static std::shared_ptr<Label> MakeTabLabel(const std::wstring& title) {
            auto l = std::make_shared<Label>(title);
            l->SetAlignment(Label::HAlign::Center, Label::VAlign::Center);
            return l;
        }
        int AddTab(const std::wstring& title, std::shared_ptr<UIElement> content = nullptr, bool closable = false) {
            return AddTab(MakeTabLabel(title), content, closable);
        }
        int AddTab(std::shared_ptr<Label> label, std::shared_ptr<UIElement> content = nullptr, bool closable = false) {
            auto _rg = RenderGuard();
            if (!label) label = MakeTabLabel(L"");
            label->SetParent(this);
            Tab t; t.label = label; t.page = MakePage(content); t.closable = closable;
            tabs_.push_back(std::move(t));
            int idx = (int)tabs_.size() - 1;
            if (contentHost_) contentHost_->AddPage(tabs_[idx].page);
            int oldSel = selectedIndex_;
            if (selectedIndex_ < 0) selectedIndex_ = 0;
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
            if (selectedIndex_ != oldSel) SelectionChanged(selectedIndex_);   // 首个页签自动选中也要通知
            return idx;
        }
        void InsertTab(int index, const std::wstring& title, std::shared_ptr<UIElement> content = nullptr, bool closable = false) {
            InsertTab(index, MakeTabLabel(title), content, closable);
        }
        void InsertTab(int index, std::shared_ptr<Label> label, std::shared_ptr<UIElement> content = nullptr, bool closable = false) {
            auto _rg = RenderGuard();
            if (index < 0) index = 0;
            if (index > (int)tabs_.size()) index = (int)tabs_.size();
            if (!label) label = MakeTabLabel(L"");
            label->SetParent(this);
            Tab t; t.label = label; t.page = MakePage(content); t.closable = closable;
            tabs_.insert(tabs_.begin() + index, std::move(t));
            int oldSel = selectedIndex_;
            if (selectedIndex_ < 0) selectedIndex_ = 0;
            else if (index <= selectedIndex_) selectedIndex_++;
            RebuildHost();
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
            if (selectedIndex_ != oldSel) SelectionChanged(selectedIndex_);
        }
        void RebuildHost() {
            if (!contentHost_) return;
            contentHost_->ClearPages();
            for (auto& t : tabs_) contentHost_->AddPage(t.page);
            if (selectedIndex_ >= 0) contentHost_->SetCurrentIndexInstant(selectedIndex_);
        }
        void RemoveTab(int index) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size()) return;
            if (contentHost_) contentHost_->RemovePage(index);
            tabs_.erase(tabs_.begin() + index);
            int oldSel = selectedIndex_;
            if (tabs_.empty()) selectedIndex_ = -1;
            else if (selectedIndex_ > index) selectedIndex_--;
            else if (selectedIndex_ == index) selectedIndex_ = min(index, (int)tabs_.size() - 1);
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
            // 选定页被删或索引变化 → 通知（索引值相同但指向了不同页签也要通知）
            if (selectedIndex_ != oldSel || oldSel == index) SelectionChanged(selectedIndex_);
        }
        void ClearTabs() {
            auto _rg = RenderGuard();
            int oldSel = selectedIndex_;
            tabs_.clear(); selectedIndex_ = -1;
            if (contentHost_) contentHost_->ClearPages();
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
            if (selectedIndex_ != oldSel) SelectionChanged(selectedIndex_);
        }
        int GetTabCount() const { return (int)tabs_.size(); }
        void SetTabTitle(int index, const std::wstring& title) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size()) return;
            if (!tabs_[index].label) { tabs_[index].label = MakeTabLabel(title); tabs_[index].label->SetParent(this); }
            else tabs_[index].label->SetText(title);
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
        }
        std::wstring GetTabTitle(int index) const {
            if (index < 0 || index >= (int)tabs_.size() || !tabs_[index].label) return std::wstring();
            return tabs_[index].label->GetText();
        }
        // 直接替换/取回页签的 Label（可设图标、颜色、子控件等）
        void SetTabLabel(int index, std::shared_ptr<Label> label) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size()) return;
            if (!label) label = MakeTabLabel(L"");
            label->SetParent(this);
            tabs_[index].label = label;
            stripDirty_ = true; InvalidateLayout(); RequestRepaint();
        }
        std::shared_ptr<Label> GetTabLabel(int index) const {
            return (index >= 0 && index < (int)tabs_.size()) ? tabs_[index].label : nullptr;
        }
        void SetTabContent(int index, std::shared_ptr<UIElement> content) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size()) return;
            tabs_[index].page = MakePage(content);
            RebuildHost();
            InvalidateLayout(); RequestRepaint();
        }
        std::shared_ptr<UIElement> GetTabContent(int index) const {
            return (index >= 0 && index < (int)tabs_.size()) ? tabs_[index].page : nullptr;
        }
        void SetTabClosable(int index, bool closable) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size()) return;
            tabs_[index].closable = closable; stripDirty_ = true; InvalidateLayout(); RequestRepaint();
        }

        // ---------- 选中 ----------
        void SetSelectedIndex(int index) {
            auto _rg = RenderGuard();
            if (index < 0 || index >= (int)tabs_.size() || index == selectedIndex_) return;
            int oldIndex = selectedIndex_;
            selectedIndex_ = index;
            stripDirty_ = true;
            if (contentHost_) {
                if (autoTransition_)
                    contentHost_->SetTransitionDirection(index > oldIndex ? PageHost::TransitionDirection::Left
                                                                         : PageHost::TransitionDirection::Right);
                contentHost_->NavigateTo(index);   // 复用 PageHost 的过渡动画
            }
            InvalidateLayout();
            RequestRepaint();
            SelectionChanged(index);
        }
        int GetSelectedIndex() const { return selectedIndex_; }
        std::shared_ptr<UIElement> GetSelectedContent() const {
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)tabs_.size()) return tabs_[selectedIndex_].page;
            return nullptr;
        }

        // ---------- 样式 ----------
        void SetTabHeight(float h) { tabHeight_ = max(20.0f, h); stripDirty_ = true; InvalidateLayout(); RequestRepaint(); }
        void SetTabMinWidth(float w) { tabMinWidth_ = max(20.0f, w); stripDirty_ = true; InvalidateLayout(); RequestRepaint(); }
        void SetTabPadding(float p) { tabTextPad_ = max(4.0f, p); stripDirty_ = true; InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorHeight(float h) { indicatorHeight_ = max(0.0f, h); RequestRepaint(); }
        void SetCornerRadius(float r) { cornerRadius_ = max(0.0f, r); RequestRepaint(); }
        float GetCornerRadius() const { return cornerRadius_; }
        void SetBorder(bool visible, Color color = Color(0, 0, 0, 0.10f), float width = 1.0f) {
            borderVisible_ = visible;
            if (visible) { borderColor_ = color; borderWidth_ = max(0.0f, width); }
            RequestRepaint();
        }
        // 切换页签时按"目标在左/右"自动选择过渡方向（更自然）；设 false 则用 SetTransitionDirection 指定
        void SetAutoTransitionDirection(bool on) { autoTransition_ = on; }
        void SetIndicatorColor(Color c) { indicatorColor_ = c; RequestRepaint(); }
        void SetBackgroundColor(Color c) { backgroundColor_ = c; RequestRepaint(); }
        void SetContentColor(Color c) { contentColor_ = c; RequestRepaint(); }
        void SetTextColor(Color normal, Color selected) { textColor_ = normal; selectedTextColor_ = selected; RequestRepaint(); }
        void SetAnimationSpeeds(float indicator, float hover) { indicatorSpeed_ = max(0.1f, indicator); hoverSpeed_ = max(0.1f, hover); }
        void SetSelectedTabColor(Color c) { selectedTabColor_ = c; RequestRepaint(); }
        void SetScrollWheelStep(float px) { scrollWheelStep_ = max(1.0f, px); }
        void SetScrollBarThickness(float px) { scrollBarThickness_ = max(0.0f, px); RequestRepaint(); }
        void SetShowScrollBar(bool on) { showScrollBar_ = on; RequestRepaint(); }
        void SetTabMoveSpeed(float s) { tabMoveSpeed_ = max(0.1f, s); }

        // ---------- UIElement 接口 ----------
        bool IsFocusable() const override { return true; }
        bool UseCache() const override { return false; }
        std::optional<D2D1_RECT_F> GetClipRect() const override {
            // 裁剪矩形会同时裁到"本元素自身的绘制"（描边/圆角）→ 按出血外扩，避免边框被裁
            float b = bleed_;
            return D2D1::RectF(arrangedRect_.x - b, arrangedRect_.y - b,
                arrangedRect_.x + arrangedRect_.width + b, arrangedRect_.y + arrangedRect_.height + b);
        }

        Size MeasureOverride(const Size& availableSize) override {
            float w = width_ > 0 ? width_ : (availableSize.width != FLT_MAX ? availableSize.width : max(minWidth_, 0.0f));
            float h = height_ > 0 ? height_ : (availableSize.height != FLT_MAX ? availableSize.height : max(minHeight_, 0.0f));
            if (contentHost_) contentHost_->Measure(Size(max(0.0f, w), max(0.0f, h - tabHeight_)));
            return Size(w, h);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            LayoutStrip();   // 先算页签条（含 overflow_ 与滚动条位置）
            // 标签条与内容之间的间距 = 滚动条「完全收缩」时的粗细（视觉上正好容下那条细线）
            float band = overflow_ ? max(1.5f, scrollBarThickness_ * ScrollBar::ShrunkWidthRatio) : 0.0f;
            contentRect_ = Rect(finalRect.x, finalRect.y + tabHeight_ + band, finalRect.width,
                max(0.0f, finalRect.height - tabHeight_ - band));
            if (contentHost_) contentHost_->Arrange(contentRect_);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || !rt) return;
            if (!(std::isfinite(arrangedRect_.x) && std::isfinite(arrangedRect_.y) &&
                  std::isfinite(arrangedRect_.width) && std::isfinite(arrangedRect_.height))) return;   // NaN/Inf 守卫

            const float cr = cornerRadius_;
            // 内容区背景（圆角）
            if (!contentBrush_) rt->CreateSolidColorBrush(contentColor_.ToD2D(), contentBrush_.GetAddressOf());
            else contentBrush_->SetColor(contentColor_.ToD2D());
            if (contentBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(contentRect_.ToD2D(), cr, cr), contentBrush_.Get());

            // 页签条背景（顶部圆角）
            D2D1_RECT_F strip = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + tabHeight_);
            if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_.ToD2D(), bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(backgroundColor_.ToD2D());
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(strip, cr, cr), bgBrush_.Get());

            rt->PushAxisAlignedClip(strip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            IDWriteTextFormat* fmt = GetFontFormat();
            for (int i = 0; i < (int)tabs_.size(); ++i) {
                D2D1_RECT_F r = TabRect(i);
                bool sel = (i == selectedIndex_);

                // 选中页签：浅蓝底
                if (sel) {
                    float fr = min(6.0f, tabHeight_ * 0.4f);
                    if (!selBrush_) rt->CreateSolidColorBrush(selectedTabColor_.ToD2D(), selBrush_.GetAddressOf());
                    else selBrush_->SetColor(selectedTabColor_.ToD2D());
                    if (selBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(r, fr, fr), selBrush_.Get());
                }

                // hover 底（圆角；选中项也响应悬停）
                if (i == hoverIndex_ && hoverProgress_ > 0.01f) {
                    float fr = min(6.0f, tabHeight_ * 0.4f);
                    D2D1_COLOR_F hc = hoverColor_.ToD2D(); hc.a *= hoverProgress_;
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hc, hoverBrush_.GetAddressOf());
                    else hoverBrush_->SetColor(hc);
                    if (hoverBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(r, fr, fr), hoverBrush_.Get());
                }

                // 标题：直接摆一个 Label（可自带图标、整体对齐），选中/未选中切换文字色
                if (tabs_[i].label) {
                    float innerW = (r.right - r.left) - tabTextPad_ * 2.0f;
                    if (tabs_[i].closable) innerW -= (closeBox_ + 6.0f);
                    if (innerW > 0.0f) {
                        Color want = sel ? selectedTextColor_ : textColor_;
                        D2D1_COLOR_F wc = want.ToD2D(), hc = tabs_[i].label->GetTextColor().ToD2D();
                        if (wc.r != hc.r || wc.g != hc.g || wc.b != hc.b || wc.a != hc.a)
                            tabs_[i].label->SetTextColor(want);
                        tabs_[i].label->Arrange(Rect((float)r.left + tabTextPad_, (float)r.top, innerW, tabHeight_));
                        tabs_[i].label->Draw(rt);
                    }
                }

                // 关闭 ×（带背景：常态浅色，悬停背景变深色、图标转白）
                if (tabs_[i].closable) {
                    bool hot = (i == closeHoverIndex_);
                    D2D1_RECT_F cb = CloseBoxRect(i);
                    D2D1_COLOR_F bg = (hot ? closeHoverColor_ : closeBgColor_).ToD2D();
                    if (!closeBtnBrush_) rt->CreateSolidColorBrush(bg, closeBtnBrush_.GetAddressOf());
                    else closeBtnBrush_->SetColor(bg);
                    if (closeBtnBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(cb, 3.0f, 3.0f), closeBtnBrush_.Get());
                    D2D1_COLOR_F gc = (hot ? closeHoverGlyphColor_ : textColor_).ToD2D();
                    if (!closeBrush_) rt->CreateSolidColorBrush(gc, closeBrush_.GetAddressOf());
                    else closeBrush_->SetColor(gc);
                    float cx = (cb.left + cb.right) * 0.5f, cy = (cb.top + cb.bottom) * 0.5f;
                    float e = closeBox_ * 0.26f;
                    if (closeBrush_) {
                        rt->DrawLine(D2D1::Point2F(cx - e, cy - e), D2D1::Point2F(cx + e, cy + e), closeBrush_.Get(), 1.3f);
                        rt->DrawLine(D2D1::Point2F(cx + e, cy - e), D2D1::Point2F(cx - e, cy + e), closeBrush_.Get(), 1.3f);
                    }
                }
            }

            // 溢出滚动按钮（仅在溢出时）
            if (overflow_) {
                auto drawBtn = [&](const D2D1_RECT_F& r, bool left, bool hot) {
                    D2D1_COLOR_F hc = hoverColor_.ToD2D(); hc.a = hot ? 0.16f : 0.06f;
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hc, hoverBrush_.GetAddressOf());
                    else hoverBrush_->SetColor(hc);
                    D2D1_RECT_F rr = D2D1::RectF(r.left + 2.0f, r.top + 3.0f, r.right - 2.0f, r.bottom - 3.0f);
                    float rad = min(rr.right - rr.left, rr.bottom - rr.top) * 0.4f;
                    if (hoverBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(rr, rad, rad), hoverBrush_.Get());
                    D2D1_COLOR_F lc = selectedTextColor_.ToD2D();
                    if (!textBrush_) rt->CreateSolidColorBrush(lc, textBrush_.GetAddressOf());
                    else textBrush_->SetColor(lc);
                    float cx = (r.left + r.right) * 0.5f, cy = (r.top + r.bottom) * 0.5f, e = 4.0f;
                    if (textBrush_) {
                        if (left) {
                            rt->DrawLine(D2D1::Point2F(cx + e * 0.5f, cy - e), D2D1::Point2F(cx - e * 0.5f, cy), textBrush_.Get(), 1.6f);
                            rt->DrawLine(D2D1::Point2F(cx - e * 0.5f, cy), D2D1::Point2F(cx + e * 0.5f, cy + e), textBrush_.Get(), 1.6f);
                        }
                        else {
                            rt->DrawLine(D2D1::Point2F(cx - e * 0.5f, cy - e), D2D1::Point2F(cx + e * 0.5f, cy), textBrush_.Get(), 1.6f);
                            rt->DrawLine(D2D1::Point2F(cx + e * 0.5f, cy), D2D1::Point2F(cx - e * 0.5f, cy + e), textBrush_.Get(), 1.6f);
                        }
                    }
                };
                if (showLeftBtn_) drawBtn(overflowLeftRect_, true, hoverOverflow_ == -1);
                if (showRightBtn_) drawBtn(overflowRightRect_, false, hoverOverflow_ == 1);
            }

            // 页签条底部横向滚动条由 hBar_（ScrollBar 子类）自绘，这里不再手绘

            // 底部边框线
            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_.ToD2D(), borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_.ToD2D());
            if (borderBrush_)
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x, arrangedRect_.y + tabHeight_ - 0.5f),
                    D2D1::Point2F(arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + tabHeight_ - 0.5f), borderBrush_.Get(), 1.0f);

            // 选中指示器（贴底、圆角）
            if (indicatorW_ > 0.5f && indicatorHeight_ > 0.0f) {
                if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_.ToD2D(), indicatorBrush_.GetAddressOf());
                else indicatorBrush_->SetColor(indicatorColor_.ToD2D());
                D2D1_RECT_F ir = D2D1::RectF(indicatorX_, arrangedRect_.y + tabHeight_ - indicatorHeight_,
                    indicatorX_ + indicatorW_, arrangedRect_.y + tabHeight_);
                if (indicatorBrush_)
                    rt->FillRoundedRectangle(D2D1::RoundedRect(ir, indicatorHeight_ * 0.5f, indicatorHeight_ * 0.5f), indicatorBrush_.Get());
            }
            rt->PopAxisAlignedClip();

            // 整体边框（可关；圆角跟 cornerRadius_）
            if (borderVisible_ && borderWidth_ > 0.0f) {
                if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_.ToD2D(), borderBrush_.GetAddressOf());
                else borderBrush_->SetColor(borderColor_.ToD2D());
                if (borderBrush_) {
                    float hw = borderWidth_ * 0.5f;   // 内缩半个线宽 → 描边完整落在控件内，不被裁
                    D2D1_RECT_F br = D2D1::RectF(arrangedRect_.x + hw, arrangedRect_.y + hw,
                        arrangedRect_.x + arrangedRect_.width - hw, arrangedRect_.y + arrangedRect_.height - hw);
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(br, cr, cr), borderBrush_.Get(), borderWidth_);
                }
            }
        }

        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            if (contentHost_) childrenView_.push_back(contentHost_.get());
            if (hBar_) childrenView_.push_back(hBar_.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            if (contentHost_) contentHost_->AttachWindowRecursive(w);
            if (hBar_) hBar_->AttachWindowRecursive(w);
            for (auto& t : tabs_) if (t.label) t.label->AttachWindowRecursive(w);
        }

        void ReleaseDeviceResources() override {
            for (auto& t : tabs_) if (t.label) t.label->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            if (hBar_ && hBar_->IsVisible()) { if (UIElement* h = hBar_->HitTest(x, y)) return h; }   // 底部滚动条优先（否则点不到）
            if (contentRect_.Contains(x, y) && contentHost_) {
                if (UIElement* h = contentHost_->HitTest(x, y)) return h;
            }
            return this;
        }

        // ---------- 输入 ----------
        void OnMouseEnter() override { pointerInStrip_ = true; }

        void OnMouseMove(float x, float y) override {
            pointerInStrip_ = true;
            int ov = 0;
            if (overflow_) {
                if (showLeftBtn_ && x >= overflowLeftRect_.left && x < overflowLeftRect_.right && y >= overflowLeftRect_.top && y < overflowLeftRect_.bottom) ov = -1;
                else if (showRightBtn_ && x >= overflowRightRect_.left && x < overflowRightRect_.right && y >= overflowRightRect_.top && y < overflowRightRect_.bottom) ov = +1;
            }
            int idx = (ov != 0) ? -1 : TabIndexAt(x, y);
            int closeIdx = -1;
            if (idx >= 0 && tabs_[idx].closable && PointInCloseBox(idx, x, y)) closeIdx = idx;
            if (idx != hoverIndex_ || closeIdx != closeHoverIndex_ || ov != hoverOverflow_) {
                hoverIndex_ = idx; closeHoverIndex_ = closeIdx; hoverOverflow_ = ov;
                RequestRepaint();
            }
        }
        void OnMouseLeave() override {
            pointerInStrip_ = false;
            if (hoverIndex_ != -1 || closeHoverIndex_ != -1 || hoverOverflow_ != 0 || hoverScrollThumb_) {
                hoverIndex_ = -1; closeHoverIndex_ = -1; hoverOverflow_ = 0; hoverScrollThumb_ = false; RequestRepaint();
            }
        }
        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;
            // 底部横向滚动条由 hBar_ 自己处理（它是子元素，会先命中）
            if (showLeftBtn_ && x >= overflowLeftRect_.left && x < overflowLeftRect_.right && y >= overflowLeftRect_.top && y < overflowLeftRect_.bottom) { ScrollStripBy(-arrangedRect_.width * 0.6f); return; }
            if (showRightBtn_ && x >= overflowRightRect_.left && x < overflowRightRect_.right && y >= overflowRightRect_.top && y < overflowRightRect_.bottom) { ScrollStripBy(+arrangedRect_.width * 0.6f); return; }
            int idx = TabIndexAt(x, y);
            if (idx < 0) return;
            if (tabs_[idx].closable && PointInCloseBox(idx, x, y)) { TabCloseRequested(idx); return; }
            SetSelectedIndex(idx);
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            (void)deltaX;
            // 只在"页签条"上滚动页签；在内容区滚动应交给外层滚动容器。
            // 注意 deltaY 是"格数"(±1)，要乘每格像素数，否则一格格挪动几乎不动。
            if (!overflow_ || !pointerInStrip_) return false;
            float before = stripScrollTarget_;
            ScrollStripBy(-deltaY * scrollWheelStep_);
            return stripScrollTarget_ != before;   // ScrollStripBy 改的是"目标值"，要和它比才判断得出"是否消费"
        }
        void OnKeyDown(WPARAM key, LPARAM) override {
            int n = (int)tabs_.size();
            if (n == 0) return;
            switch (key) {
            case VK_LEFT:   SetSelectedIndex((selectedIndex_ - 1 + n) % n); break;
            case VK_RIGHT:  SetSelectedIndex((selectedIndex_ + 1) % n); break;
            case VK_HOME:   SetSelectedIndex(0); break;
            case VK_END:    SetSelectedIndex(n - 1); break;
            case VK_DELETE: if (selectedIndex_ >= 0 && tabs_[selectedIndex_].closable) TabCloseRequested(selectedIndex_); break;
            default: break;
            }
        }

        // ---------- 动画 ----------
        void UpdateAnimation(float deltaTime) override {
            // 页签条滚动 / 页签移动中：指示器直接贴住目标（避免滞后、停下后对不齐）
            bool moving = fabs(stripScroll_ - stripScrollTarget_) > 0.5f;
            for (int i = 0; !moving && i < (int)tabs_.size() && i < (int)tabTargetX_.size(); ++i)
                if (fabs(tabs_[i].x - tabTargetX_[i]) > 0.5f) moving = true;
            if (moving) {
                if (indicatorX_ != targetIndicatorX_ || indicatorW_ != targetIndicatorW_) {
                    indicatorX_ = targetIndicatorX_; indicatorW_ = targetIndicatorW_;
                    RequestRepaint();
                }
            }
            else if (indicatorX_ != targetIndicatorX_ || indicatorW_ != targetIndicatorW_) {
                float t = clamp(indicatorSpeed_ * deltaTime, 0.0f, 1.0f);
                indicatorX_ += (targetIndicatorX_ - indicatorX_) * t;
                indicatorW_ += (targetIndicatorW_ - indicatorW_) * t;
                if (fabs(indicatorX_ - targetIndicatorX_) < 0.5f) indicatorX_ = targetIndicatorX_;
                if (fabs(indicatorW_ - targetIndicatorW_) < 0.5f) indicatorW_ = targetIndicatorW_;
                RequestRepaint();
            }
            float ht = (hoverIndex_ >= 0) ? 1.0f : 0.0f;
            if (hoverProgress_ != ht) {
                float s = clamp(hoverSpeed_ * deltaTime, 0.0f, 1.0f);
                hoverProgress_ += (ht - hoverProgress_) * s;
                if (fabs(hoverProgress_ - ht) < 0.01f) hoverProgress_ = ht;
                RequestRepaint();
            }
            // 页签条平滑滚动（点滚动条 / 按钮 / 滚轮）
            {
                float mx = max(0.0f, stripTotalWidth_ - arrangedRect_.width);
                stripScrollTarget_ = clamp(stripScrollTarget_, 0.0f, mx);
                if (fabs(stripScroll_ - stripScrollTarget_) > 0.5f) {
                    stripScroll_ += (stripScrollTarget_ - stripScroll_) * min(1.0f, scrollAnimSpeed_ * deltaTime);
                    if (fabs(stripScroll_ - stripScrollTarget_) < 0.5f) stripScroll_ = stripScrollTarget_;
                    stripDirty_ = true; InvalidateLayout(); RequestRepaint();
                }
            }
            // 页签平滑移动（关闭/增删时后续页签滑过来）
            for (int i = 0; i < (int)tabs_.size() && i < (int)tabTargetX_.size(); ++i) {
                float tx = tabTargetX_[i];
                if (tabs_[i].x != tx) {
                    tabs_[i].x += (tx - tabs_[i].x) * min(1.0f, tabMoveSpeed_ * deltaTime);
                    if (fabs(tabs_[i].x - tx) < 0.5f) tabs_[i].x = tx;
                    RequestRepaint();
                }
            }
            if (hBar_) hBar_->UpdateAnimation(deltaTime);
            if (contentHost_) contentHost_->UpdateAnimation(deltaTime);
        }
        bool HasActiveAnimation() const override {
            if (fabs(indicatorX_ - targetIndicatorX_) > 0.1f || fabs(indicatorW_ - targetIndicatorW_) > 0.1f) return true;
            if (hoverProgress_ > 0.001f && hoverProgress_ < 0.999f) return true;
            if (fabs(stripScroll_ - stripScrollTarget_) > 0.5f) return true;
            for (int i = 0; i < (int)tabs_.size() && i < (int)tabTargetX_.size(); ++i)
                if (fabs(tabs_[i].x - tabTargetX_[i]) > 0.5f) return true;
            if (contentHost_) return contentHost_->HasActiveAnimation();
            return false;
        }

    private:
        float TabTextWidth(int i) const {
            if (i < 0 || i >= (int)tabs_.size() || !tabs_[i].label) return 0.0f;
            return tabs_[i].label->Measure(Size(FLT_MAX, 10000.0f)).width;
        }
        float TabWidth(int i) const {
            float w = TabTextWidth(i) + tabTextPad_ * 2.0f;
            if (i >= 0 && i < (int)tabs_.size() && tabs_[i].closable) w += closeBox_ + 6.0f;
            return max(w, tabMinWidth_);
        }
        void LayoutStrip() {
            if (arrangedRect_.width <= 0.0f || arrangedRect_.height <= 0.0f) { tabWidths_.clear(); tabTargetX_.clear(); return; }
            int n = (int)tabs_.size();
            tabWidths_.resize(n);
            tabTargetX_.resize(n);
            float x = arrangedRect_.x - stripScroll_;
            for (int i = 0; i < n; ++i) {
                float w = TabWidth(i);
                tabWidths_[i] = w;
                tabTargetX_[i] = x;
                if (!tabs_[i].xInit) { tabs_[i].x = x; tabs_[i].xInit = true; }
                x += w;
            }
            stripTotalWidth_ = x - (arrangedRect_.x - stripScroll_);
            if (stopAtCap_) stripScroll_ = clamp(stripScroll_, 0.0f, max(0.0f, stripTotalWidth_ - arrangedRect_.width));

            if (selectedIndex_ >= 0 && selectedIndex_ < n) {
                targetIndicatorX_ = tabs_[selectedIndex_].x;
                targetIndicatorW_ = tabWidths_[selectedIndex_];
            }
            else { targetIndicatorX_ = 0.0f; targetIndicatorW_ = 0.0f; }
            if (indicatorInit_) { indicatorX_ = targetIndicatorX_; indicatorW_ = targetIndicatorW_; indicatorInit_ = false; }

            // 溢出 + 只在"还能往该方向滚"时才显示对应按钮
            overflow_ = (stripTotalWidth_ > arrangedRect_.width + 0.5f);
            float maxScroll = max(0.0f, stripTotalWidth_ - arrangedRect_.width);
            showLeftBtn_ = overflow_ && stripScroll_ > 0.5f;
            showRightBtn_ = overflow_ && stripScroll_ < maxScroll - 0.5f;
            overflowLeftRect_ = D2D1::RectF(arrangedRect_.x, arrangedRect_.y, arrangedRect_.x + overflowBtnW_, arrangedRect_.y + tabHeight_);
            overflowRightRect_ = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - overflowBtnW_, arrangedRect_.y,
                arrangedRect_.x + arrangedRect_.width, arrangedRect_.y + tabHeight_);
            UpdateScrollBar(maxScroll);
        }
        void UpdateScrollBar(float maxScroll) {
            if (!hBar_) return;
            float th = max(2.0f, scrollBarThickness_);
            float trackX = arrangedRect_.x + (showLeftBtn_ ? overflowBtnW_ : 0.0f);
            float trackW = arrangedRect_.width - (showLeftBtn_ ? overflowBtnW_ : 0.0f) - (showRightBtn_ ? overflowBtnW_ : 0.0f);
            bool show = showScrollBar_ && overflow_ && maxScroll > 0.5f && trackW > 8.0f;
            hBar_->SetVisibleNoInvalidate(show);
            if (!show) return;
            hBar_->SetBarWidth(th);
            hBar_->SetRange(stripScroll_, maxScroll, arrangedRect_.width);
            // 收缩态细线底对齐落在 band 内；展开时向上盖过标签条下缘（不挤占内容）
            float band = max(1.5f, th * ScrollBar::ShrunkWidthRatio);
            hBar_->Arrange(Rect(trackX, arrangedRect_.y + tabHeight_ + band - th, trackW, th));
        }
        void ScrollStripBy(float d) {
            float maxScroll = max(0.0f, stripTotalWidth_ - arrangedRect_.width);
            float before = stripScrollTarget_;
            stripScrollTarget_ = clamp(stripScrollTarget_ + d, 0.0f, maxScroll);
            if (stripScrollTarget_ != before) { stripDirty_ = true; InvalidateLayout(); RequestRepaint(); }
        }
        // 由动画后的 x 现算矩形（关闭/增删时页签平滑移动）
        D2D1_RECT_F TabRect(int i) const {
            float xx = (i >= 0 && i < (int)tabs_.size()) ? tabs_[i].x : 0.0f;
            float w = (i >= 0 && i < (int)tabWidths_.size()) ? tabWidths_[i] : 0.0f;
            return D2D1::RectF(xx, arrangedRect_.y, xx + w, arrangedRect_.y + tabHeight_);
        }
        int TabIndexAt(float x, float y) const {
            if (y < arrangedRect_.y || y >= arrangedRect_.y + tabHeight_) return -1;
            for (int i = 0; i < (int)tabs_.size(); ++i) {
                D2D1_RECT_F r = TabRect(i);
                if (x >= r.left && x < r.right) return i;
            }
            return -1;
        }
        D2D1_RECT_F CloseBoxRect(int i) const {
            D2D1_RECT_F tr = TabRect(i);
            float right = tr.right - tabTextPad_;
            float cy = (tr.top + tr.bottom) * 0.5f;
            return D2D1::RectF(right - closeBox_, cy - closeBox_ * 0.5f, right, cy + closeBox_ * 0.5f);
        }
        bool PointInCloseBox(int i, float x, float y) const {
            if (i < 0 || i >= (int)tabs_.size()) return false;
            D2D1_RECT_F cb = CloseBoxRect(i);
            return x >= cb.left && x < cb.right && y >= cb.top && y < cb.bottom;
        }

        std::vector<Tab> tabs_;
        std::shared_ptr<PageHost> contentHost_;   // 内容托管（复用 PageHost 过渡动画）
        std::shared_ptr<ScrollBar> hBar_;         // 底部横向滚动条（复用 ScrollBar 子类）
        int selectedIndex_ = -1;
        Rect contentRect_{};
        std::vector<float> tabWidths_;
        std::vector<float> tabTargetX_;
        float stripTotalWidth_ = 0.0f, stripScroll_ = 0.0f, stripScrollTarget_ = 0.0f;
        bool stopAtCap_ = true;
        float scrollAnimSpeed_ = 16.0f;

        // 样式值
        float tabHeight_ = DefaultTabHeight;
        float tabMinWidth_ = DefaultTabMinWidth;
        float tabTextPad_ = DefaultTabTextPad;
        float indicatorHeight_ = DefaultIndicatorHeight;
        float closeBox_ = DefaultCloseBox;
        Color backgroundColor_ = DefaultBackgroundColor;
        Color contentColor_ = DefaultContentColor;
        Color textColor_ = DefaultTextColor;
        Color selectedTextColor_ = DefaultSelectedTextColor;
        Color hoverColor_ = DefaultHoverColor;
        Color indicatorColor_ = DefaultIndicatorColor;
        Color borderColor_ = DefaultBorderColor;
        Color selectedTabColor_ = DefaultSelectedTabColor;   // 选中页签浅蓝底
        float tabMoveSpeed_ = 12.0f;                          // 页签移动动画速度
        // 关闭按钮配色：常态浅底，悬停变深底 + 图标转白
        Color closeBgColor_ = Color(0, 0, 0, 0.06f);
        Color closeHoverColor_ = Color(0, 0, 0, 0.30f);
        Color closeHoverGlyphColor_ = Color(1, 1, 1, 1);
        float cornerRadius_ = 8.0f;      // 整体圆角
        bool  borderVisible_ = true;     // 整体边框（可 SetBorder(false) 取消）
        float borderWidth_ = 1.0f;
        bool  autoTransition_ = true;    // 切换方向自动跟索引左右

        // 交互 / 动画
        int hoverIndex_ = -1, closeHoverIndex_ = -1;
        bool stripDirty_ = true;
        float hoverProgress_ = 0.0f, hoverSpeed_ = 14.0f;
        float indicatorX_ = 0.0f, indicatorW_ = 0.0f;
        float targetIndicatorX_ = 0.0f, targetIndicatorW_ = 0.0f;
        float indicatorSpeed_ = 16.0f;
        bool indicatorInit_ = true;
        // 溢出滚动按钮
        float overflowBtnW_ = 22.0f;
        bool overflow_ = false;
        bool showLeftBtn_ = false, showRightBtn_ = false;
        int hoverOverflow_ = 0;   // 0=无, -1=左, +1=右
        D2D1_RECT_F overflowLeftRect_ = D2D1::RectF(0, 0, 0, 0);
        D2D1_RECT_F overflowRightRect_ = D2D1::RectF(0, 0, 0, 0);
        // 页签条底部横向滚动条
        float scrollBarThickness_ = DefaultScrollBarThickness;
        float scrollWheelStep_ = DefaultScrollWheelStep;
        bool  showScrollBar_ = true;
        Color scrollThumbColor_ = Color(0, 0, 0, 0.35f);
        Color scrollThumbHoverColor_ = Color(0, 0, 0, 0.55f);
        D2D1_RECT_F scrollTrackRect_ = D2D1::RectF(0, 0, 0, 0);
        D2D1_RECT_F scrollThumbRect_ = D2D1::RectF(0, 0, 0, 0);
        bool  scrollDragging_ = false, hoverScrollThumb_ = false;
        float scrollDragGrab_ = 0.0f;
        bool  pointerInStrip_ = false;   // 鼠标是否在页签条上（决定滚轮是否归 tab）

        ComPtr<ID2D1SolidColorBrush> bgBrush_, contentBrush_, textBrush_, hoverBrush_, indicatorBrush_, borderBrush_, closeBrush_, closeBtnBrush_, selBrush_, scroll2Brush_;
    };

    // ============================================================================
    // ProgressRing：环形进度（确定值弧 / 不确定值旋转）
    // ============================================================================
    class ProgressRing : public UIElement {
    public:
        inline static float DefaultSize = 40.0f;
        inline static float DefaultThickness = 4.0f;
        inline static float DefaultAnimationSpeed = 1.0f;
        inline static Color DefaultColor = Color(0.0f, 0.47f, 0.84f, 1.0f);
        inline static Color DefaultTrackColor = Color(0, 0, 0, 0.10f);

        ProgressRing() { size_ = DefaultSize; thickness_ = DefaultThickness; color_ = DefaultColor; trackColor_ = DefaultTrackColor; }
        explicit ProgressRing(float size) : ProgressRing() { size_ = size; }

        void SetValue(float v) { v = clamp(v, 0.0f, 1.0f); if (v != value_) { value_ = v; RequestRepaint(); } }
        float GetValue() const { return value_; }
        void SetIndeterminate(bool on) { if (on != indeterminate_) { indeterminate_ = on; RequestRepaint(); } }
        bool IsIndeterminate() const { return indeterminate_; }
        void SetColor(Color c) { color_ = c; RequestRepaint(); }
        void SetTrackColor(Color c) { trackColor_ = c; RequestRepaint(); }
        void SetThickness(float t) { thickness_ = max(1.0f, t); InvalidateLayout(); RequestRepaint(); }
        void SetSize(float s) { size_ = max(8.0f, s); InvalidateLayout(); RequestRepaint(); }
        void SetAnimationSpeed(float s) { animSpeed_ = max(0.1f, s); }

        bool UseCache() const override { return false; }   // 每帧都变 → 缓存只会白重建一遍，反而更耗

        Size MeasureOverride(const Size&) override { return Size(size_, size_); }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || !rt) return;
            if (!(std::isfinite(arrangedRect_.x) && std::isfinite(arrangedRect_.y) &&
                  std::isfinite(arrangedRect_.width) && std::isfinite(arrangedRect_.height))) return;   // NaN/Inf 守卫
            float s = min(arrangedRect_.width, arrangedRect_.height);
            if (s <= 1.0f) s = size_;
            float th = min(thickness_, s * 0.5f);
            float cx = arrangedRect_.x + arrangedRect_.width * 0.5f;
            float cy = arrangedRect_.y + arrangedRect_.height * 0.5f;
            float r = (s - th) * 0.5f;
            if (r <= 0.5f) return;
            ID2D1StrokeStyle* ss = RoundStroke(rt);
            if (!brush_) rt->CreateSolidColorBrush(color_.ToD2D(), brush_.GetAddressOf());
            else brush_->SetColor(color_.ToD2D());
            if (!indeterminate_) {
                if (displayValue_ < 0.999f && trackColor_.a > 0.0f) {
                    if (!trackBrush_) rt->CreateSolidColorBrush(trackColor_.ToD2D(), trackBrush_.GetAddressOf());
                    else trackBrush_->SetColor(trackColor_.ToD2D());
                    if (trackBrush_) DrawArc(rt, cx, cy, r, 0.0f, 359.9f, th, trackBrush_.Get(), ss);
                }
                if (displayValue_ > 0.001f && brush_) DrawArc(rt, cx, cy, r, -90.0f, 360.0f * displayValue_, th, brush_.Get(), ss);
            }
            else if (brush_) {
                // 头部每周期整整 2 圈（720°=360°×2）：indT_ 回绕时 head 720°→0° 视觉连续，不跳（540° 会跳 180°）。
                // 扫角 20°↔240° 呼吸：最大变化速率 ≈110·2π≈691°/周期 < 720°/周期 → 尾端 (head-sweep) 始终向前，不倒走。
                float head = indT_ * 720.0f;
                float sweep = 130.0f - 70.0f * cosf(2.0f * 3.14159265f * indT_);   // 60°↔200°；速率峰值≈440°/周期 → 尾端最低速≈280°/周期（不再明显卡顿）
                DrawArc(rt, cx, cy, r, head - sweep, sweep, th, brush_.Get(), ss);
            }
        }
        void UpdateAnimation(float dt) override {
            if (indeterminate_) {
                indT_ += dt * animSpeed_ / indPeriod_;
                while (indT_ >= 1.0f) indT_ -= 1.0f;
                RequestRepaint();
                return;
            }
            if (fabs(displayValue_ - value_) > 0.001f) {   // 值变化走缓动
                displayValue_ += (value_ - displayValue_) * min(1.0f, animSpeed_ * 8.0f * dt);
                if (fabs(displayValue_ - value_) < 0.001f) displayValue_ = value_;
                RequestRepaint();
            }
        }
        bool HasActiveAnimation() const override {
            if (indeterminate_) return true;
            return fabs(displayValue_ - value_) > 0.001f;
        }
        void ReleaseDeviceResources() override { brush_.Reset(); trackBrush_.Reset(); UIElement::ReleaseDeviceResources(); }

    private:
        static ID2D1StrokeStyle* RoundStroke(ID2D1RenderTarget* rt) {
            static ComPtr<ID2D1StrokeStyle> ss;
            if (!ss && rt) {
                ComPtr<ID2D1Factory> f; rt->GetFactory(&f);
                if (f) f->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                    D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &ss);
            }
            return ss.Get();
        }
        static void DrawArc(ID2D1RenderTarget* rt, float cx, float cy, float r, float startDeg, float sweepDeg,
            float thickness, ID2D1Brush* brush, ID2D1StrokeStyle* ss) {
            sweepDeg = clamp(sweepDeg, -359.9f, 359.9f);
            if (fabs(sweepDeg) < 0.05f) return;
            const float kPi = 3.14159265358979f;
            float a0 = startDeg * kPi / 180.0f;
            float a1 = (startDeg + sweepDeg) * kPi / 180.0f;
            D2D1_POINT_2F p0 = D2D1::Point2F(cx + r * cosf(a0), cy + r * sinf(a0));
            D2D1_POINT_2F p1 = D2D1::Point2F(cx + r * cosf(a1), cy + r * sinf(a1));
            ComPtr<ID2D1Factory> f; rt->GetFactory(&f);
            if (!f) return;
            ComPtr<ID2D1PathGeometry> path;
            if (FAILED(f->CreatePathGeometry(&path)) || !path) return;
            ComPtr<ID2D1GeometrySink> sink;
            if (FAILED(path->Open(&sink)) || !sink) return;
            sink->BeginFigure(p0, D2D1_FIGURE_BEGIN_HOLLOW);
            D2D1_ARC_SEGMENT arc;
            arc.point = p1;
            arc.size = D2D1::SizeF(r, r);
            arc.rotationAngle = 0.0f;
            arc.sweepDirection = (sweepDeg >= 0.0f) ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
            arc.arcSize = (fabs(sweepDeg) > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
            sink->AddArc(arc);
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();
            rt->DrawGeometry(path.Get(), brush, thickness, ss);
        }

        float size_ = DefaultSize, thickness_ = DefaultThickness;
        float value_ = 0.0f, displayValue_ = 0.0f, indT_ = 0.0f, indPeriod_ = 1.8f, animSpeed_ = DefaultAnimationSpeed;
        bool indeterminate_ = false;
        Color color_, trackColor_;
        ComPtr<ID2D1SolidColorBrush> brush_, trackBrush_;
    };

    // ============================================================================
    // NumberBox / Spinner：数字输入（只接受数字；范围 / 步进 / 滚轮 / 上下按钮）
    // ============================================================================
    class NumberBox : public UIElement {
    public:
        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Edit; }
        inline static float DefaultSpinWidth = 22.0f;
        inline static float DefaultHeight = 32.0f;
        inline static Color DefaultSpinBgColor = Color(0, 0, 0, 0.04f);
        inline static Color DefaultArrowColor = Color(0.30f, 0.30f, 0.30f, 1.0f);
        inline static Color DefaultAccentColor = Color(0.0f, 0.47f, 0.84f, 1.0f);

        ZSignal<double> ValueChanged;
        ZSignal<const std::wstring&> TextChanged;   // 每次文本变化（可做实时校验/错误态）

        NumberBox(double value = 0.0) {
            value_ = value;
            height_ = DefaultHeight;
            spinBgColor_ = DefaultSpinBgColor; arrowColor_ = DefaultArrowColor; accentColor_ = DefaultAccentColor;
            default_ = value;
            text_ = std::make_shared<TextBox>();
            text_->SetParent(this);
            text_->SetImeEnabled(false);   // 数字框不弹输入法
            text_->SetInputFilter([](wchar_t c) {
                return (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' || c == L'+';
            });
            text_->Connect(text_->TextChanged, [this](const std::wstring& s) {
                if (updating_) return;                        // 程序性同步不触发校验
                edited_ = true;
                // 空文本 = 空集，不属于任何数值（也超出范围）→ 控件层面直接标错误
                if (s.empty()) { text_->SetError(true); }
                else { wchar_t* e = nullptr; wcstod(s.c_str(), &e); if (!e || *e != L'\0') text_->SetError(true); }
                InvalidateLayout();   // 重置按钮显隐会改变按钮区宽度 → 叠加层需重排
                RequestRepaint();
                TextChanged(s);
            });
            text_->Connect(text_->ReturnPressed, [this]() { CommitText(); });
            text_->Connect(text_->Blurred, [this]() { CommitText(); });
            SyncText();
            upBtn_ = std::make_shared<SpinButton>(); upBtn_->owner = this; upBtn_->kind = SpinButton::Kind::Up; upBtn_->SetParent(this);
            downBtn_ = std::make_shared<SpinButton>(); downBtn_->owner = this; downBtn_->kind = SpinButton::Kind::Down; downBtn_->SetParent(this);
            clearBtn_ = std::make_shared<SpinButton>(); clearBtn_->owner = this; clearBtn_->kind = SpinButton::Kind::Clear; clearBtn_->SetParent(this);
            clearBtn_->SetVisibleNoInvalidate(false);
        }

        std::shared_ptr<TextBox> GetTextBox() const { return text_; }

        // 三个独立的小按钮（清除 / 上 / 下）：各自用框架的 OnMouseEnter/Leave 管悬停，最稳
        struct SpinButton : public UIElement {
            enum class Kind { Clear, Up, Down };
            NumberBox* owner = nullptr;
            Kind kind = Kind::Up;
            Size MeasureOverride(const Size&) override { return Size(0, 0); }
            void Draw(ID2D1RenderTarget* rt) override { if (owner) owner->DrawSpinButton(rt, arrangedRect_, kind, hoverProg_); }
            UIElement* HitTest(float x, float y) override { return (visible_ && arrangedRect_.Contains(x, y)) ? this : nullptr; }
            void OnMouseEnter() override { hover_ = true; RequestRepaint(); }
            void OnMouseLeave() override { hover_ = false; RequestRepaint(); }
            void OnMouseDown(float, float) override { if (owner) owner->OnSpinButton(kind); }
            void UpdateAnimation(float dt) override {
                float t = hover_ ? 1.0f : 0.0f;
                if (fabs(t - hoverProg_) > 0.001f) {
                    hoverProg_ += (t - hoverProg_) * min(1.0f, 14.0f * dt);
                    if (fabs(t - hoverProg_) < 0.001f) hoverProg_ = t;
                    RequestRepaint();
                }
            }
            bool HasActiveAnimation() const override { return hover_ ? hoverProg_ < 0.999f : hoverProg_ > 0.001f; }
        private:
            bool hover_ = false;
            float hoverProg_ = 0.0f;
        };

        void OnSpinButton(SpinButton::Kind kind) {
            if (!IsEffectivelyEnabled()) return;
            if (kind == SpinButton::Kind::Clear) ResetToDefault();
            else if (kind == SpinButton::Kind::Up) StepUp();
            else StepDown();
        }
        void DrawSpinButton(ID2D1RenderTarget* rt, const Rect& r, SpinButton::Kind kind, float prog) {
            if (!visible_ || !rt || r.width <= 0.5f) return;
            if (prog > 0.01f) {
                D2D1_COLOR_F hc = hoverBgColor_.ToD2D(); hc.a *= prog;
                if (!hoverBtnBrush_) rt->CreateSolidColorBrush(hc, hoverBtnBrush_.GetAddressOf());
                else hoverBtnBrush_->SetColor(hc);
                if (hoverBtnBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(r.ToD2D(), 4.0f, 4.0f), hoverBtnBrush_.Get());
            }
            // 用系统图标字体的字形（比手画线整齐、居中）
            unsigned short code; D2D1_COLOR_F c;
            if (kind == SpinButton::Kind::Clear) { code = 0xE711; c = (prog > 0.5f ? clearHoverColor_ : arrowColor_).ToD2D(); }
            else if (kind == SpinButton::Kind::Up) { code = 0xE70E; c = (prog > 0.5f ? accentColor_ : arrowColor_).ToD2D(); }
            else { code = 0xE70D; c = (prog > 0.5f ? accentColor_ : arrowColor_).ToD2D(); }
            DrawGlyph(rt, code, r, c, min(r.width, r.height) * 0.62f);
        }
        static const std::wstring& IconFontFamily() {
            static const std::wstring f = []() -> std::wstring {
                IDWriteFactory* fac = FontManager::Instance().GetFactory();
                if (fac) {
                    ComPtr<IDWriteFontCollection> c;
                    if (SUCCEEDED(fac->GetSystemFontCollection(&c)) && c) {
                        UINT32 i = 0; BOOL e = FALSE;
                        if (SUCCEEDED(c->FindFamilyName(L"Segoe Fluent Icons", &i, &e)) && e) return L"Segoe Fluent Icons";
                    }
                }
                return L"Segoe MDL2 Assets";
            }();
            return f;
        }
        void DrawGlyph(ID2D1RenderTarget* rt, unsigned short code, const Rect& r, const D2D1_COLOR_F& color, float size) {
            FontSpec spec; spec.familyName = IconFontFamily(); spec.size = size;
            IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(spec);
            if (!fmt) return;
            std::wstring g(1, (wchar_t)code);
            ComPtr<IDWriteTextLayout> layout = FontManager::Instance().GetStyledLayout(g, fmt, r.width, r.height, true, 1, 1, 0.0f, 0);
            if (!layout) return;
            if (!arrowBrush_) rt->CreateSolidColorBrush(color, arrowBrush_.GetAddressOf());
            else arrowBrush_->SetColor(color);
            if (arrowBrush_) rt->DrawTextLayout(D2D1::Point2F(Snap(r.x), Snap(r.y)), layout.Get(), arrowBrush_.Get());
        }

        void SetValue(double v, bool fire = true) {
            v = Clamp(v);
            if (v == value_) { SyncText(); return; }
            value_ = v; SyncText();
            if (fire) ValueChanged(value_);
            RequestRepaint();
        }
        double GetValue() const { return value_; }
        void SetRange(double lo, double hi) { min_ = lo; max_ = hi; SetValue(value_); }
        void SetMin(double v) { min_ = v; SetValue(value_); }
        void SetMax(double v) { max_ = v; SetValue(value_); }
        double GetMin() const { return min_; } double GetMax() const { return max_; }
        void SetStep(double s) { step_ = (s > 0.0 ? s : 1.0); }
        double GetStep() const { return step_; }
        void SetDecimals(int d) { decimals_ = max(0, d); SyncText(); }
        void SetWrap(bool on) { wrap_ = on; }
        void SetSpinButtons(bool on) { showSpin_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetSpinWidth(float w) { spinWidth_ = max(0.0f, w); InvalidateLayout(); RequestRepaint(); }
        void SetColors(Color spinBg, Color arrow, Color accent) { spinBgColor_ = spinBg; arrowColor_ = arrow; accentColor_ = accent; RequestRepaint(); }
        // 转发到内部输入框
        void SetPlaceholder(const std::wstring& t) { if (text_) text_->SetPlaceholder(t); }
        void SetEnabled(bool e) { UIElement::SetEnabled(e); if (text_) text_->SetEnabled(e); }

        void StepUp() { ApplyStep(+1); }
        void StepDown() { ApplyStep(-1); }

        // 默认值：值 != 默认值时，按钮区左侧会显示一个「清除 ×」恢复到默认值
        void SetDefaultValue(double v) { default_ = v; InvalidateLayout(); RequestRepaint(); }
        double GetDefaultValue() const { return default_; }
        void ResetToDefault() {
            SetValue(default_);
            if (text_) text_->SetError(false);   // 恢复默认后必须清错误态，否则一直红着
            std::wstring t = text_ ? text_->GetText() : std::wstring();
            TextChanged(t);                      // 通知应用重新校验
        }
        bool IsDefaultValue() const { return fabs(value_ - default_) < 1e-9; }
        // 重置按钮是否显示：按"当前输入框文本"实时判断（不是已提交的缓存值），所以输入过程中就会显示
        bool ShowClear() const {
            if (!text_) return false;
            wchar_t buf[64]; swprintf(buf, 64, L"%.*f", decimals_, default_);
            return text_->GetText() != buf;
        }
        // 错误态（转发给内部输入框）
        void SetError(bool on) { if (text_) text_->SetError(on); }
        bool IsError() const { return text_ ? text_->IsError() : false; }

        bool IsFocusable() const override { return true; }

        Size MeasureOverride(const Size& availableSize) override {
            float w = width_ > 0 ? width_ : (availableSize.width != FLT_MAX ? availableSize.width : 120.0f);
            float h = height_ > 0 ? height_ : DefaultHeight;
            if (text_) text_->Measure(Size(max(0.0f, w - (showSpin_ ? spinWidth_ : 0.0f)), h));
            return Size(w, h);
        }
        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (text_) text_->Arrange(finalRect);   // 输入框占满；按钮叠在其右侧之上
            float sw = showSpin_ ? min(spinWidth_, finalRect.width * 0.4f) : 0.0f;
            float cw = (showSpin_ && ShowClear()) ? min(clearWidth_, max(0.0f, finalRect.width - sw)) : 0.0f;
            float x0 = finalRect.x + finalRect.width - sw - cw;
            float hh = finalRect.height * 0.5f;
            if (upBtn_) { upBtn_->SetVisibleNoInvalidate(showSpin_ && sw > 1.0f); upBtn_->Arrange(Rect(finalRect.x + finalRect.width - sw, finalRect.y, sw, hh)); }
            if (downBtn_) { downBtn_->SetVisibleNoInvalidate(showSpin_ && sw > 1.0f); downBtn_->Arrange(Rect(finalRect.x + finalRect.width - sw, finalRect.y + hh, sw, hh)); }
            if (clearBtn_) { clearBtn_->SetVisibleNoInvalidate(showSpin_ && cw > 1.0f); clearBtn_->Arrange(Rect(x0, finalRect.y + finalRect.height * 0.25f, cw, finalRect.height * 0.5f)); }
        }
        void Draw(ID2D1RenderTarget* rt) override {
            // 上下按钮的浅色底（画在按钮之下）
            if (!visible_ || !rt || !showSpin_) return;
            float sw = min(spinWidth_, arrangedRect_.width * 0.4f);
            if (sw <= 1.0f) return;
            Rect col(arrangedRect_.x + arrangedRect_.width - sw, arrangedRect_.y, sw, arrangedRect_.height);
            if (!spinBrush_) rt->CreateSolidColorBrush(spinBgColor_.ToD2D(), spinBrush_.GetAddressOf());
            else spinBrush_->SetColor(spinBgColor_.ToD2D());
            if (spinBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(col.ToD2D(), 4.0f, 4.0f), spinBrush_.Get());
        }
        void DrawButtons(ID2D1RenderTarget* rt, const Rect& r) {
            if (!visible_ || !rt || !showSpin_ || r.width <= 1.0f) return;
            float sw = min(spinWidth_, r.width);
            Rect spin(r.x + r.width - sw, r.y, sw, r.height);
            if (!spinBrush_) rt->CreateSolidColorBrush(spinBgColor_.ToD2D(), spinBrush_.GetAddressOf());
            else spinBrush_->SetColor(spinBgColor_.ToD2D());
            if (spinBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(spin.ToD2D(), 4.0f, 4.0f), spinBrush_.Get());
            // 悬停按钮的圆角背景（带渐入动画）
            if (btnHover_ > 0.01f) {
                Rect hr = hoverClear_ ? ClearRect(r) : (hoverUp_ ? UpRect(r) : (hoverDown_ ? DownRect(r) : Rect(0, 0, 0, 0)));
                if (hr.width > 0.5f) {
                    D2D1_COLOR_F hc = hoverBgColor_.ToD2D(); hc.a *= btnHover_;
                    if (!hoverBtnBrush_) rt->CreateSolidColorBrush(hc, hoverBtnBrush_.GetAddressOf());
                    else hoverBtnBrush_->SetColor(hc);
                    if (hoverBtnBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(hr.ToD2D(), 4.0f, 4.0f), hoverBtnBrush_.Get());
                }
            }
            float half = spin.height * 0.5f;
            DrawArrow(rt, spin.x, spin.y, sw, half, true, hoverUp_);
            DrawArrow(rt, spin.x, spin.y + half, sw, half, false, hoverDown_);
            if (ShowClear()) {
                float cw = min(clearWidth_, max(0.0f, r.width - sw));
                DrawClear(rt, Rect(r.x + r.width - sw - cw, r.y, cw, r.height));
            }
        }
        void DrawClear(ID2D1RenderTarget* rt, const Rect& r) {
            D2D1_COLOR_F c = (hoverClear_ ? accentColor_ : arrowColor_).ToD2D();
            if (!arrowBrush_) rt->CreateSolidColorBrush(c, arrowBrush_.GetAddressOf());
            else arrowBrush_->SetColor(c);
            if (!arrowBrush_) return;
            float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
            float e = min(r.width, r.height) * 0.24f * (1.0f + 0.12f * btnHover_);   // 本体随 hover 微放大
            rt->DrawLine(D2D1::Point2F(cx - e, cy - e), D2D1::Point2F(cx + e, cy + e), arrowBrush_.Get(), 1.4f);
            rt->DrawLine(D2D1::Point2F(cx + e, cy - e), D2D1::Point2F(cx - e, cy + e), arrowBrush_.Get(), 1.4f);
        }
        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            if (text_) childrenView_.push_back(text_.get());
            if (clearBtn_) childrenView_.push_back(clearBtn_.get());
            if (upBtn_) childrenView_.push_back(upBtn_.get());
            if (downBtn_) childrenView_.push_back(downBtn_.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }
        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            if (text_) text_->AttachWindowRecursive(w);
            if (clearBtn_) clearBtn_->AttachWindowRecursive(w);
            if (upBtn_) upBtn_->AttachWindowRecursive(w);
            if (downBtn_) downBtn_->AttachWindowRecursive(w);
        }
        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            if (clearBtn_) { if (UIElement* h = clearBtn_->HitTest(x, y)) return h; }
            if (upBtn_) { if (UIElement* h = upBtn_->HitTest(x, y)) return h; }
            if (downBtn_) { if (UIElement* h = downBtn_->HitTest(x, y)) return h; }
            if (text_) { if (UIElement* h = text_->HitTest(x, y)) return h; }
            return this;
        }
        void ButtonsHover(float x, float y, const Rect& r) {
            bool cu = ShowClear() && ClearRect(r).Contains(x, y);
            bool uu = UpRect(r).Contains(x, y), dd = DownRect(r).Contains(x, y);
            if (cu != hoverClear_ || uu != hoverUp_ || dd != hoverDown_) { hoverClear_ = cu; hoverUp_ = uu; hoverDown_ = dd; RequestRepaint(); }
        }
        void ButtonsLeave() { if (hoverClear_ || hoverUp_ || hoverDown_) { hoverClear_ = hoverUp_ = hoverDown_ = false; RequestRepaint(); } }
        void ButtonsDown(float x, float y, const Rect& r) {
            if (!IsEffectivelyEnabled()) return;
            if (ShowClear() && ClearRect(r).Contains(x, y)) { ResetToDefault(); return; }
            if (DownRect(r).Contains(x, y)) { StepDown(); return; }
            if (UpRect(r).Contains(x, y)) { StepUp(); return; }
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            (void)deltaX;
            if (!IsEffectivelyEnabled()) return false;
            if (deltaY > 0.0f) { StepUp(); return true; }
            if (deltaY < 0.0f) { StepDown(); return true; }
            return false;
        }
        void UpdateAnimation(float dt) override {
            float ht = (hoverUp_ || hoverDown_ || hoverClear_) ? 1.0f : 0.0f;
            if (fabs(ht - btnHover_) > 0.001f) {
                btnHover_ += (ht - btnHover_) * min(1.0f, 14.0f * dt);
                if (fabs(ht - btnHover_) < 0.001f) btnHover_ = ht;
                RequestRepaint();
            }
            if (clearBtn_) clearBtn_->UpdateAnimation(dt);
            if (upBtn_) upBtn_->UpdateAnimation(dt);
            if (downBtn_) downBtn_->UpdateAnimation(dt);
            if (text_) text_->UpdateAnimation(dt);
        }
        bool HasActiveAnimation() const override {
            if ((clearBtn_ && clearBtn_->HasActiveAnimation()) || (upBtn_ && upBtn_->HasActiveAnimation()) ||
                (downBtn_ && downBtn_->HasActiveAnimation())) return true;
            return text_ ? text_->HasActiveAnimation() : false;
        }
        void ReleaseDeviceResources() override { spinBrush_.Reset(); arrowBrush_.Reset(); hoverBtnBrush_.Reset(); if (text_) text_->ReleaseDeviceResources(); UIElement::ReleaseDeviceResources(); }

    private:
        double Clamp(double v) const { if (v < min_) return min_; if (v > max_) return max_; return v; }
        Rect UpRect(const Rect& r) const { float sw = min(spinWidth_, r.width); return Rect(r.x + r.width - sw, r.y, sw, r.height * 0.5f); }
        Rect DownRect(const Rect& r) const { float sw = min(spinWidth_, r.width); return Rect(r.x + r.width - sw, r.y + r.height * 0.5f, sw, r.height * 0.5f); }
        Rect ClearRect(const Rect& r) const {
            if (!ShowClear()) return Rect(0, 0, 0, 0);
            float sw = min(spinWidth_, r.width);
            float cw = min(clearWidth_, max(0.0f, r.width - sw));
            return Rect(r.x + r.width - sw - cw, r.y, cw, r.height);
        }
        void ApplyStep(int dir) {
            CommitText();   // 关键：先读输入框里的"当前"值（用户可能已改但没失焦），否则会用旧缓存值把改动顶掉
            double v = value_ + (dir > 0 ? step_ : -step_);
            if (!wrap_) v = Clamp(v);
            else { if (v > max_) v = min_; else if (v < min_) v = max_; }
            SetValue(v);
        }
        void SyncText() {
            if (!text_) return;
            wchar_t buf[64];
            swprintf(buf, 64, L"%.*f", decimals_, value_);
            updating_ = true;
            text_->SetText(buf);
            updating_ = false;
            edited_ = false;
        }
        void CommitText() {
            if (!text_ || !edited_) return;
            std::wstring s = text_->GetText();
            double v = value_;
            if (!s.empty()) { const wchar_t* p = s.c_str(); wchar_t* end = nullptr; v = wcstod(p, &end); }
            SetValue(v);
            edited_ = false;
        }
        void DrawArrow(ID2D1RenderTarget* rt, float x, float y, float w, float h, bool up, bool hot) {
            D2D1_COLOR_F c = (hot ? accentColor_ : arrowColor_).ToD2D();
            if (!arrowBrush_) rt->CreateSolidColorBrush(c, arrowBrush_.GetAddressOf());
            else arrowBrush_->SetColor(c);
            if (!arrowBrush_) return;
            float cx = x + w * 0.5f, cy = y + h * 0.5f, e = min(w, h) * 0.22f;
            if (up) {
                rt->DrawLine(D2D1::Point2F(cx - e, cy + e * 0.7f), D2D1::Point2F(cx, cy - e * 0.7f), arrowBrush_.Get(), 1.4f);
                rt->DrawLine(D2D1::Point2F(cx, cy - e * 0.7f), D2D1::Point2F(cx + e, cy + e * 0.7f), arrowBrush_.Get(), 1.4f);
            }
            else {
                rt->DrawLine(D2D1::Point2F(cx - e, cy - e * 0.7f), D2D1::Point2F(cx, cy + e * 0.7f), arrowBrush_.Get(), 1.4f);
                rt->DrawLine(D2D1::Point2F(cx, cy + e * 0.7f), D2D1::Point2F(cx + e, cy - e * 0.7f), arrowBrush_.Get(), 1.4f);
            }
        }

        std::shared_ptr<TextBox> text_;
        std::shared_ptr<SpinButton> clearBtn_, upBtn_, downBtn_;
        double value_ = 0.0, min_ = -1e15, max_ = 1e15, step_ = 1.0, default_ = 0.0;
        int decimals_ = 0;
        bool wrap_ = false, showSpin_ = true, hoverUp_ = false, hoverDown_ = false, hoverClear_ = false, edited_ = false, updating_ = false;
        float spinWidth_ = DefaultSpinWidth, clearWidth_ = 28.0f;
        float btnHover_ = 0.0f;
        Rect spinRect_{};
        Color spinBgColor_, arrowColor_, accentColor_, hoverBgColor_ = Color(0, 0, 0, 0.18f);
        Color clearHoverColor_ = Color(0.80f, 0.13f, 0.13f, 1.0f);   // 清除键悬停→红
        ComPtr<ID2D1SolidColorBrush> spinBrush_, arrowBrush_, hoverBtnBrush_;
    };

    // ============================================================================
    // SplitView：两栏 + 可拖动分隔条（左右 / 上下）
    // ============================================================================
    class SplitView : public UIElement {
    public:
        enum class Orientation { Vertical, Horizontal };   // Vertical = 左右两栏（竖分隔条）
        inline static float DefaultSplitterWidth = 6.0f;
        inline static Color DefaultSplitterColor = Color(0, 0, 0, 0.06f);
        inline static Color DefaultHoverColor = Color(0.0f, 0.47f, 0.84f, 0.35f);

        ZSignal<float> SplitChanged;   // 新比例 0..1

        SplitView() { width_ = 0; height_ = 0; fillWidth_ = true; fillHeight_ = true; splitterColor_ = DefaultSplitterColor; hoverColor_ = DefaultHoverColor; }

        void SetOrientation(Orientation o) { if (o != orientation_) { orientation_ = o; InvalidateLayout(); RequestRepaint(); } }
        Orientation GetOrientation() const { return orientation_; }
        void SetFirst(std::shared_ptr<UIElement> e) {
            if (first_) first_->SetParent(nullptr);
            first_ = e; if (e) e->SetParent(this);
            MarkChildrenDirty(); InvalidateLayout(); RequestRepaint();
        }
        void SetSecond(std::shared_ptr<UIElement> e) {
            if (second_) second_->SetParent(nullptr);
            second_ = e; if (e) e->SetParent(this);
            MarkChildrenDirty(); InvalidateLayout(); RequestRepaint();
        }
        std::shared_ptr<UIElement> GetFirst() const { return first_; }
        std::shared_ptr<UIElement> GetSecond() const { return second_; }
        void SetSplitRatio(float r) { r = clamp(r, 0.0f, 1.0f); if (r != ratio_) { ratio_ = r; InvalidateLayout(); RequestRepaint(); } }
        float GetSplitRatio() const { return ratio_; }
        void SetSplitterWidth(float w) { splitterW_ = max(1.0f, w); InvalidateLayout(); RequestRepaint(); }
        float GetSplitterWidth() const { return splitterW_; }
        void SetMinFirst(float px) { minFirst_ = max(0.0f, px); InvalidateLayout(); }
        void SetMinSecond(float px) { minSecond_ = max(0.0f, px); InvalidateLayout(); }
        void SetSplitterColor(Color c) { splitterColor_ = c; RequestRepaint(); }
        void SetHoverColor(Color c) { hoverColor_ = c; RequestRepaint(); }

        bool UseCache() const override { return false; }

        Size MeasureOverride(const Size& availableSize) override {
            float w = width_ > 0 ? width_ : (availableSize.width != FLT_MAX ? availableSize.width : 200.0f);
            float h = height_ > 0 ? height_ : (availableSize.height != FLT_MAX ? availableSize.height : 200.0f);
            bool vert = (orientation_ == Orientation::Vertical);
            float usable = max(0.0f, (vert ? w : h) - splitterW_);
            float firstLen = clamp(ratio_ * usable, minFirst_, max(0.0f, usable - minSecond_));
            float secondLen = max(0.0f, usable - firstLen);
            if (vert) {
                if (first_) first_->Measure(Size(firstLen, h));
                if (second_) second_->Measure(Size(secondLen, h));
            }
            else {
                if (first_) first_->Measure(Size(w, firstLen));
                if (second_) second_->Measure(Size(w, secondLen));
            }
            return Size(w, h);
        }
        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            ApplyArrange(finalRect);
        }
        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || !rt) return;
            if (!(std::isfinite(arrangedRect_.x) && std::isfinite(arrangedRect_.y) &&
                  std::isfinite(arrangedRect_.width) && std::isfinite(arrangedRect_.height))) return;   // NaN/Inf 守卫
            if (!splitterBrush_) rt->CreateSolidColorBrush(splitterColor_.ToD2D(), splitterBrush_.GetAddressOf());
            else splitterBrush_->SetColor((hovered_ || dragging_) ? hoverColor_.ToD2D() : splitterColor_.ToD2D());
            if (splitterBrush_) {
                bool vert = (orientation_ == Orientation::Vertical);
                float ins = 2.0f;   // 内缩一点 + 圆角（不再方角贴边）
                D2D1_RECT_F sr = D2D1::RectF(splitterRect_.x + (vert ? ins : 0.0f), splitterRect_.y + (vert ? 0.0f : ins),
                    splitterRect_.x + splitterRect_.width - (vert ? ins : 0.0f), splitterRect_.y + splitterRect_.height - (vert ? 0.0f : ins));
                float rad = min(sr.right - sr.left, sr.bottom - sr.top) * 0.5f;
                rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), splitterBrush_.Get());
            }
            // 中间的小握把线
            if (!gripBrush_) rt->CreateSolidColorBrush(Color(1, 1, 1, 0.9f).ToD2D(), gripBrush_.GetAddressOf());
            if (gripBrush_) {
                float cx = splitterRect_.x + splitterRect_.width * 0.5f;
                float cy = splitterRect_.y + splitterRect_.height * 0.5f;
                if (orientation_ == Orientation::Vertical) {
                    rt->DrawLine(D2D1::Point2F(cx, cy - 10.0f), D2D1::Point2F(cx, cy + 10.0f), gripBrush_.Get(), 1.0f);
                }
                else {
                    rt->DrawLine(D2D1::Point2F(cx - 10.0f, cy), D2D1::Point2F(cx + 10.0f, cy), gripBrush_.Get(), 1.0f);
                }
            }
        }
        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            if (first_) childrenView_.push_back(first_.get());
            if (second_) childrenView_.push_back(second_.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }
        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            if (first_) first_->AttachWindowRecursive(w);
            if (second_) second_->AttachWindowRecursive(w);
        }
        // 关键：容器必须把 UpdateAnimation / HasActiveAnimation 递归给子控件，否则子控件的动画（悬停/翻页）不会跑
        void UpdateAnimation(float dt) override {
            if (first_) first_->UpdateAnimation(dt);
            if (second_) second_->UpdateAnimation(dt);
        }
        bool HasActiveAnimation() const override {
            return (first_ && first_->HasActiveAnimation()) || (second_ && second_->HasActiveAnimation());
        }
        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            // 分隔条带（含一点额外命中区，方便拖动）
            Rect hit(splitterRect_.x - 3.0f, splitterRect_.y - 3.0f, splitterRect_.width + 6.0f, splitterRect_.height + 6.0f);
            if (hit.Contains(x, y)) return this;
            if (first_) { if (UIElement* h = first_->HitTest(x, y)) return h; }
            if (second_) { if (UIElement* h = second_->HitTest(x, y)) return h; }
            return this;
        }
        void OnMouseMove(float x, float y) override {
            if (dragging_) { UpdateDrag(x, y); return; }
            Rect hit(splitterRect_.x - 3.0f, splitterRect_.y - 3.0f, splitterRect_.width + 6.0f, splitterRect_.height + 6.0f);
            bool h = hit.Contains(x, y);
            if (h != hovered_) { hovered_ = h; RequestRepaint(); }
        }
        void OnMouseLeave() override { if (hovered_) { hovered_ = false; RequestRepaint(); } }
        void OnMouseDown(float x, float y) override {
            Rect hit(splitterRect_.x - 3.0f, splitterRect_.y - 3.0f, splitterRect_.width + 6.0f, splitterRect_.height + 6.0f);
            if (hit.Contains(x, y)) { dragging_ = true; RequestRepaint(); }
        }
        void OnMouseUp(float, float) override { if (dragging_) { dragging_ = false; RequestRepaint(); } }

    private:
        void ApplyArrange(const Rect& fr) {
            bool vert = (orientation_ == Orientation::Vertical);
            float total = (vert ? fr.width : fr.height);
            float usable = max(0.0f, total - splitterW_);
            float firstLen = clamp(ratio_ * usable, minFirst_, max(0.0f, usable - minSecond_));
            float secondLen = max(0.0f, usable - firstLen);
            auto clipTo = [](UIElement* e, const Rect& r) {
                float b = e->GetBleed();
                e->SetClipRect(Rect(r.x - b, r.y - b, r.width + b * 2, r.height + b * 2));   // 保留出血（阴影/边框）不被裁
            };
            if (vert) {
                if (first_) { Rect r(fr.x, fr.y, firstLen, fr.height); first_->Arrange(r); clipTo(first_.get(), r); }
                splitterRect_ = Rect(fr.x + firstLen, fr.y, splitterW_, fr.height);
                if (second_) { Rect r(fr.x + firstLen + splitterW_, fr.y, secondLen, fr.height); second_->Arrange(r); clipTo(second_.get(), r); }
            }
            else {
                if (first_) { Rect r(fr.x, fr.y, fr.width, firstLen); first_->Arrange(r); clipTo(first_.get(), r); }
                splitterRect_ = Rect(fr.x, fr.y + firstLen, fr.width, splitterW_);
                if (second_) { Rect r(fr.x, fr.y + firstLen + splitterW_, fr.width, secondLen); second_->Arrange(r); clipTo(second_.get(), r); }
            }
        }
        void UpdateDrag(float x, float y) {
            bool vert = (orientation_ == Orientation::Vertical);
            float total = (vert ? arrangedRect_.width : arrangedRect_.height);
            float usable = max(1.0f, total - splitterW_);
            float pos = (vert ? x : y) - (vert ? arrangedRect_.x : arrangedRect_.y) - splitterW_ * 0.5f;
            float r = clamp(pos / usable, 0.0f, 1.0f);
            if (r != ratio_) {
                ratio_ = r;
                ApplyArrange(arrangedRect_);             // 立即摆位（跟手）
                if (first_) first_->InvalidateLayout();  // 变的是"子控件"的尺寸 → 标记子控件（不是重排整个 SplitView）
                if (second_) second_->InvalidateLayout();
                SplitChanged(ratio_);
                RequestRepaint();
            }
        }

        Orientation orientation_ = Orientation::Vertical;
        std::shared_ptr<UIElement> first_, second_;
        float ratio_ = 0.35f, splitterW_ = DefaultSplitterWidth;
        float minFirst_ = 0.0f, minSecond_ = 0.0f;
        bool dragging_ = false, hovered_ = false;
        Rect splitterRect_{};
        Color splitterColor_, hoverColor_;
        ComPtr<ID2D1SolidColorBrush> splitterBrush_, gripBrush_;
    };

} // namespace ZufyUI