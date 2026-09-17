/** @file
 * One source image encoded as PNG, lossless WebP, lossy WebP and JPEG.
 * Each picture is decoded from the encoded bytes and its size is measured.
 * The separate transaction writes PNG bytes through a mounted Hub, then
 * reads them as an image. Encoding chooses representation; the Hub resolves
 * and invalidates resource views.
 */

// TAGS: Media/Images

#include <include/core/SkData.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Draw.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace io = sigil::io;
namespace mskia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 700};
constexpr float kCell = 189.6f;
constexpr float kPicture = 190;

constexpr int kLossy = 24;  // the quality the lossy cells ask for
constexpr int kSide = 176;  // the source image's side, px
const char* kMount = "out://";

/** The source: a smooth ramp under hard edges and fine text-sized
 *  detail, which is the pair of things the lossy codecs disagree about. */
sk_sp<SkImage> source() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
  sigil::draw::on(
      *surface->getCanvas(), {kSide, kSide}, [](sigil::draw::Pen& pen) {
        pen.background(
            mskia::Paint::linearUnit({0, 0}, {1, 1},
                                     {{0.0f, {0.10f, 0.16f, 0.30f, 1}},
                                      {1.0f, {0.92f, 0.62f, 0.30f, 1}}}));
        pen.noStroke();
        pen.fill(SkColor4f{0.98f, 0.97f, 0.94f, 1});
        for (int i = 0; i < 9; ++i)
          pen.rect(14, 18.0f + (float)i * 8.0f, (float)(i * 15 % 120), 2.5f);
        const SkPoint eye{kSide * 0.62f, kSide * 0.64f};
        pen.fill(SkColor4f{0.05f, 0.05f, 0.08f, 1});
        pen.circle(eye, kSide * 0.44f);
        pen.fill(SkColor4f{0.98f, 0.97f, 0.94f, 1});
        pen.circle(eye, kSide * 0.22f);
      });
  return surface->makeImageSnapshot();
}

sketch::kit::ComparisonCase encoded(const char* title, const char* control,
                                    sk_sp<SkImage> picture, size_t bytes) {
  return {.title = title,
          .control = control,
          .figure = sketch::kit::well(
              {.width = kCell,
               .height = kPicture,
               .content = sketch::kit::Well::Content{}},
              picture ? image(picture).width(kSide).height(kSide)
                      : text("Encoder unavailable").width(kCell - 24)),
          .note = kit::formatted("%zu bytes", bytes)};
}

}  // namespace

struct EncodeWrite {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
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
        kit::formatted("write %s\nread back %s · %d×%d",
                       wrote ? "true" : "false", read ? "true" : "false",
                       read ? read->width() : 0, read ? read->height() : 0);

    Element codecs = sketch::kit::comparison(
        {.cases =
             {{.title = "SOURCE",
               .control = "176 × 176 · N32",
               .figure =
                   sketch::kit::well({.width = kCell,
                                      .height = kPicture,
                                      .content = sketch::kit::Well::Content{}},
                                     image(art).width(kSide).height(kSide)),
               .note = "Fine rules, a smooth ramp and a hard circular edge."},
              encoded("PNG", "Lossless", png, pngBytes),
              encoded("WEBP", "quality = 100 · lossless", webpLossless,
                      losslessBytes),
              encoded("WEBP", "quality = 24 · lossy", webpLossy, lossyBytes),
              encoded("JPEG", "quality = 24 · lossy", jpeg, jpegBytes)},
         .measure = 1020,
         .gap = 18});

    Element transaction = box().row().gap(28).children(
        {sketch::kit::well({.width = 150,
                            .height = 150,
                            .content = sketch::kit::Well::Content{}},
                           image(read && !read->frames().empty()
                                     ? read->frames().front().image
                                     : nullptr)
                               .width(132)
                               .height(132)),
         box().column().gap(12).width(390).children(
             {text("WRITE THE ENCODED BYTES").styleClass("captionLabel"),
              text("encodeImage → Hub::write → Hub::image")
                  .styleClass("readout"),
              text(uri).styleClass("captionNote"),
              text("The file is read back through the same mount. Encoding "
                   "chooses the representation; the hub chooses its "
                   "destination.")
                  .width(390)}),
         sketch::kit::well({.width = 424, .height = 150, .padding = 20})
             .column()
             .gap(16)
             .children({text("READ AFTER WRITE").styleClass("captionLabel"),
                        text(written).styleClass("readout").width(384),
                        text("Writing invalidates the URI's cached views.")
                            .width(384)})});

    ctx.composer.render(sketch::kit::page(
        {.title = "What the encoder keeps",
         .subtitle = "One source, four decoded results. Compare the fine edges "
                     "before reading the byte counts.",
         .footer = "PNG ignores quality. WebP at 100 selects its lossless "
                   "codec; lower values select lossy compression."},
        box().column().gap(30).children(
            {std::move(codecs),
             sketch::kit::sectionHeader({.label = "FROM PIXELS TO A FILE"}),
             std::move(transaction)})));
  }
};

SIGIL_SKETCH(EncodeWrite, "Kit · API",
             "one image through each encoder and straight back, with the "
             "byte counts, and then out through the hub and read back as a "
             "file")
