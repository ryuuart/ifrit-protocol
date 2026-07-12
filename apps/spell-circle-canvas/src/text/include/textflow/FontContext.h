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
//   - per-(typeface, codepoint) glyph coverage + system font fallback
//   - the word shape cache (see Shaper.h)
//
// None of it is locked: create one FontContext per layout thread. All caches
// key off SkTypeface::uniqueID(), so typefaces must outlive the context or be
// consistently owned by it (they are ref'd where retained).
class FontContext {
public:
  // `defaultTypeface` is used for styles with a null typeface; when itself
  // null, the font manager's default family is matched lazily.
  explicit FontContext(sk_sp<SkFontMgr> fontMgr,
                       sk_sp<SkTypeface> defaultTypeface = nullptr);
  ~FontContext();

  FontContext(const FontContext &) = delete;
  FontContext &operator=(const FontContext &) = delete;

  SkFontMgr *fontMgr() const;
  const sk_sp<SkTypeface> &defaultTypeface() const;

  // Typeface to shape `codepoint` with: `primary` (or the default) when it
  // covers the codepoint, otherwise a system-fallback match via
  // SkFontMgr::matchFamilyStyleCharacter. Memoized per (primary, codepoint),
  // so warm itemization passes never touch CoreText.
  sk_sp<SkTypeface> resolveTypeface(const sk_sp<SkTypeface> &primary,
                                    int32_t codepoint, const char *language);

  // Drops every cached shape result (not the hb fonts / fallback map).
  void purgeShapeCache();

  // Cache observability for tests and benchmarks.
  struct Stats {
    uint64_t shapeCalls = 0;        // actual hb_shape invocations
    uint64_t shapeCacheHits = 0;    // words served from the cache
    uint64_t fallbackQueries = 0;   // matchFamilyStyleCharacter calls
    uint64_t coverageQueries = 0;   // uncached glyph-coverage probes
  };
  const Stats &stats() const;
  void resetStats();

  // Implementation access for Shaper.cpp / Paragraph.cpp only.
  struct Impl;
  Impl &impl() { return *m_impl; }

private:
  std::unique_ptr<Impl> m_impl;
};

} // namespace textflow
