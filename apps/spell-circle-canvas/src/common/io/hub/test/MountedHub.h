#pragma once

/** @file
 * What a hub needs before it can be asked anything, shared by every file
 * of the hub's cases: one scratch directory mounted at res://, the
 * fixture classes the suite names hang off, and the three helpers more
 * than one file needs — a decodable image whose size says which file it
 * came from, an mtime stamped forward rather than waited for, and a
 * lease's URIs as a comparable vector.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Sink.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "ScratchDir.h"

namespace sigil::io::test {

/** A size x size solid PNG, for tests that need a real decodable
 *  image whose dimensions identify which file (or which version of a
 *  file) a decode came from. */
inline void writePng(const std::filesystem::path& path, int size,
                     SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  ASSERT_TRUE(writeBytes(path, png->data(), png->size()));
}

/** Stamps @p path in the near future, so a reload that watches mtimes
 *  cannot mistake this write for the one it already saw. A filesystem's
 *  timestamp granularity can be coarser than the gap between two writes
 *  in one case; the stamp is set outright rather than waited for,
 *  because waiting would put somebody else's granularity into this
 *  test's running time. */
inline void touchForward(const std::filesystem::path& path) {
  std::filesystem::last_write_time(
      path,
      std::filesystem::file_time_type::clock::now() + std::chrono::seconds(2));
}

inline std::vector<std::string> leaseUris(const ResourceLease& lease) {
  return {lease.uris().begin(), lease.uris().end()};
}

}  // namespace sigil::io::test

/** WHAT A HUB NEEDS BEFORE IT CAN BE ASKED ANYTHING: one scratch
 *  directory of its own, mounted at res://. */
class MountedHub : public ::testing::Test {
 protected:
  MountedHub() { hub.mount("res://", dir.path); }

  sigil::test::ScratchDir dir{"sigilio_hub"};
  sigil::io::Hub hub;
};

// The suites the hub's cases are grouped under. Each is one fixture
// class shared by every file that adds cases to that suite.
class IOSource : public MountedHub {};
class IOHub : public MountedHub {};
class IOOiio : public MountedHub {};
class IOChannels : public MountedHub {};
