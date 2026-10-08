# ZufyUI 硬编码颜色字面量审计清单

> 只读审计，未修改任何源码。范围：`ZufyUI.h` / `ZufyUIWidgets.h` / `ZDataViewer.h` / `ZufyUICharts.h` / `ZufyUIImages.h` / `ZufyUIWindowTool.h` / `ZufyUIDragDrop.h` / `ZufyUI.cpp`。

## 每文件数量汇总

| 文件 | 字面量数量 |
|---|---:|
| ZufyUI.h | 124 |
| ZufyUIWidgets.h | 149 |
| ZDataViewer.h | 45 |
| ZufyUICharts.h | 30 |
| ZufyUIImages.h | 1 |
| ZufyUIWindowTool.h | 20 |
| ZufyUIDragDrop.h | 0 |
| ZufyUI.cpp | 66 |
| **合计** | **435** |

## 全部硬编码颜色字面量

| 颜色(RGB) | Alpha | 形式 | 文件:行 | 控件/类 | 函数/功能 | 备注 |
|---|---:|---|---|---|---|---|
| #000000 | 255 | Color | ZufyUI.h:633 | Theme | Theme::Theme | 主题构造：把所有角色预置为黑(a=1) |
| #1C1C1C | 255 | FromArgb | ZufyUI.h:644 | Theme | Theme::Light | 主题预设·文字 |
| #606060 | 255 | FromArgb | ZufyUI.h:645 | Theme | Theme::Light | 主题预设·次要文字 |
| #989898 | 255 | FromArgb | ZufyUI.h:646 | Theme | Theme::Light | 主题预设·禁用文字 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:647 | Theme | Theme::Light | 主题预设·强调色上文字 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:648 | Theme | Theme::Light | 主题预设·表面背景 |
| #F3F3F3 | 255 | FromArgb | ZufyUI.h:649 | Theme | Theme::Light | 主题预设·表面背景(次) |
| #FCFCFC | 255 | FromArgb | ZufyUI.h:650 | Theme | Theme::Light | 主题预设·表面背景(浮起) |
| #000000 | 60 | FromArgb | ZufyUI.h:651 | Theme | Theme::Light | 主题预设·遮罩 |
| #F3F3F3 | 255 | FromArgb | ZufyUI.h:652 | Theme | Theme::Light | 主题预设·控件背景 |
| #E4E4E4 | 255 | FromArgb | ZufyUI.h:653 | Theme | Theme::Light | 主题预设·控件悬停背景 |
| #D2D2D2 | 255 | FromArgb | ZufyUI.h:654 | Theme | Theme::Light | 主题预设·控件按下背景 |
| #EEEEEE | 255 | FromArgb | ZufyUI.h:655 | Theme | Theme::Light | 主题预设·控件禁用背景 |
| #0078D4 | 255 | FromArgb | ZufyUI.h:656 | Theme | Theme::Light | 主题预设·强调色 |
| #1A8CE4 | 255 | FromArgb | ZufyUI.h:657 | Theme | Theme::Light | 主题预设·强调色悬停 |
| #0064B4 | 255 | FromArgb | ZufyUI.h:658 | Theme | Theme::Light | 主题预设·强调色按下 |
| #C8C8C8 | 255 | FromArgb | ZufyUI.h:659 | Theme | Theme::Light | 主题预设·边框 |
| #E2E2E2 | 255 | FromArgb | ZufyUI.h:660 | Theme | Theme::Light | 主题预设·分割线 |
| #0078D4 | 255 | FromArgb | ZufyUI.h:661 | Theme | Theme::Light | 主题预设·焦点环 |
| #B3D7F3 | 255 | FromArgb | ZufyUI.h:662 | Theme | Theme::Light | 主题预设·选中背景 |
| #E6E6E6 | 200 | FromArgb | ZufyUI.h:663 | Theme | Theme::Light | 主题预设·滚动条轨道 |
| #828282 | 230 | FromArgb | ZufyUI.h:664 | Theme | Theme::Light | 主题预设·滚动条滑块 |
| #505050 | 255 | FromArgb | ZufyUI.h:665 | Theme | Theme::Light | 主题预设·滚动条滑块悬停 |
| #C42B1C | 255 | FromArgb | ZufyUI.h:666 | Theme | Theme::Light | 主题预设·危险红 |
| #E09814 | 255 | FromArgb | ZufyUI.h:667 | Theme | Theme::Light | 主题预设·警告黄 |
| #107C10 | 255 | FromArgb | ZufyUI.h:668 | Theme | Theme::Light | 主题预设·成功绿 |
| #0078D4 | 255 | FromArgb | ZufyUI.h:669 | Theme | Theme::Light | 主题预设·信息蓝 |
| #EBEBEB | 255 | FromArgb | ZufyUI.h:675 | Theme | Theme::Dark | 主题预设·文字 |
| #B2B2B2 | 255 | FromArgb | ZufyUI.h:676 | Theme | Theme::Dark | 主题预设·次要文字 |
| #787878 | 255 | FromArgb | ZufyUI.h:677 | Theme | Theme::Dark | 主题预设·禁用文字 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:678 | Theme | Theme::Dark | 主题预设·强调色上文字 |
| #202020 | 255 | FromArgb | ZufyUI.h:679 | Theme | Theme::Dark | 主题预设·表面背景 |
| #2B2B2B | 255 | FromArgb | ZufyUI.h:680 | Theme | Theme::Dark | 主题预设·表面背景(次) |
| #323232 | 255 | FromArgb | ZufyUI.h:681 | Theme | Theme::Dark | 主题预设·表面背景(浮起) |
| #000000 | 120 | FromArgb | ZufyUI.h:682 | Theme | Theme::Dark | 主题预设·遮罩 |
| #3A3A3A | 255 | FromArgb | ZufyUI.h:683 | Theme | Theme::Dark | 主题预设·控件背景 |
| #4A4A4A | 255 | FromArgb | ZufyUI.h:684 | Theme | Theme::Dark | 主题预设·控件悬停背景 |
| #5A5A5A | 255 | FromArgb | ZufyUI.h:685 | Theme | Theme::Dark | 主题预设·控件按下背景 |
| #303030 | 255 | FromArgb | ZufyUI.h:686 | Theme | Theme::Dark | 主题预设·控件禁用背景 |
| #4CC2FF | 255 | FromArgb | ZufyUI.h:687 | Theme | Theme::Dark | 主题预设·强调色 |
| #6ECDFF | 255 | FromArgb | ZufyUI.h:688 | Theme | Theme::Dark | 主题预设·强调色悬停 |
| #28A0DC | 255 | FromArgb | ZufyUI.h:689 | Theme | Theme::Dark | 主题预设·强调色按下 |
| #5A5A5A | 255 | FromArgb | ZufyUI.h:690 | Theme | Theme::Dark | 主题预设·边框 |
| #464646 | 255 | FromArgb | ZufyUI.h:691 | Theme | Theme::Dark | 主题预设·分割线 |
| #4CC2FF | 255 | FromArgb | ZufyUI.h:692 | Theme | Theme::Dark | 主题预设·焦点环 |
| #264F78 | 255 | FromArgb | ZufyUI.h:693 | Theme | Theme::Dark | 主题预设·选中背景 |
| #3C3C3C | 200 | FromArgb | ZufyUI.h:694 | Theme | Theme::Dark | 主题预设·滚动条轨道 |
| #828282 | 230 | FromArgb | ZufyUI.h:695 | Theme | Theme::Dark | 主题预设·滚动条滑块 |
| #B4B4B4 | 255 | FromArgb | ZufyUI.h:696 | Theme | Theme::Dark | 主题预设·滚动条滑块悬停 |
| #FF6347 | 255 | FromArgb | ZufyUI.h:697 | Theme | Theme::Dark | 主题预设·危险红 |
| #FFB900 | 255 | FromArgb | ZufyUI.h:698 | Theme | Theme::Dark | 主题预设·警告黄 |
| #6CCB5F | 255 | FromArgb | ZufyUI.h:699 | Theme | Theme::Dark | 主题预设·成功绿 |
| #4CC2FF | 255 | FromArgb | ZufyUI.h:700 | Theme | Theme::Dark | 主题预设·信息蓝 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:706 | Theme | Theme::HighContrast | 主题预设·文字 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:707 | Theme | Theme::HighContrast | 主题预设·次要文字 |
| #A0A0A0 | 255 | FromArgb | ZufyUI.h:708 | Theme | Theme::HighContrast | 主题预设·禁用文字 |
| #000000 | 255 | FromArgb | ZufyUI.h:709 | Theme | Theme::HighContrast | 主题预设·强调色上文字 |
| #000000 | 255 | FromArgb | ZufyUI.h:710 | Theme | Theme::HighContrast | 主题预设·表面背景 |
| #000000 | 255 | FromArgb | ZufyUI.h:711 | Theme | Theme::HighContrast | 主题预设·表面背景(次) |
| #000000 | 255 | FromArgb | ZufyUI.h:712 | Theme | Theme::HighContrast | 主题预设·表面背景(浮起) |
| #000000 | 160 | FromArgb | ZufyUI.h:713 | Theme | Theme::HighContrast | 主题预设·遮罩 |
| #000000 | 255 | FromArgb | ZufyUI.h:714 | Theme | Theme::HighContrast | 主题预设·控件背景 |
| #282828 | 255 | FromArgb | ZufyUI.h:715 | Theme | Theme::HighContrast | 主题预设·控件悬停背景 |
| #3C3C3C | 255 | FromArgb | ZufyUI.h:716 | Theme | Theme::HighContrast | 主题预设·控件按下背景 |
| #141414 | 255 | FromArgb | ZufyUI.h:717 | Theme | Theme::HighContrast | 主题预设·控件禁用背景 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:718 | Theme | Theme::HighContrast | 主题预设·强调色 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:719 | Theme | Theme::HighContrast | 主题预设·强调色悬停 |
| #DCDC00 | 255 | FromArgb | ZufyUI.h:720 | Theme | Theme::HighContrast | 主题预设·强调色按下 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:721 | Theme | Theme::HighContrast | 主题预设·边框 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:722 | Theme | Theme::HighContrast | 主题预设·分割线 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:723 | Theme | Theme::HighContrast | 主题预设·焦点环 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:724 | Theme | Theme::HighContrast | 主题预设·选中背景 |
| #000000 | 255 | FromArgb | ZufyUI.h:725 | Theme | Theme::HighContrast | 主题预设·滚动条轨道 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:726 | Theme | Theme::HighContrast | 主题预设·滚动条滑块 |
| #FFFFFF | 255 | FromArgb | ZufyUI.h:727 | Theme | Theme::HighContrast | 主题预设·滚动条滑块悬停 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:728 | Theme | Theme::HighContrast | 主题预设·危险红 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:729 | Theme | Theme::HighContrast | 主题预设·警告黄 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:730 | Theme | Theme::HighContrast | 主题预设·成功绿 |
| #FFFF00 | 255 | FromArgb | ZufyUI.h:731 | Theme | Theme::HighContrast | 主题预设·信息蓝 |
| #2ECC71 | 255 | hex | ZufyUI.h:1116 | free(全局) | 默认值常量 | 调试高亮框默认绿(ARGB) |
| #000005 | 107 | ColorF | ZufyUI.h:1623 | UIElement | 成员默认值 | 元素阴影默认色（近黑 42%） |
| #FFFFFF | 255 | FromArgb / DEFAULT-const | ZufyUI.h:2319 | Card | 默认值常量 | 背景 |
| #C8C8C8 | 255 | FromArgb / DEFAULT-const | ZufyUI.h:2320 | Card | 默认值常量 | 背景 |
| #F2F2F2 | 255 | ColorF | ZufyUI.h:2370 | Card | Draw | Card 禁用态背景 |
| #CCCCCC | 255 | ColorF | ZufyUI.h:2380 | Card | Draw | Card 禁用态边框 |
| #000000 | 0 | Color | ZufyUI.h:2441 | Page | — | Page 默认背景（全透明） |
| #FFFFFF | 255 | hex | ZufyUI.h:3092 | Window | 默认值常量 | 默认背景色(不透明白) |
| #FFFFFF | 0 | ColorF | ZufyUI.h:3101 | BackgroundParams | 成员默认值 | Mica/Acrylic 默认 tint（全透明白） |
| #000000 | 0 | ColorF | ZufyUI.h:3102 | BackgroundParams | 成员默认值 | 默认 Luminosity 层（全透明黑） |
| #000000 | 0 | ColorF | ZufyUI.h:3109 | Window | — | Mica 默认 Luminosity(全透明黑) |
| #FFFFFF | 223 | ColorF | ZufyUI.h:3109 | Window | — | Mica 默认 tint(白 87.5%) |
| #202020 | 102 | ColorF | ZufyUI.h:3129 | Window | SetBackgroundParams | 背景 |
| #000000 | 0 | ColorF | ZufyUI.h:3130 | Window | SetBackgroundParams | SetBackgroundParams |
| #202020 | 102 | ColorF | ZufyUI.h:3133 | Window | SetBackgroundParams | SetBackgroundParams |
| #202020 | 204 | ColorF | ZufyUI.h:3134 | Window | SetBackgroundParams | SetBackgroundParams |
| #000000 | 140 | ColorF | ZufyUI.h:3137 | Window | SetBackgroundParams | SetBackgroundParams |
| #262626 | 217 | ColorF | ZufyUI.h:3138 | Window | SetBackgroundParams | SetBackgroundParams |
| #FFFFFF | 51 | ColorF | ZufyUI.h:3141 | Window | SetBackgroundParams | SetBackgroundParams |
| #FFFFFF | 76 | ColorF | ZufyUI.h:3142 | Window | SetBackgroundParams | SetBackgroundParams |
| #000000 | 0 | Color / DEFAULT-const | ZufyUI.h:3151 | Window | 默认值常量 | 窗口默认背景色（全透明） |
| #000000 | 255 | RGB | ZufyUI.h:3156 | Window | — | 文字色 |
| #F0F0F0 | 255 | RGB | ZufyUI.h:3156 | Window | — | 标题栏底色 |
| #FFFFFF | 255 | hex | ZufyUI.h:3156 | Window | Window (Ctor) | 边框色 |
| #000000 | 0 | hex | ZufyUI.h:3311 | Window | SetBackdrop | 背景着色默认全透明 |
| #000000 | 0 | hex | ZufyUI.h:3334 | Window | SetDefaultBackdrop | 默认背景着色全透明 |
| #0078D6 | 230 | ColorF | ZufyUI.h:4942 | Window | DrawFocusAndTooltip | 焦点框/工具提示边框蓝 |
| #000000 | 76 | ColorF | ZufyUI.h:5002 | Window | DrawToolTip | 工具提示软阴影（黑 30%） |
| #000000 | 0 | ColorF | ZufyUI.h:5052 | Window | DrawDirtyCaches | 缓存清理为全透明 |
| #FFFFFF | 255 | ColorF | ZufyUI.h:5807 | Window | BuildWallpaperBitmap | 壁纸合成先铺纯白底 |
| #000000 | 255 | hex | ZufyUI.h:6008 | Window | BuildWallpaperBitmap | 壁纸噪点灰度底(A=255) |
| #000000 | 0 | hex | ZufyUI.h:6515 | MenuWindowBase | CreatePopup | 弹窗背景全透明 |
| #FFFFFF | 255 | ColorF | ZufyUI.h:6811 | MenuWindow | EnsureBrushes | 菜单底白色 |
| #000000 | 31 | ColorF | ZufyUI.h:6812 | MenuWindow | EnsureBrushes | 菜单项悬停黑 12% |
| #000000 | 255 | ColorF | ZufyUI.h:6813 | MenuWindow | EnsureBrushes | 菜单文字黑 |
| #CCCCCC | 255 | ColorF | ZufyUI.h:6814 | MenuWindow | EnsureBrushes | 菜单分隔线灰 |
| #000000 | 8 | ColorF | ZufyUI.h:6815 | MenuWindow | EnsureBrushes | 菜单阴影黑 3% |
| #000000 | 8 | ColorF | ZufyUI.h:6839 | MenuWindow | DrawMenuBody | 菜单阴影黑 3% |
| #0078D6 | 255 | ColorF | ZufyUI.h:6875 | MenuWindow | DrawMenu | 菜单高亮蓝 |
| #0078D6 | 255 | ColorF | ZufyUI.h:6880 | MenuWindow | DrawMenu | 菜单高亮蓝 |
| #FFFFFF | 255 | ColorF | ZufyUI.h:6882 | MenuWindow | DrawMenu | 菜单选中文字白 |
| #000000 | 255 | ColorF | ZufyUI.h:6895 | MenuWindow | DrawMenu | 菜单项文字黑 |
| #999999 | 255 | ColorF | ZufyUI.h:6897 | MenuWindow | DrawMenu | 菜单禁用文字灰 |
| #DB3333 | 255 | ColorF | ZufyUI.h:6898 | MenuWindow | DrawMenu | 菜单危险项红 |
| #737373 | 255 | ColorF | ZufyUI.h:6905 | MenuWindow | DrawMenu | 菜单分组/次要文字灰 |
| #000000 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:202 | Label | 默认值常量 | 文字 |
| #969696 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:208 | Label | 默认值常量 | 禁用 |
| #000000 | 0 | Color | ZufyUIWidgets.h:635 | Label | 成员默认值 | 背景 |
| #000000 | 0 | Color | ZufyUIWidgets.h:646 | Label | 成员默认值 | 字形 |
| #0078D4 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:668 | Button | 默认值常量 | 悬停 |
| #0069BE | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:669 | Button | 默认值常量 | 悬停 |
| #005AAA | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:670 | Button | 默认值常量 | 悬停 |
| #FFFFFF | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:671 | Button | 默认值常量 | 文字 |
| #D2D2D2 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:678 | Button | 默认值常量 | 禁用 |
| #787878 | 255 | FromArgb | ZufyUIWidgets.h:788 | Button | Draw | 文字 |
| #FAFAFA | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:900 | TextBox | 默认值常量 | 背景 |
| #CCCCCC | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:901 | TextBox | 默认值常量 | 背景 |
| #000000 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:902 | TextBox | 默认值常量 | 文字 |
| #B2D9FF | 128 | ColorF / DEFAULT-const | ZufyUIWidgets.h:903 | TextBox | 默认值常量 | 文字 |
| #EDEDED | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:904 | TextBox | 默认值常量 | 文字 |
| #666666 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:905 | TextBox | 默认值常量 | 文字 |
| #FFF7F7 | 255 | ColorF | ZufyUIWidgets.h:1102 | TextBox | Draw | 错误态 |
| #000000 | 255 | ColorF | ZufyUIWidgets.h:1198 | TextBox | Draw | 光标 |
| #000000 | 255 | ColorF | ZufyUIWidgets.h:1199 | TextBox | Draw | 光标 |
| #999999 | 255 | ColorF | ZufyUIWidgets.h:1678 | TextBox | 成员默认值 | 占位文字 |
| #0078D6 | 255 | ColorF | ZufyUIWidgets.h:1692 | TextBox | 成员默认值 | 文字 |
| #CC2121 | 255 | ColorF | ZufyUIWidgets.h:1693 | TextBox | 成员默认值 | 错误色 |
| #F2F2F2 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:1726 | ComboBox | 默认值常量 | 背景 |
| #E0E0E0 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:1727 | ComboBox | 默认值常量 | 背景 |
| #D9D9D9 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:1728 | ComboBox | 默认值常量 | 背景 |
| #999999 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:1729 | ComboBox | 默认值常量 | 背景 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:1730 | ComboBox | 默认值常量 | 背景 |
| #000000 | 255 | ColorF | ZufyUIWidgets.h:2103 | ComboBox | Draw | 选中 |
| #000000 | 255 | ColorF | ZufyUIWidgets.h:2104 | ComboBox | Draw | 选中 |
| #999999 | 255 | ColorF | ZufyUIWidgets.h:2129 | ComboBox | Draw | 占位文字 |
| #1A1A1A | 255 | ColorF | ZufyUIWidgets.h:2137 | ComboBox | Draw | 光标 |
| #4C4C4C | 255 | ColorF | ZufyUIWidgets.h:2145 | ComboBox | Draw | 箭头 |
| #4C4C4C | 255 | ColorF | ZufyUIWidgets.h:2146 | ComboBox | Draw | 箭头 |
| #4C4C4C | 255 | ColorF | ZufyUIWidgets.h:2155 | ComboBox | Draw | 字形 |
| #FFFFFF | 255 | ColorF | ZufyUIWidgets.h:2199 | ComboBox | DrawExpandedList | 下拉列表背景 |
| #FFFFFF | 255 | ColorF | ZufyUIWidgets.h:2200 | ComboBox | DrawExpandedList | 下拉列表背景 |
| #999999 | 255 | ColorF | ZufyUIWidgets.h:2203 | ComboBox | DrawExpandedList | 下拉列表背景 |
| #999999 | 255 | ColorF | ZufyUIWidgets.h:2204 | ComboBox | DrawExpandedList | 下拉列表背景 |
| #FFFFFF | 255 | ColorF | ZufyUIWidgets.h:2217 | ComboBox | DrawExpandedList | 列表项背景 |
| #E6E6E6 | 255 | ColorF | ZufyUIWidgets.h:2218 | ComboBox | DrawExpandedList | 列表项背景 |
| #B2B2B2 | 255 | ColorF | ZufyUIWidgets.h:2220 | ComboBox | DrawExpandedList | 列表项背景 |
| #CCCCCC | 255 | ColorF | ZufyUIWidgets.h:2221 | ComboBox | DrawExpandedList | 列表项背景 |
| #000000 | 255 | ColorF | ZufyUIWidgets.h:2227 | ComboBox | DrawExpandedList | 列表项文字 |
| #A6A6A6 | 255 | ColorF | ZufyUIWidgets.h:2227 | ComboBox | DrawExpandedList | 列表项文字 |
| #E6E6E6 | 204 | ColorF | ZufyUIWidgets.h:2254 | ComboBox | DrawExpandedList | 滚动条轨道 |
| #E6E6E6 | 204 | ColorF | ZufyUIWidgets.h:2255 | ComboBox | DrawExpandedList | 滚动条轨道 |
| #808080 | 230 | ColorF | ZufyUIWidgets.h:2260 | ComboBox | DrawExpandedList | 滚动条滑块 |
| #808080 | 230 | ColorF | ZufyUIWidgets.h:2261 | ComboBox | DrawExpandedList | 滚动条滑块 |
| #0078D4 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:2706 | ToggleSwitch | 默认值常量 | 进度 |
| #C8C8C8 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:2707 | ToggleSwitch | 默认值常量 | 进度 |
| #FFFFFF | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:2708 | ToggleSwitch | 默认值常量 | 旋钮 |
| #E0E0E0 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:2713 | ToggleSwitch | 默认值常量 | 禁用 |
| #8C8C8C | 255 | ColorF | ZufyUIWidgets.h:2794 | ToggleSwitch | Draw | 标签 |
| #262626 | 255 | ColorF | ZufyUIWidgets.h:2853 | ToggleSwitch | 成员默认值 | 标签 |
| #808080 | 230 | ColorF / DEFAULT-const | ZufyUIWidgets.h:2875 | ScrollBar | 默认值常量 | 滑块 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:2876 | ScrollBar | 默认值常量 | 滑块 |
| #000000 | 15 | ColorF / DEFAULT-const | ZufyUIWidgets.h:2877 | ScrollBar | 默认值常量 | 轨道 |
| #E6E6E6 | 204 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3070 | ScrollViewer | 默认值常量 | 轨道 |
| #808080 | 230 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3071 | ScrollViewer | 默认值常量 | 轨道 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3072 | ScrollViewer | 默认值常量 | 轨道 |
| #D9D9D9 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3595 | ProgressBar | 默认值常量 | 轨道 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3596 | ProgressBar | 默认值常量 | 轨道 |
| #999999 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3597 | ProgressBar | 默认值常量 | 边框 |
| #ADADAD | 255 | ColorF | ZufyUIWidgets.h:3735 | ProgressBar | DrawFillContent | 填充 |
| #262626 | 255 | ColorF | ZufyUIWidgets.h:3752 | ProgressBar | 成员默认值 | 文字 |
| #D9D9D9 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3776 | Slider | 默认值常量 | 轨道 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3777 | Slider | 默认值常量 | 轨道 |
| #F2F2F2 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3778 | Slider | 默认值常量 | 轨道 |
| #808080 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3779 | Slider | 默认值常量 | 轨道 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3780 | Slider | 默认值常量 | 边框 |
| #1A1A1A | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3781 | Slider | 默认值常量 | 边框 |
| #E6E6E6 | 255 | ColorF | ZufyUIWidgets.h:3847 | Slider | Draw | 轨道 |
| #BDBDBD | 255 | ColorF | ZufyUIWidgets.h:3857 | Slider | Draw | 轨道 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3982 | CheckBox | 默认值常量 | 方框色 |
| #9E9E9E | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3983 | CheckBox | 默认值常量 | 边框 |
| #FFFFFF | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:3984 | CheckBox | 默认值常量 | 边框 |
| #CCCCCC | 255 | ColorF | ZufyUIWidgets.h:4046 | CheckBox | DrawBox | 方框色 |
| #D1D1D1 | 255 | ColorF | ZufyUIWidgets.h:4047 | CheckBox | DrawBox | 边框 |
| #FFFFFF | 255 | ColorF | ZufyUIWidgets.h:4048 | CheckBox | DrawBox | 边框 |
| #8C8C8C | 255 | ColorF | ZufyUIWidgets.h:4123 | CheckBox | Draw | 边框 |
| #262626 | 255 | ColorF | ZufyUIWidgets.h:4199 | CheckBox | 成员默认值 | 标签 |
| #0078D6 | 38 | ColorF | ZufyUIWidgets.h:4200 | CheckBox | 成员默认值 | 悬停方框 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:4219 | RadioButton | 默认值常量 | 悬停 |
| #9E9E9E | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:4220 | RadioButton | 默认值常量 | 边框 |
| #262626 | 255 | ColorF / DEFAULT-const | ZufyUIWidgets.h:4221 | RadioButton | 默认值常量 | 边框 |
| #D6E8FB | 255 | ColorF | ZufyUIWidgets.h:4232 | RadioButton | RadioButton | 边框 |
| #CCCCCC | 255 | ColorF | ZufyUIWidgets.h:4339 | RadioButton | Draw | 悬停 |
| #D1D1D1 | 255 | ColorF | ZufyUIWidgets.h:4340 | RadioButton | Draw | 边框 |
| #8C8C8C | 255 | ColorF | ZufyUIWidgets.h:4362 | RadioButton | Draw | 标签 |
| #D6E8FB | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:4557 | TabView | 默认值常量 | 选中 |
| #FFFFFF | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:4558 | TabView | 默认值常量 | 选中 |
| #FFFFFF | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:4559 | TabView | 默认值常量 | 选中 |
| #5C5C5C | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:4560 | TabView | 默认值常量 | 文字 |
| #212121 | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:4561 | TabView | 默认值常量 | 文字 |
| #000000 | 13 | Color / DEFAULT-const | ZufyUIWidgets.h:4562 | TabView | 默认值常量 | 文字 |
| #0078D6 | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:4563 | TabView | 默认值常量 | 文字 |
| #000000 | 26 | Color / DEFAULT-const | ZufyUIWidgets.h:4564 | TabView | 默认值常量 | 文字 |
| #000000 | 0 | ColorF | ZufyUIWidgets.h:4584 | TabView | TabView | 控件/滚动条配色 |
| #000000 | 26 | Color | ZufyUIWidgets.h:4751 | TabView | 成员默认值 | 边框 |
| #000000 | 15 | Color | ZufyUIWidgets.h:5196 | TabView | 成员默认值 | 选中 |
| #000000 | 76 | Color | ZufyUIWidgets.h:5197 | TabView | 成员默认值 | 背景 |
| #FFFFFF | 255 | Color | ZufyUIWidgets.h:5198 | TabView | 成员默认值 | 背景 |
| #000000 | 89 | Color | ZufyUIWidgets.h:5223 | TabView | 成员默认值 | 滚动条滑块 |
| #000000 | 140 | Color | ZufyUIWidgets.h:5224 | TabView | 成员默认值 | 滚动条滑块 |
| #0078D6 | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:5242 | ProgressRing | 默认值常量 | DefaultColor |
| #000000 | 26 | Color / DEFAULT-const | ZufyUIWidgets.h:5243 | ProgressRing | 默认值常量 | 轨道 |
| #000000 | 10 | Color / DEFAULT-const | ZufyUIWidgets.h:5364 | NumberBox | 默认值常量 | 背景 |
| #4C4C4C | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:5365 | NumberBox | 默认值常量 | 背景 |
| #0078D6 | 255 | Color / DEFAULT-const | ZufyUIWidgets.h:5366 | NumberBox | 默认值常量 | 背景 |
| #000000 | 46 | Color | ZufyUIWidgets.h:5710 | NumberBox | 成员默认值 | 背景 |
| #CC2121 | 255 | Color | ZufyUIWidgets.h:5711 | NumberBox | 成员默认值 | 背景 |
| #000000 | 15 | Color / DEFAULT-const | ZufyUIWidgets.h:5722 | SplitView | 默认值常量 | 分隔条 |
| #0078D6 | 89 | Color / DEFAULT-const | ZufyUIWidgets.h:5723 | SplitView | 默认值常量 | 悬停 |
| #FFFFFF | 230 | Color | ZufyUIWidgets.h:5790 | SplitView | Draw | 分隔条 |
| #1E1E1E | 255 | FromArgb | ZufyUIWidgets.h:5903 | TextRun | 成员默认值 | 圆环 |
| #1E1E1E | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5920 | TextEdit | 默认值常量 | 文字 |
| #FFFFFF | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5921 | TextEdit | 默认值常量 | 文字 |
| #0078D7 | 120 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5922 | TextEdit | 默认值常量 | 文字 |
| #828282 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5923 | TextEdit | 默认值常量 | 文字 |
| #F5F5F5 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5924 | TextEdit | 默认值常量 | 文字 |
| #0078D7 | 30 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5925 | TextEdit | 默认值常量 | 文字 |
| #141414 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5926 | TextEdit | 默认值常量 | 文字 |
| #D2D2D2 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:5927 | TextEdit | 默认值常量 | 背景 |
| #000000 | 0 | ColorF | ZufyUIWidgets.h:5941 | TextEdit | TextEdit | 控件/滚动条配色 |
| #595959 | 255 | ColorF | ZufyUIWidgets.h:5941 | TextEdit | TextEdit | 控件/滚动条配色 |
| #8C8C8C | 230 | ColorF | ZufyUIWidgets.h:5941 | TextEdit | TextEdit | 控件/滚动条配色 |
| #000000 | 0 | ColorF | ZufyUIWidgets.h:5945 | TextEdit | TextEdit | 控件/滚动条配色 |
| #595959 | 255 | ColorF | ZufyUIWidgets.h:5945 | TextEdit | TextEdit | 控件/滚动条配色 |
| #8C8C8C | 230 | ColorF | ZufyUIWidgets.h:5945 | TextEdit | TextEdit | 控件/滚动条配色 |
| #787878 | 150 | FromArgb | ZufyUIWidgets.h:6282 | TextEdit | Draw | 换行符 |
| #282828 | 255 | FromArgb | ZufyUIWidgets.h:6869 | SettingsList | 默认值常量 | 副文字 |
| #8C8C8C | 255 | FromArgb | ZufyUIWidgets.h:6869 | SettingsList | 默认值常量 | 副文字 |
| #ECECEC | 255 | FromArgb | ZufyUIWidgets.h:6870 | SettingsList | 成员默认值 | 悬停 |
| #F6F6F6 | 255 | FromArgb | ZufyUIWidgets.h:6870 | SettingsList | 成员默认值 | 悬停 |
| #202020 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:6918 | MenuBar | 默认值常量 | 文字 |
| #000000 | 28 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:6919 | MenuBar | 默认值常量 | 文字 |
| #000000 | 46 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:6920 | MenuBar | 默认值常量 | 文字 |
| #000000 | 0 | FromArgb | ZufyUIWidgets.h:7020 | MenuBar | 成员默认值 | 文字 |
| #F5F5F5 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:7030 | StatusBar | 默认值常量 | 背景 |
| #DEDEDE | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:7031 | StatusBar | 默认值常量 | 背景 |
| #464646 | 255 | FromArgb / DEFAULT-const | ZufyUIWidgets.h:7032 | StatusBar | 默认值常量 | 文字 |
| #202020 | 255 | FromArgb | ZufyUIWidgets.h:7094 | ExpanderHeader | 成员默认值 | 标题 |
| #787878 | 255 | FromArgb | ZufyUIWidgets.h:7094 | ExpanderHeader | 成员默认值 | 标题 |
| #FCFCFC | 255 | FromArgb | ZufyUIWidgets.h:7094 | ExpanderHeader | 成员默认值 | 标题 |
| #EEEEEE | 255 | FromArgb | ZufyUIWidgets.h:7095 | ExpanderHeader | 成员默认值 | 悬停背景 |
| #E0E0E0 | 255 | FromArgb | ZufyUIWidgets.h:7140 | Expander | 默认值常量 | 边框 |
| #FCFCFC | 255 | FromArgb | ZufyUIWidgets.h:7140 | Expander | 默认值常量 | 边框 |
| #202020 | 255 | FromArgb | ZufyUIWidgets.h:7141 | Expander | 成员默认值 | 边框 |
| #787878 | 255 | FromArgb | ZufyUIWidgets.h:7141 | Expander | 成员默认值 | 边框 |
| #FFFFFF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:13 | ListView | 默认值常量 | 背景 |
| #000000 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:14 | ListView | 默认值常量 | 文字 |
| #B2D9FF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:15 | ListView | 默认值常量 | 文字 |
| #E6E6E6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:16 | ListView | 默认值常量 | 文字 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:17 | ListView | 默认值常量 | 文字 |
| #999999 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:18 | ListView | 默认值常量 | 选中 |
| #E6E6E6 | 204 | ColorF / DEFAULT-const | ZDataViewer.h:19 | ListView | 默认值常量 | 边框 |
| #808080 | 230 | ColorF / DEFAULT-const | ZDataViewer.h:20 | ListView | 默认值常量 | 边框 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZDataViewer.h:21 | ListView | 默认值常量 | 边框 |
| #338CE6 | 46 | ColorF | ZDataViewer.h:704 | ListView | Draw | 框选填充 |
| #0078D6 | 242 | ColorF | ZDataViewer.h:705 | ListView | Draw | 边框 |
| #0078D6 | 255 | ColorF | ZDataViewer.h:1312 | ListView | 成员默认值 | 指示条 |
| #F7F7F7 | 255 | ColorF | ZDataViewer.h:1342 | ListView | 成员默认值 | 交替行背景 |
| #FFFFFF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1385 | TableView | 默认值常量 | 背景 |
| #EDEDED | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1386 | TableView | 默认值常量 | 背景 |
| #000000 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1387 | TableView | 默认值常量 | 文字 |
| #000000 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1388 | TableView | 默认值常量 | 文字 |
| #B2D9FF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1389 | TableView | 默认值常量 | 文字 |
| #E6E6E6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1390 | TableView | 默认值常量 | 文字 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1391 | TableView | 默认值常量 | 文字 |
| #CCCCCC | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1392 | TableView | 默认值常量 | 选中 |
| #999999 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1393 | TableView | 默认值常量 | 网格线 |
| #E6E6E6 | 204 | ColorF / DEFAULT-const | ZDataViewer.h:1394 | TableView | 默认值常量 | 网格线 |
| #808080 | 230 | ColorF / DEFAULT-const | ZDataViewer.h:1395 | TableView | 默认值常量 | 网格线 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZDataViewer.h:1396 | TableView | 默认值常量 | 边框 |
| #338CE6 | 46 | ColorF | ZDataViewer.h:2189 | TableView | Draw | 框选填充 |
| #0078D6 | 242 | ColorF | ZDataViewer.h:2190 | TableView | Draw | 边框 |
| #0078D6 | 255 | ColorF | ZDataViewer.h:2974 | TableView | 成员默认值 | 指示条 |
| #F7F7F7 | 255 | ColorF | ZDataViewer.h:3021 | TableView | 成员默认值 | 交替行背景 |
| #000000 | 0 | Color | ZDataViewer.h:3071 | TreeNode | 成员默认值 | 选中 |
| #FFFFFF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3092 | TreeView | 默认值常量 | 背景 |
| #EDEDED | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3093 | TreeView | 默认值常量 | 背景 |
| #000000 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3094 | TreeView | 默认值常量 | 文字 |
| #000000 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3095 | TreeView | 默认值常量 | 文字 |
| #B2D9FF | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3096 | TreeView | 默认值常量 | 文字 |
| #E6E6E6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3097 | TreeView | 默认值常量 | 文字 |
| #0078D6 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3098 | TreeView | 默认值常量 | 文字 |
| #999999 | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3099 | TreeView | 默认值常量 | 选中 |
| #CCCCCC | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3100 | TreeView | 默认值常量 | 网格线 |
| #E6E6E6 | 204 | ColorF / DEFAULT-const | ZDataViewer.h:3101 | TreeView | 默认值常量 | 网格线 |
| #808080 | 230 | ColorF / DEFAULT-const | ZDataViewer.h:3102 | TreeView | 默认值常量 | 网格线 |
| #4C4C4C | 255 | ColorF / DEFAULT-const | ZDataViewer.h:3103 | TreeView | 默认值常量 | 网格线 |
| #338CE6 | 46 | ColorF | ZDataViewer.h:4005 | TreeView | Draw | 框选填充 |
| #0078D6 | 242 | ColorF | ZDataViewer.h:4006 | TreeView | Draw | 边框 |
| #F7F7F7 | 255 | ColorF | ZDataViewer.h:4949 | TreeView | 成员默认值 | 选中 |
| #4C8BF5 | 255 | FromArgb | ZufyUICharts.h:28 | ChartBase | — | 调色板 |
| #FF9933 | 255 | FromArgb | ZufyUICharts.h:28 | ChartBase | — | 调色板 |
| #4CAF50 | 255 | FromArgb | ZufyUICharts.h:29 | ChartBase | — | 调色板 |
| #E53935 | 255 | FromArgb | ZufyUICharts.h:29 | ChartBase | — | 调色板 |
| #00BCD4 | 255 | FromArgb | ZufyUICharts.h:30 | ChartBase | — | 调色板 |
| #9C27B0 | 255 | FromArgb | ZufyUICharts.h:30 | ChartBase | — | 调色板 |
| #795548 | 255 | FromArgb | ZufyUICharts.h:31 | ChartBase | — | 颜色常量 |
| #FFC107 | 255 | FromArgb | ZufyUICharts.h:31 | ChartBase | — | 颜色常量 |
| #DC3C3C | 255 | FromArgb | ZufyUICharts.h:76 | ChartBase | 成员默认值 | 标签 |
| #3C8CDC | 255 | FromArgb | ZufyUICharts.h:78 | ChartBase | 成员默认值 | 标签 |
| #000000 | 0 | ColorF | ZufyUICharts.h:243 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #5A5A5A | 255 | FromArgb | ZufyUICharts.h:243 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #8C8C8C | 200 | FromArgb | ZufyUICharts.h:243 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #000000 | 0 | ColorF | ZufyUICharts.h:246 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #5A5A5A | 255 | FromArgb | ZufyUICharts.h:246 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #8C8C8C | 200 | FromArgb | ZufyUICharts.h:246 | ChartBase | InitScrollBars | 控件/滚动条配色 |
| #282828 | 255 | FromArgb | ZufyUICharts.h:379 | ChartBase | 成员默认值 | 成员默认值 |
| #FFFFFF | 255 | FromArgb | ZufyUICharts.h:379 | ChartBase | 成员默认值 | 成员默认值 |
| #000000 | 0 | FromArgb | ZufyUICharts.h:392 | ChartBase | 成员默认值 | 背景 |
| #5A5A5A | 255 | FromArgb | ZufyUICharts.h:392 | ChartBase | 成员默认值 | 背景 |
| #787878 | 60 | FromArgb | ZufyUICharts.h:392 | ChartBase | 成员默认值 | 背景 |
| #A0A0A0 | 255 | FromArgb | ZufyUICharts.h:392 | ChartBase | 成员默认值 | 背景 |
| #DC3C3C | 255 | FromArgb | ZufyUICharts.h:400 | ChartBase | 成员默认值 | 标签 |
| #3C8CDC | 255 | FromArgb | ZufyUICharts.h:401 | ChartBase | 成员默认值 | 标签 |
| #505050 | 255 | FromArgb | ZufyUICharts.h:438 | BarChart | 成员默认值 | 成员默认值 |
| #0078D7 | 28 | FromArgb | ZufyUICharts.h:492 | BarChart | DrawData | 选中 |
| #0078D7 | 28 | FromArgb | ZufyUICharts.h:537 | BarChart | DrawData | 选中 |
| #505050 | 255 | FromArgb | ZufyUICharts.h:575 | BarChart | 成员默认值 | 成员默认值 |
| #000000 | 0 | FromArgb | ZufyUICharts.h:711 | PieChart | 成员默认值 | 标签 |
| #5A5A5A | 255 | FromArgb | ZufyUICharts.h:889 | PieChart | 成员默认值 | 标签 |
| #000000 | 0 | ColorF | ZufyUIImages.h:565 | Image | Image::Bake | 清除位图为全透明 |
| #E5E5E5 | 255 | FromArgb / DEFAULT-const | ZufyUIWindowTool.h:68 | CaptionButton | 默认值常量 | 悬停 |
| #D6D6D6 | 255 | FromArgb / DEFAULT-const | ZufyUIWindowTool.h:69 | CaptionButton | 默认值常量 | 悬停 |
| #C42B1C | 255 | FromArgb / DEFAULT-const | ZufyUIWindowTool.h:70 | CaptionButton | 默认值常量 | 悬停 |
| #B12418 | 255 | FromArgb / DEFAULT-const | ZufyUIWindowTool.h:71 | CaptionButton | 默认值常量 | 悬停 |
| #1E1E1E | 255 | FromArgb / DEFAULT-const | ZufyUIWindowTool.h:72 | CaptionButton | 默认值常量 | 悬停 |
| #FFFFFF | 255 | FromArgb | ZufyUIWindowTool.h:142 | CaptionButton | Draw | 字形 |
| #969696 | 255 | FromArgb | ZufyUIWindowTool.h:144 | CaptionButton | Draw | 字形 |
| #0078D4 | 255 | FromArgb | ZufyUIWindowTool.h:213 | CaptionButton | 成员默认值 | 字形 |
| #DC2828 | 255 | FromArgb | ZufyUIWindowTool.h:233 | TitleBar | 成员默认值 | 徽章 |
| #DC2828 | 255 | FromArgb | ZufyUIWindowTool.h:481 | TitleBar | 成员默认值 | 徽章 |
| #0078D4 | 255 | FromArgb | ZufyUIWindowTool.h:483 | TitleBar | 成员默认值 | 进度 |
| #000000 | 60 | FromArgb | ZufyUIWindowTool.h:484 | TitleBar | 成员默认值 | 背景 |
| #000000 | 0 | FromArgb | ZufyUIWindowTool.h:499 | TitleBar | 成员默认值 | 背景 |
| #000000 | 0 | FromArgb | ZufyUIWindowTool.h:500 | TitleBar | 成员默认值 | 背景 |
| #464646 | 255 | FromArgb | ZufyUIWindowTool.h:501 | TitleBar | 成员默认值 | 背景 |
| #1A1A1A | 255 | FromArgb | ZufyUIWindowTool.h:502 | TitleBar | 成员默认值 | 背景 |
| #DC2828 | 255 | FromArgb | ZufyUIWindowTool.h:907 | TrayIcon | 成员默认值 | 徽章 |
| #DC2828 | 255 | FromArgb | ZufyUIWindowTool.h:1186 | TrayIcon | 成员默认值 | 徽章 |
| #FF0000 | 255 | Color | ZufyUIWindowTool.h:1502 | ColorDialog | 成员默认值 | initial |
| #000000 | 255 | hex | ZufyUIWindowTool.h:2166 | free(全局) | 调试高亮写入 | 高亮色ARGB掩码(A=255) |
| #000000 | 255 | FromArgb | ZufyUI.cpp:33 | ThemeSwatch | Draw | 主题色卡文字：亮底黑字 |
| #FFFFFF | 255 | FromArgb | ZufyUI.cpp:33 | ThemeSwatch | Draw | 主题色卡文字：暗底白字 |
| #FFFFFF | 128 | hex | ZufyUI.cpp:53 | 示例/free | WinMain | 演示默认背景:亚克力+半透明白 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:180 | 示例/free | WinMain | 文字 |
| #A02828 | 255 | FromArgb | ZufyUI.cpp:185 | 示例/free | WinMain | 控件/滚动条配色 |
| #B43C3C | 255 | FromArgb | ZufyUI.cpp:185 | 示例/free | WinMain | 控件/滚动条配色 |
| #C85050 | 255 | FromArgb | ZufyUI.cpp:185 | 示例/free | WinMain | 控件/滚动条配色 |
| #007800 | 255 | FromArgb | ZufyUI.cpp:195 | 示例/free | WinMain(回调) | 文字 |
| #780000 | 255 | FromArgb | ZufyUI.cpp:195 | 示例/free | WinMain(回调) | 文字 |
| #000000 | 120 | FromArgb | ZufyUI.cpp:245 | 示例/free | WinMain | 阴影 |
| #0078D7 | 255 | FromArgb | ZufyUI.cpp:258 | 示例/free | WinMain | 悬停方框 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:287 | 示例/free | WinMain | 文字 |
| #3C3C3C | 255 | FromArgb | ZufyUI.cpp:332 | 示例/free | WinMain | 文字 |
| #C85050 | 255 | FromArgb | ZufyUI.cpp:354 | 示例/free | WinMain | 占位文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:363 | 示例/free | WinMain | 文字 |
| #0064B4 | 255 | FromArgb | ZufyUI.cpp:367 | 示例/free | WinMain | 文字 |
| #0000FF | 255 | Color | ZufyUI.cpp:409 | 示例/free | WinMain(回调) | ColorDialog 自定义色:蓝 |
| #00FF00 | 255 | Color | ZufyUI.cpp:409 | 示例/free | WinMain(回调) | ColorDialog 自定义色:绿 |
| #FF0000 | 255 | Color | ZufyUI.cpp:409 | 示例/free | WinMain(回调) | ColorDialog 自定义色:红 |
| #3399FF | 255 | Color | ZufyUI.cpp:410 | 示例/free | WinMain(回调) | WinMain(回调) |
| #282828 | 255 | FromArgb | ZufyUI.cpp:428 | 示例/free | WinMain | 文字 |
| #0064B4 | 255 | FromArgb | ZufyUI.cpp:431 | 示例/free | WinMain | 文字 |
| #000000 | 40 | FromArgb | ZufyUI.cpp:471 | 示例/free | WinMain | WinMain |
| #000000 | 120 | FromArgb | ZufyUI.cpp:471 | 示例/free | WinMain | WinMain |
| #000000 | 200 | FromArgb | ZufyUI.cpp:471 | 示例/free | WinMain | WinMain |
| #282828 | 255 | FromArgb | ZufyUI.cpp:482 | 示例/free | WinMain | 文字 |
| #FF0000 | 255 | FromArgb | ZufyUI.cpp:538 | 示例/free | WinMain | 文字 |
| #FF0000 | 255 | FromArgb | ZufyUI.cpp:543 | 示例/free | WinMain(回调) | 文字 |
| #008000 | 255 | FromArgb | ZufyUI.cpp:545 | 示例/free | WinMain(回调) | 文字 |
| #0000FF | 255 | FromArgb | ZufyUI.cpp:547 | 示例/free | WinMain(回调) | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:565 | 示例/free | WinMain | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:683 | 示例/free | WinMain | 文字 |
| #C83C3C | 255 | FromArgb | ZufyUI.cpp:698 | 示例/free | WinMain | 文字 |
| #005AA0 | 255 | FromArgb | ZufyUI.cpp:748 | 示例/free | WinMain(回调) | 文字 |
| #C83C3C | 255 | FromArgb | ZufyUI.cpp:750 | 示例/free | WinMain(回调) | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:798 | 示例/free | WinMain | 文字 |
| #007800 | 255 | FromArgb | ZufyUI.cpp:826 | 示例/free | WinMain | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:905 | 示例/free | WinMain | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:985 | 示例/free | WinMain | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1106 | 示例/free | WinMain | 文字 |
| #C82828 | 255 | FromArgb | ZufyUI.cpp:1125 | 示例/free | WinMain | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1135 | 示例/free | WinMain | 图标文字 |
| #0078D4 | 255 | FromArgb | ZufyUI.cpp:1136 | 示例/free | WinMain | 图标文字 |
| #C83C3C | 255 | FromArgb | ZufyUI.cpp:1137 | 示例/free | WinMain | 图标文字 |
| #107C10 | 255 | FromArgb | ZufyUI.cpp:1138 | 示例/free | WinMain | 图标文字 |
| #C89600 | 255 | FromArgb | ZufyUI.cpp:1139 | 示例/free | WinMain | 图标蓝 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1209 | 示例/free | WinMain | 文字 |
| #D9EDFF | 255 | Color | ZufyUI.cpp:1375 | 示例/free | WinMain(回调) | 背景 |
| #1A9940 | 255 | Color | ZufyUI.cpp:1376 | 示例/free | WinMain(回调) | 文字 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1581 | 示例/free | WinMain | 文字 |
| #FFFFFF | 128 | hex | ZufyUI.cpp:1585 | 示例/free | WinMain | 背景着色状态半透明白 |
| #000000 | 0 | hex | ZufyUI.cpp:1610 | 示例/free | WinMain | 背景色预设:全透明 |
| #202020 | 255 | hex | ZufyUI.cpp:1610 | 示例/free | WinMain | 背景色预设:深灰 |
| #FF0000 | 128 | hex | ZufyUI.cpp:1610 | 示例/free | WinMain | 背景色预设:半透明红 |
| #FFFFFF | 255 | hex | ZufyUI.cpp:1610 | 示例/free | WinMain | 背景色预设:不透明白 |
| #FFFFFF | 128 | hex | ZufyUI.cpp:1610 | 示例/free | WinMain | 背景色预设:半透明白 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1654 | 示例/free | WinMain | 文字 |
| #107C10 | 255 | FromArgb | ZufyUI.cpp:1771 | 示例/free | WinMain | 进度 |
| #0066CC | 255 | FromArgb | ZufyUI.cpp:1861 | 示例/free | WinMain | 填充 |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1863 | 示例/free | WinMain | WinMain |
| #C82828 | 255 | FromArgb | ZufyUI.cpp:1864 | 示例/free | WinMain | WinMain |
| #282828 | 255 | FromArgb | ZufyUI.cpp:1865 | 示例/free | WinMain | WinMain |
| #F0F5FA | 255 | FromArgb | ZufyUI.cpp:2004 | 示例/free | WinMain | 背景 |
| #CDE4FA | 255 | FromArgb | ZufyUI.cpp:2018 | 示例/free | WinMain(回调) | 背景 |
| #F0F5FA | 255 | FromArgb | ZufyUI.cpp:2021 | 示例/free | WinMain(回调) | 背景 |
| #F0F5FA | 255 | FromArgb | ZufyUI.cpp:2042 | 示例/free | WinMain(回调) | 背景 |

## 默认值常量清单

| 文件:行 | 类 | 常量 | 值(RGBA) |
|---|---|---|---|
| ZufyUI.h:2319 | Card | DefaultBgColor | #FFFFFF a=255 |
| ZufyUI.h:2320 | Card | DefaultBorderColor | #C8C8C8 a=255 |
| ZufyUI.h:3151 | Window | DefaultBackgroundColor | #000000 a=0 |
| ZufyUIWidgets.h:202 | Label | DefaultTextColor | #000000 a=255 |
| ZufyUIWidgets.h:208 | Label | DefaultDisabledColor | #969696 a=255 |
| ZufyUIWidgets.h:668 | Button | DefaultNormalColor | #0078D4 a=255 |
| ZufyUIWidgets.h:669 | Button | DefaultHoverColor | #0069BE a=255 |
| ZufyUIWidgets.h:670 | Button | DefaultPressedColor | #005AAA a=255 |
| ZufyUIWidgets.h:671 | Button | DefaultTextColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:678 | Button | DefaultDisabledColor | #D2D2D2 a=255 |
| ZufyUIWidgets.h:900 | TextBox | DefaultBgColor | #FAFAFA a=255 |
| ZufyUIWidgets.h:901 | TextBox | DefaultBorderColor | #CCCCCC a=255 |
| ZufyUIWidgets.h:902 | TextBox | DefaultTextColor | #000000 a=255 |
| ZufyUIWidgets.h:903 | TextBox | DefaultSelectionColor | #B2D9FF a=128 |
| ZufyUIWidgets.h:904 | TextBox | DefaultHoverBgColor | #EDEDED a=255 |
| ZufyUIWidgets.h:905 | TextBox | DefaultHoverBorderColor | #666666 a=255 |
| ZufyUIWidgets.h:1726 | ComboBox | DefaultNormalBgColor | #F2F2F2 a=255 |
| ZufyUIWidgets.h:1727 | ComboBox | DefaultHoverBgColor | #E0E0E0 a=255 |
| ZufyUIWidgets.h:1728 | ComboBox | DefaultHoverItemColor | #D9D9D9 a=255 |
| ZufyUIWidgets.h:1729 | ComboBox | DefaultBorderColor | #999999 a=255 |
| ZufyUIWidgets.h:1730 | ComboBox | DefaultIndicatorColor | #0078D6 a=255 |
| ZufyUIWidgets.h:2706 | ToggleSwitch | DefaultOnColor | #0078D4 a=255 |
| ZufyUIWidgets.h:2707 | ToggleSwitch | DefaultOffColor | #C8C8C8 a=255 |
| ZufyUIWidgets.h:2708 | ToggleSwitch | DefaultKnobColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:2713 | ToggleSwitch | DefaultDisabledColor | #E0E0E0 a=255 |
| ZufyUIWidgets.h:2875 | ScrollBar | DefaultThumbColor | #808080 a=230 |
| ZufyUIWidgets.h:2876 | ScrollBar | DefaultHoverThumbColor | #4C4C4C a=255 |
| ZufyUIWidgets.h:2877 | ScrollBar | DefaultTrackColor | #000000 a=15 |
| ZufyUIWidgets.h:3070 | ScrollViewer | DefaultTrackColor | #E6E6E6 a=204 |
| ZufyUIWidgets.h:3071 | ScrollViewer | DefaultThumbColor | #808080 a=230 |
| ZufyUIWidgets.h:3072 | ScrollViewer | DefaultHoverThumbColor | #4C4C4C a=255 |
| ZufyUIWidgets.h:3595 | ProgressBar | DefaultTrackColor | #D9D9D9 a=255 |
| ZufyUIWidgets.h:3596 | ProgressBar | DefaultFillColor | #0078D6 a=255 |
| ZufyUIWidgets.h:3597 | ProgressBar | DefaultBorderColor | #999999 a=255 |
| ZufyUIWidgets.h:3776 | Slider | DefaultTrackColor | #D9D9D9 a=255 |
| ZufyUIWidgets.h:3777 | Slider | DefaultFillColor | #0078D6 a=255 |
| ZufyUIWidgets.h:3778 | Slider | DefaultThumbColor | #F2F2F2 a=255 |
| ZufyUIWidgets.h:3779 | Slider | DefaultHoverThumbColor | #808080 a=255 |
| ZufyUIWidgets.h:3780 | Slider | DefaultThumbBorderColor | #4C4C4C a=255 |
| ZufyUIWidgets.h:3781 | Slider | DefaultHoverThumbBorderColor | #1A1A1A a=255 |
| ZufyUIWidgets.h:3982 | CheckBox | DefaultBoxColor | #0078D6 a=255 |
| ZufyUIWidgets.h:3983 | CheckBox | DefaultBorderColor | #9E9E9E a=255 |
| ZufyUIWidgets.h:3984 | CheckBox | DefaultCheckColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:4219 | RadioButton | DefaultAccentColor | #0078D6 a=255 |
| ZufyUIWidgets.h:4220 | RadioButton | DefaultBorderColor | #9E9E9E a=255 |
| ZufyUIWidgets.h:4221 | RadioButton | DefaultLabelColor | #262626 a=255 |
| ZufyUIWidgets.h:4557 | TabView | DefaultSelectedTabColor | #D6E8FB a=255 |
| ZufyUIWidgets.h:4558 | TabView | DefaultBackgroundColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:4559 | TabView | DefaultContentColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:4560 | TabView | DefaultTextColor | #5C5C5C a=255 |
| ZufyUIWidgets.h:4561 | TabView | DefaultSelectedTextColor | #212121 a=255 |
| ZufyUIWidgets.h:4562 | TabView | DefaultHoverColor | #000000 a=13 |
| ZufyUIWidgets.h:4563 | TabView | DefaultIndicatorColor | #0078D6 a=255 |
| ZufyUIWidgets.h:4564 | TabView | DefaultBorderColor | #000000 a=26 |
| ZufyUIWidgets.h:5242 | ProgressRing | DefaultColor | #0078D6 a=255 |
| ZufyUIWidgets.h:5243 | ProgressRing | DefaultTrackColor | #000000 a=26 |
| ZufyUIWidgets.h:5364 | NumberBox | DefaultSpinBgColor | #000000 a=10 |
| ZufyUIWidgets.h:5365 | NumberBox | DefaultArrowColor | #4C4C4C a=255 |
| ZufyUIWidgets.h:5366 | NumberBox | DefaultAccentColor | #0078D6 a=255 |
| ZufyUIWidgets.h:5722 | SplitView | DefaultSplitterColor | #000000 a=15 |
| ZufyUIWidgets.h:5723 | SplitView | DefaultHoverColor | #0078D6 a=89 |
| ZufyUIWidgets.h:5920 | TextEdit | DefaultTextColor | #1E1E1E a=255 |
| ZufyUIWidgets.h:5921 | TextEdit | DefaultBgColor | #FFFFFF a=255 |
| ZufyUIWidgets.h:5922 | TextEdit | DefaultSelectionColor | #0078D7 a=120 |
| ZufyUIWidgets.h:5923 | TextEdit | DefaultGutterTextColor | #828282 a=255 |
| ZufyUIWidgets.h:5924 | TextEdit | DefaultGutterBgColor | #F5F5F5 a=255 |
| ZufyUIWidgets.h:5925 | TextEdit | DefaultCurrentLineColor | #0078D7 a=30 |
| ZufyUIWidgets.h:5926 | TextEdit | DefaultCursorColor | #141414 a=255 |
| ZufyUIWidgets.h:5927 | TextEdit | DefaultBorderColor | #D2D2D2 a=255 |
| ZufyUIWidgets.h:6918 | MenuBar | DefaultTextColor | #202020 a=255 |
| ZufyUIWidgets.h:6919 | MenuBar | DefaultHoverBg | #000000 a=28 |
| ZufyUIWidgets.h:6920 | MenuBar | DefaultOpenBg | #000000 a=46 |
| ZufyUIWidgets.h:7030 | StatusBar | DefaultBg | #F5F5F5 a=255 |
| ZufyUIWidgets.h:7031 | StatusBar | DefaultBorder | #DEDEDE a=255 |
| ZufyUIWidgets.h:7032 | StatusBar | DefaultTextColor | #464646 a=255 |
| ZDataViewer.h:13 | ListView | DefaultBackgroundColor | #FFFFFF a=255 |
| ZDataViewer.h:14 | ListView | DefaultTextColor | #000000 a=255 |
| ZDataViewer.h:15 | ListView | DefaultSelectedColor | #B2D9FF a=255 |
| ZDataViewer.h:16 | ListView | DefaultHoverColor | #E6E6E6 a=255 |
| ZDataViewer.h:17 | ListView | DefaultIndicatorColor | #0078D6 a=255 |
| ZDataViewer.h:18 | ListView | DefaultBorderColor | #999999 a=255 |
| ZDataViewer.h:19 | ListView | DefaultScrollTrackColor | #E6E6E6 a=204 |
| ZDataViewer.h:20 | ListView | DefaultScrollThumbColor | #808080 a=230 |
| ZDataViewer.h:21 | ListView | DefaultScrollHoverThumbColor | #4C4C4C a=255 |
| ZDataViewer.h:1385 | TableView | DefaultBackgroundColor | #FFFFFF a=255 |
| ZDataViewer.h:1386 | TableView | DefaultHeaderBackgroundColor | #EDEDED a=255 |
| ZDataViewer.h:1387 | TableView | DefaultTextColor | #000000 a=255 |
| ZDataViewer.h:1388 | TableView | DefaultHeaderTextColor | #000000 a=255 |
| ZDataViewer.h:1389 | TableView | DefaultSelectedColor | #B2D9FF a=255 |
| ZDataViewer.h:1390 | TableView | DefaultHoverColor | #E6E6E6 a=255 |
| ZDataViewer.h:1391 | TableView | DefaultIndicatorColor | #0078D6 a=255 |
| ZDataViewer.h:1392 | TableView | DefaultGridLineColor | #CCCCCC a=255 |
| ZDataViewer.h:1393 | TableView | DefaultBorderColor | #999999 a=255 |
| ZDataViewer.h:1394 | TableView | DefaultScrollTrackColor | #E6E6E6 a=204 |
| ZDataViewer.h:1395 | TableView | DefaultScrollThumbColor | #808080 a=230 |
| ZDataViewer.h:1396 | TableView | DefaultScrollHoverThumbColor | #4C4C4C a=255 |
| ZDataViewer.h:3092 | TreeView | DefaultBackgroundColor | #FFFFFF a=255 |
| ZDataViewer.h:3093 | TreeView | DefaultHeaderBackgroundColor | #EDEDED a=255 |
| ZDataViewer.h:3094 | TreeView | DefaultTextColor | #000000 a=255 |
| ZDataViewer.h:3095 | TreeView | DefaultHeaderTextColor | #000000 a=255 |
| ZDataViewer.h:3096 | TreeView | DefaultSelectedColor | #B2D9FF a=255 |
| ZDataViewer.h:3097 | TreeView | DefaultHoverColor | #E6E6E6 a=255 |
| ZDataViewer.h:3098 | TreeView | DefaultIndicatorColor | #0078D6 a=255 |
| ZDataViewer.h:3099 | TreeView | DefaultBorderColor | #999999 a=255 |
| ZDataViewer.h:3100 | TreeView | DefaultGridLineColor | #CCCCCC a=255 |
| ZDataViewer.h:3101 | TreeView | DefaultScrollTrackColor | #E6E6E6 a=204 |
| ZDataViewer.h:3102 | TreeView | DefaultScrollThumbColor | #808080 a=230 |
| ZDataViewer.h:3103 | TreeView | DefaultScrollHoverThumbColor | #4C4C4C a=255 |
| ZufyUIWindowTool.h:68 | CaptionButton | DefaultHoverColor | #E5E5E5 a=255 |
| ZufyUIWindowTool.h:69 | CaptionButton | DefaultPressedColor | #D6D6D6 a=255 |
| ZufyUIWindowTool.h:70 | CaptionButton | DefaultCloseHoverColor | #C42B1C a=255 |
| ZufyUIWindowTool.h:71 | CaptionButton | DefaultClosePressedColor | #B12418 a=255 |
| ZufyUIWindowTool.h:72 | CaptionButton | DefaultGlyphColor | #1E1E1E a=255 |

## 同一颜色高频出现

| 颜色(RGB) | 出现次数 | Alpha 变体(次数) |
|---|---:|---|
| #000000 | 76 | 255(25), 0(24), 120(3), 15(3), 26(3), 60(2), 140(2), 76(2), 8(2), 46(2), 160(1), 31(1), 13(1), 89(1), 10(1), 28(1), 40(1), 200(1) |
| #FFFFFF | 41 | 255(33), 128(3), 0(1), 223(1), 51(1), 76(1), 230(1) |
| #0078D6 | 22 | 255(16), 242(3), 230(1), 38(1), 89(1) |
| #282828 | 20 | 255(20) |
| #E6E6E6 | 12 | 204(6), 255(5), 200(1) |
| #4C4C4C | 10 | 255(10) |
| #999999 | 10 | 255(10) |
| #0078D4 | 8 | 255(8) |
| #202020 | 8 | 255(5), 102(2), 204(1) |
| #808080 | 8 | 230(7), 255(1) |
| #8C8C8C | 8 | 255(4), 230(2), 200(2) |
| #CCCCCC | 8 | 255(8) |
| #FFFF00 | 8 | 255(8) |
| #5A5A5A | 6 | 255(6) |
| #787878 | 6 | 255(4), 150(1), 60(1) |
| #0078D7 | 5 | 28(2), 120(1), 30(1), 255(1) |
| #262626 | 5 | 255(4), 217(1) |
| #FF0000 | 5 | 255(4), 128(1) |
| #B2D9FF | 4 | 255(3), 128(1) |
| #DC2828 | 4 | 255(4) |
| #0064B4 | 3 | 255(3) |
| #107C10 | 3 | 255(3) |
| #1A1A1A | 3 | 255(3) |
| #1E1E1E | 3 | 255(3) |
| #338CE6 | 3 | 46(3) |
| #3C3C3C | 3 | 255(2), 200(1) |
| #464646 | 3 | 255(3) |
| #4CC2FF | 3 | 255(3) |
| #505050 | 3 | 255(3) |
| #828282 | 3 | 230(2), 255(1) |
| #C83C3C | 3 | 255(3) |
| #C8C8C8 | 3 | 255(3) |
| #D2D2D2 | 3 | 255(3) |
| #D9D9D9 | 3 | 255(3) |
| #E0E0E0 | 3 | 255(3) |
| #EDEDED | 3 | 255(3) |
| #F0F5FA | 3 | 255(3) |
| #F2F2F2 | 3 | 255(3) |
| #F7F7F7 | 3 | 255(3) |
| #FCFCFC | 3 | 255(3) |
| #0000FF | 2 | 255(2) |
| #007800 | 2 | 255(2) |
| #141414 | 2 | 255(2) |
| #3C8CDC | 2 | 255(2) |
| #595959 | 2 | 255(2) |
| #969696 | 2 | 255(2) |
| #9E9E9E | 2 | 255(2) |
| #A0A0A0 | 2 | 255(2) |
| #B2B2B2 | 2 | 255(2) |
| #C42B1C | 2 | 255(2) |
| #C82828 | 2 | 255(2) |
| #C85050 | 2 | 255(2) |
| #CC2121 | 2 | 255(2) |
| #D1D1D1 | 2 | 255(2) |
| #D6E8FB | 2 | 255(2) |
| #DC3C3C | 2 | 255(2) |
| #EEEEEE | 2 | 255(2) |
| #F3F3F3 | 2 | 255(2) |
| #F5F5F5 | 2 | 255(2) |
| #000005 | 1 | 107(1) |
| #005AA0 | 1 | 255(1) |
| #005AAA | 1 | 255(1) |
| #0066CC | 1 | 255(1) |
| #0069BE | 1 | 255(1) |
| #008000 | 1 | 255(1) |
| #00BCD4 | 1 | 255(1) |
| #00FF00 | 1 | 255(1) |
| #1A8CE4 | 1 | 255(1) |
| #1A9940 | 1 | 255(1) |
| #1C1C1C | 1 | 255(1) |
| #212121 | 1 | 255(1) |
| #264F78 | 1 | 255(1) |
| #28A0DC | 1 | 255(1) |
| #2B2B2B | 1 | 255(1) |
| #2ECC71 | 1 | 255(1) |
| #303030 | 1 | 255(1) |
| #323232 | 1 | 255(1) |
| #3399FF | 1 | 255(1) |
| #3A3A3A | 1 | 255(1) |
| #4A4A4A | 1 | 255(1) |
| #4C8BF5 | 1 | 255(1) |
| #4CAF50 | 1 | 255(1) |
| #5C5C5C | 1 | 255(1) |
| #606060 | 1 | 255(1) |
| #666666 | 1 | 255(1) |
| #6CCB5F | 1 | 255(1) |
| #6ECDFF | 1 | 255(1) |
| #737373 | 1 | 255(1) |
| #780000 | 1 | 255(1) |
| #795548 | 1 | 255(1) |
| #989898 | 1 | 255(1) |
| #9C27B0 | 1 | 255(1) |
| #A02828 | 1 | 255(1) |
| #A6A6A6 | 1 | 255(1) |
| #ADADAD | 1 | 255(1) |
| #B12418 | 1 | 255(1) |
| #B3D7F3 | 1 | 255(1) |
| #B43C3C | 1 | 255(1) |
| #B4B4B4 | 1 | 255(1) |
| #BDBDBD | 1 | 255(1) |
| #C89600 | 1 | 255(1) |
| #CDE4FA | 1 | 255(1) |
| #D6D6D6 | 1 | 255(1) |
| #D9EDFF | 1 | 255(1) |
| #DB3333 | 1 | 255(1) |
| #DCDC00 | 1 | 255(1) |
| #DEDEDE | 1 | 255(1) |
| #E09814 | 1 | 255(1) |
| #E2E2E2 | 1 | 255(1) |
| #E4E4E4 | 1 | 255(1) |
| #E53935 | 1 | 255(1) |
| #E5E5E5 | 1 | 255(1) |
| #EBEBEB | 1 | 255(1) |
| #ECECEC | 1 | 255(1) |
| #F0F0F0 | 1 | 255(1) |
| #F6F6F6 | 1 | 255(1) |
| #FAFAFA | 1 | 255(1) |
| #FF6347 | 1 | 255(1) |
| #FF9933 | 1 | 255(1) |
| #FFB900 | 1 | 255(1) |
| #FFC107 | 1 | 255(1) |
| #FFF7F7 | 1 | 255(1) |
