/** @file
 * osc_desk — a desk in the room, over OSC, both ways.
 *
 * A desk is whatever is standing beside the screen sending OSC:
 * TouchDesigner, Max, a control surface, a phone with faders on it.
 * This sketch is the other end of that cable. A `data::Connection` is
 * the door one floor up from a feed — the bytes are read by the scheme
 * the URI names, so an `osc://` door hands a reader each message as a
 * value and never as a packet — and three handlers registered on it are
 * the whole of the wiring. Nothing here parses anything.
 *
 * THE THREE THINGS A DESK SAYS, and what each one is. `/sky/wind` is
 * STATE: a fader reading, true until the next one, which the sky eases
 * to. `/sky/gust` is an EVENT: it happened, it is over, and what is left
 * of it is a motion started on the ticker that rises and falls away by
 * itself. `/sky/palette` is state again, nine floats that are three
 * colours. The sky they move is the one schema_scene draws beside this
 * file: bands crossing the canvas, the wind they drift on, the palette
 * they are tinted from.
 *
 * ONE DOOR, ANSWERED. `osc://:27070` binds a port and takes messages
 * from whoever writes to it, which means it holds no one peer to send
 * to — but every message names the sender it came from, and `reply()`
 * answers that one. So the eased wind goes back to whoever moved the
 * fader, which is what a motorised fader follows, and it goes as the
 * answer to each `/sky/wind` message: the sky moves every frame while a
 * handler runs only on an arrival, so an arrival is answered with the
 * sky as it stands when it lands. A capture replays a recording, which
 * holds the messages and not who sent them, so there is nobody to
 * answer and a plate is the listening half alone.
 *
 * `desk.py` beside this file is the desk: it sends the traffic to the
 * port and prints the answers, which come back to the one socket it
 * sent them from, and it writes the recording a capture replays.
 *
 *     python3 desk.py                                  # a window moves
 *     python3 desk.py --record data/desk.feed --seconds 6
 *
 * EDIT THESE FIRST
 *   kDesk       the URI the desk's messages arrive on
 *   kRecording  what a capture replays instead of listening
 *   kGustPush   how much drift a gust of full strength adds
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
#include <string>
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

const char* kDesk = "osc://:27070";         // where the desk's messages land
const char* kRecording = "data/desk.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.04f, 0.04f, 0.09f, 1};
/** A gust is in the air and the colours have turned once by here. */
constexpr double kCaptureAt = 2.0;

constexpr float kTop = 72.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

constexpr float kWindEase = 0.45f;  // how long the sky takes to reach a reading
constexpr float kGustRise = 0.18f;  // how long a gust takes to rise
constexpr float kGustFall = 1.0f;   // how long it falls, told nothing else
constexpr float kGustPush = 240.0f;  // px a second a full gust adds
constexpr float kGustLift = 0.15f;   // the alpha a full gust lifts a band by
constexpr float kBandAlpha = 0.82f;  // what a colour off the wire is drawn at
constexpr float kWindSpan = 34.0f;  // the reading the drawn fader reads as full

/** The three colours the bands are tinted from, in turn, and the three
 *  numbers each of them is written as. Nine floats is this address's
 *  whole vocabulary, so both counts are constants here rather than
 *  something read off a message. */
constexpr size_t kTints = 3;
constexpr size_t kChannels = 3;
using Palette = std::array<material::Color, kTints>;

/** WHAT THE SKY IS BEFORE THE DESK HAS SAID ANYTHING. A canvas nobody
 *  is sending to is still a sky, so there is nothing to wait for and no
 *  placeholder to draw. */
constexpr Palette kOpeningPalette = {{{0.48f, 0.66f, 0.90f, kBandAlpha},
                                      {0.62f, 0.56f, 0.86f, kBandAlpha},
                                      {0.42f, 0.76f, 0.80f, kBandAlpha}}};

/** A BAND'S OWN SHAPE, which the desk does not send: how thick it is,
 *  how fast it runs against or with the wind, and how far it breathes.
 *  What a band IS belongs to the sketch; what it DOES belongs to the
 *  desk. */
struct Band {
  float height;
  float speed;
  float wobble;
};

constexpr std::array<Band, 6> kBands = {{{54, -18, 16},
                                         {34, 24, 11},
                                         {66, -11, 20},
                                         {40, 19, 13},
                                         {58, -26, 18},
                                         {30, 13, 9}}};

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Nothing continuous is in here: the wind and the gust move every
 *  frame, and a description rebuilt every frame is a description
 *  nothing can be pruned against. Those two are drawn by the pen. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  uint64_t gusts = 0;
  uint64_t palettes = 0;
  uint64_t replies = 0;
  std::string spoken;
  std::string address;
  std::string trouble;
  bool operator==(const Reading&) const = default;
};

/** One row of the readout: what it is called, and what it answers. */
compose::Element row(const char* name, const std::string& answer) {
  return compose::text(
      compose::kit::formatted("%-11s %s", name, answer.c_str()));
}

}  // namespace

namespace {

struct OscDesk {
  /** THE DESK'S DOOR: the port it sends to, read as OSC because the URI
   *  says osc. The handlers below run on the hub's dispatch, which is
   *  the call the host already makes once a frame — so a message that
   *  arrived is folded in before the frame it belongs to is drawn. */
  data::Connection desk;
  /** The session's ticker, which outlives this sketch. A handler runs
   *  on the dispatch rather than inside a describe, so it cannot hold
   *  the per-frame context the ticker is otherwise reached through. */
  motion::Ticker* ticker = nullptr;

  /** The fader, eased: what the desk last said, reached over kWindEase
   *  rather than jumped to. */
  ch::Output<float> wind{0};
  /** The burst: what a gust rose to, falling away on its own. */
  ch::Output<float> gust{0};
  Palette palette = kOpeningPalette;

  /** How far the sky has travelled, in px. Integrated rather than read
   *  as a speed times the clock, because a speed that changes would
   *  rewrite where the sky has already been. */
  double drift = 0;
  /** The scene time the last frame stood at, for the step between. */
  double seconds = 0;

  uint64_t gusts = 0;
  uint64_t palettes = 0;
  /** How many answers went back to a desk, which is none at all under a
   *  capture: a recording has no sender to answer. */
  uint64_t replies = 0;
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
    if (ctx.deterministic) hub.mount(kDesk, hub.resolve(ctx.local(kRecording)));

    wind = 0.0f;
    gust = 0.0f;
    palette = kOpeningPalette;
    drift = 0;
    seconds = 0;
    gusts = 0;
    palettes = 0;
    replies = 0;
    shown = {};

    desk = data::Connection(hub, kDesk);
    desk.on("/sky/wind", [this](const data::Json& message) { fader(message); });
    desk.on("/sky/gust", [this](const data::Json& message) { burst(message); });
    desk.on("/sky/palette",
            [this](const data::Json& message) { colours(message); });

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += ((double)wind() + (double)gust() * kGustPush) * step;
    // The data path: the sky itself moves without being described
    // again, because the pen reads it on every frame. What is described
    // again is the readout, and only when one of its figures changed.
    if (read()) describe(ctx);
  }

  /** A FADER READING IS A STATE: the sky eases to it, so a desk moving
   *  its fader in steps still drifts smoothly. IT IS ALSO ANSWERED, to
   *  the sender that moved it, with where the easing has REACHED rather
   *  than with the reading that just arrived — that is the wind the
   *  bands are drifting on, and what a motorised fader follows. A
   *  recording has no sender, so under a capture the answer is refused
   *  and nothing goes out. */
  void fader(const data::Json& message) {
    if (desk.reply("/sky/state", data::Json::Array{(double)wind()})) ++replies;
    if (!ticker) return;
    ticker->timeline().apply(&wind).then<ch::RampTo>(
        (float)message["arguments"][0].number(), kWindEase, &ch::easeOutQuad);
  }

  /** A GUST IS AN EVENT: it rises to the strength the desk asked for
   *  and falls away over the seconds the desk gave it, and nothing has
   *  to arrive for it to end. A second gust starts from wherever the
   *  first had fallen to, which is what two gusts in a row look like. */
  void burst(const data::Json& message) {
    ++gusts;
    if (!ticker) return;
    const float strength = (float)message["arguments"][0].number();
    const float falls = (float)message["arguments"][1].number(kGustFall);
    ticker->timeline()
        .apply(&gust)
        .then<ch::RampTo>(strength, kGustRise, &ch::easeOutQuad)
        .then<ch::RampTo>(0.0f, std::max(falls, kGustRise), &ch::easeInQuad);
  }

  /** NINE FLOATS ARE THREE COLOURS. Where one colour stops and the next
   *  begins is this address's own rule, so it is read here; a message
   *  carrying fewer leaves the colours it did not reach where they
   *  were. */
  void colours(const data::Json& message) {
    const data::Json& arguments = message["arguments"];
    for (size_t index = 0; index != kTints; ++index) {
      const size_t at = index * kChannels;
      if (arguments[at + 2].null()) break;
      palette[index] = {(float)arguments[at].number(),
                        (float)arguments[at + 1].number(),
                        (float)arguments[at + 2].number(), kBandAlpha};
    }
    ++palettes;
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Reading now{.generation = desk.generation(),
                .undecodable = desk.undecodable(),
                .gusts = gusts,
                .palettes = palettes,
                .replies = replies,
                .spoken = std::string(desk.latest()["address"].text()),
                .address = desk.address(),
                .trouble = desk.error()};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where
    // the theme in force is no longer this page's, so the colours the
    // faders are drawn in are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const material::Color rule = look.palette.rule;
    const material::Color figure = look.palette.figure;
    const material::Color ash = look.palette.ash;
    compose::Element picture =
        compose::stack()
            .width(kCanvas.width())
            .height(kCanvas.height())
            .children({compose::pen("osc_desk.sky", [this, rule, figure,
                                                     ash](Pen& pen) {
                         sky(pen);
                         faders(pen, rule, figure, ash);
                       }).cover()});
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "An open sound control.",
                  .subtitle = "OSC / CONTROL  /  Wind and palette set the "
                              "scene. A gust starts a passing motion.",
                  .footer = "Run desk.py beside this sketch to send data  ·  "
                            "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "Move the desk controls to steer the field; the meters show "
                 "their current values."},
        std::move(picture).fill(kGround), readout()));
  }

  /** The bands: each a ribbon of segments the width of the canvas, all
   *  of them carried on the one drift the wind and the gusts have
   *  built, breathing by its own wobble and tinted from the palette in
   *  turn. A band's OWN speed is a constant and so is read straight off
   *  the clock; only the wind, which changes, has to be integrated. A
   *  gust lifts what a band is drawn at, so a surge is seen as well as
   *  felt. */
  void sky(Pen& pen) {
    pen.noStroke();
    const float period = kSegment + kGap;
    const float lift = std::min(gust() * kGustLift, 1.0f - kBandAlpha);
    size_t index = 0;
    for (const Band& band : kBands) {
      material::Color tint = palette[index % kTints];
      tint.a = std::min(tint.a + lift, 1.0f);
      pen.fill(tint);
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

  /** THE TWO CONTINUOUS FIGURES, where a live number belongs: the wind
   *  the sky is drifting on, which is also what goes back to the desk,
   *  and the gust standing over it. They are drawn rather than
   *  written because a description carrying a number that changes every
   *  frame is a description rebuilt every frame. */
  void faders(Pen& pen, material::Color rule, material::Color figure,
              material::Color ash) {
    constexpr float kLength = 220.0f;
    constexpr float kThickness = 6.0f;
    constexpr float kLabelGap = 56.0f;  // room for the word beside a bar
    constexpr float kBarGap = 18.0f;    // air between the two bars
    const float right = pen.width - kMargin;
    const float base = pen.height - kMargin;
    const float left = right - kLength;
    const float above = base - kThickness - kBarGap;
    pen.noStroke();
    pen.textSize(10);
    // The wind runs either side of a centre mark, because still air is
    // the middle of a fader's travel and not the end of it.
    const float centre = left + kLength * 0.5f;
    const float reach =
        std::clamp(wind() / kWindSpan, -1.0f, 1.0f) * kLength * 0.5f;
    pen.fill(rule);
    pen.rect(left, base - kThickness, kLength, kThickness, kThickness * 0.5f);
    pen.fill(figure);
    pen.rect(std::min(centre, centre + reach), base - kThickness,
             std::abs(reach), kThickness, kThickness * 0.5f);
    pen.circle(centre + reach, base - kThickness * 0.5f, kThickness * 2.2f);
    pen.fill(ash);
    pen.text("WIND", left - kLabelGap, base);
    // The gust above it: one bar from nothing to full strength.
    pen.fill(rule);
    pen.rect(left, above - kThickness, kLength, kThickness, kThickness * 0.5f);
    pen.fill(figure);
    pen.rect(left, above - kThickness, kLength * std::clamp(gust(), 0.0f, 1.0f),
             kThickness, kThickness * 0.5f);
    pen.fill(ash);
    pen.text("GUST", left - kLabelGap, above);
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: how many
   *  messages have arrived, how many of them were no OSC packet at all,
   *  what the newest one was addressed to, where the door is — or the
   *  sentence saying why there is none — and how many answers have gone
   *  back to a desk. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("desk", kDesk),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable)),
        row("gusts",
            compose::kit::formatted("%llu  ·  palettes %llu",
                                    (unsigned long long)shown.gusts,
                                    (unsigned long long)shown.palettes)),
        row("last",
            shown.spoken.empty() ? "nothing has arrived" : shown.spoken)};
    if (!shown.trouble.empty())
      lines.push_back(row("error", shown.trouble));
    else
      lines.push_back(
          row("address", shown.address.empty() ? "-" : shown.address));
    lines.push_back(row(
        "replies",
        compose::kit::formatted("%llu", (unsigned long long)shown.replies)));
    return compose::box()
        .column()
        .gap(12)
        .font(look.font({.size = 12, .mono = true}))
        .ink(look.palette.ink)
        .children(std::move(lines));
  }
};

}  // namespace

SIGIL_SKETCH(OscDesk, "Data",
             "A desk on the other end of an OSC cable: its faders ease the "
             "wind, its bursts start a gust on the ticker, its nine floats "
             "are three colours, and the eased wind is answered back to "
             "whoever moved the fader for its motor to follow.")
