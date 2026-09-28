#pragma once
#include "ZufyUI.h"

// ============================================================================
// 图标系统（FontIcon）
// ----------------------------------------------------------------------------
// 用系统图标字体的字形当图标：Win11 = "Segoe Fluent Icons"，Win10 = "Segoe MDL2 Assets"，
// 两者码点基本一致。运行期探测可用性，缺前者自动回退后者。
//
// 设计要点：
//   - Icon 枚举值**就是字体码点本身**（`Icon::Add = 0xE710`），不维护平行码点表 → 不会漂移。
//   - FontIcon 只画一个字形；字号决定图标大小，颜色可设，居中对齐。
//   - 字形 layout 走 FontManager 全局缓存（key 含文本/字体/尺寸/对齐）→ 大量图标不重复建 layout。
// ============================================================================

namespace ZufyUI {

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
