/** @file
 * shared_sky — a sky handed over, not sent.
 *
 * The scene is feed_sky's: bands crossing the canvas, the wind they
 * drift on, the palette they are tinted from. What differs is the WAY
 * IN. There the sky arrives as a datagram on a port; here it is a
 * region of memory another process on this machine wrote, mapped
 * read-only and looked at on a timer. No socket is opened, nothing is
 * copied through the kernel, and the two programs share a name and
 * nothing else — which is the path the product itself takes when the
 * thing drawing and the thing deciding are two processes on one
 * machine.
 *
 * WHAT A REGION PROMISES, and why a scene may read one on the frame:
 * the region carries a count of the messages written into it, odd while
 * one is being written and even while one stands whole, so a reader
 * that copies between two equal even counts has a whole message and one
 * that does not looks again. It never blocks the writer and the writer
 * never waits for it. A region holds the message standing NOW and no
 * backlog, which is what a sky is: a state, not a history.
 *
 * ONE DOOR, TWO SOURCES. The URI is opened on the hub: one that
 * resolves through the mount table to a file is played back from that
 * recording as the hub's time moves forward, and any other opens
 * through the transport its scheme names. So a capture mounts the
 * recording beside this file onto the region's URI and reads exactly
 * what a window reads from a live writer — arrival for arrival, at the
 * same seconds. The mount stands before the door opens, because a feed
 * is made once per URI and every later ask answers that same one.
 *
 * `sender.py` beside this file is both ends of it: it writes the sky
 * into the region, and it writes the recording a capture replays.
 * EITHER END MAY START FIRST — the door holds the region's NAME rather
 * than the memory behind it, so a sender started after the window is
 * read as soon as it makes the region, and a sender stopped and started
 * again is read as the region it makes afresh.
 *
 *     python3 sender.py                                   # a window moves
 *     python3 sender.py --record data/sky.feed --seconds 6 --rate 4
 *
 * THE MESSAGES ARE JSON and nothing declares their shape, which is the
 * other half of what this sketch is beside feed_sky: there a schema
 * says what a sky is and an arrival that does not fit it is counted
 * rather than drawn, here every field is read where it is used and a
 * missing one falls back to what the reading asks for.
 *
 * EDIT THESE FIRST
 *   kRegion     the region the sky is handed over in
 *   kRecording  what a capture replays instead of mapping
 *   kSpacing    how far apart the bands stand
 *   kSegment    the length of a ribbon's segments, and their gap
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

using sigil::draw::Pen;

namespace {

const char* kRegion = "shm://shared_sky";  // where the sky is handed over
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
  uint64_t undecodable = 0;
  bool closed = false;
  std::string address;
  std::string trouble;
  bool operator==(const Vitals&) const = default;
};

Vitals vitalsOf(const data::Connection& door) {
  return {.generation = door.generation(),
          .undecodable = door.undecodable(),
          .closed = door.closed(),
          .address = door.address(),
          .trouble = door.error()};
}

/** One channel of a colour, or @p fallback where the message left it
 *  out. */
float channel(const data::Json& colour, std::string_view name, float fallback) {
  return (float)colour[name].number(fallback);
}

}  // namespace

namespace {

struct SharedSky {
  /** The door: a region another process wrote, read as JSON. The same
   *  feed for the same URI while anybody holds it, so a reload that
   *  sets this sketch up again is handed the one it was already
   *  reading. */
  data::Connection sky;
  /** What the description standing now was written from. */
  Vitals shown;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window maps the
    // region. Mounting the URI onto the file is the whole of the
    // difference: the line below opens either one.
    if (ctx.deterministic)
      hub.mount(kRegion, hub.resolve(ctx.local(kRecording)));
    sky = data::Connection(hub, kRegion);

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
   *  which is the only one a region has — and is read where it is
   *  drawn. */
  bool read() {
    const Vitals now = vitalsOf(sky);
    if (now == shown) return false;
    shown = now;
    return true;
  }

  /** Whether a sky has been handed over and read as one. */
  bool arrived() const { return !sky.latest().null(); }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the one colour the
    // placeholder is drawn in is read here and carried in by value.
    const material::Color rule = sketch::kit::theme().palette.rule;
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("shared_sky.sky", [this, rule](Pen& pen) {
                      draw(pen, rule);
                    }).cover());
    if (!arrived()) parts.push_back(waiting());
    compose::Element picture = compose::stack()
                                   .width(kCanvas.width())
                                   .height(kCanvas.height())
                                   .children(std::move(parts));
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A scene between processes.",
                  .subtitle = "SHARED MEMORY / SCHEMA  /  One process writes "
                              "the sky. Another reads its newest state.",
                  .footer = "Run sender.py beside this sketch to send data  ·  "
                            "Captures replay the local recording"},
         .pictureSize = kCanvas,
         .pictureWidth = 816,
         .note = "The horizon becomes a field of bands when the writer "
                 "publishes a scene."},
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
   *  to draw yet. A region nobody has made is the ordinary case of it:
   *  nothing is wrong, the door names the region it is waiting at, and
   *  the line says nothing has been written there. */
  std::string state() const {
    if (!shown.trouble.empty()) return shown.trouble;
    if (shown.undecodable != 0)
      return compose::kit::formatted("%llu messages were no sky",
                                     (unsigned long long)shown.undecodable);
    if (shown.closed) return "the region is closed: nothing else is coming";
    return compose::kit::formatted(
        "reading %s · nothing has been written",
        shown.address.empty() ? kRegion : shown.address.c_str());
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
   *  been handed over, how many were no sky, and what the door is
   *  reading — or the sentence saying why it is reading nothing.
   *  Nothing here is computed about the sky; every line is what the
   *  door answers. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<std::string> lines{
        compose::kit::formatted("door        %s", kRegion),
        compose::kit::formatted("generation  %llu",
                                (unsigned long long)shown.generation),
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

SIGIL_SKETCH(SharedSky, "Data",
             "A sky drawn from a region of shared memory another process on "
             "this machine wrote: no socket, no copy through the kernel, the "
             "recording beside the sketch in a capture, and the door's own "
             "vitals shown beside it.")
