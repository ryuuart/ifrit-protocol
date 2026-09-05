#pragma once

// Shared helpers used by every demo scene (src/text/demo/Scene*.cpp).
// The palette, style shorthand, filler paragraph, and timing conversion come
// from SigilWeaveKit; this header keeps only what is demo-specific — PNG
// output. Nothing here is scene-specific; scene-specific state lives in
// each scene's own file. Frame statistics are SigilMeasure's
// (<sigilmeasure/stats/Samples.h>) when a scene wants them.

#include <include/core/SkSurface.h>
#include <sigilweave/SigilWeave.h>
#include <sigilweave/kit/SigilWeaveKit.h>

#include "Palette.h"

#include <chrono>
#include <filesystem>
#include <sigilmeasure/time/Stopwatch.h>

using Clock = std::chrono::steady_clock;

inline constexpr SkColor kInk = sigil::weave::examples::palette::kInk;
inline constexpr SkColor kAccent = sigil::weave::examples::palette::kAccent;
inline constexpr SkColor kBlue = sigil::weave::examples::palette::kBlue;
inline constexpr SkColor kShape = sigil::weave::examples::palette::kShape;
inline constexpr SkColor kPaper = sigil::weave::examples::palette::kPaper;

using sigil::measure::toMicroseconds;

/** Writes a raster surface's pixels to a PNG file at `path`. */
void writePng(SkSurface* surface, const std::filesystem::path& path);

/** Creates a single-span style for demo paragraphs. */
inline sigil::weave::TextStyle style(float fontSize, SkColor color = kInk,
                                     const char* languageTag = "") {
  return sigil::weave::kit::makeStyle(fontSize, color, languageTag);
}

/** ~wordCount words of mixed Latin/CJK in alternating color/style chunks. */
inline sigil::weave::Paragraph makeBigParagraph(int wordCount, float fontSize) {
  return sigil::weave::kit::mixedScriptFiller(wordCount, fontSize);
}
