"""Apply a small, reproducible patch to the pinned PDFium checkout."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parent
pdf = root / 'vendor/pdfium'
dst = pdf / 'yiz_tool'
dst.mkdir(exist_ok=True)
for file in (root / 'src').iterdir():
    if file.is_file():
        shutil.copy2(file, dst / file.name)
driver = pdf / 'core/fxge/win32/cgdi_printer_driver.cpp'
source = driver.read_text(encoding='utf-8')
include = '#include "yiz_tool/alternate_text.h"'
if include not in source:
    source = source.replace('#include <math.h>', include + '\n\n#include <math.h>')
    marker = 'const CFX_TextRenderOptions& /*options*/) {\n  return false;'
    assert source.count(marker) == 1
    source = source.replace(marker, 'const CFX_TextRenderOptions& /*options*/) {\n  return YizDrawDeviceText(m_hDC, pCharPos, pFont, mtObject2Device, font_size, color);')
    driver.write_text(source, encoding='utf-8')
print('Prepared PDFium text output hook and CLI target')

def replace(path, old, new):
    file = pdf / path
    text = file.read_text(encoding='utf-8')
    if old in text:
        file.write_text(text.replace(old, new), encoding='utf-8')
    elif new not in text:
        raise RuntimeError('Unexpected source: ' + path)

replace('BUILD.gn', 'deps = [ ":pdfium_all" ]', 'deps = [ "//yiz_tool:yiz-print-pdf" ]')
replace('build/toolchain/win/BUILD.gn',
        'if (current_toolchain == default_toolchain) {',
        'if (current_toolchain == default_toolchain && is_component_build) {')
replace('build/toolchain/win/setup_toolchain.py',
        "args.append('10.0.20348.0')", "args.append(os.environ.get('YIZ_WINDOWS_SDK', '10.0.26100.0'))")
replace('build/config/win/BUILD.gn', 'NTDDI_VERSION=NTDDI_WIN10_FE', 'NTDDI_VERSION=0x06010000')
replace('build/config/win/BUILD.gn', '"_WIN32_WINNT=0x0A00"', '"_WIN32_WINNT=0x0601"')
replace('build/config/win/BUILD.gn', '"WINVER=0x0A00"', '"WINVER=0x0601"')
replace('build/config/win/visual_studio_version.gni', 'visual_studio_runtime_dirs = []', 'visual_studio_runtime_dirs = "unused"')
replace('third_party/base/win/win_util.cc',
        'typedef decltype(\n        GetProcessMitigationPolicy)* GetProcessMitigationPolicyType;',
        'typedef BOOL(WINAPI* GetProcessMitigationPolicyType)(HANDLE, PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);')
replace('third_party/icu/BUILD.gn', 'script = "scripts/asm_to_inline_asm.py"', 'script = "//yiz_tool/icu_data.py"')
replace('build/config/win/BUILD.gn', 'subsystem_version_suffix = ",5.01"', 'subsystem_version_suffix = ",6.01"')
replace('build/config/win/BUILD.gn', 'subsystem_version_suffix = ",5.02"', 'subsystem_version_suffix = ",6.01"')
(pdf / 'build/config/gclient_args.gni').write_text('', encoding='utf-8')
# Test fonts are never linked into the CLI; GN still visits the upstream test target.
(pdf / 'third_party/test_fonts').mkdir(exist_ok=True)
(pdf / 'third_party/test_fonts/BUILD.gn').write_text('group("test_fonts") {}\n', encoding='utf-8')

notices = ['yiz-print-pdf: original Windows CLI and device text implementation.\n'
           'PDFium revision: 89aa9f9ebe803480ec697fb39470ed106869e272\n'
           'Third-party license notices follow.\n']
for base in [pdf / 'LICENSE', pdf / 'third_party', pdf / 'base/allocator/partition_allocator']:
    paths = [base] if base.is_file() else sorted(base.rglob('*'))
    for file in paths:
        if file.is_file() and file.name.upper() in {'LICENSE', 'LICENSE.TXT', 'LICENSE-2.0.TXT', 'COPYING', 'COPYING.TXT', 'FTL.TXT'}:
            notices.append('\n=== ' + file.relative_to(pdf).as_posix() + ' ===\n' + file.read_text(encoding='utf-8', errors='replace'))
notice_text = '\n'.join(notices)
# Escape in short literals to avoid MSVC's limit on individual string literals.
import json
header = 'static const char* const kNotices[] = {\n' + ''.join(json.dumps(line + '\n', ensure_ascii=True) + ',\n' for line in notice_text.splitlines()) + '};\n'
(dst / 'notices.h').write_text(header, encoding='ascii')
