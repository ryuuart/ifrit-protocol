// beethoven_study.cpp — a STUDY of Josef Müller-Brockmann's "beethoven"
// (Tonhalle Zürich, 1955). REFERENCES.md §7 — every number below is from
// the measured arc table (pixel analysis of the archival scan): center at
// (0.2693W, 0.7156H), ring widths doubling 1:2:4:8:16 (u = 0.0140W), every
// arc end an exact multiple of the 11.25° module.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/beethoven_study.cpp
//
// Construction is pure composition: each arc run is a circle OUTLINE that
// starts its own path at the run's canvas start angle, stroked at the
// ring's width and revealed with trim() — the self-drawing primitive. The
// reveal animates in the poster's OWN units (the §7 motion inference):
// ring durations DOUBLE inward-out (120/240/480/960/1920ms), sweeps run
// linearly — geometric progressions, never decorative eases.
//
// Headless: ComposeSketch <this> --frame beethoven.png --at 3.2
// (mid-reveal at --at 0.8)

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Layouts.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

constexpr float W = 720.0f;
constexpr float H = W * 1.4144f; // the Weltformat √2 rectangle

constexpr SkColor4f kPaper{0.961f, 0.953f, 0.933f, 1}; // #F5F3EE
constexpr SkColor4f kInk{0.066f, 0.062f, 0.058f, 1};

// The measured arc table (REFERENCES.md §7). Angles in MATH convention
// (0°=+x, CCW+, y-up); canvas angle = −math angle (y-down, CW+).
struct Run {
  float rInner, rOuter;   // × W
  float startDeg, endDeg; // math convention
};
const std::vector<Run> kRuns = {
    {0.3480f, 0.3607f, -33.75f, 22.50f},
    {0.3480f, 0.3607f, -146.25f, -135.00f},
    {0.3640f, 0.3913f, -22.50f, 45.00f},
    {0.3640f, 0.3913f, -157.50f, -90.00f},
    {0.3940f, 0.4473f, 0.00f, 78.75f},
    {0.3940f, 0.4473f, -135.00f, -45.00f},
    {0.4500f, 0.5560f, -150.00f, 90.00f},
    {0.5600f, 0.7770f, -150.00f, 45.00f},
    {0.7820f, 1.2070f, -150.00f, 101.25f},
};
// Ring index per run (for the doubling reveal durations).
const std::vector<int> kRing = {0, 0, 1, 1, 2, 2, 3, 4, 5};

sigil::weave::TextStyle type(float size, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(kInk.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

} // namespace

struct BeethovenStudy : sketch::Sketch {
  bool revealed = false;

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kPaper);
    revealed = false;
    ctx.composer.render(describe(0)); // everything at trim ~0
  }

  /** One arc run: a box centered on the poster's arc center, sized to the
   *  run's mid-radius circle, whose outline STARTS at the run's canvas
   *  start angle — so trim(0 → sweep/360) reveals exactly the measured run. */
  Element arcRun(const Run &run, int ring, int phase) {
    const SkPoint C{0.2693f * W, 0.7156f * H};
    const float rMid = (run.rInner + run.rOuter) * 0.5f * W;
    const float width = (run.rOuter - run.rInner) * W;
    const float canvasStart = -run.endDeg; // y-down mapping
    const float sweep = run.endDeg - run.startDeg;
    const float span = std::min(sweep / 360.0f, 0.9995f);

    PathFormat inkStroke;
    inkStroke.width = width;
    inkStroke.strokeFill = Fill::color(kInk);

    Element e =
        box().width(2 * rMid).height(2 * rMid)
            .inset(C.x() - rMid, C.y() - rMid, W - C.x() - rMid,
                   H - C.y() - rMid)
            .absolute()
            .outline([canvasStart](SkSize s) {
              SkPathBuilder b;
              b.addArc(SkRect::MakeWH(s.width(), s.height()), canvasStart,
                       359.9f);
              return b.detach();
            })
            .stroke(inkStroke);
    if (phase == 0) {
      e.trim(0.0f, 0.0001f);
    } else {
      // The §7 motion rule: durations double per ring, linear sweeps.
      const auto duration =
          std::chrono::milliseconds(120 << std::min(ring, 5));
      e.trim(0.0f, with(span, {duration, &ch::easeNone}));
    }
    return e;
  }

  Element describe(int phase) {
    auto poster = stack().fill(Fill::color(kPaper));
    for (size_t i = 0; i < kRuns.size(); ++i)
      poster.child(arcRun(kRuns[i], kRing[i], phase)
                       .key("arc" + std::to_string(i)));

    // The type block: flush-left, upper-left quadrant, Akzidenz-class
    // grotesque, lowercase-forward — snapped to a baseline rhythm.
    // Type block: flush-left grotesque, lowercase-forward. Placed in the
    // paper bowl left of the arc center (adapted from the original's
    // top-left seat so the study's arc system stays unobstructed).
    poster.child(
        box().column().absolute()
            .inset(0.066f * W, 0.315f * H, 0.42f * W, 0.47f * H)
            .zIndex(2)
            .child(text(toU8("beethoven"), type(58, 0.5f)).key("title"))
            .child(text(toU8("tonhalle grosser saal"), type(15, 0.4f))
                       .margin(0, 14, 0, 0))
            .child(text(toU8("donnerstag, den 27. januar 1955"),
                        type(15, 0.4f))
                       .margin(0, 7, 0, 0))
            .child(text(toU8("erich schmid · géza anda"), type(15, 0.4f))
                       .margin(0, 7, 0, 0)));
    return poster;
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    if (!revealed && elapsed > 0.15) {
      revealed = true;
      ctx.composer.render(describe(1)); // the doubling-duration reveal
    }
  }
};

SIGIL_SKETCH(BeethovenStudy)
