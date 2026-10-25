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
#include <sigilmaterial/skia/Color.h>
#include <sigilmedia/advanced/Formats.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Placement.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilweave/fonts/FontContext.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <print>
#include <string>
#include <vector>

#include "sigilweave/advanced/Skia.h"

namespace sigil::sketch {

namespace {

constexpr SkColor kGround = SK_ColorBLACK;
constexpr SkColor kInk = SK_ColorWHITE;

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

bool appendTitle(media::Encoder& encoder, SkSurface& surface,
                 const sk_sp<SkTypeface>& face, int frames,
                 const std::string& card) {
  for (int frame = 0; frame < frames; ++frame) {
    SkCanvas& canvas = *surface.getCanvas();
    canvas.clear(kGround);
    const float scale =
        std::min(surface.width() / 1080.0f, surface.height() / 1920.0f);
    drawLabel(canvas, face, card, 72.0f * scale, surface.height() * 0.5f,
              72.0f * scale, kInk);
    const sk_sp<SkImage> image = surface.makeImageSnapshot();
    if (!image || !encoder.append(image)) return false;
  }
  return true;
}

SkRect fit(const SkImage& image, const SkRect& box, float zoom) {
  const SkSize size = {(float)image.width(), (float)image.height()};
  return fitInto(size, box, zoom).rect(size);
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
    std::println(stderr,
                 "story dimensions must be positive and even; frame rate, "
                 "frames and bit rate must be positive");
    return 2;
  }
  if (media::formatForPath(options.outputPath) != media::Format::Mp4) {
    std::println(stderr, "story output must end in .mp4");
    return 2;
  }

  media::Encoder encoder({.width = options.width,
                          .height = options.height,
                          .framesPerSecond = options.framesPerSecond,
                          .bitRate = options.bitRate,
                          .hardware = options.hardware});
  if (!encoder) {
    std::println(stderr, "no H.264 encoder accepted the story options: {}",
                 encoder.error());
    return 1;
  }
  sk_sp<SkSurface> output = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(options.width, options.height));
  if (!output) {
    std::println(stderr, "could not allocate the story surface");
    return 1;
  }
  const sk_sp<SkTypeface> face = fonts.defaultTypeface();
  if (!face) {
    std::println(stderr, "story could not resolve a display typeface");
    return 1;
  }
  if (!appendTitle(encoder, *output, face, options.introFrames,
                   options.title)) {
    std::println(stderr, "story intro encode failed: {}", encoder.error());
    return 1;
  }

  const std::vector<int> chosen = selection(options.only, options.kind);
  const std::vector<Entry>& entries = registry();
  std::vector<int> available;
  int skipped = 0;
  available.reserve(chosen.size());
  for (int index : chosen) {
    std::string why;
    if (entries[index].available(&why)) {
      available.push_back(index);
    } else {
      std::println("story {:<24} [skipped: {}]", entries[index].name, why);
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
        std::println("story {:<24} [skipped: no runtime]", entry.name);
        ++skipped;
        continue;
      }
      std::unique_ptr<Session> session =
          kind->open(fonts, assets, true, entry.key);
      if (!session) {
        std::println("story {:<24} [skipped: did not open]", entry.name);
        ++skipped;
        continue;
      }
      const SkSize declared = session->canvas().size;
      if (declared.width() <= 0 || declared.height() <= 0) {
        std::println("story {:<24} [skipped: empty canvas]", entry.name);
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
        std::println(stderr, "story could not allocate {} source surface",
                     entry.name);
        return 1;
      }
      const sk_sp<SkImage> labels =
          makeLabelLayer(entry, face, options.width, options.height);
      if (!labels) {
        std::println(stderr, "story could not allocate {} label layer",
                     entry.name);
        return 1;
      }
      SkCanvas& sourceCanvas = *source->getCanvas();
      const double declaredMoment = session->canvas().captureSeconds;
      const double captureMoment = declaredMoment > 0.0 ? declaredMoment : 1.5;
      const double frameStep = 1.0 / options.framesPerSecond;
      const double preRollStep = 1.0 / std::max(60, options.framesPerSecond);
      // Persistent draw state still paints on discarded steps; update-only
      // sketches can omit drawing until the output frame.
      const sk_sp<SkSurface> scratch =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
      if (!scratch) {
        std::println(stderr, "story could not allocate its pre-roll surface");
        return 1;
      }
      const auto step = [&](SkCanvas& canvas, float scale, double dt,
                            bool discarded = false) {
        canvas.clear(material::skia::toSkColor(session->canvas().background));
        canvas.save();
        canvas.scale(scale, scale);
        if (discarded)
          session->discardedFrame(canvas, dt);
        else
          session->frame(canvas, dt);
        canvas.restore();
      };

      // Reach the moment the sketch named by ordinary display-sized steps.
      // A single large delta makes fixed-rate simulations discard work at
      // their catch-up limit, leaving a long sketch visibly short of the
      // state it declared worth capturing.
      double elapsed = 0.0;
      SkCanvas& preRollCanvas = *scratch->getCanvas();
      while (elapsed + preRollStep < captureMoment) {
        step(preRollCanvas, 1.0f, preRollStep, true);
        elapsed += preRollStep;
      }
      step(preRollCanvas, 1.0f, captureMoment - elapsed, true);
      // The final state, formed once at the output extent.
      step(sourceCanvas, sourceScale, 0.0);

      for (int frame = 0; frame < options.framesPerSketch; ++frame) {
        if (frame > 0) step(sourceCanvas, sourceScale, frameStep);
        const sk_sp<SkImage> image = source->makeImageSnapshot();
        drawClip(*output->getCanvas(), image, labels, options.width,
                 options.height);
        const sk_sp<SkImage> encodedFrame = output->makeImageSnapshot();
        if (!encodedFrame || !encoder.append(encodedFrame)) {
          std::println(stderr, "story frame encode failed at {}: {}",
                       entry.name, encoder.error());
          return 1;
        }
      }
      ++rendered;
      std::println("story {:<24} {}/{}", entry.name, rendered,
                   available.size());
      std::fflush(stdout);
    } catch (const std::exception& exception) {
      std::println("story {:<24} [skipped: {}]", entry.name, exception.what());
      ++skipped;
    }
  }

  if (rendered == 0) {
    std::println(stderr, "story selection contained no renderable sketches");
    return 1;
  }
  if (!appendTitle(encoder, *output, face, options.outroFrames,
                   "EVERY SKETCH")) {
    std::println(stderr, "story outro encode failed: {}", encoder.error());
    return 1;
  }
  const std::vector<std::byte> mp4 = encoder.finish().value_or(std::vector<std::byte>{});
  if (mp4.empty()) {
    std::println(stderr, "story finalization failed: {}", encoder.error());
    return 1;
  }
  if (!io::writeBytes(options.outputPath, mp4.data(), mp4.size())) {
    std::println(stderr, "could not write story to {}", options.outputPath);
    return 1;
  }
  std::println("wrote {}: {} sketches, {} skipped, {} frames, {}",
               options.outputPath, rendered, skipped, encoder.frameCount(),
               encoder.codec());
  return 0;
}

}  // namespace sigil::sketch
