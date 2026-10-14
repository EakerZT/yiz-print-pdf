#include "yiz_print_pdf.h"
#include <cstdio>
#include <vector>
#include <windows.h>
int wmain(int argc, wchar_t** argv) {
  if (argc < 2) { puts("example PDF [printer] [emf-path | --print]"); return 2; }
  // Default is inspect only; printing requires explicit --print.
  YizPrintOptions options;
  if (YizInitOptions(&options, sizeof(options))) return 2;
  options.operation = YIZ_INSPECT;
  if (argc > 3) {
    options.printer = argv[2]; options.scale_mode = YIZ_SCALE_FIT;
    options.operation = wcscmp(argv[3], L"--print") ? YIZ_EMF : YIZ_PRINT;
    if (options.operation == YIZ_EMF) options.emf_path = argv[3];
  }
  HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) return 3;
  LARGE_INTEGER length;
  if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 512LL*1024*1024) { CloseHandle(file); return 3; }
  std::vector<unsigned char> pdf(size_t(length.QuadPart));
  DWORD read = 0;
  bool ok = ReadFile(file, pdf.data(), DWORD(pdf.size()), &read, nullptr) && read == pdf.size();
  CloseHandle(file);
  if (!ok) return 3;
  YizResult* result = nullptr;
  int code = YizPrintMemory(pdf.data(), pdf.size(), &options, &result);
  puts(YizResultJson(result)); fputs(YizResultLogs(result), stderr);
  YizFreeResult(result);
  return code;
}
