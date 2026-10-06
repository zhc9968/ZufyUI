# 配图（极简）

**原则**：尽量不配图；能被文字讲清的（流程 / 结构 / 分层 / 映射）一律用文字，不画图。
只保留“必须看真实界面”的截图。

## 需要的图片（就这几张，够用）

| 图片（放 `docs/images/`） | 位置 | 产出方式 |
|---|---|---|
| `quickstart.png` | README「快速开始」第 1 步 | 编译运行 `screenshots.cpp`（无参）截图 |
| `backdrop-mica.png` | README 第 2 步 | `screenshots.exe --mica` 截图 |
| `custom-titlebar.png` | README 第 3 步 | `screenshots.exe --titlebar` 截图 |
| `demo-main.png` | README「更多控件总览」 | 运行 demo 主示例程序首屏截图 |

> 这 4 张 README 里已有 `![]()` 占位（旧图），**同名覆盖**即可，路径不用改。

可选（**非必需**，只是“长啥样”的展示；不加也不影响）：
- `textedit.png` ← demo「多行文本」页
- `charts-pie.png` ← demo「饼图」页（一张代表三种图）

**编译**（把 `C:\project\ZufyUI` 加进包含路径；系统库库内已自动链接）：
```bat
cl /std:c++17 /EHsc /O2 /DUNICODE /D_UNICODE /SUBSYSTEM:WINDOWS screenshots.cpp user32.lib gdi32.lib
```

---

## 文字版“示意图”（想加就直接贴进对应章节，不用画图）

### 离屏缓存流水线（元素基类 · 缓存）
```
更新阶段(UI 线程):  UpdateAnimation ─▶ RefreshChildren ─▶ RequestRepaint(标脏 → pendingRepaint)
缓存预通道(主帧前):  DrawDirtyCaches ─▶ EnsureCache(尺寸变? 重建 bitmap) ─▶ elem->Draw 画进 cacheBitmap
合成(主帧):          RenderContent ─▶ blit 各元素 cacheBitmap
```
要点：只有“脏元素”重建位图，其余直接复用；`UseCache()==false` 的元素每帧直接 Draw，不走缓存。

### 数据视图两套坐标（数据视图）
```
源数组 items_[src]（源行号，稳定）
        │  隐藏 / 筛选 / 排序（SetItemHidden / SetFilter / SortView）
        ▼
可见序 view_[vi]（屏幕第几个）
```
互换：`SourceOfVisible(vi) → src`、`VisibleOfSource(src) → vi`。旧 API/信号用**源行号**；虚拟化/隐藏/筛选相关用**可见序**。

### PageHost 过渡（布局 · PageHost）
```
视口   [ 源页 from 向左平移 ] [ 目标页 to 向右平移 ]
t: 0 ─────────────────────────────────────────────▶ 1
```
过渡期间 `GetChildren()` 同时返回“源页 + 目标页”；切换是**异步**的（动画结束才定）；隐藏页缓存延迟释放。

### 背景叠层（窗口 · 外观）
```
[ 系统背景：空 / Acrylic / Mica ]  ← 最底
        ↑ 上面叠 DefaultBackdropColor（RGB + Alpha）
        ↑ 上面是 根内容（控件树）
```

### 自定义标题栏（窗口 · 标题栏）
```
Window 客户区
┌───────────────────────────────┐
│ 自定义标题栏（位置 (0,0)，DrawBeforeLayout，不参与布局）│
├───────────────────────────────┤
│ 根内容（被下移 customTitleBarHeight）                  │
└───────────────────────────────┘
```

### 图表绘图区（图表 · ChartBase）
```
┌── arrangedRect ───────────────────────────────┐
│ 图例 legendRect                                │
│ 值轴刻度 │ plot_：数据 / 网格 / 参考线        │ vBar
│ (axisLeft)│                                    │
│           ├────────────────────────────────────┤
│           │ 类别标签                            │
└───────────┴────────────────────────────────────┘
              hBar
```
吸附轴：plot 滚出视口时，值轴/类别轴贴在视口边缘。面积填充基线 = `ValueY(clamp(0, axisMin, axisMax))`。

### 布局拉伸权值（布局系统）
```
ColumnBox：先量各子元素 base 高，剩余 = 总高 − Σbase，再按子元素权值分剩余。
SetFillHeight(true) 等价“该轴垂直权值 1”；SetHeight(x) 是固定 base，不等于权重。
```

### 调试通道（无障碍与调试通道）
```
调试器 ── WM_COPYDATA(dwData=命令号 1..22) ──▶ 目标窗口（需 SetDebugEnabled(true)）
调试器 ◀── WM_COPYDATA(dwData=0x5A554631) ── 目标的隐藏 reply sink 窗口
```
发送侧要带超时（`SendMessageTimeout`，不加 `SMTO_BLOCK`），否则目标忙时会卡死调试器。库不读写文件，只返回当前帧信息。
