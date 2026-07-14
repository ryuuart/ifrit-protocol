#pragma once

#include "Style.h"

#include <include/core/SkFontMgr.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <cstdint>
#include <memory>

namespace textflow {

// Per-thread service object owning every cache the pipeline leans on:
//   - hb_face/hb_font per SkTypeface (font data is parsed once, ever)
//   - per-(typeface, code point) glyph coverage + system font fallback
//   - the word shape cache (see Shaper.h)
//
// None of it is locked: create one FontContext per layout thread. All caches
// key off SkTypeface::uniqueID(), so typefaces must outlive the context or be
// consistently owned by it (they are ref'd where retained).
class FontContext {
public:
  // `defaultTypeface` is used for styles with a null typeface; when itself
  // null, the font manager's default family is matched lazily.
  explicit FontContext(sk_sp<SkFontMgr> fontManager,
                       sk_sp<SkTypeface> defaultTypeface = nullptr);
  ~FontContext();

  FontContext(const FontContext &) = delete;
  FontContext &operator=(const FontContext &) = delete;

  /** Returns the font manager used for fallback resolution. */
  [[nodiscard]] SkFontMgr *fontManager() const;
  /** Returns the typeface used when a shaping style has none. */
  [[nodiscard]] const sk_sp<SkTypeface> &defaultTypeface() const;

  // Typeface to shape `codePoint` with: `primary` (or the default) when it
  // covers the code point, otherwise a system-fallback match via
  // SkFontMgr::matchFamilyStyleCharacter. Memoized per (primary, codepoint),
  // so warm itemization passes never touch CoreText.
  [[nodiscard]] sk_sp<SkTypeface>
  resolveTypeface(const sk_sp<SkTypeface> &primaryTypeface, int32_t codePoint,
                  const char *languageTag);

  // Drops every cached shape result (not HarfBuzz fonts or fallback map).
  void purgeShapeCache();

  // Cache observability for tests and benchmarks.
  struct Stats {
    uint64_t shapeCalls = 0;      // actual hb_shape invocations
    uint64_t shapeCacheHits = 0;  // words served from the cache
    uint64_t fallbackQueries = 0; // matchFamilyStyleCharacter calls
    uint64_t coverageQueries = 0; // uncached glyph-coverage probes
  };
  /** Returns cumulative cache and shaping counters. */
  [[nodiscard]] const Stats &stats() const;
  /** Resets observable counters without clearing any caches. */
  void resetStats();

  // Implementation access for Shaper.cpp / Paragraph.cpp only.
  struct Impl;
  /** Returns internal per-thread services used by pipeline implementation. */
  [[nodiscard]] Impl &impl() { return *m_impl; }

private:
  std::unique_ptr<Impl> m_impl;
};

} // namespace textflow
