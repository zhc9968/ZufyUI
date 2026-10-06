#pragma once
// ============================================================================
// ZufyUICharts.h —— 图表控件（条形图 / 折线 / 饼图 …）
//   依赖 ZufyUI.h（核心）。ChartBase 抽出公用：绘图区几何、值轴 Nice 刻度、
//   类别轴、网格、图例、调色板、内部滚动（横竖）、吸附轴、悬停/命中、Ctrl 缩放、
//   拖动平移、进场动画。BarChart / LineChart / … 只实现「画数据」。
// ============================================================================
#include "ZufyUI.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>

namespace ZufyUI {

    // =========================================================================
    // ChartBase —— 图表基类
    // =========================================================================
    class ChartBase : public UIElement {
    public:
        enum class AText { Left, Center, Right };
        enum class AVert { Top, Center };

        struct Series { std::wstring name; std::vector<double> values; Color color; };

        inline static std::vector<Color> DefaultPalette = {
            Color::FromArgb(255, 0x4C, 0x8B, 0xF5), Color::FromArgb(255, 0xFF, 0x99, 0x33),
            Color::FromArgb(255, 0x4C, 0xAF, 0x50), Color::FromArgb(255, 0xE5, 0x39, 0x35),
            Color::FromArgb(255, 0x9C, 0x27, 0xB0), Color::FromArgb(255, 0x00, 0xBC, 0xD4),
            Color::FromArgb(255, 0xFF, 0xC1, 0x07), Color::FromArgb(255, 0x79, 0x55, 0x48),
        };

        ZSignal<int, int> PointClicked;    // (series, category)
        ZSignal<int> CategoryClicked;      // (category)
        ZSignal<int, int> PointRightClicked;   // (series, category) 右键命中数据点
        ZSignal<int> CategoryRightClicked;     // (category)

        // ---------- 数据 ----------
        void AddCategory(const std::wstring& name) { categories_.push_back(name); for (auto& s : series_) s.values.resize(categories_.size(), 0.0); OnDataChanged(); }
        void AddSeries(const std::wstring& name, const std::vector<double>& v = {}) { Series s; s.name = name; s.values = v; s.color = palette_[series_.size() % palette_.size()]; s.values.resize(categories_.size(), 0.0); series_.push_back(s); OnDataChanged(); }
        void SetValue(int s, int c, double value) { if (s < 0 || s >= (int)series_.size()) return; auto& vals = series_[s].values; if (c >= (int)vals.size()) vals.resize(c + 1, 0.0); vals[c] = value; OnDataChanged(); }
        void SetSeriesValues(int s, const std::vector<double>& v) { if (s < 0 || s >= (int)series_.size()) return; series_[s].values = v; series_[s].values.resize(categories_.size(), 0.0); OnDataChanged(); }
    void UpdateSeriesValues(int s, const std::vector<double>& v) { if (s < 0 || s >= (int)series_.size()) return; series_[s].values = v; series_[s].values.resize(categories_.size(), 0.0); RequestRepaint(); }   // 不重播进场动画（实时刷新用）
        void ClearData() { categories_.clear(); series_.clear(); OnDataChanged(); }
        int CategoryCount() const { return (int)categories_.size(); }
        int SeriesCount() const { return (int)series_.size(); }
        std::vector<Series>& SeriesData() { return series_; }

        // ---------- 配置 ----------
        void SetPalette(const std::vector<Color>& p) { if (!p.empty()) { palette_ = p; for (size_t i = 0; i < series_.size(); ++i) series_[i].color = palette_[i % palette_.size()]; RequestRepaint(); } }
        void SetSeriesColor(int s, Color c) { if (s >= 0 && s < (int)series_.size()) { series_[s].color = c; RequestRepaint(); } }
        void SetValueAxisRange(double min, double max) { axisFixed_ = (max > min); axisMin_ = min; axisMax_ = max; RequestRepaint(); }
        void SetValueAxisAutoRange() { axisFixed_ = false; RequestRepaint(); }
        void SetValueTickCount(int n) { valueTickCount_ = (n < 2 ? 2 : (n > 20 ? 20 : n)); RequestRepaint(); }
        int  GetValueTickCount() const { return valueTickCount_; }
        void SetValueTickStep(double step) { valueStep_ = (step > 0 ? step : 0.0); RequestRepaint(); }
        double GetValueTickStep() const { return valueStep_; }
        void SetValueFormat(int decimals, const std::wstring& prefix = L"", const std::wstring& suffix = L"") { decimals_ = max(0, decimals); valuePrefix_ = prefix; valueSuffix_ = suffix; RequestRepaint(); }
        void SetShowGrid(bool on) { showGrid_ = on; RequestRepaint(); }          // 水平网格（值轴刻度处）
        void SetShowVGrid(bool on) { showVGrid_ = on; RequestRepaint(); }        // 竖直网格（每个类别/数据点处）
        bool IsShowVGrid() const { return showVGrid_; }
        void SetShowLegend(bool on) { showLegend_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetShowValues(bool on) { showValues_ = on; RequestRepaint(); }
        void SetShowCategoryLabels(bool on) { showCategoryLabels_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetLabelFont(const FontSpec& f) { labelFont_ = f; valueFont_ = f; InvalidateLayout(); RequestRepaint(); }
        void SetColors(Color axis, Color grid, Color label) { axisColor_ = axis; gridColor_ = grid; labelColor_ = label; RequestRepaint(); }
        void SetBackgroundColor(Color c) { bgColor_ = c; RequestRepaint(); }
        void SetStickyAxes(bool on) { stickyAxes_ = on; RequestRepaint(); }
        void SetAnimationEnabled(bool on) { animate_ = on; if (!on) animProg_ = 1.0f; RequestRepaint(); }   // 关掉进场动画
        void SetAnimationDuration(float seconds) { animSeconds_ = max(0.01f, seconds); }
        bool IsStickyAxes() const { return stickyAxes_; }
        void SetCategoryWidth(float w) { categoryW_ = max(24.0f, w); RequestRepaint(); }
        void SetPlotHeight(float h) { plotH_ = max(60.0f, h); RequestRepaint(); }
        void SetFitPlotSize(bool on) { fitPlot_ = on; InvalidateLayout(); RequestRepaint(); }   // 值轴方向填满控件（不滚动）
        void SetReferenceLine(double value, const std::wstring& label = L"", Color c = Color::FromArgb(255, 220, 60, 60)) { refEnabled_ = true; refValue_ = value; refLabel_ = label; refColor_ = c; RequestRepaint(); }   // 水平参考线（值）
        void ClearReferenceLine() { refEnabled_ = false; RequestRepaint(); }
        void SetVReferenceLine(int category, const std::wstring& label = L"", Color c = Color::FromArgb(255, 60, 140, 220)) { vRefEnabled_ = true; vRefCat_ = category; vRefLabel_ = label; vRefColor_ = c; RequestRepaint(); }   // 竖直参考线（类别）
        void ClearVReferenceLine() { vRefEnabled_ = false; RequestRepaint(); }

        float GetDefaultHorizontalStretchWeight() const override { return 0.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 0.0f; }
        Size MeasureOverride(const Size& avail) override {
            float w = (GetFillWidth()  && avail.width  != FLT_MAX && avail.width  > 0.0f) ? avail.width  : width_;
            float h = (GetFillHeight() && avail.height != FLT_MAX && avail.height > 0.0f) ? avail.height : height_;
            return Size(w, h);
        }

        void ArrangeOverride(const Rect& finalRect) override { UIElement::ArrangeOverride(finalRect); ComputeContent(); SyncBars(); }

        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;
            if (vBar_ && vBar_->IsVisible() && vBar_->HitTest(x, y)) return vBar_.get();
            if (hBar_ && hBar_->IsVisible() && hBar_->HitTest(x, y)) return hBar_.get();
            return this;
        }
        void RefreshChildren() override {
            if (!childrenDirty_) return;
            childrenDirty_ = false;
            childrenView_.clear();
            if (vBar_) childrenView_.push_back(vBar_.get());
            if (hBar_) childrenView_.push_back(hBar_.get());
        }
        const std::vector<UIElement*>& GetChildren() const override { return childrenView_; }
        void AttachWindowRecursive(Window* w) override { windowId_ = WindowIdOf(w); if (vBar_) vBar_->AttachWindowRecursive(w); if (hBar_) hBar_->AttachWindowRecursive(w); }

        std::wstring GetToolTip() const override {
            std::wstring custom = UIElement::GetToolTip();
            if (!custom.empty()) return custom;
            return HoveredText();
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            ComputeContent();
            SyncBars();
            rt->PushAxisAlignedClip(arrangedRect_.ToD2D(), D2D1_ANTIALIAS_MODE_ALIASED);
            if (bgColor_.a > 0.0f) { if (!bgBrush_) rt->CreateSolidColorBrush(bgColor_.ToD2D(), bgBrush_.GetAddressOf()); else bgBrush_->SetColor(bgColor_.ToD2D()); if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get()); }
            DrawGrid(rt);                              // 网格在最下（数据之下）
            DrawData(rt);                              // 数据
            if (refEnabled_) DrawReference(rt);        // 水平参考线在数据之上
            if (vRefEnabled_) DrawVReference(rt);      // 竖直参考线在数据之上
            DrawAxes(rt);                              // 轴线/刻度/类别标签在数据之上（含吸附轴）
            if (showLegend_) DrawLegend(rt);
            rt->PopAxisAlignedClip();
        }

        void OnMouseMove(float x, float y) override {
            if (pressed_) {
                if (!panning_ && (fabs(x - pressX_) + fabs(y - pressY_) > 4.0f)) panning_ = true;
                if (panning_) { scrollX_ = clamp(panStartScrollX_ - (x - pressX_), 0.0f, maxScrollX_); scrollY_ = clamp(panStartScrollY_ - (y - pressY_), 0.0f, maxScrollY_); PushScrollToBars(); RequestRepaint(); return; }
            }
            UpdateHover(x, y);   // 子类更新悬停项
        }
        void OnMouseLeave() override { if (hoveredS_ != -1 || hoveredC_ != -1) { hoveredS_ = hoveredC_ = -1; RequestRepaint(); } }
        void OnMouseDown(float x, float y) override { pressed_ = true; panning_ = false; pressX_ = x; pressY_ = y; panStartScrollX_ = scrollX_; panStartScrollY_ = scrollY_; }
        void OnMouseUp(float x, float y) override {
            if (pressed_ && !panning_) {
                OnDataClick(x, y);
                if (clickedS_ >= 0) PointClicked(clickedS_, clickedC_);
                if (clickedC_ >= 0) CategoryClicked(clickedC_);
            }
            pressed_ = false; panning_ = false;
        }
        bool OnMouseWheel(float dx, float dy) override {
            (void)dx;
            if (GetKeyState(VK_CONTROL) & 0x8000) { zoom_ = clamp(zoom_ * (1.0f + dy * 0.12f), 0.4f, 3.0f); InvalidateLayout(); RequestRepaint(); return true; }
            // 只有内容真的可滚时才消费，否则返回 false 冒泡给外层（否则外层滚动会“卡住”）
            if (GetKeyState(VK_SHIFT) & 0x8000) { if (maxScrollX_ <= 0.0f) return false; scrollX_ = clamp(scrollX_ - dy * 40.0f, 0.0f, maxScrollX_); }
            else { if (maxScrollY_ <= 0.0f) return false; scrollY_ = clamp(scrollY_ - dy * 40.0f, 0.0f, maxScrollY_); }
            PushScrollToBars(); RequestRepaint(); return true;
        }
        bool OnContextMenu(float x, float y) override {
            UpdateHover(x, y);   // 命中最近的数据点（子类实现）
            if (hoveredC_ >= 0) {
                CategoryRightClicked(hoveredC_);
                if (hoveredS_ >= 0) PointRightClicked(hoveredS_, hoveredC_);
            }
            return false;        // false：若元素也设了 SetContextMenu，仍会显示（信号已先发）
        }
        void UpdateAnimation(float dt) override {
            if (animProg_ < 1.0f) { animProg_ = min(1.0f, animProg_ + dt / max(0.05f, animSeconds_)); RequestRepaint(); }
            if (vBar_) vBar_->UpdateAnimation(dt);
            if (hBar_) hBar_->UpdateAnimation(dt);
        }
        bool HasActiveAnimation() const override { return animProg_ < 1.0f || (vBar_ && vBar_->HasActiveAnimation()) || (hBar_ && hBar_->HasActiveAnimation()); }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); gridBrush_.Reset(); axisBrush_.Reset(); textBrush_.Reset(); legendBrush_.Reset(); refBrush_.Reset(); vRefBrush_.Reset();
            if (vBar_) vBar_->ReleaseDeviceResources();
            if (hBar_) hBar_->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    protected:
        // ---- 子类实现 ----
        virtual void DrawData(ID2D1RenderTarget* rt) = 0;
        virtual void ComputeValueRange(double& lo, double& hi) { bool any = false; for (auto& s : series_) for (double v : s.values) { if (!any) { lo = hi = v; any = true; } else { lo = min(lo, v); hi = max(hi, v); } } if (!any) { lo = 0; hi = 1; } lo = min(lo, 0.0); hi = max(hi, 0.0); }
        virtual void UpdateHover(float, float) {}
        virtual void OnDataClick(float, float) {}
        virtual std::wstring HoveredText() const { return std::wstring(); }

        // ---- 公用几何 ----
        void ComputeContent() {
            const Rect& r = arrangedRect_;
            if (r.width <= 1.0f || r.height <= 1.0f) return;
            IDWriteTextFormat* lf = FontManager::Instance().GetFormat(labelFont_);
            float pad = 10.0f;
            double dmin, dmax; ComputeValueRange(dmin, dmax);
            double axMin, axMax, step; int tickCount = valueTickCount_;
            if (axisFixed_ && axisMax_ > axisMin_) { axMin = axisMin_; axMax = axisMax_; step = (axMax - axMin) / (tickCount - 1); }
            else if (valueStep_ > 0.0) { axMin = std::floor(dmin / valueStep_) * valueStep_; axMax = std::ceil(dmax / valueStep_) * valueStep_; step = valueStep_; }
            else NiceScale(dmin, dmax, tickCount, axMin, axMax, step);
            axisMin_ = axMin; axisMax_ = axMax;
            ticks_.clear();
            for (double v = axMin; v <= axMax + 1e-9; v += step) { Tick t; t.value = v; t.label = FormatValue(v); ticks_.push_back(t); if ((int)ticks_.size() > 50) break; }
            float axisLeft = 0; if (lf) for (auto& t : ticks_) axisLeft = max(axisLeft, MeasureTextW(t.label, lf)); axisLeft += 8.0f;
            int C = (int)categories_.size(), S = (int)series_.size();
            float legendH = (showLegend_ && S > 0) ? ((labelFont_.size * 1.9f) * (float)((S + 3) / 4) + 6.0f) : 0.0f;
            float catLabelW = 0; if (lf) for (auto& c : categories_) catLabelW = max(catLabelW, MeasureTextW(c, lf));
            float catAxisH = showCategoryLabels_ ? (labelFont_.size * 1.5f) : 4.0f;
            float valueAxisH = labelFont_.size * 1.5f;
            float valueSize = plotH_ * zoom_;
            if (fitPlot_)   // 值轴方向填满可用空间（不产生滚动条），便于小窗口
                valueSize = max(40.0f, horizontal_ ? (r.width - pad * 2 - (showCategoryLabels_ ? catLabelW + 8.0f : 8.0f))
                                                   : (r.height - pad * 2 - legendH - catAxisH));
            if (!horizontal_) {
                axisLeft_ = axisLeft;
                axisBottom_ = catAxisH;
                contentW_ = pad + axisLeft_ + max(1, C) * (categoryW_ * zoom_) + pad;   // 横向也随 zoom 缩放
                contentH_ = pad + legendH + valueSize + axisBottom_ + pad;
                plot_ = Rect(0, 0, max(1, C) * (categoryW_ * zoom_), valueSize);
            }
            else {
                axisLeft_ = showCategoryLabels_ ? (catLabelW + 8.0f) : 8.0f;
                axisBottom_ = valueAxisH + 2.0f;
                contentW_ = pad + axisLeft_ + valueSize + pad;
                contentH_ = pad + legendH + max(1, C) * (categoryW_ * zoom_) + axisBottom_ + pad;
                plot_ = Rect(0, 0, valueSize, max(1, C) * (categoryW_ * zoom_));
            }
            float viewW = max(0.0f, r.width - (hBarVisible_ ? barW_ : 0.0f));
            float viewH = max(0.0f, r.height - (vBarVisible_ ? barW_ : 0.0f));
            maxScrollX_ = max(0.0f, contentW_ - viewW);
            maxScrollY_ = max(0.0f, contentH_ - viewH);
            scrollX_ = clamp(scrollX_, 0.0f, maxScrollX_);
            scrollY_ = clamp(scrollY_, 0.0f, maxScrollY_);
            baseX_ = r.x - scrollX_; baseY_ = r.y - scrollY_;
            plot_.x = baseX_ + pad + axisLeft_; plot_.y = baseY_ + pad + legendH;
            legendRect_ = Rect(baseX_ + pad, baseY_ + pad, contentW_ - pad * 2, legendH);
        }
        void SyncBars() {
            if (!vBar_ || !hBar_) return;
            vBarVisible_ = (contentH_ > arrangedRect_.height + 0.5f);
            hBarVisible_ = (contentW_ > arrangedRect_.width + 0.5f);
            float vw = max(0.0f, arrangedRect_.width - (hBarVisible_ ? barW_ : 0.0f));
            float vh = max(0.0f, arrangedRect_.height - (vBarVisible_ ? barW_ : 0.0f));
            vBar_->Arrange(Rect(arrangedRect_.x + arrangedRect_.width - barW_, arrangedRect_.y, barW_, vh)); vBar_->SetVisibleNoInvalidate(vBarVisible_); vBar_->SetRange(scrollY_, maxScrollY_, vh);
            hBar_->Arrange(Rect(arrangedRect_.x, arrangedRect_.y + arrangedRect_.height - barW_, vw, barW_)); hBar_->SetVisibleNoInvalidate(hBarVisible_); hBar_->SetRange(scrollX_, maxScrollX_, vw);
        }
        void PushScrollToBars() { if (vBar_) vBar_->SetValue(scrollY_); if (hBar_) hBar_->SetValue(scrollX_); }
        void OnDataChanged() { animProg_ = animate_ ? 0.0f : 1.0f; InvalidateLayout(); RequestRepaint(); }
        void InitScrollBars() {
            vBar_ = std::make_shared<ScrollBar>(true); vBar_->SetParent(this); vBar_->SetBarWidth(barW_); vBar_->SetMinLength(20.0f); vBar_->SetHitExtra(4.0f);
            vBar_->SetColors(Color::FromArgb(200, 140, 140, 140).ToD2D(), Color::FromArgb(255, 90, 90, 90).ToD2D(), D2D1::ColorF(0, 0, 0, 0));
            vBar_->ValueChanged = [this](float v, bool) { scrollY_ = v; RequestRepaint(); };
            hBar_ = std::make_shared<ScrollBar>(false); hBar_->SetParent(this); hBar_->SetBarWidth(barW_); hBar_->SetMinLength(20.0f); hBar_->SetHitExtra(4.0f);
            hBar_->SetColors(Color::FromArgb(200, 140, 140, 140).ToD2D(), Color::FromArgb(255, 90, 90, 90).ToD2D(), D2D1::ColorF(0, 0, 0, 0));
            hBar_->ValueChanged = [this](float v, bool) { scrollX_ = v; RequestRepaint(); };
        }
        float ValueY(double v) const { return plot_.y + plot_.height - (float)((v - axisMin_) / (axisMax_ - axisMin_)) * plot_.height; }
        float ValueX(double v) const { return plot_.x + (float)((v - axisMin_) / (axisMax_ - axisMin_)) * plot_.width; }
        float CatX(int c) const { int C = max(1, (int)categories_.size()); return plot_.x + (c + 0.5f) * (plot_.width / C); }
        float CatY(int c) const { int C = max(1, (int)categories_.size()); return plot_.y + (c + 0.5f) * (plot_.height / C); }
        double ValueOf(int s, int c) const { if (s < 0 || s >= (int)series_.size()) return 0; const auto& v = series_[s].values; return (c >= 0 && c < (int)v.size()) ? v[c] : 0.0; }
        std::wstring FormatValue(double v) const { wchar_t buf[64]; double mul = std::pow(10.0, decimals_); double rv = std::round(v * mul) / mul; swprintf(buf, 64, L"%.*f", decimals_, rv); return valuePrefix_ + buf + valueSuffix_; }

        void DrawGrid(ID2D1RenderTarget* rt) {   // 网格线（数据之下）
            if (!showGrid_ && !showVGrid_) return;
            const Rect& rr = arrangedRect_;
            if (!gridBrush_) rt->CreateSolidColorBrush(gridColor_.ToD2D(), gridBrush_.GetAddressOf());
            if (!gridBrush_) return;
            if (showGrid_) for (auto& t : ticks_) {
                if (!horizontal_) { float y = ValueY(t.value); if (y >= rr.y && y <= rr.y + rr.height) rt->DrawLine(D2D1::Point2F(plot_.x, y), D2D1::Point2F(plot_.x + plot_.width, y), gridBrush_.Get(), 1.0f); }
                else { float x = ValueX(t.value); if (x >= rr.x && x <= rr.x + rr.width) rt->DrawLine(D2D1::Point2F(x, plot_.y), D2D1::Point2F(x, plot_.y + plot_.height), gridBrush_.Get(), 1.0f); }
            }
            if (showVGrid_) {   // 每个类别/数据点处线（类比刻度）
                int C = max(1, (int)categories_.size());
                for (int c = 0; c < (int)categories_.size(); ++c) {
                    if (!horizontal_) { float x = plot_.x + (c + 0.5f) * (plot_.width / C); if (x >= rr.x && x <= rr.x + rr.width) rt->DrawLine(D2D1::Point2F(x, plot_.y), D2D1::Point2F(x, plot_.y + plot_.height), gridBrush_.Get(), 1.0f); }
                    else { float y = plot_.y + (c + 0.5f) * (plot_.height / C); if (y >= rr.y && y <= rr.y + rr.height) rt->DrawLine(D2D1::Point2F(plot_.x, y), D2D1::Point2F(plot_.x + plot_.width, y), gridBrush_.Get(), 1.0f); }
                }
            }
        }
        void DrawAxes(ID2D1RenderTarget* rt) {    // 轴线 + 刻度标签 + 类别标签（数据之上，含吸附轴）
            const Rect& rr = arrangedRect_;
            if (!axisBrush_) rt->CreateSolidColorBrush(axisColor_.ToD2D(), axisBrush_.GetAddressOf()); else axisBrush_->SetColor(axisColor_.ToD2D());
            if (!horizontal_) {
                float yAxisX = plot_.x, yLabelRight = plot_.x - 6.0f;
                if (stickyAxes_ && plot_.x < rr.x + 0.5f) { yAxisX = rr.x + axisLeft_; yLabelRight = rr.x + axisLeft_ - 6.0f; }
                float xAxisY = plot_.y + plot_.height; bool xBelow = true;
                if (stickyAxes_ && xAxisY > rr.y + rr.height - 0.5f) { xAxisY = rr.y + rr.height; xBelow = false; }
                for (auto& t : ticks_) {   // 值轴刻度标签
                    float y = ValueY(t.value);
                    if (y >= rr.y - 8 && y <= rr.y + rr.height + 8) DrawText(rt, t.label, Rect(yLabelRight - axisLeft_, y - 8.0f, axisLeft_, 16.0f), labelColor_, AText::Right, AVert::Center, labelFont_);
                }
                if (axisBrush_) {
                    float vy0 = max(plot_.y, rr.y), vy1 = min(plot_.y + plot_.height, rr.y + rr.height);
                    if (vy1 > vy0) rt->DrawLine(D2D1::Point2F(yAxisX, vy0), D2D1::Point2F(yAxisX, vy1), axisBrush_.Get(), 1.0f);
                    float hx0 = max(plot_.x, rr.x), hx1 = min(plot_.x + plot_.width, rr.x + rr.width);
                    if (hx1 > hx0) rt->DrawLine(D2D1::Point2F(hx0, xAxisY), D2D1::Point2F(hx1, xAxisY), axisBrush_.Get(), 1.0f);
                }
                if (showCategoryLabels_) {
                    float catLabelH = labelFont_.size * 1.5f;
                    float labelY = xBelow ? (xAxisY + 2.0f) : (xAxisY - catLabelH - 2.0f);
                    int C = max(1, (int)categories_.size());
                    for (int c = 0; c < (int)categories_.size(); ++c) { float cw = plot_.width / C; DrawText(rt, categories_[c], Rect(plot_.x + c * cw, labelY, cw, catLabelH), labelColor_, AText::Center, AVert::Top, labelFont_); }
                }
            }
            else {
                float xAxisY = plot_.y + plot_.height, yAxisX = plot_.x; bool xBelow = true;
                if (stickyAxes_ && xAxisY > rr.y + rr.height - 0.5f) { xAxisY = rr.y + rr.height; xBelow = false; }
                if (stickyAxes_ && plot_.x < rr.x + 0.5f) yAxisX = rr.x + axisLeft_;
                float valueLabelH = labelFont_.size * 1.5f;
                for (auto& t : ticks_) {   // 值轴刻度标签（底部横轴）
                    float x = ValueX(t.value);
                    if (x >= rr.x - 8 && x <= rr.x + rr.width + 8) DrawText(rt, t.label, Rect(x - 40.0f, xBelow ? (xAxisY + 2.0f) : (xAxisY - valueLabelH - 2.0f), 80.0f, valueLabelH), labelColor_, AText::Center, AVert::Top, labelFont_);
                }
                if (axisBrush_) {
                    float hx0 = max(plot_.x, rr.x), hx1 = min(plot_.x + plot_.width, rr.x + rr.width);
                    if (hx1 > hx0) rt->DrawLine(D2D1::Point2F(hx0, xAxisY), D2D1::Point2F(hx1, xAxisY), axisBrush_.Get(), 1.0f);
                    float vy0 = max(plot_.y, rr.y), vy1 = min(plot_.y + plot_.height, rr.y + rr.height);
                    if (vy1 > vy0) rt->DrawLine(D2D1::Point2F(yAxisX, vy0), D2D1::Point2F(yAxisX, vy1), axisBrush_.Get(), 1.0f);
                }
                if (showCategoryLabels_) {   // 类别标签在左侧纵轴
                    float labelW = axisLeft_ - 6.0f;
                    int C = max(1, (int)categories_.size());
                    for (int c = 0; c < (int)categories_.size(); ++c) { float ch = plot_.height / C; DrawText(rt, categories_[c], Rect(plot_.x - labelW - 2.0f, plot_.y + c * ch, labelW, ch), labelColor_, AText::Right, AVert::Center, labelFont_); }
                }
            }
        }
        void DrawReference(ID2D1RenderTarget* rt) {
            if (!refBrush_) rt->CreateSolidColorBrush(refColor_.ToD2D(), refBrush_.GetAddressOf()); else refBrush_->SetColor(refColor_.ToD2D());
            if (!horizontal_) {
                float ry = ValueY(refValue_);
                if (ry < plot_.y - 1 || ry > plot_.y + plot_.height + 1) return;
                if (refBrush_) rt->DrawLine(D2D1::Point2F(plot_.x, ry), D2D1::Point2F(plot_.x + plot_.width, ry), refBrush_.Get(), 1.0f);
                if (!refLabel_.empty()) DrawText(rt, refLabel_, Rect(plot_.x + 4, ry - 14, plot_.width - 8, 14), refColor_, AText::Left, AVert::Center, valueFont_);
            } else {
                float rx = ValueX(refValue_);
                if (rx < plot_.x - 1 || rx > plot_.x + plot_.width + 1) return;
                if (refBrush_) rt->DrawLine(D2D1::Point2F(rx, plot_.y), D2D1::Point2F(rx, plot_.y + plot_.height), refBrush_.Get(), 1.0f);
                if (!refLabel_.empty()) DrawText(rt, refLabel_, Rect(rx + 3, plot_.y, max(10.0f, plot_.width - (rx - plot_.x) - 3), 14), refColor_, AText::Left, AVert::Top, valueFont_);
            }
        }
        void DrawVReference(ID2D1RenderTarget* rt) {
            if (!vRefBrush_) rt->CreateSolidColorBrush(vRefColor_.ToD2D(), vRefBrush_.GetAddressOf()); else vRefBrush_->SetColor(vRefColor_.ToD2D());
            int C = max(1, (int)categories_.size());
            if (!horizontal_) {
                float x = plot_.x + (vRefCat_ + 0.5f) * (plot_.width / C);
                if (x < plot_.x - 1 || x > plot_.x + plot_.width + 1) return;
                if (vRefBrush_) rt->DrawLine(D2D1::Point2F(x, plot_.y), D2D1::Point2F(x, plot_.y + plot_.height), vRefBrush_.Get(), 1.0f);
                if (!vRefLabel_.empty()) DrawText(rt, vRefLabel_, Rect(x + 3, plot_.y, max(10.0f, plot_.width - (x - plot_.x) - 3), 14), vRefColor_, AText::Left, AVert::Top, valueFont_);
            } else {
                float y = plot_.y + (vRefCat_ + 0.5f) * (plot_.height / C);
                if (y < plot_.y - 1 || y > plot_.y + plot_.height + 1) return;
                if (vRefBrush_) rt->DrawLine(D2D1::Point2F(plot_.x, y), D2D1::Point2F(plot_.x + plot_.width, y), vRefBrush_.Get(), 1.0f);
                if (!vRefLabel_.empty()) DrawText(rt, vRefLabel_, Rect(plot_.x + 4, y - 14, plot_.width - 8, 14), vRefColor_, AText::Left, AVert::Center, valueFont_);
            }
        }
        void DrawLegend(ID2D1RenderTarget* rt) {
            if (series_.empty()) return;
            const float rowH = labelFont_.size * 1.9f; float x = legendRect_.x, y = legendRect_.y, sw = labelFont_.size;
            IDWriteTextFormat* lf = FontManager::Instance().GetFormat(labelFont_);
            for (auto& s : series_) {
                float tw = MeasureTextW(s.name, lf);
                if (x + sw + 4 + tw > legendRect_.x + legendRect_.width) { x = legendRect_.x; y += rowH; }
                if (!legendBrush_) rt->CreateSolidColorBrush(s.color.ToD2D(), legendBrush_.GetAddressOf()); else legendBrush_->SetColor(s.color.ToD2D());
                if (legendBrush_) rt->FillRectangle(D2D1::RectF(x, y + 2.0f, x + sw, y + rowH - 3.0f), legendBrush_.Get());
                DrawText(rt, s.name, Rect(x + sw + 4, y, max(10.0f, legendRect_.x + legendRect_.width - (x + sw + 4)), rowH), labelColor_, AText::Left, AVert::Center, labelFont_);
                x += sw + 4 + tw + 14;
            }
        }
        float MeasureTextW(const std::wstring& s, IDWriteTextFormat* fmt) {
            if (!fmt || s.empty()) return 0; IDWriteFactory* f = FontManager::Instance().GetFactory(); if (!f) return 0;
            ComPtr<IDWriteTextLayout> lay; if (FAILED(f->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt, 1e5f, 1e5f, &lay)) || !lay) return 0;
            DWRITE_TEXT_METRICS m{}; lay->GetMetrics(&m); return m.width;
        }
        void DrawText(ID2D1RenderTarget* rt, const std::wstring& s, const Rect& rect, Color col, AText ha, AVert va, const FontSpec& fs) {
            if (s.empty()) return; IDWriteTextFormat* fmt = FontManager::Instance().GetFormat(fs); if (!fmt) return;
            IDWriteFactory* f = FontManager::Instance().GetFactory(); if (!f) return;
            ComPtr<IDWriteTextLayout> lay;
            if (FAILED(f->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt, max(1.0f, rect.width), max(1.0f, rect.height), &lay)) || !lay) return;
            lay->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            lay->SetTextAlignment(ha == AText::Left ? DWRITE_TEXT_ALIGNMENT_LEADING : ha == AText::Right ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_CENTER);
            DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; lay->SetTrimming(&tr, nullptr);
            if (!textBrush_) rt->CreateSolidColorBrush(col.ToD2D(), textBrush_.GetAddressOf()); else textBrush_->SetColor(col.ToD2D());
            if (textBrush_) rt->DrawTextLayout(D2D1::Point2F(rect.x, rect.y), lay.Get(), textBrush_.Get());
        }
        static Color Lighten(Color c, float t) { return Color(c.r + (1 - c.r) * t, c.g + (1 - c.g) * t, c.b + (1 - c.b) * t, c.a); }
        static Color ContrastColor(Color c) { float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; return lum > 0.6f ? Color::FromArgb(255, 40, 40, 40) : Color::FromArgb(255, 255, 255, 255); }
        static double NiceNum(double range, bool round) { if (range <= 0) return 1; double exp = std::floor(std::log10(range)); double f = range / std::pow(10.0, exp), nf; if (round) nf = (f < 1.5) ? 1 : (f < 3) ? 2 : (f < 7) ? 5 : 10; else nf = (f <= 1) ? 1 : (f <= 2) ? 2 : (f <= 5) ? 5 : 10; return nf * std::pow(10.0, exp); }
        static void NiceScale(double lo, double hi, int maxTicks, double& outMin, double& outMax, double& outStep) {
            if (hi <= lo) hi = lo + 1.0; double range = NiceNum(hi - lo, false); double step = NiceNum(range / (maxTicks - 1), true);
            outMin = std::floor(lo / step) * step; outMax = std::ceil(hi / step) * step; outStep = step;
        }

        // ---- 成员 ----
        struct Bar { int series, category; Rect rect; };
        struct Tick { double value; std::wstring label; };
        std::vector<std::wstring> categories_;
        std::vector<Series> series_;
        std::vector<Color> palette_ = DefaultPalette;
        Color axisColor_ = Color::FromArgb(255, 160, 160, 160), gridColor_ = Color::FromArgb(60, 120, 120, 120), labelColor_ = Color::FromArgb(255, 90, 90, 90), bgColor_ = Color::FromArgb(0, 0, 0, 0);
        FontSpec labelFont_, valueFont_;
        bool axisFixed_ = false; double axisMin_ = 0, axisMax_ = 1;
        int valueTickCount_ = 5; double valueStep_ = 0.0;
        int decimals_ = 0; std::wstring valuePrefix_, valueSuffix_;
        bool showGrid_ = true, showVGrid_ = false, showLegend_ = true, showValues_ = false, showCategoryLabels_ = true, stickyAxes_ = true;
        bool horizontal_ = false;                                   // BarChart 横向时置 true（值轴=横轴、类别轴=纵轴）
        float axisBottom_ = 4.0f;                                   // 底部轴占高（含刻度标签）
        bool refEnabled_ = false; double refValue_ = 0; std::wstring refLabel_; Color refColor_ = Color::FromArgb(255, 220, 60, 60);
        bool vRefEnabled_ = false; int vRefCat_ = 0; std::wstring vRefLabel_; Color vRefColor_ = Color::FromArgb(255, 60, 140, 220);
        float animProg_ = 0.0f, animSeconds_ = 0.5f; bool animate_ = true;
        float categoryW_ = 70.0f, plotH_ = 260.0f, barW_ = 10.0f, zoom_ = 1.0f, axisLeft_ = 40.0f; bool fitPlot_ = false;
        float scrollX_ = 0, scrollY_ = 0, maxScrollX_ = 0, maxScrollY_ = 0, contentW_ = 0, contentH_ = 0, baseX_ = 0, baseY_ = 0;
        bool vBarVisible_ = false, hBarVisible_ = false;
        Rect plot_, legendRect_;
        std::vector<Tick> ticks_;
        int hoveredS_ = -1, hoveredC_ = -1;
        int clickedS_ = -1, clickedC_ = -1;
        bool pressed_ = false, panning_ = false;
        float pressX_ = 0, pressY_ = 0, panStartScrollX_ = 0, panStartScrollY_ = 0;
        std::shared_ptr<ScrollBar> vBar_, hBar_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_, gridBrush_, axisBrush_, textBrush_, legendBrush_, refBrush_, vRefBrush_;

        ChartBase() { width_ = 420.0f; height_ = 260.0f; labelFont_.size = 12.0f; valueFont_.size = 11.0f; }
    };

    // =========================================================================
    // BarChart —— 条形图
    // =========================================================================
    class BarChart : public ChartBase {
    public:
        enum class Orientation { Vertical, Horizontal };
        enum class StackMode { Grouped, Stacked, PercentStacked, Overlapped };

        inline static float DefaultWidth = 420.0f, DefaultHeight = 260.0f;

        BarChart() { InitScrollBars(); }

        void SetOrientation(Orientation o) { orientation_ = o; horizontal_ = (o == Orientation::Horizontal); InvalidateLayout(); RequestRepaint(); }
        Orientation GetOrientation() const { return orientation_; }
        void SetStackMode(StackMode m) { stackMode_ = m; OnDataChanged(); }
        StackMode GetStackMode() const { return stackMode_; }
        void SetBarGap(float ratio) { barGap_ = clamp(ratio, 0.0f, 0.9f); RequestRepaint(); }
        void SetStackBarRatio(float r) { stackBarRatio_ = clamp(r, 0.1f, 1.0f); RequestRepaint(); }
        void SetGroupGap(float ratio) { groupGap_ = clamp(ratio, 0.0f, 0.9f); RequestRepaint(); }
        void SetCornerRadius(float r) { cornerRadius_ = max(0.0f, r); RequestRepaint(); }
        void SetBarOutline(float w, Color c = Color::FromArgb(255, 80, 80, 80)) { outlineW_ = max(0.0f, w); outlineColor_ = c; RequestRepaint(); }
        void SetBarColor(int s, int c, Color col) { barColorMap_[Key(s, c)] = col; RequestRepaint(); }
        void SetSelectedCategory(int c) { selectedC_ = c; RequestRepaint(); }
        int GetSelectedCategory() const { return selectedC_; }

    protected:
        void ComputeValueRange(double& lo, double& hi) override {
            if (stackMode_ == StackMode::Stacked) {
                int C = (int)categories_.size(), S = (int)series_.size(); lo = 0; hi = 0; bool any = false;
                for (int c = 0; c < C; ++c) { double p = 0, n = 0; for (int s = 0; s < S; ++s) { double v = ValueOf(s, c); if (v >= 0) p += v; else n += v; } hi = max(hi, p); lo = min(lo, n); any = true; }
                if (!any) { lo = 0; hi = 1; }
            }
            else if (stackMode_ == StackMode::PercentStacked) {
                bool neg = false; for (auto& s : series_) for (double v : s.values) if (v < 0) neg = true;
                lo = neg ? -100.0 : 0.0; hi = 100.0;
            }
            else ChartBase::ComputeValueRange(lo, hi);
        }
        unsigned long long Key(int s, int c) { return ((unsigned long long)(unsigned)s << 32) | (unsigned)c; }
        Color BarColor(int s, int c) { auto it = barColorMap_.find(Key(s, c)); return it != barColorMap_.end() ? it->second : (s >= 0 && s < (int)series_.size() ? series_[s].color : palette_[0]); }

        void DrawData(ID2D1RenderTarget* rt) override {
            float range = (float)(axisMax_ - axisMin_);
            baseY_ = ValueY(0.0);
            bars_.clear();
            int C = (int)categories_.size(), S = (int)series_.size();
            if (C <= 0 || S <= 0) return;
            if (horizontal_) {                     // 横向条形：值轴=横、类别轴=纵
                baseX_ = ValueX(0.0);
                float slot = plot_.height / C; slotW_ = slot;
                float groupInner = slot * (1.0f - groupGap_);
                for (int c = 0; c < C; ++c) {
                    float gy = plot_.y + c * slot + (slot - groupInner) * 0.5f;
                    if (stackMode_ == StackMode::Stacked || stackMode_ == StackMode::PercentStacked) {
                        float stH = groupInner * stackBarRatio_, stY = gy + (groupInner - stH) * 0.5f;
                        double posSum = 0, negSum = 0; for (int s = 0; s < S; ++s) { double v = ValueOf(s, c); if (v >= 0) posSum += v; else negSum -= v; }
                        float xPos = baseX_, xNeg = baseX_;
                        for (int s = 0; s < S; ++s) {
                            double v = ValueOf(s, c);
                            if (stackMode_ == StackMode::PercentStacked) v = (v >= 0) ? (posSum > 0 ? v / posSum * 100.0 : 0) : (negSum > 0 ? v / negSum * 100.0 : 0);
                            float w = (float)(v / range) * plot_.width;
                            if (v >= 0) { bars_.push_back({ s, c, Rect(xPos, stY, fabs(w), stH) }); xPos += w; }
                            else { bars_.push_back({ s, c, Rect(xNeg, stY, fabs(w), stH) }); xNeg += w; }
                        }
                    }
                    else if (stackMode_ == StackMode::Overlapped) {
                        for (int s = 0; s < S; ++s) { double v = ValueOf(s, c); float right = baseX_ + (float)(v / range) * plot_.width; bars_.push_back({ s, c, Rect(min(baseX_, right), gy, fabs(right - baseX_), groupInner) }); }
                    }
                    else {
                        float step2 = groupInner / S, bh = step2 * (1.0f - barGap_);
                        for (int s = 0; s < S; ++s) { float by = gy + s * step2 + (step2 - bh) * 0.5f; double v = ValueOf(s, c); float right = baseX_ + (float)(v / range) * plot_.width; bars_.push_back({ s, c, Rect(min(baseX_, right), by, fabs(right - baseX_), bh) }); }
                    }
                }
                float p = animProg_;
                if (selectedC_ >= 0 && selectedC_ < C) { if (!selBrush_) rt->CreateSolidColorBrush(Color::FromArgb(28, 0, 120, 215).ToD2D(), selBrush_.GetAddressOf()); if (selBrush_) rt->FillRectangle(D2D1::RectF(plot_.x, plot_.y + selectedC_ * slotW_, plot_.x + plot_.width, plot_.y + (selectedC_ + 1) * slotW_), selBrush_.Get()); }
                for (auto& br : bars_) {
                    float x0 = br.rect.x, x1 = br.rect.x + br.rect.width, y = br.rect.y, h = br.rect.height;
                    float anchor = (fabs(x1 - baseX_) <= fabs(x0 - baseX_)) ? x1 : x0, farEnd = (anchor == x1) ? x0 : x1, fa = anchor + (farEnd - anchor) * p;
                    D2D1_RECT_F r = D2D1::RectF(min(anchor, fa), y, max(anchor, fa), y + h);
                    Color c = BarColor(br.series, br.category);
                    if (stackMode_ == StackMode::Overlapped) c = Color(c.r, c.g, c.b, 0.65f);
                    if (br.series == hoveredS_ && br.category == hoveredC_) c = Lighten(c, 0.15f);
                    if (!barBrush_) rt->CreateSolidColorBrush(c.ToD2D(), barBrush_.GetAddressOf()); else barBrush_->SetColor(c.ToD2D());
                    if (barBrush_) { if (cornerRadius_ > 0.0f) rt->FillRoundedRectangle(D2D1::RoundedRect(r, cornerRadius_, cornerRadius_), barBrush_.Get()); else rt->FillRectangle(r, barBrush_.Get()); }
                    if (outlineW_ > 0.0f) { if (!outlineBrush_) rt->CreateSolidColorBrush(outlineColor_.ToD2D(), outlineBrush_.GetAddressOf()); else outlineBrush_->SetColor(outlineColor_.ToD2D()); if (outlineBrush_) rt->DrawRectangle(r, outlineBrush_.Get(), outlineW_); }
                    if (showValues_ && p > 0.99f) {
                        std::wstring vs = FormatValue(ValueOf(br.series, br.category));
                        float xa = min(anchor, fa), xb = max(anchor, fa);
                        if (stackMode_ == StackMode::Grouped) DrawText(rt, vs, Rect(xb + 4.0f, y, 70.0f, h), labelColor_, AText::Left, AVert::Center, valueFont_);
                        else if (xb - xa > valueFont_.size * 1.2f) DrawText(rt, vs, Rect(xa, y, xb - xa, h), ContrastColor(c), AText::Center, AVert::Center, valueFont_);
                    }
                }
                return;
            }
            float slot = plot_.width / C; slotW_ = slot;
            float groupInner = slot * (1.0f - groupGap_);
            for (int c = 0; c < C; ++c) {
                float gx = plot_.x + c * slot + (slot - groupInner) * 0.5f;
                if (stackMode_ == StackMode::Stacked || stackMode_ == StackMode::PercentStacked) {
                    float stW = groupInner * stackBarRatio_, stX = gx + (groupInner - stW) * 0.5f;
                    double posSum = 0, negSum = 0; for (int s = 0; s < S; ++s) { double v = ValueOf(s, c); if (v >= 0) posSum += v; else negSum -= v; }
                    float yPos = baseY_, yNeg = baseY_;
                    for (int s = 0; s < S; ++s) {
                        double v = ValueOf(s, c);
                        if (stackMode_ == StackMode::PercentStacked) v = (v >= 0) ? (posSum > 0 ? v / posSum * 100.0 : 0) : (negSum > 0 ? v / negSum * 100.0 : 0);
                        float h = (float)(v / range) * plot_.height;
                        if (v >= 0) { bars_.push_back({ s, c, Rect(stX, yPos - h, stW, fabs(h)) }); yPos -= h; }
                        else { bars_.push_back({ s, c, Rect(stX, yNeg, stW, fabs(h)) }); yNeg += fabs(h); }
                    }
                }
                else if (stackMode_ == StackMode::Overlapped) {
                    for (int s = 0; s < S; ++s) { double v = ValueOf(s, c); float top = baseY_ - (float)(v / range) * plot_.height; bars_.push_back({ s, c, Rect(gx, min(top, baseY_), groupInner, fabs(top - baseY_)) }); }
                }
                else {
                    float step2 = groupInner / S, bw = step2 * (1.0f - barGap_);
                    for (int s = 0; s < S; ++s) { float bx = gx + s * step2 + (step2 - bw) * 0.5f; double v = ValueOf(s, c); float top = baseY_ - (float)(v / range) * plot_.height; bars_.push_back({ s, c, Rect(bx, min(top, baseY_), bw, fabs(top - baseY_)) }); }
                }
            }
            float p = animProg_;
            if (selectedC_ >= 0 && selectedC_ < C) { if (!selBrush_) rt->CreateSolidColorBrush(Color::FromArgb(28, 0, 120, 215).ToD2D(), selBrush_.GetAddressOf()); if (selBrush_) rt->FillRectangle(D2D1::RectF(plot_.x + selectedC_ * slotW_, plot_.y, plot_.x + (selectedC_ + 1) * slotW_, plot_.y + plot_.height), selBrush_.Get()); }
            for (auto& br : bars_) {
                float x = br.rect.x, w = br.rect.width, y0 = br.rect.y, y1 = br.rect.y + br.rect.height;
                float anchor = (fabs(y1 - baseY_) <= fabs(y0 - baseY_)) ? y1 : y0, farEnd = (anchor == y1) ? y0 : y1, fa = anchor + (farEnd - anchor) * p;
                D2D1_RECT_F r = D2D1::RectF(x, min(anchor, fa), x + w, max(anchor, fa));
                Color c = BarColor(br.series, br.category);
                if (stackMode_ == StackMode::Overlapped) c = Color(c.r, c.g, c.b, 0.65f);
                if (br.series == hoveredS_ && br.category == hoveredC_) c = Lighten(c, 0.15f);
                if (!barBrush_) rt->CreateSolidColorBrush(c.ToD2D(), barBrush_.GetAddressOf()); else barBrush_->SetColor(c.ToD2D());
                if (barBrush_) { if (cornerRadius_ > 0.0f) rt->FillRoundedRectangle(D2D1::RoundedRect(r, cornerRadius_, cornerRadius_), barBrush_.Get()); else rt->FillRectangle(r, barBrush_.Get()); }
                if (outlineW_ > 0.0f) { if (!outlineBrush_) rt->CreateSolidColorBrush(outlineColor_.ToD2D(), outlineBrush_.GetAddressOf()); else outlineBrush_->SetColor(outlineColor_.ToD2D()); if (outlineBrush_) rt->DrawRectangle(r, outlineBrush_.Get(), outlineW_); }
                if (showValues_ && p > 0.99f) {
                    std::wstring vs = FormatValue(ValueOf(br.series, br.category));
                    float yTop = min(anchor, fa), yBot = max(anchor, fa);
                    if (stackMode_ == StackMode::Grouped) DrawText(rt, vs, Rect(x, yTop - 16.0f, w, 16.0f), labelColor_, AText::Center, AVert::Center, valueFont_);
                    else if (yBot - yTop > valueFont_.size * 1.2f) DrawText(rt, vs, Rect(x, yTop, w, yBot - yTop), ContrastColor(c), AText::Center, AVert::Center, valueFont_);
                }
            }
        }
        void UpdateHover(float x, float y) override {
            int s = -1, c = -1; for (auto& br : bars_) if (br.rect.Contains(x, y)) { s = br.series; c = br.category; break; }
            if (s != hoveredS_ || c != hoveredC_) { hoveredS_ = s; hoveredC_ = c; RequestRepaint(); }
        }
        void OnDataClick(float x, float y) override {
            clickedS_ = clickedC_ = -1;
            for (auto& br : bars_) if (br.rect.Contains(x, y)) { selectedC_ = br.category; clickedS_ = br.series; clickedC_ = br.category; RequestRepaint(); break; }
        }
        std::wstring HoveredText() const override {
            if (hoveredC_ >= 0 && hoveredS_ >= 0 && hoveredC_ < (int)categories_.size() && hoveredS_ < (int)series_.size())
                return categories_[hoveredC_] + L"  " + series_[hoveredS_].name + L": " + FormatValue(ValueOf(hoveredS_, hoveredC_));
            return std::wstring();
        }
        void ReleaseDeviceResources() override { barBrush_.Reset(); outlineBrush_.Reset(); selBrush_.Reset(); ChartBase::ReleaseDeviceResources(); }

    private:
        Orientation orientation_ = Orientation::Vertical;
        StackMode stackMode_ = StackMode::Grouped;
        float barGap_ = 0.15f, groupGap_ = 0.2f, cornerRadius_ = 2.0f, stackBarRatio_ = 0.6f;
        float outlineW_ = 0; Color outlineColor_ = Color::FromArgb(255, 80, 80, 80);
        int selectedC_ = -1;
        float slotW_ = 0;
        std::map<unsigned long long, Color> barColorMap_;
        std::vector<Bar> bars_;
        ComPtr<ID2D1SolidColorBrush> barBrush_, outlineBrush_, selBrush_;
    };

    // =========================================================================
    // LineChart —— 折线图
    // =========================================================================
    class LineChart : public ChartBase {
    public:
        enum class Marker { None, Circle, Square };
        inline static float DefaultWidth = 420.0f, DefaultHeight = 260.0f;

        LineChart() { InitScrollBars(); }

        void SetLineWidth(float w) { lineWidth_ = max(0.5f, w); RequestRepaint(); }
        void SetSmooth(bool on) { smooth_ = on; RequestRepaint(); }
        void SetDashed(bool on) { dashed_ = on; RequestRepaint(); }
        void SetShowMarkers(bool on) { showMarkers_ = on; RequestRepaint(); }
        void SetMarkerSize(float s) { markerSize_ = max(1.0f, s); RequestRepaint(); }
        void SetMarker(Marker m) { markerShape_ = m; RequestRepaint(); }
        void SetAreaFill(bool on, float alpha = 0.25f) { areaFill_ = on; areaAlpha_ = clamp(alpha, 0.0f, 1.0f); RequestRepaint(); }   // 面积填充（半透明，到 0 基线）

    protected:
        void DrawData(ID2D1RenderTarget* rt) override {
            int C = (int)categories_.size(), S = (int)series_.size();
            if (C <= 0 || S <= 0) return;
            float reveal = animProg_ * max(1, C);
            int n = (int)std::ceil(reveal); if (n < 1) n = 1; if (n > C) n = C;
            ID2D1Factory* fac = nullptr; rt->GetFactory(&fac);
            if (fac && !dashedStyle_) { float d[] = { 6.0f, 5.0f }; fac->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_CUSTOM, 0.0f), d, 2, dashedStyle_.GetAddressOf()); }
            for (int s = 0; s < S; ++s) {
                Color col = series_[s].color;
                if (!lineBrush_) rt->CreateSolidColorBrush(col.ToD2D(), lineBrush_.GetAddressOf()); else lineBrush_->SetColor(col.ToD2D());
                if (!lineBrush_) continue;
                std::vector<D2D1_POINT_2F> pts; pts.reserve(C);
                for (int c = 0; c < C; ++c) pts.push_back(D2D1::Point2F(CatX(c), ValueY(ValueOf(s, c))));
                // 面积填充（到 0 基线，半透明；在折线之下）
                if (areaFill_ && fac && n >= 2) {
                    ComPtr<ID2D1PathGeometry> ag; fac->CreatePathGeometry(ag.GetAddressOf());
                    if (ag) {
                        ComPtr<ID2D1GeometrySink> as; ag->Open(as.GetAddressOf());
                        if (as) {
                            as->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
                            if (smooth_) for (int i = 0; i < n - 1; ++i) { D2D1_POINT_2F p0 = pts[max(0, i - 1)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[min(n - 1, i + 2)]; as->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6), D2D1::Point2F(p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6), p2)); }
                            else for (int i = 1; i < n; ++i) as->AddLine(pts[i]);
                            double base = 0.0; if (base < axisMin_) base = axisMin_; if (base > axisMax_) base = axisMax_;   // 基线夹在可见范围内（自动量程时不至于填满整块）
                            float bY = ValueY(base);
                            as->AddLine(D2D1::Point2F(pts[n - 1].x, bY)); as->AddLine(D2D1::Point2F(pts[0].x, bY));
                            as->EndFigure(D2D1_FIGURE_END_CLOSED); as->Close();
                            if (!areaBrush_) rt->CreateSolidColorBrush(Color(col.r, col.g, col.b, areaAlpha_).ToD2D(), areaBrush_.GetAddressOf()); else areaBrush_->SetColor(Color(col.r, col.g, col.b, areaAlpha_).ToD2D());
                            if (areaBrush_) rt->FillGeometry(ag.Get(), areaBrush_.Get());
                        }
                    }
                }
                if (fac && n >= 2) {
                    ComPtr<ID2D1PathGeometry> geo; fac->CreatePathGeometry(geo.GetAddressOf());
                    if (geo) {
                        ComPtr<ID2D1GeometrySink> sink; geo->Open(sink.GetAddressOf());
                        if (sink) {
                            sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_HOLLOW);
                            if (smooth_) {
                                for (int i = 0; i < n - 1; ++i) {
                                    D2D1_POINT_2F p0 = pts[max(0, i - 1)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[min(n - 1, i + 2)];
                                    D2D1_POINT_2F c1 = D2D1::Point2F(p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6);
                                    D2D1_POINT_2F c2 = D2D1::Point2F(p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6);
                                    sink->AddBezier(D2D1::BezierSegment(c1, c2, p2));
                                }
                            }
                            else for (int i = 1; i < n; ++i) sink->AddLine(pts[i]);
                            sink->EndFigure(D2D1_FIGURE_END_OPEN); sink->Close();
                            rt->DrawGeometry(geo.Get(), lineBrush_.Get(), lineWidth_, dashed_ ? dashedStyle_.Get() : nullptr);
                        }
                    }
                }
                if (showMarkers_ && markerShape_ != Marker::None) {
                    for (int c = 0; c < n; ++c) {
                        bool hov = (s == hoveredS_ && c == hoveredC_);
                        float ms = markerSize_ * (hov ? 1.7f : 1.0f);
                        if (markerShape_ == Marker::Circle) rt->FillEllipse(D2D1::Ellipse(pts[c], ms, ms), lineBrush_.Get());
                        else rt->FillRectangle(D2D1::RectF(pts[c].x - ms, pts[c].y - ms, pts[c].x + ms, pts[c].y + ms), lineBrush_.Get());
                    }
                }
                if (showValues_ && n >= C) {   // 数值标签（仅揭示完成）
                    for (int c = 0; c < C; ++c) DrawText(rt, FormatValue(ValueOf(s, c)), Rect(pts[c].x - 20.0f, pts[c].y - 16.0f, 40.0f, 14.0f), col, AText::Center, AVert::Center, valueFont_);
                }
            }
        }
        void UpdateHover(float x, float y) override {
            int bs = -1, bc = -1; float best = 12.0f * 12.0f;
            for (int s = 0; s < (int)series_.size(); ++s) for (int c = 0; c < (int)categories_.size(); ++c) {
                float px = CatX(c), py = ValueY(ValueOf(s, c)); float d = (x - px) * (x - px) + (y - py) * (y - py);
                if (d < best) { best = d; bs = s; bc = c; }
            }
            if (bs != hoveredS_ || bc != hoveredC_) { hoveredS_ = bs; hoveredC_ = bc; RequestRepaint(); }
        }
        void OnDataClick(float x, float y) override {
            clickedS_ = clickedC_ = -1; int bs = -1, bc = -1; float best = 12.0f * 12.0f;
            for (int s = 0; s < (int)series_.size(); ++s) for (int c = 0; c < (int)categories_.size(); ++c) {
                float px = CatX(c), py = ValueY(ValueOf(s, c)); float d = (x - px) * (x - px) + (y - py) * (y - py);
                if (d < best) { best = d; bs = s; bc = c; }
            }
            if (bs >= 0) { clickedS_ = bs; clickedC_ = bc; RequestRepaint(); }
        }
        std::wstring HoveredText() const override {
            if (hoveredC_ >= 0 && hoveredS_ >= 0 && hoveredC_ < (int)categories_.size() && hoveredS_ < (int)series_.size())
                return categories_[hoveredC_] + L"  " + series_[hoveredS_].name + L": " + FormatValue(ValueOf(hoveredS_, hoveredC_));
            return std::wstring();
        }
        void ReleaseDeviceResources() override { lineBrush_.Reset(); areaBrush_.Reset(); ChartBase::ReleaseDeviceResources(); }

    private:
        float lineWidth_ = 2.0f, markerSize_ = 3.5f, areaAlpha_ = 0.25f;
        bool smooth_ = false, dashed_ = false, showMarkers_ = true, areaFill_ = false;
        Marker markerShape_ = Marker::Circle;
        ComPtr<ID2D1SolidColorBrush> lineBrush_, areaBrush_;
        ComPtr<ID2D1StrokeStyle> dashedStyle_;
    };

    // =========================================================================
    // PieChart —— 饼图 / 环形图
    // =========================================================================
    class PieChart : public UIElement {
    public:
        enum class LabelMode { None, Percent, Value, LabelAndPercent };
        struct Slice { std::wstring label; double value; Color color; };
        inline static std::vector<Color> DefaultPalette = ChartBase::DefaultPalette;

        AccessibleRole DefaultAccessibleRole() const override { return AccessibleRole::Group; }
        ZSignal<int> SliceClicked;   // (slice index)

        PieChart() { width_ = 320.0f; height_ = 260.0f; labelFont_.size = 12.0f; valueFont_.size = 11.0f; }

        void AddSlice(const std::wstring& label, double value, Color color = Color::FromArgb(0, 0, 0, 0)) {
            if (color.a <= 0.0f) color = palette_[slices_.size() % palette_.size()];
            slices_.push_back({ label, max(0.0, value), color });
            sliceHoverProg_.resize(slices_.size(), 0.0f);
            OnChanged();
        }
        void Clear() { slices_.clear(); sliceHoverProg_.clear(); OnChanged(); }
        int SliceCount() const { return (int)slices_.size(); }
        void SetDonut(float ratio) { donut_ = clamp(ratio, 0.0f, 0.9f); RequestRepaint(); }     // 0=饼图，0.5=环形
        void SetStartAngle(float deg) { startAngle_ = deg; RequestRepaint(); }
        void SetClockwise(bool on) { clockwise_ = on; RequestRepaint(); }
        void SetSliceGap(float deg) { gapDeg_ = clamp(deg, 0.0f, 5.0f); RequestRepaint(); }
        void SetLabelMode(LabelMode m) { labelMode_ = m; InvalidateLayout(); RequestRepaint(); }
        void SetShowLegend(bool on) { showLegend_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetZoom(float z) { zoom_ = clamp(z, 0.4f, 4.0f); RequestRepaint(); }
        float GetZoom() const { return zoom_; }
        void SetAnimationEnabled(bool on) { animate_ = on; if (!on) animProg_ = 1.0f; RequestRepaint(); }
        void SetPalette(const std::vector<Color>& p) { if (!p.empty()) { palette_ = p; for (size_t i = 0; i < slices_.size(); ++i) slices_[i].color = palette_[i % palette_.size()]; RequestRepaint(); } }
        void SetLabelFont(const FontSpec& f) { labelFont_ = f; valueFont_ = f; InvalidateLayout(); RequestRepaint(); }

        float GetDefaultHorizontalStretchWeight() const override { return 0.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 0.0f; }
        Size MeasureOverride(const Size& avail) override {
            float w = (GetFillWidth()  && avail.width  != FLT_MAX && avail.width  > 0.0f) ? avail.width  : width_;
            float h = (GetFillHeight() && avail.height != FLT_MAX && avail.height > 0.0f) ? avail.height : height_;
            return Size(w, h);
        }
        void ArrangeOverride(const Rect& finalRect) override { UIElement::ArrangeOverride(finalRect); }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            rt->PushAxisAlignedClip(arrangedRect_.ToD2D(), D2D1_ANTIALIAS_MODE_ALIASED);
            const Rect& r = arrangedRect_;
            double total = 0; for (auto& s : slices_) total += s.value;
            float legendH = (showLegend_ && !slices_.empty()) ? (labelFont_.size * 1.9f + 6.0f) : 0.0f;
            float cx = r.x + r.width * 0.5f, cy = r.y + (r.height - legendH) * 0.5f;
            float radius = (max(10.0f, min(r.width, r.height - legendH) * 0.5f - 24.0f) * zoom_);
            float ir = radius * donut_;
            if (total <= 0 || slices_.empty()) { rt->PopAxisAlignedClip(); return; }
            ID2D1Factory* fac = nullptr; rt->GetFactory(&fac);
            if (!fac) { rt->PopAxisAlignedClip(); return; }

            float a0 = (startAngle_ - 90.0f) * 3.14159265f / 180.0f;
            int dir = clockwise_ ? 1 : -1;
            float revealEnd = a0 + (float)(dir * animProg_ * 2 * 3.14159265);   // 整圆 0→360 扫入
            for (int i = 0; i < (int)slices_.size(); ++i) {
                double frac = slices_[i].value / total;
                float s0 = a0, s1 = a0 + (float)(dir * frac * 2 * 3.14159265);
                bool plus = true;
                float ca1 = s1;
                if (dir > 0) { if (s0 >= revealEnd - 1e-5f) break; ca1 = min(s1, revealEnd); plus = (ca1 >= s1 - 1e-4f); }
                else { if (s0 <= revealEnd + 1e-5f) break; ca1 = max(s1, revealEnd); plus = (ca1 <= s1 + 1e-4f); }
                float gap = gapDeg_ * 3.14159265f / 180.0f;
                float ca0 = s0 + (dir > 0 ? gap * 0.5f : -gap * 0.5f), cae = ca1 - (dir > 0 ? gap * 0.5f : -gap * 0.5f);
                if (fabs(cae - ca0) > 1e-4f) {
                    ComPtr<ID2D1PathGeometry> geo; fac->CreatePathGeometry(geo.GetAddressOf());
                    if (geo) {
                        ComPtr<ID2D1GeometrySink> sink; geo->Open(sink.GetAddressOf());
                        if (sink) {
                            bool hov = (i == hovered_);
                            float hp = (i < (int)sliceHoverProg_.size()) ? sliceHoverProg_[i] : 0.0f;   // 悬停弹出动画
                            float off = 6.0f * hp;
                            float midA = (ca0 + cae) * 0.5f; float ox = off * cosf(midA), oy = off * sinf(midA);
                            D2D1_POINT_2F po0 = D2D1::Point2F(cx + ox + radius * cosf(ca0), cy + oy + radius * sinf(ca0));
                            D2D1_POINT_2F po1 = D2D1::Point2F(cx + ox + radius * cosf(cae), cy + oy + radius * sinf(cae));
                            D2D1_POINT_2F pi1 = D2D1::Point2F(cx + ox + ir * cosf(cae), cy + oy + ir * sinf(cae));
                            D2D1_POINT_2F pi0 = D2D1::Point2F(cx + ox + ir * cosf(ca0), cy + oy + ir * sinf(ca0));
                            sink->BeginFigure(po0, D2D1_FIGURE_BEGIN_FILLED);
                            sink->AddArc(D2D1::ArcSegment(po1, D2D1::SizeF(radius, radius), 0.0f, (dir > 0 ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE), (fabs(cae - ca0) > 3.14159265f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL)));
                            if (ir > 0.5f) { sink->AddLine(pi1); sink->AddArc(D2D1::ArcSegment(pi0, D2D1::SizeF(ir, ir), 0.0f, (dir > 0 ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE), (fabs(cae - ca0) > 3.14159265f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL))); }
                            else sink->AddLine(D2D1::Point2F(cx + ox, cy + oy));
                            sink->EndFigure(D2D1_FIGURE_END_CLOSED); sink->Close();
                            Color c = slices_[i].color; if (hov) c = Color(c.r + (1 - c.r) * 0.15f, c.g + (1 - c.g) * 0.15f, c.b + (1 - c.b) * 0.15f, c.a);
                            if (!sliceBrush_) rt->CreateSolidColorBrush(c.ToD2D(), sliceBrush_.GetAddressOf()); else sliceBrush_->SetColor(c.ToD2D());
                            if (sliceBrush_) rt->FillGeometry(geo.Get(), sliceBrush_.Get());
                            // 标签
                            if (labelMode_ != LabelMode::None && plus) {
                                bool leftSide = (cosf(midA) < 0);
                                float ex = cx + ox + radius * cosf(midA), ey = cy + oy + radius * sinf(midA);          // 扇区边缘
                                float lx = cx + ox + (radius + 16.0f) * cosf(midA), ly = cy + oy + (radius + 16.0f) * sinf(midA);  // 引线末端
                                std::wstring txt = fmtLabel(i) + L" " + fmtPct(i, total);
                                if (labelMode_ == LabelMode::Percent) txt = fmtPct(i, total);
                                else if (labelMode_ == LabelMode::Value) txt = fmtVal(i);
                                float tw = MeasureTextW(txt, FontManager::Instance().GetFormat(valueFont_));
                                rt->DrawLine(D2D1::Point2F(ex, ey), D2D1::Point2F(lx, ly), sliceBrush_.Get(), 1.0f);
                                Rect tr = leftSide ? Rect(lx - 4.0f - tw, ly - 8.0f, tw, 16.0f) : Rect(lx + 4.0f, ly - 8.0f, tw, 16.0f);
                                DrawText(rt, txt, tr, labelColor_, (leftSide ? 1 : 0), valueFont_);   // 左半边右对齐（贴在线左端），右半边左对齐
                            }
                        }
                    }
                }
                a0 = s1;
            }
            if (showLegend_) DrawLegend(rt);
            rt->PopAxisAlignedClip();
        }

        void OnMouseMove(float x, float y) override {
            int i = HitSlice(x, y);
            if (i != hovered_) { hovered_ = i; RequestRepaint(); }
        }
        void OnMouseLeave() override { if (hovered_ != -1) { hovered_ = -1; RequestRepaint(); } }
        void OnMouseDown(float x, float y) override { int i = HitSlice(x, y); if (i >= 0) SliceClicked(i); }
        bool OnMouseWheel(float, float dy) override { zoom_ = clamp(zoom_ * (1.0f + dy * 0.12f), 0.4f, 4.0f); RequestRepaint(); return true; }
        std::wstring GetToolTip() const override {
            std::wstring custom = UIElement::GetToolTip(); if (!custom.empty()) return custom;
            if (hovered_ >= 0 && hovered_ < (int)slices_.size()) return slices_[hovered_].label + L": " + fmtVal(hovered_);
            return std::wstring();
        }
        void UpdateAnimation(float dt) override {
            bool any = false;
            if (animProg_ < 1.0f) { animProg_ = min(1.0f, animProg_ + dt / 0.7f); any = true; }   // 整圆扫入
            for (int i = 0; i < (int)sliceHoverProg_.size(); ++i) {
                float t = (i == hovered_) ? 1.0f : 0.0f; float f = sliceHoverProg_[i];
                if (f != t) { f += (t - f) * (1.0f - expf(-dt * 16.0f)); if (fabs(f - t) < 0.01f) f = t; sliceHoverProg_[i] = f; any = true; }
            }
            if (any) RequestRepaint();
        }
        bool HasActiveAnimation() const override {
            if (animProg_ < 1.0f) return true;
            for (float f : sliceHoverProg_) if (f > 0.001f && f < 0.999f) return true;
            return false;
        }
        void ReleaseDeviceResources() override { sliceBrush_.Reset(); textBrush_.Reset(); legendBrush_.Reset(); UIElement::ReleaseDeviceResources(); }

    private:
        void OnChanged() { animProg_ = animate_ ? 0.0f : 1.0f; InvalidateLayout(); RequestRepaint(); }
        std::wstring fmtLabel(int i) const { return slices_[i].label; }
        std::wstring fmtVal(int i) const { wchar_t b[64]; swprintf(b, 64, L"%.0f", slices_[i].value); return b; }
        std::wstring fmtPct(int i, double total) const { wchar_t b[32]; swprintf(b, 32, L"%.0f%%", total > 0 ? slices_[i].value / total * 100.0 : 0); return b; }
        float MeasureTextW(const std::wstring& s, IDWriteTextFormat* f) { if (!f || s.empty()) return 0; IDWriteFactory* dw = FontManager::Instance().GetFactory(); if (!dw) return 0; ComPtr<IDWriteTextLayout> l; if (FAILED(dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, 1e5f, 1e5f, &l)) || !l) return 0; DWRITE_TEXT_METRICS m{}; l->GetMetrics(&m); return m.width; }
        void DrawText(ID2D1RenderTarget* rt, const std::wstring& s, const Rect& rect, Color col, int align, const FontSpec& fs) {   // align: 0=左 1=右
            if (s.empty()) return; IDWriteTextFormat* fmt_ = FontManager::Instance().GetFormat(fs); if (!fmt_) return; IDWriteFactory* dw = FontManager::Instance().GetFactory(); if (!dw) return;
            ComPtr<IDWriteTextLayout> l; if (FAILED(dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt_, max(1.0f, rect.width), max(1.0f, rect.height), &l)) || !l) return;
            l->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); l->SetTextAlignment(align == 0 ? DWRITE_TEXT_ALIGNMENT_LEADING : DWRITE_TEXT_ALIGNMENT_TRAILING);
            if (!textBrush_) rt->CreateSolidColorBrush(col.ToD2D(), textBrush_.GetAddressOf()); else textBrush_->SetColor(col.ToD2D());
            if (textBrush_) rt->DrawTextLayout(D2D1::Point2F(rect.x, rect.y), l.Get(), textBrush_.Get());
        }
        void DrawLegend(ID2D1RenderTarget* rt) {
            const Rect& r = arrangedRect_; float rowH = labelFont_.size * 1.9f; float y = r.y + r.height - rowH - 4.0f; float x = r.x + 8.0f, sw = labelFont_.size;
            IDWriteTextFormat* lf = FontManager::Instance().GetFormat(labelFont_);
            for (auto& s : slices_) {
                float tw = MeasureTextW(s.label, lf);
                if (x + sw + 4 + tw > r.x + r.width - 4.0f) { x = r.x + 8.0f; y -= rowH; }
                if (!legendBrush_) rt->CreateSolidColorBrush(s.color.ToD2D(), legendBrush_.GetAddressOf()); else legendBrush_->SetColor(s.color.ToD2D());
                if (legendBrush_) rt->FillRectangle(D2D1::RectF(x, y + 2.0f, x + sw, y + rowH - 3.0f), legendBrush_.Get());
                DrawText(rt, s.label, Rect(x + sw + 4, y, max(10.0f, tw + 6), rowH), labelColor_, 0, labelFont_);
                x += sw + 4 + tw + 14;
            }
        }
        int HitSlice(float x, float y) {
            const Rect& r = arrangedRect_; double total = 0; for (auto& s : slices_) total += s.value;
            if (total <= 0 || slices_.empty()) return -1;
            float legendH = (showLegend_ && !slices_.empty()) ? (labelFont_.size * 1.9f + 6.0f) : 0.0f;
            float cx = r.x + r.width * 0.5f, cy = r.y + (r.height - legendH) * 0.5f;
            float radius = (max(10.0f, min(r.width, r.height - legendH) * 0.5f - 24.0f) * zoom_), ir = radius * donut_;
            float dx = x - cx, dy = y - cy, dist = sqrtf(dx * dx + dy * dy);
            if (dist < ir - 2 || dist > radius + 8) return -1;
            float a = atan2f(dy, dx);   // -pi..pi, 0=east
            int dir = clockwise_ ? 1 : -1;
            float a0 = (startAngle_ - 90.0f) * 3.14159265f / 180.0f;
            for (int i = 0; i < (int)slices_.size(); ++i) {
                float sweep = (float)(dir * (slices_[i].value / total) * 2 * 3.14159265);
                float s0 = a0, s1 = a0 + sweep;
                float lo = min(s0, s1), hi = max(s0, s1);
                float aa = a; while (aa < lo - 3.14159265f) aa += 2 * 3.14159265f; while (aa > hi + 3.14159265f) aa -= 2 * 3.14159265f;
                if (aa >= lo && aa <= hi) return i;
                a0 = s1;
            }
            return -1;
        }

        std::vector<Slice> slices_;
        std::vector<Color> palette_ = DefaultPalette;
        float donut_ = 0.0f, startAngle_ = 0.0f, gapDeg_ = 0.0f;
        bool clockwise_ = true, showLegend_ = true;
        LabelMode labelMode_ = LabelMode::LabelAndPercent;
        FontSpec labelFont_, valueFont_;
        Color labelColor_ = Color::FromArgb(255, 90, 90, 90);
        float animProg_ = 0.0f; bool animate_ = true; float zoom_ = 1.0f;
        int hovered_ = -1;
        std::vector<float> sliceHoverProg_;
        ComPtr<ID2D1SolidColorBrush> sliceBrush_, textBrush_, legendBrush_;
    };

} // namespace ZufyUI
