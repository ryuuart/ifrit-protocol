// Scene: letter rain on an umbrella (full-paragraph relayout).
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <random>

using namespace textflow;

namespace gallery {

namespace {

class RainScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double /*elapsedSeconds*/,
                    int frameNumber, const SceneParams & /*params*/,
                    FontContext &fontContext) override {
    if (m_paragraph.text().empty())
      m_paragraph = makeBigParagraph(700, 13.0f);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const SkPoint domeCenter = {canvasWidth * 0.5f, canvasHeight * 0.80f};
    const float domeRadius = std::min(canvasWidth, canvasHeight) * 0.20f;
    const float standoffDistance = domeRadius + 8;

    // The measure breathes: a different line-breaking problem every frame.
    const float width =
        (canvasWidth - 90) *
        (1.0f + 0.05f * std::sin(static_cast<float>(frameNumber) * 0.02f));
    BlockFlow flow(SkRect::MakeXYWH(40, 16, width, canvasHeight * 0.52f));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 21;

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    size_t glyphCount = 0;
    forEachPlacedGlyph(layout, m_paragraph, [&](auto...) { glyphCount++; });
    // The breathing measure can overflow a varying tail of words, so the
    // placed-glyph count drifts a little frame to frame — resize keeps the
    // surviving particles' state instead of resetting the whole sky.
    if (m_particles.size() != glyphCount)
      m_particles.resize(glyphCount);
    if (m_particles.empty())
      return {toMicroseconds(layoutEndTime - layoutStartTime),
              static_cast<int>(layout.runs.size()), 0};

    if (frameNumber > 30)
      for (int detachAttempt = 0; detachAttempt < 18; ++detachAttempt) {
        Particle &particle = m_particles[m_randomEngine() % m_particles.size()];
        if (particle.mode == Particle::kAttached)
          particle.mode = Particle::kFalling;
      }

    m_batches.clear();
    size_t particleIndex = 0;
    forEachPlacedGlyph(
        layout, m_paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          Particle &particle = m_particles[particleIndex % m_particles.size()];
          particleIndex++;
          const float halfAdvance = glyphAdvance * 0.5f;
          const SkPoint restingCenter =
              restingOrigin + SkVector{halfAdvance, 0};
          float cosine = 1;
          float sine = 0;
          SkPoint drawingCenter = restingCenter;
          switch (particle.mode) {
          case Particle::kAttached:
            break;
          case Particle::kFalling: {
            if (particle.verticalVelocity == 0 && particle.position.fY == 0) {
              particle.position = restingCenter;
              particle.verticalVelocity =
                  2.5f + static_cast<float>(m_randomEngine() % 100) * 0.025f;
              particle.horizontalVelocity = 0;
              particle.angularVelocity =
                  (static_cast<float>(m_randomEngine() % 100) - 50.0f) *
                  0.0015f;
              particle.angle = 0;
            }
            particle.position.fY += particle.verticalVelocity;
            particle.position.fX += particle.horizontalVelocity;
            particle.angle += particle.angularVelocity;
            const SkVector direction = particle.position - domeCenter;
            if (direction.length() < standoffDistance &&
                particle.position.fY < domeCenter.fY)
              particle.mode = Particle::kSliding;
            if (particle.position.fY > size.height() + 30)
              particle = Particle{}; // Drained: rejoin the paragraph.
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
              m_batches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back({cosine, sine,
                                      drawingCenter.fX - cosine * halfAdvance,
                                      drawingCenter.fY - sine * halfAdvance});
        });

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
    pole.setStrokeWidth(6);
    pole.setColor(kInk);
    canvas->drawLine(domeCenter.fX, domeCenter.fY - 6, domeCenter.fX,
                     canvasHeight - 10, pole);
    const int drawnGlyphCount = m_batches.draw(canvas);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), drawnGlyphCount};
  }

private:
  struct Particle {
    enum Mode : uint8_t { kAttached, kFalling, kSliding } mode = kAttached;
    SkPoint position = {0, 0};
    float horizontalVelocity = 0;
    float verticalVelocity = 0;
    float angle = 0;
    float angularVelocity = 0;
  };
  Paragraph m_paragraph;
  std::vector<Particle> m_particles;
  GlyphRSXformBatches m_batches;
  std::mt19937 m_randomEngine{9};
};

SceneDescriptor makeRainDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Letter rain — 700 words");
  descriptor.textEditable = false;
  descriptor.displayOrder = 40;
  descriptor.make = [] { return std::make_unique<RainScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeRainDescriptor())

} // namespace gallery
