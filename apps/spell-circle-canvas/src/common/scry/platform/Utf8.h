#pragma once

/** @file
 * An Ultralight string as a UTF-8 std::string. Internal: it names an
 * Ultralight type, and Ultralight is private to the library.
 */

#include <Ultralight/String.h>

#include <string>

namespace sigil::scry {

inline std::string toUtf8(const ultralight::String& str) {
  const ultralight::String8& utf8 = str.utf8();
  return std::string(utf8.data(), utf8.length());
}

}  // namespace sigil::scry
