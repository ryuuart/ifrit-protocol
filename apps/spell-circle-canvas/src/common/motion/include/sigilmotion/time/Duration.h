#pragma once

/** @file
 * @ingroup motion-time
 *
 * `Duration`, the one time unit every length, delay, step and period in
 * this library is stated in: a `std::chrono` duration counted in seconds
 * as a double, so `380ms`, `1.2s` and `1s / 60.0` all convert to it at a
 * call site with no cast and no unit suffix on a field name, and
 * `readDuration`, which reads one from CSS text, `"300ms"` or `"0.3s"`.
 */

#include <charconv>
#include <chrono>
#include <cmath>
#include <optional>
#include <string_view>

namespace sigil::motion {

/** A LENGTH OF TIME, in seconds as a double.
 *
 *  Every chrono literal converts to it implicitly — `380ms`, `1.2s`,
 *  `std::chrono::microseconds(16667)` — because a double count of seconds
 *  loses nothing an integer count of a smaller unit held. `count()` reads
 *  it back as seconds, which is the unit a phase or a spring is written
 *  in. */
using Duration = std::chrono::duration<double>;

/** THE LENGTH OF TIME CSS TEXT @p text WRITES, or nothing where it writes
 *  none.
 *
 *  A time is one finite number with its unit straight after it, `s` or
 *  `ms`, in any case, with blank space around it ignored: `"300ms"`,
 *  `"0.3s"`, `"1.5S"`. CSS has no unitless time, so a bare `"0.3"` is
 *  refused rather than read as seconds or as milliseconds, which are the
 *  two meanings a number has in the two conventions an author arrives
 *  from. A negative time is read as written; a slot that cannot run
 *  backwards refuses it where it is used. */
inline std::optional<Duration> readDuration(std::string_view text) {
  const auto blank = [](char letter) {
    return letter == ' ' || letter == '\t' || letter == '\n' ||
           letter == '\r' || letter == '\f';
  };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  // A LEADING PLUS is the number's, and from_chars reads only the minus.
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
    if (!text.empty() && text.front() == '-') return std::nullopt;
  }
  if (text.empty()) return std::nullopt;
  double amount = 0.0;
  const char* const last = text.data() + text.size();
  const std::from_chars_result number =
      std::from_chars(text.data(), last, amount);
  if (number.ec != std::errc{} || !std::isfinite(amount)) return std::nullopt;
  const std::string_view unit(number.ptr, (size_t)(last - number.ptr));
  const auto lower = [](char letter) {
    return letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter;
  };
  if (unit.size() == 1 && lower(unit[0]) == 's') return Duration(amount);
  if (unit.size() == 2 && lower(unit[0]) == 'm' && lower(unit[1]) == 's')
    return Duration(amount / 1000.0);
  return std::nullopt;
}

}  // namespace sigil::motion
