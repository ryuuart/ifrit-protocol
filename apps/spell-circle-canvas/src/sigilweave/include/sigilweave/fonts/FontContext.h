#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * The per-thread service object at the center of the pipeline: font
 * management (HarfBuzz faces, variable-font clones, fallback resolution)
 * and the content-addressed word shape cache, with observable Stats for
 * tests and benchmarks. Create one FontContext per layout thread and hand
 * it to every Paragraph / layoutParagraph call.
 */

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>

#include "sigilweave/style/Style.h"

namespace sigil::weave {

struct ShapedWord;

/// Per-thread service object owning the shaping and font-resolution caches:
///   - hb_face/hb_font per SkTypeface (font data is parsed once, ever)
///   - per-(typeface, code point, language) glyph coverage + font fallback
///   - the word shape cache (see Shaper.h)
///
/// None of it is locked: create one FontContext per layout thread. All
/// caches key off SkTypeface::uniqueID(), so typefaces must outlive the
/// context or be consistently owned by it (they are ref'd where retained).
class FontContext {
 public:
  /** Chooses a fallback for a code point missing from the primary
   * typeface; returning null leaves the primary in place and therefore
   * permits a missing glyph. It is called only on a fallback-cache miss.
   * @trap The language tag is borrowed for the call and is NOT
   * guaranteed NUL-terminated — copy it before any C API sees it.
   */
  using FallbackResolver = std::function<sk_sp<SkTypeface>(
      SkFontMgr& fontManager, const SkTypeface& primaryTypeface,
      int32_t codePoint, std::string_view languageTag)>;

  /** Constructs a context using @p fontManager for fallback resolution.
   * @p defaultTypeface serves styles with a null typeface, and when it is
   * itself null the manager's default family is matched lazily; a null
   * @p fallbackResolver selects the platform fallback cascade.
   */
  explicit FontContext(sk_sp<SkFontMgr> fontManager,
                       sk_sp<SkTypeface> defaultTypeface = nullptr,
                       FallbackResolver fallbackResolver = {});
  ~FontContext();

  FontContext(const FontContext&) = delete;
  FontContext& operator=(const FontContext&) = delete;

  /** Returns the font manager used for fallback resolution. */
  [[nodiscard]] SkFontMgr* fontManager() const;
  /** Returns the typeface used when a shaping style has none. */
  [[nodiscard]] const sk_sp<SkTypeface>& defaultTypeface() const;

  /** THE FACE @p family NAMES AT @p style, found through the font manager
   *  and kept: two asks for one family and style return the SAME face, so
   *  its unique id is a stable shape-cache identity. The manager picks the
   *  family's nearest face to the style, which is an upright one where the
   *  family has no italic.
   *  @trap Null where the manager knows no family of that name, never
   *  another family standing in for it; an empty name is null too. */
  [[nodiscard]] sk_sp<SkTypeface> familyTypeface(
      std::string_view family, SkFontStyle style = SkFontStyle::Normal());

  /** Returns the typeface to shape @p codePoint with: the primary, or
   * the default, when it covers the code point, and otherwise the
   * resolver's match. Memoized per primary, code point and language.
   */
  [[nodiscard]] sk_sp<SkTypeface> resolveTypeface(
      const sk_sp<SkTypeface>& primaryTypeface, int32_t codePoint,
      const char* languageTag);

  /** Returns the MEMOIZED varied clone of @p base for @p variations —
   * @p base itself, or the context default when it is null, when the
   * variations are empty or cloning fails. The memo is the correctness
   * mechanism and not a speed-up: identical requests return the SAME
   * object, so its unique id is a stable shape-cache identity.
   */
  [[nodiscard]] sk_sp<SkTypeface> variedTypeface(
      const sk_sp<SkTypeface>& base, std::span<const FontVariation> variations);

  /** Returns a varied clone of @p base that this context does NOT retain:
   * it lives exactly as long as the caller's reference. It is for a
   * coordinate that is not on a ladder, where the memoized call's uncapped
   * table would retain a clone per frame forever.
   * @trap A face from here has no stable identity, so it must never key a
   * cache that outlives the frame — `ShapingStyle::variations` above all. */
  [[nodiscard]] sk_sp<SkTypeface> variedTypefaceTransient(
      const sk_sp<SkTypeface>& base, std::span<const FontVariation> variations);

  /** Returns how many varied clones this context is retaining — the memo
   * `variedTypeface` fills and `variedTypefaceTransient` does not. Tests
   * assert a bound on it; nothing else should read it. */
  [[nodiscard]] size_t variedTypefaceCount() const;

  /** TRUE exactly when driving @p axisTag anywhere in its design range
   *  leaves every glyph advance of @p base unchanged, advances sampled at
   *  both extremes — the gate an axis must pass to be animated at DRAW
   *  time rather than re-shaped. FALSE when the face lacks the axis. */
  [[nodiscard]] bool axisIsAdvanceInvariant(const sk_sp<SkTypeface>& base,
                                            const char (&axisTag)[5]);

  /** The pen travel @p glyph adds in @p base, as a fraction of the em,
   *  along the axis a run of the given orientation steps on: the
   *  horizontal advance for a level run, the VERTICAL advance for an
   *  upright column. Ems rather than pixels, because a caller comparing
   *  two advances is asking about the FACE. Zero when @p base and the
   *  context default are both null. */
  [[nodiscard]] float glyphAdvanceEm(const sk_sp<SkTypeface>& base,
                                     SkGlyphID glyph, bool vertical);

  /** Drops every cached shape result (not HarfBuzz fonts or fallback map). */
  void purgeShapeCache();

  /** Drops every cache this context owns: shape results, the
   * per-typeface HarfBuzz faces and fonts, coverage and fallback memos,
   * varied clones, the faces found by family name, optical-kerning
   * measurements and interned language ids. Safe while shaped words are
   * outstanding, since they own their data; the next analysis re-fills at
   * first-use cost.
   */
  void purgeAllCaches();

  /// Cache observability for tests and benchmarks.
  struct Stats {
    uint64_t shapeCalls = 0;       ///< actual hb_shape invocations
    uint64_t shapeCacheHits = 0;   ///< words served from the cache
    uint64_t fallbackQueries = 0;  ///< fallback-resolver invocations
    uint64_t coverageQueries = 0;  ///< uncached glyph-coverage probes
    uint64_t opticalProfileQueries =
        0;  ///< glyph outlines measured for kerning
    uint64_t opticalReferenceQueries = 0;  ///< per-face reference gaps measured
  };
  /** Returns cumulative cache and shaping counters. */
  [[nodiscard]] const Stats& stats() const;
  /** Resets observable counters without clearing any caches. */
  void resetStats();

 private:
  friend std::shared_ptr<const ShapedWord> shapeWord(FontContext&,
                                                     const ShapingStyle&,
                                                     const sk_sp<SkTypeface>&,
                                                     std::u16string_view,
                                                     uint32_t, bool, bool);
  /// Reads the per-face zero advance this context measures once and keeps.
  friend float zeroAdvanceOf(const TextStyle&, FontContext&);
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::weave
