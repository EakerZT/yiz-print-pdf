param(
    [string]$VisualStudioPath = '',
    [string]$WindowsSdkVersion = '10.0.26100.0',
    [ValidateSet('x86', 'x64', 'all')]
    [string]$Architecture = 'all',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
if ($Architecture -eq 'all') {
    foreach ($arch in @('x86', 'x64')) {
        & $PSCommandPath -Architecture $arch -VisualStudioPath $VisualStudioPath -WindowsSdkVersion $WindowsSdkVersion -Jobs $Jobs
    }
    return
}
$root = $PSScriptRoot
$pdf = Join-Path $root 'vendor\pdfium'
$toolDir = Join-Path $root '.tools'
New-Item -ItemType Directory -Force $toolDir | Out-Null

function Run([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $program" }
}
function ToolZip([string]$url, [string]$archiveName, [string]$destination, [string]$hash) {
    $archive = Join-Path $toolDir $archiveName
    if (!(Test-Path $archive)) { Invoke-WebRequest $url -OutFile $archive -UseBasicParsing }
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $hash) { throw "Checksum mismatch: $archive" }
    if (!(Test-Path $destination)) { Expand-Archive $archive $destination }
}
ToolZip 'https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip' 'python.zip' (Join-Path $toolDir 'python') '009D6BF7E3B2DDCA3D784FA09F90FE54336D5B60F0E0F305C37F400BF83CFD3B'
ToolZip 'https://chrome-infra-packages.appspot.com/dl/gn/gn/windows-amd64/+/git_revision:11dc0b1f438bd26380774e9d50fd4c63f346d41a' 'gn.zip' (Join-Path $toolDir 'gn') '1BBF1F0C478F92A37E8B4DD0A26B6EF95778688078697C264B1DB4F15D127AAB'
$python = Join-Path $toolDir 'python\python.exe'
$pth = Join-Path $toolDir 'python\python311._pth'
if (Test-Path $pth) { Move-Item -LiteralPath $pth -Destination "$pth.disabled" -Force }
Copy-Item $python (Join-Path $toolDir 'python\python3.exe') -Force
$gn = Join-Path $toolDir 'gn\gn.exe'

if (!$VisualStudioPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (!$VisualStudioPath) { throw 'Install Visual Studio C++ build tools and Windows SDK first.' }
$ninja = Join-Path $VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (!(Test-Path $ninja)) { throw 'Install the Visual Studio C++ CMake tools (Ninja).' }

$revision = '89aa9f9ebe803480ec697fb39470ed106869e272'
if (!(Test-Path (Join-Path $pdf '.git'))) {
    Run 'git' @('clone', '--depth=1', '--single-branch', '--branch', 'chromium/5414', 'https://pdfium.googlesource.com/pdfium.git', $pdf)
}
$head = & git -C $pdf rev-parse HEAD
if ($head -ne $revision) { throw "Unexpected PDFium revision: $head; expected $revision" }
Run $python @((Join-Path $root 'fetch-deps.py'))
Run $python @((Join-Path $root 'prepare-build.py'))
Run $python @((Join-Path $pdf 'build\util\lastchange.py'), '-o', (Join-Path $pdf 'build\util\LASTCHANGE'))

$out = Join-Path $pdf "out\Release-$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
$vsGn = $VisualStudioPath.Replace('\', '/')
@"
is_debug = false
is_component_build = false
target_cpu = "$Architecture"
is_clang = false
use_lld = false
use_custom_libcxx = false
pdf_enable_v8 = false
pdf_enable_xfa = false
pdf_use_skia = false
pdf_use_skia_paths = false
pdf_use_partition_alloc = false
pdf_is_standalone = false
icu_use_data_file = false
symbol_level = 0
treat_warnings_as_errors = false
visual_studio_path = "$vsGn"
visual_studio_version = "2022"
wdk_path = "C:/Program Files (x86)/Windows Kits/10"
windows_sdk_path = "C:/Program Files (x86)/Windows Kits/10"
"@ | Set-Content (Join-Path $out 'args.gn') -Encoding ASCII

$oldPath = $env:PATH
$oldToolchain = $env:DEPOT_TOOLS_WIN_TOOLCHAIN
$oldVs = $env:GYP_MSVS_OVERRIDE_PATH
$oldSdk = $env:YIZ_WINDOWS_SDK
try {
    $env:PATH = (Join-Path $toolDir 'python') + ';' + $env:PATH
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = '0'
    $env:GYP_MSVS_OVERRIDE_PATH = $VisualStudioPath
    $env:YIZ_WINDOWS_SDK = $WindowsSdkVersion
    Run $gn @('gen', $out, "--root=$pdf")
    Run $ninja @('-C', $out, 'yiz-print-pdf', 'yiz-print-pdf-sdk', '-j', "$Jobs")
    $dist = Join-Path $root "dist\win-$Architecture"
    New-Item -ItemType Directory -Force $dist | Out-Null
    Copy-Item (Join-Path $out 'yiz-print-pdf.exe') $dist -Force
    Copy-Item (Join-Path $out 'yiz-print-pdf.dll') $dist -Force
    Copy-Item (Join-Path $out 'yiz-print-pdf.dll.lib') (Join-Path $dist 'yiz-print-pdf.lib') -Force
    New-Item -ItemType Directory -Force (Join-Path $dist 'include') | Out-Null
    Copy-Item (Join-Path $root 'src\yiz_print_pdf.h') (Join-Path $dist 'include') -Force
    Copy-Item (Join-Path $root 'bindings') $dist -Recurse -Force
    foreach ($language in @('cpp', 'csharp')) {
        $exampleDist = Join-Path $dist "examples\$language"
        New-Item -ItemType Directory -Force $exampleDist | Out-Null
        Get-ChildItem (Join-Path $root "examples\$language") -File | Copy-Item -Destination $exampleDist -Force
    }
    $exampleProject = Join-Path $dist 'examples\csharp\Example.csproj'
    (Get-Content $exampleProject -Raw).Replace('>x86</PlatformTarget>', ">$Architecture</PlatformTarget>") | Set-Content $exampleProject -Encoding UTF8
    Copy-Item (Join-Path $root 'README.md') $dist -Force
    & (Join-Path $dist 'yiz-print-pdf.exe') --licenses | Set-Content (Join-Path $dist 'THIRD-PARTY-NOTICES.txt') -Encoding UTF8
    if ($LASTEXITCODE -ne 0) { throw 'Failed to export third-party notices' }
    Write-Host "Built: $dist\yiz-print-pdf.exe"
} finally {
    $env:PATH = $oldPath
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = $oldToolchain
    $env:GYP_MSVS_OVERRIDE_PATH = $oldVs
    $env:YIZ_WINDOWS_SDK = $oldSdk
}
