#pragma once
/** @file
 * A Compose warning, said through the one reporting door under the
 * library's own name.
 */

#include <sigilcore/report/Report.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace sigil::compose::detail {

/** SAYS A WARNING ON BEHALF OF SigilCompose: @p format filled in from the
 *  arguments after it as `printf` fills it, handed to
 *  `sigil::core::report`, so a host that installed a reporter hears it and
 *  every other run finds it on the standard error as `[SigilCompose] …`.
 *  The message names no library and ends in no newline; the door adds
 *  the one and needs neither. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
inline void reportWarning(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  std::va_list measuring;
  va_copy(measuring, arguments);
  const int length = std::vsnprintf(nullptr, 0, format, measuring);
  va_end(measuring);
  std::string message;
  if (length > 0) {
    message.resize((size_t)length + 1);
    std::vsnprintf(message.data(), message.size(), format, arguments);
    message.resize((size_t)length);
  }
  va_end(arguments);
  sigil::core::report("SigilCompose", message);
}

}  // namespace sigil::compose::detail
