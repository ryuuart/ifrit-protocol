// vertigo_titles.cpp — the spirograph passage of Saul Bass's title
// sequence for Alfred Hitchcock's VERTIGO (Paramount, 1958, 1.85:1),
// whose curves John Whitney drew on a converted WWII M-5 anti-aircraft
// gun director.
//
// ---------------------------------------------------------------------
// WHAT IS FROM THE RECORD, AND WHAT IS THIS STUDY'S OWN RECONSTRUCTION
// ---------------------------------------------------------------------
// FROM THE RECORD (sources read directly; see the citation block that
// this sketch prints on its own canvas):
//   * artofthetitle.com/title/vertigo — Bass design, Whitney spirals,
//     Herrmann score, Paramount, 1.85:1; the sequence described as "an
//     odd synthesis" of Novak close-ups and "spirographic imagery...
//     called Lissajous waves."
//   * typotheque.com, Emily King, "Taking Credit: Film Title Sequences
//     1955–1965 / 5. Spiralling Aspirations: Vertigo (1958)" — the shot
//     order (mouth, then eyes, "the screen is suddenly stained red,"
//     the title expanding out of the pupil); the equipment "adapted
//     from the radar equipment used in the second world war"; and the
//     TYPOGRAPHY, which is the reason this sketch has two type
//     registers: "all credits appear in serif capitals... larger
//     titles... in outline type through which the image beneath can be
//     seen," body credits "in solid black capitals of the same
//     typeface." Also: Herrmann's score is "loosely synchronized," not
//     beat-matched — which is why the timing here is musical rather
//     than frame-locked.
//   * patrycerichter.wordpress.com/2016/12/15 — a timestamped shot
//     breakdown of one surviving cut; the spirals described as "cool
//     tones: blue and purple, as well as warm tones: orange and red."
//     That sentence is the ONLY colour source located for the spirals.
//   * hitchcocksvertigo.substack.com "Saul Bass and John Whitney",
//     cross-checked against diyphotography.net, dish.andrewsullivan.com
//     ("Hitchcock's Artillery") and rhizome.org ("Did Vertigo Introduce
//     Computer Graphics to Cinema?") — the mechanism: a WWII M-5
//     anti-aircraft gun director, 850 lb, ~11,000 components, crewed by
//     five; its rotating gun plate held electrical contact through a
//     full turn (so the stand could spin forever without tangling);
//     Whitney bolted a flat aluminium plate over it for the artwork and
//     linked its rotation to a ceiling-hung pendulum whose pen was fed
//     from a 24-foot pressurized ink reservoir. Directly stated there:
//     "Bass drew spiraling, twisting shapes based on graphs of
//     parametric equations by the 19th-century mathematician Jules
//     Lissajous."
//   * fontsinuse.com "Vertigo Opening Titles" (corroborating
//     Typotheque) — the faces are Clarendon (titles) and News Gothic
//     (body). macOS ships SuperClarendon, Apple's Clarendon revival, so
//     the display register here is a REAL Clarendon; News Gothic is not
//     installed and is stood in for by Helvetica Neue, condensed 0.95.
//
// THIS STUDY'S OWN, NOT MEASURED — flagged rather than smuggled:
//   * The four a:b:δ:k:R presets. No source read gives Whitney's actual
//     frequency ratios or the M-5's gearing. They are small-integer
//     Lissajous ratios chosen because they read as flowers rather than
//     open loops — mathematically real, historically invented.
//   * All four spiral hexes (#E0601A / #C81E2C / #1C4F9C / #5A2E82).
//     They are a saturated-jewel realization of patrycerichter's four
//     colour WORDS. No frame grab or colorimetry of the Technicolor
//     dye-transfer print was located.
//   * The chrome palette, the panel geometry, and every millisecond in
//     the timing tables. No public EDL of the sequence survives.
//   * The DAMPING term in §4's math. The brief's derivation has none;
//     every real harmonograph and every real pendulum damps, and
//     Whitney's pen hung from a ceiling on a physical string, so a small
//     exp(-λt) envelope is added per card. It is what makes the figure
//     spiral INWARD rather than retrace one closed rosette — i.e. what
//     makes it look like the film. Called out because it is an addition.
//   * Amplitudes are the brief's R table scaled by 0.88. The rotated
//     composition reaches R·√2 from centre, not R — the raw table plus
//     filament()'s glow overflows a 596px-tall panel.
//   * T = 12π, not the brief's 6π. Three pendulum periods draws a clean
//     wireframe rosette — a plotted function. Six, under the damping
//     envelope, is where it turns into ink.
//   * The credit line is set in CLARENDON, not the gothic. Typotheque
//     says the body credits are "solid black capitals of the SAME
//     typeface" as the titles, so News Gothic's stand-in is confined to
//     the study's own chrome.
//   * The ring legends on the limbus, the per-card corner slugs, and the
//     concentric pupil/limbus rings are study chrome, not the film.
//
// ---------------------------------------------------------------------
// THE ALGORITHM (the rig, translated)
// ---------------------------------------------------------------------
// The pendulum swings in two harmonic axes over the table; the table
// turns underneath it. So the curve the camera photographs, in the
// STATIONARY frame, is a Lissajous figure composed with a continuous
// rotation — a precessing rosette:
//
//     t   = i/(N-1) · T                 T = 12π (6 pendulum periods)
//     env = exp(-damp·t)                (this study's damping)
//     xl  = R·env·sin(a·t + δ)          pendulum, in the table's frame
//     yl  = R·env·sin(b·t)
//     θ   = k·t                         the table's own rotation
//     p   = centre + rot(θ) · (xl, yl)
//
// Sampled once into an open SkPath and handed to Element::outline();
// revealed by Element::trim(0, &growth) at a CONSTANT rate (easeNone —
// a motor does not ease); spun forever by one shared rotate(&spin).
//
// ---------------------------------------------------------------------
// WHERE THE LIBRARY MADE THIS AWKWARD (the honest list)
// ---------------------------------------------------------------------
//  1. No shapes::parametric()/lissajous(). Shapes.h generates closed
//     SHAPES; nothing evaluates a caller's t → (x, y). The curve is a
//     hand-rolled SkPathBuilder loop inside outline().
//  2. No derived Outputs. `penTip` must be a second, independently-owned
//     Output the ticker re-copies from `growth − 0.008` every tick; so
//     must `penA`. Four cards × two shadow cells = eight scalars kept in
//     sync by hand.
//  3. ONE trim window per NODE. trim() is an Element property and
//     decorations receive the already-trimmed outline, so the pen-tip
//     highlight is not a second stroke() — it is a duplicate node that
//     rebuilds and re-measures the same 2000-segment path.
//  4. Kinetic text is solid-fill only. paintKineticText() reduces every
//     glyph to (font, colour, RSXform) and drops the style's SkPaint, so
//     hollow display caps and glyphFx are mutually exclusive. "VERTIGO"
//     rebuilds pop() one tier up: seven letter nodes under
//     staggerChildren(30ms). Element::onPath() has the same limitation.
//
// Two more turned up in Element::onPath() and were fixed the same night
// (8498b1d), so the ring captions below now read the straightforward way
// rather than around a workaround: autoFlip used to turn each glyph over
// IN PLACE, which mirrors the run ("TECHNICOLOR" came out "ROLOCINHCET"),
// and a centred run at at = 0 dropped every glyph at a negative distance,
// eating half the caption on a ring where fraction 0 and 1 are the same
// point.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/vertigo_titles.cpp \
//       --frame shots/vertigo_titles.png --at 5.2
//
//   Motion check (the pen edge advancing along arc length across card B's
//   1400 ms draw window):  --at 4.30 --frames 8 --fps 5

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/Style.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// chrome palette — this study's own (film-base warm black, deliberately
// warmer than a neutral UI near-black)

constexpr SkColor4f kInk = hex(0x0A0806);      // canvas
constexpr SkColor4f kPlate = hex(0x110D0A);    // sidebar plates
constexpr SkColor4f kBone = hex(0xEDE6D8);     // primary type
constexpr SkColor4f kSteel = hex(0x8A7D68);    // secondary type
constexpr SkColor4f kSteelDim = hex(0x8A7D68, 0.62f);
constexpr SkColor4f kKeyline = hex(0x3A342C);  // panel keylines
constexpr SkColor4f kSolidInk = hex(0x050403); // "solid black capitals"

// ---------------------------------------------------------------------------
// canvas / panel geometry — 1480x800 is the film's own 1.85:1

constexpr float kW = 1480.0f;
constexpr float kH = 800.0f;
constexpr float kPad = 36.0f;
constexpr float kPanelW = 900.0f;
constexpr float kPanelH = 596.0f;
constexpr SkPoint kEye{kPanelW * 0.5f, kPanelH * 0.5f}; // 450, 298
constexpr float kSideW = 476.0f;

// ---------------------------------------------------------------------------
// the four spiral cards. a/b/delta/k/R are this study's invention (see
// the header); damp is too. `T = 12π` for all four.

struct Card {
  const char *tag;
  float a, b;         // Lissajous frequency ratio
  float deltaDeg;     // pendulum phase offset
  float k;            // precession (table turns per radian of swing)
  float amp;          // R, px, before the 0.88 fit scale
  float damp;         // exp(-damp·t) envelope — this study's addition
  SkColor4f core;     // ink colour
  const char *line1;  // sidebar index caption
  const char *line2;
};

constexpr float kFit = 0.88f; // R·√2 + glow must clear kPanelH/2
// The brief specifies T = 6π (3 pendulum periods). At three periods the
// figure reads as a clean wireframe rosette — a plotted function. Whitney's
// cards are dense nested ink; six periods under the damping envelope is
// where it turns over. Deviation from the brief, deliberate.
constexpr float kT = 12.0f * kPi;
constexpr int kSamples = 2000;

const std::array<Card, 4> kCards = {{
    {"A", 3, 2, 90.0f, 0.15f, 200.0f, 0.035f, hex(0xE0601A),
     "A — WARM / ORANGE · a:b = 3:2 · δ 90°",
     "k 0.15 · R 176 px · 3-petal rosette, slow precession"},
    {"B", 5, 4, 0.0f, 0.10f, 195.0f, 0.020f, hex(0xC81E2C),
     "B — RED · a:b = 5:4 · δ 0°",
     "k 0.10 · R 172 px · tight weave, near-static precession"},
    {"C", 2, 1, 45.0f, 0.22f, 205.0f, 0.045f, hex(0x1C4F9C),
     "C — COOL / BLUE · a:b = 2:1 · δ 45°",
     "k 0.22 · R 180 px · figure-eight base, fast precession"},
    {"D", 5, 3, 60.0f, 0.12f, 190.0f, 0.030f, hex(0x5A2E82),
     "D — PURPLE · a:b = 5:3 · δ 60°",
     "k 0.12 · R 167 px · 5-lobe flower"},
}};

/** The pendulum-over-turntable curve, sampled into an OPEN contour.
 *  `amplitude` overrides the card's R (the sidebar chips draw the same
 *  figure at 15 px). Centred on the node's own box. */
std::function<SkPath(SkSize)> lissajous(Card c, float amplitude,
                                        int samples = kSamples) {
  return [c, amplitude, samples](SkSize s) {
    SkPathBuilder p;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float d = c.deltaDeg * kDeg;
    for (int i = 0; i < samples; ++i) {
      const float t = (float)i / (float)(samples - 1) * kT;
      const float env = std::exp(-c.damp * t);
      const float xl = amplitude * env * std::sin(c.a * t + d);
      const float yl = amplitude * env * std::sin(c.b * t);
      const float th = c.k * t;
      const float ct = std::cos(th), st = std::sin(th);
      const float x = xl * ct - yl * st;
      const float y = xl * st + yl * ct;
      if (i == 0)
        p.moveTo(cx + x, cy + y);
      else
        p.lineTo(cx + x, cy + y);
    }
    return p.detach(); // deliberately NOT closed
  };
}

// ---------------------------------------------------------------------------
// type

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0,
                             float condense = 1.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** The OUTLINE register: PaintLayer::outline()'s stroked paint installed
 *  as the node's ENTIRE foreground pass — no fill underneath, so the
 *  spiral is visible straight through the counters. Typotheque: "outline
 *  type through which the image beneath can be seen." */
sigil::weave::TextStyle hollow(sk_sp<SkTypeface> face, float size,
                               SkColor4f color, float width,
                               float tracking = 0) {
  sigil::weave::TextStyle s = type(std::move(face), size, color, tracking);
  s.paint.foreground =
      sigil::weave::PaintLayer::outline(color.toSkColor(), width).paint;
  s.paint.foreground.setAntiAlias(true);
  return s;
}

Transition ramp(float delayMs, float durMs, ch::EaseFn ease = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

/** A clockwise circle of radius @p r centred on the node's box, starting
 *  at @p startDeg — the baseline for the onPath() ring captions.
 *  Fraction 0.25 is 12 o'clock and 0.75 is 6 o'clock at the default
 *  start. The last sample repeats the first, so the contour reads as
 *  geometrically closed and centred runs straddle the seam. */
std::function<SkPath(SkSize)> circlePath(float r, float startDeg = 180.0f) {
  return [r, startDeg](SkSize s) {
    SkPathBuilder p;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    for (int i = 0; i <= 360; ++i) {
      const float a = (startDeg + (float)i) * kDeg;
      const SkPoint q{cx + r * std::cos(a), cy + r * std::sin(a)};
      if (i == 0)
        p.moveTo(q);
      else
        p.lineTo(q);
    }
    return p.detach();
  };
}

/** A concentric ring on the panel — the pupil edge and the limbus, which
 *  are what make a radial ramp read as an EYE rather than a vignette. */
Element ring(float r, SkColor4f color, float width) {
  return disc(kEye, r) // util::disc — was four lines of box() arithmetic
      .corners({r})
      .fill(Fill::none())
      .stroke(stroke(width, Fill::color(color)));
}

Element plate(float height) {
  return box()
      .height(height)
      .corners({8})
      .padding(16)
      .column()
      .clip(true)
      .fill(Fill::color(kPlate))
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner));
}

} // namespace

// ===========================================================================

struct VertigoTitles : sigil::compose::sketch::Sketch {
  // --- the perpetual loop's live cells (14 scalars, all hand-stepped) ---
  ch::Output<float> spin{0};
  std::array<ch::Output<float>, 4> growth{};   // trim end   — the pen
  std::array<ch::Output<float>, 4> penTip{};   // trim start — growth − ε
  std::array<ch::Output<float>, 4> cardA{};    // card opacity
  std::array<ch::Output<float>, 4> penA{};     // nib opacity (fades at arrival)

  sk_sp<SkTypeface> faceDisplay, faceGothic, faceGothicBold;
  Material irisMat, filmGrain, paperGrain;

  // ------------------------------------------------------------------
  // one spiral card = TWO nodes over the same curve. The library has one
  // trim window per NODE (trim() is an Element property; decorations
  // receive the already-trimmed outline as PaintContext::outline), so a
  // pen-tip highlight riding just behind the drawing edge cannot be a
  // second stroke on the same node — it is a sibling with an identical
  // outline and its own [penTip, growth] window.
  void spiralCard(Element &into, int i) {
    const Card &c = kCards[i];
    const float R = c.amp * kFit;
    const std::string tag = c.tag;

    into.child(box()
                   .key("curve" + tag)
                   .inset(0)
                   .outline(lissajous(c, R))
                   .stroke(brushes::filament(c.core, hex(0xFFE9CF), 0.48f))
                   .trim(0.0f, &growth[i])
                   .rotate(&spin)
                   .opacity(&cardA[i]));

    // the nib: a short bright plus-blended window at the trailing edge
    into.child(box()
                   .key("nib" + tag)
                   .inset(0)
                   .outline(lissajous(c, R))
                   .stroke(brushes::pulse({1.0f, 0.90f, 0.72f, 0.42f},
                                          {1, 1, 1, 0.95f}, 0.7f))
                   .trim(&penTip[i], &growth[i])
                   .rotate(&spin)
                   .opacity(&penA[i]));
  }

  // ------------------------------------------------------------------
  Element screenPanel() {
    auto panel = box()
                     .width(kPanelW)
                     .height(kPanelH)
                     .shrink(0)
                     .corners({10})
                     .clip(true)
                     .key("screen");

    // perf-pass: irisMat (Material::blend of a radial + a sweep, built once
    // in setup) is STATIC but 85 ms/frame of live eval on raster. It was the
    // panel's fill; move it into a cached background child so the panel's
    // LIVE children (the red stain, the precessing spiral cards, the VERTIGO
    // entrance) still animate every frame over a one-time blit. Clipped to
    // the same rounded panel, so pixels are unchanged. Cache STICKS (static).
    panel.child(box().inset(0).fill(irisMat).cache(Cache::Texture));

    panel.child(ring(61.0f, hex(0x090604, 0.85f), 3.0f)
                    .key("pupil-edge")
                    .opacity(withFrom(0.0f, 1.0f, ramp(300, 420))));
    panel.child(ring(146.0f, hex(0x2A1D10, 0.40f), 1.2f).key("iris-mid"));
    panel.child(ring(262.0f, hex(0x120C07, 0.24f), 10.0f).key("limbus"));

    // "the screen is suddenly stained red" — kColor keeps the iris's
    // luminance and swaps its hue/saturation, so it TINTS rather than
    // covers. Sudden onset: easeInQuad.
    panel.child(box()
                    .key("stain")
                    .inset(0)
                    .blend(SkBlendMode::kColor)
                    .fill(withFrom(Fill::color(hex(0x3A2A1C)),
                                   Fill::color(hex(0xC81E2C)),
                                   ramp(700, 500, ch::easeInQuad))));

    for (int i = 0; i < 4; ++i)
      spiralCard(panel, i);

    // VERTIGO — hollow Clarendon expanding out of the pupil.
    //
    // This SHOULD be one text() with glyphFx(glyphfx::pop()). It can't be:
    // paintKineticText() collapses every glyph to (font, color, RSXform)
    // and drops the style's SkPaint entirely, so kinetic text is always a
    // solid fill — the hollow register and per-glyph motion are mutually
    // exclusive in the library today. pop() is therefore rebuilt one tier
    // up: seven letter nodes in a row, staggerChildren() cascading their
    // withFrom() entrances, easeOutBack(1.70158) per letter — the same
    // curve glyphfx::pop() applies internally.
    auto word = box()
                    .row()
                    .gap(3)
                    .alignItems(Align::Baseline)
                    .centerAt(kEye)
                    .key("vertigo")
                    .staggerChildren(30ms);
    {
      auto face = hollow(faceDisplay, 76, kBone, 2.2f);
      // Legibility underlay over the busiest cards. NOT dropShadow() —
      // that is a FILLED blurred copy and would plug the counters, which
      // is the one thing the outline register exists to keep open. A
      // blurred STROKE hugs the letterform and leaves the spiral visible
      // straight through it.
      {
        SkPaint halo;
        halo.setAntiAlias(true);
        halo.setStyle(SkPaint::kStroke_Style);
        halo.setStrokeWidth(7.0f);
        halo.setColor(0xA6000000);
        face.paint.underlays.push_back(
            sigil::weave::PaintLayer::blurred(halo, 3.5f));
      }
      const char *letters[] = {"V", "E", "R", "T", "I", "G", "O"};
      for (int i = 0; i < 7; ++i)
        word.child(
            text(toU8(letters[i]), face)
                .key(std::string("v") + letters[i])
                .scale(withFrom(0.30f, 1.0f, ramp(780, 480, ease::outBack())))
                .opacity(withFrom(0.0f, 1.0f, ramp(780, 220))));
    }
    panel.child(std::move(word));

    // the other register — "solid black capitals of the SAME typeface"
    // (Typotheque), so Clarendon again, not the gothic. Deliberately
    // unadorned next to VERTIGO's hollow display caps, and deliberately
    // laid over the busiest part of the card: that is where the film puts
    // its body credits too.
    panel.child(text(toU8("TITLE DESIGN SAUL BASS · SPIRALS JOHN WHITNEY"),
                     type(faceDisplay, 15, kSolidInk, 2.6f))
                    .key("credit")
                    .centerAt({kEye.x(), kEye.y() + 152.0f})
                    .opacity(withFrom(0.0f, 1.0f, ramp(1550, 300)))
                    .translateY(withFrom(10.0f, 0.0f, ramp(1550, 300))));

    // the instrument-dial legend, set on the limbus itself with the
    // brand-new Element::onPath() — one text leaf where hand-placing
    // curved lettering would have been one leaf and one measure() per
    // glyph.
    panel.child(
        text(toU8("JOHN WHITNEY · M-5 GUN DIRECTOR · PENDULUM OVER PLATE"),
             type(faceGothic, 11, hex(0xEDE6D8, 0.42f), 3.4f))
            .key("ring-top")
            .inset(0)
            .onPath({.path = circlePath(272.0f),
                     .at = 0.25f,
                     .align = TextPath::Align::Center,
                     .offset = 3.0f,
                     .autoFlip = false})
            .opacity(withFrom(0.0f, 1.0f, ramp(1000, 500))));
    panel.child(
        text(toU8("PARAMOUNT 1958 · 1.85:1 · TECHNICOLOR"),
             type(faceGothic, 11, hex(0xEDE6D8, 0.42f), 3.4f))
            .key("ring-bottom")
            .inset(0)
            // Same clockwise baseline as the top caption, half a turn
            // round, flipped so it reads right way up — which is the
            // straightforward spelling, and only became the straight-
            // forward spelling once autoFlip stopped mirroring the run.
            .onPath({.path = circlePath(272.0f),
                     .at = 0.75f,
                     .align = TextPath::Align::Center,
                     .offset = 3.0f,
                     .autoFlip = true})
            .opacity(withFrom(0.0f, 1.0f, ramp(1120, 500))));

    // the card slug: four of them stacked in the same corner, each riding
    // its own card's opacity — so the caption cross-dissolves with the
    // curve it describes, on the same 240 ms optical-printer window.
    static constexpr const char *kSlug[] = {
        "CARD A · a:b 3:2 · δ 90° · k 0.15 · R 176 px",
        "CARD B · a:b 5:4 · δ 0° · k 0.10 · R 172 px",
        "CARD C · a:b 2:1 · δ 45° · k 0.22 · R 180 px",
        "CARD D · a:b 5:3 · δ 60° · k 0.12 · R 167 px",
    };
    for (int i = 0; i < 4; ++i)
      panel.child(text(toU8(kSlug[i]), type(faceGothic, 10, kBone, 1.8f))
                      .key(std::string("slug") + kCards[i].tag)
                      .left(22)
                      .top(20)
                      .opacity(&cardA[i]));
    panel.child(text(toU8("T = 12π · N = 2000 · TURNTABLE 18°/s · easeNone"),
                     type(faceGothic, 10, hex(0xEDE6D8, 0.50f), 1.8f))
                    .key("slug-rig")
                    .left(22)
                    .bottom(20)
                    .opacity(withFrom(0.0f, 1.0f, ramp(1200, 400))));

    // film gate: grain, then a soft vignette
    panel.child(box()
                    .inset(0)
                    .fill(filmGrain)
                    .blend(SkBlendMode::kOverlay)
                    .opacity(0.42f));
    panel.child(box().inset(0).fill(radialGradient(
        kEye, kPanelW * 0.52f,
        {hex(0x000000, 0.0f), hex(0x000000, 0.10f), hex(0x050302, 0.80f)},
        {0.0f, 0.44f, 1.0f})));

    // the bezel is its OWN node: trim() on the panel would reveal the
    // iris fill along with the keyline.
    panel.child(box()
                    .key("bezel")
                    .inset(0)
                    .corners({10})
                    .fill(Fill::none())
                    .stroke(stroke(2.0f, Fill::color(kKeyline),
                                   PathFormat::Align::Inner))
                    .trim(0.0f, withFrom(0.0f, 1.0f,
                                         ramp(260, 480, ch::easeOutCubic))));
    return panel;
  }

  // ------------------------------------------------------------------
  Element typeSpecimen() {
    auto p = plate(140).gap(6);
    // something to see THROUGH the counters — the whole point of the
    // outline register. Card C's own curve, spinning with the rest.
    p.child(box()
                .key("spec-bed")
                .left(-46)
                .top(-64)
                .width(352)
                .height(268)
                .outline(lissajous(kCards[2], 74.0f, 900))
                .stroke(stroke(0.8f, Fill::color(hex(0x2E5C9E, 0.55f))))
                .rotate(&spin));
    p.child(text(toU8("VERTIGO"), hollow(faceDisplay, 34, kBone, 1.1f, 4.0f))
                .key("spec-outline"));
    p.child(text(toU8("SAUL BASS · JOHN WHITNEY"),
                 type(faceDisplay, 14, kBone, 2.0f))
                .key("spec-solid"));
    p.child(text(toU8("OUTLINE DISPLAY OVER THE IMAGE / SOLID BODY BELOW IT "
                      "— BOTH CLARENDON."),
                 type(faceGothic, 10, kSteel, 0.6f))
                .key("spec-cap"));
    return p;
  }

  Element spiralIndex() {
    auto p = plate(240).gap(8);
    for (int i = 0; i < 4; ++i) {
      const Card &c = kCards[i];
      auto row = box().row().height(46).gap(12).alignItems(Align::Center).key(
          std::string("idx") + c.tag);
      // the chip draws the card's OWN curve at 15px — same generator,
      // same six constants, 1/12 the amplitude
      row.child(box()
                    .width(38)
                    .height(38)
                    .shrink(0)
                    .corners({3})
                    .fill(Fill::color(hex(0x080605)))
                    .stroke(stroke(1.0f, Fill::color(kKeyline),
                                   PathFormat::Align::Inner))
                    .child(box()
                               .inset(0)
                               .outline(lissajous(c, 13.0f, 420))
                               .stroke(stroke(0.9f, Fill::color(c.core)))
                               .rotate(&spin)));
      row.child(box()
                    .column()
                    .grow(1)
                    .gap(2)
                    .child(text(toU8(c.line1),
                                type(faceGothicBold, 11, kBone, 0.7f)))
                    .child(text(toU8(c.line2), type(faceGothic, 9, kSteel))));
      p.child(std::move(row));
    }
    return p;
  }

  Element rigPlate() {
    static constexpr const char *kFacts[] = {
        "850 LB · 11,000 PARTS — WWII ANTI-AIRCRAFT COMPUTER, "
        "CREWED BY FIVE",
        "REPURPOSED BY JOHN WHITNEY, 1957–58",
        "ROTATING GUN PLATE (CONTACT THROUGH A FULL TURN) + CEILING "
        "PENDULUM, PEN FED FROM A 24-FT PRESSURIZED INK RESERVOIR",
        "CURVES PLOT JULES LISSAJOUS'S PARAMETRIC EQUATIONS",
    };
    auto p = plate(176).gap(5);
    p.child(text(toU8("THE M-5 GUN DIRECTOR"),
                 type(faceGothicBold, 13, kBone, 1.6f))
                .key("rig-h"));
    for (int i = 0; i < 4; ++i)
      p.child(text(toU8(kFacts[i]), type(faceGothic, 10.5f, kSteel, 0.3f))
                  .key("rig" + std::to_string(i))
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(900.0f + (float)i * 90.0f, 300))));
    p.child(box().grow(1));
    p.child(text(toU8("hitchcocksvertigo.substack.com · rhizome.org "
                      "· diyphotography.net"),
                 type(faceGothic, 9, kSteelDim))
                .key("rig-cite"));
    return p;
  }

  // ------------------------------------------------------------------
  Element describe() {
    auto root = box().column().padding(kPad).gap(28).fill(Fill::color(kInk));

    // ---- header ---------------------------------------------------
    auto head = box().row().height(104).alignItems(Align::End);

    GlyphFx rise;
    rise.effect = glyphfx::rise(18.0f);
    rise.stagger = {.eachMs = 26, .amountMs = 0, .durationMs = 420};
    rise.progress = withFrom(0.0f, 1.0f, ramp(140, 900, ch::easeOutExpo));

    head.child(
        box()
            .column()
            .grow(1)
            .gap(7)
            .child(text(toU8("TECHNIQUE STUDY 02 · PRECESSING LISSAJOUS "
                             "FIGURES"),
                        type(faceGothicBold, 11, kSteel, 3.0f))
                       .key("eyebrow")
                       .opacity(withFrom(0.0f, 1.0f, ramp(0, 260)))
                       .translateY(withFrom(8.0f, 0.0f, ramp(0, 260))))
            .child(text(toU8("VERTIGO, 1958"),
                        type(faceDisplay, 42, kBone, 1.0f))
                       .key("heading")
                       .glyphFx(std::move(rise)))
            .child(text(toU8("Saul Bass, title design — John Whitney, "
                             "spirals — Paramount, dir. Alfred Hitchcock"),
                        type(faceGothic, 12, kSteel, 0.4f))
                       .key("cite")
                       .opacity(withFrom(0.0f, 1.0f, ramp(420, 240)))));

    auto sources = box().column().gap(4).alignItems(Align::End);
    static constexpr const char *kSrc[] = {
        "artofthetitle.com/title/vertigo",
        "typotheque.com — Emily King, “Taking Credit” (5)",
        "patrycerichter.wordpress.com — shot breakdown, 2016",
        "fontsinuse.com — Clarendon / News Gothic",
    };
    for (int i = 0; i < 4; ++i)
      sources.child(text(toU8(kSrc[i]), type(faceGothic, 9.5f, kSteelDim))
                        .key("src" + std::to_string(i))
                        .opacity(withFrom(0.0f, 1.0f,
                                          ramp(520.0f + (float)i * 70.0f, 260))));
    head.child(std::move(sources));
    root.child(std::move(head));

    // hairline under the header
    root.child(box()
                   .height(1)
                   .fill(Fill::color(kKeyline))
                   .transformOrigin(0.0f, 0.5f)
                   .scale(withFrom(0.0f, 1.0f, ramp(200, 620, ch::easeOutCubic))));

    // ---- body -----------------------------------------------------
    root.child(box()
                   .row()
                   .gap(32)
                   .height(kPanelH)
                   .child(screenPanel())
                   .child(box()
                              .width(kSideW)
                              .shrink(0)
                              .column()
                              .gap(20)
                              .child(typeSpecimen())
                              .child(spiralIndex())
                              .child(rigPlate())));

    // ---- the whole sheet under one very faint tooth ---------------
    // perf-pass: full-canvas procedural paperGrain overlay, STATIC, but
    // 126 ms/frame of live eval on the CPU raster backend (opacity+overlay
    // refuses an auto-bake). Bake once — GPU was already 3.8ms, provably
    // static so the cache STICKS.
    root.child(box()
                   .inset(0)
                   .fill(paperGrain)
                   .blend(SkBlendMode::kOverlay)
                   .opacity(0.16f)
                   .cache(Cache::Texture));
    return root;
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kInk);

    auto family = [&](const char *name, SkFontStyle s) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, s);
    };
    // Clarendon is REAL here: macOS ships Apple's SuperClarendon.
    faceDisplay = family("SuperClarendon", SkFontStyle::Bold());
    if (!faceDisplay)
      faceDisplay = family("Super Clarendon", SkFontStyle::Bold());
    if (!faceDisplay)
      faceDisplay = family("Rockwell", SkFontStyle::Bold());
    if (!faceDisplay)
      faceDisplay = family("Bodoni 72", SkFontStyle::Bold());
    // News Gothic is NOT installed — Helvetica Neue stands in, condensed.
    faceGothic = family("Helvetica Neue", SkFontStyle::Normal());
    faceGothicBold = family("Helvetica Neue", SkFontStyle::Bold());

    // ---- the iris: TWO gradient kinds flattened into one shader ----
    // radial sepia ramp (pupil → bright inner iris → limbus → dark) with
    // an angular sweep of fibre striations laid over it in soft light.
    std::vector<Stop> fibres;
    for (int i = 0; i <= 96; ++i) {
      const float v = (i % 2 == 0) ? 0.482f : 0.518f;
      const float j = 0.012f * shapes::detail::hashNoise(17u, (uint32_t)i);
      fibres.push_back({(float)i / 96.0f, {v + j, v + j, v + j, 1}});
    }
    irisMat = Material::blend(
        {{Material::radial(kEye, 360.0f,
                           {{0.00f, hex(0x100C09)},  // pupil
                            {0.11f, hex(0x17110B)},
                            {0.17f, hex(0x8A6A44)},  // bright inner iris
                            {0.40f, hex(0x6E5230)},
                            {0.72f, hex(0x6A5030)},
                            {1.00f, hex(0x36271A)}}),
          SkBlendMode::kSrc},
         {Material::sweep(kEye, fibres, 0.0f, 360.0f),
          SkBlendMode::kSoftLight}});

    // LUMINANCE noise — the `contrast` knob is the difference between
    // film grain and concrete.
    filmGrain = patterns::grain(0.62f, 3, 4.0f, 0.34f, 1.0f);
    paperGrain = patterns::grain(0.42f, 2, 11.0f, 0.28f, 1.0f);

    // ---- the perpetual loop: 14 hand-stepped scalars ---------------
    // spin never syncs to the 16 s card cycle (lcm(16,20) = 80 s), the
    // way a motor keeps running across cuts the editor made without it.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      spin = (float)std::fmod(t * 18.0, 360.0);
      const double cycle = std::fmod(t, 16.0);
      for (int i = 0; i < 4; ++i) {
        const double L = std::fmod(cycle - (double)i * 4.0 + 16.0, 16.0);
        float op = 0.0f, g = 0.0f;
        if (L < 4.0) {
          op = L < 0.24 ? (float)(L / 0.24)
               : L < 3.76 ? 1.0f
                          : (float)((4.0 - L) / 0.24);
          // easeNone by construction: a motor draws at a constant rate
          g = std::clamp((float)((L - 0.24) / 1.40), 0.0f, 1.0f);
        }
        cardA[i] = op;
        growth[i] = g;
        // shadow state the sketch keeps in sync BY HAND — there is no
        // "this Output, minus 0.008" PropValue form (see the gaps note).
        penTip[i] = std::max(0.0f, g - 0.008f);
        penA[i] = op * std::clamp((1.0f - g) / 0.06f, 0.0f, 1.0f) *
                  std::clamp(g / 0.02f, 0.0f, 1.0f);
      }
      return true;
    });

    ctx.composer.render(describe());
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(VertigoTitles)
