#pragma once
// ============================================================================
// ZufyUIAcrylic.h —— 亚克力/云母材质的 DComp 效果图实现（独立头）
// ----------------------------------------------------------------------------
// 配方移植自 ALTaleX531/Win32Acrylic（MIT），其内容又来自 microsoft-ui-xaml 的
// AcrylicBrush.cpp。包含：手写 IGraphicsEffect 效果类 + 采集系统噪点纹理 +
// 组装官方效果图（模糊 / 亮度-颜色混合 / 色调 / 噪点）。
//
// 官方配方（Win11/19H1+ Luminosity 版）：
//   Blend(MULTIPLY, noise)
//     ├─ OpacityEffect(0.02) ← Border(wrap) ← Noise texture
//     └─ Blend(LUMINOSITY)   ← TintColor
//          └─ Blend(COLOR)   ← LuminosityColor
//               └─ GaussianBlur(30) ← Backdrop
// 注意：D2D 里 Luminosity 与 Color 两个 blend mode 名字是反的，代码里有体现。
// ============================================================================

#include <windows.h>
#include <roapi.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <d2d1_1helper.h>
#include <d2d1effects_2.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <windows.foundation.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>
#include <windows.ui.composition.effects.h>
#include <windows.graphics.effects.h>
#include <windows.graphics.effects.interop.h>
#include <windows.graphics.directx.h>
#include <unordered_map>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dxguid.lib")

namespace ZufyUI {
    namespace detail_fx {
        using Microsoft::WRL::ComPtr;
        using Microsoft::WRL::RuntimeClass;
        using Microsoft::WRL::RuntimeClassFlags;
        using Microsoft::WRL::WinRtClassicComMix;
        using Microsoft::WRL::Wrappers::HString;
        using Microsoft::WRL::Wrappers::HStringReference;
        using namespace ABI::Windows::UI::Composition::Effects;
        using namespace ABI::Windows::UI::Composition;
        using namespace ABI::Windows::UI::Composition::Desktop;
        using namespace ABI::Windows::Graphics::Effects;
        using namespace ABI::Windows::Graphics::DirectX;
        using namespace ABI::Windows::Foundation;

        class CompositionEffectSource {
            ComPtr<ICompositionEffectSourceParameter> param_;
        public:
            explicit CompositionEffectSource(const HSTRING& name) {
                ComPtr<ICompositionEffectSourceParameterFactory> factory;
                if (FAILED(GetActivationFactory(
                    HStringReference(RuntimeClass_Windows_UI_Composition_CompositionEffectSourceParameter).Get(),
                    &factory))) return;
                factory->Create(name, &param_);
            }
            operator ICompositionEffectSourceParameter* () { return param_.Get(); }
            operator IGraphicsEffectSource* () {
                ComPtr<IGraphicsEffectSource> src;
                if (param_) param_->QueryInterface(IID_PPV_ARGS(&src));
                return src.Get();
            }
        };

        class CompositionEffect :
            public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IGraphicsEffect, IGraphicsEffectSource, IGraphicsEffectD2D1Interop> {
        public:
            explicit CompositionEffect(REFCLSID effectId) : effectId_(effectId) {
                GetActivationFactory(
                    HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(),
                    &propertyValueFactory_);
            }
            virtual ~CompositionEffect() = default;

            HRESULT STDMETHODCALLTYPE get_Name(HSTRING* name) override {
                return WindowsDuplicateString(name_.Get(), name);
            }
            HRESULT STDMETHODCALLTYPE put_Name(HSTRING name) override { return name_.Set(name); }
            HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) override {
                if (!id) return E_POINTER;
                *id = effectId_;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR, UINT*, GRAPHICS_EFFECT_PROPERTY_MAPPING*) override {
                return E_NOTIMPL;
            }
            HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) override {
                if (!count) return E_POINTER;
                *count = (UINT)properties_.size();
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE GetProperty(UINT index, IPropertyValue** value) override {
                if (!value) return E_POINTER;
                auto it = properties_.find((int)index);
                if (it == properties_.end()) return E_INVALIDARG;
                return it->second.CopyTo(value);
            }
            HRESULT STDMETHODCALLTYPE GetSource(UINT index, IGraphicsEffectSource** source) override {
                if (!source) return E_POINTER;
                auto it = sources_.find((int)index);
                if (it == sources_.end()) return E_INVALIDARG;
                return it->second.CopyTo(source);
            }
            HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) override {
                if (!count) return E_POINTER;
                *count = (UINT)sources_.size();
                return S_OK;
            }

            void SetInput(UINT index, IGraphicsEffectSource* source) {
                sources_[(int)index] = ComPtr<IGraphicsEffectSource>(source);
            }
            void SetInput(IGraphicsEffectSource* source) { SetInput(0, source); }

        protected:
            ComPtr<IPropertyValue> Property(float value) {
                ComPtr<IPropertyValue> pv;
                propertyValueFactory_->CreateSingle(value, &pv);
                return pv;
            }
            ComPtr<IPropertyValue> Property(UINT32 value) {
                ComPtr<IPropertyValue> pv;
                propertyValueFactory_->CreateUInt32(value, &pv);
                return pv;
            }
            template<size_t N>
            ComPtr<IPropertyValue> Property(float(&array)[N]) {
                ComPtr<IPropertyValue> pv;
                propertyValueFactory_->CreateSingleArray((UINT32)N, array, &pv);
                return pv;
            }
            void SetProperty(UINT index, const ComPtr<IPropertyValue>& value) {
                properties_[(int)index] = value;
            }

            CLSID effectId_{};
            HString name_;
            std::unordered_map<int, ComPtr<IPropertyValue>> properties_;
            ComPtr<IPropertyValueStatics> propertyValueFactory_;
            std::unordered_map<int, ComPtr<IGraphicsEffectSource>> sources_;
        };

        class GaussianBlurEffect : public CompositionEffect {
        public:
            explicit GaussianBlurEffect(float deviation = 30.0f) : CompositionEffect(CLSID_D2D1GaussianBlur) {
                SetProperty(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, Property(deviation));
                SetProperty(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, Property((UINT32)D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED));
                SetProperty(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, Property((UINT32)D2D1_BORDER_MODE_HARD));
            }
        };

        class BorderEffect : public CompositionEffect {
        public:
            BorderEffect() : CompositionEffect(CLSID_D2D1Border) {
                SetProperty(D2D1_BORDER_PROP_EDGE_MODE_X, Property((UINT32)D2D1_BORDER_EDGE_MODE_CLAMP));
                SetProperty(D2D1_BORDER_PROP_EDGE_MODE_Y, Property((UINT32)D2D1_BORDER_EDGE_MODE_CLAMP));
            }
            void SetExtendX(D2D1_BORDER_EDGE_MODE m) { SetProperty(D2D1_BORDER_PROP_EDGE_MODE_X, Property((UINT32)m)); }
            void SetExtendY(D2D1_BORDER_EDGE_MODE m) { SetProperty(D2D1_BORDER_PROP_EDGE_MODE_Y, Property((UINT32)m)); }
        };

        class OpacityEffect : public CompositionEffect {
        public:
            OpacityEffect() : CompositionEffect(CLSID_D2D1Opacity) {
                SetProperty(D2D1_OPACITY_PROP_OPACITY, Property(1.0f));
            }
            void SetOpacity(float v) { SetProperty(D2D1_OPACITY_PROP_OPACITY, Property(v)); }
        };

        class ColorSourceEffect : public CompositionEffect {
        public:
            ColorSourceEffect() : CompositionEffect(CLSID_D2D1Flood) {
                float d[4] = { 0.f, 0.f, 0.f, 1.f };
                SetProperty(D2D1_FLOOD_PROP_COLOR, Property(d));
            }
            void SetColor(const D2D1_COLOR_F& c) {
                float v[4] = { c.r, c.g, c.b, c.a };
                SetProperty(D2D1_FLOOD_PROP_COLOR, Property(v));
            }
        };

        class SaturationEffect : public CompositionEffect {
        public:
            SaturationEffect() : CompositionEffect(CLSID_D2D1Saturation) {
                SetProperty(D2D1_SATURATION_PROP_SATURATION, Property(1.0f));
            }
            void SetSaturation(float v) { SetProperty(D2D1_SATURATION_PROP_SATURATION, Property(v)); }
        };

        class BlendEffect : public CompositionEffect {
        public:
            BlendEffect() : CompositionEffect(CLSID_D2D1Blend) {
                SetProperty(D2D1_BLEND_PROP_MODE, Property((UINT32)D2D1_BLEND_MODE_MULTIPLY));
            }
            void SetBlendMode(D2D1_BLEND_MODE m) { SetProperty(D2D1_BLEND_PROP_MODE, Property((UINT32)m)); }
            void SetBackground(IGraphicsEffectSource* s) { SetInput(0, s); }
            void SetForeground(IGraphicsEffectSource* s) { SetInput(1, s); }
        };

        class CompositeStepEffect : public CompositionEffect {
        public:
            CompositeStepEffect() : CompositionEffect(CLSID_D2D1Composite) {
                SetProperty(D2D1_COMPOSITE_PROP_MODE, Property((UINT32)D2D1_COMPOSITE_MODE_SOURCE_OVER));
            }
            void SetCompositeMode(D2D1_COMPOSITE_MODE m) { SetProperty(D2D1_COMPOSITE_PROP_MODE, Property((UINT32)m)); }
            void SetDestination(IGraphicsEffectSource* s) { SetInput(0, s); }
            void SetSource(IGraphicsEffectSource* s) { SetInput(1, s); }
        };

        // 通用简单效果（按 CLSID 设置 float/UINT 属性）
        class SimpleEffect : public CompositionEffect {
        public:
            explicit SimpleEffect(REFCLSID id) : CompositionEffect(id) {}
            void SetFloat(UINT index, float v) { SetProperty(index, Property(v)); }
            void SetUint(UINT index, UINT32 v) { SetProperty(index, Property(v)); }
        };

        // ---- 采集系统噪点纹理（与官方一致的 2% 噪点源）----
        // 官方噪点是“1 物理像素一颗”。高 DPI 下把噪点按 96/dpi 缩小并平铺，
        // 否则 256 DIP 的贴图会被放大成一格一格的块（200% 下 1 颗变 2px）。
        // 从系统 XAML 控件资源加载官方噪点贴图（WIC 位图，亚克力/手绘云母共用）
        inline ComPtr<IWICBitmap> LoadSystemNoiseWIC(IWICImagingFactory* wic) {
            if (!wic) return nullptr;
            // 缓存：模块句柄只 LoadLibrary 一次；解码后的噪点位图只解一次。
            // 二者都与 DPI 无关，而噪点 brush 会在 WM_DPICHANGED 时 Reset → 重建，
            // 否则每次 DPI 变化都要重新 LoadLibrary + 找资源 + WIC 解码。
            static HINSTANCE s_mod = nullptr;
            if (!s_mod) {
                s_mod = LoadLibraryExW(L"Windows.UI.Xaml.Controls.dll", nullptr,
                    LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
                if (!s_mod) return nullptr;
            }
            static ComPtr<IWICBitmap> s_cached;
            if (s_cached) return s_cached;

            HRSRC hResource = FindResourceW(s_mod, MAKEINTRESOURCEW(2000), RT_RCDATA);
            if (!hResource) return nullptr;
            HGLOBAL hGlobal = LoadResource(s_mod, hResource);
            DWORD size = SizeofResource(s_mod, hResource);
            BYTE* data = hGlobal ? (BYTE*)LockResource(hGlobal) : nullptr;
            ComPtr<IStream> stream(data ? SHCreateMemStream(data, size) : nullptr);
            if (hGlobal) { UnlockResource(hGlobal); FreeResource(hGlobal); }
            if (!stream) return nullptr;
            ComPtr<IWICBitmapDecoder> decoder;
            if (FAILED(wic->CreateDecoderFromStream(stream.Get(), &GUID_VendorMicrosoft,
                WICDecodeMetadataCacheOnDemand, &decoder))) return nullptr;
            ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;
            ComPtr<IWICFormatConverter> conv;
            if (FAILED(wic->CreateFormatConverter(&conv))) return nullptr;
            if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return nullptr;
            if (FAILED(wic->CreateBitmapFromSource(conv.Get(), WICBitmapNoCache, &s_cached))) return nullptr;
            return s_cached;
        }

        inline ComPtr<ICompositionSurfaceBrush> CreateSystemNoiseBrush(
            ICompositor* compositor, ICompositionGraphicsDevice* graphicsDevice, IWICImagingFactory* wic) {
            if (!compositor || !graphicsDevice || !wic) return nullptr;

            // 官方噪点贴图是 1 物理像素一颗。关键：把绘制用的 D2D 上下文 DPI 强制为 96，
            // 使 DrawBitmap 的目标矩形单位 = 物理像素；否则会按系统 DPI 缩放 → 噪点被插值糊掉。
            UINT sysDpi = GetDpiForSystem(); if (!sysDpi) sysDpi = 96;
            const int baseSize = 256;                                    // 系统噪点纹理的物理尺寸
            int physSize = (baseSize * (int)sysDpi + 95) / 96;           // 200% -> 512
            if (physSize < baseSize) physSize = baseSize;

            ComPtr<ICompositionDrawingSurface> surface;
            if (FAILED(graphicsDevice->CreateDrawingSurface(
                { (FLOAT)physSize, (FLOAT)physSize },
                DirectXPixelFormat::DirectXPixelFormat_B8G8R8A8UIntNormalized,
                DirectXAlphaMode::DirectXAlphaMode_Premultiplied, &surface)) || !surface) return nullptr;

            ComPtr<ICompositionSurfaceBrush> surfaceBrush;
            if (FAILED(compositor->CreateSurfaceBrushWithSurface(
                reinterpret_cast<ICompositionSurface*>(surface.Get()), &surfaceBrush)) || !surfaceBrush) return nullptr;

            ComPtr<ICompositionDrawingSurfaceInterop> surfaceInterop;
            if (FAILED(surface->QueryInterface(IID_PPV_ARGS(&surfaceInterop)))) return nullptr;

            ComPtr<IWICBitmap> wicBitmap = LoadSystemNoiseWIC(wic);
            if (!wicBitmap) return nullptr;

            ComPtr<ID2D1DeviceContext> dc;
            POINT offset = { 0, 0 };
            if (FAILED(surfaceInterop->BeginDraw(nullptr, IID_PPV_ARGS(&dc), &offset)) || !dc) return nullptr;

            FLOAT oldDpiX = 96.0f, oldDpiY = 96.0f;
            dc->GetDpi(&oldDpiX, &oldDpiY);
            dc->SetDpi(96.0f, 96.0f);   // 1 DIP = 1 物理像素，杜绝插值
            dc->Clear();
            ComPtr<ID2D1Bitmap1> bmp;
            // 显式给源位图 96 DPI（与上下文一致；否则会按 DPI 比例缩放）
            D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                96.0f, 96.0f);
            if (SUCCEEDED(dc->CreateBitmapFromWicBitmap(wicBitmap.Get(), &bp, &bmp)) && bmp) {
                // 用纹理真实尺寸平铺（不同系统版本纹理尺寸可能不是 256）
                D2D1_SIZE_U sz = bmp->GetPixelSize();
                int tw = (int)sz.width; if (tw <= 0) tw = baseSize;
                int th = (int)sz.height; if (th <= 0) th = baseSize;
                int repsX = (physSize + tw - 1) / tw;
                int repsY = (physSize + th - 1) / th;
                for (int y = 0; y < repsY; ++y)
                    for (int x = 0; x < repsX; ++x)
                        dc->DrawBitmap(bmp.Get(), D2D1::RectF(
                            (FLOAT)(x * tw), (FLOAT)(y * th),
                            (FLOAT)(x * tw + tw), (FLOAT)(y * th + th)),
                            1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);   // 必须 NEAREST
            }
            dc->SetDpi(oldDpiX, oldDpiY);
            surfaceInterop->EndDraw();

            surfaceBrush->put_HorizontalAlignmentRatio(0.f);
            surfaceBrush->put_VerticalAlignmentRatio(0.f);
            return surfaceBrush;
        }

        // ---- 官方亚克力配方（Luminosity 版）----
        inline ComPtr<ICompositionBrush> BuildAcrylicBrush(
            ICompositor* compositor, ICompositionBrush* backdrop, ICompositionBrush* noise,
            const D2D1_COLOR_F& tint, const D2D1_COLOR_F& luminosity, float saturation = 1.0f,
            float noiseOpacity = 0.02f, float blur = 30.0f) {
            if (!compositor) return nullptr;

            auto border = Microsoft::WRL::Make<BorderEffect>();
            border->SetExtendX(D2D1_BORDER_EDGE_MODE_WRAP);
            border->SetExtendY(D2D1_BORDER_EDGE_MODE_WRAP);
            border->SetInput(static_cast<IGraphicsEffectSource*>(CompositionEffectSource(HStringReference(L"Noice").Get())));

            auto opacity = Microsoft::WRL::Make<OpacityEffect>();
            opacity->SetOpacity(noiseOpacity);
            opacity->SetInput(static_cast<IGraphicsEffectSource*>(border.Get()));

            auto blurFx = Microsoft::WRL::Make<GaussianBlurEffect>(blur);
            blurFx->SetInput(static_cast<IGraphicsEffectSource*>(CompositionEffectSource(HStringReference(L"Backdrop").Get())));

            // Saturation 层（官方 Legacy/RS2 配方有这一步；Luminosity 配方 saturation==1 时跳过）
            IGraphicsEffectSource* afterBlur = static_cast<IGraphicsEffectSource*>(blurFx.Get());
            ComPtr<SaturationEffect> satFx;
            if (saturation != 1.0f) {
                satFx = Microsoft::WRL::Make<SaturationEffect>();
                satFx->SetSaturation(saturation);
                satFx->SetInput(afterBlur);
                afterBlur = static_cast<IGraphicsEffectSource*>(satFx.Get());
            }

            auto tintFx = Microsoft::WRL::Make<ColorSourceEffect>();
            tintFx->SetColor(tint);
            auto lumFx = Microsoft::WRL::Make<ColorSourceEffect>();
            lumFx->SetColor(luminosity);

            // 名字是反的：COLOR 做 Luminosity，LUMINOSITY 做 Color
            auto lumBlend = Microsoft::WRL::Make<BlendEffect>();
            lumBlend->SetBlendMode(D2D1_BLEND_MODE_COLOR);
            lumBlend->SetBackground(afterBlur);
            lumBlend->SetForeground(static_cast<IGraphicsEffectSource*>(lumFx.Get()));

            // Tint 层：用 CompositeStep(SOURCE_OVER) 叠色 —— 这样 tint 的 alpha 才生效
            auto tintStep = Microsoft::WRL::Make<CompositeStepEffect>();
            tintStep->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
            tintStep->SetDestination(static_cast<IGraphicsEffectSource*>(lumBlend.Get()));
            tintStep->SetSource(static_cast<IGraphicsEffectSource*>(tintFx.Get()));

            auto noiseBlend = Microsoft::WRL::Make<BlendEffect>();
            noiseBlend->SetBlendMode(D2D1_BLEND_MODE_MULTIPLY);
            noiseBlend->SetBackground(static_cast<IGraphicsEffectSource*>(tintStep.Get()));
            noiseBlend->SetForeground(static_cast<IGraphicsEffectSource*>(opacity.Get()));

            ComPtr<ICompositionEffectFactory> factory;
            if (FAILED(compositor->CreateEffectFactory(noiseBlend.Get(), &factory))) return nullptr;
            ComPtr<ICompositionEffectBrush> brush;
            if (FAILED(factory->CreateBrush(&brush))) return nullptr;
            if (noise) brush->SetSourceParameter(HStringReference(L"Noice").Get(), noise);
            if (backdrop) brush->SetSourceParameter(HStringReference(L"Backdrop").Get(), backdrop);
            ComPtr<ICompositionBrush> out;
            brush.As(&out);
            return out;
        }

        // ---- 官方云母配方（Win11 的“模糊壁纸”背景刷）----
        inline ComPtr<ICompositionBrush> BuildMicaBrush(
            ICompositor* compositor, ICompositionBrush* noise,
            const D2D1_COLOR_F& tint, const D2D1_COLOR_F& luminosity,
            float noiseOpacity = 0.02f) {
            if (!compositor) return nullptr;
            ComPtr<ICompositorWithBlurredWallpaperBackdropBrush> cwb;
            if (FAILED(compositor->QueryInterface(IID_PPV_ARGS(&cwb)))) return nullptr;
            ComPtr<ICompositionBackdropBrush> wallpaper;
            if (FAILED(cwb->TryCreateBlurredWallpaperBackdropBrush(&wallpaper)) || !wallpaper) return nullptr;

            auto border = Microsoft::WRL::Make<BorderEffect>();
            border->SetExtendX(D2D1_BORDER_EDGE_MODE_WRAP);
            border->SetExtendY(D2D1_BORDER_EDGE_MODE_WRAP);
            border->SetInput(static_cast<IGraphicsEffectSource*>(CompositionEffectSource(HStringReference(L"Noice").Get())));

            auto opacity = Microsoft::WRL::Make<OpacityEffect>();
            opacity->SetOpacity(noiseOpacity);
            opacity->SetInput(static_cast<IGraphicsEffectSource*>(border.Get()));

            auto tintFx = Microsoft::WRL::Make<ColorSourceEffect>();
            tintFx->SetColor(tint);
            auto lumFx = Microsoft::WRL::Make<ColorSourceEffect>();
            lumFx->SetColor(luminosity);

            auto lumBlend = Microsoft::WRL::Make<BlendEffect>();
            lumBlend->SetBlendMode(D2D1_BLEND_MODE_COLOR);
            lumBlend->SetBackground(static_cast<IGraphicsEffectSource*>(CompositionEffectSource(HStringReference(L"BlurredWallpaperBackdrop").Get())));
            lumBlend->SetForeground(static_cast<IGraphicsEffectSource*>(lumFx.Get()));

            auto colorBlend = Microsoft::WRL::Make<BlendEffect>();
            colorBlend->SetBlendMode(D2D1_BLEND_MODE_LUMINOSITY);
            colorBlend->SetBackground(static_cast<IGraphicsEffectSource*>(lumBlend.Get()));
            colorBlend->SetForeground(static_cast<IGraphicsEffectSource*>(tintFx.Get()));

            auto noiseBlend = Microsoft::WRL::Make<BlendEffect>();
            noiseBlend->SetBlendMode(D2D1_BLEND_MODE_MULTIPLY);
            noiseBlend->SetBackground(static_cast<IGraphicsEffectSource*>(colorBlend.Get()));
            noiseBlend->SetForeground(static_cast<IGraphicsEffectSource*>(opacity.Get()));

            ComPtr<ICompositionEffectFactory> factory;
            if (FAILED(compositor->CreateEffectFactory(noiseBlend.Get(), &factory))) return nullptr;
            ComPtr<ICompositionEffectBrush> brush;
            if (FAILED(factory->CreateBrush(&brush))) return nullptr;
            if (noise) brush->SetSourceParameter(HStringReference(L"Noice").Get(), noise);
            brush->SetSourceParameter(HStringReference(L"BlurredWallpaperBackdrop").Get(),
                reinterpret_cast<ICompositionBrush*>(wallpaper.Get()));
            ComPtr<ICompositionBrush> out;
            brush.As(&out);
            return out;
        }

    } // namespace detail_fx
} // namespace ZufyUI
