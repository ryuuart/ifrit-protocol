#pragma once

/** @file
 * The logger Ultralight's Platform is handed: every message — the
 * engine's own, a page's console, the library's diagnostics — goes to
 * one callback tagged with a LogLevel, or to stderr when there is none.
 * Internal: it names an Ultralight type, and Ultralight is private to
 * the library.
 */

#include <Ultralight/platform/Logger.h>
#include <sigilscry/platform/LogLevel.h>

#include <functional>
#include <string>

namespace sigil::scry {

class CallbackLogger final : public ultralight::Logger {
 public:
  /** @p callback receives every message; when it is empty, Error and
   *  Warning go to stderr and Info is dropped. */
  explicit CallbackLogger(
      std::function<void(LogLevel, const std::string&)> callback)
      : m_callback(std::move(callback)) {}

  // ultralight::Logger
  void LogMessage(ultralight::LogLevel level,
                  const ultralight::String& message) override;

  /** The library's own diagnostics take the same route. */
  void log(LogLevel level, const std::string& message);

 private:
  std::function<void(LogLevel, const std::string&)> m_callback;
};

}  // namespace sigil::scry
