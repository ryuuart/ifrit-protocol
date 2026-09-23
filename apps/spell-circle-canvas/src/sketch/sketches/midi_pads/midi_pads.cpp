/** @file
 * midi_pads — the controller on the desk, both ways.
 *
 * A MIDI controller is the thing standing beside the screen with pads
 * to strike and a knob to turn, and this sketch is the other end of its
 * cable. A `data::Connection` on a `midi://` door hands a reader each
 * message as a value — its kind, the channel it was played on, and the
 * fields that kind carries — so nothing here reads a byte, and three
 * handlers registered on it are the whole of the wiring.
 *
 * THE THREE THINGS A CONTROLLER SAYS. A `NoteOn` is an EVENT: a pad was
 * struck, this hard, and what is left of it is a cell lit on the canvas.
 * A `ControlChange` is STATE: the knob stands here now, true until it
 * is moved again, which the sky eases its wind to. A `NoteOff` is the
 * pad coming back up, and the cell fades away — a keyboard says that
 * with a note on at no velocity, and the codec reads that as the
 * release it is, so a scene never has to know the difference.
 *
 * AND THE CABLE RUNS BOTH WAYS. A pad controller lights its own pads
 * from whatever is sent BACK to it, so a second door — `midi://out/` —
 * carries a note on for every cell the scene lights and a note off for
 * every one that goes dark, and the LEDs under the fingers follow the
 * canvas. What goes out is the DIFFERENCE alone: a controller told the
 * same thing sixty times a second is a cable carrying nothing new. A
 * capture has no controller to light, so it opens no output at all and
 * replays the recording beside this file instead.
 *
 * `pads.py` beside this file writes that recording. There is no MIDI in
 * Python's standard library, so it is the recording alone: live use is
 * a controller plugged in, or any application with a virtual MIDI
 * output.
 *
 *     python3 pads.py --record data/pads.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kController  a piece of your controller's name, empty for the first
 *   kFirstNote   the note the first pad sends
 *   kRecording   what a capture replays instead of listening
 *   kKnob        which controller number the knob sends on
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

/** WHICH CONTROLLER. A piece of the port's own name, matched with the
 *  case left out of it — "launchpad", "nanoPAD", "MPK" — and nothing at
 *  all is the first controller plugged in, which is the one on a desk
 *  that has one. A name no port answers to leaves the sentence saying
 *  which ports there are on the readout below. */
const char* kController = "";

const char* kPadsIn = "midi://in/";         // the pads and the knob, arriving
const char* kLightsOut = "midi://out/";     // the same controller's lights
const char* kRecording = "data/pads.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.04f, 0.04f, 0.09f, 1};
/** Three pads are down and the knob stands mid-way by here. */
constexpr double kCaptureAt = 3.0;

/** HOW MANY CELLS, and how they stand: four across and two down, which
 *  is the shape of the pad grid under a hand. */
constexpr size_t kCells = 8;
constexpr size_t kColumns = 4;

/** The note the first pad sends. Which pad sends which note is the
 *  controller's own business, so a note outside the eight is folded
 *  back into them rather than refused: every pad on the grid lights
 *  something. */
constexpr int kFirstNote = 36;
/** The controller number a knob turns on, and the channel the lights
 *  are written back on. */
constexpr int kKnob = 74;
constexpr int kChannel = 1;

constexpr float kTop = 92.0f;       // where the first band stands
constexpr float kSpacing = 84.0f;   // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

constexpr float kStrike = 0.05f;     // how fast a struck pad reaches its glow
constexpr float kFade = 0.32f;       // how long a released pad takes to go out
constexpr float kWindEase = 0.5f;    // how long the sky takes to reach the knob
constexpr float kWindSpan = 150.0f;  // px a second at the top of the knob
constexpr float kBandAlpha = 0.72f;  // what a band is drawn at

/** WHEN A CELL COUNTS AS LIT, which is what the lights are written
 *  from: a cell on its way out stops being lit well before it stops
 *  being drawn, so a controller's LED goes off with the finger rather
 *  than with the last of the fade. */
constexpr float kLitAt = 0.35f;
/** How hard a light is written back. A pad controller reads the
 *  velocity of what it is sent as which colour to light, and full is
 *  the one every controller agrees on. */
constexpr int kLightVelocity = 127;

/** The three colours the cells are lit in, taken in turn down the grid
 *  so two pads struck side by side are told apart. */
constexpr size_t kTints = 3;
constexpr std::array<material::Color, kTints> kLitTints = {
    {{0.42f, 0.78f, 0.94f, 1},
     {0.96f, 0.58f, 0.42f, 1},
     {0.68f, 0.52f, 0.95f, 1}}};

/** A BAND'S OWN SHAPE, which no message carries: how thick it is, how
 *  fast it runs against or with the wind, and how far it breathes. */
struct Band {
  float height;
  float speed;
  float wobble;
};

constexpr std::array<Band, 6> kBands = {{{48, -16, 15},
                                         {30, 22, 10},
                                         {60, -10, 19},
                                         {36, 17, 12},
                                         {52, -24, 17},
                                         {26, 12, 8}}};

/** ONE CELL: how brightly it is lit, how hard the pad was struck, and
 *  whether the lights have been told about it. The glow is a motion the
 *  ticker owns, so a pad rises the moment it is struck and falls away
 *  by itself; the strike is a plain number, being over the moment it
 *  arrived. */
struct Cell {
  ch::Output<float> glow{0};
  float strike = 0;
  /** What the lights were last told, which is what says whether there
   *  is anything to tell them. */
  bool shown = false;
};

/** WHAT THE READOUT SAYS, and the whole of what says to write it again.
 *  Nothing continuous is in here: the glows and the wind move every
 *  frame, and a description rebuilt every frame is a description
 *  nothing can be pruned against. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  uint64_t strikes = 0;
  uint64_t turns = 0;
  uint64_t lights = 0;
  std::string address;
  std::string trouble;
  std::string lit;
  bool operator==(const Reading&) const = default;
};

/** One row of the readout: what it is called, and what it answers. */
compose::Element row(const char* name, const std::string& answer) {
  return compose::text(
      compose::kit::formatted("%-11s %s", name, answer.c_str()));
}

}  // namespace

namespace {

struct MidiPads {
  /** THE CONTROLLER'S DOOR: every message its pads and its knob send,
   *  read as MIDI because the URI says midi. The handlers below run on
   *  the hub's dispatch, which is the call the host already makes once
   *  a frame — so a pad struck between two frames is folded in before
   *  the frame it belongs to is drawn. */
  data::Connection pads;
  /** THE WAY BACK: the same controller's lights. A capture has no
   *  controller to light, so it holds a connection onto nothing, which
   *  every send is false on. */
  data::Connection lights;
  /** The session's ticker, which outlives this sketch. A handler runs
   *  on the dispatch rather than inside a describe, so it cannot hold
   *  the per-frame context the ticker is otherwise reached through. */
  motion::Ticker* ticker = nullptr;

  std::array<Cell, kCells> cells;
  /** The knob, eased: where it stands, reached over kWindEase rather
   *  than jumped to, so a knob turned in steps still drifts smoothly. */
  ch::Output<float> wind{0};

  /** How far the sky has travelled, in px. Integrated rather than read
   *  as a speed times the clock, because a speed that changes would
   *  rewrite where the sky has already been. */
  double drift = 0;
  /** The scene time the last frame stood at, for the step between. */
  double seconds = 0;

  uint64_t strikes = 0;
  uint64_t turns = 0;
  uint64_t sent = 0;
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    ticker = &ctx.ticker;

    io::Hub& hub = ctx.assets.hub();
    const std::string arriving = std::string(kPadsIn) + kController;
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window opens the
    // cable. Mounting the URI onto the file is the whole of the
    // difference, and the mount stands first because a feed is made once
    // per URI and every later ask answers that same one.
    if (ctx.deterministic)
      hub.mount(arriving, hub.resolve(ctx.local(kRecording)));

    for (Cell& cell : cells) {
      cell.glow = 0.0f;
      cell.strike = 0;
      cell.shown = false;
    }
    wind = 0.0f;
    drift = 0;
    seconds = 0;
    strikes = 0;
    turns = 0;
    sent = 0;
    shown = {};

    pads = data::Connection(hub, arriving);
    pads.on("NoteOn", [this](const data::Json& message) { strike(message); });
    pads.on("NoteOff", [this](const data::Json& message) { release(message); });
    pads.on("ControlChange",
            [this](const data::Json& message) { turn(message); });

    // THE LIGHTS ARE A WINDOW'S ALONE. A capture has no controller in
    // front of it, and a plate that opened a port would depend on what
    // happened to be plugged in.
    lights = ctx.deterministic
                 ? data::Connection()
                 : data::Connection(hub, std::string(kLightsOut) + kController);

    read();
    describe(ctx);
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = elapsed - seconds;
    seconds = elapsed;
    drift += (double)wind() * step;
    relight();
    // The data path: the sky and the cells move without being described
    // again, because the pen reads them on every frame. What is
    // described again is the readout, and only when a figure changed.
    if (read()) describe(ctx);
  }

  /** WHICH CELL A NOTE IS. Which pad sends which note is the
   *  controller's own business — a grid may start at 36, at 60 or
   *  anywhere else — so a note outside the eight is folded back into
   *  them rather than dropped: every pad lights something. */
  static size_t cellOf(int note) {
    const int away = note - kFirstNote;
    const int folded = ((away % (int)kCells) + (int)kCells) % (int)kCells;
    return (size_t)folded;
  }

  /** AN EVENT: the pad was struck, this hard, and it is over. What is
   *  left of it is a cell rising to the brightness the strike asked
   *  for, which then stands until the pad comes up. */
  void strike(const data::Json& message) {
    ++strikes;
    if (!ticker) return;
    Cell& cell = cells[cellOf((int)message["note"].number())];
    cell.strike =
        std::clamp((float)message["velocity"].number() / 127.0f, 0.0f, 1.0f);
    ticker->timeline()
        .apply(&cell.glow)
        .then<ch::RampTo>(cell.strike, kStrike, &ch::easeOutQuad);
  }

  /** THE PAD CAME UP, and the cell falls away over kFade. A keyboard
   *  says this with a note on at no velocity, which the codec reads as
   *  the release it is, so both spellings arrive here. */
  void release(const data::Json& message) {
    if (!ticker) return;
    Cell& cell = cells[cellOf((int)message["note"].number())];
    ticker->timeline()
        .apply(&cell.glow)
        .then<ch::RampTo>(0.0f, kFade, &ch::easeInQuad);
  }

  /** STATE: the knob stands where it stands, and the sky eases to it.
   *  One knob is read and the rest are left alone — what any other
   *  controller number means is that controller's own arrangement. */
  void turn(const data::Json& message) {
    if ((int)message["controller"].number() != kKnob) return;
    ++turns;
    if (!ticker) return;
    const float across = (float)message["value"].number() / 127.0f;
    // The knob runs either side of still air, because a sky that only
    // drifts one way is a knob with half its travel wasted.
    ticker->timeline().apply(&wind).then<ch::RampTo>(
        (across * 2.0f - 1.0f) * kWindSpan, kWindEase, &ch::easeOutQuad);
  }

  /** THE LIGHTS, AND THE DIFFERENCE ALONE. An LED follows the scene, so
   *  what goes out is the cells that have just lit and the cells that
   *  have just gone dark; a controller told the same thing every frame
   *  is a cable carrying nothing new. A window with no controller
   *  plugged in sends into a door that says so, which is false and
   *  costs the frame nothing. */
  void relight() {
    for (size_t index = 0; index != kCells; ++index) {
      Cell& cell = cells[index];
      const bool now = cell.glow() >= kLitAt;
      if (now == cell.shown) continue;
      cell.shown = now;
      const bool wrote = lights.send(data::Json(data::Json::Object{
          {"kind", data::Json(now ? "NoteOn" : "NoteOff")},
          {"channel", data::Json(kChannel)},
          {"note", data::Json(kFirstNote + (int)index)},
          {"velocity", data::Json(now ? kLightVelocity : 0)}}));
      if (wrote) ++sent;
    }
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    std::string lit;
    for (size_t index = 0; index != kCells; ++index)
      lit += cells[index].shown ? '#' : '.';
    Reading now{.generation = pads.generation(),
                .undecodable = pads.undecodable(),
                .strikes = strikes,
                .turns = turns,
                .lights = sent,
                .address = pads.address(),
                .trouble = pads.error(),
                .lit = std::move(lit)};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where
    // the theme in force is no longer this page's, so the colours the
    // grid is drawn in are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const material::Color rule = look.palette.rule;
    const material::Color figure = look.palette.figure;
    compose::Element picture =
        compose::stack()
            .width(kCanvas.width())
            .height(kCanvas.height())
            .children(
                {compose::pen("midi_pads.room", [this, rule, figure](Pen& pen) {
                   sky(pen);
                   grid(pen, rule);
                   knob(pen, rule, figure);
                 }).cover()});
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A controller, both ways.",
                  .subtitle = "MIDI / INPUT + OUTPUT  /  Play a pad, turn a "
                              "knob, and send the light back.",
                  .footer =
                      "Connect a MIDI controller or virtual MIDI output  ·  "
                      "pads.py writes the capture recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "Eight pads light the grid. The knob steers the wind; lit "
                 "cells drive controller LEDs."},
        std::move(picture).fill(kGround), readout()));
  }

  /** THE SKY THE CELLS STAND OVER: bands carried on the drift the knob
   *  has built, each breathing by its own wobble. A band's OWN speed is
   *  a constant and so is read straight off the clock; only the wind,
   *  which changes, has to be integrated. */
  void sky(Pen& pen) {
    pen.noStroke();
    const float period = kSegment + kGap;
    size_t index = 0;
    for (const Band& band : kBands) {
      material::Color tint = kLitTints[index % kTints];
      tint.a = kBandAlpha * 0.32f;
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

  /** THE EIGHT CELLS: a tile each, standing dark until a pad is struck
   *  and glowing from the middle out while it is down. The halo is laid
   *  as washes one inside the next rather than as one bright edge,
   *  because what a lit pad looks like across a room is light in the
   *  air around it and not a border. */
  void grid(Pen& pen, material::Color rule) {
    constexpr float kTileGap = 18.0f;
    constexpr float kGridAcross = 0.56f;  // of the canvas, so the sky is a sky
    constexpr float kTileTall = 0.72f;    // of a tile's own width
    constexpr float kCorner = 12.0f;
    constexpr int kHalo = 6;           // washes around a lit tile
    constexpr float kHaloStep = 8.0f;  // how much further out each one goes
    const float rows = (float)(kCells / kColumns);
    const float across = pen.width * kGridAcross;
    const float tile =
        (across - kTileGap * (float)(kColumns - 1)) / (float)kColumns;
    const float height = tile * kTileTall;
    // Centred both ways, with air on every side: the grid stands IN the
    // sky rather than over it, and the readout keeps the corner.
    const float left = (pen.width - across) * 0.5f;
    const float top =
        (pen.height - (height * rows + kTileGap * (rows - 1.0f))) * 0.5f;
    pen.noStroke();
    for (size_t index = 0; index != kCells; ++index) {
      const float x = left + (float)(index % kColumns) * (tile + kTileGap);
      const float y = top + (float)(index / kColumns) * (height + kTileGap);
      const float glow = std::clamp(cells[index].glow(), 0.0f, 1.0f);
      // The tile itself, which stands whether or not anybody is playing:
      // a grid with nothing lit is still a grid, and eight of them are
      // what says how many pads there are to strike. It is laid over
      // the sky rather than in it, so the bands pass behind the grid.
      material::Color ground = rule;
      ground.a *= 0.72f;
      pen.fill(ground);
      pen.rect(x, y, tile, height, kCorner);
      if (glow <= 0.0f) continue;
      material::Color tint = kLitTints[index % kTints];
      for (int step = kHalo; step != 0; --step) {
        const float out = (float)step * kHaloStep;
        tint.a = glow * 0.055f;
        pen.fill(tint);
        pen.rect(x - out, y - out, tile + out * 2.0f, height + out * 2.0f,
                 kCorner + out);
      }
      tint.a = glow;
      pen.fill(tint);
      pen.rect(x, y, tile, height, kCorner);
    }
  }

  /** THE KNOB, where a live number belongs: one bar either side of
   *  centre, because still air is the middle of its travel and not the
   *  end of it. It is drawn rather than written because a description
   *  carrying a number that changes every frame is a description
   *  rebuilt every frame. */
  void knob(Pen& pen, material::Color rule, material::Color figure) {
    constexpr float kLength = 260.0f;
    constexpr float kThickness = 6.0f;
    const float right = pen.width - kMargin;
    const float base = pen.height - kMargin - kThickness;
    const float left = right - kLength;
    const float centre = left + kLength * 0.5f;
    const float reach =
        std::clamp(wind() / kWindSpan, -1.0f, 1.0f) * kLength * 0.5f;
    pen.noStroke();
    pen.fill(rule);
    pen.rect(left, base, kLength, kThickness, kThickness * 0.5f);
    pen.fill(figure);
    pen.rect(std::min(centre, centre + reach), base, std::abs(reach),
             kThickness, kThickness * 0.5f);
    pen.circle(centre + reach, base + kThickness * 0.5f, kThickness * 2.2f);
  }

  /** WHAT THE CONNECTION ANSWERS, in the reading panel: how many
   *  messages have arrived, how many of them were no MIDI message at
   *  all, how many pads have been struck and how far the knob has been
   *  turned, which cells are lit, how many lights have gone back down
   *  the cable, and where the door is — or the sentence saying why
   *  there is none, which on a cable is the list of the ports that do
   *  exist. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("controller", kController[0] == '\0' ? "(the first one plugged in)"
                                                 : kController),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable)),
        row("struck", compose::kit::formatted("%llu  ·  knob %llu",
                                              (unsigned long long)shown.strikes,
                                              (unsigned long long)shown.turns)),
        row("lit", shown.lit),
        row("lights",
            compose::kit::formatted("%llu", (unsigned long long)shown.lights))};
    if (!shown.trouble.empty())
      lines.push_back(row("error", shown.trouble));
    else
      lines.push_back(row("port", shown.address.empty() ? "-" : shown.address));
    return compose::box()
        .column()
        .gap(12)
        .font(look.font({.size = 12, .mono = true}))
        .ink(look.palette.ink)
        .children(std::move(lines));
  }
};

}  // namespace

SIGIL_SKETCH(MidiPads, "Data",
             "The controller on the desk, both ways: its pads light eight "
             "cells and its knob eases the wind the sky drifts on, while "
             "every cell that lights writes a note back down the cable for "
             "the pad's own LED to follow.")
