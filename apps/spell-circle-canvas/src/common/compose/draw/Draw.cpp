/** @file
 * Both ways through the door: a pen held by a custom leaf, and a
 * composer held by a pen.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <memory>
#include <utility>

namespace sigil::compose {

namespace {

/** The pen a node draws with, and what the paint context does not
 *  carry: the step since the last frame and the count of frames. */
struct Held {
  draw::Pen pen;
  double lastSeconds = -1.0;
  int frames = 0;
};

PaintProgram over(PenProgram program) {
  auto held = std::make_shared<Held>();
  return [held, program = std::move(program)](SkCanvas& canvas,
                                              const PaintContext& ctx) {
    draw::Frame frame;
    frame.width = ctx.size.width();
    frame.height = ctx.size.height();
    frame.seconds = ctx.elapsedSeconds;
    frame.deltaSeconds =
        held->lastSeconds < 0.0 ? 0.0 : ctx.elapsedSeconds - held->lastSeconds;
    held->lastSeconds = ctx.elapsedSeconds;
    frame.frameCount = ++held->frames;
    frame.fonts = ctx.fonts;
    held->pen.begin(canvas, frame);
    program(held->pen);
    held->pen.end();
  };
}

/** What a pen keeps for one retained element: a composer with the clock
 *  and ticker it runs on, stepped by the pen and never by the wall. */
struct Guest {
  motion::FrameClock clock;
  motion::Ticker ticker;
  /** The context the composer holds a REFERENCE to for its life, kept so
   *  a pen arriving with a different one is answered with a composer
   *  built on THAT one rather than with one measuring against a context
   *  nobody holds any more. */
  weave::FontContext* fonts = nullptr;
  std::unique_ptr<Composer> composer;
  explicit Guest(weave::FontContext& context) { adopt(context); }
  void adopt(weave::FontContext& context) {
    fonts = &context;
    composer = std::make_unique<Composer>(ticker, context);
    composer->setClock(&clock);
  }
};

}  // namespace

Element pen(PenProgram program) {
  return custom(over(std::move(program))).cache(Cache::None);
}

Element pen(std::string_view key, PenProgram program) {
  return custom(key, over(std::move(program))).cache(Cache::None);
}

void paintRetained(draw::Pen& pen, const Element& element, const SkRect& box,
                   draw::Slot slot) {
  SkCanvas* canvas = pen.canvas();
  weave::FontContext* fonts = pen.fonts();
  if (!canvas || !fonts) return;
  Guest& guest = pen.retained().get<Guest>(
      slot, [fonts] { return std::make_shared<Guest>(*fonts); });
  // A pen drawing with another font context gets a composer built on it:
  // the guest is kept across frames and the reference it holds is not
  // this pen's to assume.
  if (guest.fonts != fonts) guest.adopt(*fonts);
  const double step = guest.clock.advance(pen.deltaTime / 1000.0);
  guest.ticker.tick(step);
  guest.composer->setSize({box.width(), box.height()});
  guest.composer->render(element);
  // The box is the guest's viewport, not a clip: what a node paints past
  // its box — a shadow, a stroke's outer half — paints past it here too.
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(box.left(), box.top());
  guest.composer->draw(*canvas);
}

}  // namespace sigil::compose
