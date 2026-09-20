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
//               round, both with `backface(material::Backface::Hidden)`
//               — so one face is drawn and hit at any turn and the
//               other never shows through it.
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

// TAGS: Geometry/Meshes

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/style/Type.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace material = sigil::material;

using namespace sigil::compose;

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

/** THE SIX FACES, front first and back last — which the depth sort must
 *  and does ignore. Each is turned about its own centre and pushed half
 *  an edge along the cube's own axes, which is what `push` counts in. */
struct Face {
  SkColor4f fill;
  const char* name;
  float turnX, turnY;
  float pushX, pushY, pushZ;
};
constexpr Face kFaces[6] = {
    {{0.88f, 0.34f, 0.24f, 1}, "F", 0, 0, 0, 0, 1},
    {{0.95f, 0.72f, 0.20f, 1}, "R", 0, 90, 1, 0, 0},
    {{0.30f, 0.70f, 0.45f, 1}, "T", 90, 0, 0, -1, 0},
    {{0.16f, 0.42f, 0.78f, 1}, "L", 0, -90, -1, 0, 0},
    {{0.62f, 0.36f, 0.78f, 1}, "B", -90, 0, 0, 1, 0},
    {{0.85f, 0.85f, 0.80f, 1}, "K", 0, 180, 0, 0, -1},
};

/** The house sheet in this one's restrained palette: paper, ink and one
 *  accent per panel. Every line on the sheet is set from it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = kGround;
  look.palette.cellGround = kPanel;
  look.palette.ink = kInk;
  look.palette.ash = kAsh;
  return look;
}

/** A panel: a dark plate with its subject standing in the middle of it,
 *  a caption in the corner, and the view every child of the plate is seen
 *  through. It takes an equal share of the row it stands in, so nothing
 *  here divides the canvas by hand. */
Element panel(const char* caption, Element content) {
  return kit::centred(std::move(content))
      .grow(1)
      .corners({10})
      .fill(Fill::color(sketch::kit::theme().palette.cellGround))
      .perspective(kViewDistance)
      .children({text(caption)
                     .font({.size = 13, .color = kAsh, .track = 2})
                     .absolute()
                     .left(18)
                     .bottom(14)});
}

}  // namespace

struct CardFlip {
  choreograph::Output<float> flip{0}, spinX{0}, spinY{0}, sway{0};

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // MID-TURN. At 2.2 s the card is past its quarter turn and the back
    // has just taken over, the cube shows three faces at an oblique, and
    // the plate is near the end of its sway.
    sketch::kit::stage(ctx,
                       {.size = kCanvas,
                        .captureAt = 2.2,
                        .background = sketch::kit::theme().palette.ground});
    flip = 0;
    spinX = 0;
    spinY = 0;
    sway = 0;
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      flip = motion::phase(t, kFlipPeriod);
      spinX = motion::phase(t, kSpinXPeriod);
      spinY = motion::phase(t, kSpinYPeriod);
      sway = (float)std::sin(t * 6.283185 / kSwayPeriod);
    });
    ctx.composer.render(describe());
  }

  /** THE CARD: front and back are two children of one turning host; each
   *  hides its back, so whichever faces the viewer is the one drawn. */
  Element card() const {
    constexpr float w = 220, h = 320;
    const auto face = [](const char* title, const char* line, SkColor4f fill,
                         float turn) {
      return kit::at(0, 0, w, h)
          .corners({16})
          .fill(Fill::color(fill))
          .foreground(stroke(1.5f, Fill::color(kEdge)))
          .column()
          .padding(22)
          .justify(Justify::SpaceBetween)
          .rotateY(turn)
          .backface(material::Backface::Hidden)
          .font({.color = kPaper, .track = 1})
          .children({text(title).font({.size = 30}),
                     text(line).font({.size = 14}).width(pct(100))});
    };
    return box()
        .width(w)
        .height(h)
        .preserve3d()
        .rotateY(motion::bind(&flip).target(0, 360))
        .children({face("FRONT", "rotateY · backface hidden", kCardFront, 0),
                   face("BACK", "pre-turned half round", kCardBack, 180)});
  }

  /** THE CUBE: six planes in one shared space, each turned into place
   *  and pushed half an edge out along the cube's axis. */
  Element cube() const {
    constexpr float edge = 180, half = edge * 0.5f;
    const auto face = [](const Face& one) {
      return kit::at(kit::centred(text(one.name).font({.size = 64})), 0, 0,
                     edge, edge)
          .fill(Fill::color(one.fill))
          .foreground(stroke(1.0f, Fill::color(kEdge)))
          .rotateX(one.turnX)
          .rotateY(one.turnY)
          .translateX(one.pushX * half)
          .translateY(one.pushY * half)
          .translateZ(one.pushZ * half);
    };
    return box()
        .width(edge)
        .height(edge)
        .preserve3d()
        .rotateX(motion::bind(&spinX).target(0, 360))
        .rotateY(motion::bind(&spinY).target(0, 360))
        .children({each(kFaces, face)});
  }

  /** THE PLATE: a paragraph on a plane tipped away, projected at draw. */
  Element plate() const {
    const char* passage =
        "A plane tipped away from the viewer keeps its type: the letters "
        "are shaped and placed in the plane and projected as they are "
        "drawn, so nothing is rasterised flat and resampled. The near edge "
        "is as sharp as the far one, and a cache taken here is taken in "
        "the plane too.";
    return box()
        .width(300)
        .height(250)
        .corners({8})
        .fill(Fill::color(kPaper))
        .padding(22)
        .column()
        .gap(10)
        .transformOrigin(0.5f, 1.0f)  // hinged along its bottom edge
        .rotateX(kTilt)
        .rotateY(motion::bind(&sway).source(-1, 1).target(-14, 14))
        .children({text("TILTED PLATE").font({.size = 18, .track = 3}),
                   text(passage).font({.size = 14}).width(pct(100))});
  }

  Element describe() const {
    constexpr float gap = 20, top = 40;
    const sketch::kit::Theme& look = sketch::kit::theme();
    return stack()
        .fill(Fill::color(look.palette.ground))
        // The sheet's face and ink, inherited by every line: the captions
        // and the title name their ash, the card its paper.
        .font({.face = look.type.sans})
        .ink(look.palette.ink)
        .children(
            {text("THE DEPTH LANES — A NODE IS A PLANE")
                 .font({.size = 14, .color = kAsh, .track = 3})
                 .absolute()
                 .left(gap)
                 .top(14),
             box()
                 .absolute()
                 .inset(gap, top, gap, gap)
                 .row()
                 .gap(gap)
                 .children(
                     {panel("CARD · rotateY under perspective, backs hidden",
                            card()),
                      panel("CUBE · six planes in one space, sorted by depth",
                            cube()),
                      panel("PLATE · type on a tilted plane stays sharp",
                            plate())})});
  }
};

SIGIL_SKETCH(CardFlip, "Kit · Depth",
             "a card flipping on rotateY with its backs hidden, a cube of "
             "six faces depth-sorted in one shared space, and a paragraph "
             "on a tilted plane staying sharp")
