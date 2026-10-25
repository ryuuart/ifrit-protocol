#pragma once

/** @file
 * The face a family name and an italic choose, found through the
 * composer's font context when the cascade resolves a node or a span.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/TextStyle.h>
#include <sigilweave/style/Type.h>

#include <optional>
#include <string>

namespace sigil::compose::detail {

/** WRITES INTO @p font THE FACE @p family AND @p italic CHOOSE, over the
 *  face @p font already carries — the one in force, or the context's
 *  default where it carries none. @p family is a CSS family list, and the
 *  face is the first of its families the context has, found at the style
 *  asked for; the list is walked once for the run, never per character,
 *  whose fallback stays the context's. A null @p family keeps the family
 *  of the face in force. The weight asked for is the font's own `weight`
 *  where it states one, the face's otherwise; its width is the face's.
 *
 *  An italic is the family's italic face, else its `ital` axis at 1,
 *  else an oblique of 14 degrees on `slnt`, which is said once; an
 *  upright style sets an `ital` axis the face has back to 0.
 *  @trap A list naming no family the context can find leaves the face in
 *  force and says so once per list. A face in force that is not the one its own
 *  family's name finds at its style — one loaded from a file — is its own
 *  only face, so an italic there is its axis or the lean. */
void chooseFace(sigil::weave::FontContext& fonts, sigil::weave::Type& font,
                const std::string* family, bool italic);

/** THE FACE OF @p family WHOSE FONT NAMES ITS STYLE @p styleName, found
 *  through the context — its adopted faces answering before its font
 *  manager — as `fontFamily(family, styleName)` names one. @p family is a
 *  CSS family list, and the face is the named style of the first of its
 *  families that has one.
 *  @trap Null where no family of the list has a style of that name, which
 *  is said once naming both; the family then chooses at the style in
 *  force, as it would with no name. */
sk_sp<SkTypeface> namedStyleFace(sigil::weave::FontContext& fonts,
                                 const std::string& family,
                                 const std::string& styleName);

/** THE FACE A RANGE'S PARTIAL CHOOSES — a span's, or a run a sheet names —
 *  written into @p partial's face, slant and variations over @p base, the
 *  style the range is set in, as `chooseFace` chooses a node's: where the
 *  partial names @p family, where @p italic differs from
 *  @p italicInForce, where it states a face under an italic, or where it
 *  states a weight under @p familyInForce, the leaf's family by name. The
 *  weight asked for is the partial's, else the one @p base is set at.
 *  A @p styleName beside @p family chooses the face its font names so,
 *  upright of any lean, where the family has one. Leaves @p partial alone
 *  otherwise. */
void chooseRangeFace(sigil::weave::FontContext& fonts,
                     sigil::weave::Type& partial,
                     const sigil::weave::TextStyle& base,
                     const std::string* familyInForce, bool italicInForce,
                     const std::string* family, std::optional<bool> italic,
                     const std::string* styleName = nullptr);

}  // namespace sigil::compose::detail
