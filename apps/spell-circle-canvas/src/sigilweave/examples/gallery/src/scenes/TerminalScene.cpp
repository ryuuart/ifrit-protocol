// Scene: retro terminal — per-glyph typewriter reveal with per-glyph shading.
//
// The stress test this scene answers: can a caller animate a proportional-font
// paragraph in letter by letter — each glyph with its own brightness, glow,
// and glitch — on the public API alone? It extends the Rain/Ripple
// choreography recipe one step: walk the placed runs in logical order, derive
// a deterministic reveal time per glyph from that order (typing jitter plus
// word and newline pauses, all in "character units" so one clock scrub replays
// the whole tape), then instead of GlyphRSXformBatches' flat-color draw,
// bucket glyphs by quantized (brightness, fade) and draw every bucket twice:
// a blur-mask phosphor glow pass under a crisp core pass, with scene-owned
// SkPaints. Glitched glyphs leave those buckets for RGB-split ones. At the
// end of each loop the tape dissolves: every glyph scrambles through random
// substitutes drawn from its typeface's own placed-glyph pool while its
// alpha fades to nothing — per-glyph *identity* control on top of position
// and shading. The library only ever laid the paragraph out; everything
// per-glyph is a draw-loop concern.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <include/core/SkColor.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace sigil::weave;

namespace gallery {

namespace {

QString terminalDefaultText() {
  return QStringLiteral(
      "IFRIT PROTOCOL v3.7 — SIGIL TERMINAL\n"
      "> boot sequence initiated\n"
      "> binding daemons: icu · harfbuzz · skia ... ok\n"
      "> shape cache warm; summoning circle locked at 60 Hz\n"
      "\n"
      "The lattice accepts proportional glyphs now. The old monospace wards "
      "are gone: every letter arrives alone, measured by its own advance, "
      "flares white-hot for an instant, then settles into green phosphor. "
      "Edit this text and the tape retypes itself.\n"
      "\n"
      "> query: reveal the flow one cluster at a time?\n"
      "> affirmative. watch the letters land.");
}

// Low-bias integer hash → [0, 1): every random-looking choice in this scene
// (typing jitter, shimmer phase, glitch bursts) is a pure function of glyph
// index and time slice, so pausing, scrubbing, and looping stay coherent.
float hash01(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return static_cast<float>(value) * (1.0f / 4294967296.0f);
}

constexpr int kBrightnessLevels = 24;
constexpr int kFadeLevels = 16;
constexpr float kSteadyBrightness = 0.62f; // where the reveal flash decays to

// Core fill for one quantized brightness: dim green floor → phosphor green →
// white-hot in the top quarter (freshly revealed glyphs).
SkColor phosphorColor(float t) {
  const float white = std::clamp((t - 0.72f) / 0.28f, 0.0f, 1.0f);
  const auto channel = [](float v) {
    return static_cast<U8CPU>(std::clamp(v, 0.0f, 255.0f));
  };
  return SkColorSetARGB(255, channel(14 + 96 * t + 145 * white),
                        channel(68 + 187 * t),
                        channel(34 + 82 * t + 139 * white));
}

class TerminalScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int frameNumber, const SceneParams &params,
                    FontContext &fontContext) override {
    m_body.ensure(params, terminalDefaultText(), fontContext.defaultTypeface());
    Paragraph &paragraph = m_body.paragraph;

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const SkRect box = SkRect::MakeXYWH(48, 36, canvasWidth - 96,
                                        canvasHeight - 84);

    // Terminal output is left-ragged by nature, so alignment stays kStart;
    // the breaker choice still comes from the panel. Layout is memoized —
    // the reveal never touches it, only the draw loop below.
    double layoutMicroseconds = 0;
    m_layoutGuard.ensure(
        paragraph, {size, params.lineBreakStrategy, params.fontSize}, [&] {
          BlockFlow flow(box);
          ParagraphLayoutOptions options;
          options.alignment = TextAlignment::kStart;
          options.lineBreakStrategy = params.lineBreakStrategy;
          options.lineMetrics.height = params.fontSize * 1.55f;

          const kit::Stopwatch layoutTime;
          m_layout = layoutParagraph(fontContext, paragraph, flow, options);
          layoutMicroseconds = layoutTime.microseconds();
          rebuildGlyphPools(paragraph);
        });

    const float charsPerSecond =
        std::max(1.0f, params.floatValue(QStringLiteral("speed"), 60.0f));
    const float glowAmount = params.floatValue(QStringLiteral("glow"), 0.65f);
    const float glitchAmount =
        params.floatValue(QStringLiteral("glitch"), 0.35f);

    // The tape clock, in character units. One loop: type the schedule, hold
    // with a blinking cursor, dissolve (every glyph scrambles then fades on
    // its own staggered clock), sit dark for a beat, retype.
    const double holdChars = 3.0 * charsPerSecond;
    const double staggerChars = 1.2 * charsPerSecond;   // dissolve start spread
    const double dissolveChars = 1.05 * charsPerSecond; // scramble + fade span
    const double darkChars = 0.8 * charsPerSecond;
    const double dissolveStart = m_scheduleChars + holdChars;
    double elapsedChars = elapsedSeconds * charsPerSecond;
    if (m_scheduleChars > 0)
      elapsedChars = std::fmod(elapsedChars, dissolveStart + staggerChars +
                                                 dissolveChars + darkChars);

    // Mains-hum flicker shared by every glyph; per-glyph shimmer is added in
    // the walk below.
    const float flicker =
        1.0f + 0.018f * std::sin(static_cast<float>(elapsedSeconds) * 37.0f) +
        0.03f * (hash01(static_cast<uint32_t>(frameNumber)) - 0.5f);

    // Glitch bursts live on an ~85ms grid: a burst picks a horizontal tear
    // band (every glyph inside shears sideways) and a deterministic sample of
    // glyphs to knock into the RGB-split buckets.
    const auto slice = static_cast<uint32_t>(elapsedSeconds / 0.085);
    const bool burst = hash01(slice * 2246822519u + 3) < glitchAmount * 0.24f;
    const float tearCenterY = hash01(slice * 3266489917u + 7) * canvasHeight;
    const float tearShift =
        (hash01(slice * 668265263u + 11) - 0.5f) * 40.0f * glitchAmount;

    m_buckets.clear();
    m_splitBuckets.clear();
    m_segmentCounters.assign(paragraph.words().size(), 0);

    // The reveal walk: runs arrive in logical word order, so accumulating
    // per-glyph typing jitter plus word/newline pauses *is* the schedule —
    // recomputed from scratch each frame, no per-glyph state to keep in sync
    // with edits.
    double revealAt = 8.0; // boot pause before the first character
    uint32_t previousWordIndex = ~0u;
    uint32_t glyphIndex = 0;
    int drawnGlyphs = 0;
    bool cursorPlaced = false;
    SkPoint cursorPen = {box.left(), box.top() + params.fontSize};
    float cursorFontSize = params.fontSize;
    for (const PositionedRun &run : m_layout.runs) {
      const Word &word = paragraph.words()[run.wordIndex];
      if (word.segments.empty())
        continue;
      if (previousWordIndex != ~0u && run.wordIndex != previousWordIndex)
        revealAt += paragraph.words()[previousWordIndex].mandatoryBreakAfter
                        ? 14.0  // carriage return: the beat between lines
                        : 1.2;  // the space bar
      previousWordIndex = run.wordIndex;
      const WordSegment &segment =
          word.segments[m_segmentCounters[run.wordIndex]++ %
                        word.segments.size()];
      const ShapedWord &shaped = *segment.shaped;
      const std::vector<SkGlyphID> &scramblePool =
          m_glyphPools.bucketFor(shaped.typeface.get()).glyphs;
      for (size_t glyph = 0; glyph < shaped.glyphs.size(); ++glyph) {
        const uint32_t index = glyphIndex++;
        revealAt += 0.55 + 1.1 * hash01(index * 2654435761u + 17);
        const SkPoint rest = run.origin + shaped.positions[glyph];
        if (!cursorPlaced) { // first glyph anchors the cursor pre-typing
          cursorPen = rest;
          cursorFontSize = shaped.fontSize;
          cursorPlaced = true;
        }
        if (revealAt > elapsedChars)
          continue; // not typed yet
        cursorPen = {rest.fX + shaped.advances[glyph], rest.fY};
        cursorFontSize = shaped.fontSize;

        // Dissolve: past its staggered start the glyph stops being itself —
        // it cycles through random substitutes from its typeface's pool
        // (fresh pick every 55ms slice), flickering, while its alpha runs
        // out; fully faded glyphs vanish until the loop retypes them.
        SkGlyphID glyphId = shaped.glyphs[glyph];
        float fade = 1.0f;
        bool scrambling = false;
        const double glyphDissolveAt =
            dissolveStart + hash01(index * 1274126177u + 29) * staggerChars;
        if (elapsedChars > glyphDissolveAt) {
          scrambling = true;
          const float into = static_cast<float>(
              (elapsedChars - glyphDissolveAt) / charsPerSecond);
          constexpr float kScrambleSeconds = 0.55f;
          constexpr float kFadeSeconds = 0.5f;
          if (into > kScrambleSeconds)
            fade = 1.0f - (into - kScrambleSeconds) / kFadeSeconds;
          if (fade <= 0.0f || scramblePool.empty())
            continue; // gone until the next loop
          const auto scrambleSlice =
              static_cast<uint32_t>(elapsedSeconds / 0.055);
          glyphId = scramblePool[static_cast<size_t>(
              hash01(index * 2654435761u + scrambleSlice * 40503u) *
              static_cast<float>(scramblePool.size()))];
        }
        drawnGlyphs++;

        SkPoint position = rest;
        if (burst && std::abs(rest.fY - tearCenterY) < 24.0f)
          position.fX += tearShift;
        if (burst && !scrambling) {
          const float pick = hash01(index * 374761393u + slice * 97u);
          if (pick < 0.08f + 0.30f * glitchAmount) {
            position.fX += (hash01(index * 83492791u + slice) - 0.5f) * 14.0f;
            m_splitBuckets.add(&shaped, glyphId, position);
            continue; // split copies replace the core+glow draw
          }
        }

        float brightness;
        if (scrambling) {
          // Scrambled letters burn hot and unstable, not phosphor-steady.
          brightness = 0.82f + 0.18f * hash01(index * 513u +
                                              static_cast<uint32_t>(
                                                  elapsedSeconds / 0.055) *
                                                  77u);
        } else {
          const float age =
              static_cast<float>(elapsedChars - revealAt) / charsPerSecond;
          brightness = kSteadyBrightness +
                       (1.0f - kSteadyBrightness) * std::exp(-age * 7.0f);
          brightness += 0.05f * std::sin(static_cast<float>(elapsedSeconds) *
                                             11.0f +
                                         hash01(index * 97u + 13) * 6.2832f);
        }
        brightness = std::clamp(brightness * flicker, 0.35f, 1.0f);
        m_buckets.add(
            {&shaped,
             static_cast<int>(brightness * (kBrightnessLevels - 1) + 0.5f),
             static_cast<int>(fade * (kFadeLevels - 1) + 0.5f)},
            glyphId, position);
      }
    }
    m_scheduleChars = revealAt; // next frame's loop length

    drawBackground(canvas, size);

    // Pass 1 — phosphor glow: the same buckets drawn through a blur mask,
    // alpha rising steeply with brightness so fresh letters bloom (and
    // falling with the dissolve fade).
    if (glowAmount > 0.01f) {
      SkPaint glowPaint;
      glowPaint.setAntiAlias(true);
      m_buckets.drawEach([&](const auto &bucket) {
        const float t =
            static_cast<float>(bucket.key.level) / (kBrightnessLevels - 1);
        const float fade =
            static_cast<float>(bucket.key.fade) / (kFadeLevels - 1);
        glowPaint.setColor(SkColorSetARGB(
            static_cast<U8CPU>(std::min(
                255.0f, 235.0f * glowAmount * t * t * fade * fade)),
            64, 255, 128));
        glowPaint.setMaskFilter(SkMaskFilter::MakeBlur(
            kNormal_SkBlurStyle,
            bucket.key.font->fontSize * (0.10f + 0.14f * glowAmount)));
        kit::drawPositionedGlyphs(canvas, *bucket.key.font, bucket.glyphs,
                                  bucket.placements, {0, 0}, glowPaint);
      });
    }

    // Pass 2 — crisp cores.
    SkPaint corePaint;
    corePaint.setAntiAlias(true);
    m_buckets.drawEach([&](const auto &bucket) {
      const SkColor color = phosphorColor(static_cast<float>(bucket.key.level) /
                                          (kBrightnessLevels - 1));
      corePaint.setColor(SkColorSetA(
          color,
          static_cast<U8CPU>(255.0f * bucket.key.fade / (kFadeLevels - 1))));
      kit::drawPositionedGlyphs(canvas, *bucket.key.font, bucket.glyphs,
                                bucket.placements, {0, 0}, corePaint);
    });

    // Glitched glyphs: two additive chroma-shifted copies instead of a core.
    SkPaint splitPaint;
    splitPaint.setAntiAlias(true);
    splitPaint.setBlendMode(SkBlendMode::kPlus);
    m_splitBuckets.drawEach([&](const auto &bucket) {
      splitPaint.setColor(0xE0FF2E55);
      kit::drawPositionedGlyphs(canvas, *bucket.key, bucket.glyphs,
                                bucket.placements, {-1.7f, 0}, splitPaint);
      splitPaint.setColor(0xE02EF0D8);
      kit::drawPositionedGlyphs(canvas, *bucket.key, bucket.glyphs,
                                bucket.placements, {1.7f, 0}, splitPaint);
    });

    if (elapsedChars <= dissolveStart) // hidden once the dissolve begins
      drawCursor(canvas, cursorPen, cursorFontSize, elapsedSeconds,
                 elapsedChars >= m_scheduleChars, glowAmount);
    if (params.boolValue(QStringLiteral("scanlines"), true))
      drawCrtOverlays(canvas, size, elapsedSeconds);
    drawFooter(canvas, fontContext, box.left(), canvasHeight - 16);

    return {layoutMicroseconds, static_cast<int>(m_layout.runs.size()),
            drawnGlyphs};
  }

private:
  /// Core/glow bucket key: font source plus quantized brightness and fade.
  struct Shade {
    const ShapedWord *font = nullptr;
    int level = 0;
    int fade = kFadeLevels - 1;
    bool operator==(const Shade &) const = default;
  };

  /// Scramble pools: every glyph ID the layout placed, per typeface —
  /// guaranteed-valid substitutes, naturally weighted toward common letters.
  /// Rebuilt with the layout (see render); the placements arrays stay unused
  /// because a pool is a substitution alphabet, not a draw batch.
  void rebuildGlyphPools(const Paragraph &paragraph) {
    m_glyphPools.buckets.clear(); // drop stale typefaces, not just glyphs
    for (const PositionedRun &run : m_layout.runs)
      for (const WordSegment &segment :
           paragraph.words()[run.wordIndex].segments) {
        const ShapedWord &shaped = *segment.shaped;
        std::vector<SkGlyphID> &pool =
            m_glyphPools.bucketFor(shaped.typeface.get()).glyphs;
        pool.insert(pool.end(), shaped.glyphs.begin(), shaped.glyphs.end());
      }
  }

  void drawBackground(SkCanvas *canvas, SkISize size) {
    const sk_sp<SkShader> &background = m_background.ensure({size}, [&] {
      const SkPoint center = {size.width() * 0.5f, size.height() * 0.46f};
      const SkColor4f colors[] = {SkColor4f::FromColor(0xFF08170E),
                                  SkColor4f::FromColor(0xFF030B06)};
      return SkShaders::RadialGradient(
          center, std::max(size.width(), size.height()) * 0.75f,
          SkGradient(SkGradient::Colors(colors, SkTileMode::kClamp), {}));
    });
    SkPaint paint;
    paint.setShader(background);
    canvas->drawRect(SkRect::MakeIWH(size.width(), size.height()), paint);
  }

  void drawCursor(SkCanvas *canvas, SkPoint pen, float fontSize,
                  double elapsedSeconds, bool typingDone, float glowAmount) {
    if (typingDone && std::fmod(elapsedSeconds, 1.06) >= 0.53)
      return; // idle blink; solid while typing
    const SkRect block = SkRect::MakeXYWH(pen.fX + 2.0f,
                                          pen.fY - fontSize * 0.78f,
                                          fontSize * 0.55f, fontSize * 0.95f);
    SkPaint paint;
    paint.setAntiAlias(true);
    if (glowAmount > 0.01f) {
      paint.setColor(SkColorSetARGB(
          static_cast<U8CPU>(200 * glowAmount), 64, 255, 128));
      paint.setMaskFilter(
          SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, fontSize * 0.22f));
      canvas->drawRect(block, paint);
      paint.setMaskFilter(nullptr);
    }
    paint.setColor(0xF0A6FFC4);
    canvas->drawRect(block, paint);
  }

  void drawCrtOverlays(SkCanvas *canvas, SkISize size,
                       double elapsedSeconds) {
    const SkRect full = SkRect::MakeIWH(size.width(), size.height());
    const sk_sp<SkShader> &scanlines = m_scanlines.ensure([&] {
      const SkColor4f colors[] = {SkColor4f::FromColor(0x00000000),
                                  SkColor4f::FromColor(0x66000000),
                                  SkColor4f::FromColor(0x00000000)};
      const SkPoint points[] = {{0, 0}, {0, 3}};
      return SkShaders::LinearGradient(
          points,
          SkGradient(SkGradient::Colors(colors, SkTileMode::kRepeat), {}));
    });
    SkPaint paint;
    paint.setShader(scanlines);
    canvas->drawRect(full, paint);

    // A slow refresh band sweeping down the tube.
    const float bandCenter =
        std::fmod(static_cast<float>(elapsedSeconds) * 110.0f,
                  size.height() + 320.0f) -
        160.0f;
    const SkPoint bandPoints[] = {{0, bandCenter - 100}, {0, bandCenter + 100}};
    const SkColor4f bandColors[] = {SkColor4f::FromColor(0x0040FF80),
                                    SkColor4f::FromColor(0x1640FF80),
                                    SkColor4f::FromColor(0x0040FF80)};
    paint.setShader(SkShaders::LinearGradient(
        bandPoints,
        SkGradient(SkGradient::Colors(bandColors, SkTileMode::kClamp), {})));
    canvas->drawRect(full, paint);

    const sk_sp<SkShader> &vignette = m_vignette.ensure({size}, [&] {
      const SkPoint center = {size.width() * 0.5f, size.height() * 0.5f};
      const SkColor4f colors[] = {SkColor4f::FromColor(0x00000000),
                                  SkColor4f::FromColor(0x00000000),
                                  SkColor4f::FromColor(0x9A000000)};
      const float stops[] = {0, 0.62f, 1.0f};
      return SkShaders::RadialGradient(
          center, std::max(size.width(), size.height()) * 0.72f,
          SkGradient(SkGradient::Colors(colors, stops, SkTileMode::kClamp),
                     {}));
    });
    paint.setShader(vignette);
    canvas->drawRect(full, paint);
  }

  void drawFooter(SkCanvas *canvas, FontContext &fontContext, float left,
                  float baseline) {
    if (m_footer.text().empty())
      m_footer.appendText(
          u8"per-glyph reveal · brightness-bucketed glow + core passes · "
          u8"RGB-split glitches · scramble-and-fade dissolve — public API, "
          u8"proportional font",
          makeStyle(11.0f, 0xFF2F7D4C));
    layoutSingleLine(fontContext, m_footer, {left, baseline})
        .draw(canvas, m_footer);
  }

  BodyCache m_body;
  ParagraphLayout m_layout; // memoized; the reveal is draw-side only
  kit::LayoutGuard<SkISize, LineBreakStrategy, float> m_layoutGuard;
  double m_scheduleChars = 0; // total tape length, character units
  kit::GlyphBuckets<const SkTypeface *> m_glyphPools; // scramble alphabets
  kit::GlyphBuckets<Shade> m_buckets;                 // core + glow passes
  kit::GlyphBuckets<const ShapedWord *> m_splitBuckets; // RGB-split glitches
  std::vector<uint32_t> m_segmentCounters;
  Paragraph m_footer;
  kit::CachedValue<sk_sp<SkShader>, SkISize> m_background;
  kit::CachedValue<sk_sp<SkShader>> m_scanlines;
  kit::CachedValue<sk_sp<SkShader>, SkISize> m_vignette;
};

SceneDescriptor makeTerminalDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Retro terminal — per-glyph reveal");
  descriptor.defaultText = terminalDefaultText();
  descriptor.displayOrder = 55; // with its choreography siblings, rain/ripple
  descriptor.parameters = {
      {QStringLiteral("speed"), QStringLiteral("Typing speed"),
       SceneParameter::Type::kFloat, 60.0, 8, 320, QStringLiteral(" cps"),
       {}},
      {QStringLiteral("glow"), QStringLiteral("Phosphor glow"),
       SceneParameter::Type::kFloat, 0.65, 0, 1, {}, {}},
      {QStringLiteral("glitch"), QStringLiteral("Glitch"),
       SceneParameter::Type::kFloat, 0.35, 0, 1, {}, {}},
      {QStringLiteral("scanlines"), QStringLiteral("Scanlines & CRT"),
       SceneParameter::Type::kBool, true, 0, 1, {}, {}},
  };
  descriptor.make = [] { return std::make_unique<TerminalScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeTerminalDescriptor())

} // namespace gallery
