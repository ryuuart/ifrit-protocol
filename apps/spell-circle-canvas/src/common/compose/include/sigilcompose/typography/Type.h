#pragma once

/** @file
 * SigilCompose typography — the compose-side spellings of a text style and
 * a face: `type()` builds a weave::TextStyle out of the numbers a call site
 * has, as a designated-init aggregate rather than a positional signature,
 * and `pickFace()` resolves the first installed family of a fallback
 * chain. Neither decides anything — there is no type scale and no opinion
 * about which face stands in for which; a study's decisions are its own.
 *
 * `pickFace()` walks the system font manager, so a translation unit that
 * calls it links SigilWeavePorts.
 */

#include <include/core/SkColor.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Style.h>

#include <initializer_list>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// Type — a designated-init facade over weave::TextStyle.

/** The parameters of a text style, as a designated-init aggregate rather
 *  than a positional signature.
 *
 *  Every caller needs a face, a size and a colour, and then some subset of
 *  tracking, condensation and variable-font axes — and which subset differs
 *  per call site. A positional helper cannot grow another parameter without
 *  breaking every existing call; an aggregate can, and
 *  `Transition{.duration = …}` and `Grid{.columns = …}` are already spelled
 *  that way here.
 *
 *      text(toU8(s), type({.face = faceMono, .size = 10.5f,
 *                          .color = kInk, .track = 1.2f}))
 *
 *  Anything not here is added to the RETURNED style — a mask-filter blur, a
 *  kPlus blend, a mandatory underlay. Those are per-artefact decisions and
 *  stay at the call site. */
struct Type {
  /** null → the FontContext's default family (plus its fallback chain). */
  sk_sp<SkTypeface> face;
  float size = 16.0f;
  SkColor4f color = {0, 0, 0, 1};
  /** px of tracking added after each cluster (ShapingStyle::letterSpacing).
   *  NOT per-mille: a reference that quotes tracking in per-mille is
   *  converted at the call site, where the unit's own em size is known. */
  float track = 0.0f;
  /** Horizontal condensation (ShapingStyle::scaleX) — how to condense a
   *  face that has no `wdth` axis to ask instead. */
  float condense = 1.0f;
  /** > 0 → a `wght` variable-font axis. Weight changes advances, so this
   *  participates in shaping identity. */
  float weight = 0.0f;
  /** != 0 → a `slnt` axis (negative leans right, per the OpenType sign). */
  float slant = 0.0f;
  /** Hard-edged glyph rasterisation (ShapingStyle::aliased). Skia takes
   *  edging from the SkFont, never from the paint, so this is the only way
   *  to ask — `paint.setAntiAlias(false)` is silently ignored on text. */
  bool aliased = false;
  /** The glyph paint's own antialias flag (edges of strokes/decorations on
   *  the paint, not the glyph edging above). */
  bool antiAlias = true;
  /** Anything else in design space — appended after weight/slant, so the
   *  order is stable and two styles built the same way share one
   *  varied-face memo entry. */
  std::vector<sigil::weave::FontVariation> variations;
};

/** Type{} → weave::TextStyle. */
inline sigil::weave::TextStyle type(const Type& t) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = t.face;
  s.shaping.fontSize = t.size;
  s.shaping.letterSpacing = t.track;
  s.shaping.scaleX = t.condense;
  s.shaping.aliased = t.aliased;
  s.paint.foreground.setColor4f(t.color, nullptr);
  s.paint.foreground.setAntiAlias(t.antiAlias);
  if (t.weight > 0) s.variation("wght", t.weight);
  if (t.slant != 0) s.variation("slnt", t.slant);
  for (const sigil::weave::FontVariation& v : t.variations)
    s.shaping.variations.push_back(v);
  return s;
}

// ---------------------------------------------------------------------------
// Faces — the fallback chain, once.

/** The first of @p families the system font manager resolves, at @p style.
 *
 *  The list is the point: reconstructing a reference names a face that may
 *  not be installed on the machine running the code, so callers pass the
 *  face they want followed by the stand-ins they will accept.
 *
 *  The last resort is the default family AT THE REQUESTED STYLE, not at
 *  `SkFontStyle::Normal()` — falling back to Normal would silently drop the
 *  weight the caller asked for.
 *
 *  `matchFamilyStyle` walks the system font list; hold the result in a
 *  `static` rather than calling this per frame. */
inline sk_sp<SkTypeface> pickFace(std::initializer_list<const char*> families,
                                  SkFontStyle style = SkFontStyle::Normal()) {
  sk_sp<SkFontMgr> mgr = sigil::weave::ports::systemFontManager();
  if (!mgr) return nullptr;
  for (const char* family : families)
    if (sk_sp<SkTypeface> face = mgr->matchFamilyStyle(family, style))
      return face;
  return mgr->matchFamilyStyle(nullptr, style);
}

/** `pickFace` spelled with a weight and a slant, for the (common) case
 *  where the caller has those two numbers and not an SkFontStyle. */
inline sk_sp<SkTypeface> pickFace(
    std::initializer_list<const char*> families, int weight,
    SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant) {
  return pickFace(families,
                  SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
}

}  // namespace sigil::compose
