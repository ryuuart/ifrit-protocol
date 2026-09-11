// The texture bake under a transform: a quarter turn, a layer of the bake's
// own, and a host perspective the bake either tracks or refuses.

#include "support/BrushTestSupport.h"

namespace {

/** 196×33 of 1 px hairlines: content that a bake at the wrong resolution
 *  cannot fake. This library's entire output is hairlines at 1×. */
Element hairlinePill() {
  Element p =
      box().width(196).height(33).fill(Fill::color({0.85f, 0.86f, 0.9f, 1}));
  for (int i = 0; i < 46; ++i)
    p.child(box()
                .absolute()
                .left(4 + (float)i * 4)
                .top(5)
                .width(1)
                .height(23)
                .fill(Fill::color({0.05f, 0.06f, 0.08f, 1})));
  return p;
}

/** Cache::None on the wrapper, and NOT as a convenience: a device-space
 *  bake is pinned to one device rect, so it must never be recorded into a
 *  picture — a picture can be replayed under a different matrix than it
 *  was recorded at (an ancestor with a live transform keeps its picture
 *  and replays it under the motion). Inside a recording the node keeps the
 *  local bake, which is matrix-independent and inexact. A cacheable
 *  wrapper would therefore paint this node exactly once, into its parent's
 *  recording, and measure the path this test is not about. */
Element rotatedPill(float degrees, bool cached) {
  Element p = hairlinePill().absolute().left(52).top(133).rotate(degrees);
  if (cached) p.cache(Cache::Texture);
  return box().cache(Cache::None).child(std::move(p));
}

/** Pixels that differ at all, and the mean |Δ| over ink — the count is the
 *  claim, the mean is what makes a failure legible. */
struct BakeError {
  size_t differing = 0;
  double meanInk = 0;
};

BakeError bakeErrorAt(float degrees) {
  Host plain(300, 300), baked(300, 300);
  plain.composer.render(rotatedPill(degrees, false));
  baked.composer.render(rotatedPill(degrees, true));
  // Three frames: the bake must be taken on the first (the gate reads the
  // node's declared transform, not paint history) AND still be the same
  // pixels on the third, which is what catches a bake mode that oscillates.
  for (int i = 0; i < 3; ++i) {
    plain.frame();
    baked.frame();
  }
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bb.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  plain.surface->readPixels(ba.pixmap(), 0, 0);
  baked.surface->readPixels(bb.pixmap(), 0, 0);
  BakeError out;
  double total = 0;
  size_t ink = 0;
  for (int y = 0; y < 300; ++y)
    for (int x = 0; x < 300; ++x) {
      const SkColor pa = ba.getColor(x, y), pb = bb.getColor(x, y);
      if (pa != pb) ++out.differing;
      if (pa == SK_ColorBLACK && pb == SK_ColorBLACK) continue;
      ++ink;
      total += (std::abs((int)SkColorGetR(pa) - (int)SkColorGetR(pb)) +
                std::abs((int)SkColorGetG(pa) - (int)SkColorGetG(pb)) +
                std::abs((int)SkColorGetB(pa) - (int)SkColorGetB(pb))) /
               3.0;
    }
  out.meanInk = ink ? total / (double)ink : 0.0;
  return out;
}

}  // namespace

TEST(ComposeCache, TextureBakeSurvivesAQuarterTurn) {
  // A settled bake is taken in DEVICE space, snapped out to whole device
  // pixels and blitted with the matrix reset, so it is a literal copy of
  // the pixels the uncached draw would have produced — at ANY angle, not
  // merely close at the convenient ones.
  //
  // Two independent things have to be right, and getting only the first
  // leaves a picture that still looks nearly correct at every angle:
  //
  //  - the bake RESOLUTION, which must come from the matrix's singular
  //    values rather than its diagonal (a quarter turn puts the whole scale
  //    in the skew terms, so the diagonal reads zero and the bake clamps to
  //    the ladder's floor); and
  //  - the bake SPACE, which must be device space. A bake held in LOCAL
  //    space is resampled by whatever transform blits it, so its texel grid
  //    lands off the device grid and softens every hairline.
  //
  // Correct resolution alone is necessary and not sufficient, which is why
  // the assertion is exact pixel equality at every angle rather than a
  // tolerance.
  for (float degrees : {0.0f, 90.0f, -90.0f, 180.0f, 45.0f}) {
    const BakeError e = bakeErrorAt(degrees);
    EXPECT_EQ(e.differing, 0u)
        << "rotate(" << degrees << "): " << e.differing
        << " pixels differ from the uncached render (mean |delta| over ink "
        << e.meanInk << ") — the bake is being resampled, not copied";
  }
}

TEST(ComposeCache, ATextureBakeCompositesThroughItsOwnLayer) {
  // The exact bake blits with the matrix RESET, and an opacity/blend node
  // is already inside a saveLayer when it does. That is only correct
  // because an identity CTM is global canvas space even inside a layer —
  // the layer device carries its own origin. If that were wrong the image
  // would land somewhere else entirely, so it is worth an assertion rather
  // than an argument.
  const auto pill = [](bool cached, bool rotate) {
    Element p = hairlinePill()
                    .absolute()
                    .left(52)
                    .top(133)
                    .rotate(rotate ? -90.0f : 0.0f)
                    .opacity(0.5f)
                    .blend(SkBlendMode::kScreen);
    if (cached) p.cache(Cache::Texture);
    return box().cache(Cache::None).child(std::move(p));
  };
  for (bool rotate : {false, true}) {
    Host plain(300, 300), baked(300, 300);
    plain.composer.render(pill(false, rotate));
    baked.composer.render(pill(true, rotate));
    for (int i = 0; i < 3; ++i) {
      plain.frame();
      baked.frame();
    }
    // Not "roughly where it should be" — the layer receives exactly the
    // pixels paintContent would have drawn into it, so the composite is
    // the same composite.
    EXPECT_TRUE(identicalPixels(plain, baked, 300, 300))
        << "a cached node at opacity/blend" << (rotate ? " + rotate(-90)" : "")
        << " did not composite the same as the uncached one";
  }
}

namespace {

/** A host camera that is PURE perspective: the keystone `[1,0,0; 0,1,0;
 *  0,p,1]` — a plate tipped away from the viewer, anchored at the top
 *  edge. Deliberately NOT a perspective·rotateY SkM44: that matrix's 2D
 *  projection also carries a skew term, so the `upright` gate would
 *  refuse it for the skew alone, and this test would pass even with the
 *  perspective clause removed. This matrix is upright by every other test
 *  the gate makes — scale 1, skew 0 — so only `hasPerspective()` says no. */
SkMatrix hostCamera(float p) {
  SkMatrix m = SkMatrix::I();
  m.setPerspY(p);
  return m;
}

/** Host::frame, under a host concat — the camera compose never sees. */
void frameUnder(Host& h, const SkMatrix& camera) {
  SkCanvas* canvas = h.surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->save();
  canvas->concat(camera);
  h.composer.draw(*canvas);
  canvas->restore();
}

/** Mean |Δ| over ink between two hosts — bakeErrorAt's meter, reusable
 *  for frames drawn under a camera. */
double meanInkDiff(Host& a, Host& b, int w, int h) {
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bb.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  a.surface->readPixels(ba.pixmap(), 0, 0);
  b.surface->readPixels(bb.pixmap(), 0, 0);
  double total = 0;
  size_t ink = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor pa = ba.getColor(x, y), pb = bb.getColor(x, y);
      if (pa == SK_ColorBLACK && pb == SK_ColorBLACK) continue;
      ++ink;
      total += (std::abs((int)SkColorGetR(pa) - (int)SkColorGetR(pb)) +
                std::abs((int)SkColorGetG(pa) - (int)SkColorGetG(pb)) +
                std::abs((int)SkColorGetB(pa) - (int)SkColorGetB(pb))) /
               3.0;
    }
  return ink ? total / (double)ink : 0.0;
}

}  // namespace

TEST(ComposeCache, PromotionRefusesAHostPerspectiveCtm) {
  // Guard #1, promotion's `upright` gate: a perspective CTM must read as
  // Transformed — a device bake under it would pin a projected quad to an
  // axis-aligned rect. The no-perspective arm is the positive control: the
  // SAME node under the same harness PROMOTES, so a silently widened gate
  // fails the arm that proves the refusal was the guard's doing.
  for (bool perspective : {false, true}) {
    Host host(300, 300);
    host.composer.setProfiling(true);
    host.composer.render(profiledUnder(
        expensivePanel().absolute().left(40).top(40).key("underCamera")));
    const SkMatrix camera = perspective ? hostCamera(0.0012f) : SkMatrix::I();
    for (int i = 0; i < 30; ++i) frameUnder(host, camera);
    const Composer::NodeCost* row = requireRow(host.composer, "underCamera");
    ASSERT_NE(row, nullptr);
    if (perspective) {
      EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
          << "promoted a node under a host perspective CTM";
      EXPECT_TRUE(row->refused(Composer::Promotion::Transformed))
          << "the refusal must name the geometry";
    } else {
      EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
          << "the control arm did not promote — the refusal assertion above "
             "is not testing the perspective guard";
    }
  }
}

namespace {

/** Smooth content for the projected-bake meter: a ramp panel with thick
 *  bars, not hairlines — the LOCAL bake legitimately resamples through
 *  the projection, so the pin bounds the error rather than demanding
 *  byte equality (that is the device bake's claim, and the device bake
 *  is exactly what perspective refuses). */
Element rampPanel(bool cached) {
  Element p = box().width(180).height(120).absolute().left(60).top(90).fill(
      material::skia::Paint::linearUnit(
          {0, 0}, {1, 1},
          {{0.0f, {0.9f, 0.3f, 0.1f, 1}}, {1.0f, {0.1f, 0.4f, 0.9f, 1}}}));
  for (int i = 0; i < 4; ++i)
    p.child(box()
                .absolute()
                .left(15 + (float)i * 42)
                .top(18)
                .width(18)
                .height(84)
                .fill(Fill::color({0.92f, 0.93f, 0.95f, 1})));
  if (cached) p.cache(Cache::Texture);
  return box().cache(Cache::None).child(std::move(p));
}

}  // namespace

TEST(ComposeCache, ATextureBakeUnderPerspectiveTracksTheCamera) {
  // Guard #3, the Cache::Texture device bake: under a perspective CTM the
  // node must fall back to the matrix-independent LOCAL bake (unpromoted
  // in the device sense) rather than bake a device-rect-pinned texture.
  // Pinned through what an author can see: the baked node's pixels track
  // the camera, frame over frame, staying near the uncached twin — a
  // device-pinned texture would replay the OLD projection after the
  // camera moved, and the second sweep below would blow up.
  Host plain(300, 300), baked(300, 300);
  plain.composer.render(rampPanel(false));
  baked.composer.render(rampPanel(true));
  const SkMatrix before = hostCamera(0.0008f);
  size_t bakes = 0;
  for (int i = 0; i < 3; ++i) {
    frameUnder(plain, before);
    frameUnder(baked, before);
    bakes += baked.composer.stats().texturesBaked;
  }
  const double still = meanInkDiff(plain, baked, 300, 300);
  EXPECT_LT(still, 4.0) << "the perspective fallback bake drifted from the "
                           "live render while the camera held still";

  // THE CAMERA MOVES. The projection changes enough to be its own control
  // (the two plain frames must differ), and the baked node must follow.
  const SkMatrix after = hostCamera(0.0016f);
  Host plainAfter(300, 300);
  plainAfter.composer.render(rampPanel(false));
  frameUnder(plainAfter, after);
  const double cameraMoved = meanInkDiff(plain, plainAfter, 300, 300);
  EXPECT_GT(cameraMoved, 8.0)
      << "the two camera angles are too close to distinguish a pinned bake "
         "from a tracking one — the pin below is vacuous";
  frameUnder(baked, after);
  bakes += baked.composer.stats().texturesBaked;
  const double tracked = meanInkDiff(plainAfter, baked, 300, 300);
  EXPECT_LT(tracked, 4.0)
      << "the baked node did not track the camera — a device-rect-pinned "
         "texture replayed the old projection (mean |delta| over ink "
      << tracked << " against " << cameraMoved << " of real motion)";
  // The BAKE COUNT, not the pixels, is what separates the two paths. A
  // wrongly-taken device bake mostly self-heals through its rect-stability
  // test, so a pixel comparison alone passes either way. The local fallback
  // bakes ONCE and its bake is matrix-independent, so camera motion re-bakes
  // nothing; a device bake is pinned to its rect and has to be re-taken
  // every time the camera moves.
  EXPECT_EQ(bakes, 1u)
      << "a Cache::Texture node under a perspective camera took a "
         "device-space bake (or re-baked under camera motion) instead of "
         "holding one local bake";
}

TEST(ComposeCache, SparseFurnitureSurvivesAFractionalCaptureScale) {
  const auto scene = [](Cache cache) {
    auto furniture = box().inset(0).cache(cache);
    furniture.child(
        box()
            .left(72)
            .top(58)
            .width(1296)
            .height(936)
            .fill(Fill::none())
            .foreground(decorations::border(7, Fill::color({1, 1, 1, 1}))));
    furniture.child(
        box()
            .left(442)
            .top(311)
            .width(556)
            .height(556)
            .shape(geometry::shapes::circle())
            .fill(Fill::none())
            .foreground(decorations::border(5, Fill::color({1, 1, 1, 1}))));
    return box()
        .inset(0)
        .cache(Cache::None)
        .effect(
            material::skia::Effect::filter(SkImageFilters::Blur(1, 1, nullptr)))
        .child(std::move(furniture));
  };
  Host cached(1440, 1052), plain(1440, 1052);
  cached.composer.render(scene(Cache::Texture));
  plain.composer.render(scene(Cache::None));
  cached.composer.setAutoTexturePromotion(false);
  plain.composer.setAutoTexturePromotion(false);
  for (int i = 0; i < 3; ++i) cached.frame();
  for (Host* host : {&cached, &plain}) {
    host->surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2400, 1754));
    host->surface->getCanvas()->scale(5.0f / 3.0f, 5.0f / 3.0f);
    host->frame();
  }
  SkBitmap expected, actual;
  expected.allocPixels(SkImageInfo::MakeN32Premul(2400, 1754));
  actual.allocPixels(expected.info());
  plain.surface->readPixels(expected.pixmap(), 0, 0);
  cached.surface->readPixels(actual.pixmap(), 0, 0);
  int lit = 0, lost = 0;
  for (int y = 0; y < 1754; ++y)
    for (int x = 0; x < 2400; ++x) {
      if (SkColorGetR(expected.getColor(x, y)) < 200) continue;
      ++lit;
      if (SkColorGetR(actual.getColor(x, y)) < 100) ++lost;
    }
  EXPECT_GT(lit, 10000);
  EXPECT_EQ(lost, 0);
}
