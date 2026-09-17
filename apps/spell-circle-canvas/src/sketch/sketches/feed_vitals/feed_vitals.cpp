/** @file
 * feed_vitals — the feed itself, on a meter.
 *
 * Every other sketch that opens a feed draws what the messages MEAN.
 * This one draws the feed: how many messages have arrived, when each of
 * them landed, how many fell off the queue behind a reader, what the
 * newest one holds as bytes, and where the door is. Nothing here is
 * decoded and nothing is inferred — every figure on the sheet is one
 * the feed answered.
 *
 * THE TWO READERS STAND SIDE BY SIDE. `receive()` hands out the
 * arrivals this reader has not taken yet, in order, and never waits: it
 * is what the strip chart is drawn from, one tick per message at the
 * time that message carries. `latest()` is the newest message and is
 * never dropped, whatever the queue does: it is what the row of hex
 * pairs is read from. A reader that falls behind loses the OLDEST
 * arrivals, and `dropped()` is the count of them — which is why that
 * number stands on this sheet beside the generation rather than being
 * left to a comment.
 *
 * `pulse.py` beside this file is both ends of the signal: it sends the
 * pulse to the port, and it writes the recording a capture replays. A
 * capture mounts that recording onto the URI, so the still is taken of
 * the same arrivals at the same seconds on every run.
 *
 *     python3 pulse.py                                  # a window meters
 *     python3 pulse.py --record data/pulse.feed --seconds 8
 *
 * EDIT THESE FIRST
 *   kAddress    the URI the pulse arrives on
 *   kRecording  what a capture replays instead of listening
 *   kWindow     how far back the strip chart reaches
 *   kHexBytes   how much of the newest message the row shows
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

constexpr SkSize kCanvas = {1280, 420};
constexpr double kCaptureAt = 3.0;  // a moment in one of the signal's gaps

constexpr float kStripWidth = 548;
constexpr float kCell = 196;
constexpr float kWideCell = 236;
constexpr float kPicture = 200;
constexpr float kGap = 14;

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

/** The plate the strip chart stands on, and the two the readings do. */
const sketch::kit::Cell kStripPlate{
    .plate = {.width = kStripWidth, .height = kPicture, .padding = 12}};
const sketch::kit::Cell kReading{
    .plate = {.width = kCell, .height = kPicture, .padding = 12}};
const sketch::kit::Cell kWideReading{
    .plate = {.width = kWideCell, .height = kPicture, .padding = 12}};

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
    const sketch::kit::Provide presentation(sketch::kit::specimenTheme());
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
    const sketch::kit::Provide presentation(sketch::kit::specimenTheme());
    // A paint program runs after the describe scope has closed, where the
    // theme in force is no longer this page's, so the colours the strip
    // is drawn in are read here and carried in by value.
    const sketch::kit::Theme& look = sketch::kit::theme();
    const SkColor4f rule = look.palette.rule;
    const SkColor4f figure = look.palette.figure;
    ctx.composer.render(sketch::kit::page(
        {.title = "A feed on a meter",
         .subtitle = "dials · the URI the pulse arrives on · the recording a "
                     "capture replays · how far back the strip reaches",
         .footer = "every figure here is one the feed answered: the ticks are "
                   "its arrivals, the row is its bytes, and a gap in the "
                   "strip is a silence on the wire"},
        sketch::kit::cells(
            {.cells = {sketch::kit::cell(
                           kStripPlate, "feed->receive()",
                           "one tick per message, placed by the time it "
                           "carries · the newest stands at the right, and "
                           "each hairline is one second back",
                           pen("feed_vitals.strip",
                               [this, rule, figure](Pen& pen) {
                                 strip(pen, rule, figure);
                               })
                               .cover()),
                       sketch::kit::cell(kReading, "feed->generation()",
                                         "every message the feed has taken, "
                                         "counted from one · and the oldest "
                                         "arrivals that fell off the queue "
                                         "behind this reader",
                                         counts()),
                       sketch::kit::cell(kWideReading, "feed->latest()",
                                         "the newest message as it arrived · "
                                         "never dropped, whatever the queue "
                                         "does, and never decoded here",
                                         bytes()),
                       sketch::kit::cell(kReading, "feed->address()",
                                         "the local end the transport bound · "
                                         "or the sentence saying why there is "
                                         "no door, since a feed that could "
                                         "not open still exists",
                                         door())},
             .gap = kGap})));
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
        .children(each(rows, [](const std::string& row) {
          return text(row).width(kCell - 24);
        }));
  }
};

}  // namespace

SIGIL_SKETCH(FeedVitals, "Kit · API",
             "a feed read as a signal rather than as a scene: its arrivals "
             "on a strip chart, its generation and its dropped count, the "
             "bytes of its newest message, and the door it opened")
