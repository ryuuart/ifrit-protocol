/** @file
 * The harness's hooks in their order, its verbs, and the printer that
 * puts the host's own account ahead of a case's first failure.
 */

#include "sigilsketch/testing/Harness.h"

#include <unistd.h>

#include <cstdio>
#include <string>
#include <system_error>

namespace sigil::sketch::testing {

namespace {

/** The case whose host a failure is read from, while its body runs. */
Harness* g_running = nullptr;
/** Whether that case's account has been printed. */
bool g_printed = false;
/** How the running case's account is asked for. */
std::string (*g_readout)(Harness&) = nullptr;

/** GoogleTest's own printer, with the running harness's account printed
 *  ahead of the first failure of its case. Every event is handed on to
 *  the printer it wraps, which it owns. */
class ReadoutFirst final : public ::testing::TestEventListener {
 public:
  explicit ReadoutFirst(::testing::TestEventListener* printer)
      : m_printer(printer) {}
  ~ReadoutFirst() override { delete m_printer; }

  void OnTestProgramStart(const ::testing::UnitTest& unit) override {
    m_printer->OnTestProgramStart(unit);
  }
  void OnTestIterationStart(const ::testing::UnitTest& unit,
                            int iteration) override {
    m_printer->OnTestIterationStart(unit, iteration);
  }
  void OnEnvironmentsSetUpStart(const ::testing::UnitTest& unit) override {
    m_printer->OnEnvironmentsSetUpStart(unit);
  }
  void OnEnvironmentsSetUpEnd(const ::testing::UnitTest& unit) override {
    m_printer->OnEnvironmentsSetUpEnd(unit);
  }
  void OnTestSuiteStart(const ::testing::TestSuite& suite) override {
    m_printer->OnTestSuiteStart(suite);
  }
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const ::testing::TestCase& suite) override {
    m_printer->OnTestCaseStart(suite);
  }
#endif
  void OnTestStart(const ::testing::TestInfo& info) override {
    m_printer->OnTestStart(info);
  }
  void OnTestDisabled(const ::testing::TestInfo& info) override {
    m_printer->OnTestDisabled(info);
  }
  void OnTestPartResult(const ::testing::TestPartResult& result) override {
    if (result.failed() && g_running && g_readout && !g_printed) {
      g_printed = true;
      const std::string account = g_readout(*g_running);
      std::printf("[ HARNESS  ] what the host says, before the failure:\n%s",
                  account.c_str());
      std::fflush(stdout);
    }
    m_printer->OnTestPartResult(result);
  }
  void OnTestEnd(const ::testing::TestInfo& info) override {
    m_printer->OnTestEnd(info);
  }
  void OnTestSuiteEnd(const ::testing::TestSuite& suite) override {
    m_printer->OnTestSuiteEnd(suite);
  }
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const ::testing::TestCase& suite) override {
    m_printer->OnTestCaseEnd(suite);
  }
#endif
  void OnEnvironmentsTearDownStart(const ::testing::UnitTest& unit) override {
    m_printer->OnEnvironmentsTearDownStart(unit);
  }
  void OnEnvironmentsTearDownEnd(const ::testing::UnitTest& unit) override {
    m_printer->OnEnvironmentsTearDownEnd(unit);
  }
  void OnTestIterationEnd(const ::testing::UnitTest& unit,
                          int iteration) override {
    m_printer->OnTestIterationEnd(unit, iteration);
  }
  void OnTestProgramEnd(const ::testing::UnitTest& unit) override {
    m_printer->OnTestProgramEnd(unit);
  }

 private:
  ::testing::TestEventListener* m_printer;
};

/** Puts the wrapping printer in place of GoogleTest's own, once a
 *  process; a run whose printer was already taken out keeps its own. */
void installReadoutFirst() {
  static bool installed = false;
  if (installed) return;
  installed = true;
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  if (::testing::TestEventListener* printer =
          listeners.Release(listeners.default_result_printer()))
    listeners.Append(new ReadoutFirst(printer));
}

/** The directory a case keeps its state in, named for the case. */
std::filesystem::path directoryFor(const ::testing::TestInfo* info) {
  std::string name = info ? std::string(info->test_suite_name()) + "." +
                                std::string(info->name())
                          : std::string("case");
  for (char& letter : name)
    if (letter == '/') letter = '_';
  return std::filesystem::temp_directory_path() / "sigil-harness" /
         (name + "-" + std::to_string(::getpid()));
}

}  // namespace

Harness::Harness() = default;
Harness::~Harness() = default;

void Harness::SetUp() {
  installReadoutFirst();
  g_readout = [](Harness& harness) { return harness.readout(); };
  m_stateDirectory =
      directoryFor(::testing::UnitTest::GetInstance()->current_test_info());
  std::error_code error;
  std::filesystem::remove_all(m_stateDirectory, error);
  std::filesystem::create_directories(m_stateDirectory, error);
  SetUpStateDirectory(m_stateDirectory);
  InProcessHostOptions options;
  options.program = "Harness";
  options.stateDirectory = m_stateDirectory;
  SetUpHostOptions(options);
  m_host = std::make_unique<InProcessHost>(std::move(options));
  g_running = this;
  g_printed = false;
  SetUpOnHost();
}

void Harness::TearDown() {
  if (m_host) TearDownOnHost();
  g_running = nullptr;
  m_host.reset();
  std::error_code error;
  if (HasFailure()) {
    std::printf("[ HARNESS  ] state directory kept: %s\n",
                m_stateDirectory.string().c_str());
    return;
  }
  std::filesystem::remove_all(m_stateDirectory, error);
}

std::string Harness::readout() {
  if (!m_host) return "no host stands\n";
  return m_host->readout() +
         "state directory: " + m_stateDirectory.string() + "\n";
}

protocol::Answer<protocol::session::values::Summary> Harness::open(
    const std::string& sketch) {
  return m_host->open(sketch);
}

protocol::Answer<protocol::values::Empty> Harness::clock(
    protocol::clock::Policy policy, std::optional<double> budgetSeconds) {
  return m_host->clock(policy, budgetSeconds);
}

protocol::Answer<protocol::clock::values::StepResult> Harness::step(
    double seconds, double rate) {
  return m_host->step(seconds, rate);
}

protocol::Answer<protocol::session::values::StillResult> Harness::still(
    double density, const std::string& path) {
  return m_host->still(density, path);
}

Comparison Harness::compare(const protocol::session::values::StillResult& still,
                            const std::filesystem::path& expected) const {
  return testing::compare(still.path, expected);
}

}  // namespace sigil::sketch::testing
