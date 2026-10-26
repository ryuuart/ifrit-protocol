/** @file
 * Both ways through the door: a pen held by a custom leaf, the kept
 * canvas beside it, and a composer held by a pen.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace sigil::compose {

namespace {

/** The pen a node draws with, and what the paint context does not
 *  carry: the step since the last frame and the count of frames. */
struct PenState {
  draw::Pen pen;
  double lastSeconds = -1.0;
  int frames = 0;

  /** The frame for a node of @p size, and the step since the last one. */
  draw::Frame nextFrame(const PaintContext& ctx) {
    draw::Frame frame;
    frame.width = ctx.size.x;
    frame.height = ctx.size.y;
    // Where the node stands in the composer's root, so a world-space
    // material the program paints spans the root as a node's fill does.
    frame.rootWidth = ctx.rootSize.x;
    frame.rootHeight = ctx.rootSize.y;
    frame.toRoot = ctx.toRoot.matrix;
    frame.seconds = ctx.elapsedSeconds;
    frame.deltaSeconds =
        lastSeconds < 0.0 ? 0.0 : ctx.elapsedSeconds - lastSeconds;
    lastSeconds = ctx.elapsedSeconds;
    frame.frameCount = ++frames;
    frame.fonts = ctx.fonts;
    // The input the host fed the composer, in the node's own box.
    frame.mouseX = ctx.pointer.at.x;
    frame.mouseY = ctx.pointer.at.y;
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

PaintProgram penPainter(PenProgram program) {
  auto held = std::make_shared<PenState>();
  return [held, program = std::move(program)](draw::Pen& pen,
                                              const PaintContext& ctx) {
    SkCanvas& canvas = *pen.canvas();
    held->pen.begin(canvas, held->nextFrame(ctx));
    const SkScopeExit endFrame([&] { held->pen.end(); });
    held->pen.inherit(ctx.ink, ctx.font);
    held->pen.inheritForGuests(ctx.vars);
    program(held->pen, ctx);
  };
}

/** The kept canvas a `graphics` node draws on, the pen that puts it down,
 *  and what p5's loop words are judged against: the time since the
 *  program last ran, and the slack a requested rate carries forward. */
struct GraphicsState {
  PenState host;
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
bool shouldRun(draw::Pen& pen, GraphicsState& held) {
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

PaintProgram graphicsPainter(PenProgram program) {
  auto held = std::make_shared<GraphicsState>();
  return [held, program = std::move(program)](draw::Pen& pen,
                                              const PaintContext& ctx) {
    SkCanvas& canvas = *pen.canvas();
    const draw::Frame frame = held->host.nextFrame(ctx);
    held->sinceDraw += frame.deltaSeconds;
    held->host.pen.begin(canvas, frame);
    const SkScopeExit endHost([&] { held->host.pen.end(); });
    held->host.pen.inherit(ctx.ink, ctx.font);
    held->host.pen.inheritForGuests(ctx.vars);
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
    {
      draw::Pen& g = held->surface->begin(held->host.pen);
      const SkScopeExit endSurface([&] { held->surface->end(); });
      g.inheritFrom(held->host.pen);
      if (shouldRun(g, *held)) {
        // The program's own clock, as p5 keeps it: the count counts runs,
        // and the step is the time since the last one, so a program under
        // `frameRate(30)` that divides by `deltaTime` divides by a
        // thirtieth and not by the node's sixtieth.
        g.frameCount = ++held->runs;
        g.deltaTime = held->sinceDraw * 1000.0;
        program(g, ctx);
        held->sinceDraw = 0.0;
      }
    }
    held->host.pen.image(*held->surface, 0, 0);
  };
}

/** What a pen keeps for one retained element: a composer with the engine
 *  it runs on, stepped by the pen and never by the wall. */
struct RetainedGuest {
  motion::Engine engine;
  /** A changed borrowed font context requires a new composer. */
  weave::FontContext* fonts = nullptr;
  std::unique_ptr<Composer> composer;
  explicit RetainedGuest(weave::FontContext& context) { adopt(context); }
  void adopt(weave::FontContext& context) {
    fonts = &context;
    composer = std::make_unique<Composer>(engine, context);
  }
};

}  // namespace

// A CANVAS FILLS THE BOX IT STANDS IN, which is what p5's own does: the
// node comes back covering, and a pen given a box of its own states that
// box — an inset, a rect, or a size in a flow the node is put back into.
Element pen(PenProgram program, Cache caching) {
  return custom(penPainter(std::move(program))).cache(caching).cover();
}

Element pen(std::string_view key, PenProgram program, Cache caching) {
  return custom(key, penPainter(std::move(program))).cache(caching).cover();
}

namespace {

/** THE PEN DOOR AS AN ADDING OPERATOR: one pen over the scope, handed the
 *  scope as it stood when layout settled. No equality — a program has
 *  none — so the operator is the escape hatch; the key, when given, is
 *  what lets the pen it attaches prune. */
struct DrawWith {
  std::string key;
  ScopeProgram program;

  /** Equal KEYS assert equal programs — the author's contract, the one
   *  `custom(key)` states — so a keyed operator prunes; an unkeyed one
   *  compares equal to nothing and its node is described afresh. */
  bool operator==(const DrawWith& other) const {
    return !key.empty() && key == other.key;
  }
  /** The key vouches for a program no comparison can read. */
  bool equalityIsTheAuthorsWord() const { return !key.empty(); }

  void add(Scope& scope) const {
    // The program reads the scope at PAINT time, so it is handed a copy
    // taken now: the nodes as they stand, attaching nothing.
    const PenProgram drawScope = [snapshot = scope.snapshot(),
                                  program = program](draw::Pen& pen) {
      if (program) program(pen, snapshot);
    };
    scope.attach(key.empty() ? pen(drawScope) : pen(key, drawScope));
  }
};

}  // namespace

Operator drawWith(ScopeProgram program) {
  return DrawWith{{}, std::move(program)};
}

Operator drawWith(std::string_view key, ScopeProgram program) {
  return DrawWith{std::string(key), std::move(program)};
}

Element graphics(PenProgram program, Cache caching) {
  return custom(graphicsPainter(std::move(program))).cache(caching).cover();
}

Element graphics(std::string_view key, PenProgram program, Cache caching) {
  return custom(key, graphicsPainter(std::move(program)))
      .cache(caching)
      .cover();
}

void paintRetained(draw::Pen& pen, const Element& element,
                   const geometry::path::Rect& box, draw::Slot slot) {
  SkCanvas* canvas = pen.canvas();
  weave::FontContext* fonts = pen.fonts();
  if (!canvas || !fonts) return;
  RetainedGuest& guest = pen.retained().get<RetainedGuest>(
      slot, [fonts] { return std::make_shared<RetainedGuest>(*fonts); });
  // A pen drawing with another font context gets a composer built on it:
  // the guest is kept across frames and the reference it holds is not
  // this pen's to assume.
  if (guest.fonts != fonts) guest.adopt(*fonts);
  // The host has already chosen the frame's movement. A stated step is
  // passed through unchanged, including a fixed-step capture's long frame.
  const motion::Duration step(pen.deltaTime / 1000.0);
  guest.engine.advance(guest.engine.elapsed() + step);
  guest.composer->setSize({box.width(), box.height()});
  // THE TREE CASCADES FROM THE PEN: what the pen was told it inherits is
  // what this tree's root inherits, so a guest painted inside a pen
  // program begins in the colour and the type the pen's own verbs do, and
  // under the custom properties in force where the pen stands.
  const VarTable* properties = pen.guestInheritance<VarTable>();
  guest.composer->setInherited(pen.inheritedFont(), pen.inheritedInk(),
                               properties ? *properties : VarTable{});
  guest.composer->render(element);
  // The box is the guest's viewport, not a clip: what a node paints past
  // its box — a shadow, a stroke's outer half — paints past it here too.
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(box.left(), box.top());
  guest.composer->draw(*canvas);
}

}  // namespace sigil::compose
