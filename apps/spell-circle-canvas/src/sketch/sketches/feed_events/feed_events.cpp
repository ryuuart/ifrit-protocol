/** @file
 * feed_events — events become motions, state stays state.
 *
 * Messages arrive on one door and are read as JSON, and a `kind` says
 * which reader each is for. That one word splits them into two things
 * that behave nothing alike, which is the whole subject of this sketch.
 *
 * STATE IS WHAT IS TRUE NOW. A `Wind` says what the wind is, and it
 * stays that until another one says otherwise; a `Palette` says what
 * the colours are. A handler writes them down and the scene reads them
 * for as long as they stand. A reader that fell behind wants the newest
 * and not every step that led to it.
 *
 * AN EVENT IS OVER THE MOMENT IT ARRIVES. A `Gust` is not a value the
 * scene can hold: what it leaves behind is a MOTION, started on the
 * ticker, that crosses the canvas and fades on its own. Nothing has to
 * arrive for it to end, and four of them may be in the air at once.
 * Reading it as state would leave the sky permanently gusting.
 *
 * AND A THIRD THING: a kind nobody here reads. A `"*"` handler tallies
 * every kind that arrives, so what this scene cannot use is counted and
 * shown rather than dropped in silence — which is what makes a sender
 * that changed its vocabulary visible instead of mysterious. Nothing
 * calls `receive()`: handlers see every message, and the queue behind
 * them is for a reader that would rather drain than register.
 *
 * `events.py` beside this file is both ends of it: it sends the
 * sequence to the port, and it writes the recording a capture replays.
 *
 *     python3 events.py                                # a window moves
 *     python3 events.py --record data/events.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kDoor       the URI the messages arrive on
 *   kRecording  what a capture replays instead of listening
 *   kWaves      how many gusts may be crossing at once
 *   kSpacing    how far apart the bands stand
 */

// TAGS: Data/Sources, Runtime/Resources

#include <choreograph/Choreograph.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Instrument.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;
namespace motion = sigil::motion;
namespace ch = choreograph;

using sigil::draw::Pen;

namespace {

const char* kDoor = "udp://:27072";           // where the messages arrive
const char* kRecording = "data/events.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.04f, 0.04f, 0.09f, 1};
/** A gust is halfway across the canvas by here, which is the frame that
 *  shows an event and a state in the same picture. */
constexpr double kCaptureAt = 3.0;

constexpr float kTop = 72.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

constexpr float kWindEase = 0.5f;  // how long the sky takes to reach a reading
constexpr float kBandAlpha = 0.82f;   // what a colour off the wire is drawn at
constexpr float kWaveWidth = 300.0f;  // how wide a gust's crossing front is
constexpr int kWaveSlices = 16;       // the steps its soft tail is built from
constexpr float kWaveInk = 0.115f;    // what one of those steps is laid at
constexpr float kWaveFall = 1.0f;     // how long it crosses when none was given

/** HOW MANY GUSTS MAY BE CROSSING AT ONCE. An event leaves a motion
 *  behind, and a motion needs somewhere to be written: these are those
 *  places, taken in turn, so a fifth gust overtakes the oldest still in
 *  the air rather than being refused or growing the list without end. */
constexpr size_t kWaves = 4;

/** The colours the bands are tinted from, in turn. */
constexpr size_t kTints = 3;
using Palette = std::array<material::Color, kTints>;

constexpr Palette kOpeningPalette = {{{0.44f, 0.60f, 0.90f, kBandAlpha},
                                      {0.54f, 0.50f, 0.86f, kBandAlpha},
                                      {0.38f, 0.72f, 0.84f, kBandAlpha}}};

/** A BAND'S OWN SHAPE, which no message carries: how thick it is, how
 *  fast it runs against or with the wind, and how far it breathes. */
struct Band {
  float height;
  float speed;
  float wobble;
};

constexpr std::array<Band, 6> kBands = {{{50, -16, 15},
                                         {32, 22, 10},
                                         {62, -10, 19},
                                         {38, 17, 12},
                                         {54, -24, 17},
                                         {28, 12, 8}}};

/** ONE GUST IN THE AIR: how far across it has travelled, and how hard
 *  it was. The travel is a motion the ticker owns; the strength is a
 *  plain number, because it does not change once the gust is over. */
struct Wave {
  ch::Output<float> travel{1.0f};
  float strength = 0;
};

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Every kind that has arrived is in it, in the order they first did,
 *  so a vocabulary this scene does not read still shows up by name. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  std::vector<std::pair<std::string, uint64_t>> kinds;
  std::string address;
  std::string trouble;
  bool operator==(const Reading&) const = default;
};

/** One row of the readout: what it is called, and what it answers. */
compose::Element row(const std::string& name, const std::string& answer) {
  return compose::text(
      compose::kit::formatted("%-11s %s", name.c_str(), answer.c_str()));
}

}  // namespace

namespace {

struct FeedEvents {
  /** THE DOOR, read as JSON because the scheme is not osc. The handlers
   *  below run on the hub's dispatch, which is the call the host
   *  already makes once a frame, so a message that arrived is folded in
   *  before the frame it belongs to is drawn. */
  data::Connection door;
  /** The session's ticker, which outlives this sketch. A handler runs
   *  on the dispatch rather than inside a describe, so it cannot hold
   *  the per-frame context the ticker is otherwise reached through. */
  motion::Ticker* ticker = nullptr;

  /** THE STATE: what the wind is, and what the colours are. */
  ch::Output<float> wind{0};
  Palette palette = kOpeningPalette;

  /** THE EVENTS: what is left of the gusts that have happened. */
  std::array<Wave, kWaves> waves;
  size_t nextWave = 0;

  /** How far the sky has travelled, in px. Integrated rather than read
   *  as a speed times the clock, because a speed that changes would
   *  rewrite where the sky has already been. */
  double drift = 0;
  /** The scene time the last frame stood at, for the step between. */
  double seconds = 0;

  /** Every kind that has arrived, first-seen order, with how many of
   *  each — the three this scene acts on and the ones it does not,
   *  counted the same way. */
  std::vector<std::pair<std::string, uint64_t>> kinds;
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.ticker;

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window listens
    // on the port. Mounting the URI onto the file is the whole of the
    // difference, and the mount stands first because a feed is made
    // once per URI and every later ask answers that same one.
    if (ctx.deterministic) hub.mount(kDoor, hub.resolve(ctx.local(kRecording)));

    wind = 0.0f;
    palette = kOpeningPalette;
    for (Wave& wave : waves) {
      wave.travel = 1.0f;
      wave.strength = 0;
    }
    nextWave = 0;
    drift = 0;
    seconds = 0;
    kinds.clear();
    shown = {};

    door = data::Connection(hub, kDoor);
    door.on("Wind", [this](const data::Json& message) { blow(message); });
    door.on("Gust", [this](const data::Json& message) { gust(message); });
    door.on("Palette", [this](const data::Json& message) { colours(message); });
    // LAST, AND OVER EVERYTHING: several handlers may name one message
    // and each runs once, in the order they were registered, so this
    // one tallies every kind after the three above have acted on theirs.
    door.on("*", [this](const data::Json& message) { tally(message); });

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += (double)wind() * step;
    // The data path: the sky and the waves move without being described
    // again, because the pen reads them on every frame. What is
    // described again is the readout, and only when a figure changed.
    if (read()) describe(ctx);
  }

  /** STATE: the wind is what the message says, reached over kWindEase
   *  so a sender stepping in whole numbers still drifts smoothly. */
  void blow(const data::Json& message) {
    if (!ticker) return;
    ticker->timeline().apply(&wind).then<ch::RampTo>(
        (float)message["value"].number(), kWindEase, &ch::easeOutQuad);
  }

  /** AN EVENT: the gust is over, and what is left of it is a wave
   *  crossing the canvas over the seconds the message gave it. It takes
   *  the next place in turn, so several may be in the air at once. */
  void gust(const data::Json& message) {
    if (!ticker) return;
    Wave& wave = waves[nextWave];
    nextWave = (nextWave + 1) % kWaves;
    wave.strength = (float)message["strength"].number();
    wave.travel = 0.0f;
    ticker->timeline()
        .apply(&wave.travel)
        .then<ch::RampTo>(1.0f, (float)message["seconds"].number(kWaveFall),
                          &ch::easeOutQuad);
  }

  /** STATE: the colours the bands are tinted from. A message carrying
   *  fewer leaves the ones it did not reach where they were. */
  void colours(const data::Json& message) {
    const std::span<const data::Json> written = message["colors"].items();
    for (size_t index = 0; index != kTints && index != written.size(); ++index)
      palette[index] = {(float)written[index][0].number(),
                        (float)written[index][1].number(),
                        (float)written[index][2].number(),
                        (float)written[index][3].number(kBandAlpha)};
  }

  /** EVERY KIND, COUNTED — the ones acted on above and the ones nobody
   *  here reads. A message with no kind at all is counted under the
   *  name this readout gives it, because a message that named nothing
   *  is still a message that arrived. */
  void tally(const data::Json& message) {
    const std::string_view named = message["kind"].text("(unnamed)");
    for (std::pair<std::string, uint64_t>& counted : kinds)
      if (counted.first == named) {
        ++counted.second;
        return;
      }
    kinds.emplace_back(named, 1);
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Reading now{.generation = door.generation(),
                .undecodable = door.undecodable(),
                .kinds = kinds,
                .address = door.address(),
                .trouble = door.error()};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where
    // the theme in force is no longer this page's, so the one colour a
    // wave is drawn in is read here and carried in by value.
    const material::Color crest = sketch::kit::theme().palette.figure;
    compose::Element picture =
        compose::stack()
            .width(kCanvas.width())
            .height(kCanvas.height())
            .children({compose::pen("feed_events.sky", [this, crest](Pen& pen) {
                         sky(pen);
                         crossings(pen, crest);
                       }).cover()});
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "State meets an event.",
                  .subtitle = "UDP / EVENTS  /  A steady signal, with a moment "
                              "that travels through it.",
                  .footer = "Run events.py beside this sketch to send data  ·  "
                            "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "State sets the wind and palette. Events send a crest across "
                 "the field."},
        std::move(picture).fill(kGround), readout()));
  }

  /** THE STATE, DRAWN: bands carried on the drift the wind has built,
   *  each breathing by its own wobble and tinted from the palette in
   *  turn. A band's OWN speed is a constant and so is read straight off
   *  the clock; only the wind, which changes, has to be integrated. */
  void sky(Pen& pen) {
    pen.noStroke();
    const float period = kSegment + kGap;
    size_t index = 0;
    for (const Band& band : kBands) {
      pen.fill(palette[index % kTints]);
      const double travelled = drift + (double)band.speed * seconds;
      const float phase =
          (float)(std::fmod(std::fmod(travelled, period) + period, period));
      const float y =
          kTop + kSpacing * (float)index +
          band.wobble * std::sin((float)seconds * 0.7f + (float)index);
      for (float x = phase - period; x < pen.width; x += period)
        pen.rect(x, y, kSegment, band.height, band.height * 0.5f);
      ++index;
    }
  }

  /** THE EVENTS, DRAWN: each gust still in the air as a bright front
   *  crossing left to right, brightest at the moment it happened and
   *  gone by the time it leaves — so what is on the canvas is the
   *  history of the gusts, which is all an event ever leaves. */
  void crossings(Pen& pen, material::Color crest) {
    pen.noStroke();
    for (const Wave& wave : waves) {
      const float across = wave.travel();
      if (across >= 1.0f || wave.strength <= 0) continue;
      const float lead = across * (pen.width + kWaveWidth);
      const float fading = wave.strength * (1.0f - across);
      const float slice = kWaveWidth / (float)kWaveSlices;
      material::Color ink = crest;
      ink.a = fading * kWaveInk;
      pen.fill(ink);
      // EACH STEP STARTS FURTHER FORWARD AND ALL OF THEM END AT THE
      // FRONT, so what builds the tail is the laying of one over
      // another rather than a ramp cut into steps — every edge but the
      // front's is the start of another wash, and nothing seams.
      for (int step = 0; step != kWaveSlices; ++step) {
        const float from = lead - kWaveWidth + (float)step * slice;
        pen.rect(from, 0, kWaveWidth - (float)step * slice, pen.height);
      }
    }
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: how many
   *  messages have arrived, how many were no JSON document at all, one
   *  line per kind with how many of each, and where the door is — or
   *  the sentence saying why there is none. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("door", kDoor),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable))};
    if (shown.kinds.empty())
      lines.push_back(row("kinds", "nothing has arrived"));
    for (const std::pair<std::string, uint64_t>& counted : shown.kinds)
      lines.push_back(row(
          counted.first,
          compose::kit::formatted(
              "%llu%s", (unsigned long long)counted.second,
              handled(counted.first) ? "" : "   · no reader for this kind")));
    if (!shown.trouble.empty())
      lines.push_back(row("error", shown.trouble));
    else
      lines.push_back(
          row("address", shown.address.empty() ? "-" : shown.address));
    return compose::box()
        .column()
        .gap(12)
        .font(look.font({.size = 12, .mono = true}))
        .ink(look.palette.ink)
        .children(std::move(lines));
  }

  /** Whether a kind is one this scene has a handler for. */
  static bool handled(std::string_view kind) {
    return kind == "Wind" || kind == "Gust" || kind == "Palette";
  }
};

}  // namespace

SIGIL_SKETCH(FeedEvents, "Data",
             "One door, two kinds of message: state a handler eases into and "
             "holds, events a handler turns into motions that cross and fade, "
             "and a tally of every kind that arrived — including the ones "
             "nothing here reads.")
