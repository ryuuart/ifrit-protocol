/** @file
 * The comparison of two plate directories: that two identical plates
 * read as no distance apart, that a channel moved by a known amount
 * reads as that amount, and that a plate standing in only one of the two
 * directories is named rather than passed over.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <sigilimage/encode/Encode.h>
#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/plate/Sweep.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ScratchDir.h"

namespace {

using sigil::sketch::compare;
using sigil::sketch::CompareOptions;
using sigil::sketch::kPlatePrefix;
using sigil::test::ScratchDir;

/** A plate of one flat colour, written where the sweep would write it. */
void writePlate(const std::filesystem::path& dir, const std::string& name,
                SkColor color, int size = 8) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png->data()),
            (std::streamsize)png->size());
}

TEST(SketchCompare, IdenticalPlatesStandNoDistanceApart) {
  const ScratchDir scratch("compare_same");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE);
  writePlate(second, "probe", SK_ColorBLUE);

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({first.string(), second.string()}), 0);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_NE(report.find("compared probe mean 0.0000 p99 0 max 0"),
            std::string::npos)
      << report;
}

TEST(SketchCompare, ReportsTheChannelDistanceItMeasured) {
  const ScratchDir scratch("compare_moved");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  // One channel of every pixel moved by 8; the other three and the alpha
  // stand still, so the mean over four channels is a quarter of it.
  writePlate(first, "probe", SkColorSetARGB(255, 100, 0, 0));
  writePlate(second, "probe", SkColorSetARGB(255, 108, 0, 0));

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({first.string(), second.string()}), 0);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_NE(report.find("compared probe mean 2.0000 p99 8 max 8"),
            std::string::npos)
      << report;
}

TEST(SketchCompare, NamesAPlateThatStandsInOnlyOneDirectory) {
  const ScratchDir scratch("compare_missing");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE);
  writePlate(first, "alone", SK_ColorRED);
  writePlate(second, "probe", SK_ColorBLUE);

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({first.string(), second.string()}), 1);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_NE(report.find("missing alone second"), std::string::npos) << report;
  EXPECT_NE(report.find("compared probe"), std::string::npos) << report;
}

TEST(SketchCompare, ADirectoryThatIsNotThereIsNotAComparison) {
  const ScratchDir scratch("compare_absent");
  const std::filesystem::path first = scratch.path / "a";
  writePlate(first, "probe", SK_ColorBLUE);
  EXPECT_EQ(compare({first.string(), (scratch.path / "gone").string()}), 2);
}

}  // namespace
