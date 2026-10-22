#pragma once
// ZufyUIDebugger 预编译头。使用主库头（include 路径由工程指向上一级目录）。
#define ZufyUI_DEBUG
#include <windows.h>
#include <uiautomation.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <map>
#include <algorithm>
#include <pdh.h>
#include <psapi.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#include "ZufyUI.h"
#include "ZufyUIWidgets.h"
#include "ZufyUIAcrylic.h"
#include "ZufyUIImages.h"
#include "ZufyUIWindowTool.h"
#include "ZDataViewer.h"
#include "ZufyUICharts.h"
