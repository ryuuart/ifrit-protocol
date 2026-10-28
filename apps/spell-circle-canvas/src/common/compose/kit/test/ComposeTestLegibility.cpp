// kit/Legibility.h — what keeps type readable over a ground it does not
// control: the underlay a halo adds, the offset fill a shade is, the
// padding a scrim grows a run by, and the immediate-mode halo a custom
// leaf draws with.

#include <include/core/SkFont.h>
#include <sigilcompose/kit/Legibility.h>

#include <utility>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitLegibility, HaloedAddsExactlyOneUnderlayAndLeavesTheInputAlone) {
  sigil::weave::TextStyle base;
  base.shaping.fontSize = 12;
  const size_t before = base.paint.underlays.size();
  const sigil::weave::TextStyle out =
      kit::haloed(base, {.colour = {1, 1, 1, 1}, .width = 2.2f});
  EXPECT_EQ(base.paint.underlays.size(), before) << "input was mutated";
  ASSERT_EQ(out.paint.underlays.size(), before + 1);
  const SkPaint& p = out.paint.underlays.back().paint;
  EXPECT_EQ(p.getStyle(), SkPaint::kStroke_Style);
  EXPECT_FLOAT_EQ(p.getStrokeWidth(), 2.2f);
  EXPECT_EQ(p.getStrokeJoin(), SkPaint::kRound_Join);
}

TEST(KitLegibility, ShadeIsAnOffsetFillNotAStroke) {
  sigil::weave::TextStyle base;
  const sigil::weave::TextStyle out =
      kit::shaded(base, {.colour = {0, 0, 0, 0.9f}, .offset = {1, 1}});
  ASSERT_EQ(out.paint.underlays.size(), 1u);
  EXPECT_EQ(out.paint.underlays[0].paint.getStyle(), SkPaint::kFill_Style);
  EXPECT_FLOAT_EQ(out.paint.underlays[0].offset.x, 1.0f);
}
