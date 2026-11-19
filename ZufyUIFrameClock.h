#pragma once
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace ZufyUI::detail {

// A one-shot pacing clock, not a rendering thread.
// All window state, layout, drawing and presentation stay on the window thread.
// The worker only waits for a high-resolution timer and posts a bounded wake-up
// message so a busy UI queue cannot turn this into a busy loop.
class DisplayFrameClock {
public:
    static constexpr UINT Message = WM_APP + 0x6D1;

    ~DisplayFrameClock() { Stop(); }

    bool Start(HWND window) {
        if (timer_) return true;
        window_ = window;
        stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        timer_ = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer_) timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        if (!stop_ || !timer_) {
            Stop();
            return false;
        }
        try {
            worker_ = std::thread([this] {
                HANDLE handles[] = { stop_, timer_ };
                while (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    // A cancelled timer may already have signalled. Do not let
                    // that old wake-up consume a newly armed future deadline.
                    if (Now() + 0.05 < deadline_) continue;
                    if (scheduled_.load() && !PostMessageW(window_, Message,
                        reinterpret_cast<WPARAM>(this), generation_))
                        scheduled_.store(false);
                }
            });
        }
        catch (...) {
            Stop();
            return false;
        }
        UpdateMonitor(true);
        return true;
    }

    void Stop() {
        if (stop_) SetEvent(stop_);
        if (worker_.joinable()) worker_.join();
        if (timer_) CloseHandle(timer_);
        if (stop_) CloseHandle(stop_);
        timer_ = stop_ = nullptr;
        scheduled_.store(false);
        window_ = nullptr;
    }

    bool Running() const { return timer_ != nullptr; }

    void UpdateMonitor(bool force) {
        HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
        if (!force && monitor == monitor_) return;
        monitor_ = monitor;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        hz_ = 60.0;
        if (GetMonitorInfoW(monitor, &info) &&
            EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) &&
            mode.dmDisplayFrequency > 1)
            hz_ = double(mode.dmDisplayFrequency);
    }

    void SetLimit(int fps) { limit_ = fps; }
    double RefreshHz() const { return hz_; }

    void Request() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!timer_ || scheduled_.exchange(true)) return;
        ++generation_;
        double rate = hz_;
        if (limit_ > 0 && limit_ < rate) rate = double(limit_);
        const double now = Now();
        const double period = 1000.0 / rate;
        // Preserve cadence across fast frames; skip missed slots without bursts.
        deadline_ += period;
        if (deadline_ < now) deadline_ = now;
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>((deadline_ - now) * 10000.0 + 1);
        if (!SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
            scheduled_.store(false);
    }

    bool Consume(WPARAM identity, LPARAM generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        return identity == reinterpret_cast<WPARAM>(this) &&
            generation == generation_ && scheduled_.exchange(false);
    }

    void Cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        scheduled_.store(false);
        if (timer_) CancelWaitableTimer(timer_);
    }

private:
    static double Now() {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    HWND window_ = nullptr;
    HMONITOR monitor_ = nullptr;
    HANDLE timer_ = nullptr;
    HANDLE stop_ = nullptr;
    std::thread worker_;
    std::atomic<bool> scheduled_{ false };
    std::mutex mutex_;
    LPARAM generation_ = 0;
    double hz_ = 60.0;
    double deadline_ = 0.0;
    int limit_ = 0;
};

} // namespace ZufyUI::detail
