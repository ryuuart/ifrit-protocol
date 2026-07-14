#include "GalleryScenes.h"

#include <textflow/Query.h>
#include <textflowqt/TextFlowQt.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRSXform.h>

#include <QStringLiteral>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <random>
#include <string>

using namespace textflow;

namespace gallery {

namespace {

using Clock = std::chrono::steady_clock;

constexpr SkColor kInk = 0xFF23252B;
constexpr SkColor kAccent = 0xFFC63D2F;
constexpr SkColor kBlue = 0xFF2B5AA7;
constexpr SkColor kShape = 0x33808A99;
constexpr SkColor kPaper = 0xFFFAF7F0;

/// Converts a steady-clock duration to the unit used by the gallery HUD.
double toMicroseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

/// Creates the single-span styles shared by the gallery scenes.
TextStyle makeStyle(float fontSize, SkColor color, const char *language = "",
                    sk_sp<SkTypeface> typeface = nullptr) {
  TextStyle style;
  style.shaping.typeface = std::move(typeface);
  style.shaping.fontSize = fontSize;
  style.shaping.languageTag = language;
  style.paint.color = color;
  return style;
}

/// Caches a scene body paragraph until one of its shaping inputs changes.
struct BodyCache {
  Paragraph paragraph;
  QString builtText;
  const SkTypeface *builtTypeface = nullptr;
  float builtFontSize = 0;

  /// Rebuilds the paragraph when its text, typeface, or size has changed.
  /// Returns true when rebuilding occurred.
  bool ensure(const SceneParams &params, const QString &fallbackText,
              const sk_sp<SkTypeface> &fallbackTypeface) {
    const QString &text = params.text.isEmpty() ? fallbackText : params.text;
    const sk_sp<SkTypeface> &typeface =
        params.typeface ? params.typeface : fallbackTypeface;
    if (text == builtText && typeface.get() == builtTypeface &&
        params.fontSize == builtFontSize)
      return false;
    builtText = text;
    builtTypeface = typeface.get();
    builtFontSize = params.fontSize;
    paragraph.clear();
    // Zero-copy: QString and Paragraph both store UTF-16.
    textflowqt::appendText(paragraph, text,
                           makeStyle(params.fontSize, kInk, "", typeface));
    return true;
  }
};

/// Resolves the preferred body serif, falling back to the context default.
sk_sp<SkTypeface> defaultSerif(FontContext &fontContext) {
  sk_sp<SkTypeface> typeface =
      fontContext.fontManager()->matchFamilyStyle("Noto Serif", SkFontStyle());
  return typeface ? typeface : fontContext.defaultTypeface();
}

/// Draws a small single-line explanatory caption.
void drawCaption(SkCanvas *canvas, FontContext &fontContext, const char *text,
                 SkPoint baselineOrigin, float width = 520) {
  Paragraph paragraph;
  paragraph.appendText(text, makeStyle(12.0f, kBlue));
  BlockFlow flow(
      SkRect::MakeXYWH(baselineOrigin.x(), baselineOrigin.y(), width, 32));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

/// Builds mixed Latin/CJK filler in alternating color chunks.
Paragraph makeBigParagraph(int wordCount, float fontSize) {
  const char *latin[] = {"the",     "letters", "fall",   "away",    "from",
                         "their",   "lines",   "and",    "return",  "again",
                         "layout",  "engine",  "words",  "measure", "glyph",
                         "cascade", "gentle",  "steady", "rhythm",  "flowing"};
  const char *cjk[] = {"文字", "雨",   "波紋", "字形", "빗물", "글자",
                       "물결", "여울", "漣漪", "文雨", "字落", "縦横"};
  const TextStyle styles[3] = {makeStyle(fontSize, kInk),
                               makeStyle(fontSize, kBlue),
                               makeStyle(fontSize, kAccent)};
  std::mt19937 randomEngine(23);
  Paragraph paragraph;
  std::string chunk;
  int chunkStyle = 0;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (wordIndex % 5 == 4)
      chunk += cjk[randomEngine() % 12];
    else
      chunk += latin[randomEngine() % 20];
    chunk += ' ';
    if (wordIndex % 120 == 119 || wordIndex + 1 == wordCount) {
      paragraph.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return paragraph;
}

// A spiky ring whose points breathe in and out independently, around an
// even-odd hole that stays open to text. Rebuilt every frame: each frame's
// path carries a fresh generation ID, so unlike the drifting donut (cached
// flattening + pathOffset) this shape exercises live re-flattening and the
// layout adapts to the changing silhouette as it morphs.
SkPath spikyRingPath(float elapsedSeconds, float radius) {
  SkPathBuilder pathBuilder;
  constexpr int kSpikes = 9;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  for (int spikeIndex = 0; spikeIndex < kSpikes; ++spikeIndex) {
    const float baseAngle = static_cast<float>(spikeIndex) * kTwoPi / kSpikes;
    const float tipRadius =
        radius *
        (0.76f + 0.24f * std::sin(elapsedSeconds * 1.7f +
                                  static_cast<float>(spikeIndex) * 2.3f));
    const SkPoint tipPoint = {tipRadius * std::cos(baseAngle),
                              tipRadius * std::sin(baseAngle)};
    const float valleyAngle = baseAngle + kTwoPi / kSpikes * 0.5f;
    const SkPoint valleyPoint = {radius * 0.5f * std::cos(valleyAngle),
                                 radius * 0.5f * std::sin(valleyAngle)};
    if (spikeIndex == 0)
      pathBuilder.moveTo(tipPoint);
    else
      pathBuilder.lineTo(tipPoint);
    pathBuilder.lineTo(valleyPoint);
  }
  pathBuilder.close();
  pathBuilder.addCircle(0, 0, radius * 0.32f);
  pathBuilder.setFillType(SkPathFillType::kEvenOdd);
  return pathBuilder.detach();
}

// ── Scene 1: exclusions & SkPath shapes ───────────────────────────────────

class ExclusionsScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Exclusions & shapes");
  }
  QString defaultText() const override {
    return QStringLiteral(
        "Typography is the craft of arranging type, and glyphs flow around "
        "obstacles the way water flows around stones. 日本語のテキストも同じ"
        "流れに乗って進み、한국어 단어들도 자연스럽게 흐르고, 中文字符同样围"
        "绕形状排布。Latin and CJK mix freely because every word is shaped "
        "once, cached, and repositioned with pure arithmetic. The orbiting "
        "circle is a classic exclusion; the pulsing spiky ring and the donut "
        "are arbitrary SkPaths — the ring is a brand-new path every frame as "
        "its points breathe in and out, and both holes stay open to text. "
        "When a shape slides across a line the line splits into fragments "
        "and each fragment justifies itself independently; when it moves on, "
        "the fragments knit back together as if nothing happened. No word is "
        "ever reshaped for any of this, frame after frame after frame. "
        "Everything you read here can be rewritten live from the panel on "
        "the left: retype the body, swap the family, drag the size — the "
        "shape cache absorbs each keystroke and the flow simply re-places "
        "the words. 形が動くたびに行は裂け、また元通りに繋がる。글자는 한 번"
        "만 성형되고 계속 재사용됩니다. The donut's hole is part of the same "
        "even-odd path, so with enough text the lines pour straight through "
        "its centre while avoiding the ring around it.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    m_body.ensure(params, defaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = params.fontSize;
    ExclusionFlow flow(
        SkRect::MakeXYWH(28, 24, canvasWidth - 56, canvasHeight - 48));

    const float circleRadius = std::min(canvasWidth, canvasHeight) * 0.13f;
    const SkPoint circleCenter = {
        canvasWidth * 0.32f +
            canvasWidth * 0.2f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.9f),
        canvasHeight * 0.38f +
            canvasHeight * 0.24f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.53f)};
    flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(
        SkRect::MakeXYWH(circleCenter.x() - circleRadius,
                         circleCenter.y() - circleRadius, 2 * circleRadius,
                         2 * circleRadius),
        fontSize * 0.5f));

    if (m_pathSize != size) {
      m_pathSize = size;
      SkPathBuilder donut;
      const float donutRadius = std::min(canvasWidth, canvasHeight) * 0.16f;
      donut.addCircle(canvasWidth * 0.72f, canvasHeight * 0.68f, donutRadius);
      donut.addCircle(canvasWidth * 0.72f, canvasHeight * 0.68f,
                      donutRadius * 0.5f);
      donut.setFillType(SkPathFillType::kEvenOdd);
      m_donut = donut.detach();
    }
    // Morphing exclusion: a brand-new spiky path every frame, points
    // pulsing in and out — the flow re-flattens and relayouts live.
    const SkPath spiky =
        spikyRingPath(static_cast<float>(elapsedSeconds),
                      std::min(canvasWidth, canvasHeight) * 0.19f);
    ExclusionFlow::Shape star =
        ExclusionFlow::Shape::fromPath(spiky, fontSize * 0.4f);
    star.pathOffset = {
        canvasWidth * 0.6f +
            canvasWidth * 0.18f *
                std::cos(static_cast<float>(elapsedSeconds) * 0.4f),
        canvasHeight * 0.3f +
            canvasHeight * 0.1f *
                std::sin(static_cast<float>(elapsedSeconds) * 0.7f)};
    flow.shapes().push_back(star);
    // The donut drifts too: moving a path via pathOffset reuses its cached
    // flattening, and every frame is a full live relayout around it.
    ExclusionFlow::Shape donut =
        ExclusionFlow::Shape::fromPath(m_donut, fontSize * 0.4f);
    donut.pathOffset = {
        canvasWidth * 0.05f *
            std::sin(static_cast<float>(elapsedSeconds) * 0.6f),
        canvasHeight * 0.06f *
            std::cos(static_cast<float>(elapsedSeconds) * 0.45f)};
    flow.shapes().push_back(donut);
    flow.setMinIntervalWidth(fontSize * 3);

    ParagraphLayoutOptions options;
    options.alignment = params.alignment;
    options.lineBreakStrategy = params.lineBreakStrategy;
    options.lineMetrics.height = fontSize * 1.7f;
    options.knuthPlass.minimumIntervalWidth = fontSize * 3;
    options.overflow.ellipsis =
        u"…"; // paste a novel: the tail is marked, not shaped

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_body.paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint ghost;
    ghost.setAntiAlias(true);
    ghost.setColor(kShape);
    canvas->drawCircle(circleCenter.x(), circleCenter.y(), circleRadius, ghost);
    canvas->save();
    canvas->translate(star.pathOffset.x(), star.pathOffset.y());
    canvas->drawPath(spiky, ghost);
    canvas->restore();
    canvas->save();
    canvas->translate(donut.pathOffset.x(), donut.pathOffset.y());
    canvas->drawPath(m_donut, ghost);
    canvas->restore();
    layout.drawBatched(canvas, m_body.paragraph);

    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  SkPath m_donut;
  SkISize m_pathSize = {0, 0};
};

// ── Scene 2: Knuth-Plass vs greedy, hyphenation, last-line modes ──────────

class KnuthPlassScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Knuth–Plass & hyphens");
  }
  QString defaultText() const override {
    // Soft hyphens (U+00AD) give both breakers discretionary break points.
    return QString::fromUtf8(
        "The para­graph breaker con­sid­ers every way to "
        "break this text into lines and picks the one with the least "
        "bad­ness, ex­act­ly like TeX. Greedy breaking "
        "com­mits line by line and leaves rag­ged, "
        "in­con­sis­tent spac­ing be­hind; "
        "op­ti­mal breaking spreads the slack across the whole "
        "para­graph in­stead, tak­ing dis­cre­tion"
        "­ary hy­phens when they pay for them­selves. The "
        "measure breathes to pose a fresh prob­lem every frame.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    const bool rebuilt = m_body.ensure(params, defaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = params.fontSize;
    // Whole-pixel measure: the breathing sine moves well under a pixel per
    // frame, so most frames pose the *same* problem as the last — quantize
    // and reuse the cached layouts instead of re-breaking 2× per frame.
    // Sub-pixel measure changes are invisible anyway.
    const float measure = std::round(
        canvasWidth * 0.36f *
        (1.0f + 0.10f * std::sin(static_cast<float>(elapsedSeconds) * 0.5f)));

    double layoutMicroseconds = 0;
    if (rebuilt || m_body.paragraph.needsShaping() ||
        measure != m_lastMeasure || size != m_lastSize ||
        params.alignment != m_lastAlignment) {
      m_lastMeasure = measure;
      m_lastSize = size;
      m_lastAlignment = params.alignment;

      ParagraphLayoutOptions options;
      options.alignment = params.alignment;
      options.lineMetrics.height = fontSize * 1.6f;
      options.hyphenation.enabled = true;
      options.knuthPlass.minimumIntervalWidth = fontSize * 3;
      options.overflow.ellipsis = u"…";

      const auto layoutStartTime = Clock::now();
      for (int pass = 0; pass < 2; ++pass) {
        options.lineBreakStrategy = pass == 0 ? LineBreakStrategy::kGreedy
                                              : LineBreakStrategy::kKnuthPlass;
        const float left =
            pass == 0 ? canvasWidth * 0.07f : canvasWidth * 0.55f;
        BlockFlow flow(SkRect::MakeXYWH(left, 48, measure, canvasHeight - 80));
        m_layouts[pass] =
            layoutParagraph(fontContext, m_body.paragraph, flow, options);
      }
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStartTime);
    }

    canvas->clear(kPaper);
    drawCaption(canvas, fontContext, "greedy", {canvasWidth * 0.07f, 18});
    drawCaption(canvas, fontContext, "knuth-plass (optimal, TeX badness)",
                {canvasWidth * 0.55f, 18});

    int runCount = 0;
    for (int pass = 0; pass < 2; ++pass) {
      const float left = pass == 0 ? canvasWidth * 0.07f : canvasWidth * 0.55f;
      runCount += static_cast<int>(m_layouts[pass].runs.size());
      SkPaint rule;
      rule.setAntiAlias(true);
      rule.setStyle(SkPaint::kStroke_Style);
      rule.setStrokeWidth(1);
      rule.setColor(0x3323252B);
      canvas->drawLine(left + measure, 44, left + measure, canvasHeight - 44,
                       rule);
      m_layouts[pass].drawBatched(canvas, m_body.paragraph);
    }
    return {layoutMicroseconds, runCount, 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  std::array<ParagraphLayout, 2> m_layouts;
  float m_lastMeasure = -1;
  SkISize m_lastSize = {0, 0};
  TextAlignment m_lastAlignment = TextAlignment::kStart;
};

// ── Scene 3: infinite loop marquee on a closed figure-eight ───────────────

class LoopScene final : public Scene {
public:
  QString name() const override { return QStringLiteral("Infinite loop"); }
  QString defaultText() const override {
    return QStringLiteral("and the words go round 終わらない文字の環 끝나지 "
                          "않는 글의 고리 文字環繞不息 round and round again "
                          "— ")
        .repeated(4);
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    m_body.ensure(params, defaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    if (m_size != size) {
      m_size = size;
      SkPathBuilder pathBuilder;
      const SkPoint center = {canvasWidth * 0.5f, canvasHeight * 0.5f};
      const int steps = 400;
      for (int stepIndex = 0; stepIndex <= steps; ++stepIndex) {
        const float angle = static_cast<float>(stepIndex) / steps * 2.0f *
                            std::numbers::pi_v<float>;
        const SkPoint point = {
            center.x() + canvasWidth * 0.4f * std::sin(angle),
            center.y() +
                canvasHeight * 0.78f * std::sin(angle) * std::cos(angle)};
        if (stepIndex == 0)
          pathBuilder.moveTo(point);
        else
          pathBuilder.lineTo(point);
      }
      pathBuilder.close();
      m_eight = pathBuilder.detach();
      SkContourMeasureIter contourIterator(m_eight, false);
      m_contour = contourIterator.next();
    }
    if (!m_contour)
      return {};
    const float loopLength = m_contour->length();

    LineSetFlow flow;
    LineInterval interval;
    interval.contour = m_contour;
    interval.length = loopLength;
    interval.contourStart =
        std::fmod(static_cast<float>(elapsedSeconds) * 110.0f, loopLength);
    flow.lines().push_back({interval});

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_body.paragraph, flow);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint rail;
    rail.setAntiAlias(true);
    rail.setStyle(SkPaint::kStroke_Style);
    rail.setStrokeWidth(1.2f);
    rail.setColor(0x2A23252B);
    canvas->drawPath(m_eight, rail);
    layout.draw(canvas, m_body.paragraph);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  SkPath m_eight;
  sk_sp<SkContourMeasure> m_contour;
  SkISize m_size = {0, 0};
};

// ── Scene 4: letter rain on an umbrella (full-paragraph relayout) ─────────

class RainScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Letter rain — 700 words");
  }
  bool supportsTextEdit() const override { return false; }

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

// ── Scene 5: ripple pool (click to drop) ──────────────────────────────────

class RippleScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Ripple pool — click me");
  }
  bool supportsTextEdit() const override { return false; }

  void pointerPress(SkPoint position) override {
    m_pendingDrops.push_back(position);
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double /*elapsedSeconds*/,
                    int frameNumber, const SceneParams & /*params*/,
                    FontContext &fontContext) override {
    if (m_paragraph.text().empty())
      m_paragraph = makeBigParagraph(700, 13.0f);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    if (frameNumber % 90 == 0)
      m_drops.push_back(
          {{60.0f + static_cast<float>(
                        m_randomEngine() %
                        std::max(1, static_cast<int>(canvasWidth) - 120)),
            60.0f + static_cast<float>(
                        m_randomEngine() %
                        std::max(1, static_cast<int>(canvasHeight) - 120))},
           frameNumber});
    for (const SkPoint &pendingPosition : m_pendingDrops)
      m_drops.push_back({pendingPosition, frameNumber});
    m_pendingDrops.clear();
    std::erase_if(m_drops, [&](const Drop &drop) {
      return frameNumber - drop.birthFrameNumber > 280;
    });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    static const char *swaps[] = {"letters", "glyphs ", "symbols", "strokes"};
    if (frameNumber > 0 && frameNumber % 150 == 0) {
      const size_t textOffset = m_paragraph.text().find(u"letters");
      if (textOffset != std::u16string::npos)
        m_paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                static_cast<uint32_t>(textOffset + 7),
                                swaps[(frameNumber / 150) % 4]);
    }

    BlockFlow flow(
        SkRect::MakeXYWH(36, 24, canvasWidth - 72, canvasHeight - 48));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 21;

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    m_batches.clear();
    forEachPlacedGlyph(
        layout, m_paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          SkVector offset = {0, 0};
          float tilt = 0;
          for (const Drop &drop : m_drops) {
            const SkVector radialVector = restingOrigin - drop.center;
            const float distance = radialVector.length() + 1.0f;
            const float ringRadius =
                6.0f * static_cast<float>(frameNumber - drop.birthFrameNumber);
            const float normalizedRingDistance =
                (distance - ringRadius) / 46.0f;
            if (normalizedRingDistance > 3 || normalizedRingDistance < -3)
              continue;
            const float amplitude =
                42.0f * std::exp(-static_cast<float>(frameNumber -
                                                     drop.birthFrameNumber) /
                                 150.0f);
            const float pulse = amplitude * std::exp(-normalizedRingDistance *
                                                     normalizedRingDistance);
            offset += radialVector * (pulse / distance);
            tilt +=
                pulse * 0.012f * (normalizedRingDistance < 0 ? -1.0f : 1.0f);
          }
          float cosine;
          float sine;
          quantizeAngle(tilt, cosine, sine);
          const SkPoint drawingOrigin = restingOrigin + offset;
          const float halfAdvance = glyphAdvance * 0.5f;
          GlyphRSXformBatches::Batch &batch =
              m_batches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back(
              {cosine, sine,
               drawingOrigin.fX - cosine * halfAdvance + halfAdvance,
               drawingOrigin.fY - sine * halfAdvance});
        });

    canvas->clear(kPaper);
    const int drawnGlyphCount = m_batches.draw(canvas);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), drawnGlyphCount};
  }

private:
  struct Drop {
    SkPoint center;
    int birthFrameNumber;
  };
  Paragraph m_paragraph;
  std::vector<Drop> m_drops;
  std::vector<SkPoint> m_pendingDrops;
  GlyphRSXformBatches m_batches;
  std::mt19937 m_randomEngine{31};
};

// ── Scene 6: vertical CJK with ruby, kenten, tate-chu-yoko ────────────────

class VerticalScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Vertical CJK — ruby · kenten · 縦中横");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams & /*params*/,
                    FontContext &fontContext) override {
    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize =
        std::clamp(std::min(canvasWidth, canvasHeight) / 24.0f, 16.0f, 30.0f);
    if (!m_mincho) {
      m_mincho = fontContext.fontManager()->matchFamilyStyle(
          "Hiragino Mincho ProN", SkFontStyle());
      if (!m_mincho)
        m_mincho = fontContext.defaultTypeface();
    }
    if (fontSize != m_fontSize) {
      m_fontSize = fontSize;
      buildParagraphs(fontSize);
    }

    canvas->clear(kPaper);

    // Vertical-rl block on the right.
    VerticalBlockFlow verticalFlow(SkRect::MakeXYWH(
        canvasWidth * 0.42f, 30, canvasWidth * 0.55f, canvasHeight - 90));
    ParagraphLayoutOptions verticalOptions;
    verticalOptions.lineMetrics.height = fontSize * 1.9f;
    const auto layoutStartTime = Clock::now();
    ParagraphLayout verticalLayout = layoutParagraph(
        fontContext, m_verticalParagraph, verticalFlow, verticalOptions);
    const auto layoutEndTime = Clock::now();
    verticalLayout.draw(canvas, m_verticalParagraph);

    // Ruby + kenten are external utilities over the placed runs.
    rubyVertical(canvas, fontContext, verticalLayout, u"縦組", "たてぐみ");
    rubyVertical(canvas, fontContext, verticalLayout, u"文章", "ぶんしょう");
    rubyVertical(canvas, fontContext, verticalLayout, u"対応", "たいおう");
    kentenVertical(
        canvas, verticalLayout, u"上から下へ",
        0.6f + 0.4f * static_cast<float>(std::sin(elapsedSeconds * 2.0)));

    // Horizontal comparison block on the left.
    BlockFlow horizontalFlow(SkRect::MakeXYWH(
        30, canvasHeight * 0.14f, canvasWidth * 0.34f, canvasHeight * 0.7f));
    ParagraphLayoutOptions horizontalOptions;
    horizontalOptions.lineMetrics.height = fontSize * 1.9f;
    ParagraphLayout horizontalLayout = layoutParagraph(
        fontContext, m_horizontalParagraph, horizontalFlow, horizontalOptions);
    horizontalLayout.draw(canvas, m_horizontalParagraph);
    rubyHorizontal(canvas, fontContext, horizontalLayout, u"漢字", "かんじ");
    rubyHorizontal(canvas, fontContext, horizontalLayout, u"圏点", "けんてん");

    drawCaption(canvas, fontContext,
                "vertical-rl: UTR#50 orientation, 'vert' forms, "
                "tate-chu-yoko digits, ruby + kenten on top",
                {30, canvasHeight - 28}, canvasWidth - 60);

    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(verticalLayout.runs.size() +
                             horizontalLayout.runs.size()),
            0};
  }

private:
  /// Creates a Japanese style with the requested vertical glyph behavior.
  TextStyle
  japaneseStyle(float fontSize,
                VerticalForm verticalForm = VerticalForm::kAuto) const {
    TextStyle style = makeStyle(fontSize, kInk, "ja", m_mincho);
    style.shaping.verticalForm = verticalForm;
    return style;
  }

  /// Rebuilds both comparison paragraphs for a new display size.
  void buildParagraphs(float fontSize) {
    m_verticalParagraph.clear();
    m_verticalParagraph.appendText("縦組みの文章は、上から下へ、",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.appendText("右から左へと流れる。平成",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.appendText(
        "31", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
    m_verticalParagraph.appendText("年", japaneseStyle(fontSize));
    m_verticalParagraph.appendText(
        "12", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
    m_verticalParagraph.appendText("月、", japaneseStyle(fontSize));
    m_verticalParagraph.appendText("TextFlow", japaneseStyle(fontSize));
    m_verticalParagraph.appendText("は縦書きに対応した。",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.setWritingMode(WritingMode::kVerticalRL);

    m_horizontalParagraph.clear();
    m_horizontalParagraph.appendText(
        "漢字にルビを振ると、誰でも読みやすい。強調したい語には圏点を打つ。",
        japaneseStyle(fontSize * 0.8f));
  }

  /// Places a ruby annotation alongside the first matching vertical range.
  void rubyVertical(SkCanvas *canvas, FontContext &fontContext,
                    const ParagraphLayout &layout, std::u16string_view baseText,
                    const char *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_verticalParagraph, baseText);
    if (matches.empty())
      return;
    bool valid = false;
    int line = 0;
    SkPoint origin = {0, 0};
    float rangeBegin = 0;
    float rangeEnd = 0;
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_verticalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start ||
          word.textBegin >= matches[0].end || run.transformed)
        continue;
      if (!valid) {
        valid = true;
        line = run.lineIndex;
        origin = run.origin;
        rangeBegin = 0;
        rangeEnd = word.width;
      } else if (run.lineIndex == line) {
        const float offset = run.origin.y() - origin.y();
        rangeBegin = std::min(rangeBegin, offset);
        rangeEnd = std::max(rangeEnd, offset + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, japaneseStyle(m_fontSize * 0.5f));
    ruby.setWritingMode(WritingMode::kVerticalRL);
    const float length = ruby.naturalWidth(fontContext);
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {origin.x() + m_fontSize * 0.62f,
         origin.y() + (rangeBegin + rangeEnd) * 0.5f - length * 0.5f},
        {0, 1},
        length + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  }

  /// Draws emphasis dots beside matching glyphs in the vertical paragraph.
  void kentenVertical(SkCanvas *canvas, const ParagraphLayout &layout,
                      std::u16string_view emphasis, float alpha) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_verticalParagraph, emphasis);
    if (matches.empty())
      return;
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    dot.setAlphaf(alpha);
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_verticalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start ||
          word.textBegin >= matches[0].end || run.transformed ||
          !run.shaped->vertical)
        continue;
      float penAdvance = 0;
      const ShapedWord &shapedWord = *run.shaped;
      for (size_t glyphIndex = 0; glyphIndex < shapedWord.advances.size();
           ++glyphIndex) {
        const uint32_t textOffset =
            word.textBegin + shapedWord.clusters[glyphIndex];
        if (textOffset >= matches[0].start && textOffset < matches[0].end)
          canvas->drawCircle(run.origin.x() + m_fontSize * 0.60f,
                             run.origin.y() + penAdvance +
                                 shapedWord.advances[glyphIndex] * 0.5f,
                             m_fontSize * 0.07f, dot);
        penAdvance += shapedWord.advances[glyphIndex];
      }
    }
  }

  /// Places a ruby annotation above the first matching horizontal range.
  void rubyHorizontal(SkCanvas *canvas, FontContext &fontContext,
                      const ParagraphLayout &layout,
                      std::u16string_view baseText, const char *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_horizontalParagraph, baseText);
    if (matches.empty())
      return;
    float rangeLeft = 0;
    float rangeRight = 0;
    float baseline = 0;
    bool valid = false;
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_horizontalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start || word.textBegin >= matches[0].end)
        continue;
      if (!valid) {
        valid = true;
        rangeLeft = run.origin.x();
        rangeRight = run.origin.x() + word.width;
        baseline = run.origin.y();
      } else if (run.origin.y() == baseline) {
        rangeRight = std::max(rangeRight, run.origin.x() + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, japaneseStyle(m_fontSize * 0.4f));
    const float width = ruby.naturalWidth(fontContext);
    LineSetFlow flow;
    flow.lines().push_back(
        {LineInterval{{(rangeLeft + rangeRight) * 0.5f - width * 0.5f,
                       baseline - m_fontSize * 0.92f},
                      {1, 0},
                      width + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  }

  Paragraph m_verticalParagraph;
  Paragraph m_horizontalParagraph;
  sk_sp<SkTypeface> m_mincho;
  float m_fontSize = 0;
};

// ── Scene 7: query layer — regex markers that follow live edits ───────────

class MarkersScene final : public Scene {
public:
  QString name() const override { return QStringLiteral("Query & markers"); }
  QString defaultText() const override {
    return QStringLiteral(
        "Captain Ada watches the Beacon while Turing recalibrates the "
        "Lattice. Every capitalized Word in this paragraph was found once by "
        "an ICU regex and registered as a named marker set; the highlights "
        "you see are plain setPaint calls over those ranges. The paragraph "
        "keeps editing itself — words swap in and out below the markers — "
        "and the ranges follow the edits through the Paragraph's revision "
        "log, exactly like DOM Range objects. Delete the Beacon and its "
        "marker collapses; retype it and a fresh query picks it up again.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int frameNumber, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    if (m_body.ensure(params, defaultText(), m_serif)) {
      // Scoped query: only search the window the box can actually place
      // (the frontier of the previous layout). Paste a novel and the regex
      // scans the visible text, not megabytes that will never land — and
      // the marker set stays small, so the per-frame repaint below stays
      // cheap too.
      const uint32_t textLength =
          static_cast<uint32_t>(m_body.paragraph.text().size());
      const CharRange scope{0, m_placedTextEnd > 0
                                   ? std::min(m_placedTextEnd, textLength)
                                   : textLength};
      m_queryWasScoped = scope.end < textLength;
      m_markers = MarkerSet(m_body.paragraph);
      m_markers.setRanges("caps", findRegexMatches(m_body.paragraph,
                                                   "\\b\\p{Lu}\\p{Ll}+", scope)
                                      .value_or(std::vector<CharRange>{}));
    }

    // Scripted live edits: swap a word every ~2.5s; markers ride along.
    static const char *cycle[] = {"watches", "guards ", "studies", "shadows"};
    if (frameNumber > 0 && frameNumber % 150 == 0) {
      for (const char *word : cycle) {
        const size_t textOffset = m_body.paragraph.text().find(
            std::u16string(word, word + 7).c_str());
        if (textOffset != std::u16string::npos) {
          m_body.paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                       static_cast<uint32_t>(textOffset + 7),
                                       cycle[(frameNumber / 150) % 4]);
          break;
        }
      }
    }

    // Hue-cycling highlight over every marker range — paint-only restyle,
    // zero reshaping.
    PaintStyle highlight;
    const SkScalar hueSaturationValue[3] = {
        std::fmod(static_cast<float>(elapsedSeconds) * 40.0f, 360.0f), 0.75f,
        0.72f};
    highlight.color = SkHSVToColor(hueSaturationValue);
    m_markers.applyPaint(m_body.paragraph, "caps", highlight);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const SkRect box = SkRect::MakeXYWH(canvasWidth * 0.1f, 40,
                                        canvasWidth * 0.8f, canvasHeight - 80);

    // The layout is memoized: the hue cycle repaints the same ranges every
    // frame, which leaves the span structure (and so every run's styleIndex)
    // untouched — draws resolve the new color live from the paragraph, no
    // relayout needed. Only real changes (edits, controls, resize, a marker
    // set whose boundaries moved) re-break lines, so a screenful of 10k
    // words costs its layout once, not 60× a second.
    double layoutMicroseconds = 0;
    if (m_body.paragraph.needsShaping() ||
        m_body.paragraph.revision() != m_lastRevision || size != m_lastSize ||
        params.alignment != m_lastAlignment ||
        params.lineBreakStrategy != m_lastLineBreakStrategy ||
        m_layout.runs.empty()) {
      m_lastRevision = m_body.paragraph.revision();
      m_lastSize = size;
      m_lastAlignment = params.alignment;
      m_lastLineBreakStrategy = params.lineBreakStrategy;

      BlockFlow flow(box);
      ParagraphLayoutOptions options;
      options.alignment = params.alignment;
      options.lineBreakStrategy = params.lineBreakStrategy;
      options.lineMetrics.height = params.fontSize * 1.8f;

      const auto layoutStartTime = Clock::now();
      m_layout = layoutParagraph(fontContext, m_body.paragraph, flow, options);
      layoutMicroseconds = toMicroseconds(Clock::now() - layoutStartTime);

      // Remember how much text actually landed: the next re-query (text
      // edit) scopes itself to this window.
      m_placedTextEnd =
          m_layout.overflowed()
              ? m_body.paragraph.words()[m_layout.firstUnplacedWord].textBegin
              : static_cast<uint32_t>(m_body.paragraph.text().size());
    }

    canvas->clear(kPaper);
    // The breaker already stops placing words once the box is full, but a
    // hard clip guards the last visible line's descenders/overshoot too —
    // type or paste past capacity and the box still never bleeds into the
    // caption below.
    canvas->save();
    canvas->clipRect(box);
    m_layout.drawBatched(canvas, m_body.paragraph);
    canvas->restore();

    drawCaption(canvas, fontContext,
                m_queryWasScoped
                    ? "regex \\b\\p{Lu}\\p{Ll}+ → MarkerSet; query scoped to "
                      "the placed window — overflow text is never scanned"
                    : "regex \\b\\p{Lu}\\p{Ll}+ → MarkerSet; ranges follow "
                      "the scripted edits",
                {canvasWidth * 0.1f, canvasHeight - 30}, canvasWidth * 0.8f);
    return {layoutMicroseconds, static_cast<int>(m_layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  MarkerSet m_markers;
  sk_sp<SkTypeface> m_serif;
  ParagraphLayout m_layout; // memoized across frames (see render)
  uint64_t m_lastRevision = ~0ull;
  SkISize m_lastSize = {0, 0};
  TextAlignment m_lastAlignment = TextAlignment::kStart;
  LineBreakStrategy m_lastLineBreakStrategy = LineBreakStrategy::kGreedy;
  uint32_t m_placedTextEnd = 0; // Text frontier of last layout (0 = unknown).
  bool m_queryWasScoped = false;
};

// ── Scene 8: inline placeholders — pills and figures woven into the flow ──

class SlotsScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Inline slots & pills");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif) {
      m_serif = defaultSerif(fontContext);
      m_sansTypeface = fontContext.fontManager()->matchFamilyStyle(
          "Noto Sans", SkFontStyle());
      if (!m_sansTypeface)
        m_sansTypeface = fontContext.defaultTypeface();
    }
    const float fontSize = params.fontSize;
    if (m_builtFontSize != fontSize) {
      m_builtFontSize = fontSize;
      build(fontContext, fontSize);
    }

    // Pulse the first pill — resizing an inline object relayouts the whole
    // paragraph live, and reshapes exactly zero words.
    m_paragraph.setPlaceholder(
        0, {m_pillWidths[0] *
                (1.0f + 0.4f * (0.5f + 0.5f * static_cast<float>(std::sin(
                                                  elapsedSeconds * 1.7)))),
            fontSize * 1.35f, fontSize * 0.3f});

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    BlockFlow flow(SkRect::MakeXYWH(canvasWidth * 0.1f, 44, canvasWidth * 0.8f,
                                    canvasHeight - 90));
    ParagraphLayoutOptions options;
    options.alignment = params.alignment;
    options.lineBreakStrategy = params.lineBreakStrategy;
    options.lineMetrics.height = fontSize * 2.1f;

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, m_paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    layout.drawBatched(canvas, m_paragraph);

    // Draw whatever belongs in each slot: pills for the first four, a
    // little inline "figure" for the last.
    const SkFont pillFont = makeFont(m_sansTypeface, fontSize * 0.68f);
    SkFontMetrics pillMetrics;
    pillFont.getMetrics(&pillMetrics);
    for (const ParagraphLayout::PlacedPlaceholder &placed :
         layout.placeholderRects(m_paragraph)) {
      if (placed.index < static_cast<int>(m_pillTexts.size())) {
        SkPaint backgroundPaint;
        backgroundPaint.setAntiAlias(true);
        backgroundPaint.setColor(placed.index == 0 ? kAccent : kBlue);
        canvas->drawRoundRect(placed.rect, placed.rect.height() * 0.5f,
                              placed.rect.height() * 0.5f, backgroundPaint);
        Paragraph &label = m_pillLabelParagraphs.paragraphFor(
            m_pillTexts[static_cast<size_t>(placed.index)], m_sansTypeface,
            fontSize * 0.68f);
        const float textWidth = label.naturalWidth(fontContext);
        PaintStyle white;
        white.color = SK_ColorWHITE;
        layoutSingleLine(
            fontContext, label,
            {placed.rect.centerX() - textWidth * 0.5f,
             placed.rect.centerY() -
                 (pillMetrics.fAscent + pillMetrics.fDescent) * 0.5f})
            .draw(canvas, label, &white);
      } else {
        SkPaint fill;
        fill.setAntiAlias(true);
        fill.setColor(kShape);
        canvas->drawRoundRect(placed.rect, 6, 6, fill);
        SkPaint line;
        line.setAntiAlias(true);
        line.setStyle(SkPaint::kStroke_Style);
        line.setStrokeWidth(2);
        line.setColor(kAccent);
        // A tiny sparkline so the "figure" reads as content.
        SkPathBuilder spark;
        for (int pointIndex = 0; pointIndex <= 16; ++pointIndex) {
          const float pointX =
              placed.rect.left() + placed.rect.width() * pointIndex / 16.0f;
          const float pointY = placed.rect.centerY() -
                               placed.rect.height() * 0.3f *
                                   std::sin(pointIndex * 0.7f +
                                            static_cast<float>(elapsedSeconds));
          if (pointIndex == 0)
            spark.moveTo(pointX, pointY);
          else
            spark.lineTo(pointX, pointY);
        }
        canvas->drawPath(spark.detach(), line);
      }
    }

    drawCaption(canvas, fontContext,
                "appendPlaceholder() weaves fixed-size slots into the flow; "
                "placeholderRects() reports where they landed",
                {canvasWidth * 0.1f, canvasHeight - 30}, canvasWidth * 0.8f);
    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(layout.runs.size()), 0};
  }

private:
  /// Rebuilds the paragraph and measures every inline pill label.
  void build(FontContext &fontContext, float fontSize) {
    m_pillTexts = {"LOW RISK", "42 ms", "β-channel", "cache-hot"};
    m_pillWidths.clear();
    for (const char *text : m_pillTexts) {
      Paragraph &label = m_pillLabelParagraphs.paragraphFor(
          text, m_sansTypeface, fontSize * 0.68f);
      m_pillWidths.push_back(label.naturalWidth(fontContext) + fontSize * 1.1f);
    }

    m_paragraph.clear();
    const TextStyle body = makeStyle(fontSize, kInk, "", m_serif);
    const Placeholder pill{0, fontSize * 1.35f, fontSize * 0.3f};
    auto addPill = [&](size_t pillIndex) {
      Placeholder placeholder = pill;
      placeholder.width = m_pillWidths[pillIndex];
      m_paragraph.appendPlaceholder(placeholder, body);
    };
    m_paragraph.appendText("Status pills ride the baseline like words — ",
                           body);
    addPill(0);
    m_paragraph.appendText(
        " — and the flow simply makes room: it wraps them, justifies around "
        "them, and re-places them when they resize. Round-trip latency ",
        body);
    addPill(1);
    m_paragraph.appendText(" stays within budget, the experiment on the ",
                           body);
    addPill(2);
    m_paragraph.appendText(" keeps converging, and every relayout here is ",
                           body);
    addPill(3);
    m_paragraph.appendText(
        ". Even a small figure can sit inline without leaving the "
        "paragraph ",
        body);
    // Sized to the line: lines don't grow per-box yet (see README
    // limitations), so a slot taller than the leading would overlap the
    // previous line.
    m_paragraph.appendPlaceholder(
        {fontSize * 7.0f, fontSize * 1.8f, fontSize * 0.3f}, body);
    m_paragraph.appendText(
        " because the breaker treats every slot as an unbreakable word — "
        "the object-replacement character anchors it in the text, and the "
        "layout reports the rect where it landed.",
        body);
  }

  Paragraph m_paragraph;
  SingleLineParagraphCache m_pillLabelParagraphs;
  std::vector<const char *> m_pillTexts;
  std::vector<float> m_pillWidths;
  sk_sp<SkTypeface> m_serif;
  sk_sp<SkTypeface> m_sansTypeface;
  float m_builtFontSize = 0;
};

// ── Scene 9: overflow & ellipsis — CSS text-overflow semantics ───────────

class OverflowScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Overflow & ellipsis");
  }
  QString defaultText() const override {
    return QStringLiteral(
        "The box below breathes between comfortably containing this "
        "paragraph and cutting it well short. The left pane simply stops "
        "wherever the last word happens to fit; the right pane trims its "
        "final line until a shaped ellipsis lands cleanly at the edge — "
        "CSS text-overflow semantics, except the marker is a real glyph run "
        "instead of a string glued onto the end. Nothing here reshapes: the "
        "same cached words are just placed into a shorter or taller measure "
        "as the box resizes, frame after frame.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    m_body.ensure(params, defaultText(), m_serif);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize = params.fontSize;
    const float breathe =
        0.5f + 0.5f * static_cast<float>(std::sin(elapsedSeconds * 0.4));
    const float maximumBoxHeight =
        std::max(canvasHeight - 140.0f, fontSize * 3.0f);
    const float boxHeight =
        fontSize * 2.4f +
        std::max(0.0f, maximumBoxHeight - fontSize * 2.4f) * breathe;
    const float paneWidth = canvasWidth * 0.4f;

    canvas->clear(kPaper);
    drawCaption(canvas, fontContext, "clipped — no ellipsis",
                {canvasWidth * 0.06f, 18});
    drawCaption(canvas, fontContext, "options.overflow.ellipsis = \"…\"",
                {canvasWidth * 0.54f, 18});

    double layoutMicroseconds = 0;
    int runCount = 0;
    QString status[2];
    for (int pass = 0; pass < 2; ++pass) {
      const float left = pass == 0 ? canvasWidth * 0.06f : canvasWidth * 0.54f;
      const SkRect box = SkRect::MakeXYWH(left, 48, paneWidth, boxHeight);

      BlockFlow flow(box);
      ParagraphLayoutOptions options;
      options.alignment = params.alignment;
      options.lineBreakStrategy = params.lineBreakStrategy;
      options.lineMetrics.height = fontSize * 1.5f;
      if (pass == 1)
        options.overflow.ellipsis = u"…";

      const auto layoutStartTime = Clock::now();
      ParagraphLayout layout =
          layoutParagraph(fontContext, m_body.paragraph, flow, options);
      layoutMicroseconds += toMicroseconds(Clock::now() - layoutStartTime);
      runCount += static_cast<int>(layout.runs.size());

      SkPaint border;
      border.setAntiAlias(true);
      border.setStyle(SkPaint::kStroke_Style);
      border.setStrokeWidth(1.2f);
      border.setColor(layout.overflowed() ? kAccent : 0x3323252B);
      canvas->drawRect(box, border);
      layout.drawBatched(canvas, m_body.paragraph);

      if (layout.overflowed()) {
        const int unplacedWordCount = static_cast<int>(
            m_body.paragraph.words().size() - layout.firstUnplacedWord);
        const QString word = unplacedWordCount == 1 ? QStringLiteral("word")
                                                    : QStringLiteral("words");
        status[pass] = QStringLiteral("overflowed — %1 %2 cut%3")
                           .arg(unplacedWordCount)
                           .arg(word)
                           .arg(pass == 1 && layout.ellipsized
                                    ? QStringLiteral(", ellipsis drawn")
                                    : QString());
      } else {
        status[pass] = QStringLiteral("fits — nothing cut");
      }
    }

    drawCaption(canvas, fontContext, status[0].toUtf8().constData(),
                {canvasWidth * 0.06f, 48 + boxHeight + 22}, paneWidth);
    drawCaption(canvas, fontContext, status[1].toUtf8().constData(),
                {canvasWidth * 0.54f, 48 + boxHeight + 22}, paneWidth);

    return {layoutMicroseconds, runCount, 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
};

} // namespace

std::vector<std::unique_ptr<Scene>> makeScenes() {
  std::vector<std::unique_ptr<Scene>> scenes;
  scenes.push_back(std::make_unique<ExclusionsScene>());
  scenes.push_back(std::make_unique<KnuthPlassScene>());
  scenes.push_back(std::make_unique<LoopScene>());
  scenes.push_back(std::make_unique<RainScene>());
  scenes.push_back(std::make_unique<RippleScene>());
  scenes.push_back(std::make_unique<VerticalScene>());
  scenes.push_back(std::make_unique<MarkersScene>());
  scenes.push_back(std::make_unique<SlotsScene>());
  scenes.push_back(std::make_unique<OverflowScene>());
  return scenes;
}

} // namespace gallery
