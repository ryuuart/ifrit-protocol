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

#include <include/core/SkBlendMode.h>
#include <include/core/SkRect.h>
#include <sigilcompose/video/Video.h>
#include <sigilio/IO.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace sketch = sigil::sketch;
namespace io = sigil::io;
namespace vid = sigil::video;
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

using Documents = std::array<std::shared_ptr<const io::Bytes>, kSources.size()>;
using Clips = std::array<std::shared_ptr<vid::Video>, kSources.size()>;

std::shared_ptr<vid::Video> openVideo(
    const std::shared_ptr<const io::Bytes>& encoded, std::string_view uri) {
  if (!encoded || encoded->bytes.empty()) return nullptr;
  vid::DecodeOptions options;
  options.cachedFrames = 12;
  return vid::decodeVideo(encoded->bytes.data(), encoded->bytes.size(), options,
                          std::filesystem::path(uri));
}

VideoOptions optionsFor(int source, int cell, bool overlay) {
  VideoOptions options;
  options.startSeconds = source * 0.41;
  options.playbackRate = 0.72 + source * 0.13;
  options.loop = true;
  options.fit = source == 4 ? VideoFit::Contain : VideoFit::Cover;
  options.opacity = overlay && source == 2 ? 0.90f : 1.0f;
  if (source == 2 || source == 3 || (source == 4 && (cell & 1)))
    options.blend = SkBlendMode::kPlus;
  return options;
}

}  // namespace

struct VideoCompose final : sketch::Sketch {
  choreograph::Output<float> loading{0.0f};

  static bool available(std::string* why) {
    return sketch::requireCached(
        {kDaySky, kNightSky, kDust, kColorBurst, kAlphaVideo}, why);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kWidth, kHeight);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(4.25);

    Documents documents;
    io::Hub& hub = ctx.assets.hub();
    for (size_t i = 0; i < kSources.size(); ++i)
      documents[i] = hub.blob(kSources[i]);

    Clips clips;
    for (size_t i = 0; i < kSources.size(); ++i)
      clips[i] = openVideo(documents[i], kSources[i]);

    std::shared_ptr<vid::Playback> playback;
    std::array<vid::Playback::Handle, kSources.size()> handles{};
    loading = 0.0f;
    if (!ctx.deterministic) {
      playback = std::make_shared<vid::Playback>(
          vid::Playback::Options{.workerThreads = 8});
      for (size_t i = 0; i < clips.size(); ++i) {
        handles[i] = playback->add(clips[i]);
        playback->request(handles[i],
                          optionsFor((int)i, 0, i >= 2).startSeconds);
      }
      loading = 1.0f;
      ctx.ticker.add([this, playback, handles](double) {
        for (const vid::Playback::Handle handle : handles)
          if (!playback->ready(handle)) return true;
        loading = 0.0f;
        return false;
      });
    }

    Element stage = stack().width(kWidth).height(kHeight);
    const float cellWidth = kWidth / kColumns;
    const float cellHeight = kHeight / kRows;
    const auto addLeaf = [&](int source, int cell, bool overlay) {
      const VideoOptions options = optionsFor(source, cell, overlay);
      Element leaf =
          playback ? video(clips[source], playback, handles[source], options)
                   : video(clips[source], options);
      const int column = cell % kColumns;
      const int row = cell / kColumns;
      leaf.rect(SkRect::MakeXYWH(column * cellWidth, row * cellHeight,
                                 cellWidth + 0.5f, cellHeight + 0.5f));
      stage.child(std::move(leaf));
    };

    for (int cell = 0; cell < kCells; ++cell) {
      addLeaf(cell & 1, cell, false);
      addLeaf(2 + cell % 3, cell, true);
    }

    const weave::TextStyle title = weave::textStyle(
        {.size = 27, .color = {1, 1, 1, 0.96f}, .track = 5.5f});
    stage.child(text(u8"100 / COMPOSE VIDEO", title)
                    .absolute()
                    .inset(42, 42, 42, kHeight - 96));
    stage.child(box()
                    .absolute()
                    .inset(0)
                    .fill(Fill::color({0, 0, 0, 1}))
                    .opacity(&loading)
                    .alignItems(Align::Center)
                    .justify(Justify::Center)
                    .child(text(u8"BUFFERING / 005 SOURCES", title)));
    ctx.composer.render(std::move(stage));
  }
};

static_assert(kVideoLeaves == 100);

SIGIL_SKETCH(VideoCompose, "Media",
             "One hundred Compose video leaves share a bounded playback "
             "scheduler across sky, additive VFX, and native alpha sources.")
