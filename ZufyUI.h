#pragma once
#include <windows.h>
#include <windowsx.h>
#include <d2d1helper.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <roapi.h>
#include <dispatcherqueue.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.interop.h>
#include <windows.ui.composition.effects.h>
#include <windows.foundation.h>
#include <windows.graphics.effects.h>
#include <windows.graphics.effects.interop.h>
#include <d2d1effects_2.h>
#include <d2d1effects.h>
#include <wincodec.h>
#include <wrl/implements.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <propkey.h>
#include <propvarutil.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <mutex>
#include <thread>
#include <utility>
#include <tuple>
#include <wrl/client.h>
#include <imm.h>
#include <mmsystem.h>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <array>
#include "ZufyUIAcrylic.h"   // 手写 DComp 效果类 + 官方亚克力/云母配方（需放在 namespace ZufyUI 之前）

// 调试输出宏：默认关闭，定义 ZufyUI_DEBUG 后启用（不删除调试代码）
#ifdef ZufyUI_DEBUG
#define ZufyUI_DEBUG_LOG_W(msg) OutputDebugStringW(msg)
#define ZufyUI_DEBUG_LOG_A(msg) OutputDebugStringA(msg)
#else
#define ZufyUI_DEBUG_LOG_W(msg) ((void)0)
#define ZufyUI_DEBUG_LOG_A(msg) ((void)0)
#endif

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "windowsapp.lib")

// Windows 头把 CreateWindow 定义为宏，这里取消，避免与 Application::CreateWindow 冲突
#ifdef CreateWindow
#undef CreateWindow
#endif

// ---------- ZufyUI 版本 ----------
#define ZufyUI_VERSION_MAJOR 1
#define ZufyUI_VERSION_MINOR 9
#define ZufyUI_VERSION_PATCH 6
#define ZufyUI_VERSION_STRING L"1.9.6"

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_CAPTION_BUTTON_BOUNDS
#define DWMWA_CAPTION_BUTTON_BOUNDS 5
#endif
#ifndef DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
#define DWMWA_VISIBLE_FRAME_BORDER_THICKNESS 37
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
#define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif
#ifndef DWMSBT_AUTO
#define DWMSBT_AUTO 0
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMWCP_DEFAULT
#define DWMWCP_DEFAULT 0
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWCP_ROUNDSMALL
#define DWMWCP_ROUNDSMALL 3
#endif

namespace ZufyUI {

    using Microsoft::WRL::ComPtr;
    using namespace ABI::Windows::UI::Composition;
    using namespace ABI::Windows::UI::Composition::Desktop;

    template<typename T>
    T clamp(T value, T low, T high) {
        return value < low ? low : (value > high ? high : value);
    }

    // ---------- DPI 物理像素吸附 ----------
    // 动画/滚动进度保持 float 平滑；一旦落到 Arrange 或 Draw，就用 Snap() 吸附到物理像素
    inline float& GlobalDpiScaleRef() {
        static thread_local float scale = 1.0f;
        return scale;
    }

    inline void SetGlobalDpiScale(float scale) {
        GlobalDpiScaleRef() = (scale > 0.0f) ? scale : 1.0f;
    }

    inline float GetGlobalDpiScale() {
        return GlobalDpiScaleRef();
    }

    // 把 DIP 坐标吸附到最近的物理像素（返回值仍以 DIP 表示）
    inline float Snap(float dip) {
        float s = GlobalDpiScaleRef();
        return (s > 0.0f) ? std::round(dip * s) / s : dip;
    }

    // ---------- 未文档化亚克力相关结构 ----------
    enum ACCENT_STATE {
        ACCENT_DISABLED = 0,
        ACCENT_ENABLE_GRADIENT = 1,
        ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
        ACCENT_ENABLE_BLURBEHIND = 3,
        ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
        ACCENT_ENABLE_HOSTBACKDROP = 5
    };

    struct ACCENT_POLICY {
        ACCENT_STATE AccentState;
        int AccentFlags;
        int GradientColor;
        int AnimationId;
    };

    enum WINDOWCOMPOSITIONATTRIB {
        WCA_ACCENT_POLICY = 19
    };

    struct WINDOWCOMPOSITIONATTRIBDATA {
        WINDOWCOMPOSITIONATTRIB Attrib;
        PVOID pvData;
        SIZE_T cbData;
    };

    // ---------- 背景层（只有三个选项） ----------
    enum class Backdrop {
        None,     // 无：背景完全透明
        Acrylic,  // 亚克力
        Mica      // 云母
    };

    // ---------- 基础类型 ----------
    struct Color {
        float r, g, b, a;
        Color(float r = 0, float g = 0, float b = 0, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        }
        D2D1_COLOR_F ToD2D() const { return D2D1::ColorF(r, g, b, a); }
        static Color Lerp(const Color& c1, const Color& c2, float t) {
            return Color(c1.r + (c2.r - c1.r) * t,
                c1.g + (c2.g - c1.g) * t,
                c1.b + (c2.b - c1.b) * t,
                c1.a + (c2.a - c1.a) * t);
        }
    };

    struct Rect {
        float x, y, width, height;
        Rect(float x = 0, float y = 0, float w = 0, float h = 0) : x(x), y(y), width(w), height(h) {}
        bool Contains(float px, float py) const {
            return px >= x && px < x + width && py >= y && py < y + height;
        }
        D2D1_RECT_F ToD2D() const { return D2D1::RectF(x, y, x + width, y + height); }
    };

    struct Thickness {
        float left, top, right, bottom;
        Thickness(float l = 0, float t = 0, float r = 0, float b = 0) : left(l), top(t), right(r), bottom(b) {}
    };

    struct Size {
        float width, height;
        Size(float w = 0, float h = 0) : width(w), height(h) {}
    };

    // 前向声明
    class Label;
    class Menu;
    class MenuWindow;
    class ComboBox;
    class Window;

    // ========== 信号槽机制 ==========
    enum class ConnectionThread {
        CurrentThread,
        NewThread,
        UIThread
    };

    class ConnectionGroup;

    namespace detail {
        inline std::thread::id g_uiThreadId;
        inline HWND g_uiDispatcherWindow = nullptr;
        inline constexpr UINT WM_UI_TASK = WM_APP + 1;
        inline void CloseAllOpenMenus();   // 定义在文件后部（需要 Window 完整类型）

        inline void InitializeUIThread() {
            g_uiThreadId = std::this_thread::get_id();
            if (!g_uiDispatcherWindow) {
                static bool classRegistered = false;
                if (!classRegistered) {
                    WNDCLASSEXW wc = {};
                    wc.cbSize = sizeof(WNDCLASSEXW);
                    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                        if (msg == WM_UI_TASK) {
                            auto* task = reinterpret_cast<std::function<void()>*>(lParam);
                            if (task) {
                                (*task)();
                                delete task;
                            }
                            return 0;
                        }
                        return DefWindowProc(hwnd, msg, wParam, lParam);
                        };
                    wc.hInstance = GetModuleHandle(nullptr);
                    wc.lpszClassName = L"ZufyUI_DispatcherWindow";
                    RegisterClassExW(&wc);
                    classRegistered = true;
                }
                g_uiDispatcherWindow = CreateWindowExW(0, L"ZufyUI_DispatcherWindow", L"",
                    WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
            }
        }

        inline void PostToUIThread(std::function<void()> fn) {
            if (std::this_thread::get_id() == g_uiThreadId) {
                fn();
            }
            else if (g_uiDispatcherWindow) {
                auto* task = new std::function<void()>(std::move(fn));
                PostMessage(g_uiDispatcherWindow, WM_UI_TASK, 0, (LPARAM)task);
            }
        }

        struct ConnectionState {
            std::function<void()> disconnect;
            std::weak_ptr<ConnectionGroup> group;
            std::shared_ptr<bool> alive;
            ConnectionState() : alive(std::make_shared<bool>(true)) {}
        };
    }

    class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    public:
        ConnectionGroup() = default;
        ~ConnectionGroup() { disconnectAll(); }

        ConnectionGroup(const ConnectionGroup&) = delete;
        ConnectionGroup& operator=(const ConnectionGroup&) = delete;

        void disconnectAll() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& weak : connections_) {
                if (auto state = weak.lock()) {
                    if (state->disconnect && state->alive && *state->alive) {
                        state->disconnect();
                    }
                    state->disconnect = nullptr;
                }
            }
            connections_.clear();
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (auto& weak : connections_) {
                if (!weak.expired()) ++count;
            }
            return count;
        }

        void addState(const std::shared_ptr<detail::ConnectionState>& state) {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.push_back(state);
            state->group = weak_from_this();
        }

    private:
        mutable std::mutex mutex_;
        std::vector<std::weak_ptr<detail::ConnectionState>> connections_;
    };

    // Qt 风格的“被动连接句柄”：析构不自动断连。
    // - 自动断连由 ConnectionGroup（元素析构时）负责；
    // - 需要单独断连时显式调用 conn.disconnect()；
    // - 因此 Connect(...) 可以安全地返回它，忽略返回值也没问题。
    class Connection {
    public:
        Connection() = default;
        Connection(std::shared_ptr<detail::ConnectionState> state) : state_(std::move(state)) {}
        ~Connection() = default;                 // 关键：不在这里自动 disconnect

        Connection(const Connection&) = default;
        Connection& operator=(const Connection&) = default;
        Connection(Connection&&) noexcept = default;
        Connection& operator=(Connection&&) noexcept = default;

        void disconnect() {
            if (state_) {
                // 信号已析构时 alive 为 false：绝不能再回调其 disconnect（捕获了已销毁对象的 this）
                if (state_->disconnect && state_->alive && *state_->alive) {
                    state_->disconnect();
                }
                state_->disconnect = nullptr;
                state_.reset();
            }
        }

        bool isConnected() const {
            return state_ && state_->disconnect != nullptr && state_->alive && *state_->alive;
        }
        explicit operator bool() const { return isConnected(); }

    private:
        std::shared_ptr<detail::ConnectionState> state_;
    };

    // 信号类：使用 Fire 避免宏冲突
    template <typename... TArgs>
    class ZSignal {
    public:
        using SlotType = std::function<void(TArgs...)>;

        ZSignal() = default;
        ~ZSignal() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& data : connections_) {
                if (data && data->state && data->state->alive) {
                    *(data->state->alive) = false;
                }
            }
            connections_.clear();
        }

        ZSignal(const ZSignal&) = delete;
        ZSignal& operator=(const ZSignal&) = delete;
        ZSignal(ZSignal&& other) noexcept {
            std::lock_guard<std::mutex> lock(other.mutex_);
            connections_ = std::move(other.connections_);
        }
        ZSignal& operator=(ZSignal&& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> lock1(mutex_);
                std::lock_guard<std::mutex> lock2(other.mutex_);
                connections_ = std::move(other.connections_);
            }
            return *this;
        }

        Connection connect(SlotType slot,
            ConnectionThread thread = ConnectionThread::CurrentThread,
            std::shared_ptr<ConnectionGroup> group = nullptr) {
            auto state = std::make_shared<detail::ConnectionState>();
            auto data = std::make_shared<ConnectionData>(std::move(slot), thread, state);

            state->disconnect = [this, weak = std::weak_ptr<ConnectionData>(data)]() {
                if (auto shared = weak.lock()) {
                    this->removeConnection(shared);
                }
                };

            {
                std::lock_guard<std::mutex> lock(mutex_);
                connections_.push_back(data);
            }

            if (group) {
                group->addState(state);
            }

            return Connection(state);
        }

        void Fire(TArgs... targs) const {
            // 复用线程本地缓冲，避免每次触发都堆分配；
            // 重入（slot 内再次触发同一信号）时退回局部拷贝，保证正确性。
            static thread_local std::vector<std::shared_ptr<ConnectionData>> tlSnapshot;
            static thread_local int tlDepth = 0;
            std::vector<std::shared_ptr<ConnectionData>> reentrant;
            std::vector<std::shared_ptr<ConnectionData>>* snapshot = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (tlDepth == 0) {
                    tlSnapshot.assign(connections_.begin(), connections_.end());
                    snapshot = &tlSnapshot;
                }
                else {
                    reentrant = connections_;
                    snapshot = &reentrant;
                }
                tlDepth++;
            }

            for (auto& data : *snapshot) {
                if (!data || !data->state || !*(data->state->alive)) continue;
                switch (data->thread) {
                case ConnectionThread::CurrentThread:
                    data->slot(targs...);
                    break;
                case ConnectionThread::NewThread: {
                    auto tupleArgs = std::make_tuple(targs...);
                    std::thread([slot = data->slot, tupleArgs = std::move(tupleArgs)]() mutable {
                        std::apply(slot, std::move(tupleArgs));
                        }).detach();
                    break;
                }
                case ConnectionThread::UIThread: {
                    auto tupleArgs = std::make_tuple(targs...);
                    detail::PostToUIThread([slot = data->slot, tupleArgs = std::move(tupleArgs)]() mutable {
                        std::apply(slot, std::move(tupleArgs));
                        });
                    break;
                }
                }
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                tlDepth--;
                if (tlDepth == 0) tlSnapshot.clear();   // 释放最后一次快照持有的 shared_ptr
            }
        }

        void operator()(TArgs... targs) const {
            Fire(std::forward<TArgs>(targs)...);
        }

    private:
        struct ConnectionData {
            SlotType slot;
            ConnectionThread thread;
            std::shared_ptr<detail::ConnectionState> state;

            ConnectionData(SlotType s, ConnectionThread t,
                std::shared_ptr<detail::ConnectionState> st)
                : slot(std::move(s)), thread(t), state(std::move(st)) {}
        };

        void removeConnection(const std::shared_ptr<ConnectionData>& data) {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.erase(
                std::remove(connections_.begin(), connections_.end(), data),
                connections_.end());
        }

        mutable std::mutex mutex_;
        std::vector<std::shared_ptr<ConnectionData>> connections_;
    };

    // ========== 字体管理 ==========
    struct FontSpec {
        std::wstring familyName = L"Segoe UI";
        float size = 14.0f;
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
        std::wstring locale = L"en-us";

        bool operator==(const FontSpec& other) const {
            return familyName == other.familyName &&
                size == other.size &&
                weight == other.weight &&
                style == other.style &&
                stretch == other.stretch &&
                locale == other.locale;
        }
        bool operator!=(const FontSpec& other) const { return !(*this == other); }
    };

    struct FontSpecHash {
        size_t operator()(const FontSpec& spec) const {
            size_t h = std::hash<std::wstring>()(spec.familyName);
            h ^= std::hash<float>()(spec.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.weight) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.style) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.stretch) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::wstring>()(spec.locale) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    // 全局字体管理器：单例，持有唯一的 DWriteFactory 和 IDWriteTextFormat 缓存
    // 全局文本布局缓存 key：文本 + 格式 + 量化后的宽高（0.5 DIP）+ noWrap + mode(0=原始,1=显示)
    struct TextLayoutKey {
        std::wstring text; const void* fmt; int w2, h2; bool noWrap; int mode;
        bool operator==(const TextLayoutKey& o) const {
            return fmt == o.fmt && w2 == o.w2 && h2 == o.h2 && noWrap == o.noWrap && mode == o.mode && text == o.text;
        }
    };
    struct TextLayoutKeyHash {
        size_t operator()(const TextLayoutKey& k) const {
            size_t h = std::hash<std::wstring>{}(k.text);
            h ^= std::hash<const void*>{}(k.fmt) + 0x9e3779b9u + (h << 6) + (h >> 2);
            h ^= (size_t)(k.w2 * 73856093) ^ (size_t)(k.h2 * 19349663) ^ (size_t)(k.noWrap ? 1 : 0) ^ (size_t)(k.mode * 83492791);
            return h;
        }
    };

    class FontManager {
    public:
        static FontManager& Instance() {
            static FontManager instance;
            return instance;
        }

        IDWriteFactory* GetFactory() {
            if (!dwriteFactory_) {
                DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory_);
            }
            return dwriteFactory_.Get();
        }

        // 按 spec 获取共享的 IDWriteTextFormat（带缓存）
        IDWriteTextFormat* GetFormat(const FontSpec& spec) {
            auto it = formatCache_.find(spec);
            if (it != formatCache_.end()) return it->second.Get();

            IDWriteFactory* factory = GetFactory();
            if (!factory) return nullptr;

            ComPtr<IDWriteTextFormat> fmt;
            HRESULT hr = factory->CreateTextFormat(
                spec.familyName.c_str(), nullptr,
                spec.weight, spec.style, spec.stretch,
                spec.size, spec.locale.c_str(), &fmt);
            if (FAILED(hr) || !fmt) return nullptr;

            fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            IDWriteTextFormat* raw = fmt.Get();
            formatCache_.emplace(spec, std::move(fmt));
            return raw;
        }

        // ---- 全局文本布局缓存（跨所有控件共享，避免每帧每单元格 CreateTextLayout）----
        // 安全性依赖 formatCache_ **永不淘汰**（同一 FontSpec 永远返回同一 IDWriteTextFormat*）；
        // 若未来给 formatCache_ 加淘汰，必须同时清空 layoutCache_。
        // 只返回"基础"layout 供绘制，调用方**不得修改**它（需 lineSpacing/trimming 的请自建旁路缓存）。
        IDWriteTextLayout* GetRawLayout(const std::wstring& text, IDWriteTextFormat* fmt,
            float maxWidth, float maxHeight, bool noWrap) {
            return GetOrCreate(text, fmt, maxWidth, maxHeight, noWrap, 0);
        }
        // 显示布局：按【原文本】缓存截断后的最终 layout（命中即跳过整段截断计算）
        IDWriteTextLayout* GetDisplayLayout(const std::wstring& origText, IDWriteTextFormat* fmt,
            float maxWidth, float maxHeight, bool noWrap) {
            if (!fmt || origText.empty() || maxWidth <= 0.0f || maxHeight <= 0.0f) return nullptr;
            auto it = layoutCache_.find(Key(origText, fmt, maxWidth, maxHeight, noWrap, 1));
            return (it != layoutCache_.end() && it->second) ? it->second.Get() : nullptr;
        }
        void CacheDisplayLayout(const std::wstring& origText, IDWriteTextFormat* fmt,
            float maxWidth, float maxHeight, bool noWrap, IDWriteTextLayout* layout) {
            if (!fmt || origText.empty() || !layout) return;
            Store(Key(origText, fmt, maxWidth, maxHeight, noWrap, 1), layout);
        }

        // 设置全局默认字体，触发 GlobalFontChanged 让所有未覆盖的控件重建
        void SetGlobalFont(const FontSpec& spec) {
            globalFont_ = spec;
            GlobalFontChanged();
        }

        const FontSpec& GetGlobalFont() const { return globalFont_; }

        ZSignal<> GlobalFontChanged;

    private:
        FontManager() = default;
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;

        TextLayoutKey Key(const std::wstring& text, IDWriteTextFormat* fmt, float w, float h, bool noWrap, int mode) {
            return TextLayoutKey{ text, fmt, (int)std::lround(w * 2.0f), (int)std::lround(h * 2.0f), noWrap, mode };
        }
        void Store(TextLayoutKey key, IDWriteTextLayout* layout) {
            if (!layout) return;
            auto exist = layoutCache_.find(key);
            if (exist != layoutCache_.end()) { exist->second = layout; return; }   // 已存在：覆盖，不 push fifo（防 map/fifo 不同步）
            if (layoutCache_.size() >= kLayoutCacheMax) {   // 有界：一次淘汰最旧的 1/4
                size_t toDrop = layoutCache_.size() / 4 + 1;
                for (size_t i = 0; i < toDrop && !layoutFifo_.empty(); ++i) {
                    layoutCache_.erase(layoutFifo_.front());
                    layoutFifo_.pop_front();
                }
            }
            layoutCache_[key] = layout;              // ComPtr 赋值会 AddRef
            layoutFifo_.push_back(std::move(key));
        }
        IDWriteTextLayout* GetOrCreate(const std::wstring& text, IDWriteTextFormat* fmt,
            float w, float h, bool noWrap, int mode) {
            if (!fmt || text.empty() || w <= 0.0f || h <= 0.0f) return nullptr;
            TextLayoutKey key = Key(text, fmt, w, h, noWrap, mode);
            auto it = layoutCache_.find(key);
            if (it != layoutCache_.end() && it->second) return it->second.Get();
            IDWriteFactory* factory = GetFactory();
            if (!factory) return nullptr;
            ComPtr<IDWriteTextLayout> layout;
            if (FAILED(factory->CreateTextLayout(text.c_str(), (UINT32)text.length(), fmt, w, h, &layout)) || !layout) return nullptr;
            if (noWrap) layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            Store(std::move(key), layout.Get());
            return layout.Get();
        }

        ComPtr<IDWriteFactory> dwriteFactory_;
        std::unordered_map<FontSpec, ComPtr<IDWriteTextFormat>, FontSpecHash> formatCache_;   // 契约：永不淘汰
        // 全局文本布局缓存（跨控件共享；有界 FIFO）
        static constexpr size_t kLayoutCacheMax = 400;
        std::unordered_map<TextLayoutKey, ComPtr<IDWriteTextLayout>, TextLayoutKeyHash> layoutCache_;
        std::deque<TextLayoutKey> layoutFifo_;
        FontSpec globalFont_;
    };

    class UIElement;  // 前向声明

    namespace UIZSignals {
        // 叠加绘制（参数：所属窗口, 渲染目标）。订阅者必须按窗口过滤，避免把 A 窗口的弹层画到 B 窗口。
        inline ZSignal<Window*, ID2D1RenderTarget*> DrawOverlay;

        // 全局鼠标按下（参数：所在窗口, x, y，单位 dip）
        inline ZSignal<Window*, float, float> GlobalMouseDown;

        // 窗口失去激活（失活或最小化），参数为失活的窗口
        inline ZSignal<Window*> WindowDeactivated;
    inline ZSignal<Window*> WindowActivated;   // 任一窗口被激活（弹窗据此在 owner 激活时改挂为子窗口）

        // 控件请求/释放鼠标捕获（参数：所属窗口, 控件）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*, UIElement*> ElementCaptureRequest;
        inline ZSignal<Window*, UIElement*> ElementCaptureRelease;

        // 重绘请求（参数：所属窗口, 控件）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*, UIElement*> RepaintRequest;

        // 布局失效（参数：所属窗口）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*> LayoutInvalidated;

        // 渲染设备资源被丢弃/重建（设备丢失、DPI 变化、窗口销毁等）。
        // 订阅者应清掉自己按渲染目标/设备缓存的东西（如 ImageManager 的图像位图缓存）。
        inline ZSignal<> DeviceReset;

        // 亚克力/材质参数发生变化时触发：所有窗口重新应用背景（重建 DComp 效果图 / 重绘）。
        // 应用改完 Window::Acrylic* 参数后，调用 Window::SetBackgroundParams(Backdrop::Acrylic, Window::AcrylicParams) 或直接 Fire 本信号。
        inline ZSignal<> ReloadAcrylic;
    }

    // ---------- 基础元素 ----------
    class UIElement {
    public:
        UIElement() : parent_(nullptr), visible_(true), width_(0), height_(0), arrangedRect_(),
            minWidth_(0), minHeight_(0), maxWidth_(FLT_MAX), maxHeight_(FLT_MAX),
            fillWidth_(false), fillHeight_(false),
            connectionGroup_(std::make_shared<ConnectionGroup>()),
            cacheValid_(false), useCache_(true) {
            // 订阅全局字体变更：未覆盖字体的控件自动重建（随本元素的 ConnectionGroup 自动断开）
            FontManager::Instance().GlobalFontChanged.connect(
                [this]() {
                    if (!fontOverride_) OnFontChanged();
                },
                ConnectionThread::CurrentThread,
                connectionGroup_
            );
        }

        virtual ~UIElement() = default;

        // ---------- 布局相关 ----------
        void InvalidateLayout();   // 定义见文件末尾（需要 Window 完整类型才能路由到所属窗口）
        // 布局是否需要重算（第 2 期：两级脏位）
        bool NeedsLayout() const { return measureDirty_ || selfArrangeDirty_ || subtreeDirty_; }

        void SetMinWidth(float w) { minWidth_ = w; InvalidateLayout(); }
        void SetMinHeight(float h) { minHeight_ = h; InvalidateLayout(); }
        void SetMaxWidth(float w) { maxWidth_ = w; InvalidateLayout(); }
        void SetMaxHeight(float h) { maxHeight_ = h; InvalidateLayout(); }
        void SetMinSize(float w, float h) { minWidth_ = w; minHeight_ = h; InvalidateLayout(); }
        void SetMaxSize(float w, float h) { maxWidth_ = w; maxHeight_ = h; InvalidateLayout(); }
        float GetMinWidth() const { return minWidth_; }
        float GetMinHeight() const { return minHeight_; }
        float GetMaxWidth() const { return maxWidth_; }
        float GetMaxHeight() const { return maxHeight_; }

        void SetFillWidth(bool fill) { fillWidth_ = fill; InvalidateLayout(); }
        void SetFillHeight(bool fill) { fillHeight_ = fill; InvalidateLayout(); }
        bool GetFillWidth() const { return fillWidth_; }
        bool GetFillHeight() const { return fillHeight_; }

        void SetMargin(const Thickness& margin) { margin_ = margin; InvalidateLayout(); }
        Thickness GetMargin() const { return margin_; }
        void SetWidth(float width) { width_ = width; InvalidateLayout(); }
        void SetHeight(float height) { height_ = height; InvalidateLayout(); }
        float GetWidth() const { return width_; }
        float GetHeight() const { return height_; }

        void SetStretchWeights(float horizontal, float vertical) {
            horizontalStretchWeight_ = horizontal;
            verticalStretchWeight_ = vertical;
        }
        void SetHorizontalStretchWeight(float weight) { horizontalStretchWeight_ = weight; }
        void SetVerticalStretchWeight(float weight) { verticalStretchWeight_ = weight; }
        float GetHorizontalStretchWeight() const {
            return horizontalStretchWeight_.has_value() ? horizontalStretchWeight_.value() : GetDefaultHorizontalStretchWeight();
        }
        float GetVerticalStretchWeight() const {
            return verticalStretchWeight_.has_value() ? verticalStretchWeight_.value() : GetDefaultVerticalStretchWeight();
        }

        virtual float GetDefaultHorizontalStretchWeight() const { return 0.0f; }
        virtual float GetDefaultVerticalStretchWeight() const { return 0.0f; }

        // Measure：基类统一做 DesiredSize 缓存（子类实现 MeasureOverride）
        Size Measure(const Size& avail) {
            if (!measureDirty_ && avail.width == previousAvailableSize_.width
                && avail.height == previousAvailableSize_.height) return desiredSize_;
            Size newSize = MeasureOverride(avail);
            // 自身自然尺寸真的变了才作废缓存（祖先虽因冒泡也重测，但尺寸没变→不动缓存，避免"子变脏拖累祖先"）
            if (newSize.width != desiredSize_.width || newSize.height != desiredSize_.height)
                cacheValid_ = false;
            desiredSize_ = newSize;
            previousAvailableSize_ = avail;
            measureDirty_ = false;
            return desiredSize_;
        }
        virtual Size MeasureOverride(const Size& availableSize) = 0;
        Size GetDesiredSize() const { return desiredSize_; }

        // Arrange：基类包装。
        //   selfArrangeDirty_ : 我自己要重跑 ArrangeOverride
        //   subtreeDirty_     : 我的子树里有脏节点（要跑 ArrangeOverride 才能到达）
        // 二者都不为真且矩形没变 → 整棵子树跳过；缓存只由"尺寸变化/重测"驱动，位置变化不清缓存。
        void Arrange(const Rect& finalRect) {
            bool rectChanged = !(finalRect.x == arrangedRect_.x && finalRect.y == arrangedRect_.y
                && finalRect.width == arrangedRect_.width && finalRect.height == arrangedRect_.height);
            bool sizeChanged = (finalRect.width != arrangedRect_.width || finalRect.height != arrangedRect_.height);
            if (!selfArrangeDirty_ && !subtreeDirty_ && !rectChanged) return;
            // 仅在"约束变了"时重测（C2）；命中缓存则 O(1)
            if (finalRect.width != previousAvailableSize_.width || finalRect.height != previousAvailableSize_.height)
                Measure(Size(finalRect.width, finalRect.height));
            ArrangeOverride(finalRect);   // arrangedRect_ 由 ArrangeOverride（或其基类默认实现）设置，包装器不再覆盖
            selfArrangeDirty_ = false;
            subtreeDirty_ = false;
            if (sizeChanged) cacheValid_ = false;   // 只有尺寸变化才需要重建缓存（位置变化位图照 blit）
        }
        virtual void ArrangeOverride(const Rect& finalRect) { arrangedRect_ = finalRect; }
        Rect GetArrangedRect() const { return arrangedRect_; }

        // 获取 IME 候选框定位矩形（DIP 坐标）
        virtual Rect GetImeCandidateRect() const {
            return arrangedRect_;
        }
        virtual void SetCompositionText(const std::wstring& text, bool hasComposition, int cursorPos = -1) {}

        // ---------- 绘图 ----------
        virtual void Draw(ID2D1RenderTarget* rt) = 0;

        // ---------- 子元素列表（新增） ----------
        virtual const std::vector<UIElement*>& GetChildren() const { return childrenView_; }

        // ---------- 缓存相关（新增） ----------
        // 控件可重写此方法声明不使用离屏缓存（如动画频繁的控件）
        virtual bool UseCache() const { return useCache_; }
        void SetUseCache(bool use) { useCache_ = use; }

        // 获取元素需要应用于其子元素的裁剪矩形，返回 std::nullopt 表示不裁剪
        virtual std::optional<D2D1_RECT_F> GetClipRect() const {
            return clipRect_ ? std::optional<D2D1_RECT_F>(clipRect_->ToD2D()) : std::nullopt;
        }
        // 设置该元素子树的裁剪矩形（用于 ScrollViewer 把内容裁到视口，避免画到滚动条下）
        void SetClipRect(const std::optional<Rect>& r) { clipRect_ = r; }

        // 请求重绘（仅视觉变化）；定义见文件末尾
        void RequestRepaint();

        // 缓存有效性标记（由 Window 管理，但为了方便检查放在这里）
        bool cacheValid_ = false;
        std::optional<Rect> clipRect_;   // 该元素子树的裁剪矩形（可选）
        ComPtr<ID2D1BitmapRenderTarget> cacheRT_;
        // 缓存尺寸记录
        Size cacheSize_;

        // ---------- 事件 ----------
        virtual UIElement* HitTest(float x, float y) {
            if (visible_ && arrangedRect_.Contains(x, y)) return this;
            return nullptr;
        }

        virtual void OnMouseEnter() {}
        virtual void OnMouseLeave() {}
        virtual void OnMouseMove(float x, float y) {}
        virtual void OnMouseDown(float x, float y) {}
        virtual void OnMouseUp(float x, float y) {}
        // 右键菜单：返回 true 表示控件已自行处理（框架不再弹默认菜单）
        virtual bool OnContextMenu(float x, float y) { return false; }
        virtual void OnKeyDown(WPARAM key, LPARAM lParam) {}
        virtual void OnKeyUp(WPARAM key, LPARAM lParam) {}
        virtual void OnChar(wchar_t ch) {}
        virtual void OnFocus() {}
        virtual void OnBlur() {}
        virtual void UpdateAnimation(float deltaTime) {}
        virtual bool OnMouseWheel(float deltaX, float deltaY) { return false; }
        virtual bool HasActiveAnimation() const { return false; }
        virtual void CollectExpandedComboBoxes(std::vector<ComboBox*>& list) {}

        virtual void ReleaseDeviceResources() {
            // 释放缓存资源
            cacheRT_.Reset();
            cacheValid_ = false;
        }

        // 类型判断辅助
        virtual bool IsTextInput() const { return false; }

        virtual D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const {
            return D2D1::Matrix3x2F::Identity();
        }

        // ---------- 信号连接管理 ----------
        // 连接登记进本元素的 ConnectionGroup，随元素析构自动断开；
        // 同时返回一个 Qt 风格的被动 Connection 句柄：忽略返回值安全（析构不断连），
        // 需要单独断开时调用 `auto c = Connect(...); ... c.disconnect();`。
        template<typename Signal, typename Slot>
        Connection Connect(Signal& signal, Slot&& slot) {
            return signal.connect(std::forward<Slot>(slot), ConnectionThread::CurrentThread, connectionGroup_);
        }

        // 在 UIElement 类内（public 或 protected）
        template<typename T>
        static void ConvergeValue(T& value, const T& target, float epsilon = 0.001f) {
            if (fabs(value - target) < epsilon) {
                value = target;
            }
        }

        // ---------- 事件信号 ----------
        ZSignal<> MouseEnter;
        ZSignal<> MouseLeave;
        ZSignal<float, float> MouseMove;
        ZSignal<float, float> MouseDown;
        ZSignal<float, float> MouseUp;
        ZSignal<WPARAM, LPARAM> KeyDown;
        ZSignal<WPARAM, LPARAM> KeyUp;
        ZSignal<wchar_t> Char;
        ZSignal<> Focused;
        ZSignal<> Blurred;

        virtual bool IsFocusable() const { return false; }

        // 窗口标题变化时通知（自定义标题栏元素可重写以同步显示；默认无操作）
        virtual void SetWindowTitle(const std::wstring&) {}
        // 窗口图标下发给“非参与布局”的窗口级控件（如 TitleBar 显示的应用图标）
        virtual void SetWindowIconFromHICON(HICON) {}

        // ---------- 父子关系 ----------
        void SetParent(UIElement* parent) {
            if (parent) parent->childrenDirty_ = true;   // 新挂载 → 父的子元素列表缓存失效
            parent_ = parent;
            // 挂到已属于某窗口的父级时，立即把自己的子树也归属到该窗口；
            // 若父级尚未挂载，则等父级挂载时由 AttachWindowRecursive 统一传播。
            AttachWindowRecursive(parent ? parent->GetWindow() : nullptr);
        }
        UIElement* GetParent() const { return parent_; }
        void MarkChildrenDirty() { childrenDirty_ = true; }   // 子元素列表变化时手动置脏（如 PageHost 的动画状态）

        // 所属窗口（挂载到窗口的树后由框架设置；未挂载或窗口已销毁时返回 nullptr）
        // 说明：内部用“窗口 id”而非裸指针保存归属，窗口销毁后查找返回 nullptr，
        //       从根本上避免“元素持有已销毁窗口指针”导致的悬垂崩溃。
        Window* GetWindow() const;
        static int WindowIdOf(Window* w);
        // 把“所属窗口”沿子树传播；容器需重写以递归自己的子元素
        virtual void AttachWindowRecursive(Window* w) { windowId_ = WindowIdOf(w); }
        void SetVisible(bool visible) {
            if (visible_ != visible) {
                visible_ = visible;
                if (!visible_) {
                    cacheRT_.Reset();
                    cacheValid_ = false;
                }
                OnVisibilityChanged(visible);
                InvalidateLayout();
            }
        }
        // 设置可见性但不触发布局失效：仅供 Arrange 内部使用，避免布局过程中再次标脏导致布局循环
        void SetVisibleNoInvalidate(bool visible) {
            if (visible_ != visible) {
                visible_ = visible;
                if (!visible_) {
                    cacheRT_.Reset();
                    cacheValid_ = false;
                }
                OnVisibilityChanged(visible);
            }
        }
        // 可见性变化钩子：展开类控件（如 ComboBox）应在隐藏时收起自身弹层
        virtual void OnVisibilityChanged(bool /*visible*/) {}
        bool IsVisible() const { return visible_; }
        void SetContextMenu(std::shared_ptr<Menu> menu) { contextMenu_ = menu; }
        std::shared_ptr<Menu> GetContextMenu() const { return contextMenu_; }
        // 设置出血尺寸（单位：DIP），影响缓存大小和贴图偏移
        void SetBleed(float bleed) { bleed_ = max(0.0f, bleed); }
        float GetBleed() const { return bleed_; }

        // ---------- 布局参与 / 绘制时机 ----------
        // Normal：参与布局（默认，现有行为）。
        // DrawBeforeLayout：不参与布局；在正常布局树绘制之前单独绘制（位置由设置者直接给）。
        // DrawAfterLayout：不参与布局；在正常布局树绘制之后、覆盖层之前单独绘制。
        enum class LayoutParticipation { Normal, DrawBeforeLayout, DrawAfterLayout };
        void SetLayoutParticipation(LayoutParticipation p) {
            if (layoutParticipation_ != p) { layoutParticipation_ = p; InvalidateLayout(); RequestRepaint(); }
        }
        LayoutParticipation GetLayoutParticipation() const { return layoutParticipation_; }
        bool ParticipatesInLayout() const { return layoutParticipation_ == LayoutParticipation::Normal; }

        // ---------- 拖动区域（供 Window 收集 → WM_NCHITTEST 返回 HTCAPTION）----------
        void SetDraggable(bool on) { draggableWhole_ = on; if (on) dragRegion_ = Rect(); }
        void SetDragRegion(const Rect& localRect) { dragRegion_ = localRect; draggableWhole_ = false; }
        void ClearDragRegion() { draggableWhole_ = false; dragRegion_ = Rect(); }
        bool HasDragRegion() const { return draggableWhole_ || dragRegion_.width > 0.0f; }
        Rect GetDragRegion() const {
            if (draggableWhole_) return arrangedRect_;
            return Rect(arrangedRect_.x + dragRegion_.x, arrangedRect_.y + dragRegion_.y,
                dragRegion_.width, dragRegion_.height);
        }
        bool IsPointInDragRegion(float x, float y) const {
            if (!visible_ || !HasDragRegion()) return false;
            return GetDragRegion().Contains(x, y);
        }
        // 收集自身拖动区域到 out（客户坐标，DIP）；Window 递归调用（用 GetChildren 遍历）
        virtual void CollectDragRegions(std::vector<Rect>& out) const {
            if (visible_ && HasDragRegion()) out.push_back(GetDragRegion());
        }

        // 非客户区命中覆盖：自定义标题栏把按钮报告为 HTMINBUTTON/HTMAXBUTTON/HTCLOSE，
        // 从而启用系统行为（核心是“最大化按钮悬浮出现 Snap Layouts”由资源管理器渲染）。返回 0 表示不用。
        virtual int NonClientHitTest(float /*x*/, float /*y*/) const { return 0; }
        // 非客户区按钮的按下视觉（由 Window 在 WM_NCLBUTTON* 时调用）
        virtual void SetNonClientButtonPressed(int /*hit*/, bool /*pressed*/) {}

        // ---------- 禁用态 ----------
        void SetEnabled(bool enabled) {
            if (enabled_ != enabled) { enabled_ = enabled; cacheValid_ = false; RequestRepaint(); }
        }
        bool IsEnabled() const { return enabled_; }
        // 考虑父链的实际可用性
        bool IsEffectivelyEnabled() const {
            const UIElement* p = this;
            while (p) { if (!p->enabled_) return false; p = p->parent_; }
            return true;
        }

        // ---------- 悬停提示 ----------
        void SetToolTip(const std::wstring& text) { tooltip_ = text; }
        virtual std::wstring GetToolTip() const { return tooltip_; }

        // ---------- 阴影（默认关闭；元素设置，Window 合成进缓存）----------
        void SetShadow(bool enable) { shadowEnabled_ = enable; cacheValid_ = false; RequestRepaint(); }
        bool HasShadow() const { return shadowEnabled_; }
        void SetShadowColor(Color c) { shadowColor_ = c.ToD2D(); cacheValid_ = false; RequestRepaint(); }
        void SetShadowBlur(float blur) { shadowBlur_ = max(0.0f, blur); cacheValid_ = false; RequestRepaint(); }
        void SetShadowOffset(float x, float y) { shadowOffsetX_ = x; shadowOffsetY_ = y; cacheValid_ = false; RequestRepaint(); }
        void SetShadowCornerRadius(float r) { shadowCornerRadius_ = r; cacheValid_ = false; RequestRepaint(); }
        D2D1_COLOR_F GetShadowColor() const { return shadowColor_; }
        float GetShadowBlur() const { return shadowBlur_; }
        float GetShadowOffsetX() const { return shadowOffsetX_; }
        float GetShadowOffsetY() const { return shadowOffsetY_; }
        float GetShadowCornerRadius() const { return shadowCornerRadius_; }
        float GetShadowExtent() const {
            return shadowEnabled_ ? (shadowBlur_ * 2.0f + max(fabs(shadowOffsetX_), fabs(shadowOffsetY_))) : 0.0f;
        }
        bool enabled_ = true;
        std::wstring tooltip_;
        bool shadowEnabled_ = false;
        D2D1_COLOR_F shadowColor_ = D2D1::ColorF(0.0f, 0.0f, 0.02f, 0.42f);
        float shadowBlur_ = 10.0f;
        float shadowOffsetX_ = 0.0f;
        float shadowOffsetY_ = 3.0f;
        float shadowCornerRadius_ = -1.0f;

        // 缓存内容实际绘制的原点（相对于缓存位图左上角）
        float cacheOriginX_ = 0.0f;
        float cacheOriginY_ = 0.0f;

        // ---------- 字体接口 ----------
        // 全局字体控制（转发到 FontManager）
        static void SetGlobalFont(const FontSpec& spec) { FontManager::Instance().SetGlobalFont(spec); }
        static void SetGlobalFontFamily(const std::wstring& family) {
            FontSpec s = FontManager::Instance().GetGlobalFont();
            s.familyName = family;
            FontManager::Instance().SetGlobalFont(s);
        }
        static void SetGlobalFontSize(float size) {
            FontSpec s = FontManager::Instance().GetGlobalFont();
            s.size = size;
            FontManager::Instance().SetGlobalFont(s);
        }
        static FontSpec GetGlobalFont() { return FontManager::Instance().GetGlobalFont(); }

        // 实例级字体覆盖
        void SetFont(const FontSpec& spec) {
            fontOverride_ = spec;
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }
        void SetFontFamily(const std::wstring& family) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.familyName = family;
            SetFont(spec);
        }
        void SetFontSize(float size) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.size = size;
            SetFont(spec);
        }
        void SetFontWeight(DWRITE_FONT_WEIGHT weight) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.weight = weight;
            SetFont(spec);
        }
        void ClearFont() {
            fontOverride_.reset();
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }
        bool HasFontOverride() const { return fontOverride_.has_value(); }

        // 子类可重写：返回该类型的默认字体（例如 Label 用 16，TextBox 用 14）
        virtual std::optional<FontSpec> GetTypeDefaultFont() const { return std::nullopt; }

        // 字体解析链：实例覆盖 → 类型默认 → 全局默认
        FontSpec GetEffectiveFontSpec() const {
            if (fontOverride_) return *fontOverride_;
            if (auto t = GetTypeDefaultFont()) return *t;
            return FontManager::Instance().GetGlobalFont();
        }

        // 获取共享的 IDWriteTextFormat（带每实例缓存，快速命中时零开销）
        IDWriteTextFormat* GetFontFormat() const {
            FontSpec spec = GetEffectiveFontSpec();
            if (cachedFormatValid_ && cachedSpec_ == spec && cachedFormatRaw_) {
                return cachedFormatRaw_;
            }
            cachedFormatRaw_ = FontManager::Instance().GetFormat(spec);
            cachedSpec_ = spec;
            cachedFormatValid_ = (cachedFormatRaw_ != nullptr);
            return cachedFormatRaw_;
        }

        // 字体变化时的回调（默认：失效缓存 + 重新布局）
        virtual void OnFontChanged() {
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }

    protected:
        float bleed_ = 4.0f;  // 离屏缓存出血尺寸（四周额外空间）
        UIElement* parent_;
        bool visible_;
        Thickness margin_;
        float width_;
        float height_;
        Rect arrangedRect_;
        float minWidth_, minHeight_, maxWidth_, maxHeight_;
        bool fillWidth_, fillHeight_;
        Size desiredSize_{};                                  // Measure 输出（DesiredSize 缓存）
        Size previousAvailableSize_{ -1.0f, -1.0f };          // 上轮测量可用的尺寸
        bool measureDirty_ = true;
        bool selfArrangeDirty_ = true;    // 自身需要重跑 ArrangeOverride
        bool subtreeDirty_ = false;       // 子树里有脏节点（需跑 ArrangeOverride 到达）
        std::shared_ptr<Menu> contextMenu_;
        std::optional<float> horizontalStretchWeight_;
        std::optional<float> verticalStretchWeight_;

        LayoutParticipation layoutParticipation_ = LayoutParticipation::Normal;
        bool draggableWhole_ = false;
        Rect dragRegion_{ 0, 0, 0, 0 };

        std::shared_ptr<ConnectionGroup> connectionGroup_;

        bool useCache_; // 默认 true，可被重写
        int windowId_ = 0;  // 所属窗口 id（0 表示未挂载）；用 id 而非裸指针，避免窗口销毁后悬垂
        mutable std::vector<UIElement*> childrenView_; // GetChildren 复用的视图缓冲，避免每帧分配
        mutable bool childrenDirty_ = true;            // 子元素列表变了才重建 childrenView_（由 SetParent 置脏）

        // ---------- 字体相关成员 ----------
        std::optional<FontSpec> fontOverride_;
        mutable IDWriteTextFormat* cachedFormatRaw_ = nullptr;
        mutable FontSpec cachedSpec_;
        mutable bool cachedFormatValid_ = false;
    };

    // ---------- 布局基类 ----------
    class Layout : public UIElement {
    public:
        virtual ~Layout() = default;
        // 布局容器默认不使用缓存
        bool UseCache() const override { return false; }
    };

    // ---------- 垂直布局容器 ----------
    class ColumnBox : public Layout {
    public:
        ColumnBox() : spacing_(0) {}
        void AddChild(std::shared_ptr<UIElement> child) {
            children_.push_back(child);
            child->SetParent(this);
            InvalidateLayout();
        }
        void ClearChildren() {
            for (auto& c : children_) if (c) c->SetParent(nullptr);
            children_.clear();
            InvalidateLayout();
        }
        void SetSpacing(float spacing) { spacing_ = spacing; InvalidateLayout(); }
        float GetSpacing() const { return spacing_; }

        float GetDefaultHorizontalStretchWeight() const override { return 1.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 0.0f; }

        Size MeasureOverride(const Size& availableSize) override {
            float totalHeight = 0;
            float maxWidth = 0;
            for (auto& child : children_) {
                if (!child->IsVisible() || !child->ParticipatesInLayout()) continue;
                Thickness m = child->GetMargin();
                float availW = availableSize.width - m.left - m.right;
                float childW = child->GetWidth() > 0 ? child->GetWidth() : availW;
                if (child->GetFillWidth()) childW = availW;
                childW = clamp(childW, child->GetMinWidth(), child->GetMaxWidth());
                Size childSize = child->Measure(Size(childW, FLT_MAX));   // 与 Arrange 同一约束，使 GetDesiredSize 可信
                totalHeight += childSize.height;
                maxWidth = max(maxWidth, childSize.width + m.left + m.right);
                totalHeight += m.top + m.bottom;
            }
            if (!children_.empty()) totalHeight += spacing_ * (children_.size() - 1);
            return Size(width_ > 0 ? width_ : maxWidth, height_ > 0 ? height_ : totalHeight);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            float y = finalRect.y;
            for (auto& child : children_) {
                if (!child->IsVisible() || !child->ParticipatesInLayout()) continue;
                Thickness margin = child->GetMargin();
                y += margin.top;
                float availW = finalRect.width - margin.left - margin.right;
                float childW = child->GetWidth() > 0 ? child->GetWidth() : availW;
                if (child->GetFillWidth()) childW = availW;
                childW = clamp(childW, child->GetMinWidth(), child->GetMaxWidth());
                Size childSize = child->GetDesiredSize();   // 用测量缓存，避免 Arrange 内重复 Measure
                float childH = child->GetHeight() > 0 ? child->GetHeight() : childSize.height;
                if (child->GetFillHeight()) childH = finalRect.height - y;
                childH = clamp(childH, child->GetMinHeight(), child->GetMaxHeight());
                child->Arrange(Rect(finalRect.x + margin.left, y, childW, childH));
                y += childH + margin.bottom + spacing_;
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 布局容器自身无视觉内容，不绘制子元素（由 Window 合成）
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& child : children_) childrenView_.push_back(child.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& child : children_) child->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
                if (UIElement* hit = (*it)->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& child : children_) child->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& child : children_) {
                if (child->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& child : children_) child->ReleaseDeviceResources();
        }

    private:
        std::vector<std::shared_ptr<UIElement>> children_;
        float spacing_;
    };

    // ---------- 水平布局容器 ----------
    class RowBox : public Layout {
    public:
        RowBox() : spacing_(0) {}
        void AddChild(std::shared_ptr<UIElement> child) {
            children_.push_back(child);
            child->SetParent(this);
            InvalidateLayout();
        }
        void ClearChildren() {
            for (auto& c : children_) if (c) c->SetParent(nullptr);
            children_.clear();
            InvalidateLayout();
        }
        void SetSpacing(float spacing) { spacing_ = spacing; InvalidateLayout(); }
        float GetSpacing() const { return spacing_; }

        float GetDefaultHorizontalStretchWeight() const override { return 0.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 1.0f; }

        Size MeasureOverride(const Size& availableSize) override {
            float totalWidth = 0;
            float maxHeight = 0;
            for (auto& child : children_) {
                if (!child->IsVisible() || !child->ParticipatesInLayout()) continue;
                Thickness m = child->GetMargin();
                float availH = availableSize.height - m.top - m.bottom;
                float childH = child->GetHeight() > 0 ? child->GetHeight() : availH;
                if (child->GetFillHeight()) childH = availH;
                childH = clamp(childH, child->GetMinHeight(), child->GetMaxHeight());
                Size childSize = child->Measure(Size(FLT_MAX, childH));   // 与 Arrange 同一约束
                totalWidth += childSize.width;
                maxHeight = max(maxHeight, childSize.height + m.top + m.bottom);
                totalWidth += m.left + m.right;
            }
            if (!children_.empty()) totalWidth += spacing_ * (children_.size() - 1);
            return Size(width_ > 0 ? width_ : totalWidth, height_ > 0 ? height_ : maxHeight);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            float x = finalRect.x;
            for (auto& child : children_) {
                if (!child->IsVisible() || !child->ParticipatesInLayout()) continue;
                Thickness margin = child->GetMargin();
                x += margin.left;
                float availH = finalRect.height - margin.top - margin.bottom;
                float childH = child->GetHeight() > 0 ? child->GetHeight() : availH;
                if (child->GetFillHeight()) childH = availH;
                childH = clamp(childH, child->GetMinHeight(), child->GetMaxHeight());
                Size childSize = child->GetDesiredSize();   // 用测量缓存，避免 Arrange 内重复 Measure
                float childW = child->GetWidth() > 0 ? child->GetWidth() : childSize.width;
                if (child->GetFillWidth()) childW = finalRect.width - x;
                childW = clamp(childW, child->GetMinWidth(), child->GetMaxWidth());
                child->Arrange(Rect(x, finalRect.y + margin.top, childW, childH));
                x += childW + margin.right + spacing_;
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 无自身视觉内容
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& child : children_) childrenView_.push_back(child.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& child : children_) child->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
                if (UIElement* hit = (*it)->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& child : children_) child->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& child : children_) {
                if (child->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& child : children_) child->ReleaseDeviceResources();
        }

    private:
        std::vector<std::shared_ptr<UIElement>> children_;
        float spacing_;
    };

    // ---------- 网格布局 ----------
    class GridLayout : public Layout {
    public:
        enum class Alignment { Start, Center, End };

        struct GridItem {
            std::shared_ptr<UIElement> element;
            int row, col;
            int rowSpan, colSpan;
        };

        GridLayout() : horizontalSpacing_(0), verticalSpacing_(0),
            horizontalAlignment_(Alignment::Start), verticalAlignment_(Alignment::Start) {}

        void AddChild(std::shared_ptr<UIElement> child, int row, int col, int rowSpan = 1, int colSpan = 1) {
            GridItem item;
            item.element = child;
            item.row = row;
            item.col = col;
            item.rowSpan = max(1, rowSpan);
            item.colSpan = max(1, colSpan);
            items_.push_back(item);
            child->SetParent(this);
            InvalidateLayout();
        }

        void SetSpacing(float horizontal, float vertical) {
            horizontalSpacing_ = horizontal;
            verticalSpacing_ = vertical;
            InvalidateLayout();
        }

        void SetColumnStretch(int col, float weight) {
            if (col >= 0) {
                if (colStretch_.size() <= col) colStretch_.resize(col + 1, 0.0f);
                colStretch_[col] = weight;
                InvalidateLayout();
            }
        }
        void SetRowStretch(int row, float weight) {
            if (row >= 0) {
                if (rowStretch_.size() <= row) rowStretch_.resize(row + 1, 0.0f);
                rowStretch_[row] = weight;
                InvalidateLayout();
            }
        }

        void SetHorizontalAlignment(Alignment align) { horizontalAlignment_ = align; InvalidateLayout(); }
        void SetVerticalAlignment(Alignment align) { verticalAlignment_ = align; InvalidateLayout(); }

        float GetDefaultHorizontalStretchWeight() const override { return 1.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 1.0f; }

        Size MeasureOverride(const Size& availableSize) override {
            int maxRow = 0, maxCol = 0;
            for (auto& item : items_) {
                if (!item.element->ParticipatesInLayout()) continue;
                maxRow = max(maxRow, item.row + item.rowSpan);
                maxCol = max(maxCol, item.col + item.colSpan);
            }
            if (maxRow == 0 || maxCol == 0) return Size(0, 0);

            std::vector<float> rowHeights(maxRow, 0.0f);
            std::vector<float> colWidths(maxCol, 0.0f);

            for (auto& item : items_) {
                if (!item.element->IsVisible() || !item.element->ParticipatesInLayout()) continue;
                Size childSize = item.element->Measure(availableSize);
                float heightPerRow = childSize.height / item.rowSpan;
                float widthPerCol = childSize.width / item.colSpan;
                for (int r = item.row; r < item.row + item.rowSpan; ++r) rowHeights[r] = max(rowHeights[r], heightPerRow);
                for (int c = item.col; c < item.col + item.colSpan; ++c) colWidths[c] = max(colWidths[c], widthPerCol);
            }

            float totalWidth = (maxCol > 1) ? (maxCol - 1) * horizontalSpacing_ : 0;
            for (float w : colWidths) totalWidth += w;
            float totalHeight = (maxRow > 1) ? (maxRow - 1) * verticalSpacing_ : 0;
            for (float h : rowHeights) totalHeight += h;
            return Size(totalWidth, totalHeight);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            int maxRow = 0, maxCol = 0;
            for (auto& item : items_) {
                if (!item.element->ParticipatesInLayout()) continue;
                maxRow = max(maxRow, item.row + item.rowSpan);
                maxCol = max(maxCol, item.col + item.colSpan);
            }
            if (maxRow == 0 || maxCol == 0) return;

            std::vector<float> rowMinHeights(maxRow, 0.0f);
            std::vector<float> colMinWidths(maxCol, 0.0f);
            for (auto& item : items_) {
                if (!item.element->IsVisible() || !item.element->ParticipatesInLayout()) continue;
                Size childSize = item.element->Measure(Size(FLT_MAX, FLT_MAX));
                float hPerRow = childSize.height / item.rowSpan;
                float wPerCol = childSize.width / item.colSpan;
                for (int r = item.row; r < item.row + item.rowSpan; ++r)
                    rowMinHeights[r] = max(rowMinHeights[r], hPerRow);
                for (int c = item.col; c < item.col + item.colSpan; ++c)
                    colMinWidths[c] = max(colMinWidths[c], wPerCol);
            }

            float minTotalHeight = (maxRow > 1) ? (maxRow - 1) * verticalSpacing_ : 0;
            for (float h : rowMinHeights) minTotalHeight += h;
            float minTotalWidth = (maxCol > 1) ? (maxCol - 1) * horizontalSpacing_ : 0;
            for (float w : colMinWidths) minTotalWidth += w;

            float extraHeight = max(0.0f, finalRect.height - minTotalHeight);
            float extraWidth = max(0.0f, finalRect.width - minTotalWidth);

            std::vector<float> rowHeights = rowMinHeights;
            std::vector<float> colWidths = colMinWidths;

            std::vector<float> rowStretchWeights(maxRow, 0.0f);
            std::vector<float> colStretchWeights(maxCol, 0.0f);

            if (!rowStretch_.empty()) {
                for (int r = 0; r < maxRow; ++r)
                    if (r < rowStretch_.size()) rowStretchWeights[r] = rowStretch_[r];
            }
            else {
                for (auto& item : items_) {
                    if (!item.element->IsVisible() || !item.element->ParticipatesInLayout()) continue;
                    float weight = item.element->GetVerticalStretchWeight();
                    for (int r = item.row; r < item.row + item.rowSpan; ++r)
                        rowStretchWeights[r] = max(rowStretchWeights[r], weight);
                }
            }

            if (!colStretch_.empty()) {
                for (int c = 0; c < maxCol; ++c)
                    if (c < colStretch_.size()) colStretchWeights[c] = colStretch_[c];
            }
            else {
                for (auto& item : items_) {
                    if (!item.element->IsVisible() || !item.element->ParticipatesInLayout()) continue;
                    float weight = item.element->GetHorizontalStretchWeight();
                    for (int c = item.col; c < item.col + item.colSpan; ++c)
                        colStretchWeights[c] = max(colStretchWeights[c], weight);
                }
            }

            if (extraHeight > 0) {
                float totalWeight = 0.0f;
                for (float w : rowStretchWeights) totalWeight += w;
                if (totalWeight > 0) {
                    for (int r = 0; r < maxRow; ++r) {
                        if (rowStretchWeights[r] > 0)
                            rowHeights[r] += extraHeight * (rowStretchWeights[r] / totalWeight);
                    }
                }
            }
            else if (minTotalHeight > 0) {
                float scale = finalRect.height / minTotalHeight;
                for (auto& h : rowHeights) h *= scale;
            }

            if (extraWidth > 0) {
                float totalWeight = 0.0f;
                for (float w : colStretchWeights) totalWeight += w;
                if (totalWeight > 0) {
                    for (int c = 0; c < maxCol; ++c) {
                        if (colStretchWeights[c] > 0)
                            colWidths[c] += extraWidth * (colStretchWeights[c] / totalWeight);
                    }
                }
            }
            else if (minTotalWidth > 0) {
                float scale = finalRect.width / minTotalWidth;
                for (auto& w : colWidths) w *= scale;
            }

            float offsetX = 0.0f, offsetY = 0.0f;
            if (extraWidth > 0 && colStretchWeights.empty() && maxCol > 1) {
                switch (horizontalAlignment_) {
                case Alignment::Start: offsetX = 0.0f; break;
                case Alignment::Center: offsetX = extraWidth / 2.0f; break;
                case Alignment::End: offsetX = extraWidth; break;
                }
            }
            if (extraHeight > 0 && rowStretchWeights.empty() && maxRow > 1) {
                switch (verticalAlignment_) {
                case Alignment::Start: offsetY = 0.0f; break;
                case Alignment::Center: offsetY = extraHeight / 2.0f; break;
                case Alignment::End: offsetY = extraHeight; break;
                }
            }

            std::vector<float> rowY(maxRow);
            std::vector<float> colX(maxCol);
            float y = finalRect.y + offsetY;
            for (int r = 0; r < maxRow; ++r) {
                rowY[r] = y;
                y += rowHeights[r];
                if (r < maxRow - 1) y += verticalSpacing_;
            }
            float x = finalRect.x + offsetX;
            for (int c = 0; c < maxCol; ++c) {
                colX[c] = x;
                x += colWidths[c];
                if (c < maxCol - 1) x += horizontalSpacing_;
            }

            for (auto& item : items_) {
                if (!item.element->IsVisible() || !item.element->ParticipatesInLayout()) continue;
                float itemX = colX[item.col];
                float itemY = rowY[item.row];
                float itemW = 0;
                for (int c = item.col; c < item.col + item.colSpan; ++c) {
                    itemW += colWidths[c];
                    if (c < item.col + item.colSpan - 1) itemW += horizontalSpacing_;
                }
                float itemH = 0;
                for (int r = item.row; r < item.row + item.rowSpan; ++r) {
                    itemH += rowHeights[r];
                    if (r < item.row + item.rowSpan - 1) itemH += verticalSpacing_;
                }
                item.element->Arrange(Rect(itemX, itemY, itemW, itemH));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 无自身视觉内容
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& item : items_) childrenView_.push_back(item.element.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& item : items_) if (item.element) item.element->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
                if (UIElement* hit = it->element->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& item : items_) item.element->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& item : items_) {
                if (item.element->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& item : items_) item.element->ReleaseDeviceResources();
        }

    private:
        std::vector<GridItem> items_;
        float horizontalSpacing_;
        float verticalSpacing_;
        std::vector<float> colStretch_;
        std::vector<float> rowStretch_;
        Alignment horizontalAlignment_;
        Alignment verticalAlignment_;
    };

    // ---------- 可替换布局容器基类 ----------
    class LayoutHost : public UIElement {
    public:
        LayoutHost() {
            auto grid = std::make_shared<GridLayout>();
            grid->SetParent(this);
            layout_ = grid;
        }
        virtual ~LayoutHost() = default;

        std::shared_ptr<UIElement> GetLayout() const { return layout_; }

        void SetLayout(std::shared_ptr<UIElement> layout) {
            if (!layout || layout.get() == this) return;
            layout_ = layout;
            layout_->SetParent(this);
            InvalidateLayout();
        }

        template<typename T>
        std::shared_ptr<T> GetLayoutAs() const {
            return std::dynamic_pointer_cast<T>(layout_);
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            if (layout_) childrenView_.push_back(layout_.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            if (layout_) layout_->AttachWindowRecursive(w);
        }

        bool UseCache() const override { return false; }

    protected:
        std::shared_ptr<UIElement> layout_;
    };

    // ---------- 卡片控件 ----------
    class Card : public LayoutHost {
    public:
        inline static float DefaultPadding = 12.0f;
        inline static float DefaultCornerRadius = 8.0f;
        inline static Color DefaultBgColor = Color::FromArgb(255, 255, 255, 255);
        inline static Color DefaultBorderColor = Color::FromArgb(255, 200, 200, 200);
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        Card() : padding_(DefaultPadding), cornerRadius_(DefaultCornerRadius),
            bgColor_(DefaultBgColor), borderColor_(DefaultBorderColor),
            hoverBgColor_(DefaultBgColor), hoverBorderColor_(DefaultBorderColor) {}

        void SetPadding(float padding) { padding_ = padding; InvalidateLayout(); }
        void SetCornerRadius(float radius) { cornerRadius_ = radius; RequestRepaint(); }
        void SetBackgroundColor(Color color) { bgColor_ = color; bgBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color; borderBrush_.Reset(); RequestRepaint(); }
        void SetHoverBackgroundColor(Color color) { hoverBgColor_ = color; RequestRepaint(); }
        void SetHoverBorderColor(Color color) { hoverBorderColor_ = color; RequestRepaint(); }
        void SetHoverAnimationSpeed(float speed) { hoverSpeed_ = speed; }

        static void SetDefaultPadding(float padding) { DefaultPadding = padding; }
        static void SetDefaultCornerRadius(float radius) { DefaultCornerRadius = radius; }
        static void SetDefaultBgColor(Color color) { DefaultBgColor = color; }
        static void SetDefaultBorderColor(Color color) { DefaultBorderColor = color; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size MeasureOverride(const Size& availableSize) override {
            if (!layout_) return Size(0, 0);
            Size childSize = layout_->Measure(Size(availableSize.width - padding_ * 2, availableSize.height - padding_ * 2));
            return Size(childSize.width + padding_ * 2, childSize.height + padding_ * 2);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (layout_) {
                layout_->Arrange(Rect(finalRect.x + padding_, finalRect.y + padding_, finalRect.width - padding_ * 2, finalRect.height - padding_ * 2));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            bool en = IsEffectivelyEnabled();

            D2D1_COLOR_F bg = en ? D2D1::ColorF(
                bgColor_.r + (hoverBgColor_.r - bgColor_.r) * hoverProgress_,
                bgColor_.g + (hoverBgColor_.g - bgColor_.g) * hoverProgress_,
                bgColor_.b + (hoverBgColor_.b - bgColor_.b) * hoverProgress_,
                bgColor_.a + (hoverBgColor_.a - bgColor_.a) * hoverProgress_)
                : D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
            if (!bgBrush_) rt->CreateSolidColorBrush(bg, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bg);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), cornerRadius_, cornerRadius_), bgBrush_.Get());

            D2D1_COLOR_F bd = en ? D2D1::ColorF(
                borderColor_.r + (hoverBorderColor_.r - borderColor_.r) * hoverProgress_,
                borderColor_.g + (hoverBorderColor_.g - borderColor_.g) * hoverProgress_,
                borderColor_.b + (hoverBorderColor_.b - borderColor_.b) * hoverProgress_,
                borderColor_.a + (hoverBorderColor_.a - borderColor_.a) * hoverProgress_)
                : D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
            if (!borderBrush_) rt->CreateSolidColorBrush(bd, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(bd);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), cornerRadius_, cornerRadius_), borderBrush_.Get(), 1.0f);

            // 不再绘制 layout_，子元素由 Window 合成
        }

        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); MouseLeave.Fire(); }

        UIElement* HitTest(float x, float y) override {
            if (visible_ && arrangedRect_.Contains(x, y)) {
                if (layout_) if (UIElement* hit = layout_->HitTest(x, y)) return hit;
                return this;
            }
            return nullptr;
        }

        void UpdateAnimation(float deltaTime) override {
            float target = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < target) { hoverProgress_ += hoverSpeed_ * deltaTime; if (hoverProgress_ > target) hoverProgress_ = target; RequestRepaint(); }
            else if (hoverProgress_ > target) { hoverProgress_ -= hoverSpeed_ * deltaTime; if (hoverProgress_ < target) hoverProgress_ = target; RequestRepaint(); }
            if (layout_) layout_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            bool hoverAnim = (hovered_ ? (hoverProgress_ < 0.999f) : (hoverProgress_ > 0.001f));
            return hoverAnim || (layout_ ? layout_->HasActiveAnimation() : false);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            borderBrush_.Reset();
            UIElement::ReleaseDeviceResources();
            if (layout_) layout_->ReleaseDeviceResources();
        }

        bool UseCache() const override { return HasShadow(); }

    private:
        float padding_;
        float cornerRadius_;
        Color bgColor_;
        Color borderColor_;
        Color hoverBgColor_;
        Color hoverBorderColor_;
        float hoverProgress_ = 0.0f;
        bool hovered_ = false;
        float hoverSpeed_ = 8.0f;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
    };

    // ---------- 页面（Page） ----------
    class Page : public LayoutHost {
    public:
        inline static float DefaultPadding = 10.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        Page() : padding_(DefaultPadding), backgroundColor_(Color(0, 0, 0, 0)) {}

        void SetPadding(float padding) { padding_ = padding; InvalidateLayout(); }
        void SetBackgroundColor(Color color) { backgroundColor_ = color; bgBrush_.Reset(); RequestRepaint(); }

        static void SetDefaultPadding(float padding) { DefaultPadding = padding; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size MeasureOverride(const Size& availableSize) override {
            if (!layout_) return Size(0, 0);
            Size childSize = layout_->Measure(Size(availableSize.width - padding_ * 2, availableSize.height - padding_ * 2));
            return Size(childSize.width + padding_ * 2, childSize.height + padding_ * 2);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            if (layout_) {
                layout_->Arrange(Rect(finalRect.x + padding_, finalRect.y + padding_, finalRect.width - padding_ * 2, finalRect.height - padding_ * 2));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            if (backgroundColor_.a > 0.0f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_.ToD2D(), bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(backgroundColor_.ToD2D());
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            // 不绘制 layout_
        }

        UIElement* HitTest(float x, float y) override {
            if (visible_ && arrangedRect_.Contains(x, y)) {
                if (layout_) if (UIElement* hit = layout_->HitTest(x, y)) return hit;
                return this;
            }
            return nullptr;
        }

        void UpdateAnimation(float deltaTime) override {
            if (layout_) layout_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            return layout_ ? layout_->HasActiveAnimation() : false;
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            UIElement::ReleaseDeviceResources();
            if (layout_) layout_->ReleaseDeviceResources();
        }

        bool UseCache() const override { return false; }

    private:
        float padding_;
        Color backgroundColor_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
    };

    // ---------- 页面宿主（PageHost） ----------
    class PageHost : public UIElement {
    public:
        enum class TransitionDirection { Left, Right, Up, Down };
        // 过渡缓动曲线：Linear=匀速；EaseInOut=缓入缓出（默认，像指示器那种平滑）；EaseOut=缓出
        enum class TransitionEasing { Linear, EaseInOut, EaseOut };
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        PageHost() : currentIndex_(-1), animating_(false), animProgress_(0.0f),
            fromIndex_(-1), toIndex_(-1), direction_(TransitionDirection::Left),
            animDuration_(0.3f) {
            width_ = 0; height_ = 0;
            minWidth_ = 100.0f;
            minHeight_ = 100.0f;
            fillWidth_ = true;
            fillHeight_ = true;
        }

        void AddPage(std::shared_ptr<Page> page) {
            if (!page) return;
            pages_.push_back(page);
            page->SetParent(this);
            if (currentIndex_ == -1) currentIndex_ = 0;
            InvalidateLayout();
        }

        void NavigateTo(int index) {
            if (index < 0 || index >= (int)pages_.size() || index == currentIndex_) return;
            fromIndex_ = currentIndex_;
            toIndex_ = index;
            animating_ = true;
            animProgress_ = 0.0f;
            MarkChildrenDirty();   // 子元素列表随 animating_ 变化
            RequestRepaint(); // 动画开始需要重绘
        }

        void SetTransitionDirection(TransitionDirection dir) { direction_ = dir; }
        void SetTransitionEasing(TransitionEasing e) { easing_ = e; }
        TransitionEasing GetTransitionEasing() const { return easing_; }
        void SetAnimationDuration(float seconds) { animDuration_ = max(0.01f, seconds); }

        int GetCurrentIndex() const { return currentIndex_; }
        std::shared_ptr<Page> GetCurrentPage() const {
            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size())
                return pages_[currentIndex_];
            return nullptr;
        }

        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        // PageHost 不使用缓存，因为动画期间内容变化频繁
        bool UseCache() const override { return false; }

        Size MeasureOverride(const Size& availableSize) override {
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
            for (auto& page : pages_) page->Arrange(finalRect);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // M5：不在这里画页面——页面（背景+子树）由合成通道通过 GetChildren + GetChildRenderTransform 递归绘制，
            // 原来的 pages_[i]->Draw 是同一帧的重复背景绘制 + 与递归变换双通道。这里留空。
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            if (animating_) {
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size())
                    childrenView_.push_back(pages_[fromIndex_].get());
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size())
                    childrenView_.push_back(pages_[toIndex_].get());
            }
            else if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                childrenView_.push_back(pages_[currentIndex_].get());
            }
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& p : pages_) if (p) p->AttachWindowRecursive(w);
        }

        std::optional<D2D1_RECT_F> GetClipRect() const override {
            // 裁剪到自身区域
            return arrangedRect_.ToD2D();
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;

            if (animating_) {
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size()) {
                    UIElement* hit = pages_[toIndex_]->HitTest(x, y);
                    if (hit) return hit;
                }
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size()) {
                    UIElement* hit = pages_[fromIndex_]->HitTest(x, y);
                    if (hit) return hit;
                }
                return this;
            }

            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                UIElement* hit = pages_[currentIndex_]->HitTest(x, y);
                if (hit) return hit;
            }
            return this;
        }

        void UpdateAnimation(float deltaTime) override {
#ifdef ZufyUI_DEBUG
            // 如果动画已结束但仍有子元素在动画，打印子元素状态
            if (!animating_ && animProgress_ >= 1.0f) {
                for (auto& page : pages_) {
                    if (page && page->HasActiveAnimation()) {
                        // 递归打印子元素动画状态（简化版，只打印一层）
                        wchar_t msg[256];
                        swprintf(msg, 256, L"  Sub-page has animation: %hs\n", typeid(*page).name());
                        ZufyUI_DEBUG_LOG_W(msg);
                        // 如果有 layout，继续检查
                        if (auto* layout = dynamic_cast<LayoutHost*>(page.get())) {
                            if (auto* inner = layout->GetLayout().get()) {
                                if (inner->HasActiveAnimation()) {
                                    swprintf(msg, 256, L"    Layout has animation: %hs\n", typeid(*inner).name());
                                    ZufyUI_DEBUG_LOG_W(msg);
                                }
                            }
                        }
                    }
                }
            }
#endif

            bool justFinished = false;   // 只在“过渡完成的那一帧”为 true，避免每帧重复释放
            if (animating_) {
                animProgress_ += deltaTime / animDuration_;
                // 强制收敛：若进度接近 1.0（浮点误差），立即完成动画
                const float epsilon = 0.0001f;
                if (animProgress_ >= 1.0f - epsilon) {
                    animProgress_ = 1.0f;
                    animating_ = false;
                    currentIndex_ = toIndex_;
                    fromIndex_ = -1;
                    toIndex_ = -1;
                    MarkChildrenDirty();   // 子元素列表随 animating_ 变化
                    justFinished = true;
                }
                // A6：过渡只改变换（GetChildRenderTransform），页面内容不变 —— 不再每帧 RequestRepaint 两个
                // 页面（那会每帧重建两页的离屏缓存）。窗口重合成由 PageHost 的活跃动画 + lastActiveAnimElements_ 驱动。
            }
            // 更新页面动画：动画中只更新源/目标页，否则只更新当前页。
            // 注意：动画期间 currentIndex_ == fromIndex_，若三条路径都走会导致同一页每帧被更新两次、速度翻倍。
            if (animating_) {
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size())
                    pages_[fromIndex_]->UpdateAnimation(deltaTime);
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size())
                    pages_[toIndex_]->UpdateAnimation(deltaTime);
            }
            else if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                pages_[currentIndex_]->UpdateAnimation(deltaTime);
            }

            // 同步页面可见性：非当前、非过渡页设为不可见（可让其中的展开控件自动收起，且不参与绘制）
            for (size_t i = 0; i < pages_.size(); ++i) {
                if (!pages_[i]) continue;
                bool shouldBeVisible = ((int)i == currentIndex_) ||
                    (animating_ && ((int)i == fromIndex_ || (int)i == toIndex_));
                bool wasVisible = pages_[i]->IsVisible();
                pages_[i]->SetVisibleNoInvalidate(shouldBeVisible);   // page 填满 host，可见性不影响布局
                // 关键：释放时机从"过渡完成那一帧"(高频切换时 animProgress_ 被反复重置、justFinished 永不触发 → 从不释放)
                // 改为"任何页由可见变不可见的那一帧"，避免隐藏页的离屏缓存无限累积。
                if (wasVisible && !shouldBeVisible) pages_[i]->ReleaseDeviceResources();
            }

            if (justFinished) {
                // 过渡刚结束的这一帧：释放非当前页面的设备资源（只做一次）
                for (size_t i = 0; i < pages_.size(); ++i) {
                    if (i != (size_t)currentIndex_ && pages_[i]) {
                        pages_[i]->ReleaseDeviceResources();
                    }
                }
            }
        }

        bool HasActiveAnimation() const override {
            if (animating_) return true;  // 自身过渡动画

            // 检查当前页面
            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                if (pages_[currentIndex_]->HasActiveAnimation()) return true;
            }

            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& page : pages_) page->ReleaseDeviceResources();
        }

        D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const override {
            // 非动画状态，所有子元素无额外变换
            if (!animating_) return D2D1::Matrix3x2F::Identity();

            float t = EasedProgress();
            float w = arrangedRect_.width;
            float h = arrangedRect_.height;
            float offsetX = 0.0f, offsetY = 0.0f;

            // 判断子元素是源页面还是目标页面，并计算对应偏移
            if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size() && child == pages_[fromIndex_].get()) {
                // 源页面逐渐移出
                switch (direction_) {
                case TransitionDirection::Left:  offsetX = -w * t; break;
                case TransitionDirection::Right: offsetX = w * t; break;
                case TransitionDirection::Up:    offsetY = -h * t; break;
                case TransitionDirection::Down:  offsetY = h * t; break;
                }
            }
            else if (toIndex_ >= 0 && toIndex_ < (int)pages_.size() && child == pages_[toIndex_].get()) {
                // 目标页面逐渐移入
                switch (direction_) {
                case TransitionDirection::Left:  offsetX = w * (1.0f - t); break;
                case TransitionDirection::Right: offsetX = -w * (1.0f - t); break;
                case TransitionDirection::Up:    offsetY = h * (1.0f - t); break;
                case TransitionDirection::Down:  offsetY = -h * (1.0f - t); break;
                }
            }
            // 其他子元素（理论上动画期间不会出现）返回单位矩阵
            return D2D1::Matrix3x2F::Translation(offsetX, offsetY);
        }

    private:
        // 把 animProgress_ 按当前缓动曲线映射为 [0,1]（默认 smoothstep 缓入缓出）
        float EasedProgress() const {
            float t = clamp(animProgress_, 0.0f, 1.0f);
            switch (easing_) {
            case TransitionEasing::Linear: return t;
            case TransitionEasing::EaseOut: { float u = 1.0f - t; return 1.0f - u * u * u; }
            case TransitionEasing::EaseInOut:
            default: return t * t * (3.0f - 2.0f * t);
            }
        }
        std::vector<std::shared_ptr<Page>> pages_;
        int currentIndex_;
        bool animating_;
        float animProgress_;
        int fromIndex_;
        int toIndex_;
        TransitionDirection direction_;
        TransitionEasing easing_ = TransitionEasing::EaseInOut;
        float animDuration_;
    };

    // ========== 右键菜单 ==========
    class MenuItem {
    public:
        enum class Type { Normal, Separator, Submenu };
        std::wstring text;
        ZSignal<> Clicked;        // 菜单项被点击（可用 Connect 连接）
        std::shared_ptr<Menu> submenu;
        Type type = Type::Normal;
        bool enabled = true;
        std::shared_ptr<Label> icon; // 前向声明即可
    };

    class Menu : public std::enable_shared_from_this<Menu> {
    public:
        void AddItem(const std::wstring& text, std::function<void()> callback = nullptr) {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Normal;
            item->text = text;
            if (callback) item->Clicked.connect(callback);
            items.push_back(item);
        }
        void AddSeparator() {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Separator;
            items.push_back(item);
        }
        void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu) {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Submenu;
            item->text = text;
            item->submenu = submenu;
            items.push_back(item);
        }
        std::vector<std::shared_ptr<MenuItem>> items;

        // 独立弹出：不绑定任何窗口（典型用途：托盘图标右键菜单）。
        // 坐标为屏幕像素；会先关掉上一个独立菜单。
        void ShowAt(int screenX, int screenY);
        void ShowAtCursor();
    };

    class MenuWindow {
    public:
        MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y)
            : menu_(menu), owner_(owner), screenX_(x), screenY_(y) {
            dpi_ = GetDpiForWindow(owner_);
            if (dpi_ == 0) dpi_ = GetDpiForSystem();
            if (dpi_ == 0) dpi_ = 96;
            CreateWindowResources();
        }

        ~MenuWindow() {
            if (hwnd_ && IsWindow(hwnd_))
                DestroyWindow(hwnd_);
            DiscardDeviceResources();
        }

        void Show(int x, int y) {
            if (!hwnd_) return;
            if (visible_) return;

            AdjustPositionToScreen(x, y, contentWidthPx_, contentHeightPx_);
            screenX_ = x;
            screenY_ = y;
            winX_ = x - shadowPx_;
            winY_ = y - shadowPx_;

            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }

            ShowWindow(hwnd_, SW_SHOWNA);   // 分层窗口：可见性用 ShowWindow，位置/尺寸由 ULW 设置

            animating_ = true;
            fade_ = 0.0f;                   // 渐显：透明度 0 → 1
            visible_ = true;
            RenderLayered();

            SetTimer(hwnd_, animTimerId_, 10, nullptr);
            if (standalone_) SetTimer(hwnd_, kStandalonePollTimerId, 30, nullptr);
        }

        void Hide() {
            if (!hwnd_ || !IsWindow(hwnd_)) return;
            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }
            ShowWindow(hwnd_, SW_HIDE);
            visible_ = false;
            KillTimer(hwnd_, kSubmenuTimerId);
            KillTimer(hwnd_, kSubmenuHideTimerId);
            if (childMenu_) childMenu_->Hide();
        }

        void CloseAll() {
            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }
            visible_ = false;
            if (standalone_ && hwnd_) KillTimer(hwnd_, kStandalonePollTimerId);
            if (childMenu_) {
                childMenu_->CloseAll();
                childMenu_.reset();
            }
            if (hwnd_ && IsWindow(hwnd_)) {
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }
        }

        // 独立模式：没有 owner 窗口帮忙关闭菜单，靠自身轮询（点菜单外/按 Esc）关闭。
        // 注意：不使用 Win32 SetCapture。
        void SetStandalone(bool on) { standalone_ = on; }
        static std::shared_ptr<MenuWindow>& StandaloneHolder() {
            static std::shared_ptr<MenuWindow> s;
            return s;
        }

    private:
        static constexpr int kSubmenuDelayMs = 300;
        static constexpr int kSubmenuHideDelayMs = 300;
        static constexpr UINT_PTR kSubmenuTimerId = 1;
        static constexpr UINT_PTR kSubmenuHideTimerId = 3;
        static constexpr UINT_PTR animTimerId_ = 4;
        static constexpr UINT_PTR kStandalonePollTimerId = 6;

        int itemHeightDip_ = 30;
        int separatorHeightDip_ = 9;
        int paddingDip_ = 6;
        int arrowWidthDip_ = 20;
        float cornerRadiusDip_ = 12.0f;

        bool animating_ = false;
        int currentHeightPx_ = 0;
        int targetHeightPx_ = 0;
        bool visible_ = false;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            MenuWindow* self = nullptr;
            if (msg == WM_NCCREATE) {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                self = reinterpret_cast<MenuWindow*>(cs->lpCreateParams);
                self->hwnd_ = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<MenuWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }
            if (self) return self->HandleMessage(msg, wParam, lParam);
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
            switch (msg) {
            case WM_PAINT: OnPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_MOUSEMOVE: OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
            case WM_LBUTTONDOWN: {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT client;
                GetClientRect(hwnd_, &client);
                if (!PtInRect(&client, pt)) {
                    POINT screenPt = pt;
                    ClientToScreen(hwnd_, &screenPt);
                    CloseAll();
                    ScreenToClient(owner_, &screenPt);
                    SendMessage(owner_, WM_LBUTTONDOWN, wParam, MAKELPARAM(screenPt.x, screenPt.y));
                    SendMessage(owner_, WM_LBUTTONUP, wParam, MAKELPARAM(screenPt.x, screenPt.y));
                    return 0;
                }
                OnMouseDown(pt.x, pt.y);
                return 0;
            }
            case WM_LBUTTONUP: OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
            case WM_MOUSELEAVE: OnMouseLeave(); return 0;
            case WM_CAPTURECHANGED: pressedIndex_ = -1; return 0;
            case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
            case WM_NCACTIVATE: return FALSE;
            case WM_TIMER:
                if (wParam == kSubmenuTimerId) {
                    KillTimer(hwnd_, kSubmenuTimerId);
                    if (submenuPendingIndex_ >= 0) {
                        OpenSubmenu(submenuPendingIndex_);
                        submenuPendingIndex_ = -1;
                    }
                }
                else if (wParam == kSubmenuHideTimerId) {
                    KillTimer(hwnd_, kSubmenuHideTimerId);
                    POINT pt;
                    GetCursorPos(&pt);
                    if (!IsPointInMenuTree(pt)) {
                        if (childMenu_) childMenu_->Hide();
                    }
                    else {
                        SetTimer(hwnd_, kSubmenuHideTimerId, kSubmenuHideDelayMs, nullptr);
                    }
                }
                else if (wParam == animTimerId_) {
                    HandleAnimationTimer();
                }
                else if (wParam == kStandalonePollTimerId) {
                    // 独立菜单：轮询检测「点菜单外」或「Esc」来关闭（不用 SetCapture）
                    if (!standalone_) return 0;
                    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { CloseAll(); return 0; }
                    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                        POINT pt; GetCursorPos(&pt);
                        if (!IsPointInMenuTree(pt)) { CloseAll(); return 0; }
                    }
                }
                return 0;
            case WM_DPICHANGED:
                dpi_ = HIWORD(wParam);
                if (dpi_ == 0) dpi_ = 96;
                DiscardDeviceResources();
                CreateWindowResources();   // 重新计算 winX_/winY_/windowWidthPx_/windowHeightPx_
                // 用 winX_/winY_（含阴影偏移），不是内容坐标 screenX_/screenY_，否则会整体偏移
                SetWindowPos(hwnd_, nullptr, winX_, winY_, windowWidthPx_, windowHeightPx_, SWP_NOZORDER);
                RenderLayered();
                return 0;
            case WM_DESTROY:
                if (animating_) {
                    KillTimer(hwnd_, animTimerId_);
                    animating_ = false;
                }
                KillTimer(hwnd_, kSubmenuTimerId);
                KillTimer(hwnd_, kSubmenuHideTimerId);
                DiscardDeviceResources();
                return 0;
            }
            return DefWindowProc(hwnd_, msg, wParam, lParam);
        }

        void HandleAnimationTimer() {
            if (!animating_) { KillTimer(hwnd_, animTimerId_); return; }
            fade_ += 0.18f;
            if (fade_ >= 1.0f) {
                fade_ = 1.0f;
                animating_ = false;
                KillTimer(hwnd_, animTimerId_);
            }
            RenderLayered();
        }

        bool IsPointInMenuTree(POINT ptScreen) {
            RECT rc;
            GetWindowRect(hwnd_, &rc);
            if (PtInRect(&rc, ptScreen)) return true;
            if (childMenu_ && childMenu_->IsPointInMenuTree(ptScreen)) return true;
            return false;
        }

        void CreateWindowResources() {
            static bool classRegistered = false;
            if (!classRegistered) {
                WNDCLASSEXW wc = {};
                wc.cbSize = sizeof(WNDCLASSEXW);
                wc.lpfnWndProc = MenuWindow::WndProc;
                wc.hInstance = GetModuleHandle(nullptr);
                wc.lpszClassName = L"ZufyUI_MenuWindow";
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                wc.hbrBackground = nullptr;
                wc.style = CS_DROPSHADOW;
                RegisterClassExW(&wc);
                classRegistered = true;
            }

            if (!sharedDWriteFactory_) {
                DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &sharedDWriteFactory_);
            }
            if (sharedDWriteFactory_) {
                sharedDWriteFactory_->CreateTextFormat(
                    L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    14.0f, L"en-us", &textFormat_);
                if (textFormat_) {
                    textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                }
            }

            CalculateWindowSizeDip();
            contentWidthPx_ = MulDiv(windowWidthDip_, dpi_, 96);
            contentHeightPx_ = MulDiv(windowHeightDip_, dpi_, 96);
            shadowPx_ = MulDiv(shadowDip_, dpi_, 96);
            windowWidthPx_ = contentWidthPx_ + shadowPx_ * 2;
            windowHeightPx_ = contentHeightPx_ + shadowPx_ * 2;

            // 按“菜单本体”避让屏幕边缘，再整体左上偏移阴影厚度
            {
                int px = screenX_, py = screenY_;
                AdjustPositionToScreen(px, py, contentWidthPx_, contentHeightPx_);
                screenX_ = px;
                screenY_ = py;
                winX_ = px - shadowPx_;
                winY_ = py - shadowPx_;
            }

            // 分层窗口：逐像素 alpha，才能自绘柔阴影 + 圆角（不再用 SetWindowRgn）
            hwnd_ = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                L"ZufyUI_MenuWindow", L"",
                WS_POPUP,
                winX_, winY_, windowWidthPx_, windowHeightPx_,
                owner_, nullptr, GetModuleHandle(nullptr), this);
            if (!hwnd_) return;

            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
            if (d2dFactory_) {
                D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                    D2D1_RENDER_TARGET_TYPE_DEFAULT,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                    (FLOAT)dpi_, (FLOAT)dpi_);
                ID2D1DCRenderTarget* dcRT = nullptr;
                if (SUCCEEDED(d2dFactory_->CreateDCRenderTarget(&props, &dcRT))) {
                    renderTarget_ = dcRT;
                    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                }
            }

            EnsureDib(windowWidthPx_ + 8, windowHeightPx_ + 8);   // 位图比 ULW 尺寸大一圈，避免读到边界外内存（右/下出现黑边）

            if (renderTarget_) {
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bgBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.85f, 0.85f), &hoverBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &textBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.8f), &separatorBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.12f), &shadowBrush_);
            }
        }

        // 与窗口像素尺寸一致的自上而下 32bpp DIB + 内存 DC
        void EnsureDib(int w, int h) {
            if (w <= 0 || h <= 0) return;
            if (dib_ && dibW_ == w && dibH_ == h) return;
            if (dib_) { DeleteObject(dib_); dib_ = nullptr; dibBits_ = nullptr; }
            if (memDC_) { DeleteDC(memDC_); memDC_ = nullptr; }

            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;              // 负数 = 自上而下
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            dib_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
            memDC_ = CreateCompatibleDC(nullptr);
            if (memDC_ && dib_) SelectObject(memDC_, dib_);
            dibW_ = w; dibH_ = h;
        }

        // 画进 DIB 后 UpdateLayeredWindow 提交；fade_ 控制整窗不透明度（渐显动画）
        void RenderLayered() {
            if (!renderTarget_ || !memDC_ || !dib_ || !hwnd_) return;
            RECT dcRect = { 0, 0, dibW_, dibH_ };

            // 强制把 DIB 清零为纯透明：D2D 的 DC 渲染目标在预乘 alpha 下不保证覆盖到最后一行/列，
            // 不清会导致 ULW 把右/下边缘的未初始化内存当不透明黑像素显示（黑边）。
            if (dibBits_ && dibW_ > 0 && dibH_ > 0) {
                memset(dibBits_, 0, (size_t)dibW_ * dibH_ * 4);
            }

            if (FAILED(renderTarget_->BindDC(memDC_, &dcRect))) return;

            SetGlobalDpiScale(dpi_ / 96.0f);
            renderTarget_->BeginDraw();
            renderTarget_->Clear(D2D1::ColorF(0, 0, 0, 0));

            // 柔阴影：像 tooltip、比它厚一点。少量低透明度图层叠加，near-body 总 alpha ~0.18
            if (shadowBrush_) {
                float sd = (float)shadowDip_;
                D2D1_RECT_F body = D2D1::RectF(sd, sd, sd + (float)windowWidthDip_, sd + (float)windowHeightDip_);
                int steps = 6;
                float maxE = sd - 3.0f;            // 外缘留 3 DIP 全透明，避免阴影被窗口边硬裁出黑/灰边
                if (maxE < 1.0f) maxE = 1.0f;
                for (int i = steps; i >= 1; --i) {
                    float e = (float)i * (maxE / (float)steps);
                    shadowBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.03f));
                    renderTarget_->FillRoundedRectangle(
                        D2D1::RoundedRect(
                            D2D1::RectF(body.left - e, body.top - e, body.right + e, body.bottom + e),
                            cornerRadiusDip_ + e, cornerRadiusDip_ + e), shadowBrush_);
                }
            }

            // 内容平移到阴影内边距之后绘制
            D2D1::Matrix3x2F old;
            renderTarget_->GetTransform(&old);
            renderTarget_->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)shadowDip_, (FLOAT)shadowDip_) * old);
            DrawMenu();
            renderTarget_->SetTransform(old);
            renderTarget_->EndDraw();

            HDC screenDC = GetDC(nullptr);
            POINT dst = { winX_, winY_ };
            POINT src = { 0, 0 };
            SIZE size = { windowWidthPx_, windowHeightPx_ };
            BYTE a = (BYTE)(fade_ * 255.0f + 0.5f);
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, a, AC_SRC_ALPHA };
            UpdateLayeredWindow(hwnd_, screenDC, &dst, &size, memDC_, &src, 0, &bf, ULW_ALPHA);
            ReleaseDC(nullptr, screenDC);
        }

        void DiscardDeviceResources() {
            if (shadowBrush_) { shadowBrush_->Release(); shadowBrush_ = nullptr; }
            if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
            if (bgBrush_) { bgBrush_->Release(); bgBrush_ = nullptr; }
            if (hoverBrush_) { hoverBrush_->Release(); hoverBrush_ = nullptr; }
            if (textBrush_) { textBrush_->Release(); textBrush_ = nullptr; }
            if (separatorBrush_) { separatorBrush_->Release(); separatorBrush_ = nullptr; }
            if (textFormat_) { textFormat_->Release(); textFormat_ = nullptr; }
            if (d2dFactory_) { d2dFactory_->Release(); d2dFactory_ = nullptr; }
            if (memDC_) { DeleteDC(memDC_); memDC_ = nullptr; }
            if (dib_) { DeleteObject(dib_); dib_ = nullptr; dibBits_ = nullptr; }
            dibW_ = dibH_ = 0;
        }

        void CalculateWindowSizeDip() {
            int maxTextWidth = 0;
            windowHeightDip_ = paddingDip_ * 2;
            for (auto& item : menu_->items) {
                if (item->type == MenuItem::Type::Separator) {
                    windowHeightDip_ += separatorHeightDip_;
                    continue;
                }
                ComPtr<IDWriteTextLayout> layout;
                if (sharedDWriteFactory_ && textFormat_) {
                    sharedDWriteFactory_->CreateTextLayout(
                        item->text.c_str(), (UINT32)item->text.length(),
                        textFormat_, 10000.0f, 10000.0f, &layout);
                    if (layout) {
                        DWRITE_TEXT_METRICS metrics;
                        layout->GetMetrics(&metrics);
                        int width = static_cast<int>(metrics.width + 0.5f);
                        if (item->type == MenuItem::Type::Submenu) width += arrowWidthDip_;
                        maxTextWidth = max(maxTextWidth, width);
                    }
                }
                windowHeightDip_ += itemHeightDip_;
            }
            windowWidthDip_ = paddingDip_ * 2 + maxTextWidth + 24;
            windowWidthDip_ = max(windowWidthDip_, 60);
            windowHeightDip_ = max(windowHeightDip_, 34);
        }

        void AdjustPositionToScreen(int& x, int& y, int width, int height) {
            // 用“该点所在（或最近）显示器”的工作区，多显示器下才精确
            POINT pt = { x, y };
            HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(MONITORINFO) };
            RECT workArea;
            if (GetMonitorInfoW(mon, &mi)) workArea = mi.rcWork;
            else SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
            if (x + width > workArea.right) x = workArea.right - width;
            if (y + height > workArea.bottom) y = workArea.bottom - height;
            if (x < workArea.left) x = workArea.left;
            if (y < workArea.top) y = workArea.top;
        }

        void OnPaint() {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            RenderLayered();
            EndPaint(hwnd_, &ps);
        }

        void DrawMenu() {
            if (!renderTarget_ || !bgBrush_ || !textFormat_) return;

            D2D1_ROUNDED_RECT bgRect = D2D1::RoundedRect(
                D2D1::RectF(0, 0, (FLOAT)windowWidthDip_, (FLOAT)windowHeightDip_),
                cornerRadiusDip_, cornerRadiusDip_);
            renderTarget_->FillRoundedRectangle(bgRect, bgBrush_);

            float y = (float)paddingDip_;
            for (int i = 0; i < (int)menu_->items.size(); ++i) {
                auto& item = menu_->items[i];
                if (item->type == MenuItem::Type::Separator) {
                    D2D1_POINT_2F p1 = D2D1::Point2F((float)(paddingDip_ + 10), y + separatorHeightDip_ * 0.5f);
                    D2D1_POINT_2F p2 = D2D1::Point2F((float)(windowWidthDip_ - paddingDip_ - 10), y + separatorHeightDip_ * 0.5f);
                    renderTarget_->DrawLine(p1, p2, separatorBrush_, 1.0f);
                    y += separatorHeightDip_;
                    continue;
                }

                D2D1_RECT_F itemRect = D2D1::RectF(
                    (float)(paddingDip_ + 2), y,
                    (float)(windowWidthDip_ - paddingDip_ - 2), y + (float)itemHeightDip_);
                if (i == hoveredIndex_ || i == pressedIndex_) {
                    renderTarget_->FillRoundedRectangle(
                        D2D1::RoundedRect(itemRect, cornerRadiusDip_ * 0.6f, cornerRadiusDip_ * 0.6f),
                        hoverBrush_);
                }

                float textX = (float)paddingDip_ + 14.0f;
                if (item->icon) textX += 20;
                float textRight = itemRect.right - 4;
                if (item->type == MenuItem::Type::Submenu) {
                    textRight = itemRect.right - arrowWidthDip_ - 2;
                }
                D2D1_RECT_F textRect = D2D1::RectF(textX, y, textRight, y + itemHeightDip_);
                if (!item->text.empty()) {
                    renderTarget_->DrawText(item->text.c_str(), (UINT32)item->text.length(),
                        textFormat_, textRect, textBrush_);
                }

                if (item->type == MenuItem::Type::Submenu) {
                    float arrowRight = itemRect.right - 8;
                    D2D1_POINT_2F arrowCenter = D2D1::Point2F(
                        arrowRight - arrowWidthDip_ / 2 + 2, y + itemHeightDip_ / 2);
                    D2D1_POINT_2F p1 = D2D1::Point2F(arrowCenter.x - 3, arrowCenter.y - 5);
                    D2D1_POINT_2F p2 = D2D1::Point2F(arrowCenter.x - 3, arrowCenter.y + 5);
                    D2D1_POINT_2F p3 = D2D1::Point2F(arrowCenter.x + 2, arrowCenter.y);
                    renderTarget_->DrawLine(p1, p2, textBrush_, 1.0f);
                    renderTarget_->DrawLine(p2, p3, textBrush_, 1.0f);
                    renderTarget_->DrawLine(p3, p1, textBrush_, 1.0f);
                }
                y += itemHeightDip_;
            }
        }

        void OnMouseMove(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_) - shadowDip_;
            int dipY = MulDiv(y, 96, dpi_) - shadowDip_;
            int oldHover = hoveredIndex_;
            hoveredIndex_ = HitTestDip(dipX, dipY);

            if (hoveredIndex_ != oldHover) {
                if (submenuPendingIndex_ >= 0) {
                    KillTimer(hwnd_, kSubmenuTimerId);
                    submenuPendingIndex_ = -1;
                }

                if (childMenu_) {
                    bool hoverOnSubmenuItem = (hoveredIndex_ >= 0 && hoveredIndex_ < (int)menu_->items.size()
                        && menu_->items[hoveredIndex_]->type == MenuItem::Type::Submenu);
                    if (!hoverOnSubmenuItem) {
                        KillTimer(hwnd_, kSubmenuHideTimerId);
                        SetTimer(hwnd_, kSubmenuHideTimerId, kSubmenuHideDelayMs, nullptr);
                    }
                    else {
                        KillTimer(hwnd_, kSubmenuHideTimerId);
                    }
                }

                if (hoveredIndex_ >= 0 && hoveredIndex_ < (int)menu_->items.size()) {
                    auto& item = menu_->items[hoveredIndex_];
                    if (item->type == MenuItem::Type::Submenu && item->submenu) {
                        submenuPendingIndex_ = hoveredIndex_;
                        SetTimer(hwnd_, kSubmenuTimerId, kSubmenuDelayMs, nullptr);
                    }
                }

                InvalidateRect(hwnd_, nullptr, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
        }

        void OnMouseLeave() {
            if (!hwnd_) return;
            hoveredIndex_ = -1;
            pressedIndex_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void OnMouseDown(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_) - shadowDip_;
            int dipY = MulDiv(y, 96, dpi_) - shadowDip_;
            pressedIndex_ = HitTestDip(dipX, dipY);
            if (pressedIndex_ >= 0) {
                SetCapture(hwnd_);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void OnMouseUp(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_) - shadowDip_;
            int dipY = MulDiv(y, 96, dpi_) - shadowDip_;
            int idx = HitTestDip(dipX, dipY);
            if (idx >= 0 && idx == pressedIndex_) {
                auto& item = menu_->items[idx];
                if (item->type == MenuItem::Type::Normal) {
                    auto clicked = item;                 // 先保活，避免 Fire 里回调销毁菜单
                    pressedIndex_ = -1;
                    ReleaseCapture();
                    // 关整棵树：在子菜单里点项目时，根菜单也必须一起关（之前只关了子菜单）
                    MenuWindow* root = this;
                    while (root->parent_) root = root->parent_;
                    root->CloseAll();
                    clicked->Clicked.Fire();
                    return;
                }
                else if (item->type == MenuItem::Type::Submenu) {
                    pressedIndex_ = -1;
                    ReleaseCapture();
                    OpenSubmenu(idx);
                    return;
                }
            }
            pressedIndex_ = -1;
            ReleaseCapture();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        int HitTestDip(int x, int y) {
            if (x < 0 || x >= windowWidthDip_ || y < 0 || y >= windowHeightDip_)
                return -1;
            float fy = (float)y;
            float curY = (float)paddingDip_;
            for (int i = 0; i < (int)menu_->items.size(); ++i) {
                auto& item = menu_->items[i];
                if (item->type == MenuItem::Type::Separator) {
                    curY += separatorHeightDip_;
                    continue;
                }
                if (fy >= curY && fy < curY + itemHeightDip_) {
                    return i;
                }
                curY += itemHeightDip_;
            }
            return -1;
        }

        void OpenSubmenu(int index) {
            if (index < 0 || index >= (int)menu_->items.size()) return;
            auto& item = menu_->items[index];
            if (item->type != MenuItem::Type::Submenu || !item->submenu) return;

            POINT pt;
            RECT rc;
            GetWindowRect(hwnd_, &rc);
            float curY = (float)paddingDip_;
            for (int i = 0; i < index; ++i) {
                if (menu_->items[i]->type == MenuItem::Type::Separator)
                    curY += separatorHeightDip_;
                else
                    curY += itemHeightDip_;
            }
            pt.x = rc.right - shadowPx_;
            pt.y = rc.top + shadowPx_ + MulDiv((int)curY, dpi_, 96);

            if (childMenu_ && childMenu_->menu_ == item->submenu) {
                if (childMenu_->visible_) return;
                else {
                    childMenu_->Show(pt.x, pt.y);
                    return;
                }
            }

            if (childMenu_) {
                childMenu_->CloseAll();
                childMenu_.reset();
            }

            childMenu_ = std::make_unique<MenuWindow>(item->submenu, owner_, pt.x, pt.y);
            childMenu_->parent_ = this;
            childMenu_->Show(pt.x, pt.y);
        }

        std::shared_ptr<Menu> menu_;
        HWND hwnd_ = nullptr;
        HWND owner_ = nullptr;
        int screenX_, screenY_;
        UINT dpi_ = 96;
        int windowWidthDip_ = 0, windowHeightDip_ = 0;
        int windowWidthPx_ = 0, windowHeightPx_ = 0;

        int hoveredIndex_ = -1;
        int pressedIndex_ = -1;
        bool isClosing_ = false;
        bool standalone_ = false;

        std::unique_ptr<MenuWindow> childMenu_;
        MenuWindow* parent_ = nullptr;
        int submenuPendingIndex_ = -1;

        ID2D1Factory* d2dFactory_ = nullptr;
        ID2D1DCRenderTarget* renderTarget_ = nullptr;   // 分层窗口用 DC 渲染目标

        // 分层窗口（UpdateLayeredWindow）：DIB + 内存 DC，用于逐像素 alpha → 自绘柔阴影 + 圆角
        HDC memDC_ = nullptr;
        HBITMAP dib_ = nullptr;
        void* dibBits_ = nullptr;
        int dibW_ = 0, dibH_ = 0;
        int shadowDip_ = 10;                          // 自绘柔阴影厚度（DIP，比 tooltip 稍厚）
        int shadowPx_ = 0;
        float fade_ = 1.0f;                           // 渐显动画：整窗不透明度
        int contentWidthPx_ = 0, contentHeightPx_ = 0;
        int winX_ = 0, winY_ = 0;                     // 窗口（含阴影）左上角屏幕坐标
        ID2D1SolidColorBrush* shadowBrush_ = nullptr;
        ID2D1SolidColorBrush* bgBrush_ = nullptr;
        ID2D1SolidColorBrush* hoverBrush_ = nullptr;
        ID2D1SolidColorBrush* textBrush_ = nullptr;
        ID2D1SolidColorBrush* separatorBrush_ = nullptr;
        IDWriteTextFormat* textFormat_ = nullptr;

        static ComPtr<IDWriteFactory> sharedDWriteFactory_;
    };

    inline ComPtr<IDWriteFactory> MenuWindow::sharedDWriteFactory_ = nullptr;

    // 独立弹出：owner 用进程级隐藏消息窗口（不绑定任何用户窗口），典型用于托盘右键菜单。
    inline void Menu::ShowAt(int screenX, int screenY) {
        detail::InitializeUIThread();
        detail::CloseAllOpenMenus();   // 先关掉其它已打开的菜单（避免同时存在多个）
        HWND owner = detail::g_uiDispatcherWindow ? detail::g_uiDispatcherWindow : GetDesktopWindow();
        auto& holder = MenuWindow::StandaloneHolder();
        holder = std::make_shared<MenuWindow>(shared_from_this(), owner, screenX, screenY);
        holder->SetStandalone(true);
        holder->Show(screenX, screenY);
    }

    inline void Menu::ShowAtCursor() {
        POINT pt{};
        GetCursorPos(&pt);
        ShowAt(pt.x, pt.y);
    }

    // ---------- 应用核心（进程 / UI 线程级单例） ----------
    namespace detail {
        class AppCore {
        public:
            static AppCore& Instance() { static AppCore s; return s; }

            ID2D1Factory* GetFactory() {
                if (!d2dFactory_) D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
                return d2dFactory_.Get();
            }

            // 共享 D2D 1.1 设备（由 D3D11 设备创建），多窗口共用；失败返回 nullptr
            ID2D1Device* GetD2DDevice() {
                if (!d2dDevice_) {
                    D3D_FEATURE_LEVEL fl[]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
                    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                        D3D11_CREATE_DEVICE_BGRA_SUPPORT, fl, ARRAYSIZE(fl), D3D11_SDK_VERSION,
                        d3dDevice_.GetAddressOf(), nullptr, nullptr))) return nullptr;
                    if (FAILED(d3dDevice_.As(&dxgiDevice_))) return nullptr;
                    ComPtr<ID2D1Factory1> f1;
                    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                        nullptr, reinterpret_cast<void**>(f1.GetAddressOf())))) return nullptr;
                    if (FAILED(f1->CreateDevice(dxgiDevice_.Get(), d2dDevice_.GetAddressOf()))) return nullptr;
                }
                return d2dDevice_.Get();
            }
            IDXGIDevice* GetDXGIDevice() { if (!dxgiDevice_) GetD2DDevice(); return dxgiDevice_.Get(); }

            // 共享 WinRT Compositor（每 UI 线程一个），失败返回 nullptr
            ICompositor* GetCompositor() {
                if (!compositor_) {
                    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // Compositor 需要 STA
                    DispatcherQueueOptions opts{ sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA };
                    if (FAILED(CreateDispatcherQueueController(opts, reinterpret_cast<PDISPATCHERQUEUECONTROLLER*>(dqController_.GetAddressOf())))) return nullptr;
                    Microsoft::WRL::ComPtr<IInspectable> insp;
                    if (FAILED(RoActivateInstance(
                        Microsoft::WRL::Wrappers::HStringReference(
                            RuntimeClass_Windows_UI_Composition_Compositor).Get(),
                        insp.GetAddressOf()))) return nullptr;
                    if (FAILED(insp->QueryInterface(IID_PPV_ARGS(&compositor_)))) return nullptr;
                }
                return compositor_.Get();
            }

            // 共享 CompositionGraphicsDevice（噪点绘制表面用）
            ICompositionGraphicsDevice* GetCompositionGraphicsDevice() {
                if (!compositionGfx_) {
                    ICompositor* comp = GetCompositor();
                    ID2D1Device* d2d = GetD2DDevice();
                    if (!comp || !d2d) return nullptr;
                    ComPtr<ICompositorInterop> interop;
                    if (FAILED(comp->QueryInterface(IID_PPV_ARGS(&interop)))) return nullptr;
                    interop->CreateGraphicsDevice(d2d, &compositionGfx_);
                }
                return compositionGfx_.Get();
            }
            IWICImagingFactory* GetWICFactory() {
                if (!wicFactory_)
                    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory_));
                return wicFactory_.Get();
            }
            // 系统噪点刷（懒建，官方的 2% 噪点源）。内部按系统 DPI 缩放，保证颗粒 1 物理像素一颗。
            ICompositionBrush* GetNoiseBrush() {
                if (!noiseBrush_) {
                    auto sb = detail_fx::CreateSystemNoiseBrush(GetCompositor(), GetCompositionGraphicsDevice(), GetWICFactory());
                    if (sb) sb.As(&noiseBrush_);
                }
                return noiseBrush_.Get();
            }

            void AddWindow(Window* w) {
                if (w && std::find(windows_.begin(), windows_.end(), w) == windows_.end())
                    windows_.push_back(w);
            }
            void RemoveWindow(Window* w) {
                windows_.erase(std::remove(windows_.begin(), windows_.end(), w), windows_.end());
                for (auto it = windowsById_.begin(); it != windowsById_.end(); ) {
                    if (it->second == w) it = windowsById_.erase(it); else ++it;
                }
                if (windows_.empty()) Quit(0);   // 最后一个窗口关闭才退出
            }
            // 分配窗口 id 并登记（元素只保存 id，窗口销毁后查找返回 nullptr，杜绝悬垂）
            int RegisterWindow(Window* w) {
                int id = nextWindowId_++;
                windowsById_[id] = w;
                AddWindow(w);
                return id;
            }
            void UnregisterWindow(int id, Window* w) {
                windowsById_.erase(id);
                windows_.erase(std::remove(windows_.begin(), windows_.end(), w), windows_.end());
                if (windows_.empty()) Quit(0);
            }
            Window* GetWindowById(int id) const {
                if (id == 0) return nullptr;
                auto it = windowsById_.find(id);
                return it == windowsById_.end() ? nullptr : it->second;
            }
            int WindowCount() const { return (int)windows_.size(); }
            const std::vector<Window*>& Windows() const { return windows_; }

            int Run() {
                InitializeUIThread();
                if (running_) return exitCode_;   // 防止嵌套消息循环
                running_ = true;
                MSG msg;
                while (GetMessage(&msg, nullptr, 0, 0)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                running_ = false;
                return exitCode_;
            }
            bool IsRunning() const { return running_; }
            void Quit(int code = 0) {
                exitCode_ = code;
                if (running_) PostQuitMessage(code);
            }

        private:
            AppCore() { timeBeginPeriod(1); }
            ~AppCore() { timeEndPeriod(1); }
            ComPtr<ID2D1Factory> d2dFactory_;
            ComPtr<ID3D11Device> d3dDevice_;
            ComPtr<IDXGIDevice> dxgiDevice_;
            ComPtr<ID2D1Device> d2dDevice_;
            ComPtr<ICompositor> compositor_;
            ComPtr<ICompositionGraphicsDevice> compositionGfx_;
            ComPtr<IWICImagingFactory> wicFactory_;
            ComPtr<ICompositionBrush> noiseBrush_;
            ComPtr<IUnknown> dqController_;   // 持有并释放，避免泄漏
            std::vector<Window*> windows_;
            std::unordered_map<int, Window*> windowsById_;
            int nextWindowId_ = 1;
            bool running_ = false;
            int exitCode_ = 0;
        };
    }

    // ========== DComp 亚克力效果封装（移植自 ALTaleX531/Win32Acrylic，MIT） ==========
    // detail_fx（手写 IGraphicsEffect 效果类 + 官方亚克力/云母配方）已移至 ZufyUIAcrylic.h

    // ---------- 窗口 ----------
    class Window {
    public:
        inline static Backdrop DefaultBackdrop = Backdrop::None;
        inline static DWORD DefaultBackdropColor = 0x00000000;
        // ---------- 手动背景 4 层参数（全部可通过 API 调整）----------
        //   Blur 层       : blurAmount（高斯模糊 σ）
        //   Luminosity 层 : brightness / contrast / saturation（亮度 / 对比度 / 饱和度）
        //   Tint 层       : tint（颜色，含 alpha → 叠色/白纱）
        //   Noise 层      : noiseOpacity（噪点不透明度）
        struct BackgroundParams {
            float blurAmount = 30.0f;                                        // Blur 层
            float saturation = 1.0f;                                         // Legacy 配方：饱和度（Luminosity 配方无此步）
            D2D1_COLOR_F tint = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f);        // Tint 层：颜色（含 alpha）
            D2D1_COLOR_F luminosity = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);  // Luminosity 层：亮度色
            float noiseOpacity = 0.02f;                                      // Noise 层
        };
        // 亚克力（源 = 宿主背景 = 后面窗口的内容）
        inline static BackgroundParams AcrylicParams{};
        // 云母（源 = 缓存桌面壁纸）；默认 = 调好的比例（模糊 50% / 饱和 75% / 色调 alpha 87.5% / 噪点 5%）
        inline static BackgroundParams MicaParams{ 200.0f, 2.25f,
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.875f), D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), 0.005f };
        // ---------- 官方亚克力配方预设 ----------
        enum class AcrylicPreset {
            Legacy,      // Legacy / RS2：无 Luminosity 层（兼容 RS5 及更低）
            Luminosity,  // Luminosity（19H1+）：默认现代配方（含 Luminosity 层）
            Base,        // DesktopAcrylic Base：厚，颜色重、模糊强
            Thin         // DesktopAcrylic Thin：薄，颜色浅、通透
        };
        // 用一个函数设置某背景层的全部参数，并立即生效
        static void SetBackgroundParams(Backdrop which, const BackgroundParams& p) {
            if (which == Backdrop::Mica) MicaParams = p; else AcrylicParams = p;
            UIZSignals::ReloadAcrylic.Fire();
        }
        // 预设重载
        static void SetBackgroundParams(Backdrop which, AcrylicPreset preset) {
            BackgroundParams p{};
            switch (preset) {
            case AcrylicPreset::Legacy:
                p = { 30.0f, 1.0f, D2D1::ColorF(0.125f, 0.125f, 0.125f, 0.40f),
                      D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), 0.02f };
                break;
            case AcrylicPreset::Luminosity:
                p = { 30.0f, 1.0f, D2D1::ColorF(0.125f, 0.125f, 0.125f, 0.40f),
                      D2D1::ColorF(0.125f, 0.125f, 0.125f, 0.80f), 0.02f };
                break;
            case AcrylicPreset::Base:
                p = { 40.0f, 1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f),
                      D2D1::ColorF(0.15f, 0.15f, 0.15f, 0.85f), 0.03f };
                break;
            case AcrylicPreset::Thin:
                p = { 20.0f, 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.20f),
                      D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.30f), 0.01f };
                break;
            }
            if (which == Backdrop::Mica) MicaParams = p; else AcrylicParams = p;
            UIZSignals::ReloadAcrylic.Fire();
        }
        // 改完参数后调用：所有窗口重新加载背景
        inline static Color DefaultBackgroundColor = Color(0, 0, 0, 0);

        Window() : core_(&detail::AppCore::Instance()), hwnd_(nullptr), d2dFactory_(nullptr), renderTarget_(nullptr),
            rootElement_(nullptr), currentHovered_(nullptr), pressedElement_(nullptr),
            focusedElement_(nullptr), dpi_(96),
            captionColor_(RGB(240, 240, 240)), textColor_(RGB(0, 0, 0)), borderColor_(0xFFFFFFFF),
            hasCustomMinSize_(false), customMinWidth_(0), customMinHeight_(0),
            lastTime_(std::chrono::steady_clock::now()), layoutNeeded_(true),
            backdrop_(DefaultBackdrop), backdropColor_(DefaultBackdropColor), backgroundColor_(DefaultBackgroundColor),
            animationTimerActive_(false),
            layoutInvalidated_(false) {}

        virtual ~Window() {
            acrylicReloadConn_.disconnect();
            if (rootElement_) rootElement_->AttachWindowRecursive(nullptr);
            if (customTitleBar_) customTitleBar_->AttachWindowRecursive(nullptr);   // 析构路径同样清归属
            if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
            DiscardDeviceResources();
            // d2dFactory_ 由 AppCore 共享，不在此释放
        }

        void SetMouseCapture(UIElement* elem) { mouseCaptureElement_ = elem; }
        void ReleaseMouseCapture(UIElement* elem) {
            if (mouseCaptureElement_ == elem) mouseCaptureElement_ = nullptr;
        }

        // 窗口级按键钩子（在派发给焦点元素之前调用；返回 true 表示已处理）。供弹窗 ESC/Enter 等使用。
        virtual bool OnWindowKeyDown(int vk) { (void)vk; return false; }

        // 窗口级定时器钩子（WM_TIMER；返回 true 表示已处理）。id 为 SetTimer 传入的 id。
        virtual bool OnWindowTimer(int id) { (void)id; return false; }

        // 窗口尺寸变化钩子（WM_SIZE）；用于依赖客户区宽度的收尾布局（如弹窗按钮靠右）
        virtual void OnWindowSize() {}

        // 屏蔽本窗口输入（模态弹窗作为本窗口子窗口时用；不 disable HWND，避免连带影响子弹窗）
        void SetInputBlocked(bool on) { inputBlocked_ = on; }
        bool IsInputBlocked() const { return inputBlocked_; }

        // 背景效果：Backdrop=要什么（亚克力/云母/普通/毛玻璃…），tint=ARGB 着色（不需要可省略）
        void SetBackdrop(Backdrop backdrop, DWORD tint = 0x00000000) {
            backdrop_ = backdrop;
            backdropColor_ = tint;
            ApplyBackdrop();
        }
        Backdrop GetBackdrop() const { return backdrop_; }
        // 背景实现方式：用什么 API（Auto / System / Accent）
        // 当前是否在用系统材质（Win11 Mica/Acrylic）；false 表示走了 AccentState 路径
        bool IsSystemBackdropActive() const { return systemBackdropActive_; }

        // 信号：所选方案在当前系统运行时不受支持（例如 Win10 上强制 System 云母）
        ZSignal<> BackdropUnsupported;
        // 信号：设备丢失（GPU 移除/重置/交换链失效）；槽内可重建自有 GPU 资源
        ZSignal<> DeviceLost;
        // 信号：渲染致命错误（非设备丢失类）
        ZSignal<HRESULT> RenderingError;

        // 背景色（等价于 SetBackdrop 的 argb 参数）：A=透出多少背景，RGB=叠加色
        void SetBackgroundColor(Color color) {
            auto to8 = [](float f) -> DWORD { f = clamp(f, 0.0f, 1.0f); return (DWORD)(f * 255.0f + 0.5f); };
            backdropColor_ = (to8(color.a) << 24) | (to8(color.r) << 16) | (to8(color.g) << 8) | to8(color.b);
            if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
        }
        static void SetDefaultBackdrop(Backdrop backdrop, DWORD tint = 0x00000000) {
            DefaultBackdrop = backdrop;
            DefaultBackdropColor = tint;
        }

        bool Create(int width, int height, const std::wstring& title) {
            // 进程级 DPI 感知只需设置一次（多窗口重复调用无意义）
            static bool s_dpiAwareSet = false;
            if (!s_dpiAwareSet) {
                s_dpiAwareSet = true;
                HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
                if (hUser32) {
                    typedef BOOL(WINAPI* pSetProcessDpiAwarenessContext)(HANDLE);
                    pSetProcessDpiAwarenessContext SetProcessDpiAwarenessContext = (pSetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
                    if (SetProcessDpiAwarenessContext) {
                        SetProcessDpiAwarenessContext((HANDLE)-4);
                    }
                    else {
                        typedef BOOL(WINAPI* pSetProcessDPIAware)(void);
                        pSetProcessDPIAware SetProcessDPIAware = (pSetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
                        if (SetProcessDPIAware) SetProcessDPIAware();
                    }
                }
            }

            UINT sysDpi = GetDpiForSystem();
            if (sysDpi == 0) sysDpi = 96;
            int physicalWidth = MulDiv(width, sysDpi, 96);
            int physicalHeight = MulDiv(height, sysDpi, 96);

            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.lpfnWndProc = Window::WndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"ZufyUIWindowClass";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassExW(&wc);

            // 关键：owner 必须在“创建时”通过 CreateWindowEx 的父窗口参数建立。
            // 若先以 nullptr 创建、事后再 SetWindowLongPtr(GWLP_HWNDPARENT)，窗口在创建时
            // 已作为独立顶层窗口登记，会拿到自己的任务栏按钮，且最小化不会随所有者隐藏。
            HWND hwndOwner = owner_ ? owner_->hwnd_ : nullptr;
            DWORD style = WS_OVERLAPPEDWINDOW;
            if (owner_ && ownedMinimizePolicy_ == OwnedMinimizePolicy::DisableMinimize)
                style &= ~WS_MINIMIZEBOX;   // 方案4：创建时就置灰最小化按钮
            hwnd_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, L"ZufyUIWindowClass", title.c_str(), style,
                CW_USEDEFAULT, CW_USEDEFAULT, physicalWidth, physicalHeight,
                hwndOwner, nullptr, GetModuleHandle(nullptr), this);
            if (!hwnd_) return false;

            dpi_ = GetDpiForWindow(hwnd_);
            if (dpi_ == 0) dpi_ = 96;
            { RECT rc0; GetClientRect(hwnd_, &rc0);   // WM_SIZE 守卫的初始尺寸（必须在窗口创建后取）
              lastClientW_ = (UINT)(rc0.right - rc0.left); lastClientH_ = (UINT)(rc0.bottom - rc0.top); }

            d2dFactory_ = core_->GetFactory();   // 共享工厂（进程级）
            if (!d2dFactory_) return false;
            if (FAILED(CreateDeviceResources())) return false;
            if (FAILED(CreateCompositionBackend())) return false;

            id_ = core_->RegisterWindow(this);

            ApplyBackdrop();
            ApplyTitleBarColors();
            ApplyWindowCorner();

            // 亚克力参数变化 → 重新加载亚克力（重建 DComp 效果图 + 重绘）
            acrylicReloadConn_ = UIZSignals::ReloadAcrylic.connect([this]() {
                noiseBrush_.Reset();   // 噪点参数可能变了，重建
                DestroyWallpaperLayer();   // 手动背景参数可能变了，强制重建图层
                ApplyBackdrop();
                if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
                }, ConnectionThread::CurrentThread, nullptr);

            auto defaultRoot = std::make_shared<ColumnBox>();
            defaultRoot->SetMargin(Thickness(20, 20, 20, 20));
            defaultRoot->SetSpacing(10);
            rootElement_ = defaultRoot;
            rootElement_->AttachWindowRecursive(this);
            layoutNeeded_ = true;

            defaultIMC_ = ImmGetContext(hwnd_);
            if (defaultIMC_) ImmReleaseContext(hwnd_, defaultIMC_);

            // 不在这里显示窗口：何时显示由应用决定（可先配置好背景/标题栏再 Show()）。

            // 启动常驻定时器
            UpdateTimerState();
            animationTimerActive_ = true; // 常驻定时器标志

            return true;
        }

        void SetRootLayout(std::shared_ptr<Layout> layout) {
            if (!layout) return;
            if (rootElement_ && rootElement_ != layout) rootElement_->AttachWindowRecursive(nullptr);
            rootElement_ = layout;
            rootElement_->AttachWindowRecursive(this);
            layoutNeeded_ = true;
            layoutInvalidated_ = true;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        std::shared_ptr<Layout> GetRootLayout() const { return rootElement_; }

        void SetContextMenu(std::shared_ptr<Menu> menu) { windowContextMenu_ = menu; }
        void CloseContextMenu() { CloseActiveMenuWindow(); }

        std::shared_ptr<ColumnBox> GetRootColumnBox() const {
            return std::dynamic_pointer_cast<ColumnBox>(rootElement_);
        }

        void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border) {
            captionColor_ = caption; textColor_ = text; borderColor_ = border;
            if (hwnd_) ApplyTitleBarColors();
        }
        void SetCaptionColor(COLORREF color) { captionColor_ = color; if (hwnd_) ApplyTitleBarColors(); }
        void SetTitleTextColor(COLORREF color) { textColor_ = color; if (hwnd_) ApplyTitleBarColors(); }
        void SetBorderColor(COLORREF color) { borderColor_ = color; if (hwnd_) ApplyTitleBarColors(); }

        void SetMinSize(int width, int height) {
            hasCustomMinSize_ = true;
            customMinWidth_ = width;
            customMinHeight_ = height;
        }

        // 多窗口常用：把窗口移动到指定位置 / 设置尺寸（单位 DIP）
        void SetPosition(int x, int y) {
            if (!hwnd_) return;
            SetWindowPos(hwnd_, nullptr, MulDiv(x, dpi_, 96), MulDiv(y, dpi_, 96), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        void SetSize(int width, int height) {
            if (!hwnd_) return;
            SetWindowPos(hwnd_, nullptr, 0, 0, MulDiv(width, dpi_, 96), MulDiv(height, dpi_, 96), SWP_NOMOVE | SWP_NOZORDER);
        }
        bool IsValid() const { return hwnd_ != nullptr; }
        int GetId() const { return id_; }
        HWND GetHwnd() const { return hwnd_; }

        // 便捷：显示 / 隐藏 / 置顶（z 序最上）
        void Show() { if (hwnd_) ShowWindow(hwnd_, SW_SHOW); }
        void ShowNoActivate() { if (hwnd_) ShowWindow(hwnd_, SW_SHOWNOACTIVATE); }
        void Hide() { if (hwnd_) ShowWindow(hwnd_, SW_HIDE); }
        void Raise() { if (hwnd_) SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }

        // 父子（owned）窗口：设置所有者后，本窗口会始终位于所有者之上，并随所有者最小化。
        void SetOwner(Window* owner) {
            owner_ = owner;
            if (hwnd_) SetWindowLongPtr(hwnd_, GWLP_HWNDPARENT, (LONG_PTR)(owner ? owner->hwnd_ : nullptr));
        }
        Window* GetOwner() const { return owner_; }

        // ---- 便捷：取得本窗口拥有的全部子窗口（owner == this）----
        std::vector<Window*> GetOwnedWindows() const {
            std::vector<Window*> out;
            if (!core_) return out;
            for (Window* w : core_->Windows())
                if (w && w != this && w->GetOwner() == this) out.push_back(w);
            return out;
        }

        // ================= 自定义标题栏 / 窗口外观 =================
        // 安装自定义标题栏（参数一般是 ZufyUIWindowTool.h 里的 TitleBar；基类 UIElement 即可）。
        // 传 nullptr 取消，恢复原生标题栏。该控件“不参与布局”，由 Window 放到 (0,0)，
        // 并把根布局整体下移其高度；绘制时机由控件自身声明（默认 DrawAfterLayout）。
        void SetCustomTitleBar(std::shared_ptr<UIElement> bar) {
            if (customTitleBar_ && customTitleBar_ != bar) {
                customTitleBar_->SetParent(nullptr);
                customTitleBar_->AttachWindowRecursive(nullptr);
            }
            customTitleBar_ = std::move(bar);
            if (customTitleBar_) {
                customTitleBar_->SetParent(nullptr);   // 不进入根布局的父子链
                customTitleBar_->AttachWindowRecursive(this);
                // 默认绘制在“正常布局之后、覆盖层之前”（可由控件自行改成 DrawBeforeLayout）
                if (customTitleBar_->GetLayoutParticipation() == UIElement::LayoutParticipation::Normal)
                    customTitleBar_->SetLayoutParticipation(UIElement::LayoutParticipation::DrawAfterLayout);
                // 套用当前标题栏可见性，避免“先 SetTitleBarVisible(false) 再装栏”时状态不一致
                customTitleBar_->SetVisible(titleBarVisible_);
                // 把当前窗口标题灌进标题栏（否则自定义标题栏标题为空）
                { wchar_t buf[512] = {}; if (hwnd_) GetWindowTextW(hwnd_, buf, 512); lastWindowTitle_ = buf; customTitleBar_->SetWindowTitle(lastWindowTitle_); }
            }
            bool wasCustom = customFrame_;
            customFrame_ = (customTitleBar_ != nullptr);
            if (!customTitleBar_) customTitleBarHeight_ = 0.0f;
            // 保留 WS_CAPTION：靠 WM_NCCALCSIZE 把客户区扩展到整窗（不改窗口样式）。
            ApplyWindowCorner();
            if (hwnd_ && customFrame_ != wasCustom) {
                SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                // 恢复原生标题栏时，强制重绘非客户区，否则 DWM 可能不重画系统标题栏（表现为透明/看不见）
                RedrawWindow(hwnd_, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                // 重新应用背景/材质，避免取消后边框/材质状态没恢复
                ApplyBackdrop();
                layoutNeeded_ = true;
                layoutInvalidated_ = true;
                InvalidateRect(hwnd_, nullptr, TRUE);
            }
        }
        std::shared_ptr<UIElement> GetCustomTitleBar() const { return customTitleBar_; }
        bool HasCustomTitleBar() const { return customTitleBar_ != nullptr; }
        float GetCustomTitleBarHeight() const { return customTitleBarHeight_; }

        // 查询系统标题栏按钮的度量（DWM）。只把“右边距”当可靠锚点，
        // 按钮宽/高/垂直位置作为默认参考（用户可覆盖）。返回 valid=false 表示查询失败。
        struct CaptionMetrics { bool valid = false; float rightMargin = 0.0f; float top = 0.0f; float height = 0.0f; float width = 0.0f; };
        CaptionMetrics QueryCaptionMetrics() const {
            CaptionMetrics m;
            if (!hwnd_ || IsIconic(hwnd_)) return m;
            RECT rc;
            if (FAILED(DwmGetWindowAttribute(hwnd_, DWMWA_CAPTION_BUTTON_BOUNDS, &rc, sizeof(rc)))) return m;
            if (rc.right <= rc.left || rc.bottom <= rc.top) return m;
            RECT wr; GetWindowRect(hwnd_, &wr);
            int winW = wr.right - wr.left;
            int relRight = rc.right, relTop = rc.top;
            if (rc.left > winW) { relRight = rc.right - wr.left; relTop = rc.top - wr.top; }  // 屏幕坐标 → 窗口坐标
            float s = 96.0f / dpi_;
            m.valid = true;
            m.rightMargin = (winW - relRight) * s;
            m.top = relTop * s;
            m.height = (rc.bottom - rc.top) * s;
            m.width = (rc.right - rc.left) * s;
            return m;
        }

        // 隐藏/显示自定义标题栏（保留自定义边框；隐藏后根布局占满整窗）
        void SetTitleBarVisible(bool on) {
            titleBarVisible_ = on;
            if (customTitleBar_) customTitleBar_->SetVisible(on);
            if (hwnd_) { layoutNeeded_ = true; InvalidateRect(hwnd_, nullptr, TRUE); }
        }
        bool IsTitleBarVisible() const { return titleBarVisible_ && customTitleBar_ != nullptr; }

        // 原生标题栏文字 / 图标（装了自定义标题栏时，标题请用 TitleBar::SetTitle）
        void SetTitle(const std::wstring& title) { if (hwnd_) SetWindowTextW(hwnd_, title.c_str()); }
        std::wstring GetTitle() const { wchar_t b[512] = {}; if (hwnd_) GetWindowTextW(hwnd_, b, 512); return b; }
        void SetIcon(HICON bigIcon, HICON smallIcon) {
            if (!hwnd_) return;
            if (bigIcon) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)bigIcon);
            if (smallIcon) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
        }

        // 统一设置应用图标：一次同时设「原生标题栏 / 任务栏 / Alt-Tab」+「自定义标题栏上的图标」
        void SetAppIcon(HICON bigIcon, HICON smallIcon) {
            if (!hwnd_) return;
            HICON big = bigIcon ? bigIcon : smallIcon;
            HICON smallIconUse = smallIcon ? smallIcon : bigIcon;
            if (big) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)big);
            if (smallIconUse) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)smallIconUse);
            if (customTitleBar_) customTitleBar_->SetWindowIconFromHICON(big);
            // 同时更新窗口类图标：没有自定义标题栏（原生标题栏/新窗口）时也能取到图标
            if (big) SetClassLongPtrW(hwnd_, GCLP_HICON, (LONG_PTR)big);
            if (smallIconUse) SetClassLongPtrW(hwnd_, GCLP_HICONSM, (LONG_PTR)smallIconUse);
            // 强制刷新非客户区（部分系统要这个才会立刻换掉标题栏图标）
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        // 从当前模块资源加载（smallId <= 0 时用 bigId）
        void SetAppIconFromResource(int bigId, int smallId = 0) {
            HINSTANCE h = GetModuleHandleW(nullptr);
            HICON big = (HICON)LoadImageW(h, MAKEINTRESOURCEW(bigId), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
            HICON smallIcon = big;
            if (smallId > 0) smallIcon = (HICON)LoadImageW(h, MAKEINTRESOURCEW(smallId), IMAGE_ICON, 16, 16, LR_SHARED);
            SetAppIcon(big, smallIcon);
        }
        // 直接吃库自带 Image（或任何带 ToHICON() 的图片类型），自动转成系统图标
        template <class TImage>
        void SetAppIcon(std::shared_ptr<TImage> big, std::shared_ptr<TImage> smallIcon = nullptr) {
            if (!big && !smallIcon) return;
            HICON hb = big ? big->ToHICON() : nullptr;
            HICON hs = smallIcon ? smallIcon->ToHICON() : (hb ? CopyIcon(hb) : nullptr);
            SetAppIcon(hb, hs);
        }

        // ---- 任务栏状态（进度 / 覆盖徽章 / 闪烁）----
        enum class TaskbarProgress { None = 0, Indeterminate = 1, Normal = 2, Error = 4, Paused = 8 };
        void SetTaskbarProgress(TaskbarProgress state, ULONGLONG completed = 0, ULONGLONG total = 0) {
            EnsureTaskbarList();
            if (!taskbarList_) return;
            taskbarList_->SetProgressState(hwnd_, (TBPFLAG)(int)state);
            if (total > 0) taskbarList_->SetProgressValue(hwnd_, completed, total);
        }
        void SetTaskbarOverlayIcon(HICON icon, const std::wstring& description = L"") {
            EnsureTaskbarList();
            if (!taskbarList_) return;
            taskbarList_->SetOverlayIcon(hwnd_, icon, description.c_str());
        }
        void ClearTaskbarProgress() {
            EnsureTaskbarList();
            if (taskbarList_) taskbarList_->SetProgressState(hwnd_, TBPF_NOPROGRESS);
        }
        void ClearTaskbarOverlayIcon() {
            EnsureTaskbarList();
            if (taskbarList_) taskbarList_->SetOverlayIcon(hwnd_, nullptr, L"");
        }
        void SetTaskbarProgressValue(ULONGLONG completed, ULONGLONG total) {
            EnsureTaskbarList();
            if (taskbarList_ && total > 0) taskbarList_->SetProgressValue(hwnd_, completed, total);
        }
        // 直接吃库自带 Image（带 ToHICON() 的类型）作为任务栏覆盖徽章
        template <class TImage>
        void SetTaskbarOverlayIcon(std::shared_ptr<TImage> img, const std::wstring& description = L"") {
            if (!img) { ClearTaskbarOverlayIcon(); return; }
            SetTaskbarOverlayIcon(img->ToHICON(), description);
        }

        // ---- 缩略图工具栏（悬停任务栏按钮时出现的按钮）----
        enum class ThumbButtonId : UINT { Play = 0, Pause = 1, Prev = 2, Next = 3, MaxId = 4 };
        ZSignal<ThumbButtonId> ThumbButtonClicked;
        void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons,
                             HIMAGELIST imageList = nullptr) {
            EnsureTaskbarList();
            if (!taskbarList_ || !hwnd_) return;
            if (imageList) taskbarList_->ThumbBarSetImageList(hwnd_, imageList);
            std::vector<THUMBBUTTON> tb(buttons.size());
            for (size_t i = 0; i < buttons.size(); ++i) {
                tb[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
                tb[i].iId = (UINT)buttons[i].first;
                tb[i].iBitmap = (UINT)i;
                wcsncpy_s(tb[i].szTip, buttons[i].second.c_str(), _TRUNCATE);
                tb[i].dwFlags = THBF_ENABLED;
            }
            taskbarList_->ThumbBarAddButtons(hwnd_, (UINT)tb.size(), tb.data());
        }
        void UpdateThumbButton(ThumbButtonId id, bool enabled) {
            EnsureTaskbarList();
            if (!taskbarList_ || !hwnd_) return;
            THUMBBUTTON tb = {};
            tb.dwMask = THB_FLAGS;
            tb.iId = (UINT)id;
            tb.dwFlags = enabled ? THBF_ENABLED : THBF_DISABLED;
            taskbarList_->ThumbBarUpdateButtons(hwnd_, 1, &tb);
        }
        // 便捷：直接用库自带 Image 生成 HIMAGELIST（Image → HICON → HIMAGELIST），无需用户自己准备 HIMAGELIST
        template <class TImage>
        void SetThumbButtons(const std::vector<std::pair<ThumbButtonId, std::wstring>>& buttons,
                             const std::vector<std::shared_ptr<TImage>>& images) {
            EnsureTaskbarList();
            if (!taskbarList_ || !hwnd_) return;
            int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);
            if (cx <= 0) cx = 16;
            if (cy <= 0) cy = 16;
            if (thumbImageList_) { ImageList_Destroy(thumbImageList_); thumbImageList_ = nullptr; }
            HIMAGELIST il = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, (int)images.size(), 1);
            if (!il) { SetThumbButtons(buttons, (HIMAGELIST)nullptr); return; }
            for (auto& im : images) {
                HICON h = im ? im->ToHICON() : nullptr;
                if (h) { ImageList_ReplaceIcon(il, -1, h); DestroyIcon(h); }
                else {   // 占位空白图，保持索引对齐
                    HBITMAP bmp = CreateBitmap(cx, cy, 1, 32, nullptr);
                    if (bmp) { ImageList_AddMasked(il, bmp, 0); DeleteObject(bmp); }
                }
            }
            thumbImageList_ = il;   // 由本窗口持有，WM_DESTROY 时销毁
            SetThumbButtons(buttons, il);
        }

        // ---- 跳转列表（任务栏按钮右键）----
        // Tasks = 自定义动作（不可钉选）；categories = 自定义分类（可钉选）；另可选带出系统“最近使用”
        struct JumpListItem {
            std::wstring title;        // 显示文字
            std::wstring arguments;    // 传给 target 的参数（点它时以这些参数启动）
            std::wstring target;       // 可空：默认当前进程 exe；也可给文件/网址
            std::wstring iconPath;     // 可空：默认用 target
            int iconIndex = 0;
        };
        // 进程级 AppUserModelID（跳转列表归属；建议在创建窗口前设置一次）
        static void SetProcessAppUserModelID(const std::wstring& id) {
            SetCurrentProcessExplicitAppUserModelID(id.c_str());
        }
        void SetAppUserModelID(const std::wstring& id) { appUserModelId_ = id; }

        void SetJumpList(const std::vector<JumpListItem>& tasks = {},
                         const std::vector<std::pair<std::wstring, std::vector<JumpListItem>>>& categories = {},
                         bool includeRecent = false) {
            ComPtr<ICustomDestinationList> dl;
            if (FAILED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dl)))) return;
            if (!appUserModelId_.empty()) dl->SetAppID(appUserModelId_.c_str());
            UINT maxSlots = 0;
            ComPtr<IObjectArray> removed;
            if (FAILED(dl->BeginList(&maxSlots, IID_PPV_ARGS(&removed)))) return;
            if (includeRecent) dl->AppendKnownCategory(KDC_RECENT);
            if (!tasks.empty()) {
                ComPtr<IObjectCollection> tc;
                if (SUCCEEDED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tc)))) {
                    for (auto& it : tasks) { auto l = MakeJumpLink(it); if (l) tc->AddObject(l.Get()); }
                    ComPtr<IObjectArray> arr;
                    if (SUCCEEDED(tc.As(&arr))) dl->AddUserTasks(arr.Get());
                }
            }
            for (auto& cat : categories) {
                ComPtr<IObjectCollection> cc;
                if (FAILED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&cc)))) continue;
                for (auto& it : cat.second) { auto l = MakeJumpLink(it); if (l) cc->AddObject(l.Get()); }
                ComPtr<IObjectArray> arr;
                if (SUCCEEDED(cc.As(&arr))) dl->AppendCategory(cat.first.c_str(), arr.Get());
            }
            dl->CommitList();
        }

        // 是否允许拖动边框调整大小
        void SetResizable(bool on) {
            resizable_ = on;
            if (hwnd_) {
                SetWindowStyleFlag(WS_THICKFRAME, on);
                layoutNeeded_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        }
        bool IsResizable() const { return resizable_; }

        // 圆角偏好（内部只调一次 DwmSetWindowAttribute；不支持的系统会被忽略）
        enum class WindowCorner { Default, Square, Round, RoundSmall };
        void SetWindowCorner(WindowCorner c) { corner_ = c; ApplyWindowCorner(); }
        WindowCorner GetWindowCorner() const { return corner_; }

        // 窗口动作（供自定义标题栏的按钮调用）
        void Minimize() { if (hwnd_) ShowWindow(hwnd_, SW_MINIMIZE); }
        void Maximize() { if (hwnd_) ShowWindow(hwnd_, SW_MAXIMIZE); }
        void Restore() { if (hwnd_) ShowWindow(hwnd_, SW_RESTORE); }
        void MaximizeRestore() { if (hwnd_) ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE); }
        bool IsMaximizedWindow() const { return hwnd_ && IsZoomed(hwnd_) != FALSE; }
        // 发起系统级拖动：拖动 / Aero Snap / 双击最大化全部交给系统
        void BeginSystemDrag() { if (hwnd_) { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0); } }

        // ---- 闪烁提示 ----
        // 注意：FLASHW_CAPTION 对“自定义标题栏”无效；要任务栏也闪必须用 FLASHW_ALL。
        // FLASHW_TIMERNOFG 会忽略 uCount（一直闪到前台），默认不用它。
        void Flash(int times = 5, bool alsoTaskbar = true) {
            if (!hwnd_) return;
            FLASHWINFO fi = {};
            fi.cbSize = sizeof(fi);
            fi.hwnd = hwnd_;
            fi.dwFlags = alsoTaskbar ? FLASHW_ALL : FLASHW_CAPTION;
            fi.uCount = (times > 0) ? (UINT)times : 1;
            fi.dwTimeout = 0;
            FlashWindowEx(&fi);
        }
        void FlashUntilForeground() {   // 一直闪到窗口被激活
            if (!hwnd_) return;
            FLASHWINFO fi = {};
            fi.cbSize = sizeof(fi);
            fi.hwnd = hwnd_;
            fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
            fi.uCount = 0;
            fi.dwTimeout = 0;
            FlashWindowEx(&fi);
        }
        void StopFlash() {
            if (!hwnd_) return;
            FLASHWINFO fi = {}; fi.cbSize = sizeof(fi); fi.hwnd = hwnd_; fi.dwFlags = FLASHW_STOP;
            FlashWindowEx(&fi);
        }

        // ---- 显示/激活：按需要的强度分四种 ----
        enum class ActivateMode {
            Raise,        // 1：提升 Z 序，强制到最前（临时置顶，比普通 topmost 更前）
            Activate,     // 2：只激活（SetActiveWindow）
            Foreground,   // 3：SetForegroundWindow
            All           // 4：2 → 3 → 1 依次执行
        };
        void RaiseTopmost() {
            if (!hwnd_) return;
            // 先置顶（排到所有非置顶窗口之前），再取消置顶（保持最前但不 always-on-top）
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        void ShowActivate(ActivateMode mode = ActivateMode::All) {
            if (!hwnd_) return;
            if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
            else Show();
            bool doActivate = (mode == ActivateMode::Activate || mode == ActivateMode::All);
            bool doForeground = (mode == ActivateMode::Foreground || mode == ActivateMode::All);
            bool doRaise = (mode == ActivateMode::Raise || mode == ActivateMode::All);

            if (doForeground) {
                HWND fg = GetForegroundWindow();
                DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
                DWORD myThread = GetCurrentThreadId();
                bool attached = false;
                if (fgThread && fgThread != myThread)
                    attached = AttachThreadInput(fgThread, myThread, TRUE) != FALSE;
                if (doActivate) SetActiveWindow(hwnd_);
                SetForegroundWindow(hwnd_);
                BringWindowToTop(hwnd_);
                if (attached) AttachThreadInput(fgThread, myThread, FALSE);
            }
            else if (doActivate) {
                SetActiveWindow(hwnd_);
            }
            if (doRaise) RaiseTopmost();
        }

        // ---- 便捷：窗口样式 / 扩展样式（如 WS_EX_TOOLWINDOW、WS_MINIMIZEBOX 等）----
        DWORD GetStyle() const { return hwnd_ ? (DWORD)GetWindowLongPtr(hwnd_, GWL_STYLE) : 0; }
        DWORD GetExStyle() const { return hwnd_ ? (DWORD)GetWindowLongPtr(hwnd_, GWL_EXSTYLE) : 0; }
        void SetWindowStyleFlag(DWORD flag, bool on) {
            if (!hwnd_) return;
            LONG_PTR s = GetWindowLongPtr(hwnd_, GWL_STYLE);
            s = on ? (s | (LONG_PTR)flag) : (s & ~(LONG_PTR)flag);
            SetWindowLongPtr(hwnd_, GWL_STYLE, s);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        void SetWindowExStyleFlag(DWORD flag, bool on) {
            if (!hwnd_) return;
            LONG_PTR s = GetWindowLongPtr(hwnd_, GWL_EXSTYLE);
            s = on ? (s | (LONG_PTR)flag) : (s & ~(LONG_PTR)flag);
            SetWindowLongPtr(hwnd_, GWL_EXSTYLE, s);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }

        // ---- owned 子窗口“被单独最小化”的处理方案 ----
        enum class OwnedMinimizePolicy {
            None,             // 方案1：不处理（保持现状，会出现小瓷砖等）
            Hide,             // 方案2：拦截最小化 → 隐藏；父窗口还原/激活时恢复显示
            DisableMinimize   // 方案3：禁用最小化按钮(WS_MINIMIZEBOX)，并在消息处理里忽略最小化相关消息
        };
        void SetOwnedMinimizePolicy(OwnedMinimizePolicy p) {
            ownedMinimizePolicy_ = p;
            if (hwnd_) {
                // 方案4 需要把最小化按钮置灰；切换到其它方案时恢复
                SetWindowStyleFlag(WS_MINIMIZEBOX, p != OwnedMinimizePolicy::DisableMinimize);
            }
        }
        OwnedMinimizePolicy GetOwnedMinimizePolicy() const { return ownedMinimizePolicy_; }

        // ---- owned 窗口跟随：父窗口最小化/还原时，无条件隐藏/恢复其所有 owned 窗口 ----
        // 不能依赖系统的“最小化分组”自动行为：那是外壳按“前台窗口组”触发的，
        // 父窗口在后台（焦点不在子窗口）被最小化时不会连带（实测已确认）。
        void HideOwnedWindows() {
            hiddenOwnedIds_.clear();
            if (!core_) return;
            for (Window* w : core_->Windows()) {
                if (!w || w == this || w->GetOwner() != this || !w->hwnd_) continue;
                if (IsWindowVisible(w->hwnd_)) {
                    ShowWindow(w->hwnd_, SW_HIDE);
                    hiddenOwnedIds_.push_back(w->id_);
                }
            }
        }
        void ShowOwnedWindows() {
            if (!core_) { hiddenOwnedIds_.clear(); return; }
            for (int id : hiddenOwnedIds_) {
                if (Window* w = core_->GetWindowById(id)) {
                    if (w->hwnd_) ShowWindow(w->hwnd_, SW_SHOW);
                }
            }
            hiddenOwnedIds_.clear();
        }

        void Run() { core_->Run(); }

        // 以“模态”方式运行本窗口：官方做法 —— 用 EnableWindow(FALSE) 禁用所有者，
        // 把本窗口设为活动窗口，再跑一个嵌套消息循环；结束时恢复所有者。
        // 点击被禁用的所有者时，系统会自行给出提示（蜂鸣/闪烁），不需要任何钩子。
        int RunModal(Window* owner = nullptr) {
            if (!hwnd_) return 0;
            Window* ow = owner ? owner : owner_;
            HWND ownerHwnd = ow ? ow->hwnd_ : nullptr;
            if (!ownerHwnd) ownerHwnd = GetActiveWindow();
            if (ownerHwnd == hwnd_) ownerHwnd = nullptr;   // 不要禁用自己
            // 记录原使能状态：支持模态嵌套（内层不要再“恢复”外层禁用的 owner，否则鼠标会在外层模态期间复活/卡死）
            bool ownerWasEnabled = ownerHwnd && IsWindowEnabled(ownerHwnd) != FALSE;
            if (ownerWasEnabled) EnableWindow(ownerHwnd, FALSE);

            SetActiveWindow(hwnd_);
            SetForegroundWindow(hwnd_);

            int modalExit = 0;
            MSG msg;
            while (IsWindow(hwnd_) && GetMessage(&msg, nullptr, 0, 0)) {
                if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam); break; }
                // 不用 IsDialogMessage：它会把键盘消息当对话框导航吞掉（不派发），导致弹窗里
                // 英文/数字/Tab 全按不动（只有 IME 的 WM_IME_* 能穿透）。ZufyUI 自己处理 Tab 焦点导航。
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (ownerHwnd && ownerWasEnabled && IsWindow(ownerHwnd)) {
                EnableWindow(ownerHwnd, TRUE);
                SetForegroundWindow(ownerHwnd);
            }
            return modalExit;
        }

        // 关闭本窗口（其余窗口不受影响；全部关闭后消息循环才会退出）
        void Close() { if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0); }

        // ---------- 自定义标题栏 / 窗口外观：实现 ----------
        void ApplyWindowCorner() {
            if (!hwnd_) return;
            // 注意：对逐像素透明(NRB+DComp)窗口，DWMWCP_DEFAULT / ROUNDSMALL / DONOTROUND 可能让
            // DWM 丢掉圆角阴影（实测只有显式 ROUND 稳定）。这里统一用 ROUND，并在设置后强制重算
            // 非客户区，确保边框/阴影/圆角不被丢。
            int pref = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }

        void CollectDragRegions() {
            dragRegions_.clear();
            if (customTitleBar_) CollectDragRegionsRec(customTitleBar_.get());
            if (rootElement_) CollectDragRegionsRec(rootElement_.get());
        }
        void CollectDragRegionsRec(UIElement* elem) {
            if (!elem || !elem->IsVisible()) return;
            elem->CollectDragRegions(dragRegions_);
            for (auto* c : elem->GetChildren()) CollectDragRegionsRec(c);
        }
        bool PointInDragRegion(float x, float y) const {
            for (auto& r : dragRegions_) if (r.Contains(x, y)) return true;
            return false;
        }
        // 统一的命中测试：先自定义标题栏，再根布局树（标题栏不参与布局、不在根树下，
        // 必须单独纳入，否则它收不到任何鼠标事件）。
        UIElement* HitTestElement(float x, float y) {
            if (customTitleBar_) { if (UIElement* h = customTitleBar_->HitTest(x, y)) return h; }
            return rootElement_ ? rootElement_->HitTest(x, y) : nullptr;
        }
        // 自定义边框下的非客户区命中测试（缩放边 + 拖动区）
        LRESULT HitTestNonClient(POINT clientPtPx) {
            float x = clientPtPx.x * 96.0f / dpi_;
            float y = clientPtPx.y * 96.0f / dpi_;
            RECT rc; GetClientRect(hwnd_, &rc);
            float w = (rc.right - rc.left) * 96.0f / dpi_;
            float h = (rc.bottom - rc.top) * 96.0f / dpi_;
            // 最大化时禁用边框调整（最大化窗口不可调整；分屏/普通状态仍保留）
            if (resizable_ && !IsZoomed(hwnd_)) {
                const float b = 8.0f;   // 缩放热区（DIP）
                bool L = x < b, R = x >= w - b, T = y < b, B = y >= h - b;
                if (T && L) return HTTOPLEFT;
                if (T && R) return HTTOPRIGHT;
                if (B && L) return HTBOTTOMLEFT;
                if (B && R) return HTBOTTOMRIGHT;
                if (L) return HTLEFT;
                if (R) return HTRIGHT;
                if (T) return HTTOP;
                if (B) return HTBOTTOM;
            }
            // 自定义标题栏按钮 → 报告为系统按钮码（HTMINBUTTON/HTMAXBUTTON/HTCLOSE），
            // 让系统提供原生行为（尤其“最大化按钮悬浮的 Snap Layouts”由资源管理器渲染）。
            if (customTitleBar_) {
                int nc = customTitleBar_->NonClientHitTest(x, y);
                if (nc) return nc;
            }
            UIElement* hit = HitTestElement(x, y);
            if (hit && hit->IsPointInDragRegion(x, y)) return HTCAPTION;
            if (hit) return HTCLIENT;
            if (PointInDragRegion(x, y)) return HTCAPTION;
            return HTCLIENT;
        }
        // 收集“不参与布局”的元素（按绘制时机分类），递归整棵子树
        void CollectNonParticipating(UIElement* elem, UIElement::LayoutParticipation phase, std::vector<UIElement*>& out) {
            if (!elem || !elem->IsVisible()) return;
            if (elem->GetLayoutParticipation() == phase) out.push_back(elem);
            for (auto* c : elem->GetChildren()) CollectNonParticipating(c, phase, out);
        }

        // 由 UIElement 直接路由，多窗口互不干扰
        void MarkRepaint(UIElement* elem) { if (elem) pendingRepaint_.insert(elem); }
        void MarkLayoutInvalidated() { layoutInvalidated_ = true; }

        // 控件鼠标捕获（按窗口路由）
        void RequestElementCapture(UIElement* elem) { if (elem) mouseCaptureElement_ = elem; }
        void ReleaseElementCapture(UIElement* elem) { if (mouseCaptureElement_ == elem) mouseCaptureElement_ = nullptr; }

    public:
        // 窗口实例信号：按窗口区分激活/失活/关闭
        ZSignal<> Activated;
        ZSignal<> Deactivated;
        // 请求关闭时触发（点关闭按钮 / Close() / 系统关闭菜单）。槽把 *cancel 置 true 可阻止关闭，
        // 用于“关闭前询问保存”等。
        ZSignal<bool*> Closing;
        ZSignal<> Closed;

    private:
        // 移除 kAnimationIdleFramesToStop 相关逻辑，定时器常驻

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
            Window* self = nullptr;
            if (message == WM_NCCREATE) {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                self = reinterpret_cast<Window*>(cs->lpCreateParams);
                self->hwnd_ = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }
            if (self) return self->HandleMessage(message, wParam, lParam);
            return DefWindowProc(hwnd, message, wParam, lParam);
        }

        void UpdateIMEAssociation() {
            if (!hwnd_) return;
            bool needIME = (focusedElement_ && focusedElement_->IsTextInput());
            if (needIME) {
                ImmAssociateContext(hwnd_, defaultIMC_);
            }
            else {
                ImmAssociateContext(hwnd_, NULL);
            }
        }

#ifdef ZufyUI_DEBUG
        void PrintActiveAnimations(UIElement* elem, int depth) {
            if (!elem) return;
            if (elem->HasActiveAnimation()) {
                std::wstring indent(depth * 2, L' ');
                const char* typeName = typeid(*elem).name();
                wchar_t wtype[128];
                MultiByteToWideChar(CP_ACP, 0, typeName, -1, wtype, 128);
                wchar_t buf[512];
                swprintf(buf, 512, L"%s%s (0x%p)\n", indent.c_str(), wtype, (void*)elem);
                ZufyUI_DEBUG_LOG_W(buf);
            }
            for (auto* child : elem->GetChildren()) {
                PrintActiveAnimations(child, depth + 1);
            }
        }
#endif

        LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
            // 模态弹窗（子窗口形态）时屏蔽本窗口鼠标/键盘输入
            if (inputBlocked_) {
                switch (message) {
                case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
                case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                case WM_NCMOUSEMOVE: case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP: case WM_NCLBUTTONDBLCLK:
                case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
                    return 0;
                default: break;
                }
            }
            switch (message) {
            case WM_IME_SETCONTEXT:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
                }
                else {
                    lParam &= ~(ISC_SHOWUICOMPOSITIONWINDOW | ISC_SHOWUICANDIDATEWINDOW);
                }
                LRESULT lResult = DefWindowProc(hwnd_, message, wParam, lParam);
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    SetImePosition();
                }
                return lResult;
            }
            case WM_IME_STARTCOMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    UpdateCompositionText();
                    focusedElement_->SetCompositionText(compositionText_, hasComposition_, compositionCursorPos_);
                    SetImePosition();
                    return 0;
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_COMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    if (lParam & GCS_RESULTSTR) {
                        hasComposition_ = false;
                        compositionText_.clear();
                        focusedElement_->SetCompositionText(L"", false);
                    }
                    else if (lParam == 0) {
                        hasComposition_ = false;
                        compositionText_.clear();
                        focusedElement_->SetCompositionText(L"", false);
                    }
                    else {
                        UpdateCompositionText();
                        focusedElement_->SetCompositionText(compositionText_, hasComposition_, compositionCursorPos_);
                    }
                    SetImePosition();
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_ENDCOMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    hasComposition_ = false;
                    compositionText_.clear();
                    focusedElement_->SetCompositionText(L"", false);
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_CHAR:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    focusedElement_->OnChar(static_cast<wchar_t>(wParam));
                }
                return 0;
            }
            case WM_IME_NOTIFY:
            {
                if (wParam == IMN_OPENCANDIDATE || wParam == IMN_SETCANDIDATEPOS ||
                    wParam == IMN_CHANGECANDIDATE || wParam == IMN_CLOSECANDIDATE) {
                    if (focusedElement_ && focusedElement_->IsTextInput()) {
                        SetImePosition();
                    }
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_REQUEST:
            {
                if (wParam == IMR_QUERYCHARPOSITION && focusedElement_ && focusedElement_->IsTextInput()) {
                    Rect caretRect = focusedElement_->GetImeCandidateRect();
                    float scale = dpi_ / 96.0f;
                    POINT pt;
                    pt.x = static_cast<LONG>(caretRect.x * scale);
                    pt.y = static_cast<LONG>(caretRect.y * scale);
                    ClientToScreen(hwnd_, &pt);

                    IMECHARPOSITION* pCharPos = reinterpret_cast<IMECHARPOSITION*>(lParam);
                    pCharPos->pt = pt;
                    pCharPos->cLineHeight = static_cast<DWORD>(focusedElement_->GetArrangedRect().height * scale);
                    pCharPos->rcDocument = { 0, 0, 0, 0 };
                    return 1;
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_PAINT: OnPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_NCCALCSIZE:
                // 自定义边框：客户区覆盖整个窗口 → 去掉系统标题栏。
                if (customFrame_ && wParam == TRUE) {
                    NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    // 最大化：客户区 = 工作区（窗口矩形比工作区大一圈，直接用会把内容推出屏幕）。
                    if (IsZoomed(hwnd_)) {
                        HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi = { sizeof(MONITORINFO) };
                        if (GetMonitorInfoW(mon, &mi)) p->rgrc[0] = mi.rcWork;
                        return 0;
                    }
                    // 非最大化：客户区 = 窗口矩形。
                    return 0;
                }
                break;
            case WM_NCHITTEST:
                if (customFrame_) {
                    if (inSizeMove_) return HTCAPTION;   // 拖动/缩放中直接返回，避免每次 NCHITTEST 都整树命中检测（拖动高 CPU 主因）
                    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ScreenToClient(hwnd_, &pt);
                    return HitTestNonClient(pt);
                }
                break;
            case WM_NCMOUSEMOVE:
                if (customFrame_) {
                    if (inSizeMove_) return 0;   // 拖动/缩放中不做 hover 命中（否则每帧整树 HitTest + 触发按钮 hover 动画 → 每帧重绘）
                    // 按钮在非客户区（HTMINBUTTON/HTMAXBUTTON/HTCLOSE），用 NC 移动驱动自定义 hover
                    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ScreenToClient(hwnd_, &pt);
                    UpdateHover(PixelToDipX(pt.x), PixelToDipY(pt.y));
                    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd_, 0 };
                    TrackMouseEvent(&tme);
                    return 0;
                }
                break;
            case WM_NCMOUSELEAVE:
                if (customFrame_) { OnMouseLeave(); return 0; }
                break;
            case WM_NCLBUTTONDOWN:
                if (customFrame_ && (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE)) {
                    if (customTitleBar_) customTitleBar_->SetNonClientButtonPressed((int)wParam, true);
                    return 0;
                }
                break;
            case WM_NCLBUTTONUP:
                if (customFrame_) {
                    if (customTitleBar_) customTitleBar_->SetNonClientButtonPressed((int)wParam, false);
                    if (wParam == HTMINBUTTON) { Minimize(); return 0; }
                    if (wParam == HTMAXBUTTON) { MaximizeRestore(); return 0; }
                    if (wParam == HTCLOSE) { Close(); return 0; }
                }
                break;
            case WM_NCRBUTTONUP:
                if (customFrame_ && wParam == HTCAPTION) { ShowSystemMenu(); return 0; }
                break;
            case WM_NCACTIVATE:
                if (customFrame_) return TRUE;   // 不绘制默认非客户区
                break;
            case WM_NCPAINT:
                if (customFrame_) return 0;
                break;
            case 0x00AE:   // WM_NCUAHDRAWCAPTION
            case 0x00AF:   // WM_NCUAHDRAWFRAME
                if (customFrame_) return 0;
                break;
            case WM_ENTERSIZEMOVE:
                inSizeMove_ = true;       // 拖动/缩放循环：期间跳过整树命中检测
                return 0;
            case WM_EXITSIZEMOVE:
                inSizeMove_ = false;
                return 0;
            case WM_SETTEXT:
                // 系统/应用改动窗口标题（SetWindowText/SetTitle）时，同步给自定义标题栏
                if (customTitleBar_) {
                    lastWindowTitle_ = lParam ? std::wstring((const wchar_t*)lParam) : std::wstring();
                    customTitleBar_->SetWindowTitle(lastWindowTitle_);
                }
                break;   // 交给 DefWindowProc 真正设置窗口标题
            case WM_MOVE:
                UpdateWallpaperLayer();   // 手动背景：只挪图层 Offset，不重画 -> 实时跟手
                return 0;
            case WM_SIZE:
                UpdateTimerState();
                OnWindowSize();
                if (wParam == SIZE_MINIMIZED) {
                    // 方案4：禁用最小化的 owned 子窗口，最小化相关消息一律不处理
                    if (owner_ && ownedMinimizePolicy_ == OwnedMinimizePolicy::DisableMinimize) return 0;
                    Deactivated(); UIZSignals::WindowDeactivated(this);
                    wasMinimized_ = true;
                    HideOwnedWindows();          // 父窗口最小化 → 无条件隐藏 owned 子窗口
                }
                else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                    // 只在“从最小化还原”时恢复 owned 子窗口。
                    // 注意：拖拽改变窗口大小时也会收到 SIZE_RESTORED，若每次都恢复，
                    // 之前被隐藏的 owned 子窗口会莫名其妙又冒出来（之前的 bug）。
                    if (wasMinimized_) {
                        wasMinimized_ = false;
                        ShowOwnedWindows();      // 还原/最大化 → 恢复 owned 子窗口
                    }
                }
                if (IsIconic(hwnd_)) {
                    if (currentHovered_) { currentHovered_->OnMouseLeave(); currentHovered_ = nullptr; }
                    return 0;
                }
                {
                    RECT rc; GetClientRect(hwnd_, &rc);
                    UINT nw = (UINT)(rc.right - rc.left), nh = (UINT)(rc.bottom - rc.top);
                    if (nw == lastClientW_ && nh == lastClientH_) return 0;   // 尺寸没变，跳过（拖动/SetWindowPos 会空触发 WM_SIZE）
                    lastClientW_ = nw; lastClientH_ = nh;
                    clientWidthDip_ = (rc.right - rc.left) * 96.0f / dpi_;
                    clientHeightDip_ = (rc.bottom - rc.top) * 96.0f / dpi_;
                    if (swapChain_) ResizeSwapChain(nw, nh);
                    layoutNeeded_ = true;
                    layoutInvalidated_ = true;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            case WM_ACTIVATE:
                UpdateTimerState();
                if (LOWORD(wParam) == WA_INACTIVE) {
                    Deactivated(); UIZSignals::WindowDeactivated(this);
                }
                else {
                    Activated();
                    UIZSignals::WindowActivated(this);
                    ShowOwnedWindows();   // 本窗口重新获得焦点/置顶 → 恢复被隐藏的 owned 子窗口
                    // 激活时（窗口已就绪）再应用一次背景，确保 Mica/Acrylic 稳定生效
                    if (wParam != WA_INACTIVE && backdrop_ != Backdrop::None) ApplyBackdrop();
                }
                return 0;
            case WM_COMMAND:
                if (HIWORD(wParam) == THBN_CLICKED) {   // 缩略图工具栏按钮
                    ThumbButtonClicked((ThumbButtonId)LOWORD(wParam));
                    return 0;
                }
                break;
            case WM_DPICHANGED:
                UpdateTimerState();
                dpi_ = HIWORD(wParam); if (dpi_ == 0) dpi_ = 96;                // DPI 变了必须整条渲染链重建：D2D 上下文 + DComp 目标/visual 树 + 交换链。
                // 只重建 renderTarget_ 的话，swapChain_/contentVisual_ 等仍是 nullptr，
                // OnPaint 会在 EnsureSwapBackBuffer() 直接失败 → 一帧都不画，客户区变全透明
                //（命中测试/DWM 边框与渲染链无关，所以看起来"窗口还活着"）。
                DiscardDeviceResources();
                if (FAILED(CreateDeviceResources()) || FAILED(CreateCompositionBackend())) {
                    RenderingError.Fire(E_FAIL);
                    return 0;
                }
                DestroyWallpaperLayer();   // 手动背景图层也挂在被继承重建的资源上
                ApplyBackdrop();           // 亚克力/云母效果链要重新挂到新的 visual
                ApplyWindowCorner();
                ClearAllCaches();
                pendingRepaint_.clear();
                if (rootElement_) CollectVisibleCachedElements(rootElement_.get(), pendingRepaint_);
                layoutNeeded_ = true;
                layoutInvalidated_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case WM_DISPLAYCHANGE: InvalidateRect(hwnd_, nullptr, FALSE); UpdateTimerState(); return 0;
            case WM_TIMER:
                if (OnWindowTimer((int)wParam)) return 0;
                if (wParam == 1) {
#ifdef ZufyUI_DEBUG
                    // ---- 调试输出开始 ----
                    bool hasLayout = layoutInvalidated_;
                    size_t pendingCount = pendingRepaint_.size();
                    bool hasAnim = rootElement_ ? rootElement_->HasActiveAnimation() : false;

                    wchar_t buf[256];
                    swprintf(buf, 256, L"[Timer] layout=%d, pending=%zu, anim=%d\n",
                        hasLayout, pendingCount, hasAnim ? 1 : 0);
                    ZufyUI_DEBUG_LOG_W(buf);

                    // 如果动画存在，打印详细元素树
                    if (hasAnim && rootElement_) {
                        ZufyUI_DEBUG_LOG_W(L"--- Active Animation Elements ---\n");
                        PrintActiveAnimations(rootElement_.get(), 0);
                        ZufyUI_DEBUG_LOG_W(L"--- End ---\n");
                    }
                    // ---- 调试输出结束 ----
#endif
                    UpdateTooltip();
                    // 兜底：轮询真实窗口标题，变化就同步给自定义标题栏并重绘
                    // （不依赖 WM_SETTEXT 是否到达——外部 SetWindowText/系统改标题都能覆盖）
                    if (customTitleBar_) {
                        wchar_t tbuf[512] = {};
                        GetWindowTextW(hwnd_, tbuf, 512);
                        if (lastWindowTitle_ != tbuf) {
                            lastWindowTitle_ = tbuf;
                            customTitleBar_->SetWindowTitle(lastWindowTitle_);
                        }
                    }
                    if (HasRenderWork()) {          // 关键：先判断是否有工作
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    }
                    // 若无工作，则什么都不做，定时器继续运行
                }
                return 0;
            case WM_MOUSEMOVE:
                OnMouseMove(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_MOUSELEAVE: OnMouseLeave(); return 0;
            case WM_LBUTTONDOWN:
                if (activeMenuRoot_) { CloseActiveMenuWindow(); }
                OnMouseDown(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_LBUTTONUP:
                OnMouseUp(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_RBUTTONUP:
                OnContextMenu(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_SETFOCUS:
                if (focusedElement_) focusedElement_->OnFocus();
                UpdateIMEAssociation();
                return 0;
            case WM_KILLFOCUS:
                UpdateTimerState();
                CloseActiveMenuWindow();
                if (focusedElement_) focusedElement_->OnBlur();
                ImmAssociateContext(hwnd_, NULL);
                return 0;
            case WM_KEYDOWN:
                if (OnWindowKeyDown((int)wParam)) return 0;
                if (wParam == VK_TAB) { MoveFocusByTab((GetKeyState(VK_SHIFT) & 0x8000) != 0); return 0; }
                if (focusedElement_ && focusedElement_->IsEffectivelyEnabled()) focusedElement_->OnKeyDown(wParam, lParam);
                return 0;
            case WM_KEYUP: if (focusedElement_) focusedElement_->OnKeyUp(wParam, lParam); return 0;
            case WM_CHAR: if (focusedElement_) focusedElement_->OnChar(static_cast<wchar_t>(wParam)); return 0;
            case WM_MOUSEWHEEL: {
                POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hwnd_, &pt);
                float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                OnMouseWheel(PixelToDipX(pt.x), PixelToDipY(pt.y), 0.0f, delta);
                return 0;
            }
            case WM_MOUSEHWHEEL: {
                POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hwnd_, &pt);
                float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                OnMouseWheel(PixelToDipX(pt.x), PixelToDipY(pt.y), delta, 0.0f);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                if (hasCustomMinSize_) {
                    mmi->ptMinTrackSize.x = MulDiv(customMinWidth_, dpi_, 96);
                    mmi->ptMinTrackSize.y = MulDiv(customMinHeight_, dpi_, 96);
                }
                else {
                    if (rootElement_) {
                        Size minSize = rootElement_->Measure(Size(0, 0));
                        int minWidthPx = MulDiv((int)ceil(minSize.width), dpi_, 96);
                        int minHeightPx = MulDiv((int)ceil(minSize.height), dpi_, 96);
                        if (minWidthPx < 200) minWidthPx = 200;
                        if (minHeightPx < 150) minHeightPx = 150;
                        RECT rect = { 0, 0, minWidthPx, minHeightPx };
                        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
                        mmi->ptMinTrackSize.x = rect.right - rect.left;
                        mmi->ptMinTrackSize.y = rect.bottom - rect.top;
                    }
                }
                // 自定义边框：最大化时用显示器工作区，避免盖住任务栏
                if (customFrame_) {
                    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetMonitorInfoW(mon, &mi)) {
                        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                        mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                        mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                        mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                        mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
                        mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
                    }
                }
                return 0;
            }
            case WM_SETCURSOR:
                if (currentHovered_ && currentHovered_->IsTextInput()) {
                    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                    return TRUE;
                }
                break;
            case WM_SHOWWINDOW:
                UpdateTimerState();
                // 窗口显示后再应用一次背景（创建时 DWM 可能尚未就绪 → Mica/Acrylic 偶尔不生效）
                if (wParam) ApplyBackdrop();
                return 0;
            case WM_CAPTURECHANGED:
                // 捕获被系统/其他窗口夺走时，清理拖拽按下状态，避免松手后残留
                pressedElement_ = nullptr;
                return 0;
            case WM_SYSCOMMAND:
                // owned 子窗口被“单独最小化”的处理（见 OwnedMinimizePolicy）
                if (owner_ && (wParam & 0xFFF0) == SC_MINIMIZE) {
                    if (ownedMinimizePolicy_ == OwnedMinimizePolicy::Hide) {
                        // 不真正最小化，改为隐藏；登记到父窗口，等其还原/激活时恢复
                        ShowWindow(hwnd_, SW_HIDE);
                        owner_->hiddenOwnedIds_.push_back(id_);
                        return 0;
                    }
                    // DisableMinimize 策略在创建/切换时已去掉 WS_MINIMIZEBOX，系统不会再发该命令
                }
                break;
            case WM_CLOSE: {
                bool cancel = false;
                Closing.Fire(&cancel);
                if (cancel) return 0;   // 槽取消关闭（如“关闭前询问保存”）
                break;                  // 交给 DefWindowProc → DestroyWindow
            }
            case WM_DESTROY:
                if (taskbarList_) {   // 清理任务栏进度/覆盖徽章，避免窗口关闭后残留
                    taskbarList_->SetProgressState(hwnd_, TBPF_NOPROGRESS);
                    taskbarList_->SetOverlayIcon(hwnd_, nullptr, L"");
                }
                if (thumbImageList_) { ImageList_Destroy(thumbImageList_); thumbImageList_ = nullptr; }
                if (timerRunning_) {
                    KillTimer(hwnd_, 1);
                    timerRunning_ = false;
                }
                // 关键：本窗口销毁前，清除其它窗口对它的所有裸引用，
                // 否则地址被复用后会牵连到“不相干”的窗口（最小化/显示错窗口）。
                if (core_) {
                    int myId = id_;
                    for (Window* w : core_->Windows()) {
                        if (!w || w == this) continue;
                        if (w->owner_ == this) w->owner_ = nullptr;
                        auto& ids = w->hiddenOwnedIds_;
                        ids.erase(std::remove(ids.begin(), ids.end(), myId), ids.end());
                    }
                }
                if (rootElement_) rootElement_->AttachWindowRecursive(nullptr);  // 清除整棵树的窗口指针，避免外部持有元素时悬垂
                if (customTitleBar_) customTitleBar_->AttachWindowRecursive(nullptr);  // 标题栏同样清归属
                hwnd_ = nullptr;
                Closed();
                if (core_ && id_) { core_->UnregisterWindow(id_, this); id_ = 0; }
                return 0;
            }
            return DefWindowProc(hwnd_, message, wParam, lParam);
        }

        // 沿父链找第一个带 tooltip 的元素（子控件没设时用父控件的，如 Button 里的 Label）
        UIElement* ResolveTooltipOwner() const {
            for (UIElement* e = currentHovered_; e; e = e->GetParent()) {
                if (e->IsEffectivelyEnabled() && e->IsVisible() && !e->GetToolTip().empty()) return e;
            }
            return nullptr;
        }

        void UpdateTooltip() {
            UIElement* owner = ResolveTooltipOwner();
            bool candidate = (owner != nullptr);
            if (!candidate) {
                if (tooltipTarget_) { tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f; }
                return;
            }
            if (!tooltipTarget_) {
                if (GetTickCount() - hoverStartTick_ >= 500) {
                    tooltipTarget_ = owner;
                    tooltipAnchorPt_ = D2D1::Point2F(mouseX_, mouseY_);
                    tooltipProgress_ = 0.0f;
                }
                return;
            }
            if (tooltipTarget_ != owner) {
                tooltipTarget_ = nullptr;
                tooltipProgress_ = 0.0f;
                return;
            }
            if (tooltipProgress_ < 1.0f) {
                tooltipProgress_ += 0.12f;
                if (tooltipProgress_ > 1.0f) tooltipProgress_ = 1.0f;
            }
        }
        bool HasRenderWork() const {
            if (layoutInvalidated_ || focusDirty_ || !pendingRepaint_.empty()) return true;
            // 用上次合成收集到的活跃动画集合判断，避免每个 timer tick 都全树递归 HasActiveAnimation()
            if (!activeAnimScratch_.empty()) return true;
            if (tooltipTarget_ || tooltipProgress_ > 0.0f) return true;
            if (currentHovered_ && currentHovered_->IsEffectivelyEnabled() && !currentHovered_->GetToolTip().empty()) return true;
            return false;
        }

        void Compose(UIElement* elem, ID2D1RenderTarget* rt) {
            ComposeImpl(elem, rt);
        }
        // 带裁剪剔除的合成：clip 为累计裁剪矩形（DIP 绝对坐标），完全在裁剪外的子树直接跳过
        void ComposeImpl(UIElement* elem, ID2D1RenderTarget* rt,
                         const D2D1_RECT_F& clip = D2D1::RectF(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX),
                         bool hasClip = false) {
            if (!elem || !elem->IsVisible()) return;

            if (hasClip) {
                Rect er = elem->GetArrangedRect();
                if (er.x >= clip.right || er.x + er.width <= clip.left ||
                    er.y >= clip.bottom || er.y + er.height <= clip.top)
                    return;   // 完全在裁剪区外，跳过整棵子树
            }

            // 应用裁剪（如果有）
            bool clipPushed = false;
            D2D1_RECT_F effClip = clip;
            bool effHasClip = hasClip;
            if (auto c = elem->GetClipRect()) {
                rt->PushAxisAlignedClip(*c, D2D1_ANTIALIAS_MODE_ALIASED);
                clipPushed = true;
                if (effHasClip) {
                    effClip = D2D1::RectF(max(effClip.left, c->left), max(effClip.top, c->top),
                        min(effClip.right, c->right), min(effClip.bottom, c->bottom));
                }
                else {
                    effClip = *c;
                    effHasClip = true;
                }
            }

            // 判断是否使用缓存
            if (elem->UseCache()) {
                // 确保缓存存在且尺寸匹配
                EnsureCache(elem);

                // 如果待重绘或缓存无效，则绘制到缓存
                if (pendingRepaint_.count(elem) || !elem->cacheValid_) {
                    if (elem->cacheRT_) {
                        auto cacheRT = elem->cacheRT_.Get();
                        cacheRT->BeginDraw();
                        cacheRT->Clear(D2D1::ColorF(0, 0, 0, 0)); // 透明背景

                        // 设置平移变换，使元素内部使用的绝对坐标映射到缓存原点
                        D2D1::Matrix3x2F oldTransform;
                        cacheRT->GetTransform(&oldTransform);
                        // 内容原点吸附到物理像素，避免缓存内容整体落在半像素上模糊
                        D2D1::Matrix3x2F newTransform = D2D1::Matrix3x2F::Translation(
                            Snap(elem->cacheOriginX_ - elem->GetArrangedRect().x),
                            Snap(elem->cacheOriginY_ - elem->GetArrangedRect().y)
                        );
                        cacheRT->SetTransform(newTransform);

                        if (elem->HasShadow()) DrawShadow(cacheRT, elem);

                        elem->Draw(cacheRT);

                        cacheRT->SetTransform(oldTransform);
                        cacheRT->EndDraw();

                        elem->cacheValid_ = true;
                        pendingRepaint_.erase(elem);
                    }
                }

                // 将缓存位图绘制到主 RT
                if (elem->cacheRT_) {
                    ComPtr<ID2D1Bitmap> bitmap;
                    elem->cacheRT_->GetBitmap(&bitmap);
                    if (bitmap) {
                        Rect r = elem->GetArrangedRect();
                        float scaleX = (float)dpi_ / 96.0f;   // DPI 一帧内恒定：直接用成员，免掉每元素 GetDpi() COM 调用
                        float scaleY = (float)dpi_ / 96.0f;

                        // 目标像素尺寸必须与缓存位图的像素尺寸“完全一致”，否则任何插值
                        // 都会把缓存内容整体重采样，文字/线条就会发糊。
                        // 之前用 DIP 圆整推导目标宽度，可能与位图实际像素数差 1px，
                        // 叠加 NEAREST 后整个缓存被轻微缩放 → 模糊。这里直接用位图像素数反推。
                        D2D1_SIZE_U bpx = bitmap->GetPixelSize();
                        float dstX = std::round((r.x - elem->cacheOriginX_) * scaleX) / scaleX;
                        float dstY = std::round((r.y - elem->cacheOriginY_) * scaleY) / scaleY;
                        float dstW = (float)bpx.width / scaleX;
                        float dstH = (float)bpx.height / scaleY;

                        rt->DrawBitmap(
                            bitmap.Get(),
                            D2D1::RectF(dstX, dstY, dstX + dstW, dstY + dstH),
                            1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                        );
                    }
                }
            }
            else {
                // 无缓存元素，直接绘制
                elem->Draw(rt);
                // 从待重绘集合中移除
                pendingRepaint_.erase(elem);
            }

            // 递归处理子元素
            for (auto* child : elem->GetChildren()) {
                // 不参与布局的元素由 Window 的 before/after 通道单独绘制，这里必须跳过，
                // 否则若它被用户塞进了 root 树，会被画两次。
                if (!child->ParticipatesInLayout()) continue;
                D2D1::Matrix3x2F childTransform = elem->GetChildRenderTransform(child);
                bool hasTransform = !childTransform.IsIdentity();

                if (hasTransform) {
                    D2D1::Matrix3x2F oldTransform;
                    rt->GetTransform(&oldTransform);
                    rt->SetTransform(oldTransform * childTransform);
                    ComposeImpl(child, rt, effClip, effHasClip);
                    rt->SetTransform(oldTransform);
                }
                else {
                    ComposeImpl(child, rt, effClip, effHasClip);
                }
            }

            if (clipPushed) {
                rt->PopAxisAlignedClip();
            }
        }

        // 高斯 CDF 分层的软阴影：让累积 alpha 逼近 targetA*(1-Φ(d/σ))，接近 DWM 质感
        void DrawSoftShadow(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius,
                            float ox, float oy, float blur, D2D1_COLOR_F col, int steps, float alphaScale = 1.0f) {
            if (steps < 2) steps = 2;
            if (steps > 63) steps = 63;
            if (blur <= 0.0f) blur = 0.001f;
            col.a *= alphaScale;
            ComPtr<ID2D1SolidColorBrush> brush;
            rt->CreateSolidColorBrush(col, brush.GetAddressOf());
            if (!brush) return;

            float targetA = min(col.a * 2.0f, 0.98f);   // col.a 视为“边缘可见 alpha”
            float sigma = blur * 0.5f;                  // blur 约等于 2σ
            float extent = sigma * 3.0f;                // 3σ 覆盖约 99.7%

            auto Phi = [](float x) { return 0.5f * (1.0f + erff(x * 0.70710678f)); };
            std::array<float, 64> S{};
            std::array<float, 64> alphas{};
            for (int k = steps - 1; k >= 0; --k) {
                float t = (float)k / (float)(steps - 1);
                float phi = Phi(3.0f * t);
                S[k] = -logf(max(1e-6f, 1.0f - targetA * (1.0f - phi)));
            }
            for (int k = 0; k < steps; ++k) alphas[k] = 1.0f - expf(-(S[k] - S[k + 1]));

            for (int k = steps - 1; k >= 0; --k) {
                float t = (float)k / (float)(steps - 1);
                float grow = extent * t;
                D2D1_COLOR_F c = col; c.a = alphas[k];
                brush->SetColor(c);
                D2D1_RECT_F rr = D2D1::RectF(rect.left + ox - grow, rect.top + oy - grow,
                    rect.right + ox + grow, rect.bottom + oy + grow);
                rt->FillRoundedRectangle(D2D1::RoundedRect(rr, radius + grow, radius + grow), brush.Get());
            }
        }

        void DrawShadow(ID2D1RenderTarget* rt, UIElement* elem) {
            float radius = elem->GetShadowCornerRadius();
            if (radius < 0.0f) radius = 8.0f;
            DrawSoftShadow(rt, elem->GetArrangedRect().ToD2D(), radius,
                elem->GetShadowOffsetX(), elem->GetShadowOffsetY(),
                elem->GetShadowBlur(), elem->GetShadowColor(), 40);
        }

        void DrawFocusAndTooltip(ID2D1RenderTarget* rt) {
            if (showFocusRing_ && focusedElement_ && focusedElement_->IsVisible() && focusedElement_->IsEffectivelyEnabled()) {
                Rect fr = focusedElement_->GetArrangedRect();
                ComPtr<ID2D1SolidColorBrush> fb;
                rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.9f), fb.GetAddressOf());
                if (fb) rt->DrawRoundedRectangle(D2D1::RoundedRect(fr.ToD2D(), 4, 4), fb.Get(), 2.0f);
            }
            if (tooltipTarget_ && tooltipProgress_ > 0.001f) {
                std::wstring tip = tooltipTarget_->GetToolTip();
                if (!tip.empty()) DrawToolTip(rt, tip, tooltipAnchorPt_, tooltipProgress_);
            }
        }
        void DrawToolTip(ID2D1RenderTarget* rt, const std::wstring& text, const D2D1_POINT_2F& anchorPt, float alpha) {
            auto fmt = FontManager::Instance().GetFormat(FontManager::Instance().GetGlobalFont());
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            if (!fmt || !factory) return;
            alpha = clamp(alpha, 0.0f, 1.0f);
            const float maxTextWidth = 320.0f;
            ComPtr<IDWriteTextLayout> layout;
            // maxHeight=0 表示不约束高度，避免段落对齐导致文字被画到布局中部
            factory->CreateTextLayout(text.c_str(), (UINT32)text.length(), fmt, maxTextWidth, 0.0f, &layout);
            if (layout) {
                layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            }
            DWRITE_TEXT_METRICS tm{};
            if (layout) layout->GetMetrics(&tm);
            float pad = 8.0f;
            float tw = (tm.width > 0.0f ? tm.width : 20.0f);
            float th = (tm.height > 0.0f ? tm.height : 16.0f);
            if (th > 400.0f) th = 400.0f;
            float w = tw + pad * 2.0f, h = th + pad * 2.0f;
            D2D1_SIZE_F sz = rt->GetSize();
            // 固定在“显示时的鼠标位置”上方，带固定偏移（不跟随鼠标移动）
            float x = anchorPt.x - w / 2.0f;
            float y = anchorPt.y - h - 14.0f;
            if (x < 4.0f) x = 4.0f;
            if (x + w > sz.width - 4.0f) x = sz.width - w - 4.0f;
            if (y < 4.0f) y = anchorPt.y + 18.0f;      // 上方空间不足则放到鼠标下方
            if (y + h > sz.height - 4.0f) y = sz.height - h - 4.0f;
            if (x < 4.0f) x = 4.0f;
            D2D1_RECT_F rr = D2D1::RectF(x, y, x + w, y + h);
            // 柔和阴影（与元素阴影同一套高斯 CDF 分层，ToolTip 每帧绘制用较少层数）
            DrawSoftShadow(rt, rr, 6.0f, 0.5f, 2.0f, 5.0f, D2D1::ColorF(0, 0, 0, 0.30f), 20, alpha);
            ComPtr<ID2D1SolidColorBrush> bg, fg;
            rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f * alpha), bg.GetAddressOf());
            rt->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.10f, alpha), fg.GetAddressOf());
            if (bg) rt->FillRoundedRectangle(D2D1::RoundedRect(rr, 6, 6), bg.Get());
            if (layout && fg) rt->DrawTextLayout(D2D1::Point2F(x + pad, y + pad), layout.Get(), fg.Get());
        }
        void CollectFocusable(UIElement* elem, std::vector<UIElement*>& out) {
            if (!elem || !elem->IsVisible() || !elem->IsEffectivelyEnabled()) return;
            if (elem->IsFocusable()) out.push_back(elem);
            for (auto* c : elem->GetChildren()) CollectFocusable(c, out);
        }
        void SetFocusElement(UIElement* e, bool showRing = false) {
            showFocusRing_ = showRing;
            if (focusedElement_ == e) { focusDirty_ = true; return; }
            if (focusedElement_) focusedElement_->OnBlur();
            focusedElement_ = e;
            if (focusedElement_) focusedElement_->OnFocus();
            UpdateIMEAssociation();
            focusDirty_ = true;
        }
        void MoveFocusByTab(bool backward) {
            std::vector<UIElement*> list;
            CollectFocusable(rootElement_.get(), list);
            if (list.empty()) return;
            int cur = -1;
            for (int i = 0; i < (int)list.size(); ++i) if (list[i] == focusedElement_) { cur = i; break; }
            int n = (int)list.size();
            int next;
            if (cur < 0) next = backward ? n - 1 : 0;
            else next = ((cur + (backward ? -1 : 1)) % n + n) % n;
            SetFocusElement(list[next], true);
        }

        void EnsureCache(UIElement* elem) {
            Rect r = elem->GetArrangedRect();
            if (r.width <= 0 || r.height <= 0) {
                elem->cacheRT_.Reset();
                elem->cacheValid_ = false;
                return;
            }

            float bleed = elem->GetBleed() + elem->GetShadowExtent();
            float w = r.width + bleed * 2;
            float h = r.height + bleed * 2;

            bool needCreate = !elem->cacheRT_;
            if (elem->cacheRT_) {
                if (elem->cacheSize_.width != w || elem->cacheSize_.height != h) {
                    needCreate = true;
                }
            }

            if (needCreate) {
                elem->cacheRT_.Reset();
                if (renderTarget_) {
                    HRESULT hr = renderTarget_->CreateCompatibleRenderTarget(
                        D2D1::SizeF(w, h),
                        elem->cacheRT_.GetAddressOf()
                    );
                    if (SUCCEEDED(hr)) {
                        elem->cacheSize_ = Size(w, h);
                        elem->cacheValid_ = false;
                        elem->cacheRT_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        elem->cacheRT_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);   // 必须改回 GRAYSCALE
                        elem->cacheRT_->SetDpi((FLOAT)dpi_, (FLOAT)dpi_);
                    }
                }
            }

            elem->cacheOriginX_ = bleed;
            elem->cacheOriginY_ = bleed;
        }

        void ClearAllCaches() {
            std::function<void(UIElement*)> clearRecursive = [&](UIElement* elem) {
                if (!elem) return;
                if (elem->UseCache()) {
                    elem->cacheRT_.Reset();
                    elem->cacheValid_ = false;
                }
                for (auto* child : elem->GetChildren()) {
                    clearRecursive(child);
                }
                };
            clearRecursive(rootElement_.get());
            clearRecursive(customTitleBar_.get());
        }

        void CollectVisibleCachedElements(UIElement* elem, std::unordered_set<UIElement*>& set) {
            if (!elem || !elem->IsVisible()) return;
            if (elem->UseCache()) {
                set.insert(elem);
            }
            for (auto* child : elem->GetChildren()) {
                CollectVisibleCachedElements(child, set);
            }
        }

        void OnPaint() {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            if (IsIconic(hwnd_)) { EndPaint(hwnd_, &ps); return; }

            if (!renderTarget_) {
                if (FAILED(CreateDeviceResources())) {
                    EndPaint(hwnd_, &ps);
                    return;
                }
            }

            RECT rc; GetClientRect(hwnd_, &rc);
            float clientWidthDip = (rc.right - rc.left) * 96.0f / dpi_;
            float clientHeightDip = (rc.bottom - rc.top) * 96.0f / dpi_;

            Thickness rootMargin = rootElement_ ? rootElement_->GetMargin() : Thickness();
            float left = rootMargin.left, top = rootMargin.top;
            float availWidth = clientWidthDip - left - rootMargin.right;
            float availHeight = clientHeightDip - top - rootMargin.bottom;

            // 自定义标题栏：不参与布局，放在 (0,0)；根布局整体下移其高度
            if (customTitleBar_ && titleBarVisible_) {
                Size tbSize = customTitleBar_->Measure(Size(clientWidthDip, FLT_MAX));
                customTitleBarHeight_ = max(0.0f, tbSize.height);
                customTitleBar_->Arrange(Rect(0.0f, 0.0f, clientWidthDip, customTitleBarHeight_));
                top += customTitleBarHeight_;
                availHeight -= customTitleBarHeight_;
                if (availHeight < 0.0f) availHeight = 0.0f;
            }
            else {
                customTitleBarHeight_ = 0.0f;
            }

            // 1. 帧开始，计算时间差
            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
            lastTime_ = now;
            // 钳制 deltaTime：空闲/最小化/断点恢复后首帧可能得到很大的 dt，
            // 会让线性累加型动画（Button hover、PageHost 切页、CheckBox 等）一帧跳到终点。
            // 上限取 ~2 帧（0.033s），最坏也只表现为一次轻微卡顿。
            if (deltaTime > 0.033f) deltaTime = 0.033f;
            if (deltaTime < 0.0f) deltaTime = 0.0f;

            // 2. 布局检查
            if (layoutInvalidated_ || layoutNeeded_ || (rootElement_ && rootElement_->NeedsLayout())) {
                if (rootElement_) {
                    rootElement_->Measure(Size(availWidth, availHeight));
                    rootElement_->Arrange(Rect(left, top, availWidth, availHeight));
                }
                layoutNeeded_ = false;
                layoutInvalidated_ = false;
                // 第 2 期：不再 ClearAllCaches()——Arrange 包装器已把真正重排元素的 cacheValid_ 置 false，
                // 未变化的子树缓存保持有效（这正是省内存的关键）。
                CollectDragRegions();   // 拖动区依赖布局：只在重排后重建（原来每帧全树收集）
                npBefore_.clear(); npAfter_.clear();
                if (rootElement_) {
                    CollectNonParticipating(rootElement_.get(), UIElement::LayoutParticipation::DrawBeforeLayout, npBefore_);
                    CollectNonParticipating(rootElement_.get(), UIElement::LayoutParticipation::DrawAfterLayout, npAfter_);
                }
                if (customTitleBar_) {
                    if (customTitleBar_->GetLayoutParticipation() == UIElement::LayoutParticipation::DrawBeforeLayout) npBefore_.push_back(customTitleBar_.get());
                    else npAfter_.push_back(customTitleBar_.get());
                }
            }

            // 3. 动画更新
            if (rootElement_) {
                rootElement_->UpdateAnimation(deltaTime);
            }
            if (customTitleBar_) {
                customTitleBar_->UpdateAnimation(deltaTime);
            }

            // 自动收集活跃动画元素（确保动画期间每帧重绘这些元素）
            std::swap(activeAnimScratch_, lastActiveAnimElements_);   // 交换缓冲代替每帧 hashset 深拷贝；last 保留上一帧活跃集
            activeAnimScratch_.clear();
            CollectActiveAnimations(rootElement_.get(), activeAnimScratch_);
            if (customTitleBar_) CollectActiveAnimations(customTitleBar_.get(), activeAnimScratch_);
            for (auto* elem : activeAnimScratch_) {
                pendingRepaint_.insert(elem);
            }
            // 上一帧活跃但当前不活跃的元素也加入，确保动画结束状态正确
            for (auto* elem : lastActiveAnimElements_) {
                if (activeAnimScratch_.find(elem) == activeAnimScratch_.end()) {
                    pendingRepaint_.insert(elem);
                }
            }

            // 4. 合成绘制（D2D 1.1 DeviceContext -> 交换链后备缓冲 -> Present）
            SetGlobalDpiScale(dpi_ / 96.0f);
            HRESULT hr = E_FAIL;
            if (SUCCEEDED(EnsureSwapBackBuffer())) {
                renderTarget_->SetTarget(swapBackBuffer_);
                renderTarget_->SetDpi((FLOAT)dpi_, (FLOAT)dpi_);
                renderTarget_->BeginDraw();
                {
                    // 背景色 = 叠加在“背景（空 / 亚克力 / 云母）之上”的颜色。
                    // RGB = 颜色，A = 透出多少背景（0 = 全透、255 = 完全盖住）。对所有模式统一生效。
                    // D2D 的 Clear 接收“非预乘”颜色、内部自行预乘。
                    DWORD bg = backdropColor_;
                    float a = ((bg >> 24) & 0xFF) / 255.0f;
                    float r = ((bg >> 16) & 0xFF) / 255.0f;
                    float g = ((bg >> 8) & 0xFF) / 255.0f;
                    float b = (bg & 0xFF) / 255.0f;
                    renderTarget_->Clear(D2D1::ColorF(r, g, b, a));
                }

                D2D1_SIZE_F rsz = renderTarget_->GetSize();
                if (rsz.width <= 0.0f || rsz.height <= 0.0f)
                    rsz = D2D1::SizeF(clientWidthDip_, clientHeightDip_);

                if (rootElement_ || customTitleBar_) {
                    D2D1_RECT_F full = D2D1::RectF(0, 0, rsz.width, rsz.height);
                    for (auto* e : npBefore_) ComposeImpl(e, renderTarget_, full, true);
                    if (rootElement_) ComposeImpl(rootElement_.get(), renderTarget_, full, true);
                    for (auto* e : npAfter_) ComposeImpl(e, renderTarget_, full, true);
                }

                UIZSignals::DrawOverlay(this, renderTarget_);
                DrawFocusAndTooltip(renderTarget_);
                focusDirty_ = false;

                hr = renderTarget_->EndDraw();
                renderTarget_->SetTarget(nullptr);

                if (SUCCEEDED(hr) && swapChain_) {
                    DXGI_PRESENT_PARAMETERS pp{};
                    hr = swapChain_->Present1(1, 0, &pp);
                }
            }

            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                DiscardDeviceResources();
                DeviceLost.Fire();
                if (FAILED(CreateDeviceResources()) || FAILED(CreateCompositionBackend())) {
                    // 库不替应用决定如何善后：只发信号，由应用决定提示/关闭/重建
                    RenderingError.Fire(hr);
                    EndPaint(hwnd_, &ps);
                    return;
                }
                // 设备重建后，清空缓存，但布局不需要重做
                ClearAllCaches();
                pendingRepaint_.clear();
                if (rootElement_) {
                    CollectVisibleCachedElements(rootElement_.get(), pendingRepaint_);
                }
            }
            else {
                // 本帧所有可见元素都已重新绘制，清空待重绘集合，避免空闲时残留 pending
                pendingRepaint_.clear();
            }

            EndPaint(hwnd_, &ps);
        }

        // 以下为原有事件处理函数，保留不变
        void OnMouseMove(float x, float y) {
            // 只有真实位移才重置悬停计时并关闭提示；重复的 WM_MOUSEMOVE（坐标未变）忽略
            bool realMove = fabs(x - mouseX_) > 0.5f || fabs(y - mouseY_) > 0.5f;
            mouseX_ = x; mouseY_ = y;
            if (realMove) {
                hoverStartTick_ = GetTickCount();
                if (tooltipTarget_ || tooltipProgress_ > 0.0f) { tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f; }
            }
            if (!rootElement_ && !customTitleBar_) return;
            if (mouseCaptureElement_) {
                mouseCaptureElement_->OnMouseMove(x, y);
                return;
            }
            if (pressedElement_) {
                pressedElement_->OnMouseMove(x, y);
                return;
            }
            UpdateHover(x, y);
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
        }

        void OnMouseLeave() {
            if (currentHovered_) { currentHovered_->OnMouseLeave(); currentHovered_ = nullptr; }
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
        }

        void OnMouseDown(float x, float y) {
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
            if (mouseCaptureElement_) {
                if (pressedElement_) {
                    pressedElement_ = nullptr;
                    ReleaseCapture();
                }
                mouseCaptureElement_->OnMouseDown(x, y);
                return;
            }

            UIZSignals::GlobalMouseDown(this, x, y);

            UIElement* hit = HitTestElement(x, y);
            if (hit && !hit->IsEffectivelyEnabled()) hit = nullptr;   // disabled 不接收鼠标按下（与 hover 一致）
            if (hit) {
                hit->OnMouseDown(x, y);
                SetCapture(hwnd_);
                pressedElement_ = hit;

                if (hit->IsFocusable()) {
                    SetFocusElement(hit, false);
                }
                else {
                    if (focusedElement_) { focusedElement_->OnBlur(); focusedElement_ = nullptr; }
                }
            }
            else {
                if (focusedElement_) { focusedElement_->OnBlur(); focusedElement_ = nullptr; }
            }
            UpdateIMEAssociation();
        }

        void OnMouseUp(float x, float y) {
            // 注意：Win32 捕获(SetCapture)只在“按住鼠标”期间需要；
            // 元素级捕获(mouseCaptureElement_，如 ComboBox 展开、拖拽)与它无关，松开鼠标必须释放 Win32 捕获，
            // 否则整个线程的鼠标都会被本窗口截走，其他窗口无法交互。
            UIElement* a = pressedElement_;
            UIElement* b = mouseCaptureElement_;
            if (a) a->OnMouseUp(x, y);
            if (b && b != a) b->OnMouseUp(x, y);
            pressedElement_ = nullptr;
            if (GetCapture() == hwnd_) ReleaseCapture();
            UpdateHover(x, y);
        }

        // 在标题栏区域右键时弹系统菜单（与原生一致；显式 TrackPopupMenu 更可靠）
        void ShowSystemMenu() {
            if (!hwnd_) return;
            HMENU hMenu = GetSystemMenu(hwnd_, FALSE);
            if (!hMenu) return;
            EnableMenuItem(hMenu, SC_RESTORE, IsZoomed(hwnd_) ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, SC_SIZE, resizable_ ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, SC_MINIMIZE, MF_ENABLED);
            EnableMenuItem(hMenu, SC_MAXIMIZE, IsZoomed(hwnd_) ? MF_GRAYED : MF_ENABLED);
            EnableMenuItem(hMenu, SC_CLOSE, MF_ENABLED);
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd_);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
                pt.x, pt.y, 0, hwnd_, nullptr);
            if (cmd) PostMessageW(hwnd_, WM_SYSCOMMAND, (WPARAM)cmd, 0);
        }

        void OnContextMenu(float x, float y) {
            // 自定义边框下，标题栏区域右键 → 系统菜单
            if (customFrame_ && y <= customTitleBarHeight_) { ShowSystemMenu(); return; }
            if (!rootElement_ && !customTitleBar_) return;
            UIElement* hit = HitTestElement(x, y);
            if (hit && hit->OnContextMenu(x, y)) return;   // 控件已处理右键
            std::shared_ptr<Menu> menu;
            if (hit && hit->GetContextMenu()) {
                menu = hit->GetContextMenu();
            }
            else if (windowContextMenu_) {
                menu = windowContextMenu_;
            }
            if (menu) {
                POINT pt;
                pt.x = static_cast<LONG>(MulDiv(static_cast<int>(x), static_cast<int>(dpi_), 96));
                pt.y = static_cast<LONG>(MulDiv(static_cast<int>(y), static_cast<int>(dpi_), 96));
                ClientToScreen(hwnd_, &pt);
            CloseActiveMenuWindow();
            detail::CloseAllOpenMenus();   // 关掉可能已打开的独立菜单
            activeMenuRoot_ = std::make_unique<MenuWindow>(menu, hwnd_, pt.x, pt.y);
                activeMenuRoot_->Show(pt.x, pt.y);
            }
        }

        void UpdateHover(float x, float y) {
            if (!rootElement_ && !customTitleBar_) return;
            UIElement* hit = HitTestElement(x, y);
            if (hit && !hit->IsEffectivelyEnabled()) hit = nullptr;
            if (hit != currentHovered_) {
                if (currentHovered_) currentHovered_->OnMouseLeave();
                if (hit) hit->OnMouseEnter();
                currentHovered_ = hit;
                hoverStartTick_ = GetTickCount();
            }
            if (hit) hit->OnMouseMove(x, y);
        }

        void CloseActiveMenuWindow() {
            if (activeMenuRoot_) {
                activeMenuRoot_->CloseAll();
                activeMenuRoot_.reset();
            }
        }

        void OnMouseWheel(float x, float y, float deltaX, float deltaY) {
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
            if (!rootElement_ && !customTitleBar_) return;
            UIElement* elem = currentHovered_;
            if (!elem) elem = HitTestElement(x, y);
            while (elem) {
                if (elem->OnMouseWheel(deltaX, deltaY)) break;
                elem = elem->GetParent();
            }
        }

        HRESULT CreateDeviceResources() {
            if (renderTarget_) return S_OK;
            ID2D1Device* dev = core_ ? core_->GetD2DDevice() : nullptr;
            if (!dev) return E_FAIL;
            if (FAILED(dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &renderTarget_))) return E_FAIL;
            renderTarget_->SetDpi((FLOAT)dpi_, (FLOAT)dpi_);
            renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            return S_OK;
        }

        HRESULT CreateSwapChain(UINT w, UINT h) {
            if (swapChain_) return ResizeSwapChain(w, h);
            IDXGIDevice* dxgi = core_ ? core_->GetDXGIDevice() : nullptr;
            if (!dxgi) return E_FAIL;
            ComPtr<IDXGIAdapter> adapter;
            if (FAILED(dxgi->GetAdapter(&adapter))) return E_FAIL;
            ComPtr<IDXGIFactory2> factory;
            if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return E_FAIL;
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = w; desc.Height = h;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.Scaling = DXGI_SCALING_STRETCH;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            return factory->CreateSwapChainForComposition(dxgi, &desc, nullptr, &swapChain_);
        }

        HRESULT ResizeSwapChain(UINT w, UINT h) {
            if (!swapChain_) return E_FAIL;
            if (renderTarget_) { renderTarget_->SetTarget(nullptr); renderTarget_->Flush(); }
            if (swapBackBuffer_) { swapBackBuffer_->Release(); swapBackBuffer_ = nullptr; }
            return swapChain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        }

        HRESULT EnsureSwapBackBuffer() {
            if (swapBackBuffer_) return S_OK;
            if (!swapChain_ || !renderTarget_) return E_FAIL;
            ComPtr<IDXGISurface> bb;
            if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&bb)))) return E_FAIL;
            D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                (FLOAT)dpi_, (FLOAT)dpi_);
            return renderTarget_->CreateBitmapFromDxgiSurface(bb.Get(), &props, &swapBackBuffer_);
        }

        HRESULT CreateCompositionBackend() {
            if (contentVisual_) return S_OK;
            compositor_ = core_ ? core_->GetCompositor() : nullptr;
            if (!compositor_) return E_FAIL;
            ComPtr<ICompositorDesktopInterop> interop;
            if (FAILED(compositor_->QueryInterface(IID_PPV_ARGS(&interop)))) return E_FAIL;
            if (FAILED(interop->CreateDesktopWindowTarget(hwnd_, TRUE, &dcompTarget_))) return E_FAIL;
            if (FAILED(compositor_->CreateSpriteVisual(&rootVisual_))) return E_FAIL;
            ComPtr<IVisual2> r2;
            if (SUCCEEDED(rootVisual_->QueryInterface(IID_PPV_ARGS(&r2)))) r2->put_RelativeSizeAdjustment({ 1.f, 1.f });

            RECT rc; GetClientRect(hwnd_, &rc);
            clientWidthDip_ = (rc.right - rc.left) * 96.0f / dpi_;
            clientHeightDip_ = (rc.bottom - rc.top) * 96.0f / dpi_;
            if (FAILED(CreateSwapChain((UINT)(rc.right - rc.left), (UINT)(rc.bottom - rc.top)))) return E_FAIL;

            ComPtr<ICompositionSurface> surf;
            ComPtr<ICompositorInterop> cinterop;
            if (FAILED(compositor_->QueryInterface(IID_PPV_ARGS(&cinterop)))) return E_FAIL;
            if (FAILED(cinterop->CreateCompositionSurfaceForSwapChain(swapChain_, &surf))) return E_FAIL;
            ComPtr<ICompositionSurfaceBrush> brush;
            if (FAILED(compositor_->CreateSurfaceBrushWithSurface(surf.Get(), &brush))) return E_FAIL;
            brush->put_HorizontalAlignmentRatio(0.f);
            brush->put_VerticalAlignmentRatio(0.f);
            if (FAILED(compositor_->CreateSpriteVisual(&contentVisual_))) return E_FAIL;
            ComPtr<IVisual2> c2;
            if (SUCCEEDED(contentVisual_->QueryInterface(IID_PPV_ARGS(&c2)))) c2->put_RelativeSizeAdjustment({ 1.f, 1.f });
            contentVisual_->put_Brush(reinterpret_cast<ICompositionBrush*>(brush.Get()));

            ComPtr<ICompositionTarget> ct;
            if (FAILED(dcompTarget_->QueryInterface(IID_PPV_ARGS(&ct)))) return E_FAIL;
            ct->put_Root(reinterpret_cast<IVisual*>(rootVisual_));
            ComPtr<IContainerVisual> container;
            if (FAILED(rootVisual_->QueryInterface(IID_PPV_ARGS(&container)))) return E_FAIL;
            ComPtr<IVisualCollection> children;
            container->get_Children(&children);
            children->InsertAtTop(reinterpret_cast<IVisual*>(contentVisual_));
            // 依据当前 Backdrop 建立/更新根视觉背景（亚克力时挂 HostBackdropBrush+模糊）
            UpdateDCompBackdrop();
            return S_OK;
        }

        // 背景是否用手动实现（Mica 用缓存壁纸图层；Acrylic 用我们自己的配方）
        bool IsManualBackdrop() const {
            return backdrop_ == Backdrop::Mica;
        }

        // 读取桌面壁纸 -> 缩放到虚拟屏幕 -> 高斯模糊 + 白纱，烘焙成一张位图
        bool BuildWallpaperBitmap() {
            if (wallpaperBitmap_) return true;
            IWICImagingFactory* wic = core_ ? core_->GetWICFactory() : nullptr;
            ID2D1Device* dev = core_ ? core_->GetD2DDevice() : nullptr;
            if (!wic || !dev) return false;
            WCHAR path[MAX_PATH] = {};
            if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) || !path[0]) return false;
            ComPtr<IWICBitmapDecoder> dec;
            if (FAILED(wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) return false;
            ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(dec->GetFrame(0, &frame))) return false;
            ComPtr<IWICFormatConverter> conv;
            if (FAILED(wic->CreateFormatConverter(&conv))) return false;
            if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;

            wallpaperVirtX_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
            wallpaperVirtY_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
            wallpaperVirtW_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            wallpaperVirtH_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if (wallpaperVirtW_ <= 0 || wallpaperVirtH_ <= 0) return false;

            ComPtr<ID2D1DeviceContext> dc;
            if (FAILED(dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return false;
            dc->SetDpi(96.0f, 96.0f);

            ComPtr<ID2D1Bitmap1> src;
            ComPtr<IWICBitmapScaler> scaler;
            if (SUCCEEDED(wic->CreateBitmapScaler(&scaler)) &&
                SUCCEEDED(scaler->Initialize(conv.Get(), (UINT)wallpaperVirtW_, (UINT)wallpaperVirtH_, WICBitmapInterpolationModeFant))) {
                if (FAILED(dc->CreateBitmapFromWicBitmap(scaler.Get(), nullptr, &src))) return false;
            }
            else {
                if (FAILED(dc->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &src))) return false;
            }

            D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
            ComPtr<ID2D1Bitmap1> out;
            if (FAILED(dc->CreateBitmap(D2D1::SizeU((UINT32)wallpaperVirtW_, (UINT32)wallpaperVirtH_), nullptr, 0, &bp, &out))) return false;
            const bool isMica = (backdrop_ == Backdrop::Mica);
            const float sigma = isMica ? MicaParams.blurAmount : AcrylicParams.blurAmount;
            const float saturation = isMica ? MicaParams.saturation : AcrylicParams.saturation;
            const float noiseOpacity = isMica ? MicaParams.noiseOpacity : AcrylicParams.noiseOpacity;
            const D2D1_COLOR_F veil = isMica ? MicaParams.tint : AcrylicParams.tint;
            const D2D1_RECT_F full = D2D1::RectF(0, 0, (float)wallpaperVirtW_, (float)wallpaperVirtH_);

            dc->SetTarget(out.Get());
            dc->BeginDraw();
            dc->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));   // 先铺一层白色底，再在上面算

            // 官方配方的步骤：
            //   Luminosity(19H1+) : Blur -> Blend(Color, luminosity色) -> Blend(Luminosity, tint色) -> Noise
            //   Legacy(RS2)       : Blur -> Saturation -> Blend(Exclusion) -> CompositeStep(tint) -> Noise
            // 这里：Blur +（Legacy 的）Saturation + Tint 叠色 + Noise；不加官方没有的步骤。
            ID2D1Effect* result = nullptr;
            ComPtr<ID2D1Effect> blur, sat;
            if (SUCCEEDED(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blur)) && blur) {
                blur->SetInput(0, src.Get());
                blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma);
                blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
                result = blur.Get();
                // Saturation 是 Legacy(RS2) 配方的步骤；Luminosity 配方没有（saturation==1 时不加）
                if (saturation != 1.0f && SUCCEEDED(dc->CreateEffect(CLSID_D2D1Saturation, &sat)) && sat) {
                    sat->SetInputEffect(0, blur.Get());
                    sat->SetValue(D2D1_SATURATION_PROP_SATURATION, saturation);
                    result = sat.Get();
                }
            }
            if (result) dc->DrawImage(result);
            else dc->DrawBitmap(src.Get(), full);

            // 2) 叠白（云母）
            if (veil.a > 0.0f) {
                ComPtr<ID2D1SolidColorBrush> tint;
                dc->CreateSolidColorBrush(veil, &tint);
                if (tint) dc->FillRectangle(full, tint.Get());
            }

            // 3) 噪点：复用官方系统噪点贴图（云母 1% / 亚克力 2%），1:1 NEAREST 平铺
            if (noiseOpacity > 0.0f && core_) {
                IWICImagingFactory* wic = core_->GetWICFactory();
                ComPtr<IWICBitmap> noiseWic = wic ? detail_fx::LoadSystemNoiseWIC(wic) : nullptr;
                if (noiseWic) {
                    D2D1_BITMAP_PROPERTIES1 nbp = D2D1::BitmapProperties1(
                        D2D1_BITMAP_OPTIONS_NONE,
                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
                    ComPtr<ID2D1Bitmap1> noiseBmp;
                    if (SUCCEEDED(dc->CreateBitmapFromWicBitmap(noiseWic.Get(), &nbp, &noiseBmp)) && noiseBmp) {
                        ComPtr<ID2D1BitmapBrush> nb;
                        if (SUCCEEDED(dc->CreateBitmapBrush(noiseBmp.Get(), &nb)) && nb) {
                            nb->SetExtendModeX(D2D1_EXTEND_MODE_WRAP);
                            nb->SetExtendModeY(D2D1_EXTEND_MODE_WRAP);
                            nb->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
                            nb->SetOpacity(noiseOpacity);
                            dc->FillRectangle(full, nb.Get());
                        }
                    }
                }
            }

            dc->EndDraw();
            wallpaperBitmap_ = out;
            return true;
        }

        // 把烘焙好的壁纸放进一个独立的合成器图层（在内容层下方），移动时只改 Offset
        bool BuildWallpaperLayer() {
            if (wallpaperVisual_) return true;
            if (!compositor_ || !rootVisual_ || !renderTarget_ || !core_) return false;
            if (!BuildWallpaperBitmap() || !wallpaperBitmap_) return false;
            IDXGIDevice* dxgi = core_->GetDXGIDevice();
            if (!dxgi) return false;
            ComPtr<IDXGIAdapter> adapter;
            if (FAILED(dxgi->GetAdapter(&adapter))) return false;
            ComPtr<IDXGIFactory2> factory;
            if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
            DXGI_SWAP_CHAIN_DESC1 d{};
            d.Width = (UINT)wallpaperVirtW_; d.Height = (UINT)wallpaperVirtH_;
            d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            d.SampleDesc.Count = 1;
            d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            d.BufferCount = 2;   // FLIP_SEQUENTIAL 需要 >= 2
            d.Scaling = DXGI_SCALING_STRETCH;
            d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            if (FAILED(factory->CreateSwapChainForComposition(dxgi, &d, nullptr, &wallpaperSwap_))) return false;
            {
                ComPtr<IDXGISurface> bb;
                if (FAILED(wallpaperSwap_->GetBuffer(0, IID_PPV_ARGS(&bb)))) return false;
                D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
                ComPtr<ID2D1Bitmap1> tgt;
                if (FAILED(renderTarget_->CreateBitmapFromDxgiSurface(bb.Get(), &bp, &tgt))) return false;
                renderTarget_->SetTarget(tgt.Get());
                FLOAT oldDpiX = 96.0f, oldDpiY = 96.0f;
                renderTarget_->GetDpi(&oldDpiX, &oldDpiY);
                renderTarget_->SetDpi(96.0f, 96.0f);   // 1 DIP = 1 物理像素，否则位图被 2 倍放大 / 裁掉一半
                renderTarget_->BeginDraw();
                renderTarget_->Clear();
                renderTarget_->DrawBitmap(wallpaperBitmap_.Get(),
                    D2D1::RectF(0, 0, (float)wallpaperVirtW_, (float)wallpaperVirtH_),
                    1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                    D2D1::RectF(0, 0, (float)wallpaperVirtW_, (float)wallpaperVirtH_));
                renderTarget_->EndDraw();
                renderTarget_->SetDpi(oldDpiX, oldDpiY);
                renderTarget_->SetTarget(swapBackBuffer_);
                renderTarget_->Flush();
                wallpaperSwap_->Present(0, 0);
            }
            ComPtr<ICompositorInterop> cinterop;
            if (FAILED(compositor_->QueryInterface(IID_PPV_ARGS(&cinterop)))) return false;
            ComPtr<ICompositionSurface> surf;
            if (FAILED(cinterop->CreateCompositionSurfaceForSwapChain(wallpaperSwap_.Get(), &surf))) return false;
            ComPtr<ICompositionSurfaceBrush> brush;
            if (FAILED(compositor_->CreateSurfaceBrushWithSurface(surf.Get(), &brush))) return false;
            brush->put_HorizontalAlignmentRatio(0.f);
            brush->put_VerticalAlignmentRatio(0.f);
            brush->put_Stretch(CompositionStretch_Fill);
            if (FAILED(compositor_->CreateSpriteVisual(&wallpaperVisual_))) return false;
            ComPtr<ICompositionBrush> cb; brush.As(&cb);
            wallpaperVisual_->put_Brush(cb.Get());
            ComPtr<IVisual> v; wallpaperVisual_.As(&v);
            v->put_Size({ (float)wallpaperVirtW_, (float)wallpaperVirtH_ });   // 物理像素
            ComPtr<IContainerVisual> container;
            if (FAILED(rootVisual_->QueryInterface(IID_PPV_ARGS(&container)))) return false;
            ComPtr<IVisualCollection> children;
            if (FAILED(container->get_Children(&children))) return false;
            children->InsertAtBottom(v.Get());   // 放到内容层下方
            UpdateWallpaperLayer();
            return true;
        }

        void UpdateWallpaperLayer() {
            if (!wallpaperVisual_ || !hwnd_) return;
            POINT p = { 0, 0 }; ClientToScreen(hwnd_, &p);
            ComPtr<IVisual> v; wallpaperVisual_.As(&v);
            v->put_Offset({ (float)(-p.x), (float)(-p.y), 0.0f });   // 物理像素
        }

        void DestroyWallpaperLayer() {
            if (wallpaperVisual_) wallpaperVisual_->put_Brush(nullptr);
            wallpaperVisual_.Reset();
            wallpaperSwap_.Reset();
            wallpaperBitmap_.Reset();
        }

        // 依据当前 backdrop_ 重建根视觉的 DComp 背景层。
        // 只有 Acrylic 需要（HostBackdropBrush + 高斯模糊）；其它效果清掉背景层。
        // 可在 Create 时、设备重建时、以及运行时 SetBackdrop/SetBackdropMode 时调用。
        void UpdateDCompBackdrop() {
            if (!rootVisual_ || !compositor_) return;
            rootVisual_->put_Brush(nullptr);              // 先清掉旧背景
            // 云母：ZufyUI 自绘缓存壁纸图层
            if (backdrop_ == Backdrop::Mica) {
                if (!wallpaperVisual_) { DestroyWallpaperLayer(); BuildWallpaperLayer(); }
                return;
            }
            DestroyWallpaperLayer();
            // 亚克力：用我们自己的配方（源 = 宿主背景 = 后面窗口的内容）
            if (backdrop_ != Backdrop::Acrylic) return;

            ComPtr<ICompositionBackdropBrush> backdropBrush;
            HRESULT hrBackdrop = E_FAIL;
            ComPtr<ICompositor3> c3;
            if (SUCCEEDED(compositor_->QueryInterface(IID_PPV_ARGS(&c3))))
                hrBackdrop = c3->CreateHostBackdropBrush(&backdropBrush);
            if (FAILED(hrBackdrop)) {
                ComPtr<ICompositor2> c2;
                if (SUCCEEDED(compositor_->QueryInterface(IID_PPV_ARGS(&c2))))
                    hrBackdrop = c2->CreateBackdropBrush(&backdropBrush);
            }
            if (FAILED(hrBackdrop) || !backdropBrush) return;

            // 官方亚克力配方：模糊 + 亮度/颜色混合 + 噪点。参数走 ManualAcrylic*（滑块可调）。
            D2D1_COLOR_F tint = AcrylicParams.tint;
            ComPtr<ICompositionBrush> noise = core_ ? core_->GetNoiseBrush() : nullptr;
            ComPtr<ICompositionBrush> brush = detail_fx::BuildAcrylicBrush(
                compositor_, reinterpret_cast<ICompositionBrush*>(backdropBrush.Get()),
                noise.Get(),
                tint, AcrylicParams.luminosity, AcrylicParams.saturation,
                AcrylicParams.noiseOpacity, AcrylicParams.blurAmount);
            if (!brush) {
                // 兜底：完整配方失败时退回“宿主背景 + 高斯模糊”，至少能看到模糊背景
                auto blur = Microsoft::WRL::Make<detail_fx::GaussianBlurEffect>(AcrylicParams.blurAmount);
                blur->SetInput(static_cast<ABI::Windows::Graphics::Effects::IGraphicsEffectSource*>(
                    detail_fx::CompositionEffectSource(Microsoft::WRL::Wrappers::HStringReference(L"Backdrop").Get())));
                ComPtr<ICompositionEffectFactory> factory;
                if (SUCCEEDED(compositor_->CreateEffectFactory(blur.Get(), &factory))) {
                    ComPtr<ICompositionEffectBrush> eb;
                    if (SUCCEEDED(factory->CreateBrush(&eb))) {
                        eb->SetSourceParameter(Microsoft::WRL::Wrappers::HStringReference(L"Backdrop").Get(),
                            reinterpret_cast<ICompositionBrush*>(backdropBrush.Get()));
                        eb.As(&brush);
                    }
                }
            }
            if (brush) rootVisual_->put_Brush(brush.Get());
        }

        // 动态生成一张 64x64 的随机灰度噪点（Alpha=255），并用 wrap 平铺铺满整个窗口。
        // 不内置资源位图；AcrylicNoiseOpacity 变化时重建。
        void EnsureNoiseBrush() {
            if (noiseBrush_ || !renderTarget_) return;
            ID2D1RenderTarget* rt = renderTarget_;   // 用 D2D 1.0 基接口
            const int N = 64;
            std::vector<DWORD> px((size_t)N * N);
            for (int i = 0; i < N * N; ++i) {
                DWORD v = (DWORD)(rand() & 0xFF);
                px[i] = 0xFF000000u | (v << 16) | (v << 8) | v;   // A=255, premultiplied 灰度
            }
            D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            if (FAILED(rt->CreateBitmap(D2D1::SizeU(N, N), px.data(), N * 4, &props, noiseBitmap_.GetAddressOf()))) return;
            if (FAILED(rt->CreateBitmapBrush(noiseBitmap_.Get(), noiseBrush_.GetAddressOf()))) return;
            if (noiseBrush_) {
                noiseBrush_->SetExtendModeX(D2D1_EXTEND_MODE_WRAP);
                noiseBrush_->SetExtendModeY(D2D1_EXTEND_MODE_WRAP);
            }
        }

        void DiscardDeviceResources() {
            // 手动背景图层持有 swapchain/visual，设备重建时必须先拆掉
            if (wallpaperVisual_) wallpaperVisual_->put_Brush(nullptr);
            wallpaperVisual_.Reset(); wallpaperSwap_.Reset(); wallpaperBitmap_.Reset();
            noiseBitmap_.Reset();
            noiseBrush_.Reset();
            if (renderTarget_) { renderTarget_->SetTarget(nullptr); renderTarget_->Flush(); renderTarget_->Release(); renderTarget_ = nullptr; }
            if (swapBackBuffer_) { swapBackBuffer_->Release(); swapBackBuffer_ = nullptr; }
            if (contentVisual_) { contentVisual_->Release(); contentVisual_ = nullptr; }
            if (rootVisual_) { rootVisual_->Release(); rootVisual_ = nullptr; }
            if (dcompTarget_) { dcompTarget_->Release(); dcompTarget_ = nullptr; }
            if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
            compositor_ = nullptr;   // 共享对象，不 Release
            if (rootElement_) {
                // 释放所有元素的缓存和设备资源
                std::function<void(UIElement*)> releaseRecursive = [&](UIElement* elem) {
                    if (!elem) return;
                    elem->ReleaseDeviceResources();
                    for (auto* child : elem->GetChildren()) {
                        releaseRecursive(child);
                    }
                    };
                releaseRecursive(rootElement_.get());
            }
            // 告知订阅者（如图像缓存）清掉按渲染目标/设备缓存的位图，
            // 避免旧渲染目标指针被新对象复用后命中错误缓存、旧位图泄漏。
            UIZSignals::DeviceReset.Fire();
        }

        // 应用 AccentState（Win10 / Win11 无系统材质时的实现方式）
        void ApplyAccentState(ACCENT_STATE state) {
            HMODULE hUser = GetModuleHandleW(L"user32.dll");
            if (!hUser) return;
            auto pSetWindowCompositionAttribute = (BOOL(WINAPI*)(HWND, void*))GetProcAddress(hUser, "SetWindowCompositionAttribute");
            if (!pSetWindowCompositionAttribute) return;

            // GradientColor 的字节序是 ABGR（不是 ARGB），必须转换；否则红色会显示成蓝色。
            DWORD argb = backdropColor_;
            DWORD aa = (argb >> 24) & 0xFF, rr = (argb >> 16) & 0xFF, gg = (argb >> 8) & 0xFF, bb = argb & 0xFF;
            ACCENT_POLICY accent = {};
            accent.AccentState = state;
            accent.GradientColor = (aa << 24) | (bb << 16) | (gg << 8) | rr;   // AABBGGRR
            accent.AccentFlags = 0;
            accent.AnimationId = 0;

            WINDOWCOMPOSITIONATTRIBDATA data = {};
            data.Attrib = WCA_ACCENT_POLICY;
            data.pvData = &accent;
            data.cbData = sizeof(accent);
            pSetWindowCompositionAttribute(hwnd_, &data);
        }

        void ApplyBackdrop() {
            if (!hwnd_) return;

            // DComp 背景层随 Backdrop 变化重建（运行时 SetBackdrop/SetBackdropMode 也能生效）
            UpdateDCompBackdrop();

            // 实现方式（不互相近似）：
            // 背景全部走**手动实现**（不再区分系统/手动）：
            //   Acrylic : DWMWA_USE_HOSTBACKDROPBRUSH（让宿主背景可用=后面窗口内容）+ 我们自己的配方
            //   Mica    : ZufyUI 自绘的缓存壁纸图层
            //   None    : 关闭
            int noneType = DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &noneType, sizeof(noneType));
            ApplyAccentState(ACCENT_DISABLED);
            systemBackdropActive_ = false;

            switch (backdrop_) {
            case Backdrop::Acrylic: {
                BOOL on = TRUE;
                systemBackdropActive_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, DWMWA_USE_HOSTBACKDROPBRUSH, &on, sizeof(on)));
                return;
            }
            case Backdrop::Mica:
                return;   // 画面由 UpdateDCompBackdrop 里的缓存壁纸图层提供
            case Backdrop::None:
                ApplyAccentState(ACCENT_DISABLED);
                return;
            }
            BackdropUnsupported.Fire();
        }

        void ApplyTitleBarColors() {
            if (!hwnd_) return;
            DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionColor_, sizeof(captionColor_));
            DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &textColor_, sizeof(textColor_));
            DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &borderColor_, sizeof(borderColor_));
        }

        void UpdateCompositionText() {
            HIMC hIMC = ImmGetContext(hwnd_);
            if (!hIMC) return;

            LONG strLen = ImmGetCompositionString(hIMC, GCS_COMPSTR, nullptr, 0);
            if (strLen > 0) {
                std::vector<wchar_t> buffer(strLen / sizeof(wchar_t) + 1);
                ImmGetCompositionString(hIMC, GCS_COMPSTR, buffer.data(), strLen);
                buffer[strLen / sizeof(wchar_t)] = L'\0';
                compositionText_ = buffer.data();
                hasComposition_ = true;

                LONG cursorPos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, nullptr, 0);
                if (cursorPos >= 0) compositionCursorPos_ = cursorPos;
                else compositionCursorPos_ = static_cast<int>(compositionText_.size());
            }
            else {
                hasComposition_ = false;
                compositionText_.clear();
                compositionCursorPos_ = 0;
            }
            ImmReleaseContext(hwnd_, hIMC);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void SetImePosition() {
            if (imePosUpdating_) return;
            imePosUpdating_ = true;

            if (!focusedElement_ || !focusedElement_->IsTextInput()) {
                imePosUpdating_ = false;
                return;
            }

            if (layoutNeeded_ || rootElement_->NeedsLayout()) {
                RECT rc;
                GetClientRect(hwnd_, &rc);
                float clientWidthDip = (rc.right - rc.left) * 96.0f / dpi_;
                float clientHeightDip = (rc.bottom - rc.top) * 96.0f / dpi_;
                Thickness rootMargin = rootElement_->GetMargin();
                float left = rootMargin.left, top = rootMargin.top;
                float availWidth = clientWidthDip - left - rootMargin.right;
                float availHeight = clientHeightDip - top - rootMargin.bottom;
                rootElement_->Measure(Size(availWidth, availHeight));
                rootElement_->Arrange(Rect(left, top, availWidth, availHeight));
                layoutNeeded_ = false;
            }

            HIMC hIMC = ImmGetContext(hwnd_);
            if (!hIMC) { imePosUpdating_ = false; return; }

            float scale = dpi_ / 96.0f;
            Rect caretRect = focusedElement_->GetImeCandidateRect();
            float elemHeight = focusedElement_->GetArrangedRect().height * scale;

            COMPOSITIONFORM compForm = {};
            compForm.dwStyle = CFS_FORCE_POSITION;
            compForm.ptCurrentPos.x = static_cast<LONG>(caretRect.x * scale);
            compForm.ptCurrentPos.y = static_cast<LONG>(caretRect.y * scale + 2 * scale);
            compForm.rcArea = { 0, 0, 0, 0 };
            ImmSetCompositionWindow(hIMC, &compForm);

            CANDIDATEFORM candForm = {};
            candForm.dwIndex = 0;
            candForm.dwStyle = CFS_CANDIDATEPOS;
            candForm.ptCurrentPos.x = static_cast<LONG>(caretRect.x * scale);
            candForm.ptCurrentPos.y = static_cast<LONG>((caretRect.y + caretRect.height - 2) * scale);
            candForm.rcArea = { 0, 0, 0, 0 };
            ImmSetCandidateWindow(hIMC, &candForm);

            ImmReleaseContext(hwnd_, hIMC);
            imePosUpdating_ = false;
        }
        void CollectActiveAnimations(UIElement* elem, std::unordered_set<UIElement*>& activeSet) {
            if (!elem || !elem->IsVisible()) return;

            if (elem->HasActiveAnimation()) {
                activeSet.insert(elem);
            }
            for (auto* child : elem->GetChildren()) {
                CollectActiveAnimations(child, activeSet);
            }
        }

        void UpdateTimerState() {
            bool shouldRun = false;
            if (hwnd_ && IsWindow(hwnd_)) {
                shouldRun = IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
            }
            if (shouldRun && !timerRunning_) {
                SetTimer(hwnd_, 1, 16, nullptr);
                timerRunning_ = true;
            }
            else if (!shouldRun && timerRunning_) {
                KillTimer(hwnd_, 1);
                timerRunning_ = false;
            }
        }

        float PixelToDipX(int pixelX) const { return pixelX * 96.0f / dpi_; }
        float PixelToDipY(int pixelY) const { return pixelY * 96.0f / dpi_; }

        HWND hwnd_;
        detail::AppCore* core_ = nullptr;
        int id_ = 0;
        ID2D1Factory* d2dFactory_;
        ID2D1DeviceContext* renderTarget_;      // D2D 1.1 设备上下文（替代 HwndRenderTarget）
        IDXGISwapChain1* swapChain_ = nullptr;
        ID2D1Bitmap1* swapBackBuffer_ = nullptr;
        ICompositor* compositor_ = nullptr;     // 共享（AppCore 生命周期）
        IDesktopWindowTarget* dcompTarget_ = nullptr;
        ISpriteVisual* rootVisual_ = nullptr;
        ISpriteVisual* contentVisual_ = nullptr;
        float clientWidthDip_ = 0.0f, clientHeightDip_ = 0.0f;
        UINT lastClientW_ = 0, lastClientH_ = 0;   // WM_SIZE 尺寸守卫：拖动/SetWindowPos 空触发时跳过重排
        std::wstring lastWindowTitle_;             // 上次同步到自定义标题栏的窗口标题（定时器轮询比对）
        std::shared_ptr<Layout> rootElement_;
        UIElement* currentHovered_;
        UIElement* pressedElement_;
        UIElement* focusedElement_;
        float mouseX_ = 0.0f, mouseY_ = 0.0f;
        DWORD hoverStartTick_ = 0;
        UIElement* tooltipTarget_ = nullptr;
        float tooltipProgress_ = 0.0f;
        D2D1_POINT_2F tooltipAnchorPt_ = {};
        bool focusDirty_ = false;
        bool showFocusRing_ = false;
        UINT dpi_;
        COLORREF captionColor_, textColor_, borderColor_;
        bool hasCustomMinSize_;
        int customMinWidth_, customMinHeight_;
        std::chrono::steady_clock::time_point lastTime_;
        bool layoutNeeded_;
        Backdrop backdrop_;
        bool systemBackdropActive_ = false;
        DWORD backdropColor_;
        Color backgroundColor_;
        // 手动背景（BackdropMode::Manual）：缓存桌面壁纸的独立合成器图层，位于内容层下方
        ComPtr<ISpriteVisual> wallpaperVisual_;
        ComPtr<IDXGISwapChain1> wallpaperSwap_;
        ComPtr<ID2D1Bitmap1> wallpaperBitmap_;
        int wallpaperVirtX_ = 0, wallpaperVirtY_ = 0, wallpaperVirtW_ = 0, wallpaperVirtH_ = 0;
        // 亚克力噪点：动态生成的小位图，用 wrap 平铺铺满（受 AcrylicNoiseOpacity 控制）
        ComPtr<ID2D1Bitmap> noiseBitmap_;
        ComPtr<ID2D1BitmapBrush> noiseBrush_;
        Connection acrylicReloadConn_;   // ReloadAcrylic 信号的连接
        bool animationTimerActive_;   // 常驻定时器，始终 true
        std::shared_ptr<Menu> windowContextMenu_;
        std::unique_ptr<MenuWindow> activeMenuRoot_;
        ComPtr<ITaskbarList3> taskbarList_;
        std::mutex taskbarMtx_;
        HIMAGELIST thumbImageList_ = nullptr;
        void EnsureTaskbarList() {
            std::lock_guard<std::mutex> lock(taskbarMtx_);
            if (taskbarList_ || !hwnd_) return;
            if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList_)))) {
                taskbarList_.Reset();
                return;
            }
            taskbarList_->HrInit();
        }
        ComPtr<IShellLinkW> MakeJumpLink(const JumpListItem& it) {
            ComPtr<IShellLinkW> link;
            if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return nullptr;
            std::wstring target = it.target;
            if (target.empty()) { wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH); target = exe; }
            link->SetPath(target.c_str());
            link->SetArguments(it.arguments.c_str());
            std::wstring icon = it.iconPath.empty() ? target : it.iconPath;
            link->SetIconLocation(icon.c_str(), it.iconIndex);
            ComPtr<IPropertyStore> ps;
            if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&ps)))) {
                PROPVARIANT pv; PropVariantInit(&pv);
                pv.vt = VT_LPWSTR;
                pv.pwszVal = const_cast<wchar_t*>(it.title.c_str());   // SetValue 会拷贝
                ps->SetValue(PKEY_Title, pv);
                ps->Commit();
            }
            return link;
        }
        std::wstring appUserModelId_;
        UIElement* mouseCaptureElement_ = nullptr;
        Window* owner_ = nullptr;
        std::vector<int> hiddenOwnedIds_;   // 父窗口最小化时被隐藏的 owned 窗口 id（还原时恢复）
        // 自定义标题栏 / 窗口外观
        std::shared_ptr<UIElement> customTitleBar_;
        float customTitleBarHeight_ = 0.0f;
        bool titleBarVisible_ = true;
        bool resizable_ = true;
        WindowCorner corner_ = WindowCorner::Default;
        bool customFrame_ = false;          // 是否处于自定义边框模式
        std::vector<Rect> dragRegions_;     // 收集到的可拖动区域（客户坐标 DIP）
        std::vector<UIElement*> npBefore_, npAfter_;   // 不参与布局的元素（仅重排后重建，避免每帧全树收集）
        OwnedMinimizePolicy ownedMinimizePolicy_ = OwnedMinimizePolicy::Hide;   // 见 OwnedMinimizePolicy
        bool inputBlocked_ = false;   // 模态弹窗（子窗口形态）时屏蔽本窗口输入
        bool wasMinimized_ = false;         // 上一状态是否最小化（只有“最小化→还原”才恢复 owned 子窗口）
        bool inSizeMove_ = false;           // 正在拖动/缩放循环：WM_NCHITTEST 直接返回 HTCAPTION，避免每次全树命中检测
        bool imePosUpdating_ = false;
        HIMC defaultIMC_ = nullptr;

        std::wstring compositionText_;
        bool hasComposition_ = false;
        int compositionCursorPos_ = 0;

        // 新增成员
        std::unordered_set<UIElement*> pendingRepaint_;
        bool layoutInvalidated_;
        // 用于记录上一帧活跃动画元素
        std::unordered_set<UIElement*> lastActiveAnimElements_;
        std::unordered_set<UIElement*> activeAnimScratch_;

        bool timerRunning_ = false;
    };

    // ---------- UIElement 路由实现（需 Window 完整类型） ----------
    inline int UIElement::WindowIdOf(Window* w) { return w ? w->GetId() : 0; }
    inline Window* UIElement::GetWindow() const {
        return detail::AppCore::Instance().GetWindowById(windowId_);
    }
    inline void UIElement::RequestRepaint() {
        if (Window* w = GetWindow()) w->MarkRepaint(this);
        else UIZSignals::RepaintRequest(nullptr, this);
    }
    inline void UIElement::InvalidateLayout() {
        // measureDirty_ 冒泡到根（父需读子新 desiredSize，C1 正确）；
        // selfArrangeDirty_ 只置自身；祖先只标 subtreeDirty_（"子树有脏"），不再把自身重排语义污染给祖先。
        measureDirty_ = true; selfArrangeDirty_ = true;
        for (UIElement* p = parent_; p; p = p->parent_) { p->measureDirty_ = true; p->subtreeDirty_ = true; }
        if (Window* w = GetWindow()) w->MarkLayoutInvalidated();
        else UIZSignals::LayoutInvalidated(nullptr);
    }

    // ---------- 应用（Qt 风格：app.CreateWindow(...) -> app.Run()） ----------
    class Application {
    public:
        Application() = default;

        // 创建并注册一个窗口；返回的 shared_ptr 需要被持有以维持窗口存活
        std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title) {
            auto w = std::make_shared<Window>();
            if (!w->Create(width, height, title)) return nullptr;
            return w;
        }
        // 带所有者的窗口（父子/owned）
        std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title, Window* owner) {
            auto w = std::make_shared<Window>();
            if (owner) w->SetOwner(owner);   // 必须在 Create 之前设置，使其以正确的所有者创建
            if (!w->Create(width, height, title)) return nullptr;
            return w;
        }
        // 把已有窗口登记进应用（一般由 Create 自动完成）
        void AddWindow(const std::shared_ptr<Window>& w) {
            if (w) detail::AppCore::Instance().AddWindow(w.get());
        }
        int Run() { return detail::AppCore::Instance().Run(); }
        void Quit(int code = 0) { detail::AppCore::Instance().Quit(code); }
        void CloseAllWindows() {
            auto wins = detail::AppCore::Instance().Windows();
            for (auto* w : wins) if (w) w->Close();
        }
        size_t WindowCount() const { return (size_t)detail::AppCore::Instance().WindowCount(); }

        static Application& Instance() { static Application a; return a; }
    };

    // 关闭当前所有已打开的菜单（独立菜单 + 各窗口的右键菜单）——打开新菜单前先调用，保证互斥
    inline void detail::CloseAllOpenMenus() {
        if (auto& h = MenuWindow::StandaloneHolder()) {
            if (h) { h->CloseAll(); h.reset(); }
        }
        for (Window* w : detail::AppCore::Instance().Windows()) {
            if (w) w->CloseContextMenu();
        }
    }

} // namespace ZufyUI
