#include "sigilvideo/decode/Decode.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRect.h>
#include <include/gpu/graphite/Recorder.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "Device.h"

namespace sigil::video {
namespace {

std::string ffmpegError(int code) {
  char text[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(code, text, sizeof(text));
  return text;
}

struct FrameDeleter {
  void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

using SharedFrame = std::shared_ptr<AVFrame>;

SharedFrame cloneFrame(const AVFrame* frame) {
  return SharedFrame(av_frame_clone(frame), FrameDeleter{});
}

double rational(AVRational value) {
  return value.den ? static_cast<double>(value.num) / value.den : 0.0;
}

bool pixelFormatHasAlpha(AVPixelFormat format) {
  const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
  return descriptor && (descriptor->flags & AV_PIX_FMT_FLAG_ALPHA);
}

bool streamDeclaresAlpha(const AVStream& stream) {
  const AVCodecParameters& parameters = *stream.codecpar;
  if (parameters.alpha_mode != AVALPHA_MODE_UNSPECIFIED) return true;
  if (parameters.format >= 0 &&
      pixelFormatHasAlpha(static_cast<AVPixelFormat>(parameters.format)))
    return true;
  const AVDictionaryEntry* alpha =
      av_dict_get(stream.metadata, "alpha_mode", nullptr, 0);
  return alpha && std::strcmp(alpha->value, "0") != 0;
}

std::string formatHint(const std::filesystem::path& pathHint) {
  std::string hint = pathHint.string();
  if (const size_t suffix = hint.find_first_of("?#");
      suffix != std::string::npos)
    hint.resize(suffix);
  if (const size_t slash = hint.find_last_of("/\\"); slash != std::string::npos)
    hint.erase(0, slash + 1);
  return hint;
}

/** The libswscale coefficient table for a stream's tagged matrix. An
 *  untagged stream is treated as ITU-R BT.601, the same reading the
 *  device executor gives an untagged native frame, so the two executors
 *  agree on every clip. */
int swscaleColorspace(AVColorSpace colorspace) {
  switch (colorspace) {
    case AVCOL_SPC_BT709:
      return SWS_CS_ITU709;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      return SWS_CS_BT2020;
    case AVCOL_SPC_SMPTE240M:
      return SWS_CS_SMPTE240M;
    case AVCOL_SPC_FCC:
      return SWS_CS_FCC;
    default:
      return SWS_CS_ITU601;
  }
}

const AVCodec* alphaCodec(AVCodecID id) {
  if (id == AV_CODEC_ID_VP8) return avcodec_find_decoder_by_name("libvpx");
  if (id == AV_CODEC_ID_VP9) return avcodec_find_decoder_by_name("libvpx-vp9");
  return avcodec_find_decoder(id);
}

}  // namespace

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
                std::filesystem::path hint)
      : options(requested), pathHint(std::move(hint)) {
    options.cachedFrames = std::max<size_t>(1, options.cachedFrames);
    encoded.resize(size);
    std::memcpy(encoded.data(), source, size);
  }

  ~Impl() {
    sws_freeContext(sws);
    av_frame_free(&receiveFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    av_buffer_unref(&hardwareDevice);
    if (formatContext) avformat_close_input(&formatContext);
    if (avioContext) {
      av_freep(&avioContext->buffer);
      avio_context_free(&avioContext);
    }
  }

  static int read(void* opaque, uint8_t* destination, int requested) {
    auto* self = static_cast<Impl*>(opaque);
    const size_t remaining = self->encoded.size() - self->readPosition;
    if (remaining == 0) return AVERROR_EOF;
    const size_t count = std::min(remaining, static_cast<size_t>(requested));
    std::memcpy(destination, self->encoded.data() + self->readPosition, count);
    self->readPosition += count;
    return static_cast<int>(count);
  }

  static int64_t seek(void* opaque, int64_t offset, int whence) {
    auto* self = static_cast<Impl*>(opaque);
    if (whence == AVSEEK_SIZE)
      return static_cast<int64_t>(self->encoded.size());
    whence &= ~AVSEEK_FORCE;
    int64_t base = 0;
    if (whence == SEEK_CUR)
      base = static_cast<int64_t>(self->readPosition);
    else if (whence == SEEK_END)
      base = static_cast<int64_t>(self->encoded.size());
    else if (whence != SEEK_SET)
      return AVERROR(EINVAL);

    const int64_t position = base + offset;
    if (position < 0 || position > static_cast<int64_t>(self->encoded.size()))
      return AVERROR(EINVAL);
    self->readPosition = static_cast<size_t>(position);
    return position;
  }

  static AVPixelFormat chooseFormat(AVCodecContext* context,
                                    const AVPixelFormat* formats) {
    auto* self = static_cast<Impl*>(context->opaque);
    for (const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE;
         ++format)
      if (*format == self->hardwarePixelFormat) return *format;
    self->hardwareActive = false;
    if (self->options.hardware == HardwarePreference::Required)
      return AV_PIX_FMT_NONE;
    return formats[0];
  }

  bool openContainer() {
    constexpr int kIoBufferSize = 32 * 1024;
    uint8_t* ioBuffer = static_cast<uint8_t*>(av_malloc(kIoBufferSize));
    if (!ioBuffer) return false;
    avioContext = avio_alloc_context(ioBuffer, kIoBufferSize, 0, this, read,
                                     nullptr, seek);
    if (!avioContext) {
      av_free(ioBuffer);
      return false;
    }

    formatContext = avformat_alloc_context();
    if (!formatContext) return false;
    formatContext->pb = avioContext;
    formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
    const std::string name = formatHint(pathHint);
    const int opened = avformat_open_input(
        &formatContext, name.empty() ? nullptr : name.c_str(), nullptr,
        nullptr);
    if (opened < 0) {
      error = ffmpegError(opened);
      return false;
    }
    const int info = avformat_find_stream_info(formatContext, nullptr);
    if (info < 0) {
      error = ffmpegError(info);
      return false;
    }
    videoStream = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1,
                                      nullptr, 0);
    if (videoStream < 0) {
      error = ffmpegError(videoStream);
      return false;
    }

    stream = formatContext->streams[videoStream];
    const AVCodecParameters* parameters = stream->codecpar;
    metadata.width = parameters->width;
    metadata.height = parameters->height;
    metadata.codec = avcodec_get_name(parameters->codec_id);
    if (formatContext->iformat && formatContext->iformat->name)
      metadata.container = formatContext->iformat->name;
    const AVRational guessed =
        av_guess_frame_rate(formatContext, stream, nullptr);
    metadata.frameRate = rational(guessed);
    if (stream->duration != AV_NOPTS_VALUE)
      metadata.durationSeconds = stream->duration * rational(stream->time_base);
    else if (formatContext->duration != AV_NOPTS_VALUE)
      metadata.durationSeconds =
          static_cast<double>(formatContext->duration) / AV_TIME_BASE;
    metadata.frameCount = stream->nb_frames;
    if (metadata.frameCount <= 0 && metadata.frameRate > 0 &&
        metadata.durationSeconds > 0)
      metadata.frameCount = static_cast<int64_t>(
          std::llround(metadata.frameRate * metadata.durationSeconds));
    for (unsigned i = 0; i < formatContext->nb_streams; ++i)
      metadata.hasAudio |=
          formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
    metadata.hasAlpha = streamDeclaresAlpha(*stream);
    streamStartSeconds = stream->start_time == AV_NOPTS_VALUE
                             ? 0.0
                             : stream->start_time * rational(stream->time_base);
    return true;
  }

  bool configureDecoder(bool useHardware) {
    const AVCodecParameters* parameters = stream->codecpar;
    const AVCodec* codec = metadata.hasAlpha
                               ? alphaCodec(parameters->codec_id)
                               : avcodec_find_decoder(parameters->codec_id);
    if (!codec) {
      error = "video decoder is unavailable";
      return false;
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) return false;
    int result = avcodec_parameters_to_context(codecContext, parameters);
    if (result < 0) {
      error = ffmpegError(result);
      return false;
    }

    if (useHardware) {
#ifdef __APPLE__
      const AVCodecHWConfig* configuration = nullptr;
      for (int i = 0; (configuration = avcodec_get_hw_config(codec, i)); ++i) {
        if (configuration->device_type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX &&
            (configuration->methods &
             AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
          hardwarePixelFormat = configuration->pix_fmt;
          break;
        }
      }
      if (!configuration) {
        error = "the codec has no VideoToolbox device configuration";
        return false;
      }
      result = av_hwdevice_ctx_create(
          &hardwareDevice, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
      if (result < 0) {
        error = ffmpegError(result);
        return false;
      }
      codecContext->hw_device_ctx = av_buffer_ref(hardwareDevice);
      codecContext->opaque = this;
      codecContext->get_format = chooseFormat;
#else
      error = "no platform video decoder is available";
      return false;
#endif
    }

    result = avcodec_open2(codecContext, codec, nullptr);
    if (result < 0) {
      error = ffmpegError(result);
      return false;
    }
    hardwareActive = useHardware;
    packet = av_packet_alloc();
    receiveFrame = av_frame_alloc();
    return packet && receiveFrame;
  }

  void resetDecoder() {
    av_frame_free(&receiveFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    av_buffer_unref(&hardwareDevice);
    hardwarePixelFormat = AV_PIX_FMT_NONE;
    hardwareActive = false;
  }

  bool open() {
    if (!openContainer()) return false;
    if (metadata.hasAlpha) {
      if (options.hardware == HardwarePreference::Required) {
        error = "the native video decoder cannot preserve an alpha plane";
        return false;
      }
      return configureDecoder(false);
    }
    const bool wantsHardware = options.hardware != HardwarePreference::Disabled;
    if (wantsHardware && configureDecoder(true)) return true;
    if (options.hardware == HardwarePreference::Required) return false;
    resetDecoder();
    error.clear();
    return configureDecoder(false);
  }

  bool seekTo(double seconds) {
    const double absolute = seconds + streamStartSeconds;
    const int64_t timestamp =
        static_cast<int64_t>(absolute / rational(stream->time_base));
    const int result = av_seek_frame(formatContext, videoStream, timestamp,
                                     AVSEEK_FLAG_BACKWARD);
    if (result < 0) {
      error = ffmpegError(result);
      return false;
    }
    avcodec_flush_buffers(codecContext);
    sentDrain = false;
    cursorSeconds = -std::numeric_limits<double>::infinity();
    return true;
  }

  CachedFrame* decodeNext() {
    while (true) {
      int result = avcodec_receive_frame(codecContext, receiveFrame);
      if (result >= 0) {
        const int64_t timestamp = receiveFrame->best_effort_timestamp;
        double presentation =
            timestamp == AV_NOPTS_VALUE
                ? (metadata.frameRate > 0 ? decodedSequence / metadata.frameRate
                                          : cursorSeconds)
                : timestamp * rational(stream->time_base) - streamStartSeconds;
        presentation = std::max(0.0, presentation);
        double duration =
            receiveFrame->duration > 0
                ? receiveFrame->duration * rational(stream->time_base)
                : (metadata.frameRate > 0 ? 1.0 / metadata.frameRate
                                          : 1.0 / 30.0);
        CachedFrame cached;
        cached.decoded = cloneFrame(receiveFrame);
        metadata.hasAlpha |= pixelFormatHasAlpha(
            static_cast<AVPixelFormat>(receiveFrame->format));
        cached.presentationSeconds = presentation;
        cached.durationSeconds = duration;
        cached.index = metadata.frameRate > 0
                           ? static_cast<int64_t>(std::llround(
                                 presentation * metadata.frameRate))
                           : decodedSequence;
        cached.hardwareDecoded =
            receiveFrame->format == hardwarePixelFormat && hardwareActive;
        if (options.hardware == HardwarePreference::Required &&
            !cached.hardwareDecoded) {
          error = "the decoder did not produce a native hardware frame";
          av_frame_unref(receiveFrame);
          return nullptr;
        }
        if (cached.hardwareDecoded)
          cached.native = device::retainNativeFrame(receiveFrame);
        av_frame_unref(receiveFrame);
        cursorSeconds = presentation;
        ++decodedSequence;
        cache.push_back(std::move(cached));
        while (cache.size() > options.cachedFrames) cache.pop_front();
        return &cache.back();
      }
      if (result == AVERROR_EOF) return nullptr;
      if (result != AVERROR(EAGAIN)) {
        error = ffmpegError(result);
        return nullptr;
      }

      bool submitted = false;
      while (!submitted) {
        result = av_read_frame(formatContext, packet);
        if (result < 0) {
          if (!sentDrain) {
            result = avcodec_send_packet(codecContext, nullptr);
            sentDrain = true;
            if (result < 0 && result != AVERROR_EOF)
              error = ffmpegError(result);
          }
          submitted = true;
          break;
        }
        if (packet->stream_index == videoStream) {
          result = avcodec_send_packet(codecContext, packet);
          av_packet_unref(packet);
          if (result < 0 && result != AVERROR(EAGAIN)) {
            error = ffmpegError(result);
            return nullptr;
          }
          submitted = true;
        } else {
          av_packet_unref(packet);
        }
      }
    }
  }

  CachedFrame* cachedAt(double seconds) {
    for (auto found = cache.begin(); found != cache.end(); ++found) {
      if (found->presentationSeconds <= seconds &&
          seconds < found->presentationSeconds + found->durationSeconds) {
        if (std::next(found) != cache.end()) {
          CachedFrame promoted = std::move(*found);
          cache.erase(found);
          cache.push_back(std::move(promoted));
        }
        return &cache.back();
      }
    }
    return nullptr;
  }

  sk_sp<SkImage> rasterize(const SharedFrame& decoded) {
    if (!decoded) return nullptr;
    const AVFrame* source = decoded.get();
    AVFrame* transferred = nullptr;
    if (source->format == hardwarePixelFormat && hardwareActive) {
      transferred = av_frame_alloc();
      if (!transferred ||
          av_hwframe_transfer_data(transferred, source, 0) < 0) {
        av_frame_free(&transferred);
        return nullptr;
      }
      source = transferred;
    }

    sws = sws_getCachedContext(sws, source->width, source->height,
                               static_cast<AVPixelFormat>(source->format),
                               source->width, source->height, AV_PIX_FMT_RGBA,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
      av_frame_free(&transferred);
      return nullptr;
    }
    // The stream's own matrix and range drive the YUV→RGB conversion:
    // libswscale's default is BT.601 limited range, which mis-colours an
    // HD clip and crushes a full-range one. The details belong to one
    // context, and the cached context is remade whenever the source's
    // size or format changes, so they are re-applied whenever any of
    // those or the tags differ from the last frame; a source that is not
    // YUV ignores them.
    const SwsSource description{source->width, source->height, source->format,
                                swscaleColorspace(source->colorspace),
                                source->color_range == AVCOL_RANGE_JPEG};
    if (description != swsSource) {
      sws_setColorspaceDetails(sws, sws_getCoefficients(description.colorspace),
                               description.fullRange ? 1 : 0,
                               sws_getCoefficients(SWS_CS_DEFAULT), 1, 0,
                               1 << 16, 1 << 16);
      swsSource = description;
    }

    const int width = source->width;
    const int height = source->height;
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<uint8_t> pixels(rowBytes * height);
    uint8_t* destinations[] = {pixels.data(), nullptr, nullptr, nullptr};
    int destinationStrides[] = {static_cast<int>(rowBytes), 0, 0, 0};
    const int rows = sws_scale(sws, source->data, source->linesize, 0, height,
                               destinations, destinationStrides);
    const bool hasAlpha =
        metadata.hasAlpha ||
        pixelFormatHasAlpha(static_cast<AVPixelFormat>(source->format));
    const AVAlphaMode alphaMode = source->alpha_mode;
    av_frame_free(&transferred);
    if (rows != height) return nullptr;

    if (hasAlpha && alphaMode != AVALPHA_MODE_PREMULTIPLIED) {
      for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        const unsigned alpha = pixels[offset + 3];
        pixels[offset + 0] = static_cast<uint8_t>(
            (static_cast<unsigned>(pixels[offset + 0]) * alpha + 127) / 255);
        pixels[offset + 1] = static_cast<uint8_t>(
            (static_cast<unsigned>(pixels[offset + 1]) * alpha + 127) / 255);
        pixels[offset + 2] = static_cast<uint8_t>(
            (static_cast<unsigned>(pixels[offset + 2]) * alpha + 127) / 255);
      }
    }

    sk_sp<SkData> data = SkData::MakeWithCopy(pixels.data(), pixels.size());
    const SkImageInfo info =
        SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                          hasAlpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
                          SkColorSpace::MakeSRGB());
    return SkImages::RasterFromData(info, std::move(data), rowBytes);
  }

  VideoFrame materialize(CachedFrame& cached,
                         skgpu::graphite::Recorder* recorder,
                         bool decodeOnly = false) {
    sk_sp<SkImage> image;
    if (cached.hardwareDecoded && recorder) {
      if (!deviceContext)
        deviceContext = device::makeContext(options.metalDevice);
      if (!cached.deviceImage || cached.deviceRecorder != recorder) {
        cached.deviceImage = deviceContext
                                 ? device::wrapNativeFrame(
                                       cached.native, recorder, *deviceContext)
                                 : nullptr;
        cached.deviceRecorder = recorder;
      }
      image = cached.deviceImage;
    }
    if (!image && (!decodeOnly || !cached.hardwareDecoded)) {
      if (!cached.raster) cached.raster = rasterize(cached.decoded);
      image = cached.raster;
    }
    return {
        .image = std::move(image),
        .native = cached.native,
        .presentationSeconds = cached.presentationSeconds,
        .durationSeconds = cached.durationSeconds,
        .index = cached.index,
        .hardwareDecoded = cached.hardwareDecoded,
        .hasAlpha = metadata.hasAlpha,
    };
  }

  VideoFrame frameAt(double seconds, skgpu::graphite::Recorder* recorder) {
    return frameAt(seconds, recorder, false);
  }

  VideoFrame frameAt(double seconds, skgpu::graphite::Recorder* recorder,
                     bool decodeOnly) {
    seconds = std::max(0.0, seconds);
    if (CachedFrame* found = cachedAt(seconds))
      return materialize(*found, recorder, decodeOnly);

    const bool unopened = !std::isfinite(cursorSeconds);
    if (unopened || seconds + 0.001 < cursorSeconds ||
        seconds > cursorSeconds + 2.0) {
      if (!seekTo(unopened && metadata.hasAlpha ? 0.0 : seconds)) return {};
    }

    // The newest decoded frame at or before the asked time. When frame
    // durations leave a gap before the next frame, it is the answer; it
    // is materialized in the cache, never on this copy, so its raster or
    // device wrap is there for the next ask.
    std::optional<CachedFrame> preceding;
    const auto materializePreceding = [&]() {
      CachedFrame* cached = cachedAt(preceding->presentationSeconds);
      if (!cached) {
        cache.push_back(std::move(*preceding));
        while (cache.size() > options.cachedFrames) cache.pop_front();
        cached = &cache.back();
      }
      return materialize(*cached, recorder, decodeOnly);
    };
    for (int decoded = 0; decoded < 10000; ++decoded) {
      CachedFrame* current = decodeNext();
      if (!current) break;
      if (current->presentationSeconds <= seconds) preceding = *current;
      if (seconds < current->presentationSeconds + current->durationSeconds) {
        if (current->presentationSeconds <= seconds || !preceding)
          return materialize(*current, recorder, decodeOnly);
        return materializePreceding();
      }
      if (current->presentationSeconds > seconds)
        return preceding ? materializePreceding()
                         : materialize(*current, recorder, decodeOnly);
    }

    if (preceding) return materializePreceding();
    CachedFrame* last = nullptr;
    for (CachedFrame& frame : cache)
      if (!last || frame.presentationSeconds > last->presentationSeconds)
        last = &frame;
    return last ? materialize(*last, recorder, decodeOnly) : VideoFrame{};
  }

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
  bool sentDrain = false;
};

Video::Video(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Video::~Video() = default;

const VideoProbe& Video::probe() const { return m_impl->metadata; }

bool Video::hardwareAccelerated() const { return m_impl->hardwareActive; }

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
