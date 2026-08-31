#pragma once

/** @file
 * @ingroup shaping
 *
 * The per-thread service object at the center of the pipeline: font
 * management (HarfBuzz faces, variable-font clones, fallback resolution)
 * and the content-addressed word shape cache, with observable Stats for
 * tests and benchmarks. Create one FontContext per layout thread and hand
 * it to every Paragraph / layoutParagraph call.
 */

#include <include/core/SkFontMgr.h>
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

/// Per-thread service object owning every cache the pipeline leans on:
///   - hb_face/hb_font per SkTypeface (font data is parsed once, ever)
///   - per-(typeface, code point, language) glyph coverage + font fallback
///   - the word shape cache (see Shaper.h)
///
/// None of it is locked: create one FontContext per layout thread. All
/// caches key off SkTypeface::uniqueID(), so typefaces must outlive the
/// context or be consistently owned by it (they are ref'd where retained).
class FontContext {
 public:
  /** Chooses a fallback for a code point missing from `primaryTypeface`.
   * Returning null leaves the primary in place (and therefore permits a
   * missing glyph). The default resolver uses
   * SkFontMgr::matchFamilyStyleCharacter; applications can instead encode
   * their own family lists, script preferences, or platform cascade policy.
   * The resolver is called only on a fallback-cache miss.
   *
   * `languageTag` is a borrowed view valid only for the duration of the
   * call, and it is NOT guaranteed to be NUL-terminated — copy it before
   * handing it to any C API that expects a C string (never pass `.data()`
   * through directly).
   */
  using FallbackResolver = std::function<sk_sp<SkTypeface>(
      SkFontMgr& fontManager, const SkTypeface& primaryTypeface,
      int32_t codePoint, std::string_view languageTag)>;

  /** Constructs a context using `fontManager` for fallback resolution.
   * `defaultTypeface` is used for styles with a null typeface; when itself
   * null, the font manager's default family is matched lazily. A null
   * `fallbackResolver` selects the font manager's platform fallback cascade.
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

  /** Returns the typeface to shape `codePoint` with: `primaryTypeface` (or
   * the default) when it covers the code point, otherwise the configured
   * fallback resolver's match. Memoized per (primary, codepoint, language),
   * so warm itemization passes never invoke the resolver.
   */
  [[nodiscard]] sk_sp<SkTypeface> resolveTypeface(
      const sk_sp<SkTypeface>& primaryTypeface, int32_t codePoint,
      const char* languageTag);

  /** Returns the memoized varied clone of `base` for `variations` — `base`
   * itself (or the context default when `base` is null) when `variations`
   * is empty or cloning fails.
   *
   * Memoization is the correctness mechanism, not just a speed-up: repeated
   * identical requests return the *same* SkTypeface object, so its uniqueID
   * is a stable shape-cache identity — a fresh makeClone per shape would
   * mint a new id every time and defeat the cache. The pipeline calls this
   * for every ShapingStyle with a non-empty `variations`; applications only
   * need it to inspect the resolved face themselves.
   */
  [[nodiscard]] sk_sp<SkTypeface> variedTypeface(
      const sk_sp<SkTypeface>& base, std::span<const FontVariation> variations);

  /** Returns a varied clone of `base` for `variations` that this context does
   * NOT retain — it lives exactly as long as the caller's reference.
   *
   * For a coordinate that is not on a ladder. `variedTypeface()`'s memo is
   * keyed on the coordinate's exact bytes and has no cap and no eviction, so
   * a caller that feeds it a continuously varying value adds a permanently
   * retained clone per frame forever; that is a leak whether or not the
   * caller ever asks for the same value twice. The cost model here is the
   * honest one for such a caller: the face is built fresh on every call and
   * its glyphs are rasterized fresh, and the price is CONSTANT per frame
   * rather than growing with how long the process has been running.
   *
   * What that bounds and what it does not:
   *   - Bounded: this context retains nothing. The clone and everything
   *     hanging off it are released when the last caller reference drops.
   *   - NOT bounded by this class: Skia's own strike cache sits underneath
   *     and keys on the face, so a distinct coordinate still mints distinct
   *     glyph strikes. That cache evicts against its own byte budget, so the
   *     memory stays capped, but the rasterization is genuinely repeated.
   *
   * A face from here has no stable identity — two calls with the same
   * coordinate return different objects — so it must never be used where an
   * identity is a cache key that outlives the frame. That rules out
   * ShapingStyle::variations, whose whole point is a stable shape-cache
   * identity; it suits a draw-time drive, where the identity is only a
   * batch key within one frame's draw.
   */
  [[nodiscard]] sk_sp<SkTypeface> variedTypefaceTransient(
      const sk_sp<SkTypeface>& base, std::span<const FontVariation> variations);

  /** Returns how many varied clones this context is retaining — the memo
   * `variedTypeface()` fills and `variedTypefaceTransient()` does not.
   * Tests assert a bound on it; nothing else should read it. */
  [[nodiscard]] size_t variedTypefaceCount() const;

  /** TRUE iff driving @p axisTag anywhere in its design range leaves every
   *  glyph advance of @p base unchanged (advances sampled at both axis
   *  extremes) — the VariationDrive gate: an advance-invariant axis (GRAD,
   *  most fonts' slnt) may be animated at DRAW time without reshaping,
   *  while wght moves advances on most fonts and must re-shape instead.
   *  FALSE when the face lacks the axis entirely. */
  [[nodiscard]] bool axisIsAdvanceInvariant(const sk_sp<SkTypeface>& base,
                                            const char (&axisTag)[5]);

  /** The pen travel @p glyph adds in @p base, as a fraction of the em, along
   *  the axis a run of the given orientation steps on: the horizontal
   *  advance for a level run, the VERTICAL advance for an upright column.
   *
   *  Read from the same font the shaper reads, so it is the number the pen
   *  actually moves by — Skia's glyph metrics carry no vertical advance at
   *  all. A face with no vertical metrics table answers with the one
   *  vertical advance it gives every glyph, which is a fact about that face
   *  and not a failure: in it, every glyph does step the same distance down
   *  a column.
   *
   *  Ems rather than pixels because the pixel advance scales with the size
   *  the glyph is drawn at, and a caller comparing two advances is asking
   *  about the FACE. Zero when @p base and the context default are both
   *  null.
   */
  [[nodiscard]] float glyphAdvanceEm(const sk_sp<SkTypeface>& base,
                                     SkGlyphID glyph, bool vertical);

  /** Drops every cached shape result (not HarfBuzz fonts or fallback map). */
  void purgeShapeCache();

  /** Drops every cache this context owns: shape results, the per-typeface
   * HarfBuzz faces/fonts, glyph-coverage/fallback memos, and interned
   * language ids.
   *
   * For long-lived processes whose typeface population changes over time —
   * the per-typeface and per-(typeface, codepoint) maps are otherwise never
   * pruned. Safe while ShapedWords are outstanding (they own their data);
   * the next analysis simply re-fills at first-use cost.
   */
  void purgeAllCaches();

  /// Cache observability for tests and benchmarks.
  struct Stats {
    uint64_t shapeCalls = 0;       ///< actual hb_shape invocations
    uint64_t shapeCacheHits = 0;   ///< words served from the cache
    uint64_t fallbackQueries = 0;  ///< fallback-resolver invocations
    uint64_t coverageQueries = 0;  ///< uncached glyph-coverage probes
  };
  /** Returns cumulative cache and shaping counters. */
  [[nodiscard]] const Stats& stats() const;
  /** Resets observable counters without clearing any caches. */
  void resetStats();

  /// Implementation access for Shaper.cpp / Paragraph.cpp only.
  struct Impl;
  /** Returns internal per-thread services used by pipeline implementation. */
  [[nodiscard]] Impl& impl() { return *m_impl; }

 private:
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::weave
