/** @file
 * The session's policy clock, driven over the clock domain.
 */

#include "sigilsketch/live/agent/ClockAgent.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace sigil::sketch {

namespace {

namespace values = protocol::clock::values;
using protocol::Answer;
using protocol::ErrorCode_failed;
using protocol::refusal;

/** The slowest rate a step is taken at: a frame of a quarter second is
 *  the longest a frame clock moves at once. */
constexpr double kSlowestRate = 4.0;

/** What seconds may leave over whole frames and still count as none: the
 *  rounding a product of seconds and rate carries. */
constexpr double kLeftOver = 1e-12;

const char* nameOf(motion::ClockPolicy policy) {
  switch (policy) {
    case motion::ClockPolicy::Advance:
      return "Advance";
    case motion::ClockPolicy::Pause:
      return "Pause";
    case motion::ClockPolicy::PauseWhileLoading:
      return "PauseWhileLoading";
    case motion::ClockPolicy::Wall:
      break;
  }
  return "Wall";
}

}  // namespace

motion::ClockPolicy policyOf(protocol::clock::Policy policy) {
  switch (policy) {
    case protocol::clock::Policy_Advance:
      return motion::ClockPolicy::Advance;
    case protocol::clock::Policy_Pause:
      return motion::ClockPolicy::Pause;
    case protocol::clock::Policy_PauseWhileLoading:
      return motion::ClockPolicy::PauseWhileLoading;
    default:
      return motion::ClockPolicy::Wall;
  }
}

protocol::clock::Policy policyOf(motion::ClockPolicy policy) {
  switch (policy) {
    case motion::ClockPolicy::Advance:
      return protocol::clock::Policy_Advance;
    case motion::ClockPolicy::Pause:
      return protocol::clock::Policy_Pause;
    case motion::ClockPolicy::PauseWhileLoading:
      return protocol::clock::Policy_PauseWhileLoading;
    case motion::ClockPolicy::Wall:
      break;
  }
  return protocol::clock::Policy_Wall;
}

ClockAgent::ClockAgent(protocol::Dispatcher& dispatcher, SessionAgent& session)
    : m_dispatcher(dispatcher), m_session(session) {
  m_session.onBudgetExpired([this](double seconds) {
    values::BudgetExpiredEvent event;
    event.seconds = seconds;
    if (!protocol::clock::ClockEvents(m_dispatcher.events()).budgetExpired(event))
      std::fprintf(stderr, "clock.budgetExpired could not be sent\n");
  });
}

Answer<protocol::values::Empty> ClockAgent::setPolicy(
    const values::SetPolicyParameters& parameters) {
  m_session.setPolicy(policyOf(parameters.policy), parameters.budget_seconds);
  return protocol::values::Empty{};
}

void ClockAgent::step(const values::StepParameters& parameters,
                      protocol::Reply<values::StepResult> reply) {
  const motion::PolicyClock& clock = m_session.clock();
  if (clock.policy() != motion::ClockPolicy::Advance) {
    reply(refusal(ErrorCode_failed,
                  std::string("clock.step: the clock moves by steps only "
                              "under Advance, and its policy is ") +
                      nameOf(clock.policy())));
    return;
  }
  if (!std::isfinite(parameters.rate) || parameters.rate < kSlowestRate) {
    reply(refusal(ErrorCode_failed,
                  "clock.step: a rate under four frames a second steps "
                  "longer than one frame of a clock moves"));
    return;
  }
  const double frame = 1.0 / parameters.rate;
  uint64_t frames = parameters.frames;
  // What seconds leave over whole frames, taken as one shorter frame; zero
  // seconds is one frame of no length, which runs the sketch where it
  // stands.
  std::optional<double> remainder;
  if (parameters.seconds) {
    const double seconds = *parameters.seconds;
    if (!std::isfinite(seconds) || seconds < 0) {
      reply(refusal(ErrorCode_failed,
                    "clock.step: seconds is a finite time no earlier than "
                    "now"));
      return;
    }
    frames = (uint64_t)std::floor(seconds * parameters.rate);
    const double left = seconds - (double)frames * frame;
    if (frames == 0 || left > kLeftOver) remainder = std::max(0.0, left);
  }
  std::string why;
  if (!m_session.step(frames, frame, &why) ||
      (remainder && !m_session.step(1, *remainder, &why))) {
    reply(refusal(ErrorCode_failed, "clock.step: " + why));
    return;
  }
  values::StepResult result;
  result.seconds = clock.elapsed();
  result.frame = clock.frames();
  reply(result);
}

Answer<values::CurrentResult> ClockAgent::current() {
  const motion::PolicyClock& clock = m_session.clock();
  values::CurrentResult result;
  result.policy = policyOf(clock.policy());
  result.seconds = clock.elapsed();
  result.frame = clock.frames();
  result.paused = clock.held();
  result.time_scale = clock.timeScale();
  result.budget_remaining = clock.budgetRemaining();
  return result;
}

Answer<protocol::values::Empty> ClockAgent::pause(
    const values::PauseParameters& parameters) {
  m_session.setHeld(parameters.paused);
  return protocol::values::Empty{};
}

Answer<protocol::values::Empty> ClockAgent::setTimeScale(
    const values::TimeScaleParameters& parameters) {
  if (!std::isfinite(parameters.scale) || parameters.scale < 0)
    return refusal(ErrorCode_failed,
                   "clock.setTimeScale: a scale is a finite number of clock "
                   "seconds per wall second, none of them negative");
  m_session.setTimeScale(parameters.scale);
  return protocol::values::Empty{};
}

}  // namespace sigil::sketch
