#pragma once

/** @file
 * What the Python sketch cases share: a scratch directory a case writes a
 * sketch's sources into, the smallest sketch that opens, the fixture a
 * case that bakes a 3D frame stands on, and two readings of what a session
 * drew.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include "ScratchDir.h"
#include "support/Devices.h"
#include "support/Fixtures.h"

namespace sigil::sketch::test {

class PythonSource {
 public:
  explicit PythonSource(std::string_view name) : scratch(name) {}

  void write(const std::string& name, std::string_view source) {
    scratch.write(name, source);
    // Edits differ below one second, so timestamp-based bytecode caches
    // would reuse stale code for same-size changes.
    std::filesystem::last_write_time(
        scratch.path / name, m_epoch + std::chrono::milliseconds(++m_edit));
  }

  [[nodiscard]] std::filesystem::path entry() const {
    return scratch.path / "entry.py";
  }

  [[nodiscard]] sketch::Host::Options options() const {
    sketch::Host::Options result;
    result.pythonLoader = &sketch::python::load;
    result.sketchPath = entry();
    result.assetsDirectory = scratch.path;
    result.siblingScanInterval = std::chrono::milliseconds(0);
    result.clock = sigil::motion::ClockPolicy::Advance;
    return result;
  }

  sigil::test::ScratchDir scratch;

 private:
  std::filesystem::file_time_type m_epoch = std::filesystem::file_time_type{
      std::chrono::duration_cast<std::chrono::seconds>(
          std::filesystem::file_time_type::clock::now().time_since_epoch())};
  int m_edit = 0;
};

inline constexpr std::string_view kGood =
    "class Study:\n"
    "    def setup(self, ctx):\n"
    "        ctx.canvas(120, 90)\n";

inline std::string familyOf(const SkTypeface* face) {
  SkString name;
  if (face) face->getFamilyName(&name);
  return name.c_str();
}

/** A CASE THAT BAKES A 3D FRAME from Python: the frame is drawn on the
 *  device, so the device runtime is installed for the process before it
 *  and taken away after, or the case skips naming what is missing. */
class SketchPythonBakes : public sketch::test::OnDeviceRuntime {};

/** One frame of @p session, @p seconds after the last, drawn into a
 *  raster canvas of @p width by @p height and read back. */
inline SkBitmap drawnFrame(sketch::Session& session, int width, int height,
                           double seconds) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  session.frame(*surface->getCanvas(), seconds);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  if (!surface->readPixels(pixels, 0, 0)) return {};
  return pixels;
}

}  // namespace sigil::sketch::test
