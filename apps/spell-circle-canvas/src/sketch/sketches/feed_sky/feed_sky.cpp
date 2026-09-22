/** @file
 * feed_sky — a sky that keeps arriving.
 *
 * The scene is the one schema_scene draws beside this file: bands
 * crossing the canvas, the wind they drift on, the palette they are
 * tinted from. What differs is where the sky COMES FROM. There it is a
 * file, read once and re-read when it changes; here it is the newest
 * message on a CONNECTION — a door that keeps delivering — so the sky
 * is whatever the sender last said, and a canvas nothing has reached
 * shows a quiet horizon and the door's own words about itself.
 *
 * ONE DOOR, TWO SOURCES. The URI is opened on the hub: one that
 * resolves through the mount table to a file is played back from that
 * recording as the hub's time moves forward, and any other opens
 * through the transport its scheme names. So a capture mounts the
 * recording beside this file onto the port and reads exactly what a
 * window hears from a sender — arrival for arrival, at the same seconds
 * — which is what makes a still of a live scene a plate at all. The
 * mount stands before the door opens, because a feed is made once per
 * URI and every later ask answers that same one.
 *
 * `sender.py` beside this file is both ends of it: it sends the sky to
 * the port, and it writes the recording a capture replays.
 *
 *     python3 sender.py                                   # a window moves
 *     python3 sender.py --record data/sky.feed --seconds 6 --rate 4
 *
 * EVERY MESSAGE COMES THROUGH THE SKETCH'S OWN SCHEMA. `feed_sky.fbs`
 * beside this file states what a sky is, the build compiles it into
 * `feed_sky_generated.h`, and `data::schema<feed_sky::Sky>()` is that
 * schema as one value the door is opened with. So a sender may write
 * the schema's JSON form or the buffer itself and the scene reads the
 * same value either way, and an arrival that does not FIT the schema is
 * counted as undecodable rather than drawn as a sky missing its fields.
 *
 * EDIT THESE FIRST
 *   feed_sky.fbs  what a sky is: its bands, its wind and its palette
 *   kAddress      the URI the sky arrives on
 *   kRecording    what a capture replays instead of listening
 *   kSpacing      how far apart the bands stand
 *   kSegment      the length of a ribbon's segments, and their gap
 */

// TAGS: Data/Schema, Data/Sources, Runtime/Resources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Instrument.h>
#include <sigilsketch/kit/Theme.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "feed_sky_generated.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;

using sigil::draw::Pen;

namespace {

const char* kAddress = "udp://:27020";     // where the sky arrives
const char* kRecording = "data/sky.feed";  // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr material::Color kGround = {0.04f, 0.04f, 0.09f, 1};
constexpr double kCaptureAt = 2.0;  // by now several messages have landed

constexpr float kTop = 80.0f;       // where the first band stands
constexpr float kSpacing = 104.0f;  // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

/** WHAT A DOOR SAYS ABOUT ITSELF — the whole of it. The readout prints
 *  these, and a reading that differs from the one the description was
 *  written from is what says to write it again. */
struct Vitals {
  uint64_t generation = 0;
  uint64_t dropped = 0;
  uint64_t undecodable = 0;
  bool closed = false;
  std::string address;
  std::string trouble;
  bool operator==(const Vitals&) const = default;
};

Vitals vitalsOf(const data::Connection& door) {
  return {.generation = door.generation(),
          .dropped = door.dropped(),
          .undecodable = door.undecodable(),
          .closed = door.closed(),
          .address = door.address(),
          .trouble = door.error()};
}

/** One channel of a colour, or @p fallback where the message left it
 *  out. A colour is an object in the schema's form, a struct's fields
 *  being named there as a table's are. */
float channel(const data::Json& colour, std::string_view name, float fallback) {
  return (float)colour[name].number(fallback);
}

}  // namespace

namespace {

struct FeedSky {
  /** The door, read through the sketch's own schema: what arrives is a
   *  Sky or it is nothing. The same feed for the same URI while anybody
   *  holds it, so a reload that sets this sketch up again is handed the
   *  one it was already reading. */
  data::Connection sky;
  /** What the description standing now was written from. */
  Vitals shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window listens.
    // Mounting the URI onto the file is the whole of the difference: the
    // line below opens either one.
    if (ctx.deterministic)
      hub.mount(kAddress, hub.resolve(ctx.local(kRecording)));
    sky = data::Connection(hub, kAddress, data::schema<feed_sky::Sky>());

    shown = {};
    read();
    describe(ctx);
  }

  /** The data path: a message that arrived since the last frame is a new
   *  sky and a new readout, and nothing else in the scene moves by being
   *  described again. */
  void update(double, sketch::SketchContext& ctx) {
    if (read()) describe(ctx);
  }

  /** Answers whether what the description says about the door has
   *  changed. The sky itself is the door's newest message — the NEWEST,
   *  not the backlog, a sky being a state, so a reader that fell behind
   *  draws what is true now rather than every step that led to it — and
   *  is read where it is drawn. */
  bool read() {
    const Vitals now = vitalsOf(sky);
    if (now == shown) return false;
    shown = now;
    return true;
  }

  /** Whether a sky has arrived and been read as one. */
  bool arrived() const { return !sky.latest().null(); }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the one colour the
    // placeholder is drawn in is read here and carried in by value.
    const material::Color rule = sketch::kit::theme().palette.rule;
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("feed_sky.sky", [this, rule](Pen& pen) {
                      draw(pen, rule);
                    }).cover());
    if (!arrived()) parts.push_back(waiting());
    compose::Element picture = compose::stack()
                                   .width(kCanvas.width())
                                   .height(kCanvas.height())
                                   .children(std::move(parts));
    ctx.composer.render(sketch::kit::instrument(
        {.page =
             {.title = "A sky, received.",
              .subtitle =
                  "UDP / SCHEMA  /  The newest message becomes the picture.",
              .footer = "Run sender.py beside this sketch to send data  ·  "
                        "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "Each band comes from the schema: height, drift, colour and "
                 "wind."},
        std::move(picture).fill(kGround), readout()));
  }

  /** The sky, or the placeholder where there is none. */
  void draw(Pen& pen, material::Color rule) {
    pen.noStroke();
    if (!arrived()) {
      horizon(pen, rule);
      return;
    }
    bands(pen);
  }

  /** THE QUIET PLACEHOLDER: one dim line where the bands would cross, so
   *  a canvas nothing has reached still says where the sky is. */
  void horizon(Pen& pen, material::Color rule) {
    pen.stroke(rule);
    pen.strokeWeight(1);
    pen.line(kMargin, pen.height * 0.5f, pen.width - kMargin,
             pen.height * 0.5f);
    pen.noStroke();
  }

  /** The bands: each a ribbon of segments the width of the canvas,
   *  drifting on the wind plus its own speed and breathing by its
   *  wobble, tinted from the palette in turn. */
  void bands(Pen& pen) {
    const data::Json& state = sky.latest();
    const std::span<const data::Json> rows = state["bands"].items();
    const std::span<const data::Json> palette = state["palette"].items();
    const float wind = (float)state["wind"]["x"].number();
    const float period = kSegment + kGap;
    const float seconds = (float)(pen.millis() * 0.001);
    size_t index = 0;
    for (const data::Json& band : rows) {
      if (!palette.empty()) {
        const data::Json& tint = palette[index % palette.size()];
        pen.fill(channel(tint, "r", 1) * 255, channel(tint, "g", 1) * 255,
                 channel(tint, "b", 1) * 255, channel(tint, "a", 1) * 255);
      } else {
        pen.fill(255, 255, 255, 120);
      }
      const float height = (float)band["height"].number();
      const float drift = (wind + (float)band["speed"].number()) * seconds;
      const float phase = std::fmod(std::fmod(drift, period) + period, period);
      const float y = kTop + kSpacing * (float)index +
                      (float)band["wobble"].number() *
                          std::sin(seconds * 0.7f + (float)index);
      for (float x = phase - period; x < pen.width; x += period)
        pen.rect(x, y, kSegment, height, height * 0.5f);
      ++index;
    }
  }

  /** WHAT THE DOOR IS DOING, in one line, for the canvas that has no sky
   *  to draw yet. */
  std::string state() const {
    if (!shown.trouble.empty()) return shown.trouble;
    if (shown.undecodable != 0)
      return compose::kit::formatted("%llu arrivals did not fit the schema",
                                     (unsigned long long)shown.undecodable);
    if (shown.closed) return "the feed is closed: nothing else is coming";
    return compose::kit::formatted(
        "listening on %s · nothing has arrived",
        shown.address.empty() ? kAddress : shown.address.c_str());
  }

  /** The door's state where the sky would be, until there is one. */
  compose::Element waiting() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::text(state())
        .font(look.font({.size = 15, .mono = true}))
        .ink(look.palette.ash)
        .centerAt({kCanvas.width() * 0.5f, kCanvas.height() * 0.5f - 24.0f});
  }

  /** THE DOOR'S OWN WORDS, in the reading panel: how many messages have
   *  arrived, how many fell off the queue behind a reader, how many were
   *  no sky under this sketch's schema, and where the door is — or the
   *  sentence saying why there is none. Nothing here is computed about
   *  the sky; every line is what the door answers. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<std::string> lines{
        compose::kit::formatted("feed        %s", kAddress),
        compose::kit::formatted("generation  %llu",
                                (unsigned long long)shown.generation),
        compose::kit::formatted("dropped     %llu",
                                (unsigned long long)shown.dropped),
        compose::kit::formatted("undecodable %llu",
                                (unsigned long long)shown.undecodable)};
    if (!shown.trouble.empty())
      lines.push_back(
          compose::kit::formatted("error       %s", shown.trouble.c_str()));
    else
      lines.push_back(compose::kit::formatted(
          "address     %s", shown.address.empty()
                                ? (shown.closed ? "closed" : "-")
                                : shown.address.c_str()));
    return compose::box()
        .column()
        .gap(12)
        .font(look.font({.size = 12, .mono = true}))
        .ink(look.palette.ink)
        .children(compose::each(lines, [](const std::string& line) {
          return compose::text(line);
        }));
  }
};

}  // namespace

SIGIL_SKETCH(FeedSky, "Data",
             "A sky drawn from the newest message on a connection read "
             "through the sketch's own schema: a port in a window, the "
             "recording beside the sketch in a capture, and the door's own "
             "vitals shown beside it.")
