// web_panel.cpp — a live web page standing in a compose scene, and Skia
// drawing standing inside that page.
//
// The door is compose's `web(view)` leaf: a page is an Element like any
// other, sized and placed by the layout around it. Both directions are
// here, because they are one picture — the page's background is
// transparent, so the scene's gradient shows between its cards, and the
// disc inside it is a Skia drawing the page displays through an image
// slot it names.
//
// ONE SKETCH FOR BOTH BACKENDS, and it could not honestly be two. A
// process is allowed exactly one web renderer for its lifetime, so a
// second sketch that booted its own engine would get nothing in a
// Sketchbook that already has one. And there would be nothing to see
// anyway: `WebView::draw()` absorbs which renderer produced the frame —
// a wrapped texture on a device engine, a raster frame on this one — so
// the two spellings of this picture would be the same call.
//
// WHY THE PLATE IS REPRODUCIBLE. The engine repaints on its own thread
// on its own cadence, which is a race a fixed-step capture would lose.
// So the page is loaded and awaited to a settled frame while the sketch
// is being declared: a static document publishes one picture and then
// stops, and every frame after that draws the same published frame no
// matter when it is asked for.
//
// AND THE ONE WAY IT IS NOT. The wait has a wall-clock deadline, so a
// machine on which the engine takes longer than that draws a card saying
// what is missing instead of the page — the same build, a different
// picture, decided by how fast the machine ran. A plate that shows the
// card is a plate of the deadline, not of this sketch.
//
// EDIT THESE FIRST
//   kPage                     — the document. It is the subject.
//   kPageWidth / kPageHeight  — the view's own pixels, which the layout
//                               around it is sized against.

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcompose/web/Web.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebImage.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace scry = sigil::scry;

using namespace sigil::compose;

namespace {

constexpr int kPageWidth = 640;
constexpr int kPageHeight = 470;
constexpr int kSlotSize = 168;

constexpr SkColor4f kInk = hex(0xe8ecf6);
constexpr SkColor4f kDim = hex(0x93a0bd);

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
    <img src="sigil.imgsrc" style="width:104px;height:104px" />
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

/** The disc the page displays: rings and chords, drawn with Skia into
 *  the slot's own canvas. */
void drawSigil(SkCanvas& canvas, float size) {
  canvas.clear(SkColors::kTransparent);
  const float centre = size / 2;
  SkPaint ring;
  ring.setAntiAlias(true);
  ring.setStyle(SkPaint::kStroke_Style);
  for (int i = 0; i < 3; ++i) {
    ring.setStrokeWidth(3.0f - (float)i);
    ring.setColor4f(i % 2 ? hex(0xb18cff) : hex(0x7ee8ff), nullptr);
    canvas.drawCircle(centre, centre, centre * (0.92f - 0.22f * (float)i),
                      ring);
  }
  SkPaint chord;
  chord.setAntiAlias(true);
  chord.setStyle(SkPaint::kStroke_Style);
  chord.setStrokeWidth(1.4f);
  chord.setColor4f(hex(0x7ee8ff, 0.7f), nullptr);
  const float radius = centre * 0.92f;
  for (int i = 0; i < 6; ++i) {
    const float a = (float)i * 2.0f * (float)M_PI / 6.0f;
    const float b = (float)((i + 2) % 6) * 2.0f * (float)M_PI / 6.0f;
    canvas.drawLine(
        centre + radius * std::cos(a), centre + radius * std::sin(a),
        centre + radius * std::cos(b), centre + radius * std::sin(b), chord);
  }
}

/** THE ONE ENGINE THIS PROCESS IS ALLOWED, and the one slot it
 *  publishes.
 *
 *  Both are process-wide rather than per-sketch: a renderer may be
 *  created once for the lifetime of the program, and a slot's name is
 *  unique per engine — two sessions of this sketch overlap while a host
 *  reopens it, and the second would be asking for a name the first still
 *  holds. */
struct Browser {
  std::shared_ptr<scry::WebEngine> engine;
  std::shared_ptr<scry::WebImage> sigil;
};

const Browser& browser() {
  static const Browser one = [] {
    Browser started;
    started.engine = scry::WebEngine::create({});
    if (!started.engine) return started;
    // Registered before any page loads: a page naming a slot that does
    // not exist yet gets a warning and a hole.
    started.sigil = started.engine->createImage("sigil", kSlotSize, kSlotSize);
    if (started.sigil)
      started.sigil->paint(
          [](SkCanvas& canvas) { drawSigil(canvas, (float)kSlotSize); });
    return started;
  }();
  return one;
}

/** Waits for the page to publish a frame and stop changing it. The
 *  document is static, so this is a bounded wait for the load and the
 *  first paint rather than a poll of an animation. */
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

Element note(std::u8string heading, std::u8string body) {
  return box()
      .width(236)
      .corners({12})
      .padding(14)
      .fill(Fill::color(hex(0x121a2c, 0.86f)))
      .foreground(stroke(1.0f, Fill::color(hex(0x7ee8ff, 0.22f))))
      .column()
      .gap(6)
      .child(text(std::move(heading), weave::textStyle({.size = 14, .color = kInk})))
      .child(text(std::move(body), weave::textStyle({.size = 11.5f, .color = kDim})));
}

}  // namespace

struct WebPanelSketch final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE. The engine's ICU tables and
   *  certificate bundle ship with the application rather than with its
   *  dylibs, so a build that links the SDK can still find nothing to lay
   *  out with. */
  static bool available(std::string* why) {
    return scry::available(why);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(980, 660);
    ctx.background(hex(0x0b0a16));
    ctx.captureAt(1.0);

    std::string why;
    if (!browser().engine) {
      why = "the web engine did not boot in this process";
    } else {
      m_view = browser().engine->createView(kPageWidth, kPageHeight);
      m_view->loadHTML(kPage);
      if (!settled(*m_view)) {
        why = "the page never published a frame";
        m_view.reset();
      }
    }
    ctx.composer.render(m_view ? scene() : unavailable(why));
  }

  [[nodiscard]] Element scene() const {
    return stack()
        .fill(linearGradient({0, 0}, {0, 660},
                             {hex(0x140e26), hex(0x241033), hex(0x0d1424)}))
        .child(text(u8"A PAGE AS A LEAF",
                    weave::textStyle({.size = 15, .color = kInk, .track = 2.4f}))
                   .left(40)
                   .top(32))
        // The page at its own pixel size: the view is created at exactly
        // the box it is laid into, so nothing resamples.
        .child(box()
                   .inset(40, 96, 300, 94)
                   .corners({16})
                   .clip()
                   .background(shadow(hex(0x000000, 0.55f), {0, 10}, 26))
                   .child(web(m_view).width(kPageWidth).height(kPageHeight)))
        .child(box()
                   .left(704)
                   .top(96)
                   .column()
                   .gap(14)
                   .child(note(u8"HTML → canvas",
                               u8"The engine publishes each repaint as a "
                               u8"frame; the leaf draws the newest one into "
                               u8"the bounds the layout gave it."))
                   .child(note(u8"canvas → HTML",
                               u8"A slot the page names by URL, filled by "
                               u8"drawing into the canvas the engine hands "
                               u8"back — no adapter either way."))
                   .child(note(u8"one renderer",
                               u8"A process boots exactly one engine, so it "
                               u8"is held beside the sketch rather than "
                               u8"inside it.")))
        .child(text(u8"the page background is transparent — the scene's "
                    u8"gradient is what shows between its cards",
                    weave::textStyle({.size = 12, .color = kDim}))
                   .left(40)
                   .top(590));
  }

  /** What a host shows in place of the piece when the engine has nothing
   *  to lay out with. It is drawn rather than left blank because the app
   *  opens whatever is selected; a sweep never reaches it, having asked
   *  the registry first. */
  [[nodiscard]] static Element unavailable(const std::string& why) {
    return stack()
        .fill(Fill::color(hex(0x0b0a16)))
        .child(box()
                   .inset(40, 40, 40, 40)
                   .corners({16})
                   .padding(28)
                   .fill(Fill::color(hex(0x121a2c, 0.9f)))
                   .foreground(stroke(1.0f, Fill::color(hex(0x7ee8ff, 0.2f))))
                   .column()
                   .gap(10)
                   .child(text(u8"no web engine here",
                               weave::textStyle({.size = 22, .color = kInk})))
                   .child(text(toU8(why), weave::textStyle({.size = 13, .color = kDim}))));
  }

 private:
  std::shared_ptr<scry::WebView> m_view;
};

SIGIL_SKETCH(WebPanelSketch, "Start & fixtures",
             "A live web page as a compose leaf, and Skia drawn back into "
             "the page")
