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
        IDWriteTextLayout* finalLayout = fm.GetDisplayLayout(text, fmt, maxWidth, maxHeight, forceNoWrap);
        if (!finalLayout) {
            // 未命中：原始布局（也走缓存）测宽 → 超宽二分截断（中间 layout 不缓存）
            IDWriteTextLayout* measureLayout = fm.GetRawLayout(text, fmt, maxWidth, maxHeight, forceNoWrap);
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
            fm.CacheDisplayLayout(text, fmt, maxWidth, maxHeight, forceNoWrap, finalLayout);
        }

        if (!textBrush) rt->CreateSolidColorBrush(color, textBrush.GetAddressOf());
        else textBrush->SetColor(color);

        DWRITE_TEXT_METRICS fm2{};
        finalLayout->GetMetrics(&fm2);
        float drawX = rect.left;
        if (align == TextHAlign::Center) drawX = rect.left + (maxWidth - fm2.width) / 2.0f;
        else if (align == TextHAlign::Right) drawX = rect.left + (maxWidth - fm2.width);
        if (drawX < rect.left) drawX = rect.left;
        rt->DrawTextLayout(D2D1::Point2F(Snap(drawX), Snap(rect.top)), finalLayout, textBrush.Get());
    }

    // ---------- 标签（支持对齐、换行/省略号，最终修正版） ----------
    class Label : public UIElement {
    public:
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
        std::wstring GetText() const { return text_; }
        void SetTextColor(Color color) { textColor_ = color; textBrush_.Reset(); RequestRepaint(); }
        Color GetTextColor() const { return textColor_; }
        void SetTextOverflow(TextOverflow mode) { overflow_ = mode; InvalidateLayout(); RequestRepaint(); }
        TextOverflow GetTextOverflow() const { return overflow_; }
        void SetAlignment(HAlign hAlign, VAlign vAlign) {
            hAlign_ = hAlign;
            vAlign_ = vAlign;
            InvalidateLayout();
            RequestRepaint();
        }
        HAlign GetHorizontalAlignment() const { return hAlign_; }
        VAlign GetVerticalAlignment() const { return vAlign_; }
        void SetPadding(const Thickness& p) { padding_ = p; InvalidateLayout(); RequestRepaint(); }
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

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& c : children_) if (c) childrenView_.push_back(c.get());
            return childrenView_;
        }
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

            // 图标尺寸
            float iw = 0, ih = 0;
            if (image_ && !image_->IsNull()) {
                iw = iconSize_.width > 0 ? iconSize_.width : (float)image_->Width();
                ih = iconSize_.height > 0 ? iconSize_.height : (float)image_->Height();
            }

            // 文本尺寸
            float textW = 0, textH = 0;
            if (!text_.empty()) {
                IDWriteFactory* factory = FontManager::Instance().GetFactory();
                IDWriteTextFormat* fmt = GetFontFormat();
                if (factory && fmt) {
                    if (overflow_ == TextOverflow::Wrap && availableSize.width != FLT_MAX && availableSize.width > 0) {
                        ComPtr<IDWriteTextLayout> tempLayout;
                        float availW = max(0.0f, availableSize.width - padW - (iw > 0 ? iw + iconSpacing_ : 0.0f));
                        factory->CreateTextLayout(text_.c_str(), (UINT32)text_.length(), fmt, availW, 10000.0f, &tempLayout);
                        if (tempLayout) {
                            tempLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                            DWRITE_TEXT_METRICS metrics;
                            tempLayout->GetMetrics(&metrics);
                            textW = min(availW, metrics.width);
                            textH = metrics.height;
                        }
                    }
                    else {
                        ComPtr<IDWriteTextLayout> layout;
                        factory->CreateTextLayout(text_.c_str(), (UINT32)text_.length(), fmt, 10000.0f, 10000.0f, &layout);
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
            float x = finalRect.x + padding_.left;
            float cy = finalRect.y + finalRect.height * 0.5f;
            iconRect_ = D2D1::RectF(0, 0, 0, 0);
            if (measuredIconW_ > 0) {
                float iy = cy - measuredIconH_ * 0.5f;
                iconRect_ = D2D1::RectF(x, iy, x + measuredIconW_, iy + measuredIconH_);
                x += measuredIconW_;
                if (measuredTextW_ > 0 || !children_.empty()) x += iconSpacing_;
            }
            textLeft_ = x;
            x += measuredTextW_;
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
            if (rect.right < rect.left) rect.right = rect.left;
            if (rect.bottom < rect.top) rect.bottom = rect.top;
            std::wstring displayText = text_;

            // ---------- Ellipsis 模式：先做截断判定，得到 displayText ----------
            if (overflow_ == TextOverflow::Ellipsis) {
                ComPtr<IDWriteTextLayout> measureLayout;
                factory->CreateTextLayout(text_.c_str(), (UINT32)text_.length(), fmt,
                    rect.right - rect.left, rect.bottom - rect.top, &measureLayout);
                if (measureLayout) {
                    measureLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    DWRITE_TEXT_METRICS metrics;
                    measureLayout->GetMetrics(&metrics);
                    if (metrics.width > (rect.right - rect.left) && displayText.length() > 3) {
                        const std::wstring suffix = L"...";
                        int len = (int)displayText.length();
                        int lo = 0, hi = len - 1, best = -1;
                        while (lo <= hi) {                       // 二分（与 DrawTextWithEllipsis 对齐；原来逐字符 O(n)）
                            int mid = (lo + hi) / 2;
                            std::wstring test;
                            test.reserve((size_t)mid + suffix.size());
                            test.assign(displayText, 0, mid);
                            test += suffix;
                            ComPtr<IDWriteTextLayout> testLayout;
                            factory->CreateTextLayout(test.c_str(), (UINT32)test.length(), fmt,
                                rect.right - rect.left, rect.bottom - rect.top, &testLayout);
                            if (!testLayout) break;
                            testLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                            DWRITE_TEXT_METRICS tm;
                            testLayout->GetMetrics(&tm);
                            if (tm.width <= (rect.right - rect.left)) { best = mid; lo = mid + 1; }
                            else hi = mid - 1;
                        }
                        displayText = (best >= 0) ? (displayText.substr(0, best) + suffix) : suffix;
                    }
                }
            }

            // ---------- 统一绘制：创建带对齐的 layout ----------
            ComPtr<IDWriteTextLayout> layout;
            factory->CreateTextLayout(displayText.c_str(), (UINT32)displayText.length(), fmt,
                rect.right - rect.left, rect.bottom - rect.top, &layout);
            if (!layout) return;

            if (lineSpacing_ > 0.0f)
                layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineSpacing_, lineSpacing_ * 0.8f);
            if (maxLines_ > 0) {
                DWRITE_LINE_METRICS lm{};
                UINT32 lc = 0;
                layout->GetLineMetrics(&lm, 1, &lc);
                float lh = lm.height > 0 ? lm.height : layout->GetFontSize() * 1.4f;
                layout->SetMaxHeight(maxLines_ * lh);
                ComPtr<IDWriteInlineObject> trimmingSign;
                if (SUCCEEDED(factory->CreateEllipsisTrimmingSign(fmt, &trimmingSign))) {
                    DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                    layout->SetTrimming(&trimming, trimmingSign.Get());
                }
            }

            // 水平对齐
            switch (hAlign_) {
            case HAlign::Left:   layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
            case HAlign::Center: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
            case HAlign::Right:  layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
            }
            // 垂直对齐
            switch (vAlign_) {
            case VAlign::Top:    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); break;
            case VAlign::Center: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); break;
            case VAlign::Bottom: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR); break;
            }
            // 换行策略
            if (overflow_ == TextOverflow::Wrap) {
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            }
            else {
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }

            if (!textBrush_) rt->CreateSolidColorBrush(textColor_.ToD2D(), textBrush_.GetAddressOf());
            else textBrush_->SetColor(textColor_.ToD2D());
            if (!IsEffectivelyEnabled() && textBrush_) textBrush_->SetColor(DefaultDisabledColor.ToD2D());

            rt->DrawTextLayout(D2D1::Point2F(Snap(rect.left), Snap(rect.top)), layout.Get(), textBrush_.Get());
        }

        void ReleaseDeviceResources() override {
            textBrush_.Reset();
            bgBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
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
        Size iconSize_{ 0, 0 };
        float iconSpacing_ = 6.0f;
        std::vector<std::shared_ptr<UIElement>> children_;
        std::vector<Size> childSizes_;
        float measuredIconW_ = 0, measuredIconH_ = 0;
        float measuredTextW_ = 0, measuredTextH_ = 0;
        float textLeft_ = 0;
        D2D1_RECT_F iconRect_ = D2D1::RectF(0, 0, 0, 0);
    };

    // ---------- 按钮（内部使用 Label 渲染文本） ----------
    class Button : public UIElement {
    public:
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
        // 悬停时：若文本被截断，自动用完整文本当 tooltip；用户设置过的 tooltip 优先
        std::wstring GetToolTip() const override {
            std::wstring custom = UIElement::GetToolTip();
            if (!custom.empty()) return custom;
            if (text_.empty()) return L"";
            float availW = GetArrangedRect().width - padding_ * 2.0f;
            if (availW <= 0.0f) return L"";
            IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(GetEffectiveFontSpec());
            if (!fmt) return L"";
            // GetDisplayLayout 非空 = 需要截断显示（即文本放不下）
            IDWriteTextLayout* layout = FontManager::Instance().GetDisplayLayout(text_, fmt, availW, 1.0e6f, true);
            return layout ? text_ : L"";
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
        inline static float DefaultWidth = 160.0f;
        inline static float DefaultHeight = 30.0f;
        inline static D2D1_COLOR_F DefaultBgColor = D2D1::ColorF(0.98f, 0.98f, 0.98f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
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
        bool IsTextInput() const override { return true; }

        Rect GetImeCandidateRect() const override {
            float cursorX = GetTextPositionX(cursorPos_ + (hasComposition_ ? min(compositionCursorPos_, (int)compositionText_.size()) : 0));
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

            // 1. 背景和边框（不裁剪）
            D2D1_COLOR_F bgCol = D2D1::ColorF(
                bgColor_.r + (hoverBgColor_.r - bgColor_.r) * hoverProgress_,
                bgColor_.g + (hoverBgColor_.g - bgColor_.g) * hoverProgress_,
                bgColor_.b + (hoverBgColor_.b - bgColor_.b) * hoverProgress_, 1.0f);
            if (!bgBrush_) rt->CreateSolidColorBrush(bgCol, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bgCol);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            D2D1_COLOR_F borderCol = D2D1::ColorF(
                borderColor_.r + (hoverBorderColor_.r - borderColor_.r) * hoverProgress_,
                borderColor_.g + (hoverBorderColor_.g - borderColor_.g) * hoverProgress_,
                borderColor_.b + (hoverBorderColor_.b - borderColor_.b) * hoverProgress_, 1.0f);
            if (!borderBrush_) rt->CreateSolidColorBrush(borderCol, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderCol);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);

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
            if (focused_ && showCursor_ && selectionStart_ == selectionEnd_) {
                int globalCursorPos = cursorPos_;
                if (hasComposition_) {
                    globalCursorPos += min(compositionCursorPos_, (int)compositionText_.size());
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
            if (OpenClipboard(GetActiveWindow())) {
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
            if (!OpenClipboard(GetActiveWindow())) return;
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
                globalCursorPos += min(compositionCursorPos_, (int)compositionText_.size());
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

            UIZSignals::DrawOverlay.connect(
                [this](Window* w, ID2D1RenderTarget* rt) {
                    if (w != GetWindow()) return;   // 只画在自己所属窗口上
                    if (expandProgress_ > 0.01f || expanded_) DrawExpandedList(rt);
                },
                ConnectionThread::CurrentThread,
                connectionGroup_
            );

            UIZSignals::GlobalMouseDown.connect(
                [this](Window* w, float x, float y) {
                    if (w && w != GetWindow()) return;   // 只处理本窗口的点击
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
            allItems_.push_back(item);
            ApplyFilter();
        }
        void SetItems(const std::vector<std::wstring>& items) {
            allItems_ = items;
            editText_.clear();
            caretPos_ = 0;
            ApplyFilter();
        }
        void SetSelectedIndex(int index) {
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

        bool IsExpanded() const { return expanded_; }
        // 所属页面/元素被隐藏时自动收起下拉（避免隐藏页里的展开弹层继续通过全局 DrawOverlay 绘制）
        void OnVisibilityChanged(bool visible) override {
            if (!visible && (expanded_ || expandProgress_ > 0.01f)) CollapseInternal();
        }
        float GetExpandProgress() const { return expandProgress_; }
        void Collapse() { CollapseInternal(); }

        // ---------- 可编辑 / 输入过滤 ----------
        void SetEditable(bool editable) { editable_ = editable; RequestRepaint(); }
        bool IsEditable() const { return editable_; }
        void SetFilterEnabled(bool enable) { filterEnabled_ = enable; ApplyFilter(); }
        bool IsFilterEnabled() const { return filterEnabled_; }
        void SetEditText(const std::wstring& text) {
            editText_ = text;
            caretPos_ = (int)editText_.size();
            if (filterEnabled_) ApplyFilter(); else { UpdateToolTip(); RequestRepaint(); }
        }
        std::wstring GetEditText() const { return editText_; }
        bool IsFocusable() const override { return editable_; }
        static std::wstring ToLower(std::wstring s) { for (auto& c : s) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32); return s; }
        void ApplyFilter() {
            std::wstring prevSel = (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) ? items_[selectedIndex_] : L"";
            if (!filterEnabled_ || editText_.empty()) items_ = allItems_;
            else {
                std::wstring key = ToLower(editText_);
                items_.clear();
                for (auto& s : allItems_) if (ToLower(s).find(key) != std::wstring::npos) items_.push_back(s);
            }
            disabledItems_.clear();
            selectedIndex_ = -1;
            for (int i = 0; i < (int)items_.size(); ++i)
                if (!prevSel.empty() && items_[i] == prevSel) { selectedIndex_ = i; break; }   // 保留原选中
            if (selectedIndex_ < 0 && !items_.empty()) selectedIndex_ = 0;
            itemWidthsDirty_ = true;
            UpdateIndicatorPosition();
            UpdateToolTip();
            InvalidateLayout();
            RequestRepaint();
        }

        // ---------- 数据操作 / 占位符 / 每项禁用 / 开合信号 / 最大可见项 ----------
        void InsertItem(int index, const std::wstring& item) {
            index = max(0, min((int)allItems_.size(), index));
            allItems_.insert(allItems_.begin() + index, item);
            ApplyFilter();
        }
        void RemoveItemAt(int index) {
            if (index < 0 || index >= (int)allItems_.size()) return;
            allItems_.erase(allItems_.begin() + index);
            ApplyFilter();
        }
        void RemoveItem(const std::wstring& item) {
            for (int i = 0; i < (int)allItems_.size(); ++i) if (allItems_[i] == item) { RemoveItemAt(i); return; }
        }
        void ClearItems() { allItems_.clear(); items_.clear(); disabledItems_.clear(); selectedIndex_ = -1; UpdateIndicatorPosition(); InvalidateLayout(); RequestRepaint(); }
        int GetItemCount() const { return (int)items_.size(); }
        std::wstring GetItemAt(int index) const { return (index >= 0 && index < (int)items_.size()) ? items_[index] : L""; }
        const std::vector<std::wstring>& GetItems() const { return items_; }
        void SetItemDisabled(int index, bool disabled = true) {
            if (index < 0 || index >= (int)items_.size()) return;
            if ((int)disabledItems_.size() < (int)items_.size()) disabledItems_.resize(items_.size(), false);
            disabledItems_[index] = disabled;
            RequestRepaint();
        }
        bool IsItemDisabled(int index) const { return index >= 0 && index < (int)disabledItems_.size() && disabledItems_[index]; }
        void SetPlaceholder(const std::wstring& placeholder) { placeholder_ = placeholder; RequestRepaint(); }
        const std::wstring& GetPlaceholder() const { return placeholder_; }
        void SetMaxVisibleItems(int count) { maxVisibleItems_ = max(0, count); InvalidateLayout(); RequestRepaint(); }
        int GetMaxVisibleItems() const { return maxVisibleItems_; }
        void Expand() { ExpandInternal(); }
        void SetOpen(bool open) { if (open) ExpandInternal(); else CollapseInternal(); }
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

            float fullListHeight = (float)items_.size() * listItemHeight_;
            float visibleListHeight = fullListHeight;
            if (maxVisibleItems_ > 0) visibleListHeight = min(fullListHeight, maxVisibleItems_ * listItemHeight_);
            float currentListHeight = visibleListHeight * expandProgress_;

            D2D1_SIZE_F rtSize = rt->GetSize();
            float windowHeightDip = rtSize.height;
            float windowWidthDip = rtSize.width;

            float belowSpace = windowHeightDip - (arrangedRect_.y + arrangedRect_.height);
            float aboveSpace = arrangedRect_.y;
            if (belowSpace < fullListHeight && aboveSpace > belowSpace) {
                expandUp_ = true;
            }
            else {
                expandUp_ = false;
            }

            float listY;
            float availableHeight;

            if (expandUp_) {
                listY = arrangedRect_.y - currentListHeight;
                availableHeight = aboveSpace;
                if (availableHeight < 0) availableHeight = 0;
                if (availableHeight < currentListHeight) currentListHeight = availableHeight;
                if (listY < 0) listY = 0;
            }
            else {
                listY = arrangedRect_.y + arrangedRect_.height;
                availableHeight = belowSpace;
                if (availableHeight < 0) availableHeight = 0;
                if (availableHeight < currentListHeight) currentListHeight = availableHeight;
            }

            listViewHeight_ = currentListHeight;
            if (listViewHeight_ <= 0.0f) return;

            float totalContentHeight = fullListHeight;
            listMaxScroll_ = max(0.0f, totalContentHeight - listViewHeight_);
            listScrollOffset_ = clamp(listScrollOffset_, 0.0f, listMaxScroll_);

            D2D1_RECT_F listRect = D2D1::RectF(arrangedRect_.x, listY, arrangedRect_.x + ListWidth(), listY + listViewHeight_);

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
            int lastVisibleIndex = min((int)items_.size() - 1, (int)((listScrollOffset_ + listViewHeight_) / listItemHeight_));

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
                if (indicatorBottom > listY && indicatorDrawTop < listY + listViewHeight_) {
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
                float trackHeight = listViewHeight_ - 4.0f;
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
            if (!expanded_) {
                hoveredItemIndex_ = -1;
                MouseMove.Fire(x, y);
                return;
            }

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
            RequestRepaint();
            MouseMove.Fire(x, y);
        }

        void OnMouseDown(float x, float y) override {
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
            if (expanded_ && listMaxScroll_ > 0) {
                listScrollOffset_ = clamp(listScrollOffset_ - deltaY * 30.0f, 0.0f, listMaxScroll_);
                RequestRepaint();
                return true;
            }
            return false;
        }

        void OnKeyDown(WPARAM key, LPARAM lParam) override {
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

        void ExpandInternal() {
            if (expanded_ || items_.empty()) return;
            expanded_ = true;
            justExpanded_ = true;
            expandUp_ = false;
            AcquireControlCapture();
            hoveredItemIndex_ = -1;
            pressedItemIndex_ = -1;
            pressedOnSelf_ = false;
            listScrollOffset_ = 0.0f;
            float fullListHeight = (float)items_.size() * listItemHeight_;
            if (maxVisibleItems_ > 0) fullListHeight = min(fullListHeight, maxVisibleItems_ * listItemHeight_);
            listViewHeight_ = fullListHeight;
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
        std::vector<bool> disabledItems_;
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

    // ==================== 滚动容器（ScrollViewer） ====================
    class ScrollViewer : public UIElement {
    public:
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
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        // 滚动条子元素（内部类）
        class ScrollBar : public UIElement {
        public:
            ScrollBar(bool vertical, ScrollViewer* owner)
                : vertical_(vertical), owner_(owner), dragging_(false),
                dragStartMouse_(0.0f), dragStartValue_(0.0f) {
                visible_ = false;
                bleed_ = 4.0f;
            }

            // 禁用滚动条自身的离屏缓存，确保每次直接绘制，实时同步
            bool UseCache() const override { return false; }

            void UpdateFromOwner() {
                // 从所有者读取最新值
                if (vertical_) {
                    maxValue_ = owner_->maxScrollY_;
                    viewportSize_ = owner_->arrangedRect_.height - (owner_->showHorizontalScrollBar_ ? owner_->scrollBarWidth_ : 0);
                    currentValue_ = owner_->scrollOffsetY_;
                }
                else {
                    maxValue_ = owner_->maxScrollX_;
                    viewportSize_ = owner_->arrangedRect_.width - (owner_->showVerticalScrollBar_ ? owner_->scrollBarWidth_ : 0);
                    currentValue_ = owner_->scrollOffsetX_;
                }
            }

            UIElement* HitTest(float x, float y) override {
                if (!visible_) return nullptr;
                Rect r = GetArrangedRect();
                float extra = owner_->scrollBarHitExtra_;
                Rect hotRect(r.x - extra, r.y - extra, r.width + extra * 2, r.height + extra * 2);
                if (hotRect.Contains(x, y)) return this;
                return nullptr;
            }

            void OnMouseEnter() override {
                // 更新所有者悬停状态，触发扩张动画
                if (vertical_) owner_->isVerticalHovered_ = true;
                else owner_->isHorizontalHovered_ = true;
                RequestRepaint();
            }

            void OnMouseLeave() override {
                if (vertical_) owner_->isVerticalHovered_ = false;
                else owner_->isHorizontalHovered_ = false;
                if (!dragging_) RequestRepaint();
            }

            void OnMouseDown(float x, float y) override {
                UpdateFromOwner();
                Rect r = GetArrangedRect();
                float thumbPos, thumbLength;
                GetThumbInfo(r, thumbPos, thumbLength);

                if (vertical_) {
                    float thumbStart = r.y + thumbPos;
                    float thumbEnd = thumbStart + thumbLength;
                    if (y >= thumbStart && y <= thumbEnd) {
                        dragging_ = true;
                        dragStartMouse_ = y;
                        dragStartValue_ = currentValue_;
                    }
                    else {
                        float ratio = (y - r.y - thumbLength / 2) / (r.height - thumbLength);
                        float newVal = clamp(ratio * maxValue_, 0.0f, maxValue_);
                        owner_->ScrollTo(owner_->scrollOffsetX_, newVal, true);
                    }
                }
                else {
                    float thumbStart = r.x + thumbPos;
                    float thumbEnd = thumbStart + thumbLength;
                    if (x >= thumbStart && x <= thumbEnd) {
                        dragging_ = true;
                        dragStartMouse_ = x;
                        dragStartValue_ = currentValue_;
                    }
                    else {
                        float ratio = (x - r.x - thumbLength / 2) / (r.width - thumbLength);
                        float newVal = clamp(ratio * maxValue_, 0.0f, maxValue_);
                        owner_->ScrollTo(newVal, owner_->scrollOffsetY_, true);
                    }
                }
            }

            void OnMouseMove(float x, float y) override {
                if (!dragging_) return;
                UpdateFromOwner();
                Rect r = GetArrangedRect();
                float thumbLength;
                if (vertical_) {
                    thumbLength = max(owner_->scrollBarMinLength_,
                        r.height * (r.height / (maxValue_ + r.height)));
                    if (r.height > thumbLength) {
                        float ratio = (y - dragStartMouse_) / (r.height - thumbLength);
                        float newVal = clamp(dragStartValue_ + ratio * maxValue_, 0.0f, maxValue_);
                        // 立即更新实际偏移，并同步目标值
                        owner_->scrollOffsetY_ = newVal;
                        owner_->targetScrollOffsetY_ = newVal;
                        owner_->ArrangeContent();
                        owner_->RequestRepaint(); // 通知父级更新
                    }
                }
                else {
                    thumbLength = max(owner_->scrollBarMinLength_,
                        r.width * (r.width / (maxValue_ + r.width)));
                    if (r.width > thumbLength) {
                        float ratio = (x - dragStartMouse_) / (r.width - thumbLength);
                        float newVal = clamp(dragStartValue_ + ratio * maxValue_, 0.0f, maxValue_);
                        owner_->scrollOffsetX_ = newVal;
                        owner_->targetScrollOffsetX_ = newVal;
                        owner_->ArrangeContent();
                        owner_->RequestRepaint();
                    }
                }
            }

            void OnMouseUp(float x, float y) override {
                if (dragging_) {
                    dragging_ = false;
                    RequestRepaint();
                }
            }

            Size MeasureOverride(const Size& availableSize) override {
                if (vertical_) return Size(owner_->scrollBarWidth_, 100.0f);
                else return Size(100.0f, owner_->scrollBarWidth_);
            }

            void ArrangeOverride(const Rect& finalRect) override {
                UIElement::ArrangeOverride(finalRect);
            }

            void Draw(ID2D1RenderTarget* rt) override {
                if (!visible_) return;
                UpdateFromOwner(); // 确保用最新值绘制
                Rect r = GetArrangedRect();

                // 获取当前悬停动画进度（由 ScrollViewer::UpdateAnimation 更新）
                float hoverProgress = vertical_ ? owner_->verticalHoverProgress_ : owner_->horizontalHoverProgress_;

                // 轨道颜色和滑块颜色
                D2D1_COLOR_F trackCol = owner_->trackColor_;
                D2D1_COLOR_F thumbCol = owner_->thumbColor_;
                if (hoverProgress > 0.01f) {
                    thumbCol = D2D1::ColorF(
                        owner_->thumbColor_.r + (owner_->hoverThumbColor_.r - owner_->thumbColor_.r) * hoverProgress,
                        owner_->thumbColor_.g + (owner_->hoverThumbColor_.g - owner_->thumbColor_.g) * hoverProgress,
                        owner_->thumbColor_.b + (owner_->hoverThumbColor_.b - owner_->thumbColor_.b) * hoverProgress,
                        owner_->thumbColor_.a + (owner_->hoverThumbColor_.a - owner_->thumbColor_.a) * hoverProgress);
                }

                // 轨道宽度根据动画进度增加
                float trackWidth = owner_->scrollBarWidth_ * (1.0f + 0.25f * hoverProgress);
                D2D1_ROUNDED_RECT trackRect;
                if (vertical_) {
                    float trackX = r.x + r.width - trackWidth;
                    trackRect = D2D1::RoundedRect(D2D1::RectF(trackX, r.y, trackX + trackWidth, r.y + r.height),
                        trackWidth / 2, trackWidth / 2);
                }
                else {
                    float trackY = r.y + r.height - trackWidth;
                    trackRect = D2D1::RoundedRect(D2D1::RectF(r.x, trackY, r.x + r.width, trackY + trackWidth),
                        trackWidth / 2, trackWidth / 2);
                }

                if (!owner_->trackBrush_) {
                    rt->CreateSolidColorBrush(trackCol, owner_->trackBrush_.GetAddressOf());
                }
                else {
                    owner_->trackBrush_->SetColor(trackCol);
                }
                if (owner_->trackBrush_) rt->FillRoundedRectangle(trackRect, owner_->trackBrush_.Get());

                // 滑块
                float thumbPos, thumbLength;
                GetThumbInfo(r, thumbPos, thumbLength);
                D2D1_ROUNDED_RECT thumbRect;
                if (vertical_) {
                    float thumbX = trackRect.rect.left + 1;
                    float thumbW = trackWidth - 2;
                    thumbRect = D2D1::RoundedRect(D2D1::RectF(thumbX, r.y + thumbPos,
                        thumbX + thumbW, r.y + thumbPos + thumbLength),
                        thumbW / 2, thumbW / 2);
                }
                else {
                    float thumbY = trackRect.rect.top + 1;
                    float thumbH = trackWidth - 2;
                    thumbRect = D2D1::RoundedRect(D2D1::RectF(r.x + thumbPos, thumbY,
                        r.x + thumbPos + thumbLength, thumbY + thumbH),
                        thumbH / 2, thumbH / 2);
                }

                if (!owner_->thumbBrush_) {
                    rt->CreateSolidColorBrush(thumbCol, owner_->thumbBrush_.GetAddressOf());
                }
                else {
                    owner_->thumbBrush_->SetColor(thumbCol);
                }
                if (owner_->thumbBrush_) rt->FillRoundedRectangle(thumbRect, owner_->thumbBrush_.Get());
            }

            void UpdateAnimation(float deltaTime) override {
                // 滚动条自身无动画，动画由所有者统一驱动
            }

            bool HasActiveAnimation() const override { return false; }

        void ReleaseDeviceResources() override {
                UIElement::ReleaseDeviceResources();
            }

        private:
            void GetThumbInfo(const Rect& r, float& pos, float& length) const {
                if (maxValue_ <= 0) {
                    pos = 0;
                    length = (vertical_ ? r.height : r.width);
                    return;
                }
                float total = vertical_ ? r.height : r.width;
                length = max(owner_->scrollBarMinLength_, total * (total / (maxValue_ + total)));
                float ratio = Snap(currentValue_) / maxValue_;
                pos = (total - length) * ratio;
            }

            bool vertical_;
            ScrollViewer* owner_;
            float maxValue_, viewportSize_, currentValue_;
            bool dragging_;
            float dragStartMouse_, dragStartValue_;
        }; // 结束 ScrollBar 内部类

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
            vScrollBar_ = std::make_shared<ScrollBar>(true, this);
            vScrollBar_->SetParent(this);

            // 创建水平滚动条子元素
            hScrollBar_ = std::make_shared<ScrollBar>(false, this);
            hScrollBar_->SetParent(this);
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

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            if (content_)
                childrenView_.push_back(content_.get());
            if (vScrollBar_)
                childrenView_.push_back(vScrollBar_.get());
            if (hScrollBar_)
                childrenView_.push_back(hScrollBar_.get());
            return childrenView_;
        }

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

            // 更新 hover 动画进度（平滑扩张/收缩）
            float targetV = isVerticalHovered_ ? 1.0f : 0.0f;
            if (verticalHoverProgress_ != targetV) {
                verticalHoverProgress_ += (targetV - verticalHoverProgress_) * min(1.0f, hoverAnimationSpeed_ * deltaTime);
                if (fabs(verticalHoverProgress_ - targetV) < 0.001f) verticalHoverProgress_ = targetV;
                RequestRepaint();
                if (vScrollBar_) vScrollBar_->RequestRepaint();
            }
            float targetH = isHorizontalHovered_ ? 1.0f : 0.0f;
            if (horizontalHoverProgress_ != targetH) {
                horizontalHoverProgress_ += (targetH - horizontalHoverProgress_) * min(1.0f, hoverAnimationSpeed_ * deltaTime);
                if (fabs(horizontalHoverProgress_ - targetH) < 0.001f) horizontalHoverProgress_ = targetH;
                RequestRepaint();
                if (hScrollBar_) hScrollBar_->RequestRepaint();
            }

            // 更新子元素动画
            content_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            const float epsilon = 0.1f;
            bool scrollAnim = (fabs(targetScrollOffsetX_ - scrollOffsetX_) > epsilon) ||
                (fabs(targetScrollOffsetY_ - scrollOffsetY_) > epsilon);
            bool hoverAnim = (verticalHoverProgress_ > 0.001f && verticalHoverProgress_ < 0.999f) ||
                (horizontalHoverProgress_ > 0.001f && horizontalHoverProgress_ < 0.999f);
            bool contentAnim = content_ ? content_->HasActiveAnimation() : false;
            return scrollAnim || hoverAnim || contentAnim;
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

        void SetChecked(bool checked) { SetState(checked ? State::Checked : State::Unchecked); }
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

} // namespace ZufyUI