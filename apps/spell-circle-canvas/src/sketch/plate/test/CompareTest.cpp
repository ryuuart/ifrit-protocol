/** @file
 * The comparison of two plate directories: that two identical plates
 * read as no distance apart, that a channel moved by a known amount
 * reads as that amount, that the worst distance is split by what the
 * pixel under it is — nothing, an edge both plates draw, or content —
 * and that a plate standing in only one of the two directories is named
 * rather than passed over.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPathBuilder.h>
#include <sigilimage/encode/Encode.h>
#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/plate/Sweep.h>

#include <cstdio>
#include <cstring>
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

/** TWO SIZES ARE NOT A DISTANCE. A plate that changed shape is a plate
 *  whose scene changed, and averaging one against the other would answer
 *  a number about two different pictures — so it is named and the run
 *  fails, exactly as a missing plate does. */
TEST(SketchCompare, APlateThatChangedShapeIsNamedRatherThanMeasured) {
  const ScratchDir scratch("compare_resized");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE, 8);
  writePlate(second, "probe", SK_ColorBLUE, 12);

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({first.string(), second.string()}), 1);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_NE(report.find("size probe 8x8 12x12"), std::string::npos) << report;
  EXPECT_EQ(report.find("compared probe"), std::string::npos) << report;
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

/** A plate whose top half is transparent black and whose bottom half
 *  holds a colour, with a different amount moved in each half. */
void writeSplitPlate(const std::filesystem::path& dir, const std::string& name,
                     SkColor overClear, SkColor overContent) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(8, 8));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  bitmap.erase(overClear, SkIRect::MakeLTRB(0, 0, 8, 4));
  bitmap.erase(overContent, SkIRect::MakeLTRB(0, 4, 8, 8));
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png->data()),
            (std::streamsize)png->size());
}

/** THE WORST DIFFERENCE, SPLIT BY WHAT IS UNDER IT. A caller whose
 *  tolerance is "one code value over nothing, two over something" needs
 *  the two numbers apart, and only the FIRST plate — the reference — can
 *  say which side a pixel is on. */
TEST(SketchCompare, SplitsTheWorstDistanceByWhatTheFirstPlateHolds) {
  const ScratchDir scratch("compare_split");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  // The reference: nothing at all in the top half, an opaque red in the
  // bottom. The comparand moves the top by 3 and the bottom by 9.
  writeSplitPlate(first, "probe", SK_ColorTRANSPARENT,
                  SkColorSetARGB(255, 100, 0, 0));
  writeSplitPlate(second, "probe", SkColorSetARGB(3, 0, 0, 0),
                  SkColorSetARGB(255, 109, 0, 0));

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({first.string(), second.string()}), 0);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_NE(report.find("max 9 clear 3 content 9"), std::string::npos)
      << report;
}

/** A CURVE, AND A MARK BESIDE IT. The curve is stroked and antialiased, so
 *  every pixel along it carries partial coverage; @p shift slides it by a
 *  fraction of a pixel, which is what a mark standing one float step of its
 *  device coordinate from another rasterisation of itself looks like. The
 *  mark is a hard square the curve never touches: dropping it takes whole
 *  pixels off a flat ground, which is what a picture that MOVED looks
 *  like. */
void writeCurvePlate(const std::filesystem::path& dir, const std::string& name,
                     float shift, bool withMark) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  bitmap.eraseColor(SkColorSetARGB(255, 240, 238, 232));
  SkCanvas canvas(bitmap);
  SkPaint ink;
  ink.setAntiAlias(true);
  ink.setColor(SkColorSetARGB(255, 20, 18, 16));
  ink.setStyle(SkPaint::kStroke_Style);
  ink.setStrokeWidth(2.5f);
  SkPathBuilder curve;
  curve.moveTo(4 + shift, 58);
  curve.quadTo(20 + shift, 4, 60 + shift, 26);
  canvas.drawPath(curve.detach(), ink);
  if (withMark) {
    SkPaint solid;
    solid.setAntiAlias(false);
    solid.setColor(SkColorSetARGB(255, 20, 18, 16));
    canvas.drawRect(SkRect::MakeXYWH(8, 8, 7, 7), solid);
  }
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png->data()),
            (std::streamsize)png->size());
}

/** The numbers off one comparison of two directories. */
struct Split {
  int worst = 0, clear = 0, content = 0, graze = 0, perComposite = 0;
  long long grazing = 0, stacked = 0;
};

Split splitOf(const std::string& report) {
  Split split;
  const size_t at = report.find("max ");
  EXPECT_NE(at, std::string::npos) << report;
  if (at == std::string::npos) return split;
  std::sscanf(report.c_str() + at,
              "max %d clear %d content %d graze %d %lld composited %d %lld",
              &split.worst, &split.clear, &split.content, &split.graze,
              &split.grazing, &split.perComposite, &split.stacked);
  return split;
}

/** A COMPOSITE-COUNT PLANE beside a plate: one grey level per pixel,
 *  saying how many cached rasters were blitted over it. Flat, because
 *  what the case is about is the division and not the map. */
void writeCountPlane(const std::filesystem::path& dir, const std::string& name,
                     int composites, int size = 64) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(size, size, kGray_8_SkColorType, kOpaque_SkAlphaType));
  std::memset(bitmap.getPixels(), composites, bitmap.computeByteSize());
  const sk_sp<SkData> png =
      sigil::image::encodeImage(bitmap.pixmap(), sigil::image::Format::Png);
  ASSERT_TRUE(png);
  std::filesystem::create_directories(dir);
  std::ofstream out(
      dir / (std::string(sigil::sketch::kCountPrefix) + name + ".png"),
      std::ios::binary);
  out.write(reinterpret_cast<const char*>(png->data()),
            (std::streamsize)png->size());
}

}  // namespace

namespace {

/** A GRAZE IS NOT A DEFECT, AND THE PIXELS SAY WHICH IS WHICH. A curve
 *  slid a quarter of a pixel differs only where both plates draw its edge,
 *  and every one of those pixels sits on a coverage ramp in each of them.
 *  A mark that is GONE differs where one plate has a flat ground and the
 *  other has flat ink — no ramp in either, nothing an edge explains — so
 *  it lands on `content` however antialiased the rest of the picture is. */
TEST(SketchCompare, TellsAGrazingEdgeFromAMarkThatIsGone) {
  const ScratchDir scratch("compare_graze");
  const std::filesystem::path base = scratch.path / "base";
  const std::filesystem::path slid = scratch.path / "slid";
  const std::filesystem::path gone = scratch.path / "gone";
  writeCurvePlate(base, "probe", 0.0f, true);
  writeCurvePlate(slid, "probe", 0.25f, true);
  writeCurvePlate(gone, "probe", 0.0f, false);

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({base.string(), slid.string()}), 0);
  const Split grazed = splitOf(testing::internal::GetCapturedStdout());
  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({base.string(), gone.string()}), 0);
  const Split dropped = splitOf(testing::internal::GetCapturedStdout());

  // The slid curve: the whole difference is edge-confined, and there is
  // real ink in it — a quarter of a pixel on a hard-contrast stroke is tens
  // of code values.
  EXPECT_GT(grazed.graze, 20);
  EXPECT_GT(grazed.grazing, 20);
  EXPECT_LE(grazed.content, 2);
  // The dropped mark: the difference is the ink itself, and it is content.
  EXPECT_GT(dropped.content, 180);
  EXPECT_EQ(dropped.worst, dropped.content);
}

/** A CACHED RASTER IS A COMPOSITE, AND A PIXEL CAN STAND UNDER MANY. Each
 *  one rounds, so what a difference costs is a bound per composite times
 *  the count — and a difference of four under four of them is the same
 *  fact as a difference of one under one. The count plane the second
 *  directory carries is what says which; without one every pixel stands
 *  under one composite and the figure is the content difference itself. */
TEST(SketchCompare, PricesTheContentDifferenceByTheCompositesUnderIt) {
  const ScratchDir scratch("compare_composites");
  const std::filesystem::path base = scratch.path / "base";
  const std::filesystem::path gone = scratch.path / "gone";
  writeCurvePlate(base, "probe", 0.0f, true);
  writeCurvePlate(gone, "probe", 0.0f, false);

  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({base.string(), gone.string()}), 0);
  const Split alone = splitOf(testing::internal::GetCapturedStdout());
  EXPECT_EQ(alone.perComposite, alone.content);
  EXPECT_EQ(alone.stacked, 0);

  writeCountPlane(gone, "probe", 4);
  testing::internal::CaptureStdout();
  EXPECT_EQ(compare({base.string(), gone.string()}), 0);
  const Split stacked = splitOf(testing::internal::GetCapturedStdout());
  EXPECT_EQ(stacked.content, alone.content) << "the raw difference moved";
  EXPECT_EQ(stacked.perComposite, (alone.content + 3) / 4);
  EXPECT_GT(stacked.stacked, 0);
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
