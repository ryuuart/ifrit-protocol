/** Which way up the presented canvas is: a top band and a bottom band
 *  that no encoder rounds, with the declared ground between them, so a
 *  photograph of the window says which end of the texture reached the
 *  top of the item. */
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
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
    ctx.composer.render(
        custom("bands",
               [](SkCanvas& canvas) {
                 canvas.clear(SK_ColorBLACK);
                 SkPaint paint;
                 paint.setColor(SK_ColorRED);
                 canvas.drawRect(SkRect::MakeXYWH(0, 0, kWidth, kBand), paint);
                 paint.setColor(SK_ColorBLUE);
                 canvas.drawRect(
                     SkRect::MakeXYWH(0, kHeight - kBand, kWidth, kBand),
                     paint);
               })
            .inset(0)
            .cache(Cache::None));
  }
};

}  // namespace

SIGIL_SKETCH(Orientation, "Test",
             "a red top band and a blue bottom band, to say which way up "
             "the canvas is presented")
