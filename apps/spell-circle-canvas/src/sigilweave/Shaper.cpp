#include "sigilweave/Shaper.h"
#include "FontContextImpl.h"

#include <include/core/SkFont.h>
#include <include/core/SkTextBlob.h>

#include <cstring>

namespace sigil::weave {

SkFont makeFont(const sk_sp<SkTypeface> &typeface, float fontSize,
                float scaleX) {
  SkFont font(typeface, fontSize);
  if (scaleX != 1.0f)
    font.setScaleX(scaleX);
  font.setEdging(SkFont::Edging::kAntiAlias);
  // Subpixel positioning only where it's visible (small text). At large
  // sizes it multiplies every glyph into per-phase atlas entries, and
  // animated layouts then re-rasterize thousands of big masks every frame
  // — whole-pixel snapping at 48px+ is imperceptible (<2% of an em).
  font.setSubpixel(fontSize < 48.0f);
  font.setHinting(SkFontHinting::kNone);
  font.setLinearMetrics(true);
  return font;
}

ShapedWordRef shapeWord(FontContext &fontContext, const ShapingStyle &style,
                        const sk_sp<SkTypeface> &typeface,
                        std::u16string_view text, ScriptTag script,
                        bool rightToLeft, bool vertical) {
  FontContext::Impl &implementation = fontContext.impl();

  // Probe with a borrowed view — the warm path allocates nothing.
  ShapeKeyView view;
  view.typefaceId = typeface ? typeface->uniqueID() : 0;
  std::memcpy(&view.fontSizeBits, &style.fontSize, sizeof(float));
  std::memcpy(&view.letterSpacingBits, &style.letterSpacing, sizeof(float));
  std::memcpy(&view.scaleXBits, &style.scaleX, sizeof(float));
  view.script = script;
  view.rightToLeft = rightToLeft;
  view.vertical = vertical;
  view.languageTag = style.languageTag;
  view.fontFeatures = style.fontFeatures.data();
  view.featureCount = style.fontFeatures.size();
  view.text = text;

  if (auto cachedShape = implementation.shapeCache.find(view);
      cachedShape != implementation.shapeCache.end()) {
    ++implementation.stats.shapeCacheHits;
    return cachedShape->second;
  }

  ShapeKey key;
  key.typefaceId = view.typefaceId;
  key.fontSizeBits = view.fontSizeBits;
  key.letterSpacingBits = view.letterSpacingBits;
  key.scaleXBits = view.scaleXBits;
  key.script = script;
  key.rightToLeft = rightToLeft;
  key.vertical = vertical;
  key.languageTag = style.languageTag;
  key.fontFeatures = style.fontFeatures;
  key.text.assign(text.begin(), text.end());

  ++implementation.stats.shapeCalls;
  auto shapedWord = std::make_shared<ShapedWord>();
  shapedWord->typeface = typeface;
  shapedWord->fontSize = style.fontSize;
  shapedWord->scaleX = style.scaleX;
  shapedWord->vertical = vertical;

  if (!typeface || text.empty()) {
    implementation.shapeCache.emplace(std::move(key), shapedWord);
    return shapedWord;
  }

  FontContext::Impl::TypefaceRecord &typefaceRecord =
      implementation.recordForTypeface(typeface);

  hb_buffer_t *shapingBuffer = implementation.shapingBuffer;
  hb_buffer_clear_contents(shapingBuffer);
  hb_buffer_add_utf16(
      shapingBuffer, reinterpret_cast<const uint16_t *>(text.data()),
      static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
  // Vertical shaping (TTB) makes HarfBuzz apply the font's 'vert' glyph
  // substitutions and vertical advances/origins automatically.
  hb_buffer_set_direction(shapingBuffer, vertical      ? HB_DIRECTION_TTB
                                         : rightToLeft ? HB_DIRECTION_RTL
                                                       : HB_DIRECTION_LTR);
  hb_buffer_set_script(shapingBuffer, static_cast<hb_script_t>(script));
  hb_buffer_set_language(
      shapingBuffer,
      style.languageTag.empty()
          ? hb_language_get_default()
          : hb_language_from_string(style.languageTag.c_str(), -1));
  hb_buffer_set_cluster_level(shapingBuffer,
                              HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);

  std::vector<hb_feature_t> harfBuzzFeatures;
  harfBuzzFeatures.reserve(style.fontFeatures.size());
  for (const FontFeature &feature : style.fontFeatures) {
    hb_feature_t harfBuzzFeature;
    harfBuzzFeature.tag =
        HB_TAG(feature.tag[0], feature.tag[1], feature.tag[2], feature.tag[3]);
    harfBuzzFeature.value = feature.value;
    harfBuzzFeature.start = 0;
    harfBuzzFeature.end = static_cast<unsigned>(-1);
    harfBuzzFeatures.push_back(harfBuzzFeature);
  }

  hb_shape(typefaceRecord.harfBuzzFont, shapingBuffer,
           harfBuzzFeatures.empty() ? nullptr : harfBuzzFeatures.data(),
           static_cast<unsigned>(harfBuzzFeatures.size()));

  const unsigned glyphCount = hb_buffer_get_length(shapingBuffer);
  const hb_glyph_info_t *glyphInformation =
      hb_buffer_get_glyph_infos(shapingBuffer, nullptr);
  const hb_glyph_position_t *glyphPositions =
      hb_buffer_get_glyph_positions(shapingBuffer, nullptr);

  const float scale =
      style.fontSize / static_cast<float>(typefaceRecord.unitsPerEm);
  // Condensation: x-axis quantities shrink by scaleX (glyph shapes condense
  // via SkFont::setScaleX at draw). Vertical columns keep their advance —
  // only the glyph's horizontal centering condenses. Tracking is unscaled.
  const float scaleXAxis = scale * style.scaleX;

  shapedWord->glyphs.reserve(glyphCount);
  shapedWord->positions.reserve(glyphCount);
  shapedWord->advances.reserve(glyphCount);
  shapedWord->clusters.reserve(glyphCount);

  // HarfBuzz emits glyphs in visual (left-to-right pen) order for both
  // horizontal directions, so the pen always accumulates forward; in
  // vertical mode the pen runs top to bottom instead. HarfBuzz has already
  // subtracted each glyph's vertical origin from the offsets, so vertical
  // positions below are relative to Skia's horizontal-origin convention
  // (x roughly centres the glyph on the column axis).
  float penPosition = 0;
  for (unsigned glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    // (y-down canvas: HarfBuzz y advances/offsets point up, so they negate.)
    float glyphAdvance =
        vertical ? -glyphPositions[glyphIndex].y_advance * scale
                 : glyphPositions[glyphIndex].x_advance * scaleXAxis;
    // Tracking applies once per cluster, on the cluster's last glyph.
    const bool clusterEnd = glyphIndex + 1 == glyphCount ||
                            glyphInformation[glyphIndex + 1].cluster !=
                                glyphInformation[glyphIndex].cluster;
    if (clusterEnd)
      glyphAdvance += style.letterSpacing;

    shapedWord->glyphs.push_back(
        static_cast<uint16_t>(glyphInformation[glyphIndex].codepoint));
    if (vertical)
      shapedWord->positions.push_back(
          {glyphPositions[glyphIndex].x_offset * scaleXAxis,
           penPosition - glyphPositions[glyphIndex].y_offset * scale});
    else
      shapedWord->positions.push_back(
          {penPosition + glyphPositions[glyphIndex].x_offset * scaleXAxis,
           -glyphPositions[glyphIndex].y_offset * scale});
    shapedWord->advances.push_back(glyphAdvance);
    shapedWord->clusters.push_back(glyphInformation[glyphIndex].cluster);
    penPosition += glyphAdvance;
  }
  shapedWord->advance = penPosition;

  if (implementation.shapeCache.size() >= FontContext::Impl::kMaxShapeEntries)
    implementation.shapeCache.clear();
  implementation.shapeCache.emplace(std::move(key), shapedWord);
  return shapedWord;
}

const sk_sp<SkTextBlob> &wordBlob(const ShapedWord &word) {
  if (!word.blobCache && !word.glyphs.empty()) {
    SkTextBlobBuilder builder;
    const SkFont font = makeFont(word.typeface, word.fontSize, word.scaleX);
    const auto &blobRun =
        builder.allocRunPos(font, static_cast<int>(word.glyphs.size()));
    std::memcpy(blobRun.glyphs, word.glyphs.data(),
                word.glyphs.size() * sizeof(uint16_t));
    std::memcpy(blobRun.points(), word.positions.data(),
                word.positions.size() * sizeof(SkPoint));
    word.blobCache = builder.make();
  }
  return word.blobCache;
}

} // namespace sigil::weave
