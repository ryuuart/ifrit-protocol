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
//   * The DAMPING term. A pure Lissajous-plus-rotation has none, but
//     every real harmonograph and every real pendulum damps, and
//     Whitney's pen hung from a ceiling on a physical string — so a small
//     exp(-λt) envelope is applied per card. It is what makes the figure
//     spiral INWARD rather than retrace one closed rosette, i.e. what
//     makes it look like the film. Called out because it is an addition.
//   * Amplitudes carry a 0.88 fit scale. The rotated composition reaches
//     R·√2 from centre rather than R, and at full R that plus
//     filament()'s glow overflows a 596 px-tall panel.
//   * T = 6π, three pendulum periods. The record's spirals are sparse —
//     a fine line making a few dozen readable loops — so the count is set
//     where the precession can still be followed round rather than where
//     the card turns into a ball of thread.
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
//     t   = i/(N-1) · T                 T = 6π (3 pendulum periods)
//     env = exp(-damp·t)                (this study's damping)
//     xl  = R·env·sin(a·t + δ)          pendulum, in the table's frame
//     yl  = R·env·sin(b·t)
//     θ   = k·t                         the table's own rotation
//     p   = centre + rot(θ) · (xl, yl)
//
// That is `shapes::harmonograph(a, b, δ, damping, precession, turns)` to
// the line, so it is asked for rather than spelled: the six constants go
// in and a COMPARABLE value comes out, which is what lets the thirteen
// nodes holding one prune instead of re-patching on every describe. The
// amplitude R is the node's own half-extent, because the curve is sampled
// in a unit frame that the box scales.
//
// Revealed by a stroke pass over `spans::upTo(&growth)` at a CONSTANT
// rate (a motor does not ease); spun forever by one shaped binding on the
// clock.
//
// ---------------------------------------------------------------------
// TWO WINDOWS OVER ONE CURVE VALUE
// ---------------------------------------------------------------------
// A SPAN IS PER STROKE PASS, so the pen tip riding just behind the
// drawing edge is a second pass over `spans::range(growth - nib, growth)`
// and needs no node of its own. It is still drawn as a sibling here, for
// the ordering the plus-blended nib wants over the filament under it —
// and because the curve IS a comparable value, the two nodes hold the
// same Harmonograph, compare equal, and the second costs a window rather
// than a second figure.
//
// EDIT THESE FIRST
//   kTurns   — how many pendulum periods each card draws. Three is a
//              readable rosette; past four the loops stop resolving.
//   kCards   — the four a:b:δ:k:R:damping presets and their inks.
//   kFit     — how much of its 2R box the figure is allowed; the rotated
//              composition reaches R·√2, so this keeps the glow inside.
//   the 16 s card cycle in the ticker — four cards, four seconds each.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/kit/PaintLayers.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
namespace noise = sigil::core::noise;
using namespace std::chrono_literals;
using sigil::material::skia::Paint;
using sigil::material::skia::Stop;
namespace ch = choreograph;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

// ---------------------------------------------------------------------------
// chrome palette — this study's own (film-base warm black, deliberately
// warmer than a neutral UI near-black)

constexpr SkColor4f kInk = hex(0x0A0806);    // canvas
constexpr SkColor4f kPlate = hex(0x110D0A);  // sidebar plates
constexpr SkColor4f kBone = hex(0xEDE6D8);   // primary type
constexpr SkColor4f kSteel = hex(0x8A7D68);  // secondary type
constexpr SkColor4f kSteelDim = hex(0x8A7D68, 0.62f);
constexpr SkColor4f kKeyline = hex(0x3A342C);   // panel keylines
constexpr SkColor4f kSolidInk = hex(0x050403);  // "solid black capitals"

// ---------------------------------------------------------------------------
// canvas / panel geometry — 1480x800 is the film's own 1.85:1

constexpr float kW = 1480.0f;
constexpr float kH = 800.0f;
constexpr float kPad = 36.0f;
constexpr float kPanelW = 900.0f;
constexpr float kPanelH = 596.0f;
constexpr SkPoint kEye{kPanelW * 0.5f, kPanelH * 0.5f};  // 450, 298
constexpr float kSideW = 476.0f;

// ---------------------------------------------------------------------------
// the four spiral cards. a/b/delta/k/R are this study's invention (see
// the header); damp is too. `T = 6π` for all four.

struct Card {
  const char* tag;
  float a, b;         // Lissajous frequency ratio
  float deltaDeg;     // pendulum phase offset
  float k;            // precession (table turns per radian of swing)
  float amp;          // R, px, before the 0.88 fit scale
  float damp;         // exp(-damp·t) envelope — this study's addition
  SkColor4f core;     // ink colour
  const char* line1;  // sidebar index caption
  const char* line2;
};

constexpr float kFit = 0.88f;  // R·√2 + glow must clear kPanelH/2
// THREE pendulum periods. The record's spirals are sparse — a single fine
// line making a few dozen readable loops, so the eye can follow the
// precession round. Past four the loops stop resolving and the card reads
// as a ball of thread, which is a scribble rather than a mechanism.
constexpr float kTurns = 3.0f;
constexpr int kSamples = 1100;
/** How far behind the drawing edge the bright nib window trails, as a
 *  fraction of the contour's arc length. */
constexpr float kNib = 0.008f;

constexpr std::array<Card, 4> kCards = {{
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
     "D — PURPLE · a:b = 5:3 · δ 60°", "k 0.12 · R 167 px · 5-lobe flower"},
}};

/** The pendulum-over-turntable curve. `shapes::harmonograph` IS the rig:
 *  a Lissajous whose amplitudes decay under exp(-damping·t), rotated by
 *  precession·t as it draws — the pendulum over the turning plate, in one
 *  comparable value. The card's six constants go in unchanged.
 *
 *  AMPLITUDE IS THE NODE'S OWN BOX. The curve is sampled in a unit frame
 *  where ±1 spans the box, so a card's R is spelled by giving its node a
 *  2R square rather than by scaling inside the generator — which is what
 *  makes the shape a value the node can compare and prune on rather than
 *  a callable it must re-patch on every describe. */
shapes::Harmonograph figure(const Card& c, int samples = kSamples) {
  return shapes::harmonograph(c.a, c.b, c.deltaDeg, c.damp, c.k, kTurns,
                              samples);
}

/** A node sized and placed so its box gives @p radius as the curve's
 *  amplitude about @p centre. */
Element figureBox(SkPoint centre, float radius) {
  return box().width(radius * 2.0f).height(radius * 2.0f).centerAt(centre);
}

// ---------------------------------------------------------------------------
// type

weave::TextStyle faced(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0,
                             float condense = 1.0f) {
  return weave::textStyle({.face = std::move(face),
                               .size = size,
                               .color = color,
                               .track = tracking,
                               .condense = condense});
}

/** The OUTLINE register: sigil::weave::kit::outline()'s stroked paint installed
 *  as the node's ENTIRE foreground pass — no fill underneath, so the
 *  spiral is visible straight through the counters. Typotheque: "outline
 *  type through which the image beneath can be seen." */
weave::TextStyle hollow(sk_sp<SkTypeface> face, float size,
                               SkColor4f color, float width,
                               float tracking = 0) {
  weave::TextStyle s = faced(std::move(face), size, color, tracking);
  s.paint.foreground =
      sigil::weave::kit::outline(color.toSkColor(), width).paint;
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** THE RING BASELINE for the onPath() legends: a clockwise circle
 *  starting at 9 o'clock, so fraction 0.25 is 12 o'clock and 0.75 is 6
 *  o'clock. The radius is the node's own half-extent, as everywhere else
 *  a curve is sampled here.
 *
 *  Keyed, not bare. A raw callable never compares equal to a separately
 *  constructed one, so the node holding it re-patches on every describe
 *  and can never prune; the key is the promise that this name always
 *  means this curve. */
shapes::KeyedParametric ringPath() {
  return shapes::parametric(
      "vertigo-limbus",
      [](float a) { return SkPoint{std::cos(a), std::sin(a)}; }, kPi,
      kPi + 2.0f * kPi, 361);
}

/** A concentric ring on the panel — the pupil edge and the limbus, which
 *  are what make a radial ramp read as an EYE rather than a vignette. */
Element ring(float r, SkColor4f color, float width) {
  return kit::disc(kEye, r)
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

}  // namespace

// ===========================================================================

struct VertigoTitles : sketch::Sketch {
  // --- the perpetual loop's live cells ---------------------------------
  // One clock and three cells per card. What used to be a fourth cell —
  // the nib's trailing edge — is a shaped binding on `growth`, and what
  // used to be a fifth is the turntable, which is a shaped binding on the
  // clock. A value derived from another one is not its own state.
  ch::Output<float> secs{0};
  std::array<ch::Output<float>, 4> growth{};  // trim end   — the pen
  std::array<ch::Output<float>, 4> cardA{};   // card opacity
  std::array<ch::Output<float>, 4> penA{};    // nib opacity (fades at arrival)

  /** THE TURNTABLE: 18°/s, folded into [0,360) — which is exactly what
   *  `fmod(seconds · 18, 360)` computes, said as a lane instead of as a
   *  scalar the ticker has to write. It never syncs to the 16 s card
   *  cycle (lcm(16,20) = 80 s), the way a motor keeps running across cuts
   *  the editor made without it. */
  Bound turntable() const { return bind(&secs).scale(18.0f).wrap(360.0f); }

  sk_sp<SkTypeface> faceDisplay, faceGothic, faceGothicBold;
  Paint irisMat, filmGrain, paperGrain;

  // ------------------------------------------------------------------
  // one spiral card = TWO nodes over the same curve value. A span rides
  // the stroke PASS, so both windows could sit on one node; they are
  // siblings for the draw order the plus-blended nib wants over the
  // filament, and the second node costs a window rather than a second
  // figure because the Harmonograph compares equal.
  void spiralCard(Element& into, int i) {
    const Card& c = kCards[i];
    const float R = c.amp * kFit;
    const std::string tag = c.tag;

    into.child(
        figureBox(kEye, R)
            .key("curve" + tag)
            .shape(figure(c))
            .stroke(spans::upTo(&growth[i]),
                    brush::presets::filament(c.core, hex(0xFFE9CF), 0.48f))
            .rotate(turntable())
            .opacity(&cardA[i]));

    // the nib: a short bright plus-blended window at the trailing edge
    // The nib's trailing edge is `growth` MINUS a constant — a derived
    // value, which is a shaped binding on the same Output rather than a
    // second Output kept in step by hand.
    into.child(
        figureBox(kEye, R)
            .key("nib" + tag)
            .shape(figure(c))
            .stroke(spans::range(bind(&growth[i]).offset(-kNib).clamp(0, 1),
                                 &growth[i]),
                    brush::presets::pulse({1.0f, 0.90f, 0.72f, 0.42f},
                                               {1, 1, 1, 0.95f}, 0.7f))
            .rotate(turntable())
            .opacity(&penA[i]));
  }

  // ------------------------------------------------------------------
  Element screenPanel() {
    // The whole screen panel is a saveLayer: the rounded clip and the
    // kColor "stain" descendant — which blends against what is already on
    // the canvas — each force one, so the full panel is re-composited every
    // frame. Asking for `.cache(Cache::Texture)` here does nothing but add
    // overhead: a subtree that blends with the canvas cannot be baked in
    // isolation, so the bake is refused and the composite happens anyway.
    // It is deliberately left uncached. This is the per-frame floor for the
    // sketch, and it is why the CPU raster backend struggles with it while
    // the GPU one does not.
    auto panel = box()
                     .width(kPanelW)
                     .height(kPanelH)
                     .shrink(0)
                     .corners({10})
                     .clip(true)
                     .key("screen")
                     .fill(irisMat);

    panel.child(ring(61.0f, hex(0x090604, 0.85f), 3.0f)
                    .key("pupil-edge")
                    .opacity(animate(from(0.0f).to(1.0f), ramp(300, 420))));
    panel.child(ring(146.0f, hex(0x2A1D10, 0.40f), 1.2f).key("iris-mid"));
    panel.child(ring(262.0f, hex(0x120C07, 0.24f), 10.0f).key("limbus"));

    // "the screen is suddenly stained red" — kColor keeps the iris's
    // luminance and swaps its hue/saturation, so it TINTS rather than
    // covers. Sudden onset: easeInQuad.
    panel.child(
        box()
            .key("stain")
            .inset(0)
            .blend(SkBlendMode::kColor)
            .fill(animate(
                from(Fill::color(hex(0x3A2A1C))).to(Fill::color(hex(0xC81E2C))),
                ramp(700, 500, ch::easeInQuad))));

    for (int i = 0; i < 4; ++i) spiralCard(panel, i);

    // VERTIGO — hollow Clarendon expanding out of the pupil: one text
    // node, one fx::pop() track cascading the capitals 30 ms apart. The
    // track's batched draw carries the style's whole paint — the blurred
    // stroke underlay stays beneath the hollow stroke while the letters
    // pop. The 3 px between capitals is tracking, not a gap: with one text
    // node the spacing IS letterspacing.
    {
      auto face = hollow(faceDisplay, 76, kBone, 2.2f, 3.0f);
      // Legibility underlay over the busiest cards. NOT dropShadow() —
      // that is a FILLED blurred copy and would plug the counters, which
      // is the one thing the outline register exists to keep open. A
      // blurred STROKE hugs the letterform and leaves the spiral visible
      // straight through it.
      {
        SkPaint halo;
        halo.setAntiAlias(true);
        halo.setStyle(SkPaint::kStroke_Style);
        // Kept THIN and weak on purpose. A heavy underlay reads as a
        // fill, and a filled display cap is the one thing this register
        // exists not to be: the record's whole point is outline type the
        // image is seen through.
        halo.setStrokeWidth(4.0f);
        halo.setColor(0x59000000);
        face.paint.underlays.push_back(
            weave::PaintLayer::blurred(halo, 2.4f));
      }
      // The entrance ramp covers the cascade's own span, so the last
      // capital lands exactly when the master progress does.
      const Spread cascade{.eachMs = 30, .durationMs = 480};
      panel.child(text(toU8("VERTIGO"), face)
                      .key("vertigo")
                      .centerAt(kEye)
                      .fx({.effect = fx::pop(0.30f),
                           .stagger = cascade,
                           .progress = animate(from(0.0f).to(1.0f),
                                               ramp(780, cascade.spanMs(7)))}));
    }

    // the other register — "solid black capitals of the SAME typeface"
    // (Typotheque), so Clarendon again, not the gothic. Deliberately
    // unadorned next to VERTIGO's hollow display caps, and deliberately
    // laid over the busiest part of the card: that is where the film puts
    // its body credits too.
    panel.child(
        text(toU8("TITLE DESIGN SAUL BASS · SPIRALS JOHN WHITNEY"),
             faced(faceDisplay, 15, kSolidInk, 2.6f))
            .key("credit")
            .centerAt({kEye.x(), kEye.y() + 152.0f})
            .opacity(animate(from(0.0f).to(1.0f), ramp(1550, 300)))
            .translateY(animate(from(10.0f).to(0.0f), ramp(1550, 300))));

    // the instrument-dial legend, set on the limbus itself with
    // Element::onPath() — one text leaf where hand-placing curved
    // lettering would have been one leaf and one measure() per glyph.
    panel.child(
        text(toU8("JOHN WHITNEY · M-5 GUN DIRECTOR · PENDULUM OVER PLATE"),
             faced(faceGothic, 11, hex(0xEDE6D8, 0.42f), 3.4f))
            .key("ring-top")
            .width(544)
            .height(544)
            .centerAt(kEye)
            .onPath({.path = ringPath(),
                     .at = 0.25f,
                     .align = TextPath::Align::Center,
                     .offset = 3.0f,
                     .autoFlip = false})
            .opacity(animate(from(0.0f).to(1.0f), ramp(1000, 500))));
    panel.child(
        text(toU8("PARAMOUNT 1958 · 1.85:1 · TECHNICOLOR"),
             faced(faceGothic, 11, hex(0xEDE6D8, 0.42f), 3.4f))
            .key("ring-bottom")
            .width(544)
            .height(544)
            .centerAt(kEye)
            // Same clockwise baseline as the top caption, half a turn
            // round. autoFlip turns the whole run over so it reads right
            // way up on the underside of the ring; glyph order and glyph
            // orientation both follow, so the text is not mirrored.
            .onPath({.path = ringPath(),
                     .at = 0.75f,
                     .align = TextPath::Align::Center,
                     .offset = 3.0f,
                     .autoFlip = true})
            .opacity(animate(from(0.0f).to(1.0f), ramp(1120, 500))));

    // the card slug: four of them stacked in the same corner, each riding
    // its own card's opacity — so the caption cross-dissolves with the
    // curve it describes, on the same 240 ms optical-printer window.
    static constexpr const char* kSlug[] = {
        "CARD A · a:b 3:2 · δ 90° · k 0.15 · R 176 px",
        "CARD B · a:b 5:4 · δ 0° · k 0.10 · R 172 px",
        "CARD C · a:b 2:1 · δ 45° · k 0.22 · R 180 px",
        "CARD D · a:b 5:3 · δ 60° · k 0.12 · R 167 px",
    };
    for (int i = 0; i < 4; ++i)
      panel.child(text(toU8(kSlug[i]), faced(faceGothic, 10, kBone, 1.8f))
                      .key(std::string("slug") + kCards[i].tag)
                      .left(22)
                      .top(20)
                      .opacity(&cardA[i]));
    panel.child(text(toU8("T = 6π · N = 1100 · TURNTABLE 18°/s · easeNone"),
                     faced(faceGothic, 10, hex(0xEDE6D8, 0.50f), 1.8f))
                    .key("slug-rig")
                    .left(22)
                    .bottom(20)
                    .opacity(animate(from(0.0f).to(1.0f), ramp(1200, 400))));

    // Film gate: grain, and NO vignette. The one colour source located
    // for this passage describes a flat saturated field — cool tones and
    // warm tones, not a centre that falls off to black. A ramp to the
    // corners is a modern device and it was reading as the subject.
    panel.child(box()
                    .inset(0)
                    .fill(filmGrain)
                    .blend(SkBlendMode::kOverlay)
                    .opacity(0.42f));

    // the bezel is its OWN node: trim() on the panel would reveal the
    // iris fill along with the keyline.
    panel.child(
        box()
            .key("bezel")
            .inset(0)
            .corners({10})
            .fill(Fill::none())
            .stroke(
                spans::upTo(animate(from(0.0f).to(1.0f),
                                    ramp(260, 480, ch::easeOutCubic))),
                stroke(2.0f, Fill::color(kKeyline), PathFormat::Align::Inner)));
    return panel;
  }

  // ------------------------------------------------------------------
  Element typeSpecimen() {
    auto p = plate(140).gap(6);
    // something to see THROUGH the counters — the whole point of the
    // outline register. Card C's own curve, spinning with the rest.
    p.child(figureBox({130.0f, 70.0f}, 74.0f)
                .key("spec-bed")
                .shape(figure(kCards[2], 700))
                .stroke(stroke(0.8f, Fill::color(hex(0x2E5C9E, 0.55f))))
                .rotate(turntable()));
    p.child(text(toU8("VERTIGO"), hollow(faceDisplay, 34, kBone, 1.1f, 4.0f))
                .key("spec-outline"));
    p.child(text(toU8("SAUL BASS · JOHN WHITNEY"),
                 faced(faceDisplay, 14, kBone, 2.0f))
                .key("spec-solid"));
    p.child(text(toU8("OUTLINE DISPLAY OVER THE IMAGE / SOLID BODY BELOW IT "
                      "— BOTH CLARENDON."),
                 faced(faceGothic, 10, kSteel, 0.6f))
                .key("spec-cap"));
    return p;
  }

  Element spiralIndex() {
    auto p = plate(240).gap(8);
    for (int i = 0; i < 4; ++i) {
      const Card& c = kCards[i];
      auto row = box()
                     .row()
                     .height(46)
                     .gap(12)
                     .alignItems(Align::Center)
                     .key(std::string("idx") + c.tag);
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
                    .child(figureBox({19.0f, 19.0f}, 13.0f)
                               .shape(figure(c, 360))
                               .stroke(stroke(0.9f, Fill::color(c.core)))
                               .rotate(turntable())));
      row.child(
          box()
              .column()
              .grow(1)
              .gap(2)
              .child(text(toU8(c.line1), faced(faceGothicBold, 11, kBone, 0.7f)))
              .child(text(toU8(c.line2), faced(faceGothic, 9, kSteel))));
      p.child(std::move(row));
    }
    return p;
  }

  Element rigPlate() {
    static constexpr const char* kFacts[] = {
        "850 LB · 11,000 PARTS — WWII ANTI-AIRCRAFT COMPUTER, "
        "CREWED BY FIVE",
        "REPURPOSED BY JOHN WHITNEY, 1957–58",
        "ROTATING GUN PLATE (CONTACT THROUGH A FULL TURN) + CEILING "
        "PENDULUM, PEN FED FROM A 24-FT PRESSURIZED INK RESERVOIR",
        "CURVES PLOT JULES LISSAJOUS'S PARAMETRIC EQUATIONS",
    };
    auto p = plate(176).gap(5);
    p.child(text(toU8("THE M-5 GUN DIRECTOR"),
                 faced(faceGothicBold, 13, kBone, 1.6f))
                .key("rig-h"));
    for (int i = 0; i < 4; ++i)
      p.child(text(toU8(kFacts[i]), faced(faceGothic, 10.5f, kSteel, 0.3f))
                  .key("rig" + std::to_string(i))
                  .opacity(animate(from(0.0f).to(1.0f),
                                   ramp(900.0f + (float)i * 90.0f, 300))));
    p.child(box().grow(1));
    p.child(text(toU8("hitchcocksvertigo.substack.com · rhizome.org "
                      "· diyphotography.net"),
                 faced(faceGothic, 9, kSteelDim))
                .key("rig-cite"));
    return p;
  }

  // ------------------------------------------------------------------
  Element describe() {
    auto root = box().column().padding(kPad).gap(28).fill(Fill::color(kInk));

    // ---- header ---------------------------------------------------
    auto head = box().row().height(104).alignItems(Align::End);

    Track rise{.effect = fx::rise(18.0f),
               .stagger = {.eachMs = 26, .amountMs = 0, .durationMs = 420},
               .progress = animate(from(0.0f).to(1.0f),
                                   ramp(140, 900, ch::easeOutExpo))};

    head.child(
        box()
            .column()
            .grow(1)
            .gap(7)
            .child(text(toU8("PRECESSING LISSAJOUS FIGURES"),
                        faced(faceGothicBold, 11, kSteel, 3.0f))
                       .key("eyebrow")
                       .opacity(animate(from(0.0f).to(1.0f), ramp(0, 260)))
                       .translateY(animate(from(8.0f).to(0.0f), ramp(0, 260))))
            .child(
                text(toU8("VERTIGO, 1958"), faced(faceDisplay, 42, kBone, 1.0f))
                    .key("heading")
                    .fx(std::move(rise)))
            .child(text(toU8("Saul Bass, title design — John Whitney, "
                             "spirals — Paramount, dir. Alfred Hitchcock"),
                        faced(faceGothic, 12, kSteel, 0.4f))
                       .key("cite")
                       .opacity(animate(from(0.0f).to(1.0f), ramp(420, 240)))));

    auto sources = box().column().gap(4).alignItems(Align::End);
    static constexpr const char* kSrc[] = {
        "artofthetitle.com/title/vertigo",
        "typotheque.com — Emily King, “Taking Credit” (5)",
        "patrycerichter.wordpress.com — shot breakdown, 2016",
        "fontsinuse.com — Clarendon / News Gothic",
    };
    for (int i = 0; i < 4; ++i)
      sources.child(
          text(toU8(kSrc[i]), faced(faceGothic, 9.5f, kSteelDim))
              .key("src" + std::to_string(i))
              .opacity(animate(from(0.0f).to(1.0f),
                               ramp(520.0f + (float)i * 70.0f, 260))));
    head.child(std::move(sources));
    root.child(std::move(head));

    // hairline under the header
    root.child(box()
                   .height(1)
                   .fill(Fill::color(kKeyline))
                   .transformOrigin(0.0f, 0.5f)
                   .scale(animate(from(0.0f).to(1.0f),
                                  ramp(200, 620, ch::easeOutCubic))));

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
    // A full-canvas procedural grain shader. It never changes, but the
    // opacity + overlay blend on this node keeps it from being auto-baked,
    // so without the explicit Cache::Texture the shader is re-evaluated
    // over every pixel of the canvas on every frame. Nothing here is
    // animated, so the baked texture stays valid for the whole run.
    root.child(box()
                   .inset(0)
                   .fill(paperGrain)
                   .blend(SkBlendMode::kOverlay)
                   .opacity(0.16f)
                   .cache(Cache::Texture));
    return root;
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 5.2, .background = kInk});

    // Clarendon is REAL here: macOS ships Apple's SuperClarendon. The
    // rest of the list is what this plate will accept instead of it, in
    // order — which is the whole reason the face verb takes a chain.
    faceDisplay = weave::ports::face(
        {"SuperClarendon", "Super Clarendon", "Rockwell", "Bodoni 72"},
        SkFontStyle::Bold());
    // News Gothic is NOT installed — Helvetica Neue stands in, condensed.
    faceGothic = weave::ports::face({"Helvetica Neue", "Helvetica"});
    faceGothicBold = weave::ports::face({"Helvetica Neue", "Helvetica"},
                                        SkFontStyle::kBold_Weight);

    // ---- the iris: TWO gradient kinds flattened into one shader ----
    // radial sepia ramp (pupil → bright inner iris → limbus → dark) with
    // an angular sweep of fibre striations laid over it in soft light.
    std::vector<Stop> fibres;
    for (int i = 0; i <= 96; ++i) {
      const float v = (i % 2 == 0) ? 0.482f : 0.518f;
      const float j = 0.012f * noise::hash(17u, (uint32_t)i);
      fibres.push_back({(float)i / 96.0f, {v + j, v + j, v + j, 1}});
    }
    irisMat = Paint::blend(
        {{Paint::radial(kEye, 360.0f,
                           {{0.00f, hex(0x100C09)},  // pupil
                            {0.11f, hex(0x17110B)},
                            {0.17f, hex(0x8A6A44)},  // bright inner iris
                            {0.40f, hex(0x6E5230)},
                            {0.72f, hex(0x6A5030)},
                            {1.00f, hex(0x36271A)}}),
          SkBlendMode::kSrc},
         {Paint::sweep(kEye, fibres, 0.0f, 360.0f),
          SkBlendMode::kSoftLight}});

    // LUMINANCE noise — the `contrast` knob is the difference between
    // film grain and concrete.
    filmGrain = Paint::recipe(field::grain(0.62f, 3, 4.0f, 0.34f, 1.0f));
    paperGrain = Paint::recipe(field::grain(0.42f, 2, 11.0f, 0.28f, 1.0f));

    // ---- the perpetual loop --------------------------------------
    // One clock, and the card cycle's own three cells. Everything the
    // turntable and the nib need is derived from these where it is used.
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      secs = (float)t;
      const double cycle = std::fmod(t, 16.0);
      for (int i = 0; i < 4; ++i) {
        const double L = std::fmod(cycle - (double)i * 4.0 + 16.0, 16.0);
        float op = 0.0f, g = 0.0f;
        if (L < 4.0) {
          op = L < 0.24   ? (float)(L / 0.24)
               : L < 3.76 ? 1.0f
                          : (float)((4.0 - L) / 0.24);
          // easeNone by construction: a motor draws at a constant rate
          g = std::clamp((float)((L - 0.24) / 1.40), 0.0f, 1.0f);
        }
        cardA[i] = op;
        growth[i] = g;
        penA[i] = op * std::clamp((1.0f - g) / 0.06f, 0.0f, 1.0f) *
                  std::clamp(g / 0.02f, 0.0f, 1.0f);
      }
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    VertigoTitles, "Study \xc2\xb7 Motion",
    "Bass and Whitney's Vertigo titles (1958) \xe2\x80\x94 a Lissajous off an "
    "M-5 gun director")
