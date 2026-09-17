/** @file
 * A feed displayed as arrival timing, queue health and raw payload.
 * receive() drains arrivals in order; latest() supplies the newest message
 * independently of queue drops. The strip uses the timestamps carried by
 * those arrivals. A deterministic capture mounts the local pulse recording;
 * a live window listens on the same URI and pulse.py supplies example data.
 */

// TAGS: Runtime/Resources, Data/Sources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace io = sigil::io;

using namespace sigil::compose;
using sigil::draw::Pen;

namespace {

const char* kAddress = "udp://:27021";       // where the pulse arrives
const char* kRecording = "data/pulse.feed";  // what a capture replays

constexpr SkSize kCanvas = {1180, 720};
constexpr double kCaptureAt = 3.0;  // a moment in one of the signal's gaps

constexpr float kStripWidth = 736;
constexpr float kCell = 328;

constexpr float kWindow = 4.0f;   // seconds the strip chart reaches back
constexpr size_t kHexBytes = 32;  // how much of a message the row shows
constexpr size_t kHexPerRow = 8;  // pairs on one line of it

/** WHAT A FEED SAYS ABOUT ITSELF — the whole of it. A reading that
 *  differs from the one the sheet was written from is what says to write
 *  it again. */
struct Vitals {
  uint64_t generation = 0;
  uint64_t dropped = 0;
  bool closed = false;
  std::string address;
  std::string trouble;
  bool operator==(const Vitals&) const = default;
};

Vitals vitalsOf(const io::Feed& feed) {
  return {.generation = feed.generation(),
          .dropped = feed.dropped(),
          .closed = feed.closed(),
          .address = feed.address(),
          .trouble = feed.error()};
}

}  // namespace

namespace {

struct FeedVitals {
  /** The door. The same object for the same URI while anybody holds it. */
  std::shared_ptr<io::Feed> arrivals;
  /** When each message still on the strip arrived, on the clock it was
   *  stamped with: one tick each, drained in order and never revisited. */
  std::deque<double> ticks;
  /** What the sheet standing now was written from. */
  Vitals shown;
  /** The scene time the strip is drawn against — the same number the
   *  hub was moved to before this frame, so a tick's place on the strip
   *  is its own arrival time against the clock that delivered it. */
  double now = 0;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = kCaptureAt});

    io::Hub& hub = ctx.assets.hub();
    // A CAPTURE READS THE RECORDING BESIDE THIS FILE, a window listens.
    // Mounting the URI onto the file is the whole of the difference: the
    // line below opens either one, and the mount stands first because a
    // feed is made once per URI and every later ask answers that one.
    if (ctx.deterministic)
      hub.mount(kAddress, hub.resolve(ctx.local(kRecording)));
    arrivals = hub.feed(kAddress);

    ticks.clear();
    shown = {};
    now = 0;
    drain();
    describe(ctx);
  }

  void update(double seconds, sketch::SketchContext& ctx) {
    now = seconds;
    if (drain()) describe(ctx);
  }

  /** Takes every arrival since the last frame, in order, and lets go of
   *  the ones that have scrolled off the strip. True when the sheet has
   *  to be written again. */
  bool drain() {
    if (!arrivals) return false;
    while (const std::optional<io::Arrival> arrival = arrivals->receive())
      ticks.push_back(arrival->at);
    while (!ticks.empty() && ticks.front() < now - (double)kWindow)
      ticks.pop_front();
    const Vitals reading = vitalsOf(*arrivals);
    if (reading == shown) return false;
    shown = reading;
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the colours the strip
    // is drawn in are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const SkColor4f rule = look.palette.rule;
    const SkColor4f figure = look.palette.figure;
    ctx.composer.render(sketch::kit::page(
        {.title = "Listen to the wire",
         .subtitle = "Arrival timing, queue health and the newest payload are "
                     "three views of the same feed.",
         .footer = "A still replays data/pulse.feed. A live window listens at "
                   "udp://:27021; pulse.py sends the example traffic."},
        box().column().gap(26).children(
            {box().row().gap(28).children(
                 {box()
                      .column()
                      .gap(12)
                      .width(kStripWidth)
                      .children(
                          {sketch::kit::sectionHeader(
                               {.label = "ARRIVALS",
                                .note = "One mark per message · receive()"}),
                           sketch::kit::well(
                               {.width = kStripWidth,
                                .height = 206,
                                .padding = 16},
                               pen("feed_vitals.strip",
                                   [this, rule, figure](Pen& pen) {
                                     strip(pen, rule, figure);
                                   })),
                           box()
                               .row()
                               .justify(Justify::SpaceBetween)
                               .padding(16, 0)
                               .children({text("−4 s").styleClass("readout"),
                                          text("−3 s").styleClass("readout"),
                                          text("−2 s").styleClass("readout"),
                                          text("−1 s").styleClass("readout"),
                                          text("NOW").styleClass("readout")})}),
                  sketch::kit::well(
                      {.width = 336, .height = 252, .padding = 22})
                      .column()
                      .gap(16)
                      .children(
                          {text("QUEUE HEALTH").styleClass("captionLabel"),
                           counts()})}),
             sketch::kit::sectionHeader({.label = "THE MOST RECENT MESSAGE",
                                         .note = "latest() stays available if "
                                                 "older arrivals are dropped"}),
             box().row().gap(28).children(
                 {sketch::kit::well(
                      {.width = 540, .height = 178, .padding = 20})
                      .row()
                      .gap(24)
                      .children({bytes(),
                                 text("Raw bytes, in arrival order. No "
                                      "decoding or interpretation is applied.")
                                     .width(208)}),
                  sketch::kit::well(
                      {.width = 532, .height = 178, .padding = 20})
                      .column()
                      .gap(14)
                      .children({text("TRANSPORT").styleClass("captionLabel"),
                                 door()})})})));
  }

  /** THE ARRIVALS: one tick per message, placed by the time that message
   *  carries against the scene clock, so a burst is a cluster and a
   *  silence is empty strip. */
  void strip(Pen& pen, SkColor4f rule, SkColor4f figure) {
    const float base = pen.height - 16.0f;
    const float top = 22.0f;
    pen.strokeWeight(1);
    pen.stroke(rule);
    // A hairline a second, so the gaps between bursts have a measure.
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

  /** The two counts a feed keeps: what it has taken, and what it lost at
   *  the front of a full queue. */
  Element counts() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return box()
        .column()
        .gap(8)
        .alignItems(Align::Start)
        .children(
            {text("GENERATION").styleClass("eyebrow").ink(look.palette.ash),
             text(kit::formatted("%llu", (unsigned long long)shown.generation))
                 .font(look.font({.size = 52, .mono = true},
                                 look.palette.figure)),
             text("DROPPED").styleClass("eyebrow").ink(look.palette.ash),
             text(kit::formatted("%llu", (unsigned long long)shown.dropped))
                 .font(
                     look.font({.size = 22, .mono = true}, look.palette.ink))});
  }

  /** THE NEWEST MESSAGE AS BYTES. What a message means is its sender's
   *  and its reader's business; what a feed answers is bytes, and this
   *  is them. */
  Element bytes() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    const std::shared_ptr<const io::Bytes> message =
        arrivals ? arrivals->latest() : nullptr;
    std::string headline = "nothing has arrived";
    std::vector<std::string> rows;
    if (message) {
      const size_t held = message->bytes.size();
      const size_t taken = std::min(held, kHexBytes);
      headline = taken < held
                     ? kit::formatted("%zu bytes · the first %zu", held, taken)
                     : kit::formatted("%zu bytes", held);
      std::string row;
      for (size_t at = 0; at < taken; ++at) {
        row += kit::formatted(
            "%02x ", (unsigned)std::to_integer<uint8_t>(message->bytes[at]));
        if ((at + 1) % kHexPerRow == 0) {
          rows.push_back(row);
          row.clear();
        }
      }
      if (!row.empty()) rows.push_back(row);
    }
    return box()
        .column()
        .gap(8)
        .alignItems(Align::Start)
        .children({text(headline).styleClass("eyebrow").ink(look.palette.ash),
                   box()
                       .column()
                       .gap(look.spacing.rowGap)
                       .font(look.font({.size = 13, .mono = true}))
                       .ink(look.palette.figure)
                       .children(each(rows, [](const std::string& row) {
                         return text(row);
                       }))});
  }

  /** Where the door is, and what is standing in the way of one. */
  Element door() {
    const sketch::kit::Theme& look = sketch::kit::theme();
    std::vector<std::string> rows{
        kit::formatted("uri     %s", kAddress),
        kit::formatted("address %s",
                       shown.address.empty() ? "-" : shown.address.c_str()),
        kit::formatted("state   %s", shown.closed ? "closed" : "open")};
    if (!shown.trouble.empty()) rows.push_back(shown.trouble);
    return box()
        .column()
        .gap(look.spacing.rowGap)
        .alignItems(Align::Start)
        .font(look.font({.size = 11, .mono = true}))
        .ink(look.palette.ash)
        .children(each(
            rows, [](const std::string& row) { return text(row).width(492); }));
  }
};

}  // namespace

SIGIL_SKETCH(FeedVitals, "Kit · API",
             "a feed read as a signal rather than as a scene: its arrivals "
             "on a strip chart, its generation and its dropped count, the "
             "bytes of its newest message, and the door it opened")
