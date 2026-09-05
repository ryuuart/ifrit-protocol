// shipping_forecast.cpp — THE SHIPPING FORECAST, set as a sheet that
// performs itself: the 0048 bulletin of BBC Radio 4, whose sea areas are
// read in one fixed clockwise order round the British Isles and whose
// every adjective is a defined quantity.
// =============================================================================
// SUBJECT  A broadcast that is really a TYPOGRAPHIC form. The forecast is
//          not prose that happens to be regular; it is a controlled
//          vocabulary in a fixed order, and every word in it means a
//          number. That is why it sets so well: the areas are a ring, the
//          terms are a glossary, the pressure is a readout, and the one
//          thing that changes between bulletins is which area is being
//          read. So the sheet has exactly one dominant move — the area
//          name arriving in the middle of its own ring — and everything
//          else supports it.
//
// -----------------------------------------------------------------------------
// FROM THE RECORD
//
//   * Broadcast by BBC Radio 4 on behalf of the Maritime and Coastguard
//     Agency, from a Met Office bulletin. The 0048 edition goes out on
//     LONG WAVE, 198 kHz — which is why the spine of this sheet says so —
//     and is preceded by Ronald Binge's "Sailing By" (1963).
//   * THIRTY-ONE SEA AREAS, read in ONE fixed order that runs broadly
//     clockwise from Viking, off the north-east of Scotland, round the
//     islands. The ring here carries the first sixteen of that order, each
//     at its own compass bearing from the middle of the islands,
//     unaltered: Viking, North Utsire, South Utsire, Forties, Cromarty,
//     Forth, Tyne, Dogger, Fisher, German Bight, Humber, Thames, Dover,
//     Wight, Portland, Plymouth.
//   * THE TIMING TERMS ARE DEFINITIONS, not adverbs, timed from the
//     bulletin's issue: IMMINENT is within six hours, SOON is six to
//     twelve, LATER is more than twelve. They are set here in a serif
//     italic for that reason — they are glossary entries embedded in the
//     sentence, not emphasis.
//   * THE VISIBILITY TERMS ARE DEFINITIONS TOO: GOOD is more than five
//     nautical miles, MODERATE two to five, POOR a thousand metres to two
//     miles, FOG less than a thousand metres.
//   * THE PRESSURE TENDENCY TERMS ARE RATES, in millibars per three
//     hours: STEADY under 0.1, SLOWLY 0.1 to 1.5, plain rising or falling
//     1.6 to 3.5, QUICKLY 3.6 to 6.0, VERY RAPIDLY over 6.0.
//   * Wind is given as a direction and a BEAUFORT FORCE, which is why the
//     numerals are the one thing in the paragraph picked out by pattern
//     rather than by role.
//
// THIS STUDY'S OWN, flagged rather than smuggled:
//   * The forecast text itself. "Southwesterly 5 to 7, occasionally gale 8
//     later…" is a PLAUSIBLE German Bight forecast in the real vocabulary
//     and the real sentence order; it is not a bulletin that was read.
//     The coastal-station rows and the 1003 falling slowly are the same
//     kind of invention.
//   * Every colour, every millisecond, the ring radii, the sheet size, and
//     the decision to set the areas on a ring at all. The broadcast has no
//     visual form; this is one.
//
// -----------------------------------------------------------------------------
// THE COMPOSITION, as choreography
//
// ONE CLOCK. A ticker lambda steps two scalars and nothing else:
//   cycle   — seconds within one 15 s bulletin, wrapping
//   secs    — seconds since start, never wrapping (the ring's marquee)
// Every beat in the piece is then `motion::bind(&cycle).window(lo, hi)`, which
// is a beat on a timeline and not a second timeline: the window clamps outside
// its range, so a track that has not started reads 0 and one that has
// finished reads 1, and the whole sheet re-performs on the wrap.
//
// The two shapes a window cannot make are stages on the same chain: the GRAD
// swell is `.cosine()` over its own period, and the sheet's own fade is a
// `.trapezoid()` that holds and then leaves before the wrap. Neither is a
// scalar this file steps.
//
// THE DOMINANT MOVE is the area name. Two hero lines rise through their own
// clipped line boxes on an amount-budgeted per-letter cascade, and then keep
// breathing on the GRAD axis for as long as they are on screen. Nothing else in
// the sheet moves that far or that slowly, which is what makes it the thing the
// eye follows.
//
// WHAT EACH SUPPORTING PART IS FOR, and what it proves:
//   ring     one text leaf, its baseline a circle, its position along that
//            circle a wrapping phase — so sixteen sea areas orbit for the
//            cost of a repaint. Its entrance is the NESTED cascade — one
//            beat per area, letters beating inside each — and it rides the
//            curve's own local perpendicular, because the baseline places
//            the glyph and the track deviates from THAT placement.
//   gale     a two-phase sequence with a crossfade: the strip slides in,
//            and lerps out of the slide into an elastic settle rather than
//            cutting between them.
//   forecast one paragraph, three faces, and three tracks numbered over the
//            PARAGRAPH — the first letter of every word, the rest of every
//            word, and a grade over the initials the face can carry it on.
//            Word ten is beat ten in all three, so a track may address as
//            little as it likes without moving anyone else's beat. The
//            numerals are found by pattern afterwards and painted, never
//            re-shaped.
//   synopsis the coarsest cascade the engine has: one beat per LINE of the
//            current layout, which re-breaks with the column — and the
//            millibar readings picked out in colour and in GRADE by span,
//            neither of which moves a letter the breaking placed.
//   pressure a monospaced readout that decodes into place. The substitution
//            keeps the original pen positions, so a proportional face would
//            refuse it and this one does not; and the decode is HELD, so a
//            character waiting its turn is absent rather than churning.
//   spine    the same engine running DOWN the page: a column whose Latin
//            lies on its side, entering cluster by cluster off its own
//            rotated baseline.
//
// -----------------------------------------------------------------------------
// WHERE THIS STILL COSTS SOMETHING
//
//  1. A grade is an axis one face carries and another does not, and the
//     forecast paragraph is set in three faces. The glossary is kept out of
//     the drive by `sel::style("term")` — the runs addressed by the name
//     they were written in — so the copy can gain a fourth defined term
//     without a fourth string in this file. The one thing still spelled
//     twice is the name itself: once in `forecastStyles` and once in the
//     selector, which is what naming anything costs.
//  2. A number picked out by pattern is picked out in weight as well as
//     in colour: a `spanStyle` that changes only an advance-invariant
//     axis holds it on the glyphs without re-shaping them, which is what the
//     synopsis's millibar readings take. The forecast paragraph does NOT, and
//     the reason is composition rather than a missing verb — every initial
//     there is already inside a grade sweep, and a static span axis over the
//     numerals would replace a moving coordinate with a still one.
//
// EDIT THESE FIRST
//   kAreaRing  — the sixteen sea areas and the compass bearing each is
//                set at. Move a bearing and the name moves round the ring.
//   kLoop      — one bulletin, in seconds. Everything else is a window,
//                a swell or an envelope shaped out of it.
//   kBreathPeriod — the grade swell's period. The declared moment is its
//                second peak, so changing it moves the still.
//   kRingR / kInnerR — the ring's two radii, and therefore how much arc a
//                name has before it meets its neighbour.
//   kAmber     — the one accent. Everything picked out on this sheet is
//                picked out in it.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/shipping_forecast.cpp \
//       --frame /tmp/shipping_forecast.png
//
//   The whole bulletin:  --at 0.2 --frames 30 --fps 4

#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace motion = sigil::motion;
namespace mskia = sigil::material::skia;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// The sheet

constexpr float kW = 1440.0f;
constexpr float kH = 880.0f;

constexpr SkColor4f kSea = hex(0x06090E);      // ground
constexpr SkColor4f kSeaLift = hex(0x0B111A);  // panel wash
constexpr SkColor4f kBone = hex(0xE9E5DB);     // primary type
constexpr SkColor4f kSlate = hex(0x76828F);    // secondary type
constexpr SkColor4f kSlateDim = hex(0x76828F, 0.62f);
constexpr SkColor4f kKeyline = hex(0x1A2532);
constexpr SkColor4f kAmber = hex(0xF0A03C);  // the one accent

constexpr float kRingBox = 660.0f;  // the square the ring panel occupies
constexpr float kRingR = 292.0f;    // sea-area baseline radius
constexpr float kInnerR = 238.0f;   // hairline inside the lettering
constexpr SkPoint kEye{kRingBox * 0.5f, kRingBox * 0.5f};

constexpr float kHero = 92.0f;
constexpr float kColW = 556.0f;  // the left column

// ---- the bulletin's clock, in seconds ------------------------------------
// One pass is a bulletin. The last two seconds are dark on purpose: a loop
// that cuts while nothing is lit has no seam to see.
constexpr double kLoop = 15.0;
constexpr double kBreathPeriod = 7.2;  // peak at 3.6 s — the capture moment
constexpr double kRingPeriod = 46.0;   // one lap of the sea areas
// The sheet's own envelope, as fractions of the loop: up, held, and out
// while there is still time to be dark before the cut.
constexpr float kInFrom = 0.04f / (float)kLoop, kInTo = 0.42f / (float)kLoop;
constexpr float kOutFrom = 12.6f / (float)kLoop, kOutTo = 14.2f / (float)kLoop;

/** THE SEA AREAS AT THEIR OWN BEARINGS. The first sixteen of the order lie
 *  off the east and south of the British Isles, and the bulletin reads them
 *  broadly clockwise from Viking, north-east of Shetland, round to Plymouth
 *  in the south-west. Each carries its compass bearing from the middle of
 *  the islands, so the ring is a chart rather than an ornament: where a
 *  name sits IS where the sea is, and the reading order is that sweep made
 *  visible.
 *
 *  Bearings are degrees clockwise from north, rounded to the ring's own
 *  legibility rather than to a chart's precision — two adjacent areas
 *  whose true bearings differ by three degrees would set as one word. */
struct Area {
  const char* name;
  float bearingDeg;
};
constexpr Area kAreaRing[] = {
    {"VIKING", 14.0f},        {"NORTH UTSIRE", 33.0f}, {"SOUTH UTSIRE", 50.0f},
    {"FORTIES", 65.0f},       {"CROMARTY", 78.0f},     {"FORTH", 90.0f},
    {"TYNE", 101.0f},         {"DOGGER", 112.0f},      {"FISHER", 124.0f},
    {"GERMAN BIGHT", 137.0f}, {"HUMBER", 152.0f},      {"THAMES", 166.0f},
    {"DOVER", 178.0f},        {"WIGHT", 192.0f},       {"PORTLAND", 206.0f},
    {"PLYMOUTH", 222.0f}};
constexpr int kAreaCount = (int)(sizeof(kAreaRing) / sizeof(kAreaRing[0]));

}  // namespace

// ===========================================================================

struct ShippingForecast : sketch::Sketch {
  // The two hand-stepped scalars. Everything else is a SHAPE of one of
  // them — a window, a swell, an envelope — which is why there are two
  // rather than a dozen.
  ch::Output<float> cycle{0};  // 0 → kLoop, wrapping: the bulletin
  ch::Output<float> secs{0};   // monotonic: the ring's marquee

  sk_sp<SkTypeface> faceDisplay, faceBody, faceBold, faceTerm, faceMono;
  mskia::Paint heroInk;

  /** A beat on the bulletin's timeline. `window` clamps outside its range,
   *  so a track that has not started reads 0 and one that is over reads 1 —
   *  which is what makes a list of these a schedule rather than a set of
   *  independent animations. */
  [[nodiscard]] motion::Animatable<float> beat(float from, float to) {
    return motion::bind(&cycle).window(from, to);
  }

  /** The sheet's own envelope: up at the head of the bulletin, held, and
   *  out before the wrap, so the loop's cut happens on a dark sheet. The
   *  curve is what rounds the two shoulders — the corners stay exactly
   *  where the constants put them. */
  [[nodiscard]] motion::Animatable<float> envelope() {
    return motion::bind(&cycle)
        .source(0.0f, (float)kLoop)
        .trapezoid(kInFrom, kInTo, kOutFrom, kOutTo)
        .map(&ch::easeInOutQuad);
  }

  // ------------------------------------------------------------------
  // Type

  [[nodiscard]] sigil::weave::TextStyle body(float size, SkColor4f color,
                                             float track = 0) const {
    return weave::textStyle(
        {.face = faceBody, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle label(float size, SkColor4f color,
                                              float track = 2.4f) const {
    return weave::textStyle(
        {.face = faceBold, .size = size, .color = color, .track = track});
  }

  /** The three registers the forecast paragraph weaves together, as a NAMED
   *  set rather than as three call sites: the run says what it IS ("dir",
   *  "term") and the set says what that looks like. A name the set does not
   *  register falls back to the base, so a misspelling shows as body copy
   *  rather than as a run that did not draw. */
  [[nodiscard]] sigil::weave::StyleSet forecastStyles() const {
    sigil::weave::StyleSet set{body(19.5f, kBone)};
    // The wind direction: the one thing in the sentence that is a heading,
    // so it is set as one — condensed, tracked, and a shade brighter.
    set.set("dir", weave::textStyle({.face = faceBold,
                                     .size = 19.5f,
                                     .color = kBone,
                                     .track = 0.6f,
                                     .condense = 0.94f}));
    // A defined term. A serif italic inside a grotesque paragraph reads as
    // a citation of a glossary, which is exactly what these words are.
    set.set(
        "term",
        weave::textStyle(
            {.face = faceTerm, .size = 20.5f, .color = kAmber, .track = 0.2f}));
    return set;
  }

  // ------------------------------------------------------------------
  // The dominant move

  /** One hero line: the mask, the rise, and the breath.
   *
   *  The MASK is the clip on the wrapping box — the letters travel a whole
   *  cap height and are simply not there above the line box, which is what
   *  makes a rise read as a reveal rather than as a slide.
   *
   *  The CASCADE is per glyph and budgeted as a TOTAL rather than as a
   *  per-letter delay: a longer sea area name shortens each letter's wait
   *  instead of lengthening the whole reveal, which is what keeps the two
   *  lines in step whatever the bulletin is naming. (The nested cascade —
   *  words beating, glyphs beating inside each word's beat — is on the ring,
   *  where there are sixteen words for it to beat over.) */
  [[nodiscard]] Element heroLine(const char* words, const char* key,
                                 float delay) {
    Track rise{.effect = fx::rise(kHero * 1.24f),
               .stagger = {.amountMs = 320,
                           .durationMs = 560,
                           .from = motion::Spread::From::Start},
               .over = weave::unit::Glyph,
               .progress = beat(0.55f + delay, 2.55f + delay)};

    // The swell. GRAD is the advance-invariant weight axis — it thickens a
    // letter without moving the letter after it — so it can be driven at
    // DRAW time over glyphs that were shaped once. A small per-glyph offset
    // makes the swell roll along the line instead of pulsing as a block.
    // The track is NOT continuous: the snapping ladder is cut per rendered
    // size, so a hero this large gets a ladder fine enough that the steps do
    // not show — and the swell keeps the memoized faces a bounded ladder
    // buys instead of minting one per frame.
    Track swell{
        .effect = fx::variableAxisSweep("GRAD", 400.0f, 880.0f),
        .stagger = {.eachMs = 34, .durationMs = 620},
        // A swell, not an arrival: `cosine()` is 0 at both ends of
        // its period and 1 in the middle, which is what a window —
        // one-way by construction — cannot say.
        .progress =
            motion::bind(&secs).source(0.0f, (float)kBreathPeriod).cosine()};

    return box().clip().width(pct(100)).child(
        text(toU8(words), weave::textStyle({.face = faceDisplay,
                                            .size = kHero,
                                            .color = kBone,
                                            .track = 1.5f}))
            .key(key)
            .width(pct(100))
            .textAlign(sigil::weave::TextAlignment::kCenter)
            // The ramp is pinned to the line's METRIC BAND, not to
            // the glyphs — so a letter still under the mask is
            // painted with the bottom of the ramp and arrives into
            // the top of it. The gradient does not travel with the
            // letter; the letter travels through the gradient.
            .textFill(heroInk)
            .fx(std::move(rise))
            .fx(std::move(swell)));
  }

  // ------------------------------------------------------------------
  // The ring

  /** The letters of one area name, entering off their own curved
   *  baseline. Each name is its own run at its own bearing, so the sweep
   *  across the sixteen is a delay per run rather than an outer level of
   *  one cascade. */
  [[nodiscard]] static motion::Spread ringCascade() {
    return {.eachMs = 20, .durationMs = 420};
  }

  [[nodiscard]] Element ringPanel() {
    Element panel = box().width(kRingBox).height(kRingBox).shrink(0);

    // The wash under the ring: a soft light filling the square, so the
    // lettering has something to sit on without a visible plate edge.
    panel.child(box().inset(0).fill(mskia::Paint::glowUnit(
        {0.5f, 0.5f}, 0.94f,
        {{0.0f, kSeaLift}, {0.62f, hex(0x090E15)}, {1.0f, kSea}})));

    const auto hair = [](float r, SkColor4f color, float width) {
      return kit::disc(kEye, r)
          .corners({r})
          .fill(Fill::none())
          .stroke(stroke(width, Fill::color(color)));
    };
    panel.child(hair(kRingR + 21.0f, kKeyline, 1.0f).key("ring-outer"));
    panel.child(hair(kInnerR, kKeyline, 1.0f).key("ring-inner"));
    panel.child(hair(kInnerR - 9.0f, hex(0x121B26), 1.0f).key("ring-inner-2"));

    // THE COMPASS. A tick at every area's own bearing, and a longer one
    // with a letter at each cardinal point, so the ring can be read as a
    // bearing and not only as a list.
    for (int i = 0; i < kAreaCount; ++i) {
      const float rad = kAreaRing[i].bearingDeg * 3.14159265f / 180.0f;
      const float sx = std::sin(rad), sy = -std::cos(rad);
      panel.child(box()
                      .key("tick" + std::to_string(i))
                      .width(1.0f)
                      .height(9.0f)
                      .rotate(kAreaRing[i].bearingDeg)
                      .centerAt({kEye.x() + sx * (kRingR + 28.0f),
                                 kEye.y() + sy * (kRingR + 28.0f)})
                      .fill(Fill::color(kSlateDim))
                      .opacity(beat(0.10f, 1.20f)));
    }
    const char* kCardinals[4] = {"N", "E", "S", "W"};
    for (int q = 0; q < 4; ++q) {
      const float rad = (float)q * 1.5707963f;
      panel.child(text(toU8(kCardinals[q]), label(12.0f, kAmber, 2.0f))
                      .key(std::string("card") + kCardinals[q])
                      .centerAt({kEye.x() + std::sin(rad) * (kRingR + 46.0f),
                                 kEye.y() - std::cos(rad) * (kRingR + 46.0f)})
                      .opacity(beat(0.10f, 1.20f)));
    }

    // THE AREAS, one run each, ON the circle at its own bearing. `at` is
    // where along the baseline a run sits as a fraction of the whole path,
    // and shapes::circle() starts at due east and runs clockwise, so a
    // bearing becomes a fraction by subtracting the quarter turn between
    // the two conventions. Every run is centred on its own bearing, which
    // is the whole difference between a chart ring and a band of type
    // going round.
    //
    // NOT flipped. A ring has no side to be turned over to, so the rule is
    // the engraver's: glyph-up points radially outward everywhere, which is
    // what a coin's legend and a chart's compass ring both do.
    for (int i = 0; i < kAreaCount; ++i) {
      const float frac =
          std::fmod(kAreaRing[i].bearingDeg / 360.0f + 0.75f, 1.0f);
      // TWO RADII, ALTERNATING. Sixteen bearings thirteen to seventeen
      // degrees apart, and a name twenty degrees of arc long: on one
      // circle every neighbour collides, and the fix a chart uses is not
      // to move a label off its bearing but to move it off its
      // neighbour's ring.
      const float radius = (i % 2 == 0) ? kRingR : kRingR - 31.0f;
      // The sixteen beat in READ ORDER, not in bearing order — they are the
      // same sweep here, and saying it once in the delay is what makes that
      // visible rather than coincidental.
      const float start = 0.20f + (float)i * 0.17f;
      panel.child(text(toU8(kAreaRing[i].name),
                       weave::textStyle({.face = faceBold,
                                         .size = 11.5f,
                                         .color = hex(0xBFC7D1),
                                         .track = 1.1f}))
                      .key(std::string("area") + std::to_string(i))
                      .inset(kRingBox * 0.5f - radius)
                      .onPath({.path = shapes::circle(),
                               .at = frac,
                               .align = TextPath::Align::Center,
                               .offset = 7.0f,
                               .autoFlip = false})
                      .fx({.effect = fx::rise(13.0f),
                           .stagger = ringCascade(),
                           .progress = beat(start, start + 0.62f)}));
    }

    // The area being read, in the middle of its own ring.
    Element name = box()
                       .column()
                       .width(2.0f * kInnerR - 40.0f)
                       .centerAt({kEye.x(), kEye.y() - 6.0f})
                       .key("hero")
                       .child(heroLine("GERMAN", "hero-1", 0.0f))
                       .child(heroLine("BIGHT", "hero-2", 0.22f));
    panel.child(std::move(name));

    panel.child(text(toU8("SEA AREA \xc2\xb7 READ IN ORDER FROM VIKING"),
                     label(11.0f, kSlateDim, 3.0f))
                    .key("ring-cap")
                    .centerAt({kEye.x(), kEye.y() + 118.0f})
                    .opacity(beat(2.30f, 2.95f)));
    return panel;
  }

  // ------------------------------------------------------------------
  // The left column

  /** The gale warning: a two-phase sequence with a crossfade at the joint.
   *
   *  Each phase sees a renormalised 0→1 over its own window, so the slide
   *  runs its whole curve in the first 46% and the settle runs its whole
   *  curve in the rest. Without the crossfade the joint is a cut — the
   *  glyph is at the end of the slide on one frame and at the start of the
   *  settle on the next. With it, the last fifth of the slide is lerped
   *  into the settle's opening, so the strip arrives and compresses in one
   *  gesture. */
  [[nodiscard]] Element galeStrip() {
    TextEffect arrive = fx::seq(fx::slide(-46.0f).until(0.46f).xfade(0.20f),
                                fx::pop(0.86f, 2.6f));
    return box()
        .row()
        .alignItems(Align::Center)
        .gap(12)
        .padding(13, 10)
        .corners({3})
        .fill(Fill::color(hex(0x1C1206)))
        .stroke(
            stroke(1.0f, Fill::color(hex(0x4A3411)), PathFormat::Align::Inner))
        .opacity(beat(0.10f, 0.70f))
        .child(box().width(7).height(7).corners({4}).shrink(0).fill(
            Fill::color(kAmber)))
        .child(text(toU8("GALE WARNING \xc2\xb7 GERMAN BIGHT \xc2\xb7 "
                         "IMMINENT"),
                    label(13.5f, kAmber, 2.8f))
                   .key("gale")
                   .fx({.effect = std::move(arrive),
                        .stagger = {.eachMs = 0,
                                    .amountMs = 520,
                                    .durationMs = 620},
                        .progress = beat(0.25f, 1.85f)}));
  }

  /** The forecast itself: one paragraph, three faces, two tracks and one
   *  pattern.
   *
   *  TWO TRACKS PARTITION IT EXACTLY. `each(Word).take(1)` is the first
   *  letter of every word; `each(Word).drop(1)` is everything else. No
   *  glyph is in both and none is in neither, so the paragraph is covered
   *  twice over by two different moves rather than once by a compromise
   *  between them — the initials arrive with a longer lift, the bodies of
   *  the words follow them in flat — and a THIRD track lays the grade swell
   *  over the initials the face can carry it on.
   *
   *  ALL THREE CASCADES ARE NUMBERED OVER THE PARAGRAPH, which is what
   *  makes that a partition in TIME and not only in space: every glyph of
   *  word ten is on beat ten in every track, whatever any one selection
   *  turns out to cover. Without it a track could not narrow at all without
   *  dragging every word after the gap onto a different beat.
   *
   *  THE NUMERALS ARE FOUND, NOT DECLARED. A Beaufort force is a number
   *  wherever it appears, so it is addressed by pattern after the fact.
   *  `spanPaint` is paint only: those are the glyphs the unpainted
   *  paragraph shaped, at the positions it shaped them, wearing a different
   *  colour. */
  [[nodiscard]] Element forecast() {
    const sigil::weave::StyleSet set = forecastStyles();
    weave::RichText copy = weave::rich(set.base());
    copy.styles(set)
        .add(u8"Southwesterly", "dir")
        .add(u8" 5 to 7, occasionally gale 8 ")
        .add(u8"later", "term")
        .add(u8". Rain then showers. Moderate or good, occasionally ")
        .add(u8"poor", "term")
        .add(u8".");

    // THREE TRACKS OVER ONE PARAGRAPH, ON ONE CLOCK. `beats::Text` numbers
    // each cascade by the word's place in the PARAGRAPH rather than by its
    // place in that track's own selection, so every glyph of word ten
    // arrives on beat ten whatever any one selection turns out to cover.
    //
    // That is what makes the third track affordable. GRAD is an axis THIS
    // face carries and the serif does not, so the drive is kept off the
    // glossary by addressing the runs' NAME — the treatment they were
    // written in, which is exactly the thing that decides whether the axis
    // is there — while the LIFT still reaches every initial, because it is
    // a separate track over the whole set. Numbered over each track's own
    // selection the two would run cascades of different lengths, and the
    // grade would arrive on a different beat from the letter it grades.
    const weave::Selector everyInitial =
        weave::sel::each(weave::unit::Word).take(1);
    const weave::Selector glossary = sel::style("term");
    // ONE CLOCK ACROSS THE THREE. `beats::Text` numbers every word of the
    // paragraph, addressed or not, so three tracks that partition one
    // sentence share a ladder BY CONSTRUCTION; under the default numbering
    // each would count only its own selection and the grade would land on
    // a different beat from the letter it grades. It is a TRACK's answer,
    // beside the unit — the spread itself says nothing about text.
    const auto wordClock = [](float durationMs) {
      return motion::Spread{.eachMs = 46, .durationMs = durationMs};
    };
    Track initials{.where = everyInitial,
                   .effect = fx::rise(16.0f),
                   .stagger = wordClock(460.0f),
                   .over = weave::unit::Word,
                   .beatsOver = beats::Text,
                   .progress = beat(1.75f, 4.10f)};
    Track grade{.where = everyInitial & !glossary,
                .effect = fx::variableAxisSweep("GRAD", 400.0f, 900.0f),
                .stagger = wordClock(460.0f),
                .over = weave::unit::Word,
                .beatsOver = beats::Text,
                .progress = beat(1.75f, 4.10f)};
    Track bodies{.where = weave::sel::each(weave::unit::Word).drop(1),
                 .effect = fx::rise(9.0f),
                 .stagger = wordClock(500.0f),
                 .over = weave::unit::Word,
                 .beatsOver = beats::Text,
                 .progress = beat(1.83f, 4.30f)};

    return box()
        .column()
        .gap(9)
        .child(text(toU8("AREA FORECAST"), label(11.0f, kSlateDim, 3.0f))
                   .key("fc-eyebrow")
                   .opacity(beat(1.50f, 2.10f)))
        .child(text(copy)
                   .key("forecast")
                   .width(pct(100))
                   .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass)
                   .spanPaint(weave::sel::regex(u8"[0-9]+"),
                              sigil::weave::PaintStyle(kAmber.toSkColor()))
                   .fx(std::move(initials))
                   .fx(std::move(grade))
                   .fx(std::move(bodies)));
  }

  /** The barometer. A substitution draws a different letter at the original
   *  glyph's pen position, so it is honoured only where the replacement has
   *  the original's advance — which is why this readout is monospaced and
   *  why its charset is digits and capitals of one width. On a proportional
   *  face the runtime measures both, refuses, and draws the true letter. */
  [[nodiscard]] Element barometer() {
    const sigil::weave::TextStyle mono = weave::textStyle(
        {.face = faceMono, .size = 27.0f, .color = kBone, .track = 3.0f});
    return box()
        .column()
        .gap(7)
        .child(text(toU8("PRESSURE \xc2\xb7 TENDENCY"),
                    label(11.0f, kSlateDim, 3.0f))
                   .key("baro-eyebrow")
                   .opacity(beat(2.10f, 2.65f)))
        .child(text(toU8("1003 FALLING SLOWLY"), mono)
                   .key("baro")
                   // HELD, because a decode is otherwise churning at local
                   // 0: the substitution is in force from the track's first
                   // frame, so a glyph waiting its turn would show a wrong
                   // letter rather than no letter. The hold is per GLYPH,
                   // which is what a node-wide fade cannot be — each
                   // character of the readout arrives on its own beat and
                   // is simply absent before it.
                   .fx({.effect = fx::hold(fx::scramble(
                            U"0123456789ABCDEFGHJKLMNPRSTUVWXYZ", 16)),
                        .stagger = {.eachMs = 26,
                                    .durationMs = 520,
                                    .from = motion::Spread::From::Start},
                        .progress = beat(2.25f, 4.10f)}))
        .child(text(toU8("SLOWLY \xe2\x80\x94 0.1 TO 1.5 MB IN THREE HOURS"),
                    body(12.0f, kSlateDim, 0.6f))
                   .key("baro-note")
                   .opacity(beat(3.30f, 3.90f)));
  }

  /** The general synopsis — which in the bulletin comes BEFORE the areas,
   *  and is the only part of it that describes the weather rather than
   *  reporting it.
   *
   *  Its cascade beats over LINES, which is the coarsest unit a track can
   *  take and the right one here: a synopsis is read as a sequence of
   *  clauses, and a line is as close to a clause as a laid-out paragraph
   *  gets. The lines are found in the CURRENT layout, so a narrower column
   *  would re-break the passage and the cascade would follow it.
   *
   *  THE PRESSURES ARE PICKED OUT TWICE, and neither pick moves a letter: a
   *  grade by a `spanStyle` that changes nothing but the grade, then a
   *  colour by `spanPaint`. The order is the point: a restyle is measured
   *  against the text as the earlier declarations left it, so the grade is
   *  declared first, while the numerals still wear the base paint and differ
   *  from `graded` in the axis alone; declared after the colour it would
   *  differ in paint as well, re-shape, and — later winning — put the base
   *  paint back over the amber. A millibar reading is the one quantity in a
   *  synopsis a reader looks for rather than reads, and it wants the weight
   *  a colour alone cannot carry. GRAD is advance-invariant, so the whole
   *  numeral thickens where the paragraph already set it — a heavier FACE
   *  would be a `spanStyle`, and would re-break the passage the line cascade
   *  is beating over. */
  [[nodiscard]] Element synopsis() {
    const sigil::weave::StyleSet set = forecastStyles();
    sigil::weave::TextStyle graded = set.base();
    graded.variation("GRAD", 800.0f);
    weave::RichText copy = weave::rich(set.base());
    copy.styles(set)
        .add(u8"Low", "dir")
        .add(u8", Rockall, ")
        .add(u8"987")
        .add(u8", ")
        .add(u8"deepening rapidly", "term")
        .add(u8", expected Fair Isle ")
        .add(u8"968")
        .add(u8" by 0700 tomorrow. Atlantic high losing its grip.");

    return box()
        .column()
        .gap(9)
        .child(text(toU8("GENERAL SYNOPSIS \xc2\xb7 0100 UTC"),
                    label(11.0f, kSlateDim, 3.0f))
                   .key("syn-eyebrow")
                   .opacity(beat(2.60f, 3.10f)))
        .child(text(copy)
                   .key("synopsis")
                   .width(pct(100))
                   .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass)
                   .spanStyle(weave::sel::regex(u8"[0-9]+"), graded)
                   .spanPaint(weave::sel::regex(u8"[0-9]+"),
                              sigil::weave::PaintStyle(kAmber.toSkColor()))
                   .fx({.effect = fx::slide(-22.0f),
                        .stagger = {.eachMs = 150, .durationMs = 620},
                        .over = weave::unit::Line,
                        .progress = beat(2.70f, 4.60f)}));
  }

  /** Coastal stations: the quiet part of the sheet, and deliberately still.
   *  A supporting block that also moved would compete with the area name,
   *  and there is only one thing here the eye is meant to follow. */
  [[nodiscard]] Element stations() {
    struct Row {
      const char* place;
      const char* wind;
      const char* baro;
    };
    static constexpr Row kRows[] = {
        {"TIREE AUTOMATIC", "SW 6", "1008 / 1.4 FALLING"},
        {"STORNOWAY", "WSW 5", "1011 / 0.8 FALLING"},
        {"LERWICK", "S 7", "0999 / 2.9 FALLING"},
    };
    PathFormat rule;
    rule.width = 1.0f;
    rule.strokeFill = Fill::color(kKeyline);
    Element table = box().column().gap(0).child(
        text(toU8("COASTAL STATIONS \xc2\xb7 0100 UTC"),
             label(11.0f, kSlateDim, 3.0f))
            .key("st-eyebrow")
            .opacity(beat(2.66f, 3.16f))
            .margin(0, 0, 0, 8));
    for (int i = 0; i < 3; ++i) {
      const Row& r = kRows[i];
      table.child(
          box()
              .row()
              .height(29)
              .alignItems(Align::Center)
              .key(std::string("st") + std::to_string(i))
              .foreground(onEdges(sigil::geometry::path::Edge::Top, rule))
              .opacity(beat(2.80f + (float)i * 0.14f, 3.40f + (float)i * 0.14f))
              .child(text(toU8(r.place), body(12.5f, kBone, 0.8f)).grow(1))
              .child(text(toU8(r.wind), label(12.5f, kSlate, 1.4f))
                         .width(74)
                         .textAlign(sigil::weave::TextAlignment::kEnd))
              .child(text(toU8(r.baro), weave::textStyle({.face = faceMono,
                                                          .size = 12.0f,
                                                          .color = kSlate,
                                                          .track = 0.4f}))
                         .width(166)
                         .textAlign(sigil::weave::TextAlignment::kEnd)));
    }
    return table;
  }

  /** The Beaufort scale, which is the reason the paragraph has numerals in
   *  it at all: a force is a named band of wind, and the forecast quotes
   *  the number rather than the name. The four bands this bulletin names —
   *  fresh breeze through gale — carry the accent, and the numeral row is
   *  the same amber the paragraph's found numerals wear, so the two read as
   *  one fact stated twice. */
  [[nodiscard]] Element beaufort() {
    Element strip = box().row().gap(6).height(56).alignItems(Align::End);
    for (int f = 0; f <= 12; ++f) {
      const bool named = f >= 5 && f <= 8;
      const SkColor4f ink = named ? kAmber : hex(0x37475B);
      strip.child(
          box()
              .grow(1)
              .column()
              .gap(6)
              .alignItems(Align::Center)
              .key("bf" + std::to_string(f))
              .child(box()
                         .width(pct(100))
                         .height(6.0f + (float)f * 2.6f)
                         .fill(Fill::color(ink)))
              .child(text(toU8(std::to_string(f)),
                          label(10.5f, named ? kAmber : kSlateDim, 0.4f))));
    }
    return box()
        .column()
        .gap(9)
        .opacity(beat(3.20f, 3.80f))
        .child(text(toU8("BEAUFORT FORCE \xc2\xb7 5 TO 7, OCCASIONALLY 8"),
                    label(11.0f, kSlateDim, 3.0f))
                   .key("bf-eyebrow"))
        .child(std::move(strip))
        .child(text(toU8("5 FRESH BREEZE \xc2\xb7 6 STRONG BREEZE \xc2\xb7 "
                         "7 NEAR GALE \xc2\xb7 8 GALE"),
                    body(10.5f, kSlateDim, 0.8f))
                   .key("bf-names"));
  }

  // ------------------------------------------------------------------
  // The spine — the same engine, running down the page

  /** A column, not a rotated line. In vertical writing the reading axis is
   *  y, columns advance right to left, and Latin lies on its side by the
   *  standard's own rule — which is precisely the sideways slug a printed
   *  sheet carries down its gutter.
   *
   *  The entrance is worth watching: a track deviates in the frame the
   *  layout placed the glyph in, and here that frame is turned with the
   *  column, so the lift runs ACROSS the column rather than up the page. */
  [[nodiscard]] Element spine() {
    return text(toU8("BBC RADIO 4 \xc2\xb7 198 kHz LONG WAVE \xc2\xb7 0048"),
                label(12.5f, kSlateDim, 2.6f))
        .key("spine")
        .left(40)
        .top(196)
        .width(28)
        .height(560)
        .writingMode(sigil::weave::WritingMode::kVerticalRL)
        .fx({.effect = fx::rise(11.0f),
             .stagger = {.eachMs = 0,
                         .amountMs = 780,
                         .durationMs = 420,
                         .from = motion::Spread::From::Start},
             .progress = beat(0.45f, 2.70f)});
  }

  // ------------------------------------------------------------------

  [[nodiscard]] Element header() {
    Element left =
        box()
            .column()
            .grow(1)
            .gap(8)
            .child(text(toU8("MET OFFICE \xc2\xb7 FOR THE MARITIME AND "
                             "COASTGUARD AGENCY"),
                        label(11.0f, kSlateDim, 3.2f))
                       .key("eyebrow")
                       .opacity(beat(0.05f, 0.55f)))
            .child(text(toU8("THE SHIPPING FORECAST"),
                        weave::textStyle({.face = faceDisplay,
                                          .size = 34.0f,
                                          .color = kBone,
                                          .track = 1.0f}))
                       .key("title")
                       .fx({.effect = fx::rise(16.0f),
                            .stagger = {.eachMs = 0,
                                        .amountMs = 420,
                                        .durationMs = 520},
                            .progress = beat(0.15f, 1.30f)}));

    Element right = box().column().gap(5).alignItems(Align::End);
    static constexpr const char* kSlug[] = {
        "ISSUED 0015 UTC \xc2\xb7 VALID TO 0600 UTC TOMORROW",
        // The literals break after an en dash on purpose: \x93 followed by
        // a digit would be read as one out-of-range hex escape.
        "IMMINENT: WITHIN 6 H \xc2\xb7 SOON: 6\xe2\x80\x93"
        "12 H \xc2\xb7 LATER: BEYOND 12 H",
        "GOOD > 5 NM \xc2\xb7 MODERATE 2\xe2\x80\x93"
        "5 NM \xc2\xb7 POOR 1000 M \xe2\x80\x93 2 NM",
    };
    for (int i = 0; i < 3; ++i)
      right.child(text(toU8(kSlug[i]), body(11.5f, kSlateDim, 0.5f))
                      .key("slug" + std::to_string(i))
                      .opacity(beat(0.55f + (float)i * 0.16f,
                                    1.15f + (float)i * 0.16f)));

    return box()
        .row()
        .alignItems(Align::End)
        .child(std::move(left))
        .child(std::move(right));
  }

  [[nodiscard]] Element describe() {
    Element column =
        box()
            .column()
            .inset(112, 40, 44, 38)
            .gap(26)
            // The whole performance under one envelope: it rises once at
            // the head of the bulletin and leaves before the wrap, so the
            // loop's cut happens on a dark sheet.
            .opacity(envelope())
            .child(header())
            .child(box().height(1).fill(Fill::color(kKeyline)))
            .child(box()
                       .row()
                       .gap(48)
                       .grow(1)
                       .child(box()
                                  .width(kColW)
                                  .shrink(0)
                                  .column()
                                  .gap(26)
                                  .child(galeStrip())
                                  .child(forecast())
                                  .child(barometer())
                                  .child(synopsis())
                                  .child(box().grow(1))
                                  .child(beaufort())
                                  .child(stations()))
                       .child(box()
                                  .grow(1)
                                  .alignItems(Align::Center)
                                  .justify(Justify::Center)
                                  .child(ringPanel())))
            .child(text(toU8("EVERY ADJECTIVE IN THE BULLETIN IS A DEFINED "
                             "QUANTITY \xc2\xb7 THE ORDER OF THE AREAS IS "
                             "FIXED AND RUNS CLOCKWISE"),
                        body(11.0f, kSlateDim, 0.5f))
                       .key("foot")
                       .opacity(beat(3.10f, 3.75f)));

    return stack()
        .fill(linearGradient({0, 0}, {0, kH}, {kSea, kSeaLift, hex(0x05080C)},
                             {0.0f, 0.55f, 1.0f}))
        .child(spine().opacity(envelope()))
        .child(std::move(column));
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext& ctx) override {
    // EVERY SCRAMBLE HAS RESOLVED. The barometer's readout runs an
    // `fx::hold(fx::scramble(...))` to 4.10 s and the forecast paragraph's
    // initials converge on their bodies after that, so a still taken
    // before either lands photographs a nonsense word under a caption that
    // defines the real one, and a paragraph that reads as a rendering
    // fault. The grade swell peaks every 7.2 s, so the second peak is the
    // frame where the swell is at its height AND nothing is mid-decode.
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 10.8, .background = kSea});

    // The system grotesque is the face that carries GRAD, the
    // advance-invariant weight axis the swell needs. The stand-ins keep the
    // sheet legible where it is absent; the swell then simply does not
    // happen, and says so once.
    faceDisplay =
        weave::ports::face({".SF NS", "SF Pro", "Helvetica Neue"}, 700);
    faceBold = weave::ports::face({".SF NS", "SF Pro", "Helvetica Neue"}, 600);
    faceBody = weave::ports::face({".SF NS", "SF Pro", "Helvetica Neue"}, 400);
    faceTerm = weave::ports::face({"Iowan Old Style", "Charter", "Georgia"},
                                  400, SkFontStyle::kItalic_Slant);
    faceMono = weave::ports::face({"Menlo", "SF Mono", "Courier New"}, 400);

    // The hero's ink: a ramp pinned to the metric band, warm at the
    // baseline and bone at the cap line, so a letter arriving from below
    // cools as it rises into place.
    heroInk = mskia::Paint::linearUnit(
        {0.5f, 0.0f}, {0.5f, 1.0f},
        {{0.00f, hex(0xFFFBF2)}, {0.52f, kBone}, {1.00f, hex(0xC9A46A)}});

    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      cycle = motion::phase(t, kLoop) * (float)kLoop;
      secs = (float)t;
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    ShippingForecast, "Study \xc2\xb7 Type",
    "BBC Radio 4's 0048 bulletin as a sheet that performs itself "
    "\xe2\x80\x94 a fixed vocabulary, a ring of sea areas, one dominant "
    "move")
