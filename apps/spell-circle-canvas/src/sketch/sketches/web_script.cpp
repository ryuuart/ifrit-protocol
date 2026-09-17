/** @file
 * One HTML document loaded into four independent web views.
 * The comparison isolates loading, script evaluation, scrolling and a
 * synthetic click. Answers below each view come from its page or load state.
 * Settling waits for the requested state and for painting to finish; a
 * capture holds the completed frame instead of sampling an arbitrary delay.
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

constexpr SkSize kCanvas = {1360, 660};

constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .track = 0.4f};
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
    ctx.composer.render(sketch::kit::page(
        {.title = "Four ways to drive one document",
         .subtitle =
             "Each view starts with the same HTML. A single operation changes "
             "the document, its viewport or its interaction state.",
         .footer = expired   ? "A settling wait expired; inspect the result "
                               "before relying on this capture."
                   : arrived ? "Every operation has answered and the views "
                               "have finished painting."
                             : "The views are still loading or repainting."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases =
                      {cell("plain", 0, "LOAD", "loadHTML",
                            "The unmodified document is the visual reference."),
                       cell("scripted", 1, "EVALUATE", "evaluateScript",
                            "The script changes the heading, row text and "
                            "alternating fills."),
                       cell("scrolled", 2, "SCROLL", "scroll(0, -220)",
                            "Only the viewport moves. The document remains "
                            "unchanged."),
                       cell("pressed", 3, "PRESS", "mouseMove / Down / Up",
                            "The page's own handler changes the button after "
                            "the synthetic click.")},
                  .measure = 1280,
                  .gap = 24}),
             sketch::kit::sectionHeader(
                 {.label = "ANSWERS FROM THE PAGE",
                  .note = "Observed state, not a timer guess"}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "LOAD / PAINT",
                        .figure = text(kit::formatted(
                                           "loaded: %s\npainted: %s",
                                           pages[0]->loaded() ? "yes" : "no",
                                           pages[0]->painted() ? "yes" : "no"))
                                      .styleClass("readout")
                                      .width(302)},
                       {.title = "SCRIPT RESULT",
                        .figure = text(pages[1]->reply())
                                      .styleClass("readout")
                                      .width(302)},
                       {.title = "CONFIRMED SCROLL",
                        .figure =
                            text(pages[2]->arrived() ? "220 px"
                                                     : "Awaiting confirmation")
                                .styleClass("readout")
                                .width(302)},
                       {.title = "CONFIRMED BUTTON CLASS",
                        .figure =
                            text(pages[3]->arrived() ? "hit"
                                                     : "Awaiting confirmation")
                                .styleClass("readout")
                                .width(302)}},
                  .measure = 1280,
                  .gap = 24})})));
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
  sketch::kit::ComparisonCase cell(const std::string& name, size_t at,
                                   const char* title, const char* control,
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
    return {.title = title,
            .control = control,
            .figure = std::move(picture)
                          .width((float)kViewW)
                          .height((float)kViewH)
                          .fill(Fill::color(kCellGround)),
            .note = std::move(note)};
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
