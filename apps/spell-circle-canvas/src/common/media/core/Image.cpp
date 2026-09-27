/** @file
 * The decoded image document: frames placed on the document's clock from
 * their durations, and the frame a playback time answers.
 */

#include "sigilmedia/core/Image.h"

#include <utility>

namespace sigil::media {

Image::Image(std::vector<Frame> frames, int repetitions)
    : m_frames(std::move(frames)),
      m_repetitions(m_frames.size() > 1 ? repetitions : -1) {
  std::chrono::duration<double> at{};
  int64_t index = 0;
  for (Frame& frame : m_frames) {
    frame.time = at;
    frame.index = index++;
    // A still has no length: its one frame stands at every time.
    if (m_frames.size() == 1) frame.duration = {};
    at += frame.duration;
  }
  m_duration = at;
  if (!m_frames.empty() && m_frames.front().image)
    m_size = m_frames.front().image.size();
}

std::shared_ptr<const Image> Image::of(Picture picture) {
  if (!picture) return std::make_shared<const Image>();
  std::vector<Frame> frames(1);
  frames.front().image = std::move(picture);
  return std::make_shared<const Image>(std::move(frames));
}

Frame Image::frameAt(std::chrono::duration<double> elapsed,
                     const Timing& timing) const {
  if (m_frames.empty()) return {};
  if (m_frames.size() == 1 || m_duration <= std::chrono::duration<double>{})
    return m_frames.front();
  const std::chrono::duration<double> time =
      timing.documentTime(elapsed, m_duration, m_repetitions);
  for (const Frame& frame : m_frames)
    if (time < frame.time + frame.duration) return frame;
  return m_frames.back();
}

}  // namespace sigil::media
