// What a glyph's CENTRE does frame to frame while its run is moving — a
// marquee turning on its baseline, a figure turning above the type, or a
// track carrying the letters on its own schedule. Everything here renders
// consecutive frames at a fixed step and reads the INK back, so the measure
// is what the screen shows rather than what the placement arithmetic
// intended — the rounding a glyph's device origin takes happens below both.

#include "support/TextTestSupport.h"

namespace {

/// The ink centroid of one frame, weighted by coverage. A single glyph on
/// an otherwise empty field, so this IS the glyph's centre.
SkPoint inkCentroid(Host& host, int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  double sx = 0, sy = 0, sw = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const double weight = SkColorGetG(bm.getColor(x, y)) / 255.0;
      sx += weight * x;
      sy += weight * y;
      sw += weight;
    }
  if (sw <= 0) return {-1, -1};
  return {(float)(sx / sw), (float)(sy / sw)};
}

struct MotionStats {
  double rmsJerk = 0;   ///< RMS of |c[i+1] - 2c[i] + c[i-1]|, px
  double meanStep = 0;  ///< mean |c[i] - c[i-1]|, px
  double minStep = 1e9;
};

/// The track's steps and its second differences. True motion along a ring
/// this large has a second difference of v²/r — thousandths of a pixel at
/// these step sizes — so whatever a quantizer does is the whole signal.
MotionStats motionOf(const std::vector<SkPoint>& track) {
  MotionStats s;
  double sum = 0;
  int n = 0;
  for (size_t i = 1; i + 1 < track.size(); ++i) {
    const double jerk =
        std::hypot(track[i + 1].x() - 2 * track[i].x() + track[i - 1].x(),
                   track[i + 1].y() - 2 * track[i].y() + track[i - 1].y());
    sum += jerk * jerk;
    ++n;
  }
  s.rmsJerk = n ? std::sqrt(sum / n) : 0;
  double total = 0;
  for (size_t i = 1; i < track.size(); ++i) {
    const double d = std::hypot(track[i].x() - track[i - 1].x(),
                                track[i].y() - track[i - 1].y());
    s.minStep = std::min(s.minStep, d);
    total += d;
  }
  if (track.size() > 1) s.meanStep = total / (double)(track.size() - 1);
  return s;
}

constexpr int kField = 400;

// Slow enough that a whole pixel cannot hide inside one frame's travel:
// about half a pixel of arc per frame at this radius.
constexpr float kPhaseStep = 1.0f / 2400.0f;

constexpr int kFrames = 90;

Element ringAt(sigil::motion::Animatable<float> at, float pixelSize) {
  return box().child(text(u8"H", whiteStyle(pixelSize))
                         .key("ring")
                         .width(kField)
                         .height(kField)
                         .absolute()
                         .left(0)
                         .top(0)
                         .onPath({.path = geometry::shapes::circle(),
                                  .at = std::move(at),
                                  .align = TextPath::Align::Center}));
}

/// ONE LETTER riding a ring whose phase is BOUND — the marquee's own
/// idiom, and the way the run declares that it is turning. A single letter
/// so the frame's whole ink is the glyph and the centroid needs no
/// segmentation.
std::vector<SkPoint> ringTrack(float pixelSize) {
  Host host(kField, kField);
  choreograph::Output<float> phase{0.05f};
  std::vector<SkPoint> track;
  for (int i = 0; i < kFrames; ++i) {
    phase = 0.05f + kPhaseStep * (float)i;
    host.composer.render(ringAt(&phase, pixelSize));
    host.frame();
    track.push_back(inkCentroid(host, kField, kField));
  }
  return track;
}

/// The same ring wearing one fx track, with its baseline phase written as a
/// PLAIN NUMBER — the run declares nothing through the baseline, so whatever
/// grid its glyphs land on is the track's answer and nobody else's.
Element ringWith(float at, float pixelSize, Track track) {
  return box().child(text(u8"H", whiteStyle(pixelSize))
                         .key("ring")
                         .width(kField)
                         .height(kField)
                         .absolute()
                         .left(0)
                         .top(0)
                         .onPath({.path = geometry::shapes::circle(),
                                  .at = at,
                                  .align = TextPath::Align::Center})
                         .fx(std::move(track)));
}

/// The most distinct frames a WHOLE-PIXEL origin can produce over that
/// slide, which is arithmetic rather than a measurement: 1.5 px of travel
/// crosses at most two pixel boundaries, and a run re-uses one
/// rasterization between crossings, so at most three frames differ. A
/// subpixel grid is not bounded by it -- every step lands somewhere new --
/// so the two answers are separated by the count itself and by no fitted
/// number.
constexpr int kWholePixelCeiling = 3;

/// How many DISTINCT frames a run produces as it slides through a pixel
/// and a half of arc.
int distinctFramesAcrossOnePixel(Host& host,
                                 const std::function<Element(float)>& scene) {
  constexpr int kSteps = 8;
  constexpr float kSpan = 1.5f / (2.0f * 3.14159265f * 200.0f);
  std::vector<SkBitmap> frames;
  for (int i = 0; i < kSteps; ++i) {
    host.composer.render(scene(0.05f + kSpan * (float)i / (float)(kSteps - 1)));
    host.frame();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kField, kField));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    frames.push_back(std::move(bm));
  }
  auto same = [](const SkBitmap& a, const SkBitmap& b) {
    for (int y = 0; y < kField; ++y)
      for (int x = 0; x < kField; ++x)
        if (a.getColor(x, y) != b.getColor(x, y)) return false;
    return true;
  };
  int distinct = 0;
  for (size_t i = 0; i < frames.size(); ++i) {
    bool seen = false;
    for (size_t j = 0; j < i; ++j) seen |= same(frames[i], frames[j]);
    distinct += !seen;
  }
  return distinct;
}

}  // namespace

namespace {

/// THE OTHER WAY A RING TURNS: the phase stands still and a transform
/// above the text rotates the whole figure. Its letters creep across the
/// device exactly as a marquee's do, so the same declaration has to reach
/// them -- through the node's placement rather than through its baseline.
std::vector<SkPoint> turnedFigureTrack(float pixelSize) {
  Host host(kField, kField);
  choreograph::Output<float> spin{0.0f};
  std::vector<SkPoint> track;
  for (int i = 0; i < kFrames; ++i) {
    spin = 0.15f * (float)i;
    host.composer.render(box().child(box()
                                         .key("figure")
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(kField)
                                         .height(kField)
                                         .rotate(&spin)
                                         .child(ringAt(0.05f, pixelSize))));
    host.frame();
    track.push_back(inkCentroid(host, kField, kField));
  }
  return track;
}

/// THE THIRD WAY A RUN CREEPS: nothing above the type is turning and its
/// baseline is a level line standing still, but a live track carries every
/// letter across the device on its own schedule.
///
/// The master is driven so the LETTER's travel is uniform, which is what a
/// constant angular step buys the ring: `fx::slide` places a glyph at
/// (1-t)^3 of its distance, so stepping the inverse of that curve advances
/// the letter by exactly half a pixel a frame. Half a pixel is the
/// interval that separates the two placements -- two steps of the finer
/// grid, and never enough to be sure of crossing a whole-pixel boundary.
/// The window sits where the slide's own fade has long since finished, so
/// the letter is solid ink throughout and the centroid is measuring
/// placement alone.
std::vector<SkPoint> slidingTrack(float pixelSize) {
  constexpr float kDistance = 640.0f;
  constexpr float kFromPx = 40.0f;  // where in the slide the window opens
  constexpr float kTravel = 0.5f;   // …and how far the letter goes per frame
  constexpr int kSlideFrames = 40;
  Host host(kField, kField);
  choreograph::Output<float> progress{0.0f};
  std::vector<SkPoint> track;
  for (int i = 0; i < kSlideFrames; ++i) {
    progress = 1.0f - std::cbrt((kFromPx - kTravel * (float)i) / kDistance);
    host.composer.render(box().child(
        text(u8"H", whiteStyle(pixelSize))
            .key("run")
            .width(kField)
            .height(kField)
            .absolute()
            .left(0)
            .top(0)
            .fx({.effect = fx::slide(kDistance), .progress = &progress})));
    host.frame();
    track.push_back(inkCentroid(host, kField, kField));
  }
  return track;
}

/// One declaration that a run is moving, at one type size.
///
/// `uniformAdvance` says whether the DECLARATION makes the letter's travel
/// constant frame to frame. A ring stepped by a constant angle does; a
/// slide driven through the inverse of its own easing curve only
/// approximately does, so its second difference carries real motion and a
/// jerk bound there would be measuring the driver rather than the library.
struct MovingRun {
  const char* what;
  float pixelSize;
  std::vector<SkPoint> (*track)(float);
  bool uniformAdvance;
};

class RunInMotion : public testing::TestWithParam<MovingRun> {};

}  // namespace

// A MOVING RUN'S LETTERS MOVE SMOOTHLY, however the motion was declared and
// at every size. The placement is continuous in whatever drives it, so
// every quantizer between that placement and the pixels shows up here and
// in nothing else: the ladder a tangent snaps to, and the grid a glyph's
// device origin lands on.
//
// Two bounds, because the two failures look different. `minStep` catches
// the whole-pixel origin, whose signature is a letter that does not move AT
// ALL for a frame or two and then hops a whole pixel -- the step goes
// exactly to zero, the same mask drawn in the same place. `rmsJerk` catches
// a rotation ladder coarse enough to tick, which varies the step without
// ever stopping it. Both are stated against the run's own mean step, so
// neither is a number fitted to this machine.
TEST_P(RunInMotion, ItsLettersAdvanceWithoutStallingOrTicking) {
  const MotionStats s = motionOf(GetParam().track(GetParam().pixelSize));
  SCOPED_TRACE(testing::Message()
               << "meanStep " << s.meanStep << " minStep " << s.minStep
               << " rmsJerk " << s.rmsJerk);
  ASSERT_GT(s.meanStep, 0.1) << "the run did not move at all";
  EXPECT_GT(s.minStep, 0.25 * s.meanStep) << "a frame the letter stood still";
  if (GetParam().uniformAdvance)
    EXPECT_LT(s.rmsJerk, 0.5 * s.meanStep) << "the letter's advance ticks";
}

INSTANTIATE_TEST_SUITE_P(
    ComposePathMotion, RunInMotion,
    testing::Values(
        MovingRun{"ABoundBaselinePhaseSmall", 14.0f, ringTrack, true},
        MovingRun{"ABoundBaselinePhaseLarge", 44.0f, ringTrack, true},
        MovingRun{"AnAncestorsRotationSmall", 14.0f, turnedFigureTrack, true},
        MovingRun{"AnAncestorsRotationLarge", 44.0f, turnedFigureTrack, true},
        MovingRun{"ALiveDisplacingTrackSmall", 14.0f, slidingTrack, false},
        MovingRun{"ALiveDisplacingTrackLarge", 44.0f, slidingTrack, false}),
    [](const testing::TestParamInfo<MovingRun>& info) {
      return info.param.what;
    });

// …AND TYPE AT REST KEEPS WHOLE-PIXEL ORIGINS, which is the other half of
// the contract and the half that costs nothing. A phase written as a plain
// number declares no motion, so a run slid a pixel and a half along its
// baseline re-uses one rasterization until its origin crosses a pixel
// boundary; the same slide with the phase BOUND lands on the subpixel grid
// and reads differently nearly every step.
//
// Counted over eight positions rather than compared between two, so
// neither verdict can be an accident of where inside a pixel the run
// happened to start.
TEST(ComposePathMotion, TypeAtRestKeepsWholePixelOrigins) {
  Host resting(kField, kField);
  const int atRest = distinctFramesAcrossOnePixel(
      resting, [](float at) { return ringAt(at, 44.0f); });

  Host turning(kField, kField);
  choreograph::Output<float> phase{0.05f};
  const int inMotion = distinctFramesAcrossOnePixel(turning, [&](float at) {
    phase = at;
    return ringAt(&phase, 44.0f);
  });

  EXPECT_LE(atRest, kWholePixelCeiling)
      << "a resting run is paying for the subpixel grid";
  EXPECT_GT(inMotion, kWholePixelCeiling)
      << "a turning run is rounding to whole pixels";
}

// …AND A TRACK THAT MOVES NOTHING BUYS NOTHING. A fade is a coverage ramp:
// every pen position is where the layout put it on every frame of it, so the
// run is type at rest however hard its progress is running, and it keeps the
// whole-pixel origins and the cheap atlas that go with that.
TEST(ComposePathMotion, AFadeOnlyTrackKeepsWholePixelOrigins) {
  Host host(kField, kField);
  choreograph::Output<float> progress{0.5f};  // bound: the track IS live
  const TextEffect fade = fx::keys({{0.0f, {.alpha = 0.0f}}, {1.0f, {}}});
  const int distinct = distinctFramesAcrossOnePixel(host, [&](float at) {
    return ringWith(at, 44.0f, {.effect = fade, .progress = &progress});
  });
  EXPECT_LE(distinct, kWholePixelCeiling)
      << "a fade-only track is paying for the subpixel grid";
}

// A TABLE IS ANSWERED BY ITS OWN ENTRIES, and the two verdicts are measured
// the same way so neither can be an accident of the measurement: the same
// ring, the same live progress, the same slide of a pixel and a half — only
// the lane the table publishes into differs.
TEST(ComposePathMotion, AKeysTableEngagesTheGridOnlyWhereItMovesGlyphs) {
  choreograph::Output<float> progress{0.5f};
  const TextEffect colourOnly =
      fx::keys({{0.0f, {.colorMul = {0.3f, 0.3f, 0.3f, 1.0f}}}, {1.0f, {}}});
  const TextEffect offset = fx::keys({{0.0f, {.dx = 9.0f}}, {1.0f, {}}});

  Host cheapHost(kField, kField);
  const int cheap = distinctFramesAcrossOnePixel(cheapHost, [&](float at) {
    return ringWith(at, 44.0f, {.effect = colourOnly, .progress = &progress});
  });
  Host movingHost(kField, kField);
  const int moving = distinctFramesAcrossOnePixel(movingHost, [&](float at) {
    return ringWith(at, 44.0f, {.effect = offset, .progress = &progress});
  });

  EXPECT_LE(cheap, kWholePixelCeiling)
      << "a colour-only table is paying for the subpixel grid";
  EXPECT_GT(moving, kWholePixelCeiling)
      << "a table with an offset is rounding to whole pixels";
}

// A SETTLED TRACK IS TYPE AT REST, whatever it does while it runs. Its
// glyphs are standing somewhere else and standing still, which is what
// whole-pixel origins are for — and the proof is bytes: a settled slide
// deviates nothing at all, so its frame must be the frame with no track on
// it, mask for mask. Read off the declaration, so the last frame of the
// motion and the first frame of the rest do not disagree.
TEST(ComposePathMotion, ASettledDisplacingTrackReturnsToWholePixels) {
  const auto shot = [](Host& host, const Element& tree) {
    host.composer.render(tree);
    host.frame();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kField, kField));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  Host bare(kField, kField);
  const SkBitmap untracked = shot(bare, ringAt(0.05f, 44.0f));
  Host settled(kField, kField);
  const SkBitmap withTrack =
      shot(settled,
           ringWith(0.05f, 44.0f, {.effect = fx::slide(), .progress = 1.0f}));
  for (int y = 0; y < kField; ++y)
    for (int x = 0; x < kField; ++x)
      ASSERT_EQ(untracked.getColor(x, y), withTrack.getColor(x, y))
          << "a settled slide moved the ink at " << x << "," << y;

  Host sliding(kField, kField);
  const int distinct = distinctFramesAcrossOnePixel(sliding, [](float at) {
    return ringWith(at, 44.0f, {.effect = fx::slide(), .progress = 1.0f});
  });
  EXPECT_LE(distinct, kWholePixelCeiling)
      << "a settled track is paying for the subpixel grid";
}

// A TRACK'S OWN ROTATION TAKES THE SAME LADDER as the baseline's tangent.
// The two compose onto one letter — the baseline turns it to the curve and
// the track turns it further — so a ladder coarse on either side is the
// coarsest thing in the letter's motion and ticks whatever the other side
// does.
//
// Counted as DISTINCT RENDERINGS across a sweep, which is what a ladder
// decides and nothing else does: turned through twelve of its own steps a
// letter must answer twelve different frames, where a ladder several times
// coarser answers the same two or three over and over and the letter sits
// still between them. The sweep is cut by size for the same reason the
// ladder is, so the count means the same thing at both.
TEST(ComposePathMotion, ATrackRotationTurnsOnTheSameLadderAsTheBaseline) {
  constexpr int kSteps = 12;
  for (const float size : {14.0f, 44.0f}) {
    // Twelve steps of a ladder cut at sixteen per pixel of em.
    const float sweep = (float)kSteps * 360.0f / (16.0f * size);
    const TextEffect turn(
        "turn", {},
        [sweep](const GlyphInfo&, float t, sigil::core::noise::Mix64Stream&) {
          GlyphMod m;
          m.rotateDeg = sweep * t;
          return m;
        },
        60.0f);
    Host host(kField, kField);
    choreograph::Output<float> phase{0.05f};  // bound, and standing still
    choreograph::Output<float> progress{0.0f};
    std::vector<SkBitmap> frames;
    frames.reserve(kSteps);
    for (int i = 0; i < kSteps; ++i) {
      progress = (float)i / (float)(kSteps - 1);
      host.composer.render(
          box().child(text(u8"H", whiteStyle(size))
                          .key("ring")
                          .width(kField)
                          .height(kField)
                          .absolute()
                          .left(0)
                          .top(0)
                          .onPath({.path = geometry::shapes::circle(),
                                   .at = &phase,
                                   .align = TextPath::Align::Center})
                          .fx({.effect = turn, .progress = &progress})));
      host.frame();
      SkBitmap bm;
      bm.allocPixels(SkImageInfo::MakeN32Premul(kField, kField));
      host.surface->readPixels(bm.pixmap(), 0, 0);
      frames.push_back(std::move(bm));
    }
    auto same = [](const SkBitmap& a, const SkBitmap& b) {
      for (int y = 0; y < kField; ++y)
        for (int x = 0; x < kField; ++x)
          if (a.getColor(x, y) != b.getColor(x, y)) return false;
      return true;
    };
    int distinct = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
      bool seen = false;
      for (size_t j = 0; j < i; ++j) seen |= same(frames[i], frames[j]);
      distinct += !seen;
    }
    SCOPED_TRACE(testing::Message() << "size " << size << " sweep " << sweep
                                    << " deg, distinct " << distinct);
    // A ladder twice as coarse as the sweep can answer at most half the
    // steps, so half is where the two verdicts part -- a derived bound
    // rather than a fitted one.
    EXPECT_GT(distinct, kSteps / 2)
        << "the track's rotation is on a coarser ladder than the "
           "baseline's tangent";
  }
}
