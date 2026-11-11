# Yiz.PrintPdf

Windows 原生 PDF 打印 SDK / Native PDF printing SDK for Windows.

包含 C# 封装及 x86/x64 原生 DLL，支持文件、内存、批量打印和替代文字绘制。Contains a C# wrapper and x86/x64 native libraries, with file/memory/batch printing and alternative text rendering.

## 安装 / Install

```powershell
dotnet add package Yiz.PrintPdf --version 0.5.0 --source <local-package-directory>
```

本包面向 Windows 上采用 PackageReference 的现代 .NET 应用（.NET 6+）。明确设置 `RuntimeIdentifier` 为 `win-x86` 或 `win-x64`，并匹配 `PlatformTarget`。NuGet 按 RID 选择并复制原生 DLL，无需手动引用绑定源文件。本包暂不承诺支持 .NET Framework 或 packages.config；需要时可使用 GitHub Release 中的手动集成 SDK。

从源码生成本地包时，需先构建 x86/x64 原生文件，再执行 `dotnet pack nuget/Yiz.PrintPdf.csproj -c Release` 并指定本地包目录。ABI v2 要求同步更新原生 DLL 与托管绑定，不兼容 ABI v1。

For modern .NET applications (.NET 6+) on Windows using PackageReference. Set RuntimeIdentifier to win-x86 or win-x64 and match PlatformTarget. NuGet selects the corresponding native DLL; no manual binding-source reference is needed. .NET Framework and packages.config are not validated for this package; use the manual SDK from GitHub Releases if needed.

```xml
<PropertyGroup>
  <RuntimeIdentifier>win-x64</RuntimeIdentifier>
  <PlatformTarget>x64</PlatformTarget>
</PropertyGroup>
```

## 调用 / Usage

```csharp
using YizPrinting;

var options = PrintOptions.Create("XP-80C");
options.ScaleMode = ScaleMode.Fit;
options.Orientation = Orientation.Landscape;
var result = await Task.Run(() => PrinterSdk.PrintMemory(pdfBytes, options));
// Multiple PDFs, shared settings, separate spool jobs:
var batch = await Task.Run(() =>
    PrinterSdk.PrintMemoryBatch(new[] { pdfBytes1, pdfBytes2 }, options));
// result.Code / result.Json / result.Logs
```

调用同步阻塞，请在 UI 后台线程执行。成功仅表示提交至 Windows 打印队列。批量按顺序执行，首次失败后停止，不自动重试。内存输入无需临时 PDF，但 Windows 可能写入 spool 文件。仍需安装打印机驱动。

Calls block; use a background thread in UI applications. Success means queue submission, not physical print completion. Batches run in order and stop at the first failure without retry. Memory input avoids temporary PDFs; Windows may still write spool files. An installed printer driver is required.

原生 SDK 以 Windows 7 SP1 为兼容性目标，尚未实机验证；托管应用还须满足所选 .NET 运行时的系统要求。The native SDK targets Windows 7 SP1 but is not yet tested on a real Win7 machine; the managed application's chosen .NET runtime has its own OS requirements.

[完整文档 / Documentation](https://github.com/EakerZT/yiz-print-pdf)
