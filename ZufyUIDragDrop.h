#pragma once
// ZufyUI 拖放 —— 阶段 1：接收拖入（OLE IDropTarget 实现 + DragData 读取 + Window 注册）。
// 由 ZufyUI.h 在末尾 #include（此时 ZufyUI 的类型已全部定义）。
#include "ZufyUI.h"
#include <shlobj.h>    // SHCreateStdEnumFMTETC / CFSTR_PREFERREDDROPEFFECT

namespace ZufyUI {

// ===================== DragData（只读数据读取）=====================
inline bool DragData::HasText() const {
    if (!obj_) return false;
    FORMATETC fe{ (CLIPFORMAT)CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return obj_->QueryGetData(&fe) == S_OK;
}
inline std::wstring DragData::GetText() const {
    std::wstring r;
    if (!obj_) return r;
    FORMATETC fe{ (CLIPFORMAT)CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm{};
    if (FAILED(obj_->GetData(&fe, &sm))) return r;
    if (sm.hGlobal) { const wchar_t* p = (const wchar_t*)GlobalLock(sm.hGlobal); if (p) { r = p; GlobalUnlock(sm.hGlobal); } }
    ReleaseStgMedium(&sm);
    return r;
}
inline bool DragData::HasFiles() const {
    if (!obj_) return false;
    FORMATETC fe{ (CLIPFORMAT)CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return obj_->QueryGetData(&fe) == S_OK;
}
inline std::vector<std::wstring> DragData::GetFiles() const {
    std::vector<std::wstring> out;
    if (!obj_) return out;
    FORMATETC fe{ (CLIPFORMAT)CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm{};
    if (FAILED(obj_->GetData(&fe, &sm))) return out;
    if (sm.hGlobal) {
        HDROP h = (HDROP)sm.hGlobal;
        UINT n = DragQueryFileW(h, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            UINT len = DragQueryFileW(h, i, nullptr, 0);
            std::wstring s(len, L'\0');
            UINT got = DragQueryFileW(h, i, &s[0], len + 1);
            s.resize(got);
            out.push_back(std::move(s));
        }
    }
    ReleaseStgMedium(&sm);
    return out;
}
inline bool DragData::HasFormat(const wchar_t* formatName) const {
    if (!obj_ || !formatName) return false;
    CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatW(formatName);
    if (!cf) return false;
    FORMATETC fe{ cf, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return obj_->QueryGetData(&fe) == S_OK;
}
inline bool DragData::GetFormatData(const wchar_t* formatName, std::vector<BYTE>& out) const {
    out.clear();
    if (!obj_ || !formatName) return false;
    CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatW(formatName);
    if (!cf) return false;
    FORMATETC fe{ cf, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm{};
    if (FAILED(obj_->GetData(&fe, &sm))) return false;
    bool ok = false;
    if (sm.hGlobal) {
        SIZE_T sz = GlobalSize(sm.hGlobal);
        const BYTE* p = (const BYTE*)GlobalLock(sm.hGlobal);
        if (p) { out.assign(p, p + sz); GlobalUnlock(sm.hGlobal); ok = true; }
    }
    ReleaseStgMedium(&sm);
    return ok;
}

// ===================== Window::DropTargetAt（命中 + 向上冒泡）=====================
inline UIElement* Window::DropTargetAt(float dipX, float dipY) {
    UIElement* e = HitTestElement(dipX, dipY);
    while (e) { if (e->IsDropTargetEnabled()) return e; e = e->GetParent(); }
    return nullptr;
}

// ===================== OLE IDropTarget（路由到命中元素）=====================
namespace detail {
class WindowDropTarget : public IDropTarget {
public:
    explicit WindowDropTarget(Window* w) : window_(w) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropTarget)) {
            *ppv = static_cast<IDropTarget*>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override { LONG n = InterlockedDecrement(&ref_); if (n == 0) delete this; return (ULONG)n; }

    STDMETHOD(DragEnter)(IDataObject* obj, DWORD keyState, POINTL pt, DWORD* effect) override {
        data_ = DragData(obj);
        Dispatch(keyState, pt, effect ? *effect : (DWORD)DROPEFFECT_COPY, effect, Mode::Over);
        return S_OK;
    }
    STDMETHOD(DragOver)(DWORD keyState, POINTL pt, DWORD* effect) override {
        Dispatch(keyState, pt, effect ? *effect : (DWORD)DROPEFFECT_COPY, effect, Mode::Over);
        return S_OK;
    }
    STDMETHOD(DragLeave)() override { SendLeave(); return S_OK; }
    STDMETHOD(Drop)(IDataObject* obj, DWORD keyState, POINTL pt, DWORD* effect) override {
        data_ = DragData(obj);
        Dispatch(keyState, pt, effect ? *effect : (DWORD)DROPEFFECT_COPY, effect, Mode::Drop);
        hover_ = nullptr;
        return S_OK;
    }

private:
    enum class Mode { Over, Drop };

    UIElement* TargetAt(POINTL pt, float& lx, float& ly, float& wdx, float& wdy) {
        POINT p{ pt.x, pt.y };
        ScreenToClient(window_->GetHwnd(), &p);
        wdx = window_->ScreenPxToDipX(p.x); wdy = window_->ScreenPxToDipY(p.y);
        UIElement* e = window_->DropTargetAt(wdx, wdy);
        if (e) { Rect r = e->GetArrangedRect(); lx = wdx - r.x; ly = wdy - r.y; }
        return e;
    }
    void SendLeave() {
        if (hover_) {
            DragEventArgs e{ data_, 0.0f, 0.0f, DROPEFFECT_NONE, DROPEFFECT_NONE, 0 };
            hover_->OnDragLeave(e);
            hover_ = nullptr;
        }
        if (hoverWindow_) {
            DragEventArgs e{ data_, 0.0f, 0.0f, DROPEFFECT_NONE, DROPEFFECT_NONE, 0 };
            window_->DragLeave.Fire(e);
            hoverWindow_ = false;
        }
    }
    void Dispatch(DWORD keyState, POINTL pt, DWORD allowed, DWORD* effect, Mode mode) {
        float lx = 0.0f, ly = 0.0f, wdx = 0.0f, wdy = 0.0f;
        UIElement* target = TargetAt(pt, lx, ly, wdx, wdy);
        bool useWindow = (!target && window_->IsDropTargetEnabled());   // 未命中元素 → 整窗接收
        if (target != hover_ || useWindow != hoverWindow_) {
            SendLeave();
            hover_ = target; hoverWindow_ = useWindow;
            if (hover_) { DragEventArgs e{ data_, lx, ly, allowed, DROPEFFECT_NONE, keyState }; hover_->OnDragEnter(e); }
            else if (hoverWindow_) { DragEventArgs e{ data_, wdx, wdy, allowed, DROPEFFECT_NONE, keyState }; window_->DragEnter.Fire(e); }
        }
        if (hover_) {
            DWORD eff = allowed & hover_->GetAllowedDropEffects();
            DragEventArgs e{ data_, lx, ly, allowed, eff, keyState };
            if (mode == Mode::Drop) { eff = hover_->OnDrop(e); }
            else { hover_->OnDragOver(e); eff = e.effect & allowed & hover_->GetAllowedDropEffects(); }
            if (effect) *effect = eff;
        } else if (hoverWindow_) {
            DragEventArgs e{ data_, wdx, wdy, allowed, allowed, keyState };
            if (mode == Mode::Drop) { window_->Drop.Fire(e); }
            else { window_->DragOver.Fire(e); }
            if (effect) *effect = (e.effect & allowed);
        } else {
            if (effect) *effect = DROPEFFECT_NONE;
        }
    }

    LONG ref_ = 1;                       // 我们持有的引用；RegisterDragDrop 会再 +1
    Window* window_ = nullptr;
    UIElement* hover_ = nullptr;
    bool hoverWindow_ = false;
    DragData data_;
};
} // namespace detail

// ===================== Window 成员实现 =====================
inline void Window::RegisterDropTarget() {
    if (!hwnd_ || dropTarget_) return;
    auto* dt = new detail::WindowDropTarget(this);       // ref_ = 1
    if (FAILED(RegisterDragDrop(hwnd_, dt))) { dt->Release(); dropTarget_ = nullptr; return; }
    dropTarget_ = dt;
}
inline void Window::UnregisterDropTarget() {
    if (hwnd_) RevokeDragDrop(hwnd_);
    if (dropTarget_) { dropTarget_->Release(); dropTarget_ = nullptr; }
}

// ===================== 阶段 2：拖出（IDataObject / IDropSource / BeginDrag）=====================
namespace detail {

// 内存实现的数据对象：持有若干 (格式, 字节) 项，GetData 时分配 HGLOBAL 返回。
class EnumFormatEtcImpl : public IEnumFORMATETC {
public:
    explicit EnumFormatEtcImpl(std::vector<FORMATETC> fes) : fes_(std::move(fes)) {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER; *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IEnumFORMATETC)) { *ppv = static_cast<IEnumFORMATETC*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override { LONG n = InterlockedDecrement(&ref_); if (n == 0) delete this; return (ULONG)n; }
    STDMETHOD(Next)(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override {
        ULONG n = 0; while (n < celt && idx_ < (ULONG)fes_.size()) rgelt[n++] = fes_[idx_++];
        if (pceltFetched) *pceltFetched = n;
        return (n == celt) ? S_OK : S_FALSE;
    }
    STDMETHOD(Skip)(ULONG celt) override { idx_ += celt; return (idx_ <= fes_.size()) ? S_OK : S_FALSE; }
    STDMETHOD(Reset)() override { idx_ = 0; return S_OK; }
    STDMETHOD(Clone)(IEnumFORMATETC** out) override { if (!out) return E_POINTER; *out = new EnumFormatEtcImpl(fes_); return S_OK; }
private:
    LONG ref_ = 1; std::vector<FORMATETC> fes_; ULONG idx_ = 0;
};

class DataObjectImpl : public IDataObject {
public:
    struct Item { CLIPFORMAT cf; std::vector<BYTE> bytes; };
    std::vector<Item> items;
    DWORD performedEffect_ = 0;   // 目标写入的 CFSTR_PERFORMEDDROPEFFECT（优化移动）

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDataObject)) { *ppv = static_cast<IDataObject*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override { LONG n = InterlockedDecrement(&ref_); if (n == 0) delete this; return (ULONG)n; }

    STDMETHOD(GetData)(FORMATETC* fe, STGMEDIUM* sm) override {
        if (!fe || !sm) return E_INVALIDARG;
        for (auto& it : items) if (it.cf == fe->cfFormat) {
            if (!(fe->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, it.bytes.size() ? it.bytes.size() : 1);
            if (!h) return E_OUTOFMEMORY;
            if (void* p = GlobalLock(h)) { if (!it.bytes.empty()) memcpy(p, it.bytes.data(), it.bytes.size()); GlobalUnlock(h); }
            sm->tymed = TYMED_HGLOBAL; sm->hGlobal = h; sm->pUnkForRelease = nullptr;
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    STDMETHOD(GetDataHere)(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    STDMETHOD(QueryGetData)(FORMATETC* fe) override {
        if (!fe) return E_INVALIDARG;
        for (auto& it : items) if (it.cf == fe->cfFormat) return (fe->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
        return DV_E_FORMATETC;
    }
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC*, FORMATETC* out) override { if (out) out->ptd = nullptr; return E_NOTIMPL; }
    STDMETHOD(SetData)(FORMATETC* fe, STGMEDIUM* sm, BOOL) override {
        if (!fe || !sm) return E_INVALIDARG;
        CLIPFORMAT performed = (CLIPFORMAT)RegisterClipboardFormatW(CFSTR_PERFORMEDDROPEFFECT);
        if (performed && fe->cfFormat == performed && (sm->tymed & TYMED_HGLOBAL) && sm->hGlobal) {
            if (DWORD* p = (DWORD*)GlobalLock(sm->hGlobal)) { performedEffect_ = *p; GlobalUnlock(sm->hGlobal); }
            return S_OK;
        }
        return E_NOTIMPL;
    }
    STDMETHOD(EnumFormatEtc)(DWORD dir, IEnumFORMATETC** out) override {
        if (dir != DATADIR_GET || !out) return E_NOTIMPL;
        std::vector<FORMATETC> fes; fes.reserve(items.size());
        for (auto& it : items) { FORMATETC f{}; f.cfFormat = it.cf; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL; fes.push_back(f); }
        *out = new EnumFormatEtcImpl(std::move(fes));
        return S_OK;
    }
    STDMETHOD(DAdvise)(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHOD(DUnadvise)(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }

private:
    LONG ref_ = 1;
};

class DropSourceImpl : public IDropSource {
public:
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropSource)) { *ppv = static_cast<IDropSource*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override { LONG n = InterlockedDecrement(&ref_); if (n == 0) delete this; return (ULONG)n; }
    // 松开左右键 → 落点；按 Esc → 取消
    STDMETHOD(QueryContinueDrag)(BOOL escape, DWORD keyState) override {
        if (escape) return DRAGDROP_S_CANCEL;
        if (!(keyState & MK_LBUTTON) && !(keyState & MK_RBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    STDMETHOD(GiveFeedback)(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
private:
    LONG ref_ = 1;
};

inline std::vector<BYTE> BuildHDrop(const std::vector<std::wstring>& files) {
    size_t wchars = 1;                       // 末尾双空
    for (auto& f : files) wchars += f.size() + 1;
    std::vector<BYTE> buf(sizeof(DROPFILES) + wchars * sizeof(wchar_t), 0);
    DROPFILES* df = (DROPFILES*)buf.data();
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    wchar_t* p = (wchar_t*)(buf.data() + sizeof(DROPFILES));
    for (auto& f : files) { memcpy(p, f.c_str(), f.size() * sizeof(wchar_t)); p += f.size() + 1; }
    return buf;
}
} // namespace detail

// 组装拖出数据（链式）
class DragDataBuilder {
public:
    void AddText(const std::wstring& s) {
        std::vector<BYTE> b((s.size() + 1) * sizeof(wchar_t));
        memcpy(b.data(), s.c_str(), b.size());
        items_.push_back({ (CLIPFORMAT)CF_UNICODETEXT, std::move(b) });
    }
    void AddFiles(const std::vector<std::wstring>& paths) {
        items_.push_back({ (CLIPFORMAT)CF_HDROP, detail::BuildHDrop(paths) });
    }
    void AddFile(const std::wstring& path) { AddFiles({ path }); }
    void AddCustom(const wchar_t* formatName, const void* data, size_t bytes) {
        if (!formatName) return;
        CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatW(formatName);
        if (!cf) return;
        std::vector<BYTE> b(bytes);
        if (bytes && data) memcpy(b.data(), data, bytes);
        items_.push_back({ cf, std::move(b) });
    }
    void SetPreferredEffect(DWORD eff) {
        CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
        if (!cf) return;
        std::vector<BYTE> b(sizeof(DWORD));
        memcpy(b.data(), &eff, sizeof(DWORD));
        items_.push_back({ cf, std::move(b) });
    }
    bool Empty() const { return items_.empty(); }
    ComPtr<IDataObject> Build() const {
        auto* o = new detail::DataObjectImpl();
        o->items = items_;
        lastImpl_ = o;
        ComPtr<IDataObject> p; p.Attach(o);   // 接管引用（o->ref_ 已为 1）
        return p;
    }
    // DoDragDrop 之后：目标写入的“已执行效果”（MOVE 表示资源管理器做了优化移动）
    DWORD PerformedEffect() const { return lastImpl_ ? lastImpl_->performedEffect_ : 0; }
private:
    std::vector<detail::DataObjectImpl::Item> items_;
    mutable detail::DataObjectImpl* lastImpl_ = nullptr;
};

inline DWORD Window::BeginDrag(DragDataBuilder& data, DWORD allowed, UIElement*) {
    if (!hwnd_ || data.Empty()) return DROPEFFECT_NONE;
    ComPtr<IDataObject> obj = data.Build();
    if (!obj) return DROPEFFECT_NONE;
    // 系统拖拽影像（半透明缩略图 + 落点动画）
    ComPtr<IDragSourceHelper> helper;
    if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(helper.GetAddressOf()))) && helper) {
        POINT pt = { 0, 0 };
        helper->InitializeFromWindow(hwnd_, &pt, obj.Get());
    }
    ComPtr<IDropSource> src; src.Attach(new detail::DropSourceImpl());
    DWORD effect = DROPEFFECT_NONE;
    HRESULT hr = DoDragDrop(obj.Get(), src.Get(), allowed, &effect);
    if (!SUCCEEDED(hr)) return DROPEFFECT_NONE;
    DWORD performed = data.PerformedEffect();   // 目标标记的“已执行效果”（优化移动）
    return performed ? performed : effect;
}

} // namespace ZufyUI
