// The opening ornament: an element given a silhouette excludes the copy
// that flows around it by that silhouette rather than by its box, so the
// first line starts further into the measure than a rectangular ornament
// of the same size allows.

#include <string>
#include <utility>

#include "support/KitType.h"
#include "support/ShapeTestSupport.h"

TEST(KitOrnament, AnOrnamentKeepsItsSilhouetteAsTheOpeningExclusion) {
  const std::u8string passage =
      u8"small words keep moving through the opening measure until the "
      u8"ornament has passed and the full line becomes available again "
      u8"below it, with enough copy to make that recovery visible";
  const auto scene = [&](bool silhouette) {
    Element ornament =
        box().width(90).height(90).fill(Fill::color({0, 1, 0, 1}));
    if (silhouette) ornament.shape(geometry::shapes::circle());
    ornament.key("ornament").absolute().left(0.0f).top(0.0f);
    return box().width(220).height(260).children(
        {std::move(ornament), text(passage, pixelStyle(12))
                                  .key("body")
                                  .width(220)
                                  .flowAround("ornament", 4)});
  };

  Host boxed(220, 260), round(220, 260);
  boxed.composer.render(scene(false));
  boxed.frame();
  round.composer.render(scene(true));
  round.frame();

  ASSERT_TRUE(round.composer.bounds("ornament").has_value());
  EXPECT_FLOAT_EQ(round.composer.bounds("ornament")->width(), 90);
  EXPECT_FLOAT_EQ(round.composer.bounds("ornament")->height(), 90);
  const auto firstLineStart = [](const Host& host) {
    float start = 10000;
    for (const weave::PositionedRun& run :
         host.composer.paragraphLayout("body")->runs)
      if (run.lineIndex == 0) start = std::min(start, run.origin.x());
    return start;
  };
  EXPECT_LT(firstLineStart(round), firstLineStart(boxed));
}
