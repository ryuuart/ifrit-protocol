/** @file
 * The `Video` a caller holds: what it says about the clip it opened, the
 * frame it answers for a time, the draw that normalizes that time first,
 * and the two doors that open encoded bytes.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "DecodeInternal.h"

namespace sigil::video {

Video::Video(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Video::~Video() = default;

const VideoProbe& Video::probe() const { return m_impl->metadata; }

bool Video::hardwareConfigured() const { return m_impl->hardwareActive; }

bool Video::hardwareDecoding() const { return m_impl->decodedNative; }

VideoFrame Video::frameAt(double seconds, skgpu::graphite::Recorder* recorder) {
  return m_impl->frameAt(seconds, recorder);
}

VideoFrame Video::decodeAt(double seconds) {
  return m_impl->frameAt(seconds, nullptr, true);
}

bool Video::draw(SkCanvas& canvas, const SkRect& destination, double seconds,
                 const SkSamplingOptions& sampling, bool loop) {
  const double duration = m_impl->metadata.durationSeconds;
  if (duration > 0) {
    if (loop) {
      seconds = std::fmod(seconds, duration);
      if (seconds < 0) seconds += duration;
    } else {
      seconds = std::clamp(seconds, 0.0, std::nextafter(duration, 0.0));
    }
  }
  VideoFrame frame = frameAt(seconds, canvas.recorder());
  if (!frame.image) return false;
  canvas.drawImageRect(frame.image, destination, sampling);
  return true;
}

std::shared_ptr<Video> decodeVideo(const std::byte* bytes, size_t size,
                                   const DecodeOptions& options,
                                   const std::filesystem::path& pathHint) {
  if (!bytes || size == 0) return nullptr;
  auto impl = std::make_unique<Video::Impl>(bytes, size, options, pathHint);
  if (!impl->open()) return nullptr;
  return std::shared_ptr<Video>(new Video(std::move(impl)));
}

std::optional<VideoProbe> probeVideo(const std::byte* bytes, size_t size,
                                     const std::filesystem::path& pathHint) {
  DecodeOptions options;
  options.hardware = HardwarePreference::Disabled;
  std::shared_ptr<Video> video = decodeVideo(bytes, size, options, pathHint);
  if (!video) return std::nullopt;
  return video->probe();
}

}  // namespace sigil::video
