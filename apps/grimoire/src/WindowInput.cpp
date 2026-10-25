/** @file
 * The window's input door: `session.pointer` and `session.key` handed to
 * the render thread for the sketch on screen.
 */

#include "WindowInput.h"

#include <sigilprotocol/definition/Mount.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>

#include <QtCore/QMutexLocker>
#include <cmath>
#include <cstdio>
#include <exception>
#include <print>
#include <string>
#include <utility>

#include "GrimoireView.h"

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;
namespace values = protocol::session::values;

namespace {

/** Whether the window has a sketch running for input to reach. */
bool sketchRunning() {
  QMutexLocker lock(&GrimoireView::hostMutex);
  return GrimoireView::host && GrimoireView::host->live();
}

/** @p deliver handed to the render thread, where the sketch's session
 *  stands between two frames; a session that refuses it says so on the
 *  standard error, since the command was answered when it was handed
 *  over. */
template <class Deliver>
void handOver(const char* method, Deliver deliver) {
  GrimoireView::runOnSketchThread([method, deliver = std::move(deliver)](
                                      sketch::Host* host) {
    sketch::Session* session = host && host->live() ? host->session() : nullptr;
    if (!session) return;
    try {
      deliver(*session);
    } catch (const std::exception& error) {
      std::println(stderr, "[grimoire] {}: {}", method, error.what());
    }
  });
}

}  // namespace

void mountWindowInput(protocol::Dispatcher& dispatcher) {
  dispatcher.mount(
      "session.pointer",
      protocol::answerNow<values::PointerParameters, protocol::values::Empty>(
          "session.pointer",
          [](const values::PointerParameters& parameters)
              -> protocol::Answer<protocol::values::Empty> {
            if (!std::isfinite(parameters.x) || !std::isfinite(parameters.y))
              return protocol::refusal(
                  protocol::ErrorCode_failed,
                  "session.pointer: a pointer stands at a finite point on "
                  "the canvas");
            if (!sketchRunning())
              return protocol::refusal(
                  protocol::ErrorCode_failed,
                  "session.pointer: the window shows no running sketch");
            handOver("session.pointer", [parameters](sketch::Session& session) {
              session.pointer(parameters.x, parameters.y, parameters.pressed);
            });
            return protocol::values::Empty{};
          }));

  dispatcher.mount(
      "session.key",
      protocol::answerNow<values::KeyParameters, protocol::values::Empty>(
          "session.key",
          [](const values::KeyParameters& parameters)
              -> protocol::Answer<protocol::values::Empty> {
            if (parameters.name.empty())
              return protocol::refusal(
                  protocol::ErrorCode_failed,
                  "session.key: a key is named as a keyboard spells it");
            if (!sketchRunning())
              return protocol::refusal(
                  protocol::ErrorCode_failed,
                  "session.key: the window shows no running sketch");
            handOver("session.key", [parameters](sketch::Session& session) {
              session.key(parameters.name, parameters.code, parameters.pressed);
            });
            return protocol::values::Empty{};
          }));
}
