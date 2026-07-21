#pragma once

/** @file
 * @ingroup shaping
 *
 * Lower-level shaping types the pipeline is built on. A ShapedWord is the
 * immutable, cache-shared result of shaping one word-sized segment with one
 * resolved typeface / script / direction (glyphs, advances, clusters, and a
 * lazily built origin-relative SkTextBlob). Most callers never include this
 * directly — Paragraph owns the shaping and ParagraphLayout owns the
 * placement; reach for it only to inspect or reuse individual glyph runs.
 */

#include "Style.h"

#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTextBlob.h>
#include <include/core/SkTypeface.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace sigil::weave {

class FontContext;

/// Four-byte script tag in HarfBuzz encoding (hb_script_t), e.g. 'Latn'.
/// Kept as a plain uint32 so HarfBuzz types stay out of the public headers.
using ScriptTag = uint32_t;

/// The immutable result of shaping one word-sized segment with one resolved
/// typeface / script / direction. Instances are shared out of the shape
/// cache; a layout never mutates one, it only decides where to draw it.
struct ShapedWord {
  sk_sp<SkTypeface> typeface; ///< resolved face the glyph IDs index into
  float fontSize = 0;         ///< px size every metric below is scaled to
  float scaleX = 1.0f;        ///< horizontal condensation baked into
                              ///< positions/advances (draw fonts must match)

  std::vector<uint16_t> glyphs;   ///< glyph IDs in `typeface`
  std::vector<SkPoint> positions; ///< pen-relative glyph origins
  std::vector<float> advances;    ///< per-glyph advance (letter spacing baked
                                  ///< in) — pen travel, used for curved lines
  std::vector<uint32_t> clusters; ///< UTF-16 index into the shaped text
  float advance = 0;              ///< total advance, letter spacing included

  /// Shaped top-to-bottom ('vert' forms, vertical metrics): positions stack
  /// glyphs downward from the origin (x centred on the column axis) and
  /// `advance`/`advances` measure vertical pen travel.
  bool vertical = false;

  /// Origin-relative blob built on first placement and reused by every frame
  /// and every layout thereafter (shape once, reposition forever). Mutable
  /// because building it is a pure cache fill on an otherwise-const value.
  mutable sk_sp<SkTextBlob> blobCache;
};

/// Shared handle to a cache-owned, immutable ShapedWord — cheap to copy and
/// safe to hold across layouts.
using ShapedWordRef = std::shared_ptr<const ShapedWord>;

/** Shapes `text` with HarfBuzz, going through FontContext's shape cache.
 * `typeface` must already be fallback-resolved (see
 * FontContext::resolveTypeface); `rightToLeft` selects the HarfBuzz direction
 * and `vertical` shapes top-to-bottom (mutually exclusive with rightToLeft).
 */
[[nodiscard]] ShapedWordRef
shapeWord(FontContext &fontContext, const ShapingStyle &style,
          const sk_sp<SkTypeface> &typeface, std::u16string_view text,
          ScriptTag script, bool rightToLeft, bool vertical = false);

/** Returns the shared origin-relative SkTextBlob for `word`, building and
 * memoizing it on first use. Cheap on every call after the first.
 */
[[nodiscard]] const sk_sp<SkTextBlob> &wordBlob(const ShapedWord &word);

/** SkFont configured the way SigilWeave shapes: unhinted, subpixel, linear
 * metrics — rendering must match shaping or positions drift.
 */
[[nodiscard]] SkFont makeFont(const sk_sp<SkTypeface> &typeface,
                              float fontSize, float scaleX = 1.0f);

} // namespace sigil::weave
