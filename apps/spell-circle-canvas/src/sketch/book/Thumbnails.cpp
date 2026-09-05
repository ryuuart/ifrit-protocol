/** @file
 * The thumbnail store and the CPU still that fills it.
 */

#include "Thumbnails.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/core/CanvasSpec.h>
#include <sigilsketch/core/Kind.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Host.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
namespace image = sigil::image;
namespace io = sigil::io;

namespace sigil::sketch::book {

namespace {

/** The one fixed rate the plate sweep walks at, so a thumbnail is the
 *  same picture the plate tier captures. */
constexpr double kRate = 60.0;
constexpr double kStep = 1.0 / kRate;

/** The scene time a sketch that names none is captured at — the sweep's
 *  own derived default, so an undeclaring sketch's thumbnail and plate
 *  land on the same frame. */
constexpr double kDefaultMoment = 6.0;

/** The separator between a stem and its key in a thumbnail's filename.
 *  A stem cannot contain it, so a glob for `<stem>__` finds exactly this
 *  stem's thumbnails whatever key each carries. */
constexpr std::string_view kKeyMark = "__";

void hashInto(std::uint64_t& seed, std::uint64_t value) {
  // A plain mixing step — the key only has to differ when the source or
  // the host does, not to resist anything.
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

void hashFile(std::uint64_t& seed, const fs::path& file) {
  std::error_code ec;
  const auto size = fs::file_size(file, ec);
  hashInto(seed, ec ? 0 : (std::uint64_t)size);
  const auto when = fs::last_write_time(file, ec);
  hashInto(seed, ec ? 0 : (std::uint64_t)when.time_since_epoch().count());
  hashInto(seed, std::hash<std::string>{}(file.filename().string()));
}

}  // namespace

std::string thumbnailKey(const fs::path& entrySource) {
  std::uint64_t seed = 0;
  // The host's build identity first: new drawing code is a new picture,
  // whatever the sources did.
  hashInto(seed,
           (std::uint64_t)hostBinaryTime().time_since_epoch().count());
  if (directorySketch(entrySource)) {
    // Every source beside the entry: a directory sketch is built from all
    // of them, so a still is stale when any of them changed.
    std::error_code ec;
    std::vector<fs::path> files;
    for (auto it = fs::directory_iterator(entrySource.parent_path(), ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
      const fs::path& p = it->path();
      const fs::path ext = p.extension();
      if (ext == ".cpp" || ext == ".h" || ext == ".hpp") files.push_back(p);
    }
    std::sort(files.begin(), files.end());  // directory order is not stable
    for (const fs::path& file : files) hashFile(seed, file);
  } else {
    hashFile(seed, entrySource);
  }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)seed);
  return hex;
}

fs::path thumbnailFile(const fs::path& dir, std::string_view stem,
                       std::string_view key) {
  return dir / (std::string(stem) + std::string(kKeyMark) + std::string(key) +
                ".png");
}

fs::path freshThumbnail(const fs::path& dir, std::string_view stem,
                        std::string_view key) {
  if (dir.empty()) return {};
  const fs::path file = thumbnailFile(dir, stem, key);
  std::error_code ec;
  return fs::exists(file, ec) ? file : fs::path{};
}

namespace {

/** Removes the stale thumbnails of @p stem under @p dir — every
 *  `<stem>__*.png` that is not @p keep — so a source that changed does
 *  not leave a spent still behind. */
void pruneStale(const fs::path& dir, std::string_view stem,
                const fs::path& keep) {
  std::error_code ec;
  const std::string prefix = std::string(stem) + std::string(kKeyMark);
  for (auto it = fs::directory_iterator(dir, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    const fs::path& p = it->path();
    if (p == keep) continue;
    const std::string name = p.filename().string();
    if (name.rfind(prefix, 0) == 0 && p.extension() == ".png") {
      std::error_code rm;
      fs::remove(p, rm);
    }
  }
}

}  // namespace

bool renderThumbnail(const Entry& entry, weave::FontContext& fonts,
                     Assets& assets, const fs::path& out, int maxDimension) {
  const Kind kind = entry.kind();
  if (!kind) return false;
  // Deterministic, so a sketch that measured something about its own
  // execution pins it — a thumbnail is a picture that will be looked at
  // beside a plate, and the two must agree.
  std::unique_ptr<Session> session = kind->open(fonts, assets, true);
  if (!session) return false;
  // The plate tier renders with cost-based promotion held off; a
  // thumbnail must be that same picture.
  session->setAutoPromotion(false);

  const CanvasSpec& spec = session->canvas();
  const SkSize size = spec.size;
  if (size.width() <= 0 || size.height() <= 0) return false;
  const SkColor4f background = spec.background;

  // Step from zero to the sketch's declared moment on a working surface
  // its own size — the frame the plate tier photographs.
  const double moment =
      spec.captureSeconds > 0 ? spec.captureSeconds : kDefaultMoment;
  const int frames = std::max(0, (int)std::lround(moment * kRate));
  const SkImageInfo workInfo =
      SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height());
  sk_sp<SkSurface> work = SkSurfaces::Raster(workInfo);
  if (!work) return false;
  for (int f = 0; f < frames; ++f) {
    work->getCanvas()->clear(background);
    session->frame(*work->getCanvas(), kStep);
  }

  // The still, scaled so its larger side is maxDimension: a canvas
  // sketch re-renders here at the smaller scale, a set or a pen presents
  // the frame just finished.
  const float longest = std::max(size.width(), size.height());
  const float scale = std::min(1.0f, (float)maxDimension / longest);
  const SkImageInfo thumbInfo = SkImageInfo::MakeN32Premul(
      std::max(1, (int)std::lround(size.width() * scale)),
      std::max(1, (int)std::lround(size.height() * scale)));
  sk_sp<SkSurface> thumb = SkSurfaces::Raster(thumbInfo);
  if (!thumb) return false;
  thumb->getCanvas()->clear(background);
  thumb->getCanvas()->scale(scale, scale);
  session->still(*thumb->getCanvas());

  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(thumbInfo)) return false;
  if (!thumb->readPixels(bitmap.pixmap(), 0, 0)) return false;
  const sk_sp<SkData> png =
      image::encodeImage(bitmap.pixmap(), image::Format::Png);
  if (!png || !io::writeBytes(out, png->data(), png->size())) return false;
  pruneStale(out.parent_path(), out.stem().string().substr(
                                    0, out.stem().string().find(kKeyMark)),
             out);
  return true;
}

}  // namespace sigil::sketch::book
