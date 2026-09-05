/** @file
 * card flip — the depth lanes, the CSS way: a card turning on rotateY
 * under a perspective with its backs hidden, a cube of six faces sharing
 * one space and drawn back to front, and a paragraph on a tilted plane
 * staying sharp because it is projected at draw, never resampled.
 */

// Three panels, one model. A node is a PLANE: the depth lanes turn it and
// move it, and it projects onto the plane its parent paints on — one 4x4
// per node, flattened at paint, so tree order stays draw order and every
// cache a node holds lives in its own plane exactly as before. What this
// is not: a scene. Nothing intersects and nothing is lit; a set is where
// that lives.
//
//   THE CARD    a preserve3d host turning on a bound rotateY under a
//               perspective, holding a front and a back pre-turned half
//               round, both with `backface(Backface::Hidden)` — so one
//               face is drawn and hit at any turn and the other never
//               shows through it.
//   THE CUBE    six faces of one preserve3d host, each turned about its
//               own centre and moved half an edge along the cube's axis
//               in the host's frame (the lanes compose with the translate
//               outermost, so a face is `rotateY(90).translateX(w/2)`),
//               depth-sorted by the z of their centres every frame while
//               the host turns on two bound lanes. Declared front first
//               and back last, which the sort must and does ignore.
//   THE PLATE   a paragraph on a plane tipped away by rotateX under a
//               view, swaying on a bound rotateY. The type is shaped and
//               placed in the plane and projected at draw, so the near
//               edge is as sharp as the far one.
//
// The three phases are Outputs driven from one ticker lambda and
// re-zeroed in setup(), because a scene can be activated more than once.
//
// EDIT THESE FIRST
//   kFlipPeriod        — seconds per full turn of the card.
//   kViewDistance      — the viewer's distance in front of each panel;
//                        shorter is a stronger projection.
//   kTilt              — the plate's pitch away from the viewer, degrees.

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1200, 560};
constexpr float kFlipPeriod = 6.0f;    // seconds per turn of the card
constexpr float kSpinXPeriod = 11.0f;  // the cube's pitch period
constexpr float kSpinYPeriod = 7.0f;   // the cube's yaw period
constexpr float kSwayPeriod = 5.0f;    // the plate's sway period
constexpr float kViewDistance = 900;   // px, in front of each panel
constexpr float kTilt = 38;            // the plate's pitch, degrees

// A restrained palette: paper, ink, and one accent per panel.
constexpr SkColor4f kGround{0.055f, 0.06f, 0.075f, 1};
constexpr SkColor4f kPanel{0.09f, 0.10f, 0.12f, 1};
constexpr SkColor4f kPaper{0.93f, 0.91f, 0.86f, 1};
constexpr SkColor4f kInk{0.10f, 0.10f, 0.12f, 1};
constexpr SkColor4f kAsh{0.56f, 0.57f, 0.62f, 1};
constexpr SkColor4f kCardFront{0.88f, 0.34f, 0.24f, 1};
constexpr SkColor4f kCardBack{0.16f, 0.42f, 0.78f, 1};
constexpr SkColor4f kEdge{1, 1, 1, 0.22f};

/** The six faces' colours, front first and back last. */
constexpr SkColor4f kFaces[6] = {
    {0.88f, 0.34f, 0.24f, 1}, {0.95f, 0.72f, 0.20f, 1}, {0.30f, 0.70f, 0.45f, 1},
    {0.16f, 0.42f, 0.78f, 1}, {0.62f, 0.36f, 0.78f, 1}, {0.85f, 0.85f, 0.80f, 1},
};
constexpr const char* kFaceNames[6] = {"F", "R", "T", "L", "B", "K"};

weave::TextStyle type(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** A panel: a dark plate with a caption under it, and the view every
 *  child of the plate is seen through. */
Element panel(SkRect frame, const char* caption) {
  return box()
      .absolute()
      .rect(frame)
      .corners({10})
      .fill(Fill::color(kPanel))
      .perspective(kViewDistance)
      .child(text(toU8(caption), type(13, kAsh, 2))
                 .absolute()
                 .left(18)
                 .bottom(14));
}

}  // namespace

struct CardFlip final : sketch::Sketch {
  choreograph::Output<float> flip{0}, spinX{0}, spinY{0}, sway{0};

  void setup(sketch::SketchContext& ctx) override {
    // MID-TURN. At 2.2 s the card is past its quarter turn and the back
    // has just taken over, the cube shows three faces at an oblique, and
    // the plate is near the end of its sway.
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 2.2,
                        .background = kGround});
    flip = 0;
    spinX = 0;
    spinY = 0;
    sway = 0;
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      flip = motion::phase(t, kFlipPeriod);
      spinX = motion::phase(t, kSpinXPeriod);
      spinY = motion::phase(t, kSpinYPeriod);
      sway = (float)std::sin(t * 6.283185 / kSwayPeriod);
      return true;
    });
    ctx.composer.render(describe());
  }

  /** THE CARD: front and back are two children of one turning host; each
   *  hides its back, so whichever faces the viewer is the one drawn. */
  Element card() const {
    constexpr float w = 220, h = 320;
    const auto face = [&](const char* title, const char* line,
                          SkColor4f fill, float turn) {
      return box()
          .absolute()
          .rect(SkRect::MakeXYWH(0, 0, w, h))
          .corners({16})
          .fill(Fill::color(fill))
          .foreground(stroke(1.5f, Fill::color(kEdge)))
          .column()
          .padding(22)
          .justify(Justify::SpaceBetween)
          .rotateY(turn)
          .backface(Backface::Hidden)
          .child(text(toU8(title), type(30, kPaper, 1)))
          .child(text(toU8(line), type(14, kPaper, 1)).width(pct(100)));
    };
    return box()
        .absolute()
        .rect(SkRect::MakeXYWH((380 - w) * 0.5f, (440 - h) * 0.5f, w, h))
        .preserve3d()
        .rotateY(motion::bind(&flip).target(0, 360))
        .child(face("FRONT", "rotateY · backface hidden", kCardFront, 0))
        .child(face("BACK", "pre-turned half round", kCardBack, 180));
  }

  /** THE CUBE: six planes in one shared space, each turned into place
   *  and pushed half an edge out along the cube's axis. */
  Element cube() const {
    constexpr float edge = 180, half = edge * 0.5f;
    const auto face = [&](int i) {
      return box()
          .absolute()
          .rect(SkRect::MakeXYWH(0, 0, edge, edge))
          .fill(Fill::color(kFaces[i]))
          .foreground(stroke(1.0f, Fill::color(kEdge)))
          .alignItems(Align::Center)
          .justify(Justify::Center)
          .child(text(toU8(kFaceNames[i]), type(64, kInk)));
    };
    return box()
        .absolute()
        .rect(SkRect::MakeXYWH((380 - edge) * 0.5f, (440 - edge) * 0.5f, edge,
                               edge))
        .preserve3d()
        .rotateX(motion::bind(&spinX).target(0, 360))
        .rotateY(motion::bind(&spinY).target(0, 360))
        .child(face(0).translateZ(half))
        .child(face(1).rotateY(90).translateX(half))
        .child(face(2).rotateX(90).translateY(-half))
        .child(face(3).rotateY(-90).translateX(-half))
        .child(face(4).rotateX(-90).translateY(half))
        .child(face(5).rotateY(180).translateZ(-half));
  }

  /** THE PLATE: a paragraph on a plane tipped away, projected at draw. */
  Element plate() const {
    constexpr float w = 300, h = 250;
    const char* passage =
        "A plane tipped away from the viewer keeps its type: the letters "
        "are shaped and placed in the plane and projected as they are "
        "drawn, so nothing is rasterised flat and resampled. The near edge "
        "is as sharp as the far one, and a cache taken here is taken in "
        "the plane too.";
    return box()
        .absolute()
        .rect(SkRect::MakeXYWH((380 - w) * 0.5f, (440 - h) * 0.5f, w, h))
        .corners({8})
        .fill(Fill::color(kPaper))
        .padding(22)
        .column()
        .gap(10)
        .transformOrigin(0.5f, 1.0f)  // hinged along its bottom edge
        .rotateX(kTilt)
        .rotateY(motion::bind(&sway).source(-1, 1).target(-14, 14))
        .child(text(toU8("TILTED PLATE"), type(18, kInk, 3)))
        .child(text(toU8(passage), type(14, kInk)).width(pct(100)));
  }

  Element describe() const {
    constexpr float gap = 20, top = 40, ph = 440;
    constexpr float pw = (kCanvas.fWidth - 4 * gap) / 3;
    return stack()
        .fill(Fill::color(kGround))
        .child(text(toU8("THE DEPTH LANES \xe2\x80\x94 A NODE IS A PLANE"),
                    type(14, kAsh, 3))
                   .absolute()
                   .left(gap)
                   .top(14))
        .child(panel(SkRect::MakeXYWH(gap, top, pw, ph),
                     "CARD \xc2\xb7 rotateY under perspective, backs hidden")
                   .child(card()))
        .child(panel(SkRect::MakeXYWH(2 * gap + pw, top, pw, ph),
                     "CUBE \xc2\xb7 six planes in one space, sorted by depth")
                   .child(cube()))
        .child(panel(SkRect::MakeXYWH(3 * gap + 2 * pw, top, pw, ph),
                     "PLATE \xc2\xb7 type on a tilted plane stays sharp")
                   .child(plate()));
  }
};

SIGIL_SKETCH(CardFlip, "Kit \xc2\xb7 Depth",
             "a card flipping on rotateY with its backs hidden, a cube of "
             "six faces depth-sorted in one shared space, and a paragraph "
             "on a tilted plane staying sharp")
