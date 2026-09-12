/** @file
 * The build directory one process's hosts share, and the walk that
 * clears the directories of runs that are gone.
 */

#include "BuildDirectory.h"

#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

#include "sigilsketch/live/Host.h"

namespace sigil::sketch {

namespace {

constexpr std::string_view kBuildDirectoryPrefix = "sigil_sketch_";

std::filesystem::path buildDirectoryFor(pid_t pid) {
  std::error_code ec;
  const std::filesystem::path root = std::filesystem::temp_directory_path(ec);
  if (ec) return {};
  return root / (std::string(kBuildDirectoryPrefix) + std::to_string(pid));
}

void removeBuildDirectory(const std::filesystem::path& directory) {
  if (directory.empty()) return;
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
}

void removeThisProcessBuildDirectory() {
  removeBuildDirectory(buildDirectoryFor(getpid()));
}

/** THE PID IN A BUILD DIRECTORY'S NAME, or zero when the name is not one
 *  of ours. Only all-digits after the prefix counts, so a scratch
 *  directory a test named for itself is left alone. */
pid_t processIdOfBuildDirectory(const std::string& name) {
  if (name.rfind(kBuildDirectoryPrefix, 0) != 0) return 0;
  const std::string digits = name.substr(kBuildDirectoryPrefix.size());
  // Longer than any pid can be: a name that is not a number at all.
  if (digits.empty() || digits.size() > 9) return 0;
  for (const unsigned char c : digits)
    if (std::isdigit(c) == 0) return 0;
  const long pid = std::strtol(digits.c_str(), nullptr, 10);
  return pid > 0 ? (pid_t)pid : 0;
}

/** True unless the system says NOBODY HOLDS @p pid. ESRCH is the only
 *  answer that means the process is gone; EPERM is a live one owned by
 *  another user, and anything else is an answer we did not understand,
 *  which is a reason to leave the directory standing. */
bool processAlive(pid_t pid) { return ::kill(pid, 0) == 0 || errno != ESRCH; }

std::mutex g_buildDirectoryMutex;
int g_buildDirectoryHosts = 0;

std::atomic<int> g_nextHostId{0};

}  // namespace

std::filesystem::path acquireBuildDirectory() {
  const std::filesystem::path directory = buildDirectoryFor(getpid());
  const std::lock_guard lock(g_buildDirectoryMutex);
  if (g_buildDirectoryHosts++ == 0) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    // The last host going out of scope is the ordinary end; this catches
    // a process that exits without unwinding to it, which is what the
    // window does.
    static const bool atExit =
        std::atexit(&removeThisProcessBuildDirectory) == 0;
    (void)atExit;
  }
  return directory;
}

void releaseBuildDirectory() {
  const std::lock_guard lock(g_buildDirectoryMutex);
  if (--g_buildDirectoryHosts == 0) removeThisProcessBuildDirectory();
}

int nextHostId() { return ++g_nextHostId; }

namespace {

/** Whether this process's one walk has been claimed, so a host being
 *  built does not walk again after an owner already took it. Claimed
 *  rather than set by the walk, so that an owner starting the walk on a
 *  thread of its own has taken it before the first host can ask. */
std::atomic_bool g_swept{false};

}  // namespace

bool Host::claimSweep() { return !g_swept.exchange(true); }

void Host::sweepAbandonedBuildDirectories() {
  std::error_code ec;
  const std::filesystem::path root = std::filesystem::temp_directory_path(ec);
  if (ec) return;
  for (auto it = std::filesystem::directory_iterator(root, ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    const std::filesystem::path directory = it->path();
    std::error_code stat;
    if (!std::filesystem::is_directory(directory, stat) || stat) continue;
    const pid_t pid = processIdOfBuildDirectory(directory.filename().string());
    if (pid == 0 || processAlive(pid)) continue;
    removeBuildDirectory(directory);
  }
}

}  // namespace sigil::sketch
