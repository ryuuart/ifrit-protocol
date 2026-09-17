/** @file
 * webrtc_sky — a phone in the audience, with nothing in between.
 *
 * The sketch holds a door and hands out the PAGE that opens it, exactly
 * as its websocket sibling does — but the socket the page comes back on
 * is used for one thing only: the two ends say what addresses they
 * might be reachable at until one pair answers. From then on the
 * messages go STRAIGHT from the phone to this machine, through no
 * server at all, which is what lets a phone on a mobile network reach a
 * scene behind a router with nothing installed at either end.
 *
 * TWO URIS, ONE PORT. `ws://:8849/signal?pages=<uri>` is the door that
 * serves the page and carries the introductions;
 * `webrtc://sky?signal=<that>` is the conversation held over it. A
 * phone opens the address, is served `index.html`, and that page makes
 * its own connection back — one channel per phone, and the sketch is
 * the far end of however many of them are attached.
 *
 * WHAT A PHONE IS TOLD TO OPEN IS WHAT THE DOOR ANSWERS. The
 * conversation reports the signal as it BOUND, so the port on the page
 * is the one standing rather than the one asked for — and `kPort` may
 * be written 0, which is the kernel's to fill, the door then saying
 * which one it gave.
 *
 * WHAT GOES EACH WAY, and it is the sibling's own vocabulary: out,
 * twenty times a second, one `Sky` message carrying the bands as they
 * stand; in, the things a hand can do — a `Palette` swaps the colours
 * the bands are tinted from, a `Gust` leaves a burst of wind that
 * decays on its own.
 *
 * ONE SEND REACHES EVERY PHONE. A door holding several peers writes on
 * every channel that is open, which is what makes the audience see one
 * sky rather than each its own.
 *
 * `phone.py` beside this file writes the recording a capture replays,
 * so a still of this scene is the scene a phone drove. It cannot stand
 * in for a live phone the way the sibling's does: speaking this door
 * needs a WebRTC stack, which Python does not ship.
 *
 *     python3 phone.py --seconds 6
 *
 * EDIT THESE FIRST
 *   kPort       the port a phone reaches, and the pages come from
 *   kRoom       the conversation a phone joins on that port
 *   kIce        the server that says what address this machine has to
 *               the world — needed only across networks
 *   kRecording  what a capture replays instead of listening
 *   kTellEvery  how often the phones are told what the sky is
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

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;
namespace motion = sigil::motion;
namespace ch = choreograph;

using sigil::draw::Pen;

namespace {

constexpr int kPort = 8849;                  // where a phone reaches this, or 0
const char* kPath = "/signal";               // the path the introductions cross
const char* kRoom = "sky";                   // the conversation held over them
const char* kPages = "data/pages";           // where index.html stands
const char* kRecording = "data/phone.feed";  // what a capture replays

/** WHERE THIS MACHINE ASKS WHAT ADDRESS IT HAS TO THE WORLD. Two ends
 *  on one network find each other without one and this is empty; a
 *  phone on another network needs a server named here AND in the page
 *  beside this file, both ends having to ask the same question. */
const char* kIce = "";

constexpr SkSize kCanvas = {1280, 720};
constexpr SkColor4f kGround = {0.04f, 0.04f, 0.09f, 1};
/** A palette turn has landed and a gust is half a second old, which is
 *  the frame that shows both things a phone can do. */
constexpr double kCaptureAt = 2.5;

constexpr float kTop = 72.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

constexpr float kBaseWind = 24.0f;       // what the sky drifts at with no gust
constexpr float kGustFall = 1.6f;        // how long a gust takes to die away
constexpr float kGustStrength = 140.0f;  // what a gust carrying no figure is
/** How hard a gust has to be to smear a segment to twice its length. A
 *  still cannot show speed, so the wind shows as the stretch it puts
 *  into the ribbons. */
constexpr float kGustStretch = 400.0f;

/** HOW OFTEN THE PHONES ARE TOLD. A phone draws when it is told and on
 *  a clock of its own the rest of the time, so the sky goes out on a
 *  cadence rather than once a frame: a frame is not news, and a phone
 *  that is handed sixty of them a second spends its battery on the ones
 *  it could not tell apart. */
constexpr double kTellEvery = 1.0 / 20.0;

constexpr float kBandAlpha = 0.82f;  // what a colour off the wire is drawn at

/** The colours the bands are tinted from, in turn. */
constexpr size_t kTints = 3;
using Palette = std::array<SkColor4f, kTints>;

/** What the sky is tinted from until a phone turns it — the first of
 *  the three a page's buttons send. */
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

/** The door the introductions cross: the port a phone reaches, the path
 *  it opens back on, and the pages the same port hands out, which stand
 *  beside this file. */
std::string signalOf(const sketch::SketchContext& ctx) {
  return "ws://:" + std::to_string(kPort) + kPath +
         "?pages=" + ctx.local(kPages);
}

/** The conversation itself, held over that door. The signal stands in
 *  the query whole, pages and all, so the page a phone loads and the
 *  socket the introductions cross are one address and one feed. */
std::string doorOf(const sketch::SketchContext& ctx) {
  std::string uri =
      "webrtc://" + std::string(kRoom) + "?signal=" + signalOf(ctx);
  if (*kIce != '\0') uri += "&ice=" + std::string(kIce);
  return uri;
}

/** The port the door at @p address is reached on: the digits behind the
 *  last colon of it, a path behind them not being read. Zero where it
 *  names none, which is what a capture replaying a recording answers,
 *  no port being held at all. */
int portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  int port = 0;
  for (size_t at = colon + 1; at != address.size(); ++at) {
    if (address[at] < '0' || address[at] > '9') break;
    port = port * 10 + (address[at] - '0');
  }
  return port;
}

/** A number as far as a phone can use it. A tenth of a pixel is finer
 *  than anything a screen draws, and every digit past it is wire nobody
 *  reads. */
double toThePhone(double value) { return std::round(value * 10.0) / 10.0; }

/** One channel of a colour written as four numbers, or @p fallback where
 *  the message left it out. */
float channel(const data::Json& colour, size_t at, float fallback) {
  return (float)colour[at].number(fallback);
}

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Nothing here is computed about the sky; every figure is one the
 *  connection or a hand on a phone answered. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  uint64_t turns = 0;
  uint64_t messages = 0;
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

struct WebRtcSky {
  /** THE CONVERSATION, and the door it was introduced over. Read as
   *  JSON, because the scheme is not osc; the handlers below run on the
   *  hub's dispatch, which is also the call that carries an
   *  introduction, so a phone attaches and speaks on the same frame a
   *  scene already has. */
  data::Connection phone;
  /** The session's ticker, which outlives this sketch. A handler runs
   *  on the dispatch rather than inside a describe, so it cannot hold
   *  the per-frame context the ticker is otherwise reached through. */
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
  /** When the phones were last told what the sky is. */
  double told = 0;

  /** How many palette turns landed, and how many messages the phones
   *  sent in all — the turns, the gusts and whatever else reached the
   *  channel. */
  uint64_t turns = 0;
  uint64_t messages = 0;
  /** The URI the pages stand at, as the signal's query spelled it. */
  std::string pages;
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.ticker;

    pages = ctx.local(kPages);
    const std::string door = doorOf(ctx);
    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE REPLAYS THE RECORDING BESIDE THIS FILE and opens no
    // port and no conversation at all; a window holds the port, serves
    // the pages and is introduced to whoever loads them. The mount
    // table matches a URI by its PREFIX, so the key is the whole URI,
    // query and all — the same string the feed is asked for. It stands
    // first because a feed is made once per URI and every later ask
    // answers that same one.
    if (ctx.deterministic) hub.mount(door, hub.resolve(ctx.local(kRecording)));

    palette = kOpeningPalette;
    gust = 0.0f;
    drift = 0;
    seconds = 0;
    told = 0;
    turns = 0;
    messages = 0;
    shown = {};

    phone = data::Connection(hub, door);
    phone.on("Palette", [this](const data::Json& message) { turn(message); });
    phone.on("Gust", [this](const data::Json& message) { blow(message); });
    // LAST, AND OVER EVERYTHING: several handlers may name one message
    // and each runs once, in the order they were registered, so this
    // one counts every message after the two above have acted on
    // theirs — including the kinds nothing here reads.
    phone.on("*", [this](const data::Json&) { ++messages; });

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += (double)wind() * step;
    // THE SKY GOES OUT WHEN IT HAS MOVED FAR ENOUGH TO BE NEWS, not
    // once a frame. A send on a door no phone is attached to reaches
    // nobody, which is what a window with no phone in the room is and
    // what a capture is.
    if (seconds - told >= kTellEvery) {
      told = seconds;
      phone.send(sky());
    }
    // The bands move without being described again, because the pen
    // reads them on every frame. What is described again is the
    // readout, and only when a figure in it changed.
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
   *  message carrying fewer than there are bands to tint is cycled, so
   *  a page's button may send one colour or three. */
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
   *  pixels, so a phone scales it to its screen and needs no clock of
   *  its own to place a band. */
  data::Json sky() const {
    data::Json::Array ribbons;
    for (size_t index = 0; index != kBands.size(); ++index)
      ribbons.push_back(
          data::Json::Object{{"y", toThePhone(topOf(index))},
                             {"height", (double)kBands[index].height},
                             {"offset", toThePhone(phaseOf(index))}});
    data::Json::Array colours;
    for (const SkColor4f& tint : palette)
      colours.push_back(data::Json::Array{toThePhone(tint.fR),
                                          toThePhone(tint.fG),
                                          toThePhone(tint.fB), tint.fA});
    return data::Json::Object{{"kind", "Sky"},
                              {"width", (double)kCanvas.width()},
                              {"height", (double)kCanvas.height()},
                              {"wind", toThePhone(wind())},
                              {"segment", toThePhone(segment())},
                              {"period", toThePhone(segment() + kGap)},
                              {"palette", std::move(colours)},
                              {"bands", std::move(ribbons)}};
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Reading now{.generation = phone.generation(),
                .undecodable = phone.undecodable(),
                .turns = turns,
                .messages = messages,
                .address = phone.address(),
                .trouble = phone.error()};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("webrtc_sky.sky", [this](Pen& pen) {
                      bands(pen);
                    }).cover());
    if (shown.messages == 0) parts.push_back(invitation());
    compose::Element picture = compose::stack()
                                   .width(kCanvas.width())
                                   .height(kCanvas.height())
                                   .children(std::move(parts));
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A peer becomes a control.",
                  .subtitle = "WEBRTC / DATA CHANNEL  /  A browser sends "
                              "directly to the scene over a data channel.",
                  .footer = "Run phone.py beside this sketch to send data  ·  "
                            "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "The address in the preview opens the controller and "
                 "negotiates its connection."},
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

  /** THE PORT A PHONE OPENS, as the door ANSWERS it: a conversation
   *  waiting reports the signal it bound, so a port asked for as zero
   *  is readable here and what is shown is never a port nothing stands
   *  on. Where no door is open at all — a capture, which replays the
   *  recording and holds nothing — what was asked for is all there
   *  is. */
  int reachedOn() const {
    const int bound = portOf(shown.address);
    return bound != 0 ? bound : kPort;
  }

  /** WHERE TO POINT A PHONE, until one has spoken. The page that opens
   *  this conversation is served by the same port the introduction
   *  crosses, so the whole of what a person needs is the address. */
  compose::Element invitation() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::text(
               compose::kit::formatted(
                   "open this machine on port %d in a phone's browser",
                   reachedOn()))
        .font(look.font({.size = 15, .mono = true}))
        .ink(look.palette.ash)
        .centerAt({kCanvas.width() * 0.5f, kCanvas.height() - 96.0f});
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: the room the
   *  phones join and the door they are introduced over, where its pages
   *  stand, how many messages have arrived, how many were no message at
   *  all, how many of them turned the palette, and the conversation as
   *  the transport named it — or the sentence saying why there is
   *  none. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("room", compose::kit::formatted("webrtc://%s", kRoom)),
        row("signal", compose::kit::formatted("ws://:%d%s", kPort, kPath)),
        row("pages", pages),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable)),
        row("messages", compose::kit::formatted(
                            "%llu", (unsigned long long)shown.messages)),
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

SIGIL_SKETCH(WebRtcSky, "Data",
             "A door that hands out the page that opens it, and then gets out "
             "of the way: the page is served over one port, the two ends are "
             "introduced over it, and the sky and what a hand does to it cross "
             "straight between phone and machine.")
