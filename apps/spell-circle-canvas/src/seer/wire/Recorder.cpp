/** @file
 * Starting and stopping a recording, and opening a URI back onto one.
 */

#include "sigilseer/wire/Recorder.h"

#include <utility>

#include "sigilseer/wire/Wires.h"

namespace sigil::seer {

Recorder::Recorder(Wires& wires) : m_wires(wires) {}

Recorder::~Recorder() { stop(); }

bool Recorder::record(const std::shared_ptr<io::Feed>& feed,
                      std::filesystem::path path) {
  if (!feed) return false;
  stop();
  feed->record(path);
  m_feed = feed;
  m_path = std::move(path);
  return true;
}

void Recorder::stop() {
  if (const std::shared_ptr<io::Feed> feed = m_feed.lock()) feed->record({});
  m_feed.reset();
  m_path.clear();
}

bool Recorder::recording() const { return !m_feed.expired(); }

std::shared_ptr<io::Feed> Recorder::replay(std::string_view uri,
                                           const std::filesystem::path& path) {
  // The door has to be let go before the file can take its place: a feed
  // answers for a URI as long as anyone holds it, so a wire still open
  // on this one would hand back the socket it already has.
  m_wires.close(uri);
  m_wires.mountRecording(uri, path);
  return m_wires.open(uri);
}

}  // namespace sigil::seer
