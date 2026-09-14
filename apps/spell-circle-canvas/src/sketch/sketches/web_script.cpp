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
 * The settling is the awkward part and is stated rather than hidden:
 * every call above is asynchronous across the web thread, so each cell is
 * a SEQUENCE the page is put through — load, the call, the page's own
 * answer that it landed, the view going quiet, a whole painting — and
 * every step of it turns on the ENGINE'S OWN EVENTS, never on a stretch
 * of clock. A machine that runs the engine slowly reaches those events
 * later and draws this same sheet.
 *
 * A CAPTURE IS HELD ON THAT SEQUENCE AND A WINDOW IS NOT. setup() runs on
 * the thread that presents, so the four sequences are started there and
 * advanced from update(): each cell draws its own view live until its
 * sequence stops on a frame, the footer says the pages are still
 * arriving, and the sheet is described again as each one lands. One
 * declaration, two ways of driving it, decided by ctx.deterministic.
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
#include <sigilsketch/scry/Settling.h>
#include <sigilsketch/scry/SharedEngine.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
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

struct WebScript {
  /** WHAT THIS MACHINE MUST HAVE: an engine with its layout tables and
   *  certificates, which ship with the application rather than with its
   *  dylibs, so a build that links the SDK can still find nothing to lay
   *  out with. */
  static bool available(std::string* why) { return scry::available(why); }

  /** One view per cell, held for the sketch's life because a view owns
   *  the frame drawn from it, and one settle per view beside it. THE
   *  PICTURE IS NOT "whatever the view holds now": a page goes on
   *  repainting after it has settled, so each cell draws the frame its
   *  own settle stopped on — and the view itself only until there is
   *  one. */
  std::vector<std::shared_ptr<scry::WebView>> views;
  std::vector<std::unique_ptr<sketch::scry::Settling>> pages;
  /** How many times the sheet has been declared: what names a still
   *  apart from an earlier declaration's under the same cell. */
  int declared = 0;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // every stage is done before the first frame
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const std::shared_ptr<scry::WebEngine> web = sketch::scry::sharedEngine();
    if (!web) {
      ctx.composer.render(missing("this sketch host has no shared web engine"));
      return;
    }

    // THE FOUR VIEWS ARE MADE BEFORE ANY OF THEM IS LOADED, because
    // creating a view is a call the engine answers on its own thread and
    // a page already loading holds that thread: a view asked for in
    // between waits for a document that is nothing to do with it.
    // The settles go before the views they latched, and a declaration
    // later than the first names its stills apart from the first's.
    pages.clear();
    views.clear();
    ++declared;
    for (int cell = 0; cell < 4; ++cell) views.push_back(open(*web));

    // One document, four views, one call apart. Each settle owns its
    // view's load, so nothing the engine says can land in the gap
    // between asking and listening, and each is driven the way this
    // session asks for: a capture is held here until the page is there,
    // a window comes straight back and advances them from update().
    const auto put = [&](sketch::scry::Sequence sequence) {
      pages.push_back(sketch::scry::settle(
          *views[pages.size()], std::move(sequence), ctx.deterministic));
    };

    // ---- the load callback, on a view that is only loaded ------------
    // The load says the document is here; the page going quiet says its
    // picture is finished.
    put({.html = page(),
         .question = "String(document.readyState)",
         .expected = "complete",
         .quiet = true,
         .whole = true});

    // ---- the script, and what it evaluated to ------------------------
    // The heading the script rewrites is the page's own statement that
    // the rewrite has landed AND been painted.
    put({.html = page(),
         .run = kScript,
         .question = "document.getElementById('head').textContent",
         .expected = "EVALUATED",
         .quiet = true,
         .whole = true});

    // ---- the wheel ---------------------------------------------------
    // A wheel's delta is what the CONTENT moves by, so moving DOWN the
    // page is a negative dy. THE ENGINE WALKS A WHEEL SMOOTHLY, so the
    // frame after the call is the page part of the way down. The page's
    // own scroll offset says where the walk is HEADING — it is reported
    // as a whole number, so it reads 220 while the rows are still a
    // fraction of a pixel short of it, which is a row edge antialiased
    // two ways. What says the walk is OVER is the view going quiet.
    put({.html = page(),
         .wheel = {0, -kScrollBy},
         .question = "String(window.scrollY)",
         .expected = std::to_string(kScrollBy),
         .quiet = true,
         .whole = true});

    // ---- the press ---------------------------------------------------
    // The class the page's own handler adds is its statement that the
    // click arrived and the button has been repainted in it.
    put({.html = page(),
         .press = kClickAt,
         .question = "document.getElementById('btn').className",
         .expected = "hit",
         .quiet = true,
         .whole = true});

    describe(ctx);
  }

  /** THE PAGES ARRIVE RATHER THAN BEING WAITED FOR: every settle is
   *  advanced here, on the thread that draws, and the sheet is described
   *  again on the frame any of them finishes. A capture has finished all
   *  four already and this moves nothing. */
  void update(double, sketch::SketchContext& ctx) {
    bool landed = false;
    for (const std::unique_ptr<sketch::scry::Settling>& settling : pages)
      landed = settling->advance() || landed;
    if (landed) describe(ctx);
  }

  /** The sheet as the four pages stand now. */
  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    const bool arrived =
        std::all_of(pages.begin(), pages.end(),
                    [](const auto& page) { return page->arrived(); });
    const bool expired =
        std::any_of(pages.begin(), pages.end(),
                    [](const auto& page) { return page->broken(); });
    const int atX = kClickAt.x(), atY = kClickAt.y();
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
             (expired   ? "; one of those waits expired"
              : arrived ? ""
                        : "; the pages are still arriving")},
        kit::cells(
            {.cells =
                 {cell("plain", 0, "loadHTML + setLoadCallback",
                       std::string("the load callback ") +
                           (pages[0]->loaded() ? "fired" : "never fired") +
                           ", and " +
                           (pages[0]->painted() ? "a frame was published"
                                                : "nothing was published")),
                  cell("scripted", 1, "evaluateScript(js, onResult)",
                       std::string("the page answered “") + pages[1]->reply() +
                           "”"),
                  cell("scrolled", 2, "scroll(0, -dy)", wheel),
                  cell("pressed", 3, "mouseMove / mouseDown / mouseUp", press)},
             .gap = 18,
             .divider = Fill::color(sketch::kit::theme().palette.rule)})));
  }

  std::shared_ptr<scry::WebView> open(scry::WebEngine& web) const {
    return web.createView(kViewW, kViewH);
  }

  /** One cell: THE FRAME THIS CELL'S SETTLE STOPPED ON, at its own pixel
   *  size so nothing resamples — and the view itself while that frame is
   *  still coming.
   *
   *  ONE DRAWING UNDER ONE OF TWO KEYS, because a key IS a program's
   *  identity and one key must name one picture: a page still arriving
   *  is the view's own latest, which changes without anything being
   *  described again and is therefore declared volatile, and a still is
   *  ONE frame of the engine's, named by the declaration that took it,
   *  and cached like any static leaf.
   *
   *  A CPU engine hands the frame over as an immutable image, and that
   *  image is the still however long the page goes on repainting. A GPU
   *  engine publishes a texture it reuses, so there is no image to keep
   *  and the view draws its latest — which is what a device rendering of
   *  a live page is either way. */
  Element cell(const std::string& name, size_t at, const char* call,
               std::string note) const {
    const SkRect where = SkRect::MakeWH((float)kViewW, (float)kViewH);
    scry::WebView::Frame still = pages[at]->still();
    const std::string key = still.image
                                ? name + " · still " + std::to_string(declared)
                                : name + " · arriving";
    // The view's own latest only once a repaint carries the loaded
    // document: the blank a view paints the moment it exists is the
    // engine's page, not this one's.
    Element picture =
        custom(key, [view = views[at], settling = pages[at].get(),
                     still = std::move(still), where](SkCanvas& canvas) {
          if (still.image)
            canvas.drawImageRect(still.image, where,
                                 SkSamplingOptions(SkFilterMode::kLinear));
          else if (view && settling->painted())
            view->draw(canvas, where);
        });
    if (!still.image) picture.cache(Cache::None);
    return sketch::kit::caption((float)kViewW, call, note,
                                std::move(picture)
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
