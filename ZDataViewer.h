#pragma once
#include "ZufyUI.h"
#include "ZufyUIWidgets.h"

namespace ZufyUI {
    // ==================== 列表视图 ListView ====================
    class ListView : public UIElement {
    public:
        // 默认样式
        inline static float DefaultItemHeight = 28.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 200.0f;
        inline static float DefaultHeight = 200.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        // 信号
        ZSignal<int> SelectionChanged;   // 选中项变化
        ZSignal<int> ItemClicked;        // 项目被点击
        ZSignal<int> ItemDoubleClicked;  // 项目双击
        ZSignal<std::vector<int>> SelectionChangedMulti;  // 多选集合变化
        ZSignal<int, bool> ItemCheckStateChanged;         // 项勾选变化
        ZSignal<int> ItemRightClicked;                    // 右键点击项（传行号；可配合 SetContextMenuFactory + GetContextRow）
        int GetContextRow() const { return lastContextRow_; }   // 最近一次右键所在行（-1=无）

        // 选择模式
        enum class SelectionMode { Single, Extended, Multi, None };

        ListView()
            : selectedIndex_(-1), hoveredIndex_(-1),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showScrollBar_(false), isDraggingScroll_(false),
            dragStartMouseY_(0.0f), dragStartScrollY_(0.0f),
            itemHeight_(DefaultItemHeight),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            textColor_(DefaultTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            scrollHoverProgress_(0.0f),
            isHovered_(false),
            isScrollBarHovered_(false),
            buttonMode_(false),
            buttonSpacing_(4.0f) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
            // 使用字体管理器，不再创建本地 textFormat_
        }

        // 数据操作
        // Label 化：项 Label 的统一初始化（套用 ListView 字体/对齐/内边距，关闭自身缓存）
        void PrepareItemLabel(std::shared_ptr<Label>& lb) {
            if (!lb) return;
            lb->SetUseCache(false);
            lb->SetFont(GetEffectiveFontSpec());
            lb->SetAlignment(Label::HAlign::Left, Label::VAlign::Center);
            lb->SetPadding(0.0f);
        }
        void AddItem(std::shared_ptr<Label> label) {
            if (!label) return;
            PrepareItemLabel(label);
            items_.push_back(label);
            label->SetParent(this);
            if (batchUpdate_ == 0) { UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        }
        void AddItem(const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            AddItem(label);
        }
        void InsertItem(int index, std::shared_ptr<Label> label) {
            if (index < 0 || index >(int)items_.size() || !label) return;
            PrepareItemLabel(label);
            items_.insert(items_.begin() + index, label);
            label->SetParent(this);
            if (selectedIndex_ >= index) selectedIndex_++;
            ShiftMultiSelForInsert(index, 1);
            if (batchUpdate_ == 0) { UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        }
        void InsertItem(int index, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            InsertItem(index, label);
        }
        // P1：批量增删。BeginUpdate 期间挂起刷新，EndUpdate 统一刷新一次（批量添加 N 项只刷新一次）
        void BeginUpdate() { ++batchUpdate_; }
        void EndUpdate() {
            if (batchUpdate_ > 0 && --batchUpdate_ == 0) { UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        }
        int batchUpdate_ = 0;
        void RemoveItem(int index) {
            if (index < 0 || index >= (int)items_.size()) return;
            {
                Label* p = items_[index].get();
                disabledItems_.erase(p);
                itemTextColors_.erase(p);
                itemTips_.erase(p);
            }
            items_.erase(items_.begin() + index);
            if (selectedIndex_ == index) selectedIndex_ = -1;
            else if (selectedIndex_ > index) selectedIndex_--;
            {
                std::unordered_set<int> ns;
                for (int s : multiSel_) { if (s == index) continue; ns.insert(s > index ? s - 1 : s); }
                multiSel_ = std::move(ns);
            }
            {
                std::unordered_set<int> nc;
                for (int s : checked_) { if (s == index) continue; nc.insert(s > index ? s - 1 : s); }
                checked_ = std::move(nc);
            }
            if (batchUpdate_ == 0) { UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        }
        void Clear() {
            items_.clear();
            multiSel_.clear();
            checked_.clear();
            disabledItems_.clear();
            itemTextColors_.clear();
            itemTips_.clear();
            selectedIndex_ = -1;
            hoveredIndex_ = -1;
            scrollOffsetY_ = 0.0f;
            targetScrollOffsetY_ = 0.0f;
            maxScrollY_ = 0.0f;
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int index, std::shared_ptr<Label> label) {
            if (index < 0 || index >= (int)items_.size() || !label) return;
            Label* oldP = items_[index].get();
            Label* newP = label.get();
            if (oldP && oldP != newP) {
                if (disabledItems_.count(oldP)) { disabledItems_.erase(oldP); disabledItems_.insert(newP); }
                auto itc = itemTextColors_.find(oldP);
                if (itc != itemTextColors_.end()) { itemTextColors_[newP] = itc->second; itemTextColors_.erase(itc); }
                auto itt = itemTips_.find(oldP);
                if (itt != itemTips_.end()) { itemTips_[newP] = itt->second; itemTips_.erase(itt); }
            }
            items_[index] = label;
            PrepareItemLabel(label);
            label->SetParent(this);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int index, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            SetItem(index, label);
        }
        std::shared_ptr<Label> GetItemLabel(int index) const {
            if (index < 0 || index >= (int)items_.size()) return nullptr;
            return items_[index];
        }
        std::wstring GetItemText(int index) const {
            auto label = GetItemLabel(index);
            return label ? label->GetText() : L"";
        }
        int GetItemCount() const { return (int)items_.size(); }

        // ---------- 扩展：插入 / 移动 / 排序 ----------
        void InsertItems(int index, const std::vector<std::wstring>& texts) {
            if (index < 0 || index > (int)items_.size()) index = (int)items_.size();
            for (int k = 0; k < (int)texts.size(); ++k) {
                auto label = std::make_shared<Label>(texts[k]);
                label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
                PrepareItemLabel(label);
                items_.insert(items_.begin() + index + k, label);
                label->SetParent(this);
            }
            ShiftMultiSelForInsert(index, (int)texts.size());
            UpdateScrollInfo(); InvalidateLayout(); RequestRepaint();
        }
        bool MoveItem(int from, int to) {
            if (from < 0 || from >= (int)items_.size()) return false;
            if (to < 0 || to >= (int)items_.size()) return false;
            auto item = items_[from];
            items_.erase(items_.begin() + from);
            items_.insert(items_.begin() + to, item);
            SetSelectedIndex(to);   // 选中跟随被移动项，并触发 SelectionChanged / EnsureVisible
            UpdateScrollInfo(); InvalidateLayout(); RequestRepaint();
            return true;
        }
        void SwapItems(int a, int b) {
            if (a < 0 || a >= (int)items_.size() || b < 0 || b >= (int)items_.size()) return;
            std::swap(items_[a], items_[b]);
            RequestRepaint();
        }
        // cmp 返回 true 表示 a 应排在 b 之前
        void SortItems(std::function<bool(const std::wstring&, const std::wstring&)> cmp) {
            std::stable_sort(items_.begin(), items_.end(), [&](const std::shared_ptr<Label>& a, const std::shared_ptr<Label>& b) {
                return cmp(a ? a->GetText() : L"", b ? b->GetText() : L"");
                });
            multiSel_.clear(); selectedIndex_ = -1;
            UpdateScrollInfo(); InvalidateLayout(); RequestRepaint();
        }
        void ScrollToItem(int index) { EnsureVisible(index); RequestRepaint(); }

        // ---------- 扩展：多选 ----------
        void SetSelectionMode(SelectionMode mode) { selectionMode_ = mode; ClearMultiSelection(); RequestRepaint(); }
        SelectionMode GetSelectionMode() const { return selectionMode_; }
        bool IsIndexSelected(int index) const {
            return index == selectedIndex_ || multiSel_.count(index) > 0;
        }
        std::vector<int> GetSelectedIndices() const {
            if (selectionMode_ == SelectionMode::Single) return selectedIndex_ >= 0 ? std::vector<int>{ selectedIndex_ } : std::vector<int>{};
            std::vector<int> v(multiSel_.begin(), multiSel_.end());
            std::sort(v.begin(), v.end());
            return v;
        }
        void ClearMultiSelection() { multiSel_.clear(); RequestRepaint(); }
        void SelectAll() {
            if (selectionMode_ == SelectionMode::Single) return;
            for (int i = 0; i < (int)items_.size(); ++i) multiSel_.insert(i);
            RequestRepaint();
        }
        void ToggleIndexSelection(int index) {
            if (index < 0 || index >= (int)items_.size()) return;
            if (multiSel_.count(index)) multiSel_.erase(index);
            else multiSel_.insert(index);
            selectedIndex_ = index;
            RequestRepaint();
        }

        // ---------- 扩展：显示选项 ----------
        void SetEmptyText(const std::wstring& text) { emptyText_ = text; RequestRepaint(); }
        std::wstring GetEmptyText() const { return emptyText_; }
        void SetAlternatingRowColors(bool enable) { alternatingRowColors_ = enable; RequestRepaint(); }
        bool GetAlternatingRowColors() const { return alternatingRowColors_; }
        void SetAlternatingRowColor(Color color) { alternateRowColor_ = color.ToD2D(); alternateBrush_.Reset(); RequestRepaint(); }

        // 框选开关 / 框选与勾选同步
        void SetMarqueeEnabled(bool e) { marqueeEnabled_ = e; if (!e) { marqueeActive_ = false; pressActive_ = false; } RequestRepaint(); }
        bool IsMarqueeEnabled() const { return marqueeEnabled_; }
        void SetMarqueeCheckSync(bool e) { marqueeCheckSync_ = e; }
        bool IsMarqueeCheckSync() const { return marqueeCheckSync_; }
        std::vector<std::wstring> GetSelectedTexts() const {
            std::vector<std::wstring> out;
            for (int i : GetSelectedIndices()) out.push_back(GetItemText(i));
            return out;
        }
        // 每项状态数组（长度 = 项目数）：框选/选中 与 勾选 分开
        std::vector<bool> GetSelectionStates() const {
            std::vector<bool> v(items_.size(), false);
            for (int i = 0; i < (int)items_.size(); ++i) v[i] = IsIndexSelected(i);
            return v;
        }
        std::vector<bool> GetCheckStates() const {
            std::vector<bool> v(items_.size(), false);
            for (int i = 0; i < (int)items_.size(); ++i) v[i] = checked_.count(i) > 0;
            return v;
        }
        std::vector<int> GetCheckedIndices() const {
            std::vector<int> v(checked_.begin(), checked_.end());
            std::sort(v.begin(), v.end());
            return v;
        }
        // 勾选
        void SetCheckable(bool enable) { itemsCheckable_ = enable; if (!enable) checked_.clear(); RequestRepaint(); }
        bool IsCheckable() const { return itemsCheckable_; }
        void SetItemChecked(int index, bool checked) {
            if (index < 0 || index >= (int)items_.size()) return;
            bool cur = checked_.count(index) > 0;
            if (cur == checked) return;
            if (checked) checked_.insert(index); else checked_.erase(index);
            ItemCheckStateChanged(index, checked);
            RequestRepaint();
        }
        bool IsItemChecked(int index) const { return checked_.count(index) > 0; }
        void SetCheckBoxColor(Color c) { checkBoxColor_ = c.ToD2D(); RequestRepaint(); }
        void SetCheckMarkColor(Color c) { checkMarkColor_ = c.ToD2D(); RequestRepaint(); }

        // 选择
        void SetSelectedIndex(int index) {
            if (index < -1 || index >= (int)items_.size()) return;
            multiSel_.clear();
            if (selectedIndex_ != index) {
                selectedIndex_ = index;
                SelectionChanged(selectedIndex_);
                EnsureVisible(selectedIndex_);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        int GetSelectedIndex() const { return selectedIndex_; }
        std::wstring GetSelectedText() const { return GetItemText(selectedIndex_); }

        // 按钮模式设置
        void SetButtonMode(bool enable) { buttonMode_ = enable; UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        bool IsButtonMode() const { return buttonMode_; }
        void SetButtonSpacing(float spacing) { buttonSpacing_ = max(0.0f, spacing); UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }

        // ---------- 单项禁用 / 文字颜色 / ToolTip / 排序 ----------
        void SetItemDisabled(int index, bool disabled = true) {
            if (index < 0 || index >= (int)items_.size()) return;
            Label* p = items_[index].get();
            if (disabled) disabledItems_.insert(p); else disabledItems_.erase(p);
            if (items_[index]) items_[index]->SetEnabled(!disabled);   // Label 化：禁用态由 Label 画
            RequestRepaint();
        }
        bool IsItemDisabled(int index) const { return index >= 0 && index < (int)items_.size() && disabledItems_.count(items_[index].get()) > 0; }
        int FindEnabledFrom(int from, int step) const {
            int i = from + step;
            while (i >= 0 && i < (int)items_.size() && IsItemDisabled(i)) i += step;
            return (i >= 0 && i < (int)items_.size()) ? i : -1;
        }

        void SetItemTextColor(int index, Color color) {
            if (index < 0 || index >= (int)items_.size()) return;
            itemTextColors_[items_[index].get()] = color.ToD2D();
            if (items_[index]) items_[index]->SetTextColor(color);   // Label 化：颜色由 Label 画
            RequestRepaint();
        }
        void ClearItemTextColor(int index) {
            if (index >= 0 && index < (int)items_.size()) {
                itemTextColors_.erase(items_[index].get());
                if (items_[index]) items_[index]->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            }
            RequestRepaint();
        }

        void SetItemToolTip(int index, const std::wstring& tip) {
            if (index < 0 || index >= (int)items_.size()) return;
            Label* p = items_[index].get();
            if (tip.empty()) itemTips_.erase(p); else itemTips_[p] = tip;
        }
        std::wstring GetItemToolTip(int index) const {
            if (index < 0 || index >= (int)items_.size()) return L"";
            auto it = itemTips_.find(items_[index].get());
            return it == itemTips_.end() ? std::wstring() : it->second;
        }

        void SetSortComparator(std::function<bool(const std::wstring&, const std::wstring&)> cmp) { sortComparator_ = std::move(cmp); }
        void Sort(bool ascending = true) {
            sortAscending_ = ascending;
            std::shared_ptr<Label> selLabel = (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) ? items_[selectedIndex_] : nullptr;
            auto cmp = sortComparator_;
            std::stable_sort(items_.begin(), items_.end(), [&](const std::shared_ptr<Label>& a, const std::shared_ptr<Label>& b) {
                std::wstring ta = a ? a->GetText() : L"";
                std::wstring tb = b ? b->GetText() : L"";
                if (ascending) return cmp ? cmp(ta, tb) : (ta < tb);
                return cmp ? cmp(tb, ta) : (ta > tb);
                });
            selectedIndex_ = -1;
            if (selLabel) for (int i = 0; i < (int)items_.size(); ++i) if (items_[i] == selLabel) { selectedIndex_ = i; break; }
            multiSel_.clear(); checked_.clear();
            SelectionChanged(selectedIndex_);   // 排序使选中项复位/移动 → 通知外部同步
            UpdateScrollInfo(); UpdateIndicatorTarget(); RequestRepaint();
        }
        bool IsSortAscending() const { return sortAscending_; }
        void SetShowSortIndicator(bool show) { showSortIndicator_ = show; RequestRepaint(); }
        void SetSortIndicatorColor(Color color) { sortIndicatorColor_ = color.ToD2D(); sortIndicatorBrush_.Reset(); RequestRepaint(); }

        // 样式设置
        void SetItemHeight(float height) { itemHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        float GetItemHeight() const { return itemHeight_; }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }

        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) {
            textColor_ = color.ToD2D();
            for (auto& label : items_) {
                if (label) label->SetTextColor(color);
            }
            textBrush_.Reset();
            RequestRepaint();
        }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // 全局默认样式
        static void SetDefaultItemHeight(float height) { DefaultItemHeight = height; }
        static void SetDefaultColors(Color bg, Color text, Color selected, Color hover, Color border) {
            DefaultBackgroundColor = bg.ToD2D();
            DefaultTextColor = text.ToD2D();
            DefaultSelectedColor = selected.ToD2D();
            DefaultHoverColor = hover.ToD2D();
            DefaultBorderColor = border.ToD2D();
        }
        static void SetDefaultIndicatorColor(Color color) { DefaultIndicatorColor = color.ToD2D(); }
        static void SetDefaultIndicatorWidth(float width) { DefaultIndicatorWidth = width; }
        static void SetDefaultIndicatorHeightRatio(float ratio) { DefaultIndicatorHeightRatio = clamp(ratio, 0.1f, 1.0f); }
        static void SetDefaultIndicatorAnimationSpeed(float speed) { DefaultIndicatorAnimSpeed = speed; }
        static void SetDefaultScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            DefaultScrollTrackColor = track.ToD2D();
            DefaultScrollThumbColor = thumb.ToD2D();
            DefaultScrollHoverThumbColor = hoverThumb.ToD2D();
        }
        static void SetDefaultScrollBarWidth(float width) { DefaultScrollBarWidth = width; }
        static void SetDefaultScrollBarMinLength(float length) { DefaultScrollBarMinLength = length; }
        static void SetDefaultScrollWheelStep(float step) { DefaultScrollWheelStep = step; }
        static void SetDefaultScrollAnimationSpeed(float speed) { DefaultScrollAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        // UIElement 接口
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            childrenDirty_ = true;              // 重排过 → 可见子元素列表必须重建（含滚动后坐标）
        }


        // Label 化：可见项 Label 作为子元素进入 Window 合成/事件/动画流程
        // 注意：必须在"滚动后"的坐标上 Arrange —— 因为 ComposeImpl 的裁剪剔除读 arrangedRect_，
        // 若只放变换、arrangedRect_ 停在原位，屏幕外的项会被误判为"裁剪区外"而整棵跳过（新项不出现）。
        void VisibleRange(int& first, int& last) const {
            float eff = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            first = 0; last = -1;
            if (eff <= 0.0f) return;
            float snapped = Snap(scrollOffsetY_);
            first = (int)(snapped / eff); if (first < 0) first = 0;
            last = (int)((snapped + arrangedRect_.height) / eff);
            if (last > (int)items_.size() - 1) last = (int)items_.size() - 1;
        }
        const std::vector<UIElement*>& GetChildren() const override {
            float eff = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float snapped = Snap(scrollOffsetY_);
            // 命中缓存要覆盖影响布局的几何：滚动量、项数、行高、勾选框（只有滚动量时会漏掉“滚动中增删项”）
            float key = snapped
                      + (float)items_.size() * 1000000.0f + eff * 100000.0f
                      + (itemsCheckable_ ? 10000.0f : 0.0f);
            if (!childrenDirty_ && key == lastChildrenSnap_) return childrenView_;
            childrenDirty_ = false;
            lastChildrenSnap_ = key;
            childrenView_.clear();
            if (eff <= 0.0f) return childrenView_;
            float left = arrangedRect_.x + (buttonMode_ ? 4.0f : 0.0f) + 8.0f;   // 按钮模式 itemRect 左移 4（与原绘制一致）
            if (itemsCheckable_) left += 15.0f + 6.0f;
            float right = arrangedRect_.x + arrangedRect_.width - (showScrollBar_ ? scrollBarWidth_ : 0) - 8.0f;
            float w = right - left; if (w < 0.0f) w = 0.0f;
            const float bleedY = 3.0f;
            int first, last; VisibleRange(first, last);
            for (int i = first; i <= last; ++i) {
                if (!items_[i]) continue;
                float y = arrangedRect_.y + i * eff - snapped;      // 就地摆到滚动后的位置
                items_[i]->Arrange(Rect(left, y - bleedY, w, itemHeight_ + bleedY * 2.0f));
                childrenView_.push_back(items_[i].get());
            }
            return childrenView_;
        }
        // 裁剪只给子元素用；向外扩 2px，避免 ListView 自己的描边/阴影被切
        std::optional<D2D1_RECT_F> GetClipRect() const override {
            return D2D1::RectF(arrangedRect_.x - 2.0f, arrangedRect_.y - 2.0f,
                arrangedRect_.x + arrangedRect_.width + 2.0f, arrangedRect_.y + arrangedRect_.height + 2.0f);
        }
        mutable float lastChildrenSnap_ = -1e30f;

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            float viewportWidth = arrangedRect_.width - (showScrollBar_ ? scrollBarWidth_ : 0);
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;

            if (!buttonMode_) {
                if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(backgroundColor_);
                if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());
            }

            D2D1_RECT_F clipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + arrangedRect_.height);
            rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            float snapped = Snap(scrollOffsetY_);
            int firstVisible = (int)(snapped / effectiveRowHeight);
            int lastVisible = (int)((snapped + arrangedRect_.height) / effectiveRowHeight);
            lastVisible = min(lastVisible, (int)items_.size() - 1);
            if (firstVisible < 0) firstVisible = 0;

            IDWriteTextFormat* fmt = GetFontFormat();

            for (int i = firstVisible; i <= lastVisible && i < (int)items_.size(); ++i) {
                float itemY = arrangedRect_.y + i * effectiveRowHeight - snapped;
                D2D1_RECT_F itemRect = D2D1::RectF(arrangedRect_.x, itemY,
                    arrangedRect_.x + viewportWidth, itemY + itemHeight_);

                if (buttonMode_) {
                    itemRect.left += 4.0f;
                    itemRect.right -= 4.0f;
                    itemRect.top += buttonSpacing_ / 2.0f;
                    itemRect.bottom -= buttonSpacing_ / 2.0f;
                }

                bool isSel = IsIndexSelected(i);
                if (isSel) {
                    if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                    if (buttonMode_) rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, 4, 4), selectedBrush_.Get());
                    else rt->FillRectangle(itemRect, selectedBrush_.Get());
                }
                else if (i == hoveredIndex_) {
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                    if (buttonMode_) rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, 4, 4), hoverBrush_.Get());
                    else rt->FillRectangle(itemRect, hoverBrush_.Get());
                }
                else if (alternatingRowColors_ && (i & 1)) {
                    if (!alternateBrush_) rt->CreateSolidColorBrush(alternateRowColor_, alternateBrush_.GetAddressOf());
                    if (buttonMode_) rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, 4, 4), alternateBrush_.Get());
                    else rt->FillRectangle(itemRect, alternateBrush_.Get());
                }

                // Label 化：复选框仍由 ListView 画；文本由该行 Label 自己画（走 Window 合成递归，见 GetChildren）
                if (itemsCheckable_) {
                    float size = 15.0f;
                    float cy = (itemRect.top + itemRect.bottom) / 2.0f;
                    D2D1_RECT_F cbRect = D2D1::RectF(itemRect.left + 8.0f, cy - size / 2.0f, itemRect.left + 8.0f + size, cy + size / 2.0f);
                    bool on = checked_.count(i) > 0;
                    CheckBox::DrawBox(rt, cbRect, on ? 1.0f : 0.0f,
                        on ? CheckBox::State::Checked : CheckBox::State::Unchecked,
                        checkBoxColor_, checkMarkColor_, borderColor_, 4.0f, listCheckBrush_);
                }
            }

            if (showSortIndicator_ && !items_.empty() && fmt) {
                if (!sortIndicatorBrush_) rt->CreateSolidColorBrush(sortIndicatorColor_, sortIndicatorBrush_.GetAddressOf());
                else sortIndicatorBrush_->SetColor(sortIndicatorColor_);
                if (sortIndicatorBrush_) {
                    std::wstring arrow = sortAscending_ ? L"\u25B2" : L"\u25BC";
                    D2D1_RECT_F ar = D2D1::RectF(arrangedRect_.x + viewportWidth - 18.0f, arrangedRect_.y,
                        arrangedRect_.x + viewportWidth - 2.0f, arrangedRect_.y + itemHeight_);
                    rt->DrawText(arrow.c_str(), (UINT32)arrow.length(), fmt, ar, sortIndicatorBrush_.Get());
                }
            }

            if (marqueeActive_) {
                float vx0 = arrangedRect_.x + min(pressStartCX_, marqueeCurCX_);
                float vx1 = arrangedRect_.x + max(pressStartCX_, marqueeCurCX_);
                float vy0 = arrangedRect_.y + min(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_);
                float vy1 = arrangedRect_.y + max(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_);
                if (!marqueeFillBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.55f, 0.90f, 0.18f), marqueeFillBrush_.GetAddressOf());
                if (!marqueeBorderBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.95f), marqueeBorderBrush_.GetAddressOf());
                D2D1_RECT_F mr = D2D1::RectF(vx0, vy0, vx1, vy1);
                if (marqueeFillBrush_) rt->FillRectangle(mr, marqueeFillBrush_.Get());
                if (marqueeBorderBrush_) rt->DrawRectangle(mr, marqueeBorderBrush_.Get(), 2.0f);
            }

            if (items_.empty() && !emptyText_.empty()) {
                D2D1_RECT_F er = D2D1::RectF(arrangedRect_.x + 8.0f, arrangedRect_.y,
                    arrangedRect_.x + viewportWidth - 8.0f, arrangedRect_.y + arrangedRect_.height);
                if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                else textBrush_->SetColor(textColor_);
                if (fmt) rt->DrawText(emptyText_.c_str(), (UINT32)emptyText_.length(), fmt, er, textBrush_.Get());
            }

            // 指示条
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                float indicatorTop = arrangedRect_.y + indicatorY_ - Snap(scrollOffsetY_);
                float indicatorHeight = itemHeight_ * indicatorHeightRatio_;
                float indicatorOffset = (itemHeight_ - indicatorHeight) / 2.0f;
                float drawTop = indicatorTop + indicatorOffset;
                float drawBottom = drawTop + indicatorHeight;

                if (drawBottom > arrangedRect_.y && drawTop < arrangedRect_.y + arrangedRect_.height) {
                    if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                    else indicatorBrush_->SetColor(indicatorColor_);
                    float indicatorLeft = buttonMode_ ? arrangedRect_.x + 4.0f : arrangedRect_.x;
                    D2D1_RECT_F indicatorRect = D2D1::RectF(indicatorLeft, drawTop,
                        indicatorLeft + indicatorWidth_, drawBottom);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                }
            }

            rt->PopAxisAlignedClip();

            if (showScrollBar_) DrawScrollBar(rt, viewportWidth);

            if (!buttonMode_) {
                if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
                else borderBrush_->SetColor(borderColor_);
                if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);
            }
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showScrollBar_) {
                    float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
                    if (x >= trackX) return this;
                }
                return this;
            }
            return nullptr;
        }

        void OnMouseEnter() override { isHovered_ = true; RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override {
            isHovered_ = false;
            hoveredIndex_ = -1;
            isScrollBarHovered_ = false;
            RequestRepaint();
            MouseLeave.Fire();
        }
        void OnMouseMove(float x, float y) override {
            if (isDraggingScroll_) {
                float trackY = arrangedRect_.y;
                float trackHeight = arrangedRect_.height;
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                if (trackHeight > thumbLength) {
                    float ratio = (y - dragStartMouseY_) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                }
                return;
            }
            if (pressActive_ && marqueeEnabled_ && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                float cx = x - arrangedRect_.x;
                float cy = y - arrangedRect_.y + Snap(scrollOffsetY_);
                if (!marqueeActive_ && (fabs(cx - pressStartCX_) > 4.0f || fabs(cy - pressStartCY_) > 4.0f)) {
                    marqueeActive_ = true;
                    multiSel_.clear();
                    selectedIndex_ = -1;
                    UpdateIndicatorTarget();
                }
                if (marqueeActive_) {
                    marqueeCurCX_ = cx;
                    marqueeCurCY_ = cy;
                    if (y < arrangedRect_.y + 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - 14.0f, 0.0f, maxScrollY_);
                    else if (y > arrangedRect_.y + arrangedRect_.height - 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ + 14.0f, 0.0f, maxScrollY_);
                    ApplyMarqueeSelection();
                    return;
                }
            }
            if (!arrangedRect_.Contains(x, y)) {
                hoveredIndex_ = -1;
                isScrollBarHovered_ = false;
                return;
            }
            isScrollBarHovered_ = (showScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            if (showScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                hoveredIndex_ = -1;
                RequestRepaint();
                return;
            }
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float relY = y - arrangedRect_.y + Snap(scrollOffsetY_);
            int idx = (int)(relY / effectiveRowHeight);
            if (idx >= 0 && idx < (int)items_.size())
                hoveredIndex_ = idx;
            else
                hoveredIndex_ = -1;
            {
                Label* hp = (hoveredIndex_ >= 0 && hoveredIndex_ < (int)items_.size()) ? items_[hoveredIndex_].get() : nullptr;
                auto it = hp ? itemTips_.find(hp) : itemTips_.end();
                SetToolTip((hp && it != itemTips_.end()) ? it->second : std::wstring());
            }
            RequestRepaint();
            MouseMove.Fire(x, y);
        }
        bool OnContextMenu(float x, float y) override {
            lastContextRow_ = -1;
            if (arrangedRect_.Contains(x, y)) {
                float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
                float relY = y - arrangedRect_.y + Snap(scrollOffsetY_);
                int idx = (int)(relY / effectiveRowHeight);
                if (idx >= 0 && idx < (int)items_.size()) {
                    lastContextRow_ = idx;
                    ItemRightClicked(idx);
                }
            }
            return false;   // 继续弹默认右键菜单（若设了 SetContextMenu / 工厂）
        }

        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;
            if (showScrollBar_) {
                float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
                if (x >= trackX) {
                    float trackY = arrangedRect_.y;
                    float trackHeight = arrangedRect_.height;
                    float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                    float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                    D2D1_RECT_F thumbRect = D2D1::RectF(trackX, thumbY, trackX + scrollBarWidth_, thumbY + thumbLength);
                    if (y >= thumbRect.top && y <= thumbRect.bottom) {
                        isDraggingScroll_ = true;
                        dragStartMouseY_ = y;
                        dragStartScrollY_ = scrollOffsetY_;
                        return;
                    }
                    else {
                        float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                        targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                        RequestRepaint();
                        return;
                    }
                }
            }
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float relY = y - arrangedRect_.y + Snap(scrollOffsetY_);
            int idx = (int)(relY / effectiveRowHeight);

            pressActive_ = true;
            marqueeActive_ = false;
            pressStartCX_ = x - arrangedRect_.x;
            pressStartCY_ = y - arrangedRect_.y + Snap(scrollOffsetY_);
            marqueeCurCX_ = pressStartCX_;
            marqueeCurCY_ = pressStartCY_;

            if (idx >= 0 && idx < (int)items_.size() && !IsItemDisabled(idx)) {
                if (itemsCheckable_) {
                    float eff = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
                    float itemTop = arrangedRect_.y + idx * eff - Snap(scrollOffsetY_);
                    float cy = itemTop + itemHeight_ / 2.0f;
                    float cbX = arrangedRect_.x + 8.0f;
                    if (x >= cbX && x <= cbX + 15.0f && y >= cy - 7.5f && y <= cy + 7.5f) {
                        SetItemChecked(idx, !IsItemChecked(idx));
                        return;
                    }
                }
                if (selectionMode_ == SelectionMode::None) {
                    ItemClicked(idx);
                    MouseDown.Fire(x, y);
                    return;
                }
                DWORD now = GetTickCount();
                bool isDouble = (now - lastClickTick_ < GetDoubleClickTime() && lastClickIndex_ == idx);
                lastClickTick_ = now;
                lastClickIndex_ = idx;
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (selectionMode_ == SelectionMode::Single) {
                    SetSelectedIndex(idx);
                }
                else if (selectionMode_ == SelectionMode::Multi) {
                    ToggleIndexSelection(idx);
                }
                else {   // Extended
                    if (ctrl) {
                        ToggleIndexSelection(idx);
                    }
                    else if (shift && selectedIndex_ >= 0) {
                        multiSel_.clear();
                        int a = min(selectedIndex_, idx), b = max(selectedIndex_, idx);
                        for (int k = a; k <= b; ++k) multiSel_.insert(k);
                        RequestRepaint();
                    }
                    else {
                        SetSelectedIndex(idx);
                    }
                }
                ItemClicked(idx);
                if (isDouble) ItemDoubleClicked(idx);
            }
            MouseDown.Fire(x, y);
        }
        void OnMouseUp(float x, float y) override {
            if (marqueeActive_) {
                marqueeActive_ = false;
                pressActive_ = false;
                RequestRepaint();
                return;
            }
            pressActive_ = false;
            if (isDraggingScroll_) {
                isDraggingScroll_ = false;
                RequestRepaint();
                return;
            }
            MouseUp.Fire(x, y);
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            if (maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                RequestRepaint();
                return true;
            }
            return false;
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || items_.empty()) return;
            switch (key) {
            case VK_UP: { int n = FindEnabledFrom(selectedIndex_ >= 0 ? selectedIndex_ : (int)items_.size(), -1); if (n >= 0) SetSelectedIndex(n); break; }
            case VK_DOWN: { int n = FindEnabledFrom(selectedIndex_ >= 0 ? selectedIndex_ : -1, 1); if (n >= 0) SetSelectedIndex(n); break; }
            case VK_HOME: { int n = FindEnabledFrom(-1, 1); if (n >= 0) SetSelectedIndex(n); break; }
            case VK_END: { int n = FindEnabledFrom((int)items_.size(), -1); if (n >= 0) SetSelectedIndex(n); break; }
            case VK_PRIOR: {
                int page = max(1, (int)(arrangedRect_.height / itemHeight_));
                int target = max(0, (selectedIndex_ < 0 ? 0 : selectedIndex_) - page);
                int n = FindEnabledFrom(target - 1, 1); if (n >= 0) SetSelectedIndex(n); break;
            }
            case VK_NEXT: {
                int page = max(1, (int)(arrangedRect_.height / itemHeight_));
                int target = min((int)items_.size() - 1, (selectedIndex_ < 0 ? 0 : selectedIndex_) + page);
                int n = FindEnabledFrom(target + 1, -1); if (n >= 0) SetSelectedIndex(n); break;
            }
            default:
                break;
            }
            KeyDown.Fire(key, lParam);
        }
        // 首字母定位（type-ahead）
        void OnChar(wchar_t ch) override {
            if (ch < 32 || items_.empty()) return;
            wchar_t lower = (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch + 32) : ch;
            DWORD now = GetTickCount();
            if (now - typeAheadTime_ > 900) typeAheadChars_.clear();
            typeAheadTime_ = now;
            typeAheadChars_.push_back(lower);
            int n = (int)items_.size();
            int start = (selectedIndex_ >= 0 && selectedIndex_ < n) ? selectedIndex_ : -1;
            for (int off = 0; off < n; ++off) {
                int i = (start + 1 + off) % n;
                std::wstring t = GetItemText(i);
                for (auto& c : t) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
                if (t.rfind(typeAheadChars_, 0) == 0 && !IsItemDisabled(i)) { SetSelectedIndex(i); break; }
            }
            Char.Fire(ch);
        }
        void OnFocus() override { RequestRepaint(); Focused.Fire(); }
        void OnBlur() override { RequestRepaint(); Blurred.Fire(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else {
                scrollOffsetY_ = targetScrollOffsetY_;
            }
            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f) RequestRepaint();

            if (isScrollBarHovered_ || isDraggingScroll_) {
                if (scrollHoverProgress_ < 1.0f) {
                    scrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (scrollHoverProgress_ > 1.0f) scrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (scrollHoverProgress_ > 0.0f) {
                    scrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (scrollHoverProgress_ < 0.0f) scrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            // 在末尾强制收敛
            ConvergeValue(scrollHoverProgress_, isScrollBarHovered_ ? 1.0f : 0.0f, 0.001f);
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.001f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.001f);
        }

        bool HasActiveAnimation() const override {
            const float eps = 0.001f;
            return fabs(targetScrollOffsetY_ - scrollOffsetY_) > eps ||
                fabs(indicatorY_ - targetIndicatorY_) > eps ||
                (isScrollBarHovered_ && scrollHoverProgress_ < 1.0f - eps) ||
                (!isScrollBarHovered_ && scrollHoverProgress_ > eps);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            textBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            borderBrush_.Reset();
            indicatorBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            alternateBrush_.Reset();
            marqueeFillBrush_.Reset();
            marqueeBorderBrush_.Reset();
            listCheckBrush_.Reset();
            for (auto& label : items_) if (label) label->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        int lastContextRow_ = -1;   // 最近一次右键所在行（OnContextMenu 记录）
        void ShiftMultiSelForInsert(int index, int count) {
            if (count <= 0) return;
            if (!multiSel_.empty()) {
                std::unordered_set<int> ns;
                for (int s : multiSel_) ns.insert(s >= index ? s + count : s);
                multiSel_ = std::move(ns);
            }
            if (!checked_.empty()) {
                std::unordered_set<int> nc;
                for (int s : checked_) nc.insert(s >= index ? s + count : s);
                checked_ = std::move(nc);
            }
        }
        void ApplyMarqueeSelection() {
            float y0 = min(pressStartCY_, marqueeCurCY_);
            float y1 = max(pressStartCY_, marqueeCurCY_);
            float eff = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            multiSel_.clear();
            for (int i = 0; i < (int)items_.size(); ++i) {
                float top = i * eff, bottom = top + eff;
                if (!(bottom < y0 || top > y1)) multiSel_.insert(i);
            }
            if (marqueeCheckSync_ && itemsCheckable_) {
                for (int i = 0; i < (int)items_.size(); ++i) {
                    bool now = multiSel_.count(i) > 0;
                    bool was = checked_.count(i) > 0;
                    if (now != was) {
                        if (now) checked_.insert(i); else checked_.erase(i);
                        ItemCheckStateChanged(i, now);
                    }
                }
            }
            SelectionChangedMulti(GetSelectedIndices());
            RequestRepaint();
        }

        void UpdateScrollInfo() {
            float contentHeight = items_.size() * (buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_);
            showScrollBar_ = contentHeight > arrangedRect_.height;
            maxScrollY_ = max(0.0f, contentHeight - arrangedRect_.height);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(int index) {
            if (index < 0) return;
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float itemTop = index * effectiveRowHeight;
            float itemBottom = itemTop + itemHeight_;
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + arrangedRect_.height) {
                targetScrollOffsetY_ = itemBottom - arrangedRect_.height;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void UpdateIndicatorTarget() {
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                targetIndicatorY_ = selectedIndex_ * (buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_);
            }
            else {
                targetIndicatorY_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
        }

        void DrawScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            if (maxScrollY_ <= 0.0f) return;   // 无滚动量，避免除零
            float baseTrackWidth = scrollBarWidth_;
            float trackWidth = baseTrackWidth * (1.0f + 0.25f * scrollHoverProgress_);
            float trackX = arrangedRect_.x + arrangedRect_.width - trackWidth;
            float trackY = arrangedRect_.y;
            float trackHeight = arrangedRect_.height;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackWidth / 2, trackWidth / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = trackWidth - 2.0f;
            if (thumbWidth < 2.0f) thumbWidth = 2.0f;
            float thumbX = trackX + (trackWidth - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (scrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * scrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * scrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * scrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * scrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                    thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        std::vector<std::shared_ptr<Label>> items_;
        int selectedIndex_;
        int hoveredIndex_;
        float scrollOffsetY_;
        float targetScrollOffsetY_;
        float maxScrollY_;
        bool showScrollBar_;
        bool isDraggingScroll_;
        float dragStartMouseY_;
        float dragStartScrollY_;
        float itemHeight_;
        std::unordered_set<Label*> disabledItems_;
        std::unordered_map<Label*, D2D1_COLOR_F> itemTextColors_;
        std::unordered_map<Label*, std::wstring> itemTips_;
        std::wstring typeAheadChars_;
        DWORD typeAheadTime_ = 0;
        std::function<bool(const std::wstring&, const std::wstring&)> sortComparator_;
        bool sortAscending_ = true;
        bool showSortIndicator_ = false;
        D2D1_COLOR_F sortIndicatorColor_ = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        ComPtr<ID2D1SolidColorBrush> sortIndicatorBrush_;
        float indicatorWidth_;
        float indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_;
        float targetIndicatorY_;
        float scrollBarWidth_;
        float scrollBarMinLength_;
        float scrollWheelStep_;
        float scrollAnimationSpeed_;
        float hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_;
        D2D1_COLOR_F textColor_;
        D2D1_COLOR_F selectedColor_;
        D2D1_COLOR_F hoverColor_;
        D2D1_COLOR_F borderColor_;
        D2D1_COLOR_F scrollTrackColor_;
        D2D1_COLOR_F scrollThumbColor_;
        D2D1_COLOR_F scrollHoverThumbColor_;
        float scrollHoverProgress_;
        bool isHovered_;
        bool isScrollBarHovered_;
        bool buttonMode_;
        float buttonSpacing_;
        SelectionMode selectionMode_ = SelectionMode::Single;
        std::unordered_set<int> multiSel_;
        std::wstring emptyText_;
        bool alternatingRowColors_ = false;
        D2D1_COLOR_F alternateRowColor_ = D2D1::ColorF(0.97f, 0.97f, 0.97f, 1.0f);
        DWORD lastClickTick_ = 0;
        int lastClickIndex_ = -1;
        ComPtr<ID2D1SolidColorBrush> alternateBrush_;
        bool pressActive_ = false;
        bool marqueeActive_ = false;
        bool marqueeEnabled_ = true;
        bool marqueeCheckSync_ = false;
        float pressStartCX_ = 0, pressStartCY_ = 0, marqueeCurCX_ = 0, marqueeCurCY_ = 0;
        ComPtr<ID2D1SolidColorBrush> marqueeFillBrush_, marqueeBorderBrush_;
        bool itemsCheckable_ = false;
        std::unordered_set<int> checked_;
        D2D1_COLOR_F checkBoxColor_ = CheckBox::DefaultBoxColor;
        D2D1_COLOR_F checkMarkColor_ = CheckBox::DefaultCheckColor;
        ComPtr<ID2D1SolidColorBrush> listCheckBrush_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_;
        ComPtr<ID2D1SolidColorBrush> hoverBrush_;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollThumbBrush_;
    };

    // ==================== 表格视图 TableView ====================
    class TableView : public UIElement {
    public:
        enum class SelectionMode { Cell, Row, Column, None };

        // 单元格键：高 32 位存行、低 32 位存列，避免用 row*10000+col 在列数很大时冲突
        static inline long long CellKey(int r, int c) {
            return ((long long)(unsigned)r << 32) | (unsigned)c;
        }
        static inline int CellKeyRow(long long k) { return (int)((unsigned long long)k >> 32); }
        static inline int CellKeyCol(long long k) { return (int)((unsigned long long)k & 0xFFFFFFFFu); }

        inline static float DefaultHeaderHeight = 26.0f;
        inline static float DefaultRowHeight = 24.0f;
        inline static float DefaultMinColumnWidth = 40.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderBackgroundColor = D2D1::ColorF(0.93f, 0.93f, 0.93f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultGridLineColor = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 400.0f;
        inline static float DefaultHeight = 300.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;
        inline static float DefaultColumnResizeHitWidth = 8.0f;

        ZSignal<int, int> CellClicked;          // 单元格点击
        ZSignal<int, int> SelectionChanged;     // 选中变化（参数 row, col）
        ZSignal<int, int> CellDoubleClicked;    // 单元格双击
        ZSignal<int, int> CurrentCellChanged;   // 当前单元格变化
        ZSignal<int> HeaderClicked;             // 点击表头（列索引）
        ZSignal<std::vector<std::pair<int, int>>> SelectionChangedCells; // 框选/多选变化（单元格列表）
        ZSignal<int, bool> ItemCheckStateChanged; // 行勾选变化
        ZSignal<int, int> CellRightClicked;       // 右键点击单元格（传行、列；可配合 SetContextMenuFactory + GetContextRow/GetContextColumn）
        int GetContextRow() const { return lastContextRow_; }      // 最近一次右键所在行（-1=无）
        int GetContextColumn() const { return lastContextCol_; }   // 最近一次右键所在列（-1=无）

        TableView()
            : rowCount_(0), colCount_(0),
            selectedRow_(-1), selectedCol_(-1),
            hoveredRow_(-1), hoveredCol_(-1),
            scrollOffsetX_(0.0f), targetScrollOffsetX_(0.0f), maxScrollX_(0.0f),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showVerticalScrollBar_(false), showHorizontalScrollBar_(false),
            isDraggingVertical_(false), isDraggingHorizontal_(false),
            dragStartMouseX_(0.0f), dragStartMouseY_(0.0f),
            dragStartScrollX_(0.0f), dragStartScrollY_(0.0f),
            isResizingColumn_(false), resizeColumnIndex_(-1),
            resizeStartMouseX_(0.0f), resizeStartColumnWidth_(0.0f),
            headerHeight_(DefaultHeaderHeight),
            rowHeight_(DefaultRowHeight),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            headerBackgroundColor_(DefaultHeaderBackgroundColor),
            textColor_(DefaultTextColor),
            headerTextColor_(DefaultHeaderTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            gridLineColor_(DefaultGridLineColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            selectionMode_(SelectionMode::Cell),
            verticalScrollHoverProgress_(0.0f),
            horizontalScrollHoverProgress_(0.0f),
            isVerticalHovered_(false),
            isHorizontalHovered_(false) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
        }

        // 数据模型
        void SetRowCount(int rows) {
            rows = max(0, rows);
            rowCount_ = rows;
            data_.resize(rows);
            for (auto& row : data_) row.resize(colCount_);
            if (selectedRow_ >= rows) selectedRow_ = -1;
            if (hoveredRow_ >= rows) hoveredRow_ = -1;
            disabledRows_.clear();
            rowHeights_.clear();
            cellTextColors_.clear();
            cellTips_.clear();
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnCount(int cols) {
            cols = max(0, cols);
            colCount_ = cols;
            headers_.resize(cols);
            columnWidths_.assign(cols, DefaultMinColumnWidth);
            for (auto& row : data_) row.resize(cols);
            if (selectedCol_ >= cols) selectedCol_ = -1;
            if (hoveredCol_ >= cols) hoveredCol_ = -1;
            cellTextColors_.clear();
            cellTips_.clear();
            hiddenColumns_.clear();
            columnAlign_.clear();
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int row, int col, std::shared_ptr<Label> label) {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_ || !label) return;
            data_[row][col] = label;
            label->SetParent(this);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int row, int col, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            SetItem(row, col, label);
        }
        std::shared_ptr<Label> GetItemLabel(int row, int col) const {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_) return nullptr;
            if (row >= (int)data_.size() || col >= (int)data_[row].size()) return nullptr;
            return data_[row][col];
        }
        std::wstring GetItemText(int row, int col) const {
            auto label = GetItemLabel(row, col);
            return label ? label->GetText() : L"";
        }

        // ---------- 行列数量与增删 ----------
        int GetRowCount() const { return rowCount_; }
        int GetColumnCount() const { return colCount_; }
        void AppendRow() { InsertRow(rowCount_); }
        void InsertRow(int index) {
            if (index < 0 || index > rowCount_) index = rowCount_;
            std::vector<std::shared_ptr<Label>> empty(colCount_);
            data_.insert(data_.begin() + index, empty);
            rowCount_++;
            OnRowInserted(index);
            if (selectedRow_ >= index) selectedRow_++;
            {
                std::unordered_set<int> nc;
                for (int s : checkedRows_) nc.insert(s >= index ? s + 1 : s);
                checkedRows_ = std::move(nc);
            }
            UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint();
        }
        void RemoveRow(int index) {
            if (index < 0 || index >= rowCount_) return;
            data_.erase(data_.begin() + index);
            rowCount_--;
            OnRowRemoved(index);
            if (selectedRow_ == index) selectedRow_ = -1;
            else if (selectedRow_ > index) selectedRow_--;
            {
                std::unordered_set<int> nc;
                for (int s : checkedRows_) { if (s == index) continue; nc.insert(s > index ? s - 1 : s); }
                checkedRows_ = std::move(nc);
            }
            UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint();
        }
        void AppendColumn() { InsertColumn(colCount_); }
        void InsertColumn(int index) {
            if (index < 0 || index > colCount_) index = colCount_;
            headers_.insert(headers_.begin() + index, L"");
            columnWidths_.insert(columnWidths_.begin() + index, DefaultMinColumnWidth);
            for (auto& row : data_) row.insert(row.begin() + index, nullptr);
            colCount_++;
            OnColumnInserted(index);
            if (selectedCol_ >= index) selectedCol_++;
            UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint();
        }
        void RemoveColumn(int index) {
            if (index < 0 || index >= colCount_) return;
            headers_.erase(headers_.begin() + index);
            if (index < (int)columnWidths_.size()) columnWidths_.erase(columnWidths_.begin() + index);
            for (auto& row : data_) if (index < (int)row.size()) row.erase(row.begin() + index);
            colCount_--;
            OnColumnRemoved(index);
            if (selectedCol_ == index) selectedCol_ = -1;
            else if (selectedCol_ > index) selectedCol_--;
            UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint();
        }
        void SetHeaderLabel(int col, const std::wstring& text) {
            if (col < 0 || col >= colCount_) return;
            if ((int)headers_.size() != colCount_) headers_.resize(colCount_);
            headers_[col] = text;
            RequestRepaint();
        }
        std::wstring GetHeaderLabel(int col) const {
            return (col >= 0 && col < (int)headers_.size()) ? headers_[col] : L"";
        }

        // ---------- 显示选项 ----------
        void SetHeaderVisible(bool visible) { headerVisible_ = visible; InvalidateLayout(); RequestRepaint(); }
        bool IsHeaderVisible() const { return headerVisible_; }
        void SetGridVisible(bool visible) { showGrid_ = visible; RequestRepaint(); }
        bool IsGridVisible() const { return showGrid_; }
        void SetAlternatingRowColors(bool enable) { alternatingRowColors_ = enable; RequestRepaint(); }
        bool GetAlternatingRowColors() const { return alternatingRowColors_; }
        void SetAlternatingRowColor(Color color) { alternateRowColor_ = color.ToD2D(); alternateBrush_.Reset(); RequestRepaint(); }

        // ---------- 行禁用 ----------
        void SetRowDisabled(int row, bool disabled = true) {
            if (row < 0 || row >= rowCount_) return;
            if (disabled) disabledRows_.insert(row); else disabledRows_.erase(row);
            if (row < (int)data_.size()) for (auto& lb : data_[row]) if (lb) lb->SetEnabled(!disabled);   // Label 化：禁用态由 Label 画
            RequestRepaint();
        }
        bool IsRowDisabled(int row) const { return disabledRows_.count(row) > 0; }

        // ---------- 选择辅助 ----------
        void SelectRow(int row) { SetSelectionMode(SelectionMode::Row); SetCurrentCell(row, 0); }
        void SelectColumn(int col) { SetSelectionMode(SelectionMode::Column); SetCurrentCell(0, col); }
        void ClearSelection() { selectedRow_ = -1; selectedCol_ = -1; cellSel_.clear(); UpdateIndicatorTarget(); RequestRepaint(); }
        void ScrollToCell(int row, int col) { EnsureVisible(row, col); RequestRepaint(); }

        void SetHorizontalHeaderLabels(const std::vector<std::wstring>& labels) {
            headers_ = labels;
            if (headers_.size() != colCount_) headers_.resize(colCount_);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnWidth(int col, float width) {
            if (col < 0 || col >= colCount_) return;
            columnWidths_[col] = max(DefaultMinColumnWidth, width);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        float GetColumnWidth(int col) const {
            if (col < 0 || col >= colCount_) return 0;
            return columnWidths_[col];
        }
        // ---------- 列可见性 / 列对齐 ----------
        void SetColumnVisible(int col, bool visible) {
            if (col < 0 || col >= colCount_) return;
            if (visible) hiddenColumns_.erase(col); else hiddenColumns_.insert(col);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        bool IsColumnVisible(int col) const { return col >= 0 && col < colCount_ && hiddenColumns_.count(col) == 0; }
        void SetColumnAlignment(int col, TextHAlign align) { columnAlign_[col] = align; RequestRepaint(); }
        TextHAlign GetColumnAlignment(int col) const {
            auto it = columnAlign_.find(col);
            return it == columnAlign_.end() ? TextHAlign::Left : it->second;
        }

        // ---------- 单元格颜色 / ToolTip / 排序 ----------
        void SetCellTextColor(int row, int col, Color color) {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_) return;
            cellTextColors_[CellKey(row, col)] = color.ToD2D();
            if (row < (int)data_.size() && col < (int)data_[row].size() && data_[row][col]) data_[row][col]->SetTextColor(color);   // Label 化
            RequestRepaint();
        }
        void ClearCellTextColor(int row, int col) { cellTextColors_.erase(CellKey(row, col)); RequestRepaint(); }
        void SetCellToolTip(int row, int col, const std::wstring& tip) {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_) return;
            if (tip.empty()) cellTips_.erase(CellKey(row, col));
            else cellTips_[CellKey(row, col)] = tip;
        }
        void SetColumnComparator(int col, std::function<bool(const std::wstring&, const std::wstring&)> cmp) {
            if (col < 0) return;
            columnComparators_[col] = std::move(cmp);
        }
        void SortByColumn(int col, bool ascending = true) {
            if (col < 0 || col >= colCount_ || rowCount_ <= 0) return;
            auto it = columnComparators_.find(col);
            auto cmp = (it != columnComparators_.end()) ? it->second
                : std::function<bool(const std::wstring&, const std::wstring&)>();
            std::vector<int> perm(rowCount_);
            for (int i = 0; i < rowCount_; ++i) perm[i] = i;
            std::stable_sort(perm.begin(), perm.end(), [&](int ia, int ib) {
                std::wstring ta = (col < (int)data_[ia].size() && data_[ia][col]) ? data_[ia][col]->GetText() : L"";
                std::wstring tb = (col < (int)data_[ib].size() && data_[ib][col]) ? data_[ib][col]->GetText() : L"";
                if (ascending) return cmp ? cmp(ta, tb) : (ta < tb);
                return cmp ? cmp(tb, ta) : (ta > tb);
                });
            std::vector<int> oldToNew(rowCount_, 0);
            std::vector<std::vector<std::shared_ptr<Label>>> newData(rowCount_);
            for (int newPos = 0; newPos < rowCount_; ++newPos) {
                int oldRow = perm[newPos];
                oldToNew[oldRow] = newPos;
                newData[newPos] = data_[oldRow];
            }
            data_ = std::move(newData);
            // 元数据随行重映射，排序后仍跟随原项目
            std::unordered_set<int> nd;
            for (int r : disabledRows_) if (r >= 0 && r < rowCount_) nd.insert(oldToNew[r]);
            disabledRows_ = std::move(nd);
            std::unordered_set<int> nck;
            for (int r : checkedRows_) if (r >= 0 && r < rowCount_) nck.insert(oldToNew[r]);
            checkedRows_ = std::move(nck);
            std::unordered_map<long long, D2D1_COLOR_F> nc;
            for (auto& kv : cellTextColors_) {
                int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first);
                if (r >= 0 && r < rowCount_) nc[CellKey(oldToNew[r], c)] = kv.second;
            }
            cellTextColors_ = std::move(nc);
            std::unordered_map<long long, std::wstring> nt;
            for (auto& kv : cellTips_) {
                int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first);
                if (r >= 0 && r < rowCount_) nt[CellKey(oldToNew[r], c)] = kv.second;
            }
            cellTips_ = std::move(nt);
            std::unordered_map<int, float> nh;
            for (auto& kv : rowHeights_) if (kv.first >= 0 && kv.first < rowCount_) nh[oldToNew[kv.first]] = kv.second;
            rowHeights_ = std::move(nh);
            sortColumn_ = col; sortAscending_ = ascending;
            cellSel_.clear(); selectedRow_ = -1; selectedCol_ = -1;
            SelectionChanged(selectedRow_, selectedCol_);   // 排序复位选中 → 通知外部同步
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            RequestRepaint();
        }
        int GetSortColumn() const { return sortColumn_; }
        bool IsSortAscending() const { return sortAscending_; }
        void SetShowSortIndicator(bool show) { showSortIndicator_ = show; RequestRepaint(); }
        void SetSortIndicatorColor(Color color) { sortIndicatorColor_ = color.ToD2D(); sortIndicatorBrush_.Reset(); RequestRepaint(); }
        void SetRowHeight(float height) { rowHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        void SetRowHeightAt(int row, float height) {
            if (row < 0 || row >= rowCount_) return;
            rowHeights_[row] = max(8.0f, height);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        float GetRowHeightAt(int row) const { return RowHeightAt(row); }
        void ClearRowHeightAt(int row) { rowHeights_.erase(row); UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        void SetHeaderHeight(float height) { headerHeight_ = height; InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }

        // 选择
        void SetSelectionMode(SelectionMode mode) { selectionMode_ = mode; InvalidateLayout(); RequestRepaint(); }
        SelectionMode GetSelectionMode() const { return selectionMode_; }
        // 框选开关 / 框选与勾选同步 / 多选结果查询
        void SetMarqueeEnabled(bool e) { marqueeEnabled_ = e; if (!e) { marqueeActive_ = false; pressActive_ = false; } RequestRepaint(); }
        bool IsMarqueeEnabled() const { return marqueeEnabled_; }
        void SetMarqueeCheckSync(bool e) { marqueeCheckSync_ = e; }
        bool IsMarqueeCheckSync() const { return marqueeCheckSync_; }
        std::vector<std::pair<int, int>> GetSelectedCells() const {
            std::vector<std::pair<int, int>> v;
            for (long long k : cellSel_) v.push_back({ CellKeyRow(k), CellKeyCol(k) });
            return v;
        }
        std::vector<int> GetSelectedRows() const {
            std::vector<int> rows;
            for (long long k : cellSel_) { int r = CellKeyRow(k); if (std::find(rows.begin(), rows.end(), r) == rows.end()) rows.push_back(r); }
            std::sort(rows.begin(), rows.end());
            return rows;
        }
        std::vector<int> GetSelectedColumns() const {
            std::vector<int> cols;
            for (long long k : cellSel_) { int c = CellKeyCol(k); if (std::find(cols.begin(), cols.end(), c) == cols.end()) cols.push_back(c); }
            std::sort(cols.begin(), cols.end());
            return cols;
        }
        // 框选状态（每行 bool）与行勾选
        std::vector<bool> GetSelectionStates() const {
            std::vector<bool> v(rowCount_, false);
            for (int r = 0; r < rowCount_; ++r) {
                for (int c = 0; c < colCount_; ++c) if (cellSel_.count(CellKey(r, c))) { v[r] = true; break; }
            }
            return v;
        }
        void SetCheckable(bool enable) { itemsCheckable_ = enable; if (!enable) checkedRows_.clear(); RequestRepaint(); }
        bool IsCheckable() const { return itemsCheckable_; }
        void SetRowChecked(int row, bool checked) {
            if (row < 0 || row >= rowCount_) return;
            bool cur = checkedRows_.count(row) > 0;
            if (cur == checked) return;
            if (checked) checkedRows_.insert(row); else checkedRows_.erase(row);
            ItemCheckStateChanged(row, checked);
            RequestRepaint();
        }
        bool IsRowChecked(int row) const { return checkedRows_.count(row) > 0; }
        std::vector<bool> GetRowCheckStates() const {
            std::vector<bool> v(rowCount_, false);
            for (int r = 0; r < rowCount_; ++r) v[r] = checkedRows_.count(r) > 0;
            return v;
        }
        std::vector<int> GetCheckedRows() const {
            std::vector<int> v(checkedRows_.begin(), checkedRows_.end());
            std::sort(v.begin(), v.end());
            return v;
        }
        void SetCheckBoxColor(Color c) { checkBoxColor_ = c.ToD2D(); RequestRepaint(); }
        void SetCheckMarkColor(Color c) { checkMarkColor_ = c.ToD2D(); RequestRepaint(); }

        void SetCurrentCell(int row, int col) {
            if (row < -1 || row >= rowCount_ || col < -1 || col >= colCount_) return;
            if (selectionMode_ == SelectionMode::None) return;
            if (row == -1 || col == -1) { row = -1; col = -1; }   // 清除态统一，避免 row=-1 而 col 被强制成 0
            else if (selectionMode_ == SelectionMode::Row) col = 0;
            else if (selectionMode_ == SelectionMode::Column) row = 0;
            if (selectedRow_ != row || selectedCol_ != col) {
                selectedRow_ = row;
                selectedCol_ = col;
                cellSel_.clear();
                SelectionChanged(row, col);
                CurrentCellChanged(row, col);
                EnsureVisible(row, col);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        int GetCurrentRow() const { return selectedRow_; }
        int GetCurrentColumn() const { return selectedCol_; }

        // 样式
        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetHeaderBackgroundColor(Color color) { headerBackgroundColor_ = color.ToD2D(); headerBgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) {
            textColor_ = color.ToD2D();
            for (auto& row : data_) for (auto& label : row) if (label) label->SetTextColor(color);
            textBrush_.Reset();
            RequestRepaint();
        }
        void SetHeaderTextColor(Color color) { headerTextColor_ = color.ToD2D(); headerTextBrush_.Reset(); RequestRepaint(); }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetGridLineColor(Color color) { gridLineColor_ = color.ToD2D(); gridLineBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // 全局默认样式
        static void SetDefaultHeaderHeight(float height) { DefaultHeaderHeight = height; }
        static void SetDefaultRowHeight(float height) { DefaultRowHeight = height; }
        static void SetDefaultMinColumnWidth(float width) { DefaultMinColumnWidth = width; }
        static void SetDefaultColors(Color bg, Color headerBg, Color text, Color headerText, Color selected, Color hover, Color gridLine, Color border) {
            DefaultBackgroundColor = bg.ToD2D();
            DefaultHeaderBackgroundColor = headerBg.ToD2D();
            DefaultTextColor = text.ToD2D();
            DefaultHeaderTextColor = headerText.ToD2D();
            DefaultSelectedColor = selected.ToD2D();
            DefaultHoverColor = hover.ToD2D();
            DefaultGridLineColor = gridLine.ToD2D();
            DefaultBorderColor = border.ToD2D();
        }
        static void SetDefaultIndicatorColor(Color color) { DefaultIndicatorColor = color.ToD2D(); }
        static void SetDefaultIndicatorWidth(float width) { DefaultIndicatorWidth = width; }
        static void SetDefaultIndicatorHeightRatio(float ratio) { DefaultIndicatorHeightRatio = clamp(ratio, 0.1f, 1.0f); }
        static void SetDefaultIndicatorAnimationSpeed(float speed) { DefaultIndicatorAnimSpeed = speed; }
        static void SetDefaultScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            DefaultScrollTrackColor = track.ToD2D();
            DefaultScrollThumbColor = thumb.ToD2D();
            DefaultScrollHoverThumbColor = hoverThumb.ToD2D();
        }
        static void SetDefaultScrollBarWidth(float width) { DefaultScrollBarWidth = width; }
        static void SetDefaultScrollBarMinLength(float length) { DefaultScrollBarMinLength = length; }
        static void SetDefaultScrollWheelStep(float step) { DefaultScrollWheelStep = step; }
        static void SetDefaultScrollAnimationSpeed(float speed) { DefaultScrollAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }
        static void SetDefaultColumnResizeHitWidth(float width) { DefaultColumnResizeHitWidth = width; }

        // UIElement 接口
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size MeasureOverride(const Size& availableSize) override { return Size(width_, height_); }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            childrenDirty_ = true;              // 重排过 → 可见单元格列表必须重建
        }

        // Label 化：单元格 Label 的统一初始化（套 TableView 字体 / 列对齐 / 内边距，关自身缓存）
        void PrepareCellLabel(const std::shared_ptr<Label>& lb, int col) const {
            if (!lb) return;
            lb->SetUseCache(false);
            FontSpec spec = GetEffectiveFontSpec();
            if (!(lb->GetEffectiveFontSpec() == spec)) lb->SetFont(spec);
            Label::HAlign ha = Label::HAlign::Left;
            TextHAlign a = GetColumnAlignment(col);
            if (a == TextHAlign::Center) ha = Label::HAlign::Center;
            else if (a == TextHAlign::Right) ha = Label::HAlign::Right;
            if (lb->GetHorizontalAlignment() != ha) lb->SetAlignment(ha, Label::VAlign::Center);
            lb->SetPadding(0.0f);
        }

        // Label 化：可见单元格 Label 作为子元素进入 Window 合成流程（就地摆到滚动后的位置）
        const std::vector<UIElement*>& GetChildren() const override {
            // 命中缓存必须覆盖“所有影响单元格布局的几何”：滚动量、列宽和、行高总和、行列数、控件矩形。
            // SetColumnHidden/SetColumnWidth 等不一定会触发 ArrangeOverride，只比滚动量会在改列后留下旧布局。
            float sumW = 0.0f;
            for (int c = 0; c < colCount_; ++c) sumW += GetEffectiveColumnWidth(c);
            float totalH = TotalRowsHeight();
            float sx = Snap(scrollOffsetX_), sy = Snap(scrollOffsetY_);
            if (!childrenDirty_ &&
                sx == lastScrollX_ && sy == lastScrollY_ && sumW == lastSumW_ && totalH == lastTotalH_ &&
                rowCount_ == lastRowCount_ && colCount_ == lastColCount_ &&
                arrangedRect_.x == lastArrX_ && arrangedRect_.y == lastArrY_ &&
                arrangedRect_.width == lastArrW_ && arrangedRect_.height == lastArrH_)
                return childrenView_;
            childrenDirty_ = false;
            lastScrollX_ = sx; lastScrollY_ = sy; lastSumW_ = sumW; lastTotalH_ = totalH;
            lastRowCount_ = rowCount_; lastColCount_ = colCount_;
            lastArrX_ = arrangedRect_.x; lastArrY_ = arrangedRect_.y;
            lastArrW_ = arrangedRect_.width; lastArrH_ = arrangedRect_.height;
            childrenView_.clear();
            if (rowCount_ <= 0 || colCount_ <= 0) return childrenView_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0.0f;
            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            Rect contentClip(arrangedRect_.x, arrangedRect_.y + headerOffset, viewportWidth, viewportHeight - headerOffset);
            int firstRow = RowAtY(scrollOffsetY_); if (firstRow < 0) firstRow = 0;
            int lastRow = RowAtY(scrollOffsetY_ + viewportHeight - headerOffset);
            if (lastRow < 0) lastRow = rowCount_ - 1;
            if (lastRow > rowCount_ - 1) lastRow = rowCount_ - 1;
            const float bleedY = 3.0f;
            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int col = 0; col < colCount_; ++col) {
                float colWidth = GetEffectiveColumnWidth(col);
                if (colWidth <= 0.0f) { colX += colWidth; continue; }
                bool colVisible = (colX + colWidth >= arrangedRect_.x) && (colX <= arrangedRect_.x + viewportWidth);
                if (colVisible) {
                    for (int row = firstRow; row <= lastRow && row < rowCount_; ++row) {
                        auto lb = GetItemLabel(row, col);
                        if (!lb) continue;
                        PrepareCellLabel(lb, col);
                        float rowY = arrangedRect_.y + headerOffset + RowTop(row) - Snap(scrollOffsetY_);
                        // 第一列要让出“指示条 + 勾选框”（与 Draw 里勾选框定位一致），其它列左右各留 8px，别贴边
                        float textLeft = colX + 8.0f;
                        if (col == 0) textLeft += indicatorWidth_ + (itemsCheckable_ ? 21.0f : 0.0f);
                        float availW = (colX + colWidth - 8.0f) - textLeft;
                        if (availW < 0.0f) availW = 0.0f;
                        lb->Arrange(Rect(textLeft, rowY - bleedY, availW, RowHeightAt(row) + bleedY * 2.0f));
                        // 裁到“表头以下、滚动条以内”的内容视口（否则会压到表头/横向滚动条/漫出左右边界）
                        lb->SetClipRect(contentClip);
                        childrenView_.push_back(lb.get());
                    }
                }
                colX += colWidth;
            }
            return childrenView_;
        }
        std::optional<D2D1_RECT_F> GetClipRect() const override {
            return D2D1::RectF(arrangedRect_.x - 2.0f, arrangedRect_.y - 2.0f,
                arrangedRect_.x + arrangedRect_.width + 2.0f, arrangedRect_.y + arrangedRect_.height + 2.0f);
        }
        mutable float lastScrollX_ = -1e30f, lastScrollY_ = -1e30f, lastSumW_ = -1e30f, lastTotalH_ = -1e30f;
        mutable float lastArrX_ = -1e30f, lastArrY_ = -1e30f, lastArrW_ = -1e30f, lastArrH_ = -1e30f;
        mutable int lastRowCount_ = -1, lastColCount_ = -1;

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(backgroundColor_);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float headerOffset = headerVisible_ ? headerHeight_ : 0;

            D2D1_RECT_F contentClipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y + headerOffset,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + viewportHeight);
            rt->PushAxisAlignedClip(contentClipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            int firstRow = RowAtY(scrollOffsetY_);
            if (firstRow < 0) firstRow = 0;
            int lastRow = RowAtY(scrollOffsetY_ + viewportHeight - headerOffset);
            if (lastRow < 0) lastRow = rowCount_ - 1;
            lastRow = min(lastRow, rowCount_ - 1);

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int col = 0; col < colCount_; ++col) {
                float colWidth = GetEffectiveColumnWidth(col);
                if (colWidth <= 0.0f) { colX += colWidth; continue; }
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    for (int row = firstRow; row <= lastRow && row < rowCount_; ++row) {
                        float rowY = arrangedRect_.y + headerOffset + RowTop(row) - Snap(scrollOffsetY_);
                        D2D1_RECT_F cellRect = D2D1::RectF(colX, rowY, colX + colWidth, rowY + RowHeightAt(row));

                        bool isSelected = cellSel_.count(CellKey(row, col)) > 0;
                        if (!isSelected) {
                            if (selectionMode_ == SelectionMode::Cell && row == selectedRow_ && col == selectedCol_)
                                isSelected = true;
                            else if (selectionMode_ == SelectionMode::Row && row == selectedRow_)
                                isSelected = true;
                            else if (selectionMode_ == SelectionMode::Column && col == selectedCol_)
                                isSelected = true;
                        }

                        bool isHovered = false;
                        if (selectionMode_ == SelectionMode::Cell)
                            isHovered = (row == hoveredRow_ && col == hoveredCol_);
                        else if (selectionMode_ == SelectionMode::Row)
                            isHovered = (row == hoveredRow_);
                        else if (selectionMode_ == SelectionMode::Column)
                            isHovered = (col == hoveredCol_);

                        if (isSelected) {
                            if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                            rt->FillRectangle(cellRect, selectedBrush_.Get());
                        }
                        else if (isHovered) {
                            if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                            rt->FillRectangle(cellRect, hoverBrush_.Get());
                        }
                        else if (alternatingRowColors_ && (row & 1)) {
                            if (!alternateBrush_) rt->CreateSolidColorBrush(alternateRowColor_, alternateBrush_.GetAddressOf());
                            rt->FillRectangle(cellRect, alternateBrush_.Get());
                        }

                        float cellTextLeft = cellRect.left + 4.0f;
                        if (col == 0) {
                            cellTextLeft += indicatorWidth_ + 4.0f;
                            if (itemsCheckable_) {
                                float size = 15.0f;
                                float ccy = (cellRect.top + cellRect.bottom) / 2.0f;
                                D2D1_RECT_F cbRect = D2D1::RectF(cellTextLeft, ccy - size / 2.0f, cellTextLeft + size, ccy + size / 2.0f);
                                bool on = checkedRows_.count(row) > 0;
                                CheckBox::DrawBox(rt, cbRect, on ? 1.0f : 0.0f,
                                    on ? CheckBox::State::Checked : CheckBox::State::Unchecked,
                                    checkBoxColor_, checkMarkColor_, gridLineColor_, 4.0f, rowCheckBrush_);
                                cellTextLeft = cbRect.right + 6.0f;
                            }
                        }
                        // Label 化：单元格文本由该格 Label 自己画（走 Window 合成递归，见 GetChildren）
                    }
                    if (showGrid_) {
                        if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                        else gridLineBrush_->SetColor(gridLineColor_);
                        if (gridLineBrush_) {
                            rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y + headerOffset),
                                D2D1::Point2F(colX, arrangedRect_.y + viewportHeight), gridLineBrush_.Get(), 1.0f);
                        }
                    }
                }
                colX += colWidth;
            }

            if (showGrid_) {
                if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                else gridLineBrush_->SetColor(gridLineColor_);
                if (gridLineBrush_) {
                    for (int row = firstRow; row <= lastRow && row < rowCount_; ++row) {
                        float lineY = arrangedRect_.y + headerOffset + RowTop(row + 1) - Snap(scrollOffsetY_);
                        rt->DrawLine(D2D1::Point2F(arrangedRect_.x, lineY),
                            D2D1::Point2F(arrangedRect_.x + viewportWidth, lineY), gridLineBrush_.Get(), 1.0f);
                    }
                }
            }

            if (selectedRow_ >= 0) {
                float indicatorTop = arrangedRect_.y + headerOffset + indicatorY_ - Snap(scrollOffsetY_);
                float indicatorHeight = RowHeightAt(selectedRow_) * indicatorHeightRatio_;
                float indicatorOffset = (RowHeightAt(selectedRow_) - indicatorHeight) / 2.0f;
                float drawTop = indicatorTop + indicatorOffset;
                float drawBottom = drawTop + indicatorHeight;

                if (drawBottom > arrangedRect_.y + headerOffset && drawTop < arrangedRect_.y + viewportHeight) {
                    if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                    else indicatorBrush_->SetColor(indicatorColor_);
                    D2D1_RECT_F indicatorRect = D2D1::RectF(arrangedRect_.x, drawTop,
                        arrangedRect_.x + indicatorWidth_, drawBottom);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                }
            }

            if (marqueeActive_) {
                float vx0 = arrangedRect_.x + min(pressStartCX_, marqueeCurCX_) - Snap(scrollOffsetX_);
                float vx1 = arrangedRect_.x + max(pressStartCX_, marqueeCurCX_) - Snap(scrollOffsetX_);
                float vy0 = arrangedRect_.y + headerOffset + min(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_);
                float vy1 = arrangedRect_.y + headerOffset + max(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_);
                if (!marqueeFillBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.55f, 0.90f, 0.18f), marqueeFillBrush_.GetAddressOf());
                if (!marqueeBorderBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.95f), marqueeBorderBrush_.GetAddressOf());
                D2D1_RECT_F mr = D2D1::RectF(vx0, vy0, vx1, vy1);
                if (marqueeFillBrush_) rt->FillRectangle(mr, marqueeFillBrush_.Get());
                if (marqueeBorderBrush_) rt->DrawRectangle(mr, marqueeBorderBrush_.Get(), 2.0f);
            }

            rt->PopAxisAlignedClip();

            if (headerVisible_) DrawHeader(rt, viewportWidth);

            if (showVerticalScrollBar_) DrawVerticalScrollBar(rt, viewportHeight);
            if (showHorizontalScrollBar_) DrawHorizontalScrollBar(rt, viewportWidth);

            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);

            // 表头分隔线最后重画，确保不被行/选中背景覆盖
            if (headerVisible_) {
                float sepY = Snap(arrangedRect_.y + headerOffset) + 0.5f;
                if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                else gridLineBrush_->SetColor(gridLineColor_);
                if (gridLineBrush_)
                    rt->DrawLine(D2D1::Point2F(arrangedRect_.x, sepY),
                        D2D1::Point2F(arrangedRect_.x + viewportWidth, sepY), gridLineBrush_.Get(), 1.0f);
            }
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) return this;
                if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) return this;
                return this;
            }
            return nullptr;
        }

        void OnMouseEnter() override { RequestRepaint(); MouseEnter.Fire(); }
        void OnMouseLeave() override {
            hoveredRow_ = -1;
            hoveredCol_ = -1;
            isVerticalHovered_ = false;
            isHorizontalHovered_ = false;
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            RequestRepaint();
            MouseLeave.Fire();
        }
        void OnMouseMove(float x, float y) override {
            if (isResizingColumn_) {
                float dx = x - resizeStartMouseX_;
                float newWidth = max(DefaultMinColumnWidth, resizeStartColumnWidth_ + dx);
                SetColumnWidth(resizeColumnIndex_, newWidth);
                return;
            }
            if (isDraggingVertical_) { HandleVerticalScrollDrag(y); return; }
            if (isDraggingHorizontal_) { HandleHorizontalScrollDrag(x); return; }
            if (pressActive_ && marqueeEnabled_ && selectionMode_ != SelectionMode::None && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                float ho = headerVisible_ ? headerHeight_ : 0;
                float cx = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float cy = y - arrangedRect_.y - ho + Snap(scrollOffsetY_);
                if (!marqueeActive_ && (fabs(cx - pressStartCX_) > 4.0f || fabs(cy - pressStartCY_) > 4.0f)) {
                    marqueeActive_ = true;
                    cellSel_.clear();
                    selectedRow_ = -1;
                    selectedCol_ = -1;
                    UpdateIndicatorTarget();
                }
                if (marqueeActive_) {
                    marqueeCurCX_ = cx;
                    marqueeCurCY_ = cy;
                    if (y < arrangedRect_.y + ho + 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - 14.0f, 0.0f, maxScrollY_);
                    else if (y > arrangedRect_.y + arrangedRect_.height - 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ + 14.0f, 0.0f, maxScrollY_);
                    ApplyMarqueeSelection();
                    RequestRepaint();
                    return;
                }
            }
            if (!arrangedRect_.Contains(x, y)) return;

            isVerticalHovered_ = (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            isHorizontalHovered_ = (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_);

            if (headerVisible_ && y <= arrangedRect_.y + headerHeight_) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                bool nearBoundary = false;
                for (int col = 0; col < colCount_ - 1; ++col) {
                    float colWidth = GetEffectiveColumnWidth(col);
                    if (colWidth <= 0.0f) { colX += colWidth; continue; }
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        nearBoundary = true;
                        break;
                    }
                    colX += colWidth;
                }
                if (nearBoundary) SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                else SetCursor(LoadCursor(nullptr, IDC_ARROW));
                hoveredRow_ = -1;
                hoveredCol_ = -1;
                RequestRepaint();
                return;
            }
            else {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }

            float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
            float relY = y - arrangedRect_.y - (headerVisible_ ? headerHeight_ : 0) + Snap(scrollOffsetY_);
            int row = RowAtY(relY);
            int col = GetColumnIndexAtX(relX);
            if (row >= 0 && row < rowCount_ && col >= 0 && col < colCount_) {
                hoveredRow_ = row;
                hoveredCol_ = col;
            }
            else {
                hoveredRow_ = -1;
                hoveredCol_ = -1;
            }
            {
                auto it = cellTips_.find(CellKey(hoveredRow_, hoveredCol_));
                SetToolTip((hoveredRow_ >= 0 && it != cellTips_.end()) ? it->second : std::wstring());
            }
            RequestRepaint();
            MouseMove.Fire(x, y);
        }
        bool OnContextMenu(float x, float y) override {
            lastContextRow_ = -1;
            lastContextCol_ = -1;
            if (arrangedRect_.Contains(x, y) && !(headerVisible_ && y <= arrangedRect_.y + headerHeight_)) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float relY = y - arrangedRect_.y - (headerVisible_ ? headerHeight_ : 0) + Snap(scrollOffsetY_);
                int row = RowAtY(relY);
                int col = GetColumnIndexAtX(relX);
                if (row >= 0 && row < rowCount_ && col >= 0 && col < colCount_) {
                    lastContextRow_ = row;
                    lastContextCol_ = col;
                    CellRightClicked(row, col);
                }
            }
            return false;   // 继续弹默认右键菜单（若设了 SetContextMenu / 工厂）
        }

        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;

            if (headerVisible_ && y <= arrangedRect_.y + headerHeight_) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                for (int col = 0; col < colCount_ - 1; ++col) {
                    float colWidth = GetEffectiveColumnWidth(col);
                    if (colWidth <= 0.0f) { colX += colWidth; continue; }
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        isResizingColumn_ = true;
                        resizeColumnIndex_ = col;
                        resizeStartMouseX_ = x;
                        resizeStartColumnWidth_ = GetColumnWidth(col);   // 用基准宽度：SetColumnWidth 存的也是基准值，否则首帧会跳掉第一列的“指示条+勾选框”预留
                        return;
                    }
                    colX += colWidth;
                }
                int hdrCol = GetColumnIndexAtX(relX);
                if (hdrCol >= 0 && hdrCol < colCount_) HeaderClicked(hdrCol);
                return;
            }

            if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                float trackY = arrangedRect_.y;
                float trackHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                D2D1_RECT_F thumbRect = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - scrollBarWidth_, thumbY,
                    arrangedRect_.x + arrangedRect_.width, thumbY + thumbLength);
                if (y >= thumbRect.top && y <= thumbRect.bottom) {
                    isDraggingVertical_ = true;
                    dragStartMouseY_ = y;
                    dragStartScrollY_ = scrollOffsetY_;
                    return;
                }
                else {
                    float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                    return;
                }
            }

            if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) {
                float trackX = arrangedRect_.x;
                float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
                float thumbX = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
                D2D1_RECT_F thumbRect = D2D1::RectF(thumbX, arrangedRect_.y + arrangedRect_.height - scrollBarWidth_,
                    thumbX + thumbLength, arrangedRect_.y + arrangedRect_.height);
                if (x >= thumbRect.left && x <= thumbRect.right) {
                    isDraggingHorizontal_ = true;
                    dragStartMouseX_ = x;
                    dragStartScrollX_ = scrollOffsetX_;
                    return;
                }
                else {
                    float ratio = (x - trackX - thumbLength / 2) / (trackWidth - thumbLength);
                    targetScrollOffsetX_ = clamp(ratio * maxScrollX_, 0.0f, maxScrollX_);
                    RequestRepaint();
                    return;
                }
            }

            float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
            float relY = y - arrangedRect_.y - (headerVisible_ ? headerHeight_ : 0) + Snap(scrollOffsetY_);
            int row = RowAtY(relY);
            int col = GetColumnIndexAtX(relX);

            pressActive_ = true;
            marqueeActive_ = false;
            pressStartCX_ = relX;
            pressStartCY_ = relY;
            marqueeCurCX_ = relX;
            marqueeCurCY_ = relY;

            if (row >= 0 && row < rowCount_ && col >= 0 && col < colCount_) {
                if (itemsCheckable_ && col == 0) {
                    float cbRelX = indicatorWidth_ + 8.0f;
                    if (relX >= cbRelX && relX <= cbRelX + 15.0f) {
                        pressActive_ = false;
                        SetRowChecked(row, !IsRowChecked(row));
                        return;
                    }
                }
                if (IsRowDisabled(row)) { pressActive_ = false; return; }
                DWORD now = GetTickCount();
                bool isDouble = (now - lastClickTick_ < GetDoubleClickTime() && lastClickRow_ == row && lastClickCol_ == col);
                lastClickTick_ = now;
                lastClickRow_ = row;
                lastClickCol_ = col;
                SetCurrentCell(row, col);
                CellClicked(row, col);
                if (isDouble) CellDoubleClicked(row, col);
            }
            MouseDown.Fire(x, y);
        }
        void OnMouseUp(float x, float y) override {
            if (marqueeActive_) { marqueeActive_ = false; pressActive_ = false; RequestRepaint(); return; }
            pressActive_ = false;
            if (isResizingColumn_) { isResizingColumn_ = false; resizeColumnIndex_ = -1; RequestRepaint(); return; }
            if (isDraggingVertical_) { isDraggingVertical_ = false; RequestRepaint(); return; }
            if (isDraggingHorizontal_) { isDraggingHorizontal_ = false; RequestRepaint(); return; }
            MouseUp.Fire(x, y);
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            bool handled = false;
            if (deltaY != 0 && maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                handled = true;
                RequestRepaint();
            }
            if (deltaX != 0 && maxScrollX_ > 0) {
                targetScrollOffsetX_ = clamp(targetScrollOffsetX_ - deltaX * scrollWheelStep_, 0.0f, maxScrollX_);
                handled = true;
                RequestRepaint();
            }
            return handled;
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || rowCount_ == 0 || colCount_ == 0) return;
            int row = selectedRow_;
            int col = selectedCol_;
            if (selectionMode_ == SelectionMode::Row) col = 0;
            else if (selectionMode_ == SelectionMode::Column) row = 0;
            switch (key) {
            case VK_UP: { int r = row - 1; while (r >= 0 && IsRowDisabled(r)) r--; row = max(0, r); break; }
            case VK_DOWN: { int r = row + 1; while (r < rowCount_ && IsRowDisabled(r)) r++; row = min(rowCount_ - 1, r); break; }
            case VK_LEFT: col = max(0, col - 1); break;
            case VK_RIGHT: col = min(colCount_ - 1, col + 1); break;
            case VK_HOME: row = 0; col = 0; break;
            case VK_END: row = rowCount_ - 1; col = colCount_ - 1; break;
            default: return;
            }
            SetCurrentCell(row, col);
            KeyDown.Fire(key, lParam);
        }
        void OnFocus() override { RequestRepaint(); Focused.Fire(); }
        void OnBlur() override { RequestRepaint(); Blurred.Fire(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f) {
                scrollOffsetX_ += (targetScrollOffsetX_ - scrollOffsetX_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetX_ - scrollOffsetX_) <= 0.1f) scrollOffsetX_ = targetScrollOffsetX_;
                RequestRepaint();
            }
            else scrollOffsetX_ = targetScrollOffsetX_;

            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else scrollOffsetY_ = targetScrollOffsetY_;

            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f) RequestRepaint();

            if (isVerticalHovered_ || isDraggingVertical_) {
                if (verticalScrollHoverProgress_ < 1.0f) {
                    verticalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ > 1.0f) verticalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (verticalScrollHoverProgress_ > 0.0f) {
                    verticalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ < 0.0f) verticalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            if (isHorizontalHovered_ || isDraggingHorizontal_) {
                if (horizontalScrollHoverProgress_ < 1.0f) {
                    horizontalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ > 1.0f) horizontalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (horizontalScrollHoverProgress_ > 0.0f) {
                    horizontalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ < 0.0f) horizontalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.1f);
            ConvergeValue(scrollOffsetX_, targetScrollOffsetX_, 0.1f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.1f);
        }

        bool HasActiveAnimation() const override {
            return fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f ||
                fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f ||
                fabs(indicatorY_ - targetIndicatorY_) > 0.01f ||
                (isVerticalHovered_ && verticalScrollHoverProgress_ < 1.0f) ||
                (!isVerticalHovered_ && verticalScrollHoverProgress_ > 0.0f) ||
                (isHorizontalHovered_ && horizontalScrollHoverProgress_ < 1.0f) ||
                (!isHorizontalHovered_ && horizontalScrollHoverProgress_ > 0.0f);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            headerBgBrush_.Reset();
            textBrush_.Reset();
            headerTextBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            indicatorBrush_.Reset();
            gridLineBrush_.Reset();
            borderBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            alternateBrush_.Reset();
            marqueeFillBrush_.Reset();
            marqueeBorderBrush_.Reset();
            rowCheckBrush_.Reset();
            for (auto& row : data_) for (auto& label : row) if (label) label->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        int lastContextRow_ = -1, lastContextCol_ = -1;   // 最近一次右键所在行/列（OnContextMenu 记录）
        float GetEffectiveColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            if (hiddenColumns_.count(col)) return 0.0f;
            float w = columnWidths_[col];
            // 第一列左侧要容纳“选中指示条 + 勾选框”（即单元格文字右移的那段），不补回来会把文字挤窄
            if (col == 0) { w += indicatorWidth_; if (itemsCheckable_) w += 21.0f; }
            return w;
        }

        bool IsCellSelected(int row, int col) const { return cellSel_.count(CellKey(row, col)) > 0; }

        // ---------- 每行高度 ----------
        void RebuildRowMetrics() {
            rowTops_.assign(rowCount_ + 1, 0.0f);
            for (int r = 0; r < rowCount_; ++r) {
                float h = rowHeight_;
                auto it = rowHeights_.find(r);
                if (it != rowHeights_.end()) h = it->second;
                rowTops_[r + 1] = rowTops_[r] + h;
            }
        }
        float RowTop(int r) const {
            if (rowTops_.empty()) return 0.0f;
            if (r < 0) return 0.0f;
            if (r >= (int)rowTops_.size()) return rowTops_.back();
            return rowTops_[r];
        }
        float RowHeightAt(int r) const {
            if (rowTops_.empty()) return rowHeight_;
            if (r < 0 || r + 1 >= (int)rowTops_.size()) return rowHeight_;
            return rowTops_[r + 1] - rowTops_[r];
        }
        float TotalRowsHeight() const { return rowTops_.empty() ? 0.0f : rowTops_.back(); }        int RowAtY(float relY) const {
            if (rowCount_ <= 0 || rowTops_.size() < 2) return -1;
            int lo = 0, hi = rowCount_ - 1, res = -1;
            while (lo <= hi) {
                int m = (lo + hi) / 2;
                if (rowTops_[m] <= relY) { res = m; lo = m + 1; }
                else hi = m - 1;
            }
            if (res < 0) return -1;
            if (relY >= rowTops_[res + 1]) return -1;
            return res;
        }
        void ApplyMarqueeSelection() {
            float x0 = min(pressStartCX_, marqueeCurCX_), x1 = max(pressStartCX_, marqueeCurCX_);
            float y0 = min(pressStartCY_, marqueeCurCY_), y1 = max(pressStartCY_, marqueeCurCY_);
            cellSel_.clear();
            std::vector<int> hitRows, hitCols;
            for (int row = 0; row < rowCount_; ++row) {
                float top = RowTop(row), bottom = RowTop(row + 1);
                if (!(bottom < y0 || top > y1)) hitRows.push_back(row);
            }
            float colX = 0;
            for (int col = 0; col < colCount_; ++col) {
                float w = GetEffectiveColumnWidth(col);
                if (!(colX + w < x0 || colX > x1)) hitCols.push_back(col);
                colX += w;
            }
            if (selectionMode_ == SelectionMode::Row) {
                for (int r : hitRows) for (int c = 0; c < colCount_; ++c) cellSel_.insert(CellKey(r, c));
            }
            else if (selectionMode_ == SelectionMode::Column) {
                for (int c : hitCols) for (int r = 0; r < rowCount_; ++r) cellSel_.insert(CellKey(r, c));
            }
            else {
                for (int r : hitRows) for (int c : hitCols) cellSel_.insert(CellKey(r, c));
            }
            if (marqueeCheckSync_ && itemsCheckable_) {
                for (int r = 0; r < rowCount_; ++r) {
                    bool now = false;
                    for (int c = 0; c < colCount_; ++c) if (cellSel_.count(CellKey(r, c))) { now = true; break; }
                    bool was = checkedRows_.count(r) > 0;
                    if (now != was) { if (now) checkedRows_.insert(r); else checkedRows_.erase(r); ItemCheckStateChanged(r, now); }
                }
            }
            SelectionChangedCells(GetSelectedCells());
        }

        int GetColumnIndexAtX(float relX) const {
            float x = 0;
            for (int i = 0; i < colCount_; ++i) {
                float w = GetEffectiveColumnWidth(i);
                if (relX >= x && relX < x + w) return i;
                x += w;
            }
            return -1;
        }

        void OnRowInserted(int index) {
            std::unordered_set<int> nd;
            for (int r : disabledRows_) nd.insert(r >= index ? r + 1 : r);
            disabledRows_ = std::move(nd);
            std::unordered_map<int, float> nh;
            for (auto& kv : rowHeights_) nh[kv.first >= index ? kv.first + 1 : kv.first] = kv.second;
            rowHeights_ = std::move(nh);
            std::unordered_map<long long, D2D1_COLOR_F> nc;
            for (auto& kv : cellTextColors_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (r >= index) r++; nc[CellKey(r, c)] = kv.second; }
            cellTextColors_ = std::move(nc);
            std::unordered_map<long long, std::wstring> nt;
            for (auto& kv : cellTips_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (r >= index) r++; nt[CellKey(r, c)] = kv.second; }
            cellTips_ = std::move(nt);
        }
        void OnRowRemoved(int index) {
            std::unordered_set<int> nd;
            for (int r : disabledRows_) { if (r == index) continue; nd.insert(r > index ? r - 1 : r); }
            disabledRows_ = std::move(nd);
            std::unordered_map<int, float> nh;
            for (auto& kv : rowHeights_) { if (kv.first == index) continue; nh[kv.first > index ? kv.first - 1 : kv.first] = kv.second; }
            rowHeights_ = std::move(nh);
            std::unordered_map<long long, D2D1_COLOR_F> nc;
            for (auto& kv : cellTextColors_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (r == index) continue; if (r > index) r--; nc[CellKey(r, c)] = kv.second; }
            cellTextColors_ = std::move(nc);
            std::unordered_map<long long, std::wstring> nt;
            for (auto& kv : cellTips_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (r == index) continue; if (r > index) r--; nt[CellKey(r, c)] = kv.second; }
            cellTips_ = std::move(nt);
        }
        void OnColumnInserted(int index) {
            std::unordered_map<long long, D2D1_COLOR_F> nc;
            for (auto& kv : cellTextColors_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (c >= index) c++; nc[CellKey(r, c)] = kv.second; }
            cellTextColors_ = std::move(nc);
            std::unordered_map<long long, std::wstring> nt;
            for (auto& kv : cellTips_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (c >= index) c++; nt[CellKey(r, c)] = kv.second; }
            cellTips_ = std::move(nt);
        }
        void OnColumnRemoved(int index) {
            std::unordered_map<long long, D2D1_COLOR_F> nc;
            for (auto& kv : cellTextColors_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (c == index) continue; if (c > index) c--; nc[CellKey(r, c)] = kv.second; }
            cellTextColors_ = std::move(nc);
            std::unordered_map<long long, std::wstring> nt;
            for (auto& kv : cellTips_) { int r = CellKeyRow(kv.first), c = CellKeyCol(kv.first); if (c == index) continue; if (c > index) c--; nt[CellKey(r, c)] = kv.second; }
            cellTips_ = std::move(nt);
        }

        void UpdateScrollInfo() {
            RebuildRowMetrics();
            float totalContentWidth = 0;
            for (int i = 0; i < colCount_; ++i) totalContentWidth += GetEffectiveColumnWidth(i);
            float totalContentHeight = headerHeight_ + TotalRowsHeight();

            float availWidth = arrangedRect_.width;
            float availHeight = arrangedRect_.height;

            showVerticalScrollBar_ = totalContentHeight > availHeight;
            showHorizontalScrollBar_ = totalContentWidth > availWidth;

            float viewportWidth = availWidth - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = availHeight - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);

            if (!showHorizontalScrollBar_ && totalContentWidth > viewportWidth) {
                showHorizontalScrollBar_ = true;
                viewportHeight = availHeight - scrollBarWidth_;
            }
            if (!showVerticalScrollBar_ && totalContentHeight > viewportHeight) {
                showVerticalScrollBar_ = true;
                viewportWidth = availWidth - scrollBarWidth_;
            }

            maxScrollX_ = max(0.0f, totalContentWidth - viewportWidth);
            maxScrollY_ = max(0.0f, totalContentHeight - viewportHeight);

            scrollOffsetX_ = clamp(scrollOffsetX_, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(int row, int col) {
            if (row < 0 || col < 0) return;
            float rowTop = headerHeight_ + RowTop(row);
            float rowBottom = headerHeight_ + RowTop(row + 1);
            if (rowTop < scrollOffsetY_) {
                targetScrollOffsetY_ = rowTop;
            }
            else if (rowBottom > scrollOffsetY_ + arrangedRect_.height) {
                targetScrollOffsetY_ = rowBottom - arrangedRect_.height;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);

            float colX = 0;
            for (int i = 0; i < col; ++i) colX += GetEffectiveColumnWidth(i);
            float colWidth = GetEffectiveColumnWidth(col);
            if (colX < scrollOffsetX_) {
                targetScrollOffsetX_ = colX;
            }
            else if (colX + colWidth > scrollOffsetX_ + arrangedRect_.width) {
                targetScrollOffsetX_ = colX + colWidth - arrangedRect_.width;
            }
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
        }

        void HandleVerticalScrollDrag(float mouseY) {
            float trackY = arrangedRect_.y;
            float trackHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            if (trackHeight > thumbLength) {
                float ratio = (mouseY - dragStartMouseY_) / (trackHeight - thumbLength);
                targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                RequestRepaint();
            }
        }

        void HandleHorizontalScrollDrag(float mouseX) {
            float trackX = arrangedRect_.x;
            float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            if (trackWidth > thumbLength) {
                float ratio = (mouseX - dragStartMouseX_) / (trackWidth - thumbLength);
                targetScrollOffsetX_ = clamp(dragStartScrollX_ + ratio * maxScrollX_, 0.0f, maxScrollX_);
                RequestRepaint();
            }
        }

        void DrawHeader(ID2D1RenderTarget* rt, float viewportWidth) {
            if (!headerBgBrush_) rt->CreateSolidColorBrush(headerBackgroundColor_, headerBgBrush_.GetAddressOf());
            else headerBgBrush_->SetColor(headerBackgroundColor_);
            if (headerBgBrush_) {
                D2D1_RECT_F headerRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                    arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_);
                rt->FillRectangle(headerRect, headerBgBrush_.Get());
            }

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int col = 0; col < colCount_; ++col) {
                float colWidth = GetEffectiveColumnWidth(col);
                if (colWidth <= 0.0f) { colX += colWidth; continue; }
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    std::wstring headerText = (col < (int)headers_.size()) ? headers_[col] : L"";
                    if (!headerText.empty()) {
                        D2D1_RECT_F textRect = D2D1::RectF(colX + 8, arrangedRect_.y,
                            colX + colWidth - 8, arrangedRect_.y + headerHeight_);
                        DrawTextWithEllipsis(rt, headerText, textRect, headerTextColor_, spec, headerTextBrush_, fmt);
                    }
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    if (gridLineBrush_) {
                        rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y),
                            D2D1::Point2F(colX, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
                    }
                    if (showSortIndicator_ && col == sortColumn_) {
                        if (!sortIndicatorBrush_) rt->CreateSolidColorBrush(sortIndicatorColor_, sortIndicatorBrush_.GetAddressOf());
                        else sortIndicatorBrush_->SetColor(sortIndicatorColor_);
                        if (sortIndicatorBrush_ && fmt) {
                            std::wstring arrow = sortAscending_ ? L"\u25B2" : L"\u25BC";
                            D2D1_RECT_F ar = D2D1::RectF(colX + colWidth - 16, arrangedRect_.y,
                                colX + colWidth - 2, arrangedRect_.y + headerHeight_);
                            rt->DrawText(arrow.c_str(), (UINT32)arrow.length(), fmt, ar, sortIndicatorBrush_.Get());
                        }
                    }
                }
                colX += colWidth;
            }
            if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
            else gridLineBrush_->SetColor(gridLineColor_);
            if (gridLineBrush_) {
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y),
                    D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_),
                    gridLineBrush_.Get(), 1.0f);
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x, arrangedRect_.y + headerHeight_),
                    D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_),
                    gridLineBrush_.Get(), 1.0f);
            }
        }

        void DrawVerticalScrollBar(ID2D1RenderTarget* rt, float viewportHeight) {
            if (maxScrollY_ <= 0.0f) return;   // 无滚动量，避免除零
            float baseTrackWidth = scrollBarWidth_;
            float trackWidth = baseTrackWidth * (1.0f + 0.25f * verticalScrollHoverProgress_);
            float trackX = arrangedRect_.x + arrangedRect_.width - trackWidth;
            float trackY = arrangedRect_.y;
            float trackHeight = viewportHeight;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackWidth / 2, trackWidth / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = trackWidth - 2.0f;
            if (thumbWidth < 2.0f) thumbWidth = 2.0f;
            float thumbX = trackX + (trackWidth - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (verticalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * verticalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * verticalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * verticalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * verticalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                    thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        void DrawHorizontalScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            if (maxScrollX_ <= 0.0f) return;   // 无滚动量，避免除零
            float baseTrackHeight = scrollBarWidth_;
            float trackHeight = baseTrackHeight * (1.0f + 0.25f * horizontalScrollHoverProgress_);
            float trackY = arrangedRect_.y + arrangedRect_.height - trackHeight;
            float trackX = arrangedRect_.x;
            float trackWidth = viewportWidth;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackHeight / 2, trackHeight / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            float thumbPos = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
            float thumbHeight = trackHeight - 2.0f;
            if (thumbHeight < 2.0f) thumbHeight = 2.0f;
            float thumbY = trackY + (trackHeight - thumbHeight) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (horizontalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * horizontalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbPos, thumbY, thumbPos + thumbLength, thumbY + thumbHeight),
                    thumbHeight / 2, thumbHeight / 2), scrollThumbBrush_.Get());
        }

        void UpdateIndicatorTarget() {
            if (selectedRow_ >= 0 && selectedRow_ < rowCount_) {
                targetIndicatorY_ = RowTop(selectedRow_);
            }
            else {
                targetIndicatorY_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
        }

        int rowCount_, colCount_;
        std::unordered_set<int> disabledRows_;
        std::unordered_set<int> hiddenColumns_;
        std::unordered_map<int, TextHAlign> columnAlign_;
        std::unordered_map<int, float> rowHeights_;
        std::vector<float> rowTops_;
        std::unordered_map<long long, D2D1_COLOR_F> cellTextColors_;
        std::unordered_map<long long, std::wstring> cellTips_;
        std::unordered_map<int, std::function<bool(const std::wstring&, const std::wstring&)>> columnComparators_;
        int sortColumn_ = -1;
        bool sortAscending_ = true;
        bool showSortIndicator_ = false;
        D2D1_COLOR_F sortIndicatorColor_ = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        ComPtr<ID2D1SolidColorBrush> sortIndicatorBrush_;
        std::vector<std::vector<std::shared_ptr<Label>>> data_;
        std::vector<std::wstring> headers_;
        std::vector<float> columnWidths_;
        int selectedRow_, selectedCol_;
        int hoveredRow_, hoveredCol_;
        float scrollOffsetX_, targetScrollOffsetX_, maxScrollX_;
        float scrollOffsetY_, targetScrollOffsetY_, maxScrollY_;
        bool showVerticalScrollBar_, showHorizontalScrollBar_;
        bool isDraggingVertical_, isDraggingHorizontal_;
        float dragStartMouseX_, dragStartMouseY_;
        float dragStartScrollX_, dragStartScrollY_;
        bool isResizingColumn_;
        int resizeColumnIndex_;
        float resizeStartMouseX_, resizeStartColumnWidth_;
        float headerHeight_, rowHeight_;
        float indicatorWidth_;
        float indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_, targetIndicatorY_;
        float scrollBarWidth_, scrollBarMinLength_;
        float scrollWheelStep_, scrollAnimationSpeed_, hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_, headerBackgroundColor_;
        D2D1_COLOR_F textColor_, headerTextColor_;
        D2D1_COLOR_F selectedColor_, hoverColor_, gridLineColor_, borderColor_;
        D2D1_COLOR_F scrollTrackColor_, scrollThumbColor_, scrollHoverThumbColor_;
        SelectionMode selectionMode_;
        float verticalScrollHoverProgress_, horizontalScrollHoverProgress_;
        bool isVerticalHovered_, isHorizontalHovered_;
        // ---- 扩展状态 ----
        bool headerVisible_ = true;
        bool showGrid_ = true;
        bool alternatingRowColors_ = false;
        D2D1_COLOR_F alternateRowColor_ = D2D1::ColorF(0.97f, 0.97f, 0.97f, 1.0f);
        DWORD lastClickTick_ = 0;
        int lastClickRow_ = -1;
        int lastClickCol_ = -1;
        ComPtr<ID2D1SolidColorBrush> alternateBrush_;
        std::unordered_set<long long> cellSel_;
        bool itemsCheckable_ = false;
        std::unordered_set<int> checkedRows_;
        D2D1_COLOR_F checkBoxColor_ = CheckBox::DefaultBoxColor;
        D2D1_COLOR_F checkMarkColor_ = CheckBox::DefaultCheckColor;
        ComPtr<ID2D1SolidColorBrush> rowCheckBrush_;
        bool pressActive_ = false;
        bool marqueeActive_ = false;
        bool marqueeEnabled_ = true;
        bool marqueeCheckSync_ = false;
        float pressStartCX_ = 0, pressStartCY_ = 0, marqueeCurCX_ = 0, marqueeCurCY_ = 0;
        ComPtr<ID2D1SolidColorBrush> marqueeFillBrush_, marqueeBorderBrush_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> headerBgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> headerTextBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_;
        ComPtr<ID2D1SolidColorBrush> hoverBrush_;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
        ComPtr<ID2D1SolidColorBrush> gridLineBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollThumbBrush_;
    };

    // ==================== 树视图 TreeView（多列表格形式） ====================
    struct TreeNode {
        // 勾选状态（三态，参考 Qt::CheckState）
        enum class CheckState { Unchecked, PartiallyChecked, Checked };

        std::vector<std::shared_ptr<Label>> columns;        // Label 化：每列一个真 Label（文本/图标/内嵌控件由它自己画）
        TreeNode* parent = nullptr;
        std::vector<std::shared_ptr<TreeNode>> children;
        bool expanded = false;
        int depth = 0;
        void* userData = nullptr;

        // ---- 扩展字段 ----
        std::wstring icon;                                  // 第一列前置图标（一个字符或 emoji，可空）
        bool checkable = false;                             // 是否显示勾选框
        CheckState checkState = CheckState::Unchecked;      // 勾选状态（支持三态）
        bool selected = false;                              // 多选模式下的选中标记
        bool enabled = true;                                // 是否可用（置灰显示）
        std::wstring tooltip;                               // 悬停提示（可选，供上层使用）

        static std::vector<std::shared_ptr<Label>> MakeColumns(const std::vector<std::wstring>& cols) {
            std::vector<std::shared_ptr<Label>> v;
            v.reserve(cols.size());
            for (auto& s : cols) v.push_back(std::make_shared<Label>(s));
            return v;
        }
        TreeNode(const std::wstring& text) : columns(MakeColumns({ text })) {}
        TreeNode(const std::vector<std::wstring>& cols) : columns(MakeColumns(cols)) {}
    };

    class TreeView : public UIElement {
    public:
        // 默认样式
        inline static float DefaultRowHeight = 24.0f;
        inline static float DefaultIndent = 16.0f;
        inline static float DefaultHeaderHeight = 24.0f;
        inline static float DefaultMinColumnWidth = 40.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderBackgroundColor = D2D1::ColorF(0.93f, 0.93f, 0.93f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultGridLineColor = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 400.0f;
        inline static float DefaultHeight = 300.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;
        inline static float DefaultColumnResizeHitWidth = 8.0f;

        ZSignal<std::shared_ptr<TreeNode>> SelectionChanged;   // 选中节点变化
        ZSignal<std::shared_ptr<TreeNode>> NodeClicked;        // 节点点击
        ZSignal<std::shared_ptr<TreeNode>, bool> ExpandChanged; // 展开/折叠变化
        // ---- 扩展信号 ----
        ZSignal<std::shared_ptr<TreeNode>> ItemDoubleClicked;  // 双击节点
        ZSignal<std::shared_ptr<TreeNode>, bool> ItemRightClicked; // 右键节点（bool 为在首列上）
        ZSignal<int> HeaderClicked;                            // 点击表头（列索引）
        ZSignal<std::vector<std::shared_ptr<TreeNode>>> SelectionChangedMulti; // 多选集合变化
        ZSignal<std::shared_ptr<TreeNode>, TreeNode::CheckState> ItemCheckStateChanged; // 勾选变化

        // 选择模式（参考 Qt::SelectionMode）
        enum class SelectionMode { Single, Extended, Multi };

        TreeView()
            : scrollOffsetX_(0.0f), targetScrollOffsetX_(0.0f), maxScrollX_(0.0f),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showVerticalScrollBar_(false), showHorizontalScrollBar_(false),
            isDraggingVertical_(false), isDraggingHorizontal_(false),
            dragStartMouseX_(0.0f), dragStartMouseY_(0.0f),
            dragStartScrollX_(0.0f), dragStartScrollY_(0.0f),
            isResizingColumn_(false), resizeColumnIndex_(-1),
            resizeStartMouseX_(0.0f), resizeStartColumnWidth_(0.0f),
            selectedNode_(nullptr), hoveredNode_(nullptr),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            indicatorX_(0.0f), targetIndicatorX_(0.0f),
            rowHeight_(DefaultRowHeight),
            indent_(DefaultIndent),
            headerHeight_(DefaultHeaderHeight),
            headerVisible_(true),
            columnCount_(1),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            headerBackgroundColor_(DefaultHeaderBackgroundColor),
            textColor_(DefaultTextColor),
            headerTextColor_(DefaultHeaderTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            gridLineColor_(DefaultGridLineColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            verticalScrollHoverProgress_(0.0f),
            horizontalScrollHoverProgress_(0.0f),
            isVerticalHovered_(false),
            isHorizontalHovered_(false) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
            columnWidths_.push_back(DefaultMinColumnWidth);
            headerLabels_.push_back(L"名称");

            indicatorY_ = -1.0f;
            indicatorX_ = -1.0f;
        }

        // ---------- 列设置 ----------
        void SetColumnCount(int count) {
            count = max(1, count);
            columnCount_ = count;
            columnWidths_.assign(count, DefaultMinColumnWidth);
            headerLabels_.resize(count);
            std::function<void(std::shared_ptr<TreeNode>)> updateNode = [&](std::shared_ptr<TreeNode> node) {
                if (!node) return;
                node->columns.resize(count);
                for (auto& child : node->children) updateNode(child);
                };
            for (auto& root : roots_) updateNode(root);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetHeaderLabels(const std::vector<std::wstring>& labels) {
            headerLabels_ = labels;
            if (headerLabels_.size() != columnCount_) headerLabels_.resize(columnCount_);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnWidth(int col, float width) {
            if (col < 0 || col >= columnCount_) return;
            columnWidths_[col] = max(DefaultMinColumnWidth, width);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        float GetColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            return columnWidths_[col];
        }

        // ---------- 节点操作 ----------
        std::shared_ptr<TreeNode> AddRoot(const std::wstring& text) {
            auto node = std::make_shared<TreeNode>(text);
            node->columns.resize(columnCount_);
            node->depth = 0;
            node->checkable = checkableMode_;
            roots_.push_back(node);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return node;
        }
        std::shared_ptr<TreeNode> AddRoot(const std::vector<std::wstring>& columns) {
            auto node = std::make_shared<TreeNode>(columns);
            node->columns.resize(columnCount_);
            node->depth = 0;
            node->checkable = checkableMode_;
            roots_.push_back(node);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return node;
        }
        std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::wstring& text) {
            if (!parent) return nullptr;
            auto child = std::make_shared<TreeNode>(text);
            child->columns.resize(columnCount_);
            child->parent = parent.get();
            child->depth = parent->depth + 1;
            child->checkable = checkableMode_ || parent->checkable;
            parent->children.push_back(child);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return child;
        }
        std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::vector<std::wstring>& columns) {
            if (!parent) return nullptr;
            auto child = std::make_shared<TreeNode>(columns);
            child->columns.resize(columnCount_);
            child->parent = parent.get();
            child->depth = parent->depth + 1;
            child->checkable = checkableMode_ || parent->checkable;
            parent->children.push_back(child);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return child;
        }

        void RemoveNode(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            EraseCheckAnim(node);
            if (node->parent) {
                auto& siblings = node->parent->children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
            }
            else {
                roots_.erase(std::remove(roots_.begin(), roots_.end(), node), roots_.end());
            }
            if (selectedNode_ == node) selectedNode_ = nullptr;
            if (hoveredNode_ == node) hoveredNode_ = nullptr;
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }

        void Clear() {
            roots_.clear();
            visibleNodes_.clear();
            checkAnim_.clear();
            selectedNode_ = nullptr;
            hoveredNode_ = nullptr;
            scrollOffsetX_ = 0.0f;
            scrollOffsetY_ = 0.0f;
            targetScrollOffsetX_ = 0.0f;
            targetScrollOffsetY_ = 0.0f;
            maxScrollX_ = 0.0f;
            maxScrollY_ = 0.0f;
            InvalidateLayout();
            RequestRepaint();
        }

        // ---------- 展开/折叠 ----------
        void ExpandNode(std::shared_ptr<TreeNode> node, bool expand) {
            if (!node) return;
            if (node->expanded != expand) {
                node->expanded = expand;
                BuildVisibleList();
                ExpandChanged(node, expand);
                InvalidateLayout();
                RequestRepaint();
            }
        }
        void ToggleNode(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            ExpandNode(node, !node->expanded);
        }
        void ExpandNodeRecursive(std::shared_ptr<TreeNode> node, bool expand) {
            if (!node) return;
            if (node->expanded != expand) node->expanded = expand;
            for (auto& child : node->children) {
                ExpandNodeRecursive(child, expand);
            }
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }
        bool IsExpanded(std::shared_ptr<TreeNode> node) const {
            return node ? node->expanded : false;
        }

        // ---------- 选择 ----------
        void SetSelectedNode(std::shared_ptr<TreeNode> node) {
            if (selectionMode_ == SelectionMode::Single) {
                if (selectedNode_ != node) {
                    for (auto& r : roots_) SetSelectedRecursive(r, false);
                    if (node) node->selected = true;
                    selectedNode_ = node;
                    SelectionChanged(selectedNode_);
                    EnsureVisible(node);
                    UpdateIndicatorTarget();
                    InvalidateLayout();
                    RequestRepaint();
                }
            }
            else {
                if (selectedNode_ == node) return;   // 相同节点不重复触发 SelectionChanged
                if (node) node->selected = true;
                selectedNode_ = node;
                SelectionChanged(selectedNode_);
                EmitMultiSelection();
                EnsureVisible(node);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        std::shared_ptr<TreeNode> GetSelectedNode() const { return selectedNode_; }

        // ==================== 扩展：选择模式与多选 ====================
        void SetSelectionMode(SelectionMode mode) {
            selectionMode_ = mode;
            ClearSelection();
        }
        SelectionMode GetSelectionMode() const { return selectionMode_; }

        void ClearSelection() {
            for (auto& r : roots_) SetSelectedRecursive(r, false);
            selectedNode_ = nullptr;
            SelectionChanged(nullptr);
            EmitMultiSelection();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        bool IsNodeSelected(std::shared_ptr<TreeNode> node) const { return node && node->selected; }
        std::vector<std::shared_ptr<TreeNode>> GetSelectedNodes() const {
            std::vector<std::shared_ptr<TreeNode>> out;
            for (auto& r : roots_) CollectSelected(r, out);
            return out;
        }
        // 多选/勾选的数组形式：索引数组 + 每项状态数组（可见节点顺序）
        std::vector<int> GetSelectedIndices() const {
            std::vector<int> v;
            for (int i = 0; i < (int)visibleNodes_.size(); ++i)
                if (visibleNodes_[i] && visibleNodes_[i]->selected) v.push_back(i);
            return v;
        }
        std::vector<bool> GetSelectionStates() const {
            std::vector<bool> v;
            v.reserve(visibleNodes_.size());
            for (auto& n : visibleNodes_) v.push_back(n ? n->selected : false);
            return v;
        }
        std::vector<bool> GetCheckStates() const {
            std::vector<bool> v;
            v.reserve(visibleNodes_.size());
            for (auto& n : visibleNodes_) v.push_back(n ? (n->checkState != TreeNode::CheckState::Unchecked) : false);
            return v;
        }
        std::vector<std::shared_ptr<TreeNode>> GetCheckedNodes() const {
            std::vector<std::shared_ptr<TreeNode>> out;
            for (auto& r : roots_) CollectChecked(r, out);
            return out;
        }
        int GetSelectedCount() const { return (int)GetSelectedNodes().size(); }
        void SelectAll() {
            if (selectionMode_ == SelectionMode::Single) return;
            for (auto& r : roots_) SetSelectedRecursive(r, true);
            EmitMultiSelection();
            InvalidateLayout();
            RequestRepaint();
        }
        void ToggleNodeSelection(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            node->selected = !node->selected;
            selectedNode_ = node;
            EmitMultiSelection();
            RequestRepaint();
        }

        // ==================== 扩展：三态勾选 ====================
        void SetNodeCheckable(std::shared_ptr<TreeNode> node, bool checkable, bool recursive = false) {
            if (!node) return;
            node->checkable = checkable;
            if (!checkable) node->checkState = TreeNode::CheckState::Unchecked;
            if (recursive) for (auto& c : node->children) SetNodeCheckable(c, checkable, true);
            RequestRepaint();
        }
        bool IsNodeCheckable(std::shared_ptr<TreeNode> node) const { return node && node->checkable; }
        TreeNode::CheckState GetNodeCheckState(std::shared_ptr<TreeNode> node) const {
            return node ? node->checkState : TreeNode::CheckState::Unchecked;
        }
        void SetNodeCheckState(std::shared_ptr<TreeNode> node, TreeNode::CheckState state,
                               bool updateChildren = true, bool updateParent = true) {
            if (!node) return;
            if (checkMode_ == CheckMode::Independent) { updateChildren = false; updateParent = false; state = (state == TreeNode::CheckState::Unchecked) ? TreeNode::CheckState::Unchecked : TreeNode::CheckState::Checked; }
            node->checkState = state;
            EnsureCheckAnim(node);
            if (updateChildren && state != TreeNode::CheckState::PartiallyChecked) {
                for (auto& c : node->children) SetNodeCheckState(c, state, true, false);
            }
            if (updateParent) UpdateParentCheckState(node);
            ItemCheckStateChanged(node, node->checkState);
            RequestRepaint();
        }
        bool IsNodeChecked(std::shared_ptr<TreeNode> node) const {
            return node && node->checkState == TreeNode::CheckState::Checked;
        }
        // 视图级开关：让所有节点（含之后新增的）都显示勾选框
        void SetCheckable(bool enable, bool recursive = true) {
            checkableMode_ = enable;
            if (recursive) {
                std::function<void(std::shared_ptr<TreeNode>)> f = [&](std::shared_ptr<TreeNode> n) {
                    if (!n) return;
                    n->checkable = enable;
                    if (!enable) n->checkState = TreeNode::CheckState::Unchecked;
                    for (auto& c : n->children) f(c);
                    };
                for (auto& r : roots_) f(r);
            }
            RequestRepaint();
        }
        bool IsCheckable() const { return checkableMode_; }

        // 勾选关联模式：Linked=父子三态联动；Independent=每项独立（仅选中/未选，适合进程列表等）
        enum class CheckMode { Linked, Independent };
        void SetCheckMode(CheckMode m) {
            checkMode_ = m;
            if (m == CheckMode::Independent) {
                std::function<void(std::shared_ptr<TreeNode>)> f = [&](std::shared_ptr<TreeNode> n) {
                    if (!n) return;
                    if (n->checkState == TreeNode::CheckState::PartiallyChecked)
                        n->checkState = TreeNode::CheckState::Unchecked;
                    for (auto& c : n->children) f(c);
                    };
                for (auto& r : roots_) f(r);
            }
            RequestRepaint();
        }
        CheckMode GetCheckMode() const { return checkMode_; }
        void SetCheckBoxColor(Color c) { checkBoxColor_ = c.ToD2D(); RequestRepaint(); }
        void SetCheckMarkColor(Color c) { checkMarkColor_ = c.ToD2D(); RequestRepaint(); }
        void SetNodeEnabled(std::shared_ptr<TreeNode> node, bool enabled, bool recursive = false) {
            if (!node) return;
            node->enabled = enabled;
            if (recursive) for (auto& c : node->children) SetNodeEnabled(c, enabled, true);
            RequestRepaint();
        }

        // ==================== 扩展：插入 / 移动 / 排序 / 删除 ====================
        std::shared_ptr<TreeNode> InsertChild(std::shared_ptr<TreeNode> parent, int index,
                                              const std::vector<std::wstring>& columns) {
            if (!parent) return nullptr;
            auto child = std::make_shared<TreeNode>(columns);
            child->columns.resize(columnCount_);
            child->parent = parent.get();
            child->depth = parent->depth + 1;
            child->checkable = checkableMode_ || parent->checkable;
            if (defaultExpandDepth_ >= 0 && child->depth < defaultExpandDepth_) child->expanded = true;
            if (index < 0 || index > (int)parent->children.size()) index = (int)parent->children.size();
            parent->children.insert(parent->children.begin() + index, child);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return child;
        }
        std::shared_ptr<TreeNode> InsertChild(std::shared_ptr<TreeNode> parent, int index, const std::wstring& text) {
            return InsertChild(parent, index, std::vector<std::wstring>{ text });
        }
        std::shared_ptr<TreeNode> InsertRoot(int index, const std::vector<std::wstring>& columns) {
            auto node = std::make_shared<TreeNode>(columns);
            node->columns.resize(columnCount_);
            node->depth = 0;
            node->checkable = checkableMode_;
            if (defaultExpandDepth_ >= 0 && node->depth < defaultExpandDepth_) node->expanded = true;
            if (index < 0 || index > (int)roots_.size()) index = (int)roots_.size();
            roots_.insert(roots_.begin() + index, node);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return node;
        }
        int IndexOfNode(std::shared_ptr<TreeNode> node) const {
            if (!node) return -1;
            const auto& siblings = node->parent ? node->parent->children : roots_;
            for (int i = 0; i < (int)siblings.size(); ++i) if (siblings[i] == node) return i;
            return -1;
        }
        bool MoveNode(std::shared_ptr<TreeNode> node, std::shared_ptr<TreeNode> newParent, int index) {
            if (!node) return false;
            if (newParent && IsAncestorOf(node, newParent)) return false;   // 不能移到自己的后代里
            if (node->parent) {
                auto& s = node->parent->children;
                s.erase(std::remove(s.begin(), s.end(), node), s.end());
            }
            else {
                roots_.erase(std::remove(roots_.begin(), roots_.end(), node), roots_.end());
            }
            node->parent = newParent.get();
            if (newParent) {
                if (index < 0 || index > (int)newParent->children.size()) index = (int)newParent->children.size();
                newParent->children.insert(newParent->children.begin() + index, node);
            }
            else {
                if (index < 0 || index > (int)roots_.size()) index = (int)roots_.size();
                roots_.insert(roots_.begin() + index, node);
            }
            UpdateDepthRecursive(node, newParent ? newParent->depth + 1 : 0);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return true;
        }
        void RemoveChildren(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            for (auto& c : node->children) EraseCheckAnim(c);   // 先清理勾选动画缓存，避免留下悬垂 TreeNode*
            node->children.clear();
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }
        // cmp 返回 true 表示 a 应排在 b 之前
        void SortChildren(std::shared_ptr<TreeNode> parent, bool recursive,
                          std::function<bool(const std::shared_ptr<TreeNode>&, const std::shared_ptr<TreeNode>&)> cmp) {
            std::function<void(std::vector<std::shared_ptr<TreeNode>>&)> doSort =
                [&](std::vector<std::shared_ptr<TreeNode>>& list) {
                std::stable_sort(list.begin(), list.end(), cmp);
                if (recursive) for (auto& n : list) { doSort(n->children); UpdateDepthRecursive(n, n->parent ? n->parent->depth + 1 : 0); }
                };
            if (parent) doSort(parent->children);
            else doSort(roots_);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }

        // ==================== 扩展：展开辅助 ====================
        void ExpandAll() { for (auto& r : roots_) SetExpandedRecursive(r, true); BuildVisibleList(); InvalidateLayout(); RequestRepaint(); }
        void CollapseAll() { for (auto& r : roots_) SetExpandedRecursive(r, false); BuildVisibleList(); InvalidateLayout(); RequestRepaint(); }
        void ExpandToDepth(int depth) {
            std::function<void(std::shared_ptr<TreeNode>)> f = [&](std::shared_ptr<TreeNode> n) {
                if (!n) return;
                n->expanded = (n->depth < depth);
                for (auto& c : n->children) f(c);
                };
            for (auto& r : roots_) f(r);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }

        // ==================== 扩展：过滤 / 搜索 / 路径 ====================
        // 谓词返回 true 的节点及其祖先链保留显示
        void SetFilter(std::function<bool(const std::shared_ptr<TreeNode>&)> filter) {
            filter_ = std::move(filter);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }
        void ClearFilter() { filter_ = nullptr; BuildVisibleList(); InvalidateLayout(); RequestRepaint(); }
        bool HasFilter() const { return (bool)filter_; }
        // 文本搜索：任一列包含关键字即保留
        void Search(const std::wstring& keyword) {
            if (keyword.empty()) { ClearFilter(); return; }
            SetFilter([keyword](const std::shared_ptr<TreeNode>& n) {
                if (!n) return false;
                for (auto& c : n->columns) if (c && c->GetText().find(keyword) != std::wstring::npos) return true;
                return false;
                });
        }
        // 默认展开深度：小于该深度的节点在插入时自动展开
        void SetDefaultExpandDepth(int depth) { defaultExpandDepth_ = depth; }
        int GetDefaultExpandDepth() const { return defaultExpandDepth_; }
        // 节点路径（默认用第一列文本拼接）
        std::wstring GetNodePath(const std::shared_ptr<TreeNode>& node, const std::wstring& separator = L" / ") const {
            std::vector<std::wstring> parts;
            TreeNode* cur = node.get();
            while (cur) { parts.push_back((cur->columns.empty() || !cur->columns[0]) ? L"" : cur->columns[0]->GetText()); cur = cur->parent; }
            std::reverse(parts.begin(), parts.end());
            std::wstring out;
            for (size_t i = 0; i < parts.size(); ++i) { if (i) out += separator; out += parts[i]; }
            return out;
        }

        // ==================== 扩展：查询 ====================
        int GetRootCount() const { return (int)roots_.size(); }
        std::shared_ptr<TreeNode> GetRootAt(int i) const { return (i >= 0 && i < (int)roots_.size()) ? roots_[i] : nullptr; }
        int GetVisibleNodeCount() const { return (int)visibleNodes_.size(); }
        std::shared_ptr<TreeNode> GetVisibleNodeAt(int i) const { return (i >= 0 && i < (int)visibleNodes_.size()) ? visibleNodes_[i] : nullptr; }
        int GetTotalNodeCount() const { int c = 0; for (auto& r : roots_) c += CountRecursive(r); return c; }
        std::shared_ptr<TreeNode> GetNodeAtY(float y) const {
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            int idx = (int)((y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_)) / rowHeight_);
            if (idx >= 0 && idx < (int)visibleNodes_.size()) return visibleNodes_[idx];
            return nullptr;
        }
        std::vector<std::shared_ptr<TreeNode>> GetVisibleNodes() const { return visibleNodes_; }

        // ==================== 扩展：显示选项 ====================
        void SetAlternatingRowColors(bool enable) { alternatingRowColors_ = enable; RequestRepaint(); }
        bool GetAlternatingRowColors() const { return alternatingRowColors_; }
        void SetAlternatingRowColor(Color color) { alternateRowColor_ = color.ToD2D(); alternateBrush_.Reset(); RequestRepaint(); }
        void SetGridVisible(bool visible) { showGrid_ = visible; RequestRepaint(); }
        bool IsGridVisible() const { return showGrid_; }
        void SetRootDecorated(bool decorated) { rootDecorated_ = decorated; InvalidateLayout(); RequestRepaint(); }
        bool IsRootDecorated() const { return rootDecorated_; }
        void SetSortingEnabled(bool enable) { sortingEnabled_ = enable; }
        bool IsSortingEnabled() const { return sortingEnabled_; }
        // 框选开关 / 框选与勾选同步
        void SetMarqueeEnabled(bool enable) { marqueeEnabled_ = enable; if (!enable) { marqueeActive_ = false; pressActive_ = false; } RequestRepaint(); }
        bool IsMarqueeEnabled() const { return marqueeEnabled_; }
        void SetMarqueeCheckSync(bool enable) { marqueeCheckSync_ = enable; }
        bool IsMarqueeCheckSync() const { return marqueeCheckSync_; }
        int GetSortColumn() const { return sortColumn_; }
        bool IsSortAscending() const { return sortAscending_; }
        // 手工设置排序指示（实际排序请用 SortChildren 或外部排序）
        void SetSortIndicator(int col, bool ascending) { sortColumn_ = col; sortAscending_ = ascending; RequestRepaint(); }
        void ClearSortIndicator() { sortColumn_ = -1; RequestRepaint(); }
        void SetNodeIcon(std::shared_ptr<TreeNode> node, const std::wstring& icon) { if (node) { node->icon = icon; RequestRepaint(); } }
        bool IsNodeEnabled(std::shared_ptr<TreeNode> node) const { return node ? node->enabled : false; }

        void ScrollToNode(std::shared_ptr<TreeNode> node) {
            int idx = GetVisibleIndex(node);
            if (idx < 0) return;
            float itemTop = idx * rowHeight_;
            float itemBottom = itemTop + rowHeight_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float viewportHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + viewportHeight) {
                targetScrollOffsetY_ = itemBottom - viewportHeight;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
            RequestRepaint();
        }

        // ---------- 表头设置 ----------
        void SetHeaderVisible(bool visible) { headerVisible_ = visible; InvalidateLayout(); RequestRepaint(); }

        // ---------- 样式设置 ----------
        void SetRowHeight(float height) { rowHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        void SetIndent(float indent) { indent_ = indent; InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }
        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetHeaderBackgroundColor(Color color) { headerBackgroundColor_ = color.ToD2D(); headerBgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) { textColor_ = color.ToD2D(); textBrush_.Reset(); RequestRepaint(); }
        void SetHeaderTextColor(Color color) { headerTextColor_ = color.ToD2D(); headerTextBrush_.Reset(); RequestRepaint(); }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetGridLineColor(Color color) { gridLineColor_ = color.ToD2D(); gridLineBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // ---------- UIElement 接口 ----------
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size MeasureOverride(const Size& availableSize) override {
            return Size(width_, height_);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            childrenDirty_ = true;              // 重排过 → 可见节点 Label 列表必须重建
        }

        // Label 化：节点单元格 Label 的统一初始化（字体 / 每节点文本色 / 内边距，关自身缓存）
        void PrepareNodeCellLabel(const std::shared_ptr<Label>& lb, const std::shared_ptr<TreeNode>& node) const {
            if (!lb) return;
            lb->SetUseCache(false);
            FontSpec spec = GetEffectiveFontSpec();
            if (!(lb->GetEffectiveFontSpec() == spec)) lb->SetFont(spec);
            if (lb->GetHorizontalAlignment() != Label::HAlign::Left ||
                lb->GetVerticalAlignment() != Label::VAlign::Center)
                lb->SetAlignment(Label::HAlign::Left, Label::VAlign::Center);
            lb->SetPadding(0.0f);
            Color cur = lb->GetTextColor();
            Color want = Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a);
            if (node && !node->enabled) want = Color(0.6f, 0.6f, 0.6f, textColor_.a);
            if (cur.r != want.r || cur.g != want.g || cur.b != want.b || cur.a != want.a) lb->SetTextColor(want);
        }

        // Label 化：可见节点的单元格 Label 作为子元素进入 Window 合成/事件/裁剪流程（就地摆到滚动后的位置）
        const std::vector<UIElement*>& GetChildren() const override {
            // 命中缓存要覆盖所有影响单元格布局的几何（滚动量/列宽和/行高/缩进/列数/可见节点数/控件矩形）；
            // SetColumnWidth/SetRowHeight/SetIndent/BuildVisibleList 等不一定触发 ArrangeOverride。
            float sumW = 0.0f;
            for (int c = 0; c < columnCount_; ++c) sumW += GetEffectiveColumnWidth(c);
            float sx = Snap(scrollOffsetX_), sy = Snap(scrollOffsetY_);
            if (!childrenDirty_ &&
                sx == lastScrollX_ && sy == lastScrollY_ && sumW == lastSumW_ &&
                rowHeight_ == lastRowHeight_ && columnCount_ == lastColCount_ && indent_ == lastIndent_ &&
                (int)visibleNodes_.size() == lastVisibleCount_ &&
                arrangedRect_.x == lastArrX_ && arrangedRect_.y == lastArrY_ &&
                arrangedRect_.width == lastArrW_ && arrangedRect_.height == lastArrH_)
                return childrenView_;
            childrenDirty_ = false;
            lastScrollX_ = sx; lastScrollY_ = sy; lastSumW_ = sumW;
            lastRowHeight_ = rowHeight_; lastColCount_ = columnCount_; lastIndent_ = indent_;
            lastVisibleCount_ = (int)visibleNodes_.size();
            lastArrX_ = arrangedRect_.x; lastArrY_ = arrangedRect_.y;
            lastArrW_ = arrangedRect_.width; lastArrH_ = arrangedRect_.height;
            childrenView_.clear();
            if (visibleNodes_.empty() || columnCount_ <= 0 || rowHeight_ <= 0.0f) return childrenView_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0.0f;
            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            Rect contentClip(arrangedRect_.x, arrangedRect_.y + headerOffset, viewportWidth, viewportHeight - headerOffset);
            int firstVisible = (int)(sy / rowHeight_);
            int lastVisible = (int)((sy + viewportHeight - headerOffset) / rowHeight_);
            if (firstVisible < 0) firstVisible = 0;
            if (lastVisible > (int)visibleNodes_.size() - 1) lastVisible = (int)visibleNodes_.size() - 1;
            const float bleedY = 3.0f;
            float colX = arrangedRect_.x - sx;
            for (int c = 0; c < columnCount_; ++c) {
                float colWidth = GetEffectiveColumnWidth(c);
                if (colWidth <= 0.0f) { colX += colWidth; continue; }
                bool colVisible = (colX + colWidth >= arrangedRect_.x) && (colX <= arrangedRect_.x + viewportWidth);
                if (colVisible) {
                    for (int i = firstVisible; i <= lastVisible; ++i) {
                        auto node = visibleNodes_[i];
                        if (!node || c >= (int)node->columns.size()) continue;
                        auto lb = node->columns[c];
                        if (!lb) continue;
                        PrepareNodeCellLabel(lb, node);
                        float itemY = arrangedRect_.y + headerOffset + i * rowHeight_ - sy;
                        // 第一列文字左起点必须与 Draw 里 xCursor 完全一致：缩进 + 指示条 + 勾选框 + 图标
                        float textLeft = colX + 8.0f;
                        if (c == 0) {
                            textLeft = GetNodeTextStartX(node) + 2.0f + indicatorWidth_ + 4.0f;
                            if (node->checkable) textLeft += 15.0f + 6.0f;
                            if (!node->icon.empty()) textLeft += 20.0f;
                        }
                        float availW = (colX + colWidth - 8.0f) - textLeft;
                        if (availW < 0.0f) availW = 0.0f;
                        lb->Arrange(Rect(textLeft, itemY - bleedY, availW, rowHeight_ + bleedY * 2.0f));
                        // 裁到内容视口：不压表头 / 不画到横向滚动条 / 横向拖动不漫出左右边界
                        lb->SetClipRect(contentClip);
                        childrenView_.push_back(lb.get());
                    }
                }
                colX += colWidth;
            }
            return childrenView_;
        }
        std::optional<D2D1_RECT_F> GetClipRect() const override {
            return D2D1::RectF(arrangedRect_.x - 2.0f, arrangedRect_.y - 2.0f,
                arrangedRect_.x + arrangedRect_.width + 2.0f, arrangedRect_.y + arrangedRect_.height + 2.0f);
        }
        mutable float lastScrollX_ = -1e30f, lastScrollY_ = -1e30f, lastSumW_ = -1e30f, lastRowHeight_ = -1e30f, lastIndent_ = -1e30f;
        mutable float lastArrX_ = -1e30f, lastArrY_ = -1e30f, lastArrW_ = -1e30f, lastArrH_ = -1e30f;
        mutable int lastColCount_ = -1, lastVisibleCount_ = -1;

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(backgroundColor_);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float headerOffset = headerVisible_ ? headerHeight_ : 0;

            D2D1_RECT_F clipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y + headerOffset,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + viewportHeight);
            rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            int firstVisible = (int)(scrollOffsetY_ / rowHeight_);
            int lastVisible = (int)((scrollOffsetY_ + viewportHeight - headerOffset) / rowHeight_);
            lastVisible = min(lastVisible, (int)visibleNodes_.size() - 1);
            if (firstVisible < 0) firstVisible = 0;

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            std::vector<float> colXPositions(columnCount_);
            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int c = 0; c < columnCount_; ++c) {
                colXPositions[c] = colX;
                colX += GetEffectiveColumnWidth(c);
            }

            for (int i = firstVisible; i <= lastVisible && i < (int)visibleNodes_.size(); ++i) {
                auto node = visibleNodes_[i];
                if (!node) continue;

                float itemY = arrangedRect_.y + headerOffset + i * rowHeight_ - Snap(scrollOffsetY_);
                D2D1_RECT_F rowRect = D2D1::RectF(arrangedRect_.x, itemY,
                    arrangedRect_.x + viewportWidth, itemY + rowHeight_);

                bool isSel = node->selected || node == selectedNode_;

                if (isSel) {
                    if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                    rt->FillRectangle(rowRect, selectedBrush_.Get());
                }
                else if (node == hoveredNode_) {
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                    rt->FillRectangle(rowRect, hoverBrush_.Get());
                }
                else if (alternatingRowColors_ && (i & 1)) {
                    if (!alternateBrush_) rt->CreateSolidColorBrush(alternateRowColor_, alternateBrush_.GetAddressOf());
                    rt->FillRectangle(rowRect, alternateBrush_.Get());
                }

                D2D1_COLOR_F effTextColor = node->enabled ? textColor_ : D2D1::ColorF(0.6f, 0.6f, 0.6f, textColor_.a);

                for (int c = 0; c < columnCount_; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float cellX = colXPositions[c];
                    if (cellX + colWidth < arrangedRect_.x || cellX > arrangedRect_.x + viewportWidth) continue;

                    D2D1_RECT_F cellRect = D2D1::RectF(cellX, itemY, cellX + colWidth, itemY + rowHeight_);
                    if (c == 0) {
                        float indentX = GetNodeTextStartX(node);

                        // 展开/折叠箭头
                        if (!node->children.empty() && (node->depth > 0 || rootDecorated_)) {
                            std::wstring arrow = node->expanded ? L"\u25BC" : L"\u25B6";
                            if (!textBrush_) rt->CreateSolidColorBrush(effTextColor, textBrush_.GetAddressOf());
                            else textBrush_->SetColor(effTextColor);
                            if (textBrush_ && fmt) {
                                D2D1_RECT_F arrowRect = D2D1::RectF(indentX - 12.0f, cellRect.top, indentX, cellRect.bottom);
                                rt->DrawText(arrow.c_str(), (UINT32)arrow.length(), fmt, arrowRect, textBrush_.Get());
                            }
                        }

                        // 当前节点指示条
                        if (node == selectedNode_) {
                            float barX = indicatorX_;
                            float barTop = cellRect.top + (cellRect.bottom - cellRect.top) * (1.0f - indicatorHeightRatio_) / 2.0f;
                            float barBottom = barTop + (cellRect.bottom - cellRect.top) * indicatorHeightRatio_;
                            if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                            else indicatorBrush_->SetColor(indicatorColor_);
                            D2D1_RECT_F indicatorRect = D2D1::RectF(barX, barTop, barX + indicatorWidth_, barBottom);
                            rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                        }

                        float xCursor = indentX + 2.0f + indicatorWidth_ + 4.0f;

                        // 勾选框（复用 CheckBox 控件：蓝底渐显 + 对勾左→右绘制）
                        if (node->checkable) {
                            float size = 15.0f;
                            float cy = (cellRect.top + cellRect.bottom) / 2.0f;
                            D2D1_RECT_F cbRect = D2D1::RectF(xCursor, cy - size / 2.0f, xCursor + size, cy + size / 2.0f);
                            CheckBox::State cs = (node->checkState == TreeNode::CheckState::Checked) ? CheckBox::State::Checked
                                : (node->checkState == TreeNode::CheckState::PartiallyChecked) ? CheckBox::State::PartiallyChecked
                                : CheckBox::State::Unchecked;
                            CheckBox::DrawBox(rt, cbRect, GetCheckAnim(node), cs,
                                checkBoxColor_, checkMarkColor_, gridLineColor_, 4.0f, checkboxBrush_);
                            xCursor += size + 6.0f;
                        }

                        // 前置图标
                        if (!node->icon.empty() && fmt) {
                            if (!iconBrush_) rt->CreateSolidColorBrush(effTextColor, iconBrush_.GetAddressOf());
                            else iconBrush_->SetColor(effTextColor);
                            D2D1_RECT_F iconRect = D2D1::RectF(xCursor, cellRect.top, xCursor + 18.0f, cellRect.bottom);
                            rt->DrawText(node->icon.c_str(), (UINT32)node->icon.length(), fmt, iconRect, iconBrush_.Get());
                            xCursor += 20.0f;
                        }

                        // Label 化：第一列文本由 node->columns[0] 自己画（对齐/位置见 GetChildren）
                    }
                    else {
                        // Label 化：其它列文本由 node->columns[c] 自己画（对齐/位置见 GetChildren）
                    }
                }

            }

            // 网格线最后统一绘制，避免被深浅行/选中背景覆盖
            if (showGrid_) {
                if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                else gridLineBrush_->SetColor(gridLineColor_);
                if (gridLineBrush_) {
                    for (int c = 0; c < columnCount_; ++c) {
                        float gx = colXPositions[c];
                        if (gx >= arrangedRect_.x && gx <= arrangedRect_.x + viewportWidth)
                            rt->DrawLine(D2D1::Point2F(gx, arrangedRect_.y + headerOffset),
                                D2D1::Point2F(gx, arrangedRect_.y + viewportHeight), gridLineBrush_.Get(), 1.0f);
                    }
                    for (int i = firstVisible; i <= lastVisible && i < (int)visibleNodes_.size(); ++i) {
                        float ly = arrangedRect_.y + headerOffset + (i + 1) * rowHeight_ - Snap(scrollOffsetY_);
                        rt->DrawLine(D2D1::Point2F(arrangedRect_.x, ly),
                            D2D1::Point2F(arrangedRect_.x + viewportWidth, ly), gridLineBrush_.Get(), 1.0f);
                    }
                }
            }

            if (marqueeActive_) {
                float vx0 = arrangedRect_.x + (min(pressStartCX_, marqueeCurCX_) - Snap(scrollOffsetX_));
                float vx1 = arrangedRect_.x + (max(pressStartCX_, marqueeCurCX_) - Snap(scrollOffsetX_));
                float vy0 = arrangedRect_.y + headerOffset + (min(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_));
                float vy1 = arrangedRect_.y + headerOffset + (max(pressStartCY_, marqueeCurCY_) - Snap(scrollOffsetY_));
                if (!marqueeFillBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.55f, 0.90f, 0.18f), marqueeFillBrush_.GetAddressOf());
                if (!marqueeBorderBrush_) rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.95f), marqueeBorderBrush_.GetAddressOf());
                D2D1_RECT_F mr = D2D1::RectF(vx0, vy0, vx1, vy1);
                if (marqueeFillBrush_) rt->FillRectangle(mr, marqueeFillBrush_.Get());
                if (marqueeBorderBrush_) rt->DrawRectangle(mr, marqueeBorderBrush_.Get(), 2.0f);
            }

            rt->PopAxisAlignedClip();

            if (headerVisible_) DrawHeader(rt, viewportWidth);   // 表头在内容之后绘制，避免被行覆盖

            if (showVerticalScrollBar_) DrawVerticalScrollBar(rt, viewportHeight, headerOffset);
            if (showHorizontalScrollBar_) DrawHorizontalScrollBar(rt, viewportWidth);

            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);

            // 表头分隔线最后重画，确保不被行/选中背景覆盖
            if (headerVisible_) {
                float sepY = Snap(arrangedRect_.y + headerOffset) + 0.5f;
                if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                else gridLineBrush_->SetColor(gridLineColor_);
                if (gridLineBrush_)
                    rt->DrawLine(D2D1::Point2F(arrangedRect_.x, sepY),
                        D2D1::Point2F(arrangedRect_.x + viewportWidth, sepY), gridLineBrush_.Get(), 1.0f);
            }
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) return this;
                if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) return this;
                return this;
            }
            return nullptr;
        }

        void OnMouseMove(float x, float y) override {
            MouseMove.Fire(x, y);
            if (isResizingColumn_) {
                float dx = x - resizeStartMouseX_;
                float newWidth = max(DefaultMinColumnWidth, resizeStartColumnWidth_ + dx);
                SetColumnWidth(resizeColumnIndex_, newWidth);
                return;
            }
            if (isDraggingVertical_) {
                HandleVerticalScrollDrag(y);
                return;
            }
            if (isDraggingHorizontal_) {
                HandleHorizontalScrollDrag(x);
                return;
            }
            if (pressActive_ && marqueeEnabled_ && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                float ho = headerVisible_ ? headerHeight_ : 0;
                float cx = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float cy = y - arrangedRect_.y - ho + Snap(scrollOffsetY_);
                if (!marqueeActive_ && (fabs(cx - pressStartCX_) > 4.0f || fabs(cy - pressStartCY_) > 4.0f)) {
                    marqueeActive_ = true;
                    for (auto& r : roots_) SetSelectedRecursive(r, false);
                    selectedNode_ = nullptr;          // 框选出现后清除当前选中，避免歧义
                    UpdateIndicatorTarget();
                    EmitMultiSelection();
                }
                if (marqueeActive_) {
                    marqueeCurCX_ = cx;
                    marqueeCurCY_ = cy;
                    float top = arrangedRect_.y + ho;
                    float bottom = arrangedRect_.y + arrangedRect_.height;
                    if (y < top + 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - 14.0f, 0.0f, maxScrollY_);
                    else if (y > bottom - 10.0f) targetScrollOffsetY_ = clamp(targetScrollOffsetY_ + 14.0f, 0.0f, maxScrollY_);
                    ApplyMarqueeSelection();
                    RequestRepaint();
                    return;
                }
            }
            if (!arrangedRect_.Contains(x, y)) {
                hoveredNode_ = nullptr;
                isVerticalHovered_ = false;
                isHorizontalHovered_ = false;
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                RequestRepaint();
                return;
            }

            isVerticalHovered_ = (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            isHorizontalHovered_ = (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_);

            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                bool nearBoundary = false;
                for (int c = 0; c < columnCount_ - 1; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        nearBoundary = true;
                        break;
                    }
                    colX += colWidth;
                }
                if (nearBoundary) SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                else SetCursor(LoadCursor(nullptr, IDC_ARROW));
                hoveredNode_ = nullptr;
                SetToolTip(std::wstring());
                RequestRepaint();
                return;
            }
            else {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }

            float relY = y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_);
            int idx = (int)(relY / rowHeight_);
            if (idx >= 0 && idx < (int)visibleNodes_.size())
                hoveredNode_ = visibleNodes_[idx];
            else
                hoveredNode_ = nullptr;
            SetToolTip(hoveredNode_ ? hoveredNode_->tooltip : std::wstring());
            RequestRepaint();
        }

        void OnMouseDown(float x, float y) override {
            MouseDown.Fire(x, y);
            if (!arrangedRect_.Contains(x, y)) return;

            float headerOffset = headerVisible_ ? headerHeight_ : 0;

            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                for (int c = 0; c < columnCount_ - 1; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        isResizingColumn_ = true;
                        resizeColumnIndex_ = c;
                        resizeStartMouseX_ = x;
                        resizeStartColumnWidth_ = colWidth;
                        return;
                    }
                    colX += colWidth;
                }
                int hdrCol = GetColumnIndexAtX(relX);
                if (hdrCol >= 0 && hdrCol < columnCount_) {
                    HeaderClicked(hdrCol);
                    if (sortingEnabled_) ToggleSort(hdrCol);
                }
                return;
            }

            if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                float trackY = arrangedRect_.y + headerOffset;
                float trackHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                D2D1_RECT_F thumbRect = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - scrollBarWidth_, thumbY,
                    arrangedRect_.x + arrangedRect_.width, thumbY + thumbLength);
                if (y >= thumbRect.top && y <= thumbRect.bottom) {
                    isDraggingVertical_ = true;
                    dragStartMouseY_ = y;
                    dragStartScrollY_ = scrollOffsetY_;
                    return;
                }
                else {
                    float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                    return;
                }
            }

            if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) {
                float trackX = arrangedRect_.x;
                float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
                float thumbX = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
                D2D1_RECT_F thumbRect = D2D1::RectF(thumbX, arrangedRect_.y + arrangedRect_.height - scrollBarWidth_,
                    thumbX + thumbLength, arrangedRect_.y + arrangedRect_.height);
                if (x >= thumbRect.left && x <= thumbRect.right) {
                    isDraggingHorizontal_ = true;
                    dragStartMouseX_ = x;
                    dragStartScrollX_ = scrollOffsetX_;
                    return;
                }
                else {
                    float ratio = (x - trackX - thumbLength / 2) / (trackWidth - thumbLength);
                    targetScrollOffsetX_ = clamp(ratio * maxScrollX_, 0.0f, maxScrollX_);
                    RequestRepaint();
                    return;
                }
            }

            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) return;

            float relY = y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_);
            int idx = (int)(relY / rowHeight_);
            auto nodeAt = (idx >= 0 && idx < (int)visibleNodes_.size()) ? visibleNodes_[idx] : nullptr;
            if (nodeAt) {
                if (!nodeAt->children.empty()) {
                    float indentX = GetNodeTextStartX(nodeAt);
                    if (x >= indentX - 12.0f && x <= indentX) { ToggleNode(nodeAt); return; }
                }
                if (nodeAt->checkable) {
                    float indentX = GetNodeTextStartX(nodeAt);
                    float cbX = indentX + 2.0f + indicatorWidth_ + 4.0f;
                    float cy = arrangedRect_.y + headerOffset + idx * rowHeight_ - Snap(scrollOffsetY_) + rowHeight_ / 2.0f;
                    if (x >= cbX && x <= cbX + 15.0f && y >= cy - 7.5f && y <= cy + 7.5f) {
                        auto st = (nodeAt->checkState == TreeNode::CheckState::Checked)
                            ? TreeNode::CheckState::Unchecked : TreeNode::CheckState::Checked;
                        SetNodeCheckState(nodeAt, st);
                        return;
                    }
                }
            }

            // 记录拖拽起点（内容坐标，用于框选）
            pressActive_ = true;
            marqueeActive_ = false;
            pressStartCX_ = x - arrangedRect_.x + Snap(scrollOffsetX_);
            pressStartCY_ = y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_);
            marqueeCurCX_ = pressStartCX_;
            marqueeCurCY_ = pressStartCY_;

            if (nodeAt) {
                DWORD now = GetTickCount();
                bool isDouble = (now - lastClickTick_ < GetDoubleClickTime() && lastClickIndex_ == idx);
                lastClickTick_ = now;
                lastClickIndex_ = idx;

                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (selectionMode_ == SelectionMode::Single) {
                    SetSelectedNode(nodeAt);
                }
                else if (selectionMode_ == SelectionMode::Multi) {
                    ToggleNodeSelection(nodeAt);
                }
                else {   // Extended
                    if (ctrl) {
                        ToggleNodeSelection(nodeAt);
                    }
                    else if (shift && selectedNode_) {
                        int a = GetVisibleIndex(selectedNode_);
                        int b = idx;
                        if (a >= 0) {
                            if (a > b) std::swap(a, b);
                            for (auto& r : roots_) SetSelectedRecursive(r, false);
                            for (int k = a; k <= b && k < (int)visibleNodes_.size(); ++k)
                                if (visibleNodes_[k]) visibleNodes_[k]->selected = true;
                            EmitMultiSelection();
                            RequestRepaint();
                        }
                    }
                    else {
                        for (auto& r : roots_) SetSelectedRecursive(r, false);
                        SetSelectedNode(nodeAt);
                    }
                }

                NodeClicked(nodeAt);
                if (isDouble) ItemDoubleClicked(nodeAt);
            }
        }

        void OnMouseUp(float x, float y) override {
            MouseUp.Fire(x, y);
            if (marqueeActive_) {
                marqueeActive_ = false;
                pressActive_ = false;
                RequestRepaint();
                return;
            }
            pressActive_ = false;
            if (isResizingColumn_) {
                isResizingColumn_ = false;
                RequestRepaint();
                return;
            }
            if (isDraggingVertical_) { isDraggingVertical_ = false; RequestRepaint(); return; }
            if (isDraggingHorizontal_) { isDraggingHorizontal_ = false; RequestRepaint(); return; }
        }

        void OnMouseEnter() override { MouseEnter.Fire(); }

        void OnMouseLeave() override {
            hoveredNode_ = nullptr;
            isVerticalHovered_ = false;
            isHorizontalHovered_ = false;
            SetToolTip(std::wstring());
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            MouseLeave.Fire();
            RequestRepaint();
        }

        bool OnMouseWheel(float deltaX, float deltaY) override {
            bool handled = false;
            if (deltaY != 0 && maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                handled = true;
                RequestRepaint();
            }
            if (deltaX != 0 && maxScrollX_ > 0) {
                targetScrollOffsetX_ = clamp(targetScrollOffsetX_ - deltaX * scrollWheelStep_, 0.0f, maxScrollX_);
                handled = true;
                RequestRepaint();
            }
            return handled;
        }

        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || visibleNodes_.empty()) return;
            int idx = GetVisibleIndex(selectedNode_);
            switch (key) {
            case VK_UP:
                if (idx > 0) SetSelectedNode(visibleNodes_[idx - 1]);
                break;
            case VK_DOWN:
                if (idx < (int)visibleNodes_.size() - 1) SetSelectedNode(visibleNodes_[idx + 1]);
                break;
            case VK_RIGHT:
                if (selectedNode_ && !selectedNode_->children.empty() && !selectedNode_->expanded)
                    ExpandNode(selectedNode_, true);
                break;
            case VK_LEFT:
                if (selectedNode_ && selectedNode_->expanded)
                    ExpandNode(selectedNode_, false);
                else if (selectedNode_ && selectedNode_->parent) {
                    auto parentShared = FindNode(selectedNode_->parent);
                    if (parentShared) SetSelectedNode(parentShared);
                }
                break;
            case VK_HOME:
                if (!visibleNodes_.empty()) SetSelectedNode(visibleNodes_.front());
                break;
            case VK_END:
                if (!visibleNodes_.empty()) SetSelectedNode(visibleNodes_.back());
                break;
            default:
                break;
            }
            KeyDown.Fire(key, lParam);
        }

        void OnFocus() override { RequestRepaint(); Focused.Fire(); }
        void OnBlur() override { RequestRepaint(); Blurred.Fire(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else scrollOffsetY_ = targetScrollOffsetY_;

            if (fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f) {
                scrollOffsetX_ += (targetScrollOffsetX_ - scrollOffsetX_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetX_ - scrollOffsetX_) <= 0.1f) scrollOffsetX_ = targetScrollOffsetX_;
                RequestRepaint();
            }
            else scrollOffsetX_ = targetScrollOffsetX_;

            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            indicatorX_ += (targetIndicatorX_ - indicatorX_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorX_ - targetIndicatorX_) < 0.01f) indicatorX_ = targetIndicatorX_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f || fabs(indicatorX_ - targetIndicatorX_) > 0.01f)
                RequestRepaint();

            if (isVerticalHovered_ || isDraggingVertical_) {
                if (verticalScrollHoverProgress_ < 1.0f) {
                    verticalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ > 1.0f) verticalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (verticalScrollHoverProgress_ > 0.0f) {
                    verticalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ < 0.0f) verticalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            if (isHorizontalHovered_ || isDraggingHorizontal_) {
                if (horizontalScrollHoverProgress_ < 1.0f) {
                    horizontalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ > 1.0f) horizontalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (horizontalScrollHoverProgress_ > 0.0f) {
                    horizontalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ < 0.0f) horizontalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.1f);
            ConvergeValue(indicatorX_, targetIndicatorX_, 0.1f);
            ConvergeValue(scrollOffsetX_, targetScrollOffsetX_, 0.1f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.1f);
            // 末尾添加强制收敛
            const float eps = 0.1f;
            if (fabs(indicatorY_ - targetIndicatorY_) < eps) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorX_ - targetIndicatorX_) < eps) indicatorX_ = targetIndicatorX_;
            if (fabs(scrollOffsetX_ - targetScrollOffsetX_) < eps) scrollOffsetX_ = targetScrollOffsetX_;
            if (fabs(scrollOffsetY_ - targetScrollOffsetY_) < eps) scrollOffsetY_ = targetScrollOffsetY_;

            // 勾选动画
            bool checkAnim = false;
            for (auto& kv : checkAnim_) {
                if (!kv.first) continue;
                float t = (kv.first->checkState == TreeNode::CheckState::Unchecked) ? 0.0f : 1.0f;
                if (fabs(t - kv.second) > 0.001f) {
                    kv.second += (t - kv.second) * min(1.0f, indicatorAnimSpeed_ * deltaTime);
                    if (fabs(t - kv.second) <= 0.001f) kv.second = t;
                    checkAnim = true;
                }
            }
            if (checkAnim) RequestRepaint();
        }

        bool HasActiveAnimation() const override {
            return fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f ||
                fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f ||
                fabs(indicatorY_ - targetIndicatorY_) > 0.01f ||
                fabs(indicatorX_ - targetIndicatorX_) > 0.01f ||
                AnyCheckAnimActive() ||
                (isVerticalHovered_ && verticalScrollHoverProgress_ < 1.0f) ||
                (!isVerticalHovered_ && verticalScrollHoverProgress_ > 0.0f) ||
                (isHorizontalHovered_ && horizontalScrollHoverProgress_ < 1.0f) ||
                (!isHorizontalHovered_ && horizontalScrollHoverProgress_ > 0.0f);
        }

        bool OnContextMenu(float x, float y) override {
            auto node = GetNodeAtY(y);
            if (node) {
                lastContextNode_ = node;
                int col = GetColumnIndexAtX(x - arrangedRect_.x + Snap(scrollOffsetX_));
                ItemRightClicked(node, col == 0);
            }
            return false;   // 继续弹出默认右键菜单（若设置了 SetContextMenu）
        }
        std::shared_ptr<TreeNode> GetContextNode() const { return lastContextNode_; }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            headerBgBrush_.Reset();
            textBrush_.Reset();
            headerTextBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            indicatorBrush_.Reset();
            gridLineBrush_.Reset();
            borderBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            alternateBrush_.Reset();
            checkboxBrush_.Reset();
            iconBrush_.Reset();
            marqueeFillBrush_.Reset();
            marqueeBorderBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        void SetSelectedRecursive(std::shared_ptr<TreeNode> node, bool sel) {
            if (!node) return;
            node->selected = sel;
            for (auto& c : node->children) SetSelectedRecursive(c, sel);
        }
        void CollectSelected(std::shared_ptr<TreeNode> node, std::vector<std::shared_ptr<TreeNode>>& out) const {
            if (!node) return;
            if (node->selected) out.push_back(node);
            for (auto& c : node->children) CollectSelected(c, out);
        }
        void CollectChecked(std::shared_ptr<TreeNode> node, std::vector<std::shared_ptr<TreeNode>>& out) const {
            if (!node) return;
            if (node->checkState == TreeNode::CheckState::Checked) out.push_back(node);
            for (auto& c : node->children) CollectChecked(c, out);
        }
        void EmitMultiSelection() {
            if (selectionMode_ != SelectionMode::Single) SelectionChangedMulti(GetSelectedNodes());
        }
        bool IsAncestorOf(std::shared_ptr<TreeNode> ancestor, std::shared_ptr<TreeNode> node) const {
            TreeNode* p = node ? node->parent : nullptr;
            while (p) { if (p == ancestor.get()) return true; p = p->parent; }
            return false;
        }
        void UpdateDepthRecursive(std::shared_ptr<TreeNode> node, int depth) {
            if (!node) return;
            node->depth = depth;
            for (auto& c : node->children) { c->parent = node.get(); UpdateDepthRecursive(c, depth + 1); }
        }
        int CountRecursive(std::shared_ptr<TreeNode> node) const {
            if (!node) return 0;
            int c = 1;
            for (auto& ch : node->children) c += CountRecursive(ch);
            return c;
        }
        void SetExpandedRecursive(std::shared_ptr<TreeNode> node, bool e) {
            if (!node) return;
            node->expanded = e;
            for (auto& c : node->children) SetExpandedRecursive(c, e);
        }
        void UpdateParentCheckState(std::shared_ptr<TreeNode> node) {
            TreeNode* p = node ? node->parent : nullptr;
            if (!p) return;
            auto parent = FindNode(p);
            if (!parent) return;
            bool allChecked = true, allUnchecked = true;
            for (auto& c : parent->children) {
                if (c->checkState != TreeNode::CheckState::Checked) allChecked = false;
                if (c->checkState != TreeNode::CheckState::Unchecked) allUnchecked = false;
            }
            TreeNode::CheckState st = allChecked ? TreeNode::CheckState::Checked
                : (allUnchecked ? TreeNode::CheckState::Unchecked : TreeNode::CheckState::PartiallyChecked);
            if (parent->checkState != st) {
                parent->checkState = st;
                EnsureCheckAnim(parent);   // 父节点勾选动画也要生效
                ItemCheckStateChanged(parent, st);
                UpdateParentCheckState(parent);
            }
        }
        int GetColumnIndexAtX(float relX) const {
            float x = 0;
            for (int i = 0; i < columnCount_; ++i) {
                float w = GetEffectiveColumnWidth(i);
                if (relX >= x && relX < x + w) return i;
                x += w;
            }
            return -1;
        }
        void ToggleSort(int col) {
            if (sortColumn_ == col) sortAscending_ = !sortAscending_;
            else { sortColumn_ = col; sortAscending_ = true; }
        }
        float GetCheckAnim(std::shared_ptr<TreeNode> node) const {
            if (!node) return 0.0f;
            auto it = checkAnim_.find(node.get());
            if (it != checkAnim_.end()) return it->second;
            return (node->checkState == TreeNode::CheckState::Unchecked) ? 0.0f : 1.0f;
        }
        void EnsureCheckAnim(std::shared_ptr<TreeNode> node) {
            if (node && checkAnim_.find(node.get()) == checkAnim_.end())
                checkAnim_[node.get()] = (node->checkState == TreeNode::CheckState::Unchecked) ? 0.0f : 1.0f;
        }
        void EraseCheckAnim(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            checkAnim_.erase(node.get());
            for (auto& c : node->children) EraseCheckAnim(c);
        }
        bool AnyCheckAnimActive() const {
            for (auto& kv : checkAnim_) {
                if (!kv.first) continue;
                float t = (kv.first->checkState == TreeNode::CheckState::Unchecked) ? 0.0f : 1.0f;
                if (fabs(t - kv.second) > 0.001f) return true;
            }
            return false;
        }
        void ApplyMarqueeSelection() {
            float y0 = min(pressStartCY_, marqueeCurCY_);
            float y1 = max(pressStartCY_, marqueeCurCY_);
            for (int i = 0; i < (int)visibleNodes_.size(); ++i) {
                auto node = visibleNodes_[i];
                if (!node) continue;
                float top = i * rowHeight_, bottom = top + rowHeight_;
                bool hit = !(bottom < y0 || top > y1);
                node->selected = hit;
                if (marqueeCheckSync_ && node->checkable) {
                    auto st = hit ? TreeNode::CheckState::Checked : TreeNode::CheckState::Unchecked;
                    if (node->checkState != st) { node->checkState = st; EnsureCheckAnim(node); ItemCheckStateChanged(node, st); }
                }
            }
            EmitMultiSelection();
        }
        void BuildVisibleList() {
            visibleIndexDirty_ = true;   // P3：可见列表变了，索引映射必须重建
            childrenDirty_ = true;       // Label 化：可见节点集合可能变了，单元格 Label 列表必须重建
            float oldScrollX = scrollOffsetX_;
            float oldScrollY = scrollOffsetY_;
            visibleNodes_.clear();
            if (filter_) {
                std::unordered_set<TreeNode*> keep;
                std::function<bool(std::shared_ptr<TreeNode>)> mark = [&](std::shared_ptr<TreeNode> n) -> bool {
                    if (!n) return false;
                    bool m = filter_(n);
                    for (auto& c : n->children) { bool cm = mark(c); if (cm) m = true; }
                    if (m) keep.insert(n.get());
                    return m;
                    };
                for (auto& root : roots_) mark(root);
                std::function<void(std::shared_ptr<TreeNode>)> traverseFiltered = [&](std::shared_ptr<TreeNode> node) {
                    if (!node || keep.count(node.get()) == 0) return;
                    visibleNodes_.push_back(node);
                    for (auto& child : node->children) traverseFiltered(child);
                    };
                for (auto& root : roots_) traverseFiltered(root);
                UpdateScrollInfo();
                scrollOffsetX_ = clamp(oldScrollX, 0.0f, maxScrollX_);
                scrollOffsetY_ = clamp(oldScrollY, 0.0f, maxScrollY_);
                targetScrollOffsetX_ = scrollOffsetX_;
                targetScrollOffsetY_ = scrollOffsetY_;
                UpdateIndicatorTarget();
                return;
            }
            std::function<void(std::shared_ptr<TreeNode>)> traverse = [&](std::shared_ptr<TreeNode> node) {
                if (!node) return;
                visibleNodes_.push_back(node);
                if (node->expanded) {
                    for (auto& child : node->children) {
                        traverse(child);
                    }
                }
                };
            for (auto& root : roots_) {
                traverse(root);
            }
            UpdateScrollInfo();
            scrollOffsetX_ = clamp(oldScrollX, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(oldScrollY, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = scrollOffsetX_;
            targetScrollOffsetY_ = scrollOffsetY_;
            UpdateIndicatorTarget();
        }

        int GetNodeDepth(std::shared_ptr<TreeNode> node) const {
            return node ? node->depth : 0;
        }

        float GetNodeTextStartX(std::shared_ptr<TreeNode> node) const {
            int depth = GetNodeDepth(node);
            const float kBaseOffset = 12.0f;
            float indentX = arrangedRect_.x - Snap(scrollOffsetX_) + kBaseOffset + depth * indent_;
            if (indentX < arrangedRect_.x + 12.0f) {
                indentX = arrangedRect_.x + 12.0f;
            }
            return indentX;
        }

        std::shared_ptr<TreeNode> FindNode(TreeNode* rawPtr) const {
            std::function<std::shared_ptr<TreeNode>(const std::vector<std::shared_ptr<TreeNode>>&)> search =
                [&](const std::vector<std::shared_ptr<TreeNode>>& nodes) -> std::shared_ptr<TreeNode> {
                for (auto& node : nodes) {
                    if (node.get() == rawPtr) return node;
                    auto found = search(node->children);
                    if (found) return found;
                }
                return nullptr;
                };
            return search(roots_);
        }

        int GetVisibleIndex(std::shared_ptr<TreeNode> node) const {
            if (visibleIndexDirty_) {   // P3：惰性建 node->index 映射，避免每次选中都 O(n) 遍历
                visibleIndex_.clear();
                for (int i = 0; i < (int)visibleNodes_.size(); ++i) visibleIndex_[visibleNodes_[i].get()] = i;
                visibleIndexDirty_ = false;
            }
            auto it = visibleIndex_.find(node.get());
            return it == visibleIndex_.end() ? -1 : it->second;
        }
        mutable std::unordered_map<TreeNode*, int> visibleIndex_;
        mutable bool visibleIndexDirty_ = true;

        float GetEffectiveColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            return columnWidths_[col];
        }

        void UpdateScrollInfo() {
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float availWidth = arrangedRect_.width;
            float availHeight = arrangedRect_.height - headerOffset;

            float totalWidth = 0;
            for (float w : columnWidths_) totalWidth += w;
            float contentHeight = visibleNodes_.size() * rowHeight_;

            bool needV = contentHeight > availHeight;
            bool needH = totalWidth > availWidth;

            float viewportWidth = needV ? availWidth - scrollBarWidth_ : availWidth;
            float viewportHeight = needH ? availHeight - scrollBarWidth_ : availHeight;

            if (!needV && contentHeight > viewportHeight) {
                needV = true;
                viewportWidth = availWidth - scrollBarWidth_;
            }
            if (!needH && totalWidth > viewportWidth) {
                needH = true;
                viewportHeight = availHeight - scrollBarWidth_;
            }

            showVerticalScrollBar_ = needV;
            showHorizontalScrollBar_ = needH;

            maxScrollX_ = max(0.0f, totalWidth - viewportWidth);
            maxScrollY_ = max(0.0f, contentHeight - viewportHeight);

            scrollOffsetX_ = clamp(scrollOffsetX_, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(std::shared_ptr<TreeNode> node) {
            int idx = GetVisibleIndex(node);
            if (idx < 0) return;
            float itemTop = idx * rowHeight_;
            float itemBottom = itemTop + rowHeight_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float viewportHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + viewportHeight) {
                targetScrollOffsetY_ = itemBottom - viewportHeight;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void UpdateIndicatorTarget() {
            int idx = GetVisibleIndex(selectedNode_);
            if (idx >= 0) {
                targetIndicatorY_ = idx * rowHeight_;
                float indentX = GetNodeTextStartX(selectedNode_);
                targetIndicatorX_ = indentX + 2.0f;
            }
            else {
                targetIndicatorY_ = 0.0f;
                targetIndicatorX_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
            if (indicatorX_ < 0.0f) indicatorX_ = targetIndicatorX_;
        }

        void HandleVerticalScrollDrag(float mouseY) {
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float trackY = arrangedRect_.y + headerOffset;
            float trackHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            if (trackHeight > thumbLength) {
                float ratio = (mouseY - dragStartMouseY_) / (trackHeight - thumbLength);
                targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                RequestRepaint();
            }
        }

        void HandleHorizontalScrollDrag(float mouseX) {
            float trackX = arrangedRect_.x;
            float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            if (trackWidth > thumbLength) {
                float ratio = (mouseX - dragStartMouseX_) / (trackWidth - thumbLength);
                targetScrollOffsetX_ = clamp(dragStartScrollX_ + ratio * maxScrollX_, 0.0f, maxScrollX_);
                RequestRepaint();
            }
        }

        void DrawHeader(ID2D1RenderTarget* rt, float viewportWidth) {
            D2D1_RECT_F headerRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_);
            if (!headerBgBrush_) rt->CreateSolidColorBrush(headerBackgroundColor_, headerBgBrush_.GetAddressOf());
            else headerBgBrush_->SetColor(headerBackgroundColor_);
            rt->FillRectangle(headerRect, headerBgBrush_.Get());

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int c = 0; c < columnCount_; ++c) {
                float colWidth = GetEffectiveColumnWidth(c);
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    if (c < (int)headerLabels_.size() && !headerLabels_[c].empty()) {
                        if (!headerTextBrush_) rt->CreateSolidColorBrush(headerTextColor_, headerTextBrush_.GetAddressOf());
                        else headerTextBrush_->SetColor(headerTextColor_);
                        D2D1_RECT_F textRect = D2D1::RectF(colX + 4, arrangedRect_.y,
                            colX + colWidth - 4, arrangedRect_.y + headerHeight_);
                        DrawTextWithEllipsis(rt, headerLabels_[c], textRect, headerTextColor_, spec, headerTextBrush_, fmt);
                    }
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y),
                        D2D1::Point2F(colX, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
                }
                colX += colWidth;
            }
            if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
            else gridLineBrush_->SetColor(gridLineColor_);
            rt->DrawLine(D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y),
                D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
            rt->DrawLine(D2D1::Point2F(arrangedRect_.x, arrangedRect_.y + headerHeight_),
                D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
        }

        void DrawVerticalScrollBar(ID2D1RenderTarget* rt, float viewportHeight, float headerOffset) {
            if (maxScrollY_ <= 0.0f) return;   // 无滚动量，避免除零
            float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
            float trackY = arrangedRect_.y + headerOffset;
            float trackHeight = viewportHeight - headerOffset;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + scrollBarWidth_, trackY + trackHeight),
                scrollBarWidth_ / 2, scrollBarWidth_ / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = scrollBarWidth_ - 2.0f;
            float thumbX = trackX + (scrollBarWidth_ - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (verticalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * verticalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * verticalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * verticalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * verticalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        void DrawHorizontalScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            if (maxScrollX_ <= 0.0f) return;   // 无滚动量，避免除零
            float trackX = arrangedRect_.x;
            float trackY = arrangedRect_.y + arrangedRect_.height - scrollBarWidth_;
            float trackWidth = viewportWidth;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + scrollBarWidth_),
                scrollBarWidth_ / 2, scrollBarWidth_ / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            float thumbPos = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
            float thumbHeight = scrollBarWidth_ - 2.0f;
            float thumbY = trackY + (scrollBarWidth_ - thumbHeight) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (horizontalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * horizontalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbPos, thumbY, thumbPos + thumbLength, thumbY + thumbHeight),
                thumbHeight / 2, thumbHeight / 2), scrollThumbBrush_.Get());
        }

        // 成员变量
        std::vector<std::shared_ptr<TreeNode>> roots_;
        std::vector<std::shared_ptr<TreeNode>> visibleNodes_;
        std::function<bool(const std::shared_ptr<TreeNode>&)> filter_;
        int defaultExpandDepth_ = -1;
        std::shared_ptr<TreeNode> selectedNode_;
        std::shared_ptr<TreeNode> hoveredNode_;
        float scrollOffsetX_, targetScrollOffsetX_, maxScrollX_;
        float scrollOffsetY_, targetScrollOffsetY_, maxScrollY_;
        bool showVerticalScrollBar_, showHorizontalScrollBar_;
        bool isDraggingVertical_, isDraggingHorizontal_;
        float dragStartMouseX_, dragStartMouseY_;
        float dragStartScrollX_, dragStartScrollY_;
        bool isResizingColumn_;
        int resizeColumnIndex_;
        float resizeStartMouseX_, resizeStartColumnWidth_;
        float rowHeight_, indent_, headerHeight_;
        bool headerVisible_;
        int columnCount_;
        std::vector<float> columnWidths_;
        std::vector<std::wstring> headerLabels_;
        float indicatorWidth_, indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_, targetIndicatorY_;
        float indicatorX_, targetIndicatorX_;
        float scrollBarWidth_, scrollBarMinLength_;
        float scrollWheelStep_, scrollAnimationSpeed_, hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_, headerBackgroundColor_;
        D2D1_COLOR_F textColor_, headerTextColor_;
        D2D1_COLOR_F selectedColor_, hoverColor_, gridLineColor_, borderColor_;
        D2D1_COLOR_F scrollTrackColor_, scrollThumbColor_, scrollHoverThumbColor_;
        float verticalScrollHoverProgress_, horizontalScrollHoverProgress_;
        bool isVerticalHovered_, isHorizontalHovered_;
        // ---- 扩展状态 ----
        SelectionMode selectionMode_ = SelectionMode::Single;
        bool alternatingRowColors_ = false;
        D2D1_COLOR_F alternateRowColor_ = D2D1::ColorF(0.97f, 0.97f, 0.97f, 1.0f);
        bool showGrid_ = true;
        bool rootDecorated_ = true;
        bool sortingEnabled_ = false;
        int sortColumn_ = -1;
        bool sortAscending_ = true;
        bool checkableMode_ = false;
        CheckMode checkMode_ = CheckMode::Linked;
        std::unordered_map<TreeNode*, float> checkAnim_;
        D2D1_COLOR_F checkBoxColor_ = CheckBox::DefaultBoxColor;
        D2D1_COLOR_F checkMarkColor_ = CheckBox::DefaultCheckColor;
        DWORD lastClickTick_ = 0;
        int lastClickIndex_ = -1;
        std::shared_ptr<TreeNode> lastContextNode_;
        bool pressActive_ = false;
        bool marqueeActive_ = false;
        bool marqueeEnabled_ = true;
        bool marqueeCheckSync_ = false;
        float pressStartCX_ = 0, pressStartCY_ = 0, marqueeCurCX_ = 0, marqueeCurCY_ = 0;
        ComPtr<ID2D1SolidColorBrush> marqueeFillBrush_, marqueeBorderBrush_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_, headerBgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_, headerTextBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_, hoverBrush_, indicatorBrush_, gridLineBrush_, borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_, scrollThumbBrush_;
        ComPtr<ID2D1SolidColorBrush> alternateBrush_, checkboxBrush_, iconBrush_;
    };

} // namespace ZufyUI