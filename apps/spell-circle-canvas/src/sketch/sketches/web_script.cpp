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
 * AND EVERY STILL HERE IS THE FRAME THE PAGE WENT QUIET ON. The page's
 * own answer says the call landed; it does not say the picture has caught
 * up with it. A wheel is walked smoothly and `window.scrollY` is reported
 * as a whole number, so the page says 220 while the rows are still a
 * fraction of a pixel short of it, and which repaint that answer falls on
 * is decided by how loaded the machine was — a row edge antialiased two
 * ways, and a plate that moves under a busy sweep and holds when this
 * scene is rendered alone. So each stage waits for the answer AND for the
 * view to stop painting, and the last frame is the one drawn.
 *
 * EDIT THESE FIRST
 *   kScrollBy  — how far down the page the third cell walks, px.
 *   kClickAt   — where the fourth cell presses, in view pixels.
 *   kScript    — the expression the second cell evaluates.
 */

// TAGS: Interfaces/Web

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
namespace scry = sigil::scry;

using namespace sigil::compose;

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

constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};

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
    // The load says the document is here; the page going quiet says its
    // picture is finished.
    bool settled =
        sketch::scry::awaitQuiet(*plain, plainEvents,
                                 "String(document.readyState)", "complete") &&
        sketch::scry::repaintWhole(*plain, plainEvents);

    // ---- the script, and what it evaluated to ------------------------
    std::shared_ptr<scry::WebView> scripted = open(*web);
    const sketch::scry::Events scriptedEvents(*scripted);
    scripted->loadHTML(page());
    settled = scriptedEvents.awaitLoad() && settled;

    auto answered = std::make_shared<std::promise<std::string>>();
    std::future<std::string> reply = answered->get_future();
    scripted->evaluateScript(kScript, [answered](std::string result) {
      answered->set_value(std::move(result));
    });
    // The heading the script rewrites is the page's own statement that
    // the rewrite has landed AND been painted.
    settled = sketch::scry::awaitQuiet(*scripted, scriptedEvents,
                                       "document.getElementById('head')"
                                       ".textContent",
                                       "EVALUATED") &&
              sketch::scry::repaintWhole(*scripted, scriptedEvents) && settled;
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
    // where the walk is HEADING — it is reported as a whole number, so it
    // reads 220 while the rows are still a fraction of a pixel short of
    // it, which is a row edge antialiased two ways. What says the walk is
    // OVER is the view going quiet.
    settled = sketch::scry::awaitQuiet(*scrolled, scrolledEvents,
                                       "String(window.scrollY)",
                                       std::to_string(kScrollBy)) &&
              sketch::scry::repaintWhole(*scrolled, scrolledEvents) && settled;

    // ---- the press ---------------------------------------------------
    std::shared_ptr<scry::WebView> pressed = open(*web);
    const sketch::scry::Events pressedEvents(*pressed);
    pressed->loadHTML(page());
    settled = pressedEvents.awaitLoad() && settled;
    const int atX = kClickAt.x(), atY = kClickAt.y();
    pressed->mouseMove(atX, atY);
    pressed->mouseDown(atX, atY);
    pressed->mouseUp(atX, atY);
    // The class the page's own handler adds is its statement that the
    // click arrived and the button has been repainted in it.
    settled = sketch::scry::awaitQuiet(
                  *pressed, pressedEvents,
                  "document.getElementById('btn').className", "hit") &&
              sketch::scry::repaintWhole(*pressed, pressedEvents) && settled;

    views = {plain, scripted, scrolled, pressed};
    stills = {plainEvents.accepted(), scriptedEvents.accepted(),
              scrolledEvents.accepted(), pressedEvents.accepted()};

    const std::string press = kit::formatted(
        "three events for one click — the page's own "
        "handler stamped (%d, %d)",
        atX, atY);
    const std::string wheel = kit::formatted(
        "%d px down the page — a delta is what the "
        "CONTENT moves by, so down is negative",
        kScrollBy);

    ctx.composer.render(sketch::kit::page(
        {.title = "DRIVING A PAGE · setLoadCallback + "
                  "evaluateScript + scroll + mouse",
         .subtitle = "dials · the script · the wheel "
                     "· the point pressed — one "
                     "document, four views, one call apart",
         .footer =
             std::string(
                 "every call crosses to the web thread, so each cell was "
                 "driven and then waited on for the engine's own events — "
                 "the load, the page's own answer that what the call asked "
                 "for has landed, and the view going quiet — and then "
                 "painted whole, so no still carries the seams of how its "
                 "driving was broken up") +
             (settled ? "" : "; one of those waits expired")},
        kit::cells(
            {.cells =
                 {cell("plain", plain, stills[0], "loadHTML + setLoadCallback",
                       std::string("the load callback ") +
                           (fired ? "fired" : "never fired") + ", and " +
                           (painted ? "a frame was published"
                                    : "nothing was published")),
                  cell("scripted", scripted, stills[1],
                       "evaluateScript(js, onResult)",
                       std::string("the page answered “") + returned + "”"),
                  cell("scrolled", scrolled, stills[2], "scroll(0, -dy)",
                       wheel),
                  cell("pressed", pressed, stills[3],
                       "mouseMove / mouseDown / mouseUp", press)},
             .gap = 18,
             .divider = Fill::color(sketch::kit::theme().palette.rule)})));
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
        (float)kViewW, call, note,
        custom(std::move(key),
               [view, still = std::move(still), where](SkCanvas& canvas) {
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
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return box()
        .inset(0)
        .fill(Fill::color(sheet.palette.ground))
        .column()
        .gap(10)
        .padding(40)
        .children({text("no web engine here")
                       .font({.size = 20, .color = sheet.palette.ink}),
                   text(why)
                       .font({.size = 12, .color = sheet.palette.ash})
                       .width(620.0f)});
  }
};

SIGIL_SKETCH(WebScript, "Kit · API",
             "one document in four views, each driven by one call — "
             "a load stamp, a script's own answer, a wheel and a synthetic "
             "press")
