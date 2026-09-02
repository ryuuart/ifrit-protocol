#pragma once

/** @file
 * A directory on disk for one test to work in, made on the way in and
 * removed on the way out.
 */

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sigil::test {

/** A DIRECTORY THIS TEST OWNS.
 *
 *  Its name carries the process id, so two runs of one binary side by
 *  side never write into one directory, and it carries the label the
 *  caller gives, so one case may hold several at once. It is emptied on
 *  the way in as well as on the way out: a run that died left its
 *  directory standing, and a case that found the last run's files in it
 *  would pass or fail on them.
 *
 *  Moving one hands the directory over; the source then owns nothing and
 *  removes nothing. */
class ScratchDir {
 public:
  explicit ScratchDir(std::string_view label) {
    path = std::filesystem::temp_directory_path() /
           (std::string(label) + "_" + std::to_string(::getpid()));
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ScratchDir(ScratchDir&& other) noexcept : path(std::move(other.path)) {
    other.path.clear();
  }
  ScratchDir& operator=(ScratchDir&&) = delete;
  ~ScratchDir() {
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }

  /** Writes @p content to @p name under this directory, making whatever
   *  directories stand above it. */
  void write(const std::string& name, std::string_view content) const {
    std::error_code ec;
    std::filesystem::create_directories((path / name).parent_path(), ec);
    std::ofstream(path / name, std::ios::binary) << content;
  }

  std::filesystem::path path;
};

}  // namespace sigil::test
