/** @file
 * encode_write — the same pixels through each encoder, and out through
 * the hub that owns where bytes go.
 *
 * `encodeImage` hands BYTES back and nothing else. What the bytes mean,
 * where they land, under what name and through which mount is
 * SigilIO's concern, and the two are separate on purpose: the
 * encoder never looks at a filename, and `Hub::write` never looks at the
 * content. This sheet is one call of each, in that order.
 *
 * `quality` is honoured by the lossy formats alone, and WebP's 100 is
 * not "lossy at maximum": it selects the format's LOSSLESS mode, which
 * is a different codec inside the same container, because a caller
 * asking for everything wants the one that keeps everything. PNG is
 * lossless at every setting and ignores the number.
 *
 * The pixmap overload encodes the pixels EXACTLY as they are given, so
 * the colour type is the caller's; the image overload reads back at the
 * depth the format wants — premultiplied N32 for the LDR formats, RGBA
 * float for EXR — which is why a float image written as PNG is
 * tone-independent eight-bit. Asking for a PNG is asking for that.
 *
 * A format with no encoder in the build simply fails to encode, the same
 * way a format with no decoder fails to decode, so the cells print what
 * came back rather than assuming it did.
 *
 * `Hub::write` stores through the same mount table a read resolves by,
 * makes the directories above the file, and DROPS every cached view of
 * that URI so the next ask reads the file back instead of serving what
 * was there before.
 *
 * EDIT THESE FIRST
 *   kLossy — the quality the lossy cells ask for.
 *   kSide — the side of the source image, px.
 *   kMount — the prefix the written bytes are stored under.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace io = sigil::io;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 166;
constexpr float kPicture = 176;

constexpr int kLossy = 24;  // the quality the lossy cells ask for
constexpr int kSide = 176;  // the source image's side, px
const char* kMount = "out://";

constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

/** The source: a smooth ramp under hard edges and fine text-sized
 *  detail, which is the pair of things the lossy codecs disagree about. */
sk_sp<SkImage> source() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
  SkCanvas* canvas = surface->getCanvas();
  const SkPoint ends[2] = {{0, 0}, {kSide, kSide}};
  const SkColor4f stops[2] = {{0.10f, 0.16f, 0.30f, 1},
                              {0.92f, 0.62f, 0.30f, 1}};
  SkPaint paint;
  paint.setShader(SkShaders::LinearGradient(
      ends, SkGradient({{stops, 2}, {}, SkTileMode::kClamp}, {})));
  canvas->drawPaint(paint);
  paint.setShader(nullptr);
  paint.setAntiAlias(true);
  paint.setColor4f({0.98f, 0.97f, 0.94f, 1});
  for (int i = 0; i < 9; ++i) {
    const float y = 18.0f + (float)i * 8.0f;
    canvas->drawRect({14, y, 14 + (float)(i * 15 % 120), y + 2.5f}, paint);
  }
  paint.setColor4f({0.05f, 0.05f, 0.08f, 1});
  canvas->drawCircle(kSide * 0.62f, kSide * 0.64f, kSide * 0.22f, paint);
  paint.setColor4f({0.98f, 0.97f, 0.94f, 1});
  canvas->drawCircle(kSide * 0.62f, kSide * 0.64f, kSide * 0.11f, paint);
  return surface->makeImageSnapshot();
}

Element cell(const char* call, const char* note, sk_sp<SkImage> picture,
             const std::string& readout) {
  Element art = picture ? image(std::make_shared<const img::ImageAsset>(
                              img::ImageAsset::wrap(std::move(picture))))
                        : box();
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(art).absolute().inset(0))
          .child(text(toU8(readout), sketch::kit::theme().mono(10, kFigure))
                     .absolute()
                     .left(Dim(6.0f))
                     .top(Dim(6.0f))
                     .padding(4, 2)
                     .fill(Fill::color({0, 0, 0, 0.55f}))));
}

}  // namespace

struct EncodeWrite final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // every encode has already been taken
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const sk_sp<SkImage> art = source();

    /** One encode, decoded straight back so the cell shows what the
     *  bytes hold rather than what went in. */
    const auto roundTrip = [&](img::Format format, int quality) {
      sk_sp<SkData> bytes = img::encodeImage(*art, format, {quality});
      sk_sp<SkImage> back;
      if (bytes)
        if (std::optional<img::ImageAsset> decoded =
                img::ImageAsset::decode(bytes))
          back = decoded->frames().empty() ? nullptr
                                           : decoded->frames().front().image;
      return std::pair<sk_sp<SkImage>, size_t>{std::move(back),
                                               bytes ? bytes->size() : 0};
    };

    const auto [png, pngBytes] = roundTrip(img::Format::Png, 100);
    const auto [webpLossless, losslessBytes] =
        roundTrip(img::Format::Webp, 100);
    const auto [webpLossy, lossyBytes] = roundTrip(img::Format::Webp, kLossy);
    const auto [jpeg, jpegBytes] = roundTrip(img::Format::Jpeg, kLossy);

    // …and out through the hub, which is the half that knows about
    // names, mounts and directories.
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sigil-encode-write";
    io::Hub hub;
    hub.mount(kMount, dir);
    sk_sp<SkData> bytes = img::encodeImage(*art, img::Format::Png);
    const std::string uri = std::string(kMount) + "plate.png";
    const bool wrote = bytes && hub.write(uri, bytes->data(), bytes->size());
    const std::shared_ptr<const img::ImageAsset> read =
        wrote ? hub.image(uri) : nullptr;
    const std::string written =
        kit::formatted("write %s\nread back %s \xc2\xb7 %d\xc3\x97%d",
                    wrote ? "true" : "false", read ? "true" : "false",
                    read ? read->width() : 0, read ? read->height() : 0);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("ENCODE, THEN WRITE \xc2\xb7 image::encodeImage, "
                       "io::Hub::write"),
         .subtitle = toU8("dials \xc2\xb7 the format \xc2\xb7 the quality the "
                          "lossy ones honour (24) \xc2\xb7 the source's side "
                          "(176 px) \xc2\xb7 the mount the bytes are stored "
                          "under"),
         .footer = toU8("the encoder hands bytes back and never looks at "
                        "a filename; the hub stores them through the "
                        "same mount table a read resolves by and drops "
                        "every cached view of that URI, so the next ask "
                        "reads the file")},
        kit::cells(
            {.cells =
                 {cell("the source pixels",
                       "a smooth ramp under hard edges and fine detail "
                       "\xc2\xb7 the pair of things the lossy codecs "
                       "disagree about",
                       art,
                       kit::formatted("N32 premul \xc2\xb7 %d\xc3\x97%d", kSide,
                                   kSide)),
                  cell("encodeImage(art, Png)",
                       "lossless at every setting, and the quality is "
                       "ignored \xc2\xb7 the bytes decode back to the "
                       "pixels that went in",
                       png, kit::formatted("png \xc2\xb7 %zu bytes", pngBytes)),
                  cell("Webp, quality 100",
                       "100 selects the LOSSLESS codec rather than lossy "
                       "at maximum \xc2\xb7 two codecs in one container, "
                       "and this is the one that keeps everything",
                       webpLossless,
                       kit::formatted("webp \xc2\xb7 %zu bytes", losslessBytes)),
                  cell("Webp, quality 24",
                       "the same container, the other codec \xc2\xb7 the "
                       "ramp survives and the fine rules go soft",
                       webpLossy,
                       kit::formatted("webp \xc2\xb7 %zu bytes", lossyBytes)),
                  cell("Jpeg, quality 24",
                       "the quantisation quality \xc2\xb7 the blocks are "
                       "the codec's own, and they land where the edges "
                       "are",
                       jpeg, kit::formatted("jpeg \xc2\xb7 %zu bytes", jpegBytes)),
                  cell("hub.write(uri, bytes)",
                       "the bytes out through the mount table, then "
                       "asked back for as an image \xc2\xb7 the write "
                       "dropped the cached view, so this is the file",
                       read && !read->frames().empty()
                           ? read->frames().front().image
                           : nullptr,
                       written)},
             .gap = 10})));
  }
};

SIGIL_SKETCH(EncodeWrite, "Kit \xc2\xb7 API",
             "one image through each encoder and straight back, with the "
             "byte counts, and then out through the hub and read back as a "
             "file")
