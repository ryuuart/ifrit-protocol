/** @file
 * The state root a process keeps what it writes between runs under.
 */

#include "sigilsketch/core/State.h"

#include <mutex>

namespace sigil::sketch {

namespace {

/** The root and the lock it is read under: a root is set before anything
 *  starts, and a test that points it at a scratch directory and back does
 *  so while nothing reads it — but a build finishing on a worker thread
 *  can ask at any time, so the read is guarded. */
struct Root {
  std::mutex lock;
  std::filesystem::path directory;
};

Root& root() {
  static Root instance;
  return instance;
}

}  // namespace

void setStateDirectory(const std::filesystem::path& directory) {
  Root& state = root();
  const std::lock_guard<std::mutex> hold(state.lock);
  state.directory = directory;
}

std::filesystem::path stateDirectory() {
  Root& state = root();
  const std::lock_guard<std::mutex> hold(state.lock);
  return state.directory;
}

std::filesystem::path stateLocation(std::string_view kind) {
  const std::filesystem::path directory = stateDirectory();
  if (directory.empty()) return {};
  return directory / kind;
}

}  // namespace sigil::sketch
