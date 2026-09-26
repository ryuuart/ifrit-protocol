/** @file
 * The `Video` a caller holds: what it says about the clip it opened, the
 * frame it answers for a playback time — decoded here, or the newest one a
 * pool finished — and the doors that open and probe encoded bytes.
 */

#include <algorithm>
#include <memory>
#include <utility>

#include "DecodeInternal.h"
#include "PlaybackInternal.h"

namespace sigil::media {

Video::Video(std::shared_ptr<Decoder> decoder, VideoOptions options)
    : m_decoder(std::move(decoder)) {
  if (options.playback) {
    m_pool = options.playback->m_impl;
    m_slot = m_pool->add(m_decoder);
  }
}

Video::Video(Video&& other) noexcept = default;
Video& Video::operator=(Video&& other) noexcept = default;
Video::~Video() = default;

const Metadata& Video::metadata() const { return m_decoder->metadata; }

SkISize Video::size() const {
  return SkISize::Make(m_decoder->metadata.width, m_decoder->metadata.height);
}

std::chrono::duration<double> Video::duration() const {
  return m_decoder->metadata.duration;
}

bool Video::isRunning() const {
  return m_decoder->metadata.frames != 1 ||
         m_decoder->metadata.duration > std::chrono::duration<double>{};
}

bool Video::hasFrame() const { return !m_pool || m_pool->ready(m_slot); }

Frame Video::frameAt(std::chrono::duration<double> elapsed,
                     const Timing& timing) const {
  const std::chrono::duration<double> time =
      timing.documentTime(elapsed, m_decoder->metadata.duration);
  if (m_pool) return m_pool->request(m_slot, time.count());
  return m_decoder->frameAt(time.count());
}

Frame Video::decodeAt(std::chrono::duration<double> time) const {
  return m_decoder->frameAt(time.count());
}

HardwareUse Video::hardware() const {
  return {.configured = m_decoder->hardwareActive,
          .decoding = m_decoder->decodedNative};
}

std::optional<Video> decodeDocument(std::type_identity<Video>,
                                    std::span<const std::byte> bytes,
                                    const VideoOptions& options,
                                    const std::filesystem::path& nameHint) {
  if (bytes.empty()) return std::nullopt;
  auto decoder = std::make_shared<Video::Decoder>(bytes.data(), bytes.size(),
                                                  options, nameHint);
  if (!decoder->open()) return std::nullopt;
  return Video(std::move(decoder), options);
}

std::optional<Metadata> probeDocument(std::type_identity<Video>,
                                      std::span<const std::byte> bytes,
                                      const std::filesystem::path& nameHint) {
  VideoOptions options;
  options.hardware = HardwarePreference::Disabled;
  std::optional<Video> video =
      decodeDocument(std::type_identity<Video>{}, bytes, options, nameHint);
  if (!video) return std::nullopt;
  return video->metadata();
}

}  // namespace sigil::media
