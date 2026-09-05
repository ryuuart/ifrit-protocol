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
  const auto decoded = image::decodeImage(
      reinterpret_cast<const std::byte*>(encoded.data()), encoded.size(), {},
      path);
  if (!decoded) return std::nullopt;
  const sk_sp<SkImage> image = decoded->frameAt(0).image;
  if (!image) return std::nullopt;

  Plate plate;
  plate.width = image->width();
  plate.height = image->height();
  // Unpremultiplied, which is what a PNG holds: a plate is opaque, so
  // this is the identity, and asking for it keeps a plate that is not
  // from being compared through a rounding neither renderer performed.
  const SkImageInfo info = SkImageInfo::Make(plate.width, plate.height,
                                             kRGBA_8888_SkColorType,
                                             kUnpremul_SkAlphaType);
  plate.pixels.resize((size_t)plate.width * plate.height * 4);
  const SkPixmap pixels(info, plate.pixels.data(), (size_t)plate.width * 4);
  if (!image->readPixels(nullptr, pixels, 0, 0)) return std::nullopt;
  return plate;
}

/** Mean, 99th percentile and worst absolute channel difference, in
 *  0..255, over every channel of every pixel. */
struct Distance {
  double mean = 0;
  int p99 = 0;
  int worst = 0;
};

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
    std::printf("compared %s mean %.4f p99 %d max %d\n", name.c_str(),
                distance.mean, distance.p99, distance.worst);
  }
  std::fflush(stdout);
  return verdict;
}

}  // namespace sigil::sketch
