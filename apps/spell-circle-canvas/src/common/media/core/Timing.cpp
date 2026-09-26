/** @file
 * A caller's elapsed time placed on a document's clock: offset, rate, and
 * what happens past the document's end.
 */

#include <algorithm>
#include <cmath>

#include "sigilmedia/core/Frame.h"

namespace sigil::media {

std::chrono::duration<double> Timing::documentTime(
    std::chrono::duration<double> elapsed,
    std::chrono::duration<double> length, int repetitions) const {
  double time = (start + elapsed * rate).count();
  const double span = length.count();
  if (span <= 0.0) return std::chrono::duration<double>(std::max(0.0, time));
  const int plays = loop == Loop::Forever ? -1
                    : loop == Loop::Once  ? 1
                                          : repetitions;
  // A finished finite play holds its last frame, which is the time just
  // short of the end: the end itself belongs to no frame.
  const double last = std::nextafter(span, 0.0);
  // Before the beginning is the beginning, unless the caller asked for a
  // loop in both directions.
  if (loop != Loop::Forever) time = std::max(0.0, time);
  if (plays < 0) {
    const double wrapped = std::fmod(time, span);
    return std::chrono::duration<double>(wrapped < 0.0 ? wrapped + span
                                                       : wrapped);
  }
  if (time >= span * plays) return std::chrono::duration<double>(last);
  return std::chrono::duration<double>(std::fmod(time, span));
}

}  // namespace sigil::media
