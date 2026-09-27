#pragma once

/** @file
 * @ingroup media-video
 * `Video`, an encoded clip opened as a seekable document whose frames
 * decode around a playhead; `VideoOptions`, what it is opened with; and
 * `Playback`, the decode pool several clips share so the thread that
 * draws never waits. Reached as `hub.load<media::Video>(uri)` or
 * `media::decode<media::Video>(bytes)`.
 */

/** @defgroup media-video Video
 *  Encoded container bytes opened as a seekable clip, the frames decoded
 *  around a playhead, the pool that keeps many clips fed without blocking
 *  the thread that draws, and frames appended to an encoder that finishes
 *  as a container's bytes.
 *  @{ */
/** @} */

#include <glm/vec2.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "sigilmedia/core/Decode.h"
#include "sigilmedia/core/Frame.h"
#include "sigilmedia/core/Metadata.h"

namespace sigil::media {

/**
 * THE DECODE POOL SEVERAL VIDEOS SHARE: a bounded set of worker threads
 * over one queue. A video opened with a pool asks it for the frame at a
 * time and answers the newest complete frame at once, so the thread that
 * draws never waits for a decoder; a video opened without one decodes on
 * the thread that asks. Share one pool across every video a scene shows.
 */
class Playback {
 public:
  /** What a pool is made with. */
  struct Options {
    /** Unset: a bounded count sized from the machine. Zero: no worker at
     *  all — every ask decodes on the calling thread before it returns,
     *  which is what a plate or a test wants when the answer must be
     *  the frame it asked for. */
    std::optional<size_t> workers;
  };

  explicit Playback(Options options = {});
  ~Playback();
  Playback(const Playback&) = delete;
  Playback& operator=(const Playback&) = delete;

  struct Impl;

 private:
  friend class Video;
  std::shared_ptr<Impl> m_impl;
};

/** WHAT A VIDEO IS OPENED WITH: `hub.load<media::Video>(uri, {…})` and
 *  `media::decode<media::Video>(bytes, {…})`. Left alone, it decodes on
 *  the thread that asks, keeps a short run of frames around the
 *  playhead, and takes the platform's video device when it opens. */
struct VideoOptions {
  /** Null: `frameAt` decodes on the calling thread. A pool: `frameAt`
   *  answers the newest complete frame and never waits. */
  std::shared_ptr<Playback> playback;
  /** Decoded frames kept around the playhead; 1 holds only the last one
   *  asked for, so every step backwards decodes forward from a seek
   *  again. */
  size_t cachedFrames = 4;
  HardwarePreference hardware = HardwarePreference::Preferred;

  bool operator==(const VideoOptions&) const = default;
};

/**
 * AN ENCODED CLIP, OPENED: the encoded bytes are held, packets decode on
 * demand, and a small cache of decoded frames follows the playhead.
 * Opening finds the best video stream and prepares its decoder; it does
 * not decode the whole file. One `frameAt()` reads it as it reads an
 * `Image`, looping as `Timing` says.
 *
 * A hardware frame stays on the device it was decoded on: its `Frame`
 * carries no `image` until `deviceImage(frame, recorder)` binds it for
 * the recorder that will draw it. Not safe to ask from two threads at
 * once; a pool is how several threads share decode work.
 */
class Video {
 public:
  /** What `hub.load<media::Video>` and `media::decode` take. */
  using Options = VideoOptions;

  Video(Video&& other) noexcept;
  Video& operator=(Video&& other) noexcept;
  ~Video();

  /** What the container said about the stream when it was opened. */
  const Metadata& metadata() const;
  /** The frame size in pixels. */
  glm::ivec2 size() const;
  /** How long one play lasts; zero when the container states none. */
  std::chrono::duration<double> duration() const;
  /** Whether the clip moves: more than one frame, or a length. */
  bool isRunning() const;
  /** Whether a frame can be answered now. A video opened with a pool
   *  answers false until the pool has decoded its first frame, which is
   *  what a host waits on before it shows a scene; one opened without a
   *  pool always can. */
  bool hasFrame() const;

  /** THE FRAME TO SHOW @p elapsed INTO PLAYBACK, under @p timing — a
   *  video repeats forever unless @p timing says otherwise. The frame
   *  covering that document time, or the one before it when frame
   *  durations leave a gap there. With a pool it is the newest frame
   *  the pool has decoded and the ask is queued; without one it decodes
   *  here. Empty when nothing could be decoded.
   *  @trap A stream with no presentation timestamps is placed by decode
   *  order and cannot be sought inside, so an ask behind the playhead
   *  reads it again from the beginning. */
  Frame frameAt(std::chrono::duration<double> elapsed,
                const Timing& timing = {}) const;

  /** The frame covering document time @p time, decoded on this thread
   *  with no looping and no pool: the worker half of a pool's decode. */
  Frame decodeAt(std::chrono::duration<double> time) const;

  /** Which way the decoder took the platform's video device. A device
   *  grants a limited number of decompression sessions and refuses on
   *  the first decode, so `decoding` is the fact once a frame has been
   *  decoded and `configured` the intent before. */
  HardwareUse hardware() const;

  struct Decoder;

 private:
  explicit Video(std::shared_ptr<Decoder> decoder, VideoOptions options);
  std::shared_ptr<Decoder> m_decoder;
  std::shared_ptr<Playback::Impl> m_pool;
  size_t m_slot = 0;

  friend std::optional<Video> decodeDocument(std::type_identity<Video>,
                                             std::span<const std::byte>,
                                             const VideoOptions&,
                                             const std::filesystem::path&);
};

/** @p bytes opened as a video, under @p options: the container and its
 *  best stream prepared, nothing decoded. Nothing when the container or
 *  codec cannot be opened under the hardware preference. */
std::optional<Video> decodeDocument(std::type_identity<Video>,
                                    std::span<const std::byte> bytes,
                                    const VideoOptions& options,
                                    const std::filesystem::path& nameHint);

/** What @p bytes say about themselves as a video, without keeping a
 *  decoder. */
std::optional<Metadata> probeDocument(std::type_identity<Video>,
                                      std::span<const std::byte> bytes,
                                      const std::filesystem::path& nameHint);

/** What a video is loaded with, under the name a resource library asks
 *  by, so a hub takes `load<media::Video>(uri, {.cachedFrames = 8})`. */
inline VideoOptions loadOptions(std::type_identity<Video>) { return {}; }

/** The name a resource library registers and asks for a video under. */
inline std::string_view meaningName(std::type_identity<Video>) {
  return "media.Video";
}

}  // namespace sigil::media
