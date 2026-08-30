/** @file
 * The logger bridge: Ultralight's levels mapped to LogLevel and every
 * message routed to the callback, or to stderr without one.
 */

#include "Logger.h"

#include <cstdio>

#include "Utf8.h"

namespace sigil::scry {

void CallbackLogger::LogMessage(ultralight::LogLevel level,
                                const ultralight::String& message) {
  LogLevel mapped = LogLevel::Info;
  if (level == ultralight::LogLevel::Error)
    mapped = LogLevel::Error;
  else if (level == ultralight::LogLevel::Warning)
    mapped = LogLevel::Warning;
  log(mapped, toUtf8(message));
}

void CallbackLogger::log(LogLevel level, const std::string& message) {
  if (m_callback) {
    m_callback(level, message);
    return;
  }
  if (level == LogLevel::Info) return;
  std::fprintf(stderr, "[SigilScry:%s] %s\n",
               level == LogLevel::Error ? "error" : "warning", message.c_str());
}

}  // namespace sigil::scry
