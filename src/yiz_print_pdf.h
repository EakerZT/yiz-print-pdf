#ifndef YIZ_PRINT_PDF_H
#define YIZ_PRINT_PDF_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#ifdef YIZ_PRINT_PDF_STATIC
#define YIZ_API
#else
#define YIZ_API __declspec(dllimport)
#endif
#define YIZ_CALL __cdecl
#ifdef __cplusplus
extern "C" {
#endif
#define YIZ_ABI_VERSION 1
enum YizScaleMode { YIZ_SCALE_PERCENT = 0, YIZ_SCALE_FIT = 1, YIZ_SCALE_SHRINK = 2 };
enum YizPaperMode { YIZ_PAPER_DRIVER = 0, YIZ_PAPER_PDF = 1 };
enum YizTextMode { YIZ_TEXT_ALTERNATE = 0, YIZ_TEXT_ORIGINAL = 1 };
enum YizOperation { YIZ_PRINT = 0, YIZ_INSPECT = 1, YIZ_EMF = 2 };
enum YizError { YIZ_OK = 0, YIZ_ARGUMENT = 2, YIZ_PDF = 3, YIZ_PRINTER = 4,
  YIZ_WINDOWS = 5, YIZ_ALTERNATE = 6, YIZ_INTERNAL = 7 };
#pragma pack(push, 8)
typedef struct YizPrintOptions {
  uint32_t struct_size;
  uint32_t abi_version;
  const wchar_t* printer; /* UTF-16 system printer name; not an alias. */
  const wchar_t* emf_path; /* Required for YIZ_EMF. No spool job is created. */
  int32_t scale_mode, paper_mode, text_mode, operation;
  int32_t center, require_alternate; /* 0 or 1 */
  double scale_percent; /* 10..200; used only by YIZ_SCALE_PERCENT */
  double width_mm, height_mm; /* both zero (default) or both >0..3000 */
  double offset_x_mm, offset_y_mm; /* -3000..3000 */
} YizPrintOptions;
#pragma pack(pop)
typedef struct YizResult YizResult;
/* All calls are synchronous. PDF operations are serialized inside this module.
 * Input pointers remain caller-owned and must stay valid/unmodified until return.
 * No callbacks; no stdout/stderr output. Do not invoke from DllMain.
 * Result UTF-8 strings remain valid until YizFreeResult; never free them yourself.
 * Return 0 means submitted (PRINT), inspected, or saved (EMF), not paper completion.
 * A failed job may already have emitted pages. Never automatically retry. */
YIZ_API uint32_t YIZ_CALL YizGetAbiVersion(void);
YIZ_API int32_t YIZ_CALL YizInitOptions(YizPrintOptions* options, uint32_t size);
YIZ_API int32_t YIZ_CALL YizPrintFile(const wchar_t* path, const YizPrintOptions* options, YizResult** result);
YIZ_API int32_t YIZ_CALL YizPrintMemory(const void* data, size_t length, const YizPrintOptions* options, YizResult** result);
YIZ_API int32_t YIZ_CALL YizListPrinters(YizResult** result);
YIZ_API const char* YIZ_CALL YizResultJson(const YizResult* result);
YIZ_API const char* YIZ_CALL YizResultLogs(const YizResult* result);
YIZ_API void YIZ_CALL YizFreeResult(YizResult* result);
#ifdef __cplusplus
}
#endif
#endif
