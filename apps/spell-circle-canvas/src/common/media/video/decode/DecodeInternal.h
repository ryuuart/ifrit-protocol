#pragma once

/** @file
 * The decoder a `Video` stands on: the container it reads from, the codec
 * it feeds, the presentation-frame cache that follows the playhead, the
 * binding that makes a hardware frame drawable, and the small FFmpeg
 * readings every part of it shares.
 */

#include <include/core/SkImage.h>

#include "sigilmedia/advanced/Skia.h"
#include "sigilmedia/video/Video.h"

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
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Device.h"

namespace sigil::media {

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

/** What a conversion context was last configured for, so its colour
 *  details are set again only when the source changes. */
struct ConversionSource {
  int width = 0;
  int height = 0;
  int format = -1;
  int colorspace = -1;
  bool fullRange = false;
  bool operator==(const ConversionSource&) const = default;
};

/** THE CPU EXECUTOR: @p frame — transferred off the device first when
 *  @p onDevice — converted through libswscale to premultiplied sRGB as
 *  an immutable raster image. @p conversion and @p described are the
 *  caller's cached context and what it was configured for. */
sk_sp<SkImage> rasterizeFrame(const AVFrame* frame, bool onDevice,
                              bool streamHasAlpha, SwsContext*& conversion,
                              ConversionSource& described);

/** HOW A HARDWARE FRAME BECOMES AN IMAGE: its planes wrapped for the
 *  recorder that asks, once per recorder, and read back through the CPU
 *  executor when there is no recorder or the wrap is refused. Holds the
 *  decoded frame, and with it the device surface, for as long as any
 *  copy of the `Frame` does. */
class FrameBinding final : public DeviceBinding {
 public:
  FrameBinding(SharedFrame decoded, DeviceFrame surface,
               std::shared_ptr<device::Context> context, bool streamHasAlpha);
  ~FrameBinding() override;
  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override;

 private:
  std::mutex m_mutex;
  SharedFrame m_decoded;
  DeviceFrame m_surface;
  std::shared_ptr<device::Context> m_context;
  bool m_streamHasAlpha = false;
  skgpu::graphite::Recorder* m_recorder = nullptr;
  sk_sp<SkImage> m_wrapped;
  sk_sp<SkImage> m_raster;
};

struct Video::Decoder {
  struct CachedFrame {
    SharedFrame decoded;
    sk_sp<SkImage> raster;
    DeviceFrame surface;
    std::shared_ptr<FrameBinding> binding;
    double presentationSeconds = 0.0;
    double durationSeconds = 0.0;
    int64_t index = 0;
    bool hardwareDecoded = false;
  };

  explicit Decoder(const std::byte* source, size_t size, VideoOptions requested,
                   std::filesystem::path hint);
  ~Decoder();

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
  Frame materialize(CachedFrame& cached);
  /** The frame covering @p seconds of the stream, decoded here. */
  Frame frameAt(double seconds);
  double durationSeconds() const { return metadata.duration.count(); }

  VideoOptions options;
  std::filesystem::path nameHint;
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
  SwsContext* sws = nullptr;
  ConversionSource swsSource;
  std::shared_ptr<device::Context> deviceContext;
  Metadata metadata;
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

}  // namespace sigil::media
