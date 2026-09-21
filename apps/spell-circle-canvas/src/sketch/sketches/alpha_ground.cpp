/** @file
 * alpha_ground — a mark on NOTHING, for whoever composites it.
 *
 * Every other sketch here declares a ground and draws over it. This one
 * declares `{0, 0, 0, 0}`: a colour with no colour and no coverage. The
 * pixels the mark does not reach carry nothing, and the pixels at its
 * soft edges carry their own coverage and no more — which is what lets
 * another application lay the whole frame over its own scene and see its
 * own scene through it.
 *
 * IT IS WHAT THE PUBLICATION DOOR IS FOR. A frame offered under
 * `--publish` is the canvas declared here, at the size declared here,
 * over the ground declared here — so a VJ program, a projection mapper
 * or a recorder subscribing to it receives a 640 by 360 overlay it can
 * composite, and not a picture of the window this was watched in.
 *
 *     build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
 *         --sketch alpha_ground --publish Overlay
 *     Receiver Overlay --grab overlay.png
 *
 * A STILL OF IT IS MOSTLY NOTHING, and that is the honest plate: a
 * transparent ground photographs as transparent, so the PNG beside this
 * sketch is the mark and the empty ground it was drawn on.
 *
 * EDIT THESE FIRST
 *   kInk     what the mark is drawn in
 *   kTurn    seconds for one turn of the ring
 *   kCanvas  the overlay a subscriber receives, in pixels
 */

// TAGS: Drawing/Primitives, Materials/Compositing

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;

using sigil::draw::Pen;
using sigil::draw::TWO_PI;

namespace {

/** The overlay a subscriber receives, and nothing behind it. */
constexpr SkSize kCanvas = {640, 360};
constexpr SkColor4f kGround = {0, 0, 0, 0};
constexpr SkColor4f kInk = {0.96f, 0.84f, 0.43f, 1};

constexpr float kRadius = 84;      // how far out the ring stands
constexpr int kTicks = 24;         // how many marks go round it
constexpr double kTurn = 6.0;      // seconds for one turn
constexpr double kCaptureAt = 1.5;

struct AlphaGround {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas,
                .background = kGround,
                .captureSeconds = kCaptureAt});
    ctx.composer.render(compose::pen("alpha_ground.mark",
                                     [](Pen& pen) { draw(pen); })
                            .width(kCanvas.width())
                            .height(kCanvas.height()));
  }

  /** The mark, and only the mark. Nothing lays a ground down here: the
   *  pen's canvas arrives carrying nothing, and what this leaves
   *  untouched is what a subscriber sees its own scene through. */
  static void draw(Pen& pen) {
    const float cx = pen.width * 0.5f;
    const float cy = pen.height * 0.5f;
    const auto angle = (float)(pen.millis() / 1000.0 / kTurn) * TWO_PI;

    pen.noStroke();
    pen.fill(kInk);
    pen.circle(cx, cy, 26);

    pen.noFill();
    pen.stroke(kInk);
    pen.strokeWeight(2);
    pen.push();
    pen.translate(cx, cy);
    pen.rotate(angle);
    for (int tick = 0; tick < kTicks; ++tick) {
      // A long mark every sixth, so the turn is readable at a glance.
      const float length = tick % 6 == 0 ? 18.0f : 8.0f;
      pen.line(kRadius - length, 0, kRadius, 0);
      pen.rotate(TWO_PI / (float)kTicks);
    }
    pen.pop();

    pen.strokeWeight(1);
    pen.line(cx - kRadius, cy + kRadius + 24, cx + kRadius, cy + kRadius + 24);
  }
};

}  // namespace

SIGIL_SKETCH(AlphaGround, "Draw",
             "A turning ring on a fully transparent ground: the sketch a "
             "publication is offered from when the frame is meant to be "
             "composited over somebody else's scene.")
