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
// the sung colour, and `fx::tint(pale, sung)` multiplies every glyph down
// to the pale colour until its own beat arrives. It is a multiplier rather
// than a colour because that is what a `GlyphMod` carries — every pass the
// glyph's style draws is modulated, so the same track would tint a
// gradient-filled line without knowing it was a gradient.
//
// THE CASCADE IS TWO DEEP AND ITS OUTER LEVEL IS A TABLE. A real disc
// carries a time for every syllable, cut against the recording: a held note
// holds and a fast line races, and nothing evenly spaced sounds like
// singing. `cues()` is that table — one start time per WORD, in the shape
// the tune has — and `then(unit::Cluster)` sweeps the letters of each word
// evenly inside its beat, which is how a syllable's own wipe behaves. The
// table gives the line its uneven shape; the progress window gives it its
// tempo, so the same table sings faster or slower without being recut.
//
// THE BALL IS NOT PART OF THE TEXT AT ALL. It is an ordinary box, and to
// put it over the right letter it has to know where the letters are and how
// far each one's beat has run. Both come back from `Composer::beatsOf`,
// which reports the schedule the track is actually running: one `Beat` per
// letter, carrying that letter's laid-out `rect`, the `unitIndex` of the
// word it belongs to, and its own `localT` right now. The ruler is the same
// list drawn — one tick per beat at the rect the engine placed, taller
// where a new word begins — so its pitch is the cascade's real, uneven
// pitch and not a picture of an even one.
//
// Nothing here restates the cascade's arithmetic, which is the point: a
// nested beat lasts exactly as long as its inner ladder needs, and a cue
// table can be recut between takes. Neither is reproducible in the sketch,
// and neither has to be.
//
// -----------------------------------------------------------------------------
// EDIT THESE FIRST
//   wordCues() — the sung shape, one time per word of kLine1. Type a real
//               line's syllable times here and the caption follows them.
//   kEachMs   — start-to-start between the LETTERS inside one word.
//   kSwitchMs — how long ONE letter takes to change colour. Small is the
//               CD+G hard edge; large is a soft gradient crossing the word.
//   kLineSeconds — how long the whole line takes. The tempo, not the shape.
//   kHopHeight — the ball's arc. Fleischer's is high and slow.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/karaoke_wipe.cpp \
//       --frame /tmp/karaoke_wipe.png --at 1.7
//
//   The hop, frame by frame:  --at 1.20 --frames 10 --fps 12

#include <sigilcompose/core/Material.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kW = 1020.0f;
constexpr float kH = 470.0f;

constexpr SkColor4f kStage = hex(0x0A0A12);
constexpr SkColor4f kBand = hex(0x11111D);
constexpr SkColor4f kSung = hex(0x53E0C4);  // the saturated colour
constexpr SkColor4f kPale = hex(0xF2EFE6);  // the resting line
constexpr SkColor4f kNext = hex(0x585F6E);  // the line to come
constexpr SkColor4f kLabel = hex(0x767E8D);
constexpr SkColor4f kFaint = hex(0x333A47);

const char* kLine1 = "COME TAKE A TRIP IN MY AIRSHIP";
const char* kLine2 = "COME TAKE A SAIL AMONG THE STARS";

constexpr float kLyricSize = 46.0f;
constexpr float kTrack = 2.0f;

// ---- the schedule ---------------------------------------------------------
/** ONE TIME PER WORD of kLine1, in ms — the shape the tune has, not a
 *  spacing. "COME TAKE A" runs on, "A" is short, "AIRSHIP" is held. */
std::vector<float> wordCues() {
  return {0.0f, 430.0f, 900.0f, 1120.0f, 1560.0f, 1900.0f, 2210.0f};
}
constexpr float kEachMs = 46.0f;      // start-to-start, per letter of a word
constexpr float kSwitchMs = 190.0f;   // one letter's own change
constexpr double kLeadIn = 0.55;      // silence before the line starts
constexpr double kLineSeconds = 2.9;  // the whole line, tempo only
constexpr double kHold = 1.40;        // the sung line held before the loop cuts
constexpr float kHopHeight = 44.0f;   // the 1924 arc
/** How much of a word's own beat the ball SITS on it before leaving. The
 *  1924 ball rests on the word it is naming and crosses in a hurry; a value
 *  of 0 would make it slide continuously and name nothing. */
constexpr float kBallHold = 0.62f;

/** THE CASCADE: the sung times per word, the letters swept inside each. */
Stagger wipeCascade() {
  Stagger cascade = stagger(unit::Word, cues(wordCues()));
  cascade.then(unit::Cluster, {.eachMs = kEachMs, .durationMs = kSwitchMs});
  return cascade;
}

/** One word's own share of the wipe, 0 to 1: the mean of its letters'
 *  beats. Summed across the line it gives a continuous word front, whose
 *  whole part is the word the ball is on and whose fraction is how far it
 *  has to travel to the next. */
std::vector<float> wordCoverage(const std::vector<Beat>& schedule) {
  std::vector<float> total, count;
  for (const Beat& beat : schedule) {
    if (beat.unitIndex >= total.size()) {
      total.resize(beat.unitIndex + 1, 0.0f);
      count.resize(beat.unitIndex + 1, 0.0f);
    }
    total[beat.unitIndex] += beat.localT;
    count[beat.unitIndex] += 1.0f;
  }
  for (size_t w = 0; w < total.size(); ++w)
    total[w] = count[w] > 0 ? total[w] / count[w] : 0.0f;
  return total;
}

/** Where each word sits, as the union of its letters' laid-out rects. */
std::vector<SkRect> wordRects(const std::vector<Beat>& schedule) {
  std::vector<SkRect> out;
  for (const Beat& beat : schedule) {
    if (beat.unitIndex >= out.size()) out.resize(beat.unitIndex + 1);
    if (out[beat.unitIndex].isEmpty())
      out[beat.unitIndex] = beat.rect;
    else
      out[beat.unitIndex].join(beat.rect);
  }
  return out;
}

}  // namespace

// ===========================================================================

struct KaraokeWipe : sketch::Sketch {
  choreograph::Output<float> cycle{0};     // seconds into one pass, wrapping
  choreograph::Output<float> ballX{0};     // px along the line
  choreograph::Output<float> ballY{0};     // px above the ball's rest
  choreograph::Output<float> playhead{0};  // px, the ruler's cursor

  sk_sp<SkTypeface> face;
  sigil::weave::TextStyle lyric;
  /** THE SCHEDULE, read back after the first layout — one beat per letter,
   *  in the line's own coordinates. Everything that points AT the type is
   *  placed from this and from nothing else. */
  std::vector<Beat> schedule;
  std::vector<SkRect> words;  // per word, from the same list
  float lineWidth = 0;
  double loop = kLeadIn + kLineSeconds + kHold;

  [[nodiscard]] Element lyricLine() const {
    // The line is SET IN kSung and the tint multiplies it down to kPale
    // until each letter's own beat arrives — the inversion a colour
    // multiplier forces, which is why the effect takes the two colours in
    // time order and does the division itself.
    return text(toU8(kLine1), lyric)
        .key("line1")
        .fx({.effect = fx::tint(kPale, kSung),
             .stagger = wipeCascade(),
             .progress = bind(&cycle).window((float)kLeadIn,
                                             (float)(kLeadIn + kLineSeconds))});
  }

  /** The ruler: one tick per BEAT, at the rect the engine placed it in, and
   *  a taller tick where a new word begins. It is the schedule, drawn —
   *  which is the only way to see that the pitch is uneven, that a held
   *  word is held, and that the letters inside a word run at their own
   *  even step. */
  [[nodiscard]] Element ruler() const {
    Element strip = box().width(pct(100)).height(22);
    uint32_t previousWord = ~0u;
    for (size_t i = 0; i < schedule.size(); ++i) {
      const bool starts = schedule[i].unitIndex != previousWord;
      previousWord = schedule[i].unitIndex;
      strip.child(box()
                      .key("t" + std::to_string(i))
                      .left(schedule[i].rect.left())
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

  [[nodiscard]] Element describe() const {
    const sigil::weave::TextStyle small =
        type({.face = face, .size = 11.5f, .color = kLabel, .track = 2.4f});
    const sigil::weave::TextStyle note =
        type({.face = face, .size = 11.5f, .color = kFaint, .track = 0.5f});

    Element stage =
        box()
            .column()
            .gap(0)
            // The ball's lane. It is a sibling of the line, not a part of
            // it: nothing in a text node can carry a mark that is not a
            // glyph, so anything pointing AT the type lives beside it and
            // is placed from the schedule the type is running.
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
            .child(ruler().margin(0, 12, 0, 0))
            .child(text(toU8(kLine2), type({.face = face,
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
                               type({.face = face,
                                     .size = 11.5f,
                                     .color = kNext,
                                     .track = 2.4f}))))
        .child(box().height(1).fill(Fill::color(kFaint)))
        .child(box().grow(1))
        .child(box().alignItems(Align::Center).child(std::move(stage)))
        .child(box().grow(1))
        // The numbers are read off the table rather than typed beside it:
        // a caption that can disagree with the schedule it describes is the
        // one thing worse than no caption.
        .child(text(toU8("THE BALL MARKS THE POINT, THE WIPE MARKS THE "
                         "BOUNDARY \xc2\xb7 " +
                         std::to_string(wordCues().size()) + " SUNG TIMES, " +
                         std::to_string((int)kEachMs) +
                         " MS PER LETTER INSIDE A WORD, " +
                         std::to_string((int)kSwitchMs) + " MS TO CHANGE"),
                    note));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kStage);
    if (!ctx.fonts) return;

    face = pickFace({"Avenir Next", "Futura", "Helvetica Neue"}, 600);
    lyric = type(
        {.face = face, .size = kLyricSize, .color = kSung, .track = kTrack});
    schedule.clear();
    words.clear();
    lineWidth = 0;

    // Mid-line: the wipe has crossed four words, the ball is in the air
    // between two, and the ruler shows both.
    ctx.captureAt(kLeadIn + kLineSeconds * 0.55);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      cycle = (float)std::fmod(t, loop);
      return true;
    });

    ctx.composer.render(describe());
  }

  /** THE READ-BACK, every frame. `beatsOf` resolves against the layout the
   *  last draw produced and the progress the ticker has just moved, so the
   *  ball and the playhead are placed from the same numbers the glyphs are
   *  drawn from — not from a second copy of the cascade's arithmetic that
   *  a nested beat or a recut table would silently invalidate. */
  void update(double, sketch::SketchContext& ctx) override {
    const std::vector<Beat> now = ctx.composer.beatsOf("line1", 0);
    if (now.empty()) return;  // nothing laid out yet
    const std::optional<SkRect> line = ctx.composer.bounds("line1");
    if (!line) return;

    // The rects come back in the composer's space; the marks beside the
    // line are placed in the line's. One subtraction, taken from the same
    // query family.
    if (schedule.size() != now.size()) {
      schedule = now;
      for (Beat& beat : schedule) beat.rect.offset(-line->left(), -line->top());
      words = wordRects(schedule);
      lineWidth = line->width();
      ctx.composer.render(describe());  // the ruler now knows its ticks
      return;
    }
    for (size_t i = 0; i < schedule.size(); ++i)
      schedule[i].localT = now[i].localT;

    // THE PLAYHEAD is the wipe's own coverage, in pixels: every beat
    // contributes its share of the distance to the next one. It is exact
    // however many letters are mid-change at once, which a "which letter is
    // opening now" cursor is not once beats overlap.
    float x = 0;
    for (size_t i = 0; i < schedule.size(); ++i) {
      const float next =
          i + 1 < schedule.size() ? schedule[i + 1].rect.left() : lineWidth;
      x += schedule[i].localT * (next - schedule[i].rect.left());
    }
    playhead = x;

    // THE BALL hops word to word on the same list. Summed word coverage is
    // a continuous front: its whole part is the word being sung, its
    // fraction is how far that word has left to go, and the ball sits still
    // for the first `kBallHold` of it before crossing to the next.
    const std::vector<float> coverage = wordCoverage(schedule);
    float front = 0;
    for (const float share : coverage) front += share;
    const auto centre = [this](size_t w) {
      const size_t clamped = std::min(w, words.size() - 1);
      return words[clamped].centerX() - 7.5f;
    };
    const auto w = (size_t)std::floor(front);
    const float within = front - (float)w;
    const float p =
        std::clamp((within - kBallHold) / (1.0f - kBallHold), 0.0f, 1.0f);
    ballX = centre(w) + p * (centre(w + 1) - centre(w));
    ballY = -kHopHeight * 4.0f * p * (1.0f - p);
  }
};

SIGIL_SKETCH(KaraokeWipe, "Study \xc2\xb7 Type",
             "Fleischer's bouncing ball (1924) and the CD+G wipe (1985) on one "
             "schedule \xe2\x80\x94 the point against the boundary")
