/** @file
 * feed_sky — one sky, arriving through three doors at once.
 *
 * The scene is the one schema_scene draws from a file: bands crossing
 * the canvas, the wind they drift on, the palette they are tinted from.
 * Here each sky is the newest message on a CONNECTION — a door that keeps
 * delivering — and the sheet holds three of them side by side, so what a
 * reader compares is the door and nothing else: the same
 * `data::Connection`, the same `latest()`, the same vitals, over three
 * transports.
 *
 *   UDP · SCHEMA          a datagram per sky, read through `feed_sky.fbs`
 *                         beside this file: the build compiles it into
 *                         `feed_sky_generated.h`, and an arrival that does
 *                         not FIT the schema is counted as undecodable
 *                         rather than drawn as a sky missing its fields.
 *   QUIC · JSON           one encrypted stream per sky. There is no
 *                         unencrypted form of this door, so its URI names
 *                         the certificate and key it answers with — the
 *                         two files under `data/`, DEMONSTRATION
 *                         CREDENTIALS, self-signed and vouching for nobody.
 *   SHARED MEMORY · JSON  a region another process on this machine wrote,
 *                         mapped read-only and looked at on a timer: no
 *                         socket, no copy through the kernel.
 *
 * The two JSON doors read every field where it is used, and a field a
 * message left out falls back to what the reading asks for.
 *
 * ONE DOOR, TWO SOURCES. A URI the hub was told to replay is played back
 * from that recording as the hub's time moves forward, and any other
 * opens through the transport its scheme names. So a capture replays each
 * door's recording under `data/` at its whole URI — query and all, the
 * same string the door is opened on — and reads exactly what a window
 * hears, arrival for arrival, at the same seconds.
 *
 * `sender.py` beside this file sends the UDP and shared memory doors and
 * writes all three recordings; the live side of the QUIC door is Seer,
 * opening `quic://127.0.0.1:27100?insecure=1`.
 *
 *     python3 sender.py                  # the UDP panel moves
 *     python3 sender.py --door shared    # the shared memory panel moves
 *     python3 sender.py --record         # the three recordings
 *
 * THE SKY IS ELEMENTS. A band is one ribbon of rounded segments, set once
 * per message under a texture cache; its drift and its breath are bound
 * to one clock, so between two messages nothing is described again.
 *
 * EDIT THESE FIRST
 *   data/content.json  the words, and each door: its address, its
 *                      recording, its credentials, whether a schema reads it
 *   feed_sky.fbs       what a sky is: its bands, its wind and its palette
 *   kSpacing           how far apart the bands stand
 *   kSegment           the length of a ribbon's segments, and kGap after
 */

// TAGS: Data/Schema, Data/Sources, Runtime/Resources

#include <choreograph/Choreograph.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Connection.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Panel.h>
#include <sigilsketch/kit/Theme.h>

#include <cstddef>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "feed_sky_generated.h"

namespace sketch = sigil::sketch;
namespace data = sigil::data;
namespace motion = sigil::motion;
namespace material = sigil::material;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1280, 600};
constexpr double kCaptureAt = 2.0;  // by now several messages have landed

/** THE SKY'S OWN COORDINATES, the ones a message's heights and speeds are
 *  written in, and the width a panel shows it at. */
constexpr float kSkyWidth = 1280;
constexpr float kSkyHeight = 720;
constexpr float kShownWidth = 360;
constexpr float kSkyScale = kShownWidth / kSkyWidth;
constexpr float kPanelGap = 24;

constexpr float kTop = 80;       // where the first band stands
constexpr float kSpacing = 104;  // how far apart the bands stand
constexpr float kSegment = 260;  // a ribbon's segment, and its gap after
constexpr float kGap = 70;
/** Enough segments that a ribbon drifted anywhere across one period still
 *  reaches both edges of the sky. */
constexpr size_t kSegments = (size_t)(kSkyWidth / (kSegment + kGap)) + 2;
/** A band breathes on a sine of this many radians a second, each band a
 *  radian further along than the one above it. */
constexpr float kBreath = 0.7f;

/** One door of the sheet: what `data/content.json` says about it, the
 *  connection it opened, and what its panel was last described from. */
struct Door {
  const data::Json* entry = nullptr;
  data::Connection connection;
  data::Connection::Vitals shown;

  const data::Json& operator[](std::string_view key) const {
    return (*entry)[key];
  }

  /** Answers whether the door's vitals moved since its panel was
   *  described. The sky itself is the door's NEWEST message, not the
   *  backlog: a sky is a state, so a reader that fell behind draws what
   *  is true now rather than every step that led to it. */
  bool read() {
    const data::Connection::Vitals now = connection.vitals();
    if (now == shown) return false;
    shown = now;
    return true;
  }
};

/** The URI a door opens: its address, and for a door that answers with a
 *  certificate, the pair beside this sketch that it answers with. */
std::string uriOf(const Door& door, const sketch::SketchContext& ctx) {
  std::string uri(door["address"].text());
  if (!door["certificate"].null())
    uri += "?cert=" + ctx.local(door["certificate"].text()) +
           "&key=" + ctx.local(door["key"].text());
  return uri;
}

/** Band @p index's colour, taken from the palette in turn; a message with
 *  no palette tints every band a translucent white. */
material::Color tintOf(std::span<const data::Json> palette, size_t index) {
  if (palette.empty()) return {1, 1, 1, 0.47f};
  const data::Json& tint = palette[index % palette.size()];
  return {(float)tint["r"].number(1), (float)tint["g"].number(1),
          (float)tint["b"].number(1), (float)tint["a"].number(1)};
}

/** The door's state in one line, for the panel that has no sky yet. */
std::string waitingLine(const Door& door) {
  const data::Connection::Vitals& vitals = door.shown;
  if (!vitals.error.empty()) return vitals.error;
  if (vitals.undecodable != 0)
    return std::to_string(vitals.undecodable) + " arrivals were no sky";
  if (vitals.closed) return "the door is closed: nothing else is coming";
  return "waiting at " +
         (vitals.localAddress.empty() ? std::string(door["address"].text())
                                 : vitals.localAddress) +
         " · nothing has arrived";
}

struct FeedSky {
  /** The words and the doors, read from beside this sketch. */
  sketch::kit::Document words;
  /** Each door, in the order the sheet shows them. A door is the same feed
   *  for the same URI while anybody holds it, so a reload that sets this
   *  sketch up again is handed the one it was already reading. */
  std::vector<Door> doors;
  /** The one clock every band's drift and breath is bound to. */
  motion::Animatable<float> clock = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = kCaptureAt});
    ctx.engine.add([this, seconds = 0.0](double step) mutable {
      seconds += step;
      clock = (float)seconds;
      return true;
    });

    words = sketch::kit::Document(ctx, "data/content.json");
    sigil::io::Hub& hub = ctx.assets.hub();
    doors.clear();
    for (const data::Json& entry : words["doors"].items()) {
      Door door{.entry = &entry};
      const std::string uri = uriOf(door, ctx);
      // A CAPTURE READS THE RECORDING, a window opens the door. Replaying
      // the URI from the file is the whole of the difference: every later
      // ask for the URI answers the replaying feed.
      if (ctx.deterministic)
        hub.replay(uri, ctx.local(entry["recording"].text()));
      door.connection =
          entry["schema"].boolean()
              ? data::Connection(hub, uri, data::schema<feed_sky::Sky>())
              : data::Connection(hub, uri);
      door.read();
      doors.push_back(std::move(door));
    }
    describe(ctx);
  }

  /** The data path: a door whose vitals moved has a new sky and a new
   *  readout, and nothing else in the scene is described again. */
  void update(double, sketch::SketchContext& ctx) {
    bool moved = false;
    for (Door& door : doors) moved = door.read() || moved;
    if (moved) describe(ctx);
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide sheet(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    ctx.composer.render(sketch::kit::page(
        {.title = words.phrase(words["title"]),
         .subtitle = words.phrase(words["subtitle"]),
         .footer = words.phrase(words["footer"])},
        sketch::kit::panelGrid(
            {.cells = each(doors, [this](const Door& door) { return panel(door); }),
             .columns = 0,
             .gap = kPanelGap})));
  }

  /** ONE DOOR: its name, the sky it delivered, what the door is, and what
   *  it says about itself. */
  Element panel(const Door& door) const {
    return sketch::kit::panel(
        {.eyebrow = door["label"].text(), .ruled = true},
        box().column().gap(14).children(
            {sketch::kit::well({.width = Dimension(kShownWidth),
                                .height = Dimension(kSkyHeight * kSkyScale),
                                .ground = skyGround(),
                                .placed = true},
                               stack().children({sky(door), waiting(door)})),
             document::caption(door["note"].text()),
             sketch::kit::connectionReadout(
                 door.connection, {.door = door["address"].text(),
                                   .rows = {.measure = kShownWidth}})}));
  }

  /** THE NIGHT the bands cross: darker overhead, lifting toward the
   *  horizon. */
  static SurfacePaint skyGround() {
    return material::Paint::linearGradient(
        {0, 0}, {0, kSkyHeight * kSkyScale},
        {{0.02f, 0.025f, 0.06f, 1}, {0.07f, 0.06f, 0.14f, 1}},
        {.units = material::GradientUnits::Pixels});
  }

  /** The door's sky in its own coordinates, scaled into the panel: one
   *  ribbon per band of the newest message. */
  Element sky(const Door& door) const {
    const data::Json& state = door.connection.latest();
    const std::span<const data::Json> palette = state["palette"].items();
    const float wind = (float)state["wind"]["x"].number();
    return stack()
        .width(kSkyWidth)
        .height(kSkyHeight)
        .left(0)
        .top(0)
        .transformOrigin(pct(0), pct(0))
        .scale(kSkyScale)
        .children(each(state["bands"].items(),
                       [&](const data::Json& band, size_t index) {
                         return ribbon(band, index, tintOf(palette, index),
                                       wind);
                       }));
  }

  /** ONE BAND: a ribbon of segments the width of the sky, drifting on the
   *  wind plus its own speed and breathing by its wobble. The segments
   *  are set once per message and held under a texture cache; the two bound
   *  lanes move the ribbon without describing it again. */
  Element ribbon(const data::Json& band, size_t index, material::Color tint,
                 float wind) const {
    const float height = (float)band["height"].number();
    const float wobble = (float)band["wobble"].number();
    const float period = kSegment + kGap;
    // A raised cosine across [-wobble, wobble] is -wobble·cos, so the
    // breath starts a quarter turn early to run as a sine.
    const float breathStart =
        -((float)index + std::numbers::pi_v<float> * 0.5f) / kBreath;
    const float breathPeriod = 2 * std::numbers::pi_v<float> / kBreath;
    return box()
        .row()
        .gap(kGap)
        .left(-period)
        .top(kTop + kSpacing * (float)index)
        .translateX(motion::bind(clock, {.to = {0.0f, wind + (float)band["speed"].number()}, .wrap = period}))
        .translateY(motion::bind(clock, {.from = {breathStart, breathStart + breathPeriod}, .envelope = motion::envelope::cosine(), .to = {-wobble, wobble}}))
        .cache(Cache::Texture)
        .children(each(kSegments, [&] {
          return box()
              .width(kSegment)
              .height(height)
              .borderRadius({height * 0.5f})
              .fill(Fill::color(tint));
        }));
  }

  /** THE QUIET PLACEHOLDER, until a sky has arrived: one line where the
   *  bands would cross, and the door's own words about itself under it. */
  Element waiting(const Door& door) const {
    if (!door.connection.latest().null()) return box().display(Display::None);
    return box()
        .cover()
        .column()
        .justifyContent(Justify::Center)
        .alignItems(Align::Center)
        .gap(10)
        .padding(0, 16)
        .styleClass("caption")
        .children({kit::line({.length = pct(100)}).opacity(0.4f),
                   document::caption(waitingLine(door))});
  }
};

}  // namespace

SIGIL_SKETCH(FeedSky, "Data",
             "One sky arriving through three doors side by side — UDP read "
             "through the sketch's own schema, QUIC over one encrypted "
             "connection, and a shared memory region — each panel the newest "
             "message on its connection and the door's own vitals under it.")
