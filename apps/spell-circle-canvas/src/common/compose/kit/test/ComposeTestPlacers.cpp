// The kit's placers: the arithmetic that fills an instanced leaf's pool
// with a grid, a ring or a repeat chain, and which of the pool's lanes each
// parameter speaks to.

#include <sigilcompose/kit/Placers.h>

#include "support/ShapeTestSupport.h"

TEST(ComposePlacers, RepeaterLawExponentialScaleLinearEverythingElse) {
  using namespace sigil::compose::instancing;
  Pool pool;
  place::repeat(pool, 4, {10, 10}, {5, 0}, 0.1f, 0.5f, 1.0f, 0.25f);
  ASSERT_EQ(pool.size(), 4u);
  EXPECT_FLOAT_EQ(pool.positions()[3].fX, 25.0f);  // linear translate
  EXPECT_FLOAT_EQ(pool.rotations()[3], 0.3f);      // linear rotate
  EXPECT_FLOAT_EQ(pool.scales()[3], 0.125f);       // pow(0.5, 3)
  // Opacity and tint are separate lanes: the opacity ramp writes alphas[],
  // and tints[].fA stays exactly what the author put there. Folding one into
  // the other would make a tinted pool silently un-tintable.
  EXPECT_FLOAT_EQ(pool.alphas()[0], 1.0f);  // opacity lerp endpoints
  EXPECT_FLOAT_EQ(pool.alphas()[3], 0.25f);
  EXPECT_FLOAT_EQ(pool.tints()[3].fA, 1.0f);  // untouched
}

TEST(ComposePlacers, TheAlphaLaneFadesWithoutTouchingTheTint) {
  // alphas() is an opt-in lane that composes with the authored tint, and
  // place::repeat writes IT rather than tints[].fA. Sharing one lane would
  // make a faded pool silently un-tintable.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 40});
  auto pool = std::make_shared<instancing::Pool>();
  instancing::place::repeat(*pool, 2, {40, 40}, {80, 0}, 0.0f, 1.0f, 1.0f,
                            0.25f);
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned full = SkColorGetR(host.pixel(40, 40));
  const unsigned faded = SkColorGetR(host.pixel(120, 40));
  EXPECT_GT(full, 240u);   // first copy at full opacity
  EXPECT_LT(faded, 100u);  // last copy at 25% over black
  EXPECT_GT(faded, 20u);
  // …and the tint lane was never written: the fade is alphas()'s.
  EXPECT_EQ(pool->tints()[1].fA, 1.0f);
  EXPECT_TRUE(pool->hasAlphas());
}
