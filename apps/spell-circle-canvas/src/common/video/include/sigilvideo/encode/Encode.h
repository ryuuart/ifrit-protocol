#pragma once

/** @file
 * @ingroup video-encode
 * The SigilVideo encode surface. Frames enter as Skia pixels and finish as
 * encoded container bytes. Resource names and storage belong to SigilIO.
 */

#include <include/core/SkRefCnt.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "sigilvideo/Types.h"

class SkData;
class SkImage;
class SkPixmap;

namespace sigil::video {

/** The containers an encoder can be asked for. */
enum class Format {
  Mp4,
};

/** What `Encoder::make()` is given. The two sizes have no default worth
 *  guessing and must be set; the rest describe a plain progressive clip at
 *  a rate and a bit budget a caller may leave alone. */
struct EncodeOptions {
  /** Output width in pixels. Every appended frame is scaled to it. */
  int width = 0;
  /** Output height in pixels. Every appended frame is scaled to it. */
  int height = 0;
  /** The rate the container is stamped at: one append is one frame at it. */
  int framesPerSecond = 30;
  /** The bit budget the codec is asked to hold to, in bits per second. */
  int64_t bitRate = 12'000'000;
  HardwarePreference hardware = HardwarePreference::Preferred;

  bool operator==(const EncodeOptions&) const = default;
};

/** Incremental video encoder. One append is one output frame. */
class Encoder {
 public:
  ~Encoder();
  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;

  /** Opens an encoder for @p format. Null when the codec or muxer refuses
   *  the options, which a zero width or height always does. */
  static std::unique_ptr<Encoder> make(Format format,
                                       const EncodeOptions& options);

  /** Encodes one frame. False when the pixels cannot be read or converted,
   *  when the codec refuses them, or when the encoder has already
   *  finished; `error()` says which. */
  bool append(const SkPixmap& pixels);
  /** Encodes one frame from an image, reading its pixels first. */
  bool append(const SkImage& image);

  /** Flushes the codec and muxer and answers the container bytes. Null
   *  when nothing was appended, because a video is at least one frame.
   *  Finishing is terminal on either outcome: a second `finish()` and
   *  every later `append()` are refused and leave `frameCount()` where it
   *  stood. */
  sk_sp<SkData> finish();

  /** Why the last refusal happened, or empty while nothing has been
   *  refused. */
  const std::string& error() const;
  /** The name of the codec that opened, which says whether the platform
   *  device took the work or the CPU did. */
  const std::string& codec() const;
  /** How many frames have been accepted so far. */
  int64_t frameCount() const;

 private:
  struct Impl;
  explicit Encoder(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> m_impl;
};

/** The format a path's extension names, or nothing when no format claims
 *  it. Meaning only: the path is never opened. */
std::optional<Format> formatForPath(const std::filesystem::path& path);
/** The extension a format is written with, leading dot included. */
const char* extensionFor(Format format);

}  // namespace sigil::video
