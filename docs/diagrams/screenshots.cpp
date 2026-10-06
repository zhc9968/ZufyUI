// screenshots.cpp
// 用途：生成 README「快速开始」的三张配图（旧图是旧 UI，需重截）。
//   第 1 步 quickstart.png       ->  screenshots.exe
//   第 2 步 backdrop-mica.png    ->  screenshots.exe --mica
//   第 3 步 custom-titlebar.png  ->  screenshots.exe --titlebar
//
// 编译（把 C:\project\ZufyUI 加入包含路径；系统库库内已 #pragma comment 自动链接）：
//   cl /std:c++17 /EHsc /O2 /DUNICODE /D_UNICODE /SUBSYSTEM:WINDOWS screenshots.cpp user32.lib gdi32.lib
// 或直接放进一个空的 Windows 桌面项目里编译。

#include "ZufyUI.h"
#include "ZufyUIWidgets.h"
#include "ZufyUIWindowTool.h"     // DefaultTitleBar
using namespace ZufyUI;

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    Application app = Application::Instance();
    Window win;
    if (!win.Create(1000, 700, L"ZufyUI Demo"))
        return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto btn = std::make_shared<Button>(L"点我");
    btn->Connect(btn->Clicked, []() {
        MessageBoxW(nullptr, L"Hello ZufyUI!", L"提示", MB_OK);
    });

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto state = std::make_shared<Label>(L"开关：关");
    toggle->Connect(toggle->Toggled, [state](bool on) {
        state->SetText(on ? L"开关：开" : L"开关：关");
    });

    row->AddChild(btn);
    row->AddChild(toggle);
    row->AddChild(state);
    root->AddChild(row);

    const std::wstring cmd = GetCommandLineW();
    if (cmd.find(L"--mica") != std::wstring::npos)
        win.SetBackdrop(Backdrop::Mica, 0x00000000);
    if (cmd.find(L"--titlebar") != std::wstring::npos) {
        auto bar = std::make_shared<DefaultTitleBar>();
        win.SetCustomTitleBar(bar);
        win.SetBackdrop(Backdrop::Mica, 0x00000000);   // 与 README 第 3 步一致：标题栏 + 云母
    }

    win.Show();
    win.Run();
    return 0;
}
