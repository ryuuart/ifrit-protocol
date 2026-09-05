/** @file
 * web_script — driving a page rather than only displaying one.
 *
 * A `WebView` is not a picture, it is a live document, and four calls
 * reach into it. One page is loaded into four views and each is driven
 * one way, so the cells differ in exactly the call named on them.
 *
 *   setLoadCallback(fn) — fires ON THE WEB THREAD when the main frame
 *     finishes loading. It is the only honest place to say "the document
 *     is here": a view answers no frame at all until it has painted one,
 *     and a caller that waited on a clock instead would be racing.
 *   evaluateScript(js, onResult) — runs JavaScript in the page. What the
 *     expression evaluates to comes back STRINGIFIED (or the exception
 *     text does), also on the web thread, so the caption under that cell
 *     is the page's own answer and not something this file computed.
 *   scroll(dx, dy) — pixels of wheel, exactly as an input would deliver
 *     them, and a wheel's delta is WHAT THE CONTENT MOVES BY: a negative
 *     dy walks down the page. The document's own overflow does the rest.
 *   mouseMove / mouseDown / mouseUp — in view pixels. A page sees NO
 *     CLICK until the matching up arrives, which is why the fourth cell
 *     sends three events for one press and its handler stamps the point.
 *
 * A PROCESS BOOTS EXACTLY ONE ENGINE, so it is held beside the sketch
 * rather than inside it; the four views are that engine's.
 *
 * The waiting is the awkward part and is stated rather than hidden: every
 * call above is asynchronous across the web thread, so each stage ends by
 * waiting on the ENGINE'S OWN EVENTS — the load callback for the document
 * and the frame callback for the repaint a call caused — and never on a
 * stretch of clock. A machine that runs the engine slowly reaches those
 * events later and draws this same sheet. See <sigilsketch/scry/SettledPage.h>.
 *
 * EDIT THESE FIRST
 *   kScrollBy  — how far down the page the third cell walks, px.
 *   kClickAt   — where the fourth cell presses, in view pixels.
 *   kScript    — the expression the second cell evaluates.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scry/SettledPage.h>
#include <sigilsketch/scry/SharedEngine.h>
#include <sigilweave/style/Type.h>

#include <future>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace scry = sigil::scry;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr int kViewW = 300;
constexpr int kViewH = 236;
constexpr int kScrollBy = 220;  // px DOWN the page the third cell moves
constexpr SkIPoint kClickAt = {150, 203};  // view pixels the fourth presses
constexpr const char* kScript =
    "document.querySelectorAll('.row').forEach((r,i)=>{"
    "r.textContent = 'row ' + (i+1) + ' \\u2014 rewritten';"
    "r.style.background = i%2 ? '#e6dccb' : '#f2ece0';});"
    "document.getElementById('head').textContent = 'EVALUATED';"
    "'rewrote ' + document.querySelectorAll('.row').length + ' rows'";

constexpr SkSize kCanvas = {1360, 430};

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.subtitle = {.size = 11, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 0.3f};
  look.type.captionLabel = {.size = 11, .track = 0.4f};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  look.spacing.captionGap = 6;
  return look;
}

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** The document all four views load. It is deliberately taller than the
 *  view — the scroll cell needs somewhere to go — and its button records
 *  where it was pressed, so a synthetic click leaves a mark the picture
 *  can carry. */
const char* page() {
  return R"HTML(<!doctype html><meta charset="utf-8"><style>
  html,body{margin:0;background:#f7f2e7;color:#1d1a15;
    font:13px/1.4 -apple-system,Helvetica,Arial,sans-serif}
  h1{margin:0;height:16px;padding:11px 14px;font-size:12px;letter-spacing:2.4px;
    background:#1d1a15;color:#f7f2e7}
  .row{height:20px;padding:7px 14px;border-bottom:1px solid #ddd4c2;
    box-sizing:content-box}
  #btn{display:block;box-sizing:border-box;width:calc(100% - 28px);
    height:34px;margin:12px 14px;border:1px solid #8a7a5c;border-radius:6px;
    background:#efe6d4;font:12px/1 inherit;letter-spacing:1.4px}
  #btn.hit{background:#a4441f;color:#fdf6e8;border-color:#a4441f}
</style>
<h1 id="head">A LIVE DOCUMENT</h1>
<div class="row">row 1</div><div class="row">row 2</div>
<div class="row">row 3</div><div class="row">row 4</div>
<button id="btn">PRESS ME</button>
<div class="row">row 5</div><div class="row">row 6</div>
<div class="row">row 7</div><div class="row">row 8</div>
<div class="row">row 9</div><div class="row">row 10</div>
<div class="row">row 11</div><div class="row">row 12</div>
<div class="row">row 13</div><div class="row">row 14</div>
<script>
  document.getElementById('btn').addEventListener('mouseup', e => {
    const b = e.currentTarget;
    b.classList.add('hit');
    b.textContent = 'PRESSED AT ' + e.clientX + ', ' + e.clientY;
  });
</script>)HTML";
}

}  // namespace

struct WebScript final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE: an engine with its layout tables and
   *  certificates, which ship with the application rather than with its
   *  dylibs, so a build that links the SDK can still find nothing to lay
   *  out with. */
  static bool available(std::string* why) { return scry::available(why); }

  /** One view per cell, held for the sketch's life because a view owns
   *  the frame drawn from it. THE PICTURE IS NOT "whatever the view holds
   *  now": a page goes on repainting after it has settled, so each cell
   *  draws the frame its own settle accepted. */
  std::vector<std::shared_ptr<scry::WebView>> views;
  std::vector<scry::WebView::Frame> stills;

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // every stage is done before the first frame
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const std::shared_ptr<scry::WebEngine> web = sketch::scry::sharedEngine();
    if (!web) {
      ctx.composer.render(missing("this sketch host has no shared web engine"));
      return;
    }

    // Every stage latches the view's events BEFORE the call it is about
    // to make, so nothing the engine says can land in the gap between
    // asking and listening.

    // ---- the load callback, on a view that is only loaded ------------
    std::shared_ptr<scry::WebView> plain = open(*web);
    const sketch::scry::Events plainEvents(*plain);
    plain->loadHTML(page());
    const bool painted = plainEvents.awaitLoad();
    const bool fired = plainEvents.loaded();

    // ---- the script, and what it evaluated to ------------------------
    std::shared_ptr<scry::WebView> scripted = open(*web);
    const sketch::scry::Events scriptedEvents(*scripted);
    scripted->loadHTML(page());
    bool settled = scriptedEvents.awaitLoad();

    auto answered = std::make_shared<std::promise<std::string>>();
    std::future<std::string> reply = answered->get_future();
    scripted->evaluateScript(kScript, [answered](std::string result) {
      answered->set_value(std::move(result));
    });
    // The heading the script rewrites is the page's own statement that
    // the rewrite has landed AND been painted.
    settled = sketch::scry::awaitAnswer(*scripted, scriptedEvents,
                                        "document.getElementById('head')"
                                        ".textContent",
                                        "EVALUATED") &&
              settled;
    const std::string returned =
        reply.wait_for(sketch::scry::kUnresponsive) == std::future_status::ready
            ? reply.get()
            : std::string();

    // ---- the wheel ---------------------------------------------------
    std::shared_ptr<scry::WebView> scrolled = open(*web);
    const sketch::scry::Events scrolledEvents(*scrolled);
    scrolled->loadHTML(page());
    settled = scrolledEvents.awaitLoad() && settled;
    // A wheel's delta is what the CONTENT moves by, so moving DOWN the
    // page is a negative dy.
    scrolled->scroll(0, -kScrollBy);
    // THE ENGINE WALKS A WHEEL SMOOTHLY, so the frame after the call is
    // the page part of the way down. The page's own scroll offset says
    // when the walk is over — read EXACTLY, because the last fraction of
    // a pixel of that walk is a row edge antialiased two ways.
    settled = sketch::scry::awaitAnswer(*scrolled, scrolledEvents,
                                        "String(window.scrollY)",
                                        std::to_string(kScrollBy)) &&
              settled;

    // ---- the press ---------------------------------------------------
    std::shared_ptr<scry::WebView> pressed = open(*web);
    const sketch::scry::Events pressedEvents(*pressed);
    pressed->loadHTML(page());
    settled = pressedEvents.awaitLoad() && settled;
    pressed->mouseMove(kClickAt.x(), kClickAt.y());
    pressed->mouseDown(kClickAt.x(), kClickAt.y());
    pressed->mouseUp(kClickAt.x(), kClickAt.y());
    // The class the page's own handler adds is its statement that the
    // click arrived and the button has been repainted in it.
    settled = sketch::scry::awaitAnswer(
                  *pressed, pressedEvents,
                  "document.getElementById('btn').className", "hit") &&
              settled;

    views = {plain, scripted, scrolled, pressed};
    stills = {plainEvents.accepted(), scriptedEvents.accepted(),
              scrolledEvents.accepted(), pressedEvents.accepted()};

    char press[96];
    std::snprintf(press, sizeof press,
                  "three events for one click \xe2\x80\x94 the page's own "
                  "handler stamped (%d, %d)",
                  kClickAt.x(), kClickAt.y());
    char wheel[132];
    std::snprintf(wheel, sizeof wheel,
                  "%d px down the page \xe2\x80\x94 a delta is what the "
                  "CONTENT moves by, so down is negative",
                  kScrollBy);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("DRIVING A PAGE \xc2\xb7 setLoadCallback + "
                       "evaluateScript + scroll + mouse"),
         .subtitle = toU8("dials \xc2\xb7 the script \xc2\xb7 the wheel "
                          "\xc2\xb7 the point pressed \xe2\x80\x94 one "
                          "document, four views, one call apart"),
         .footer = toU8(
             std::string(
                 "every call crosses to the web thread, so each cell was "
                 "driven and then waited on for the engine's own events "
                 "\xe2\x80\x94 the load, then the page's own answer that "
                 "what the call asked for is what the latest frame shows") +
             (settled ? "" : "; one of those waits expired"))},
        kit::cells(
            {.cells = {cell("plain", plain, stills[0],
                            "loadHTML + setLoadCallback",
                            std::string("the load callback ") +
                                (fired ? "fired" : "never fired") + ", and " +
                                (painted ? "a frame was published"
                                         : "nothing was published")),
                       cell("scripted", scripted, stills[1],
                            "evaluateScript(js, onResult)",
                            std::string("the page answered \xe2\x80\x9c") +
                                returned + "\xe2\x80\x9d"),
                       cell("scrolled", scrolled, stills[2], "scroll(0, -dy)",
                            wheel),
                       cell("pressed", pressed, stills[3],
                            "mouseMove / mouseDown / mouseUp", press)},
             .gap = 18,
             .divider = Fill::color(kRule)})));
  }

  std::shared_ptr<scry::WebView> open(scry::WebEngine& web) const {
    return web.createView(kViewW, kViewH);
  }

  /** One cell: THE FRAME THIS CELL'S SETTLE ACCEPTED, at its own pixel
   *  size so nothing resamples.
   *
   *  A CPU engine hands the frame over as an immutable image, and that
   *  image is the still however long the page goes on repainting. A GPU
   *  engine publishes a texture it reuses, so there is no image to keep
   *  and the view draws its latest — which is what a device rendering of
   *  a live page is either way. */
  static Element cell(std::string key, std::shared_ptr<scry::WebView> view,
                      scry::WebView::Frame still, const char* call,
                      std::string note) {
    const SkRect where = SkRect::MakeWH((float)kViewW, (float)kViewH);
    return sketch::kit::caption(
        (float)kViewW, toU8(call), toU8(note),
        custom(std::move(key),
               [view, still = std::move(still),
                where](SkCanvas& canvas, const PaintContext&) {
                 if (still.image)
                   canvas.drawImageRect(
                       still.image, where,
                       SkSamplingOptions(SkFilterMode::kLinear));
                 else if (view)
                   view->draw(canvas, where);
               })
            .width((float)kViewW)
            .height((float)kViewH)
            .fill(Fill::color(kCellGround)));
  }

  /** What stands here when the engine has nothing to lay out with. A
   *  sweep never reaches it, having asked the registry first; the app
   *  opens whatever is selected, so it is drawn rather than left
   *  blank. */
  static Element missing(const std::string& why) {
    return box()
        .absolute()
        .inset(0)
        .fill(Fill::color(kGround))
        .column()
        .gap(10)
        .padding(40)
        .child(text(toU8("no web engine here"), label(20, kInk)))
        .child(text(toU8(why), label(12, kAsh)).width(Dim(620.0f)));
  }
};

SIGIL_SKETCH(WebScript, "Kit \xc2\xb7 API",
             "one document in four views, each driven by one call \xe2\x80\x94 "
             "a load stamp, a script's own answer, a wheel and a synthetic "
             "press")
