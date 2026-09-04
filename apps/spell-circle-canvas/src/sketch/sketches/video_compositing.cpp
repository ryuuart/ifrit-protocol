/** @file
 * video_compositing — the direct SigilVideo compositing feature plate: five
 * independently clocked network videos reused as six overlapping surfaces.
 *
 * SigilIO fetches and caches every encoded document. SigilVideo keeps the
 * long clips streaming, lets device-decodable sky frames stay native until
 * the destination recorder is known, and preserves the WebM alpha plane in
 * the foreground clip. The black-backed dust and colour burst use
 * plus blending: black contributes nothing while their light adds into the
 * scene.
 * Each source is sampled once per paint and its frame feeds every surface
 * that shows it, so decode and native-plane wrapping scale with sources rather
 * than draw count.
 *
 * The sky studies are Clouds Time Lapse by madlag and Night Sky Timelapse by
 * the US National Park Service. Dust Particles 2 and Color Explosion Short
 * are by VFX FOOTAGE and licensed CC BY 3.0. The alpha dancer is from Sam
 * Dutton's Apache-licensed simpl demo.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilio/IO.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sketch = sigil::sketch;
namespace image = sigil::image;
namespace io = sigil::io;
namespace video = sigil::video;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kWidth = 1080.0f;
constexpr float kHeight = 1920.0f;

constexpr std::string_view kDaySky =
    "https://upload.wikimedia.org/wikipedia/commons/5/54/"
    "Clouds_Time_Lapse.webm";
constexpr std::string_view kNightSky =
    "https://upload.wikimedia.org/wikipedia/commons/transcoded/e/e6/"
    "Night_Sky_Timelapse_%2830549248476%29.webm/"
    "Night_Sky_Timelapse_%2830549248476%29.webm.480p.vp9.webm";
constexpr std::string_view kDust =
    "https://commons.wikimedia.org/wiki/Special:Redirect/file/"
    "Dust_Particles_2_--FREE_FOOTAGE--.webm";
constexpr std::string_view kColorBurst =
    "https://upload.wikimedia.org/wikipedia/commons/transcoded/a/a9/"
    "Color_Explosion_short_--FREE_FOOTAGE--.webm/"
    "Color_Explosion_short_--FREE_FOOTAGE--.webm.480p.vp9.webm";
constexpr std::string_view kAlphaVideo =
    "https://raw.githubusercontent.com/samdutton/simpl/"
    "2c9682d109541f6d8407fa8cdcc4d18735d0b9c5/videoalpha/video/"
    "dancer1.webm";

std::shared_ptr<video::Video> loadVideo(io::Hub& hub, std::string_view uri) {
  const std::shared_ptr<const io::Bytes> encoded = hub.blob(uri);
  if (!encoded || encoded->bytes.empty()) return nullptr;
  video::DecodeOptions options;
  options.cachedFrames = 8;
  return video::decodeVideo(encoded->bytes.data(), encoded->bytes.size(),
                            options, std::filesystem::path(uri));
}

double loopTime(const video::Video& clip, double seconds) {
  const double duration = clip.probe().durationSeconds;
  if (duration <= 0.0) return std::max(0.0, seconds);
  double result = std::fmod(seconds, duration);
  return result < 0.0 ? result + duration : result;
}

SkRect coverSource(const SkImage& source, const SkRect& destination) {
  const float sourceAspect =
      static_cast<float>(source.width()) / static_cast<float>(source.height());
  const float destinationAspect = destination.width() / destination.height();
  if (sourceAspect > destinationAspect) {
    const float width = source.height() * destinationAspect;
    return SkRect::MakeXYWH((source.width() - width) * 0.5f, 0.0f, width,
                            static_cast<float>(source.height()));
  }
  const float height = source.width() / destinationAspect;
  return SkRect::MakeXYWH(0.0f, (source.height() - height) * 0.5f,
                          static_cast<float>(source.width()), height);
}

struct Source {
  std::shared_ptr<video::Video> clip;
  video::Playback::Handle handle = 0;
};

video::VideoFrame sampleVideo(const Source& source, video::Playback* playback,
                              double seconds,
                              skgpu::graphite::Recorder* recorder) {
  if (!source.clip) return {};
  const double time = loopTime(*source.clip, seconds);
  if (!playback) return source.clip->frameAt(time, recorder);
  playback->request(source.handle, time);
  return playback->frame(source.handle, recorder);
}

void drawFrame(SkCanvas& canvas, const video::VideoFrame& frame,
               const SkRect& destination, float opacity, SkBlendMode blend,
               bool cover = true) {
  if (!frame.image) return;
  SkPaint paint;
  paint.setAlphaf(std::clamp(opacity, 0.0f, 1.0f));
  paint.setBlendMode(blend);
  const SkRect source =
      cover ? coverSource(*frame.image, destination)
            : SkRect::MakeWH(frame.image->width(), frame.image->height());
  canvas.drawImageRect(frame.image, source, destination,
                       SkSamplingOptions(SkFilterMode::kLinear), &paint,
                       SkCanvas::kStrict_SrcRectConstraint);
}

struct Clips {
  std::shared_ptr<video::Playback> playback;
  Source day;
  Source night;
  Source dust;
  Source colorBurst;
  Source alpha;
};

}  // namespace

struct VideoCompositing final : sketch::Sketch {
  static bool available(std::string* why) {
    return sketch::requireCached(
        {kDaySky, kNightSky, kDust, kColorBurst, kAlphaVideo}, why);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kWidth, kHeight);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(4.25);

    io::Hub& hub = ctx.assets.hub();
    std::shared_ptr<video::Playback> playback =
        ctx.deterministic ? nullptr : std::make_shared<video::Playback>();
    const auto source = [&hub, &playback](std::string_view uri) {
      Source result{.clip = loadVideo(hub, uri)};
      if (playback) result.handle = playback->add(result.clip);
      return result;
    };
    Source day = source(kDaySky);
    Source night = source(kNightSky);
    Source dust = source(kDust);
    Source colorBurst = source(kColorBurst);
    Source alpha = source(kAlphaVideo);
    const Clips clips{.playback = std::move(playback),
                      .day = std::move(day),
                      .night = std::move(night),
                      .dust = std::move(dust),
                      .colorBurst = std::move(colorBurst),
                      .alpha = std::move(alpha)};

    Element stage =
        custom(
            "video.layers",
            [clips](SkCanvas& canvas, const PaintContext& paint) {
              const SkRect page =
                  SkRect::MakeWH(paint.size.width(), paint.size.height());
              const double seconds = paint.elapsedSeconds;
              const float night =
                  0.5f - 0.5f * std::cos(static_cast<float>(seconds * 0.24));
              skgpu::graphite::Recorder* recorder = canvas.recorder();
              video::Playback* playback = clips.playback.get();
              const video::VideoFrame day =
                  sampleVideo(clips.day, playback, seconds, recorder);
              const video::VideoFrame nightSky = sampleVideo(
                  clips.night, playback, seconds * 0.72 + 1.4, recorder);
              const video::VideoFrame dust = sampleVideo(
                  clips.dust, playback, seconds * 0.91 + 0.7, recorder);
              const video::VideoFrame colorBurst = sampleVideo(
                  clips.colorBurst, playback, seconds * 0.78 + 2.1, recorder);
              const video::VideoFrame dancer =
                  sampleVideo(clips.alpha, playback, seconds + 0.35, recorder);

              drawFrame(canvas, day, page, 1.0f, SkBlendMode::kSrc);

              drawFrame(canvas, dust, SkRect::MakeXYWH(80, 315, 500, 360), 1.0f,
                        SkBlendMode::kPlus);
              drawFrame(canvas, nightSky, SkRect::MakeXYWH(330, 520, 540, 390),
                        0.68f + night * 0.22f, SkBlendMode::kPlus);
              drawFrame(canvas, colorBurst, SkRect::MakeXYWH(90, 805, 560, 315),
                        0.88f, SkBlendMode::kPlus);
              drawFrame(canvas, dancer, SkRect::MakeXYWH(125, 1190, 450, 338),
                        1.0f, SkBlendMode::kSrcOver, false);
              drawFrame(canvas, dancer, SkRect::MakeXYWH(650, 1395, 300, 225),
                        0.88f, SkBlendMode::kPlus, false);
            })
            .absolute()
            .inset(0)
            .cache(Cache::None);

    const weave::TextStyle title = weave::textStyle(
        {.size = 34, .color = {1, 1, 1, 0.96f}, .track = 8.0f});
    ctx.composer.render(stack()
                            .width(kWidth)
                            .height(kHeight)
                            .child(std::move(stage))
                            .child(text(u8"SKY / SIGNAL", title)
                                       .absolute()
                                       .inset(72, 88, 72, 1720)));
  }
};

SIGIL_SKETCH(VideoCompositing, "Media",
             "A direct-video feature plate for cached network footage, "
             "additive black-backed VFX, frame reuse, and native alpha.")
