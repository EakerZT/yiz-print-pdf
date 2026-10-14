# yiz-print-pdf

**Windows 原生 PDF 打印工具与 SDK / Native PDF printing tool and SDK for Windows**

## 简介 / Introduction

**中文**

yiz-print-pdf 是基于 PDFium 的原生 C++ PDF 打印工具与 SDK，同时提供独立命令行 EXE 和可供 C/C++、C# 集成的 DLL，支持 x86 与 x64。

- **独立命令行工具**：EXE 静态链接 PDFium 和 C/C++ 运行库，无需附带 SDK DLL、.NET、Java 或 WebView2。
- **文件与内存输入**：SDK 支持直接打印内存中的 PDF，C# 可传入 `byte[]`，无需生成临时 PDF 文件。
- **替代文字绘制**：通过 Windows GDI 输出受支持的内嵌 TrueType 字形，用于改善部分低分辨率打印机的文字效果；不支持的情况保留 PDFium 原始绘制路径。
- **布局控制**：支持百分比缩放、适应页面、仅缩小、居中、偏移及纸张设置。
- **集成资料齐全**：提供 C ABI 头文件、MSVC 导入库、C# 封装和 C++ / C# 示例。

程序仍依赖 Windows 系统组件及已安装的打印机驱动。Windows 7 SP1 是构建兼容性目标，尚待实机验证；实际打印效果取决于 PDF、字体和驱动。成功返回表示提交至 Windows 打印队列，不代表纸张已输出。SDK 无需临时 PDF，但 Windows 打印后台处理仍可能写入 spool 文件。

**English**

yiz-print-pdf is a native C++ PDF printing tool and SDK built on PDFium. It provides a standalone command-line EXE and a DLL for C/C++ and C# integration, with separate x86 and x64 builds.

- **Standalone CLI:** PDFium and the C/C++ runtime are statically linked into the EXE. No accompanying SDK DLL, .NET, Java, or WebView2 is required.
- **File and memory input:** The SDK accepts PDF buffers directly. C# applications can pass a `byte[]` without creating a temporary PDF file.
- **Alternative text rendering:** Supported embedded TrueType glyphs are rendered through Windows GDI to improve text output on some low-resolution printers. Unsupported cases retain PDFium's original rendering path.
- **Layout controls:** Percentage scaling, fit, shrink-only, centering, offsets, and paper settings.
- **Integration resources:** A C ABI header, MSVC import library, C# wrapper, and C++ / C# examples are included.

Windows system components and an installed printer driver are still required. Windows 7 SP1 is the compatibility target but has not yet been tested on an actual Windows 7 machine. Output quality depends on the PDF, fonts, and driver. A successful result means submission to the Windows print queue, not physical completion. In-memory input avoids application-created temporary PDFs; Windows may still create spool files.

## 下载与使用 / Downloads and usage

Download the matching SDK from [GitHub Releases](https://github.com/EakerZT/yiz-print-pdf/releases). Choose a DLL that matches your application's process architecture. The EXE runs independently of the SDK DLL.

从 [GitHub Releases](https://github.com/EakerZT/yiz-print-pdf/releases) 下载对应架构的 SDK。DLL 必须与调用进程位数一致；EXE 可独立使用。

```powershell
# Run from the extracted SDK directory / 在解压后的 SDK 目录运行
yiz-print-pdf.exe --list-printers
yiz-print-pdf.exe --file "ticket.pdf" --printer "Printer Name" --scale fit
```

Build both architectures with `./build.ps1`, or select one with `./build.ps1 -Architecture x64`. Build outputs are placed in `dist/win-x86` and `dist/win-x64`. The following sections contain the detailed Chinese reference; the public header and C# wrapper also include English API comments.

运行 `./build.ps1` 构建两种架构，或使用 `./build.ps1 -Architecture x64` 单独构建。产物位于 `dist/win-x86` 和 `dist/win-x64`。以下为中文详细参考，公共头文件和 C# 封装也提供英文接口注释。

## 快速验证

在 yiz-print-pdf 仓库根目录运行 PowerShell：

```powershell
# 获取系统打印机名称（不使用 lc-printer 的别名）
.\dist\win-x86\yiz-print-pdf.exe --list-printers

# 替代文字输出，默认原尺寸 100%，默认使用驱动配置的纸张尺寸
.\dist\win-x86\yiz-print-pdf.exe --file "C:\路径\票据.pdf" --printer "XP-80C" --text-mode alternate

# 使用同一份 PDF 对照 PDFium 原始绘制方式
.\dist\win-x86\yiz-print-pdf.exe --file "C:\路径\票据.pdf" --printer "XP-80C" --text-mode original
```

每次调用创建一个打印作业；多页 PDF 在同一作业中顺序输出。切纸行为由驱动决定，不额外发送切刀命令。不会自动重发任务。

没有指定 DPI，使用驱动返回的实际 DPI。两种模式采用相同打印参数。日志写入标准错误，最终 UTF-8 JSON 写入标准输出。`submitted` 代表成功提交至 Windows 打印队列，不代表纸张已经输出。失败会尝试 AbortDoc，但之前已经输出的纸张不能回滚。

## 参数

| 参数 | 默认值／作用 |
| --- | --- |
| `--file <路径>` | PDF 文件，支持中文路径；当前不处理密码 PDF，文件不超过 512 MiB |
| `--printer <名称>` | Windows 系统打印机全名，必填；不会自动选择默认打印机 |
| `--text-mode alternate\|original` | 默认 `alternate` |
| `--scale <百分比>\|fit\|shrink` | 默认 `100`，百分比范围 10–200；`fit` 等比例适应可打印区域，允许放大；`shrink` 仅缩小。只能指定一次，不改变纸张尺寸 |
| `--center` | 在驱动可打印区域内水平、垂直居中，默认关闭 |
| `--offset-x <mm>` | 额外水平偏移，正数向右、负数向左；默认 0，范围 -3000–3000 mm |
| `--offset-y <mm>` | 额外垂直偏移，正数向下、负数向上；默认 0，范围 -3000–3000 mm |
| `--paper pdf\|driver` | 默认 `driver`，使用驱动默认纸张（含页长）；`pdf` 按 PDF 页面尺寸设置自定义纸张 |
| `--width <mm> --height <mm>` | 固定纸张，须同时指定；范围大于 0、不超过 3000 mm；覆盖 `--paper` |
| `--require-alternate` | 严格验证：文字调用出现回退，或某页没有替代字形时中止；纯图片页也会因此中止 |
| `--inspect` | 只读取并输出 PDF 各页尺寸，无需打印机，不打印 |
| `--list-printers` | 列出打印机，不打印 |
| `--emf <路径>` | 用指定打印机 DC 将第一页记录为诊断 EMF，不创建打印作业、不出纸 |
| `--licenses` | 显示内嵌的第三方许可证，无需附带许可证文件才能运行 |
| `--help` | 使用帮助 |

纸型通过 DEVMODE 交给驱动确认，最终尺寸以驱动接受的设置为准。日志输出 PDF 尺寸、请求纸张尺寸、驱动物理尺寸、可打印尺寸、左上右下硬件边距、实际缩放比例、绘制起点和输出尺寸。

百分比模式默认按物理纸张左上角对齐，保持原有行为；`fit` / `shrink` 默认按可打印区域左上角对齐，同时考虑可打印宽度和高度。`--center` 将任意模式改为在可打印区域居中；偏移在对齐之后叠加。缩放不会裁掉 PDF 自带留白，也不会改变纸张大小。`clipped=1` 表示页面矩形超出可打印区域，空白超出也会触发，并不一定代表文字被裁切。EMF 记录相同布局，但不模拟硬件裁切。

```powershell
# 先测试自动缩小到可打印区域并居中
.\dist\win-x86\yiz-print-pdf.exe --file "C:\路径\票据.pdf" --printer "XP-80C" --scale shrink --center

# 原尺寸打印，整体向左移动 2 mm（可能裁掉左侧内容）
.\dist\win-x86\yiz-print-pdf.exe --file "C:\路径\票据.pdf" --printer "XP-80C" --scale 100 --offset-x -2
```

打印示例输出：

```json
{"status":"submitted","jobId":123,"pages":1,"textMode":"alternate","alternateGlyphs":214,"fallbackRuns":0}
```

退出码：0 成功；2 参数错误；3 PDF 错误；4 打印机／纸型错误；5 Windows 打印或 EMF 输出错误；6 替代文字绘制错误或严格模式不满足。

## 替代文字的实现

固定 PDFium 分支 `chromium/5414`，提交 `89aa9f9ebe803480ec697fb39470ed106869e272`，作为 Win7 时代的验证基线。源码位于忽略提交的 `vendor/pdfium`，主仓库和源码依赖均使用 `--depth=1`。

`prepare-build.py` 在 `CGdiPrinterDriver::DrawDeviceText()` 中接入 `YizDrawDeviceText()`，不拆分图文层、不重新排版：

1. 取得 PDFium 当前字体的 TrueType 数据。
2. 仅重写字体内部名称，使用唯一名称通过 `AddFontMemResourceEx` 注册进程私有字体，防止系统字体与 PDF 子集字体同名造成字形索引错配。
3. 创建 GDI 字体，保留 PDFium 提供的逐字形位置和变换，使用 `ExtTextOutW(ETO_GLYPH_INDEX)` 输出文字。
4. 非文字部分及原有绘制顺序继续由 PDFium 处理。
5. 不支持的字体、竖排或透明文字等情况回退原路径并统计；文字输出中途失败则记录错误，中止作业，避免把已经绘制的文字再画一遍。

这是参考 QZ 的“让文字进入设备文字接口”的原理编写的 Windows 实现，并非复制 Java AWT。QZ 使用 Unicode + AWT drawString；这里使用同一内嵌字体的 glyph ID + Win32 ExtTextOutW，避免对 PDF 子集字体做二次 Unicode 字形映射。不承诺二者底层栅格化完全相同。

当前替代路径支持可注册的 TrueType 单字体（含常见内嵌宋体子集）。TTC、CFF/Type1 等不支持的输入保留 PDFium 原路径；透明度、文字裁剪等也可能由上层渲染器走其他路径。`alternateGlyphs` 是实际替代输出数量，`fallbackRuns` 仅统计进入该钩子后回退的调用，不能代表页面所有特殊文字对象的完整覆盖率。

缓存中的字体仅存在于当前进程内存，结束时释放。诊断 EMF 引用这些字体，仅用于分析绘制记录，不作为跨进程可移植的文档格式。

## 构建

构建机器需要 64 位 Windows、Git、Visual Studio 的 C++ 工具及 CMake/Ninja 组件、Windows SDK。构建工具不会成为目标机器的运行依赖。

```powershell
.\build.ps1

# 可指定 VS 和已安装的 SDK
.\build.ps1 -VisualStudioPath "C:\Program Files\Microsoft Visual Studio\18\Community" -WindowsSdkVersion "10.0.26100.0" -Jobs 8
```

脚本下载并校验便携 Python/GN，在项目内部缓存；按 PDFium DEPS 指定提交浅拉取构建依赖。禁用 V8、XFA、Skia 和 PartitionAlloc；设置静态 CRT、静态链接 ICU 数据。构建后将 EXE、DLL、导入库、头文件、绑定、示例和文档复制到 `dist`。

源文件：`src/main.cpp` 为 CLI 入口；`src/core.cpp` 为共用参数处理与 Win32 打印核心；`src/sdk.cpp` 为 C ABI 入口；`src/alternate_text.cpp` 为替代文字实现；`prepare-build.py` 为可重复的源码补丁与许可信息生成；`fetch-deps.py` 为依赖浅拉取。

## 已完成的非出纸检查

2026-10-14，在本机以 VS 2026 / MSVC 14.51 / Windows SDK 10.0.26100.0 构建：

- 单 EXE 约 3.24 MiB，导入表仅有 `KERNEL32.dll`、`USER32.dll`、`GDI32.dll`、`WINSPOOL.DRV`。
- 帮助、许可信息、打印机枚举、PDF 页面尺寸，以及参数、文件、打印机错误退出码检查通过。
- 使用之前生成的票据 PDF 和 XP-80C 的 203 × 203 DPI 设备设置记录 EMF，没有调用 StartDoc、没有出纸。
- 替代模式：30 次文字调用、214 个字形、0 回退、0 错误；EMF 中包含 214 条文字输出记录。
- 原始模式：0 条文字输出记录，字形走路径绘制。

纸张清晰度、条码扫描、切纸、多页及 Win7 实机兼容性待手动验证。

## 原生 SDK（ABI v1）

构建同时输出两个独立产物，共用 `src/core.cpp` 与文字绘制实现：

```text
dist/
  yiz-print-pdf.exe          独立 CLI，不依赖下面的 DLL
  yiz-print-pdf.dll          对应架构的原生 SDK，静态链接 PDFium 和 CRT
  yiz-print-pdf.lib          MSVC 导入库，不是静态打印核心
  include/yiz_print_pdf.h    C/C++ 公共 C ABI
  bindings/YizPrintPdf.cs    C# P/Invoke 和 byte[] 封装
  examples/cpp/main.cpp
  examples/csharp/Program.cs + Example.csproj
  README.md
  THIRD-PARTY-NOTICES.txt
```

EXE 与 DLL 各自仅依赖 Windows 系统 DLL 和安装的打印机驱动。C++ 使用 SDK 时需部署 DLL；独立 CLI 使用者只需要 EXE。C# 的运行时依赖由调用方项目决定，与原生 DLL 无关。同时交付 x86 和 x64；C# 进程位数必须与所部署的 DLL 一致，不要混用两套 DLL/导入库。示例使用 .NET 10，仅用于当前开发机；托管封装未依赖 .NET 10 特有 API，可在 .NET Framework 项目中使用，目标系统兼容性仍需实测。

### C ABI 约定

- `__cdecl` 调用约定，导出函数名不带修饰；结构体 `Pack=8`，枚举和开关均为 32 位整数。
- `YizInitOptions(&options, sizeof(options))` 初始化：ABI=1、driver 纸张、100% 缩放、alternate 文字、打印操作、无居中/偏移。
- `YizPrintFile` / `YizPrintMemory` 接收同一组结构化参数，支持 Print、Inspect、Emf 三种操作。Inspect 不需要打印机；Emf 需要打印机和输出路径，只记录第一页，不创建打印作业。
- 路径、打印机名为 UTF-16。输入字符串和 PDF 内存由调用方持有，必须在同步调用返回前保持有效且不被修改；内存输入不复制整份 PDF、不生成临时 PDF。C# 封装固定 byte[]，在返回后解除固定。
- `YizResultJson` / `YizResultLogs` 返回 UTF-8 字符串，仅在结果对象释放前有效。使用 `YizFreeResult` 在 DLL 内部释放结果，不能由调用方 `free/delete`。
- 返回码沿用 CLI 的 0、2–6，新增 7 表示内部异常。异常情况下结果可能为空；空结果的字符串读取和释放是安全的。
- ABI 不匹配、未知枚举、非法尺寸等返回 2；结构体大小与版本必须匹配当前 ABI。`YizGetAbiVersion()` 可查询版本。
- API 是阻塞调用。模块内部串行执行，保护 PDFium 全局状态和字体缓存；UI 请在后台线程调用。调用期间不要卸载 DLL，也不要在 DllMain 中调用。
- 当前每次任务初始化并关闭 PDFium；避免进程启动和 PDF 落盘，但尚未实现跨任务 PDFium 常驻缓存。
- 不主动修改宿主进程的控制台设置，不往宿主 stdout/stderr 写日志。CLI 收集完成后输出；DLL 调用方从结果中读取日志。
- PRINT 成功返回 `submitted` 和 Windows jobId，表示队列提交成功，不代表物理打印完成。无超时强杀、无自动重试；失败可能已有部分页面出纸。Windows 后台打印仍可能生成 spool 文件。

### C# 内存调用

将 `bindings/YizPrintPdf.cs` 加入项目，将 DLL 复制到应用输出目录，设置 `PlatformTarget=x86` 或 `x64`，并部署对应架构 DLL：

```csharp
var options = YizPrinting.PrintOptions.Create("XP-80C");
options.ScaleMode = YizPrinting.ScaleMode.Fit;
// pdfBytes 直接来自 PDF 生成器，不必保存临时文件。
var result = await Task.Run(() => YizPrinting.PrinterSdk.PrintMemory(pdfBytes, options));
// result.Code、result.Json、result.Logs；业务层按返回码处理，不自动重试。
```

C# 示例默认只检查 PDF；只有显式 `--print` 才会出纸。以下在 `dist/win-x86` 目录执行；x64 请进入 `dist/win-x64`，构建时添加 `-p:PlatformTarget=x64`，运行路径改为 `win-x64`：

```powershell
dotnet build examples/csharp/Example.csproj -c Release
.\examples\csharp\bin\Release\net10.0-windows\win-x86\Example.exe "C:\路径\票据.pdf"
# 内存渲染至 EMF，不出纸
.\examples\csharp\bin\Release\net10.0-windows\win-x86\Example.exe "C:\路径\票据.pdf" "XP-80C" "C:\路径\诊断.emf"
# 实际打印（请手动执行）
.\examples\csharp\bin\Release\net10.0-windows\win-x86\Example.exe "C:\路径\票据.pdf" "XP-80C" --print
```

### C/C++ 调用

头文件同时支持 C 和 C++，不暴露 STL、PDFium 类型或 C++ 类。在 x86 Native Tools 命令提示符中，进入 `dist/win-x86` 目录（x64 使用 x64 Native Tools 和 `dist/win-x64`）：

```bat
cl /nologo /EHsc /MT /I include examples\cpp\main.cpp yiz-print-pdf.lib /Fe:example-cpp.exe
example-cpp.exe "C:\路径\票据.pdf"
```

示例读取文件到内存后调用 `YizPrintMemory`；生产代码可以直接传生成器缓冲区。参数规则与 C# 示例相同，也可以直接调用 `YizPrintFile`。

默认 build.ps1 构建两套架构；可通过 -Architecture x86 或 -Architecture x64 单独构建。两套构建缓存分别位于 vendor/pdfium/out/Release-x86 和 Release-x64。


## 更名与架构

项目由 lc-print-pdf 更名为 yiz-print-pdf。原生文件名为 `yiz-print-pdf.exe/.dll/.lib`，公共头文件为 `yiz_print_pdf.h`，C API 前缀为 `Yiz`、宏前缀为 `YIZ_`，C# 命名空间为 `YizPrinting`，绑定文件为 `YizPrintPdf.cs`。旧名称不提供二进制兼容别名，调用方需同步更新引用。

默认 `build.ps1` 构建 x86 与 x64；`-Architecture x86` 或 `-Architecture x64` 只构建指定架构。`dist/win-x86`、`dist/win-x64` 各自包含完整 SDK。仓库内 C# 示例默认 x86，使用 `-p:PlatformTarget=x64` 切换；发行目录内示例默认使用所在目录的架构。
