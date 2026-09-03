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
 * spinning until the published frame version stops moving. That is a
 * bounded wait for a static document and nothing like a poll of an
 * animation.
 *
 * EDIT THESE FIRST
 *   kScrollBy  — how far down the page the third cell walks, px.
 *   kClickAt   — where the fourth cell presses, in view pixels.
 *   kScript    — the expression the second cell evaluates.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkCanvas.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(11, kInk, 0.4f),
          .note = label(10.5f, kAsh, 0.2f),
          .gap = 6,
          .noteMeasure = (float)kViewW};
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

/** The one engine, booted once for the process. */
std::shared_ptr<scry::WebEngine> engine() {
  static std::shared_ptr<scry::WebEngine> one = scry::WebEngine::create({});
  return one;
}

/** Waits until the view has published a frame and stopped changing it.
 *  Every call this sheet makes crosses to the web thread, so each stage
 *  ends here. The document is static, so this is a bounded wait for a
 *  load and a repaint rather than a poll of an animation. */
bool settled(scry::WebView& view) {
  using namespace std::chrono_literals;
  const auto deadline = std::chrono::steady_clock::now() + 15s;
  uint64_t published = 0;
  int stableTicks = 0;
  while (std::chrono::steady_clock::now() < deadline && stableTicks < 8) {
    const uint64_t version = view.frameVersion();
    stableTicks = (version > 0 && version == published) ? stableTicks + 1 : 0;
    published = version;
    std::this_thread::sleep_for(16ms);
  }
  return published > 0;
}

}  // namespace

struct WebScript final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE: an engine with its layout tables and
   *  certificates, which ship with the application rather than with its
   *  dylibs, so a build that links the SDK can still find nothing to lay
   *  out with. */
  static bool available(std::string* why) { return scry::available(why); }

  /** One view per cell. They are held for the sketch's life because a
   *  view IS the picture — the leaf draws whatever frame it last
   *  published. */
  std::vector<std::shared_ptr<scry::WebView>> views;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // every stage is done before the first frame

    const std::shared_ptr<scry::WebEngine> web = engine();
    if (!web) {
      ctx.composer.render(missing("the web engine did not boot"));
      return;
    }

    // ---- the load callback, on a view that is only loaded ------------
    auto loads = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<scry::WebView> plain = open(*web);
    plain->setLoadCallback([loads] { ++*loads; });
    plain->loadHTML(page());
    const bool painted = settled(*plain);
    const int fired = loads->load();

    // ---- the script, and what it evaluated to ------------------------
    auto answered = std::make_shared<std::mutex>();
    auto answer = std::make_shared<std::string>();
    std::shared_ptr<scry::WebView> scripted = open(*web);
    scripted->loadHTML(page());
    settled(*scripted);
    scripted->evaluateScript(kScript, [answered, answer](std::string result) {
      std::lock_guard<std::mutex> guard(*answered);
      *answer = std::move(result);
    });
    settled(*scripted);
    std::string returned;
    {
      std::lock_guard<std::mutex> guard(*answered);
      returned = *answer;
    }

    // ---- the wheel ---------------------------------------------------
    std::shared_ptr<scry::WebView> scrolled = open(*web);
    scrolled->loadHTML(page());
    settled(*scrolled);
    // A wheel's delta is what the CONTENT moves by, so moving DOWN the
    // page is a negative dy.
    scrolled->scroll(0, -kScrollBy);
    settled(*scrolled);

    // ---- the press ---------------------------------------------------
    std::shared_ptr<scry::WebView> pressed = open(*web);
    pressed->loadHTML(page());
    settled(*pressed);
    pressed->mouseMove(kClickAt.x(), kClickAt.y());
    pressed->mouseDown(kClickAt.x(), kClickAt.y());
    pressed->mouseUp(kClickAt.x(), kClickAt.y());
    settled(*pressed);

    views = {plain, scripted, scrolled, pressed};

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

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("DRIVING A PAGE \xc2\xb7 setLoadCallback + "
                           "evaluateScript + scroll + mouse"),
             .subtitle = toU8("dials \xc2\xb7 the script \xc2\xb7 the wheel "
                              "\xc2\xb7 the point pressed \xe2\x80\x94 one "
                              "document, four views, one call apart"),
             .footer = toU8("every call crosses to the web thread, so each "
                            "cell was driven and then waited on until its "
                            "published frame stopped moving"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11, kAsh, 0.6f),
             .footerStyle = label(10.5f, kAsh, 0.3f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("plain", plain, "loadHTML + setLoadCallback",
                           std::string("the load callback ") +
                               (fired > 0 ? "fired" : "never fired") +
                               ", and " +
                               (painted ? "a frame was published"
                                        : "nothing was published")),
                      cell("scripted", scripted,
                           "evaluateScript(js, onResult)",
                           std::string("the page answered \xe2\x80\x9c") +
                               returned + "\xe2\x80\x9d"),
                      cell("scrolled", scrolled, "scroll(0, -dy)", wheel),
                      cell("pressed", pressed,
                           "mouseMove / mouseDown / mouseUp", press)},
                 .gap = 18,
                 .divider = Fill::color(kRule)}))
            .absolute()
            .inset(0));
  }

  std::shared_ptr<scry::WebView> open(scry::WebEngine& web) const {
    return web.createView(kViewW, kViewH);
  }

  /** One cell: the view's latest published frame, at its own pixel size
   *  so nothing resamples. */
  static Element cell(std::string key, std::shared_ptr<scry::WebView> view,
                      const char* call, std::string note) {
    return kit::cell(
        voice(), toU8(call), toU8(note),
        custom(std::move(key),
               [view](SkCanvas& canvas, const PaintContext&) {
                 if (view) view->draw(canvas, SkRect::MakeWH((float)kViewW,
                                                             (float)kViewH));
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
