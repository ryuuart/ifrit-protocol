// Motion on a curved baseline: what a glyph's CENTRE does frame to frame
// while a marquee turns. Everything here renders consecutive frames at a
// fixed phase step and reads the INK back, so the measure is what the
// screen shows rather than what the placement arithmetic intended — the
// rounding a glyph's device origin takes happens below both.

#include "ComposeTestSupport.h"

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

Element ringAt(Animatable<float> at, float pixelSize) {
  return box().child(text(u8"H", whiteStyle(pixelSize))
                         .key("ring")
                         .width(kField)
                         .height(kField)
                         .absolute()
                         .left(0)
                         .top(0)
                         .onPath({.path = shapes::circle(),
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

/// How many DISTINCT frames a run produces as it slides through a pixel
/// and a half of arc. Whole-pixel origins re-use one rasterization until
/// the origin crosses a boundary, so the count is the number of crossings;
/// the subpixel phase grid gives a fresh one nearly every step.
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

// A TURNING RING'S LETTERS MOVE SMOOTHLY, at every size. The placement is
// continuous in the phase, so every quantizer between that placement and
// the pixels shows up here and in nothing else: the ladder the tangent
// snaps to, and the grid the glyph's device origin lands on.
//
// Two bounds, because the two failures look different. `minStep` catches
// the whole-pixel origin, whose signature is a letter that does not move AT
// ALL for a frame or two and then hops a whole pixel — the step goes
// exactly to zero, the same mask drawn in the same place. `rmsJerk`
// catches a rotation ladder coarse enough to tick, which varies the step
// without ever stopping it.
TEST(ComposePathMotion, ATurningRingAdvancesSmoothlyAtEverySize) {
  for (const float size : {14.0f, 44.0f}) {
    const MotionStats s = motionOf(ringTrack(size));
    SCOPED_TRACE(testing::Message()
                 << "size " << size << " meanStep " << s.meanStep << " minStep "
                 << s.minStep << " rmsJerk " << s.rmsJerk);
    ASSERT_GT(s.meanStep, 0.1) << "the ring did not turn";
    EXPECT_GT(s.minStep, 0.25 * s.meanStep) << "a frame the letter stood still";
    EXPECT_LT(s.rmsJerk, 0.5 * s.meanStep) << "the letter's advance ticks";
  }
}

// The OTHER way a ring turns: the phase stands still and a transform above
// the text rotates the whole figure. Its letters creep across the device
// exactly as a marquee's do, so the same declaration has to reach them —
// through the node's placement rather than through its baseline.
TEST(ComposePathMotion, ARingTurnedByAnAncestorAdvancesSmoothly) {
  for (const float size : {14.0f, 44.0f}) {
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
                                           .child(ringAt(0.05f, size))));
      host.frame();
      track.push_back(inkCentroid(host, kField, kField));
    }
    const MotionStats s = motionOf(track);
    SCOPED_TRACE(testing::Message()
                 << "size " << size << " meanStep " << s.meanStep << " minStep "
                 << s.minStep << " rmsJerk " << s.rmsJerk);
    ASSERT_GT(s.meanStep, 0.1) << "the figure did not turn";
    EXPECT_GT(s.minStep, 0.25 * s.meanStep) << "a frame the letter stood still";
    EXPECT_LT(s.rmsJerk, 0.5 * s.meanStep) << "the letter's advance ticks";
  }
}

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

  EXPECT_LE(atRest, 3) << "a resting run is paying for the subpixel grid";
  EXPECT_GE(inMotion, 6) << "a turning run is rounding to whole pixels";
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
        [sweep](const GlyphInfo&, float t, sigil::compose::Rng&) {
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
                          .onPath({.path = shapes::circle(),
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
    EXPECT_GE(distinct, 9) << "the track's rotation is on a coarser ladder "
                              "than the baseline's tangent";
  }
}
