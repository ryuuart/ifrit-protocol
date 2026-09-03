/** @file
 * The SigilSketch host's optional shared web engine: one configuration chosen
 * before use, one lazy bring-up, and one final shutdown.
 */

#include "sigilsketch/scry/SharedEngine.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

namespace sigil::sketch::scry {

namespace {

enum class Phase { Empty, Configured, Starting, Started, Closed };

struct SharedState {
  std::mutex mutex;
  std::condition_variable changed;
  Phase phase = Phase::Empty;
  std::optional<::sigil::scry::WebEngineConfig> config;
  std::shared_ptr<::sigil::scry::WebEngine> engine;
};

SharedState& state() {
  static SharedState one;
  return one;
}

}  // namespace

bool configureSharedEngine(::sigil::scry::WebEngineConfig config) {
  SharedState& shared = state();
  const std::lock_guard<std::mutex> lock(shared.mutex);
  if (shared.phase != Phase::Empty) return false;
  shared.config.emplace(std::move(config));
  shared.phase = Phase::Configured;
  return true;
}

std::shared_ptr<::sigil::scry::WebEngine> sharedEngine() {
  SharedState& shared = state();
  std::unique_lock<std::mutex> lock(shared.mutex);
  shared.changed.wait(lock,
                      [&shared] { return shared.phase != Phase::Starting; });
  if (shared.phase == Phase::Started) return shared.engine;
  if (shared.phase != Phase::Configured || !shared.config) return nullptr;

  ::sigil::scry::WebEngineConfig config = std::move(*shared.config);
  shared.config.reset();
  shared.phase = Phase::Starting;
  lock.unlock();
  std::shared_ptr<::sigil::scry::WebEngine> engine =
      ::sigil::scry::WebEngine::create(std::move(config));
  lock.lock();
  shared.engine = std::move(engine);
  shared.phase = Phase::Started;
  std::shared_ptr<::sigil::scry::WebEngine> result = shared.engine;
  lock.unlock();
  shared.changed.notify_all();
  return result;
}

void shutdownSharedEngine() {
  SharedState& shared = state();
  std::unique_lock<std::mutex> lock(shared.mutex);
  shared.changed.wait(lock,
                      [&shared] { return shared.phase != Phase::Starting; });
  if (shared.phase == Phase::Closed) return;
  std::shared_ptr<::sigil::scry::WebEngine> engine = std::move(shared.engine);
  shared.config.reset();
  shared.phase = Phase::Closed;
  lock.unlock();
  engine.reset();
}

}  // namespace sigil::sketch::scry
