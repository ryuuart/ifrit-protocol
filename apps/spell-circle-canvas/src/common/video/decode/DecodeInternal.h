#pragma once

/** @file
 * The decoder state a `Video` stands on: the container it reads from, the
 * codec it feeds, the presentation-frame cache that follows the playhead,
 * and the small FFmpeg readings every part of it shares.
 */

#include <include/core/SkImage.h>
#include <include/gpu/graphite/Recorder.h>

#include "sigilvideo/decode/Decode.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Device.h"

namespace sigil::video {

using SharedFrame = std::shared_ptr<AVFrame>;

inline std::string ffmpegError(int code) {
  char text[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(code, text, sizeof(text));
  return text;
}

inline double rational(AVRational value) {
  return value.den ? static_cast<double>(value.num) / value.den : 0.0;
}

inline bool pixelFormatHasAlpha(AVPixelFormat format) {
  const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
  return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_ALPHA);
}

struct Video::Impl {
  struct CachedFrame {
    SharedFrame decoded;
    sk_sp<SkImage> raster;
    sk_sp<SkImage> deviceImage;
    skgpu::graphite::Recorder* deviceRecorder = nullptr;
    NativeFrame native;
    double presentationSeconds = 0.0;
    double durationSeconds = 0.0;
    int64_t index = 0;
    bool hardwareDecoded = false;
  };

  explicit Impl(const std::byte* source, size_t size, DecodeOptions requested,
                std::filesystem::path hint);
  ~Impl();

  static int read(void* opaque, uint8_t* destination, int requested);
  static int64_t seek(void* opaque, int64_t offset, int whence);
  static AVPixelFormat chooseFormat(AVCodecContext* context,
                                    const AVPixelFormat* formats);

  bool openContainer();
  bool configureDecoder(bool useHardware);
  void resetDecoder();
  bool open();
  bool seekTo(double seconds);

  CachedFrame* decodeNext();
  CachedFrame* cachedAt(double seconds);
  sk_sp<SkImage> rasterize(const SharedFrame& decoded);
  VideoFrame materialize(CachedFrame& cached,
                         skgpu::graphite::Recorder* recorder,
                         bool decodeOnly = false);
  VideoFrame frameAt(double seconds, skgpu::graphite::Recorder* recorder);
  VideoFrame frameAt(double seconds, skgpu::graphite::Recorder* recorder,
                     bool decodeOnly);

  DecodeOptions options;
  std::filesystem::path pathHint;
  std::vector<uint8_t> encoded;
  size_t readPosition = 0;
  AVIOContext* avioContext = nullptr;
  AVFormatContext* formatContext = nullptr;
  AVStream* stream = nullptr;
  int videoStream = -1;
  AVCodecContext* codecContext = nullptr;
  AVBufferRef* hardwareDevice = nullptr;
  AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;
  AVPacket* packet = nullptr;
  AVFrame* receiveFrame = nullptr;
  /** What the conversion context was last configured for. */
  struct SwsSource {
    int width = 0;
    int height = 0;
    int format = -1;
    int colorspace = -1;
    bool fullRange = false;
    bool operator==(const SwsSource&) const = default;
  };
  SwsContext* sws = nullptr;
  SwsSource swsSource;
  std::shared_ptr<device::Context> deviceContext;
  VideoProbe metadata;
  std::deque<CachedFrame> cache;
  std::string error;
  double streamStartSeconds = 0.0;
  double cursorSeconds = -std::numeric_limits<double>::infinity();
  int64_t decodedSequence = 0;
  bool hardwareActive = false;
  // Whether frames arrive without presentation timestamps, so decode
  // order is the only order there is. The container says so when it
  // stamps nothing, and a frame that carries no timestamp says so too.
  bool timestampless = false;
  // The surface the last decoded frame actually arrived on. The hardware
  // configuration is chosen when the decoder opens; whether the device
  // hands back a native frame is only known once one has been decoded.
  bool decodedNative = false;
  bool sentDrain = false;
};

}  // namespace sigil::video
