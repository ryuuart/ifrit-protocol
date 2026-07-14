// Scene F(a) — a 1000-word paragraph rains onto an umbrella: the justified
// measure breathes (full relayout, every frame), attached letters track
// their re-laid-out lines, detached letters fall, slide down the dome
// tangent, drain off-screen, and re-attach to the paragraph.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <random>

using namespace textflow;

void sceneRain(FontContext &fontContext, int frames,
               const std::filesystem::path &outputDirectory) {
  Paragraph paragraph = makeBigParagraph(1000, 13.0f);

  const float baseWidth = 1080;
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 22;

  const SkPoint domeCenter = {600, 1560};
  const float domeRadius = 210;
  const float standoffDistance = domeRadius + 10;

  struct Particle {
    enum Mode : uint8_t { kAttached, kFalling, kSliding } mode = kAttached;
    SkPoint position = {0, 0};
    float horizontalVelocity = 0;
    float verticalVelocity = 0;
    float angle = 0;
    float angularVelocity = 0;
  };

  // Stable letter count from an initial layout.
  BlockFlow probe(SkRect::MakeXYWH(60, 40, baseWidth, 1320));
  ParagraphLayout first =
      layoutParagraph(fontContext, paragraph, probe, options);
  size_t glyphCount = 0;
  forEachPlacedGlyph(first, paragraph, [&](auto...) { glyphCount++; });
  std::vector<Particle> particles(glyphCount);
  std::printf("  rain: %zu letters from %zu words\n", glyphCount,
              paragraph.words().size());

  std::mt19937 randomEngine(9);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 1900));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches glyphBatches;
  TimingStats layoutTime, simulationTime, drawTime;

  for (int frame = 0; frame < frames; ++frame) {
    // The measure breathes: a genuinely different line-breaking problem
    // every frame, solved from the shape cache.
    const float width =
        baseWidth *
        (1.0f + 0.06f * std::sin(static_cast<float>(frame) * 0.02f));
    BlockFlow flow(SkRect::MakeXYWH(60, 40, width, 1320));

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    // Detach a few attached letters per frame (after a settling intro).
    if (frame > 30)
      for (int detachAttempt = 0; detachAttempt < 22; ++detachAttempt) {
        Particle &particle = particles[randomEngine() % particles.size()];
        if (particle.mode == Particle::kAttached)
          particle.mode =
              Particle::kFalling; // Position seeded from layout below.
      }

    glyphBatches.clear();
    size_t particleIndex = 0;
    forEachPlacedGlyph(
        layout, paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          Particle &particle = particles[particleIndex % particles.size()];
          particleIndex++;
          const float halfAdvance = glyphAdvance * 0.5f;
          // Anchor everything on the glyph's advance center.
          const SkPoint restingCenter =
              restingOrigin + SkVector{halfAdvance, 0};
          float cosine = 1;
          float sine = 0;
          SkPoint drawingCenter = restingCenter;
          switch (particle.mode) {
          case Particle::kAttached:
            break; // rides its (re-laid-out) line
          case Particle::kFalling: {
            if (particle.verticalVelocity == 0 && particle.position.fY == 0) {
              particle.position = restingCenter;
              particle.verticalVelocity =
                  2.5f + static_cast<float>(randomEngine() % 100) * 0.025f;
              particle.horizontalVelocity = 0;
              particle.angularVelocity =
                  (static_cast<float>(randomEngine() % 100) - 50.0f) * 0.0015f;
              particle.angle = 0;
            }
            particle.position.fY += particle.verticalVelocity;
            particle.position.fX += particle.horizontalVelocity;
            particle.angle += particle.angularVelocity;
            const SkVector direction = particle.position - domeCenter;
            if (direction.length() < standoffDistance &&
                particle.position.fY < domeCenter.fY)
              particle.mode = Particle::kSliding;
            if (particle.position.fY > 1940) {
              particle = Particle{}; // Drained: rejoin the paragraph.
            }
            break;
          }
          case Particle::kSliding: {
            SkVector direction = particle.position - domeCenter;
            direction.normalize();
            particle.position = domeCenter + direction * standoffDistance;
            SkVector tangent = {-direction.fY, direction.fX};
            if (tangent.fX * direction.fX < 0)
              tangent = -tangent;
            const float slideDistance = particle.verticalVelocity *
                                        (0.55f + 0.9f * std::abs(direction.fX));
            particle.position += tangent * slideDistance;
            particle.angle = std::atan2(tangent.fY, tangent.fX);
            if (direction.fY > -0.18f) {
              particle.mode = Particle::kFalling;
              particle.horizontalVelocity = tangent.fX * slideDistance;
            }
            break;
          }
          }
          if (particle.mode != Particle::kAttached) {
            drawingCenter = particle.position;
            quantizeAngle(particle.angle, cosine, sine);
          }
          GlyphRSXformBatches::Batch &batch =
              glyphBatches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back({cosine, sine,
                                      drawingCenter.fX - cosine * halfAdvance,
                                      drawingCenter.fY - sine * halfAdvance});
        });
    const auto simulationEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint umbrella;
    umbrella.setAntiAlias(true);
    umbrella.setColor(0xFFB9552F);
    SkPathBuilder dome;
    dome.addArc(SkRect::MakeLTRB(
                    domeCenter.fX - domeRadius, domeCenter.fY - domeRadius,
                    domeCenter.fX + domeRadius, domeCenter.fY + domeRadius),
                180, 180);
    dome.close();
    canvas->drawPath(dome.detach(), umbrella);
    SkPaint pole;
    pole.setAntiAlias(true);
    pole.setStyle(SkPaint::kStroke_Style);
    pole.setStrokeWidth(8);
    pole.setColor(kInk);
    canvas->drawLine(domeCenter.fX, domeCenter.fY - 8, domeCenter.fX,
                     domeCenter.fY + 300, pole);
    glyphBatches.draw(canvas);
    const auto drawEndTime = Clock::now();

    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));
    simulationTime.add(toMicroseconds(simulationEndTime - layoutEndTime));
    drawTime.add(toMicroseconds(drawEndTime - simulationEndTime));
    if (frame == frames / 2 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "rain_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }
  layoutTime.report("rain relayout (1000 words)");
  simulationTime.report("rain letters (sim + place)");
  drawTime.report("rain draw (CPU raster)");
}
