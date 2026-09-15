/** @file
 * The wires: opening one, closing one, and the reading each tick takes
 * off every one of them.
 */

#include "sigilseer/wire/Wires.h"

#include <sigilio/transport/Transport.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace sigil::seer {
namespace {

/** How far back a rate looks. A second is what a reader counts in, and
 *  it is long enough that a frame with no message in it does not read as
 *  a wire that went quiet. */
constexpr double kRateSeconds = 1.0;

}  // namespace

Wires::Wires() { io::registerTransports(m_hub); }

Wires::~Wires() = default;

std::shared_ptr<io::Feed> Wires::open(std::string_view uri,
                                      io::FeedPolicy policy) {
  if (const Watch* watch = watchOf(uri)) return watch->feed;
  std::shared_ptr<io::Feed> feed = m_hub.feed(uri, policy);
  m_watches.push_back({feed, {}});
  return feed;
}

bool Wires::close(std::string_view uri) {
  const auto found = std::find_if(
      m_watches.begin(), m_watches.end(),
      [uri](const Watch& watch) { return watch.feed->uri() == uri; });
  if (found == m_watches.end()) return false;
  // Closed before it is let go: the transport's end goes now rather than
  // whenever the last reader of an old answer happens to drop it.
  found->feed->close();
  m_watches.erase(found);
  const auto stale =
      std::find_if(m_vitals.begin(), m_vitals.end(),
                   [uri](const Vitals& vitals) { return vitals.uri == uri; });
  if (stale != m_vitals.end()) m_vitals.erase(stale);
  return true;
}

std::vector<std::shared_ptr<io::Feed>> Wires::feeds() const {
  std::vector<std::shared_ptr<io::Feed>> open;
  open.reserve(m_watches.size());
  for (const Watch& watch : m_watches) open.push_back(watch.feed);
  return open;
}

std::shared_ptr<io::Feed> Wires::feed(std::string_view uri) const {
  const Watch* watch = watchOf(uri);
  return watch ? watch->feed : nullptr;
}

void Wires::mountRecording(std::string_view uri,
                           const std::filesystem::path& path) {
  m_hub.mount(std::string(uri), path);
}

void Wires::dispatch(double seconds) { m_hub.dispatch(seconds); }

void Wires::readThrough(data::Schema schema) { m_schema = std::move(schema); }

void Wires::tick(double seconds) {
  m_vitals.clear();
  m_vitals.reserve(m_watches.size());
  for (Watch& watch : m_watches) {
    io::Feed& feed = *watch.feed;
    watch.samples.push_back({seconds, feed.generation()});
    // The oldest sample kept is the newest one that is already a whole
    // second old, so the span a rate is read over covers a second as
    // soon as a second has been ticked and never more than one tick
    // beyond it.
    while (watch.samples.size() > 2 &&
           seconds - watch.samples[1].at >= kRateSeconds)
      watch.samples.pop_front();

    const Sample& oldest = watch.samples.front();
    const Sample& newest = watch.samples.back();
    const double span = newest.at - oldest.at;

    Vitals vitals;
    vitals.uri = feed.uri();
    vitals.address = feed.address();
    vitals.error = feed.error();
    vitals.generation = newest.generation;
    vitals.dropped = feed.dropped();
    vitals.arrivalsPerSecond =
        span > 0 ? double(newest.generation - oldest.generation) / span : 0.0;
    vitals.closed = feed.closed();
    // The newest arrival whole, in one ask: the bytes and the sender
    // that came in together are read out together, so a wire that takes
    // a message between two asks cannot be shown one message's bytes
    // under another message's sender. A feed latches it rather than
    // queueing it, so a wire nobody drains names its sender too.
    if (const std::optional<io::Arrival> arrival = feed.newest()) {
      vitals.newest = arrival->bytes;
      vitals.lastFrom = arrival->from;
    }
    m_vitals.push_back(std::move(vitals));
  }
}

const Vitals* Wires::vitalsOf(std::string_view uri) const {
  for (const Vitals& vitals : m_vitals)
    if (vitals.uri == uri) return &vitals;
  return nullptr;
}

const Wires::Watch* Wires::watchOf(std::string_view uri) const {
  for (const Watch& watch : m_watches)
    if (watch.feed->uri() == uri) return &watch;
  return nullptr;
}

}  // namespace sigil::seer
