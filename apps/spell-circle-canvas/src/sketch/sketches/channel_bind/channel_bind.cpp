/** @file
 * channel_bind — three faders on a wire, bound straight to a drawing.
 *
 * A desk sends three faders and three properties move by them. What
 * usually stands between the two is a handler per fader: read the
 * message, write a member, scale it into the units the property wants,
 * describe again. THERE IS NO HANDLER IN THIS FILE. A
 * `sketch::kit::Channel` follows one number of one message name and
 * writes it into an output; `motion::bind` over that output does the
 * scaling, the shaping and the mapping; and the property is bound once,
 * in setup, and never written again.
 *
 * SO THE WHOLE PATH IS ONE STATEMENT PER FADER. A socket, a message, an
 * argument, a chain and a drawn property, and the only thing the sketch
 * says about any of it is which number goes into which chain.
 *
 *   /fader/1   the blade RISES     source(0,127).target(short, tall)
 *   /fader/2   the wheel TURNS     source(0,127).map(smoothstep).target(0,360)
 *   /fader/3   the plumb SWINGS    source(0,127).pingPong().target(-16,16)
 *
 * The three chains are one value each, written once and read twice: the
 * tree binds them, and the readout evaluates them to say what each
 * property is standing at. A chain spelled a second time would be a
 * second chain, and the two would drift.
 *
 * NOTHING MOVES BETWEEN ARRIVALS, which is the point rather than a
 * limitation: a channel writes its output only when the message under
 * its name carried a different number, so a still desk costs nothing and
 * the picture is exactly what the wire last said.
 *
 * `faders.py` beside this file is the desk: it sends the three faders to
 * the port, and it writes the recording a capture replays.
 *
 *     python3 faders.py                                 # a window moves
 *     python3 faders.py --record data/faders.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kDesk       the URI the faders arrive on
 *   kRecording  what a capture replays instead of listening
 *   kFull       the reading a fader stands at, at the top of its travel
 *   kSway       how far the plumb swings either way
 */

// TAGS: Data/Sources, Motion/Clocks

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/connection/Connection.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;
namespace material = sigil::material;
namespace motion = sigil::motion;

namespace {

const char* kDesk = "osc://:27080";           // where the faders land
const char* kRecording = "data/faders.feed";  // what a capture replays

constexpr SkSize kCanvas = {1180, 740};
constexpr SkColor4f kBackdrop = {0.05f, 0.05f, 0.075f, 1};
/** Every fader is away from both ends of its travel by here, and the
 *  three stand at three different points of their own rhythms. */
constexpr double kCaptureAt = 2.5;

/** WHAT A FADER READS AT THE TOP OF ITS TRAVEL. It is the desk's number
 *  and not the drawing's, which is exactly what the source stage of a
 *  chain is for: it says what the wire speaks, once, and every stage
 *  after it works in the property's own units. */
constexpr float kFull = 127.0f;

constexpr size_t kFaders = 3;
constexpr std::array<const char*, kFaders> kAddresses = {
    {"/fader/1", "/fader/2", "/fader/3"}};
/** Where the reading stands in one of those messages: an OSC message is
 *  an address and a list, and a fader's whole vocabulary is the one float
 *  under it. */
constexpr size_t kReading = 0;

/** The plate the instrument stands on: the canvas inside the page
 *  margins, and the line the three parts stand on. */
constexpr float kFigureWidth = 354;
constexpr float kFigureHeight = 320;
constexpr float kFloor = 300;

/** THE BLADE: how tall it stands at the top of the fader's travel and at
 *  the bottom of it. A height is layout and a binding is read after
 *  layout, so the blade is built at its full extent and SCALED about its
 *  foot — the one drawn property a fader can move every frame without a
 *  re-describe. */
constexpr float kBladeAt = 150;
constexpr float kBladeWide = 54;
constexpr float kBladeTall = 270;
constexpr float kBladeShort = 36;

/** THE WHEEL: a ring of hues that turns under a fixed pointer, so which
 *  colour the pointer names is the fader's reading. */
constexpr float kWheelAt = 77;
constexpr float kWheelTop = 100;
constexpr float kWheelSide = 200;
constexpr float kWheelMiddleX = kWheelAt + kWheelSide * 0.5f;
constexpr float kWheelMiddleY = kWheelTop + kWheelSide * 0.5f;
constexpr float kHubSide = 68;
constexpr int kHues = 12;

/** THE PLUMB: a bob on a cord, hung from a pivot it turns about. */
constexpr float kPlumbAt = 111;
constexpr float kPlumbTop = 50;
constexpr float kPlumbWide = 132;
constexpr float kPlumbLong = 250;
constexpr float kBobSide = 52;
constexpr float kSway = 16.0f;  // degrees the plumb swings either way

/** WHAT THE READOUT WRITES BESIDE EACH FADER: the chain it runs through
 *  and the property it ends at. They are words about the code, so they
 *  stand here beside the code rather than being built out of it. */
constexpr std::array<const char*, kFaders> kChains = {
    {"source(0,127).target(0.13,1)",
     "source(0,127).map(smoothstep).target(0,360)",
     "source(0,127).pingPong().target(-16,16)"}};
constexpr std::array<const char*, kFaders> kDriven = {
    {"blade · scaleY", "wheel · rotate", "plumb · rotate"}};

/** THE HUES OF THE WHEEL, one ring of them at one saturation and one
 *  value, so a turn of the ring is a turn of hue and nothing else. */
std::vector<material::skia::Stop> hues() {
  std::vector<material::skia::Stop> stops;
  stops.reserve(kHues + 1);
  for (int step = 0; step <= kHues; ++step) {
    const float at = (float)step / (float)kHues;
    stops.push_back({at, material::skia::toSkColor(
                             material::hsv(at * 360.0f, 0.55f, 0.94f))});
  }
  return stops;
}

}  // namespace

namespace {

struct ChannelBind {
  /** THE DESK'S DOOR, read as OSC because the URI says osc. No handler is
   *  registered on it: everything this sketch takes off the wire, it
   *  takes through the three channels below. */
  data::Connection desk;
  /** One per fader, each following one argument of one address. */
  std::array<sketch::kit::Channel, kFaders> faders;

  /** WHAT THE READOUT SAYS, and the whole of what says to write it again.
   *  Each fader's reading is in it, so a description is rebuilt exactly
   *  when a fader moved — which is also exactly when the drawing changed,
   *  the three properties being pure functions of the three readings. */
  struct Shown {
    std::array<std::optional<double>, kFaders> read;
    uint64_t generation = 0;
    uint64_t undecodable = 0;
    std::string trouble;
    bool operator==(const Shown&) const = default;
  };
  Shown shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx,
        {.size = kCanvas, .captureAt = kCaptureAt, .background = kBackdrop});

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window listens on
    // the port. Mounting the URI onto the file is the whole of the
    // difference, and the mount stands first because a feed is made once
    // per URI and every later ask answers that same one.
    if (ctx.deterministic) hub.mount(kDesk, hub.resolve(ctx.local(kRecording)));

    desk = data::Connection(hub, kDesk);
    // THE CHANNELS STAND AFTER THE DOOR. Both register on the same
    // dispatch and callbacks run in the order they were registered, so
    // each channel reads what the dispatch that opened this frame
    // delivered rather than what the one before it left.
    for (size_t index = 0; index != kFaders; ++index)
      faders[index] =
          sketch::kit::Channel(hub, desk, kAddresses[index], kReading);

    shown = {};
    read();
    describe(ctx);
  }

  void update(double, sketch::SketchContext& ctx) {
    // The drawing itself needs no frame: the three properties are bound,
    // and a bound property is read where it is painted. What is described
    // again is the readout, and only when a fader moved.
    if (read()) describe(ctx);
  }

  /** THE THREE CHAINS, one value each. The tree binds them and the
   *  readout evaluates them, so what a row says the property is standing
   *  at is the number the property is standing at. */
  motion::Bound rise() const {
    return motion::bind(&faders[0].output())
        .source(0, kFull)
        .target(kBladeShort / kBladeTall, 1.0f);
  }
  motion::Bound turn() const {
    return motion::bind(&faders[1].output())
        .source(0, kFull)
        .map(&motion::ease::smoothstep)
        .target(0, 360);
  }
  motion::Bound swing() const {
    return motion::bind(&faders[2].output())
        .source(0, kFull)
        .pingPong()
        .target(-kSway, kSway);
  }

  /** Takes what the three channels answer, and says whether the readout
   *  standing now was written from something else. */
  bool read() {
    Shown now;
    for (size_t index = 0; index != kFaders; ++index)
      now.read[index] = faders[index].lastRead();
    now.generation = desk.generation();
    now.undecodable = desk.undecodable();
    now.trouble = desk.error();
    if (now == shown) return false;
    shown = std::move(now);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    ctx.composer.render(sketch::kit::page(
        {.title = "A number arrives. A property moves.",
         .subtitle = "Three OSC faders, three bound properties · input 0–127 "
                     "becomes height, rotation and sway",
         .footer = "Bindings are installed once. Between messages the picture "
                   "holds the last value; a changed channel updates its output "
                   "and the readout."},
        compose::box().column().gap(24).children(
            {figure(), readings(), door()})));
  }

  compose::Element figure() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    const auto plate = [&](std::vector<compose::Element> parts) {
      parts.push_back(
          compose::box()
              .rect(SkRect::MakeLTRB(26, kFloor, kFigureWidth - 26, kFloor + 1))
              .fill(compose::Fill::color(look.palette.rule)));
      return sketch::kit::well(
          {.width = kFigureWidth, .height = kFigureHeight},
          compose::positioned().children(std::move(parts)));
    };
    return sketch::kit::comparison(
        {.cases = {{.title = "01 / RISE",
                    .control = kChains[0],
                    .figure = plate({blade(look)}),
                    .note = "A linear mapping scales the blade about its foot: "
                            "36–270 px tall."},
                   {.title = "02 / TURN",
                    .control = kChains[1],
                    .figure = plate(wheel(look)),
                    .note = "Smoothstep shapes the turn. The fixed pointer "
                            "reads the rotating hue wheel."},
                   {.title = "03 / SWING",
                    .control = kChains[2],
                    .figure = plate(plumb(look)),
                    .note = "Ping-pong folds the input into a return trip "
                            "between −16° and +16°."}},
         .measure = 1100,
         .gap = 19});
  }

  /** A HEIGHT, as the scale of a blade about its own foot. */
  compose::Element blade(const sketch::kit::Theme& look) {
    return compose::box()
        .rect(SkRect::MakeLTRB(kBladeAt, kFloor - kBladeTall,
                               kBladeAt + kBladeWide, kFloor))
        .fill(compose::Fill::color(look.palette.figure))
        .corners(4)
        .transformOrigin(0.5f, 1.0f)
        .scaleY(rise());
  }

  /** A HUE, as the turn of a ring under a pointer that does not move. The
   *  three pieces stand side by side rather than nested, because a
   *  placed plate positions the children it is handed and a group would
   *  put its own rect between them and it. */
  std::vector<compose::Element> wheel(const sketch::kit::Theme& look) {
    std::vector<compose::Element> pieces;
    pieces.push_back(
        compose::box()
            .rect(SkRect::MakeLTRB(kWheelAt, kWheelTop, kWheelAt + kWheelSide,
                                   kWheelTop + kWheelSide))
            .corners(kWheelSide * 0.5f)
            .fill(material::skia::Paint::sweep(
                SkPoint{kWheelSide * 0.5f, kWheelSide * 0.5f}, hues()))
            .rotate(turn()));
    // The hub, which turns the disc into a ring, and the pointer the ring
    // is read against. Neither is bound: a pointer that moved with the
    // wheel would name one colour forever.
    pieces.push_back(
        compose::box()
            .rect(SkRect::MakeLTRB(kWheelMiddleX - kHubSide * 0.5f,
                                   kWheelMiddleY - kHubSide * 0.5f,
                                   kWheelMiddleX + kHubSide * 0.5f,
                                   kWheelMiddleY + kHubSide * 0.5f))
            .corners(kHubSide * 0.5f)
            .fill(compose::Fill::color(look.palette.cellGround)));
    pieces.push_back(
        compose::box()
            .rect(SkRect::MakeLTRB(kWheelMiddleX - 5, kWheelTop - 16,
                                   kWheelMiddleX + 5, kWheelTop + 14))
            .corners(2)
            .fill(compose::Fill::color(look.palette.ink)));
    return pieces;
  }

  /** A SWAY, as the turn of a hung cord about its pivot. The bob is a
   *  child of the cord — a child's rect stands inside its parent's — so
   *  one rotation carries both; the pivot stands outside it, because a
   *  pivot that turned would not be one. */
  std::vector<compose::Element> plumb(const sketch::kit::Theme& look) {
    std::vector<compose::Element> pieces;
    pieces.push_back(
        compose::box()
            .rect(SkRect::MakeLTRB(kPlumbAt, kPlumbTop, kPlumbAt + kPlumbWide,
                                   kPlumbTop + kPlumbLong))
            .transformOrigin(0.5f, 0.0f)
            .rotate(swing())
            .children({compose::box()
                           .rect(SkRect::MakeLTRB(kPlumbWide * 0.5f - 3, 0,
                                                  kPlumbWide * 0.5f + 3,
                                                  kPlumbLong - kBobSide))
                           .fill(compose::Fill::color(look.palette.rule)),
                       compose::box()
                           .rect(SkRect::MakeLTRB(
                               kPlumbWide * 0.5f - kBobSide * 0.5f,
                               kPlumbLong - kBobSide,
                               kPlumbWide * 0.5f + kBobSide * 0.5f, kPlumbLong))
                           .corners(kBobSide * 0.5f)
                           .fill(compose::Fill::color(look.palette.figure))}));
    pieces.push_back(compose::box()
                         .rect(SkRect::MakeLTRB(
                             kPlumbAt + kPlumbWide * 0.5f - 7, kPlumbTop - 7,
                             kPlumbAt + kPlumbWide * 0.5f + 7, kPlumbTop + 7))
                         .corners(7)
                         .fill(compose::Fill::color(look.palette.ink)));
    return pieces;
  }

  /** WHAT EACH CHANNEL LAST READ off the wire, and what the chain over it
   *  puts the property at. */
  compose::Element readings() {
    std::vector<sketch::kit::ComparisonCase> cases;
    for (size_t index = 0; index != kFaders; ++index)
      cases.push_back({.title = kAddresses[index],
                       .figure = sketch::kit::readout(
                           {{.name = "LAST INPUT", .value = reading(index)},
                            {.name = kDriven[index], .value = standing(index)}},
                           {.measure = kFigureWidth, .ruled = true})});
    return sketch::kit::comparison(
        {.cases = std::move(cases), .measure = 1100, .gap = 19});
  }

  /** The reading as the wire spelled it, or the dash that says nothing
   *  has arrived under that name yet. */
  compose::Utf8 reading(size_t index) const {
    const std::optional<double> last = faders[index].lastRead();
    return last ? compose::Utf8(compose::kit::formatted("%.2f", *last))
                : compose::Utf8("-");
  }

  /** The number the chain puts the property at, evaluated through the
   *  same chain the tree bound. */
  compose::Utf8 standing(size_t index) const {
    if (index == 0)
      return compose::kit::formatted("x %.3f",
                                     rise().value().apply(faders[0].value()));
    if (index == 1)
      return compose::kit::formatted("%.1f deg",
                                     turn().value().apply(faders[1].value()));
    return compose::kit::formatted("%.1f deg",
                                   swing().value().apply(faders[2].value()));
  }

  /** WHAT THE DOOR ANSWERS, under the table: where it is, how much has
   *  arrived, how much of it was no OSC packet at all, and the sentence
   *  saying why there is no door where there is none. */
  compose::Element door() {
    return compose::text(
               compose::kit::formatted(
                   "%s   ·   arrivals %llu   ·   undecodable %llu"
                   "   ·   %s",
                   kDesk, (unsigned long long)shown.generation,
                   (unsigned long long)shown.undecodable,
                   shown.trouble.empty() ? "listening" : shown.trouble.c_str()))
        .styleClass("captionNote");
  }
};

}  // namespace

SIGIL_SKETCH(ChannelBind, "Kit · API",
             "Three faders on an OSC desk, each a kit Channel bound "
             "straight to a property: a blade's height, a hue wheel's turn "
             "and a plumb's sway, with no handler anywhere between the "
             "wire and the drawing.")
