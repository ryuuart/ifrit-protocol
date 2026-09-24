/** @file
 * A sketch host in the test's own process, every command answered by
 * turning its loop.
 */

#include "sigilsketch/testing/InProcessHost.h"

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <thread>
#include <utility>

namespace sigil::sketch::testing {

namespace {

using protocol::Answer;
using namespace std::chrono_literals;

template <class Result>
protocol::Reply<Result> into(std::optional<Answer<Result>>& slot) {
  return [&slot](Answer<Result> answer) { slot.emplace(std::move(answer)); };
}

}  // namespace

InProcessHost::InProcessHost(InProcessHostOptions options)
    : m_options(std::move(options)) {
  std::error_code error;
  std::filesystem::create_directories(m_options.stateDirectory, error);
  if (!m_options.session.fonts) {
    m_fonts = std::make_unique<weave::FontContext>(
        weave::ports::systemFontManager());
    m_options.session.fonts = m_fonts.get();
  }
  protocol::Program program;
  program.name = m_options.program;
  program.version = m_options.version;
  program.stateRoot = m_options.stateDirectory;
  m_dispatcher = std::make_unique<protocol::Dispatcher>(std::move(program));
  m_agents = std::make_unique<HostAgents>(*m_dispatcher, m_options.session,
                                          m_options.catalog);
  m_client = std::make_unique<protocol::InProcess>(*m_dispatcher);
}

InProcessHost::~InProcessHost() {
  // The agents go before the client: its detaching then finds no agent
  // to hand back what it set, rather than opening the session again for
  // a host that is going.
  m_agents.reset();
  m_client.reset();
  m_dispatcher.reset();
}

protocol::values::Error InProcessHost::overdue() {
  return protocol::refusal(protocol::ErrorCode_failed,
                           "harness: no answer came within the patience");
}

bool InProcessHost::turnUntil(const std::function<bool()>& done) {
  const auto deadline = std::chrono::steady_clock::now() + m_options.patience;
  while (!done()) {
    if (std::chrono::steady_clock::now() >= deadline) return false;
    m_agents->frame();
    if (!done()) std::this_thread::sleep_for(1ms);
  }
  return true;
}

Answer<protocol::host::values::DescribeResult> InProcessHost::describe() {
  std::optional<Answer<protocol::host::values::DescribeResult>> answered;
  protocol::host::HostClient(caller()).describe(into(answered));
  return wait(answered);
}

Answer<protocol::session::values::Summary> InProcessHost::open(
    const std::string& sketch) {
  std::optional<Answer<protocol::session::values::Summary>> answered;
  protocol::session::values::OpenParameters parameters;
  parameters.sketch = sketch;
  protocol::session::SessionClient(caller()).open(parameters, into(answered));
  return wait(answered);
}

Answer<protocol::values::Empty> InProcessHost::clock(
    protocol::clock::Policy policy, std::optional<double> budgetSeconds) {
  std::optional<Answer<protocol::values::Empty>> answered;
  protocol::clock::values::SetPolicyParameters parameters;
  parameters.policy = policy;
  parameters.budget_seconds = budgetSeconds;
  protocol::clock::ClockClient(caller()).setPolicy(parameters, into(answered));
  return wait(answered);
}

Answer<protocol::clock::values::StepResult> InProcessHost::step(double seconds,
                                                                double rate) {
  std::optional<Answer<protocol::clock::values::StepResult>> answered;
  protocol::clock::values::StepParameters parameters;
  parameters.seconds = seconds;
  parameters.rate = rate;
  protocol::clock::ClockClient(caller()).step(parameters, into(answered));
  return wait(answered);
}

Answer<protocol::clock::values::CurrentResult> InProcessHost::current() {
  std::optional<Answer<protocol::clock::values::CurrentResult>> answered;
  protocol::clock::ClockClient(caller()).current(into(answered));
  return wait(answered);
}

Answer<protocol::session::values::StillResult> InProcessHost::still(
    double density, const std::string& path) {
  std::optional<Answer<protocol::session::values::StillResult>> answered;
  protocol::session::values::StillParameters parameters;
  parameters.density = density;
  parameters.path = path;
  protocol::session::SessionClient(caller()).still(parameters, into(answered));
  return wait(answered);
}

std::string InProcessHost::send(std::string_view envelope) {
  std::optional<std::string> heard;
  m_client->send(envelope, [&heard](std::string answer) {
    heard.emplace(std::move(answer));
  });
  turnUntil([&] { return heard.has_value(); });
  if (heard) return std::move(*heard);
  return R"({"error": {"code": "failed", "message": "harness: no answer came )"
         R"(within the patience"}})";
}

std::string InProcessHost::readout() {
  std::string text;
  text += "host.describe: ";
  text += send(R"({"id": "readout-describe", "method": "host.describe"})");
  text += "\nclock.current: ";
  text += send(R"({"id": "readout-clock", "method": "clock.current"})");
  text += "\nlast still: ";
  const std::filesystem::path& last = m_agents->session().lastStill();
  text += last.empty() ? std::string("none taken") : last.string();
  text += "\n";
  return text;
}

}  // namespace sigil::sketch::testing
