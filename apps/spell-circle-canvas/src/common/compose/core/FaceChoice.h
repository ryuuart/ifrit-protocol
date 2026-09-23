#pragma once

/** @file
 * The face a family name and an italic choose, found through the
 * composer's font context when the cascade resolves a node or a span.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Type.h>

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
 *  upright style sets any `ital` axis back to 0.
 *  @trap A family the context cannot find leaves the face in force and
 *  says so once per name. A face in force that no family of the
 *  context's holds — one loaded from a file — is its own only face. */
void chooseFace(sigil::weave::FontContext& fonts, sigil::weave::Type& font,
                const std::string* family, bool italic);

}  // namespace sigil::compose::detail
