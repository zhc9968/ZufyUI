# AGENTS / 开发原则

## 协作流程规则（必须遵守）

- **提交/推送前必须先经用户同意**：不要自动 `git commit` / `git push`（含 Gitee/GitHub/gh-pages），等到用户明确说“提交/推送”再执行。
- **不要自动更新文档**：API 文档、README、更新日志等，只有用户明确指定时才更新；不要顺手自动改。
- 每次提交信息要如实描述本次改动。

## Bug 处理原则（最高优先级）

遇到任何 bug（无论轻重缓急），**先定位根源，再从根源修复**：

1. 绝不允许用“新机制”让 bug 无法触发，来掩盖问题（那是掩盖，不是修复）。
2. 如果根源确实在外部（Windows API / 系统 / 第三方库），才考虑绕过它，并在文档中明确标注“这是对外部问题的规避”。
3. 如果根源在本项目内部代码，必须**找到它并从根源修复**。
4. 如果找不到根源：**如实说明“找不到，未定位”**，交由用户（或用户的其他分析）定位；找到后再修复。**绝不猜测、绝不用“最可疑”下结论、绝不靠引入机制来掩盖。**

## 依赖与约束（避免埋雷）

- `UIElement::GetChildren()` 返回的是**元素内部缓冲的引用**，其有效期为“该元素的子列表或可视状态发生变化 / 再次调用该元素的 `GetChildren()` 之前”。遍历期间**不得对同一个元素再次调用 `GetChildren()`**（容器递归自己子元素时用的是各自独立的缓冲，安全）。
- 元素内部用**窗口 id**（不是裸指针）记录所属窗口；窗口销毁后 `GetWindow()` 返回 `nullptr`。
- `UIElement` / `Window` 继承 `ConnectionOwner`，`Connect(signal, slot)` 返回 `Connection`；连接登记在接收者 `ConnectionGroup`，随接收者析构自动断开。成员函数指针用自由 `Connect(receiver, signal, &Class::Method)` 或 `ConnectWeak`，**不要**写 `elem->Connect(sig, &Class::Method)`。
- 所有与窗口相关的全局信号（`UIZSignals::*`）都携带 `Window*`；订阅者必须用 `GetWindow()` 过滤，禁止跨窗口处理。
- 数据视图（List/Table/Tree）的按项/按行元数据要么绑定到项目对象、要么在排序/增删时重映射，禁止用会失效的下标或裸指针悬挂。

## 链式构造 / API 约定（保持调用侧一致，别让用户猜）

- **`.with(&T::Method, args...)`**：只用于**无默认参数、无重载、非初始化列表**的成员函数。
  - 带默认参数的成员函数，成员指针含全部形参 → `.with(&X::F, a)` 报“参数太少”。**不要**再往库里加这种函数；新函数若需要可选行为，拆成**两个无默认参数的重载**（如 `SetCheckable(bool)` + `SetCheckableShallow(bool)`）。
  - 需要显式给非默认值时用拆分后的具名重载（如 `...Recursive` / `...Selected` / `...Radio` / `...Full` / `...Impl`）。
- **`.apply([](T& x){ ... })`**：逃生舱 —— 仅用于初始化列表、重载歧义、或多步条件逻辑。不要当主写法。
- **`->SetXxx(...)`**：在 DSL 链中尽量不用；需要时用 `.with`；确实不行才 `->`。
- **DSL 短工厂统一 `Z` 前缀**：`ZLbl` / `ZBtn` / `ZSlider` / `ZCard` / `ZPage` / `ZMenu` …；因此同时 `using namespace ZufyUI; using namespace ZufyUI::dsl;` 也不会再和类名冲突。
- **主题钩子**：`ThemeManager::RegisterHook/RegisterSubtreeApplier` 返回 RAII `HookHandle`（析构自动断开）；想长期有效就**持有**它；库内部固定钩子用 `RegisterHookPermanent`。
- **整页滚动**：`page->SetScrollableContent(content)` 一行搞定（别再手写 `GridLayout + ScrollViewer` 样板）。
- 更完整的易错点集中在 **`docs/pitfalls.md`**（改库前先扫一遍）。

## 双语文档规则（API.md / API.en.md）

- 两版**同一章节的代码块必须逐行对齐**：**行数相同、顺序相同**（函数签名集合也自然一致）。叙述、注释、示例的**文字**可以各自不同，但**代码块的行数与顺序必须一致**——**行数不匹配 = 一定有一边漏了**。
- 改动任一版后运行校验（退出码 0 = 一致）：
  ```powershell
  python tools/check_api_parity.py
  ```
  脚本校验“每章代码块的方法名集合 + 非空行数”；任一项不一致即报错。
- 文档（`.md` / `.tex`）保存为 **UTF-8 无 BOM**；C++ 源文件（`.h`/`.cpp`）保存为 **UTF-8 带 BOM**（中文宽字面量需要）。需要无 BOM / 纯 ASCII 副本时，用 `tools/export_ascii.py` 生成 `dist/ascii`（不提交）。

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```
- 脚本通过 `vswhere` 自动探测 Visual Studio / MSBuild，**不写死安装路径**；构建前会杀掉占用进程。
- 可选：`-Configuration Debug`、`-Platform x86`、`-SkipDebugger`、`-SkipTests`、`-VsRoot <path>`。
- 纯 ASCII 分发副本：`python tools/export_ascii.py`（输出到 `dist/ascii`，见生成目录的 `README.md`）。
- 文档站点：`python tools/build_docs.py`（ZufyUI 包装；通用转换器在 `../md2html`，**不要把库专用逻辑写进它**）。
