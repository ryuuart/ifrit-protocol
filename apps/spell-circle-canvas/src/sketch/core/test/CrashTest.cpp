/** @file
 * The crash reporter: what a fault inside a guest prints, read back from
 * a child process that actually takes one.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Crash.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <csignal>
#include <functional>
#include <string>
#include <thread>

namespace {

using namespace sigil::sketch;

/** Everything a forked child writes to stderr before it dies, and the
 *  signal that killed it.
 *
 *  A FORK IS THE ONLY HONEST WAY TO ASK THIS. The handler re-raises the
 *  signal with the default disposition restored, which is what keeps a
 *  shell and a debugger seeing the real fault — so the process that
 *  takes the fault must be one this test can afford to lose. */
struct Faulted {
  std::string said;
  int signal = 0;
};

/** Runs @p body in a child that is expected to FAULT inside it, and
 *  reports what it said on the way out. */
Faulted faultIn(const std::function<void()>& body) {
  int pipes[2] = {-1, -1};
  EXPECT_EQ(::pipe(pipes), 0);
  const pid_t child = ::fork();
  if (child == 0) {
    ::dup2(pipes[1], STDERR_FILENO);
    ::close(pipes[0]);
    ::close(pipes[1]);
    body();  // takes the fault itself, inside whatever scope it set up
    ::_exit(0);  // unreachable unless the body did not fault
  }
  ::close(pipes[1]);
  Faulted out;
  // BOTH WAITS ARE COUNTED. A child that neither faults nor exits would
  // hold an open read and then an open wait forever, and a run that
  // hangs reports nothing at all where a run that fails names the claim
  // that broke. A report is a few hundred bytes, so a child still
  // writing after this many reads is one that will not stop.
  constexpr int kReads = 64;
  constexpr int kTurns = 5000;
  std::array<char, 4096> buffer{};
  for (int read = 0; read < kReads; ++read) {
    const ssize_t n = ::read(pipes[0], buffer.data(), buffer.size());
    if (n <= 0) break;
    out.said.append(buffer.data(), (size_t)n);
  }
  ::close(pipes[0]);
  int status = 0;
  for (int turn = 0; turn < kTurns; ++turn) {
    if (::waitpid(child, &status, WNOHANG) == child) break;
    if (turn + 1 == kTurns) {
      ::kill(child, SIGKILL);
      ::waitpid(child, &status, 0);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (WIFSIGNALED(status)) out.signal = WTERMSIG(status);
  return out;
}

}  // namespace

TEST(SketchCrash, NamesTheSketchItWasOnAndHowFarTheRunHadGot) {
  // The failure this exists for: a sweep opens a hundred sketches in one
  // process, one of them faults, and the process dies with nothing said —
  // leaving the last line another sketch happened to print as the only
  // evidence of which one it was.
  const Faulted report = faultIn([] {
    installCrashReporter("/somewhere/sweep");
    noteSketch("nine_slice");
    notePlates(48);
    PhaseMark mark(Phase::Draw);  // the fault lands INSIDE the phase
    ::raise(SIGSEGV);
  });
  EXPECT_EQ(report.signal, SIGSEGV);
  EXPECT_NE(report.said.find("nine_slice"), std::string::npos) << report.said;
  EXPECT_NE(report.said.find("48"), std::string::npos) << report.said;
  EXPECT_NE(report.said.find("SIGSEGV"), std::string::npos) << report.said;
  // …and the phase, which is what separates a fault in the sketch's own
  // body from one in the runtime painting what it described.
  EXPECT_NE(report.said.find("painting"), std::string::npos) << report.said;
}

TEST(SketchCrash, FallsBackToThePathWhenNoEntryIsNamed) {
  // The live host watches ONE file and names no entry; the path is the
  // whole of what it can say, and the report must still say it.
  const Faulted report = faultIn([] {
    installCrashReporter("/sketches/crossing_rule.cpp");
    ::raise(SIGSEGV);
  });
  EXPECT_EQ(report.signal, SIGSEGV);
  EXPECT_NE(report.said.find("crossing_rule.cpp"), std::string::npos)
      << report.said;
  EXPECT_EQ(report.said.find("plates:"), std::string::npos)
      << "a host that walks nothing has no plate count to report";
}
