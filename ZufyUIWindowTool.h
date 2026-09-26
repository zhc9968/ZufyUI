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
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

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
        enum class Kind { Minimize, MaximizeRestore, Close };

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
            if (!enabled) glyphColor = Color::FromArgb(255, 150, 150, 150);
            if (!glyphBrush_) rt->CreateSolidColorBrush(glyphColor.ToD2D(), glyphBrush_.GetAddressOf());
            else glyphBrush_->SetColor(glyphColor.ToD2D());
            if (!glyphBrush_) return;

            Window* w = GetWindow();
            bool maxed = w && w->IsMaximizedWindow();
            D2D1_RECT_F r = arrangedRect_.ToD2D();
            const wchar_t* glyph = L"\uE921";               // 最小化
            if (kind_ == Kind::MaximizeRestore) glyph = maxed ? L"\uE923" : L"\uE922";  // 还原 / 最大化
            else if (kind_ == Kind::Close) glyph = L"\uE8BB";                            // 关闭

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
        // 覆盖右边距（默认跟随 DWM 系统值）
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
        static constexpr UINT kCallbackMsg = WM_APP + 2;

        void ApplyTip(const std::wstring& tip) {
            currentTip_ = tip;
            wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
        }
        void ShowMenuAtCursor() {
            if (!menu_) return;
            POINT pt;
            GetCursorPos(&pt);
            menu_->ShowAt(pt.x, pt.y);
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

} // namespace ZufyUI
