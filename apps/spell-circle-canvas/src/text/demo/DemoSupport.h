#pragma once

// Shared helpers used by every demo scene (src/text/demo/Scene*.cpp): the
// shared palette, frame-timing statistics, PNG output, and the mixed
// Latin/CJK filler paragraph reused by the rain and ripple scenes. Nothing
// here is scene-specific; scene-specific state lives in each scene's own
// file.

#include <textflow/TextFlow.h>

#include <include/core/SkSurface.h>

#include <chrono>
#include <filesystem>
#include <vector>

using Clock = std::chrono::steady_clock;

inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;

struct TimingStats {
  std::vector<double> microseconds;

  /** Records one sample in microseconds. */
  void add(double sampleMicroseconds);

  /** Prints mean and percentile statistics for all recorded samples. */
  void report(const char *label) const;
};

/** Writes a raster surface's pixels to a PNG file at `path`. */
void writePng(SkSurface *surface, const std::filesystem::path &path);

/** Converts a steady-clock duration to the demo's reporting unit. */
double toMicroseconds(Clock::duration duration);

/** Creates a single-span style for demo paragraphs. */
textflow::TextStyle style(float fontSize, SkColor color = kInk,
                         const char *languageTag = "");

/** ~wordCount words of mixed Latin/CJK in alternating color/style chunks. */
textflow::Paragraph makeBigParagraph(int wordCount, float fontSize);
