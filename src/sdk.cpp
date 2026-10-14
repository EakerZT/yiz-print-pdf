#define YIZ_PRINT_PDF_STATIC
#include "yiz_print_pdf.h"
#include "core.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <new>
struct YizResult { CoreResult value; };
namespace {
int Fail(YizResult** out, int code, const char* message) {
  if (out) {
    *out = new (std::nothrow) YizResult;
    if (*out) {
      (*out)->value.code = code;
      (*out)->value.json = "{\"status\":\"failed\",\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\"}";
    }
  }
  return code;
}
std::wstring Number(double value) {
  wchar_t text[64]; swprintf_s(text, L"%.17g", value); return text;
}
int Invoke(const wchar_t* path, const void* bytes, size_t length, bool memory,
           const YizPrintOptions* o, YizResult** out) {
  if (!out) return YIZ_ARGUMENT;
  *out = nullptr;
  if (!o || o->struct_size != sizeof(*o) || o->abi_version != YIZ_ABI_VERSION)
    return Fail(out, YIZ_ARGUMENT, "Invalid options size or ABI version");
  if (o->scale_mode < 0 || o->scale_mode > 2 || o->paper_mode < 0 || o->paper_mode > 1 ||
      o->text_mode < 0 || o->text_mode > 1 || o->operation < 0 || o->operation > 2 ||
      o->center < 0 || o->center > 1 || o->require_alternate < 0 || o->require_alternate > 1)
    return Fail(out, YIZ_ARGUMENT, "Invalid enum or boolean");
  if (!std::isfinite(o->width_mm) || !std::isfinite(o->height_mm) || o->width_mm < 0 || o->height_mm < 0)
    return Fail(out, YIZ_ARGUMENT, "Invalid paper dimensions");
  if (memory ? (!bytes || !length || length > 512ULL*1024*1024) : (!path || !*path))
    return Fail(out, YIZ_ARGUMENT, "Provide a file path or 1..512 MiB memory buffer");
  if (o->operation == YIZ_EMF && (!o->emf_path || !*o->emf_path))
    return Fail(out, YIZ_ARGUMENT, "EMF operation requires emf_path");
  if (o->operation != YIZ_EMF && o->emf_path && *o->emf_path)
    return Fail(out, YIZ_ARGUMENT, "emf_path requires EMF operation");
  std::vector<std::wstring> args = {L"sdk", L"--file", memory ? L"Memory PDF" : path,
    L"--printer", o->printer ? o->printer : L"", L"--paper", o->paper_mode == YIZ_PAPER_DRIVER ? L"driver" : L"pdf",
    L"--text-mode", o->text_mode == YIZ_TEXT_ALTERNATE ? L"alternate" : L"original",
    L"--scale", o->scale_mode == YIZ_SCALE_FIT ? L"fit" : o->scale_mode == YIZ_SCALE_SHRINK ? L"shrink" : Number(o->scale_percent),
    L"--offset-x", Number(o->offset_x_mm), L"--offset-y", Number(o->offset_y_mm)};
  if (o->width_mm != 0 || o->height_mm != 0) {
    args.insert(args.end(), {L"--width", Number(o->width_mm), L"--height", Number(o->height_mm)});
  }
  if (o->center) args.push_back(L"--center");
  if (o->require_alternate) args.push_back(L"--require-alternate");
  if (o->operation == YIZ_INSPECT) args.push_back(L"--inspect");
  if (o->operation == YIZ_EMF) args.insert(args.end(), {L"--emf", o->emf_path});
  std::vector<const wchar_t*> pointers;
  for (const auto& a : args) pointers.push_back(a.c_str());
  YizResult result;
  result.value = RunCore(int(pointers.size()), pointers.data(), bytes, length);
  *out = new YizResult(std::move(result));
  return (*out)->value.code;
}
}
uint32_t YIZ_CALL YizGetAbiVersion(void) { return YIZ_ABI_VERSION; }
int32_t YIZ_CALL YizInitOptions(YizPrintOptions* o, uint32_t size) {
  if (!o || size != sizeof(*o)) return YIZ_ARGUMENT;
  *o = {}; o->struct_size = size; o->abi_version = YIZ_ABI_VERSION; o->scale_percent = 100;
  return YIZ_OK;
}
int32_t YIZ_CALL YizPrintFile(const wchar_t* path, const YizPrintOptions* o, YizResult** out) {
  try { return Invoke(path, nullptr, 0, false, o, out); }
  catch (...) { if (out) *out = nullptr; return YIZ_INTERNAL; }
}
int32_t YIZ_CALL YizPrintMemory(const void* bytes, size_t length, const YizPrintOptions* o, YizResult** out) {
  try { return Invoke(nullptr, bytes, length, true, o, out); }
  catch (...) { if (out) *out = nullptr; return YIZ_INTERNAL; }
}
int32_t YIZ_CALL YizListPrinters(YizResult** out) {
  if (!out) return YIZ_ARGUMENT;
  *out = nullptr;
  try {
    const wchar_t* args[] = {L"sdk", L"--list-printers"};
    auto value = RunCore(2, args);
    *out = new YizResult{std::move(value)};
    return (*out)->value.code;
  } catch (...) { return YIZ_INTERNAL; }
}
const char* YIZ_CALL YizResultJson(const YizResult* r) { return r ? r->value.json.c_str() : ""; }
const char* YIZ_CALL YizResultLogs(const YizResult* r) { return r ? r->value.logs.c_str() : ""; }
void YIZ_CALL YizFreeResult(YizResult* r) { delete r; }
