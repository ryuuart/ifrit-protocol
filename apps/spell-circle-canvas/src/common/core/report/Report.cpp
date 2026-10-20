/** @file
 * The reporting door: the installed reporter behind one lock, and the
 * standard error when none is installed.
 */

#include <sigilcore/report/Report.h>

#include <cstdio>
#include <mutex>
#include <utility>

namespace sigil::core {

namespace {

std::mutex& reporterLock() {
  static std::mutex lock;
  return lock;
}

Reporter& installedReporter() {
  static Reporter reporter;
  return reporter;
}

}  // namespace

std::string reportLine(std::string_view library, std::string_view message) {
  std::string line;
  line.reserve(library.size() + message.size() + 3);
  line += '[';
  line += library;
  line += "] ";
  line += message;
  return line;
}

void report(std::string_view library, std::string_view message) {
  // The reporter is copied out and called with the lock released, so one
  // that blocks or reports again holds up no other thread's warning.
  Reporter reporter;
  {
    const std::lock_guard<std::mutex> held(reporterLock());
    reporter = installedReporter();
  }
  if (reporter) {
    reporter(library, message);
    return;
  }
  const std::string line = reportLine(library, message) + '\n';
  std::fwrite(line.data(), 1, line.size(), stderr);
  std::fflush(stderr);
}

Reporter installReporter(Reporter reporter) {
  const std::lock_guard<std::mutex> held(reporterLock());
  return std::exchange(installedReporter(), std::move(reporter));
}

}  // namespace sigil::core
