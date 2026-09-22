/** @file
 * serial_sensor — a board on a cable, printing one line per reading.
 *
 * THE OLDEST WIRE THERE IS. A sensor is soldered to a board, the board
 * is plugged into this machine, and the whole protocol is that the
 * board prints a line whenever it has something to say:
 *
 *     {"lux": 412, "tilt": -3.2}
 *
 * There is no port to bind, no peer to dial and no region to map — a
 * device file, a baud rate, and lines. The door is opened exactly as
 * every other door in this tree is, on the hub and by URI, so the
 * scene below reads a cable the way it would read a socket: the newest
 * message, and a readout of what the door itself says.
 *
 * TWO READINGS, TWO THINGS THEY DO. The light is the BRIGHTNESS of the
 * ribbon field — a dark room dims the sky and a lamp brings it up — and
 * the tilt is its LEAN, the whole field turning with the board. Each is
 * reached over a moment rather than taken whole, because a sensor
 * answers in steps and a sky does not jump: what is drawn is the
 * readings and not the sampling.
 *
 * ONE DOOR, TWO SOURCES. A URI that resolves through the mount table to
 * a file is played back from that recording as the hub's time moves
 * forward, and any other opens through the transport its scheme names.
 * So a capture mounts the recording beside this file onto the port's
 * URI and reads exactly what a window reads from a live board — arrival
 * for arrival, at the same seconds. The mount stands before the door
 * opens, because a feed is made once per URI and every later ask
 * answers that same one.
 *
 * `sensor.py` beside this file is the board, for a desk that has none:
 * it makes a pseudo-terminal pair, prints the path to open this sketch
 * on, and writes a reading into it ten times a second.
 *
 *     python3 sensor.py                                 # a window moves
 *     python3 sensor.py --record data/readings.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kPort       the device the board is plugged into, and its baud rate
 *   kRecording  what a capture replays instead of opening the port
 *   kFullLight  the reading the sky is at its brightest for
 *   kFullLean   the reading the sky leans over the whole of
 */

// TAGS: Data/Sources, Runtime/Resources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Instrument.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;

using sigil::draw::Pen;

namespace {

// EDIT THESE FIRST: the board on the desk, and the file that stands in
// for it. A device is named differently on every machine — the number
// at the end of this one is the order it was plugged in — so this line
// is the one a person changes before a window can read their own board.
const char* kPort = "serial:///dev/tty.usbmodem1101?baud=115200";
const char* kRecording = "data/readings.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.03f, 0.035f, 0.06f, 1};
/** By here the light has climbed and the board has leaned, which is the
 *  frame that shows both readings doing their work at once. */
constexpr double kCaptureAt = 3.0;

/** WHAT THE READINGS MEAN IN THE PICTURE. A reading of kFullLight is a
 *  sky at its brightest and nothing is a sky barely lit; a tilt of
 *  kFullLean degrees is the whole lean the field has to give. */
constexpr double kFullLight = 900.0;
constexpr double kFullLean = 30.0;
constexpr float kDimmest = 0.16f;  // what the sky is lit to in the dark

/** How long a reading takes to be reached. A board answers in steps
 *  and this is what turns them back into a sky. */
constexpr double kEase = 0.28;

constexpr float kTop = -260.0f;      // the field stands past the canvas…
constexpr float kOverhang = 260.0f;  // …by this much, so a lean shows no edge
constexpr float kSpacing = 88.0f;    // how far apart the ribbons stand
constexpr float kSegment = 240.0f;   // a ribbon's segment, and its gap after
constexpr float kGap = 64.0f;
constexpr float kDrift = 26.0f;  // what the whole field carries, px a second
constexpr float kMargin = 28.0f;

/** A RIBBON'S OWN SHAPE, which no reading carries: how thick it is, how
 *  fast it runs against or with the field, and how far it breathes. The
 *  rows take these in turn, so a field of any depth is built from
 *  six. */
struct Ribbon {
  float height;
  float speed;
  float wobble;
};

constexpr std::array<Ribbon, 6> kRibbons = {{{46, -14, 13},
                                             {30, 20, 9},
                                             {58, -9, 17},
                                             {36, 16, 11},
                                             {50, -22, 15},
                                             {26, 11, 7}}};

/** The colours the ribbons are tinted from, in turn, at full light. */
constexpr std::array<material::Color, 3> kTints = {
    {{0.46f, 0.66f, 0.94f, 0.80f},
     {0.58f, 0.52f, 0.90f, 0.76f},
     {0.38f, 0.78f, 0.86f, 0.72f}}};

/** WHAT THE READOUT SAYS, and the whole of what says to write it again:
 *  the door's own figures, and the newest reading as the board wrote
 *  it. */
struct Reading {
  uint64_t generation = 0;
  uint64_t undecodable = 0;
  bool closed = false;
  double lux = 0;
  double tilt = 0;
  bool heard = false;
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

struct SerialSensor {
  /** THE DOOR: a device file read as JSON, one message per line. The
   *  same feed for the same URI while anybody holds it, so a reload
   *  that sets this sketch up again is handed the one it was already
   *  reading. */
  data::Connection sensor;

  /** What the board last said, and what the sky has caught up to. The
   *  two are apart for as long as a reading takes to be reached. */
  double readLux = 0;
  double readTilt = 0;
  bool heard = false;
  double light = 0;
  double lean = 0;

  /** How far the field has carried, in px, and the scene time the last
   *  frame stood at. */
  double carried = 0;
  double seconds = 0;

  /** What the description standing now was written from. */
  Reading shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window opens the
    // port. Mounting the URI onto the file is the whole of the
    // difference: the line after it opens either one.
    if (ctx.deterministic) hub.mount(kPort, hub.resolve(ctx.local(kRecording)));
    sensor = data::Connection(hub, kPort);

    readLux = 0;
    readTilt = 0;
    heard = false;
    light = 0;
    lean = 0;
    carried = 0;
    seconds = 0;
    shown = {};
    read();
    describe(ctx);
  }

  /** The data path: the sky and the field move without being described
   *  again, because the pen reads them on every frame. What is
   *  described again is the readout, and only when a figure changed. */
  void update(double elapsed, sketch::SketchContext& ctx) {
    const double step = std::max(0.0, elapsed - seconds);
    seconds = elapsed;
    listen();
    // Each reading is REACHED rather than taken, and a board reading
    // past its own scale brightens or leans no further rather than off
    // the canvas. The share of the way covered in a step is the same
    // whatever the step was, so a slow frame and a fast one land the
    // sky in the same place.
    const double caught = 1.0 - std::exp(-step / kEase);
    light += (std::clamp(readLux / kFullLight, 0.0, 1.0) - light) * caught;
    lean += (std::clamp(readTilt / kFullLean, -1.0, 1.0) - lean) * caught;
    carried += kDrift * step;
    if (read()) describe(ctx);
  }

  /** The newest line, read where the board wrote it. A message that
   *  carries only one of the two leaves the other where it was, which
   *  is what a board reporting one sensor at a time writes. */
  void listen() {
    const data::Json& newest = sensor.latest();
    if (newest.null()) return;
    readLux = newest["lux"].number(readLux);
    readTilt = newest["tilt"].number(readTilt);
    heard = true;
  }

  /** Takes what the connection answers, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Reading now{.generation = sensor.generation(),
                .undecodable = sensor.undecodable(),
                .closed = sensor.closed(),
                .lux = readLux,
                .tilt = readTilt,
                .heard = heard,
                .address = sensor.address(),
                .trouble = sensor.error()};
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where
    // the theme in force is no longer this page's, so the colours the
    // field is drawn in are read here and carried in by value.
    const material::Color rule = sketch::kit::theme().palette.rule;
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("serial_sensor.field", [this, rule](Pen& pen) {
                      draw(pen, rule);
                    }).cover());
    if (!heard) parts.push_back(waiting());
    compose::Element picture = compose::stack()
                                   .width(kCanvas.width())
                                   .height(kCanvas.height())
                                   .children(std::move(parts));
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A reading from the room.",
                  .subtitle = "SERIAL / SENSOR  /  Light changes the "
                              "brightness. Tilt changes the direction.",
                  .footer = "Run sensor.py beside this sketch to send data  ·  "
                            "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "Connect the board, or run sensor.py and set kPort to its "
                 "printed device path."},
        std::move(picture).fill(kGround), readout()));
  }

  /** The field, or the placeholder where no reading has come. */
  void draw(Pen& pen, material::Color rule) {
    pen.noStroke();
    if (!heard) {
      horizon(pen, rule);
      return;
    }
    field(pen);
  }

  /** THE QUIET PLACEHOLDER: one dim line where the field would lie, so
   *  a canvas nothing has reached still says where the sky is. */
  void horizon(Pen& pen, material::Color rule) {
    pen.stroke(rule);
    pen.strokeWeight(1);
    pen.line(kMargin, pen.height * 0.5f, pen.width - kMargin,
             pen.height * 0.5f);
    pen.noStroke();
  }

  /** THE FIELD: ribbons carried across the canvas, lit by the light the
   *  board reads and leaning by the tilt it reads. The field is drawn
   *  past every edge, so a lean turns the sky rather than showing the
   *  corner it turned off. */
  void field(Pen& pen) {
    const float lit = kDimmest + (1.0f - kDimmest) * (float)light;
    const float period = kSegment + kGap;
    pen.push();
    // Turned about the middle of the canvas: a tilt of nothing is a
    // level sky, and the sign of the reading is the side it falls to.
    pen.translate(pen.width * 0.5f, pen.height * 0.5f);
    pen.rotate((float)(lean * kFullLean * std::numbers::pi / 180.0));
    pen.translate(-pen.width * 0.5f, -pen.height * 0.5f);
    size_t index = 0;
    for (float y = kTop; y < pen.height + kOverhang; y += kSpacing) {
      const Ribbon& ribbon = kRibbons[index % kRibbons.size()];
      material::Color tint = kTints[index % kTints.size()];
      // THE LIGHT IS WHAT THE SKY IS WORTH, and a dark room is a sky
      // still there rather than a canvas gone black: the colour keeps
      // its hue and loses its value.
      tint = {tint.r * lit, tint.g * lit, tint.b * lit, tint.a * lit};
      pen.fill(tint);
      const double travelled = carried + (double)ribbon.speed * seconds;
      const float phase =
          (float)(std::fmod(std::fmod(travelled, period) + period, period));
      const float row =
          y + ribbon.wobble * std::sin((float)seconds * 0.6f + (float)index);
      for (float x = phase - period - kOverhang; x < pen.width + kOverhang;
           x += period)
        pen.rect(x, row, kSegment, ribbon.height, ribbon.height * 0.5f);
      ++index;
    }
    pen.pop();
  }

  /** WHAT THE DOOR IS DOING, for the canvas that has no reading yet. A
   *  board nobody plugged in is the ordinary case of it: the device
   *  file is not there, and the door says so in the system's own
   *  words. */
  std::string state() const {
    if (!shown.trouble.empty()) return shown.trouble;
    if (shown.undecodable != 0)
      return compose::kit::formatted("%llu lines were no reading",
                                     (unsigned long long)shown.undecodable);
    if (shown.closed) return "the port is closed: nothing else is coming";
    return compose::kit::formatted(
        "reading %s · the board has said nothing",
        shown.address.empty() ? kPort : shown.address.c_str());
  }

  /** The door's state where the sky would be, until there is one. */
  compose::Element waiting() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::text(state())
        .font(look.font({.size = 15, .mono = true}))
        .ink(look.palette.ash)
        .centerAt({kCanvas.width() * 0.5f, kCanvas.height() * 0.5f - 24.0f});
  }

  /** THE DOOR'S OWN WORDS, in the reading panel: the port a person
   *  edits, how many lines have come off it, how many were no reading,
   *  and the newest one as the board wrote it. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("port", kPort),
        row("generation", compose::kit::formatted(
                              "%llu", (unsigned long long)shown.generation)),
        row("undecodable", compose::kit::formatted(
                               "%llu", (unsigned long long)shown.undecodable))};
    lines.push_back(row("reading", shown.heard ? compose::kit::formatted(
                                                     "lux %.0f · tilt %+.1f",
                                                     shown.lux, shown.tilt)
                                               : "nothing has arrived"));
    if (!shown.trouble.empty())
      lines.push_back(row("error", shown.trouble));
    else
      lines.push_back(row("address", shown.address.empty()
                                         ? (shown.closed ? "closed" : "-")
                                         : shown.address));
    return compose::box()
        .column()
        .gap(12)
        .font(look.font({.size = 12, .mono = true}))
        .ink(look.palette.ink)
        .children(std::move(lines));
  }
};

}  // namespace

SIGIL_SKETCH(SerialSensor, "Data",
             "A board on a cable printing one line per reading: the light it "
             "reads is the brightness of a ribbon field, the tilt is its "
             "lean, the recording beside the sketch stands in for the board "
             "in a capture, and the port's own vitals are shown beside it.")
