// Scene J — CJK font-fallback coverage. Noto Sans and Noto Serif are useful
// incomplete primary families on this machine: their Latin faces force the
// Japanese, Korean, Simplified Chinese, and Traditional Chinese clauses
// through FontContext's fallback path. The scene deliberately makes no claim
// about which fallback family is correct; that policy belongs to the supplied
// SkFontMgr or FontContext::FallbackResolver.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>

#include <cstdio>
#include <set>
#include <string>

using namespace textflow;

namespace {

struct Row {
  const char *languageTag;
  const char *text; // Latin lead-in + a CJK clause requiring fallback
};

const Row kRows[] = {
    {"ja", "Japanese falls back to 日本語のテキスト for this sentence."},
    {"ko", "Korean falls back to 한국어 텍스트 for this sentence."},
    {"zh-Hans", "Simplified Chinese falls back to 简体中文文本 here."},
    {"zh-Hant", "Traditional Chinese falls back to 繁體中文文本 here."},
};

struct CoverageCheck {
  bool sawFallback = false;
  bool allGlyphsResolved = true;
  std::set<std::string> fallbackFamilies;
};

CoverageCheck checkFallbackCoverage(const Paragraph &paragraph,
                                    const SkTypeface &primaryTypeface) {
  CoverageCheck result;
  for (const Word &word : paragraph.words()) {
    for (const WordSegment &segment : word.segments) {
      const SkTypeface *resolved = segment.shaped->typeface.get();
      if (resolved && resolved != &primaryTypeface) {
        result.sawFallback = true;
        SkString family;
        resolved->getFamilyName(&family);
        result.fallbackFamilies.insert(family.c_str());
      }
      for (uint16_t glyph : segment.shaped->glyphs)
        result.allGlyphsResolved &= glyph != 0;
    }
  }
  return result;
}

std::string joinedFamilies(const std::set<std::string> &families) {
  std::string joined;
  for (const std::string &family : families) {
    if (!joined.empty())
      joined += ", ";
    joined += family;
  }
  return joined.empty() ? "(none)" : joined;
}

} // namespace

void sceneFallback(FontContext &fontContext,
                   const std::filesystem::path &outputDirectory) {
  std::printf("Scene J — CJK fallback coverage (policy-neutral)\n");

  SkFontMgr *fontManager = fontContext.fontManager();
  sk_sp<SkTypeface> sansTypeface =
      fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
  sk_sp<SkTypeface> serifTypeface =
      fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());
  if (!sansTypeface || !serifTypeface) {
    std::printf("  Noto Sans / Noto Serif missing, skipped\n\n");
    return;
  }

  constexpr int kRowCount = sizeof(kRows) / sizeof(kRows[0]);
  constexpr float kColumnWidth = 460;
  constexpr float kRowHeight = 130;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(80 + 2 * kColumnWidth,
                                 60 + kRowCount * kRowHeight));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  auto drawCaption = [&](const char *text, float left, float top) {
    Paragraph caption;
    caption.appendText(text, style(13, kAccent));
    BlockFlow flow(SkRect::MakeXYWH(left, top, kColumnWidth, 18));
    layoutParagraph(fontContext, caption, flow).draw(canvas, caption);
  };
  drawCaption("Noto Sans primary", 40, 16);
  drawCaption("Noto Serif primary", 40 + kColumnWidth, 16);

  bool allRowsResolved = true;
  for (int rowIndex = 0; rowIndex < kRowCount; ++rowIndex) {
    const Row &row = kRows[rowIndex];
    const float top = 40 + static_cast<float>(rowIndex) * kRowHeight;

    TextStyle sansStyle = style(17, kInk, row.languageTag);
    sansStyle.shaping.typeface = sansTypeface;
    Paragraph sansParagraph;
    sansParagraph.appendText(row.text, sansStyle);
    BlockFlow sansFlow(SkRect::MakeXYWH(40, top, kColumnWidth, kRowHeight - 12));
    layoutParagraph(fontContext, sansParagraph, sansFlow)
        .draw(canvas, sansParagraph);

    TextStyle serifStyle = style(17, kInk, row.languageTag);
    serifStyle.shaping.typeface = serifTypeface;
    Paragraph serifParagraph;
    serifParagraph.appendText(row.text, serifStyle);
    BlockFlow serifFlow(SkRect::MakeXYWH(40 + kColumnWidth, top, kColumnWidth,
                                         kRowHeight - 12));
    layoutParagraph(fontContext, serifParagraph, serifFlow)
        .draw(canvas, serifParagraph);

    const CoverageCheck sansCheck =
        checkFallbackCoverage(sansParagraph, *sansTypeface);
    const CoverageCheck serifCheck =
        checkFallbackCoverage(serifParagraph, *serifTypeface);
    const bool rowOk = sansCheck.sawFallback && sansCheck.allGlyphsResolved &&
                       serifCheck.sawFallback && serifCheck.allGlyphsResolved;
    allRowsResolved &= rowOk;
    std::printf("  %-8s %s\n", row.languageTag, rowOk ? "PASS" : "FAIL");
    std::printf("    sans primary -> %s\n",
                joinedFamilies(sansCheck.fallbackFamilies).c_str());
    std::printf("    serif primary -> %s\n",
                joinedFamilies(serifCheck.fallbackFamilies).c_str());
  }
  std::printf("  overall: %s\n\n", allRowsResolved ? "PASS" : "FAIL");

  writePng(surface.get(), outputDirectory / "fallback_cjk.png");
}
