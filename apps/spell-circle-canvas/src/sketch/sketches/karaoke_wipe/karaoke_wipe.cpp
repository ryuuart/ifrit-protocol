// karaoke_wipe — PATTERN: the karaoke caption. Two devices that solve the
// same problem sixty years apart, put on one timeline.
// =============================================================================
// THE PATTERN, AND WHERE IT COMES FROM
//
//   1924 — THE BOUNCING BALL. Max Fleischer's Song Car-Tunes (Inkwell
//          Studios / Red Seal) put the words of a song on screen with a
//          white ball that HOPS from word to word, landing on each one as
//          it is sung, so a cinema audience knows where it is in the line.
//          The series began in 1924; "Come Take a Trip in My Airship" (Ren
//          Shields and George Evans, 1904 — the lyric in data/airship.json,
//          and long out of copyright) is among its first titles.
//
//   1985 — THE CD+G WIPE. Karaoke discs carry graphics in the CD subcode
//          channels, and the convention that settled there is not a ball
//          but a WIPE: the whole line is on screen in a pale colour and a
//          saturated colour sweeps through it left to right in time with
//          the singing, so the reader sees both what has gone and what is
//          coming. Every lyric video and every phone karaoke app since is
//          a restatement of that.
//
// Both are the same instruction — "you are HERE in this line" — and they
// disagree about whether to mark the point or the boundary. This study runs
// them together off one schedule: the wipe crosses letter by letter, the
// ball hops word by word, and the ruler underneath carries a playhead
// across the letters at the pace the schedule sets.
//
// It is shown the way both were seen: on a lit screen in a dark room. The
// caption glows where it has been sung, the tube's lines and corner falloff
// lie over it, and the screen throws its blue onto the room around it. The
// ball has weight — it drops in, counts the singer in on four bounces that
// each take one of the count-in dots, squashes where it lands and stretches
// as it flies — and every letter kicks and flares as the colour reaches it.
//
// -----------------------------------------------------------------------------
// HOW EACH IS SPELLED
//
// THE LOOK IS ONE SHEET. The lyric is a role, `lyric`, and the sheet sets
// it: the CD+G palette as custom properties on the root, the aliased face,
// and the black keyline every CD+G caption wears — a `textStroke` under the
// fill, which is what keeps a caption legible over a picture it does not
// own. The line to come is the same role in the class `next`, so it
// differs only in size and colour.
//
// THE WIPE IS A COLOUR MULTIPLIER ON A CASCADE. The sung line is set ONCE,
// in the sung colour, and `textFx::tint(pale, sung)` multiplies every glyph
// down to the pale colour until its own beat arrives. It is a multiplier
// rather than a colour because that is what a `GlyphModifier` carries —
// every pass the glyph's style draws is modulated, the keyline included,
// and black multiplied by anything stays black.
//
// THE CASCADE IS TWO DEEP AND ITS OUTER LEVEL IS A TABLE. A real disc
// carries a time for every syllable, cut against the recording: a held note
// holds and a fast line races, and nothing evenly spaced sounds like
// singing. data/airship.json is that table — each word beside the time it
// is sung — and `then(...)` sweeps the letters of each word evenly inside
// its beat, which is how a syllable's own wipe behaves. The table gives the
// line its uneven shape; the progress window gives it its tempo, so the
// same table sings faster or slower without being recut. The file is read
// in setup, so an edit to it re-sings the line without a rebuild.
//
// THE RULER HANGS OFF THE LETTERS. Each tick is a `textAttach` on one
// letter, standing on the rect the layout gave it, taller where a word
// begins; it reserves nothing, so the next line keeps clear of it by its
// own margin. The ticks are where the letters ARE, and the playhead
// crossing them at an uneven speed is the schedule.
//
// THE BALL AND THE PLAYHEAD RIDE THE SCHEDULE READ BACK. An attached mark
// stands on a letter's REST rect; one that must move with the cascade reads
// `Composer::beatsOf` — one `Beat` per letter, carrying that letter's
// laid-out `rect`, the `unitIndex` of its word and its own `localTime` right
// now — and drives its own transform from it. `update` writes those two
// transforms every frame and describes nothing: the tree is declared once.
//
// THE SCREEN IS A STACK OF THREE LIGHTS. The glass is a radial ramp of the
// ground colour, brightest a little above centre; the sung line throws
// light — a copy of it, sung from black on the same two tracks, blurred and
// added under it, so only what has been sung glows and the glow follows the
// wipe with no schedule of its own; the tube — scanlines, grain and the
// corner falloff — lies over both as one cached texture, because it never
// changes.
//
// THE CATCH IS A SECOND TRACK ON THE SAME TABLE. Each letter lifts and
// flares toward white for a moment as its colour arrives, which is the beat a singer
// hits. It reads the same cues as the wipe with a longer beat of its own,
// and its progress window is stretched by the ratio of the two spans, so a
// cue lands at the same instant in both.
//
// THE COUNT-IN. A karaoke screen counts the singer in with dots before the
// first word. Here the ball drops onto the first word and bounces there in
// tempo, and each landing pops one dot; the fourth landing is the downbeat,
// with no dots left and the wipe starting under the ball.
//
// Nothing here restates the cascade's arithmetic, which is the point: a
// nested beat lasts exactly as long as its inner ladder needs, and a cue
// table can be recut between takes.
//
// -----------------------------------------------------------------------------
// EDIT THESE FIRST
//   data/airship.json — the sung words and the time each one starts, in ms.
//               Type a real line's syllable times here and the caption
//               follows them.
//   kEachMs   — start-to-start between the LETTERS inside one word.
//   kSwitchMs — how long ONE letter takes to change colour. Small is the
//               CD+G hard edge; large is a soft gradient crossing the word.
//   kLineSeconds — how long the whole line takes. The tempo, not the shape.
//   kHopHeight — the ball's arc. Fleischer's is high and slow.
//   kCountBeat — one count-in beat, s. Four of them are the lead-in.
//   kSquash    — how flat the ball lands; 0 is a billiard ball.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/karaoke_wipe/karaoke_wipe.cpp \
//       --frame /tmp/karaoke_wipe.png
//
//   The hop, frame by frame:  --at 1.20 --frames 10 --fps 12

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>
#include <sigilmaterial/paint/Bases.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "CrtOverlay.h"

using namespace std::chrono_literals;

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr float kWidth = 1020.0f;
constexpr float kHeight = 520.0f;
constexpr float kMargin = 46.0f;
/** The screen the caption is shown on: the page's measure, and the height
 *  the sung line, its ruler and the line to come need with the ball's arc
 *  above them. */
constexpr float kScreenWidth = kWidth - 2 * kMargin;
constexpr float kScreenHeight = 318.0f;

// THE CD+G PALETTE. A disc's graphics channel carries 16 colours at 4 bits
// a component, so every value here is a multiple of 0x11 — the only values
// that hardware could name. The ground is the saturated blue a karaoke
// screen sits on; the resting line is a PALE low-chroma blue and the sweep
// is the warm near-yellow, so the swept half is both the brightest and the
// most coloured thing on the line. Reversed — a white resting line and a
// mid-chroma sweep — the wipe reads backwards, because the part not yet
// sung is then what the eye goes to.
//
// THE TINT IS A MULTIPLIER, so the resting colour must be no brighter than
// the sung one in ANY channel: the line is set in the sung colour and
// multiplied down, and a channel asking to rise simply clamps. That is why
// the resting blue is a dim slate rather than a pale one — a pale blue
// against a warm yellow would come back olive, which looks like a bug in
// the tint rather than a choice about the palette.
//
// The sheet reads the colours it sets as custom properties on the root.
// The ground, the resting colour and the ball are not the sheet's: a
// gradient's stops and `textFx::tint`'s two ends are colours, not
// references into it, so the tint's sung end is `kSung` beside the
// `sung` the lyric is inked in, and the two must stay one value.
constexpr material::Color kStage = hexColor(0x110033);  // the screen's rim
constexpr material::Color kGlass = hexColor(0x221166);  // where the tube is lit
constexpr material::Color kSung = hexColor(0xFFEEAA);  // the saturated colour
constexpr material::Color kPale = hexColor(0x556688);  // the resting line
constexpr material::Color kNext = hexColor(0x445577);  // the line to come
constexpr material::Color kLabel = hexColor(0x88AACC);
constexpr material::Color kFaint = hexColor(0x445588);
/** The hard black keyline a disc draws as a second colour index around the
 *  glyph cell. */
constexpr material::Color kKey = hexColor(0x000000);
constexpr material::Color kBall = hexColor(0xFFFFFF);
constexpr material::Color kBezel = hexColor(0x334477);

// THE ROOM. Not the disc's colours: the dark the screen stands in, and the
// blue the screen throws onto it.
constexpr material::Color kRoom = hexColor(0x07051A);
constexpr material::Color kRoomFloor = hexColor(0x0E0A2C);
constexpr material::Color kSpill{0.20f, 0.14f, 0.62f, 0.55f};
/** The flare a letter catches as its colour arrives, a MULTIPLIER above
 *  one: it lifts the sung yellow's green and blue until the letter burns
 *  near white, and leaves the black keyline black, where a screen would
 *  lift the keyline to the flare's own colour and blur the letter's edge.
 *  Its alpha is 1 because a multiplier's alpha is the glyph's coverage. */
constexpr material::Color kFlare{1.0f, 1.07f, 1.45f, 1.0f};
/** The lift the line to come takes on as the sung line is held, screened
 *  over its resting colour: the cue that it is next. */
constexpr material::Color kCueLight{0.16f, 0.20f, 0.30f, 0};

constexpr float kLyricSize = 46.0f;
constexpr float kLabelSize = 11.5f;

// ---- the schedule ---------------------------------------------------------
constexpr float kEachMs = 46.0f;      // start-to-start, per letter of a word
constexpr float kSwitchMs = 190.0f;   // one letter's own change
constexpr float kCatchMs = 420.0f;    // one letter's lift and flare
constexpr float kCatchLift = 5.0f;    // px a letter kicks up as it is sung
constexpr int kCountIn = 4;           // the count-in's beats and dots
constexpr double kCountBeat = 0.40;   // one count-in beat, s
constexpr double kLeadIn = kCountIn * kCountBeat;  // the count, then the line
constexpr double kLineSeconds = 2.9;  // the whole line, tempo only
constexpr double kHold = 1.40;        // the sung line held before the loop cuts
constexpr float kHopHeight = 44.0f;   // the 1924 arc
constexpr float kBallRadius = 7.5f;   // Fleischer's hard white disc
/** How much of a word's own beat the ball SITS on it before leaving. The
 *  1924 ball rests on the word it is naming and crosses in a hurry; a value
 *  of 0 would make it slide continuously and name nothing. */
constexpr float kBallHold = 0.62f;
/** How flat the ball goes on landing (a share of its height) and how much
 *  of a hold the squash takes to recover; how much longer it grows in
 *  flight at its fastest; how far it falls onto the first word, and how
 *  high it bounces there on each count. */
constexpr float kSquash = 0.30f;
constexpr float kSquashShare = 0.38f;
constexpr float kStretch = 0.16f;
constexpr float kDropHeight = 120.0f;
constexpr float kCountHop = 26.0f;

// ---- the phosphor's light ----------------------------------------------------
constexpr float kHaloBlur = 9.0f;    // how far the sung letters' light spreads
constexpr float kHaloReach = 24.0f;  // room the light is given past the line
constexpr material::Color kBallLight{1.0f, 1.0f, 1.0f, 0.75f};  // the ball's own

// ---- the count-in dots, left of the sung line ------------------------------
constexpr float kCountDot = 7.0f;
constexpr float kCountSpacing = 12.0f;
constexpr float kCountLeft = -52.0f;  // the first dot, from the line's edge
constexpr float kCountDrop = 21.0f;   // from the letter's top to the dots
constexpr float kPop = 0.14f;         // s a dot takes to burst

// ---- the ruler, hung below the sung line ----------------------------------
constexpr float kRulerDrop = 12.0f;   // from the line's foot to the ruler
constexpr float kOnsetTick = 15.0f;   // the tick where a word begins
constexpr float kLetterTick = 8.0f;   // every other letter's
constexpr float kRulerDepth = 22.0f;  // the ruler's own height
constexpr float kRulerFoot = kRulerDrop + kOnsetTick;
constexpr float kPlayheadLength = 20.0f;  // from the ruler's top, past its foot

/** THE LOOK, said once. Everything inherits the face, the weight, the
 *  tracking and the label ink from the root; a role says only how it
 *  differs. */
StyleSheet look() {
  return {
      rule(":root")
          .var("sung", kSung)
          .var("next", kNext)
          .var("label", kLabel)
          .var("faint", kFaint)
          .var("key", kKey)
          .var("bezel", kBezel)
          .fontFamily("Avenir Next, Futura, Helvetica Neue, sans-serif")
          .fontWeight(600)
          .letterSpacing(0.2f)
          .ink(var("label")),
      rule("h1").fontSize(24),
      rule("caption, footer").fontSize(kLabelSize),
      rule("caption").letterSpacing(0.3f),
      // The song is credited as its 1904 sheet music was: the title in a
      // Didone italic, the writers in its small capitals.
      rule("label")
          .fontFamily("Bodoni 72, Didot, Baskerville, serif")
          .fontStyle(FontStyle::Italic)
          .fontWeight(400)
          .fontSize(21)
          .letterSpacing(0)
          .ink(var("sung")),
      rule("caption.credit")
          .fontFamily("Bodoni 72 Smallcaps, Bodoni 72, Didot, serif")
          .fontWeight(400)
          .fontSize(14)
          .letterSpacing(0.9f),
      rule("screen").borderRadius({22}).overflow(Overflow::Clip),
      rule("footer").width(700),
      rule("rule").ink(var("faint")),
      // The CD+G screen is a grid of 6x12 pixel cells and its type is a
      // bitmap face with no antialiasing at all: `aliased` lights a pixel
      // only where its centre is inside the outline, so the glyph edges
      // land on the grid instead of ramping across it.
      rule("lyric")
          .fontSize(kLyricSize)
          .letterSpacing(2)
          .font({.aliased = true, .antiAlias = false})
          .ink(var("sung"))
          .textStroke(5.0f, Fill::var("key")),
      // The ball's arc stands above the sung line; the ruler hangs below
      // it and reserves nothing, so the line to come keeps clear of it.
      rule(".line").marginTop(kHopHeight + 18.0f),
      rule("lyric.next")
          .fontSize(kLyricSize * 0.78f)
          .ink(var("next"))
          .marginTop(kRulerDrop + kRulerDepth + 22.0f),
      rule(".tick").ink(var("faint")),
      rule(".tick.onset").ink(var("label")),
      rule(".playhead").ink(var("sung")),
      rule(".count").fill(Fill::var("sung")),
  };
}

/** The song as the disc carries it: the sung words with the time each one
 *  starts, and the line to come. */
struct Song {
  std::string title;
  std::string writers;
  std::string sung;
  std::vector<float> cues;  // ms, one per word of `sung`
  std::string next;
  uint32_t widest = 1;  // letters in the longest sung word
};

Song songFrom(const sketch::kit::Document& document) {
  Song song;
  for (const sigil::data::Json& word : document["sung"].array()) {
    if (!song.sung.empty()) song.sung += ' ';
    song.sung += word[0].string();
    song.cues.push_back((float)word[1].number());
    song.widest = std::max(song.widest, (uint32_t)word[0].string().size());
  }
  song.title = std::string(document["song"].string());
  song.writers = std::string(document["writers"].string());
  song.next = std::string(document["next"].string());
  return song;
}

/** THE CASCADE: the sung times per word, the letters swept inside each.
 *  What a unit IS lives on the track — `unit = Word`, `innerUnit =
 *  Cluster` — because a spread is SigilMotion's and says nothing about
 *  text. */
motion::Spread wipeCascade(const Song& song, float letterMs = kSwitchMs) {
  motion::Spread cascade;
  cascade.cues(song.cues);
  cascade.then({.eachMs = kEachMs, .durationMs = letterMs});
  return cascade;
}

/** The progress window a cascade over the song runs in, in seconds of the
 *  loop. The wipe's span is what `kLineSeconds` is the length of; a cascade
 *  whose beats last longer runs proportionally longer, so a cue lands at
 *  the same instant in both. */
motion::Bound progressOf(const motion::Spread& cascade, const Song& song,
                         const motion::Animatable<float>& cycle) {
  const auto words = (uint32_t)song.cues.size();
  const float span = cascade.spanMs(words, song.widest) /
                     wipeCascade(song).spanMs(words, song.widest);
  return motion::bind(cycle, {.from = {(float)kLeadIn, (float)(kLeadIn + kLineSeconds * span)}, .clampFrom = true});
}

/** THE CATCH: a letter kicks up and flares as its colour arrives, and is
 *  home by the end of its own beat. Both beats open on the same cue, so
 *  the tint finishes `kSwitchMs` into a catch `kCatchMs` long, and that is
 *  where the flare peaks, over the sung colour. The kick starts at once,
 *  but the flare waits until the tint is most of the way there: over a
 *  letter still half slate it brightens slate, which reads as a grey
 *  ghost rather than a flare. */
TextEffect catchEffect() {
  constexpr float kArrived = kSwitchMs / kCatchMs;
  constexpr float kFlareOpens = 0.6f * kArrived;
  return textFx::keys(
      {{0.0f, {}},
       {kFlareOpens, {.dy = -kCatchLift * 0.8f}},
       {kArrived, {.dy = -kCatchLift, .colorMultiplier = kFlare}},
       {1.0f, {}}},
      motion::ease::outQuad);
}

/** THE RULER'S PLACE under a letter: the letter's foot, and the drop below
 *  it. A mark's insets are read in px, pt, pct, pw and ph only, its bottom
 *  sizes it rather than placing it, and its margin is not read, so "the
 *  foot plus 12 px" has no inset or margin that says it.
 *  workaround: the mark stands at the foot and a constant translate carries
 *  it down the rest of the way. */
template <class Node>
Node belowTheLetter(Node mark, float drop) {
  mark.left(0).top(pct(100)).translateY(drop);
  return mark;
}

/** One tick of the ruler on every letter, taller where a word begins. The
 *  ticks share one foot, `kRulerFoot` below the line's, so a letter's tick
 *  is the onset's with its top cut short. */
Text withRuler(Text line, const std::string& words) {
  for (size_t index = 0; index < words.size(); ++index) {
    if (words[index] == ' ') continue;
    const bool onset = index == 0 || words[index - 1] == ' ';
    const float length = onset ? kOnsetTick : kLetterTick;
    line.textAttach(
        weave::selectors::range({(uint32_t)index, (uint32_t)index + 1}),
        belowTheLetter(kit::line({.length = Dimension(length), .column = true})
                           .styleClass(onset ? "tick onset" : "tick"),
                       kRulerFoot - length));
  }
  return line;
}

/** One sung word as the schedule places it: its share of the wipe, 0 to 1,
 *  as the mean of its letters' beats, and its extent across the line. */
struct SungWord {
  float coverage = 0;
  float letters = 0;
  float left = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::lowest();
  [[nodiscard]] float centre() const { return (left + right) * 0.5f; }
};

std::vector<SungWord> sungWords(const std::vector<Beat>& beats) {
  std::vector<SungWord> words;
  for (const Beat& beat : beats) {
    if (beat.unitIndex >= words.size()) words.resize(beat.unitIndex + 1);
    SungWord& word = words[beat.unitIndex];
    word.coverage += beat.localProgress;
    word.letters += 1.0f;
    word.left = std::min(word.left, beat.rect.left());
    word.right = std::max(word.right, beat.rect.right());
  }
  for (SungWord& word : words)
    if (word.letters > 0) word.coverage /= word.letters;
  return words;
}

/** THE COUNT-IN: a row of dots before the sung line, one per beat of the
 *  lead-in, each bursting — growing as it goes out — at the moment the
 *  ball lands for that beat. */
Text withCountIn(Text line, const motion::Animatable<float>& cycle) {
  const weave::Selector first = weave::selectors::range({0, 1});
  for (int index = 0; index < kCountIn; ++index) {
    const auto landing = (float)(kCountBeat * (index + 1));
    line.textAttach(
        first, kit::at(kCountLeft + (float)index * kCountSpacing, kCountDrop,
                       kCountDot, kCountDot)
                   .shape(sigil::geometry::shapes::circle())
                   .styleClass("count")
                   .transformOrigin(pct(50), pct(50))
                   .scale(motion::bind(cycle, {.from = {landing, landing + kPop}, .clampFrom = true, .to = {1.0f, 2.6f}}))
                   .opacity(motion::bind(cycle, {.from = {landing, landing + kPop}, .clampFrom = true, .to = {1.0f, 0.0f}})));
  }
  return line;
}

/** THE TUBE over the screen: its scanlines, a grain in how much light
 *  each line gives up, and the falloff into the corners, in black. */
Element tube() {
  return box()
      .cover()
      .fill(
          karaoke_wipe::crtOverlay({.uScanPitch = 3.0f,
                                       .uScanStrength = 0.10f,
                                       .uVigInner = 1.05f,
                                       .uVigOuter = 2.05f,
                                       .uVigStrength = 0.62f,
                                       .uSqueeze = 0.62f,
                                       .uGrain = 0.05f}))
      .cache(Cache::Texture)
      .key("tube");
}

/** THE SCREEN: the lit glass, the caption, and the tube over both. The screen throws its blue onto the
 *  room as a wide soft shadow, dropped a little, as a set on a stand
 *  lights the floor more than the ceiling. */
Element screen(Element caption) {
  return stack()
      .role("screen")
      .width(kScreenWidth)
      .height(kScreenHeight)
      .background(shadow(kSpill, {0, 6}, 46))
      .fill(material::radialGradient(
          {kScreenWidth * 0.5f, kScreenHeight * 0.42f}, kScreenWidth * 0.62f,
          {kGlass, kStage}, {.units = material::GradientUnits::Pixels}))
      .stroke(stroke(1.5f, Fill::var("bezel")))
      .children({kit::centred(std::move(caption)).cover(), tube()});
}

/** THE PHOSPHOR'S BLOOM over a sung line: a second copy of the line,
 *  blurred and ADDED under the first, in a box that reaches `kHaloReach`
 *  past the line on every side so the light is not cut square at the
 *  letters. The copy is sung from black, so only what has been sung throws
 *  light and the glow follows the wipe on the wipe's own schedule.
 *  A glyph-outline shadow on the line itself would be the one-node
 *  spelling, but that outline is cut from the glyphs at rest and painted in
 *  one colour, so no track tints or moves it; and the phosphor bloom effect
 *  over a live layer does not fit a frame. */
Element glowing(Text line, Text copy) {
  return box().styleClass("line").children(
      {box()
           .absolute()
           .inset(Dimension(-kHaloReach))
           .padding(kHaloReach)
           .children({std::move(copy)})
           .filter(material::Filter::blur(kHaloBlur))
           .blendMode(material::BlendMode::PlusLighter),
       std::move(line)});
}

}  // namespace

// ===========================================================================

struct KaraokeWipe {
  motion::Animatable<float> cycle = motion::animatable(0.0f);      // seconds into one pass, wrapping
  motion::Animatable<float> ballX = motion::animatable(0.0f);      // px from the first letter's edge
  motion::Animatable<float> ballY = motion::animatable(0.0f);      // px above the ball's rest
  motion::Animatable<float> ballWidth = motion::animatable(1.0f);  // the squash and stretch, as scales
  motion::Animatable<float> ballHeight = motion::animatable(1.0f);
  motion::Animatable<float> playhead = motion::animatable(0.0f);  // px from the first letter's edge
  double loop = kLeadIn + kLineSeconds + kHold;

  /** The sung line with its two tracks: the wipe from @p resting to the
   *  sung colour, and the catch, both on the song's own table. */
  [[nodiscard]] Text singing(const Song& song, material::Color resting) const {
    const motion::Spread wipe = wipeCascade(song);
    const motion::Spread lift = wipeCascade(song, kCatchMs);
    return text(song.sung)
        .role("lyric")
        .textFx({.effect = textFx::tint(resting, kSung),
                 .stagger = wipe,
                 .unit = weave::Unit::Word,
                 .innerUnit = weave::Unit::Cluster,
                 .progress = progressOf(wipe, song, cycle)})
        .textFx({.effect = catchEffect(),
                 .stagger = lift,
                 .unit = weave::Unit::Word,
                 .innerUnit = weave::Unit::Cluster,
                 .progress = progressOf(lift, song, cycle)});
  }

  [[nodiscard]] Element describe(const Song& song) const {
    // The line is SET IN the sung colour and the tint multiplies it down to
    // the pale one until each letter's own beat arrives — the inversion a
    // colour multiplier forces, which is why the effect takes the two
    // colours in time order and does the division itself. The catch rides
    // the same table on a second track.
    Text sung = singing(song, kPale).key("sung");
    // The ball and the playhead stand on the FIRST letter and are carried
    // from there by the schedule read back in `update`. Fleischer's ball is
    // a hard white disc, the one thing on the frame that is not part of the
    // caption; it squashes and stretches about its foot, where it touches.
    const weave::Selector first = weave::selectors::range({0, 1});
    sung =
        withCountIn(withRuler(std::move(sung), song.sung), cycle)
            .textAttach(
                first,
                belowTheLetter(kit::line({.length = Dimension(kPlayheadLength),
                                          .thickness = 2,
                                          .column = true})
                                   .styleClass("playhead")
                                   .translateX(playhead),
                               kRulerDrop))
            .textAttach(first, kit::at(-kBallRadius, -(2 * kBallRadius + 3.0f),
                                       2 * kBallRadius, 2 * kBallRadius)
                                   .shape(sigil::geometry::shapes::circle())
                                   .background(shadow(kBallLight, {0, 0}, 7))
                                   .fill(kBall)
                                   .key("ball")
                                   .transformOrigin(pct(50), pct(100))
                                   .translateX(ballX)
                                   .translateY(ballY)
                                   .scaleX(ballWidth)
                                   .scaleY(ballHeight));
    // The line to come lifts toward the sung line's pale as the sung line
    // is held, word by word from the left: it is next.
    Text next = text(song.next).role("lyric").styleClass("next").textFx(
        {.effect = textFx::keys({{0.0f, {}}, {1.0f, {.colorScreen = kCueLight}}},
                                motion::ease::inOutQuad),
         .tween = {.duration = 320ms, .delay = motion::stagger(70ms)}, 
         .unit = weave::Unit::Word,
         .progress = motion::bind(cycle, {.from = {(float)(kLeadIn + kLineSeconds + 0.25), (float)(kLeadIn + kLineSeconds + kHold - 0.15)}, .clampFrom = true})});

    return box()
        .column()
        .padding(34, kMargin)
        .gap(20)
        .fill(material::linearGradient(
            {0, 0}, {0, kHeight}, {{0.35f, kRoom}, {1.0f, kRoomFloor}},
            {.units = material::GradientUnits::Pixels}))
        .applyStyleSheet(look())
        .children({
            // The song is credited as a Song Car-Tune's title card credits
            // it: the title, and who wrote it and when.
            box()
                .row()
                .justifyContent(Justify::SpaceBetween)
                .alignItems(Align::End)
                .cache(Cache::Texture)
                .key("masthead")
                .children(
                    {box().column().gap(5).children(
                         {document::h1("Follow the bouncing ball"),
                          document::caption("Fleischer 1924 · CD+G 1985")}),
                     box()
                         .column()
                         .gap(3)
                         .alignItems(Align::End)
                         .children({document::label(song.title),
                                    document::caption(song.writers)
                                        .styleClass("credit")})}),
            document::rule(),
            screen(box().column().children(
                {glowing(std::move(sung), singing(song, kKey)), next})),
            // The numbers are read off the table rather than typed beside
            // it: a caption that can disagree with the schedule it
            // describes is the one thing worse than no caption.
            document::footer(kit::formatted(
                "The ball marks the word; the wipe marks progress. %zu cues "
                "· %.0f ms per letter · %.0f ms colour change.",
                song.cues.size(), kEachMs, kSwitchMs)),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.background(kRoom);
    if (!ctx.fonts) return;

    // ON A WORD. Fleischer's ball lands on the word it is naming and
    // crosses in a hurry, so the one position it never holds is the gap
    // between two — and a moment declared mid-flight photographs exactly
    // that. This lands inside a word's own hold, with the wipe part way
    // through it and the word's first letter at the top of its catch.
    ctx.captureAt(kLeadIn + kLineSeconds * 0.48);

    ctx.engine.add([this, &ticker = ctx.engine] {
      cycle = motion::phase(ticker.elapsed(), loop) * (float)loop;
    });

    ctx.composer.render(
        describe(songFrom(sketch::kit::Document{ctx, "data/airship.json"})));
  }

  /** THE BALL'S WEIGHT from where it is in a hop: `landed` is how far into
   *  its rest it is (0 on touching down), `speed` how fast it is moving,
   *  0 to 1. It lands flat and wide and recovers across the first
   *  `kSquashShare` of its rest, and stretches along its travel in flight,
   *  keeping roughly its area. */
  void weigh(float landed, float speed) {
    const float squash =
        landed < kSquashShare
            ? std::pow(1.0f - landed / kSquashShare, 2.0f) * kSquash
            : 0.0f;
    const float stretch = kStretch * speed;
    ballHeight = (1.0f + stretch) * (1.0f - squash);
    ballWidth = (1.0f - stretch * 0.6f) * (1.0f + squash * 0.8f);
  }

  /** THE READ-BACK, every frame. `beatsOf` resolves against the layout the
   *  last draw produced and the progress the ticker has just moved, so the
   *  ball and the playhead are placed from the same numbers the glyphs are
   *  drawn from — not from a second copy of the cascade's arithmetic that
   *  a nested beat or a recut table would silently invalidate. Both marks
   *  stand on the first letter, so every distance is taken from its edge. */
  void update(double, sketch::SketchContext& ctx) {
    const std::vector<Beat> beats = ctx.composer.beatsOf("sung", 0);
    if (beats.empty()) return;  // nothing laid out yet
    const float origin = beats.front().rect.left();

    // THE PLAYHEAD is the wipe's own coverage, in pixels: every beat
    // contributes its share of the distance to the next one. It is exact
    // however many letters are mid-change at once, which a "which letter is
    // opening now" cursor is not once beats overlap.
    float swept = 0;
    for (size_t index = 0; index < beats.size(); ++index) {
      const float next = index + 1 < beats.size() ? beats[index + 1].rect.left()
                                                  : beats[index].rect.right();
      swept += beats[index].localProgress * (next - beats[index].rect.left());
    }
    playhead = swept;

    const std::vector<SungWord> words = sungWords(beats);
    const auto centre = [&](size_t index) {
      return words[std::min(index, words.size() - 1)].centre() - origin;
    };

    // THE COUNT-IN: the ball falls onto the first word, then bounces there
    // once a beat; its fourth landing is the line's first cue.
    if (cycle.value() < (float)kLeadIn) {
      const float count = cycle.value() / (float)kCountBeat;
      const auto beat = (int)count;
      const float fraction = count - (float)beat;
      ballX = centre(0);
      if (beat == 0) {
        ballY = -kDropHeight * (1.0f - fraction * fraction);
        weigh(1.0f, fraction);
      } else {
        ballY = -kCountHop * 4.0f * fraction * (1.0f - fraction);
        weigh(fraction, std::abs(1.0f - 2.0f * fraction));
      }
      return;
    }

    // THE BALL hops word to word on the same list. Summed word coverage is
    // a continuous front: its whole part is the word being sung, its
    // fraction is how far that word has left to go, and the ball sits still
    // for the first `kBallHold` of it before crossing to the next.
    float front = 0;
    for (const SungWord& word : words) front += word.coverage;
    const auto current = (size_t)front;
    const float within = front - (float)current;
    const float crossing =
        std::clamp((within - kBallHold) / (1.0f - kBallHold), 0.0f, 1.0f);
    ballX =
        centre(current) + crossing * (centre(current + 1) - centre(current));
    ballY = -kHopHeight * 4.0f * crossing * (1.0f - crossing);
    // Past the last word the front stops at the word count and the ball
    // has long since settled there.
    if (current >= words.size())
      weigh(1.0f, 0.0f);
    else if (crossing <= 0.0f)
      weigh(within / kBallHold, 0.0f);
    else
      weigh(1.0f, std::abs(1.0f - 2.0f * crossing));
  }
};

SIGIL_SKETCH(KaraokeWipe, "Study · Type",
             "Fleischer's bouncing ball (1924) and the CD+G wipe (1985) on one "
             "schedule — the point against the boundary")
