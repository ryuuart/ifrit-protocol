#pragma once

/** @file
 * @ingroup sketch-live
 *
 * The clock domain's agent: the policy clock a protocol host's session
 * is drawn at, driven over the protocol — its policy and budget, a
 * client's steps, its hold and its speed.
 */

#include <sigilprotocol/clock/ClockAgent.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilsketch/live/agent/SessionAgent.h>

namespace sigil::sketch {

/** The motion library's policy for the definition's, and back: the two
 *  enumerations name the same four policies. */
[[nodiscard]] motion::ClockPolicy policyOf(protocol::clock::Policy policy);
[[nodiscard]] protocol::clock::Policy policyOf(motion::ClockPolicy policy);

/** THE CLOCK OF THE SESSION AGENT'S SESSION, answered over the clock
 *  domain. The clock itself is SigilMotion's `motion::PolicyClock`, held
 *  by the session agent whose frames it times; this agent reads and
 *  drives it and sends `clock.budgetExpired` when a budget runs out.
 *
 *  `clock.step` is refused unless the policy is Advance and a session is
 *  open, and at a rate under four frames a second; seconds are taken in
 *  whole frames of one over the rate, rounded to the nearest, as a plate
 *  is stepped. */
class ClockAgent final : public protocol::clock::ClockAgent {
 public:
  /** The clock of @p session's session, its events sent on
   *  @p dispatcher; both outlive it. */
  ClockAgent(protocol::Dispatcher& dispatcher, SessionAgent& session);

  ClockAgent(const ClockAgent&) = delete;
  ClockAgent& operator=(const ClockAgent&) = delete;

  protocol::Answer<protocol::values::Empty> setPolicy(
      const protocol::clock::values::SetPolicyParameters& parameters)
      override;
  void step(const protocol::clock::values::StepParameters& parameters,
            protocol::Reply<protocol::clock::values::StepResult> reply)
      override;
  protocol::Answer<protocol::clock::values::CurrentResult> current() override;
  protocol::Answer<protocol::values::Empty> pause(
      const protocol::clock::values::PauseParameters& parameters) override;
  protocol::Answer<protocol::values::Empty> setTimeScale(
      const protocol::clock::values::TimeScaleParameters& parameters)
      override;

 private:
  protocol::Dispatcher& m_dispatcher;
  SessionAgent& m_session;
};

}  // namespace sigil::sketch
