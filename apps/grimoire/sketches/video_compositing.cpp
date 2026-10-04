/** @file
 * video_compositing — the direct SigilMediaVideo compositing feature plate:
 * five independently clocked network videos reused as six overlapping surfaces.
 *
 * SigilIO fetches and caches every encoded document. SigilMediaVideo keeps the
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

// TAGS: Media/Video

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/video/Video.h>
#include <sigilmotion/time/Duration.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace media = sigil::media;
namespace io = sigil::io;
namespace media = sigil::media;
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

media::Frame sampleVideo(const std::shared_ptr<const media::Video>& clip,
                         double seconds) {
  return clip ? clip->frameAt(std::chrono::duration<double>(seconds))
              : media::Frame{};
}

void drawFrame(SkCanvas& canvas, const media::Frame& frame,
               const SkRect& destination, float opacity, SkBlendMode blend,
               bool cover = true) {
  const auto image = media::deviceImage(frame, canvas.recorder());
  if (!image) return;
  SkPaint paint;
  paint.setAlphaf(std::clamp(opacity, 0.0f, 1.0f));
  paint.setBlendMode(blend);
  const SkRect source = cover ? coverSource(*image, destination)
                              : SkRect::MakeWH(image->width(), image->height());
  canvas.drawImageRect(image, source, destination,
                       SkSamplingOptions(SkFilterMode::kLinear), &paint,
                       SkCanvas::kStrict_SrcRectConstraint);
}

struct Clips {
  std::shared_ptr<media::Playback> playback;
  std::shared_ptr<const media::Video> day;
  std::shared_ptr<const media::Video> night;
  std::shared_ptr<const media::Video> dust;
  std::shared_ptr<const media::Video> colorBurst;
  std::shared_ptr<const media::Video> alpha;
};

}  // namespace

struct VideoCompositing {
  static bool available(std::string* why) {
    return sketch::requireCached(
        {kDaySky, kNightSky, kDust, kColorBurst, kAlphaVideo}, why);
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(ctx, {.size = SkSize::Make(kWidth, kHeight),
                             .captureAt = 4.25,
                             .background = material::Color{0, 0, 0, 1}});

    io::Hub& hub = ctx.assets.hub();
    std::shared_ptr<media::Playback> playback =
        ctx.deterministic ? nullptr : std::make_shared<media::Playback>();
    const auto source = [&hub, &playback](std::string_view uri) {
      return hub.load<media::Video>(uri,
                                    {.playback = playback, .cachedFrames = 8});
    };
    Clips clips{.day = source(kDaySky),
                .night = source(kNightSky),
                .dust = source(kDust),
                .colorBurst = source(kColorBurst),
                .alpha = source(kAlphaVideo)};
    clips.playback = std::move(playback);

    Element stage =
        custom(
            "video.layers",
            [clips](sigil::draw::Pen& pen, const PaintContext& paint) {
              SkCanvas& canvas = *pen.canvas();
              const SkRect page = SkRect::MakeWH(paint.size.x, paint.size.y);
              const double seconds = paint.elapsedSeconds;
              const float night =
                  0.5f - 0.5f * std::cos(static_cast<float>(seconds * 0.24));
              const media::Frame day = sampleVideo(clips.day, seconds);
              const media::Frame nightSky =
                  sampleVideo(clips.night, seconds * 0.72 + 1.4);
              const media::Frame dust =
                  sampleVideo(clips.dust, seconds * 0.91 + 0.7);
              const media::Frame colorBurst =
                  sampleVideo(clips.colorBurst, seconds * 0.78 + 2.1);
              const media::Frame dancer =
                  sampleVideo(clips.alpha, seconds + 0.35);

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
            .cover()
            .cache(Cache::None);

    ctx.composer.render(stack().width(kWidth).height(kHeight).children(
        {std::move(stage),
         sketch::kit::well({.width = kWidth - 144, .padding = 22},
                           sketch::kit::titleCard(
                               {.title = {"Layers of moving light"},
                                .subtitle = {"Cached footage · additive "
                                             "blending · alpha compositing"}}))
             .applyStyleSheet(sketch::kit::theme().styleSheet())
             .at({72, 72})}));
  }
};

SIGIL_SKETCH(VideoCompositing, "Media",
             "A direct-video feature plate for cached network footage, "
             "additive black-backed VFX, frame reuse, and native alpha.")
