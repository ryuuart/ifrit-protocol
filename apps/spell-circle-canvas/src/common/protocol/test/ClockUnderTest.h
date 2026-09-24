/** @file
 * A CLOCK AGENT A CASE CAN SEE INTO: it records what it was asked,
 * holds a step's reply until the case answers it, and keeps the policy a
 * client set as that client's alone — gone when the client detaches, as
 * a real clock's override is.
 */

#pragma once

#include <sigilprotocol/clock/ClockAgent.h>
#include <sigilprotocol/dispatch/Dispatcher.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::protocol::test {

class ClockUnderTest : public clock::ClockAgent {
 public:
  /** Mounted on @p dispatcher, with its override cleared when the client
   *  that set it detaches. */
  explicit ClockUnderTest(Dispatcher& dispatcher) : m_dispatcher(dispatcher) {
    clock::wire(dispatcher, *this);
    dispatcher.onDetach([this](const std::string& session) {
      if (m_override && m_override->first == session) m_override.reset();
    });
  }

  /** How many times step reached this agent. */
  int steps = 0;
  /** The reply of the step being held, until the case answers it. */
  std::optional<Reply<clock::values::StepResult>> heldStep;
  /** Whether a step's reply is let go unanswered instead of held. */
  bool dropSteps = false;

  Answer<values::Empty> setPolicy(
      const clock::values::SetPolicyParameters& parameters) override {
    m_override.emplace(m_dispatcher.asking(), parameters.policy);
    return values::Empty{};
  }
  void step(const clock::values::StepParameters&,
            Reply<clock::values::StepResult> reply) override {
    ++steps;
    if (!dropSteps) heldStep = std::move(reply);
  }
  Answer<clock::values::CurrentResult> current() override {
    clock::values::CurrentResult now;
    now.policy = m_override ? m_override->second : clock::Policy_Wall;
    return now;
  }
  Answer<values::Empty> pause(const clock::values::PauseParameters&) override {
    return values::Empty{};
  }
  Answer<values::Empty> setTimeScale(
      const clock::values::TimeScaleParameters&) override {
    return values::Empty{};
  }

 private:
  Dispatcher& m_dispatcher;
  /** The policy one client set, under that client's session. */
  std::optional<std::pair<std::string, clock::Policy>> m_override;
};

}  // namespace sigil::protocol::test
