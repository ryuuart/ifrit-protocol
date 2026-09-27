#pragma once

/** @file
 * @ingroup media-video
 * `Encoder`: frames appended one at a time and finished as the bytes of
 * a movie. Where the bytes go is a hub's.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sigilmedia/core/Format.h"
#include "sigilmedia/core/Frame.h"
#include "sigilmedia/core/Picture.h"

namespace sigil::media {

class Image;

/**
 * AN INCREMENTAL MOVIE ENCODER: one `append` is one output frame, and
 * `finish` answers the container's bytes. MP4 is H.264 through the
 * platform's encoder where it opens, OpenH264 otherwise; every input is
 * resized to the output and converted to the BT.709 limited-range matrix
 * the stream is tagged with, so a decoder reads back the colour it was
 * given. Not thread-safe.
 */
class Encoder {
 public:
  /** What an encoder is made with. The two sizes have no default worth
   *  guessing and must be set; the rest describe a plain progressive clip
   *  at a rate and a bit budget a caller may leave alone. */
  struct Options {
    /** Output width in pixels; every frame is scaled to it. Odd sizes
     *  are refused: interoperable 4:2:0 H.264 needs whole chroma
     *  samples. */
    int width = 0;
    int height = 0;  ///< Output height in pixels, the same way.
    Format format = Format::Mp4;
    /** The rate the container is stamped at: one append is one frame. */
    int framesPerSecond = 30;
    /** The bit budget the codec is asked to hold to, in bits a second. */
    int64_t bitRate = 12'000'000;
    HardwarePreference hardware = HardwarePreference::Preferred;

    bool operator==(const Options&) const = default;
  };

  /** An encoder under @p options; false when no codec or muxer accepted
   *  them, which a zero or odd size always does, with `error()` saying
   *  why. */
  explicit Encoder(Options options);
  ~Encoder();
  Encoder(Encoder&&) noexcept;
  Encoder& operator=(Encoder&&) noexcept;

  /** Encodes one frame. False when the pixels cannot be read or
   *  converted, when the codec refuses them, or when the encoder has
   *  finished or never opened; `error()` says which. A frame standing on
   *  a device is read back first. */
  bool append(const Frame& frame);
  /** The same for an image's first frame. */
  bool append(const Image& image);
  /** The same for a picture in hand. */
  bool append(const Picture& picture);

  /** Flushes the codec and muxer and answers the container's bytes.
   *  Empty when nothing was appended, because a movie is at least one
   *  frame. Finishing is terminal on either outcome: a second `finish()`
   *  and every later `append()` are refused and leave `frameCount()`
   *  where it stood. */
  std::vector<std::byte> finish();

  /** Why the last refusal happened; empty while nothing was refused. */
  const std::string& error() const;
  /** How many frames have been accepted. */
  int64_t frameCount() const;
  /** The codec that opened — the device's or OpenH264 — which says
   *  whether the platform took the work. */
  const std::string& codec() const;
  /** Whether a codec and muxer accepted the options. */
  explicit operator bool() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::media
