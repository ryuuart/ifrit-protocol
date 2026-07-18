#include "DemoSupport.h"

#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include <algorithm>
#include <cstdio>
#include <numeric>

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
