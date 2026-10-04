/** @file
 * video_compose — one hundred streaming video leaves under the retained
 * Compose grammar.
 *
 * Fifty cells each hold an opaque sky and one of three effect sources.
 * The effect layer rotates between black-backed additive footage and native
 * alpha. Five independently clocked decoders share one bounded playback
 * scheduler and fan their completed frames out to one hundred leaves. A solid
 * loading cover stays over the scene until every source has a frame, and a
 * deterministic capture samples the same five decoders synchronously.
 */

// TAGS: Media/Video

#include <include/core/SkRect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmedia/video/Video.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

namespace arrange = sigil::geometry::arrange;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace io = sigil::io;
namespace media = sigil::media;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {

constexpr float kWidth = 1080.0f;
constexpr float kHeight = 1920.0f;
constexpr int kColumns = 5;
constexpr int kRows = 10;
constexpr int kCells = kColumns * kRows;
constexpr int kVideoLeaves = kCells * 2;

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

constexpr std::array<std::string_view, 5> kSources = {kDaySky, kNightSky, kDust,
                                                      kColorBurst, kAlphaVideo};

using Clips = std::array<std::shared_ptr<const media::Video>, kSources.size()>;

media::Timing timingFor(int source) {
  return {.start = std::chrono::duration<double>(source * 0.41),
          .rate = 0.72 + source * 0.13,
          .loop = media::Loop::Forever};
}

}  // namespace

struct VideoCompose {
  sigil::motion::Animatable<float> loading = sigil::motion::animatable(0.0f);

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

    std::shared_ptr<media::Playback> playback;
    if (!ctx.deterministic)
      playback = std::make_shared<media::Playback>(
          media::Playback::Options{.workers = 8});

    Clips clips;
    io::Hub& hub = ctx.assets.hub();
    for (size_t index = 0; index < kSources.size(); ++index)
      clips[index] = hub.load<media::Video>(
          kSources[index], {.playback = playback, .cachedFrames = 12});

    loading = 0.0f;
    if (playback) {
      for (size_t index = 0; index < clips.size(); ++index)
        if (clips[index]) clips[index]->frameAt({}, timingFor((int)index));
      loading = 1.0f;
      ctx.engine.timer([this, clips] {
        for (const auto& clip : clips)
          if (clip && !clip->hasFrame()) return true;
        loading = 0.0f;
        return false;
      });
    }

    const glm::vec2 module =
        arrange::moduleSize({kWidth, kHeight}, kColumns, kRows, {0, 0});
    const auto leafAt = [&](int source, int cell, bool overlay) {
      Element leaf =
          image(media::PixelSource(clips[source], timingFor(source)),
                source == 4 ? material::Fit::Contain : material::Fit::Cover)
              .opacity(overlay && source == 2 ? 0.90f : 1.0f);
      if (source == 2 || source == 3 || (source == 4 && (cell & 1)))
        leaf.blendMode(material::BlendMode::PlusLighter);
      // Half a pixel of bleed on the far edges, so two neighbouring
      // leaves never leave a seam between them.
      const auto at =
          arrange::cellRect(arrange::cellAt((size_t)cell, kColumns), module);
      return leaf.rect(at.left(), at.top(), at.width() + 0.5f,
                       at.height() + 0.5f);
    };
    const auto cells = std::views::iota(0, kCells);

    ctx.composer.render(stack().width(kWidth).height(kHeight).children(
        {// A sky under every cell, and an effect source over it.
         each(cells, [&](int cell) { return leafAt(cell & 1, cell, false); }),
         each(cells,
              [&](int cell) { return leafAt(2 + cell % 3, cell, true); }),
         sketch::kit::well(
             {.width = kWidth - 84, .padding = 18},
             sketch::kit::titleCard(
                 {.title = {"One hundred video leaves"},
                  .subtitle =
                      {"Shared playback · additive effects · native alpha"}}))
             .applyStyleSheet(sketch::kit::theme().styleSheet())
             .at({42, 42}),
         // The cover the scene waits behind until every source has
         // a frame.
         kit::centred(document::label("BUFFERING / 005 SOURCES")
                          .font(sketch::kit::theme().font(
                              sketch::kit::theme().type.captionLabel))
                          .ink(sketch::kit::theme().palette.ink))
             .cover()
             .fill(Fill::color({0, 0, 0, 1}))
             .opacity(loading)}));
  }
};

static_assert(kVideoLeaves == 100);

SIGIL_SKETCH(VideoCompose, "Media",
             "One hundred Compose video leaves share a bounded playback "
             "scheduler across sky, additive VFX, and native alpha sources.")
