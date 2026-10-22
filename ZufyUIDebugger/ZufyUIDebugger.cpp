// ============================================================================
// ZufyUIDebugger —— 外部调试 / 自动化 + UIA 查看器（用 ZufyUI 自绘）
//   * “调试通道”：向目标窗口发 WM_COPYDATA，拉取“当前这一帧”的元素树 / 帧统计，
//       或远程 Invoke/设值/聚焦（自动化）。
//   * “UIA”：用 UI Automation 客户端遍历目标窗口的无障碍树。
//   * 操作全部放在窗口“右键菜单”里；结果用分列的 TreeView 展示；可保存到文件。
// ============================================================================
#include "pch.h"
#include <wincodec.h>                 // WIC：把截图编码成 PNG
#include <set>
#include <map>
#pragma comment(lib, "windowscodecs.lib")
using namespace ZufyUI;

static std::shared_ptr<TextEdit> MakeLogEdit(const std::wstring& init = L"");   // 前置声明（只读多行文本查看器）
static void SetEditPreserveScroll(const std::shared_ptr<TextEdit>& e, const std::wstring& s);   // 刷新文本但保留滚动位置

// ---------------------------------------------------------------------------
static std::wstring g_reply;
static LRESULT CALLBACK SinkProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COPYDATA) {
        auto* cds = reinterpret_cast<COPYDATASTRUCT*>(l);
        if (cds && cds->lpData && cds->cbData >= sizeof(wchar_t)) {
            g_reply.assign((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
            while (!g_reply.empty() && g_reply.back() == L'\0') g_reply.pop_back();
        }
        return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}
static HWND g_sink = nullptr;
static HWND MakeSink() {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = SinkProc;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"ZufyUIDebugSink";
        RegisterClassExW(&wc); reg = true;
    }
    return CreateWindowExW(0, L"ZufyUIDebugSink", L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}
static std::wstring SendDebugCmd(HWND target, int code, const std::wstring& arg = L"") {
    if (!target || !IsWindow(target)) return L"[无效目标窗口]";
    if (!g_sink) g_sink = MakeSink();
    COPYDATASTRUCT cds = {};
    cds.dwData = (ULONG_PTR)code;
    cds.cbData = (DWORD)((arg.size() + 1) * sizeof(wchar_t));
    cds.lpData = (PVOID)arg.c_str();
    g_reply.clear();
    DWORD_PTR res = 0;
    // 带超时且不加 SMTO_BLOCK：目标忙/挂起时不会卡死调试器；等待期间仍能处理目标回发的 WM_COPYDATA 与本线程定时器
    SendMessageTimeoutW(target, WM_COPYDATA, (WPARAM)g_sink, (LPARAM)&cds, SMTO_ABORTIFHUNG, 100, &res);
    return g_reply.empty() ? L"[无回复：目标忙或未 SetDebugEnabled(true)]" : g_reply;
}

// ---------------------------------------------------------------------------
static const wchar_t* UiaCtName(CONTROLTYPEID ct) {
    switch (ct) {
    case UIA_ButtonControlTypeId: return L"Button";
    case UIA_TextControlTypeId: return L"Text";
    case UIA_EditControlTypeId: return L"Edit";
    case UIA_CheckBoxControlTypeId: return L"CheckBox";
    case UIA_RadioButtonControlTypeId: return L"RadioButton";
    case UIA_ComboBoxControlTypeId: return L"ComboBox";
    case UIA_SliderControlTypeId: return L"Slider";
    case UIA_ProgressBarControlTypeId: return L"ProgressBar";
    case UIA_ListControlTypeId: return L"List";
    case UIA_ListItemControlTypeId: return L"ListItem";
    case UIA_TreeControlTypeId: return L"Tree";
    case UIA_TreeItemControlTypeId: return L"TreeItem";
    case UIA_TabControlTypeId: return L"Tab";
    case UIA_TabItemControlTypeId: return L"TabItem";
    case UIA_MenuControlTypeId: return L"Menu";
    case UIA_MenuItemControlTypeId: return L"MenuItem";
    case UIA_ScrollBarControlTypeId: return L"ScrollBar";
    case UIA_WindowControlTypeId: return L"Window";
    case UIA_GroupControlTypeId: return L"Group";
    case UIA_PaneControlTypeId: return L"Pane";
    case UIA_DocumentControlTypeId: return L"Document";
    case UIA_DataGridControlTypeId: return L"DataGrid";
    case UIA_ToolTipControlTypeId: return L"ToolTip";
    default: return L"Control";
    }
}
static void SetCols(const std::shared_ptr<TreeView>& tree, const std::shared_ptr<TreeNode>& n,
    const std::wstring& role, const std::wstring& name, const std::wstring& id, const std::wstring& rect) {
    tree->SetNodeText(n, 0, role);
    tree->SetNodeText(n, 1, name);
    tree->SetNodeText(n, 2, id);
    tree->SetNodeText(n, 3, rect);
}
static void WalkUia(IUIAutomationTreeWalker* walker, IUIAutomationElement* el,
    const std::shared_ptr<TreeView>& tree, const std::shared_ptr<TreeNode>& parent, int depth) {
    if (!el || depth > 14) return;
    BSTR name = nullptr; el->get_CurrentName(&name);
    BSTR aid = nullptr; el->get_CurrentAutomationId(&aid);
    CONTROLTYPEID ct = 0; el->get_CurrentControlType(&ct);
    RECT b = {}; el->get_CurrentBoundingRectangle(&b);
    wchar_t rb[64]; swprintf(rb, 64, L"%d,%d %dx%d", (int)b.left, (int)b.top, (int)(b.right - b.left), (int)(b.bottom - b.top));
    auto node = parent ? tree->AddChild(parent, UiaCtName(ct)) : tree->AddRoot(UiaCtName(ct));
    SetCols(tree, node, UiaCtName(ct), std::wstring(name ? name : L""), std::wstring(aid ? aid : L""), rb);
    if (name) SysFreeString(name);
    if (aid) SysFreeString(aid);
    IUIAutomationElement* child = nullptr;
    if (SUCCEEDED(walker->GetFirstChildElement(el, &child)) && child) {
        IUIAutomationElement* cur = child;
        while (cur) {
            IUIAutomationElement* next = nullptr;
            WalkUia(walker, cur, tree, node, depth + 1);
            walker->GetNextSiblingElement(cur, &next);
            cur->Release();
            cur = next;
        }
    }
}
static std::wstring UiaDump(HWND target, const std::shared_ptr<TreeView>& tree) {
    tree->Clear();
    IUIAutomation* uia = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia))) || !uia)
        return L"CoCreateInstance(CUIAutomation) 失败";
    IUIAutomationElement* root = nullptr;
    if (FAILED(uia->ElementFromHandle(target, &root)) || !root) { uia->Release(); return L"ElementFromHandle 失败"; }
    IUIAutomationTreeWalker* walker = nullptr;
    uia->get_ControlViewWalker(&walker);
    if (walker) { WalkUia(walker, root, tree, nullptr, 0); walker->Release(); }
    else { auto n = tree->AddRoot(L"(ControlViewWalker 不可用)"); (void)n; }
    tree->ExpandAll();
    root->Release(); uia->Release();
    return L"UIA 树已抓取（Control view）";
}

// UIA 客户端：按 AutomationId 在目标窗口里找元素（不需要目标开启调试）
static IUIAutomation* UiaCreateClient() {
    IUIAutomation* uia = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia)))) return nullptr;
    return uia;
}
static IUIAutomationElement* UiaFindById(IUIAutomation* uia, HWND target, const std::wstring& id) {
    if (!uia || !target || id.empty()) return nullptr;
    IUIAutomationElement* root = nullptr;
    if (FAILED(uia->ElementFromHandle(target, &root)) || !root) return nullptr;
    VARIANT v; VariantInit(&v); v.vt = VT_BSTR; v.bstrVal = SysAllocString(id.c_str());
    IUIAutomationCondition* cond = nullptr; uia->CreatePropertyCondition(UIA_AutomationIdPropertyId, v, &cond);
    IUIAutomationElement* found = nullptr;
    if (cond) { root->FindFirst(TreeScope_Descendants, cond, &found); cond->Release(); }
    root->Release(); VariantClear(&v);
    return found;   // 调用方 Release
}

// 解析调试通道的“缩进 + 制表符分列”文本到 TreeView
static int IndentOf(const std::wstring& s) { int n = 0; while (n * 2 + 1 < (int)s.size() && s[n * 2] == L' ' && s[n * 2 + 1] == L' ') n++; return n; }
static void FillFromIndented(const std::shared_ptr<TreeView>& tree, const std::wstring& text) {
    tree->Clear();
    std::vector<std::shared_ptr<TreeNode>> stack;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find(L'\n', i);
        std::wstring line = text.substr(i, (j == std::wstring::npos ? text.size() : j) - i);
        i = (j == std::wstring::npos) ? text.size() : j + 1;
        while (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;
        int d = IndentOf(line);
        std::wstring body = line.substr((size_t)d * 2);
        // 拆列
        std::vector<std::wstring> cols; size_t p = 0;
        while (true) { size_t q = body.find(L'\t', p); if (q == std::wstring::npos) { cols.push_back(body.substr(p)); break; } cols.push_back(body.substr(p, q - p)); p = q + 1; }
        while (cols.size() < 4) cols.push_back(L"");
        std::shared_ptr<TreeNode> node;
        if (d == 0 || stack.empty()) { node = tree->AddRoot(cols[0]); stack.assign(1, node); }
        else { if (d > (int)stack.size()) d = (int)stack.size(); stack.resize((size_t)d); node = tree->AddChild(stack.back(), cols[0]); stack.push_back(node); }
        tree->SetNodeText(node, 1, cols[1]);
        tree->SetNodeText(node, 2, cols[2]);
        tree->SetNodeText(node, 3, cols[3]);
    }
    tree->ExpandAll();
}

// 解析 Top-N 文本（表头 + `值\t类型\t名称\tAutomationId`）→ 条形图数据
static void ParseTopN(const std::wstring& text, std::vector<std::wstring>& names, std::vector<double>& vals, std::vector<std::wstring>& ids) {
    names.clear(); vals.clear(); ids.clear();
    size_t i = 0; bool firstLine = true;
    while (i < text.size()) {
        size_t j = text.find(L'\n', i);
        std::wstring line = text.substr(i, (j == std::wstring::npos ? text.size() : j) - i);
        i = (j == std::wstring::npos) ? text.size() : j + 1;
        while (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;
        if (firstLine) { firstLine = false; if (line.find(L'\t') == std::wstring::npos) continue; }
        std::vector<std::wstring> cols; size_t p = 0;
        while (true) { size_t q = line.find(L'\t', p); if (q == std::wstring::npos) { cols.push_back(line.substr(p)); break; } cols.push_back(line.substr(p, q - p)); p = q + 1; }
        if (cols.size() < 4) continue;
        double v = _wtof(cols[0].c_str());
        std::wstring nm = cols[1] + L" " + cols[2];
        if (nm.size() > 34) nm = nm.substr(0, 33) + L"\x2026";
        names.push_back(nm); vals.push_back(v); ids.push_back(cols[3]);
    }
}

// 在结果树里按 AutomationId（第 3 列）找节点
static std::shared_ptr<TreeNode> FindNodeById(const std::shared_ptr<TreeNode>& n, const std::wstring& id) {
    if (!n) return nullptr;
    if (n->columns.size() > 2 && n->columns[2] && n->columns[2]->GetText() == id) return n;
    for (auto& c : n->children) { auto r = FindNodeById(c, id); if (r) return r; }
    return nullptr;
}
// 整行键（4 列文本拼接），用于刷新后恢复选中
static std::wstring RowKey(const std::shared_ptr<TreeNode>& n) {
    std::wstring s;
    for (int c = 0; c < 4; ++c) { s += L'\x1f'; s += (c < (int)n->columns.size() && n->columns[c]) ? n->columns[c]->GetText() : std::wstring(); }
    return s;
}
static std::shared_ptr<TreeNode> FindByRowKey(const std::shared_ptr<TreeNode>& n, const std::wstring& key) {
    if (!n) return nullptr;
    if (RowKey(n) == key) return n;
    for (auto& c : n->children) { auto r = FindByRowKey(c, key); if (r) return r; }
    return nullptr;
}
static std::shared_ptr<TreeNode> FindNodeByRowKey(const std::shared_ptr<TreeView>& tree, const std::wstring& key) {
    for (int i = 0; i < tree->GetRootCount(); ++i) { auto r = FindByRowKey(tree->GetRootAt(i), key); if (r) return r; }
    return nullptr;
}

// 拾取后定位：展开祖先 + 选中该行（SetSelectedNode 内部 EnsureVisible 会滚到可见）
static bool SelectRowById(const std::shared_ptr<TreeView>& tree, const std::wstring& id) {
    if (!tree || id.empty()) return false;
    for (int i = 0; i < tree->GetRootCount(); ++i) {
        auto n = FindNodeById(tree->GetRootAt(i), id);
        if (n) {
            for (auto p = n->parent.lock(); p; p = p->parent.lock()) tree->ExpandNode(p, true);
            tree->SetSelectedNode(n);
            return true;
        }
    }
    return false;
}

// 把 BGRA 像素编码成 PNG（WIC）
static bool SaveBgraPng(const std::vector<BYTE>& bgra, int w, int h, const std::wstring& path) {
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) || !f) return false;
    bool ok = false;
    IWICStream* stream = nullptr;
    if (SUCCEEDED(f->CreateStream(&stream)) && stream && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) {
        IWICBitmapEncoder* enc = nullptr;
        if (SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && enc && SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache))) {
            IWICBitmapFrameEncode* frame = nullptr; IPropertyBag2* props = nullptr;
            if (SUCCEEDED(enc->CreateNewFrame(&frame, &props)) && frame && SUCCEEDED(frame->Initialize(props))) {
                frame->SetSize(w, h);
                WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
                frame->SetPixelFormat(&pf);
                if (IsEqualGUID(pf, GUID_WICPixelFormat32bppBGRA))
                    ok = SUCCEEDED(frame->WritePixels(h, w * 4, (UINT)bgra.size(), const_cast<BYTE*>(bgra.data())));
                frame->Commit();
            }
            if (props) props->Release();
            if (frame) frame->Release();
            enc->Commit();
            enc->Release();
        }
        stream->Release();
    }
    f->Release();
    return ok;
}
// 截图目标窗口（屏幕 BitBlt，含标题栏）→ PNG
static bool CaptureTargetPng(HWND target, const std::wstring& path) {
    RECT r; if (!target || !GetWindowRect(target, &r)) return false;
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return false;
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        ok = BitBlt(mem, 0, 0, w, h, screen, r.left, r.top, SRCCOPY | CAPTUREBLT) != 0;
        SelectObject(mem, old);
        if (ok) {
            std::vector<BYTE> buf((size_t)w * h * 4);
            memcpy(buf.data(), bits, buf.size());
            for (size_t i = 0; i < (size_t)w * h; ++i) buf[i * 4 + 3] = 255;   // 屏幕无 alpha → 强制不透明
            ok = SaveBgraPng(buf, w, h, path);
        }
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem); ReleaseDC(nullptr, screen);
    return ok;
}

// 把结果树序列化成行（depth\t col0..col3），用于快照对比
static std::wstring SerializeTree(const std::shared_ptr<TreeView>& tree) {
    std::wstring out;
    std::function<void(const std::shared_ptr<TreeNode>&, int)> rec = [&](const std::shared_ptr<TreeNode>& n, int d) {
        if (!n) return;
        out += std::to_wstring(d);
        for (int c = 0; c < 4; ++c) { out += L'\t'; out += (c < (int)n->columns.size() && n->columns[c]) ? n->columns[c]->GetText() : L""; }
        out += L'\n';
        for (auto& ch : n->children) rec(ch, d + 1);
        };
    for (int i = 0; i < tree->GetRootCount(); ++i) rec(tree->GetRootAt(i), 0);
    return out;
}
// 行集合差异（+ 新增 / - 删除；按整行比较，忽略顺序）
static std::wstring DiffLines(const std::wstring& a, const std::wstring& b) {
    auto split = [](const std::wstring& s) {
        std::vector<std::wstring> v; size_t i = 0;
        while (i < s.size()) { size_t j = s.find(L'\n', i); if (j == std::wstring::npos) j = s.size(); if (j > i) v.push_back(s.substr(i, j - i)); i = j + 1; }
        return v;
        };
    std::vector<std::wstring> av = split(a), bv = split(b);
    std::multiset<std::wstring> sa(av.begin(), av.end()), sb(bv.begin(), bv.end());
    std::wstring out; int add = 0, del = 0;
    for (const auto& k : sb) { auto it = sa.find(k); if (it != sa.end()) sa.erase(it); else { if (add < 500) out += L"+ " + k + L"\r\n"; ++add; } }
    for (const auto& k : sa) { if (del < 500) out += L"- " + k + L"\r\n"; ++del; }
    wchar_t h[64]; swprintf(h, 64, L"新增 %d 行，删除 %d 行\r\n\r\n", add, del);
    return std::wstring(h) + out;
}

// 从 "key=数字" 文本里取值（用于解析 cmd 3 帧统计）
static bool NumAfter(const std::wstring& s, const std::wstring& key, double& out) {
    size_t p = s.find(key);
    if (p == std::wstring::npos) return false;
    out = wcstod(s.c_str() + p + key.size(), nullptr);
    return true;
}

// ---------------------------------------------------------------------------
struct Target { HWND hwnd; std::wstring title; };
static const wchar_t* kZufyClass = L"ZufyUIWindowClass";
// zufyOnly=true 只列 ZufyUI 窗口；false 列全部可见窗口（UIA 窗口用）
static std::vector<Target> CollectTargets(bool zufyOnly) {
    struct S { std::vector<Target>* out; bool zufyOnly; } s{ nullptr, zufyOnly };
    std::vector<Target> v; s.out = &v;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        auto* p = reinterpret_cast<S*>(lp);
        if (!IsWindowVisible(h)) return TRUE;
        wchar_t t[256] = {}; GetWindowTextW(h, t, 256); if (!t[0]) return TRUE;
        wchar_t c[64] = {}; GetClassNameW(h, c, 64);
        if (p->zufyOnly && wcscmp(c, kZufyClass) != 0) return TRUE;
        p->out->push_back({ h, std::wstring(t) + L"  [" + c + L"]" });
        return TRUE;
        }, reinterpret_cast<LPARAM>(&s));
    return v;
}
static void FillTargetCombo(const std::shared_ptr<ComboBox>& combo, const std::vector<Target>& v) {
    std::vector<std::wstring> items;
    for (auto& t : v) items.push_back(t.title);
    if (items.empty()) items.push_back(L"(无可选窗口)");
    combo->SetItems(items);
    combo->SetSelectedIndex(0);
}
static std::vector<Target> g_targets;   // 主窗口（Zufy 专用）目标列表
static void RefreshTargets(const std::shared_ptr<ComboBox>& combo, bool zufyOnly = true) {
    g_targets = CollectTargets(zufyOnly);
    FillTargetCombo(combo, g_targets);
}
static HWND SelectedTarget(const std::shared_ptr<ComboBox>& combo) {
    int idx = combo->GetSelectedIndex();
    return (idx >= 0 && idx < (int)g_targets.size()) ? g_targets[idx].hwnd : nullptr;
}

// 给窗口装自定义标题栏：标题 + 三件套 + 最小化左边一个「置顶/取消置顶」切换按钮
static void ApplyDebugChrome(Window& w, const std::wstring& title) {
    auto tb = std::make_shared<DefaultTitleBar>();
    tb->SetTitle(title);
    tb->SetHeight(34);
    auto pin = std::make_shared<CaptionButton>(CaptionButton::Kind::Pin);
    pin->SetButtonWidth(42);
    pin->SetToolTip(L"置顶 / 取消置顶");
    auto pinned = std::make_shared<bool>(false);
    pin->Connect(pin->Clicked, [&w, pin, pinned]() {
        *pinned = !*pinned;
        SetWindowPos(w.GetHwnd(), *pinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        pin->SetActiveState(*pinned);
        });
    tb->AddCustomButton(pin);   // 追加在三件套之后 → 布局时排到「最小化」左边
    w.SetCustomTitleBar(tb);
}

// ---------------------------------------------------------------------------
// 控件详情窗口：实时（每 300ms）从目标拉取某控件的详细信息（布局次数/重绘/缓存…）
struct ControlDetailWindow : Window {
    HWND target = nullptr;
    std::wstring id;
    bool follow = false;                 // 跟随鼠标：每 tick 用光标下的控件 id
    std::shared_ptr<TextEdit> edit;
    std::shared_ptr<Timer> timer;
    std::wstring lastText_;

    void Build() {
        auto root = GetRootColumnBox();
        root->SetMargin(Thickness(6, 6, 6, 6));
        edit = MakeLogEdit(L"…");                          // 只读多行文本查看器
        edit->SetWrapMode(TextEdit::WrapMode::WidgetWidth);
        root->AddChild(edit);
        timer = CreateTimer(250);
        if (timer) timer->Tick.connect([this]() { Refresh(); });
        Refresh();
    }
    void Refresh() {
        if (!edit) return;
        bool stale = false;
        if (follow) {   // 光标下的目标进程控件 → 更新 id；找不到就沿用上一个并注明
            POINT pt; GetCursorPos(&pt);
            HWND h = WindowFromPoint(pt);
            DWORD dp = 0, tp = 0;
            if (h) GetWindowThreadProcessId(h, &dp);
            if (target) GetWindowThreadProcessId(target, &tp);
            if (h && dp == tp) {
                wchar_t buf[48]; swprintf(buf, 48, L"%d,%d", pt.x, pt.y);
                std::wstring cid = SendDebugCmd(h, 10, buf);
                if (!cid.empty() && cid[0] != L'[') id = cid;
                else stale = true;
            }
            else stale = true;
        }
        std::wstring head = (follow ? L"跟随鼠标: " : L"目标: ") + (id.empty() ? L"(无)" : id)
            + (follow && stale ? L"（当前光标下无控件，显示上一个）" : L"") + L"   （每 250ms 刷新）";
        std::wstring text = head + L"\r\n\r\n" + SendDebugCmd(target, 9, id);
        if (text != lastText_) { lastText_ = text; SetEditPreserveScroll(edit, text); }   // 内容变了才刷新，保留滚动
    }
};
static std::vector<std::shared_ptr<ControlDetailWindow>> g_details;   // 保持已开详情窗口存活
static std::shared_ptr<Timer> g_spyTimer;                            // 拾取定时器
static bool g_spyOn = false;                                         // 拾取开关
static std::shared_ptr<ControlDetailWindow> g_followDetail;          // 跟随鼠标用详情窗口

// ---- 错误面板：目标最近错误（cmd 13）+ 本调试器自身信号错误 ----
static std::wstring g_localErrors;
static std::mutex g_localErrorsMtx;
static std::vector<std::shared_ptr<Window>> g_extraWindows;           // 保持额外窗口存活
static void InstallErrorSubs(Window& w) {
    static bool done = false; if (done) return; done = true;
    UIZSignals::Error.connect([](const std::wstring& m) {          // 库内部错误（命名空间级信号）
        std::lock_guard<std::mutex> lk(g_localErrorsMtx);
        g_localErrors += L"[Error] " + m + L"\r\n";
        if (g_localErrors.size() > 20000) g_localErrors.erase(0, 10000);
        });
    w.DeviceLost.connect([]() {                                     // Window 实例信号
        std::lock_guard<std::mutex> lk(g_localErrorsMtx);
        g_localErrors += L"[DeviceLost] 设备丢失\r\n";
        });
    w.RenderingError.connect([](HRESULT hr) {
        wchar_t b[48]; swprintf(b, 48, L"[RenderingError] 0x%08X\r\n", (unsigned)hr);
        std::lock_guard<std::mutex> lk(g_localErrorsMtx);
        g_localErrors += b;
        });
}
// ---- 页面数据格式化（性能 / 错误）----
static std::wstring FormatErrors(HWND target) {
    std::wstring t = L"=== 目标最近错误（cmd 13）===\r\n" + SendDebugCmd(target, 13);
    std::wstring local;
    { std::lock_guard<std::mutex> lk(g_localErrorsMtx); local = g_localErrors; }
    if (!local.empty()) t += L"\r\n=== 本调试器错误 ===\r\n" + local;
    return t;
}
static std::wstring PerfSpark(const std::vector<double>& v, size_t maxN) {
    if (v.empty()) return L"";
    double mx = 1e-6; for (double x : v) if (x > mx) mx = x;
    size_t start = v.size() > maxN ? v.size() - maxN : 0;
    const wchar_t* lv = L"▁▂▃▄▅▆▇█";
    std::wstring s;
    for (size_t i = start; i < v.size(); ++i) {
        int k = (int)(v[i] / mx * 7.0);
        if (k < 0) k = 0; if (k > 7) k = 7;
        s += lv[k]; s += L' ';
    }
    return s;
}
static double PerfAvg(const std::vector<double>& v) { if (v.empty()) return 0; double s = 0; for (double x : v) s += x; return s / v.size(); }
static double PerfMax(const std::vector<double>& v) { double m = 0; for (double x : v) if (x > m) m = x; return m; }
static std::vector<double> g_advH, g_renH;                       // 帧耗时历史（ms）
static double g_lastDt = 0; static unsigned long long g_lastFrame = 0;
static std::wstring FormatPerf(HWND target) {
    std::wstring r = SendDebugCmd(target, 3);
    double dt = 0, adv = 0, ren = 0, elems = 0, vis = 0, pend = 0, anims = 0, fr = 0;
    NumAfter(r, L"dt=", dt); NumAfter(r, L"advance=", adv); NumAfter(r, L"render=", ren);
    NumAfter(r, L"elems=", elems); NumAfter(r, L"vis=", vis); NumAfter(r, L"pending=", pend);
    NumAfter(r, L"anims=", anims); NumAfter(r, L"frame=", fr);
    if (dt > 0.0) {
        g_advH.push_back(adv); g_renH.push_back(ren);
        while (g_advH.size() > 300) g_advH.erase(g_advH.begin());
        while (g_renH.size() > 300) g_renH.erase(g_renH.begin());
        g_lastDt = dt; g_lastFrame = (unsigned long long)fr;
    }
    double fps = g_lastDt > 0.05 ? 1000.0 / g_lastDt : 0.0;
    wchar_t b[700];
    swprintf(b, 700,
        L"FPS: %.1f   dt(帧间隔): %.2f ms   frame: %llu\r\n\r\n"
        L"advance : avg %.2f   max %.2f ms\r\n"
        L"render  : avg %.2f   max %.2f ms\r\n"
        L"elems: %d   visible: %d   pending: %d   anims: %d\r\n\r\n"
        L"render 最近 %d 帧:\r\n%s\r\n\r\n"
        L"advance 最近 %d 帧:\r\n%s\r\n",
        fps, g_lastDt, g_lastFrame,
        PerfAvg(g_advH), PerfMax(g_advH), PerfAvg(g_renH), PerfMax(g_renH),
        (int)elems, (int)vis, (int)pend, (int)anims,
        (int)g_advH.size(), PerfSpark(g_renH, 60).c_str(),
        (int)g_advH.size(), PerfSpark(g_advH, 60).c_str());
    return b;
}

// CPU / GPU 占用采样（对目标进程）
struct UsageSampler {
    DWORD pid = 0; HANDLE hProc = nullptr; int nproc = 1;
    ULARGE_INTEGER lastK{}, lastU{}, lastT{}; bool init = false;
    PDH_HQUERY gpuQuery = nullptr; PDH_HCOUNTER gpuCounter = nullptr; int gpuWarm = 0;
    UsageSampler() { SYSTEM_INFO si{}; GetSystemInfo(&si); nproc = si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1; }
    void SetTarget(HWND hwnd) {
        DWORD p = 0; if (hwnd) GetWindowThreadProcessId(hwnd, &p);
        if (p == pid) return;
        pid = p; if (hProc) { CloseHandle(hProc); hProc = nullptr; } init = false;
        if (p) hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p);
    }
    double Cpu() {
        if (!hProc) return 0;
        FILETIME c, e, k, u; if (!GetProcessTimes(hProc, &c, &e, &k, &u)) return 0;
        ULARGE_INTEGER K; K.LowPart = k.dwLowDateTime; K.HighPart = k.dwHighDateTime;
        ULARGE_INTEGER U; U.LowPart = u.dwLowDateTime; U.HighPart = u.dwHighDateTime;
        FILETIME now; GetSystemTimeAsFileTime(&now);
        ULARGE_INTEGER T; T.LowPart = now.dwLowDateTime; T.HighPart = now.dwHighDateTime;
        if (!init) { lastK = K; lastU = U; lastT = T; init = true; return 0; }
        double dCpu = (double)((K.QuadPart - lastK.QuadPart) + (U.QuadPart - lastU.QuadPart));
        double dT = (double)(T.QuadPart - lastT.QuadPart);
        lastK = K; lastU = U; lastT = T;
        if (dT <= 0) return 0;
        double pct = dCpu / dT / (double)nproc * 100.0;
        return pct < 0 ? 0 : pct;
    }
    double Gpu() {
        if (!pid) return 0;
        if (!gpuQuery) {
            if (PdhOpenQueryW(nullptr, 0, &gpuQuery) != ERROR_SUCCESS) { gpuQuery = nullptr; return 0; }
            if (PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) { PdhCloseQuery(gpuQuery); gpuQuery = nullptr; return 0; }
            PdhCollectQueryData(gpuQuery); gpuWarm = 1; return 0;
        }
        PdhCollectQueryData(gpuQuery);
        if (gpuWarm) { gpuWarm = 0; return 0; }
        DWORD sz = 0, cnt = 0;
        PdhGetFormattedCounterArrayW(gpuCounter, PDH_FMT_DOUBLE, &sz, &cnt, nullptr);
        if (sz == 0) return 0;
        std::vector<BYTE> buf(sz);
        PDH_FMT_COUNTERVALUE_ITEM_W* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buf.data();
        if (PdhGetFormattedCounterArrayW(gpuCounter, PDH_FMT_DOUBLE, &sz, &cnt, items) != ERROR_SUCCESS) return 0;
        wchar_t tag[32]; swprintf(tag, 32, L"pid_%u_", pid);
        double sum = 0;
        for (DWORD i = 0; i < cnt; ++i) if (items[i].szName && wcsstr(items[i].szName, tag)) sum += items[i].FmtValue.doubleValue;
        return sum;
    }
    double MemMB() {
        if (!hProc) return 0;
        PROCESS_MEMORY_COUNTERS pmc{}; pmc.cb = sizeof(pmc);
        if (!GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) return 0;
        return (double)pmc.WorkingSetSize / 1048576.0;
    }
};

// 性能视图（FPS 标题 + 帧耗时折线 advance/render【带面积】 + CPU/GPU 占用折线），Tab 与独立窗口复用
struct PerfView {
    std::shared_ptr<Label> head;
    std::shared_ptr<LineChart> chart, chart2, chart3;
    std::vector<double> advH, renH, cpuH, gpuH, memH;
    double lastDt = 0, lastCpu = 0, lastGpu = 0, lastMem = 0; unsigned long long lastFrame = 0;
    UsageSampler usage;
    static std::shared_ptr<LineChart> MakeChart(const std::wstring& name, Color c, float weight) {
        auto ch = std::make_shared<LineChart>();
        ch->SetFillWidth(true); ch->SetVerticalStretchWeight(weight);
        ch->SetWidth(140.0f); ch->SetHeight(60.0f);            // 最小尺寸调小，窗口可缩得更小
        ch->SetShowCategoryLabels(false); ch->SetShowVGrid(false);
        ch->SetPlotHeight(160.0f); ch->SetFitPlotSize(true); ch->SetCategoryWidth(8.0f); ch->SetStickyAxes(true);
        ch->SetShowMarkers(false); ch->SetLineWidth(1.5f); ch->SetAreaFill(true, 0.22f);
        ch->AddSeries(name); ch->SetSeriesColor(0, c);
        for (int i = 0; i < 60; ++i) ch->AddCategory(std::to_wstring(i));
        return ch;
    }
    std::shared_ptr<UIElement> Build() {
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(6);
        col->SetFillHeight(true); col->SetVerticalStretchWeight(1.0f);   // 纵向布局、铺满可用高度
        head = std::make_shared<Label>(L"未选择目标窗口");
        head->SetTextOverflow(Label::TextOverflow::Wrap);   // 换行：不因一行长文本把窗口最小宽度撑大
        head->SetTextColor(Color::FromArgb(255, 0, 100, 180));
        col->AddChild(head);
        chart = MakeChart(L"advance", Color::FromArgb(255, 60, 140, 220), 1.0f);
        chart->AddSeries(L"render"); chart->SetSeriesColor(1, Color::FromArgb(255, 220, 120, 40));
        chart->SetValueFormat(1, L"", L" ms");
        chart->SetReferenceLine(16.67, L"16.7ms (60fps)");
        chart->SetValueAxisRange(0.0, 20.0);
        col->AddChild(chart);
        chart2 = MakeChart(L"CPU", Color::FromArgb(255, 150, 90, 200), 1.0f);
        chart2->AddSeries(L"GPU"); chart2->SetSeriesColor(1, Color::FromArgb(255, 90, 170, 90));
        chart2->SetValueFormat(0, L"", L" %");
        col->AddChild(chart2);
        chart3 = MakeChart(L"内存", Color::FromArgb(255, 60, 160, 190), 1.0f);
        chart3->SetValueFormat(1, L"", L" MB");
        col->AddChild(chart3);
        Refresh(nullptr);
        return col;
    }
    void Refresh(HWND target) {
        if (!head) return;
        if (!target) { head->SetText(L"未选择目标窗口"); return; }
        usage.SetTarget(target);
        std::wstring r = SendDebugCmd(target, 3);
        double dt = 0, adv = 0, ren = 0, fr = 0;
        NumAfter(r, L"dt=", dt); NumAfter(r, L"advance=", adv); NumAfter(r, L"render=", ren); NumAfter(r, L"frame=", fr);
        double cpu = usage.Cpu(), gpu = usage.Gpu(), mem = usage.MemMB();
        if (dt > 0.0) {
            auto pushFront = [](std::vector<double>& v, double x) { v.insert(v.begin(), x); if (v.size() > 60) v.pop_back(); };
            pushFront(advH, adv); pushFront(renH, ren); pushFront(cpuH, cpu); pushFront(gpuH, gpu); pushFront(memH, mem);   // 最新在左
            lastDt = dt; lastCpu = cpu; lastGpu = gpu; lastMem = mem; lastFrame = (unsigned long long)fr;
        }
        double fps = lastDt > 0.05 ? 1000.0 / lastDt : 0.0;
        wchar_t b[360];
        swprintf(b, 360,
            L"FPS %.1f   dt %.2f ms   advance avg %.2f / max %.2f ms   render avg %.2f / max %.2f ms   CPU %.1f%%   GPU %.1f%%   内存 %.1f MB   frame %llu",
            fps, lastDt, PerfAvg(advH), PerfMax(advH), PerfAvg(renH), PerfMax(renH), lastCpu, lastGpu, lastMem, lastFrame);
        head->SetText(b);
        auto pad60 = [](std::vector<double> v) { v.resize(60, 0.0); return v; };
        if (chart && !advH.empty()) {
            double mx = 20.0;
            for (double v : advH) mx = max(mx, v);
            for (double v : renH) mx = max(mx, v);
            chart->SetValueAxisRange(0.0, mx);                 // 固定轴：保证 16.7ms 参考线可见
            chart->UpdateSeriesValues(0, pad60(advH));
            chart->UpdateSeriesValues(1, pad60(renH));
        }
        if (chart2 && !cpuH.empty()) {                         // 自动量程：让低占用的变化也看得见
            double mx = 5.0;
            for (double v : cpuH) mx = max(mx, v);
            for (double v : gpuH) mx = max(mx, v);
            chart2->SetValueAxisRange(0.0, mx * 1.2);
            chart2->UpdateSeriesValues(0, pad60(cpuH));
            chart2->UpdateSeriesValues(1, pad60(gpuH));
        }
        if (chart3 && !memH.empty()) {                         // 量程贴合数据（内存变化相对总量很小）
            double lo = memH[0], hi = memH[0];
            for (double v : memH) { lo = min(lo, v); hi = max(hi, v); }
            double span = max(hi - lo, 0.5); lo -= span * 0.5; hi += span * 0.5;
            chart3->SetValueAxisRange(lo, hi);
            chart3->UpdateSeriesValues(0, pad60(memH));
        }
    }
};

// 通用文本查看窗口（快照差异等）
struct TextViewerWindow : Window {
    std::wstring text;
    void Build() {
        auto root = GetRootColumnBox(); root->SetMargin(Thickness(6, 6, 6, 6));
        root->AddChild(MakeLogEdit(text.empty() ? L"(空)" : text));
    }
};

// 远程改属性窗口（选中行）：文本 / 位置（边距）/ 可见
struct PropEditorWindow : Window {
    HWND target = nullptr;
    std::wstring id;
    bool vis = true;
    void Build() {
        auto root = GetRootColumnBox(); root->SetSpacing(8); root->SetMargin(Thickness(10, 10, 10, 10));
        root->AddChild(std::make_shared<Label>(L"控件: " + id + L"   （需目标已 SetDebugEnabled(true)）"));
        auto mkRow = []() { auto r = std::make_shared<RowBox>(); r->SetSpacing(6); r->SetHeight(32); return r; };
        auto r1 = mkRow();
        auto txt = std::make_shared<TextBox>(); txt->SetWidth(280); txt->SetPlaceholder(L"新文本");
        auto bTxt = std::make_shared<Button>(L"设文本");
        r1->AddChild(txt); r1->AddChild(bTxt); root->AddChild(r1);
        bTxt->Connect(bTxt->Clicked, [this, txt]() { SendDebugCmd(target, 6, id + L"\t" + txt->GetText()); });
        auto r2 = mkRow();
        auto pos = std::make_shared<TextBox>(); pos->SetWidth(160); pos->SetPlaceholder(L"x,y （边距）");
        auto bPos = std::make_shared<Button>(L"设位置");
        r2->AddChild(pos); r2->AddChild(bPos); root->AddChild(r2);
        bPos->Connect(bPos->Clicked, [this, pos]() {
            std::wstring s = pos->GetText();
            for (auto& c : s) if (c == L',') c = L'\t';
            SendDebugCmd(target, 21, id + L"\t" + s);
            });
        auto r3 = mkRow();
        auto bVis = std::make_shared<Button>(L"切换可见");
        r3->AddChild(bVis); root->AddChild(r3);
        bVis->Connect(bVis->Clicked, [this]() { vis = !vis; SendDebugCmd(target, 20, id + L"\t" + (vis ? L"1" : L"0")); });
    }
};

// ---- 快照存储：把「当前 Zufy 调试的所有消息」存成文件 ----
static std::wstring ExeDir() {
    wchar_t p[MAX_PATH] = {}; GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s = p; size_t k = s.find_last_of(L"\\/");
    return (k == std::wstring::npos) ? L"." : s.substr(0, k);
}
static std::wstring SnapDir() { return ExeDir() + L"\\snapshots"; }
static void EnsureSnapDir() { CreateDirectoryW(SnapDir().c_str(), nullptr); }
static std::wstring BuildSnapshotText(HWND target) {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t hb[256];
    swprintf(hb, 256, L"# ZufyUI 快照  %04d-%02d-%02d %02d:%02d:%02d   target=0x%llX\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, (unsigned long long)(uintptr_t)target);
    std::wstring t = hb;
    t += L"\n===== 帧统计 (cmd 3) =====\n" + SendDebugCmd(target, 3);
    t += L"\n===== 元素树 (cmd 4) =====\n" + SendDebugCmd(target, 4);
    t += L"\n===== 最近错误 (cmd 13) =====\n" + SendDebugCmd(target, 13);
    return t;
}
static std::wstring SaveSnapshot(HWND target) {
    EnsureSnapDir();
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t name[80];
    swprintf(name, 80, L"snap_%04d%02d%02d_%02d%02d%02d_%03d.txt",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring path = SnapDir() + L"\\" + name;
    std::wstring content = BuildSnapshotText(target);
    FILE* f = nullptr; _wfopen_s(&f, path.c_str(), L"wb");
    if (f) { const wchar_t bom = 0xFEFF; fwrite(&bom, sizeof(bom), 1, f); fwrite(content.c_str(), sizeof(wchar_t), content.size(), f); fclose(f); }
    return path;
}
static std::wstring ReadFileText(const std::wstring& path) {
    FILE* f = nullptr; _wfopen_s(&f, path.c_str(), L"rb");
    if (!f) return L"(读取失败)";
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    std::wstring s;
    if (n >= 2) {
        std::vector<char> buf((size_t)n); fread(buf.data(), 1, (size_t)n, f);
        const wchar_t* w = (const wchar_t*)buf.data(); s.assign(w, w + n / 2);
        if (!s.empty() && s[0] == 0xFEFF) s.erase(0, 1);
    }
    fclose(f); return s;
}
static void ListSnapshots(std::vector<std::wstring>& out) {
    out.clear();
    std::wstring pat = SnapDir() + L"\\*.txt";
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(SnapDir() + L"\\" + fd.cFileName); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    std::reverse(out.begin(), out.end());   // 新的在前
}
static std::shared_ptr<ScrollViewer> MakeViewer(std::shared_ptr<Label>& out, const std::wstring& init = L"…") {
    auto sv = std::make_shared<ScrollViewer>();
    sv->SetFillWidth(true); sv->SetVerticalStretchWeight(1.0f); sv->SetVerticalScrollEnabled(true);
    auto l = std::make_shared<Label>(init);
    l->SetTextOverflow(Label::TextOverflow::Wrap);
    l->SetAlignment(Label::HAlign::Left, Label::VAlign::Top);
    l->SetTextColor(Color::FromArgb(255, 30, 30, 30));
    sv->SetContent(l); out = l; return sv;
}
// 只读多行文本查看器（用 TextEdit：可选中/复制/滚动/换行）
static std::shared_ptr<TextEdit> MakeLogEdit(const std::wstring& init) {
    auto e = std::make_shared<TextEdit>();
    e->SetReadOnly(true);
    e->SetShowLineNumbers(false);
    e->SetShowNewlines(false);
    e->SetWrapMode(TextEdit::WrapMode::NoWrap);
    e->SetBaseFont([] { FontSpec f; f.familyName = L"Consolas"; f.size = 13.0f; return f; }());
    e->SetFillWidth(true); e->SetFillHeight(true);          // 必须：TextEdit 默认 width_=320，不 fill 会挤在左边
    e->SetVerticalStretchWeight(1.0f); e->SetHorizontalStretchWeight(1.0f);
    e->SetPlainText(init);
    return e;
}
static void SetEditPreserveScroll(const std::shared_ptr<TextEdit>& e, const std::wstring& s) {
    if (!e) return;
    float y = e->GetScrollOffsetY();
    e->SetPlainText(s);
    e->SetScrollOffsetY(y);
}
static std::vector<std::wstring> g_snapFiles;   // 快照文件路径（列表索引对应）

// ---- 结果窗口：UIA 树 / Zufy 调试树（各自独立的树 + 右键菜单，不互通）----
// UIA 控件详情：只用 UIA（不依赖 Zufy 调试通道）
struct UiaDetailWindow : Window {
    HWND target = nullptr;
    std::wstring id;
    std::shared_ptr<TextEdit> edit;
    void Build() {
        auto root = GetRootColumnBox(); root->SetMargin(Thickness(10, 10, 10, 10));
        edit = MakeLogEdit(); root->AddChild(edit);
        Refresh();
    }
    void Refresh() {
        if (!edit) return;
        IUIAutomation* uia = UiaCreateClient();
        IUIAutomationElement* el = UiaFindById(uia, target, id);
        std::wstring s;
        if (!el) s = L"(未找到元素 " + id + L")";
        else {
            BSTR nm = nullptr, aid = nullptr; CONTROLTYPEID ct = 0; RECT b = {}; BOOL en = TRUE;
            el->get_CurrentName(&nm); el->get_CurrentAutomationId(&aid);
            el->get_CurrentControlType(&ct); el->get_CurrentBoundingRectangle(&b); el->get_CurrentIsEnabled(&en);
            wchar_t buf[512];
            swprintf(buf, 512, L"名称: %s\r\nAutomationId: %s\r\n控件类型: %d\r\n可用: %s\r\n矩形: %d,%d %dx%d\r\n\r\n支持的模式: ",
                nm ? nm : L"", aid ? aid : L"", (int)ct, en ? L"是" : L"否",
                (int)b.left, (int)b.top, (int)(b.right - b.left), (int)(b.bottom - b.top));
            s = buf;
            const PATTERNID pats[] = { UIA_InvokePatternId, UIA_ValuePatternId, UIA_TogglePatternId, UIA_RangeValuePatternId, UIA_ExpandCollapsePatternId };
            const wchar_t* pn[] = { L"Invoke", L"Value", L"Toggle", L"RangeValue", L"ExpandCollapse" };
            for (int i = 0; i < 5; ++i) { IUnknown* p = nullptr; if (SUCCEEDED(el->GetCurrentPattern(pats[i], &p)) && p) { s += pn[i]; s += L"  "; p->Release(); } }
            if (nm) SysFreeString(nm); if (aid) SysFreeString(aid);
            el->Release();
        }
        if (uia) uia->Release();
        edit->SetPlainText(s);
    }
};

// UIA 窗口：独立窗口、独立的窗口选取下拉、独立的按钮与右键菜单；只做通用 UIA，不含 Zufy 专用功能
struct UiaWindow : Window {
    std::shared_ptr<ComboBox> combo;
    std::shared_ptr<TreeView> tree;
    std::shared_ptr<Label> status;
    std::vector<Target> targets;

    HWND Target() const { int i = combo ? combo->GetSelectedIndex() : -1; return (i >= 0 && i < (int)targets.size()) ? targets[i].hwnd : nullptr; }
    std::wstring SelectedId() const {
        auto s = tree ? tree->GetSelectedNode() : nullptr;
        return (s && s->columns.size() > 2 && s->columns[2]) ? s->columns[2]->GetText() : std::wstring();
    }
    void RefreshCombo() { targets = CollectTargets(false); FillTargetCombo(combo, targets); }
    void Dump() {
        HWND t = Target(); if (!t) { status->SetText(L"未选择窗口"); return; }
        UiaDump(t, tree);
        tree->SetHeaderLabels({ L"角色", L"名称", L"AutomationId", L"矩形" });
        status->SetText(L"UIA 树已抓取");
    }
    void Detail() {
        HWND t = Target(); std::wstring id = SelectedId();
        if (!t || id.empty()) { status->SetText(L"先在树里选一行"); return; }
        auto d = std::make_shared<UiaDetailWindow>(); d->target = t; d->id = id;
        if (!d->Create(440, 340, L"UIA 详情 - " + id)) return;
        ApplyDebugChrome(*d, L"UIA 详情"); d->Build(); d->Show(); g_extraWindows.push_back(d);
    }
    void Action(int kind) {
        HWND t = Target(); std::wstring id = SelectedId();
        if (!t || id.empty()) { status->SetText(L"先在树里选一行"); return; }
        IUIAutomation* uia = UiaCreateClient(); IUIAutomationElement* el = UiaFindById(uia, t, id);
        bool ok = false;
        if (el) {
            if (kind == 0) { IUnknown* pat = nullptr; if (SUCCEEDED(el->GetCurrentPattern(UIA_InvokePatternId, &pat)) && pat) { ((IUIAutomationInvokePattern*)pat)->Invoke(); pat->Release(); ok = true; } }
            else if (kind == 1) { IUnknown* pat = nullptr; if (SUCCEEDED(el->GetCurrentPattern(UIA_ValuePatternId, &pat)) && pat) { BSTR b = SysAllocString(L"UIA 设值"); ((IUIAutomationValuePattern*)pat)->SetValue(b); SysFreeString(b); pat->Release(); ok = true; } }
            else { ok = SUCCEEDED(el->SetFocus()); }
            el->Release();
        }
        if (uia) uia->Release();
        status->SetText(L"UIA(" + id + L") → " + (ok ? L"OK" : L"不支持/未找到"));
    }
    void Build() {
        auto root = GetRootColumnBox(); root->SetSpacing(10); root->SetMargin(Thickness(10, 10, 10, 10));

        auto top = std::make_shared<RowBox>(); top->SetSpacing(8); top->SetHeight(32);
        combo = std::make_shared<ComboBox>(); combo->SetWidth(560);
        auto bR = std::make_shared<Button>(L"刷新窗口"); bR->SetIcon(Icon::Refresh);
        auto bD = std::make_shared<Button>(L"抓取 UIA 树"); bD->SetIcon(Icon::AllApps);
        top->AddChild(combo); top->AddChild(bR); top->AddChild(bD); root->AddChild(top);
        bR->Connect(bR->Clicked, [this]() { RefreshCombo(); status->SetText(L"已刷新窗口列表（全部窗口）"); });
        bD->Connect(bD->Clicked, [this]() { Dump(); });

        auto srow = std::make_shared<RowBox>(); srow->SetSpacing(8); srow->SetHeight(30);
        auto search = std::make_shared<TextBox>(); search->SetWidth(360); search->SetPlaceholder(L"搜索：名称 / AutomationId / 角色");
        srow->AddChild(search); root->AddChild(srow);

        tree = std::make_shared<TreeView>();
        tree->SetFillWidth(true); tree->SetVerticalStretchWeight(1.0f); tree->SetRootDecorated(true);
        tree->SetColumnCount(4);
        tree->SetHeaderLabels({ L"角色", L"名称", L"AutomationId", L"矩形" });
        tree->SetColumnWidth(0, 150); tree->SetColumnWidth(1, 520); tree->SetColumnWidth(2, 130); tree->SetColumnWidth(3, 180);
        root->AddChild(tree);
        search->TextChanged.connect([this](const std::wstring& s) { tree->Search(s); if (!s.empty()) tree->ExpandAll(); });

        status = std::make_shared<Label>(L"UIA 就绪"); status->SetTextColor(Color::FromArgb(255, 0, 100, 180)); status->SetHeight(24);
        root->AddChild(status);

        auto menu = std::make_shared<Menu>();
        menu->AddItem(L"控件详情（UIA）", [this]() { Detail(); });
        menu->AddSeparator();
        menu->AddItem(L"UIA：Invoke", [this]() { Action(0); });
        menu->AddItem(L"UIA：设值", [this]() { Action(1); });
        menu->AddItem(L"UIA：聚焦", [this]() { Action(2); });
        SetContextMenu(menu);

        tree->Connect(tree->ItemDoubleClicked, [this](std::shared_ptr<TreeNode> n) {
            if (n && n->columns.size() > 2 && n->columns[2]) Detail();
            });
        RefreshCombo();
    }
};

// ---- 性能窗口 ----
struct PerfWindow : Window {
    HWND target = nullptr;
    std::shared_ptr<PerfView> view = std::make_shared<PerfView>();
    std::shared_ptr<Timer> timer;
    void Build() {
        SetMinSize(360, 240);   // 显式最小尺寸：不让顶部那行长文本把窗口撑大
        auto root = GetRootColumnBox(); root->SetMargin(Thickness(10, 10, 10, 10));
        root->AddChild(view->Build());
        view->Refresh(target);
        timer = CreateTimer(250);
        if (timer) timer->Tick.connect([this]() { view->Refresh(target); });
    }
};

// ---- 错误窗口 ----
struct ErrorWindow : Window {
    HWND target = nullptr;
    std::shared_ptr<TextEdit> edit;
    std::shared_ptr<Timer> timer;
    void Build() {
        auto root = GetRootColumnBox(); root->SetSpacing(8); root->SetMargin(Thickness(10, 10, 10, 10));
        auto bar = std::make_shared<RowBox>(); bar->SetSpacing(8); bar->SetHeight(30);
        auto bC = std::make_shared<Button>(L"清空目标错误"); bC->SetIcon(Icon::Refresh);
        bar->AddChild(bC); root->AddChild(bar);
        edit = MakeLogEdit(FormatErrors(target)); root->AddChild(edit);
        bC->Connect(bC->Clicked, [this]() { SendDebugCmd(target, 14); edit->SetPlainText(FormatErrors(target)); });
        timer = CreateTimer(600);
        if (timer) timer->Tick.connect([this]() { if (edit) edit->SetPlainText(FormatErrors(target)); });
    }
};

static std::wstring g_snapshot;   // 快照：序列化后的结果树

// ---- 实时调试树：每 250ms 重抓 cmd 4，新增=浅绿(1s)、刚删=浅红(1s)、删除>1s/其子树=灰 ----
struct LiveDebug {
    std::shared_ptr<TreeView> tree;
    HWND target = nullptr;
    std::shared_ptr<Label> status;
    std::map<std::wstring, std::shared_ptr<TreeNode>> nodeMap;   // key -> 节点
    std::map<std::wstring, ULONGLONG> addedAt, deletedAt;
    std::wstring lastDump_;

    static std::wstring Key(int d, const std::vector<std::wstring>& c) {
        std::wstring k = std::to_wstring(d);
        for (auto& s : c) { k += L'\x1f'; k += s; }
        return k;
    }
    void Refresh() {
        if (!tree || !target) return;
        std::wstring r = SendDebugCmd(target, 4);
        // 无变化且没有待过渡（新删染色）→ 直接跳过，避免每帧重建树 + 重绘（Debugger/目标都省）
        if (r == lastDump_ && addedAt.empty() && deletedAt.empty()) return;
        lastDump_ = r;
        std::vector<int> depth; std::vector<std::vector<std::wstring>> rows;
        size_t i = 0;
        while (i < r.size()) {
            size_t j = r.find(L'\n', i);
            std::wstring line = r.substr(i, (j == std::wstring::npos ? r.size() : j) - i);
            i = (j == std::wstring::npos) ? r.size() : j + 1;
            while (!line.empty() && line.back() == L'\r') line.pop_back();
            if (line.empty()) continue;
            int d = IndentOf(line); std::wstring body = line.substr((size_t)d * 2);
            std::vector<std::wstring> cs; size_t p = 0;
            while (true) { size_t q = body.find(L'\t', p); if (q == std::wstring::npos) { cs.push_back(body.substr(p)); break; } cs.push_back(body.substr(p, q - p)); p = q + 1; }
            while (cs.size() < 4) cs.push_back(L"");
            depth.push_back(d); rows.push_back(cs);
        }
        ULONGLONG now = GetTickCount64();
        std::vector<std::shared_ptr<TreeNode>> stack;
        std::set<std::wstring> seen;
        for (size_t k = 0; k < depth.size(); ++k) {
            int d = depth[k]; auto& cs = rows[k];
            std::wstring key = Key(d, cs);
            std::shared_ptr<TreeNode> node;
            auto it = nodeMap.find(key);
            if (it != nodeMap.end()) node = it->second;
            else {
                node = (d == 0 || stack.empty()) ? tree->AddRoot(cs[0]) : tree->AddChild(stack[d - 1], cs[0]);
                for (int c = 1; c < 4; ++c) tree->SetNodeText(node, c, cs[c]);
                nodeMap[key] = node; addedAt[key] = now;
            }
            if (d == 0) stack.assign(1, node);
            else { if (d > (int)stack.size()) d = (int)stack.size(); stack.resize((size_t)d); stack.push_back(node); }
            for (auto& anc : stack) if (anc) anc->expanded = true;   // 新链展开
            seen.insert(key);
        }
        for (auto& kv : nodeMap) {
            if (seen.count(kv.first)) { if (deletedAt.erase(kv.first)) addedAt[kv.first] = now; }
            else if (!deletedAt.count(kv.first)) deletedAt[kv.first] = now;
        }
        std::shared_ptr<TreeNode> sel = tree->GetSelectedNode();
        std::vector<std::wstring> toRemove;
        for (auto& kv : nodeMap) {
            auto node = kv.second;
            Color c = Color::FromArgb(0, 0, 0, 0);
            auto itd = deletedAt.find(kv.first);
            if (itd != deletedAt.end()) {
                if (now - itd->second <= 1000) c = Color::FromArgb(255, 255, 200, 200);   // 刚删：浅红 1s
                else {
                    // 删除>1s：只有「选中节点在本节点子树内」才保留为灰（因为要保住选中的子元素），否则从树里删掉
                    bool keep = false;
                    for (auto p = sel; p; p = p->parent.lock()) if (p.get() == node.get()) { keep = true; break; }
                    if (!keep) { toRemove.push_back(kv.first); continue; }
                    c = Color::FromArgb(255, 212, 212, 212);
                }
            }
            else {
                auto ita = addedAt.find(kv.first);
                if (ita != addedAt.end()) { if (now - ita->second <= 1000) c = Color::FromArgb(255, 200, 235, 200); else addedAt.erase(kv.first); }
            }
            node->bgColor = c;
        }
        for (auto& key : toRemove) {
            auto it = nodeMap.find(key); if (it == nodeMap.end()) continue;
            tree->RemoveNode(it->second);
            nodeMap.erase(it); addedAt.erase(key); deletedAt.erase(key);
        }
        tree->RequestRepaint();
        if (status) status->SetText(L"实时调试树（250ms；新增=浅绿 / 刚删=浅红 / 删除>1s=灰）");
    }
};

// ---- Top-N 视图（横向条形图）----
struct TopNView {
    std::function<HWND()> getTarget;
    int kind = 1;   // 1 重绘 2 布局 3 缓存
    std::shared_ptr<BarChart> chart; std::shared_ptr<Label> status;
    std::vector<std::wstring> ids, lastNames; std::vector<double> lastVals;
    void Refresh() {
        HWND target = getTarget ? getTarget() : nullptr;
        if (!chart || !target) return;
        std::wstring text = SendDebugCmd(target, kind == 1 ? 15 : kind == 2 ? 16 : 17);
        std::vector<std::wstring> names, nids; std::vector<double> vals;
        ParseTopN(text, names, vals, nids);
        if (names == lastNames && vals == lastVals) return;   // 无变化不重播动画
        lastNames = names; lastVals = vals; ids = nids;
        chart->ClearData();
        chart->AddSeries(kind == 3 ? L"KB" : L"次数");
        chart->SetSeriesColor(0, kind == 1 ? Color::FromArgb(255, 60, 140, 220) : kind == 2 ? Color::FromArgb(255, 220, 120, 40) : Color::FromArgb(255, 90, 170, 90));
        for (auto& n : names) chart->AddCategory(n);
        chart->SetSeriesValues(0, vals);
        if (status) {
            wchar_t b[160];
            swprintf(b, 160, L"%s Top-N（%d 项，点击条形高亮对应元素）",
                kind == 1 ? L"重绘热点" : kind == 2 ? L"布局风暴" : L"缓存占用", (int)names.size());
            status->SetText(b);
        }
    }
    std::shared_ptr<UIElement> Build(std::function<HWND()> fn) {
        getTarget = fn;
        auto col = std::make_shared<ColumnBox>(); col->SetSpacing(6);
        col->SetFillHeight(true); col->SetVerticalStretchWeight(1.0f);   // 铺满可用高度
        chart = std::make_shared<BarChart>();
        chart->SetOrientation(BarChart::Orientation::Horizontal);
        chart->SetWidth(200.0f); chart->SetHeight(80.0f);      // 最小尺寸调小，窗口可缩得更小
        chart->SetFillWidth(true); chart->SetVerticalStretchWeight(1.0f);
        chart->SetShowLegend(false); chart->SetShowVGrid(false); chart->SetShowValues(true);
        chart->SetCategoryWidth(24.0f); chart->SetFitPlotSize(true); chart->SetPlotHeight(360.0f); chart->SetCornerRadius(0.0f);
        chart->SetValueTickCount(4); chart->SetBarGap(0.2f);
        chart->SetAnimationEnabled(false);   // 统计条关闭进场动画
        if (kind == 3) chart->SetValueFormat(1, L"", L" KB");
        chart->CategoryClicked.connect([this](int c) { HWND t = getTarget ? getTarget() : nullptr; if (t && c >= 0 && c < (int)ids.size() && !ids[c].empty()) SendDebugCmd(t, 11, ids[c]); });
        col->AddChild(chart);
        status = std::make_shared<Label>(L"Top-N"); status->SetTextColor(Color::FromArgb(255, 0, 100, 180)); status->SetHeight(22);
        col->AddChild(status);
        Refresh();
        return col;
    }
};

// ---- Top-N 独立窗口 ----
struct TopNWindow : Window {
    HWND target = nullptr; int kind = 1;
    std::shared_ptr<TopNView> view = std::make_shared<TopNView>();
    void Build() {
        SetMinSize(360, 240);
        view->kind = kind;
        auto root = GetRootColumnBox(); root->SetSpacing(8); root->SetMargin(Thickness(10, 10, 10, 10));
        auto bar = std::make_shared<RowBox>(); bar->SetSpacing(8); bar->SetHeight(30);
        auto bR = std::make_shared<Button>(L"刷新"); bR->SetIcon(Icon::Refresh); bar->AddChild(bR); root->AddChild(bar);
        bR->Connect(bR->Clicked, [this]() { view->Refresh(); });
        HWND tg = target;
        root->AddChild(view->Build([tg]() { return tg; }));
    }
};

// ---- 控件类型统计视图（PieChart）----

// ---- 实时调试树独立窗口 ----
struct DebugTreeWindow : Window {
    HWND target = nullptr;
    std::shared_ptr<LiveDebug> live;
    std::shared_ptr<Timer> timer;
    void Build() {
        live = std::make_shared<LiveDebug>();
        auto root = GetRootColumnBox(); root->SetSpacing(8); root->SetMargin(Thickness(10, 10, 10, 10));
        auto bar = std::make_shared<RowBox>(); bar->SetSpacing(8); bar->SetHeight(30);
        auto bR = std::make_shared<Button>(L"刷新"); bR->SetIcon(Icon::Refresh);
        bar->AddChild(bR); root->AddChild(bar);
        bR->Connect(bR->Clicked, [this]() { live->Refresh(); });
        live->tree = std::make_shared<TreeView>();
        live->tree->SetFillWidth(true); live->tree->SetVerticalStretchWeight(1.0f); live->tree->SetRootDecorated(true);
        live->tree->SetColumnCount(4);
        live->tree->SetHeaderLabels({ L"类型", L"名称", L"AutomationId", L"矩形" });
        live->tree->SetColumnWidth(0, 150); live->tree->SetColumnWidth(1, 480); live->tree->SetColumnWidth(2, 130); live->tree->SetColumnWidth(3, 160);
        root->AddChild(live->tree);
        live->status = std::make_shared<Label>(L"实时调试树"); live->status->SetTextColor(Color::FromArgb(255, 0, 100, 180)); live->status->SetHeight(24);
        root->AddChild(live->status);
        live->target = target;
        timer = CreateTimer(250);
        if (timer) timer->Tick.connect([this]() { live->Refresh(); });
        live->Refresh();
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Window win;
    if (!win.Create(1100, 740, L"ZufyUI Debugger")) return 1;
    win.SetMinSize(860, 540);
    win.SetBackdrop(Backdrop::None, 0x00000000);
    win.SetBackgroundColor(Color::FromArgb(255, 243, 243, 243));
    win.SetAccessibilityEnabled(true);
    ApplyDebugChrome(win, L"ZufyUI Debugger");

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);
    root->SetMargin(Thickness(12, 12, 12, 12));

    auto mkBtn = [](Icon ic, const std::wstring& t) { auto b = std::make_shared<Button>(t); b->SetIcon(ic); return b; };
    auto mkRow = []() { auto r = std::make_shared<RowBox>(); r->SetSpacing(8); r->SetHeight(30); return r; };

    auto status = std::make_shared<Label>(L"就绪");
    status->SetTextColor(Color::FromArgb(255, 0, 100, 180));
    status->SetHeight(26);

    // ===== 行 1：下拉框 + 刷新窗口 + UIA 工具 =====
    auto combo = std::make_shared<ComboBox>(); combo->SetWidth(560);
    auto target = [combo]() { return SelectedTarget(combo); };
    auto row1 = mkRow(); row1->SetHeight(32);
    row1->AddChild(combo);
    auto bList = mkBtn(Icon::Refresh, L"刷新窗口");
    auto bUiaW = mkBtn(Icon::AllApps, L"UIA 工具");
    row1->AddChild(bList); row1->AddChild(bUiaW);
    root->AddChild(row1);

    // ===== 行 2：其余按钮（横向可滚动）=====
    auto hscroll = std::make_shared<ScrollViewer>();
    hscroll->SetHeight(40); hscroll->SetHorizontalScrollEnabled(true); hscroll->SetVerticalScrollEnabled(false);
    hscroll->SetMargin(Thickness(0, 4, 0, 4));
    auto hrow = mkRow(); hrow->SetHeight(30);
    auto bSpy = std::make_shared<Button>(L"拾取跟随：关"); bSpy->SetIcon(Icon::View);
    auto bFollow = std::make_shared<Button>(L"详情跟随：关"); bFollow->SetIcon(Icon::View);
    auto bShot = mkBtn(Icon::Save, L"截图");
    auto bRefresh = mkBtn(Icon::Refresh, L"刷新调试树");
    auto bExport = mkBtn(Icon::Save, L"导出调试树");
    auto bReset = mkBtn(Icon::Refresh, L"重置计数");
    hrow->AddChild(bSpy); hrow->AddChild(bFollow); hrow->AddChild(bShot);
    hrow->AddChild(bRefresh); hrow->AddChild(bExport); hrow->AddChild(bReset);
    hscroll->SetContent(hrow);
    root->AddChild(hscroll);

    auto tabs = std::make_shared<TabView>(); tabs->SetVerticalStretchWeight(1.0f); root->AddChild(tabs);

    // ===== Tab：快照列表 =====
    auto listPage = std::make_shared<ColumnBox>(); listPage->SetSpacing(8); listPage->SetMargin(Thickness(6, 6, 6, 6));
    auto sbar = mkRow();
    auto bNew = mkBtn(Icon::Save, L"新建快照");
    auto bOpen = mkBtn(Icon::View, L"打开 / 浏览");
    auto bDel = mkBtn(Icon::Refresh, L"删除");
    auto bSnapRefresh = mkBtn(Icon::Refresh, L"刷新列表");
    auto bSnapWin = mkBtn(Icon::View, L"独立窗口");
    sbar->AddChild(bNew); sbar->AddChild(bOpen); sbar->AddChild(bDel); sbar->AddChild(bSnapRefresh); sbar->AddChild(bSnapWin);
    listPage->AddChild(sbar);
    auto snapList = std::make_shared<ListView>(); snapList->SetVerticalStretchWeight(1.0f);
    listPage->AddChild(snapList);
    tabs->AddTab(L"快照列表", listPage);

    // ===== Tab：浏览快照 =====
    auto browsePage = std::make_shared<ColumnBox>(); browsePage->SetSpacing(8); browsePage->SetMargin(Thickness(6, 6, 6, 6));
    auto bBrowseWin = mkBtn(Icon::View, L"独立窗口");
    auto browsetop = mkRow(); browsetop->AddChild(bBrowseWin); browsePage->AddChild(browsetop);
    std::shared_ptr<TextEdit> browseLabel = MakeLogEdit(L"（在「快照列表」里选一个快照后这里显示内容）");
    browsePage->AddChild(browseLabel);
    tabs->AddTab(L"浏览快照", browsePage);

    // ===== Tab：Zufy 调试（实时）=====
    auto dbgPage = std::make_shared<ColumnBox>(); dbgPage->SetSpacing(8); dbgPage->SetMargin(Thickness(6, 6, 6, 6));
    auto dbgHead = std::make_shared<Label>(L"Zufy 调试（实时）：新增=浅绿 / 刚删=浅红 / 删除>1s=灰；右键操作");
    dbgHead->SetTextColor(Color::FromArgb(255, 0, 100, 180)); dbgHead->SetHeight(26);
    dbgPage->AddChild(dbgHead);
    auto ztop = mkRow();
    auto dsearch = std::make_shared<TextBox>(); dsearch->SetWidth(380); dsearch->SetPlaceholder(L"搜索：类型 / 名称 / AutomationId");
    ztop->AddChild(dsearch); dbgPage->AddChild(ztop);
    auto live = std::make_shared<LiveDebug>();
    live->tree = std::make_shared<TreeView>();
    live->tree->SetFillWidth(true); live->tree->SetVerticalStretchWeight(1.0f); live->tree->SetRootDecorated(true);
    live->tree->SetColumnCount(4);
    live->tree->SetHeaderLabels({ L"类型", L"名称", L"AutomationId", L"矩形" });
    live->tree->SetColumnWidth(0, 150); live->tree->SetColumnWidth(1, 500); live->tree->SetColumnWidth(2, 130); live->tree->SetColumnWidth(3, 170);
    dbgPage->AddChild(live->tree);
    dsearch->TextChanged.connect([tree = live->tree](const std::wstring& s) { tree->Search(s); if (!s.empty()) tree->ExpandAll(); });
    tabs->AddTab(L"Zufy 调试", dbgPage);

    // ===== 5 个功能 Tab（每个含「独立窗口」按钮）=====
    std::shared_ptr<PerfView> perfView;
    std::shared_ptr<TextEdit> errView;
    auto tabPerf = std::make_shared<ColumnBox>(); tabPerf->SetSpacing(8); tabPerf->SetMargin(Thickness(6, 6, 6, 6));
    {
        auto r = mkRow(); auto b = mkBtn(Icon::View, L"独立窗口"); r->AddChild(b);
        tabPerf->AddChild(r); perfView = std::make_shared<PerfView>(); tabPerf->AddChild(perfView->Build());
        b->Connect(b->Clicked, [target, status]() {
            HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
            auto w = std::make_shared<PerfWindow>(); w->target = t;
            if (!w->Create(560, 460, L"性能（FPS / 帧耗时）")) return;
            ApplyDebugChrome(*w, L"性能（FPS / 帧耗时）"); w->Build(); w->Show(); g_extraWindows.push_back(w);
            });
        tabs->AddTab(L"性能", tabPerf);
    }
    auto tabErr = std::make_shared<ColumnBox>(); tabErr->SetSpacing(8); tabErr->SetMargin(Thickness(6, 6, 6, 6));
    {
        auto r = mkRow(); auto b = mkBtn(Icon::View, L"独立窗口"); r->AddChild(b);
        tabErr->AddChild(r); errView = MakeLogEdit(L"（切换到本页自动刷新）"); tabErr->AddChild(errView);
        b->Connect(b->Clicked, [target, status]() {
            HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
            auto w = std::make_shared<ErrorWindow>(); w->target = t;
            if (!w->Create(560, 460, L"错误（目标 + 本调试器）")) return;
            ApplyDebugChrome(*w, L"错误（目标 + 本调试器）"); w->Build(); w->Show(); g_extraWindows.push_back(w);
            });
        tabs->AddTab(L"错误", tabErr);
    }

    std::shared_ptr<TopNView> topViews[3];
    auto mkTopTab = [&](const wchar_t* title, int kind) {
        auto page = std::make_shared<ColumnBox>(); page->SetSpacing(8); page->SetMargin(Thickness(6, 6, 6, 6));
        auto r = mkRow(); auto b = mkBtn(Icon::View, L"独立窗口"); r->AddChild(b); page->AddChild(r);
        auto v = std::make_shared<TopNView>(); v->kind = kind + 1;
        page->AddChild(v->Build(target));
        topViews[kind] = v;
        tabs->AddTab(title, page);
        b->Connect(b->Clicked, [target, status, kind]() {
            HWND tg = target(); if (!tg) { status->SetText(L"未选择目标窗口"); return; }
            auto w = std::make_shared<TopNWindow>(); w->target = tg; w->kind = kind + 1;
            if (!w->Create(760, 560, L"Top-N")) return;
            ApplyDebugChrome(*w, L"Top-N"); w->Build(); w->Show(); g_extraWindows.push_back(w);
            });
        };
    mkTopTab(L"重绘", 0); mkTopTab(L"布局", 1); mkTopTab(L"缓存", 2);
    auto fillTop = [topViews](int kind) { if (topViews[kind]) topViews[kind]->Refresh(); };

    root->AddChild(status);

    RefreshTargets(combo, true);
    InstallErrorSubs(win);

    auto selectedId = [live]() -> std::wstring {
        auto s = live->tree->GetSelectedNode();
        return (s && s->columns.size() > 2 && s->columns[2]) ? s->columns[2]->GetText() : std::wstring();
        };
    live->target = 0;

    // ---- 快照列表逻辑 ----
    auto refreshSnaps = [snapList]() {
        ListSnapshots(g_snapFiles);
        snapList->Clear();
        if (g_snapFiles.empty()) snapList->AddItem(L"(还没有快照，点「新建快照」)");
        else for (auto& p : g_snapFiles) { size_t k = p.find_last_of(L"\\/"); snapList->AddItem(k == std::wstring::npos ? p : p.substr(k + 1)); }
        };
    refreshSnaps();
    snapList->SelectionChanged.connect([browseLabel](int idx) {
        if (idx >= 0 && idx < (int)g_snapFiles.size()) browseLabel->SetPlainText(ReadFileText(g_snapFiles[idx]));
        });
    bNew->Connect(bNew->Clicked, [target, browseLabel, refreshSnaps, status]() {
        HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
        std::wstring path = SaveSnapshot(t); refreshSnaps();
        browseLabel->SetPlainText(ReadFileText(path)); status->SetText(L"已保存快照：" + path);
        });
    bOpen->Connect(bOpen->Clicked, [snapList, browseLabel, status]() {
        int i = snapList->GetSelectedIndex();
        if (i < 0 || i >= (int)g_snapFiles.size()) { status->SetText(L"先在列表里选一个快照"); return; }
        browseLabel->SetPlainText(ReadFileText(g_snapFiles[i])); status->SetText(L"已打开 " + g_snapFiles[i]);
        });
    bDel->Connect(bDel->Clicked, [snapList, refreshSnaps, status]() {
        int i = snapList->GetSelectedIndex();
        if (i < 0 || i >= (int)g_snapFiles.size()) { status->SetText(L"先在列表里选一个快照"); return; }
        DeleteFileW(g_snapFiles[i].c_str()); refreshSnaps(); status->SetText(L"已删除该快照");
        });
    bSnapRefresh->Connect(bSnapRefresh->Clicked, [refreshSnaps, status]() { refreshSnaps(); status->SetText(L"已刷新快照列表"); });
    bSnapWin->Connect(bSnapWin->Clicked, [status]() {
        auto w = std::make_shared<TextViewerWindow>();
        ListSnapshots(g_snapFiles);
        std::wstring s = L"快照目录: " + SnapDir() + L"\r\n\r\n";
        if (g_snapFiles.empty()) s += L"(无快照)";
        else for (auto& p : g_snapFiles) { size_t k = p.find_last_of(L"\\/"); s += (k == std::wstring::npos ? p : p.substr(k + 1)); s += L"\r\n"; }
        w->text = s;
        if (!w->Create(620, 420, L"快照列表")) return;
        ApplyDebugChrome(*w, L"快照列表"); w->Build(); w->Show(); g_extraWindows.push_back(w);
        status->SetText(L"快照列表窗口已打开");
        });
    bBrowseWin->Connect(bBrowseWin->Clicked, [browseLabel, status]() {
        auto w = std::make_shared<TextViewerWindow>();
        w->text = browseLabel ? browseLabel->GetText() : L"(空)";
        if (!w->Create(700, 560, L"浏览快照")) return;
        ApplyDebugChrome(*w, L"浏览快照"); w->Build(); w->Show(); g_extraWindows.push_back(w);
        status->SetText(L"浏览快照窗口已打开");
        });


    // ---- 行 2 / 行 1 按钮 ----
    bList->Connect(bList->Clicked, [combo, status]() { RefreshTargets(combo, true); status->SetText(L"已刷新窗口列表（仅 ZufyUI 窗口）"); });
    bUiaW->Connect(bUiaW->Clicked, [status]() {
        auto w = std::make_shared<UiaWindow>();
        if (!w->Create(880, 640, L"UIA 工具（任意窗口）")) return;
        ApplyDebugChrome(*w, L"UIA 工具（任意窗口）"); w->Build(); w->Show(); g_extraWindows.push_back(w);
        status->SetText(L"UIA 窗口已打开（可选取任意窗口）");
        });
    bSpy->Connect(bSpy->Clicked, [bSpy, target, status]() {
        g_spyOn = !g_spyOn;
        if (!g_spyOn) { HWND t = target(); if (t) SendDebugCmd(t, 12); }
        bSpy->SetText(g_spyOn ? L"拾取跟随：开" : L"拾取跟随：关");
        status->SetText(g_spyOn ? L"拾取跟随已开（把鼠标移到目标窗口上）" : L"拾取跟随已关");
        });
    bFollow->Connect(bFollow->Clicked, [bFollow, target, status]() {
        HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
        if (!g_followDetail || !IsWindow(g_followDetail->GetHwnd())) {
            auto dw = std::make_shared<ControlDetailWindow>(); dw->target = t; dw->follow = true;
            if (!dw->Create(460, 500, L"控件详情 - 跟随鼠标")) return;
            ApplyDebugChrome(*dw, L"控件详情 - 跟随鼠标"); dw->Build(); dw->Show(); g_details.push_back(dw); g_followDetail = dw;
            bFollow->SetText(L"详情跟随：开"); status->SetText(L"详情跟随已开");
        }
        else {
            g_followDetail->follow = !g_followDetail->follow; g_followDetail->target = t;
            bFollow->SetText(g_followDetail->follow ? L"详情跟随：开" : L"详情跟随：关");
            status->SetText(g_followDetail->follow ? L"详情跟随已开" : L"详情跟随已关");
        }
        });
    bShot->Connect(bShot->Clicked, [target, status, &win]() {
        HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
        FileDialogOptions o; o.title = L"截图保存为 PNG"; o.defaultFileName = L"zufyui_shot"; o.defaultExtension = L"png";
        o.filters = { { L"PNG 图片", { L"*.png" } } };
        auto path = FileDialog::SaveOne(win.GetHwnd(), o); if (!path) return;
        status->SetText(CaptureTargetPng(t, *path) ? (L"已截图：" + *path) : L"截图失败（窗口不可见？）");
        });
    bRefresh->Connect(bRefresh->Clicked, [live]() { live->Refresh(); });
    bReset->Connect(bReset->Clicked, [target, status]() { if (target()) SendDebugCmd(target(), 19); status->SetText(L"已重置目标调试计数"); });
    bExport->Connect(bExport->Clicked, [live, status, &win]() {
        FileDialogOptions o; o.title = L"导出调试树"; o.defaultFileName = L"zufyui_tree"; o.defaultExtension = L"txt";
        o.filters = { { L"文本", { L"*.txt" } } };
        auto path = FileDialog::SaveOne(win.GetHwnd(), o); if (!path) return;
        FILE* f = nullptr; _wfopen_s(&f, path->c_str(), L"wb");
        if (!f) { status->SetText(L"保存失败"); return; }
        const wchar_t bom = 0xFEFF; fwrite(&bom, sizeof(bom), 1, f);
        std::function<void(std::shared_ptr<TreeNode>)> rec = [&](std::shared_ptr<TreeNode> n) {
            if (!n) return;
            for (int c = 0; c < 4; ++c) { std::wstring cell; if (c < (int)n->columns.size() && n->columns[c]) cell = n->columns[c]->GetText(); fwrite(cell.c_str(), sizeof(wchar_t), cell.size(), f); if (c < 3) { const wchar_t tab = L'\t'; fwrite(&tab, sizeof(tab), 1, f); } }
            const wchar_t nl[2] = { L'\r', L'\n' }; fwrite(nl, sizeof(wchar_t), 2, f);
            for (auto& ch : n->children) rec(ch);
            };
        for (int i = 0; i < live->tree->GetRootCount(); ++i) rec(live->tree->GetRootAt(i));
        fclose(f); status->SetText(L"已导出：" + *path);
        });

    // ---- Zufy 专用右键菜单 ----
    auto dmenu = std::make_shared<Menu>();
    auto openDetail = [target, selectedId, status]() {
        std::wstring id = selectedId(); HWND t = target(); if (id.empty() || !t) { status->SetText(L"先选一行"); return; }
        auto dw = std::make_shared<ControlDetailWindow>(); dw->target = t; dw->id = id;
        if (!dw->Create(460, 500, L"控件详情 - " + id)) return;
        ApplyDebugChrome(*dw, L"控件详情 - " + id); dw->Build(); dw->Show(); g_details.push_back(dw);
        };
    dmenu->AddItem(L"控件详情", [openDetail]() { openDetail(); });
    auto hlOn = std::make_shared<bool>(false);
    dmenu->AddCheckItemCb(L"目标内高亮（绿框）", false, [target, selectedId, status, hlOn](bool on) {
        *hlOn = on;
        HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
        if (!on) { SendDebugCmd(t, 12); status->SetText(L"已清除高亮"); return; }
        std::wstring id = selectedId(); if (id.empty()) { status->SetText(L"先选一行再勾选"); return; }
        SendDebugCmd(t, 11, id); status->SetText(L"已高亮 " + id + L"（绿框）");
        });
    dmenu->AddItem(L"高亮颜色…", [target, status, &win]() {
        auto c = ColorDialog::Pick(win.GetHwnd(), Color(0.18f, 0.80f, 0.44f, 1.0f));
        if (!c) return;
        HWND t = target(); if (!t) { status->SetText(L"未选择目标窗口"); return; }
        wchar_t buf[16]; swprintf(buf, 16, L"%02X%02X%02X",
            (unsigned)(max(0.0f, min(1.0f, c->r)) * 255 + 0.5f),
            (unsigned)(max(0.0f, min(1.0f, c->g)) * 255 + 0.5f),
            (unsigned)(max(0.0f, min(1.0f, c->b)) * 255 + 0.5f));
        SendDebugCmd(t, 22, buf);
        status->SetText(L"已设置高亮颜色（拾取/高亮共用）");
        });
    dmenu->AddItem(L"远程改属性（文本/位置/可见）", [target, selectedId, status]() {
        std::wstring id = selectedId(); HWND t = target(); if (id.empty() || !t) { status->SetText(L"先选一行"); return; }
        auto pw = std::make_shared<PropEditorWindow>(); pw->target = t; pw->id = id;
        if (!pw->Create(470, 260, L"远程改属性 - " + id)) return;
        ApplyDebugChrome(*pw, L"远程改属性 - " + id); pw->Build(); pw->Show(); g_extraWindows.push_back(pw);
        });

    // 各 tab 的右键菜单（快照/浏览=快照菜单；Zufy 调试=Zufy 菜单；功能页=各自的打开窗口）
    auto smenu = std::make_shared<Menu>();
    smenu->AddItem(L"新建快照", [bNew]() { bNew->Clicked.Fire(); });
    smenu->AddItem(L"打开 / 浏览", [bOpen]() { bOpen->Clicked.Fire(); });
    smenu->AddItem(L"删除", [bDel]() { bDel->Clicked.Fire(); });
    smenu->AddItem(L"刷新列表", [bSnapRefresh]() { bSnapRefresh->Clicked.Fire(); });
    smenu->AddItem(L"独立窗口", [bSnapWin]() { bSnapWin->Clicked.Fire(); });

    auto pageMenus = std::make_shared<std::vector<std::shared_ptr<Menu>>>();
    pageMenus->push_back(smenu);   // 快照列表
    pageMenus->push_back(smenu);   // 浏览快照
    pageMenus->push_back(dmenu);   // Zufy 调试
    pageMenus->push_back(nullptr); // 性能
    pageMenus->push_back(nullptr); // 错误
    pageMenus->push_back(dmenu);   // 重绘
    pageMenus->push_back(dmenu);   // 布局
    pageMenus->push_back(dmenu);   // 缓存
    win.SetContextMenu(smenu);
    auto refreshActive = [tabs, target, live, perfView, errView, topViews, fillTop]() {
        int idx = tabs->GetSelectedIndex(); HWND t = target(); if (!t) return;
        if (idx == 2) { live->target = t; live->Refresh(); }
        else if (idx == 3 && perfView) perfView->Refresh(t);
        else if (idx == 4 && errView) SetEditPreserveScroll(errView, FormatErrors(t));
        else if (idx == 5) fillTop(0);
        else if (idx == 6) fillTop(1);
        else if (idx == 7) fillTop(2);
        };
    tabs->SelectionChanged.connect([&win, pageMenus, refreshActive](int idx) {
        if (idx >= 0 && idx < (int)pageMenus->size() && (*pageMenus)[idx]) win.SetContextMenu((*pageMenus)[idx]);
        refreshActive();
        });

    // 双击 Zufy 调试树 → 详情
    live->tree->Connect(live->tree->ItemDoubleClicked, [openDetail](std::shared_ptr<TreeNode> n) { (void)n; if (n) openDetail(); });

    // ---- 定时器：250ms 实时刷新当前页（调试树 / 性能 / 错误 / 重绘 / 布局 / 缓存）----
    auto liveTimer = win.CreateTimer(250);
    if (liveTimer) liveTimer->Tick.connect([refreshActive]() { refreshActive(); });
    // 拾取跟随
    g_spyTimer = win.CreateTimer(100);
    if (g_spyTimer) g_spyTimer->Tick.connect([target, live, status]() {
        if (!g_spyOn) return;
        HWND tgt = target(); if (!tgt) return;
        POINT pt; GetCursorPos(&pt);
        HWND h = WindowFromPoint(pt); if (!h) return;
        DWORD dp = 0, tp = 0; GetWindowThreadProcessId(h, &dp); GetWindowThreadProcessId(tgt, &tp);
        if (dp != tp) { status->SetText(L"拾取：把鼠标移到目标窗口上…"); return; }
        wchar_t buf[48]; swprintf(buf, 48, L"%d,%d", pt.x, pt.y);
        static std::wstring lastPick;
        std::wstring id = SendDebugCmd(h, 10, buf);
        if (!id.empty() && id[0] != L'[') {
            if (id == lastPick) return;   // 没换控件就别反复发高亮（否则目标每秒被逼重绘 10 次）
            lastPick = id;
            SendDebugCmd(h, 11, id);
            bool located = SelectRowById(live->tree, id);
            status->SetText(L"拾取: " + id + (located ? L"（目标内高亮；已定位选中）" : L"（目标内高亮）"));
        }
        else { lastPick.clear(); status->SetText(L"拾取：该点无控件"); }
        });

    win.Show();
    win.Run();
    return 0;
}