#pragma once
#include <windows.h>
#include <stdint.h>
#include "core/fxcrt/fx_coordinates.h"
#include "third_party/base/span.h"
class CFX_Font;
class TextCharPos;
struct YizTextStats {
  unsigned calls = 0;
  unsigned glyphs = 0;
  unsigned fallbacks = 0;
  unsigned errors = 0;
};
void YizSetAlternateText(bool enabled);
void YizResetTextStats();
YizTextStats YizGetTextStats();
void YizReleaseFonts();
bool YizDrawDeviceText(HDC dc, pdfium::span<const TextCharPos> chars,
                      CFX_Font* font, const CFX_Matrix& matrix,
                      float size, uint32_t color);
