#pragma once
// ============================================================================
// pch.h —— 预编译头
// 放「重且相对稳定」的东西：系统头 + STL + 本库头。
//   * 改 demo(ZufyUI.cpp) → 复用 PCH，几乎只剩链接时间（日常迭代快）
//   * 改本库头 → 重建 PCH（和以前一次全量编译差不多）
// 如果中间层（日志/调试显示）要跟构建配置联动，改这里的 ZufyUI_DEBUG 即可。
// ============================================================================
#define ZufyUI_DEBUG
// 显式授权本库替应用做「应用身份自注册」（写 HKCU 注册表 + 把图标缓存到本地、退出清缓存）。
// 不上报/不联网，仅本机；不定义则 RegisterApp 只设置进程 AUMID，零副作用。
#define ZUFYUI_ALLOW_APP_REGISTRATION

// ---- Win32 / 系统 ----
#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

// ---- Direct2D / Direct3D / DirectComposition / DirectWrite / WIC / DWM ----
#include <d2d1.h>
#include <d2d1helper.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1effects.h>
#include <d2d1effects_2.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <wincodec.h>

// ---- WinRT (Composition / Effects) + WRL ----
#include <roapi.h>
#include <dispatcherqueue.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.foundation.h>
#include <windows.graphics.directx.h>
#include <windows.graphics.effects.h>
#include <windows.graphics.effects.interop.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.effects.h>
#include <windows.ui.composition.interop.h>

// ---- STL ----
#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---- 本库头（放最后，依赖上面的系统头/STL）----
#include "ZufyUI.h"
#include "ZufyUIWidgets.h"
#include "ZufyUIAcrylic.h"
#include "ZufyUIImages.h"
#include "ZufyUIWindowTool.h"
#include "ZDataViewer.h"
