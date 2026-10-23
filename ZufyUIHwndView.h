#pragma once
// ============================================================================
// ZufyUIHwndView.h —— 原生 HWND 内容宿主（顶层 owned 浮窗）
//   主窗口是 WS_EX_NOREDIRECTIONBITMAP + DComp，子 HWND 不被合成；因此用顶层浮窗跟随元素。
//   对任意 HWND（WebView2 的 HWND controller、第三方控件、摄像头、进程外窗口）都有效。
//   ⚠ 跨进程 SetParent 技术合法但不推荐：跨进程父子会串联输入队列（一方卡死拖死另一方）、
//     部分窗口消息被阻止、可能出现间歇性不稳定，且目标进程可能不知道自己的窗口被重父化。
//     优先同进程；跨进程仅作最后手段。
//   ⚠ airspace：浮窗在主窗口之上、不被 ZufyUI 裁剪；完全滚出客户区会自动隐藏。
//   需要“内容进合成（可裁剪/无 airspace）”的网页/视频，走 Window::CreateExternalVisual /
//   WebView2 CompositionController。
// ============================================================================
#include "ZufyUI.h"
#include <cmath>

namespace ZufyUI {

class HwndView : public UIElement {
public:
    explicit HwndView(HWND owner, bool activate = false) : owner_(owner), activate_(activate) { EnsureHost(); }
    ~HwndView() override { UnregisterOverlay(); DestroyInternal(); }

    HwndView(const HwndView&) = delete;
    HwndView& operator=(const HwndView&) = delete;

    HWND GetHostHwnd() const { return host_; }
    // 把外部 HWND 嵌进宿主（WS_CHILD + SetParent）。同进程最稳；跨进程见文件头风险说明。
    //   宿主是普通 Win32 窗口（有重定向表面）→ 子窗口正常合成、被宿主客户区裁剪；
    //   移动/缩放宿主时子窗口自动跟随，不再对目标窗口做跨进程 SetWindowPos。
    void SetContentHwnd(HWND external, bool own = false) {
        content_ = external; ownsContent_ = own; lastTarget_ = nullptr;
        if (content_ && host_ && IsWindow(content_)) {
            LONG_PTR st = GetWindowLongPtrW(content_, GWL_STYLE);
            SetWindowLongPtrW(content_, GWL_STYLE, (st & ~(LONG_PTR)WS_POPUP) | WS_CHILD);
            ::SetParent(content_, host_);   // Win32 SetParent（区分 UIElement::SetParent）
            // 关键：改成子窗口后必须**显式显示**（子窗口可见性是独立的，宿主显示不会自动显示它），
            // 并让框架样式变更生效，否则只能看到宿主的黑底。
            ShowWindow(content_, SW_SHOWNA);
            SetWindowPos(content_, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        ApplyVisibility();
    }
    HWND GetContentHwnd() const { return content_ ? content_ : host_; }

    void SetObscured(bool on) { if (on != obscured_) { obscured_ = on; ApplyVisibility(); } }
    bool IsObscured() const { return obscured_; }

    void SetOnBoundsChanged(std::function<void(HWND, int, int)> cb) { onBoundsChanged_ = std::move(cb); }

    Size MeasureOverride(const Size& avail) override {
        float w = (GetFillWidth()  && avail.width  != FLT_MAX && avail.width  > 0.0f) ? avail.width  : (width_  > 0.0f ? width_  : 320.0f);
        float h = (GetFillHeight() && avail.height != FLT_MAX && avail.height > 0.0f) ? avail.height : (height_ > 0.0f ? height_ : 240.0f);
        return Size(w, h);
    }

    void ArrangeOverride(const Rect& finalRect) override {
        UIElement::ArrangeOverride(finalRect);
        lastRect_ = finalRect;   // 客户区 DIP（已含滚动偏移）
        cacheValid_ = false;     // 布局/DIP/尺寸可能变了，下次 SyncBounds 重取
        EnsureHost();
        EnsureRegistered();
        SyncBounds();
    }

    void OnVisibilityChanged(bool /*visible*/) override { EnsureHost(); EnsureRegistered(); ApplyVisibility(); }

    void Draw(ID2D1RenderTarget*) override {}   // 自己不画

private:
    bool EffectivelyVisible() const {
        for (const UIElement* e = this; e; e = e->GetParent()) if (!e->IsVisible()) return false;
        return true;
    }
    void EnsureHost() {
        if (host_ || !owner_) return;
        static const wchar_t* kClass = L"ZufyUIHwndOverlay";
        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = kClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassExW(&wc);
            registered = true;
        }
        DWORD ex = WS_EX_TOOLWINDOW | (activate_ ? 0 : WS_EX_NOACTIVATE);
        host_ = CreateWindowExW(ex, kClass, L"", WS_POPUP | WS_CLIPCHILDREN, 0, 0, 1, 1, owner_, nullptr, GetModuleHandle(nullptr), nullptr);
    }
    // 注册到所属窗口：由 Window 在 WM_MOVE/WM_SIZE/WM_DPICHANGED 及兜底定时器里统一同步
    void EnsureRegistered() {
        Window* w = GetWindow();
        if (!w || w == regWindow_) return;
        UnregisterOverlay();
        regWindow_ = w;
        syncId_ = w->AddOverlaySync([this]() { SyncBounds(); });
    }
    void UnregisterOverlay() {
        if (regWindow_ && syncId_ && GetWindow() == regWindow_) regWindow_->RemoveOverlaySync(syncId_);
        regWindow_ = nullptr; syncId_ = 0;
    }
    void ApplyVisibility() { SyncBounds(); }
    void SyncBounds() {
        if (!owner_ || !IsWindow(owner_)) return;
        HWND target = host_;            // 永远移动“宿主”（同线程，快）；内容窗口是它的子窗口，自动跟随
        if (!target) return;

        if (!cacheValid_) {
            cachedDpi_ = (float)GetDpiForWindow(owner_); if (cachedDpi_ <= 0.0f) cachedDpi_ = 96.0f;
            GetClientRect(owner_, &cachedClient_);
            cacheValid_ = true;
        }
        int pw = max(1, (int)lround(lastRect_.width * cachedDpi_ / 96.0f));
        int ph = max(1, (int)lround(lastRect_.height * cachedDpi_ / 96.0f));
        int px = (int)lround(lastRect_.x * cachedDpi_ / 96.0f);
        int py = (int)lround(lastRect_.y * cachedDpi_ / 96.0f);

        bool show = EffectivelyVisible() && !obscured_ && !IsIconic(owner_);

        // 有效可见区 = 元素矩形 ∩ 主窗口客户区 ∩ 所有祖先的 clip（ScrollViewer 视口等）。
        //   clip 矩形都是“客户区 DIP”，这里换算成像素后取交；宿主再用 SetWindowRgn 裁到这个区域。
        int vx0 = px, vy0 = py, vx1 = px + pw, vy1 = py + ph;
        auto cut = [&](float l, float t, float r, float b) {
            vx0 = max(vx0, (int)lround(l)); vy0 = max(vy0, (int)lround(t));
            vx1 = min(vx1, (int)lround(r)); vy1 = min(vy1, (int)lround(b));
        };
        cut(0.0f, 0.0f, (float)cachedClient_.right, (float)cachedClient_.bottom);
        {
            float s = cachedDpi_ / 96.0f;
            for (UIElement* e = GetParent(); e; e = e->GetParent()) {
                if (auto c = e->GetClipRect()) cut(c->left * s, c->top * s, c->right * s, c->bottom * s);
            }
        }
        if (vx1 <= vx0 || vy1 <= vy0) show = false;
        bool needRegion = show && (vx0 > px || vy0 > py || vx1 < px + pw || vy1 < py + ph);
        bool regionChanged = (needRegion != lastRegion_) ||
            (needRegion && (vx0 != lastVx0_ || vy0 != lastVy0_ || vx1 != lastVx1_ || vy1 != lastVy1_));

        POINT sp{ px, py };
        ClientToScreen(owner_, &sp);

        // 位置/尺寸/显隐/目标/裁剪都没变 → 什么都不做
        if (target == lastTarget_ && sp.x == lastX_ && sp.y == lastY_ &&
            pw == lastW_ && ph == lastH_ && show == lastShow_ && !regionChanged) return;
        bool moved = (sp.x != lastX_ || sp.y != lastY_);
        bool resized = (pw != lastW_ || ph != lastH_);
        bool targetChanged = (target != lastTarget_);
        lastTarget_ = target; lastX_ = sp.x; lastY_ = sp.y; lastW_ = pw; lastH_ = ph; lastShow_ = show;
        lastRegion_ = needRegion; lastVx0_ = vx0; lastVy0_ = vy0; lastVx1_ = vx1; lastVy1_ = vy1;

        UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (!moved) flags |= SWP_NOMOVE;
        if (!resized) flags |= SWP_NOSIZE;
        SetWindowPos(target, nullptr, sp.x, sp.y, pw, ph, flags);
        // 用窗口 region 裁到有效可见区（宿主是普通 Win32 窗口 → region 会连同子窗口一起裁）
        if (show && needRegion) {
            HRGN r = CreateRectRgn(vx0 - px, vy0 - py, vx1 - px, vy1 - py);
            if (!SetWindowRgn(target, r, TRUE)) DeleteObject(r);
        }
        else if (!needRegion && lastRegionWasApplied_) {
            SetWindowRgn(target, nullptr, TRUE);   // 不再需要裁剪 → 清除区域
        }
        lastRegionWasApplied_ = needRegion;
        // 子窗口填补宿主客户区（宿主无边框，客户区=窗口尺寸）；只在尺寸变化时跨进程设置一次
        if (content_ && content_ != target && IsWindow(content_) && (resized || targetChanged))
            SetWindowPos(content_, nullptr, 0, 0, pw, ph, SWP_NOACTIVATE | SWP_NOZORDER);
        if (onBoundsChanged_ && resized) onBoundsChanged_(target, pw, ph);
    }
    void DestroyInternal() {
        // 关闭外部内容（跨进程/跨线程）：先发 WM_CLOSE；随后销毁宿主（owned）会连同子窗口一起收掉
        if (ownsContent_ && content_ && IsWindow(content_)) PostMessageW(content_, WM_CLOSE, 0, 0);
        content_ = nullptr;
        if (host_) { if (IsWindow(host_)) DestroyWindow(host_); host_ = nullptr; }
    }

    HWND owner_ = nullptr;
    HWND host_ = nullptr;
    HWND content_ = nullptr;
    bool ownsContent_ = false;
    bool obscured_ = false;
    bool activate_ = false;
    Rect lastRect_{};
    Window* regWindow_ = nullptr;
    int syncId_ = 0;
    bool cacheValid_ = false;
    float cachedDpi_ = 96.0f;
    RECT cachedClient_{};
    // 上次真正 SetWindowPos 的值
    HWND lastTarget_ = nullptr;
    int lastX_ = 0, lastY_ = 0, lastW_ = 0, lastH_ = 0;
    bool lastShow_ = false;
    // 上次的裁剪区域（客户区像素；用于跳过冗余 SetWindowRgn）
    bool lastRegion_ = false;
    bool lastRegionWasApplied_ = false;
    int lastVx0_ = 0, lastVy0_ = 0, lastVx1_ = 0, lastVy1_ = 0;
    std::function<void(HWND, int, int)> onBoundsChanged_;
};

} // namespace ZufyUI
