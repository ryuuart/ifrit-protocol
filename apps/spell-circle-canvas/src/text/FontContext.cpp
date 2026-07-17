#include "FontContextImpl.h"

#include <include/core/SkData.h>
#include <include/core/SkFontStyle.h>

#include <string_view>

namespace textflow {

namespace {

// hb_face table provider backed by SkTypeface::copyTableData. hb_tag_t and
// SkFontTableTag share the same big-endian four-cc encoding.
hb_blob_t *getTable(hb_face_t *, hb_tag_t tag, void *context) {
  auto *typeface = static_cast<SkTypeface *>(context);
  sk_sp<SkData> data = typeface->copyTableData(tag);
  if (!data)
    return nullptr;
  SkData *rawData = data.release();
  return hb_blob_create(
      static_cast<const char *>(rawData->data()), rawData->size(),
      HB_MEMORY_MODE_READONLY, rawData,
      [](void *releasedData) { static_cast<SkData *>(releasedData)->unref(); });
}

sk_sp<SkTypeface> resolveSystemFallback(SkFontMgr &fontManager,
                                        const SkTypeface &primaryTypeface,
                                        int32_t codePoint,
                                        std::string_view languageTag) {
  // matchFamilyStyleCharacter wants C strings but the resolver contract
  // passes a string_view with no NUL-termination guarantee (see
  // FallbackResolver in FontContext.h) — copy to a local first rather than
  // reading past languageTag.data()'s end. This runs only on a
  // fallback-cache miss, so the allocation is off the hot path.
  const std::string terminatedLanguageTag(languageTag);
  const char *languageTags[] = {terminatedLanguageTag.c_str()};
  return fontManager.matchFamilyStyleCharacter(
      nullptr, primaryTypeface.fontStyle(),
      languageTag.empty() ? nullptr : languageTags,
      languageTag.empty() ? 0 : 1, codePoint);
}

} // namespace

FontContext::Impl::TypefaceRecord &
FontContext::Impl::recordForTypeface(const sk_sp<SkTypeface> &typeface) {
  auto [recordEntry, inserted] =
      typefaceRecords.try_emplace(typeface->uniqueID());
  TypefaceRecord &record = recordEntry->second;
  if (!inserted)
    return record;

  record.typeface = typeface;
  record.harfBuzzFace =
      hb_face_create_for_tables(getTable, typeface.get(), nullptr);
  record.unitsPerEm = typeface->getUnitsPerEm();
  if (record.unitsPerEm <= 0)
    record.unitsPerEm = 1000;
  hb_face_set_upem(record.harfBuzzFace, record.unitsPerEm);
  record.harfBuzzFont = hb_font_create(record.harfBuzzFace);
  hb_font_set_scale(record.harfBuzzFont, record.unitsPerEm, record.unitsPerEm);

  // Variable fonts: mirror the typeface's design position (weight, width,
  // optical size…) into HarfBuzz. Without this, HarfBuzz shapes at the
  // default instance while Skia rasterizes the varied one — advances and
  // rasterized glyphs silently disagree.
  const int axisCount = typeface->getVariationDesignPosition({});
  if (axisCount > 0) {
    std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates(
        static_cast<size_t>(axisCount));
    if (typeface->getVariationDesignPosition(
            {coordinates.data(), coordinates.size()}) == axisCount) {
      std::vector<hb_variation_t> variations(coordinates.size());
      for (size_t coordinateIndex = 0; coordinateIndex < coordinates.size();
           ++coordinateIndex)
        variations[coordinateIndex] = {coordinates[coordinateIndex].axis,
                                       coordinates[coordinateIndex].value};
      hb_font_set_variations(record.harfBuzzFont, variations.data(),
                             static_cast<unsigned>(variations.size()));
    }
  }
  return record;
}

void FontContext::Impl::destroyTypefaceRecords() {
  for (auto &typefaceEntry : typefaceRecords) {
    TypefaceRecord &record = typefaceEntry.second;
    if (record.harfBuzzFont)
      hb_font_destroy(record.harfBuzzFont);
    if (record.harfBuzzFace)
      hb_face_destroy(record.harfBuzzFace);
  }
  typefaceRecords.clear();
}

FontContext::Impl::~Impl() {
  destroyTypefaceRecords();
  if (shapingBuffer)
    hb_buffer_destroy(shapingBuffer);
  if (lineBreakIterator)
    ubrk_close(lineBreakIterator);
  if (bidirectionalAnalyzer)
    ubidi_close(bidirectionalAnalyzer);
}

FontContext::FontContext(sk_sp<SkFontMgr> fontManager,
                         sk_sp<SkTypeface> defaultTypeface,
                         FallbackResolver fallbackResolver)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->fontManager = std::move(fontManager);
  m_impl->defaultTypeface = std::move(defaultTypeface);
  m_impl->fallbackResolver = fallbackResolver ? std::move(fallbackResolver)
                                              : resolveSystemFallback;
  m_impl->shapingBuffer = hb_buffer_create();
}

FontContext::~FontContext() = default;

SkFontMgr *FontContext::fontManager() const {
  return m_impl->fontManager.get();
}

const sk_sp<SkTypeface> &FontContext::defaultTypeface() const {
  if (!m_impl->defaultTypeface && m_impl->fontManager) {
    m_impl->defaultTypeface =
        m_impl->fontManager->matchFamilyStyle(nullptr, SkFontStyle());
    if (!m_impl->defaultTypeface)
      m_impl->defaultTypeface =
          m_impl->fontManager->legacyMakeTypeface(nullptr, SkFontStyle());
  }
  return m_impl->defaultTypeface;
}

sk_sp<SkTypeface>
FontContext::resolveTypeface(const sk_sp<SkTypeface> &primaryTypeface,
                             int32_t codePoint, const char *languageTag) {
  const sk_sp<SkTypeface> &resolvedPrimaryTypeface =
      primaryTypeface ? primaryTypeface : defaultTypeface();
  if (!resolvedPrimaryTypeface)
    return nullptr;

  // ASCII fast path: direct-mapped table per primary, memoizing the last
  // primary used. Entries borrow the sk_sp held by `fallbackTypefaces`
  // (which is never pruned), so the raw pointers stay valid.
  const bool isAscii = codePoint >= 0 && codePoint < 128;
  if (isAscii) {
    if (m_impl->lastAsciiTypefaceId != resolvedPrimaryTypeface->uniqueID() ||
        !m_impl->lastAsciiFallbackTable) {
      m_impl->lastAsciiTypefaceId = resolvedPrimaryTypeface->uniqueID();
      m_impl->lastAsciiFallbackTable =
          &m_impl->asciiFallbackTypefaces[resolvedPrimaryTypeface->uniqueID()];
    }
    if (SkTypeface *cachedTypeface =
            (*m_impl->lastAsciiFallbackTable)[codePoint])
      return sk_ref_sp(cachedTypeface);
  }

  const std::string_view language = languageTag ? languageTag : "";
  const FallbackKey fallbackKey{resolvedPrimaryTypeface->uniqueID(), codePoint,
                                m_impl->fallbackLanguageId(language)};
  auto cachedFallback = m_impl->fallbackTypefaces.find(fallbackKey);
  if (cachedFallback != m_impl->fallbackTypefaces.end())
    return cachedFallback->second;

  m_impl->stats.coverageQueries++;
  sk_sp<SkTypeface> resolvedTypeface = resolvedPrimaryTypeface;
  if (resolvedPrimaryTypeface->unicharToGlyph(codePoint) == 0) {
    m_impl->stats.fallbackQueries++;
    sk_sp<SkTypeface> matchingTypeface =
        m_impl->fontManager && m_impl->fallbackResolver
            ? m_impl->fallbackResolver(*m_impl->fontManager,
                                       *resolvedPrimaryTypeface, codePoint,
                                       language)
            : nullptr;
    if (matchingTypeface && matchingTypeface->unicharToGlyph(codePoint) != 0)
      resolvedTypeface = std::move(matchingTypeface);
  }
  const auto insertedEntry =
      m_impl->fallbackTypefaces.emplace(fallbackKey, resolvedTypeface).first;
  // A missing ASCII character may be resolved differently by language. Only
  // put primary-coverage hits in the language-independent fast table.
  if (isAscii && insertedEntry->second.get() == resolvedPrimaryTypeface.get())
    (*m_impl->lastAsciiFallbackTable)[codePoint] = insertedEntry->second.get();
  return resolvedTypeface;
}

void FontContext::purgeShapeCache() { m_impl->shapeCache.clear(); }

void FontContext::purgeAllCaches() {
  m_impl->shapeCache.clear();
  m_impl->destroyTypefaceRecords();
  m_impl->fallbackTypefaces.clear();
  m_impl->fallbackLanguageIds.clear();
  m_impl->asciiFallbackTypefaces.clear();
  // Every memo below borrows from a map cleared above; leaving any of them
  // set would hand out a dangling pointer (lastAsciiFallbackTable) or a
  // stale interned id on the next lookup.
  m_impl->lastAsciiTypefaceId = 0;
  m_impl->lastAsciiFallbackTable = nullptr;
  m_impl->lastFallbackLanguageTag.clear();
  m_impl->lastFallbackLanguageId = 0;
  m_impl->nextFallbackLanguageId = 1;
}

const FontContext::Stats &FontContext::stats() const { return m_impl->stats; }

void FontContext::resetStats() { m_impl->stats = {}; }

} // namespace textflow
