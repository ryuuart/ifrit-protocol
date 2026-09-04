/** @file The vertical sketch montage and its video edit. */

#include "sigilsketch/plate/Story.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkFont.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilvideo/encode/Encode.h>
#include <sigilweave/fonts/FontContext.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace sigil::sketch {

namespace {

constexpr SkColor kGround = SK_ColorBLACK;
constexpr SkColor kInk = SK_ColorWHITE;

std::vector<int> selection(const StoryOptions& options) {
  std::vector<int> chosen;
  const std::vector<Entry>& entries = registry();
  const int first = options.only >= 0 ? options.only : 0;
  const int last = options.only >= 0 ? options.only + 1 : (int)entries.size();
  for (int index = first; index < last && index < (int)entries.size();
       ++index) {
    if (!options.kind.empty()) {
      const Kind kind = entries[index].kind();
      if (!kind || kind->runtime() != options.kind) continue;
    }
    chosen.push_back(index);
  }
  return chosen;
}

void drawLabel(SkCanvas& canvas, const sk_sp<SkTypeface>& face,
               const std::string& text, float x, float y, float size,
               SkColor color) {
  SkFont font(face, size);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas.drawString(text.c_str(), x, y, font, paint);
}

sk_sp<SkImage> makeLabelLayer(const Entry& entry, const sk_sp<SkTypeface>& face,
                              int width, int height) {
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!surface) return nullptr;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  const float scale = std::min(width / 1080.0f, height / 1920.0f);
  drawLabel(canvas, face, title(entry.name), 72.0f * scale, 154.0f * scale,
            54.0f * scale, kInk);
  return surface->makeImageSnapshot();
}

bool appendTitle(video::Encoder& encoder, SkSurface& surface,
                 const sk_sp<SkTypeface>& face, int frames, bool outro) {
  for (int frame = 0; frame < frames; ++frame) {
    SkCanvas& canvas = *surface.getCanvas();
    canvas.clear(kGround);
    const float scale =
        std::min(surface.width() / 1080.0f, surface.height() / 1920.0f);
    drawLabel(canvas, face, outro ? "EVERY SKETCH" : "SIGIL SKETCHBOOK",
              72.0f * scale, surface.height() * 0.5f, 72.0f * scale, kInk);
    const sk_sp<SkImage> image = surface.makeImageSnapshot();
    if (!image || !encoder.append(*image)) return false;
  }
  return true;
}

SkRect fit(const SkImage& image, const SkRect& box, float zoom) {
  const float scale =
      std::min(box.width() / image.width(), box.height() / image.height()) *
      zoom;
  const float width = image.width() * scale;
  const float height = image.height() * scale;
  return SkRect::MakeXYWH(box.centerX() - width * 0.5f,
                          box.centerY() - height * 0.5f, width, height);
}

void drawClip(SkCanvas& canvas, const sk_sp<SkImage>& image,
              const sk_sp<SkImage>& labels, int width, int height) {
  canvas.clear(kGround);

  if (image) {
    const float scale = std::min(width / 1080.0f, height / 1920.0f);
    const SkRect viewport =
        SkRect::MakeLTRB(72.0f * scale, 220.0f * scale, width - 72.0f * scale,
                         height - 72.0f * scale);
    const SkRect destination = fit(*image, viewport, 1.0f);
    const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                     SkMipmapMode::kLinear);
    canvas.drawImageRect(image, destination, sampling);
  }
  if (labels) canvas.drawImage(labels, 0.0f, 0.0f);
}

}  // namespace

int story(const StoryOptions& options, weave::FontContext& fonts,
          Assets& assets) {
  if (options.width <= 0 || options.height <= 0 || (options.width & 1) != 0 ||
      (options.height & 1) != 0 || options.framesPerSecond <= 0 ||
      options.framesPerSketch <= 0 || options.bitRate <= 0) {
    std::fprintf(stderr,
                 "story dimensions must be positive and even; frame rate, "
                 "frames and bit rate must be positive\n");
    return 2;
  }
  if (!video::formatForPath(options.out)) {
    std::fprintf(stderr, "story output must end in .mp4\n");
    return 2;
  }

  std::unique_ptr<video::Encoder> encoder = video::Encoder::make(
      video::Format::Mp4, {.width = options.width,
                           .height = options.height,
                           .framesPerSecond = options.framesPerSecond,
                           .bitRate = options.bitRate,
                           .hardware = options.hardware});
  if (!encoder) {
    std::fprintf(stderr, "no H.264 encoder accepted the story options\n");
    return 1;
  }
  sk_sp<SkSurface> output = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(options.width, options.height));
  if (!output) {
    std::fprintf(stderr, "could not allocate the story surface\n");
    return 1;
  }
  const sk_sp<SkTypeface> face = fonts.defaultTypeface();
  if (!face) {
    std::fprintf(stderr, "story could not resolve a display typeface\n");
    return 1;
  }
  if (!appendTitle(*encoder, *output, face, options.introFrames, false)) {
    std::fprintf(stderr, "story intro encode failed: %s\n",
                 encoder->error().c_str());
    return 1;
  }

  const std::vector<int> chosen = selection(options);
  const std::vector<Entry>& entries = registry();
  std::vector<int> available;
  int skipped = 0;
  available.reserve(chosen.size());
  for (int index : chosen) {
    std::string why;
    if (entries[index].available(&why)) {
      available.push_back(index);
    } else {
      std::printf("story %-24s [skipped: %s]\n", entries[index].name,
                  why.c_str());
      ++skipped;
    }
  }
  int rendered = 0;
  for (int index : available) {
    const Entry& entry = entries[index];
    noteSketch(entry.name);
    notePlates(rendered);
    try {
      const Kind kind = entry.kind();
      if (!kind) {
        std::printf("story %-24s [skipped: no runtime]\n", entry.name);
        ++skipped;
        continue;
      }
      std::unique_ptr<Session> session = kind->open(fonts, assets, true);
      if (!session) {
        std::printf("story %-24s [skipped: did not open]\n", entry.name);
        ++skipped;
        continue;
      }
      session->setAutoPromotion(false);
      const SkSize declared = session->canvas().size;
      if (declared.width() <= 0 || declared.height() <= 0) {
        std::printf("story %-24s [skipped: empty canvas]\n", entry.name);
        ++skipped;
        continue;
      }
      constexpr float kSourceCeiling = 1120.0f;
      const float sourceScale = std::min(
          1.0f, kSourceCeiling / std::max(declared.width(), declared.height()));
      const int sourceWidth =
          std::max(1, (int)std::ceil(declared.width() * sourceScale));
      const int sourceHeight =
          std::max(1, (int)std::ceil(declared.height() * sourceScale));
      sk_sp<SkSurface> source = SkSurfaces::Raster(
          SkImageInfo::MakeN32Premul(sourceWidth, sourceHeight));
      if (!source) {
        std::fprintf(stderr, "story could not allocate %s source surface\n",
                     entry.name);
        return 1;
      }
      const sk_sp<SkImage> labels =
          makeLabelLayer(entry, face, options.width, options.height);
      if (!labels) {
        std::fprintf(stderr, "story could not allocate %s label layer\n",
                     entry.name);
        return 1;
      }
      SkCanvas& sourceCanvas = *source->getCanvas();
      const double declaredMoment = session->canvas().captureSeconds;
      const double captureMoment = declaredMoment > 0.0 ? declaredMoment : 1.5;
      const double frameStep = 1.0 / options.framesPerSecond;
      const double preRollStep = 1.0 / std::max(60, options.framesPerSecond);
      const bool keepsPixels = kind->runtime() == "draw";
      const sk_sp<SkSurface> scratch =
          keepsPixels ? nullptr
                      : SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
      if (!keepsPixels && !scratch) {
        std::fprintf(stderr, "story could not allocate its pre-roll surface\n");
        return 1;
      }
      const auto step = [&](SkCanvas& canvas, float scale, double dt) {
        canvas.clear(session->canvas().background);
        canvas.save();
        canvas.scale(scale, scale);
        session->frame(canvas, dt);
        canvas.restore();
      };

      // Reach the moment the sketch named by ordinary display-sized steps.
      // A single large delta makes fixed-rate simulations discard work at
      // their catch-up limit, leaving a long sketch visibly short of the
      // state it declared worth capturing.
      double elapsed = 0.0;
      SkCanvas& preRollCanvas =
          keepsPixels ? sourceCanvas : *scratch->getCanvas();
      const float preRollScale = keepsPixels ? sourceScale : 1.0f;
      while (elapsed + preRollStep < captureMoment) {
        step(preRollCanvas, preRollScale, preRollStep);
        elapsed += preRollStep;
      }
      step(preRollCanvas, preRollScale, captureMoment - elapsed);
      // A retained canvas or set can form the final state once at the output
      // extent. A draw sketch keeps the pixels made during pre-roll, so it
      // already stands on the source surface at the right resolution.
      if (!keepsPixels) step(sourceCanvas, sourceScale, 0.0);

      for (int frame = 0; frame < options.framesPerSketch; ++frame) {
        if (frame > 0) step(sourceCanvas, sourceScale, frameStep);
        const sk_sp<SkImage> image = source->makeImageSnapshot();
        drawClip(*output->getCanvas(), image, labels, options.width,
                 options.height);
        const sk_sp<SkImage> encodedFrame = output->makeImageSnapshot();
        if (!encodedFrame || !encoder->append(*encodedFrame)) {
          std::fprintf(stderr, "story frame encode failed at %s: %s\n",
                       entry.name, encoder->error().c_str());
          return 1;
        }
      }
      ++rendered;
      std::printf("story %-24s %d/%zu\n", entry.name, rendered,
                  available.size());
      std::fflush(stdout);
    } catch (const std::exception& exception) {
      std::printf("story %-24s [skipped: %s]\n", entry.name, exception.what());
      ++skipped;
    }
  }

  if (rendered == 0) {
    std::fprintf(stderr, "story selection contained no renderable sketches\n");
    return 1;
  }
  if (!appendTitle(*encoder, *output, face, options.outroFrames, true)) {
    std::fprintf(stderr, "story outro encode failed: %s\n",
                 encoder->error().c_str());
    return 1;
  }
  const sk_sp<SkData> mp4 = encoder->finish();
  if (!mp4) {
    std::fprintf(stderr, "story finalization failed: %s\n",
                 encoder->error().c_str());
    return 1;
  }
  if (!io::writeBytes(options.out, mp4->data(), mp4->size())) {
    std::fprintf(stderr, "could not write story to %s\n", options.out.c_str());
    return 1;
  }
  std::printf("wrote %s: %d sketches, %d skipped, %lld frames, %s\n",
              options.out.c_str(), rendered, skipped,
              (long long)encoder->frameCount(), encoder->codec().c_str());
  return 0;
}

}  // namespace sigil::sketch
