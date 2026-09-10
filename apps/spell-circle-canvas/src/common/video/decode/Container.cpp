/** @file
 * The container half of a decoder: the encoded bytes as an FFmpeg stream,
 * the best video stream and its metadata, the codec configuration — native
 * first where a platform decoder is wanted — and seeking.
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "DecodeInternal.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace sigil::video {
namespace {

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

const AVCodec* alphaCodec(AVCodecID id) {
  if (id == AV_CODEC_ID_VP8) return avcodec_find_decoder_by_name("libvpx");
  if (id == AV_CODEC_ID_VP9) return avcodec_find_decoder_by_name("libvpx-vp9");
  return avcodec_find_decoder(id);
}

}  // namespace

Video::Impl::Impl(const std::byte* source, size_t size, DecodeOptions requested,
                  std::filesystem::path hint)
    : options(requested), pathHint(std::move(hint)) {
  options.cachedFrames = std::max<size_t>(1, options.cachedFrames);
  encoded.resize(size);
  std::memcpy(encoded.data(), source, size);
}

Video::Impl::~Impl() {
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

int Video::Impl::read(void* opaque, uint8_t* destination, int requested) {
  auto* self = static_cast<Impl*>(opaque);
  const size_t remaining = self->encoded.size() - self->readPosition;
  if (remaining == 0) return AVERROR_EOF;
  const size_t count = std::min(remaining, static_cast<size_t>(requested));
  std::memcpy(destination, self->encoded.data() + self->readPosition, count);
  self->readPosition += count;
  return static_cast<int>(count);
}

int64_t Video::Impl::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<Impl*>(opaque);
  if (whence == AVSEEK_SIZE) return static_cast<int64_t>(self->encoded.size());
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

AVPixelFormat Video::Impl::chooseFormat(AVCodecContext* context,
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

bool Video::Impl::openContainer() {
  constexpr int kIoBufferSize = 32 * 1024;
  uint8_t* ioBuffer = static_cast<uint8_t*>(av_malloc(kIoBufferSize));
  if (!ioBuffer) return false;
  avioContext =
      avio_alloc_context(ioBuffer, kIoBufferSize, 0, this, read, nullptr, seek);
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
      &formatContext, name.empty() ? nullptr : name.c_str(), nullptr, nullptr);
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
  timestampless = formatContext->iformat &&
                  (formatContext->iformat->flags & AVFMT_NOTIMESTAMPS) != 0;
  streamStartSeconds = stream->start_time == AV_NOPTS_VALUE
                           ? 0.0
                           : stream->start_time * rational(stream->time_base);
  return true;
}

bool Video::Impl::configureDecoder(bool useHardware) {
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
          (configuration->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
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

void Video::Impl::resetDecoder() {
  av_frame_free(&receiveFrame);
  av_packet_free(&packet);
  avcodec_free_context(&codecContext);
  av_buffer_unref(&hardwareDevice);
  hardwarePixelFormat = AV_PIX_FMT_NONE;
  hardwareActive = false;
  decodedNative = false;
}

bool Video::Impl::open() {
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

bool Video::Impl::seekTo(double seconds) {
  // Frames that carry no presentation timestamp are placed by decode
  // order, and no time inside such a stream can be sought to. Its one
  // determinate position is the first byte, from which the order is
  // counted again.
  int result = 0;
  if (timestampless) {
    result = av_seek_frame(formatContext, videoStream, 0, AVSEEK_FLAG_BYTE);
  } else {
    const double absolute = seconds + streamStartSeconds;
    const int64_t timestamp =
        static_cast<int64_t>(absolute / rational(stream->time_base));
    result = av_seek_frame(formatContext, videoStream, timestamp,
                           AVSEEK_FLAG_BACKWARD);
  }
  if (result < 0) {
    error = ffmpegError(result);
    return false;
  }
  avcodec_flush_buffers(codecContext);
  sentDrain = false;
  cursorSeconds = -std::numeric_limits<double>::infinity();
  if (timestampless) decodedSequence = 0;
  return true;
}

}  // namespace sigil::video
