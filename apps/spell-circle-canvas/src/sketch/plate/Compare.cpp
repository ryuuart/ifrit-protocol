/** @file Two plate directories, decoded and differenced channel by channel. */

#include "sigilsketch/plate/Compare.h"

#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/decode/Decode.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sigilsketch/plate/Sweep.h"

namespace sigil::sketch {

namespace {

/** One plate, as the bytes a PNG stores: 8-bit RGBA, unpremultiplied, in
 *  row order. Decoded rather than compared as files, because two
 *  renderers agree on a picture and never on an encoding. */
struct Plate {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
};

std::optional<Plate> readPlate(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  const std::vector<char> encoded((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
  if (encoded.empty()) return std::nullopt;
  const auto decoded =
      image::decodeImage(reinterpret_cast<const std::byte*>(encoded.data()),
                         encoded.size(), {}, path);
  if (!decoded) return std::nullopt;
  const sk_sp<SkImage> image = decoded->frameAt(0).image;
  if (!image) return std::nullopt;

  Plate plate;
  plate.width = image->width();
  plate.height = image->height();
  // Unpremultiplied, which is what a PNG holds: a plate is opaque, so
  // this is the identity, and asking for it keeps a plate that is not
  // from being compared through a rounding neither renderer performed.
  const SkImageInfo info = SkImageInfo::Make(
      plate.width, plate.height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  plate.pixels.resize((size_t)plate.width * plate.height * 4);
  const SkPixmap pixels(info, plate.pixels.data(), (size_t)plate.width * 4);
  if (!image->readPixels(nullptr, pixels, 0, 0)) return std::nullopt;
  return plate;
}

/** Mean, 99th percentile and worst absolute channel difference, in
 *  0..255, over every channel of every pixel — and the worst split THREE
 *  ways by what the pixel it stands on is.
 *
 *  The split is there because a caller's tolerance can depend on it. A
 *  picture drawn over transparent black and the same picture drawn over
 *  something are not composited the same number of times, so the second is
 *  allowed a rounding the first is not. And a differing pixel that sits on
 *  an ANTIALIASED EDGE IN BOTH PLATES is a third thing again: what changed
 *  there is one pixel's coverage of an edge both pictures draw, which is
 *  what a mark standing a fraction of a device pixel from where the other
 *  drew it looks like — where a picture that MOVED takes pixels off the
 *  edges, or takes whole marks away, and shows it by differing where
 *  neither plate has an edge at all.
 *
 *  Which pixels are which is a fact about the two files and is answered
 *  here; how much each is allowed is a judgement and is not. */
struct Distance {
  double mean = 0;
  int p99 = 0;
  int worst = 0;
  int worstOverClear = 0;    ///< where the first plate is transparent black
  int worstOverContent = 0;  ///< where it holds content and is not a graze
  int worstOverGraze = 0;    ///< …and where the difference is edge-confined
  size_t grazingPixels = 0;  ///< how many pixels that was
};

/** HOW MUCH THE PICTURE ITSELF VARIES WITHIN A PIXEL OF EACH POINT: the
 *  largest spread any one channel shows over the 3x3 neighbourhood, in
 *  0..255. A pixel in the middle of a flat wash reads 0; a pixel on an
 *  antialiased edge reads the contrast of the two things the edge is
 *  between, because the ramp from one to the other is inside its
 *  neighbourhood. */
std::vector<uint8_t> localSpan(const Plate& plate) {
  const int w = plate.width, h = plate.height;
  std::vector<uint8_t> span((size_t)w * h, 0);
  for (int y = 0; y < h; ++y) {
    const int y0 = std::max(0, y - 1), y1 = std::min(h - 1, y + 1);
    for (int x = 0; x < w; ++x) {
      const int x0 = std::max(0, x - 1), x1 = std::min(w - 1, x + 1);
      int worst = 0;
      for (size_t channel = 0; channel < 4; ++channel) {
        int lo = 255, hi = 0;
        for (int ny = y0; ny <= y1; ++ny)
          for (int nx = x0; nx <= x1; ++nx) {
            const int v = plate.pixels[(((size_t)ny * w) + nx) * 4 + channel];
            lo = std::min(lo, v);
            hi = std::max(hi, v);
          }
        worst = std::max(worst, hi - lo);
      }
      span[(size_t)y * w + x] = (uint8_t)worst;
    }
  }
  return span;
}

Distance distanceBetween(const Plate& first, const Plate& second) {
  std::array<size_t, 256> histogram{};
  const size_t count = std::min(first.pixels.size(), second.pixels.size());
  for (size_t at = 0; at < count; ++at)
    ++histogram[(size_t)std::abs((int)first.pixels[at] -
                                 (int)second.pixels[at])];

  Distance distance;
  if (count == 0) return distance;
  size_t total = 0;
  for (size_t value = 0; value < histogram.size(); ++value)
    total += value * histogram[value];
  distance.mean = (double)total / (double)count;
  const double cut = (double)count * 0.99;
  size_t seen = 0;
  bool foundP99 = false;
  for (size_t value = 0; value < histogram.size(); ++value) {
    if (histogram[value] != 0) distance.worst = (int)value;
    seen += histogram[value];
    if (!foundP99 && (double)seen >= cut) {
      distance.p99 = (int)value;
      foundP99 = true;
    }
  }

  // The same differences again, gathered PER PIXEL rather than per channel,
  // because what a pixel stands on is a property of all four of its
  // channels together.
  const int w = first.width, h = first.height;
  const size_t pixels = (size_t)w * h;
  std::vector<uint8_t> moved(pixels, 0);
  for (size_t at = 0; at < pixels; ++at) {
    int worst = 0;
    for (size_t channel = 0; channel < 4; ++channel)
      worst = std::max(worst, std::abs((int)first.pixels[at * 4 + channel] -
                                       (int)second.pixels[at * 4 + channel]));
    moved[at] = (uint8_t)worst;
  }
  // A DIFFERENCE THE EDGE UNDER IT EXPLAINS. A pixel is edge-confined when
  // the picture varies by at least the difference within a pixel of it IN
  // BOTH PLATES: the two disagree about how much of an edge covers that
  // pixel, and both draw the edge. A pixel in a flat wash carries no such
  // variation, and a mark that is gone leaves none where it was, so
  // neither can be confined to an edge whatever its neighbours look like.
  const std::vector<uint8_t> spanFirst = localSpan(first);
  const std::vector<uint8_t> spanSecond = localSpan(second);
  std::vector<uint8_t> spilled(pixels, 0);
  for (size_t at = 0; at < pixels; ++at)
    spilled[at] = moved[at] > 0 &&
                  (spanFirst[at] < moved[at] || spanSecond[at] < moved[at]);
  for (int y = 0; y < h; ++y) {
    const int y0 = std::max(0, y - 1), y1 = std::min(h - 1, y + 1);
    for (int x = 0; x < w; ++x) {
      const size_t at = (size_t)y * w + x;
      if (moved[at] == 0) continue;
      const bool clear =
          first.pixels[at * 4] == 0 && first.pixels[at * 4 + 1] == 0 &&
          first.pixels[at * 4 + 2] == 0 && first.pixels[at * 4 + 3] == 0;
      if (clear) {
        distance.worstOverClear =
            std::max(distance.worstOverClear, (int)moved[at]);
        continue;
      }
      // …and CONFINED TO A RUN OF THEM: a difference that reaches a pixel
      // no edge explains is that difference, wherever else it also lands,
      // so a graze is a pixel whose neighbourhood holds no such pixel.
      bool confined = true;
      const int x0 = std::max(0, x - 1), x1 = std::min(w - 1, x + 1);
      for (int ny = y0; ny <= y1 && confined; ++ny)
        for (int nx = x0; nx <= x1; ++nx)
          if (spilled[(size_t)ny * w + nx]) {
            confined = false;
            break;
          }
      if (confined) {
        distance.worstOverGraze =
            std::max(distance.worstOverGraze, (int)moved[at]);
        ++distance.grazingPixels;
      } else {
        distance.worstOverContent =
            std::max(distance.worstOverContent, (int)moved[at]);
      }
    }
  }
  return distance;
}

/** Every plate name a directory holds, in the order the filesystem is
 *  walked; the caller merges two of these into one sorted set, so a
 *  report reads the same however either directory is enumerated. */
void collect(const std::filesystem::path& dir, std::map<std::string, int>& into,
             int side) {
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    const std::filesystem::path& path = entry.path();
    if (path.extension() != ".png") continue;
    const std::string stem = path.stem().string();
    if (stem.rfind(kPlatePrefix, 0) != 0) continue;
    into[stem.substr(kPlatePrefix.size())] |= side;
  }
}

}  // namespace

int compare(const CompareOptions& options) {
  const std::filesystem::path first(options.first);
  const std::filesystem::path second(options.second);
  std::error_code ec;
  if (!std::filesystem::is_directory(first, ec) ||
      !std::filesystem::is_directory(second, ec)) {
    std::fprintf(stderr, "--compare wants two directories of plates\n");
    return 2;
  }

  std::map<std::string, int> plates;
  collect(first, plates, 1);
  collect(second, plates, 2);
  if (plates.empty()) {
    std::fprintf(stderr, "neither directory holds a plate\n");
    return 2;
  }

  int verdict = 0;
  for (const auto& [name, sides] : plates) {
    const std::string file = std::string(kPlatePrefix) + name + ".png";
    if ((sides & 1) == 0 || (sides & 2) == 0) {
      std::printf("missing %s %s\n", name.c_str(),
                  (sides & 1) == 0 ? "first" : "second");
      verdict = 1;
      continue;
    }
    const std::optional<Plate> a = readPlate(first / file);
    const std::optional<Plate> b = readPlate(second / file);
    if (!a || !b) {
      std::printf("unreadable %s %s\n", name.c_str(), a ? "second" : "first");
      verdict = 1;
      continue;
    }
    if (a->width != b->width || a->height != b->height) {
      std::printf("size %s %dx%d %dx%d\n", name.c_str(), a->width, a->height,
                  b->width, b->height);
      verdict = 1;
      continue;
    }
    const Distance distance = distanceBetween(*a, *b);
    std::printf(
        "compared %s mean %.4f p99 %d max %d clear %d content %d graze %d "
        "%zu\n",
        name.c_str(), distance.mean, distance.p99, distance.worst,
        distance.worstOverClear, distance.worstOverContent,
        distance.worstOverGraze, distance.grazingPixels);
  }
  std::fflush(stdout);
  return verdict;
}

}  // namespace sigil::sketch
