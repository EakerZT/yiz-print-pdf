#include <windows.h>
#include <winspool.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "public/fpdfview.h"
#include "public/fpdf_edit.h"
#include "alternate_text.h"
#include "notices.h"
#include "core.h"
#include <cstdarg>
#include <stdexcept>

namespace {
thread_local CoreResult* output = nullptr;
int Capture(bool log, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int count = _vscprintf(format, args);
  va_end(args);
  if (count < 0) return count;
  std::vector<char> buffer(size_t(count) + 1);
  va_start(args, format);
  vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);
  (log ? output->logs : output->json).append(buffer.data(), count);
  return count;
}
int Line(const char* text) { return Capture(false, "%s\n", text); }

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string result(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), &result[0], n, nullptr, nullptr);
  return result;
}
std::string Json(const std::wstring& text) {
  std::string out = "\"";
  for (unsigned char c : Utf8(text)) {
    if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
    else if (c < 32) { char b[7]; sprintf_s(b, "\\u%04x", c); out += b; }
    else out += char(c);
  }
  return out + '"';
}
int Error(int code, const wchar_t* message, DWORD win32 = 0) {
  Capture(false, "{\"status\":\"failed\",\"code\":%d,\"message\":%s,\"win32Error\":%lu}\n", code, Json(message).c_str(), win32);
  return code;
}
void Help() {
  Line("yiz-print-pdf 0.1.0 / PDFium chromium/5414 / static\n"
       "Usage:\n"
       "  yiz-print-pdf.exe --list-printers\n"
       "  yiz-print-pdf.exe --file <PDF> --inspect\n"
       "  yiz-print-pdf.exe --file <PDF> --printer <system name> [options]\n"
       "Options:\n"
       "  --text-mode alternate|original  Default: alternate\n"
       "  --scale <percent>|fit|shrink    Default: 100; percent range: 10..200\n"
       "  --center                       Center within printable area\n"
       "  --offset-x <mm> --offset-y <mm> Additional offset; positive right/down\n"
       "  --paper pdf|driver              Default: driver (driver paper size)\n"
       "  --width <mm> --height <mm>       Optional fixed paper, specify both\n"
       "  --require-alternate             Abort if any text run falls back,\n"
       "                                  or a page has no alternate glyphs\n"
       "  --emf <path>                    Render first page to EMF with the\n"
       "                                  selected printer DC, WITHOUT printing\n"
       "  --help | --licenses\n"
       "One PDF = one spool job. Success means submitted, NOT physical completion.\n"
       "UTF-8 JSON result on stdout; diagnostics on stderr. No automatic retry.");
}
bool Number(const wchar_t* value, double& result) {
  wchar_t* end = nullptr;
  result = wcstod(value, &end);
  return end != value && !*end && std::isfinite(result);
}
struct Options {
  std::wstring file, printer, emf;
  bool alternate = true, inspect = false, driver_paper = true, strict = false;
  bool center = false, scale_set = false;
  std::wstring scale_mode = L"percent";
  double offset_x = 0, offset_y = 0;
  double scale = 100, width = 0, height = 0;
};
int ListPrinters() {
  DWORD bytes = 0, count = 0;
  EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, nullptr, 0, &bytes, &count);
  if (!bytes) { Line("{\"printers\":[]}"); return 0; }
  std::vector<BYTE> buffer(bytes);
  if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, buffer.data(), bytes, &bytes, &count)) return Error(4, L"EnumPrinters failed", GetLastError());
  auto* printers = reinterpret_cast<PRINTER_INFO_4W*>(buffer.data());
  Capture(false, "{\"printers\":[");
  for (DWORD i = 0; i < count; ++i) Capture(false, "%s%s", i ? "," : "", Json(printers[i].pPrinterName).c_str());
  Line("]}");
  return 0;
}
struct Document {
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  const void* data = nullptr;
  FPDF_DOCUMENT doc = nullptr;
  Document() { FPDF_InitLibrary(); }
  ~Document() {
    YizReleaseFonts();
    if (doc) FPDF_CloseDocument(doc);
    if (data) UnmapViewOfFile(data);
    if (mapping) CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    FPDF_DestroyLibrary();
  }
  bool OpenMemory(const void* bytes, size_t length) {
    if (!bytes || !length || length > 512ULL * 1024 * 1024) return false;
    doc = FPDF_LoadMemDocument64(bytes, length, nullptr);
    return doc != nullptr;
  }
  bool Open(const std::wstring& path) {
    file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 512LL * 1024 * 1024) return false;
    mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) return false;
    data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!data) return false;
    doc = FPDF_LoadMemDocument64(data, size_t(length.QuadPart), nullptr);
    return doc != nullptr;
  }
};
struct Printer {
  HANDLE handle = nullptr;
  HDC dc = nullptr;
  bool started = false;
  std::vector<BYTE> mode;
  ~Printer() {
    if (dc) { if (started) AbortDoc(dc); DeleteDC(dc); }
    if (handle) ClosePrinter(handle);
  }
  DEVMODEW* DevMode() { return reinterpret_cast<DEVMODEW*>(mode.data()); }
  bool Open(const std::wstring& name) {
    if (!OpenPrinterW(const_cast<wchar_t*>(name.c_str()), &handle, nullptr)) return false;
    LONG bytes = DocumentPropertiesW(nullptr, handle, const_cast<wchar_t*>(name.c_str()), nullptr, nullptr, 0);
    if (bytes < LONG(sizeof(DEVMODEW))) return false;
    mode.resize(bytes);
    return DocumentPropertiesW(nullptr, handle, const_cast<wchar_t*>(name.c_str()), DevMode(), nullptr, DM_OUT_BUFFER) == IDOK;
  }
  bool Configure(const Options& opt, double w, double h) {
    auto* dm = DevMode();
    dm->dmFields |= DM_COPIES;
    dm->dmCopies = 1;
    if (dm->dmFields & DM_DUPLEX) dm->dmDuplex = DMDUP_SIMPLEX;
    if (!opt.driver_paper || opt.width > 0) {
      dm->dmFields |= DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH | DM_ORIENTATION;
      dm->dmPaperSize = DMPAPER_USER;
      dm->dmPaperWidth = short(std::lround(w * 10));
      dm->dmPaperLength = short(std::lround(h * 10));
      dm->dmOrientation = DMORIENT_PORTRAIT;
    }
    if (DocumentPropertiesW(nullptr, handle, const_cast<wchar_t*>(opt.printer.c_str()), dm, dm, DM_IN_BUFFER | DM_OUT_BUFFER) != IDOK) return false;
    if (!dc) dc = CreateDCW(L"WINSPOOL", opt.printer.c_str(), nullptr, dm);
    else { HDC next = ResetDCW(dc, dm); if (!next) return false; dc = next; }
    return dc != nullptr;
  }
};
}

static int Execute(int argc, const wchar_t* const* argv, const void* memory, size_t length) {
  if (argc == 1) { Help(); return 2; }
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::wstring arg = argv[i];
    if (arg == L"--help") { Help(); return 0; }
    if (arg == L"--licenses") { for (const char* line : kNotices) Capture(false, "%s", line); return 0; }
    if (arg == L"--list-printers") return ListPrinters();
    if (arg == L"--inspect") { opt.inspect = true; continue; }
    if (arg == L"--require-alternate") { opt.strict = true; continue; }
    if (arg == L"--center") { opt.center = true; continue; }
    if (i + 1 == argc) return Error(2, L"Missing argument value");
    const wchar_t* value = argv[++i];
    if (arg == L"--file") opt.file = value;
    else if (arg == L"--printer") opt.printer = value;
    else if (arg == L"--emf") opt.emf = value;
    else if (arg == L"--text-mode") {
      if (wcscmp(value, L"alternate") && wcscmp(value, L"original")) return Error(2, L"Invalid text mode");
      opt.alternate = !wcscmp(value, L"alternate");
    } else if (arg == L"--paper") {
      if (wcscmp(value, L"pdf") && wcscmp(value, L"driver")) return Error(2, L"Invalid paper mode");
      opt.driver_paper = !wcscmp(value, L"driver");
    } else if (arg == L"--scale") {
      if (opt.scale_set) return Error(2, L"Specify --scale only once");
      opt.scale_set = true;
      if (!wcscmp(value, L"fit") || !wcscmp(value, L"shrink")) opt.scale_mode = value;
      else if (!Number(value, opt.scale) || opt.scale < 10 || opt.scale > 200) return Error(2, L"Scale must be 10..200, fit, or shrink");
    }
    else if (arg == L"--offset-x") { if (!Number(value, opt.offset_x) || std::abs(opt.offset_x) > 3000) return Error(2, L"Offset X must be -3000..3000 mm"); }
    else if (arg == L"--offset-y") { if (!Number(value, opt.offset_y) || std::abs(opt.offset_y) > 3000) return Error(2, L"Offset Y must be -3000..3000 mm"); }
    else if (arg == L"--width") { if (!Number(value, opt.width) || opt.width <= 0 || opt.width > 3000) return Error(2, L"Width must be >0 and <=3000 mm"); }
    else if (arg == L"--height") { if (!Number(value, opt.height) || opt.height <= 0 || opt.height > 3000) return Error(2, L"Height must be >0 and <=3000 mm"); }
    else return Error(2, L"Unknown argument");
  }
  if (opt.file.empty() || (!opt.inspect && opt.printer.empty())) return Error(2, L"Specify --file and --printer (or --inspect)");
  if ((opt.width > 0) != (opt.height > 0)) return Error(2, L"Specify both --width and --height");
  if (opt.strict && !opt.alternate) return Error(2, L"--require-alternate needs alternate mode");
  Document doc;
  if (!(memory ? doc.OpenMemory(memory, length) : doc.Open(opt.file))) { Capture(true, "PDFium error: %lu\n", FPDF_GetLastError()); return Error(3, L"Cannot load PDF (must be unencrypted, 1..512 MiB)", GetLastError()); }
  int count = FPDF_GetPageCount(doc.doc);
  if (count < 1) return Error(3, L"PDF has no pages");
  std::vector<FS_SIZEF> sizes(count);
  for (int i = 0; i < count; ++i) {
    if (!FPDF_GetPageSizeByIndexF(doc.doc, i, &sizes[i]) || !std::isfinite(sizes[i].width) || !std::isfinite(sizes[i].height) || sizes[i].width <= 0 || sizes[i].height <= 0 || sizes[i].width > 3000 * 72 / 25.4 || sizes[i].height > 3000 * 72 / 25.4) return Error(3, L"Invalid or oversized PDF page (max 3000 mm)");
  }
  if (opt.inspect) {
    Capture(false, "{\"pages\":[");
    for (int i = 0; i < count; ++i) Capture(false, "%s{\"page\":%d,\"widthMm\":%.3f,\"heightMm\":%.3f}", i ? "," : "", i + 1, sizes[i].width * 25.4 / 72, sizes[i].height * 25.4 / 72);
    Line("]}"); return 0;
  }
  Printer printer;
  if (!printer.Open(opt.printer)) return Error(4, L"Cannot open printer", GetLastError());
  YizSetAlternateText(opt.alternate);
  FPDF_SetPrintMode(FPDF_PRINTMODE_EMF);
  int job = 0;
  unsigned total_glyphs = 0, total_fallbacks = 0;
  int render_count = opt.emf.empty() ? count : 1;
  for (int i = 0; i < render_count; ++i) {
    double w = opt.width > 0 ? opt.width : sizes[i].width * 25.4 / 72;
    double h = opt.height > 0 ? opt.height : sizes[i].height * 25.4 / 72;
    if (!printer.Configure(opt, w, h)) return Error(4, L"Cannot configure printer", GetLastError());
    int dpi_x = GetDeviceCaps(printer.dc, LOGPIXELSX), dpi_y = GetDeviceCaps(printer.dc, LOGPIXELSY);
    if (dpi_x <= 0 || dpi_y <= 0) return Error(4, L"Printer reported invalid DPI");
    int physical_w = GetDeviceCaps(printer.dc, PHYSICALWIDTH), physical_h = GetDeviceCaps(printer.dc, PHYSICALHEIGHT);
    int printable_w = GetDeviceCaps(printer.dc, HORZRES), printable_h = GetDeviceCaps(printer.dc, VERTRES);
    int left = GetDeviceCaps(printer.dc, PHYSICALOFFSETX), top = GetDeviceCaps(printer.dc, PHYSICALOFFSETY);
    if (physical_w <= 0 || physical_h <= 0 || printable_w <= 0 || printable_h <= 0) return Error(4, L"Printer reported invalid page area");
    double natural_x = sizes[i].width / 72 * dpi_x, natural_y = sizes[i].height / 72 * dpi_y;
    double factor = opt.scale / 100;
    if (opt.scale_mode != L"percent") {
      factor = (std::min)(printable_w / natural_x, printable_h / natural_y);
      if (opt.scale_mode == L"shrink") factor = (std::min)(1.0, factor);
    }
    double scaled_x = natural_x * factor, scaled_y = natural_y * factor;
    if (!std::isfinite(scaled_x) || !std::isfinite(scaled_y) || scaled_x > 100000 || scaled_y > 100000) return Error(4, L"Rendered page dimensions out of range");
    int px = int(std::lround(scaled_x));
    int py = int(std::lround(scaled_y));
    if (px <= 0 || py <= 0 || px > 100000 || py > 100000) return Error(4, L"Rendered page dimensions out of range");
    // Numeric scaling retains physical-page alignment for backwards compatibility.
    // Fit/shrink align to the printable origin; centering always uses that area.
    int origin_x = opt.scale_mode == L"percent" ? -left : 0;
    int origin_y = opt.scale_mode == L"percent" ? -top : 0;
    if (opt.center) { origin_x = (printable_w - px) / 2; origin_y = (printable_h - py) / 2; }
    origin_x += int(std::lround(opt.offset_x / 25.4 * dpi_x));
    origin_y += int(std::lround(opt.offset_y / 25.4 * dpi_y));
    FPDF_PAGE page = FPDF_LoadPage(doc.doc, i);
    if (!page) return Error(3, L"Cannot load PDF page");
    HDC render_dc = printer.dc;
    bool emf = !opt.emf.empty();
    if (emf) {
      RECT frame = {0, 0, LONG(std::lround(w * 100)), LONG(std::lround(h * 100))};
      render_dc = CreateEnhMetaFileW(printer.dc, opt.emf.c_str(), &frame, L"yiz-print-pdf\0Diagnostic page\0\0");
      if (!render_dc) { FPDF_ClosePage(page); return Error(5, L"Cannot create EMF", GetLastError()); }
    } else {
      if (!printer.started) {
        DOCINFOW info = {}; info.cbSize = sizeof(info); info.lpszDocName = opt.file.c_str();
        job = StartDocW(printer.dc, &info);
        if (job <= 0) { FPDF_ClosePage(page); return Error(5, L"StartDoc failed", GetLastError()); }
        printer.started = true;
      }
      if (StartPage(printer.dc) <= 0) { FPDF_ClosePage(page); return Error(5, L"StartPage failed", GetLastError()); }
    }
    YizResetTextStats();
    // EMF coordinates start at physical paper origin; printer DC at printable origin.
    FPDF_RenderPage(render_dc, page, origin_x + (emf ? left : 0), origin_y + (emf ? top : 0), px, py, 0, FPDF_PRINTING | FPDF_ANNOT);
    FPDF_ClosePage(page);
    auto stats = YizGetTextStats();
    total_glyphs += stats.glyphs; total_fallbacks += stats.fallbacks;
    Capture(true, "page=%d dpi=%dx%d scale_mode=%s scale=%.4f%% pdf=%.3fx%.3fmm requested_paper=%.2fx%.2fmm physical=%.3fx%.3fmm printable=%.3fx%.3fmm margins_ltrb=%.3f,%.3f,%.3f,%.3fmm origin_dc=%d,%dpx render=%dx%dpx center=%d offset=%.3f,%.3fmm clipped=%d alternate_calls=%u glyphs=%u fallbacks=%u errors=%u\n",
      i+1, dpi_x, dpi_y, Utf8(opt.scale_mode).c_str(), factor * 100, sizes[i].width * 25.4 / 72, sizes[i].height * 25.4 / 72, w, h,
      physical_w * 25.4 / dpi_x, physical_h * 25.4 / dpi_y, printable_w * 25.4 / dpi_x, printable_h * 25.4 / dpi_y,
      left * 25.4 / dpi_x, top * 25.4 / dpi_y, (physical_w-left-printable_w) * 25.4 / dpi_x, (physical_h-top-printable_h) * 25.4 / dpi_y,
      origin_x, origin_y, px, py, opt.center, opt.offset_x, opt.offset_y,
      origin_x < 0 || origin_y < 0 || origin_x + px > printable_w || origin_y + py > printable_h,
      stats.calls, stats.glyphs, stats.fallbacks, stats.errors);
    if (emf) {
      HENHMETAFILE meta = CloseEnhMetaFile(render_dc);
      if (!meta) return Error(5, L"CloseEnhMetaFile failed", GetLastError());
      DeleteEnhMetaFile(meta);
    }
    if (stats.errors || (opt.strict && (stats.fallbacks || !stats.glyphs))) return Error(6, L"Alternate text failed or strict mode detected fallback; job aborted");
    if (!emf && EndPage(printer.dc) <= 0) return Error(5, L"EndPage failed", GetLastError());
  }
  if (printer.started) {
    if (EndDoc(printer.dc) <= 0) return Error(5, L"EndDoc failed", GetLastError());
    printer.started = false;
  }
  Capture(false, "{\"status\":\"%s\",\"jobId\":%d,\"pages\":%d,\"textMode\":\"%s\",\"alternateGlyphs\":%u,\"fallbackRuns\":%u}\n", opt.emf.empty() ? "submitted" : "emf_saved", job, render_count, opt.alternate ? "alternate" : "original", total_glyphs, total_fallbacks);
  return 0;
}

CoreResult RunCore(int argc, const wchar_t* const* argv, const void* memory, size_t length) {
  static SRWLOCK lock = SRWLOCK_INIT;
  struct Guard {
    SRWLOCK* lock;
    explicit Guard(SRWLOCK* value) : lock(value) { AcquireSRWLockExclusive(lock); }
    ~Guard() { output = nullptr; ReleaseSRWLockExclusive(lock); }
  } guard(&lock);
  CoreResult result;
  output = &result;
  try { result.code = Execute(argc, argv, memory, length); }
  catch (...) {
    result.code = 7;
    result.json = "{\"status\":\"failed\",\"code\":7,\"message\":\"Internal native exception\"}\n";
  }
  return result;
}
