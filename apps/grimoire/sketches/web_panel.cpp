/** @file
 * web_panel — a live web page standing in a compose scene, Skia drawing
 * standing inside that page, and one document driven four ways.
 *
 * A PAGE AS A LEAF. The door is compose's `web(view)` leaf: a page is an
 * Element like any other, sized and placed by the layout around it. Both
 * directions are in the first row, because they are one picture — the
 * page's background is transparent, so the sheet's ground shows between
 * its cards, and the disc inside it is a Skia drawing the page displays
 * through an image slot it names (`WebImage`).
 *
 * ONE DOCUMENT, DRIVEN FOUR WAYS. Below it, four views load the same
 * document and each is driven by one call — the load alone, a script
 * evaluated in the page, a wheel, and a synthetic press — and each cell
 * says what the PAGE answered about itself, never what a timer guessed.
 *
 * ONE SKETCH, ONE ENGINE. A process is allowed exactly one web renderer
 * for its lifetime, so every view here comes from the engine held beside
 * the sketch, and all five are made before any is loaded: creating a view
 * is a call the engine answers on its own thread, and a page already
 * loading holds that thread.
 *
 * WHY THE PLATE IS REPRODUCIBLE. The engine repaints on its own thread on
 * its own cadence, which is a race a fixed-step capture would lose. So a
 * capture holds its own thread until each page is settled — the engine's
 * own events, never a stretch of clock: the load, the page's own answer to
 * its question, the view going quiet — and each driven cell draws the
 * frame its settle stopped on. A window waits for none of it: the same
 * settles are advanced from update() and the sheet is described again
 * when one lands. One sequence, two ways of driving it, decided by
 * ctx.deterministic.
 *
 * EDIT THESE FIRST
 *   kPage — the panel's document; page() — the driven one.
 *   kPageWidth / kPageHeight — the panel view's own pixels, which the
 *     layout around it is sized against.
 *   kScrollBy, kClickAt — where the wheel and the press take the views.
 */

// TAGS: Interfaces/Web

#include <include/core/SkCanvas.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/web/Web.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebImage.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scry/Settling.h>
#include <sigilsketch/scry/SharedEngine.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace scry = sigil::scry;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr SkSize kCanvas = {1360, 1120};
constexpr float kMeasure = 1280;

constexpr int kPageWidth = 640;
constexpr int kPageHeight = 470;
constexpr int kSlotSize = 168;

constexpr int kViewWidth = 300;
constexpr int kViewHeight = 236;
constexpr int kScrollBy = 220;  // px DOWN the page the third view moves
constexpr SkIPoint kClickAt = {150, 203};  // view pixels the fourth presses
constexpr const char* kScript =
    "document.querySelectorAll('.row').forEach((r,i)=>{"
    "r.textContent = 'row ' + (i+1) + ' \\u2014 rewritten';"
    "r.style.background = i%2 ? '#e6dccb' : '#f2ece0';});"
    "document.getElementById('head').textContent = 'EVALUATED';"
    "'rewrote ' + document.querySelectorAll('.row').length + ' rows'";

constexpr material::Color kCellGround{0.12f, 0.12f, 0.14f, 1};

/** The sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.spacing.captionGap = 6;
  return look;
}

/** The page. Transparent, so what is behind it in the scene is part of
 *  the composite rather than covered by it, and static, so it settles to
 *  one picture and stays there. */
constexpr const char* kPage = R"html(
<html><head><style>
  html, body { background: transparent; margin: 0; overflow: hidden;
               font-family: -apple-system, 'Helvetica Neue', sans-serif; }
  .board { box-sizing: border-box; width: 100%; height: 100vh; padding: 22px;
           display: grid; grid-template-columns: repeat(3, 1fr);
           grid-template-rows: auto 1fr 1fr; gap: 14px; }
  h1 { grid-column: 1 / -1; margin: 0; font-size: 30px; font-weight: 800;
       color: #7ee8ff; letter-spacing: -0.4px; }
  h1 em { color: #b18cff; font-style: normal; }
  .card { background: rgba(16, 20, 34, 0.82); border-radius: 14px;
          padding: 14px; border: 1px solid rgba(126, 232, 255, 0.25);
          box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5);
          display: flex; flex-direction: column; justify-content: space-between; }
  .card h2 { margin: 0 0 6px; font-size: 16px; color: #eef3ff; }
  .card p { margin: 0; font-size: 12px; line-height: 1.45; color: #a9b4cc; }
  .tag { align-self: flex-start; margin-top: 10px; padding: 3px 9px;
         border-radius: 999px; font-size: 11px; font-weight: 600;
         color: #061018; background: linear-gradient(135deg, #7ee8ff, #b18cff); }
  .wide { grid-column: span 2; }
  .slot { align-items: center; text-align: center; }
</style></head>
<body><div class="board">
  <h1>WebKit layout &mdash; <em>on the scene canvas</em></h1>
  <div class="card wide"><div>
    <h2>Grid, flexbox, gradients, shadows</h2>
    <p>This panel is HTML and CSS laid out by the web engine and handed
       over as an image, drawn into the box the surrounding layout gave
       it &mdash; the same Element vocabulary as everything around it.</p></div>
    <span class="tag">web(view)</span></div>
  <div class="card"><div>
    <h2>Transparent</h2>
    <p>The page paints no background, so the scene's own gradient is
       what shows between these cards.</p></div>
    <span class="tag">is_transparent</span></div>
  <div class="card slot"><div>
    <h2>And the other way</h2>
    <img src="__SIGIL_SLOT__.imgsrc" style="width:104px;height:104px" />
    <p>Skia, into a slot this page names.</p></div>
    <span class="tag">WebImage</span></div>
  <div class="card wide"><div>
    <h2>Frames are snapshots</h2>
    <p>Each repaint publishes one immutable frame with a version on it.
       The leaf always paints the newest; a static document publishes one
       and stops, which is what makes a still of it reproducible.</p></div>
    <span class="tag">frameVersion()</span></div>
</div></body></html>
)html";

/** The page bound to one session's image slot. Two copies of this sketch may
 * be resident under different source paths, and a reloaded session starts
 * while the old slot's removal is queued on the web thread, so each live
 * owner names its own slot rather than racing over one process-global name. */
std::string pageFor(const std::string& slot) {
  std::string page = kPage;
  constexpr const char* marker = "__SIGIL_SLOT__";
  if (const size_t at = page.find(marker); at != std::string::npos)
    page.replace(at, std::char_traits<char>::length(marker), slot);
  return page;
}

/** The disc the page displays: three rings and six chords, drawn with the
 *  pen into the slot's own canvas. */
void drawSigil(SkCanvas& canvas, float size) {
  const float centre = size / 2;
  const float radius = centre * 0.92f;
  const auto corner = [&](int i) {
    return arrange::onRing((size_t)(i % 6), 6,
                           {.center = {centre, centre},
                            .radii = {radius, radius},
                            .fromDegrees = 0.0f});
  };
  sigil::draw::on(canvas, {size, size}, [&](sigil::draw::Pen& pen) {
    pen.clear();
    pen.noFill();
    for (int i = 0; i < 3; ++i) {
      pen.strokeWeight(3.0f - (float)i);
      pen.stroke(i % 2 ? hexColor(0xb18cff) : hexColor(0x7ee8ff));
      pen.circle({centre, centre}, 2.0f * centre * (0.92f - 0.22f * (float)i));
    }
    pen.strokeWeight(1.4f);
    pen.stroke(hexColor(0x7ee8ff, 0.7f));
    for (int i = 0; i < 6; ++i)
      pen.line(sigil::geometry::path::toSk(corner(i)),
               sigil::geometry::path::toSk(corner(i + 2)));
  });
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

/** One remark beside the panel. */
Element remark(std::u8string heading, std::u8string body) {
  return sketch::kit::well({.width = 300, .padding = 14})
      .column()
      .gap(6)
      .children({document::h2(std::move(heading)).font({.size = 14}),
                 document::caption(std::move(body))});
}

}  // namespace

struct WebPanelSketch {
  /** WHAT THIS MACHINE MUST HAVE. The engine's ICU tables and
   *  certificate bundle ship with the application rather than with its
   *  dylibs, so a build that links the SDK can still find nothing to lay
   *  out with. */
  static bool available(std::string* why) { return scry::available(why); }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 1.0});

    // The settles go before the views they latched.
    m_pages.clear();
    m_views.clear();
    ++m_declared;
    const std::shared_ptr<scry::WebEngine> engine =
        sketch::scry::sharedEngine();
    if (!engine) {
      ctx.composer.render(missing("this sketch host has no shared web engine"));
      return;
    }
    m_slotName =
        "sigil-" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    // Registered before the page loads: a page naming a slot that does not
    // exist yet gets a warning and a hole.
    m_sigil = engine->createImage(m_slotName, kSlotSize, kSlotSize);
    if (m_sigil)
      m_sigil->paint(
          [](SkCanvas& canvas) { drawSigil(canvas, (float)kSlotSize); });

    // EVERY VIEW BEFORE ANY LOAD: the panel's, then the four driven ones.
    m_views.push_back(engine->createView(kPageWidth, kPageHeight));
    for (int cell = 0; cell < 4; ++cell)
      m_views.push_back(engine->createView(kViewWidth, kViewHeight));

    // Each settle owns its view's load, so nothing the engine says can
    // land between asking and listening; a capture is held here until
    // each page is there, a window comes straight back.
    const auto put = [&](sketch::scry::Sequence sequence) {
      m_pages.push_back(sketch::scry::settle(
          *m_views[m_pages.size()], std::move(sequence), ctx.deterministic));
    };
    put({.html = pageFor(m_slotName)});
    // The load says the document is here; the page going quiet says its
    // picture is finished.
    put({.html = page(),
         .question = "String(document.readyState)",
         .expected = "complete",
         .quiet = true,
         .whole = true});
    // The heading the script rewrites is the page's own statement that
    // the rewrite has landed AND been painted.
    put({.html = page(),
         .run = kScript,
         .question = "document.getElementById('head').textContent",
         .expected = "EVALUATED",
         .quiet = true,
         .whole = true});
    // A wheel's delta is what the CONTENT moves by, so down the page is a
    // negative y. The engine walks a wheel smoothly: the scroll offset
    // says where the walk is heading, the view going quiet that it is
    // over.
    put({.html = page(),
         .wheel = {0, -kScrollBy},
         .question = "String(window.scrollY)",
         .expected = std::to_string(kScrollBy),
         .quiet = true,
         .whole = true});
    // The class the page's own handler adds is its statement that the
    // click arrived and the button was repainted in it.
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
   *  again on the frame any finishes or the panel first paints its
   *  document. A capture has finished them all and this moves nothing. */
  void update(double, sketch::Scene2DContext& ctx) {
    bool landed = false;
    for (const std::unique_ptr<sketch::scry::Settling>& settling : m_pages)
      landed = settling->advance() || landed;
    if (landed || (!m_pages.empty() && m_pages[0]->painted() != m_shownPanel))
      describe(ctx);
  }

  void describe(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    m_shownPanel = m_pages[0]->painted();
    const bool arrived =
        std::all_of(m_pages.begin(), m_pages.end(),
                    [](const auto& settling) { return settling->arrived(); });
    const bool expired =
        std::any_of(m_pages.begin(), m_pages.end(),
                    [](const auto& settling) { return settling->broken(); });
    const auto answered = [&](size_t at, const std::string& answer) {
      return m_pages[at]->arrived() ? answer
                                    : std::string("awaiting the page");
    };
    ctx.composer.render(sketch::kit::page(
        {.title = "A page as a leaf",
         .subtitle = "HTML laid out by the web engine inside the sheet, Skia "
                     "inside the page, then one document driven four ways",
         .footer = expired   ? "A settling wait expired; inspect the result "
                               "before relying on this capture."
                   : arrived ? "Every page has answered and the views have "
                               "finished painting."
                             : "The pages are still loading or repainting."},
        box().column().gap(30).children(
            {sketch::kit::section(
                 {.label = "BOTH DIRECTIONS",
                  .note = "web(view) draws the page's newest frame; the "
                          "page's background is transparent"},
                 box().row().gap(28).alignItems(Align::Start).children(
                     {box()
                          .width(kPageWidth)
                          .height(kPageHeight)
                          .borderRadius({16})
                          .overflow(Overflow::Clip)
                          .background(
                              shadow(hexColor(0x000000, 0.55f), {0, 10}, 26))
                          .children({m_shownPanel
                                         ? web(m_views[0])
                                               .width(kPageWidth)
                                               .height(kPageHeight)
                                         : box()
                                               .width(kPageWidth)
                                               .height(kPageHeight)}),
                      box().column().gap(14).children(
                          {remark(u8"HTML → canvas",
                                  u8"The engine publishes each repaint as a "
                                  u8"frame; the leaf draws the newest one "
                                  u8"into the bounds the layout gave it."),
                           remark(u8"canvas → HTML",
                                  u8"A slot the page names by URL, filled by "
                                  u8"drawing into the canvas the engine hands "
                                  u8"back."),
                           remark(u8"one renderer",
                                  u8"A process boots exactly one engine, so "
                                  u8"it is held beside the sketch rather than "
                                  u8"inside it.")})})),
             sketch::kit::section(
                 {.label = "ONE DOCUMENT, DRIVEN FOUR WAYS",
                  .note = "each view loads the same HTML; one call changes "
                          "its document, viewport or interaction state"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("plain", 1, "LOAD", "loadHTML",
                                kit::formatted(
                                    "The reference · loaded %s, painted %s",
                                    m_pages[1]->loaded() ? "yes" : "no",
                                    m_pages[1]->painted() ? "yes" : "no")),
                           cell("scripted", 2, "EVALUATE", "evaluateScript",
                                "Heading, rows and fills rewritten · " +
                                    answered(2, m_pages[2]->reply())),
                           cell("scrolled", 3, "SCROLL",
                                kit::formatted("scroll(0, -%d)", kScrollBy),
                                "Only the viewport moves · " +
                                    answered(3, kit::formatted(
                                                    "scrollY %d", kScrollBy))),
                           cell("pressed", 4, "PRESS", "mouseMove / Down / Up",
                                "The page's own handler marks the button · " +
                                    answered(4, "class hit"))},
                      .measure = kMeasure,
                      .gap = 24}))})));
  }

  /** One driven cell: THE FRAME ITS SETTLE STOPPED ON, at its own pixel
   *  size so nothing resamples — and the view itself while that frame is
   *  still coming. ONE DRAWING UNDER ONE OF TWO KEYS, because a key is a
   *  program's identity: a page still arriving is the view's own latest,
   *  which changes without a describe and is declared volatile, and a
   *  still is one frame of the engine's, named by the declaration that
   *  took it and cached like any static leaf. A GPU engine publishes a
   *  texture it reuses, so there is no image to keep and the view draws
   *  its latest. */
  sketch::kit::ComparisonCase cell(const std::string& name, size_t at,
                                   const char* title, std::string control,
                                   std::string note) const {
    const SkRect where = SkRect::MakeWH((float)kViewWidth, (float)kViewHeight);
    scry::WebView::Frame still = m_pages[at]->still();
    const std::string key = still.image ? name + " · still " +
                                              std::to_string(m_declared)
                                        : name + " · arriving";
    // The view's own latest only once a repaint carries the loaded
    // document: the blank a view paints the moment it exists is the
    // engine's page, not this one's.
    Element picture =
        custom(key, [view = m_views[at], settling = m_pages[at].get(),
                     still = std::move(still), where](sigil::draw::Pen& pen) {
          SkCanvas& canvas = *pen.canvas();
          if (still.image)
            canvas.drawImageRect(still.image, where,
                                 SkSamplingOptions(SkFilterMode::kLinear));
          else if (view && settling->painted())
            view->draw(canvas, where);
        });
    if (!still.image) picture.cache(Cache::None);
    return {.title = title,
            .control = std::move(control),
            .figure = std::move(picture)
                          .width((float)kViewWidth)
                          .height((float)kViewHeight)
                          .fill(Fill::color(kCellGround)),
            .note = std::move(note)};
  }

  /** What stands here when the engine has nothing to lay out with. A
   *  sweep never reaches it, having asked the registry first; the app
   *  opens whatever is selected, so it is drawn rather than left blank. */
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

 private:
  std::string m_slotName;
  std::shared_ptr<scry::WebImage> m_sigil;
  /** The panel's view, then the four driven views. */
  std::vector<std::shared_ptr<scry::WebView>> m_views;
  /** One settle per view, after the views, so the events each latched
   *  are released while its view still stands. */
  std::vector<std::unique_ptr<sketch::scry::Settling>> m_pages;
  /** How many times the sheet has been declared: what names a still
   *  apart from an earlier declaration's under the same cell. */
  int m_declared = 0;
  /** Whether the sheet last described showed the panel's page rather
   *  than its well. */
  bool m_shownPanel = false;
};

SIGIL_SKETCH(WebPanelSketch, "Start & fixtures",
             "a live web page as a compose leaf with Skia drawn back into "
             "the page, then one document in four views, each driven by one "
             "call and answered by the page itself")
