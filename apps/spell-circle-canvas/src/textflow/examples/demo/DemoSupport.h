#pragma once

// Shared helpers used by every demo scene (src/text/demo/Scene*.cpp).
// The palette, style shorthand, filler paragraph, and timing conversion come
// from TextFlowKit; this header keeps only what is demo-specific — frame
// statistics reporting and PNG output. Nothing here is scene-specific;
// scene-specific state lives in each scene's own file.

#include <textflow/TextFlow.h>
#include <textflowkit/TextFlowKit.h>

#include <include/core/SkSurface.h>

#include <chrono>
#include <filesystem>
#include <vector>

using Clock = std::chrono::steady_clock;

inline constexpr SkColor kInk = textflowkit::palette::kInk;
inline constexpr SkColor kAccent = textflowkit::palette::kAccent;
inline constexpr SkColor kBlue = textflowkit::palette::kBlue;
inline constexpr SkColor kShape = textflowkit::palette::kShape;
inline constexpr SkColor kPaper = textflowkit::palette::kPaper;

using textflowkit::toMicroseconds;

struct TimingStats {
  std::vector<double> microseconds;

  /** Records one sample in microseconds. */
  void add(double sampleMicroseconds);

  /** Prints mean and percentile statistics for all recorded samples. */
  void report(const char *label) const;
};

/** Writes a raster surface's pixels to a PNG file at `path`. */
void writePng(SkSurface *surface, const std::filesystem::path &path);

/** Creates a single-span style for demo paragraphs. */
inline textflow::TextStyle style(float fontSize, SkColor color = kInk,
                                 const char *languageTag = "") {
  return textflowkit::makeStyle(fontSize, color, languageTag);
}

/** ~wordCount words of mixed Latin/CJK in alternating color/style chunks. */
inline textflow::Paragraph makeBigParagraph(int wordCount, float fontSize) {
  return textflowkit::mixedScriptFiller(wordCount, fontSize);
}
