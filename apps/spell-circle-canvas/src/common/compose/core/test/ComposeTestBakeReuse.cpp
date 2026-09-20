// When a bake is taken once and blitted after: the scale it is quantised
// to, the ancestor motion it survives, the angles it stays exact at, the
// matrix move that remakes the recording holding it, the local bake a
// live transform leaves alone, the cache a memo shell carries onto its
// produce, and what a blit keeps of what was baked.

#include <algorithm>
#include <cmath>
#include <vector>

#include "support/CoreTestSupport.h"

TEST(ComposeCaching, TextureBakeScaleQuantized) {
  // A continuously changing canvas scale (live window resize, pinch
  // zoom) must not re-bake Cache::Texture nodes every frame: the bake
  // scale quantizes up to a coarse step.
  Host host;
  host.composer.render(
      box()
          .width(60)
          .height(60)
          .cache(Cache::Texture)
          .fill(red())
          .children({box().width(20).height(20).fill(green())}));
  auto drawAt = [&](float s) {
    SkCanvas& canvas = *host.surface->getCanvas();
    canvas.save();
    canvas.scale(s, s);
    host.composer.draw(canvas);
    canvas.restore();
  };
  drawAt(1.6f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);  // first bake
  drawAt(1.7f);
  drawAt(1.9f);
  drawAt(2.0f);  // still within the 2.0 step: the bake is reused
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  drawAt(2.2f);  // crossed into the 3.0 step: one re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
}

TEST(ComposeCaching, TextureBakeReusedUnderAMovingAncestor) {
  // The same guarantee from the side the test above cannot see. A bake
  // taken in DEVICE space is exact but pinned to one device rect, so it
  // may only be taken while the node is holding still — and "still" has
  // two independent measures that are easy to mistake for one:
  //
  //   * the node's own transform is not declared as animating, and
  //   * the device rect it LANDS on has not moved.
  //
  // This node declares nothing. It is dragged across the canvas by an
  // ancestor, through a Cache::None parent so no recording intervenes —
  // the one arrangement where a moving rect reaches a node that looks
  // static from every declaration available to it. A device-pinned bake
  // would re-rasterize every frame here, which is precisely the cost the
  // quantized local bake exists to avoid.
  //
  // Note this cannot be a pixel assertion: every arrangement below draws
  // the correct picture. Only the bake COUNT tells them apart.
  Host host(300, 300);
  choreograph::Output<float> slide{0.0f};
  host.composer.render(
      box()
          .cache(Cache::None)
          .children(
              {box()
                   .cache(Cache::None)
                   .absolute()
                   .translateX(&slide)
                   .children({box()
                                  .width(60)
                                  .height(60)
                                  .cache(Cache::Texture)
                                  .fill(red())
                                  .children({box().width(20).height(20).fill(
                                      green())})})}));
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);  // the first bake
  // The still -> moving transition costs exactly one re-bake, because the
  // held image is in the wrong space for the path now being taken. That is
  // inherent to having two bake spaces and is not what this test guards.
  slide = 7.0f;
  host.frame();
  // From here the guarantee is absolute: a moving node reuses ONE local
  // bake and blits it through its transform, however far it travels.
  for (int i = 2; i <= 5; ++i) {
    slide = (float)i * 7.0f;  // whole-pixel slides: the rect really moves
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "frame " << i
        << ": the bake was re-rasterized while the node slid, instead of "
           "being reused and blitted through the transform";
  }
}

namespace {

/** The largest channel difference between two hosts' canvases. */
int maxChannelDifference(Host& a, Host& b, int w, int h) {
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bb.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  a.surface->readPixels(ba.pixmap(), 0, 0);
  b.surface->readPixels(bb.pixmap(), 0, 0);
  int worst = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor p = ba.getColor(x, y), q = bb.getColor(x, y);
      worst =
          std::max({worst, std::abs((int)SkColorGetR(p) - (int)SkColorGetR(q)),
                    std::abs((int)SkColorGetG(p) - (int)SkColorGetG(q)),
                    std::abs((int)SkColorGetB(p) - (int)SkColorGetB(q))});
    }
  return worst;
}

/** A pill of type turned by @p degrees, NESTED ONE LEVEL UNDER A STATIC
 *  CONTAINER — the container carries a second child, so it takes the
 *  automatic picture and the pill is painted into that recording. */
Element turnedPill(float degrees, Cache mode) {
  return box()
      .cache(Cache::None)
      .children(
          {box()
               .inset(0)
               .children(
                   {box().absolute().left(2).top(2).width(4).height(4).fill(
                       Fill::color({0, 0.3f, 0, 1}))})
               .children({box()
                              .absolute()
                              .left(60)
                              .top(100)
                              .width(140)
                              .height(28)
                              .key("pill")
                              .cache(mode)
                              .rotate(degrees)
                              .transformOrigin(0.5f, 0.5f)
                              .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))
                              .children({text(u8"LEFT SIDE BARRIER",
                                              whiteStyle(15))})})});
}

}  // namespace

TEST(ComposeCaching, ADeviceBakeUnderAStaticContainerIsExactAtEveryAngle) {
  // A texture bake held in local space is blitted through the node's
  // rotation and resampled by it; at a quarter turn the texel grid lands
  // half a texel off the device grid on the axis carrying the type's
  // detail. The device-space bake has nothing to resample — and it is
  // reachable under a static container, whose recording is then pinned
  // to the matrix it was made under. So the cached pill must agree with
  // the uncached draw to the same tolerance at every angle, the quarter
  // turns included; a root-level pill would pass this at any angle and
  // prove nothing.
  const int w = 260, h = 260;
  int worstOnAxis = 0, worstOffAxis = 0;
  for (float degrees : {0.0f, 45.0f, 90.0f, 180.0f, -90.0f}) {
    Host cached(w, h), plain(w, h);
    cached.composer.render(turnedPill(degrees, Cache::Texture));
    cached.frame();
    cached.frame();  // the second frame replays the pinned recording
    EXPECT_EQ(cached.composer.stats().picturesRecorded, 0u) << degrees;
    plain.composer.render(turnedPill(degrees, Cache::None));
    plain.frame();
    const int worst = maxChannelDifference(cached, plain, w, h);
    if (degrees == 0.0f || degrees == 180.0f)
      worstOnAxis = std::max(worstOnAxis, worst);
    else
      worstOffAxis = std::max(worstOffAxis, worst);
    EXPECT_LE(worst, 2) << "at " << degrees << " degrees";
  }
  EXPECT_LE(worstOffAxis, worstOnAxis + 1)
      << "the turned pill was resampled where the upright one was not";
}

TEST(ComposeCaching, ARecordingHoldingADeviceBakeIsRemadeWhenItsMatrixMoves) {
  // The pin. A device blit inside a recording is exact under the matrix
  // the recording was made under and nowhere else, so the recording is
  // remade — and the bake with it — the frame the host's matrix changes,
  // and replays untouched while it holds.
  Host host(260, 260);
  host.composer.render(turnedPill(-90.0f, Cache::Texture));
  auto drawAt = [&](float s) {
    SkCanvas& canvas = *host.surface->getCanvas();
    canvas.clear(SK_ColorBLACK);
    canvas.save();
    canvas.scale(s, s);
    host.composer.draw(canvas);
    canvas.restore();
  };
  drawAt(1.0f);
  EXPECT_GE(host.composer.stats().texturesBaked, 1u);
  drawAt(1.0f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  drawAt(0.5f);  // the pinned recording is stale under any other matrix
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  // Under CONTINUOUS motion the pill falls back to the local bake, the
  // recording holds no device blit, and it replays under every matrix
  // the way any recording does.
  drawAt(0.6f);
  drawAt(0.7f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // …and once the matrix has held still for a frame the deferred device
  // bake is taken: one remake, then replays.
  drawAt(0.7f);
  EXPECT_GE(host.composer.stats().texturesBaked, 1u);
  drawAt(0.7f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeCaching, ANodeUnderALiveTransformKeepsTheLocalBakeInItsRecording) {
  // The restriction that stays. A recording made under a declared motion
  // replays under that motion, so it may hold nothing pinned to a device
  // rect: the texture node inside it takes the quantized local bake, the
  // recording stays matrix-independent, and neither is remade while the
  // ancestor slides — the same guarantee the moving-ancestor case gives
  // through a Cache::None parent, here through a recording.
  Host host(300, 300);
  choreograph::Output<float> slide{0.0f};
  host.composer.render(
      box()
          .cache(Cache::None)
          .children(
              {box()
                   .absolute()
                   .translateX(&slide)
                   .children(
                       {box().absolute().left(2).top(2).width(4).height(4).fill(
                           Fill::color({0, 0.3f, 0, 1}))})
                   .children({box()
                                  .absolute()
                                  .left(60)
                                  .top(100)
                                  .width(60)
                                  .height(60)
                                  .cache(Cache::Texture)
                                  .rotate(-90.0f)
                                  .transformOrigin(0.5f, 0.5f)
                                  .fill(red())
                                  .children({box().width(20).height(20).fill(
                                      green())})})}));
  host.frame();
  EXPECT_GE(host.composer.stats().texturesBaked, 1u);
  // The still -> moving transition costs one remake, as it does through a
  // Cache::None parent; from there the guarantee is absolute.
  slide = 7.0f;
  host.frame();
  for (int i = 2; i <= 5; ++i) {
    slide = (float)i * 7.0f;
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "frame " << i
        << ": a recording under a live transform was remade, or the bake "
           "inside it was pinned to a device rect";
  }
}

TEST(ComposeCaching, AMemoShellsCacheIsCarriedOntoItsProduce) {
  // memo(...).cache(Cache::Picture) is written on the SHELL, and the
  // reconciler retains the PRODUCE as the node's description — so the
  // explicit cache reaches the painter only if the shell's say is carried
  // across. The produce here is a childless box, which records nothing
  // under Cache::Auto (one draw beats a nested recording) and exactly one
  // picture under Cache::Picture; the parent is Cache::None so no
  // recording above it can stand in for the memo's own.
  struct Props {
    int tick = 0;
    bool operator==(const Props&) const = default;
  };
  Host host;
  const auto describe = [](int tick) {
    return box()
        .cache(Cache::None)
        .children({memo(Props{tick},
                        [](const Props& p) {
                          return box().width(40).height(40).fill(
                              p.tick % 2 ? red() : green());
                        })
                       .key("cell")
                       .cache(Cache::Picture)});
  };
  host.composer.render(describe(0));
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorGREEN);
  // Props change → the memo re-describes → its shell's cache still holds,
  // and the fresh produce is recorded once more.
  host.composer.render(describe(1));
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);
  // Equal properties → a memo hit reuses the retained produce, and its
  // recording replays untouched.
  host.composer.render(describe(1));
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);
}

namespace {

/** A RING OF TYPE turned about its own centre by a bound rotation: the
 *  shape a bake is most worth taking for, and the shape whose bake is
 *  mostly transparent. The letters ride a circle inscribed in the node's
 *  box, so the ink is a band and the corners are empty. */
Element ringOfType(Cache mode, const choreograph::Output<float>* turn) {
  const float side = 640.0f, radius = 270.0f;
  Element ring = box()
                     .key("ring")
                     .absolute()
                     .left(10)
                     .top(10)
                     .width(side)
                     .height(side)
                     .cache(mode)
                     .transformOrigin(0.5f, 0.5f);
  const char8_t* letters[] = {u8"ANIMA", u8"LUMEN", u8"ORDO",  u8"SIGNUM",
                              u8"VOX",   u8"NOMEN", u8"CIRCU", u8"TERRA",
                              u8"AQUA",  u8"IGNIS", u8"AER",   u8"SAL"};
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i * (float)(2 * M_PI) / 12.0f;
    ring.children({text(letters[i], whiteStyle(18))
                       .absolute()
                       .left(side * 0.5f + radius * std::cos(a) - 40.0f)
                       .top(side * 0.5f + radius * std::sin(a) - 12.0f)
                       .width(80)});
  }
  if (turn) ring.rotate(motion::bind(turn).target(0.0f, 360.0f));
  return ring;
}

/** …painted every frame, so the cases below watch the ring itself rather
 *  than an ancestor's recording of it. */
Element turnedRing(Cache mode, const choreograph::Output<float>* turn) {
  return profiledUnder(ringOfType(mode, turn));
}

/** …and the other placement every scene has: the same ring inside a PAGE
 *  that records and SLIDES. The page's declared motion is what keeps its
 *  recording matrix-independent, which is what keeps the ring on the local
 *  bake — the tier the ink grid describes — and the recording is replayed
 *  under a matrix of its own, which is not the page's own space. */
Element ringInASlidingPage(const choreograph::Output<float>* slide,
                           Cache mode) {
  return profiledUnder(box()
                           .key("page")
                           .absolute()
                           .left(30)
                           .top(24)
                           .width(700)
                           .height(700)
                           .translateX(slide)
                           .children({ringOfType(mode, nullptr)}));
}

}  // namespace

TEST(ComposeCaching, ARingUnderABoundRotationBakesOnceAndBlitsEveryFrame) {
  // A rotation about a node's own centre moves no pixel of its content —
  // it moves where the content lands. So the bake is taken ONCE, in the
  // node's own space, and every frame after is a blit through the turn;
  // the ladder the local bake is quantized on cannot be moved by a
  // rotation, so no rung is ever crossed and nothing is re-rasterized.
  Host host(680, 680);
  choreograph::Output<float> turn{0.0f};
  host.composer.render(turnedRing(Cache::Texture, &turn));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the one bake";
  for (int i = 1; i <= 12; ++i) {
    turn = (float)i * 0.011f;  // a few degrees a frame, past every quadrant
    host.frame(1.0 / 60.0);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "frame " << i << ": the ring was re-rasterized while it turned";
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "frame " << i << ": the ring re-recorded while it turned";
  }
}

namespace {

/** The brightest channel in each @p block-sized block of a host's canvas. */
std::vector<int> blockPeaks(Host& host, int w, int h, int block) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  std::vector<int> peaks((size_t)((w + block - 1) / block) *
                         (size_t)((h + block - 1) / block));
  const int cols = (w + block - 1) / block;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor c = bm.getColor(x, y);
      int& peak =
          peaks[(size_t)(y / block) * (size_t)cols + (size_t)(x / block)];
      peak = std::max({peak, (int)SkColorGetR(c), (int)SkColorGetG(c),
                       (int)SkColorGetB(c)});
    }
  return peaks;
}

}  // namespace

TEST(ComposeCaching, ATurnedRingsBlitLosesNoneOfWhatItBaked) {
  // Rotating a cached ring may resample its edges, but no part of the
  // visible artwork may disappear at any angle.
  const int w = 680, h = 680, block = 16;
  for (float degrees : {0.0f, 7.0f, 45.0f, 90.0f, 137.0f, -60.0f}) {
    choreograph::Output<float> turn{degrees / 360.0f};
    Host cached(w, h), plain(w, h);
    cached.composer.render(turnedRing(Cache::Texture, &turn));
    cached.frame();
    cached.frame();  // the second frame is the blit, not the bake
    EXPECT_EQ(cached.composer.stats().texturesBaked, 0u) << degrees;
    plain.composer.render(turnedRing(Cache::None, &turn));
    plain.frame();
    const std::vector<int> was = blockPeaks(plain, w, h, block);
    const std::vector<int> is = blockPeaks(cached, w, h, block);
    ASSERT_EQ(was.size(), is.size());
    int lit = 0, lost = 0;
    for (size_t i = 0; i < was.size(); ++i) {
      if (was[i] < 200) continue;  // a glyph's solid interior, not its edge
      ++lit;
      if (is[i] < 40) ++lost;
    }
    EXPECT_GT(lit, 40) << "at " << degrees << ": the ring drew nothing";
    EXPECT_EQ(lost, 0) << "at " << degrees << " degrees, " << lost << " of "
                       << lit << " lit blocks came back empty";
  }
}

TEST(ComposeCaching, ARecordedBakesBlitLosesNoneOfWhatItBaked) {
  // The same claim, for the placement that makes the blit's clip a
  // different space: the bake sits inside a PAGE that records.
  //
  // The blit is admitted by a clip on whole DEVICE pixels, and a region
  // clip ignores the matrix — that is what makes it a set of pixels rather
  // than an outline. Inside a recording the canvas's own pixels are not the
  // device's: the ops are replayed under a matrix of their own, and a
  // region computed in the page's space is applied unchanged in the space
  // the page is replayed into. Wrong units, wrong place, and blocks of the
  // ring go missing — which is what a plate is, a page drawn at a view
  // scale.
  const int w = 1200, h = 1200, block = 16;
  const float view = 1.6667f;  // the scale a plate is photographed at
  const auto drawAt = [&](Host& host) {
    SkCanvas* canvas = host.surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    canvas->save();
    canvas->scale(view, view);
    host.composer.draw(*canvas);
    canvas->restore();
  };
  Host cached(w, h), plain(w, h);
  choreograph::Output<float> cachedSlide{0.0f}, plainSlide{0.0f};
  for (Host* host : {&cached, &plain})
    host->composer.setSize({(float)w / view, (float)h / view});
  cached.composer.render(ringInASlidingPage(&cachedSlide, Cache::Texture));
  plain.composer.render(ringInASlidingPage(&plainSlide, Cache::None));
  drawAt(cached);
  // The page slides, its recording holds, and the blit inside it is replayed
  // somewhere else — which is the whole point of a recording, and the state
  // the region has to be right in.
  cachedSlide = 26.0f;
  plainSlide = 26.0f;
  drawAt(cached);
  drawAt(plain);
  const std::vector<int> was = blockPeaks(plain, w, h, block);
  const std::vector<int> is = blockPeaks(cached, w, h, block);
  ASSERT_EQ(was.size(), is.size());
  int lit = 0, lost = 0;
  for (size_t i = 0; i < was.size(); ++i) {
    if (was[i] < 200) continue;  // a glyph's solid interior, not its edge
    ++lit;
    if (is[i] < 40) ++lost;
  }
  EXPECT_GT(lit, 40) << "the ring drew nothing";
  EXPECT_EQ(lost, 0) << lost << " of " << lit
                     << " lit blocks came back empty from inside the page";
}
