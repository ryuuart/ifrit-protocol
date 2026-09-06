#pragma once

/** @file
 * The gallery's own fallback resolver: which face a code point no primary
 * face carries is set in. It is here rather than in the engine because
 * what a MACHINE has installed is the application's question — the engine
 * asks a caller and this is the gallery's answer.
 */

#include <include/core/SkFontMgr.h>
#include <sigilweave/fonts/FontContext.h>

#include <QString>

namespace gallery {

/** The face @p family names, as Qt resolves a family to one. Null when no
 *  manager or no name. */
sk_sp<SkTypeface> resolveGalleryTypeface(SkFontMgr* fontManager,
                                         const QString& family);

/** A resolver over @p fontManager: the serif companion for a Han run, the
 *  cuneiform face where the machine hides it from the family walk, and
 *  the manager's own match for everything else. */
sigil::weave::FontContext::FallbackResolver makeGalleryFallbackResolver(
    SkFontMgr& fontManager);

}  // namespace gallery
