/** @file
 * grpc_watch — the sketch as a service.
 *
 * The door is a gRPC method. `grpc://:27090/Sky/Watch` holds a port and
 * serves that one method, and every call that reaches it is a caller
 * with a stream of its own: what the caller writes arrives here, and
 * what this sends goes out to every stream standing at once. Nothing on
 * the wire is generated — the method carries bytes and this scene writes
 * JSON through it — so a caller in any language that speaks gRPC needs
 * the address and the two message kinds below and nothing else.
 *
 * WHAT GOES EACH WAY. Out, twenty times a second, one `Sky` message
 * carrying the bands as they stand — where each is, how thick, how far
 * along — so every caller draws the sky the wall is drawing. In, the
 * two things a hand can do: a `Palette` swaps the colours the bands are
 * tinted from, a `Gust` leaves a burst of wind that decays on its own.
 * State and event, the same two shapes any control surface has.
 *
 * THE LIVE OTHER SIDE IS SEER, which opens the same method as a caller:
 *
 *     Seer grpc://127.0.0.1:27090/Sky/Watch
 *
 * `watch.py` beside this file writes the recording a capture replays —
 * two palette turns and a gust — and only that, there being no gRPC in
 * the Python standard library for it to speak with.
 *
 *     python3 watch.py --record data/watch.feed
 *
 * EDIT THESE FIRST
 *   kPort       the port a caller reaches, and the method behind it
 *   kMethod     the one method this door serves
 *   kRecording  what a capture replays instead of holding the port
 *   kTellEvery  how often the callers are told what the sky is
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

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
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

constexpr int kPort = 27090;                 // where a caller reaches this
const char* kMethod = "/Sky/Watch";          // the one method this door serves
const char* kRecording = "data/watch.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.05f, 0.05f, 0.08f, 1};
/** Both palette turns have landed and a gust is half a second old,
 *  which is the frame that shows both things a caller can do. */
constexpr double kCaptureAt = 2.5;

constexpr float kTop = 84.0f;       // where the first band stands
constexpr float kSpacing = 92.0f;   // how far apart the bands stand
constexpr float kSegment = 240.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 64.0f;
constexpr float kMargin = 28.0f;

constexpr float kBaseWind = 26.0f;       // what the sky drifts at with no gust
constexpr float kGustFall = 1.6f;        // how long a gust takes to die away
constexpr float kGustStrength = 150.0f;  // what a gust carrying no figure is
/** How hard a gust has to be to smear a segment to twice its length. A
 *  still cannot show speed, so the wind shows as the stretch it puts
 *  into the ribbons. */
constexpr float kGustStretch = 420.0f;

/** HOW OFTEN THE CALLERS ARE TOLD. A caller draws when it is told and on
 *  a clock of its own the rest of the time, so the sky goes out on a
 *  cadence rather than once a frame: a frame is not news, and a stream
 *  handed sixty of them a second carries the ones nobody could tell
 *  apart. */
constexpr double kTellEvery = 1.0 / 20.0;

constexpr float kBandAlpha = 0.84f;  // what a colour off the wire is drawn at

/** The colours the bands are tinted from, in turn. */
constexpr size_t kTints = 3;
using Palette = std::array<material::Color, kTints>;

/** What the sky is tinted from until a caller turns it — the first of
 *  the three a caller sends. */
constexpr Palette kOpeningPalette = {{{0.40f, 0.58f, 0.92f, kBandAlpha},
                                      {0.50f, 0.46f, 0.88f, kBandAlpha},
                                      {0.34f, 0.74f, 0.86f, kBandAlpha}}};

/** A BAND'S OWN SHAPE, which no message carries: how thick it is, how
 *  fast it runs against or with the wind, and how far it breathes. */
struct Band {
  float height;
  float speed;
  float wobble;
};

constexpr std::array<Band, 6> kBands = {{{46, -18, 16},
                                         {30, 24, 11},
                                         {58, -12, 20},
                                         {36, 19, 13},
                                         {50, -26, 18},
                                         {26, 14, 9}}};

/** The door this sketch holds: the port a caller reaches, and the one
 *  method behind it that both ends name. */
std::string doorOf() { return "grpc://:" + std::to_string(kPort) + kMethod; }

/** A number as far as a caller can use it. A tenth of a pixel is finer
 *  than anything a screen draws, and every digit past it is wire nobody
 *  reads. */
double toTheCaller(double value) { return std::round(value * 10.0) / 10.0; }

/** One channel of a colour written as four numbers, or @p fallback where
 *  the message left it out. */
float channel(const data::Json& colour, size_t at, float fallback) {
  return (float)colour[at].number(fallback);
}

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Nothing here is computed about the sky; every figure is one the
 *  connection or a hand at the other end answered. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  uint64_t spoken = 0;
  uint64_t turns = 0;
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

struct GrpcWatch {
  /** THE DOOR. Read as JSON, the scheme not being osc; the handlers
   *  below run on the hub's dispatch, which is the call the host already
   *  makes once a frame. */
  data::Connection callers;
  /** The session's ticker, which outlives this sketch. A handler runs on
   *  the dispatch rather than inside a describe, so it cannot hold the
   *  per-frame context the ticker is otherwise reached through. */
  motion::Ticker* ticker = nullptr;

  /** THE STATE a hand can set: the colours the bands are tinted from. */
  Palette palette = kOpeningPalette;
  /** WHAT IS LEFT OF THE GUSTS: a burst of wind on top of the standing
   *  one, dying away on the ticker with nothing having to arrive for it
   *  to end. */
  ch::Output<float> gust{0};

  /** How far the sky has travelled, in px. Integrated rather than read
   *  as a speed times the clock, because a wind that changes would
   *  rewrite where the sky has already been. */
  double drift = 0;
  /** The scene time the last frame stood at, for the step between. */
  double seconds = 0;
  /** When the callers were last told what the sky is. */
  double told = 0;

  /** How many sends reached a caller, and how many palette turns
   *  landed. A send on a door with no call standing on it reaches
   *  nobody and says so, which is what an unwatched scene looks like
   *  from here. */
  uint64_t spoken = 0;
  uint64_t turns = 0;
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.ticker;

    const std::string door = doorOf();
    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE REPLAYS THE RECORDING BESIDE THIS FILE and holds no port
    // at all; a window holds the port and answers callers. The mount
    // table matches a URI by its PREFIX, so the key is the whole URI —
    // the same string the feed is asked for. It stands first because a
    // feed is made once per URI and every later ask answers that same
    // one.
    if (ctx.deterministic) hub.mount(door, hub.resolve(ctx.local(kRecording)));

    palette = kOpeningPalette;
    gust = 0.0f;
    drift = 0;
    seconds = 0;
    told = 0;
    spoken = 0;
    turns = 0;
    shown = {};

    callers = data::Connection(hub, door);
    callers.on("Palette", [this](const data::Json& message) { turn(message); });
    callers.on("Gust", [this](const data::Json& message) { blow(message); });

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += (double)wind() * step;
    // THE SKY GOES OUT WHEN IT HAS MOVED FAR ENOUGH TO BE NEWS, not once
    // a frame. A send that reached no call standing — a replayed
    // recording, or a door nobody has called yet — goes nowhere and says
    // so, which is what a capture is.
    if (seconds - told >= kTellEvery) {
      told = seconds;
      if (callers.send(sky())) ++spoken;
    }
    // The bands move without being described again, because the pen
    // reads them on every frame. What is described again is the readout,
    // and only when a figure in it changed.
    if (read()) describe(ctx);
  }

  /** The wind the bands ride: what stands, plus what is left of the last
   *  gust. */
  float wind() const { return kBaseWind + gust(); }

  /** How long a segment is drawn at now: a gust smears a ribbon out, so
   *  a still carries the wind that a moving picture carries as speed. */
  float segment() const { return kSegment * (1.0f + gust() / kGustStretch); }

  /** Where band @p index stands now: its own place, breathing by its own
   *  wobble. */
  float topOf(size_t index) const {
    return kTop + kSpacing * (float)index +
           kBands[index].wobble *
               std::sin((float)seconds * 0.7f + (float)index);
  }

  /** How far along band @p index's ribbon is, within one period of it. */
  float phaseOf(size_t index) const {
    const float period = segment() + kGap;
    const double travelled = drift + (double)kBands[index].speed * seconds;
    return (float)(std::fmod(std::fmod(travelled, period) + period, period));
  }

  /** STATE: the colours the bands are tinted from, swapped whole. A
   *  message carrying fewer than there are bands to tint is cycled, so a
   *  caller may send one colour or three. */
  void turn(const data::Json& message) {
    const std::span<const data::Json> written = message["colors"].items();
    if (written.empty()) return;
    for (size_t index = 0; index != kTints; ++index) {
      const data::Json& tint = written[index % written.size()];
      palette[index] = {channel(tint, 0, 1), channel(tint, 1, 1),
                        channel(tint, 2, 1), channel(tint, 3, kBandAlpha)};
    }
    ++turns;
  }

  /** AN EVENT: the gust is over the moment it arrives, and what it
   *  leaves behind is a burst of wind that dies away on its own. Reading
   *  it as state would leave the sky permanently gusting. */
  void blow(const data::Json& message) {
    if (!ticker) return;
    gust = (float)message["strength"].number(kGustStrength);
    ticker->timeline().apply(&gust).then<ch::RampTo>(
        0.0f, (float)message["seconds"].number(kGustFall), &ch::easeOutQuad);
  }

  /** THE SKY AS ONE MESSAGE: what the wall is drawing, in the wall's own
   *  pixels, so a caller scales it to its screen and needs no clock of
   *  its own to place a band. */
  data::Json sky() const {
    data::Json::Array ribbons;
    for (size_t index = 0; index != kBands.size(); ++index)
      ribbons.push_back(
          data::Json::Object{{"y", toTheCaller(topOf(index))},
                             {"height", (double)kBands[index].height},
                             {"offset", toTheCaller(phaseOf(index))}});
    data::Json::Array colours;
    for (const material::Color& tint : palette)
      colours.push_back(data::Json::Array{toTheCaller(tint.r),
                                          toTheCaller(tint.g),
                                          toTheCaller(tint.b), tint.a});
    return data::Json::Object{{"kind", "Sky"},
                              {"width", (double)kCanvas.width()},
                              {"height", (double)kCanvas.height()},
                              {"wind", toTheCaller(wind())},
                              {"segment", toTheCaller(segment())},
                              {"period", toTheCaller(segment() + kGap)},
                              {"palette", std::move(colours)},
                              {"bands", std::move(ribbons)}};
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Reading now{.generation = callers.generation(),
                .undecodable = callers.undecodable(),
                .spoken = spoken,
                .turns = turns,
                .address = callers.address(),
                .trouble = callers.error()};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("grpc_watch.sky", [this](Pen& pen) {
                      bands(pen);
                    }).cover());
    if (shown.generation == 0) parts.push_back(invitation());
    compose::Element picture = compose::stack()
                                   .width(kCanvas.width())
                                   .height(kCanvas.height())
                                   .children(std::move(parts));
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A scene on a stream.",
                  .subtitle = "gRPC / BIDIRECTIONAL  /  A long-lived "
                              "connection carries updates in both directions.",
                  .footer = "Open grpc://127.0.0.1:27090/Sky/Watch in Seer  ·  "
                            "watch.py writes the capture recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "A caller changes the palette or sends a gust. The scene "
                 "streams the resulting band positions back."},
        std::move(picture).fill(kGround), readout()));
  }

  /** The bands: each a ribbon of segments the width of the canvas,
   *  drifting on the wind plus its own speed and breathing by its own
   *  wobble, tinted from the palette in turn. */
  void bands(Pen& pen) {
    pen.noStroke();
    const float length = segment();
    const float period = length + kGap;
    for (size_t index = 0; index != kBands.size(); ++index) {
      pen.fill(palette[index % kTints]);
      const float y = topOf(index);
      const float height = kBands[index].height;
      for (float x = phaseOf(index) - period; x < pen.width; x += period)
        pen.rect(x, y, length, height, height * 0.5f);
    }
  }

  /** WHERE TO POINT A CALLER, until one has spoken. The whole of what
   *  somebody needs to watch this is the address and the method behind
   *  it. */
  compose::Element invitation() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::text(
               compose::kit::formatted("open grpc://127.0.0.1:%d%s in Seer",
                                       kPort, kMethod))
        .font(look.font({.size = 15, .mono = true}))
        .ink(look.palette.ash)
        .centerAt({kCanvas.width() * 0.5f, kCanvas.height() - 96.0f});
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: which door this
   *  is, how many messages have arrived, how many of them were no
   *  message at all, how many sends reached a caller, and the local end
   *  as the transport bound it — or the sentence saying why there is
   *  none. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("door", compose::kit::formatted("grpc://:%d%s", kPort, kMethod)),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable)),
        row("callers", compose::kit::formatted(
                           "%llu spoken to", (unsigned long long)shown.spoken)),
        row("palette", compose::kit::formatted(
                           "%llu turns", (unsigned long long)shown.turns))};
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
};

}  // namespace

SIGIL_SKETCH(GrpcWatch, "Data",
             "The sketch as a service: one gRPC method holds the port, every "
             "call that reaches it is a caller the sky goes out to, and what "
             "a caller writes back turns the palette and gusts the wind.")
