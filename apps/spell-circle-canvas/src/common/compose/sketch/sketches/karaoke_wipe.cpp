// karaoke_wipe.cpp — PATTERN: the karaoke caption. Two devices that solve
// the same problem sixty years apart, put on one timeline.
// =============================================================================
// THE PATTERN, AND WHERE IT COMES FROM
//
//   1924 — THE BOUNCING BALL. Max Fleischer's Song Car-Tunes (Inkwell
//          Studios / Red Seal) put the words of a song on screen with a
//          white ball that HOPS from word to word, landing on each one as
//          it is sung, so a cinema audience knows where it is in the line.
//          The series began in 1924; "Come Take a Trip in My Airship" (Ren
//          Shields and George Evans, 1904 — the lyric below, and long out
//          of copyright) is among its first titles.
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
// them together off one schedule: the wipe crosses glyph by glyph, the ball
// hops word by word, and the ruler underneath draws the schedule itself.
//
// -----------------------------------------------------------------------------
// HOW EACH IS SPELLED
//
// THE WIPE IS A COLOUR MULTIPLIER ON A CASCADE. The line is set ONCE, in
// the sung colour, and a track multiplies every glyph down to the pale
// colour until its own beat arrives:
//
//     colorMul = lerp(pale/sung, 1, smoothstep(t))
//
// which is the whole effect. It is a multiplier rather than a colour
// because that is what a `GlyphMod` carries — every pass the glyph's style
// draws is modulated, so the same track would tint a gradient-filled line
// without knowing it was a gradient. The cascade is FLAT (one beat per
// glyph, `eachMs` apart) rather than nested, for a reason the ball needs:
// see below.
//
// THE BALL IS NOT PART OF THE TEXT AT ALL. It is an ordinary box, and to
// put it over the right letter this sketch has to know where the letters
// are and when each one's beat starts — which means reproducing, in its own
// arithmetic, two things the track engine already knows:
//
//   * WHERE. `measureRun` shapes the line once and hands back per-glyph
//     advances, whose prefix sums are the pen positions. That is the
//     supported door and it works. (Note a space is not a glyph: it rides
//     the advance of the letter before it, so a glyph index is an index
//     among NON-SPACE characters.)
//   * WHEN. Nothing reports the schedule. A flat cascade is reproducible in
//     one line — glyph i starts at `i * eachMs` — and a NESTED one is not,
//     because a nested beat lasts exactly as long as its inner cascade
//     needs and that length is the engine's business. So the wipe here is
//     flat, and the study is honest about why: the prettier cascade would
//     have made the ball impossible to place.
//
// -----------------------------------------------------------------------------
// WHAT IS NOT REPRODUCED, AND SHOULD BE SAID
//
// REAL KARAOKE TIMING IS NOT UNIFORM. A CD+G disc carries a time for every
// syllable, cut against the recording; a held note holds and a fast line
// races. A `Stagger` is one spacing and one duration for the whole line,
// with an easing over the START TIMES and nothing else — there is no way to
// hand a track a table of per-unit times. The wipe below is therefore
// EVENLY spaced, which is the one thing a real caption never is.
//
// EDIT THESE FIRST
//   kEachMs   — start-to-start between letters. Raise it and the wipe
//               crawls; drop it to 0 and the line lights all at once.
//   kSwitchMs — how long ONE letter takes to change colour. Small is the
//               CD+G hard edge; large is a soft gradient crossing the word.
//   kHopHeight — the ball's arc. Fleischer's is high and slow.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/karaoke_wipe.cpp \
//       --frame /tmp/karaoke_wipe.png --at 1.7
//
//   The hop, frame by frame:  --at 1.20 --frames 10 --fps 12

#include <sigilcompose/Material.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilsketch/Sketch.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr float kW = 1020.0f;
constexpr float kH = 470.0f;

constexpr SkColor4f kStage = studio::hex(0x0A0A12);
constexpr SkColor4f kBand = studio::hex(0x11111D);
constexpr SkColor4f kSung = studio::hex(0x53E0C4);  // the saturated colour
constexpr SkColor4f kPale = studio::hex(0xF2EFE6);  // the resting line
constexpr SkColor4f kNext = studio::hex(0x585F6E);  // the line to come
constexpr SkColor4f kLabel = studio::hex(0x767E8D);
constexpr SkColor4f kFaint = studio::hex(0x333A47);

const char* kLine1 = "COME TAKE A TRIP IN MY AIRSHIP";
const char* kLine2 = "COME TAKE A SAIL AMONG THE STARS";

constexpr float kLyricSize = 46.0f;
constexpr float kTrack = 2.0f;

// ---- the schedule ---------------------------------------------------------
constexpr float kEachMs = 78.0f;     // start-to-start, per glyph
constexpr float kSwitchMs = 190.0f;  // one glyph's own change
constexpr double kLeadIn = 0.55;     // silence before the line starts
constexpr double kHold = 1.90;       // the sung line held before the loop cuts
constexpr float kHopHeight = 44.0f;  // the 1924 arc

/** The pale line as a MULTIPLIER of the sung line: what the wipe divides
 *  the colour down to before a glyph's beat. A GlyphMod carries a
 *  multiplier and not a colour, so the resting appearance is expressed as
 *  a ratio and the element is set in the destination. */
constexpr SkColor4f wipeFloor() {
  return {kPale.fR / kSung.fR, kPale.fG / kSung.fG, kPale.fB / kSung.fB, 1.0f};
}

/** THE WIPE. One glyph, one number: how far its own beat has run.
 *
 *  `smoothstep` rather than a step because a hard cut on a 46 px letter
 *  shows as a flicker at any frame rate — the edge wants to be one letter
 *  wide, which is what a short `durationMs` buys. The colour multiplier is
 *  quantized before it reaches the draw (each distinct value is its own
 *  batch bucket), and at this size the ladder does not show, so the track
 *  is left to snap. */
TextEffect wipe() {
  const SkColor4f floor = wipeFloor();
  return fx::effect("karaokeWipe",
                    [floor](const GlyphInfo&, float t, Rng&) {
                      const float e = t * t * (3.0f - 2.0f * t);
                      GlyphMod m;
                      m.colorMul = {floor.fR + (1.0f - floor.fR) * e,
                                    floor.fG + (1.0f - floor.fG) * e,
                                    floor.fB + (1.0f - floor.fB) * e, 1.0f};
                      return m;
                    },
                    0.0f, {floor.fR, floor.fG, floor.fB});
}

/** Where each glyph of @p utf8 starts, in px from the run's own origin,
 *  plus one past-the-end entry — the prefix sums of `measureRun`. */
std::vector<float> penPositions(const char* utf8,
                                const sigil::weave::TextStyle& style,
                                sigil::weave::FontContext& fonts) {
  const std::vector<float> advances = measureRun(toU8(utf8), style, fonts);
  std::vector<float> pens;
  pens.reserve(advances.size() + 1);
  float x = 0;
  for (const float a : advances) {
    pens.push_back(x);
    x += a;
  }
  pens.push_back(x);
  return pens;
}

}  // namespace

// ===========================================================================

struct KaraokeWipe : sigil::compose::sketch::Sketch {
  choreograph::Output<float> cycle{0};     // seconds into one pass, wrapping
  choreograph::Output<float> ballX{0};     // px along the line
  choreograph::Output<float> ballY{0};     // px above the ball's rest
  choreograph::Output<float> playhead{0};  // px, the ruler's cursor

  sk_sp<SkTypeface> face;
  sigil::weave::TextStyle lyric;
  std::vector<float> pens;     // per glyph, plus one past the end
  std::vector<int> wordFirst;  // glyph index of each word's first letter
  std::vector<int> wordLast;   // glyph index of each word's last letter
  int glyphs = 0;
  double lineSeconds = 0;  // the whole cascade, in seconds
  double loop = 0;

  /** The cascade's own arithmetic, restated. A flat stagger spans
   *  `durationMs + eachMs·(N−1)` of virtual time and unit i starts at
   *  `i · eachMs`, so this is the only line in the sketch that has to agree
   *  with the engine — and the only reason the wipe is not nested. */
  [[nodiscard]] double glyphStart(int i) const {
    return kLeadIn + (double)i * kEachMs / 1000.0;
  }

  [[nodiscard]] Element lyricLine() {
    return text(toU8(kLine1), lyric)
        .key("line1")
        .fx({.effect = wipe(),
             .stagger = {.eachMs = kEachMs, .durationMs = kSwitchMs},
             .progress = bind(&cycle).window((float)kLeadIn,
                                             (float)(kLeadIn + lineSeconds))});
  }

  /** The ruler: one tick per glyph at the pitch the cascade actually beats,
   *  a taller tick where a word begins, and the playhead riding the same
   *  number the ball does. It is the schedule, drawn — which is the only
   *  way to see that the spacing is uniform and that a real caption's would
   *  not be. */
  [[nodiscard]] Element ruler(float width) {
    Element strip = box().width(width).height(22);
    for (int i = 0; i < glyphs; ++i) {
      const bool starts =
          std::find(wordFirst.begin(), wordFirst.end(), i) != wordFirst.end();
      strip.child(box()
                      .key("t" + std::to_string(i))
                      .left(pens[(size_t)i])
                      .top(starts ? 0.0f : 7.0f)
                      .width(1)
                      .height(starts ? 15.0f : 8.0f)
                      .fill(Fill::color(starts ? kLabel : kFaint)));
    }
    strip.child(box()
                    .key("playhead")
                    .left(0)
                    .top(0)
                    .width(2)
                    .height(20)
                    .fill(Fill::color(kSung))
                    .translateX(&playhead));
    return strip;
  }

  [[nodiscard]] Element describe() {
    const float lineW = pens.back();
    const sigil::weave::TextStyle small = studio::type(
        {.face = face, .size = 11.5f, .color = kLabel, .track = 2.4f});
    const sigil::weave::TextStyle note = studio::type(
        {.face = face, .size = 11.5f, .color = kFaint, .track = 0.5f});

    Element stage =
        box()
            .column()
            .width(lineW)
            .gap(0)
            // The ball's lane. It is a sibling of the line, not a part of
            // it: nothing in a text node can carry a mark that is not a
            // glyph, so anything pointing AT the type lives beside it and
            // is placed from the same numbers.
            .child(box()
                       .width(pct(100))
                       .height(kHopHeight + 18.0f)
                       .child(box()
                                  .key("ball")
                                  .left(0)
                                  .top(kHopHeight)
                                  .width(15)
                                  .height(15)
                                  .corners({8})
                                  .fill(Fill::color(kPale))
                                  .translateX(&ballX)
                                  .translateY(&ballY)))
            .child(lyricLine())
            .child(ruler(lineW).margin(0, 12, 0, 0))
            .child(text(toU8(kLine2), studio::type({.face = face,
                                                    .size = kLyricSize * 0.78f,
                                                    .color = kNext,
                                                    .track = kTrack}))
                       .key("line2")
                       .margin(0, 22, 0, 0));

    return box()
        .column()
        .padding(46, 38)
        .gap(26)
        .fill(Material::linear({0, 0}, {0, kH},
                               {{0.0f, kStage}, {0.5f, kBand}, {1.0f, kStage}}))
        .child(box()
                   .row()
                   .alignItems(Align::End)
                   .child(text(toU8("FOLLOW THE BOUNCING BALL"), small).grow(1))
                   .child(text(toU8("FLEISCHER 1924 \xc2\xb7 CD+G 1985"),
                               studio::type({.face = face,
                                             .size = 11.5f,
                                             .color = kNext,
                                             .track = 2.4f}))))
        .child(box().height(1).fill(Fill::color(kFaint)))
        .child(box().grow(1))
        .child(box().alignItems(Align::Center).child(std::move(stage)))
        .child(box().grow(1))
        // The number is read off the constant rather than typed beside
        // it: a caption that can disagree with the schedule it describes is
        // the one thing worse than no caption.
        .child(text(toU8("THE BALL MARKS THE POINT, THE WIPE MARKS THE "
                         "BOUNDARY \xc2\xb7 ONE SCHEDULE, " +
                         std::to_string((int)kEachMs) + " MS PER LETTER, " +
                         std::to_string((int)kSwitchMs) + " MS TO CHANGE"),
                    note));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kStage);
    if (!ctx.fonts) return;

    face = studio::pickFace({"Avenir Next", "Futura", "Helvetica Neue"}, 600);
    lyric = studio::type(
        {.face = face, .size = kLyricSize, .color = kSung, .track = kTrack});
    pens = penPositions(kLine1, lyric, *ctx.fonts);
    glyphs = (int)pens.size() - 1;

    // A space is not a glyph — it rides the advance of the letter before
    // it — so a word's bounds are indices among the NON-SPACE characters.
    {
      const std::string s = kLine1;
      int g = 0;
      bool open = false;
      for (const char c : s) {
        if (c == ' ') {
          open = false;
          continue;
        }
        if (!open) {
          wordFirst.push_back(g);
          wordLast.push_back(g);
          open = true;
        } else {
          wordLast.back() = g;
        }
        ++g;
      }
    }

    lineSeconds = (double)(kSwitchMs + kEachMs * (float)(glyphs - 1)) / 1000.0;
    loop = kLeadIn + lineSeconds + kHold;
    // Mid-line: the wipe has crossed four words, the ball is in the air
    // between two, and the ruler shows both.
    ctx.captureAt(kLeadIn + lineSeconds * 0.55);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const double c = std::fmod(t, loop);
      cycle = (float)c;

      // The playhead reads the cascade's own front: the glyph whose beat is
      // opening right now, interpolated between its neighbours' pens.
      const double f =
          std::clamp((c - kLeadIn) * 1000.0 / kEachMs, 0.0, (double)glyphs);
      const int i = std::min((int)f, glyphs - 1);
      const float u = (float)(f - (double)i);
      playhead = pens[(size_t)i] + u * (pens[(size_t)i + 1] - pens[(size_t)i]);

      // The ball hops WORD to WORD over the same schedule: it leaves one
      // word's centre when that word's last letter has lit and lands on the
      // next word's centre as its first letter does.
      const size_t words = wordFirst.size();
      auto centre = [&](size_t w) {
        const int a = wordFirst[w], b = wordLast[w];
        return 0.5f * (pens[(size_t)a] + pens[(size_t)b + 1]) - 7.5f;
      };
      size_t w = 0;
      while (w + 1 < words && c >= glyphStart(wordFirst[w + 1])) ++w;
      if (w + 1 >= words) {
        ballX = centre(words - 1);
        ballY = 0;
      } else {
        const double a = glyphStart(wordLast[w]);
        const double b = glyphStart(wordFirst[w + 1]);
        const float p =
            (float)std::clamp((c - a) / std::max(b - a, 1.0e-3), 0.0, 1.0);
        ballX = centre(w) + p * (centre(w + 1) - centre(w));
        ballY = -kHopHeight * 4.0f * p * (1.0f - p);
      }
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(KaraokeWipe)
