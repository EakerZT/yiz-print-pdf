#include <windows.h>
#include <cstdio>
#include "core.h"
int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);
  auto result = RunCore(argc, argv);
  fputs(result.json.c_str(), stdout);
  fputs(result.logs.c_str(), stderr);
  return result.code;
}
