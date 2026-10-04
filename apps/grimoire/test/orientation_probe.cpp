/** Which way up the presented canvas is: a top band and a bottom band
 *  that no encoder rounds, with the declared ground between them, so a
 *  photograph of the window says which end of the texture reached the
 *  top of the item. */
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

namespace {

constexpr float kWidth = 640;
constexpr float kHeight = 420;
/** A sixth of the canvas at each end: wide enough that letterboxing and
 *  the window's own chrome cannot put the far band where this one was. */
constexpr float kBand = kHeight / 6;

struct Orientation {
  void setup(sigil::sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.canvas(kWidth, kHeight);
    ctx.composer.render(pen("bands",
                            [](sigil::draw::Pen& pen, const PaintContext&) {
                              pen.background(0);
                              pen.noStroke();
                              pen.fill(255, 0, 0);
                              pen.rect(0, 0, kWidth, kBand);
                              pen.fill(0, 0, 255);
                              pen.rect(0, kHeight - kBand, kWidth, kBand);
                            })
                            .inset(0)
                            .cache(Cache::None));
  }
};

}  // namespace

SIGIL_SKETCH(Orientation, "Test",
             "a red top band and a blue bottom band, to say which way up "
             "the canvas is presented")
