/** @file
 * quic_sky — the same sky, over one encrypted connection.
 *
 * The scene is feed_sky's: bands crossing the canvas, the wind they
 * drift on, the palette they are tinted from. What differs is the DOOR.
 * There a message is a datagram and anything on the network can read it;
 * here the sketch holds a QUIC port, every sender opens an encrypted
 * connection to it, and each message is a unidirectional stream of its
 * own — so a message keeps its boundary, none of them waits behind
 * another, and a scene can tell WHICH connection a sky came in on rather
 * than only which address sent a packet.
 *
 * A QUIC PORT ANSWERS FOR ITSELF. There is no unencrypted form of this
 * door, so the URI names the certificate and the private key the port
 * shows: the two files under `data/` beside this sketch, resolved
 * through the hub's mounts as the feed opens. They are DEMONSTRATION
 * CREDENTIALS — self-signed, vouching for nobody, and checked into this
 * tree where anybody may read the key — so they belong to a scene on a
 * stage and to nothing else. A sender reaches them with `insecure=1`,
 * which is the caller's way of saying it will not ask who signed for the
 * machine it is talking to.
 *
 * ONE DOOR, TWO SOURCES, exactly as beside feed_sky. A URI that resolves
 * through the mount table to a file is played back from that recording
 * as the hub's time moves forward, and any other opens through the
 * transport its scheme names. So a capture mounts the recording beside
 * this file onto the whole URI — query and all, the mount table matching
 * by prefix — and reads what a window hears from a sender, arrival for
 * arrival, at the same seconds. The mount stands before the door opens,
 * because a feed is made once per URI and every later ask answers that
 * same one.
 *
 * `sender.py` beside this file writes the recording only. It cannot
 * stand in for a live sender the way the datagram sibling's does:
 * speaking this door needs a QUIC stack and Python ships none. The live
 * other side is Seer, which registers every transport this tree carries
 * and opens `quic://127.0.0.1:27100?insecure=1` as a wire like any
 * other.
 *
 *     python3 sender.py --record data/sky.feed --seconds 6 --rate 4
 *
 * A MESSAGE IS PLAIN JSON — the sky as it stands, never a change to one,
 * so a reader that fell behind draws what is true now:
 *
 *     {"bands": [{"height": h, "speed": s, "wobble": w}, ...],
 *      "wind": {"x": x, "y": y},
 *      "palette": [{"r": r, "g": g, "b": b, "a": a}, ...]}
 *
 * EDIT THESE FIRST
 *   kPort        the port a sender reaches, and the sky arrives on
 *   kCertificate the pair that port answers with, and the key beside it
 *   kRecording   what a capture replays instead of listening
 *   kSpacing     how far apart the bands stand
 *   kSegment     the length of a ribbon's segments, and their gap
 */

// TAGS: Data/Sources, Runtime/Resources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;

using sigil::draw::Pen;

namespace {

constexpr int kPort = 27100;                 // where a sender reaches this
const char* kCertificate = "data/cert.pem";  // what the port shows
const char* kKey = "data/key.pem";           // and what it signs with
const char* kRecording = "data/sky.feed";    // what a capture replays

constexpr SkSize kCanvas = {1280, 720};
constexpr SkColor4f kGround = {0.03f, 0.05f, 0.10f, 1};
constexpr double kCaptureAt = 2.0;  // by now several messages have landed

constexpr float kTop = 80.0f;       // where the first band stands
constexpr float kSpacing = 104.0f;  // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;
constexpr float kMargin = 28.0f;

/** THE DOOR: the port, and the pair it answers with. The two files stand
 *  beside this sketch, so the URI names them the way anything beside a
 *  sketch is named and the hub resolves them as the feed opens. */
std::string doorOf(const sketch::SketchContext& ctx) {
  return "quic://:" + std::to_string(kPort) +
         "?cert=" + ctx.local(kCertificate) + "&key=" + ctx.local(kKey);
}

/** WHAT THE DOOR SAYS ABOUT ITSELF — the whole of it. The readout prints
 *  these, and a reading that differs from the one the description was
 *  written from is what says to write it again. */
struct Vitals {
  uint64_t generation = 0;
  uint64_t dropped = 0;
  uint64_t undecodable = 0;
  bool closed = false;
  std::string address;
  std::string connection;
  std::string trouble;
  bool operator==(const Vitals&) const = default;
};

/** The connection the newest message came in on, as the transport names
 *  one: the sender's address with the number of the connection behind
 *  it. Empty before anything has arrived, and on a recording, which
 *  holds the messages and not who sent them. */
std::string connectionOf(const data::Connection& door) {
  const std::shared_ptr<io::Feed> feed = door.feed();
  if (!feed) return {};
  const std::optional<io::Arrival> newest = feed->newest();
  return newest ? newest->from : std::string();
}

Vitals vitalsOf(const data::Connection& door) {
  return {.generation = door.generation(),
          .dropped = door.dropped(),
          .undecodable = door.undecodable(),
          .closed = door.closed(),
          .address = door.address(),
          .connection = connectionOf(door),
          .trouble = door.error()};
}

/** One channel of a colour, or @p fallback where the message left it
 *  out. */
float channel(const data::Json& colour, std::string_view name, float fallback) {
  return (float)colour[name].number(fallback);
}

/** One row of the readout: what it is called, and what it answers. */
compose::Element row(const std::string& name, const std::string& answer) {
  return compose::text(
      compose::kit::formatted("%-11s %s", name.c_str(), answer.c_str()));
}

}  // namespace

namespace {

struct QuicSky {
  /** The door, read as JSON: what arrives is a sky or it is nothing. The
   *  same feed for the same URI while anybody holds it, so a reload that
   *  sets this sketch up again is handed the one it was already
   *  reading. */
  data::Connection sky;
  /** What the description standing now was written from. */
  Vitals shown;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});

    const std::string door = doorOf(ctx);
    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE and holds no port
    // at all; a window holds the port and shows the pair to whoever
    // reaches it. The mount table matches a URI by its PREFIX, so the
    // key is the whole URI, query and all — the same string the feed is
    // asked for.
    if (ctx.deterministic) hub.mount(door, hub.resolve(ctx.local(kRecording)));
    sky = data::Connection(hub, door);

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
   *  not the backlog, a sky being a state — and is read where it is
   *  drawn. */
  bool read() {
    const Vitals now = vitalsOf(sky);
    if (now == shown) return false;
    shown = now;
    return true;
  }

  /** Whether a sky has arrived and been read as one. */
  bool arrived() const { return !sky.latest().null(); }

  void describe(sketch::SketchContext& ctx) {
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the one colour the
    // placeholder is drawn in is read here and carried in by value.
    const SkColor4f rule = sketch::kit::theme().palette.rule;
    std::vector<compose::Element> parts;
    parts.push_back(compose::pen("quic_sky.sky", [this, rule](Pen& pen) {
                      draw(pen, rule);
                    }).cover());
    if (!arrived()) parts.push_back(waiting());
    parts.push_back(readout());
    ctx.composer.render(compose::stack()
                            .width(kCanvas.width())
                            .height(kCanvas.height())
                            .children(std::move(parts)));
  }

  /** The sky, or the placeholder where there is none. */
  void draw(Pen& pen, SkColor4f rule) {
    pen.noStroke();
    if (!arrived()) {
      horizon(pen, rule);
      return;
    }
    bands(pen);
  }

  /** THE QUIET PLACEHOLDER: one dim line where the bands would cross, so
   *  a canvas nothing has reached still says where the sky is. */
  void horizon(Pen& pen, SkColor4f rule) {
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
      return compose::kit::formatted("%llu arrivals were no sky",
                                     (unsigned long long)shown.undecodable);
    if (shown.closed) return "the door is closed: nothing else is coming";
    return compose::kit::formatted(
        "holding %s · nothing has connected",
        shown.address.empty() ? "no port" : shown.address.c_str());
  }

  /** The door's state where the sky would be, until there is one. */
  compose::Element waiting() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::text(state())
        .font(look.font({.size = 15, .mono = true}))
        .ink(look.palette.ash)
        .centerAt({kCanvas.width() * 0.5f, kCanvas.height() * 0.5f - 24.0f});
  }

  /** THE DOOR'S OWN WORDS, small in the corner: how many messages have
   *  arrived, how many fell off the queue behind a reader, how many were
   *  no sky at all, where the port is, and WHICH CONNECTION the newest
   *  message crossed — the part a datagram door has no answer for.
   *  Nothing here is computed about the sky; every line is what the door
   *  answers. */
  compose::Element readout() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<compose::Element> lines{
        row("port", std::to_string(kPort)),
        row("generation", std::to_string(shown.generation)),
        row("dropped", std::to_string(shown.dropped)),
        row("no sky", std::to_string(shown.undecodable))};
    if (!shown.trouble.empty()) {
      lines.push_back(row("error", shown.trouble));
    } else {
      lines.push_back(row("address", shown.address.empty()
                                         ? (shown.closed ? "closed" : "-")
                                         : shown.address));
      lines.push_back(
          row("connection", shown.connection.empty() ? "-" : shown.connection));
    }
    return compose::box()
        .column()
        .gap(look.spacing.rowGap)
        .left(kMargin)
        .bottom(kMargin)
        .font(look.font({.size = 11, .mono = true}))
        .ink(look.palette.ash)
        .children(std::move(lines));
  }
};

}  // namespace

SIGIL_SKETCH(QuicSky, "Data",
             "The sky feed_sky draws, arriving over one encrypted QUIC "
             "connection instead of a datagram: a port that answers with a "
             "certificate, one stream per message, and the connection each "
             "sky crossed printed under it.")
