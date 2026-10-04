#pragma once

/** @file
 * What Grimoire's own cases stand on: the one font context and asset
 * store a test process opens a session with, the pixels a surface holds,
 * and a directory on disk a case works in.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/core/Assets.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <unistd.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace grimoire::test {

/** THE PROCESS'S FONT CONTEXT. A context memoises shaping and strike
 *  lookups, so one per process is what a case wants. Never destroyed:
 *  Skia's font manager outlives static destruction on some platforms. */
inline sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** The store a sketch that reaches for nothing is opened with: a root
 *  that names no directory, so every ask answers the placeholder. Never
 *  destroyed, because it outlives every session opened over it. */
inline sigil::sketch::Assets& assets() {
  static auto* store = new sigil::sketch::Assets("");
  return *store;
}

/** What @p surface holds now; empty when it cannot be read. */
inline SkBitmap plateOf(SkSurface& surface) {
  SkBitmap bitmap;
  bitmap.allocPixels(surface.imageInfo());
  if (!surface.readPixels(bitmap.pixmap(), 0, 0)) bitmap.reset();
  return bitmap;
}

/** A DIRECTORY THIS CASE OWNS, emptied on the way in and removed on the
 *  way out. Its name carries the label and the process id, so two runs
 *  side by side never share one. */
class ScratchDir {
 public:
  explicit ScratchDir(std::string_view label)
      : path(std::filesystem::temp_directory_path() /
             (std::string(label) + "_" + std::to_string(::getpid()))) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
    std::filesystem::create_directories(path, error);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  std::filesystem::path path;
};

}  // namespace grimoire::test
