#include "sigilvideo/encode/Encode.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSize.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace sigil::video {
namespace {

std::string ffmpegError(int code) {
  char text[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(code, text, sizeof(text));
  return text;
}

void releaseAvBuffer(const void*, void* context) { av_free(context); }

struct FrameDeleter {
  void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

bool nameContains(const AVCodec* codec, const char* word) {
  return codec && codec->name && std::strstr(codec->name, word);
}

AVPixelFormat choosePixelFormat(const AVCodec* codec) {
  const void* supported = nullptr;
  int count = 0;
  if (avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT,
                                   0, &supported, &count) < 0 ||
      !supported)
    return AV_PIX_FMT_YUV420P;
  const auto* formats = static_cast<const AVPixelFormat*>(supported);
  for (int i = 0; i < count; ++i)
    if (formats[i] == AV_PIX_FMT_YUV420P) return AV_PIX_FMT_YUV420P;
  for (int i = 0; i < count; ++i)
    if (formats[i] == AV_PIX_FMT_NV12) return AV_PIX_FMT_NV12;
  return count > 0 ? formats[0] : AV_PIX_FMT_YUV420P;
}

}  // namespace

struct Encoder::Impl {
  Impl(Format requestedFormat, EncodeOptions requested)
      : format(requestedFormat), options(requested) {}

  ~Impl() {
    sws_freeContext(sws);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    if (dynamicBufferOpen && formatContext && formatContext->pb) {
      uint8_t* abandoned = nullptr;
      const int size = avio_close_dyn_buf(formatContext->pb, &abandoned);
      if (size >= 0) av_free(abandoned);
      formatContext->pb = nullptr;
    }
    avformat_free_context(formatContext);
  }

  bool fail(std::string message) {
    lastError = std::move(message);
    return false;
  }

  bool fail(int code) { return fail(ffmpegError(code)); }

  bool open() {
    if (format != Format::Mp4) return fail("unsupported video format");
    if (options.width <= 0 || options.height <= 0 || (options.width & 1) ||
        (options.height & 1))
      return fail("video dimensions must be positive and even");
    if (options.framesPerSecond <= 0 || options.bitRate <= 0)
      return fail("frame rate and bit rate must be positive");

    int result =
        avformat_alloc_output_context2(&formatContext, nullptr, "mp4", nullptr);
    if (result < 0 || !formatContext) return fail(result);

    const AVCodec* encoder = nullptr;
#ifdef __APPLE__
    if (options.hardware != HardwarePreference::Disabled)
      encoder = avcodec_find_encoder_by_name("h264_videotoolbox");
#endif
    if (!encoder && options.hardware == HardwarePreference::Required)
      return fail("the platform H.264 encoder is unavailable");
    if (!encoder) encoder = avcodec_find_encoder_by_name("libopenh264");
    if (!encoder) return fail("an H.264 encoder is unavailable");

    stream = avformat_new_stream(formatContext, nullptr);
    if (!stream) return fail("could not create the MP4 video stream");

    const auto openCodec = [&](const AVCodec* selected) {
      avcodec_free_context(&codecContext);
      hardwareEncoder = nameContains(selected, "videotoolbox");
      codecName = selected && selected->name ? selected->name : "h264";
      codecContext = avcodec_alloc_context3(selected);
      if (!codecContext) return AVERROR(ENOMEM);
      codecContext->codec_id = selected->id;
      codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
      codecContext->width = options.width;
      codecContext->height = options.height;
      codecContext->time_base = AVRational{1, options.framesPerSecond};
      codecContext->framerate = AVRational{options.framesPerSecond, 1};
      codecContext->bit_rate = options.bitRate;
      codecContext->gop_size = options.framesPerSecond * 2;
      codecContext->max_b_frames = hardwareEncoder ? 0 : 2;
      codecContext->pix_fmt = choosePixelFormat(selected);
      codecContext->color_primaries = AVCOL_PRI_BT709;
      codecContext->color_trc = AVCOL_TRC_BT709;
      codecContext->colorspace = AVCOL_SPC_BT709;
      codecContext->color_range = AVCOL_RANGE_MPEG;
      if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

      AVDictionary* codecOptions = nullptr;
      if (hardwareEncoder) {
        av_dict_set(&codecOptions, "profile", "high", 0);
        av_dict_set(&codecOptions, "realtime", "0", 0);
      }
      const int opened = avcodec_open2(codecContext, selected, &codecOptions);
      av_dict_free(&codecOptions);
      return opened;
    };

    result = openCodec(encoder);
    if (result < 0 && hardwareEncoder &&
        options.hardware == HardwarePreference::Preferred) {
      const AVCodec* software = avcodec_find_encoder_by_name("libopenh264");
      if (software) result = openCodec(software);
    }
    if (result < 0) return fail(result);

    result = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (result < 0) return fail(result);
    stream->time_base = codecContext->time_base;

    result = avio_open_dyn_buf(&formatContext->pb);
    if (result < 0) return fail(result);
    dynamicBufferOpen = true;

    result = avformat_write_header(formatContext, nullptr);
    if (result < 0) return fail(result);

    packet = av_packet_alloc();
    return packet ? true : fail("could not allocate an encoder packet");
  }

  bool writePackets() {
    while (true) {
      const int result = avcodec_receive_packet(codecContext, packet);
      if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return true;
      if (result < 0) return fail(result);
      if (packet->duration <= 0) packet->duration = 1;
      av_packet_rescale_ts(packet, codecContext->time_base, stream->time_base);
      packet->stream_index = stream->index;
      const int written = av_interleaved_write_frame(formatContext, packet);
      av_packet_unref(packet);
      if (written < 0) return fail(written);
    }
  }

  bool send(const AVFrame* source) {
    const int result = avcodec_send_frame(codecContext, source);
    if (result < 0) return fail(result);
    return writePackets();
  }

  bool append(const SkPixmap& pixels) {
    if (finished) return fail("the video encoder is already finished");
    if (!pixels.addr() || pixels.width() <= 0 || pixels.height() <= 0)
      return fail("the input frame has no pixels");

    const SkImageInfo bgraInfo = SkImageInfo::Make(
        pixels.width(), pixels.height(), kBGRA_8888_SkColorType,
        kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());
    const size_t bgraRowBytes = bgraInfo.minRowBytes();
    bgra.resize(bgraRowBytes * bgraInfo.height());
    const SkPixmap bgraPixels(bgraInfo, bgra.data(), bgraRowBytes);
    if (!pixels.readPixels(bgraPixels))
      return fail("the input frame cannot convert to BGRA");

    std::unique_ptr<AVFrame, FrameDeleter> next(av_frame_alloc());
    if (!next) return fail("could not allocate an encoder frame");
    next->format = codecContext->pix_fmt;
    next->width = codecContext->width;
    next->height = codecContext->height;
    const int allocated = av_frame_get_buffer(next.get(), 32);
    if (allocated < 0) return fail(allocated);
    // The frame carries the tags the stream is declared with, and the
    // conversion below uses that same matrix and range, so what a decoder
    // reads off the stream is what the samples were made with.
    next->color_primaries = codecContext->color_primaries;
    next->color_trc = codecContext->color_trc;
    next->colorspace = codecContext->colorspace;
    next->color_range = codecContext->color_range;
    sws = sws_getCachedContext(sws, bgraInfo.width(), bgraInfo.height(),
                               AV_PIX_FMT_BGRA, options.width, options.height,
                               codecContext->pix_fmt, SWS_BICUBIC, nullptr,
                               nullptr, nullptr);
    if (!sws) return fail("the video pixel converter is unavailable");
    // The cached context is remade when the input size changes and takes
    // libswscale's BT.601 default with it, so the BT.709 limited-range
    // matrix the stream is tagged with is set whenever the input differs
    // from the last frame's.
    if (bgraInfo.dimensions() != swsInput) {
      sws_setColorspaceDetails(sws, sws_getCoefficients(SWS_CS_DEFAULT), 1,
                               sws_getCoefficients(SWS_CS_ITU709), 0, 0,
                               1 << 16, 1 << 16);
      swsInput = bgraInfo.dimensions();
    }
    const uint8_t* sources[] = {bgra.data(), nullptr, nullptr, nullptr};
    const int sourceStrides[] = {static_cast<int>(bgraRowBytes), 0, 0, 0};
    const int rows = sws_scale(sws, sources, sourceStrides, 0,
                               bgraInfo.height(), next->data, next->linesize);
    if (rows != options.height)
      return fail("the input frame conversion failed");
    next->pts = framesWritten;
    next->duration = 1;
    if (!send(next.get())) return false;
    ++framesWritten;
    return true;
  }

  sk_sp<SkData> finish() {
    if (finished) {
      fail("the video encoder is already finished");
      return nullptr;
    }
    finished = true;
    if (framesWritten == 0) {
      fail("a video needs at least one frame");
      return nullptr;
    }
    if (!send(nullptr)) return nullptr;
    const int trailer = av_write_trailer(formatContext);
    if (trailer < 0) {
      fail(trailer);
      return nullptr;
    }

    uint8_t* bytes = nullptr;
    const int size = avio_close_dyn_buf(formatContext->pb, &bytes);
    formatContext->pb = nullptr;
    dynamicBufferOpen = false;
    if (size <= 0 || !bytes) {
      av_free(bytes);
      fail("the MP4 muxer produced no bytes");
      return nullptr;
    }
    return SkData::MakeWithProc(bytes, static_cast<size_t>(size),
                                releaseAvBuffer, bytes);
  }

  Format format;
  EncodeOptions options;
  AVFormatContext* formatContext = nullptr;
  AVCodecContext* codecContext = nullptr;
  AVStream* stream = nullptr;
  AVPacket* packet = nullptr;
  SwsContext* sws = nullptr;
  SkISize swsInput = SkISize::MakeEmpty();
  std::vector<uint8_t> bgra;
  std::string lastError;
  std::string codecName;
  int64_t framesWritten = 0;
  bool hardwareEncoder = false;
  bool dynamicBufferOpen = false;
  bool finished = false;
};

Encoder::Encoder(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Encoder::~Encoder() = default;

std::unique_ptr<Encoder> Encoder::make(Format format,
                                       const EncodeOptions& options) {
  auto impl = std::make_unique<Impl>(format, options);
  if (!impl->open()) return nullptr;
  return std::unique_ptr<Encoder>(new Encoder(std::move(impl)));
}

bool Encoder::append(const SkPixmap& pixels) { return m_impl->append(pixels); }

bool Encoder::append(const SkImage& image) {
  const SkImageInfo info =
      SkImageInfo::Make(image.width(), image.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());
  std::vector<uint8_t> pixels(info.computeMinByteSize());
  const SkPixmap pixmap(info, pixels.data(), info.minRowBytes());
  if (!image.readPixels(nullptr, pixmap, 0, 0))
    return m_impl->fail("the input image pixels are not readable");
  return append(pixmap);
}

sk_sp<SkData> Encoder::finish() { return m_impl->finish(); }

const std::string& Encoder::error() const { return m_impl->lastError; }
const std::string& Encoder::codec() const { return m_impl->codecName; }
int64_t Encoder::frameCount() const { return m_impl->framesWritten; }

std::optional<Format> formatForPath(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension == ".mp4" || extension == ".m4v") return Format::Mp4;
  return std::nullopt;
}

const char* extensionFor(Format format) {
  switch (format) {
    case Format::Mp4:
      return ".mp4";
  }
  return ".mp4";
}

}  // namespace sigil::video
