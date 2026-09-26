#pragma once
// ============================================================================
// ZufyUIImages.h —— 图像系统（WIC 解码 + Direct2D GPU 绘制/变换）
// ----------------------------------------------------------------------------
// 设计目标（对齐 Qt 语义、尽量走 GPU）：
//   * 解码：WIC（文件 / 内存 / base64 / 程序资源(含 DLL) / HBITMAP / HICON）。
//   * 存放：设备无关的 IWICBitmapSource（已转换成 D2D 可直接用的 32bppPBGRA）。
//   * 绘制：Direct2D 的 GPU 路径。缩放、旋转、镜像、裁剪都在绘制时用
//     D2D 变换 / 源矩形完成，不做 CPU 逐像素处理。
//   * 多窗口：每个渲染目标各自缓存一张 ID2D1Bitmap（D2D 位图属于创建它的
//     渲染目标）；设备丢失时统一清理重建。
//   * 变换（Scaled/Rotated/Mirrored/Cropped）是轻量描述符：共享同一份解码数据，
//     在 Draw 时由 GPU 施加，靠近 Qt 的“图像对象”用法但避免 CPU 变换。
// ============================================================================

#include "ZufyUI.h"
#include <wincodec.h>
#include <objbase.h>
#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

#pragma comment(lib, "windowscodecs.lib")

namespace ZufyUI {

    // ------------------------------------------------------------------
    // ImageDeviceCache：某个 Image 在“每个渲染目标”上的 D2D 位图缓存
    // ------------------------------------------------------------------
    // R2：设备世代。每次 DeviceReset 递增，使"渲染目标地址被复用"时的旧缓存 key 自动失效。
    inline unsigned long long g_imageDeviceEpoch = 0;
    struct ImageCacheKey {
        unsigned long long epoch; ID2D1RenderTarget* rt;
        bool operator==(const ImageCacheKey& o) const { return epoch == o.epoch && rt == o.rt; }
    };
    struct ImageCacheKeyHash {
        size_t operator()(const ImageCacheKey& k) const {
            return std::hash<unsigned long long>{}(k.epoch) ^ (std::hash<void*>{}(k.rt) + 0x9e3779b9u);
        }
    };

    class ImageDeviceCache {
    public:
        ID2D1Bitmap* Get(ID2D1RenderTarget* rt, IWICBitmapSource* src) {
            if (!rt || !src) return nullptr;
            std::lock_guard<std::mutex> lock(mtx_);
            ImageCacheKey key{ g_imageDeviceEpoch, rt };
            auto it = map_.find(key);
            if (it != map_.end() && it->second) return it->second.Get();
            ComPtr<ID2D1Bitmap> bmp;
            HRESULT hr = rt->CreateBitmapFromWicBitmap(src, nullptr, bmp.GetAddressOf());
            if (FAILED(hr) || !bmp) return nullptr;
            map_[key] = bmp;
            return bmp.Get();
        }
        void Clear() { std::lock_guard<std::mutex> lock(mtx_); map_.clear(); }
        void Clear(ID2D1RenderTarget* rt) {
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto it = map_.begin(); it != map_.end(); ) {
                if (it->first.rt == rt) it = map_.erase(it); else ++it;
            }
        }

    private:
        std::mutex mtx_;
        std::unordered_map<ImageCacheKey, ComPtr<ID2D1Bitmap>, ImageCacheKeyHash> map_;
    };

    // ------------------------------------------------------------------
    // ImageManager：共享 WIC 工厂 + 设备缓存登记（设备丢失时统一清理）
    // ------------------------------------------------------------------
    class ImageManager {
    public:
        static ImageManager& Instance() {
            static ImageManager inst;
            return inst;
        }

        IWICImagingFactory* Factory() { return factory_.Get(); }

        void RegisterCache(const std::shared_ptr<ImageDeviceCache>& c) {
            if (!c) return;
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto& w : caches_) if (w.lock() == c) return;
            caches_.push_back(c);
        }

        // 设备丢失/重建时调用：清空所有图像在各渲染目标上的 D2D 位图缓存
        void ClearAllDeviceCaches() {
            std::lock_guard<std::mutex> lock(mtx_);
            ++g_imageDeviceEpoch;   // R2：设备换代，旧地址 key 全部失效
            for (auto it = caches_.begin(); it != caches_.end(); ) {
                if (auto c = it->lock()) { c->Clear(); ++it; }
                else it = caches_.erase(it);
            }
        }

    private:
        ImageManager() {
            // WIC 需要 COM。UI 线程通常只调用一次；重复调用返回 S_FALSE/RPC_E_CHANGED_MODE 都忽略。
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory_.GetAddressOf()));
            // 设备资源被丢弃/重建时清空图像位图缓存（避免旧渲染目标指针复用后命中错误缓存）
            // 用 ConnectionGroup 持有连接：ImageManager 析构时自动断开
            connGroup_ = std::make_shared<ConnectionGroup>();
            UIZSignals::DeviceReset.connect([this]() { ClearAllDeviceCaches(); },
                ConnectionThread::CurrentThread, connGroup_);
        }
        ImageManager(const ImageManager&) = delete;
        ImageManager& operator=(const ImageManager&) = delete;

        ComPtr<IWICImagingFactory> factory_;
        std::mutex mtx_;
        std::vector<std::weak_ptr<ImageDeviceCache>> caches_;
        std::shared_ptr<ConnectionGroup> connGroup_;
    };

    // ------------------------------------------------------------------
    // Image：解码 + 变换描述符 + GPU 绘制
    // ------------------------------------------------------------------
    class Image {
    public:
        enum class Format { Png, Jpeg, Bmp, Gif, Tiff };
        enum class Interpolation { Nearest, Linear };

        struct DrawOptions {
            float opacity = 1.0f;
            Interpolation interpolation = Interpolation::Linear;
        };

        Image() = default;

        bool IsNull() const { return !data_ || !data_->source; }
        int Width() const { return width_; }
        int Height() const { return height_; }
        Rect Bounds() const { return Rect(0, 0, (float)width_, (float)height_); }
        bool HasAlpha() const { return data_ ? data_->hasAlpha : false; }

        // ---------------- 加载 ----------------
        static std::shared_ptr<Image> FromFile(const std::wstring& path);
        static std::shared_ptr<Image> FromMemory(const void* data, size_t size);
        // 支持标准/URL-safe base64，可选 "data:image/png;base64," 前缀
        static std::shared_ptr<Image> FromBase64(const std::string& base64);
        // 程序资源：type 传 L"PNG"/L"IMAGE"/RT_RCDATA/RT_BITMAP 等
        static std::shared_ptr<Image> FromResource(HMODULE mod, const wchar_t* name, const wchar_t* type);
        static std::shared_ptr<Image> FromResource(int id, const wchar_t* type);   // 当前模块
        static std::shared_ptr<Image> FromHBITMAP(HBITMAP hbmp);
        static std::shared_ptr<Image> FromHICON(HICON hicon);
        HICON ToHICON() const;   // 转回系统原生图标（新建对象，用完需 DestroyIcon）
        // 导出像素：32bpp BGRA，直通 alpha（非预乘）——用于写 .ico 等
        bool CopyPixelsBgra(std::vector<uint8_t>& out, int& outW, int& outH) const;
        // 内部使用：从已转换好的 WIC 源构造（供内部 helper 调用）
        static std::shared_ptr<Image> FromSource(IWICBitmapSource* src, int w, int h, bool hasAlpha);

        // ---------------- 变换（轻量描述符，GPU 在 Draw 时施加） ----------------
        std::shared_ptr<Image> Scaled(float w, float h) const;
        std::shared_ptr<Image> ScaledToWidth(float w) const;
        std::shared_ptr<Image> ScaledToHeight(float h) const;
        std::shared_ptr<Image> Rotated(float degrees) const;
        std::shared_ptr<Image> Mirrored(bool horizontal = true, bool vertical = false) const;
        std::shared_ptr<Image> Cropped(const Rect& srcRect) const;

        // ---------------- 绘制（GPU） ----------------
        void Draw(ID2D1RenderTarget* rt, const Rect& dst, const DrawOptions& opt = {}) const;
        void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& opt = {}) const;
        // 按逻辑尺寸绘制在 (x,y)
        void Draw(ID2D1RenderTarget* rt, float x, float y, const DrawOptions& opt = {}) const;
        // 把当前变换“烘焙”成一张 D2D 位图（设备相关；用于需要真实位图的场合）
        ComPtr<ID2D1Bitmap> Bake(ID2D1RenderTarget* rt) const;

        // ---------------- 编码 / 保存（WIC） ----------------
        bool Save(const std::wstring& path, Format fmt = Format::Png, float quality = 0.9f) const;
        std::vector<uint8_t> Encode(Format fmt = Format::Png, float quality = 0.9f) const;

    private:
        struct Impl {
            ComPtr<IWICBitmapSource> source;              // 32bppPBGRA，设备无关
            int width = 0, height = 0;
            bool hasAlpha = false;
            std::shared_ptr<ImageDeviceCache> cache;
        };
        struct Xform {
            float rot = 0.0f;                              // 角度
            bool mh = false, mv = false;                   // 镜像
            bool hasCrop = false;
            Rect crop{ 0, 0, 0, 0 };                       // 源像素裁剪
        };

        std::shared_ptr<Impl> data_;
        Xform xf_;
        int width_ = 0, height_ = 0;                       // 变换后的逻辑尺寸

        ID2D1Bitmap* GetBitmap(ID2D1RenderTarget* rt) const;
        void DrawWithTransform(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& opt) const;
    };

    // ==================================================================
    // 实现
    // ==================================================================
    namespace detail_img {

        inline std::vector<uint8_t> Base64Decode(const std::string& base64) {
            // 去掉 data URI 前缀
            std::string s = base64;
            size_t comma = s.find(',');
            if (s.rfind("data:", 0) == 0 && comma != std::string::npos) s = s.substr(comma + 1);
            auto val = [](char c) -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A';
                if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                if (c >= '0' && c <= '9') return c - '0' + 52;
                if (c == '+' || c == '-') return 62;   // 支持 URL-safe
                if (c == '/' || c == '_') return 63;
                return -1;
                };
            std::vector<uint8_t> out;
            int buf = 0, bits = 0;
            for (char c : s) {
                if (c == '=') break;
                int v = val(c);
                if (v < 0) continue;   // 跳过空白等
                buf = (buf << 6) | v; bits += 6;
                if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((buf >> bits) & 0xFF)); }
            }
            return out;
        }

        // 从内存/资源字节解码（自动识别格式）
        inline std::shared_ptr<Image> DecodeFromMemory(const void* data, size_t size);

        // RT_BITMAP（DIB）资源 -> BMP 文件字节
        inline std::vector<uint8_t> DibResourceToBmp(const void* res, size_t size) {
            std::vector<uint8_t> out;
            if (size < sizeof(BITMAPINFOHEADER)) return out;
            const BITMAPINFOHEADER* bih = reinterpret_cast<const BITMAPINFOHEADER*>(res);
            DWORD paletteEntries = bih->biClrUsed;
            if (paletteEntries == 0 && bih->biBitCount <= 8) paletteEntries = 1u << bih->biBitCount;
            DWORD paletteBytes = paletteEntries * sizeof(RGBQUAD);
            DWORD offBits = sizeof(BITMAPFILEHEADER) + bih->biSize + paletteBytes;
            BITMAPFILEHEADER fh = {};
            fh.bfType = 0x4D42; // 'BM'
            fh.bfSize = (DWORD)size + sizeof(BITMAPFILEHEADER);
            fh.bfOffBits = offBits;
            out.resize((size_t)size + sizeof(BITMAPFILEHEADER));
            memcpy(out.data(), &fh, sizeof(fh));
            memcpy(out.data() + sizeof(fh), res, size);
            return out;
        }

    } // namespace detail_img

    inline std::shared_ptr<Image> Image::FromSource(IWICBitmapSource* src, int w, int h, bool hasAlpha) {
        if (!src) return nullptr;
        auto img = std::make_shared<Image>();
        img->data_ = std::make_shared<Impl>();
        img->data_->source = src;
        img->data_->width = w;
        img->data_->height = h;
        img->data_->hasAlpha = hasAlpha;
        img->data_->cache = std::make_shared<ImageDeviceCache>();
        ImageManager::Instance().RegisterCache(img->data_->cache);
        img->width_ = w;
        img->height_ = h;
        return img;
    }

    // 把任意 WIC 源转成 32bppPBGRA 并返回 Image
    //
    // 根源说明（务必保留这段缓存逻辑）：
    //   IWICStream::InitializeFromMemory 不复制缓冲区，调用者必须保证该内存在
    //   流的整个生命周期内有效；而帧解码 / 格式转换都是惰性的，真正的像素读取
    //   发生在后面的 ID2D1RenderTarget::CreateBitmapFromWicBitmap。
    //   FromMemory / FromBase64 / FromResource(RT_BITMAP) 传入的都是临时缓冲，
    //   函数返回后即析构——若此处不立即取像素，绘制时会访问已释放内存，
    //   结果是 CreateBitmapFromWicBitmap 返回 nullptr（什么都不画）或读到垃圾数据。
    //   因此这里用 WICBitmapCacheOnLoad 强制把像素拷进一张独立 WIC 位图，与来源内存彻底解耦。
    inline std::shared_ptr<Image> detail_img_MakeFromSource(IWICBitmapSource* src) {
        if (!src) return nullptr;
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return nullptr;
        UINT w = 0, h = 0;
        src->GetSize(&w, &h);
        ComPtr<IWICFormatConverter> conv;
        if (SUCCEEDED(f->CreateFormatConverter(conv.GetAddressOf()))) {
            if (SUCCEEDED(conv->Initialize(src, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                ComPtr<IWICBitmap> cached;
                if (SUCCEEDED(f->CreateBitmapFromSource(conv.Get(), WICBitmapCacheOnLoad, cached.GetAddressOf()))) {
                    return Image::FromSource(cached.Get(), (int)w, (int)h, true);
                }
                return Image::FromSource(conv.Get(), (int)w, (int)h, true);
            }
        }
        return Image::FromSource(src, (int)w, (int)h, false);
    }

    inline std::shared_ptr<Image> detail_img::DecodeFromMemory(const void* data, size_t size) {
        if (!data || size == 0) return nullptr;
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return nullptr;
        ComPtr<IWICStream> stream;
        if (FAILED(f->CreateStream(stream.GetAddressOf()))) return nullptr;
        if (FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size))) return nullptr;
        ComPtr<IWICBitmapDecoder> dec;
        if (FAILED(f->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, dec.GetAddressOf())))
            return nullptr;
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(dec->GetFrame(0, frame.GetAddressOf()))) return nullptr;
        return detail_img_MakeFromSource(frame.Get());
    }

    inline std::shared_ptr<Image> Image::FromFile(const std::wstring& path) {
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return nullptr;
        ComPtr<IWICBitmapDecoder> dec;
        if (FAILED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, dec.GetAddressOf()))) return nullptr;
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(dec->GetFrame(0, frame.GetAddressOf()))) return nullptr;
        return detail_img_MakeFromSource(frame.Get());
    }

    inline std::shared_ptr<Image> Image::FromMemory(const void* data, size_t size) {
        return detail_img::DecodeFromMemory(data, size);
    }

    inline std::shared_ptr<Image> Image::FromBase64(const std::string& base64) {
        std::vector<uint8_t> bytes = detail_img::Base64Decode(base64);
        if (bytes.empty()) return nullptr;
        return detail_img::DecodeFromMemory(bytes.data(), bytes.size());
    }

    inline std::shared_ptr<Image> Image::FromResource(HMODULE mod, const wchar_t* name, const wchar_t* type) {
        if (!mod) mod = GetModuleHandle(nullptr);
        HRSRC res = FindResourceW(mod, name, type);
        if (!res) return nullptr;
        HGLOBAL h = LoadResource(mod, res);
        if (!h) return nullptr;
        const void* data = LockResource(h);
        DWORD size = SizeofResource(mod, res);
        if (!data || size == 0) return nullptr;
        // RT_BITMAP 是 DIB，需要合成 BMP 文件头
        if (type == RT_BITMAP || (type && wcscmp(type, RT_BITMAP) == 0)) {
            auto bmp = detail_img::DibResourceToBmp(data, size);
            if (bmp.empty()) return nullptr;
            return detail_img::DecodeFromMemory(bmp.data(), bmp.size());
        }
        return detail_img::DecodeFromMemory(data, size);
    }

    inline std::shared_ptr<Image> Image::FromResource(int id, const wchar_t* type) {
        return FromResource(GetModuleHandle(nullptr), MAKEINTRESOURCEW(id), type);
    }

    inline std::shared_ptr<Image> Image::FromHBITMAP(HBITMAP hbmp) {
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f || !hbmp) return nullptr;
        ComPtr<IWICBitmap> bmp;
        if (FAILED(f->CreateBitmapFromHBITMAP(hbmp, nullptr, WICBitmapIgnoreAlpha, bmp.GetAddressOf())))
            return nullptr;
        return detail_img_MakeFromSource(bmp.Get());
    }

    inline std::shared_ptr<Image> Image::FromHICON(HICON hicon) {
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f || !hicon) return nullptr;
        ComPtr<IWICBitmap> bmp;
        if (FAILED(f->CreateBitmapFromHICON(hicon, bmp.GetAddressOf()))) return nullptr;
        return detail_img_MakeFromSource(bmp.Get());
    }

    // 反向：把库自带 Image 转成系统 HICON（预乘 alpha，供 WM_SETICON / 托盘 / 覆盖徽章等使用）
    inline HICON Image::ToHICON() const {
        if (!data_ || !data_->source) return nullptr;
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return nullptr;
        UINT w = 0, h = 0;
        if (FAILED(data_->source->GetSize(&w, &h)) || w == 0 || h == 0) return nullptr;
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(f->CreateFormatConverter(conv.GetAddressOf()))) return nullptr;
        if (FAILED(conv->Initialize(data_->source.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return nullptr;
        std::vector<BYTE> buf((size_t)w * h * 4);
        if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)buf.size(), buf.data()))) return nullptr;

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = (LONG)w;
        bi.bmiHeader.biHeight = -(LONG)h;   // 自上而下
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC screen = GetDC(nullptr);
        if (!screen) return nullptr;
        HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib && bits) {
            BYTE* dst = (BYTE*)bits;
            for (size_t i = 0; i < (size_t)w * h; ++i) {
                BYTE b = buf[i * 4 + 0], g = buf[i * 4 + 1], r = buf[i * 4 + 2], a = buf[i * 4 + 3];
                dst[i * 4 + 0] = (BYTE)(b * a / 255);
                dst[i * 4 + 1] = (BYTE)(g * a / 255);
                dst[i * 4 + 2] = (BYTE)(r * a / 255);
                dst[i * 4 + 3] = a;
            }
        }
        HBITMAP mask = CreateBitmap((int)w, (int)h, 1, 1, nullptr);
        HICON out = nullptr;
        if (dib) {
            ICONINFO ii = {};
            ii.fIcon = TRUE;
            ii.hbmColor = dib;
            ii.hbmMask = mask;
            out = CreateIconIndirect(&ii);
        }
        if (mask) DeleteObject(mask);
        if (dib) DeleteObject(dib);
        ReleaseDC(nullptr, screen);
        return out;
    }

    inline bool Image::CopyPixelsBgra(std::vector<uint8_t>& out, int& outW, int& outH) const {
        out.clear(); outW = 0; outH = 0;
        if (!data_ || !data_->source) return false;
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return false;
        UINT w = 0, h = 0;
        if (FAILED(data_->source->GetSize(&w, &h)) || w == 0 || h == 0) return false;
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(f->CreateFormatConverter(conv.GetAddressOf()))) return false;
        if (FAILED(conv->Initialize(data_->source.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
        out.resize((size_t)w * h * 4);
        if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)out.size(), out.data()))) return false;
        outW = (int)w; outH = (int)h;
        return true;
    }

    // ---------------- 变换描述符 ----------------
    inline std::shared_ptr<Image> Image::Scaled(float w, float h) const {
        if (IsNull() || w <= 0 || h <= 0) return nullptr;
        auto img = std::make_shared<Image>(*this);
        img->width_ = (int)std::lround(w);
        img->height_ = (int)std::lround(h);
        return img;
    }
    inline std::shared_ptr<Image> Image::ScaledToWidth(float w) const {
        if (IsNull() || w <= 0) return nullptr;
        float h = (float)height_ * (w / (float)width_);
        return Scaled(w, h);
    }
    inline std::shared_ptr<Image> Image::ScaledToHeight(float h) const {
        if (IsNull() || h <= 0) return nullptr;
        float w = (float)width_ * (h / (float)height_);
        return Scaled(w, h);
    }
    inline std::shared_ptr<Image> Image::Rotated(float degrees) const {
        if (IsNull()) return nullptr;
        auto img = std::make_shared<Image>(*this);
        img->xf_.rot = std::fmod(xf_.rot + degrees, 360.0f);
        if (img->xf_.rot < 0) img->xf_.rot += 360.0f;
        // 90/270 度交换逻辑尺寸
        float r = std::fmod(img->xf_.rot, 180.0f);
        if (fabs(r - 90.0f) < 0.01f) { int t = img->width_; img->width_ = img->height_; img->height_ = t; }
        return img;
    }
    inline std::shared_ptr<Image> Image::Mirrored(bool horizontal, bool vertical) const {
        if (IsNull()) return nullptr;
        auto img = std::make_shared<Image>(*this);
        img->xf_.mh = xf_.mh ^ horizontal;
        img->xf_.mv = xf_.mv ^ vertical;
        return img;
    }
    inline std::shared_ptr<Image> Image::Cropped(const Rect& r) const {
        if (IsNull()) return nullptr;
        auto img = std::make_shared<Image>(*this);
        img->xf_.hasCrop = true;
        img->xf_.crop = r;
        img->width_ = (int)std::lround(r.width);
        img->height_ = (int)std::lround(r.height);
        return img;
    }

    // ---------------- 绘制（GPU） ----------------
    inline ID2D1Bitmap* Image::GetBitmap(ID2D1RenderTarget* rt) const {
        if (IsNull()) return nullptr;
        return data_->cache->Get(rt, data_->source.Get());
    }

    inline void Image::DrawWithTransform(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& opt) const {
        ID2D1Bitmap* bmp = GetBitmap(rt);
        if (!bmp) return;

        D2D1_RECT_F src;
        bool useSrc = false;
        if (xf_.hasCrop) {
            src = D2D1::RectF(xf_.crop.x, xf_.crop.y, xf_.crop.x + xf_.crop.width, xf_.crop.y + xf_.crop.height);
            useSrc = true;
        }

        bool needXform = (fabs(xf_.rot) > 0.001f) || xf_.mh || xf_.mv;
        D2D1::Matrix3x2F oldT;
        if (needXform) {
            rt->GetTransform(&oldT);
            // 绕“目标矩形中心”做旋转/镜像。
            // 注意 D2D1::Matrix3x2F 的乘法是“左边的先应用”（行向量 p*M）。
            // 之前写成 Translation(+c) * ... * Translation(-c)，把中心平移用反了，
            // 图像被整体位移约 2×center，落到图标矩形之外，看起来就是“没画出来”。
            // 这里改用带 center 的重载，语义清晰且不会写反。
            D2D1_POINT_2F pivot = D2D1::Point2F((dst.left + dst.right) * 0.5f, (dst.top + dst.bottom) * 0.5f);
            D2D1::Matrix3x2F m =
                D2D1::Matrix3x2F::Rotation(xf_.rot, pivot) *
                D2D1::Matrix3x2F::Scale(xf_.mh ? -1.0f : 1.0f, xf_.mv ? -1.0f : 1.0f, pivot);
            rt->SetTransform(m * oldT);
        }

        auto mode = (opt.interpolation == Interpolation::Nearest)
            ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
            : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
        if (useSrc) rt->DrawBitmap(bmp, dst, opt.opacity, mode, src);
        else        rt->DrawBitmap(bmp, dst, opt.opacity, mode);

        if (needXform) rt->SetTransform(oldT);
    }

    inline void Image::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dst, const DrawOptions& opt) const {
        DrawWithTransform(rt, dst, opt);
    }
    inline void Image::Draw(ID2D1RenderTarget* rt, const Rect& dst, const DrawOptions& opt) const {
        DrawWithTransform(rt, dst.ToD2D(), opt);
    }
    inline void Image::Draw(ID2D1RenderTarget* rt, float x, float y, const DrawOptions& opt) const {
        DrawWithTransform(rt, D2D1::RectF(x, y, x + (float)width_, y + (float)height_), opt);
    }

    inline ComPtr<ID2D1Bitmap> Image::Bake(ID2D1RenderTarget* rt) const {
        if (!rt || IsNull()) return nullptr;
        ComPtr<ID2D1BitmapRenderTarget> off;
        if (FAILED(rt->CreateCompatibleRenderTarget(D2D1::SizeF((float)width_, (float)height_), off.GetAddressOf())))
            return nullptr;
        off->BeginDraw();
        off->Clear(D2D1::ColorF(0, 0, 0, 0));
        DrawWithTransform(off.Get(), D2D1::RectF(0, 0, (float)width_, (float)height_), {});
        off->EndDraw();
        ComPtr<ID2D1Bitmap> out;
        off->GetBitmap(out.GetAddressOf());
        return out;
    }

    // ---------------- 编码 / 保存 ----------------
    inline std::vector<uint8_t> Image::Encode(Format fmt, float quality) const {
        // 用 CreateStreamOnHGlobal 得到“可增长”的内存流；WIC 编码器直接写这个 IStream。
        std::vector<uint8_t> result;
        if (IsNull()) return result;
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f) return result;
        GUID container = GUID_ContainerFormatPng;
        switch (fmt) {
        case Format::Jpeg: container = GUID_ContainerFormatJpeg; break;
        case Format::Bmp:  container = GUID_ContainerFormatBmp; break;
        case Format::Gif:  container = GUID_ContainerFormatGif; break;
        case Format::Tiff: container = GUID_ContainerFormatTiff; break;
        default:           container = GUID_ContainerFormatPng; break;
        }

        ComPtr<IStream> memStream;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, memStream.GetAddressOf()))) return result;

        ComPtr<IWICBitmapEncoder> enc;
        if (FAILED(f->CreateEncoder(container, nullptr, enc.GetAddressOf()))) return result;
        if (FAILED(enc->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) return result;

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> props;
        if (FAILED(enc->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf()))) return result;
        if (quality >= 0.0f && quality <= 1.0f) {
            PROPBAG2 o = {}; o.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT v; VariantInit(&v); v.vt = VT_R4; v.fltVal = quality;
            props->Write(1, &o, &v);
        }
        if (FAILED(frame->Initialize(props.Get()))) return result;
        frame->SetSize((UINT)data_->width, (UINT)data_->height);
        WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
        frame->SetPixelFormat(&pf);
        if (FAILED(frame->WriteSource(data_->source.Get(), nullptr))) return result;
        if (FAILED(frame->Commit())) return result;
        if (FAILED(enc->Commit())) return result;

        // 读回内存流内容
        STATSTG st = {};
        if (FAILED(memStream->Stat(&st, STATFLAG_NONAME))) return result;
        ULONG n = (ULONG)st.cbSize.QuadPart;
        result.resize(n);
        LARGE_INTEGER zero = {}; memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG read = 0;
        memStream->Read(result.data(), n, &read);
        result.resize(read);
        return result;
    }

    inline bool Image::Save(const std::wstring& path, Format fmt, float quality) const {
        IWICImagingFactory* f = ImageManager::Instance().Factory();
        if (!f || IsNull()) return false;
        GUID container = GUID_ContainerFormatPng;
        switch (fmt) {
        case Format::Jpeg: container = GUID_ContainerFormatJpeg; break;
        case Format::Bmp:  container = GUID_ContainerFormatBmp; break;
        case Format::Gif:  container = GUID_ContainerFormatGif; break;
        case Format::Tiff: container = GUID_ContainerFormatTiff; break;
        default:           container = GUID_ContainerFormatPng; break;
        }
        ComPtr<IWICStream> stream;
        if (FAILED(f->CreateStream(stream.GetAddressOf()))) return false;
        if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) return false;
        ComPtr<IWICBitmapEncoder> enc;
        if (FAILED(f->CreateEncoder(container, nullptr, enc.GetAddressOf()))) return false;
        if (FAILED(enc->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;
        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> props;
        if (FAILED(enc->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf()))) return false;
        if (quality >= 0.0f && quality <= 1.0f) {
            PROPBAG2 o = {}; o.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT v; VariantInit(&v); v.vt = VT_R4; v.fltVal = quality;
            props->Write(1, &o, &v);
        }
        if (FAILED(frame->Initialize(props.Get()))) return false;
        frame->SetSize((UINT)data_->width, (UINT)data_->height);
        WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
        frame->SetPixelFormat(&pf);
        if (FAILED(frame->WriteSource(data_->source.Get(), nullptr))) return false;
        if (FAILED(frame->Commit())) return false;
        return SUCCEEDED(enc->Commit());
    }

} // namespace ZufyUI
