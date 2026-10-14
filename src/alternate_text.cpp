// Original implementation of the QZ-style device-text approach; no Java code copied.
#include "alternate_text.h"
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/fxge/cfx_font.h"
#include "core/fxge/text_char_pos.h"

namespace {
bool enabled = false;
YizTextStats stats;
uint32_t sequence = 0;
uint16_t U16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint32_t U32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
void W16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }
void W32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }
uint32_t Checksum(const std::vector<uint8_t>& data, size_t offset, size_t size) {
  uint32_t sum = 0;
  for (size_t i = 0; i < size; i += 4) {
    uint32_t word = 0;
    for (size_t j = 0; j < 4; ++j) word = (word << 8) | (i + j < size ? data[offset + i + j] : 0);
    sum += word;
  }
  return sum;
}
struct FontEntry {
  std::vector<uint8_t> bytes;
  HANDLE registration = nullptr;
  HFONT handle = nullptr;
  uint16_t glyph_count = 0;
  ~FontEntry() {
    if (handle) DeleteObject(handle);
    if (registration) RemoveFontMemResourceEx(registration);
  }
};
std::map<CFX_Font*, std::unique_ptr<FontEntry>> fonts;

// Give the embedded font a unique private name. Otherwise GDI can select the
// system's full SimSun instead of the PDF subset, whose glyph IDs differ.
bool RenameTrueType(FontEntry& entry, const std::wstring& name) {
  auto& bytes = entry.bytes;
  if (bytes.size() < 12 || U32(bytes.data()) != 0x00010000) return false;
  size_t count = U16(bytes.data() + 4);
  if (12 + count * 16 > bytes.size()) return false;
  size_t name_record = 0, head = 0;
  bool has_glyf = false;
  for (size_t i = 0; i < count; ++i) {
    size_t r = 12 + i * 16;
    uint32_t off = U32(bytes.data() + r + 8), len = U32(bytes.data() + r + 12);
    if (off > bytes.size() || len > bytes.size() - off) return false;
    uint32_t tag = U32(bytes.data() + r);
    if (tag == 0x6e616d65) name_record = r;
    if (tag == 0x68656164 && len >= 12) head = off;
    if (tag == 0x676c7966) has_glyf = true;
    if (tag == 0x6d617870 && len >= 6) entry.glyph_count = U16(bytes.data() + off + 4);
  }
  if (!name_record || !head || !has_glyf || !entry.glyph_count) return false;
  // Windows Unicode names: family, subfamily, unique ID, full, version, PS name.
  const uint16_t ids[] = {1, 2, 3, 4, 5, 6, 16, 17};
  std::vector<uint8_t> table(6 + 8 * 12);
  W16(table.data() + 2, 8);
  W16(table.data() + 4, uint16_t(table.size()));
  for (size_t i = 0; i < 8; ++i) {
    std::wstring value = (ids[i] == 2 || ids[i] == 17) ? L"Regular" : name;
    size_t offset = table.size() - (6 + 8 * 12);
    size_t r = 6 + i * 12;
    W16(table.data() + r, 3); W16(table.data() + r + 2, 1);
    W16(table.data() + r + 4, 0x0409); W16(table.data() + r + 6, ids[i]);
    W16(table.data() + r + 8, uint16_t(value.size() * 2));
    W16(table.data() + r + 10, uint16_t(offset));
    for (wchar_t c : value) { table.push_back(uint8_t(c >> 8)); table.push_back(uint8_t(c)); }
  }
  while (bytes.size() % 4) bytes.push_back(0);
  size_t offset = bytes.size();
  bytes.insert(bytes.end(), table.begin(), table.end());
  while (bytes.size() % 4) bytes.push_back(0);
  W32(bytes.data() + name_record + 4, Checksum(table, 0, table.size()));
  W32(bytes.data() + name_record + 8, uint32_t(offset));
  W32(bytes.data() + name_record + 12, uint32_t(table.size()));
  W32(bytes.data() + head + 8, 0);
  W32(bytes.data() + head + 8, 0xb1b0afba - Checksum(bytes, 0, bytes.size()));
  return true;
}

FontEntry* GetFont(CFX_Font* font) {
  auto found = fonts.find(font);
  if (found != fonts.end()) return found->second.get();
  auto item = std::make_unique<FontEntry>();
  auto data = font->GetFontSpan();
  item->bytes.assign(data.begin(), data.end());
  wchar_t name[LF_FACESIZE];
  swprintf_s(name, L"YizPdf%08lx%08lx", GetCurrentProcessId(), ++sequence);
  if (RenameTrueType(*item, name)) {
    DWORD count = 0;
    item->registration = AddFontMemResourceEx(item->bytes.data(), DWORD(item->bytes.size()), nullptr, &count);
    if (item->registration) {
      LOGFONTW lf = {};
      lf.lfHeight = -1000;
      lf.lfWeight = FW_DONTCARE;
      lf.lfCharSet = DEFAULT_CHARSET;
      lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
      lf.lfQuality = NONANTIALIASED_QUALITY;
      wcscpy_s(lf.lfFaceName, name);
      item->handle = CreateFontIndirectW(&lf);
    }
  }
  // Unsupported fonts retain PDFium's normal renderer; never substitute a
  // different font while keeping the original font's glyph indices.
  if (!item->handle) item.reset();
  FontEntry* result = item.get();
  fonts.emplace(font, std::move(item));
  return result;
}
bool Fallback() { ++stats.fallbacks; return false; }
}

void YizSetAlternateText(bool value) { enabled = value; }
void YizResetTextStats() { stats = {}; }
YizTextStats YizGetTextStats() { return stats; }
void YizReleaseFonts() { fonts.clear(); }

bool YizDrawDeviceText(HDC dc, pdfium::span<const TextCharPos> chars,
                      CFX_Font* font, const CFX_Matrix& matrix,
                      float size, uint32_t color) {
  if (!enabled) return false;
  ++stats.calls;
  if (chars.empty()) return true;
  if ((color >> 24) != 255 || font->IsVertical() || !std::isfinite(size) || size == 0) return Fallback();
  FontEntry* entry = GetFont(font);
  if (!entry) return Fallback();
  for (const auto& ch : chars) {
    if (ch.m_GlyphIndex >= entry->glyph_count) return Fallback();
  }
  int saved = SaveDC(dc);
  if (!saved) return Fallback();
  if (!SetGraphicsMode(dc, GM_ADVANCED) || !SelectObject(dc, entry->handle)) {
    RestoreDC(dc, saved);
    return Fallback();
  }
  // Verify that GDI selected the registered subset, not a fallback system font.
  uint8_t maxp[6] = {};
  if (GetFontData(dc, 0x7078616d, 0, maxp, sizeof(maxp)) != sizeof(maxp) || U16(maxp + 4) != entry->glyph_count) {
    RestoreDC(dc, saved);
    return Fallback();
  }
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);
  SetTextColor(dc, RGB((color >> 16) & 255, (color >> 8) & 255, color & 255));
  for (const auto& ch : chars) {
    // GDI font coordinates have Y down; PDF text coordinates have Y up.
    CFX_Matrix flip(1, 0, 0, -1, 0, 0);
    CFX_Matrix glyph(size / 1000, 0, 0, size / 1000, ch.m_Origin.x, ch.m_Origin.y);
    glyph = ch.GetEffectiveMatrix(glyph);
    flip.Concat(glyph);
    flip.Concat(matrix);
    XFORM transform = {flip.a, flip.b, flip.c, flip.d, flip.e, flip.f};
    WORD index = WORD(ch.m_GlyphIndex);
    if (!SetWorldTransform(dc, &transform) || !ExtTextOutW(dc, 0, 0, ETO_GLYPH_INDEX, nullptr, reinterpret_cast<LPCWSTR>(&index), 1, nullptr)) {
      // Some glyphs may already have been sent. Do not fall back and draw the
      // whole run twice; CLI aborts this job after the page reports an error.
      ++stats.errors;
      RestoreDC(dc, saved);
      return true;
    }
    ++stats.glyphs;
  }
  RestoreDC(dc, saved);
  return true;
}
