#include "DemoSupport.h"

#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>

using namespace textflow;

void TimingStats::add(double sampleMicroseconds) {
  microseconds.push_back(sampleMicroseconds);
}

void TimingStats::report(const char *label) const {
  if (microseconds.empty())
    return;
  std::vector<double> sorted = microseconds;
  std::sort(sorted.begin(), sorted.end());
  const double meanMicroseconds =
      std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
  const double medianMicroseconds = sorted[sorted.size() / 2];
  const double percentile95Microseconds =
      sorted[static_cast<size_t>(sorted.size() * 0.95)];
  const double maximumMicroseconds = sorted.back();
  std::printf("  %-28s mean %7.1f us   p50 %7.1f us   p95 %7.1f us   max "
              "%7.1f us   (%zu frames)\n",
              label, meanMicroseconds, medianMicroseconds,
              percentile95Microseconds, maximumMicroseconds, sorted.size());
}

void writePng(SkSurface *surface, const std::filesystem::path &path) {
  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap))
    return;
  SkFILEWStream stream(path.string().c_str());
  if (stream.isValid())
    SkPngEncoder::Encode(&stream, pixmap, {});
}

double toMicroseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

TextStyle style(float fontSize, SkColor color, const char *languageTag) {
  TextStyle textStyle;
  textStyle.shaping.fontSize = fontSize;
  textStyle.shaping.languageTag = languageTag;
  textStyle.paint.foreground.setColor(color);
  return textStyle;
}

Paragraph makeBigParagraph(int wordCount, float fontSize) {
  const char8_t *latin[] = {u8"the",    u8"letters", u8"fall",   u8"away",
                            u8"from",   u8"their",   u8"lines",  u8"and",
                            u8"return", u8"again",   u8"layout", u8"engine",
                            u8"words",  u8"measure", u8"glyph",  u8"cascade",
                            u8"gentle", u8"steady",  u8"rhythm", u8"flowing"};
  const char8_t *cjk[] = {u8"文字", u8"雨",   u8"波紋", u8"字形",
                          u8"빗물", u8"글자", u8"물결", u8"여울",
                          u8"漣漪", u8"文雨", u8"字落", u8"縦横"};
  const TextStyle styles[3] = {style(fontSize, kInk), style(fontSize, kBlue),
                               style(fontSize, kAccent)};
  std::mt19937 randomEngine(23);
  Paragraph paragraph;
  std::u8string chunk;
  int chunkStyle = 0;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (wordIndex % 5 == 4)
      chunk += cjk[randomEngine() % 12];
    else
      chunk += latin[randomEngine() % 20];
    chunk += ' ';
    if (wordIndex % 120 == 119 || wordIndex + 1 == wordCount) {
      paragraph.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return paragraph;
}
