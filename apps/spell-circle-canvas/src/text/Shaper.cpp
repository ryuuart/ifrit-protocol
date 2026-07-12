#include "textflow/Shaper.h"
#include "FontContextImpl.h"

#include <include/core/SkFont.h>
#include <include/core/SkTextBlob.h>

#include <cstring>

namespace textflow {

SkFont makeFont(const sk_sp<SkTypeface> &typeface, float size) {
  SkFont font(typeface, size);
  font.setEdging(SkFont::Edging::kAntiAlias);
  // Subpixel positioning only where it's visible (small text). At large
  // sizes it multiplies every glyph into per-phase atlas entries, and
  // animated layouts then re-rasterize thousands of big masks every frame
  // — whole-pixel snapping at 48px+ is imperceptible (<2% of an em).
  font.setSubpixel(size < 48.0f);
  font.setHinting(SkFontHinting::kNone);
  font.setLinearMetrics(true);
  return font;
}

ShapedWordRef shapeWord(FontContext &ctx, const ShapingStyle &style,
                        const sk_sp<SkTypeface> &typeface,
                        std::u16string_view text, ScriptTag script, bool rtl,
                        bool vertical) {
  FontContext::Impl &impl = ctx.impl();

  // Probe with a borrowed view — the warm path allocates nothing.
  ShapeKeyView view;
  view.typefaceID = typeface ? typeface->uniqueID() : 0;
  std::memcpy(&view.sizeBits, &style.size, sizeof(float));
  std::memcpy(&view.letterSpacingBits, &style.letterSpacing, sizeof(float));
  view.script = script;
  view.rtl = rtl;
  view.vertical = vertical;
  view.language = style.language;
  view.features = style.features.data();
  view.featureCount = style.features.size();
  view.text = text;

  if (auto it = impl.shapeCache.find(view); it != impl.shapeCache.end()) {
    impl.stats.shapeCacheHits++;
    return it->second;
  }

  ShapeKey key;
  key.typefaceID = view.typefaceID;
  key.sizeBits = view.sizeBits;
  key.letterSpacingBits = view.letterSpacingBits;
  key.script = script;
  key.rtl = rtl;
  key.vertical = vertical;
  key.language = style.language;
  key.features = style.features;
  key.text.assign(text.begin(), text.end());

  impl.stats.shapeCalls++;
  auto word = std::make_shared<ShapedWord>();
  word->typeface = typeface;
  word->size = style.size;
  word->vertical = vertical;

  if (!typeface || text.empty()) {
    impl.shapeCache.emplace(std::move(key), word);
    return word;
  }

  FontContext::Impl::FaceRec &rec = impl.faceFor(typeface);

  hb_buffer_t *buffer = impl.buffer;
  hb_buffer_clear_contents(buffer);
  hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t *>(text.data()),
                      static_cast<int>(text.size()), 0,
                      static_cast<int>(text.size()));
  // Vertical shaping (TTB) makes HarfBuzz apply the font's 'vert' glyph
  // substitutions and vertical advances/origins automatically.
  hb_buffer_set_direction(buffer, vertical ? HB_DIRECTION_TTB
                                  : rtl    ? HB_DIRECTION_RTL
                                           : HB_DIRECTION_LTR);
  hb_buffer_set_script(buffer, static_cast<hb_script_t>(script));
  hb_buffer_set_language(buffer, style.language.empty()
                                     ? hb_language_get_default()
                                     : hb_language_from_string(
                                           style.language.c_str(), -1));
  hb_buffer_set_cluster_level(buffer,
                              HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);

  std::vector<hb_feature_t> features;
  features.reserve(style.features.size());
  for (const FontFeature &f : style.features) {
    hb_feature_t hf;
    hf.tag = HB_TAG(f.tag[0], f.tag[1], f.tag[2], f.tag[3]);
    hf.value = f.value;
    hf.start = 0;
    hf.end = static_cast<unsigned>(-1);
    features.push_back(hf);
  }

  hb_shape(rec.font, buffer, features.empty() ? nullptr : features.data(),
           static_cast<unsigned>(features.size()));

  const unsigned count = hb_buffer_get_length(buffer);
  const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, nullptr);
  const hb_glyph_position_t *poss =
      hb_buffer_get_glyph_positions(buffer, nullptr);

  const float scale = style.size / static_cast<float>(rec.upem);

  word->glyphs.reserve(count);
  word->positions.reserve(count);
  word->advances.reserve(count);
  word->clusters.reserve(count);

  // HarfBuzz emits glyphs in visual (left-to-right pen) order for both
  // horizontal directions, so the pen always accumulates forward; in
  // vertical mode the pen runs top to bottom instead. HarfBuzz has already
  // subtracted each glyph's vertical origin from the offsets, so vertical
  // positions below are relative to Skia's horizontal-origin convention
  // (x roughly centres the glyph on the column axis).
  float pen = 0;
  for (unsigned i = 0; i < count; ++i) {
    // (y-down canvas: HarfBuzz y advances/offsets point up, so they negate.)
    float adv = (vertical ? -poss[i].y_advance : poss[i].x_advance) * scale;
    // Tracking applies once per cluster, on the cluster's last glyph.
    const bool clusterEnd =
        i + 1 == count || infos[i + 1].cluster != infos[i].cluster;
    if (clusterEnd)
      adv += style.letterSpacing;

    word->glyphs.push_back(static_cast<uint16_t>(infos[i].codepoint));
    if (vertical)
      word->positions.push_back(
          {poss[i].x_offset * scale, pen - poss[i].y_offset * scale});
    else
      word->positions.push_back(
          {pen + poss[i].x_offset * scale, -poss[i].y_offset * scale});
    word->advances.push_back(adv);
    word->clusters.push_back(infos[i].cluster);
    pen += adv;
  }
  word->advance = pen;

  if (impl.shapeCache.size() >= FontContext::Impl::kMaxShapeEntries)
    impl.shapeCache.clear();
  impl.shapeCache.emplace(std::move(key), word);
  return word;
}

const sk_sp<SkTextBlob> &wordBlob(const ShapedWord &word) {
  if (!word.blobCache && !word.glyphs.empty()) {
    SkTextBlobBuilder builder;
    const SkFont font = makeFont(word.typeface, word.size);
    const auto &run =
        builder.allocRunPos(font, static_cast<int>(word.glyphs.size()));
    std::memcpy(run.glyphs, word.glyphs.data(),
                word.glyphs.size() * sizeof(uint16_t));
    std::memcpy(run.points(), word.positions.data(),
                word.positions.size() * sizeof(SkPoint));
    word.blobCache = builder.make();
  }
  return word.blobCache;
}

} // namespace textflow
