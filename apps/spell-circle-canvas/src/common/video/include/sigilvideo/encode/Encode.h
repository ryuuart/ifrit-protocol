#pragma once

/** @file
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

enum class Format {
  Mp4,
};

struct EncodeOptions {
  int width = 0;
  int height = 0;
  int framesPerSecond = 30;
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

  static std::unique_ptr<Encoder> make(Format format,
                                       const EncodeOptions& options);

  bool append(const SkPixmap& pixels);
  bool append(const SkImage& image);

  /** Flushes the codec and muxer. May be called once. */
  sk_sp<SkData> finish();

  const std::string& error() const;
  const std::string& codec() const;
  int64_t frameCount() const;

 private:
  struct Impl;
  explicit Encoder(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> m_impl;
};

std::optional<Format> formatForPath(const std::filesystem::path& path);
const char* extensionFor(Format format);

}  // namespace sigil::video
