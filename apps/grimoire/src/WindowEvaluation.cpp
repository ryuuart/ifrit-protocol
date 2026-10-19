/** @file
 * The window's evaluate door: `session.evaluate` and `session.still`
 * answered on the render thread for the sketch on screen.
 */

#include "WindowEvaluation.h"

#include <sigilprotocol/definition/Mount.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/live/agent/SessionAgent.h>

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QObject>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "GrimoireView.h"

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;
namespace values = protocol::session::values;

struct WindowEvaluation::State {
  std::filesystem::path stateRoot;
  uint64_t lastReply = 0;
  uint64_t stillsTaken = 0;
  /** The replies owed, by number; read and written only on the thread
   *  that drives the dispatcher. A reply let go here unanswered is
   *  answered `failed`, which is why they never travel to the render
   *  thread. */
  std::map<uint64_t, protocol::Reply<values::EvaluateResult>> evaluations;
  std::map<uint64_t, protocol::Reply<values::StillResult>> stills;
  /** WHERE AN ANSWER FROM THE RENDER THREAD ARRIVES: a call queued on it
   *  runs on the thread that drives the dispatcher. Declared last, so it
   *  goes first, and a call still queued is dropped before the replies it
   *  would answer are let go. */
  QObject arrivals;
};

namespace {

/** Whether the window has a sketch running for a command to reach. */
bool sketchRunning() {
  QMutexLocker lock(&GrimoireView::hostMutex);
  return GrimoireView::host && GrimoireView::host->live();
}

/** @p work HANDED TO THE RENDER THREAD for the sketch on screen, and its
 *  answer handed back to the reply numbered @p number in @p owed. */
template <class Result>
void handOver(QObject* arrivals,
              std::map<uint64_t, protocol::Reply<Result>>* owed,
              uint64_t number, const char* method,
              std::function<protocol::Answer<Result>(sketch::Host&)> work) {
  GrimoireView::runOnSketchThread([arrivals, owed, number, method,
                                   work = std::move(work)](sketch::Host* host) {
    auto answer = std::make_shared<protocol::Answer<Result>>(protocol::refusal(
        protocol::ErrorCode_failed,
        std::string(method) + ": the window shows no running sketch"));
    if (host && host->live()) {
      try {
        *answer = work(*host);
      } catch (const std::exception& error) {
        *answer = protocol::refusal(protocol::ErrorCode_failed,
                                    std::string(method) + ": " + error.what());
      }
    }
    QMetaObject::invokeMethod(
        arrivals,
        [owed, number, answer] {
          const auto found = owed->find(number);
          if (found == owed->end()) return;
          protocol::Reply<Result> reply = std::move(found->second);
          owed->erase(found);
          reply(std::move(*answer));
        },
        Qt::QueuedConnection);
  });
}

/** Where a still a client asked for is written: the path it named under
 *  @p root, or @p picked there; empty with the reason in @p why where the
 *  path would stand outside the root. */
std::filesystem::path stillPath(const std::filesystem::path& root,
                                const std::string& asked,
                                const std::string& picked, std::string* why) {
  const std::filesystem::path relative =
      std::filesystem::path(asked.empty() ? picked : asked).lexically_normal();
  if (relative.is_absolute() || relative.empty() || *relative.begin() == "..") {
    *why = asked + " is not a path under the state root " + root.string();
    return {};
  }
  return root / relative;
}

}  // namespace

WindowEvaluation::WindowEvaluation(protocol::Dispatcher& dispatcher,
                                   std::filesystem::path stateRoot)
    : m_state(std::make_unique<State>()) {
  m_state->stateRoot = std::move(stateRoot);
  State* state = m_state.get();

  dispatcher.mount(
      "session.evaluate",
      protocol::answerLater<values::EvaluateParameters, values::EvaluateResult>(
          "session.evaluate",
          [state](const values::EvaluateParameters& parameters,
                  protocol::Reply<values::EvaluateResult> reply) {
            if (!sketchRunning()) {
              reply(protocol::refusal(
                  protocol::ErrorCode_failed,
                  "session.evaluate: the window shows no running sketch"));
              return;
            }
            const uint64_t number = ++state->lastReply;
            state->evaluations.emplace(number, std::move(reply));
            handOver<values::EvaluateResult>(
                &state->arrivals, &state->evaluations, number,
                "session.evaluate",
                [code = parameters.code,
                 mode = sketch::evaluationMode(parameters.mode)](
                    sketch::Host& host)
                    -> protocol::Answer<values::EvaluateResult> {
                  std::string why;
                  const std::optional<sketch::Evaluation> evaluated =
                      host.evaluate(code, mode, &why);
                  if (!evaluated)
                    return protocol::refusal(protocol::ErrorCode_failed,
                                             "session.evaluate: " + why);
                  return sketch::evaluationResult(*evaluated);
                });
          }));

  dispatcher.mount(
      "session.still",
      protocol::answerLater<values::StillParameters, values::StillResult>(
          "session.still", [state](const values::StillParameters& parameters,
                                   protocol::Reply<values::StillResult> reply) {
            const auto refuse = [&reply](const std::string& why) {
              reply(protocol::refusal(protocol::ErrorCode_failed,
                                      "session.still: " + why));
            };
            if (!sketchRunning())
              return refuse("the window shows no running sketch");
            if (state->stateRoot.empty())
              return refuse("this window names no state root to write under");
            if (!std::isfinite(parameters.density) || parameters.density <= 0)
              return refuse(
                  "a density is a positive number of pixels per "
                  "canvas unit");
            char picked[64];
            std::snprintf(picked, sizeof picked, "stills/still-%04llu.png",
                          (unsigned long long)++state->stillsTaken);
            std::string why;
            const std::filesystem::path file =
                stillPath(state->stateRoot, parameters.path, picked, &why);
            if (file.empty()) return refuse(why);
            const uint64_t number = ++state->lastReply;
            state->stills.emplace(number, std::move(reply));
            handOver<values::StillResult>(
                &state->arrivals, &state->stills, number, "session.still",
                [file, density = parameters.density](sketch::Host& host)
                    -> protocol::Answer<values::StillResult> {
                  std::error_code error;
                  std::filesystem::create_directories(file.parent_path(),
                                                      error);
                  if (!host.capture(file, (float)density))
                    return protocol::refusal(
                        protocol::ErrorCode_failed,
                        "session.still: " +
                            (host.errorLog().empty()
                                 ? std::string("the still could not be drawn")
                                 : host.errorLog()));
                  values::StillResult result;
                  result.path = file.string();
                  result.width =
                      (uint32_t)std::ceil(host.canvasSize().width() * density);
                  result.height =
                      (uint32_t)std::ceil(host.canvasSize().height() * density);
                  result.seconds = host.elapsedSeconds();
                  return result;
                });
          }));
}

WindowEvaluation::~WindowEvaluation() = default;
