#pragma once
#include "ZufyUIWidgets.h"   // Icon / IconGlyph / IconFontFamily 已下沉到 ZufyUIWidgets.h（核心图标系统）

// ============================================================================
// FontIcon：只画一个图标字形的独立元素。
// ----------------------------------------------------------------------------
// 图标系统（Icon 枚举 / IconGlyph / IconFontFamily）已下沉到 ZufyUIWidgets.h，
// 这样任何控件（尤其基于 Label 的：Button 等）都能直接用字体字形图标；
//   例如 `Label::SetIcon(Icon::Refresh)` / `Button::SetIcon(Icon::Add)`。
// 本文件只提供"单独的图标元素" FontIcon / MakeFontIcon。
//
//   - 用系统图标字体的字形当图标：Win11 = "Segoe Fluent Icons"，Win10 = "Segoe MDL2 Assets"。
//   - 字号决定图标大小，颜色可设，居中对齐。
//   - 字形 layout 走 FontManager 全局缓存（key 含文本/字体/尺寸/对齐）→ 大量图标不重复建 layout。
// ============================================================================

namespace ZufyUI {

    // ------------------------------------------------------------------
    // FontIcon：只画一个图标字形的元素。字号 = 图标大小；颜色可设；居中。
    // ------------------------------------------------------------------
    class FontIcon : public UIElement {
    public:
        inline static float DefaultSize = 16.0f;
        inline static Color DefaultColor = Color::FromArgb(255, 0, 0, 0);

        FontIcon() = default;
        explicit FontIcon(Icon icon, float size = DefaultSize) : icon_(icon), glyphSize_(size) {}
        FontIcon(Icon icon, float size, Color color) : icon_(icon), glyphSize_(size), color_(color) {}

        void SetIcon(Icon icon) { if (icon_ != icon) { icon_ = icon; InvalidateLayout(); RequestRepaint(); } }
        Icon GetIcon() const { return icon_; }
        void SetIconSize(float size) {
            if (size > 0.0f && glyphSize_ != size) { glyphSize_ = size; InvalidateLayout(); RequestRepaint(); }
        }
        float GetIconSize() const { return glyphSize_; }
        void SetColor(Color color) { color_ = color; RequestRepaint(); }
        Color GetColor() const { return color_; }

        // 图标字体（实例字体覆盖仍优先）
        std::optional<FontSpec> GetTypeDefaultFont() const override {
            FontSpec spec;
            spec.familyName = IconFontFamily();
            spec.size = glyphSize_;
            return spec;
        }

        Size MeasureOverride(const Size& availableSize) override {
            (void)availableSize;
            std::wstring glyph = IconGlyph(icon_);
            IDWriteTextFormat* fmt = GetFontFormat();
            if (glyph.empty() || !fmt) return Size(glyphSize_, glyphSize_);
            auto layout = FontManager::Instance().GetStyledLayout(glyph, fmt, 10000.0f, 10000.0f, true, 0, 0, 0.0f, 0);
            if (!layout) return Size(glyphSize_, glyphSize_);
            DWRITE_TEXT_METRICS m{};
            layout->GetMetrics(&m);
            return Size(m.width, m.height);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || !rt) return;
            std::wstring glyph = IconGlyph(icon_);
            if (glyph.empty()) return;
            IDWriteTextFormat* fmt = GetFontFormat();
            if (!fmt) return;
            float w = arrangedRect_.width > 0.0f ? arrangedRect_.width : glyphSize_;
            float h = arrangedRect_.height > 0.0f ? arrangedRect_.height : glyphSize_;
            auto layout = FontManager::Instance().GetStyledLayout(glyph, fmt, w, h, true, 1, 1, 0.0f, 0);   // 居中
            if (!layout) return;
            if (!brush_) rt->CreateSolidColorBrush(color_.ToD2D(), brush_.GetAddressOf());
            else brush_->SetColor(color_.ToD2D());
            rt->DrawTextLayout(D2D1::Point2F(Snap(arrangedRect_.x), Snap(arrangedRect_.y)), layout.Get(), brush_.Get());
        }

    private:
        Icon  icon_ = Icon::None;
        float glyphSize_ = DefaultSize;
        Color color_ = DefaultColor;
        ComPtr<ID2D1SolidColorBrush> brush_;
    };

    inline std::shared_ptr<FontIcon> MakeFontIcon(Icon icon, float size = FontIcon::DefaultSize,
        Color color = FontIcon::DefaultColor) {
        return std::make_shared<FontIcon>(icon, size, color);
    }

} // namespace ZufyUI
