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
  m_recording = feed->record(path);
  if (m_recording.stopped()) return false;
  m_path = std::move(path);
  return true;
}

void Recorder::stop() {
  m_recording.stop();
  m_path.clear();
}

bool Recorder::recording() const { return !m_recording.stopped(); }

std::shared_ptr<io::Feed> Recorder::replay(std::string_view uri,
                                           const std::filesystem::path& path) {
  return m_wires.replay(uri, path);
}

}  // namespace sigil::seer
