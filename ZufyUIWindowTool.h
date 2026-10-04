#pragma once
// ============================================================================
// ZufyUIWindowTool.h —— 窗口工具控件（自定义标题栏等）
// ----------------------------------------------------------------------------
// 这里放“窗口级”的控件，未来还会放内置的 MessageBox 之类。
// 自定义标题栏 = TitleBar（继承 UIElement），由 Window::SetCustomTitleBar 安装。
// 它“不参与布局”，由 Window 放到 (0,0)，并把根布局整体下移其高度。
// 标题栏及按钮默认 UseCache，仅状态变化时重绘；且不参与 Tab 焦点。
// ============================================================================

#include "ZufyUIImages.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>   // IFileOpenDialog / IFileSaveDialog（新版文件对话框）
#include <commdlg.h>    // ChooseColor（颜色对话框）
#include <typeinfo>         // RTTI 类型名（调试详情）
#include <uiautomation.h>   // UI Automation（无障碍）
#include <oleauto.h>        // SysAllocString / SafeArray*
#pragma comment(lib, "UIAutomationCore.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

// windows.h 里 MessageBox 是 MessageBoxW 的宏，会和 ZufyUI::MessageBox 撞；这里撤掉宏。
// 之后要用 Win32 的请显式写 MessageBoxW / MessageBoxA。
#ifdef MessageBox
#undef MessageBox
#endif

namespace ZufyUI {

    namespace detail_wintool {
        // 系统标题栏图标字体：Win11 = Segoe Fluent Icons；Win10 = Segoe MDL2 Assets
        inline IDWriteTextFormat* IconFormat(float dipSize) {
            IDWriteFactory* f = FontManager::Instance().GetFactory();
            if (!f) return nullptr;
            static IDWriteTextFormat* fmt = nullptr;
            static float cached = 0.0f;
            if (fmt && cached == dipSize) return fmt;
            if (fmt) { fmt->Release(); fmt = nullptr; }
            const wchar_t* families[] = { L"Segoe Fluent Icons", L"Segoe MDL2 Assets" };
            for (auto fam : families) {
                if (SUCCEEDED(f->CreateTextFormat(fam, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, dipSize, L"en-us", &fmt)) && fmt)
                    break;
            }
            if (fmt) {
                fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
            cached = dipSize;
            return fmt;
        }
    }

    // ------------------------------------------------------------------
    // CaptionButton：标题栏三件套按钮（最小化 / 最大化还原 / 关闭）
    // ------------------------------------------------------------------
    class CaptionButton : public UIElement {
    public:
        enum class Kind { Minimize, MaximizeRestore, Close, Pin };   // Pin = 客户端可切换按钮（不映射系统码）

        inline static Color DefaultHoverColor = Color::FromArgb(255, 229, 229, 229);
        inline static Color DefaultPressedColor = Color::FromArgb(255, 214, 214, 214);
        inline static Color DefaultCloseHoverColor = Color::FromArgb(255, 196, 43, 28);
        inline static Color DefaultClosePressedColor = Color::FromArgb(255, 177, 36, 24);
        inline static Color DefaultGlyphColor = Color::FromArgb(255, 30, 30, 30);
        inline static float DefaultWidth = 46.0f;

        explicit CaptionButton(Kind kind) : kind_(kind) {}

        // 点击信号（由 DefaultTitleBar 连接默认行为；自定义标题栏可自行连接/拦截）
        ZSignal<> Clicked;

        void SetKind(Kind k) { kind_ = k; RequestRepaint(); }
        Kind GetKind() const { return kind_; }
        void SetHoverColor(Color c) { hoverColor_ = c; RequestRepaint(); }
        void SetPressedColor(Color c) { pressedColor_ = c; RequestRepaint(); }
        void SetCloseHoverColor(Color c) { closeHoverColor_ = c; RequestRepaint(); }
        void SetClosePressedColor(Color c) { closePressedColor_ = c; RequestRepaint(); }
        void SetGlyphColor(Color c) { glyphColor_ = c; RequestRepaint(); }
        void SetGlyph(wchar_t code) { glyphCode_ = code; RequestRepaint(); }        // 自定义字形（Pin 等）
        void SetActiveState(bool on) { active_ = on; RequestRepaint(); }            // 切换态（如 Pin 已置顶高亮）
        void SetActiveGlyphColor(Color c) { pinActiveColor_ = c; RequestRepaint(); }
        void SetButtonWidth(float w) { width_ = max(0.0f, w); InvalidateLayout(); RequestRepaint(); }
        float GetButtonWidth() const { return width_ > 0.0f ? width_ : DefaultWidth; }
        bool HasCustomWidth() const { return width_ > 0.0f; }

        bool IsFocusable() const override { return false; }   // 不参与 Tab 焦点

        Size MeasureOverride(const Size& availableSize) override {
            float h = height_ > 0 ? height_ : (availableSize.height != FLT_MAX ? availableSize.height : 32.0f);
            return Size(GetButtonWidth(), h);
        }

        void UpdateAnimation(float deltaTime) override {
            bool anim = false;
            float targetHover = hovered_ ? 1.0f : 0.0f;
            float targetPress = pressed_ ? 1.0f : 0.0f;
            if (hoverProgress_ != targetHover) {
                hoverProgress_ += (targetHover - hoverProgress_) * (1.0f - exp(-deltaTime * animSpeed_));
                if (fabs(hoverProgress_ - targetHover) < 0.002f) hoverProgress_ = targetHover;
                anim = true;
            }
            if (pressProgress_ != targetPress) {
                pressProgress_ += (targetPress - pressProgress_) * (1.0f - exp(-deltaTime * animSpeed_));
                if (fabs(pressProgress_ - targetPress) < 0.002f) pressProgress_ = targetPress;
                anim = true;
            }
            if (anim) RequestRepaint();
        }
        bool HasActiveAnimation() const override {
            return hoverProgress_ != (hovered_ ? 1.0f : 0.0f) || pressProgress_ != (pressed_ ? 1.0f : 0.0f);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            bool isClose = (kind_ == Kind::Close);
            bool enabled = IsEffectivelyEnabled();
            // 背景：透明 → hover 色 → pressed 色，按进度渐变
            float hp = hoverProgress_, pp = pressProgress_;
            D2D1_COLOR_F cHover = (isClose ? closeHoverColor_ : hoverColor_).ToD2D();
            D2D1_COLOR_F cPress = (isClose ? closePressedColor_ : pressedColor_).ToD2D();
            auto mix = [](D2D1_COLOR_F a, D2D1_COLOR_F b, float t) -> D2D1_COLOR_F {
                return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                    a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
            };
            D2D1_COLOR_F bg = mix(cHover, cPress, pp);   // 颜色只在悬停色/按下色之间过渡
            bg.a *= max(hp, pp);                          // 透明度按进度渐入 → 不会经过暗色
            if (bg.a > 0.004f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(bg, bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(bg);
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            // 字形颜色：关闭键在悬停/按下时变白；禁用时置灰
            Color glyphColor = glyphColor_;
            if (isClose && (hp > 0.01f || pp > 0.01f)) glyphColor = Color::FromArgb(255, 255, 255, 255);
            if (kind_ == Kind::Pin && active_ && enabled) glyphColor = pinActiveColor_;
            if (!enabled) glyphColor = Color::FromArgb(255, 150, 150, 150);
            if (!glyphBrush_) rt->CreateSolidColorBrush(glyphColor.ToD2D(), glyphBrush_.GetAddressOf());
            else glyphBrush_->SetColor(glyphColor.ToD2D());
            if (!glyphBrush_) return;

            Window* w = GetWindow();
            bool maxed = w && w->IsMaximizedWindow();
            D2D1_RECT_F r = arrangedRect_.ToD2D();
            wchar_t gbuf[2] = { 0, 0 };
            const wchar_t* glyph = L"\uE921";               // 最小化
            if (kind_ == Kind::MaximizeRestore) glyph = maxed ? L"\uE923" : L"\uE922";  // 还原 / 最大化
            else if (kind_ == Kind::Close) glyph = L"\uE8BB";                            // 关闭
            else if (kind_ == Kind::Pin) { gbuf[0] = glyphCode_ ? glyphCode_ : L'\uE718'; glyph = gbuf; }  // 图钉

            IDWriteTextFormat* fmt = detail_wintool::IconFormat(10.0f);
            if (fmt) rt->DrawText(glyph, 1, fmt, r, glyphBrush_.Get());
            else DrawVectorGlyph(rt, r, maxed);
        }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); glyphBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

        void OnMouseEnter() override { if (!IsEffectivelyEnabled()) return; hovered_ = true; pressed_ = false; RequestRepaint(); }
        void OnMouseLeave() override { hovered_ = false; pressed_ = false; RequestRepaint(); }   // 必须清 pressed_，否则按下后移出会把按下动画卡死
        void OnMouseDown(float, float) override { if (!IsEffectivelyEnabled()) return; pressed_ = true; RequestRepaint(); }
        void OnMouseUp(float, float) override {
            if (!pressed_) return;
            pressed_ = false;
            RequestRepaint();
            if (!IsEffectivelyEnabled()) return;
            Clicked.Fire();
        }
        void SetAnimationSpeed(float s) { animSpeed_ = max(0.1f, s); }
        void SetPressedVisual(bool p) { pressed_ = p; RequestRepaint(); }   // 只改视觉，不触发动作

    private:
        void DrawVectorGlyph(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool maxed) {
            float cx = (r.left + r.right) * 0.5f, cy = (r.top + r.bottom) * 0.5f;
            const float s = 5.0f;
            if (kind_ == Kind::Minimize) {
                rt->DrawLine(D2D1::Point2F(cx - s, cy), D2D1::Point2F(cx + s, cy), glyphBrush_.Get(), 1.0f);
            } else if (kind_ == Kind::MaximizeRestore) {
                if (maxed) {
                    rt->DrawRectangle(D2D1::RectF(cx - s, cy - s + 2.0f, cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                    rt->DrawRectangle(D2D1::RectF(cx - s + 2.0f, cy - s, cx + s, cy + s - 2.0f), glyphBrush_.Get(), 1.0f);
                } else {
                    rt->DrawRectangle(D2D1::RectF(cx - s, cy - s, cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                }
            } else {
                rt->DrawLine(D2D1::Point2F(cx - s, cy - s), D2D1::Point2F(cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                rt->DrawLine(D2D1::Point2F(cx + s, cy - s), D2D1::Point2F(cx - s, cy + s), glyphBrush_.Get(), 1.0f);
            }
        }

        Kind kind_;
        float width_ = 0.0f;
        bool hovered_ = false;
        bool pressed_ = false;
        float hoverProgress_ = 0.0f;    // hover 渐变进度
        float pressProgress_ = 0.0f;    // 按下渐变进度
        float animSpeed_ = 24.0f;    // 很快，但能看到渐变
        Color hoverColor_ = DefaultHoverColor;
        Color pressedColor_ = DefaultPressedColor;
        Color closeHoverColor_ = DefaultCloseHoverColor;
        Color closePressedColor_ = DefaultClosePressedColor;
        Color glyphColor_ = DefaultGlyphColor;
        wchar_t glyphCode_ = 0;                                     // 自定义字形（0=按 Kind 默认）
        bool active_ = false;                                       // 切换态（Pin 置顶高亮）
        Color pinActiveColor_ = Color::FromArgb(255, 0, 120, 212);
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> glyphBrush_;
    };

    // ------------------------------------------------------------------
    // TitleBar：标题栏基类（图标 + 标题 + 右侧按钮区）
    // ------------------------------------------------------------------
    class TitleBar : public UIElement {
    public:
        TitleBar() { height_ = 32.0f; }

        void SetTitle(const std::wstring& t) { title_ = t; RequestRepaint(); }
        void SetWindowTitle(const std::wstring& t) override { SetTitle(t); }   // 窗口 SetTitle/SetWindowText 时同步
        const std::wstring& GetTitle() const { return title_; }
        void SetIcon(std::shared_ptr<Image> img) { icon_ = std::move(img); RequestRepaint(); }
        // Window::SetAppIcon 统一入口会调这里，把 HICON 转成 Image 显示在标题栏上
        void SetWindowIconFromHICON(HICON h) override { if (h) { SetShowIcon(true); SetIcon(Image::FromHICON(h)); } }

        // 图标上的徽章 / 进度环
        void SetIconBadge(bool on, Color color = Color::FromArgb(255, 220, 40, 40)) { iconBadgeOn_ = on; iconBadgeColor_ = color; RequestRepaint(); }
        void ClearIconBadge() { iconBadgeOn_ = false; RequestRepaint(); }
        void SetIconProgress(float progress) { iconProgress_ = progress; RequestRepaint(); }   // 0~1；<0 清除
        void ClearIconProgress() { iconProgress_ = -1.0f; RequestRepaint(); }
        std::shared_ptr<Image> GetIcon() const { return icon_; }
        void SetIconSize(float w, float h) { iconW_ = w; iconH_ = h; InvalidateLayout(); RequestRepaint(); }
        void SetShowIcon(bool on) { showIcon_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetShowTitle(bool on) { showTitle_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetContentPadding(float left, float right = 8.0f) { padLeft_ = left; padRight_ = right; InvalidateLayout(); RequestRepaint(); }
        void SetBackgroundColor(Color c) { bgColor_ = c; RequestRepaint(); }
        void SetActiveBackgroundColor(Color c) { activeBgColor_ = c; RequestRepaint(); }
        void SetTitleColor(Color c) { titleColor_ = c; RequestRepaint(); }
        void SetActiveTitleColor(Color c) { activeTitleColor_ = c; RequestRepaint(); }
        void SetButtonWidth(float w) { userButtonWidth_ = true; buttonWidth_ = max(0.0f, w); for (auto& b : buttons_) b->SetButtonWidth(w); InvalidateLayout(); RequestRepaint(); }
        void SetButtonHeight(float h) { userButtonHeight_ = true; buttonHeight_ = max(0.0f, h); InvalidateLayout(); RequestRepaint(); }
        // 覆盖右边距（默认 1.0 DIP：按钮右边缘与标题栏右边缘齐平；不跟随 DWM 推算）
        void SetRightMargin(float m) { rightMarginOverride_ = true; rightMargin_ = max(0.0f, m); InvalidateLayout(); RequestRepaint(); }
        void ClearRightMargin() { rightMarginOverride_ = false; InvalidateLayout(); RequestRepaint(); }

        bool IsFocusable() const override { return false; }   // 不参与 Tab 焦点
        bool UseCache() const override { return true; }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& b : buttons_) b->AttachWindowRecursive(w);
            if (w && !connected_) {
                connected_ = true;
                Connect(w->Activated, [this]() { active_ = true; RequestRepaint(); });
                Connect(w->Deactivated, [this]() { active_ = false; RequestRepaint(); });
            }
        }

        Size MeasureOverride(const Size& availableSize) override {
            float w = width_ > 0 ? width_ : (availableSize.width != FLT_MAX ? availableSize.width : 0.0f);
            return Size(w, height_);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);

            // 右上角锚定、按钮右边缘与标题栏右边缘齐平（不做 DWM 推算，避免被阴影/整体窗口干扰）。
            // 需要留边距可 SetRightMargin 覆盖；需要不同尺寸可 SetButtonWidth/Height。
            float rightMargin = rightMarginOverride_ ? rightMargin_ : 1.0f;
            float btnW = userButtonWidth_ ? buttonWidth_ : CaptionButton::DefaultWidth;
            float btnH = (userButtonHeight_ && buttonHeight_ > 0.0f) ? buttonHeight_ : finalRect.height;
            btnH = clamp(btnH, 1.0f, finalRect.height);
            float btnTop = (finalRect.height - btnH) * 0.5f;

            float right = finalRect.x + finalRect.width - rightMargin;
            for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
                auto& b = *it;
                if (!b->IsVisible()) continue;   // 隐藏的按钮不占位
                if (!b->HasCustomWidth()) b->SetButtonWidth(btnW);
                float bw = b->GetButtonWidth();
                b->SetHeight(btnH);
                b->Arrange(Rect(right - bw, finalRect.y + btnTop, bw, btnH));
                right -= bw;
            }
            buttonsTotalWidth_ = (finalRect.x + finalRect.width - rightMargin) - right;
            SetDragRegion(Rect(0.0f, 0.0f, max(0.0f, right - finalRect.x), finalRect.height));
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& b : buttons_) childrenView_.push_back(b.get());
            return childrenView_;
        }
        UIElement* HitTest(float x, float y) override {
            for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it)
                if (UIElement* h = (*it)->HitTest(x, y)) return h;
            return UIElement::HitTest(x, y);
        }

        // 把按钮报告为系统按钮码 → 启用系统原生行为（最大化按钮悬浮的 Snap Layouts 等）
        int NonClientHitTest(float x, float y) const override {
            for (auto& b : buttons_) {
                if (!b || !b->IsVisible() || !b->IsEffectivelyEnabled()) continue;
                if (b->GetArrangedRect().Contains(x, y)) {
                    switch (b->GetKind()) {
                    case CaptionButton::Kind::Minimize:        return HTMINBUTTON;
                    case CaptionButton::Kind::MaximizeRestore: return HTMAXBUTTON;
                    case CaptionButton::Kind::Close:           return HTCLOSE;
                    }
                }
            }
            return 0;
        }

        // 标题栏按钮是否可点击（禁用后按钮置灰且不再响应）
        void SetButtonsEnabled(bool on) {
            buttonsEnabled_ = on;
            for (auto& b : buttons_) b->SetEnabled(on);
            RequestRepaint();
        }
        bool AreButtonsEnabled() const { return buttonsEnabled_; }

        // ---- 按按钮类型单独控制（详细禁用 API） ----
        std::shared_ptr<CaptionButton> GetButton(CaptionButton::Kind k) const {
            for (auto& b : buttons_) if (b && b->GetKind() == k) return b;
            return nullptr;
        }
        // 单独禁用/启用某个按钮：置灰、不响应鼠标、不报告系统按钮码
        void SetButtonEnabled(CaptionButton::Kind k, bool on) {
            if (auto b = GetButton(k)) b->SetEnabled(on);
            RequestRepaint();
        }
        bool IsButtonEnabled(CaptionButton::Kind k) const {
            auto b = GetButton(k);
            return b && b->IsEffectivelyEnabled();
        }
        // 单独显示/隐藏某个按钮：隐藏后不再占位（布局自动重排）
        void SetButtonVisible(CaptionButton::Kind k, bool on) {
            if (auto b = GetButton(k)) { b->SetVisible(on); InvalidateLayout(); RequestRepaint(); }
        }
        bool IsButtonVisible(CaptionButton::Kind k) const {
            auto b = GetButton(k);
            return b && b->IsVisible();
        }

        // 追加自定义按钮（如 Pin 置顶切换）：插到最前 → 布局时排在最左（最小化按钮的左边）
        void AddCustomButton(std::shared_ptr<CaptionButton> b) {
            if (!b) return;
            b->SetButtonWidth(buttonWidth_);
            b->SetParent(this);
            buttons_.insert(buttons_.begin(), std::move(b));
            InvalidateLayout(); RequestRepaint();
        }

        bool HasActiveAnimation() const override {
            for (auto& b : buttons_) if (b && b->HasActiveAnimation()) return true;
            return false;
        }

        void SetNonClientButtonPressed(int hit, bool pressed) override {
            for (auto& b : buttons_) {
                if (!b) continue;
                int code = 0;
                switch (b->GetKind()) {
                case CaptionButton::Kind::Minimize:        code = HTMINBUTTON; break;
                case CaptionButton::Kind::MaximizeRestore: code = HTMAXBUTTON; break;
                case CaptionButton::Kind::Close:           code = HTCLOSE; break;
                }
                if (code == hit) b->SetPressedVisual(pressed);
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            Color bg = active_ ? activeBgColor_ : bgColor_;
            if (bg.a > 0.0f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(bg.ToD2D(), bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(bg.ToD2D());
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            float x = arrangedRect_.x + padLeft_;
            float cy = arrangedRect_.y + arrangedRect_.height * 0.5f;
            if (showIcon_ && icon_ && !icon_->IsNull()) {
                float iw = iconW_ > 0 ? iconW_ : (float)icon_->Width();
                float ih = iconH_ > 0 ? iconH_ : (float)icon_->Height();
                D2D1_RECT_F ir = D2D1::RectF(x, cy - ih * 0.5f, x + iw, cy + ih * 0.5f);
                icon_->Draw(rt, ir);
                float icx = (ir.left + ir.right) * 0.5f, icy = (ir.top + ir.bottom) * 0.5f;

                // 进度环（iconProgress_ ∈ [0,1]；<0 表示不显示）
                if (iconProgress_ >= 0.0f) {
                    float rr = max(iw, ih) * 0.5f + 2.0f;
                    if (!ringBgBrush_) rt->CreateSolidColorBrush(iconProgressBgColor_.ToD2D(), ringBgBrush_.GetAddressOf());
                    else ringBgBrush_->SetColor(iconProgressBgColor_.ToD2D());
                    if (ringBgBrush_) rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(icx, icy), rr, rr), ringBgBrush_.Get(), 2.0f);
                    if (!ringBrush_) rt->CreateSolidColorBrush(iconProgressColor_.ToD2D(), ringBrush_.GetAddressOf());
                    else ringBrush_->SetColor(iconProgressColor_.ToD2D());
                    if (ringBrush_) {
                        float p = clamp(iconProgress_, 0.0f, 1.0f);
                        if (p > 0.0f) {
                            const float PI = 3.14159265f;
                            int seg = max(2, (int)(p * 64.0f));
                            float a0 = -PI * 0.5f;
                            D2D1_POINT_2F prev = D2D1::Point2F(icx + rr * cosf(a0), icy + rr * sinf(a0));
                            for (int i = 1; i <= seg; ++i) {
                                float a = a0 + 2.0f * PI * p * (float)i / (float)seg;
                                D2D1_POINT_2F cur = D2D1::Point2F(icx + rr * cosf(a), icy + rr * sinf(a));
                                rt->DrawLine(prev, cur, ringBrush_.Get(), 2.0f);
                                prev = cur;
                            }
                        }
                    }
                }
                // 徽章红点（右下角）
                if (iconBadgeOn_) {
                    if (!badgeBrush_) rt->CreateSolidColorBrush(iconBadgeColor_.ToD2D(), badgeBrush_.GetAddressOf());
                    else badgeBrush_->SetColor(iconBadgeColor_.ToD2D());
                    float r = max(4.0f, iw * 0.28f);
                    if (badgeBrush_) rt->FillEllipse(
                        D2D1::Ellipse(D2D1::Point2F(ir.right - r * 0.4f, ir.bottom - r * 0.4f), r, r), badgeBrush_.Get());
                }
                x += iw + 8.0f;
            }
            if (showTitle_ && !title_.empty()) {
                IDWriteTextFormat* fmt = GetFontFormat();
                IDWriteFactory* dw = FontManager::Instance().GetFactory();
                if (fmt && dw) {
                    Color tc = active_ ? activeTitleColor_ : titleColor_;
                    if (!textBrush_) rt->CreateSolidColorBrush(tc.ToD2D(), textBrush_.GetAddressOf());
                    else textBrush_->SetColor(tc.ToD2D());
                    float textRight = arrangedRect_.x + arrangedRect_.width - buttonsTotalWidth_ - padRight_;
                    D2D1_RECT_F tr = D2D1::RectF(x, arrangedRect_.y, textRight, arrangedRect_.y + arrangedRect_.height);
                    if (tr.right > tr.left && textBrush_) {
                        ComPtr<IDWriteTextLayout> layout;
                        dw->CreateTextLayout(title_.c_str(), (UINT32)title_.length(), fmt,
                            tr.right - tr.left, tr.bottom - tr.top, &layout);
                        if (layout) {
                            layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                            layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                            layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                            ComPtr<IDWriteInlineObject> ellipsis;
                            if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(fmt, &ellipsis))) {
                                DWRITE_TRIMMING tm = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                                layout->SetTrimming(&tm, ellipsis.Get());
                            }
                            rt->DrawTextLayout(D2D1::Point2F(tr.left, tr.top), layout.Get(), textBrush_.Get());
                        }
                    }
                }
            }
        }
        void UpdateAnimation(float deltaTime) override { for (auto& b : buttons_) b->UpdateAnimation(deltaTime); }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); textBrush_.Reset();
            badgeBrush_.Reset(); ringBrush_.Reset(); ringBgBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    protected:
        void AddButton(std::shared_ptr<CaptionButton> b) {
            if (!b) return;
            b->SetButtonWidth(buttonWidth_);
            b->SetParent(this);
            buttons_.push_back(std::move(b));
            InvalidateLayout();
            RequestRepaint();
        }

        std::wstring title_;
        std::shared_ptr<Image> icon_;
        bool iconBadgeOn_ = false;
        Color iconBadgeColor_ = Color::FromArgb(255, 220, 40, 40);
        float iconProgress_ = -1.0f;
        Color iconProgressColor_ = Color::FromArgb(255, 0, 120, 212);
        Color iconProgressBgColor_ = Color::FromArgb(60, 0, 0, 0);
        bool showIcon_ = true;
        bool showTitle_ = true;
        float iconW_ = 16.0f, iconH_ = 16.0f;
        float padLeft_ = 12.0f, padRight_ = 8.0f;
        float buttonWidth_ = CaptionButton::DefaultWidth;
        bool userButtonWidth_ = false;
        bool userButtonHeight_ = false;
        float buttonHeight_ = 0.0f;
        bool rightMarginOverride_ = false;
        float rightMargin_ = 0.0f;
        float buttonsTotalWidth_ = 0.0f;
        bool active_ = true;
        bool connected_ = false;
        bool buttonsEnabled_ = true;
        Color bgColor_ = Color::FromArgb(0, 0, 0, 0);
        Color activeBgColor_ = Color::FromArgb(0, 0, 0, 0);
        Color titleColor_ = Color::FromArgb(255, 70, 70, 70);
        Color activeTitleColor_ = Color::FromArgb(255, 26, 26, 26);
        std::vector<std::shared_ptr<CaptionButton>> buttons_;
        mutable std::vector<UIElement*> childrenView_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> badgeBrush_, ringBrush_, ringBgBrush_;
    };

    // ------------------------------------------------------------------
    // DefaultTitleBar：内置三件套 + 图标 + 标题
    // ------------------------------------------------------------------
    class DefaultTitleBar : public TitleBar {
    public:
        DefaultTitleBar() {
            minBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::Minimize);
            maxBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::MaximizeRestore);
            closeBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::Close);
            // 右起顺序：关闭在最右，然后最大化还原，然后最小化
            AddButton(minBtn_);
            AddButton(maxBtn_);
            AddButton(closeBtn_);
            // 默认行为在这里接线（按钮本身只发 Clicked；自定义标题栏可自行改写/拦截）
            Connect(minBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->Minimize(); });
            Connect(maxBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->MaximizeRestore(); });
            Connect(closeBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->Close(); });
        }
        std::shared_ptr<CaptionButton> GetMinButton() const { return minBtn_; }
        std::shared_ptr<CaptionButton> GetMaxButton() const { return maxBtn_; }
        std::shared_ptr<CaptionButton> GetCloseButton() const { return closeBtn_; }

        // 便捷：单独启用/禁用、显示/隐藏三件套
        void SetMinimizeEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::Minimize, on); }
        void SetMaximizeEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::MaximizeRestore, on); }
        void SetCloseEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::Close, on); }
        void SetMinimizeVisible(bool on) { SetButtonVisible(CaptionButton::Kind::Minimize, on); }
        void SetMaximizeVisible(bool on) { SetButtonVisible(CaptionButton::Kind::MaximizeRestore, on); }
        void SetCloseVisible(bool on) { SetButtonVisible(CaptionButton::Kind::Close, on); }

    private:
        std::shared_ptr<CaptionButton> minBtn_, maxBtn_, closeBtn_;
    };

    // ============================================================================
    // FastButton —— 预设按钮标识符（每个占一个二进制位，用 | 组合）
    //   MessageBox box(parent, title, content, icon, FastButton::Yes | FastButton::No | FastButton::Cancel);
    //   if (HasFlag(box.GetResult(), FastButton::Yes)) { ... }
    // ============================================================================
    enum class FastButton : unsigned {
        None   = 0u,
        OK     = 1u << 0,
        Cancel = 1u << 1,
        Apply  = 1u << 2,
        Close  = 1u << 3,
        Yes    = 1u << 4,
        No     = 1u << 5,
        Help   = 1u << 6,
    };
    inline FastButton operator|(FastButton a, FastButton b) { return (FastButton)((unsigned)a | (unsigned)b); }
    inline FastButton operator&(FastButton a, FastButton b) { return (FastButton)((unsigned)a & (unsigned)b); }
    inline FastButton& operator|=(FastButton& a, FastButton b) { a = a | b; return a; }
    inline bool HasFlag(FastButton set, FastButton flag) { return ((unsigned)set & (unsigned)flag) != 0u; }

    // ============================================================================
    // MessageBox —— 预设消息框（快速调用）+ 完全自定义。复用 Window / RunModal，不另起机制。
    //
    // 快速调用（构造即建窗；blocking=true 时构造内直接跑模态循环）：
    //   ZufyUI::MessageBox box(parent, L"标题", L"正文", MessageBox::Icon::Info,
    //                       FastButton::Yes | FastButton::No | FastButton::Cancel);
    //   FastButton r = box.GetResult();
    //
    //   * parent 可为 ZufyUI::Window* / HWND / 省略（不继承、无父）
    //   * content 可为文字，也可直接传控件（Label/TextBox/GridLayout…）
    //   * icon 为内置图标标识符；也可 SetIconImage(图片对象)
    //   * 按钮文本预置中文+英文，默认英文；SetLanguage 切换；SetButtonText 单独改
    //   * Enter 选最左侧按钮；ESC 选 Cancel/Close（没有则最左）
    //   * SetUserContent 后不再放任何按钮，全部交给用户（自己加按钮 / 自己 EndDialog）
    // ============================================================================
    class MessageBox : public Window {
    public:
        enum class Icon { None, Info, Warning, Error, Question };
        enum class Lang { English, Chinese };

        // ---------- 快速调用 ----------
        MessageBox(Window* parent, const std::wstring& title, const std::wstring& content,
                   Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true)
            : blocking_(blocking) { Init(parent ? parent->GetHwnd() : nullptr, parent, title, content, icon, buttons); }
        MessageBox(HWND parent, const std::wstring& title, const std::wstring& content,
                   Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true)
            : blocking_(blocking) { Init(parent, nullptr, title, content, icon, buttons); }
        MessageBox(const std::wstring& title, const std::wstring& content,
                   Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true)
            : blocking_(blocking) { Init(nullptr, nullptr, title, content, icon, buttons); }
        // content 直接传控件
        MessageBox(Window* parent, const std::wstring& title, std::shared_ptr<UIElement> content,
                   Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true)
            : blocking_(blocking) { contentEl_ = content; Init(parent ? parent->GetHwnd() : nullptr, parent, title, L"", icon, buttons); }
        MessageBox(HWND parent, const std::wstring& title, std::shared_ptr<UIElement> content,
                   Icon icon = Icon::None, FastButton buttons = FastButton::OK, bool blocking = true)
            : blocking_(blocking) { contentEl_ = content; Init(parent, nullptr, title, L"", icon, buttons); }

        // ---------- 语言 / 文本 ----------
        static void SetLanguage(Lang l) { s_lang_ = l; }
        static Lang GetLanguage() { return s_lang_; }
        // 预设按钮的显示文本（按当前语言）
        static std::wstring ButtonLabel(FastButton b) { return TextOf(b, s_lang_); }
        // 单独改某个按钮的文本
        void SetButtonText(FastButton which, const std::wstring& text) { customText_[KeyOf(which)] = text; }

        // ---------- 完全自定义：调用后不放任何按钮，一切交给用户 ----------
        void SetUserContent(std::shared_ptr<UIElement> content) {
            userContent_ = content;
            if (!built_) return;
            auto root = GetRootColumnBox();
            if (!root) return;
            root->ClearChildren();
            if (content) root->AddChild(content);
            buttonsView_.clear();
            buttonRow_.reset();
            textLabel_.reset();
        }
        void SetIconImage(std::shared_ptr<Image> img) {
            iconImage_ = img;
            if (textLabel_) { textLabel_->SetImage(img); if (img) textLabel_->SetIconSize(32.0f, 32.0f); }
        }

        FastButton GetResult() const { return result_; }
        ZSignal<FastButton> ButtonClicked;   // 点了哪个按钮
        void EndDialog(FastButton r) {        // 自定义内容时用户主动结束
            if (done_) return;
            done_ = true;
            result_ = r;
            ButtonClicked.Fire(r);
            Close();
        }

    protected:
        bool OnWindowKeyDown(int vk) override {
            if (vk == VK_RETURN) { if ((unsigned)defaultButton_) EndDialog(defaultButton_); return true; }
            if (vk == VK_ESCAPE) { EndDialog((unsigned)cancelButton_ ? cancelButton_ : defaultButton_); return true; }
            return false;
        }
        // 窗口尺寸变化后，按最终客户区宽重摆按钮（靠右）
        void OnWindowSize() override {
            if (built_ && buttonRow_) RightAlignButtons();
        }

    private:
        static int KeyOf(FastButton b) {
            for (int i = 0; i < 7; ++i) if (((unsigned)b >> i) & 1u) return i;
            return -1;
        }
        static std::wstring TextOf(FastButton b, Lang lang) {
            switch (b) {
            case FastButton::OK:     return lang == Lang::Chinese ? L"确定" : L"OK";
            case FastButton::Cancel: return lang == Lang::Chinese ? L"取消" : L"Cancel";
            case FastButton::Apply:  return lang == Lang::Chinese ? L"应用" : L"Apply";
            case FastButton::Close:  return lang == Lang::Chinese ? L"关闭" : L"Close";
            case FastButton::Yes:    return lang == Lang::Chinese ? L"是" : L"Yes";
            case FastButton::No:     return lang == Lang::Chinese ? L"否" : L"No";
            case FastButton::Help:   return lang == Lang::Chinese ? L"帮助" : L"Help";
            default: return L"?";
            }
        }
        static std::shared_ptr<Image> StockIcon(Icon icon) {
            SHSTOCKICONID sid = (SHSTOCKICONID)0;
            switch (icon) {
            case Icon::Info:     sid = SIID_INFO;    break;
            case Icon::Warning:  sid = SIID_WARNING; break;
            case Icon::Error:    sid = SIID_ERROR;   break;
            case Icon::Question: sid = SIID_HELP;    break;
            default: return nullptr;
            }
            SHSTOCKICONINFO sii = {};
            sii.cbSize = sizeof(sii);
            if (SUCCEEDED(SHGetStockIconInfo(sid, SHGSI_ICON | SHGSI_LARGEICON, &sii)) && sii.hIcon) {
                auto img = Image::FromHICON(sii.hIcon);
                DestroyIcon(sii.hIcon);
                return img;
            }
            return nullptr;
        }

        void Init(HWND parentHwnd, Window* parentWin, const std::wstring& title, const std::wstring& content,
                  Icon icon, FastButton buttons);
        void ApplyButtons();
        void RightAlignButtons();   // 靠右（按真实客户区宽；须在最终窗口尺寸之后调用）

        std::wstring title_, content_;
        Icon iconKind_ = Icon::None;
        FastButton buttons_ = FastButton::OK;
        std::shared_ptr<Label> textLabel_;
        std::shared_ptr<RowBox> buttonRow_;
        std::shared_ptr<UIElement> contentEl_;    // 构造时传入的控件
        std::shared_ptr<UIElement> userContent_;  // SetUserContent
        std::shared_ptr<Image> iconImage_;
        std::vector<std::shared_ptr<Button>> buttonsView_;
        std::unordered_map<int, std::wstring> customText_;
        FastButton result_ = FastButton::None;
        FastButton defaultButton_ = FastButton::None;
        FastButton cancelButton_ = FastButton::None;
        int builtWidth_ = 460;
        bool blocking_ = true;
        bool done_ = false;
        bool built_ = false;

        static inline Lang s_lang_ = Lang::English;
    };

    inline void MessageBox::Init(HWND parentHwnd, Window* parentWin, const std::wstring& title,
                                 const std::wstring& content, Icon icon, FastButton buttons) {
        title_ = title.empty() ? L"Message" : title;
        content_ = content;
        iconKind_ = icon;
        buttons_ = buttons;

        if (parentWin) SetOwner(parentWin);
        if (!Create(builtWidth_, 180, title_)) return;
        SetResizable(false);
        SetWindowStyleFlag(WS_MINIMIZEBOX, false);
        SetWindowStyleFlag(WS_MAXIMIZEBOX, false);
        if (parentHwnd && !parentWin) SetWindowLongPtr(GetHwnd(), GWLP_HWNDPARENT, (LONG_PTR)parentHwnd);

        auto root = GetRootColumnBox();
        if (root) {
            if (userContent_) {
                root->AddChild(userContent_);   // 全交给用户：不放按钮
            }
            else {
                if (contentEl_) {
                    root->AddChild(contentEl_);
                }
                else {
                    auto lbl = std::make_shared<Label>(content_);
                    lbl->SetTextOverflow(Label::TextOverflow::Wrap);
                    auto img = iconImage_ ? iconImage_ : StockIcon(iconKind_);
                    if (img) { lbl->SetImage(img); lbl->SetIconSize(32.0f, 32.0f); }
                    textLabel_ = lbl;
                    root->AddChild(lbl);
                }
                buttonRow_ = std::make_shared<RowBox>();
                buttonRow_->SetSpacing(8.0f);
                root->AddChild(buttonRow_);
                ApplyButtons();

                // 自适应高度：root->Measure() 不含其自身 margin，需手补；宽度用真实客户区
                Thickness rm = root->GetMargin();
                RECT wr0, cr0; GetWindowRect(GetHwnd(), &wr0); GetClientRect(GetHwnd(), &cr0);
                int dpi = (int)GetDpiForWindow(GetHwnd()); if (dpi <= 0) dpi = 96;
                int frameW = (wr0.right - wr0.left) - (cr0.right - cr0.left);
                int frameH = (wr0.bottom - wr0.top) - (cr0.bottom - cr0.top);
                float clientW = (float)(builtWidth_ - MulDiv(frameW, 96, dpi));
                if (clientW < 60.0f) clientW = 60.0f;
                Size want = root->Measure(Size(clientW, 0.0f));
                int clientH = (int)ceil(want.height + rm.top + rm.bottom);
                if (clientH < 90) clientH = 90;
                SetSize(builtWidth_, clientH + MulDiv(frameH, 96, dpi));
                RightAlignButtons();   // 最终尺寸定下来后再摆一次按钮（时机：必须在 SetSize 之后）
            }
        }
        built_ = true;
        Show();
        if (blocking_) RunModal(parentWin);   // 阻塞：构造内跑完模态循环
    }

    inline void MessageBox::ApplyButtons() {
        defaultButton_ = FastButton::None;
        cancelButton_ = FastButton::None;
        buttonsView_.clear();
        if (!buttonRow_) return;
        buttonRow_->ClearChildren();
        // 显示顺序（左→右）：Yes No OK Apply Cancel Close Help —— Enter 选最左侧存在的那个
        static const FastButton kOrder[] = { FastButton::Yes, FastButton::No, FastButton::OK, FastButton::Apply,
                                             FastButton::Cancel, FastButton::Close, FastButton::Help };
        for (FastButton b : kOrder) {
            if (!HasFlag(buttons_, b)) continue;
            int key = KeyOf(b);
            std::wstring text = (customText_.count(key) ? customText_[key] : TextOf(b, s_lang_));
            auto btn = std::make_shared<Button>(text);
            btn->Connect(btn->Clicked, [this, b]() { EndDialog(b); });   // 尺寸用 Button 自己的（默认/用户设置），不写死
            buttonRow_->AddChild(btn);
            buttonsView_.push_back(btn);
            if ((unsigned)defaultButton_ == 0u) defaultButton_ = b;                        // 最左 = Enter 默认
            if (b == FastButton::Cancel || b == FastButton::Close) cancelButton_ = b;      // ESC 默认
        }
        RightAlignButtons();
    }

    // 按钮靠右：RowBox 不分配剩余空间，用左外边距把整排推到右边（按真实客户区宽）
    inline void MessageBox::RightAlignButtons() {
        if (!buttonRow_) return;
        int n = (int)buttonsView_.size();
        if (n <= 0) { buttonRow_->SetMargin(Thickness(0, 0, 0, 0)); return; }
        // 按每个按钮“实际宽度”求和（不写死宽度）：优先已布局宽度，其次期望宽度，最后显式宽度
        float total = 0.0f;
        for (auto& b : buttonsView_) {
            float w = b->GetArrangedRect().width;
            if (w <= 0.0f) w = b->GetDesiredSize().width;
            if (w <= 0.0f) w = b->GetWidth();
            if (w <= 0.0f) w = 100.0f;
            total += w;
        }
        if (n > 1) total += (n - 1) * buttonRow_->GetSpacing();
        float clientW = (float)builtWidth_;
        if (GetHwnd()) {
            RECT cr; GetClientRect(GetHwnd(), &cr);
            int dpi = (int)GetDpiForWindow(GetHwnd()); if (dpi <= 0) dpi = 96;
            if (cr.right > cr.left) clientW = (float)(cr.right - cr.left) * 96.0f / (float)dpi;
        }
        float contentW = clientW;
        if (auto root = GetRootColumnBox()) {
            Thickness m = root->GetMargin();
            contentW = clientW - m.left - m.right;
        }
        float left = contentW - total;
        if (left < 0.0f) left = 0.0f;
        buttonRow_->SetMargin(Thickness(left, 0, 0, 0));
    }

    // ============================================================================
    // TrayIcon —— 托盘图标（Shell_NotifyIcon + NOTIFYICON_VERSION_4）
    //   * v4：支持悬停(NIN_POPUPOPEN/CLOSE)、选择(NIN_SELECT/KEYSELECT)、右键(WM_CONTEXTMENU)
    //   * 菜单：右键弹出（可配合 Menu::ShowAt 独立弹出，不绑定窗口）
    //   * 徽章：把小红点合成到图标右下角
    //   * 气球通知：Win10/11 会自动升级成真正的 toast 通知
    // ============================================================================
    class TrayIcon {
    public:
        TrayIcon() = default;
        ~TrayIcon() { Remove(); }
        TrayIcon(const TrayIcon&) = delete;
        TrayIcon& operator=(const TrayIcon&) = delete;

        bool Add(HICON icon, const std::wstring& tooltip, UINT id = 1) {
            EnsureWindow();
            if (!hwnd_) return false;

            if (added_ && id_ == id) {   // 已加过同 ID：只更新图标/提示，避免重复 NIM_ADD 失败
                baseIcon_ = icon;
                ApplyTip(tooltip);
                InvalidateIcon();
                nid_.hIcon = CurrentIcon();
                Shell_NotifyIconW(NIM_MODIFY, &nid_);
                return true;
            }
            if (added_) {                // 换 ID：先删旧的
                Shell_NotifyIconW(NIM_DELETE, &nid_);
                added_ = false;
            }

            id_ = id;
            baseIcon_ = icon;
            nid_ = {};
            nid_.cbSize = sizeof(nid_);
            nid_.hWnd = hwnd_;
            nid_.uID = id_;
            nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
            nid_.uCallbackMessage = kCallbackMsg;
            nid_.hIcon = CurrentIcon();
            ApplyTip(tooltip);
            if (!Shell_NotifyIconW(NIM_ADD, &nid_)) return false;
            nid_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &nid_);
            added_ = true;
            return true;
        }
        bool AddFromResource(int resId, const std::wstring& tooltip, UINT id = 1) {
            HICON h = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(resId),
                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
            return Add(h, tooltip, id);
        }
        // 直接吃库自带 Image（或任何带 ToHICON() 的图片类型）
        template <class TImage>
        bool Add(std::shared_ptr<TImage> icon, const std::wstring& tooltip, UINT id = 1) {
            HICON h = icon ? icon->ToHICON() : nullptr;
            if (ownedIcon_) { DestroyIcon(ownedIcon_); ownedIcon_ = nullptr; }
            ownedIcon_ = h;   // 由 TrayIcon 负责销毁
            return Add(h, tooltip, id);
        }
        template <class TImage>
        void SetIcon(std::shared_ptr<TImage> icon) { SetIcon(icon ? icon->ToHICON() : nullptr); }
        bool AddFromFile(const std::wstring& icoPath, const std::wstring& tooltip, UINT id = 1) {
            HICON h = (HICON)LoadImageW(nullptr, icoPath.c_str(), IMAGE_ICON, 0, 0,
                LR_LOADFROMFILE | LR_DEFAULTSIZE);
            if (!h) return false;
            ownedIcon_ = h;
            return Add(h, tooltip, id);
        }
        void Remove() {
            if (added_) { Shell_NotifyIconW(NIM_DELETE, &nid_); added_ = false; }
            if (badgeIcon_) { DestroyIcon(badgeIcon_); badgeIcon_ = nullptr; }
            if (ownedIcon_) { DestroyIcon(ownedIcon_); ownedIcon_ = nullptr; }
            if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
        }

        void SetIcon(HICON icon) {
            baseIcon_ = icon;
            InvalidateIcon();
            if (added_) { nid_.hIcon = CurrentIcon(); Shell_NotifyIconW(NIM_MODIFY, &nid_); }
        }
        void SetToolTip(const std::wstring& tip) { currentTip_ = tip; ApplyTip(tip); if (added_) Shell_NotifyIconW(NIM_MODIFY, &nid_); }
        void SetMenu(std::shared_ptr<Menu> m) { menu_ = m; }

        // 徽章：右下角小红点
        void SetBadge(Color color = Color::FromArgb(255, 220, 40, 40)) {
            badgeColor_ = color;
            badgeOn_ = true;
            InvalidateIcon();
            if (added_) { nid_.hIcon = CurrentIcon(); Shell_NotifyIconW(NIM_MODIFY, &nid_); }
        }
        void ClearBadge() {
            badgeOn_ = false;
            if (badgeIcon_) { DestroyIcon(badgeIcon_); badgeIcon_ = nullptr; }
            iconDirty_ = false;
            if (added_) { nid_.hIcon = CurrentIcon(); Shell_NotifyIconW(NIM_MODIFY, &nid_); }
        }

        // 气球/toast 的主图标（对应 NOTIFYICONDATA.dwInfoFlags）
        enum class BalloonIcon : DWORD {
            None    = NIIF_NONE,      // 不显示图标
            Info    = NIIF_INFO,      // 信息
            Warning = NIIF_WARNING,   // 警告（感叹号）
            Error   = NIIF_ERROR,     // 错误（叉）
            Custom  = NIIF_USER,      // 自定义（用 customIcon；缺省用当前托盘图标）
        };
        // 气球通知（v4 下 Win10/11 自动变成 toast）。
        //   icon=Custom 时用 customIcon（缺省=当前托盘图标），并置大图标位；
        //   realtime=true → NIF_REALTIME（不被 toast 接管）；noSound=true → NIF_NOSOUND。
        void ShowBalloon(const std::wstring& title, const std::wstring& text,
                         BalloonIcon icon = BalloonIcon::Custom,
                         HICON customIcon = nullptr,
                         bool realtime = false, bool noSound = false) {
            if (!added_) return;
            NOTIFYICONDATAW n = nid_;
            n.uFlags = NIF_INFO | NIF_ICON | (realtime ? NIF_REALTIME : 0);
            n.hIcon = CurrentIcon();
            n.dwInfoFlags = (DWORD)icon;
            if (noSound) n.dwInfoFlags |= NIIF_NOSOUND;
            if (icon == BalloonIcon::Custom) {
                n.hBalloonIcon = customIcon ? customIcon : CurrentIcon();
                n.dwInfoFlags |= NIIF_LARGE_ICON;
            }
            wcsncpy_s(n.szInfoTitle, title.c_str(), _TRUNCATE);
            wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
            Shell_NotifyIconW(NIM_MODIFY, &n);
        }

        bool GetRect(RECT& out) const {
            if (!added_) return false;
            NOTIFYICONIDENTIFIER nid = {};
            nid.cbSize = sizeof(nid);
            nid.hWnd = nid_.hWnd;
            nid.uID = nid_.uID;
            return SUCCEEDED(Shell_NotifyIconGetRect(&nid, &out));
        }

        bool IsAdded() const { return added_; }
        HWND GetHwnd() const { return hwnd_; }

        // 信号
        ZSignal<> Clicked;
        ZSignal<> DoubleClicked;
        ZSignal<> RightClicked;
        ZSignal<> HoverEnter;
        ZSignal<> HoverLeave;
        ZSignal<> Selected;        // 键盘选中
        ZSignal<> BalloonClicked;
        ZSignal<> BalloonDismissed;
        ZSignal<> BalloonTimeout;

    private:
        static constexpr UINT kCallbackMsg = WM_APP + 0x400;   // 远离库内部消息号（如 WM_RENDER_TICK=WM_APP+2），避免撞号被误当回调用

        void ApplyTip(const std::wstring& tip) {
            currentTip_ = tip;
            wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
        }
        void ShowMenuAtCursor() {
            if (!menu_) {
#ifdef ZufyUI_DEBUG
                ZufyUI_DEBUG_LOG_W(L"[ZufyUI] Tray ShowMenuAtCursor: menu_ is null (SetMenu not called?)\n");
#endif
                return;
            }
            POINT pt;
            GetCursorPos(&pt);
            menu_->ShowAt(pt.x, pt.y);
#ifdef ZufyUI_DEBUG
            {
                wchar_t b[128];
                swprintf(b, 128, L"[ZufyUI] Tray ShowMenuAtCursor: ShowAt at %d,%d\n", (int)pt.x, (int)pt.y);
                ZufyUI_DEBUG_LOG_W(b);
            }
#endif
        }
        HICON CurrentIcon() {
            if (!badgeOn_) return baseIcon_;
            if (iconDirty_ || !badgeIcon_) {
                if (badgeIcon_) { DestroyIcon(badgeIcon_); badgeIcon_ = nullptr; }
                badgeIcon_ = MakeBadgeIcon(baseIcon_, badgeColor_);
                iconDirty_ = false;
            }
            return badgeIcon_ ? badgeIcon_ : baseIcon_;
        }
        void InvalidateIcon() { iconDirty_ = true; }

        // 右下角红点徽章：取原图 32bpp 像素后手动画圆并写 alpha（GDI 画法写不进 alpha）
        static HICON MakeBadgeIcon(HICON base, Color color) {
            if (!base) return nullptr;
            ICONINFO ii = {};
            if (!GetIconInfo(base, &ii)) return nullptr;
            BITMAP bm = {};
            if (!ii.hbmColor || !GetObject(ii.hbmColor, sizeof(bm), &bm)) {
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                return nullptr;
            }
            int w = bm.bmWidth, h = bm.bmHeight;
            if (w <= 0 || h <= 0) {
                DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask);
                return nullptr;
            }

            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;

            HDC screen = GetDC(nullptr);
            if (!screen) {   // 先检查，避免把 NULL 传给 GetDIBits
                DeleteObject(ii.hbmColor);
                DeleteObject(ii.hbmMask);
                return nullptr;
            }
            std::vector<uint32_t> px((size_t)w * h, 0);
            GetDIBits(screen, ii.hbmColor, 0, h, px.data(), &bi, DIB_RGB_COLORS);
            DeleteObject(ii.hbmColor);
            DeleteObject(ii.hbmMask);

            int d = w * 3 / 5; if (d < 8) d = 8;
            float cx = (float)w - d * 0.5f - 1.0f;
            float cy = (float)h - d * 0.5f - 1.0f;
            float r = d * 0.5f;
            int R = (int)(clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
            int G = (int)(clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
            int B = (int)(clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    float dx = (float)x - cx, dy = (float)y - cy;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist > r + 1.5f) continue;
                    float a = (r + 1.5f - dist) / 1.5f;   // 边缘 1.5px 软化
                    if (a > 1.0f) a = 1.0f; if (a < 0.0f) a = 0.0f;
                    int A = (int)(a * 255.0f + 0.5f);
                    // 预乘
                    int pr = R * A / 255, pg = G * A / 255, pb = B * A / 255;
                    px[(size_t)y * w + x] = ((uint32_t)A << 24) | ((uint32_t)pr << 16) | ((uint32_t)pg << 8) | (uint32_t)pb;
                }
            }

            void* bits = nullptr;
            HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
            HBITMAP mask = CreateBitmap(w, h, 1, 1, nullptr);
            HICON out = nullptr;
            if (dib && bits) {
                memcpy(bits, px.data(), (size_t)w * h * 4);
                ICONINFO ni = {};
                ni.fIcon = TRUE;
                ni.hbmColor = dib;
                ni.hbmMask = mask;
                out = CreateIconIndirect(&ni);
            }
            if (mask) DeleteObject(mask);
            if (dib) DeleteObject(dib);
            ReleaseDC(nullptr, screen);
            return out;
        }

        void EnsureWindow() {
            if (hwnd_) return;
            static bool classRegistered = false;
            if (!classRegistered) {
                WNDCLASSEXW wc = {};
                wc.cbSize = sizeof(WNDCLASSEXW);
                wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
                    TrayIcon* self = (TrayIcon*)GetWindowLongPtrW(h, GWLP_USERDATA);
                    if (m == WM_NCCREATE) {
                        CREATESTRUCTW* cs = (CREATESTRUCTW*)l;
                        self = (TrayIcon*)cs->lpCreateParams;
                        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
                    }
                    if (self) return self->Handle(h, m, w, l);
                    return DefWindowProcW(h, m, w, l);
                    };
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = L"ZufyUI_TrayIconWindow";
                RegisterClassExW(&wc);
                classRegistered = true;
            }
            taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
            hwnd_ = CreateWindowExW(0, L"ZufyUI_TrayIconWindow", L"", WS_POPUP,
                0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), this);
        }

        LRESULT Handle(HWND h, UINT msg, WPARAM w, LPARAM l) {
            if (msg == taskbarCreatedMsg_) {   // explorer 重启后自动重加
                if (added_) {
                    Shell_NotifyIconW(NIM_ADD, &nid_);
                    nid_.uVersion = NOTIFYICON_VERSION_4;
                    Shell_NotifyIconW(NIM_SETVERSION, &nid_);
                }
                return 0;
            }
            if (msg == kCallbackMsg) {
                UINT ev = LOWORD(l);
#ifdef ZufyUI_DEBUG
                { wchar_t b[64]; swprintf(b, 64, L"[ZufyUI] Tray callback ev=0x%04X\n", (unsigned)ev); ZufyUI_DEBUG_LOG_W(b); }
#endif
                switch (ev) {
                case NIN_SELECT: {   // v4：左键单击；双击系统不再单发 WM_LBUTTONDBLCLK，需要自己按时间判定
                    DWORD now = GetTickCount();
                    if (lastClickTick_ != 0 && now - lastClickTick_ < GetDoubleClickTime()) {
                        lastClickTick_ = 0;
                        DoubleClicked.Fire();
                    }
                    else {
                        lastClickTick_ = now;
                        Clicked.Fire();
                    }
                    break;
                }
                case NIN_KEYSELECT: Selected.Fire(); break;
                case NIN_POPUPOPEN: HoverEnter.Fire(); break;
                case NIN_POPUPCLOSE: HoverLeave.Fire(); break;
                case NIN_BALLOONUSERCLICK: BalloonClicked.Fire(); break;
                case NIN_BALLOONHIDE: BalloonDismissed.Fire(); break;
                case NIN_BALLOONTIMEOUT: BalloonTimeout.Fire(); break;
                case WM_CONTEXTMENU:      // v4：右键菜单以“回调消息的 lParam”形式发来（不是常规窗口消息）
                    RightClicked.Fire();
                    ShowMenuAtCursor();
                    break;
                default: break;
                }
                return 0;
            }
            if (msg == WM_CONTEXTMENU) {       // 键盘选择菜单等场景仍是常规 WM_CONTEXTMENU
                RightClicked.Fire();
                ShowMenuAtCursor();
                return 0;
            }
            if (msg == WM_DPICHANGED) {   // DPI 变化：重建徽章图标（尺寸相关）
                InvalidateIcon();
                if (added_) { nid_.hIcon = CurrentIcon(); Shell_NotifyIconW(NIM_MODIFY, &nid_); }
                return 0;
            }
            if (msg == WM_SETFOCUS) {     // 焦点进入托盘：通知 Shell（辅助功能）
                if (added_) Shell_NotifyIconW(NIM_SETFOCUS, &nid_);
                return 0;
            }
            return DefWindowProcW(h, msg, w, l);
        }

        HWND hwnd_ = nullptr;
        NOTIFYICONDATAW nid_ = {};
        bool added_ = false;
        UINT id_ = 1;
        UINT taskbarCreatedMsg_ = 0;
        HICON baseIcon_ = nullptr;
        HICON badgeIcon_ = nullptr;      // 带徽章的合成图标（我们自己管生命周期）
        HICON ownedIcon_ = nullptr;      // AddFromFile 加载的图标
        bool badgeOn_ = false;
        bool iconDirty_ = false;         // badge 图标懒重建标志
        DWORD lastClickTick_ = 0;        // v4 双击判定（v4 不再发 WM_LBUTTONDBLCLK）
        Color badgeColor_ = Color::FromArgb(255, 220, 40, 40);
        std::wstring currentTip_;
        std::shared_ptr<Menu> menu_;
    };

    // ============================================================================
    // AppRegistration —— 应用身份(AUMID)自注册（默认关闭，需显式授权）
    // ----------------------------------------------------------------------------
    // 创建窗口前调用一次 RegisterApp(...)，之后：
    //   * 设置进程级 AppUserModelID → 跳转列表 / 任务栏分组 / toast 归属统一用它
    //   * 【授权后】把图标缓存到 %LOCALAPPDATA%\ZufyUI\AppReg\<AUMID>\app.ico，
    //     并写注册表 HKCU\Software\Classes\AppUserModelId\<AUMID> 的
    //     DisplayName + IconUri → Win10/11 的 toast 左上角显示应用名与图标
    //   * 进程退出时清空该缓存目录，并撤销上面写的注册表项（避免悬空 IconUri）
    //
    // 授权：在包含本库头文件之前 #define ZUFYUI_ALLOW_APP_REGISTRATION
    // 未授权：只设置进程 AUMID，不写注册表、不落文件（零副作用）。
    // ============================================================================
    struct AppInfo {
        std::wstring displayName;       // 显示名称（toast / 跳转列表 / 任务栏分组）
        std::wstring aumid;             // 唯一 ID；留空=由 displayName 自动派生
        std::shared_ptr<Image> icon;    // 图标（可空）
    };

    namespace detail {
        inline std::wstring AppRegSanitize(const std::wstring& s) {
            std::wstring o;
            for (wchar_t c : s) {
                if ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') ||
                    (c >= L'a' && c <= L'z') || c == L'.' || c == L'-' || c == L'_')
                    o.push_back(c);
                else if (c == L' ' || c == L'\t')
                    o.push_back(L'.');
            }
            return o.empty() ? std::wstring(L"App") : o;
        }
        inline std::wstring AppRegDeriveAumid(const std::wstring& name) {
            std::wstring s = AppRegSanitize(name);
            if (s.find(L'.') == std::wstring::npos) s = L"ZufyUI." + s;
            if (s.size() > 128) s.resize(128);
            return s;
        }
        inline std::wstring AppRegRootDir() {
            wchar_t buf[MAX_PATH] = {};
            DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
            std::wstring p = (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : std::wstring(L".");
            return p + L"\\ZufyUI\\AppReg";
        }
        // 写单图 .ico（32bpp BGRA，直通 alpha，自下而上；AND 掩码全 0，透明由 alpha 决定）
        inline bool AppRegWriteIco(const std::wstring& path, const std::vector<uint8_t>& bgra, int w, int h) {
            if (w <= 0 || h <= 0 || bgra.size() < (size_t)w * h * 4) return false;
            const DWORD xorStride = (DWORD)w * 4;
            const DWORD andStride = (DWORD)(((w + 31) / 32) * 4);
            const DWORD xorSize = xorStride * (DWORD)h;
            const DWORD andSize = andStride * (DWORD)h;
            const DWORD imgSize = 40 + xorSize + andSize;

            HANDLE fh = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (fh == INVALID_HANDLE_VALUE) return false;
            auto wr = [&](const void* d, DWORD n) { DWORD g = 0; return WriteFile(fh, d, n, &g, nullptr) && g == n; };

            BYTE dir[22] = {};
            dir[2] = 1; dir[4] = 1;                              // type=icon, count=1
            dir[6] = (BYTE)(w >= 256 ? 0 : w);
            dir[7] = (BYTE)(h >= 256 ? 0 : h);
            dir[10] = 1; dir[12] = 32;                           // planes=1, bpp=32
            *(DWORD*)&dir[14] = imgSize;
            *(DWORD*)&dir[18] = 22;                              // offset
            bool ok = wr(dir, sizeof(dir));

            BYTE bih[40] = {};
            *(DWORD*)&bih[0] = 40;
            *(LONG*)&bih[4] = w;
            *(LONG*)&bih[8] = h * 2;                             // XOR + AND
            *(WORD*)&bih[12] = 1;
            *(WORD*)&bih[14] = 32;
            *(DWORD*)&bih[20] = xorSize + andSize;
            if (ok) ok = wr(bih, sizeof(bih));

            for (int y = h - 1; y >= 0 && ok; --y)               // XOR：自下而上
                ok = wr(bgra.data() + (size_t)y * xorStride, xorStride);
            if (ok) {
                std::vector<BYTE> arow(andStride, 0);
                for (int y = 0; y < h && ok; ++y) ok = wr(arow.data(), andStride);
            }
            CloseHandle(fh);
            if (!ok) DeleteFileW(path.c_str());
            return ok;
        }
        inline void AppRegRemoveDir(const std::wstring& dir) {
            if (dir.empty()) return;
            WIN32_FIND_DATAW fd{};
            HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
                        DeleteFileW((dir + L"\\" + fd.cFileName).c_str());
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            RemoveDirectoryW(dir.c_str());
        }
        struct AppRegState {
            std::wstring aumid;
            std::wstring cacheDir;
            bool registryWritten = false;
            ~AppRegState() {
#ifdef ZUFYUI_ALLOW_APP_REGISTRATION
                if (registryWritten && !aumid.empty())
                    RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\AppUserModelId\\" + aumid).c_str());
                AppRegRemoveDir(cacheDir);
#endif
            }
        };
        inline AppRegState& AppReg() { static AppRegState s; return s; }
    } // namespace detail

    // 注册应用身份；返回是否完成了完整注册（未授权宏时返回 false，但仍设置了进程 AUMID）
    inline bool RegisterApp(const AppInfo& info) {
        std::wstring aumid = info.aumid.empty() ? detail::AppRegDeriveAumid(info.displayName) : info.aumid;
        if (!aumid.empty()) SetCurrentProcessExplicitAppUserModelID(aumid.c_str());
        detail::AppReg().aumid = aumid;

        // 应用图标：设为「全局默认」→ 之后创建的所有窗口自动 SetAppIcon（一个入口统一设置名称与图标）
        if (info.icon && !info.icon->IsNull())
            Window::SetDefaultAppIcon(info.icon->ToHICON());

#ifdef ZUFYUI_ALLOW_APP_REGISTRATION
        if (aumid.empty()) return false;
        std::wstring dir = detail::AppRegRootDir() + L"\\" + detail::AppRegSanitize(aumid);
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        detail::AppReg().cacheDir = dir;
        std::wstring icoPath = dir + L"\\app.ico";

        if (info.icon && !info.icon->IsNull()) {
            std::vector<uint8_t> px; int w = 0, h = 0;
            if (info.icon->CopyPixelsBgra(px, w, h))
                detail::AppRegWriteIco(icoPath, px, w, h);
        }

        HKEY hk = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, (L"Software\\Classes\\AppUserModelId\\" + aumid).c_str(),
                0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
            auto setSz = [&](const wchar_t* nm, const std::wstring& v) {
                RegSetValueExW(hk, nm, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
            };
            if (!info.displayName.empty()) setSz(L"DisplayName", info.displayName);
            if (GetFileAttributesW(icoPath.c_str()) != INVALID_FILE_ATTRIBUTES) setSz(L"IconUri", icoPath);
            RegCloseKey(hk);
            detail::AppReg().registryWritten = true;
        }
        return true;
#else
        (void)info;
        return false;   // 未授权：只设置了 AUMID
#endif
    }

    // Application 转发到自由函数（定义在 ZufyUI.h 的 Application 类里声明）
    inline bool Application::RegisterApp(const AppInfo& info) { return ZufyUI::RegisterApp(info); }

    // ============================================================================
    // 系统对话框：文件/文件夹选择 + 颜色选择
    // ----------------------------------------------------------------------------
    // 文件/文件夹：新版 COM 接口 IFileOpenDialog / IFileSaveDialog（替代过时的
    //   GetOpenFileName/GetSaveFileName），支持：文件类型过滤(FileFilter，多组选型)、
    //   多选(FOS_ALLOWMULTISELECT)、选文件夹(FOS_PICKFOLDERS)、另存为(IFileSaveDialog)、
    //   初始目录 / 默认文件名 / 默认扩展名 / 强制文件系统项。
    // 颜色：Windows 通用颜色对话框(ChooseColor)。
    //
    // 用法：
    //   FileDialogOptions o;
    //   o.title = L"选择图片";
    //   o.filters = { { L"图片", { L"*.png", L"*.jpg", L"*.bmp" } }, { L"文本", { L"*.txt" } } };
    //   auto files  = FileDialog::OpenFiles(win.GetHwnd(), o);   // 多选
    //   auto folder = FileDialog::PickFolder(win.GetHwnd());     // 选文件夹
    //   auto color  = ColorDialog::Pick(win.GetHwnd(), Color(1,0,0,1));  // 选颜色
    // ============================================================================
    struct FileFilter {
        std::wstring label;                    // 显示名，如 "图片"
        std::vector<std::wstring> patterns;    // 如 { "*.png", "*.jpg" }
    };

    struct FileDialogOptions {
        std::wstring title;
        std::wstring initialDir;               // 初始目录
        std::wstring defaultFileName;          // 另存为默认文件名
        std::wstring defaultExtension;         // 另存为默认扩展名（如 "png"）
        std::vector<FileFilter> filters;       // 类型选型
        int filterIndex = 1;                   // 1-based，默认选中项
        bool addAllFiles = true;               // 追加"所有文件 (*.*)"
        bool pickFolders = false;              // 选文件夹
        bool multiSelect = false;              // 多选
        bool save = false;                     // 另存为
        bool forceFilesystem = true;           // 只返回真实文件系统项
    };

    class FileDialog {
    public:
        static std::vector<std::wstring> Open(HWND owner, const FileDialogOptions& o = {}) {
            FileDialogOptions x = o; x.save = false; return Run(owner, x);
        }
        // 打开并支持多选（返回全部所选）
        static std::vector<std::wstring> OpenFiles(HWND owner, const FileDialogOptions& o = {}) {
            FileDialogOptions x = o; x.save = false; x.multiSelect = true; return Run(owner, x);
        }
        // 选文件夹（可多选）
        static std::vector<std::wstring> PickFolders(HWND owner, const FileDialogOptions& o = {}) {
            FileDialogOptions x = o; x.save = false; x.pickFolders = true; return Run(owner, x);
        }
        // 另存为
        static std::vector<std::wstring> Save(HWND owner, const FileDialogOptions& o = {}) {
            FileDialogOptions x = o; x.save = true; return Run(owner, x);
        }
        static std::optional<std::wstring> OpenOne(HWND owner, const FileDialogOptions& o = {}) {
            auto v = Open(owner, o); return v.empty() ? std::nullopt : std::optional<std::wstring>(v.front());
        }
        static std::optional<std::wstring> PickFolder(HWND owner, const FileDialogOptions& o = {}) {
            auto v = PickFolders(owner, o); return v.empty() ? std::nullopt : std::optional<std::wstring>(v.front());
        }
        static std::optional<std::wstring> SaveOne(HWND owner, const FileDialogOptions& o = {}) {
            auto v = Save(owner, o); return v.empty() ? std::nullopt : std::optional<std::wstring>(v.front());
        }
        // 给"类型"下拉用的显示名列表（含可选的"所有文件"）
        static std::vector<std::wstring> FilterLabels(const std::vector<FileFilter>& filters, bool addAllFiles = true) {
            std::vector<std::wstring> out;
            for (auto& f : filters) out.push_back(f.label);
            if (addAllFiles) out.push_back(L"所有文件");
            return out;
        }

    private:
        static std::vector<std::wstring> Run(HWND owner, const FileDialogOptions& o) {
            std::vector<std::wstring> result;
            HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            bool needUninit = (hrCo == S_OK);   // S_FALSE=已初始化(勿 Uninit)；RPC_E_CHANGED_MODE=MTA(CoCreate 会失败→返回空)

            ComPtr<IFileOpenDialog> openDlg;
            ComPtr<IFileSaveDialog> saveDlg;
            IFileDialog* dlg = nullptr;
            if (o.save) {
                if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&saveDlg)))) { if (needUninit) ::CoUninitialize(); return result; }
                dlg = saveDlg.Get();
            }
            else {
                if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&openDlg)))) { if (needUninit) ::CoUninitialize(); return result; }
                dlg = openDlg.Get();
            }

            DWORD flags = 0; dlg->GetOptions(&flags);
            if (o.forceFilesystem) flags |= FOS_FORCEFILESYSTEM;
            if (o.pickFolders) flags |= FOS_PICKFOLDERS;
            if (o.multiSelect) flags |= FOS_ALLOWMULTISELECT;
            if (o.save) flags |= FOS_OVERWRITEPROMPT;
            else if (!o.multiSelect) flags |= FOS_FILEMUSTEXIST;
            dlg->SetOptions(flags);

            if (!o.title.empty()) dlg->SetTitle(o.title.c_str());
            if (o.save && !o.defaultFileName.empty()) dlg->SetFileName(o.defaultFileName.c_str());
            if (o.save && !o.defaultExtension.empty()) dlg->SetDefaultExtension(o.defaultExtension.c_str());

            std::vector<std::wstring> names, pats;
            if (!o.filters.empty()) {
                for (auto& f : o.filters) {
                    std::wstring pat;
                    for (size_t i = 0; i < f.patterns.size(); ++i) { if (i) pat += L";"; pat += f.patterns[i]; }
                    names.push_back(f.label); pats.push_back(pat);
                }
                if (o.addAllFiles) { names.push_back(L"所有文件"); pats.push_back(L"*.*"); }
                std::vector<COMDLG_FILTERSPEC> specs; specs.reserve(names.size());
                for (size_t i = 0; i < names.size(); ++i) specs.push_back({ names[i].c_str(), pats[i].c_str() });
                dlg->SetFileTypes((UINT)specs.size(), specs.data());
                int fi = max(1, min((int)specs.size(), o.filterIndex));
                dlg->SetFileTypeIndex((UINT)fi);
            }

            if (!o.initialDir.empty()) {
                ComPtr<IShellItem> folder;
                if (SUCCEEDED(::SHCreateItemFromParsingName(o.initialDir.c_str(), nullptr, IID_PPV_ARGS(&folder))))
                    dlg->SetFolder(folder.Get());
            }

            if (SUCCEEDED(dlg->Show(owner))) {
                if (o.multiSelect && !o.save && openDlg) {
                    ComPtr<IShellItemArray> items;
                    if (SUCCEEDED(openDlg->GetResults(&items)) && items) {
                        DWORD n = 0; items->GetCount(&n);
                        for (DWORD i = 0; i < n; ++i) {
                            ComPtr<IShellItem> it;
                            if (SUCCEEDED(items->GetItemAt(i, &it)) && it) {
                                PWSTR p = nullptr;
                                if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { result.push_back(p); ::CoTaskMemFree(p); }
                            }
                        }
                    }
                }
                else {
                    ComPtr<IShellItem> it;
                    if (o.save && saveDlg) saveDlg->GetResult(&it);
                    else if (openDlg) openDlg->GetResult(&it);
                    if (it) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { result.push_back(p); ::CoTaskMemFree(p); } }
                }
            }
            if (needUninit) ::CoUninitialize();
            return result;
        }
    };

    // ---------- 颜色选择 ----------
    class ColorDialog {
    public:
        struct Options {
            bool fullOpen = true;          // 展开完整调色板
            bool allowCustom = true;       // 允许自定义色槽
            std::vector<Color> custom;     // 预置自定义色（最多 16）
        };
        static std::optional<Color> Pick(HWND owner, Color initial = Color(1, 0, 0, 1), const Options& opt = {}) {
            COLORREF cust[16] = {};
            size_t n = min(opt.custom.size(), (size_t)16);
            for (size_t i = 0; i < n; ++i)
                cust[i] = RGB((int)(opt.custom[i].r * 255 + 0.5f), (int)(opt.custom[i].g * 255 + 0.5f), (int)(opt.custom[i].b * 255 + 0.5f));
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = owner;
            cc.rgbResult = RGB((int)(initial.r * 255 + 0.5f), (int)(initial.g * 255 + 0.5f), (int)(initial.b * 255 + 0.5f));
            cc.lpCustColors = cust;
            cc.Flags = CC_RGBINIT | CC_ANYCOLOR;
            if (opt.fullOpen && opt.allowCustom) cc.Flags |= CC_FULLOPEN;
            if (!opt.allowCustom) cc.Flags |= CC_PREVENTFULLOPEN;
            if (::ChooseColorW(&cc))
                return Color(GetRValue(cc.rgbResult) / 255.0f, GetGValue(cc.rgbResult) / 255.0f, GetBValue(cc.rgbResult) / 255.0f, initial.a);
            return std::nullopt;
        }
    };

    // ============================================================================
    // 无障碍（UI Automation）—— 默认开启（见 Window::SetAccessibilityEnabled）
    // ----------------------------------------------------------------------------
    // 本库**不保存历史、不做文件读写**：provider 只按需返回「当前这一帧」的属性/树。
    //   每个 Window 一个根 provider（IRawElementProviderFragmentRoot）；
    //   每个 UIElement 一个 fragment provider（IRawElementProviderSimple + Fragment）。
    //   屏幕阅读器经 WM_GETOBJECT → UiaReturnRawElementProvider 取根。
    // 里程碑 1：树 + 基础属性 + 命中/焦点；pattern（Invoke/Value/Toggle…）与事件后续补。
    // ============================================================================
    namespace detail {

        inline long UiaControlTypeOf(AccessibleRole r) {
            using R = AccessibleRole;
            switch (r) {
            case R::Button: return UIA_ButtonControlTypeId;
            case R::Text: return UIA_TextControlTypeId;
            case R::Edit: return UIA_EditControlTypeId;
            case R::CheckBox: return UIA_CheckBoxControlTypeId;
            case R::RadioButton: return UIA_RadioButtonControlTypeId;
            case R::ComboBox: return UIA_ComboBoxControlTypeId;
            case R::Slider: return UIA_SliderControlTypeId;
            case R::ProgressBar: return UIA_ProgressBarControlTypeId;
            case R::List: return UIA_ListControlTypeId;
            case R::ListItem: return UIA_ListItemControlTypeId;
            case R::Tree: return UIA_TreeControlTypeId;
            case R::TreeItem: return UIA_TreeItemControlTypeId;
            case R::Tab: return UIA_TabControlTypeId;
            case R::TabItem: return UIA_TabItemControlTypeId;
            case R::Menu: return UIA_MenuControlTypeId;
            case R::MenuItem: return UIA_MenuItemControlTypeId;
            case R::ScrollBar: return UIA_ScrollBarControlTypeId;
            case R::Window: return UIA_WindowControlTypeId;
            case R::Document: return UIA_DocumentControlTypeId;
            case R::DataGrid: return UIA_DataGridControlTypeId;
            case R::ToolTip: return UIA_ToolTipControlTypeId;
            case R::Group: default: return UIA_GroupControlTypeId;
            }
        }

        class ZufyUIElementProvider;

        struct UiaWindowContext {
            Window* window = nullptr;
            ComPtr<IRawElementProviderSimple> root;                            // 根 provider
            std::unordered_map<UIElement*, ZufyUIElementProvider*> elements;   // 弱缓存（元素析构时移除）
        };
        inline std::unordered_map<Window*, UiaWindowContext>& UiaContexts() {
            static std::unordered_map<Window*, UiaWindowContext> m; return m;
        }

        class ZufyUIElementProvider : public IRawElementProviderSimple,
                                      public IRawElementProviderFragment,
                                      public IRawElementProviderFragmentRoot,
                                      public IInvokeProvider,
                                      public IToggleProvider,
                                      public IValueProvider,
                                      public IRangeValueProvider,
                                      public IExpandCollapseProvider {
        public:
            ZufyUIElementProvider(UiaWindowContext* ctx, UIElement* e, bool isRoot)
                : ctx_(ctx), elem_(e), isRoot_(isRoot) {}

            static ZufyUIElementProvider* EnsureElementProvider(UiaWindowContext* ctx, UIElement* e) {
                if (!ctx || !e) return nullptr;
                auto it = ctx->elements.find(e);
                if (it != ctx->elements.end() && it->second) return it->second;
                auto* prov = new ZufyUIElementProvider(ctx, e, false);   // ref=1（由 ctx 持有）
                ctx->elements[e] = prov;
                return prov;
            }
            UIElement* Element() const { return elem_; }
            void DetachElement() { elem_ = nullptr; }

            // ---- IUnknown ----
            ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&ref_); }
            ULONG STDMETHODCALLTYPE Release() override {
                ULONG r = (ULONG)InterlockedDecrement(&ref_);
                if (r == 0) { if (isRoot_ && ctx_) ctx_->root.Detach(); delete this; }
                return r;
            }
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
                if (!ppv) return E_INVALIDARG; *ppv = nullptr;
                if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple))
                    *ppv = static_cast<IRawElementProviderSimple*>(this);
                else if (riid == __uuidof(IRawElementProviderFragment))
                    *ppv = static_cast<IRawElementProviderFragment*>(this);
                else if (riid == __uuidof(IRawElementProviderFragmentRoot))
                    *ppv = static_cast<IRawElementProviderFragmentRoot*>(this);
                else if (riid == __uuidof(IInvokeProvider)) *ppv = static_cast<IInvokeProvider*>(this);
                else if (riid == __uuidof(IToggleProvider)) *ppv = static_cast<IToggleProvider*>(this);
                else if (riid == __uuidof(IValueProvider)) *ppv = static_cast<IValueProvider*>(this);
                else if (riid == __uuidof(IRangeValueProvider)) *ppv = static_cast<IRangeValueProvider*>(this);
                else if (riid == __uuidof(IExpandCollapseProvider)) *ppv = static_cast<IExpandCollapseProvider*>(this);
                else return E_NOINTERFACE;
                AddRef(); return S_OK;
            }

            // ---- IRawElementProviderSimple ----
            HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* p) override {
                if (!p) return E_INVALIDARG;
                *p = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id, IUnknown** p) override {
                if (!p) return E_INVALIDARG; *p = nullptr;
                if (!elem_) return S_OK;
                AccessibleRole r = elem_->GetAccessibleRole();
                switch (id) {
                case UIA_InvokePatternId:
                    if (r == AccessibleRole::Button || r == AccessibleRole::MenuItem)
                        return QueryInterface(__uuidof(IInvokeProvider), (void**)p);
                    break;
                case UIA_TogglePatternId:
                    if (elem_->GetAccessibleToggleState() >= 0)
                        return QueryInterface(__uuidof(IToggleProvider), (void**)p);
                    break;
                case UIA_ValuePatternId:
                    if (r == AccessibleRole::Edit || r == AccessibleRole::Document)
                        return QueryInterface(__uuidof(IValueProvider), (void**)p);
                    break;
                case UIA_RangeValuePatternId:
                    if (r == AccessibleRole::Slider || r == AccessibleRole::ProgressBar)
                        return QueryInterface(__uuidof(IRangeValueProvider), (void**)p);
                    break;
                case UIA_ExpandCollapsePatternId:
                    if (elem_->GetAccessibleExpandState() >= 0)
                        return QueryInterface(__uuidof(IExpandCollapseProvider), (void**)p);
                    break;
                default: break;
                }
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id, VARIANT* p) override {
                if (!p) return E_INVALIDARG; VariantInit(p);
                if (!elem_) return S_OK;
                switch (id) {
                case UIA_ControlTypePropertyId: p->vt = VT_I4; p->lVal = UiaControlTypeOf(elem_->GetAccessibleRole()); break;
                case UIA_NamePropertyId: { std::wstring n = elem_->GetAccessibleName(); if (!n.empty()) { p->vt = VT_BSTR; p->bstrVal = SysAllocString(n.c_str()); } break; }
                case UIA_AutomationIdPropertyId: { std::wstring a = elem_->GetAutomationIdOrAuto(); p->vt = VT_BSTR; p->bstrVal = SysAllocString(a.c_str()); break; }
                case UIA_FrameworkIdPropertyId: p->vt = VT_BSTR; p->bstrVal = SysAllocString(L"ZufyUI"); break;
                case UIA_IsEnabledPropertyId: p->vt = VT_BOOL; p->boolVal = elem_->IsEffectivelyEnabled() ? VARIANT_TRUE : VARIANT_FALSE; break;
                case UIA_IsOffscreenPropertyId: p->vt = VT_BOOL; p->boolVal = elem_->IsVisible() ? VARIANT_FALSE : VARIANT_TRUE; break;
                case UIA_IsKeyboardFocusablePropertyId: p->vt = VT_BOOL; p->boolVal = elem_->IsFocusable() ? VARIANT_TRUE : VARIANT_FALSE; break;
                case UIA_HasKeyboardFocusPropertyId: p->vt = VT_BOOL; p->boolVal = (ctx_ && ctx_->window && ctx_->window->GetFocusedElement() == elem_) ? VARIANT_TRUE : VARIANT_FALSE; break;
                case UIA_IsControlElementPropertyId: p->vt = VT_BOOL; p->boolVal = VARIANT_TRUE; break;
                case UIA_IsContentElementPropertyId: p->vt = VT_BOOL; p->boolVal = VARIANT_TRUE; break;
                case UIA_ProcessIdPropertyId: p->vt = VT_I4; p->lVal = (LONG)GetCurrentProcessId(); break;
                case UIA_NativeWindowHandlePropertyId:
                    if (isRoot_ && ctx_ && ctx_->window) { p->vt = VT_I4; p->lVal = (LONG)(INT_PTR)ctx_->window->GetHwnd(); }
                    break;
                case UIA_BoundingRectanglePropertyId: {
                    double b[4];
                    if (GetScreenRect(b)) {
                        SAFEARRAY* sa = SafeArrayCreateVector(VT_R8, 0, 4);
                        if (sa) { for (LONG i = 0; i < 4; ++i) SafeArrayPutElement(sa, &i, &b[i]); p->vt = VT_R8 | VT_ARRAY; p->parray = sa; }
                    }
                    break;
                }
                default: break;
                }
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** p) override {
                if (p) *p = nullptr;
                if (isRoot_ && ctx_ && ctx_->window && ctx_->window->GetHwnd())
                    return UiaHostProviderFromHwnd(ctx_->window->GetHwnd(), p);
                return S_OK;
            }

            // ---- IRawElementProviderFragment ----
            HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection dir, IRawElementProviderFragment** p) override {
                if (p) *p = nullptr; if (!elem_ || !ctx_) return S_OK;
                UIElement* t = nullptr;
                if (dir == NavigateDirection_Parent) {
                    t = elem_->GetParent();
                }
                else {
                    std::vector<UIElement*> kids = elem_->GetAccessibleChildren();
                    if (dir == NavigateDirection_FirstChild) { if (!kids.empty()) t = kids.front(); }
                    else if (dir == NavigateDirection_LastChild) { if (!kids.empty()) t = kids.back(); }
                    else if (dir == NavigateDirection_NextSibling || dir == NavigateDirection_PreviousSibling) {
                        UIElement* par = elem_->GetParent();
                        if (par) {
                            std::vector<UIElement*> sib = par->GetAccessibleChildren();
                            for (size_t i = 0; i < sib.size(); ++i) if (sib[i] == elem_) {
                                int j = (dir == NavigateDirection_NextSibling) ? (int)i + 1 : (int)i - 1;
                                if (j >= 0 && j < (int)sib.size()) t = sib[j];
                                break;
                            }
                        }
                    }
                }
                if (t) {
                    ZufyUIElementProvider* prov = EnsureElementProvider(ctx_, t);
                    if (prov) prov->QueryInterface(__uuidof(IRawElementProviderFragment), (void**)p);
                }
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** p) override {
                if (!p) return E_INVALIDARG; *p = nullptr;
                int rid[2] = { UiaAppendRuntimeId, (int)(INT_PTR)elem_ };
                SAFEARRAY* sa = SafeArrayCreateVector(VT_I4, 0, 2);
                if (!sa) return E_OUTOFMEMORY;
                for (LONG i = 0; i < 2; ++i) SafeArrayPutElement(sa, &i, &rid[i]);
                *p = sa; return S_OK;
            }
            HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* r) override {
                if (!r) return E_INVALIDARG;
                double b[4] = { 0, 0, 0, 0 }; GetScreenRect(b);
                r->left = b[0]; r->top = b[1]; r->width = b[2]; r->height = b[3];
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** p) override { if (p) *p = nullptr; return S_OK; }
            HRESULT STDMETHODCALLTYPE SetFocus() override {
                if (elem_ && elem_->IsFocusable()) if (Window* w = elem_->GetWindow()) w->FocusElement(elem_);
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** p) override {
                if (!p) return E_INVALIDARG; *p = nullptr;
                if (ctx_ && ctx_->root) ctx_->root->QueryInterface(__uuidof(IRawElementProviderFragmentRoot), (void**)p);
                return S_OK;
            }

            // ---- IRawElementProviderFragmentRoot（仅根有效）----
            HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** p) override {
                if (p) *p = nullptr; if (!ctx_ || !ctx_->window) return S_OK;
                HWND h = ctx_->window->GetHwnd(); if (!h) return S_OK;
                UINT dpi = GetDpiForWindow(h); if (!dpi) dpi = 96;
                POINT pt = { (LONG)x, (LONG)y };
                ScreenToClient(h, &pt);
                float dx = pt.x * 96.0f / dpi, dy = pt.y * 96.0f / dpi;
                UIElement* target = ctx_->window->HitTestElementDIP(dx, dy);
                while (target && target->IsAccessibilityIgnored()) target = target->GetParent();
                if (!target) return S_OK;
                ZufyUIElementProvider* prov = EnsureElementProvider(ctx_, target);
                if (prov) prov->QueryInterface(__uuidof(IRawElementProviderFragment), (void**)p);
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** p) override {
                if (p) *p = nullptr; if (!ctx_ || !ctx_->window) return S_OK;
                UIElement* f = ctx_->window->GetFocusedElement();
                if (!f) return S_OK;
                ZufyUIElementProvider* prov = EnsureElementProvider(ctx_, f);
                if (prov) prov->QueryInterface(__uuidof(IRawElementProviderFragment), (void**)p);
                return S_OK;
            }

            // ---- Pattern 实现 ----
            HRESULT STDMETHODCALLTYPE Invoke() override { if (elem_) elem_->AccessibilityInvoke(); return S_OK; }
            HRESULT STDMETHODCALLTYPE Toggle() override { if (elem_) elem_->AccessibilityToggle(); return S_OK; }
            HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* p) override {
                if (!p) return E_INVALIDARG; if (!elem_) return E_FAIL;
                int s = elem_->GetAccessibleToggleState();
                *p = (s == 1) ? ToggleState_On : (s == 2) ? ToggleState_Indeterminate : ToggleState_Off;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR v) override { if (elem_) elem_->SetAccessibleValue(v ? v : L""); return S_OK; }
            HRESULT STDMETHODCALLTYPE get_Value(BSTR* p) override {
                if (!p) return E_INVALIDARG; if (!elem_) return E_FAIL;
                *p = SysAllocString(elem_->GetAccessibleValue().c_str()); return S_OK;
            }
            HRESULT STDMETHODCALLTYPE get_Value(double* p) override {
                if (!p) return E_INVALIDARG; if (!elem_) return E_FAIL;
                *p = elem_->GetAccessibleRangeValue(); return S_OK;
            }
            HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* p) override {
                if (!p) return E_INVALIDARG; if (!elem_) return E_FAIL;
                *p = elem_->IsAccessibleReadOnly() ? TRUE : FALSE; return S_OK;
            }
            HRESULT STDMETHODCALLTYPE SetValue(double v) override { if (elem_) elem_->SetAccessibleRangeValue(v); return S_OK; }
            HRESULT STDMETHODCALLTYPE get_Minimum(double* p) override { if (!p) return E_INVALIDARG; *p = elem_ ? elem_->GetAccessibleRangeMin() : 0.0; return S_OK; }
            HRESULT STDMETHODCALLTYPE get_Maximum(double* p) override { if (!p) return E_INVALIDARG; *p = elem_ ? elem_->GetAccessibleRangeMax() : 0.0; return S_OK; }
            HRESULT STDMETHODCALLTYPE get_LargeChange(double* p) override { if (!p) return E_INVALIDARG; *p = elem_ ? elem_->GetAccessibleRangeStep() : 0.0; return S_OK; }
            HRESULT STDMETHODCALLTYPE get_SmallChange(double* p) override { if (!p) return E_INVALIDARG; *p = elem_ ? elem_->GetAccessibleRangeStep() : 0.0; return S_OK; }
            HRESULT STDMETHODCALLTYPE Expand() override { if (elem_) elem_->AccessibilityExpand(); return S_OK; }
            HRESULT STDMETHODCALLTYPE Collapse() override { if (elem_) elem_->AccessibilityCollapse(); return S_OK; }
            HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(ExpandCollapseState* p) override {
                if (!p) return E_INVALIDARG; if (!elem_) return E_FAIL;
                int s = elem_->GetAccessibleExpandState();
                *p = (s == 1) ? ExpandCollapseState_Expanded : (s == 2) ? ExpandCollapseState_LeafNode : ExpandCollapseState_Collapsed;
                return S_OK;
            }

        private:
            bool GetScreenRect(double out[4]) const {
                out[0] = out[1] = out[2] = out[3] = 0;
                if (!elem_ || !ctx_ || !ctx_->window) return false;
                HWND h = ctx_->window->GetHwnd(); if (!h) return false;
                UINT dpi = GetDpiForWindow(h); if (!dpi) dpi = 96;
                double s = dpi / 96.0;
                Rect r = elem_->GetArrangedRect();
                POINT tl = { (LONG)(r.x * s), (LONG)(r.y * s) };
                POINT br = { (LONG)((r.x + r.width) * s), (LONG)((r.y + r.height) * s) };
                ClientToScreen(h, &tl); ClientToScreen(h, &br);
                out[0] = (double)tl.x; out[1] = (double)tl.y; out[2] = (double)(br.x - tl.x); out[3] = (double)(br.y - tl.y);
                return true;
            }
            LONG ref_ = 1;
            UiaWindowContext* ctx_ = nullptr;
            UIElement* elem_ = nullptr;
            bool isRoot_ = false;
        };

        inline IRawElementProviderSimple* EnsureRootProvider(Window* w) {
            if (!w) return nullptr;
            auto& ctx = UiaContexts()[w];
            ctx.window = w;
            if (ctx.root) return ctx.root.Get();
            auto* root = new ZufyUIElementProvider(&ctx, w->GetRootLayout().get(), true);
            ctx.root.Attach(root);   // ref=1
            return root;
        }

        inline void ReleaseAccessibility(Window* w) {
            auto it = UiaContexts().find(w);
            if (it == UiaContexts().end()) return;
            for (auto& kv : it->second.elements) { if (kv.second) { kv.second->DetachElement(); kv.second->Release(); } }
            it->second.elements.clear();
            if (it->second.root) { it->second.root->Release(); it->second.root = nullptr; }
            UiaContexts().erase(it);
        }

        inline LRESULT HandleGetObject(Window* w, HWND hwnd, WPARAM wParam, LPARAM lParam) {
            if (!w) return 0;
            if ((LONG_PTR)lParam == (LONG_PTR)UiaRootObjectId) {
                IRawElementProviderSimple* root = EnsureRootProvider(w);
                if (root) return UiaReturnRawElementProvider(hwnd, wParam, lParam, root);
                return 0;
            }
            if ((LONG_PTR)lParam == (LONG_PTR)OBJID_CLIENT) {
                IRawElementProviderSimple* host = nullptr;
                if (SUCCEEDED(UiaHostProviderFromHwnd(hwnd, &host)) && host)
                    return UiaReturnRawElementProvider(hwnd, wParam, lParam, host);
            }
            return 0;
        }

    } // namespace detail

    namespace detail {
        inline IRawElementProviderSimple* ProviderForElement(UIElement* e) {
            if (!e) return nullptr;
            Window* w = e->GetWindow(); if (!w) return nullptr;
            auto it = UiaContexts().find(w); if (it == UiaContexts().end()) return nullptr;
            return ZufyUIElementProvider::EnsureElementProvider(&it->second, e);
        }
    }
    inline void UIElement::AccessibilityNotifyPropertyChanged() {
        if (!UiaClientsAreListening()) return;
        IRawElementProviderSimple* p = detail::ProviderForElement(this);
        if (!p) return;
        VARIANT oldV; VariantInit(&oldV);
        AccessibleRole r = GetAccessibleRole();
        if (r == AccessibleRole::Edit || r == AccessibleRole::Document) {
            VARIANT nv; VariantInit(&nv); nv.vt = VT_BSTR; nv.bstrVal = SysAllocString(GetAccessibleValue().c_str());
            UiaRaiseAutomationPropertyChangedEvent(p, UIA_ValueValuePropertyId, oldV, nv);
            VariantClear(&nv);
        }
        int ts = GetAccessibleToggleState();
        if (ts >= 0) {
            VARIANT nv; VariantInit(&nv); nv.vt = VT_I4;
            nv.lVal = (ts == 1) ? ToggleState_On : (ts == 2) ? ToggleState_Indeterminate : ToggleState_Off;
            UiaRaiseAutomationPropertyChangedEvent(p, UIA_ToggleToggleStatePropertyId, oldV, nv);
        }
        if (r == AccessibleRole::Slider || r == AccessibleRole::ProgressBar) {
            VARIANT nv; VariantInit(&nv); nv.vt = VT_R8; nv.dblVal = GetAccessibleRangeValue();
            UiaRaiseAutomationPropertyChangedEvent(p, UIA_RangeValueValuePropertyId, oldV, nv);
        }
    }
    inline void UIElement::AccessibilityNotifyStructureChanged() {
        if (!UiaClientsAreListening()) return;
        IRawElementProviderSimple* p = detail::ProviderForElement(this);
        if (p) UiaRaiseStructureChangedEvent(p, StructureChangeType_ChildrenInvalidated, nullptr, 0);
    }
    inline void UIElement::AccessibilityNotifyFocus() {
        if (!UiaClientsAreListening()) return;
        Window* w = GetWindow(); if (!w || !w->IsAccessibilityEnabled()) return;
        if (auto* root = detail::EnsureRootProvider(w)) UiaRaiseAutomationEvent(root, UIA_AutomationFocusChangedEventId);
    }
    inline void Window::ForgetAccessibleElement(UIElement* e) {
        auto it = detail::UiaContexts().find(this);
        if (it == detail::UiaContexts().end()) return;
        auto jt = it->second.elements.find(e);
        if (jt != it->second.elements.end()) { if (jt->second) { jt->second->DetachElement(); jt->second->Release(); } it->second.elements.erase(jt); }
    }

    // ============================================================================
    // 调试 / 自动化通道（默认关；只返回"当前这一帧"；库不做文件读写）
    //   ZufyUI::SetDebugEnabled(true) 后，外部工具向后台调度窗口
    //   （类名 "ZufyUI_DispatcherWindow"）发 WM_COPYDATA：
    //     wParam = 回复窗口 HWND；dwData = 命令号；lpData/cbData = 可选 UTF-16 参数
    //   命令：1 Ping / 2 ListWindows / 3 GetFrameStats / 4 GetElementTree /
    //         5 ForceRepaint / 6 SetElementText("automationId\ttext") /
    //         7 InvokeElement(id) / 8 FocusElement(id)
    //   回复：向 wParam 发 WM_COPYDATA，dwData=0x5A554631('ZUF1')，lpData=UTF-16 文本。
    // ============================================================================
    namespace detail {
        inline const wchar_t* AriaRoleName(AccessibleRole r) {
            switch (r) {
            case AccessibleRole::Button: return L"Button";
            case AccessibleRole::Text: return L"Text";
            case AccessibleRole::Edit: return L"Edit";
            case AccessibleRole::CheckBox: return L"CheckBox";
            case AccessibleRole::RadioButton: return L"RadioButton";
            case AccessibleRole::ComboBox: return L"ComboBox";
            case AccessibleRole::Slider: return L"Slider";
            case AccessibleRole::ProgressBar: return L"ProgressBar";
            case AccessibleRole::List: return L"List";
            case AccessibleRole::ListItem: return L"ListItem";
            case AccessibleRole::Tree: return L"Tree";
            case AccessibleRole::TreeItem: return L"TreeItem";
            case AccessibleRole::Tab: return L"Tab";
            case AccessibleRole::TabItem: return L"TabItem";
            case AccessibleRole::Menu: return L"Menu";
            case AccessibleRole::MenuItem: return L"MenuItem";
            case AccessibleRole::ScrollBar: return L"ScrollBar";
            case AccessibleRole::Window: return L"Window";
            case AccessibleRole::Document: return L"Document";
            case AccessibleRole::DataGrid: return L"DataGrid";
            case AccessibleRole::ToolTip: return L"ToolTip";
            case AccessibleRole::Group: return L"Group";
            default: return L"None";
            }
        }
        // RTTI 类型名（MSVC: ".?AVButton@ZufyUI@@" → "Button"）
        inline std::wstring TypeNameOf(UIElement* e) {
            if (!e) return L"";
            std::string n = typeid(*e).name();
            size_t p = n.find("AV");
            if (p != std::string::npos) n = n.substr(p + 2);
            size_t at = n.find('@');
            if (at != std::string::npos) n = n.substr(0, at);
            return std::wstring(n.begin(), n.end());
        }
        inline int CountElements(UIElement* e, int* visibleOut) {
            if (!e) return 0;
            int total = 1;
            if (visibleOut && e->IsVisible()) (*visibleOut)++;
            for (UIElement* c : e->GetChildren()) total += CountElements(c, visibleOut);
            return total;
        }
        inline void CollectAll(UIElement* e, std::vector<UIElement*>& out) {
            if (!e) return;
            out.push_back(e);
            for (UIElement* c : e->GetChildren()) CollectAll(c, out);
        }
        // Top-N 热点（kind: 1=重绘 2=布局 3=缓存 4=绘制）
        inline std::wstring DumpTopN(int kind, int n) {
            std::vector<UIElement*> all;
            for (Window* w : AppCore::Instance().Windows()) if (w) CollectAll(w->GetRootLayout().get(), all);
            auto key = [kind](UIElement* e) -> double {
                switch (kind) {
                case 1: return (double)e->DebugRepaintCount();
                case 2: return (double)(e->DebugMeasureCount() + e->DebugArrangeCount());
                case 3: return (double)e->DebugCacheBytes();
                case 4: return (double)e->DebugDrawCount();
                default: return (double)(e->DebugRepaintCount() + e->DebugMeasureCount() + e->DebugArrangeCount());
                }
            };
            std::sort(all.begin(), all.end(), [&](UIElement* a, UIElement* b) { return key(a) > key(b); });
            if (n <= 0 || n > (int)all.size()) n = (int)all.size();
            const wchar_t* title = kind == 1 ? L"重绘" : kind == 2 ? L"布局" : kind == 3 ? L"缓存" : kind == 4 ? L"绘制" : L"综合";
            std::wstring out; wchar_t hb[128];
            swprintf(hb, 128, L"Top %d %s（共 %d 个元素）\n", n, title, (int)all.size()); out = hb;
            for (int i = 0; i < n; ++i) {
                UIElement* e = all[i]; wchar_t b[256];
                if (kind == 3) swprintf(b, 256, L"%.1f KB\t%s\t%s\t%s\n", key(e) / 1024.0,   // 缓存一律用 KB
                    TypeNameOf(e).c_str(), e->GetAccessibleName().c_str(), e->GetAutomationIdOrAuto().c_str());
                else swprintf(b, 256, L"%.0f\t%s\t%s\t%s\n", key(e),
                    TypeNameOf(e).c_str(), e->GetAccessibleName().c_str(), e->GetAutomationIdOrAuto().c_str());
                out += b;
            }
            return out;
        }
        inline void ResetDebugCounters() {
            std::vector<UIElement*> all;
            for (Window* w : AppCore::Instance().Windows()) if (w) CollectAll(w->GetRootLayout().get(), all);
            for (UIElement* e : all) e->DebugResetCounters();
        }
        inline void DumpElement(UIElement* e, int depth, std::wstring& out) {
            if (!e || !e->IsVisible()) return;
            out.append((size_t)depth * 2, L' ');                       // 缩进：2 空格/层（列 0 前缀）
            out += TypeNameOf(e); out += L'\t';  // 列0：类型（RTTI）
            out += e->GetAccessibleName(); out += L'\t';                // 列1：名称
            out += e->GetAutomationIdOrAuto(); out += L'\t';            // 列2：AutomationId
            Rect rc = e->GetArrangedRect();                             // 列3：矩形
            wchar_t b[96];
            swprintf(b, 96, L"%.0f,%.0f %.0fx%.0f\n", rc.x, rc.y, rc.width, rc.height);
            out += b;
            for (UIElement* c : e->GetAccessibleChildren()) DumpElement(c, depth + 1, out);
        }
        inline std::wstring DumpWindowTree(Window* w) {
            std::wstring out;
            if (!w) return out;
            wchar_t hb[48];
            swprintf(hb, 48, L"Window hwnd=0x%llX\n", (unsigned long long)(uintptr_t)w->GetHwnd());
            out = hb;
            DumpElement(w->GetRootLayout().get(), 1, out);
            return out;
        }
        inline std::wstring DumpFrameStats(Window* w) {
            if (!w) return std::wstring();
            const FrameStats& s = w->DebugStats();
            wchar_t b[512];
            swprintf(b, 512, L"hwnd=0x%llX frame=%llu dt=%.2fms advance=%.2fms render=%.2fms elems=%d vis=%d pending=%d anims=%d\n",
                (unsigned long long)(uintptr_t)w->GetHwnd(), s.frame, s.deltaTimeMs, s.advanceMs, s.renderMs,
                s.elementCount, s.visibleCount, s.pendingRepaint, s.activeAnims);
            return b;
        }
        // 单个控件的实时详细信息（类型/状态/动画/脏标志/计数/耗时/缓存/父链…）
        inline std::wstring DumpElementInfo(UIElement* e) {
            if (!e) return L"(未找到)";
            Rect rc = e->GetArrangedRect(); Size ds = e->GetDesiredSize();
            std::wstring s; wchar_t b[768];
            swprintf(b, 768, L"类型: %s\r\n角色: %s\r\n名称: %s\r\nAutomationId: %s\r\n\r\n",
                TypeNameOf(e).c_str(), AriaRoleName(e->GetAccessibleRole()), e->GetAccessibleName().c_str(), e->GetAutomationIdOrAuto().c_str());
            s += b;
            swprintf(b, 768, L"矩形: %.1f, %.1f  %.1f x %.1f\r\n期望尺寸: %.1f x %.1f\r\n\r\n",
                rc.x, rc.y, rc.width, rc.height, ds.width, ds.height);
            s += b;
            swprintf(b, 768, L"状态: 可见=%d 启用=%d 可聚焦=%d 焦点=%d 悬停=%d 按下=%d\r\n\r\n",
                e->IsVisible() ? 1 : 0, e->IsEffectivelyEnabled() ? 1 : 0, e->IsFocusable() ? 1 : 0,
                e->DebugFocused() ? 1 : 0, e->DebugHovered() ? 1 : 0, e->DebugPressed() ? 1 : 0);
            s += b;
            swprintf(b, 768, L"动画: 活跃=%d   进度=%.2f\r\n\r\n", e->HasActiveAnimation() ? 1 : 0, e->DebugAnimationProgress());
            s += b;
            swprintf(b, 768, L"脏标志: measure=%d selfArrange=%d subtree=%d children=%d cacheValid=%d\r\n\r\n",
                e->DebugMeasureDirty() ? 1 : 0, e->DebugSelfArrangeDirty() ? 1 : 0, e->DebugSubtreeDirty() ? 1 : 0,
                e->DebugChildrenDirty() ? 1 : 0, e->DebugCacheValid() ? 1 : 0);
            s += b;
            swprintf(b, 768, L"计数: measure=%llu arrange=%llu draw=%llu repaint=%llu\r\n",
                e->DebugMeasureCount(), e->DebugArrangeCount(), e->DebugDrawCount(), e->DebugRepaintCount());
            s += b;
            swprintf(b, 768, L"上帧耗时: measure=%.3fms arrange=%.3fms draw=%.3fms\r\n\r\n",
                e->DebugMeasureMs(), e->DebugArrangeMs(), e->DebugDrawMs());
            s += b;
        swprintf(b, 768, L"缓存: %s   %.1f KB\r\n可见子元素: %d\r\n\r\n",
            e->DebugCacheValid() ? L"有效" : L"无", e->DebugCacheBytes() / 1024.0, (int)e->GetChildren().size());
            s += b;
            std::vector<std::wstring> parts; int depth = 0;
            for (UIElement* p = e->GetParent(); p; p = p->GetParent()) {
                std::wstring tn = TypeNameOf(p); std::wstring nm = p->GetAccessibleName();
                parts.push_back(nm.empty() ? tn : (tn + L"\"" + nm + L"\""));
                if (++depth > 32) break;
            }
            std::wstring chain;
            for (auto it = parts.rbegin(); it != parts.rend(); ++it) { if (!chain.empty()) chain += L" > "; chain += *it; }
            swprintf(b, 768, L"深度: %d\r\n父链: %s\r\n", depth, chain.empty() ? L"(根)" : chain.c_str());
            s += b;
            swprintf(b, 768, L"\r\n地址: 0x%llX\r\n", (unsigned long long)(uintptr_t)e);
            s += b;
            return s;
        }
        inline UIElement* FindByAutomationId(UIElement* e, const std::wstring& id) {
            if (!e || id.empty()) return nullptr;
            if (e->GetAutomationIdOrAuto() == id) return e;   // 含自动编号 e123
            for (UIElement* c : e->GetChildren()) if (UIElement* f = FindByAutomationId(c, id)) return f;
            return nullptr;
        }
        inline UIElement* FindAcrossWindows(const std::wstring& id) {
            for (Window* w : AppCore::Instance().Windows()) if (w) if (UIElement* f = FindByAutomationId(w->GetRootLayout().get(), id)) return f;
            return nullptr;
        }
        inline bool DebugSetElementText(const std::wstring& id, const std::wstring& text) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false; e->SetAccessibleValue(text); return true;
        }
        inline bool DebugInvokeElement(const std::wstring& id) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false; e->AccessibilityInvoke(); return true;
        }
        inline bool DebugFocusElement(const std::wstring& id) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false;
            if (Window* w = e->GetWindow()) w->FocusElement(e);
            return true;
        }
        inline bool DebugSetElementVisible(const std::wstring& id, bool vis) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false; e->SetVisible(vis); return true;
        }
        inline bool DebugSetElementMargin(const std::wstring& id, float x, float y) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false;
            e->SetMargin(Thickness(x, y, 0, 0)); return true;
        }
        inline bool DebugSetHighlight(const std::wstring& id) {
            UIElement* e = FindAcrossWindows(id); if (!e) return false;
            g_debugHighlight.store(e, std::memory_order_relaxed);
            e->RequestRepaint();   // 触发一帧把高亮框画出来
            return true;
        }
        inline void DebugClearHighlight() {
            g_debugHighlight.store(nullptr, std::memory_order_relaxed);
            for (Window* w : AppCore::Instance().Windows()) if (w && w->GetHwnd()) InvalidateRect(w->GetHwnd(), nullptr, FALSE);
        }
        inline LRESULT HandleDebugCopyData(HWND hwnd, WPARAM wParam, LPARAM lParam) {
            if (!DebugEnabled()) return 0;
            auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
            if (!cds) return 0;
            HWND replyTo = (HWND)wParam;
            UINT cmd = (UINT)cds->dwData;
            std::wstring arg;
            if (cds->lpData && cds->cbData >= sizeof(wchar_t))
                arg.assign((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
            while (!arg.empty() && arg.back() == L'\0') arg.pop_back();

            std::wstring reply;
            switch (cmd) {
            case 1: reply = L"ZufyUI " ZufyUI_VERSION_STRING L"\n"; break;
            case 2:
                for (Window* w : AppCore::Instance().Windows()) if (w) {
                    wchar_t b[128];
                    swprintf(b, 128, L"hwnd=0x%llX dpi=%u\n", (unsigned long long)(uintptr_t)w->GetHwnd(), (unsigned)GetDpiForWindow(w->GetHwnd()));
                    reply += b;
                }
                break;
            case 3: for (Window* w : AppCore::Instance().Windows()) if (w) reply += DumpFrameStats(w); break;
            case 4: for (Window* w : AppCore::Instance().Windows()) if (w) reply += DumpWindowTree(w); break;
            case 5: for (Window* w : AppCore::Instance().Windows()) if (w) InvalidateRect(w->GetHwnd(), nullptr, FALSE); reply = L"repaint requested\n"; break;
            case 6: {
                size_t tab = arg.find(L'\t');
                if (tab == std::wstring::npos) { reply = L"ERR arg\n"; break; }
                reply = DebugSetElementText(arg.substr(0, tab), arg.substr(tab + 1)) ? L"OK\n" : L"ERR not found\n";
                break;
            }
            case 7: reply = DebugInvokeElement(arg) ? L"OK\n" : L"ERR not found\n"; break;
            case 8: reply = DebugFocusElement(arg) ? L"OK\n" : L"ERR not found\n"; break;
            case 9: reply = DumpElementInfo(FindAcrossWindows(arg)); break;   // 控件详情（arg = AutomationId）
            case 10: {   // GetElementAt("x,y") 屏幕坐标 → AutomationId（拾取用）
                int px = 0, py = 0; swscanf_s(arg.c_str(), L"%d,%d", &px, &py);
                POINT pt = { px, py }; ScreenToClient(hwnd, &pt);
                UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
                float dx = pt.x * 96.0f / dpi, dy = pt.y * 96.0f / dpi;
                UIElement* hit = nullptr;
                for (Window* w : AppCore::Instance().Windows()) if (w && w->GetHwnd() == hwnd) { hit = w->HitTestElementDIP(dx, dy); break; }
                reply = hit ? hit->GetAutomationIdOrAuto() : L"";
                break;
            }
            case 11: reply = DebugSetHighlight(arg) ? L"OK\n" : L"ERR not found\n"; break;
            case 12: DebugClearHighlight(); reply = L"OK\n"; break;
            case 13: reply = DumpErrors(); break;                              // 最近错误
            case 14: ClearErrors(); reply = L"OK\n"; break;                   // 清空错误
            case 15: reply = DumpTopN(1, 20); break;                          // 重绘热点 Top-N
            case 16: reply = DumpTopN(2, 20); break;                          // 布局风暴 Top-N
            case 17: reply = DumpTopN(3, 20); break;                          // 缓存 Top-N
            case 18: reply = DumpTopN(4, 20); break;                          // 绘制 Top-N
            case 19: ResetDebugCounters(); reply = L"OK\n"; break;            // 重置调试计数
            case 22: {   // SetHighlightColor("RRGGBB") —— 高亮框颜色（拾取/高亮共用）
                unsigned int c = (unsigned int)wcstoul(arg.c_str(), nullptr, 16);
                g_highlightColorArgb.store(0xFF000000u | (c & 0xFFFFFFu), std::memory_order_relaxed);
                if (UIElement* hl = g_debugHighlight.load()) hl->RequestRepaint();
                reply = L"OK\n";
                break;
            }
            case 20: {   // SetVisible("id\t0/1")
                size_t p = arg.find(L'\t');
                if (p == std::wstring::npos) { reply = L"ERR\n"; break; }
                reply = DebugSetElementVisible(arg.substr(0, p), arg.substr(p + 1) == L"1") ? L"OK\n" : L"ERR not found\n";
                break;
            }
            case 21: {   // SetMargin("id\tx\ty") —— 位置（边距）
                size_t p = arg.find(L'\t'); if (p == std::wstring::npos) { reply = L"ERR\n"; break; }
                std::wstring id = arg.substr(0, p), rest = arg.substr(p + 1);
                size_t q = rest.find(L'\t'); if (q == std::wstring::npos) { reply = L"ERR\n"; break; }
                double x = wcstod(rest.c_str(), nullptr), y = wcstod(rest.c_str() + q + 1, nullptr);
                reply = DebugSetElementMargin(id, (float)x, (float)y) ? L"OK\n" : L"ERR not found\n";
                break;
            }
            default: reply = L"ERR unknown cmd\n"; break;
            }

            if (replyTo) {
                COPYDATASTRUCT out = {};
                out.dwData = 0x5A554631;   // 'ZUF1'
                out.cbData = (DWORD)((reply.size() + 1) * sizeof(wchar_t));
                out.lpData = (PVOID)reply.c_str();
                SendMessageW(replyTo, WM_COPYDATA, (WPARAM)hwnd, (LPARAM)&out);
            }
            (void)hwnd;
            return 0;
        }
    } // namespace detail

} // namespace ZufyUI
