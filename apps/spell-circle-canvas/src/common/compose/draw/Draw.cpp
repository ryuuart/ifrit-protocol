/** @file
 * Both ways through the door: a pen held by a custom leaf, the kept
 * canvas beside it, and a composer held by a pen.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Graphics.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace sigil::compose {

namespace {

/** The pen a node draws with, and what the paint context does not
 *  carry: the step since the last frame and the count of frames. */
struct Held {
  draw::Pen pen;
  double lastSeconds = -1.0;
  int frames = 0;

  /** The frame for a node of @p size, and the step since the last one. */
  draw::Frame frameIn(const PaintContext& ctx) {
    draw::Frame frame;
    frame.width = ctx.size.width();
    frame.height = ctx.size.height();
    frame.seconds = ctx.elapsedSeconds;
    frame.deltaSeconds =
        lastSeconds < 0.0 ? 0.0 : ctx.elapsedSeconds - lastSeconds;
    lastSeconds = ctx.elapsedSeconds;
    frame.frameCount = ++frames;
    frame.fonts = ctx.fonts;
    // The input the host fed the composer, in the node's own box.
    frame.mouseX = ctx.pointer.at.x();
    frame.mouseY = ctx.pointer.at.y();
    frame.mouseIsPressed = ctx.pointer.pressed;
    if (ctx.keys) {
      frame.keyIsPressed = ctx.keys->pressed;
      frame.key = ctx.keys->key;
      frame.keyCode = ctx.keys->keyCode;
      frame.keysDown = ctx.keys->down;
    }
    return frame;
  }
};

PaintProgram over(PenProgram program) {
  auto held = std::make_shared<Held>();
  return [held, program = std::move(program)](SkCanvas& canvas,
                                              const PaintContext& ctx) {
    held->pen.begin(canvas, held->frameIn(ctx));
    held->pen.inherit(ctx.ink, ctx.font);
    program(held->pen);
    held->pen.end();
  };
}

/** The kept canvas a `graphics` node draws on, the pen that puts it down,
 *  and what p5's loop words are judged against: the time since the
 *  program last ran, and the slack a requested rate carries forward. */
struct HeldGraphics {
  Held host;
  std::optional<draw::Graphics> surface;
  double sinceDraw = 0.0;
  double slack = 0.0;
  /** How many times the program has RUN — p5's `frameCount` for a program
   *  under `noLoop` or a requested rate, which counts draws, not the frames
   *  the node was painted on. */
  int runs = 0;
};

/** WHETHER THE PROGRAM RUNS THIS FRAME, on p5's rules, read off the pen
 *  the program itself holds: the loop is running or one `redraw` was
 *  asked for, and a requested frame rate has room for another draw. The
 *  surface is put down either way, which is what makes `noLoop` mean what
 *  p5 means by it. */
bool shouldRun(draw::Pen& pen, HeldGraphics& held) {
  const bool asked = pen.takeRedraw();
  if (!pen.isLooping() && !asked) return false;
  if (asked) return true;
  const double target = pen.targetFrameRate();
  if (!(target > 0.0)) return true;
  const double period = 1.0 / target;
  // The time since the last run has to cover one period; the slack past
  // it carries into the next, bounded so a stall cannot bank a burst.
  if (held.sinceDraw + 1e-9 < period - held.slack) return false;
  held.slack = std::clamp(held.sinceDraw + held.slack - period, 0.0, period);
  return true;
}

PaintProgram onto(PenProgram program) {
  auto held = std::make_shared<HeldGraphics>();
  return [held, program = std::move(program)](SkCanvas& canvas,
                                              const PaintContext& ctx) {
    const draw::Frame frame = held->host.frameIn(ctx);
    held->sinceDraw += frame.deltaSeconds;
    held->host.pen.begin(canvas, frame);
    held->host.pen.inherit(ctx.ink, ctx.font);
    // The buffer is the node's box. It is formed on its first `begin`, at
    // the host pen's own density, and a box that has changed resizes it
    // with what it holds carried over rather than cleared.
    if (held->surface)
      held->surface->resize(frame.width, frame.height);
    else
      held->surface.emplace(frame.width, frame.height);
    // Formed no coarser than the composer's declared bake density: a
    // picture the host means to photograph finer than it steps it is
    // drawn finer from the first frame rather than magnified at the still.
    held->surface->setDensityFloor(ctx.bakeDensity);
    draw::Pen& g = held->surface->begin(held->host.pen);
    g.inherit(ctx.ink, ctx.font);
    if (shouldRun(g, *held)) {
      // The program's own clock, as p5 keeps it: the count counts runs,
      // and the step is the time since the last one, so a program under
      // `frameRate(30)` that divides by `deltaTime` divides by a
      // thirtieth and not by the node's sixtieth.
      g.frameCount = ++held->runs;
      g.deltaTime = held->sinceDraw * 1000.0;
      program(g);
      held->sinceDraw = 0.0;
    }
    held->surface->end();
    held->host.pen.image(*held->surface, 0, 0);
    held->host.pen.end();
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

Element graphics(PenProgram program) {
  return custom(onto(std::move(program))).cache(Cache::None);
}

Element graphics(std::string_view key, PenProgram program) {
  return custom(key, onto(std::move(program))).cache(Cache::None);
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
  // THE TREE CASCADES FROM THE PEN: what the pen was told it inherits is
  // what this tree's root inherits, so a guest painted inside a pen
  // program begins in the colour and the type the pen's own verbs do.
  guest.composer->setInherited(pen.inheritedFont(), pen.inheritedInk());
  guest.composer->render(element);
  // The box is the guest's viewport, not a clip: what a node paints past
  // its box — a shadow, a stroke's outer half — paints past it here too.
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(box.left(), box.top());
  guest.composer->draw(*canvas);
}

}  // namespace sigil::compose
