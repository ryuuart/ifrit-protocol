/** @file
 * control_surfaces — one sky, and three doors that answer back.
 *
 * The things standing beside a screen that steer it — an OSC desk, a
 * MIDI pad controller, a lighting desk on Art-Net — each reach this
 * sketch through a `data::Connection`, the door one floor up from a feed:
 * the URI's scheme names how the bytes read, so each door hands a reader
 * its messages as values, and handlers registered on it are the whole of
 * the wiring. Nothing here parses anything. And each door is answered,
 * in the way its own kind of device expects.
 *
 *   OSC · `osc://:27070`   `/sky/wind` is STATE, a fader reading the sky
 *                          eases to; `/sky/gust` is an EVENT, a motion on
 *                          the ticker that rises and falls away by itself.
 *                          The door binds a port and holds no one peer, but
 *                          every message names its sender and `reply()`
 *                          answers that one: the eased wind goes back to
 *                          whoever moved the fader, for its motor to follow.
 *   MIDI · `midi://in/`    a `NoteOn` lights one of eight cells, a `NoteOff`
 *                          lets it fade. A second door, `midi://out/`,
 *                          writes a note back for every cell that lights or
 *                          goes dark, so the LEDs under the fingers follow
 *                          the canvas.
 *   ART-NET · `artnet://:6454`  a universe of dimmers: channels 1–3 are the
 *                          wash the bands are lit in and 5 a strobe rate;
 *                          the rest of the universe is somebody else's
 *                          fixture and is only shown. A second door sends
 *                          the three brightest colours on the canvas back as
 *                          a universe of nine dimmers.
 *
 * WHAT GOES BACK IS THE DIFFERENCE ALONE. An LED and a fixture both hold
 * what they were last sent, so a door told the same thing every frame is
 * a cable carrying nothing new: lights go out when a cell crosses its
 * threshold, a universe when a level changed. A capture replays the
 * recordings beside this file, which hold the messages and not who sent
 * them, so it opens no way back at all and a plate is the listening half.
 *
 * `desk.py`, `pads.py` and `console.py` beside this file are the three
 * devices: each sends its traffic live where it can and writes the
 * recording a capture replays.
 *
 *     python3 desk.py                        # the OSC desk, answered
 *     python3 console.py                     # the lighting desk, answered
 *     python3 pads.py --record data/pads.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kDesk / kPads / kLightingDesk  the URIs each device arrives on
 *   kController   a piece of the MIDI controller's name, empty for the first
 *   kGustPush     how much drift a full gust adds
 *   kFirstPatch   the Art-Net channel the wash is patched to
 *   kSpacing      how far apart the bands stand
 */

// TAGS: Data/Sources, Runtime/Resources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Dialect.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilsketch/kit/Connection.h>
#include <sigilsketch/kit/Instrument.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilsketch/scene2d/Scene2D.h>

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

using sigil::draw::Pen;

namespace {

// THE DOORS, and what a capture replays instead of each.
const char* kDesk = "osc://:27070";
const char* kDeskRecording = "data/desk.feed";
const char* kController = "";  // a piece of the port's name; empty = first
const char* kPads = "midi://in/";
const char* kLights = "midi://out/";
const char* kPadsRecording = "data/pads.feed";
const char* kLightingDesk = "artnet://:6454";
const char* kLightingAnswer = "artnet://127.0.0.1:6455";
const char* kLightingRecording = "data/console.feed";

constexpr SkSize kCanvas = {1280, 720};
/** The page the picture and the three doors' readings stand on. */
constexpr SkSize kPage = {1280, 900};
constexpr material::Color kGround = {0.04f, 0.04f, 0.09f, 1};
/** A gust is in the air, pads are down and the wash has turned by here. */
constexpr double kCaptureAt = 2.5;

constexpr float kTop = 72.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 270.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 66.0f;
constexpr float kMargin = 28.0f;
constexpr float kBandAlpha = 0.84f;

// OSC: the wind and the gust.
constexpr float kWindEase = 0.45f;  // how long the sky takes to reach a reading
constexpr float kGustRise = 0.18f;  // how long a gust takes to rise
constexpr float kGustFall = 1.0f;   // how long it falls, told nothing else
constexpr float kGustPush = 240.0f;  // px a second a full gust adds
constexpr float kWindSpan = 34.0f;  // the reading the drawn fader reads as full

// MIDI: eight cells, four across, and the light written back.
constexpr size_t kCells = 8;
constexpr size_t kColumns = 4;
constexpr int kFirstNote = 36;      // the note the first pad sends
constexpr int kLightChannel = 1;    // the channel the lights go back on
constexpr int kLightVelocity = 127;  // full: the colour every pad agrees on
constexpr float kStrike = 0.05f;     // how fast a struck pad reaches its glow
constexpr float kRelease = 0.32f;    // how long a released pad takes to go out
constexpr float kLitAt = 0.35f;      // a cell counts as lit above this

// ART-NET: the patch, and the universe that goes back.
constexpr int kLitUniverse = 0;
constexpr int kAnsweringUniverse = 1;
constexpr int kFirstPatch = 1;  // the wash: red, green, blue from here
constexpr int kStrobeChannel = kFirstPatch + 4;
constexpr size_t kShownChannels = 8;
constexpr float kFull = 255.0f;
constexpr float kWashFade = 0.12f;
constexpr float kStrobeSlowest = 1.0f;
constexpr float kStrobeFastest = 14.0f;
constexpr float kFlashOpen = 0.36f;  // of one flash the lamp stands open for
constexpr float kFlashShut = 0.14f;  // the floor between two flashes
constexpr size_t kLamps = 3;         // lamps of red, green and blue sent back
constexpr int kSequenceCeiling = 255;

/** A BAND'S OWN SHAPE, which no device sends: how thick it is, how fast
 *  it runs against or with the wind, how far it breathes, and what
 *  weight of the wash it is lit at. */
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

/** The three colours the cells light in, in turn down the grid. */
constexpr std::array<material::Color, 3> kCellTints = {
    {{0.42f, 0.78f, 0.94f, 1},
     {0.96f, 0.58f, 0.42f, 1},
     {0.68f, 0.52f, 0.95f, 1}}};

/** One pad's cell: its glow, a motion the ticker owns, and whether the
 *  lights were last told it is lit. */
struct Cell {
  motion::Animatable<float> glow = motion::animatable(0.0f);
  bool shown = false;
};

/** WHAT THE READINGS WERE WRITTEN FROM, and the whole of what says to
 *  write them again: each door's own state, and what came through it.
 *  Nothing continuous is in here — the wind, the glows and the wash move
 *  every frame and are drawn by the pen. */
struct Reading {
  data::ConnectionState desk, pads, lighting;
  uint64_t gusts = 0, replies = 0;
  uint64_t strikes = 0, lights = 0;
  std::string lit;
  int universe = -1;
  std::string dimmers;
  uint64_t answered = 0;
  bool operator==(const Reading&) const = default;
};

float brightness(const material::Color& colour) {
  return 0.2126f * colour.r + 0.7152f * colour.g + 0.0722f * colour.b;
}

int levelOf(float unit) {
  return (int)std::lround(std::clamp(unit, 0.0f, 1.0f) * kFull);
}

}  // namespace

namespace {

struct ControlSurfaces {
  data::Connection desk, pads, lights, lighting, lightingAnswer;
  /** The session's ticker, which outlives this sketch: a handler runs on
   *  the hub's dispatch, not inside a describe. */
  motion::Engine* ticker = nullptr;

  motion::Animatable<float> wind = motion::animatable(0.0f);
  motion::Animatable<float> gust = motion::animatable(0.0f);
  std::array<Cell, kCells> cells;
  motion::Animatable<float> red = motion::animatable(0.46f);
  motion::Animatable<float> green = motion::animatable(0.64f);
  motion::Animatable<float> blue = motion::animatable(0.92f);
  float strobe = 0;  // flashes a second, 0 for a lamp standing on

  /** How far the sky has travelled, integrated, because a speed that
   *  changes would rewrite where the sky has already been. */
  double drift = 0;
  double seconds = 0;

  uint64_t gusts = 0, replies = 0, strikes = 0, lightsSent = 0, answered = 0;
  std::array<int, kLamps * 3> told{};
  int sequence = 0;
  Reading shown;

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide sheet(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(
        ctx, {.size = kPage, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.engine;

    io::Hub& hub = ctx.assets.hub();
    const std::string arriving = std::string(kPads) + kController;
    // A CAPTURE READS THE RECORDINGS BESIDE THIS FILE, a window listens.
    // Replaying a URI from a file is the whole of the difference.
    if (ctx.deterministic) {
      hub.replay(kDesk, ctx.local(kDeskRecording));
      hub.replay(arriving, ctx.local(kPadsRecording));
      hub.replay(kLightingDesk, ctx.local(kLightingRecording));
    }

    wind = 0.0f;
    gust = 0.0f;
    for (Cell& cell : cells) {
      cell.glow = 0.0f;
      cell.shown = false;
    }
    red = 0.46f;
    green = 0.64f;
    blue = 0.92f;
    strobe = 0;
    drift = 0;
    seconds = 0;
    gusts = replies = strikes = lightsSent = answered = 0;
    told.fill(-1);
    sequence = 0;
    shown = {};

    desk = data::connect(hub, kDesk);
    desk.on("/sky/wind",
            [this](const data::Message& message) { fader(message.payload); });
    desk.on("/sky/gust",
            [this](const data::Message& message) { burst(message.payload); });
    pads = data::connect(hub, arriving);
    pads.on("NoteOn",
            [this](const data::Message& message) { strike(message.payload); });
    pads.on("NoteOff",
            [this](const data::Message& message) { release(message.payload); });
    lighting = data::connect(hub, kLightingDesk);
    lighting.on("Dmx",
                [this](const data::Message& message) { wash(message.payload); });

    // THE WAYS BACK ARE A WINDOW'S ALONE: a plate that opened a port
    // would depend on what happened to be plugged in or listening.
    lights = ctx.deterministic
                 ? data::Connection()
                 : data::connect(hub, std::string(kLights) + kController);
    lightingAnswer = ctx.deterministic ? data::Connection()
                                       : data::connect(hub, kLightingAnswer);

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::Scene2DContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += ((double)wind.value() + (double)gust.value() * kGustPush) * step;
    relightPads();
    relightDesk();
    if (read()) describe(ctx);
  }

  // ---- OSC ---------------------------------------------------------------

  /** STATE, eased to — and answered, to the sender that moved it, with
   *  where the easing has REACHED: the wind the bands drift on, which is
   *  what a motorised fader follows. A recording has no sender, so under
   *  a capture the answer is refused and nothing goes out. */
  void fader(const data::Json& message) {
    if (desk.reply(data::oscMessage("/sky/state",
                                    data::Json::Array{(double)wind.value()})))
      ++replies;
    if (!ticker) return;
    ticker->animate(wind, {.to = (float)message["arguments"][0].number(),
                           .duration = motion::Duration(kWindEase),
                           .ease = motion::ease::outQuad});
  }

  /** AN EVENT: it rises to the strength asked for and falls away over the
   *  seconds given, and nothing has to arrive for it to end. */
  void burst(const data::Json& message) {
    ++gusts;
    if (!ticker) return;
    const float strength = (float)message["arguments"][0].number();
    const float falls = (float)message["arguments"][1].number(kGustFall);
    ticker->timeline()
        .add(gust, {.to = strength,
                    .duration = motion::Duration(kGustRise),
                    .ease = motion::ease::outQuad})
        .add(gust, {.to = 0.0f,
                    .duration = motion::Duration(std::max(falls, kGustRise)),
                    .ease = motion::ease::inQuad});
  }

  // ---- MIDI --------------------------------------------------------------

  /** Which cell a note is. Which pad sends which note is the controller's
   *  own business, so a note outside the eight folds back into them. */
  static size_t cellOf(int note) {
    const int away = note - kFirstNote;
    return (size_t)(((away % (int)kCells) + (int)kCells) % (int)kCells);
  }

  void strike(const data::Json& message) {
    ++strikes;
    if (!ticker) return;
    ticker->animate(
        cells[cellOf((int)message["note"].number())].glow,
        {.to = std::clamp((float)message["velocity"].number() / 127.0f, 0.0f,
                          1.0f),
         .duration = motion::Duration(kStrike),
         .ease = motion::ease::outQuad});
  }

  /** The pad came up. A keyboard says this with a note on at no velocity,
   *  which the codec reads as the release it is. */
  void release(const data::Json& message) {
    if (!ticker) return;
    ticker->animate(cells[cellOf((int)message["note"].number())].glow,
                    {.to = 0.0f,
                     .duration = motion::Duration(kRelease),
                     .ease = motion::ease::inQuad});
  }

  /** The lights: a note on for every cell that has just lit and a note
   *  off for every one just gone dark. */
  void relightPads() {
    for (size_t index = 0; index != kCells; ++index) {
      Cell& cell = cells[index];
      const bool now = cell.glow.value() >= kLitAt;
      if (now == cell.shown) continue;
      cell.shown = now;
      if (lights.send(data::Json(data::Json::Object{
              {"kind", data::Json(now ? "NoteOn" : "NoteOff")},
              {"channel", data::Json(kLightChannel)},
              {"note", data::Json(kFirstNote + (int)index)},
              {"velocity", data::Json(now ? kLightVelocity : 0)}})))
        ++lightsSent;
    }
  }

  // ---- ART-NET -----------------------------------------------------------

  /** The dimmer a desk calls @p number, as a fraction of full; a channel
   *  the packet did not reach stands at nothing. */
  static float dimmer(const data::Json& channels, int number) {
    return (float)channels[(size_t)(number - 1)].number() / kFull;
  }

  /** A universe arrived. A fixture answers to the channels it is patched
   *  to and leaves the rest to whoever else is on the wire; a strobe is a
   *  rate and is taken as it arrives, the wash is a level and is faded. */
  void wash(const data::Json& message) {
    if ((int)message["universe"].number() != kLitUniverse) return;
    const data::Json& channels = message["channels"];
    const float rate = dimmer(channels, kStrobeChannel);
    strobe = rate <= 0.0f
                 ? 0.0f
                 : kStrobeSlowest + (kStrobeFastest - kStrobeSlowest) * rate;
    if (!ticker) return;
    const auto fade = [this](motion::Animatable<float>& value, float to) {
      ticker->animate(value, {.to = to,
                              .duration = motion::Duration(kWashFade),
                              .ease = motion::ease::outQuad});
    };
    fade(red, dimmer(channels, kFirstPatch));
    fade(green, dimmer(channels, kFirstPatch + 1));
    fade(blue, dimmer(channels, kFirstPatch + 2));
  }

  /** How far open the lamp stands this instant: a strobe shuts rather
   *  than darkens, so between two flashes is a floor and not a fade. */
  float flash() const {
    if (strobe <= 0.0f) return 1.0f;
    const double turn = seconds * (double)strobe;
    return turn - std::floor(turn) < (double)kFlashOpen ? 1.0f : kFlashShut;
  }

  /** WHAT BAND @p index IS LIT IN, which the drawing and the universe sent
   *  back both read: a desk lighting a room from a canvas cannot be told
   *  two different things. */
  material::Color tint(size_t index) const {
    const float weight = kBands[index].weight * flash();
    return {red.value() * weight, green.value() * weight, blue.value() * weight,
            kBandAlpha};
  }

  /** The universe that goes back: the three brightest band colours as
   *  three lamps, sent when a level changed and at no other time. */
  void relightDesk() {
    std::array<material::Color, kBands.size()> lit{};
    for (size_t index = 0; index != kBands.size(); ++index)
      lit[index] = tint(index);
    std::sort(lit.begin(), lit.end(),
              [](const material::Color& first, const material::Color& second) {
                return brightness(first) > brightness(second);
              });
    std::array<int, kLamps * 3> levels{};
    for (size_t lamp = 0; lamp != kLamps; ++lamp) {
      levels[lamp * 3] = levelOf(lit[lamp].r);
      levels[lamp * 3 + 1] = levelOf(lit[lamp].g);
      levels[lamp * 3 + 2] = levelOf(lit[lamp].b);
    }
    if (levels == told) return;
    told = levels;
    data::Json::Array channels(levels.begin(), levels.end());
    // The sequence comes round to one rather than to nothing: 0 is what a
    // sender that does not count writes.
    sequence = sequence % kSequenceCeiling + 1;
    if (lightingAnswer.send(data::Json(
            data::Json::Object{{"kind", data::Json("Dmx")},
                               {"universe", data::Json(kAnsweringUniverse)},
                               {"sequence", data::Json(sequence)},
                               {"channels", data::Json(std::move(channels))}})))
      ++answered;
  }

  // ---- the readings --------------------------------------------------------

  /** Takes what the doors answer, and says whether the readings standing
   *  now were written from something else. */
  bool read() {
    std::string lit;
    for (const Cell& cell : cells) lit += cell.shown ? '#' : '.';
    const data::Json& newest = lighting.latest("Dmx").payload;
    std::string dimmers;
    for (size_t at = 0; at != kShownChannels; ++at)
      dimmers += at < newest["channels"].size()
                     ? compose::kit::formatted(
                           "%4d", (int)newest["channels"][at].number())
                     : std::string("   -");
    Reading now{.desk = desk.state(),
                .pads = pads.state(),
                .lighting = lighting.state(),
                .gusts = gusts,
                .replies = replies,
                .strikes = strikes,
                .lights = lightsSent,
                .lit = std::move(lit),
                .universe = newest.null() ? -1
                                          : (int)newest["universe"].number(),
                .dimmers = std::move(dimmers),
                .answered = answered};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide sheet(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // A paint program runs after the describe scope has closed, so the
    // theme's colours are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const material::Color rule = look.palette.rule;
    const material::Color figure = look.palette.figure;
    const material::Color ash = look.palette.ash;
    compose::Element picture =
        compose::stack()
            .width(kCanvas.width())
            .height(kCanvas.height())
            .children({compose::pen("control_surfaces.sky", [this, rule, figure,
                                                              ash](Pen& pen) {
                         sky(pen);
                         grid(pen, rule);
                         faders(pen, rule, figure, ash);
                         rack(pen, rule, figure, ash);
                       }).cover()});
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "Three doors that answer back.",
                  .subtitle = "OSC  ·  MIDI  ·  ART-NET  /  The desk's fader "
                              "steers the wind, the pads light the cells, the "
                              "lighting desk washes the bands.",
                  .footer = "desk.py, pads.py and console.py beside this "
                            "sketch are the devices  ·  Captures replay their "
                            "recordings and answer nobody"},
         .pictureSize = kCanvas,
         .pictureWidth = 760,
         .readingsLabel = "DOORS",
         .note = "Each door is answered its own way: OSC reply() to the "
                 "sender, MIDI notes to the pads' LEDs, an Art-Net universe "
                 "of the canvas's brightest colours."},
        std::move(picture).fill(kGround), readings()));
  }

  /** The bands: ribbons of segments carried on the one drift the wind and
   *  the gusts have built, each breathing by its own wobble and lit at its
   *  own weight of the wash. */
  void sky(Pen& pen) const {
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

  /** THE EIGHT CELLS the pads light: a tile each, glowing from the middle
   *  out while its pad is down, the halo laid as washes one inside the
   *  next. */
  void grid(Pen& pen, material::Color rule) const {
    constexpr float kTileGap = 18.0f;
    constexpr float kTile = 112.0f;
    constexpr float kTall = 80.0f;
    constexpr float kCorner = 12.0f;
    const float rows = (float)(kCells / kColumns);
    const float across = kTile * kColumns + kTileGap * (kColumns - 1);
    const float left = (pen.width - across) * 0.5f;
    const float top = (pen.height - (kTall * rows + kTileGap * (rows - 1))) *
                      0.5f;
    pen.noStroke();
    for (size_t index = 0; index != kCells; ++index) {
      const float x = left + (float)(index % kColumns) * (kTile + kTileGap);
      const float y = top + (float)(index / kColumns) * (kTall + kTileGap);
      const float glow = std::clamp(cells[index].glow.value(), 0.0f, 1.0f);
      material::Color ground = rule;
      ground.a *= 0.72f;
      pen.fill(ground);
      pen.rect(x, y, kTile, kTall, kCorner);
      if (glow <= 0.0f) continue;
      material::Color tint = kCellTints[index % kCellTints.size()];
      for (int step = 6; step != 0; --step) {
        const float out = (float)step * 8.0f;
        tint.a = glow * 0.055f;
        pen.fill(tint);
        pen.rect(x - out, y - out, kTile + out * 2, kTall + out * 2,
                 kCorner + out);
      }
      tint.a = glow;
      pen.fill(tint);
      pen.rect(x, y, kTile, kTall, kCorner);
    }
  }

  /** THE WIND AND THE GUST, where a live number belongs: the wind either
   *  side of a centre mark, since still air is the middle of a fader's
   *  travel, and the gust from nothing to full. */
  void faders(Pen& pen, material::Color rule, material::Color figure,
              material::Color ash) const {
    constexpr float kLength = 220.0f;
    constexpr float kThickness = 6.0f;
    const float left = kMargin + 56.0f;
    const float base = pen.height - kMargin;
    const float above = base - kThickness - 18.0f;
    const float centre = left + kLength * 0.5f;
    const float reach =
        std::clamp(wind.value() / kWindSpan, -1.0f, 1.0f) * kLength * 0.5f;
    pen.noStroke();
    pen.textSize(10);
    pen.fill(rule);
    pen.rect(left, base - kThickness, kLength, kThickness, kThickness * 0.5f);
    pen.rect(left, above - kThickness, kLength, kThickness, kThickness * 0.5f);
    pen.fill(figure);
    pen.rect(std::min(centre, centre + reach), base - kThickness,
             std::abs(reach), kThickness, kThickness * 0.5f);
    pen.circle(centre + reach, base - kThickness * 0.5f, kThickness * 2.2f);
    pen.rect(left, above - kThickness,
             kLength * std::clamp(gust.value(), 0.0f, 1.0f), kThickness,
             kThickness * 0.5f);
    pen.fill(ash);
    pen.text("WIND", kMargin, base);
    pen.text("GUST", kMargin, above);
  }

  /** THE HEAD OF THE UNIVERSE as a rack of faders, the channels this
   *  fixture is patched to in the figure's ink and the rest in ash. */
  void rack(Pen& pen, material::Color rule, material::Color figure,
            material::Color ash) const {
    constexpr float kHeight = 110.0f;
    constexpr float kWidth = 20.0f;
    constexpr float kColumnGap = 11.0f;
    const data::Json& channels = lighting.latest("Dmx")["channels"];
    const float right = pen.width - kMargin;
    const float base = pen.height - kMargin - 14.0f;
    const float left =
        right - (kWidth + kColumnGap) * (float)kShownChannels + kColumnGap;
    pen.noStroke();
    pen.textSize(9);
    for (size_t at = 0; at != kShownChannels; ++at) {
      const float x = left + (float)at * (kWidth + kColumnGap);
      const int number = (int)at + 1;
      const bool patched = (number >= kFirstPatch && number < kFirstPatch + 3) ||
                           number == kStrobeChannel;
      pen.fill(rule);
      pen.rect(x, base - kHeight, kWidth, kHeight, kWidth * 0.5f);
      const float reach =
          kHeight *
          std::clamp((float)channels[at].number() / kFull, 0.0f, 1.0f);
      pen.fill(patched ? figure : ash);
      pen.rect(x, base - reach, kWidth, reach, kWidth * 0.5f);
      pen.fill(ash);
      pen.text(compose::kit::formatted("%d", number), x + kWidth * 0.25f,
               base + 14.0f);
    }
  }

  /** EACH DOOR'S OWN WORDS, under its name: where it is, whether it
   *  stands, its revision, what it dropped, who spoke last — then what
   *  came through it and what went back. */
  compose::Element readings() const {
    const compose::kit::Rows rows{.measure = 380};
    return compose::box().column().gap(18).children(
        {compose::document::eyebrow("OSC · REPLY TO THE SENDER"),
         sketch::kit::connectionReadout(
             desk,
             {.door = kDesk,
              .rows = rows,
              .readings = {{.name = u8"gusts",
                            .value = std::to_string(shown.gusts)},
                           {.name = u8"replies",
                            .value = std::to_string(shown.replies)}}}),
         compose::document::eyebrow("MIDI · LIGHTS BACK TO THE PADS"),
         sketch::kit::connectionReadout(
             pads,
             {.door = kController[0] == '\0' ? "(the first one plugged in)"
                                             : kController,
              .rows = rows,
              .readings = {{.name = u8"struck",
                            .value = std::to_string(shown.strikes)},
                           {.name = u8"lit", .value = shown.lit},
                           {.name = u8"lights",
                            .value = std::to_string(shown.lights)}}}),
         compose::document::eyebrow("ART-NET · A UNIVERSE BACK"),
         sketch::kit::connectionReadout(
             lighting,
             {.door = kLightingDesk,
              .rows = rows,
              .readings = {{.name = u8"universe",
                            .value = shown.universe < 0
                                         ? std::string("nothing has arrived")
                                         : std::to_string(shown.universe)},
                           {.name = u8"channels", .value = shown.dimmers},
                           {.name = u8"answered",
                            .value = std::to_string(shown.answered)}}})});
  }
};

}  // namespace

SIGIL_SKETCH(ControlSurfaces, "Data",
             "One sky and three devices that steer it and are answered: an "
             "OSC desk's fader eases the wind and is replied to, MIDI pads "
             "light eight cells and get their LEDs back, and a lighting "
             "desk's Art-Net wash colours the bands while the canvas's "
             "brightest colours go back as a universe.")
