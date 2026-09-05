/** @file
 * sticker_collection — one small animated-media catalog over a shared stage.
 *
 * SigilIO fetches and caches the documents. SigilImage eagerly decodes GIF,
 * WebP and AVIF sequences into fully composited frames; the live leaf asks
 * each document for frameAt(elapsed) on every paint. SigilVideo streams the
 * WebM entry and keeps its native alpha plane. The fixed checker-and-wave
 * ground makes partial transparency visible without moving any sticker box.
 *
 * The GIF is Ryo Hirafuji's Twinkle Star sample. The AVIF sequence is
 * libavif's animated-alpha fixture. The transparent WebPs are Google Fonts'
 * CC BY 4.0 Noto Animated Emoji. The VP8A dancer comes from Sam Dutton's
 * Apache-licensed simpl demo.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilio/IO.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace sketch = sigil::sketch;
namespace image = sigil::image;
namespace io = sigil::io;
namespace video = sigil::video;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kWidth = 1080.0f;
constexpr float kHeight = 1350.0f;
constexpr std::string_view kSamplesCommit =
    "c666a368b73006246694919b5dbcc078317af6cc";
constexpr std::string_view kLibavifCommit =
    "66663952a677bb8a13ea1530d5694775d7d143d4";
constexpr std::string_view kSimplCommit =
    "2c9682d109541f6d8407fa8cdcc4d18735d0b9c5";

const std::string kGif =
    "https://raw.githubusercontent.com/link-u/avif-sample-images/" +
    std::string(kSamplesCommit) + "/star.gif";
const std::string kAvif =
    "https://raw.githubusercontent.com/AOMediaCodec/libavif/" +
    std::string(kLibavifCommit) +
    "/tests/data/colors-animated-8bpc-alpha-exif-xmp.avif";
constexpr std::string_view kSparkle =
    "https://fonts.gstatic.com/s/e/notoemoji/latest/2728/512.webp";
constexpr std::string_view kDiamond =
    "https://fonts.gstatic.com/s/e/notoemoji/latest/1f48e/512.webp";
constexpr std::string_view kHeart =
    "https://fonts.gstatic.com/s/e/notoemoji/latest/1f496/512.webp";
const std::string kWebm = "https://raw.githubusercontent.com/samdutton/simpl/" +
                          std::string(kSimplCommit) +
                          "/videoalpha/video/dancer1.webm";

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

void drawContained(SkCanvas& canvas, const sk_sp<SkImage>& image,
                   const SkRect& box, float rotation = 0.0f) {
  if (!image) return;
  const float scale =
      std::min(box.width() / image->width(), box.height() / image->height());
  const float width = image->width() * scale;
  const float height = image->height() * scale;
  const SkRect destination =
      SkRect::MakeXYWH(-width * 0.5f, -height * 0.5f, width, height);
  canvas.save();
  canvas.translate(box.centerX(), box.centerY());
  canvas.rotate(rotation);
  SkPaint paint;
  canvas.drawImageRect(image, destination,
                       SkSamplingOptions(SkFilterMode::kLinear), &paint);
  canvas.restore();
}

void drawGround(SkCanvas& canvas, SkSize size) {
  canvas.clear(SkColorSetRGB(248, 248, 244));
  SkPaint tile;
  tile.setColor(SkColorSetRGB(23, 23, 26));
  constexpr float kTile = 90.0f;
  for (int row = 0; row * kTile < size.height(); ++row)
    for (int column = 0; column * kTile < size.width(); ++column)
      if ((row + column) % 2 == 0)
        canvas.drawRect(
            SkRect::MakeXYWH(column * kTile, row * kTile, kTile, kTile), tile);

  SkPaint wave;
  wave.setAntiAlias(true);
  wave.setColor(SkColorSetRGB(255, 52, 167));
  wave.setStyle(SkPaint::kStroke_Style);
  wave.setStrokeCap(SkPaint::kRound_Cap);
  wave.setStrokeWidth(24.0f);
  for (int strand = 0; strand < 4; ++strand) {
    SkPathBuilder path;
    for (int point = 0; point <= 90; ++point) {
      const float x = size.width() * point / 90.0f;
      const float y = 290.0f + strand * 265.0f +
                      std::sin(point * 0.31f + strand * 1.4f) * 58.0f;
      if (point == 0)
        path.moveTo(x, y);
      else
        path.lineTo(x, y);
    }
    canvas.drawPath(path.detach(), wave);
  }
}

struct Shelf {
  std::array<std::shared_ptr<const image::ImageAsset>, 5> images;
  std::shared_ptr<video::Video> webm;
};

}  // namespace

struct StickerCollection final : sketch::Sketch {
  static bool available(std::string* why) {
    return sketch::requireCached(
        {kGif, kAvif, kSparkle, kDiamond, kHeart, kWebm}, why);
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = SkSize::Make(kWidth, kHeight),
                             .captureAt = 2.35,
                             .background = SkColor4f{0.97f, 0.97f, 0.95f, 1}});

    io::Hub& hub = ctx.assets.hub();
    const Shelf shelf{
        .images = {hub.image(kGif), hub.image(kAvif), hub.image(kSparkle),
                   hub.image(kDiamond), hub.image(kHeart)},
        .webm = loadVideo(hub, kWebm)};

    Element stage =
        custom("stickers.live",
               [shelf](SkCanvas& canvas, const PaintContext& paint) {
                 drawGround(canvas, paint.size);
                 const std::array<SkRect, 6> boxes = {
                     SkRect::MakeXYWH(70, 190, 280, 300),
                     SkRect::MakeXYWH(400, 170, 290, 320),
                     SkRect::MakeXYWH(735, 190, 270, 300),
                     SkRect::MakeXYWH(65, 665, 300, 330),
                     SkRect::MakeXYWH(405, 650, 275, 350),
                     SkRect::MakeXYWH(725, 670, 290, 320)};
                 constexpr std::array<float, 5> turns = {-8, 7, -4, 9, -6};
                 constexpr std::array<double, 5> offsets = {0.0, 270.0, 510.0,
                                                            760.0, 1030.0};
                 for (size_t i = 0; i < shelf.images.size(); ++i) {
                   const auto& asset = shelf.images[i];
                   if (!asset) continue;
                   const image::Frame& frame = asset->frameAt(
                       paint.elapsedSeconds * 1000.0 + offsets[i]);
                   drawContained(canvas, frame.image, boxes[i], turns[i]);
                 }

                 if (shelf.webm) {
                   const video::VideoFrame frame = shelf.webm->frameAt(
                       loopTime(*shelf.webm, paint.elapsedSeconds + 0.42),
                       canvas.recorder());
                   drawContained(canvas, frame.image, boxes.back(), 5.0f);
                 }
               })
            .absolute()
            .inset(0)
            .cache(Cache::None);

    const weave::TextStyle title =
        weave::textStyle({.size = 29, .color = {1, 1, 1, 1}, .track = 3.4f});
    ctx.composer.render(
        stack()
            .width(kWidth)
            .height(kHeight)
            .child(std::move(stage))
            .child(text(u8"SIGIL STICKERS / GIF · WEBP · AVIFS · WEBM", title)
                       .absolute()
                       .inset(52, 58, 52, 1190)));
  }
};

SIGIL_SKETCH(StickerCollection, "Media",
             "A cached animated-sticker shelf across GIF, WebP, AVIF "
             "sequences, and alpha WebM.")
