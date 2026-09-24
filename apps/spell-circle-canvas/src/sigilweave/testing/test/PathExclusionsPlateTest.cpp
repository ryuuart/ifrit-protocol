/** @file
 * Arbitrary path exclusions: a concave star, a heart of two cubics, and
 * a donut whose even-odd hole stays open, so the paragraph pours through
 * the middle of it.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>

#include <cmath>
#include <numbers>
#include <utility>

#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

TEST(WeavePlates, PathExclusionsDrawTheirBaseline) {
  FontContext& fonts = sigil::test::fonts();
  TextStyle body = plateStyle(16.5f, kInk);
  body.shaping.typeface =
      fonts.fontManager()->matchFamilyStyle("Noto Serif", SkFontStyle());

  Paragraph paragraph;
  for (int repetitionIndex = 0; repetitionIndex < 7; ++repetitionIndex)
    paragraph.appendText(
        u8"Any SkPath carves its exact region out of the line bands — "
        "concave stars, compound paths, cubic hearts — and even-odd holes "
        "stay open, so the paragraph pours right through the middle of the "
        "donut. ",
        body);

  // A star, concave, filled by winding.
  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle = -std::numbers::pi_v<float> / 2.0f +
                        static_cast<float>(pointIndex) * 4.0f *
                            std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {215 + 135 * std::cos(angle),
                           230 + 135 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();

  // A heart of two cubics.
  SkPathBuilder heart;
  heart.moveTo(700, 620);
  heart.cubicTo(540, 470, 590, 330, 700, 420);
  heart.cubicTo(810, 330, 860, 470, 700, 620);
  heart.close();

  // A donut filled even-odd, so its hole is open to text.
  SkPathBuilder donut;
  donut.addCircle(330, 660, 150);
  donut.addCircle(330, 660, 82);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeXYWH(40, 40, 920, 800));
  const SkPath starPath = star.detach();
  const SkPath heartPath = heart.detach();
  const SkPath donutPath = donut.detach();
  flow.exclusions().push_back({flowshape::path(starPath), 10});
  flow.exclusions().push_back({flowshape::path(heartPath), 10});
  flow.exclusions().push_back({flowshape::path(donutPath), 8});
  flow.setMinimumIntervalWidth(46);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 26;
  const weave::testing::Passage passage = weave::testing::lay(
      fonts, std::move(paragraph), flow, std::move(options));

  const weave::testing::Plate plate({1000, 880}, kPaper);
  SkPaint shapePaint;
  shapePaint.setAntiAlias(true);
  shapePaint.setColor(kShape);
  for (const SkPath& shape : {starPath, heartPath, donutPath})
    plate.canvas()->drawPath(shape, shapePaint);
  plate.draw(passage);

  expectPlate(plate, "shapes");
}
