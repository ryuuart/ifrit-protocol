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
 *  default where it carries none. A null @p family keeps the family of
 *  the face in force. The weight asked for is the font's own `weight`
 *  where it states one, the face's otherwise; its width is the face's.
 *
 *  An italic is the family's italic face, else its `ital` axis at 1,
 *  else an oblique of 14 degrees on `slnt`, which is said once; an
 *  upright style sets an `ital` axis the face has back to 0.
 *  @trap A family the context cannot find leaves the face in force and
 *  says so once per name. A face in force that is not the one its own
 *  family's name finds at its style — one loaded from a file — is its own
 *  only face, so an italic there is its axis or the lean. */
void chooseFace(sigil::weave::FontContext& fonts, sigil::weave::Type& font,
                const std::string* family, bool italic);

/** THE FACE A RANGE'S PARTIAL CHOOSES — a span's, or a run a sheet names —
 *  written into @p partial's face, slant and variations over @p base, the
 *  style the range is set in, as `chooseFace` chooses a node's: where the
 *  partial names @p family, where @p italic differs from
 *  @p italicInForce, where it states a face under an italic, or where it
 *  states a weight under @p familyInForce, the leaf's family by name. The
 *  weight asked for is the partial's, else the one @p base is set at.
 *  Leaves @p partial alone otherwise. */
void chooseRangeFace(sigil::weave::FontContext& fonts,
                     sigil::weave::Type& partial,
                     const sigil::weave::TextStyle& base,
                     const std::string* familyInForce, bool italicInForce,
                     const std::string* family, std::optional<bool> italic);

}  // namespace sigil::compose::detail
