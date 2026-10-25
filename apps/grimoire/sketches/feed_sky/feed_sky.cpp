/** @file
 * feed_sky — one sky, arriving four ways at once.
 *
 * The scene is a sky: bands crossing the canvas, the wind they drift on,
 * the palette they are tinted from — `feed_sky.fbs` beside this file says
 * so, and the build compiles it into `feed_sky_generated.h`. The sheet
 * holds four panels, so what a reader compares is the way in and nothing
 * else.
 *
 *   UDP · SCHEMA          a datagram per sky on a `data::Connection` read
 *                         through the schema: an arrival that does not
 *                         FIT it is counted as undecodable rather than
 *                         drawn as a sky missing its fields.
 *   SHARED MEMORY · JSON  a region another process on this machine wrote,
 *                         mapped read-only and looked at on a timer: no
 *                         socket, no copy through the kernel.
 *   FILE · SCHEMA JSON    `data/sky.json`, the schema's own JSON form,
 *                         read from the hub and converted through the same
 *                         schema. A file a sketch read is watched, so a
 *                         saved edit is a new sky in the window.
 *   RAW FEED · BYTES      the `io::Feed` under every door, read as itself:
 *                         `receive()` drains each arrival in order onto a
 *                         strip, `latest()` is the newest message as bytes
 *                         whatever the queue dropped, and the feed's own
 *                         revision and dropped count. A connection drains
 *                         its feed to decode it, so this panel listens on
 *                         a door of its own.
 *
 * Each connection panel is the NEWEST message on its door, not the
 * backlog: a sky is a state, so a reader that fell behind draws what is
 * true now. A field a message left out falls back to what the reading
 * asks for.
 *
 * ONE DOOR, TWO SOURCES. A URI the hub was told to replay is played back
 * from that recording as the hub's time moves forward, and any other
 * opens through the transport its scheme names. So a capture replays each
 * door's recording under `data/` at its whole URI and reads exactly what
 * a window hears, arrival for arrival, at the same seconds.
 *
 * `sender.py` beside this file sends both sky doors and writes both
 * recordings; `pulse.py` sends the raw feed's uneven rhythm of short
 * binary messages and writes its recording.
 *
 *     python3 sender.py                  # the UDP panel moves
 *     python3 sender.py --door shared    # the shared memory panel moves
 *     python3 sender.py --record         # the two recordings
 *     python3 pulse.py                   # the raw feed moves
 *
 * THE SKY IS ELEMENTS. A band is one ribbon of rounded segments, set once
 * per message under a texture cache; its drift and its breath are bound
 * to one clock, so between two messages nothing is described again.
 *
 * EDIT THESE FIRST
 *   data/content.json  the words, and each door: its address, its
 *                      recording, whether a schema reads it
 *   data/sky.json      the file's sky, in the schema's JSON form
 *   feed_sky.fbs       what a sky is: its bands, its wind and its palette
 *   kSpacing           how far apart the bands stand
 *   kSegment           the length of a ribbon's segments, and kGap after
 */

// TAGS: Data/Schema, Data/Sources, Runtime/Resources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Dialect.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Connection.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Panel.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "feed_sky_generated.h"

namespace sketch = sigil::sketch;
namespace data = sigil::data;
namespace io = sigil::io;
namespace motion = sigil::motion;
namespace material = sigil::material;
using namespace sigil::compose;
using sigil::draw::Pen;

namespace {

constexpr SkSize kCanvas = {1280, 1250};
constexpr double kCaptureAt = 2.0;  // by now several messages have landed

/** THE SKY'S OWN COORDINATES, the ones a message's heights and speeds are
 *  written in, and the width a panel shows it at. */
constexpr float kSkyWidth = 1280;
constexpr float kSkyHeight = 720;
constexpr float kShownWidth = 560;
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

constexpr float kWindow = 4.0f;   // seconds the raw strip reaches back
constexpr size_t kHexBytes = 16;  // how much of the newest message shows
constexpr size_t kHexPerRow = 8;  // pairs on one row of it

/** One door of the sheet: what `data/content.json` says about it, the
 *  connection it opened, and what its panel was last described from. */
struct Door {
  const data::Json* entry = nullptr;
  data::Connection connection;
  data::ConnectionState shown;

  const data::Json& operator[](std::string_view key) const {
    return (*entry)[key];
  }

  /** Answers whether the door's vitals moved since its panel was
   *  described. */
  bool read() {
    const data::ConnectionState now = connection.state();
    if (now == shown) return false;
    shown = now;
    return true;
  }
};

/** Band @p index's colour, taken from the palette in turn; a sky with no
 *  palette tints every band a translucent white. */
material::Color tintOf(std::span<const data::Json> palette, size_t index) {
  if (palette.empty()) return {1, 1, 1, 0.47f};
  const data::Json& tint = palette[index % palette.size()];
  return {(float)tint["r"].number(1), (float)tint["g"].number(1),
          (float)tint["b"].number(1), (float)tint["a"].number(1)};
}

/** The door's state in one line, for the panel that has no sky yet. */
std::string waitingLine(const Door& door) {
  const data::ConnectionState& vitals = door.shown;
  if (!vitals.error.empty()) return vitals.error;
  if (vitals.undecodable != 0)
    return std::to_string(vitals.undecodable) + " arrivals were no sky";
  if (vitals.readiness == io::ReadyState::Closed)
    return "the door is closed: nothing else is coming";
  return "waiting at " +
         (vitals.localAddress.empty() ? std::string(door["address"].string())
                                      : vitals.localAddress) +
         " · nothing has arrived";
}

struct FeedSky {
  /** The words and the doors, read from beside this sketch. */
  sketch::kit::Document words;
  /** Each connection door, in the order the sheet shows them. A door is
   *  the same feed for the same URI while anybody holds it, so a reload
   *  that sets this sketch up again is handed the one it was reading. */
  std::vector<Door> doors;
  /** The one clock every band's drift and breath is bound to. */
  motion::Animatable<float> clock = motion::animatable(0.0f);

  /** The file's sky, read through the schema; null with `fileWhy` saying
   *  why where the file is missing or does not fit. */
  data::Json fileSky;
  std::string fileWhy;

  /** The raw feed, and when each arrival still on its strip arrived, on
   *  the clock it was stamped with: drained in order, never revisited. */
  io::Feed pulse;
  std::deque<double> ticks;
  io::FeedState pulseShown;
  /** The scene time the strip is drawn against: the number the hub was
   *  moved to before this frame. */
  double now = 0;

  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = kCaptureAt});
    ctx.engine.timer(
        [this, seconds = 0.0](sigil::motion::Duration stepDuration) mutable {
          seconds += stepDuration.count();
          clock = (float)seconds;
          return true;
        });

    words = sketch::kit::Document(ctx, "data/content.json");
    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING, a window opens the door. Replaying
    // the URI from the file is the whole of the difference: every later
    // ask for the URI answers the replaying feed.
    const auto open = [&](const data::Json& entry) {
      const std::string uri(entry["address"].string());
      if (ctx.deterministic)
        hub.replay(uri, ctx.local(entry["recording"].string()));
      return uri;
    };
    doors.clear();
    for (const data::Json& entry : words["doors"].array()) {
      Door door{.entry = &entry};
      const std::string uri = open(entry);
      door.connection =
          entry["schema"].boolean()
              ? data::connect(hub, uri,
                              {.schema = data::schema<feed_sky::Sky>()})
              : data::connect(hub, uri);
      door.read();
      doors.push_back(std::move(door));
    }

    // THE FILE, through the same schema the UDP door reads with.
    fileSky = data::Json();
    fileWhy.clear();
    const std::string path(words["file"]["path"].string());
    if (const std::optional<std::string> text = hub.text(ctx.local(path))) {
      if (std::optional<data::Json> sky = data::decode(
              *text, data::Dialect::FlatBuffer, data::schema<feed_sky::Sky>()))
        fileSky = std::move(*sky);
      else
        fileWhy = path + " does not fit feed_sky.fbs";
    } else {
      fileWhy = path + " is missing";
    }

    // THE RAW FEED, read as the feed it is.
    pulse = hub.listen(open(words["raw"]));
    ticks.clear();
    pulseShown = {};
    now = 0;
    drain();
    describe(ctx);
  }

  /** The data path: a door whose vitals moved has a new sky and a new
   *  readout, and nothing else in the scene is described again. */
  void update(double seconds, sketch::Scene2DContext& ctx) {
    now = seconds;
    bool moved = drain();
    for (Door& door : doors) moved = door.read() || moved;
    if (moved) describe(ctx);
  }

  /** Takes every raw arrival since the last frame, in order, and lets go
   *  of the ones that scrolled off the strip. True when the feed's own
   *  state moved. */
  bool drain() {
    if (!pulse) return false;
    while (const std::optional<io::Message> arrival = pulse.receive())
      ticks.push_back(arrival->arrivedAt().count());
    while (!ticks.empty() && ticks.front() < now - (double)kWindow)
      ticks.pop_front();
    const io::FeedState reading = pulse.state();
    if (reading == pulseShown) return false;
    pulseShown = reading;
    return true;
  }

  void describe(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide sheet(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    std::vector<Element> panels =
        each(doors, [this](const Door& door) { return doorPanel(door); });
    panels.push_back(filePanel());
    panels.push_back(rawPanel());
    ctx.composer.render(sketch::kit::page(
        {.title = words.phrase(words["title"]),
         .subtitle = words.phrase(words["subtitle"]),
         .footer = words.phrase(words["footer"])},
        sketch::kit::panelGrid({.cells = std::move(panels),
                                .columns = 2,
                                .gap = kPanelGap,
                                .rowGap = 36})));
  }

  /** ONE PANEL: its name, its picture, what the way in is, and what it
   *  says about itself. */
  static Element panel(const data::Json& entry, Element picture,
                       Element readout) {
    return sketch::kit::panel(
        {.eyebrow = entry["label"].string(), .ruled = true},
        box().column().gap(14).children(
            {sketch::kit::well({.width = Dimension(kShownWidth),
                                .height = Dimension(kSkyHeight * kSkyScale),
                                .ground = skyGround(),
                                .placed = true},
                               std::move(picture)),
             document::caption(entry["note"].string()).width(kShownWidth),
             std::move(readout)}));
  }

  Element doorPanel(const Door& door) const {
    return panel(*door.entry,
                 stack().children({sky(door.connection.latest().payload),
                                   waiting(door.connection.latest().empty(),
                                           waitingLine(door))}),
                 sketch::kit::connectionReadout(
                     door.connection, {.door = door["address"].string(),
                                       .rows = {.measure = kShownWidth}}));
  }

  Element filePanel() const {
    const data::Json& entry = words["file"];
    return panel(
        entry,
        stack().children({sky(fileSky), waiting(fileSky.null(), fileWhy)}),
        kit::readout(
            std::vector<kit::Reading>{
                {.name = "file", .value = std::string(entry["path"].string())},
                {.name = "schema", .value = "feed_sky.fbs · root Sky"},
                {.name = "bands",
                 .value = fileSky.null()
                              ? fileWhy
                              : std::to_string(fileSky["bands"].size())}},
            {.measure = kShownWidth}));
  }

  Element rawPanel() const {
    const sketch::kit::Theme& look = sketch::kit::theme();
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the strip's colours are
    // read here and carried in by value.
    const material::Color rule = look.palette.rule;
    const material::Color figure = look.palette.figure;
    const data::Json& entry = words["raw"];
    return panel(
        entry,
        pen("feed_sky.strip",
            [this, rule, figure](Pen& pen) { strip(pen, rule, figure); })
            .cover(),
        kit::readout(
            std::vector<kit::Reading>{
                {.name = "door",
                 .value = pulseShown.localAddress.empty()
                              ? std::string(entry["address"].string())
                              : pulseShown.localAddress},
                {.name = "revision",
                 .value = kit::formatted(
                     "%llu  ·  dropped %llu",
                     (unsigned long long)pulseShown.revision,
                     (unsigned long long)pulseShown.dropped)},
                {.name = "newest", .value = bytes()}},
            {.measure = kShownWidth}));
  }

  /** THE NIGHT the bands cross: darker overhead, lifting toward the
   *  horizon. */
  static material::Material skyGround() {
    return sigil::material::linearGradient(
        {0, 0}, {0, kSkyHeight * kSkyScale},
        {{0.02f, 0.025f, 0.06f, 1}, {0.07f, 0.06f, 0.14f, 1}},
        {.units = material::GradientUnits::Pixels});
  }

  /** A sky in its own coordinates, scaled into the panel: one ribbon per
   *  band of @p state. */
  Element sky(const data::Json& state) const {
    const std::span<const data::Json> palette = state["palette"].array();
    const float wind = (float)state["wind"]["x"].number();
    return stack()
        .width(kSkyWidth)
        .height(kSkyHeight)
        .left(0)
        .top(0)
        .transformOrigin(percent(0), percent(0))
        .scale(kSkyScale)
        .children(each(
            state["bands"].array(), [&](const data::Json& band, size_t index) {
              return ribbon(band, index, tintOf(palette, index), wind);
            }));
  }

  /** ONE BAND: a ribbon of segments the width of the sky, drifting on the
   *  wind plus its own speed and breathing by its wobble. The segments
   *  are set once per message and held under a texture cache; the two
   *  bound lanes move the ribbon without describing it again. */
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
        .translateX(motion::bind(
            clock, {.to = {0.0f, wind + (float)band["speed"].number()},
                    .wrap = period}))
        .translateY(motion::bind(
            clock, {.from = {breathStart, breathStart + breathPeriod},
                    .envelope = motion::envelope::cosine(),
                    .to = {-wobble, wobble}}))
        .cache(Cache::Texture)
        .children(each(kSegments, [&] {
          return box()
              .width(kSegment)
              .height(height)
              .borderRadius({height * 0.5f})
              .fill(Fill::color(tint));
        }));
  }

  /** THE QUIET PLACEHOLDER, until there is a sky: one line where the bands
   *  would cross, and @p line under it. */
  static Element waiting(bool empty, const std::string& line) {
    if (!empty) return box().display(Display::None);
    return box()
        .cover()
        .column()
        .justifyContent(Justify::Center)
        .alignItems(Align::Center)
        .gap(10)
        .padding(0, 16)
        .styleClass("caption")
        .children({kit::line({.length = percent(100)}).opacity(0.4f),
                   document::caption(line)});
  }

  /** THE RAW ARRIVALS: one tick per message, placed by the time it
   *  carries against the scene clock, so a burst is a cluster and a
   *  silence is empty strip; a hairline a second gives the gaps a
   *  measure. */
  void strip(Pen& pen, material::Color rule, material::Color figure) const {
    const float base = pen.height - 24.0f;
    const float top = 24.0f;
    pen.strokeWeight(1);
    pen.stroke(rule);
    for (int second = 0; second <= (int)kWindow; ++second) {
      const float x = pen.width * (1.0f - (float)second / kWindow);
      pen.line(x, top, x, base);
    }
    pen.line(0, base, pen.width, base);
    pen.stroke(figure);
    pen.strokeWeight(2);
    for (const double at : ticks) {
      const float x = pen.width * (float)(1.0 - (now - at) / (double)kWindow);
      pen.line(x, top, x, base);
    }
    pen.noStroke();
  }

  /** THE NEWEST RAW MESSAGE AS BYTES. What a message means is its
   *  sender's and its reader's business; what a feed answers is bytes. */
  std::string bytes() const {
    const std::optional<io::Message> latest = pulse.latest();
    const std::shared_ptr<const io::Bytes> message =
        latest ? latest->payload : nullptr;
    if (!message) return "nothing has arrived";
    const size_t taken = std::min(message->size(), kHexBytes);
    std::string hex = kit::formatted("%zu bytes ·", message->size());
    for (size_t at = 0; at < taken; ++at)
      hex += kit::formatted(
          at % kHexPerRow == 0 && at != 0 ? "  %02x" : " %02x",
          (unsigned)std::to_integer<uint8_t>(message->span()[at]));
    return hex;
  }
};

}  // namespace

SIGIL_SKETCH(FeedSky, "Data",
             "One sky arriving four ways side by side — UDP read through the "
             "sketch's own schema, a shared memory region, the schema's JSON "
             "file watched on disk, and the raw feed's arrivals and bytes — "
             "each panel with what its way in says about itself.")
