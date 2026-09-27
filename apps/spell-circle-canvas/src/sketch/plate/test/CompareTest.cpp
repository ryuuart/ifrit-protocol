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
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/plate/Sweep.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "ScratchDir.h"

namespace {

using sigil::sketch::compare;
using sigil::sketch::Comparison;
using sigil::sketch::kPlatePrefix;
using sigil::sketch::PlateComparison;
using sigil::sketch::PlateOutcome;
using sigil::sketch::PlateSide;
using sigil::sketch::printComparison;
using sigil::test::ScratchDir;

/** The row a comparison holds for @p name, or a failure naming it. */
const PlateComparison& rowOf(const Comparison& comparison,
                             const std::string& name) {
  for (const PlateComparison& plate : comparison.plates)
    if (plate.name == name) return plate;
  ADD_FAILURE() << "no row for " << name;
  static const PlateComparison none;
  return none;
}

/** A plate of one flat colour, written where the sweep would write it. */
void writePlate(const std::filesystem::path& dir, const std::string& name,
                SkColor color, int size = 8) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size, size));
  bitmap.eraseColor(color);
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png.data()),
            (std::streamsize)png.size());
}

TEST(SketchCompare, IdenticalPlatesStandNoDistanceApart) {
  const ScratchDir scratch("compare_same");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE);
  writePlate(second, "probe", SK_ColorBLUE);

  const Comparison comparison = compare({first.string(), second.string()});
  EXPECT_EQ(comparison.status(), 0);
  ASSERT_EQ(comparison.plates.size(), 1u);
  const PlateComparison& probe = comparison.plates.front();
  EXPECT_EQ(probe.name, "probe");
  EXPECT_EQ(probe.outcome, PlateOutcome::Compared);
  EXPECT_EQ(probe.mean, 0.0);
  EXPECT_EQ(probe.p99, 0);
  EXPECT_EQ(probe.worst, 0);
}

/** The printed row is the value, spelled: the line Sketchbook's
 *  `--compare` writes opens with the word that says what it is. */
TEST(SketchCompare, PrintsEachRowAsTheLineItsWordOpens) {
  const ScratchDir scratch("compare_printed");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE);
  writePlate(first, "alone", SK_ColorRED);
  writePlate(second, "probe", SK_ColorBLUE);

  testing::internal::CaptureStdout();
  EXPECT_EQ(printComparison(compare({first.string(), second.string()})), 1);
  const std::string report = testing::internal::GetCapturedStdout();
  EXPECT_EQ(report,
            "missing alone second\n"
            "compared probe mean 0.0000 p99 0 max 0 clear 0 content 0 graze 0 "
            "0 composited 0 0\n");
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

  const Comparison comparison = compare({first.string(), second.string()});
  EXPECT_EQ(comparison.status(), 1);
  const PlateComparison& probe = rowOf(comparison, "probe");
  EXPECT_EQ(probe.outcome, PlateOutcome::Resized);
  EXPECT_EQ(probe.firstWidth, 8);
  EXPECT_EQ(probe.firstHeight, 8);
  EXPECT_EQ(probe.secondWidth, 12);
  EXPECT_EQ(probe.secondHeight, 12);
}

TEST(SketchCompare, ReportsTheChannelDistanceItMeasured) {
  const ScratchDir scratch("compare_moved");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  // One channel of every pixel moved by 8; the other three and the alpha
  // stand still, so the mean over four channels is a quarter of it.
  writePlate(first, "probe", SkColorSetARGB(255, 100, 0, 0));
  writePlate(second, "probe", SkColorSetARGB(255, 108, 0, 0));

  const Comparison comparison = compare({first.string(), second.string()});
  EXPECT_EQ(comparison.status(), 0);
  const PlateComparison& probe = rowOf(comparison, "probe");
  EXPECT_EQ(probe.mean, 2.0);
  EXPECT_EQ(probe.p99, 8);
  EXPECT_EQ(probe.worst, 8);
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
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png.data()),
            (std::streamsize)png.size());
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

  const Comparison comparison = compare({first.string(), second.string()});
  EXPECT_EQ(comparison.status(), 0);
  const PlateComparison& probe = rowOf(comparison, "probe");
  EXPECT_EQ(probe.worst, 9);
  EXPECT_EQ(probe.worstOverClear, 3);
  EXPECT_EQ(probe.worstOverContent, 9);
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
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / (std::string(kPlatePrefix) + name + ".png"),
                    std::ios::binary);
  out.write(reinterpret_cast<const char*>(png.data()),
            (std::streamsize)png.size());
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
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  std::filesystem::create_directories(dir);
  std::ofstream out(
      dir / (std::string(sigil::sketch::kCountPrefix) + name + ".png"),
      std::ios::binary);
  out.write(reinterpret_cast<const char*>(png.data()),
            (std::streamsize)png.size());
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

  const Comparison slidComparison = compare({base.string(), slid.string()});
  const Comparison goneComparison = compare({base.string(), gone.string()});
  EXPECT_EQ(slidComparison.status(), 0);
  EXPECT_EQ(goneComparison.status(), 0);
  const PlateComparison& grazed = rowOf(slidComparison, "probe");
  const PlateComparison& dropped = rowOf(goneComparison, "probe");

  // The slid curve: the whole difference is edge-confined, and there is
  // real ink in it — a quarter of a pixel on a hard-contrast stroke is tens
  // of code values.
  EXPECT_GT(grazed.worstOverGraze, 20);
  EXPECT_GT(grazed.grazingPixels, 20u);
  EXPECT_LE(grazed.worstOverContent, 2);
  // The dropped mark: the difference is the ink itself, and it is content.
  EXPECT_GT(dropped.worstOverContent, 180);
  EXPECT_EQ(dropped.worst, dropped.worstOverContent);
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

  const PlateComparison alone =
      rowOf(compare({base.string(), gone.string()}), "probe");
  EXPECT_EQ(alone.worstPerComposite, alone.worstOverContent);
  EXPECT_EQ(alone.stackedPixels, 0u);

  writeCountPlane(gone, "probe", 4);
  const PlateComparison stacked =
      rowOf(compare({base.string(), gone.string()}), "probe");
  EXPECT_EQ(stacked.worstOverContent, alone.worstOverContent)
      << "the raw difference moved";
  EXPECT_EQ(stacked.worstPerComposite, (alone.worstOverContent + 3) / 4);
  EXPECT_GT(stacked.stackedPixels, 0u);
}

TEST(SketchCompare, NamesAPlateThatStandsInOnlyOneDirectory) {
  const ScratchDir scratch("compare_missing");
  const std::filesystem::path first = scratch.path / "a";
  const std::filesystem::path second = scratch.path / "b";
  writePlate(first, "probe", SK_ColorBLUE);
  writePlate(first, "alone", SK_ColorRED);
  writePlate(second, "probe", SK_ColorBLUE);

  const Comparison comparison = compare({first.string(), second.string()});
  EXPECT_EQ(comparison.status(), 1);
  const PlateComparison& alone = rowOf(comparison, "alone");
  EXPECT_EQ(alone.outcome, PlateOutcome::Missing);
  EXPECT_EQ(alone.side, PlateSide::Second);
  EXPECT_EQ(rowOf(comparison, "probe").outcome, PlateOutcome::Compared);
}

TEST(SketchCompare, ADirectoryThatIsNotThereIsNotAComparison) {
  const ScratchDir scratch("compare_absent");
  const std::filesystem::path first = scratch.path / "a";
  writePlate(first, "probe", SK_ColorBLUE);
  const Comparison comparison =
      compare({first.string(), (scratch.path / "gone").string()});
  EXPECT_EQ(comparison.status(), 2);
  EXPECT_FALSE(comparison.refusal.empty());
  EXPECT_TRUE(comparison.plates.empty());
}

}  // namespace
