/** @file
 * The font service: HarfBuzz faces bound to Skia typefaces through their
 * table data, fallback resolution per code point and language with an
 * ASCII fast table, varied-typeface clones retained or transient, the
 * advance-invariance probe, and the caches' lifetime and statistics.
 */

#include <include/core/SkData.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FontContextImpl.h"
#include "sigilweave/advanced/Skia.h"

namespace sigil::weave {

namespace {

// hb_face table provider backed by SkTypeface::copyTableData. hb_tag_t and
// SkFontTableTag share the same big-endian four-cc encoding.
hb_blob_t* getTable(hb_face_t*, hb_tag_t tag, void* context) {
  auto* typeface = static_cast<SkTypeface*>(context);
  sk_sp<SkData> data = typeface->copyTableData(tag);
  if (!data) return nullptr;
  SkData* rawData = data.release();
  return hb_blob_create(
      static_cast<const char*>(rawData->data()), rawData->size(),
      HB_MEMORY_MODE_READONLY, rawData,
      [](void* releasedData) { static_cast<SkData*>(releasedData)->unref(); });
}

Face resolveSystemFallback(SkFontMgr& fontManager, const Face& primaryFace,
                           int32_t codePoint, std::string_view languageTag) {
  const sk_sp<SkTypeface> primary = toSk(primaryFace);
  const SkTypeface& primaryTypeface = *primary;
  // matchFamilyStyleCharacter wants C strings but the resolver contract
  // passes a string_view with no NUL-termination guarantee — copy to a
  // local first rather than reading past languageTag.data()'s end. This
  // runs only on a fallback-cache miss, so the allocation is off the hot
  // path.
  const std::string terminatedLanguageTag(languageTag);
  const char* languageTags[] = {terminatedLanguageTag.c_str()};
  return fontManager.matchFamilyStyleCharacter(
      nullptr, primaryTypeface.fontStyle(),
      languageTag.empty() ? nullptr : languageTags, languageTag.empty() ? 0 : 1,
      codePoint);
}

/** Whether two family names are one, ASCII case aside, as CSS compares
 *  them. */
bool sameFamily(std::string_view left, std::string_view right) {
  return std::ranges::equal(
      left, right, [](char leftCharacter, char rightCharacter) {
        return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
               std::tolower(static_cast<unsigned char>(rightCharacter));
      });
}

std::string familyNameOf(const SkTypeface& typeface) {
  SkString name;
  typeface.getFamilyName(&name);
  return name.c_str();
}

/** The faces of one family a context adopted, as the style set Skia's own
 *  CSS matcher walks, so the nearest face to a style is chosen exactly as
 *  an installed family's is. */
class AdoptedFamily final : public SkFontStyleSet {
 public:
  explicit AdoptedFamily(std::vector<sk_sp<SkTypeface>> members)
      : m_members(std::move(members)) {}
  int count() override { return static_cast<int>(m_members.size()); }
  void getStyle(int index, SkFontStyle* style, SkString* name) override {
    if (style) *style = m_members[static_cast<size_t>(index)]->fontStyle();
    if (name) name->reset();
  }
  sk_sp<SkTypeface> createTypeface(int index) override {
    return m_members[static_cast<size_t>(index)];
  }
  sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override {
    return matchStyleCSS3(pattern);
  }

 private:
  std::vector<sk_sp<SkTypeface>> m_members;
};

/** The adopted face of @p family nearest @p style, or null where nothing
 *  adopted carries that family. */
sk_sp<SkTypeface> nearestAdopted(const std::vector<AdoptedFace>& adopted,
                                 std::string_view family,
                                 const SkFontStyle& style) {
  std::vector<sk_sp<SkTypeface>> members;
  for (const AdoptedFace& entry : adopted) {
    sk_sp<SkTypeface> typeface = toSk(entry.face);
    if (typeface && sameFamily(familyNameOf(*typeface), family))
      members.push_back(std::move(typeface));
  }
  if (members.empty()) return nullptr;
  if (members.size() == 1) return members.front();
  return sk_make_sp<AdoptedFamily>(std::move(members))->matchStyle(style);
}

}  // namespace

FontContext::Impl::TypefaceRecord& FontContext::Impl::recordForTypeface(
    const sk_sp<SkTypeface>& typeface) {
  auto [recordEntry, inserted] =
      typefaceRecords.try_emplace(typeface->uniqueID());
  TypefaceRecord& record = recordEntry->second;
  if (!inserted) return record;

  record.typeface = typeface;
  record.harfBuzzFace =
      hb_face_create_for_tables(getTable, typeface.get(), nullptr);
  record.unitsPerEm = typeface->getUnitsPerEm();
  if (record.unitsPerEm <= 0) record.unitsPerEm = 1000;
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
  for (auto& typefaceEntry : typefaceRecords) {
    TypefaceRecord& record = typefaceEntry.second;
    if (record.harfBuzzFont) hb_font_destroy(record.harfBuzzFont);
    if (record.harfBuzzFace) hb_face_destroy(record.harfBuzzFace);
  }
  typefaceRecords.clear();
}

FontContext::Impl::~Impl() {
  destroyTypefaceRecords();
  if (shapingBuffer) hb_buffer_destroy(shapingBuffer);
}

FontContext::FontContext(sk_sp<SkFontMgr> fontManager, Face defaultTypeface,
                         FallbackResolver fallbackResolver)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->fontManager = std::move(fontManager);
  m_impl->defaultTypeface = std::move(defaultTypeface);
  m_impl->fallbackResolver =
      fallbackResolver ? std::move(fallbackResolver) : resolveSystemFallback;
  m_impl->shapingBuffer = hb_buffer_create();
}

FontContext::~FontContext() = default;

SkFontMgr* FontContext::fontManager() const {
  return m_impl->fontManager.get();
}

const Face& FontContext::defaultTypeface() const {
  if (!m_impl->defaultTypeface && m_impl->fontManager) {
    m_impl->defaultTypeface =
        m_impl->fontManager->matchFamilyStyle(nullptr, SkFontStyle());
    if (!m_impl->defaultTypeface)
      m_impl->defaultTypeface =
          m_impl->fontManager->legacyMakeTypeface(nullptr, SkFontStyle());
  }
  return m_impl->defaultTypeface;
}

Face FontContext::familyTypeface(std::string_view family, FaceStyle faceStyle) {
  const SkFontStyle style = toSk(faceStyle);
  if (family.empty()) return nullptr;
  // The name, then the three numbers a style is, after a separator no
  // family name carries, so two requests share an entry exactly when they
  // agree on all four.
  std::string key(family);
  key.push_back('\n');
  key.append(std::to_string(style.weight()))
      .append("/")
      .append(std::to_string(style.width()))
      .append("/")
      .append(std::to_string(static_cast<int>(style.slant())));
  const auto held = m_impl->familyTypefaces.find(key);
  if (held != m_impl->familyTypefaces.end()) return held->second;
  // An adopted family answers before the manager is asked at all.
  sk_sp<SkTypeface> found = nearestAdopted(m_impl->adoptedFaces, family, style);
  if (!found && m_impl->fontManager) {
    // The manager reads a C string, and the key begins with the name, so
    // the name is terminated in a copy of its own.
    const std::string name(family);
    found = m_impl->fontManager->matchFamilyStyle(name.c_str(), style);
  }
  return m_impl->familyTypefaces.emplace(std::move(key), std::move(found))
      .first->second;
}

void FontContext::adopt(Face face, std::string origin) {
  const sk_sp<SkTypeface> typeface = toSk(face);
  if (!typeface) return;
  std::vector<AdoptedFace>& adopted = m_impl->adoptedFaces;
  // The families whose answers this adoption can change: the new face's,
  // and that of the face it replaces.
  std::vector<std::string> touched{familyNameOf(*typeface)};
  const auto sameOrigin = [&origin](const AdoptedFace& entry) {
    return !origin.empty() && entry.origin == origin;
  };
  const auto replaced = std::ranges::find_if(adopted, sameOrigin);
  if (replaced != adopted.end()) {
    if (replaced->face == face) return;
    if (const sk_sp<SkTypeface> old = toSk(replaced->face))
      touched.push_back(familyNameOf(*old));
    replaced->face = std::move(face);
  } else {
    if (std::ranges::any_of(adopted, [&face](const AdoptedFace& entry) {
          return entry.face == face;
        }))
      return;
    adopted.push_back({std::move(face), std::move(origin)});
  }
  // A held answer for a touched family was chosen without this face, so it
  // is asked again; every other family keeps the face it answered with.
  boost::unordered::erase_if(
      m_impl->familyTypefaces, [&touched](const auto& entry) {
        const std::string_view name =
            std::string_view(entry.first).substr(0, entry.first.find('\n'));
        return std::ranges::any_of(touched, [name](const std::string& family) {
          return sameFamily(name, family);
        });
      });
}

std::vector<AdoptedFace> FontContext::adoptedFaces() const {
  return m_impl->adoptedFaces;
}

Face FontContext::resolveTypeface(const Face& primaryTypeface,
                                  int32_t codePoint, const char* languageTag) {
  const sk_sp<SkTypeface> resolvedPrimaryTypeface =
      toSk(primaryTypeface ? primaryTypeface : defaultTypeface());
  if (!resolvedPrimaryTypeface) return nullptr;

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
    if (SkTypeface* cachedTypeface =
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
            ? toSk(m_impl->fallbackResolver(*m_impl->fontManager,
                                            fromSk(resolvedPrimaryTypeface),
                                            codePoint, language))
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

namespace {

sk_sp<SkTypeface> cloneAt(const sk_sp<SkTypeface>& base,
                          std::span<const FontVariation> variations) {
  std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates;
  coordinates.reserve(variations.size());
  for (const FontVariation& variation : variations)
    coordinates.push_back({SkSetFourByteTag(variation.tag[0], variation.tag[1],
                                            variation.tag[2], variation.tag[3]),
                           variation.value});
  SkFontArguments fontArguments;
  fontArguments.setVariationDesignPosition(
      {coordinates.data(), static_cast<int>(coordinates.size())});
  sk_sp<SkTypeface> clone = base->makeClone(fontArguments);
  // Non-variable faces (or failed clones) resolve to the base: the axes are
  // simply inert, matching CSS font-variation-settings behavior.
  return clone ? clone : base;
}

}  // namespace

Face FontContext::variedTypeface(const Face& base,
                                 std::span<const FontVariation> variations) {
  const sk_sp<SkTypeface> resolvedBase = toSk(base ? base : defaultTypeface());
  if (variations.empty() || !resolvedBase) return resolvedBase;

  VariedTypefaceKey key;
  key.baseTypefaceId = resolvedBase->uniqueID();
  key.variationBytes.assign(reinterpret_cast<const char*>(variations.data()),
                            variations.size() * sizeof(FontVariation));
  auto cachedClone = m_impl->variedTypefaces.find(key);
  if (cachedClone != m_impl->variedTypefaces.end()) return cachedClone->second;

  sk_sp<SkTypeface> clone = cloneAt(resolvedBase, variations);
  m_impl->variedTypefaces.emplace(std::move(key), clone);
  return clone;
}

Face FontContext::variedTypefaceTransient(
    const Face& base, std::span<const FontVariation> variations) {
  const sk_sp<SkTypeface> resolvedBase = toSk(base ? base : defaultTypeface());
  if (variations.empty() || !resolvedBase) return resolvedBase;
  // Neither read nor written: the memo above is the ONLY thing that retains
  // a clone, so a coordinate that comes through here can never end up in it
  // — not on this call and not on a later one, because nothing hands the
  // returned face back to variedTypeface(). Probing the memo first would be
  // a legal read, and is skipped anyway: building the key allocates, which
  // on a per-glyph, per-frame path costs more than the probe saves.
  return cloneAt(resolvedBase, variations);
}

size_t FontContext::variedTypefaceCount() const {
  return m_impl->variedTypefaces.size();
}

bool FontContext::axisIsAdvanceInvariant(const Face& base,
                                         const char (&axisTag)[5]) {
  const sk_sp<SkTypeface> face = toSk(base ? base : defaultTypeface());
  if (!face) return false;
  const SkFourByteTag wanted =
      SkSetFourByteTag(axisTag[0], axisTag[1], axisTag[2], axisTag[3]);
  const int axisCount = face->getVariationDesignParameters({});
  if (axisCount <= 0) return false;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)axisCount);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  const SkFontParameters::Variation::Axis* axis = nullptr;
  for (const auto& candidate : axes)
    if (candidate.tag == wanted) {
      axis = &candidate;
      break;
    }
  if (!axis) return false;

  // Sample every glyph's advance at both extremes (capped — a CJK face's
  // first thousand glyphs are plenty to catch a moving advance).
  const FontVariation lo(axisTag, axis->min), hi(axisTag, axis->max);
  sk_sp<SkTypeface> faceLo = variedTypeface(face, {&lo, 1});
  sk_sp<SkTypeface> faceHi = variedTypeface(face, {&hi, 1});
  if (!faceLo || !faceHi) return false;
  const int glyphCount = std::min(face->countGlyphs(), 1024);
  if (glyphCount <= 0) return false;
  std::vector<SkGlyphID> glyphs((size_t)glyphCount);
  for (int i = 0; i < glyphCount; ++i) glyphs[(size_t)i] = (SkGlyphID)i;
  constexpr float kProbeSize = 64.0f;
  SkFont fontLo(faceLo, kProbeSize), fontHi(faceHi, kProbeSize);
  std::vector<SkScalar> widthsLo((size_t)glyphCount),
      widthsHi((size_t)glyphCount);
  fontLo.getWidths(SkSpan(glyphs.data(), glyphs.size()),
                   SkSpan(widthsLo.data(), widthsLo.size()));
  fontHi.getWidths(SkSpan(glyphs.data(), glyphs.size()),
                   SkSpan(widthsHi.data(), widthsHi.size()));
  for (int i = 0; i < glyphCount; ++i)
    if (std::abs(widthsLo[(size_t)i] - widthsHi[(size_t)i]) > 0.001f)
      return false;
  return true;
}

float FontContext::glyphAdvanceEm(const Face& base, uint16_t glyph,
                                  bool vertical) {
  const sk_sp<SkTypeface> face = toSk(base ? base : defaultTypeface());
  if (!face) return 0;
  Impl::TypefaceRecord& record = m_impl->recordForTypeface(face);
  // HarfBuzz points y up, so a vertical advance comes back negative for a
  // pen that runs down the page; the shaper negates it for the same reason.
  const hb_position_t advance =
      vertical ? -hb_font_get_glyph_v_advance(record.harfBuzzFont, glyph)
               : hb_font_get_glyph_h_advance(record.harfBuzzFont, glyph);
  // The font is scaled to its own units per em, so the raw position IS the
  // design-unit advance.
  return (float)advance / (float)record.unitsPerEm;
}

void FontContext::purgeShapeCache() { m_impl->shapeCache.clear(); }

void FontContext::purgeAllCaches() {
  m_impl->shapeCache.clear();
  m_impl->destroyTypefaceRecords();
  m_impl->fallbackTypefaces.clear();
  m_impl->variedTypefaces.clear();
  m_impl->glyphProfiles.clear();
  m_impl->referenceGaps.clear();
  m_impl->zeroAdvanceEms.clear();
  m_impl->familyTypefaces.clear();
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

const FontContext::Stats& FontContext::stats() const { return m_impl->stats; }

void FontContext::resetStats() { m_impl->stats = {}; }

}  // namespace sigil::weave
