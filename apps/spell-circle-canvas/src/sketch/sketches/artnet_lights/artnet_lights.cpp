/** @file
 * artnet_lights — a lighting desk in the room, over Art-Net, both ways.
 *
 * A lighting desk is the thing with the faders on it that every fixture
 * in a room is patched to, and Art-Net is how it reaches them over the
 * network: one datagram carrying a universe of dimmers, sent again many
 * times a second, each dimmer a number from 0 to 255. This sketch is
 * one more fixture on that wire. A `data::Connection` on an `artnet://`
 * door hands a reader each packet as a value — its universe, its
 * sequence and its levels — so nothing here reads a byte, and one
 * handler is the whole of the wiring.
 *
 * WHAT IS PATCHED TO WHAT. A fixture answers to the channels it was
 * patched to and ignores the rest of the universe, and so does this
 * one: channels 1, 2 and 3 are the colour wash the sky is lit in,
 * channel 4 is the wind the bands drift on, and channel 5 is a strobe
 * rate — at nothing a lamp simply stands on, and turned up it opens and
 * shuts that many times a second. Everything else on the wire is
 * somebody else's fixture and is only shown, which is what a universe
 * looks like from inside one lamp.
 *
 * AND THE WIRE RUNS BOTH WAYS. A second door — `artnet://127.0.0.1:6455`
 * — sends a universe of its own back, carrying the three brightest
 * colours standing in the scene as nine dimmers, so a desk that patched
 * three of its own fixtures to those channels lights the room in the
 * colours of the canvas. What goes out is the DIFFERENCE alone: a
 * fixture holds the level it was last sent, so a packet every frame
 * saying the same thing is a wire carrying nothing new. A capture has
 * no desk in front of it, so it opens no way back at all and replays
 * the recording beside this file instead.
 *
 * `console.py` beside this file is the desk: it sends the traffic to
 * the port and prints the universe that comes back, and it writes the
 * recording a capture replays.
 *
 *     python3 console.py                                  # a window lights
 *     python3 console.py --record data/desk.feed --seconds 6
 *
 * EDIT THESE FIRST
 *   kDesk        the URI the desk's universes arrive on
 *   kFirstPatch  the channel the wash is patched to
 *   kRecording   what a capture replays instead of listening
 *   kSpacing     how far apart the bands stand
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

const char* kDesk = "artnet://:6454";  // where the desk's universes land
/** THE WAY BACK: the desk's own listener, which a window sends the
 *  scene's colours to. A desk on another machine is that machine's
 *  address here. */
const char* kConsole = "artnet://127.0.0.1:6455";
const char* kRecording = "data/desk.feed";  // what a capture replays

/** WHICH UNIVERSE IS WHOSE. The desk lights this fixture on one and is
 *  answered on another, so the two directions never read each other. */
constexpr int kLitUniverse = 0;
constexpr int kAnsweringUniverse = 1;

/** THE PATCH: the first channel this fixture answers to, and what each
 *  of the five it takes is. A desk numbers its channels from one and
 *  the list a packet reads as counts from nothing, so the patch is
 *  written here the way it is written on the desk and turned into an
 *  index once. */
constexpr int kFirstPatch = 1;
constexpr int kRedChannel = kFirstPatch;
constexpr int kGreenChannel = kFirstPatch + 1;
constexpr int kBlueChannel = kFirstPatch + 2;
constexpr int kWindChannel = kFirstPatch + 3;
constexpr int kStrobeChannel = kFirstPatch + 4;

/** How many dimmers the readout shows, which is the head of the
 *  universe and not the whole of it: what a fixture is patched to, and
 *  a few of its neighbours' channels beside them. */
constexpr size_t kShownChannels = 8;

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.03f, 0.03f, 0.07f, 1};
/** The wash has turned twice and the wind is running by here, and the
 *  strobe has not been opened yet. */
constexpr double kCaptureAt = 2.0;

constexpr float kTop = 76.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 280.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 64.0f;
constexpr float kMargin = 28.0f;

/** The most a dimmer holds, which is what every level off the wire is
 *  read as a fraction of. */
constexpr float kFull = 255.0f;

constexpr float kFade = 0.12f;       // how long the sky takes to reach a level
constexpr float kWindSpan = 180.0f;  // px a second at the top of the fader
constexpr float kBandAlpha = 0.86f;  // what a colour off the wire is drawn at

/** WHAT A STROBE CHANNEL MEANS. At nothing the lamp simply stands on;
 *  anywhere above it the lamp opens and shuts this many times a second,
 *  slowest at the bottom of the fader and fastest at the top. */
constexpr float kStrobeSlowest = 1.0f;
constexpr float kStrobeFastest = 14.0f;
/** How much of one flash the lamp stands open for, and how far down it
 *  goes between two: a strobe shuts rather than darkens, so the shut
 *  half is a floor and not a fade. */
constexpr float kFlashOpen = 0.36f;
constexpr float kFlashShut = 0.14f;

/** A BAND'S OWN SHAPE, which the desk does not send: how thick it is,
 *  how fast it runs against or with the wind, how far it breathes, and
 *  what weight of the wash it is lit at. What a band IS belongs to the
 *  sketch; what colour it is belongs to the desk. */
struct Band {
  float height;
  float speed;
  float wobble;
  float weight;
};

constexpr std::array<Band, 6> kBands = {{{56, -18, 16, 1.00f},
                                         {34, 24, 11, 0.62f},
                                         {68, -11, 20, 0.86f},
                                         {40, 19, 13, 0.48f},
                                         {58, -26, 18, 0.74f},
                                         {30, 13, 9, 0.36f}}};

/** WHAT THE SKY IS LIT IN BEFORE THE DESK HAS SAID ANYTHING. A canvas
 *  nobody is sending to is still a lit room, so there is nothing to
 *  wait for and no placeholder to draw. */
constexpr float kOpeningRed = 0.46f;
constexpr float kOpeningGreen = 0.64f;
constexpr float kOpeningBlue = 0.92f;

/** HOW MANY FIXTURES GO BACK, and how many dimmers each of them takes:
 *  three lamps of red, green and blue, which is nine channels of the
 *  answering universe. */
constexpr size_t kLamps = 3;
constexpr size_t kLampChannels = 3;
constexpr size_t kAnsweringChannels = kLamps * kLampChannels;

/** The most a sequence number counts to before it comes round again. It
 *  starts at one, 0 being what a sender that does not count its packets
 *  writes. */
constexpr int kSequenceCeiling = 255;

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Nothing continuous is in here: the wash, the wind and the flashes
 *  move every frame, and a description rebuilt every frame is a
 *  description nothing can be pruned against. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  uint64_t answered = 0;
  int universe = -1;
  std::string dimmers;
  std::string address;
  std::string trouble;
  bool operator==(const Reading&) const = default;
};

/** One row of the readout: what it is called, and what it answers. */
compose::Element row(const char* name, const std::string& answer) {
  return compose::text(
      compose::kit::formatted("%-11s %s", name, answer.c_str()));
}

/** The dimmer a desk calls @p number, as a fraction of full, out of the
 *  list a packet reads as. A channel the packet did not reach stands at
 *  nothing, which is a dimmer nobody has raised. */
float dimmer(const data::Json& channels, int number) {
  return (float)channels[(size_t)(number - 1)].number() / kFull;
}

/** How bright a colour reads, which is what says which three of them
 *  are the brightest: the three primaries do not weigh the same to an
 *  eye, so a green at half is brighter than a blue at full. */
float brightness(const material::Color& colour) {
  return 0.2126f * colour.r + 0.7152f * colour.g + 0.0722f * colour.b;
}

int levelOf(float unit) {
  return (int)std::lround(std::clamp(unit, 0.0f, 1.0f) * kFull);
}

}  // namespace

namespace {

struct ArtNetLights {
  /** THE DESK'S DOOR: the port its universes arrive on, read as Art-Net
   *  because the URI says artnet. The handler below runs on the hub's
   *  dispatch, which is the call the host already makes once a frame —
   *  so a level that changed between two frames is folded in before the
   *  frame it belongs to is drawn. */
  data::Connection desk;
  /** THE WAY BACK: the desk's own listener. A capture holds a
   *  connection onto nothing, which every send is false on. */
  data::Connection console;
  /** The session's ticker, which outlives this sketch. A handler runs
   *  on the dispatch rather than inside a describe, so it cannot hold
   *  the per-frame context the ticker is otherwise reached through. */
  motion::Ticker* ticker = nullptr;

  /** THE WASH, eased: the colour the desk last called for, reached over
   *  kFade rather than jumped to, so a desk stepping its fader still
   *  fades. */
  ch::Output<float> red{kOpeningRed};
  ch::Output<float> green{kOpeningGreen};
  ch::Output<float> blue{kOpeningBlue};
  /** The wind fader, eased the same way. */
  ch::Output<float> wind{0};
  /** How many times a second the lamp opens and shuts, 0 for a lamp
   *  standing on. It is a rate rather than a level, so it is taken as
   *  it arrives: easing a rate would bend the flashes on either side of
   *  a change. */
  float strobe = 0;

  /** How far the sky has travelled, in px. Integrated rather than read
   *  as a speed times the clock, because a speed that changes would
   *  rewrite where the sky has already been. */
  double drift = 0;
  /** The scene time the last frame stood at, for the step between. */
  double seconds = 0;

  /** What the answering universe was last told, and the count on the
   *  packet that told it: a fixture holds its level, so what goes out
   *  is what changed. */
  std::array<int, kAnsweringChannels> told{};
  int sequence = 0;
  uint64_t answered = 0;
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.ticker;

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window binds the
    // port. Mounting the URI onto the file is the whole of the
    // difference, and the mount stands first because a feed is made once
    // per URI and every later ask answers that same one.
    if (ctx.deterministic) hub.mount(kDesk, hub.resolve(ctx.local(kRecording)));

    red = kOpeningRed;
    green = kOpeningGreen;
    blue = kOpeningBlue;
    wind = 0.0f;
    strobe = 0;
    drift = 0;
    seconds = 0;
    told.fill(-1);
    sequence = 0;
    answered = 0;
    shown = {};

    desk = data::Connection(hub, kDesk);
    desk.on("Dmx", [this](const data::Json& message) { levels(message); });

    // THE WAY BACK IS A WINDOW'S ALONE. A capture has no desk in front
    // of it, and a plate that opened a socket would depend on what
    // happened to answer at that address.
    console = ctx.deterministic ? data::Connection()
                                : data::Connection(hub, kConsole);

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += (double)wind() * step;
    relight();
    // The data path: the sky moves without being described again,
    // because the pen reads it on every frame. What is described again
    // is the readout, and only when one of its figures changed.
    if (read()) describe(ctx);
  }

  /** A UNIVERSE OF DIMMERS ARRIVED. A fixture answers to the channels
   *  it was patched to and leaves the rest of the universe to whoever
   *  else is on the wire, so a packet for another universe lights
   *  nothing here — it is still the newest packet, and the readout
   *  shows it, because what the wire carries is worth seeing whether or
   *  not this lamp is on it. */
  void levels(const data::Json& message) {
    if ((int)message["universe"].number() != kLitUniverse) return;
    const data::Json& channels = message["channels"];
    // A strobe is a rate and is taken as it arrives; the three colours
    // and the wind are levels and are faded to.
    const float rate = dimmer(channels, kStrobeChannel);
    strobe = rate <= 0.0f
                 ? 0.0f
                 : kStrobeSlowest + (kStrobeFastest - kStrobeSlowest) * rate;
    if (!ticker) return;
    fade(&red, dimmer(channels, kRedChannel));
    fade(&green, dimmer(channels, kGreenChannel));
    fade(&blue, dimmer(channels, kBlueChannel));
    // The wind runs either side of still air, because a sky that only
    // drifts one way is a fader with half its travel wasted.
    fade(&wind, (dimmer(channels, kWindChannel) * 2.0f - 1.0f) * kWindSpan);
  }

  void fade(ch::Output<float>* value, float to) {
    ticker->timeline().apply(value).then<ch::RampTo>(to, kFade,
                                                     &ch::easeOutQuad);
  }

  /** WHAT A BAND IS LIT IN THIS INSTANT: the wash at the band's own
   *  weight, shut down to the floor between two flashes where the
   *  strobe is up. It is one function because the drawing and the
   *  universe that goes back to the desk have to be the same colours —
   *  a desk lighting the room from a canvas it is not looking at cannot
   *  be told two different things. */
  material::Color tint(size_t index) const {
    const float weight = kBands[index % kBands.size()].weight * flash();
    return {red() * weight, green() * weight, blue() * weight, kBandAlpha};
  }

  /** HOW FAR OPEN THE LAMP STANDS THIS INSTANT. A strobe shuts rather
   *  than darkens, so what stands between two flashes is a floor and
   *  not a fade. */
  float flash() const {
    if (strobe <= 0.0f) return 1.0f;
    const double turn = seconds * (double)strobe;
    return turn - std::floor(turn) < (double)kFlashOpen ? 1.0f : kFlashShut;
  }

  /** THE UNIVERSE THAT GOES BACK: the three brightest colours standing
   *  in the scene, as three lamps of red, green and blue. A fixture
   *  holds the level it was last sent, so a packet goes out when a
   *  level changed and at no other time — a desk told the same thing
   *  sixty times a second is a wire carrying nothing new. The codec
   *  counts dimmers in pairs, so the nine go out as ten with the last
   *  at nothing.
   *
   *  A window with no desk answering at that address sends into a door
   *  that says so, which is false and costs the frame nothing. */
  void relight() {
    std::array<material::Color, kBands.size()> lit{};
    for (size_t index = 0; index != kBands.size(); ++index)
      lit[index] = tint(index);
    std::sort(lit.begin(), lit.end(),
              [](const material::Color& a, const material::Color& b) {
                return brightness(a) > brightness(b);
              });

    std::array<int, kAnsweringChannels> levels{};
    for (size_t lamp = 0; lamp != kLamps; ++lamp) {
      levels[lamp * kLampChannels] = levelOf(lit[lamp].r);
      levels[lamp * kLampChannels + 1] = levelOf(lit[lamp].g);
      levels[lamp * kLampChannels + 2] = levelOf(lit[lamp].b);
    }
    if (levels == told) return;
    told = levels;

    data::Json::Array channels;
    channels.reserve(kAnsweringChannels);
    for (const int level : levels) channels.push_back(data::Json(level));
    // The sequence says which packet came after which, so a datagram
    // that arrived late is told from one that arrived twice. It comes
    // round to one rather than to nothing: 0 is what a sender that does
    // not count writes.
    sequence = sequence % kSequenceCeiling + 1;
    if (console.send(data::Json(
            data::Json::Object{{"kind", data::Json("Dmx")},
                               {"universe", data::Json(kAnsweringUniverse)},
                               {"sequence", data::Json(sequence)},
                               {"channels", data::Json(std::move(channels))}})))
      ++answered;
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    const data::Json& newest = desk.latest("Dmx");
    const data::Json& channels = newest["channels"];
    std::string dimmers;
    for (size_t at = 0; at != kShownChannels; ++at)
      dimmers += at < channels.size() ? compose::kit::formatted(
                                            "%4d", (int)channels[at].number())
                                      : std::string("   -");
    Reading now{
        .generation = desk.generation(),
        .undecodable = desk.undecodable(),
        .answered = answered,
        .universe = newest.null() ? -1 : (int)newest["universe"].number(),
        .dimmers = std::move(dimmers),
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
    // dimmer row is drawn in are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const material::Color rule = look.palette.rule;
    const material::Color figure = look.palette.figure;
    const material::Color ash = look.palette.ash;
    compose::Element picture =
        compose::stack()
            .width(kCanvas.width())
            .height(kCanvas.height())
            .children({compose::pen("artnet_lights.room", [this, rule, figure,
                                                           ash](Pen& pen) {
                         sky(pen);
                         rack(pen, rule, figure, ash);
                       }).cover()});
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "Light, across a network.",
                  .subtitle = "ART-NET / DMX  /  A lighting desk sends "
                              "channels. The scene answers in colour.",
                  .footer = "Run console.py beside this sketch to send data  · "
                            " Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "The rack shows the outgoing dimmers; each ribbon follows its "
                 "lighting channel."},
        std::move(picture).fill(kGround), readout()));
  }

  /** THE SKY THE DESK LIGHTS: bands crossing the canvas, carried on the
   *  one drift the wind has built, each breathing by its own wobble and
   *  lit at its own weight of the wash. A band's OWN speed is a
   *  constant and so is read straight off the clock; only the wind,
   *  which changes, has to be integrated. */
  void sky(Pen& pen) {
    pen.noStroke();
    const float period = kSegment + kGap;
    for (size_t index = 0; index != kBands.size(); ++index) {
      const Band& band = kBands[index];
      pen.fill(tint(index));
      const double travelled = drift + (double)band.speed * seconds;
      const float phase =
          (float)(std::fmod(std::fmod(travelled, period) + period, period));
      const float y =
          kTop + kSpacing * (float)index +
          band.wobble * std::sin((float)seconds * 0.7f + (float)index);
      for (float x = phase - period; x < pen.width; x += period)
        pen.rect(x, y, kSegment, band.height, band.height * 0.5f);
    }
  }

  /** THE HEAD OF THE UNIVERSE as a rack of faders, where a live number
   *  belongs: one column per dimmer, standing at the level that dimmer
   *  arrived at, with the five this fixture is patched to marked. They
   *  are drawn rather than written because a description carrying a
   *  number that changes every frame is a description rebuilt every
   *  frame. */
  void rack(Pen& pen, material::Color rule, material::Color figure,
            material::Color ash) {
    constexpr float kHeight = 120.0f;
    constexpr float kWidth = 22.0f;
    constexpr float kColumnGap = 12.0f;
    const data::Json& channels = desk.latest("Dmx")["channels"];
    const float right = pen.width - kMargin;
    const float base = pen.height - kMargin;
    const float left =
        right - (kWidth + kColumnGap) * (float)kShownChannels + kColumnGap;
    pen.noStroke();
    pen.textSize(9);
    for (size_t at = 0; at != kShownChannels; ++at) {
      const float x = left + (float)at * (kWidth + kColumnGap);
      pen.fill(rule);
      pen.rect(x, base - kHeight, kWidth, kHeight, kWidth * 0.5f);
      const float level =
          std::clamp((float)channels[at].number() / kFull, 0.0f, 1.0f);
      // The five this fixture answers to are drawn in the figure's own
      // ink and the rest in the ash, so what is patched here is told
      // from what is somebody else's fixture at a glance.
      const int number = (int)at + 1;
      pen.fill(number >= kFirstPatch && number <= kStrobeChannel ? figure
                                                                 : ash);
      const float reach = kHeight * level;
      pen.rect(x, base - reach, kWidth, reach, kWidth * 0.5f);
      pen.fill(ash);
      pen.text(compose::kit::formatted("%d", number), x + kWidth * 0.25f,
               base + 14.0f);
    }
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: how many packets
   *  have arrived, how many of them were no Art-Net packet at all,
   *  which universe the newest one was for and what the head of it
   *  holds, how many universes have gone back to the desk, and where
   *  the door is — or the sentence saying why there is none. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("door", kDesk),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable)),
        row("universe", shown.universe < 0
                            ? "nothing has arrived"
                            : compose::kit::formatted("%d", shown.universe)),
        row("channels", shown.dimmers),
        row("answered", compose::kit::formatted(
                            "%llu", (unsigned long long)shown.answered))};
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

SIGIL_SKETCH(ArtNetLights, "Data",
             "A lighting desk on the other end of an Art-Net cable: three "
             "of its dimmers are the wash the sky is lit in, a fourth is "
             "the wind the bands drift on and a fifth a strobe rate, while "
             "the three brightest colours on the canvas go back as a "
             "universe of their own for the desk's own fixtures to follow.")
