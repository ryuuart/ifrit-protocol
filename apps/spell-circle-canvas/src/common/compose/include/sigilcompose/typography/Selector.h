#pragma once

/** @file
 * SigilCompose typography — the two selector forms that address something
 * of THIS library: `sel::style`, a run of a `weave::rich()` value written
 * under a name, and `sel::inFrame`, one frame of a chain named by its
 * `Element::key`.
 *
 * The selector VALUE and every form that addresses the text itself —
 * `weave::sel::word`, `line`, `sentence`, `range`, `regex`, `text`, `each`
 * and the `|`, `&`, `!` combinators — are SigilWeave's, in
 * `<sigilweave/query/Selector.h>`. These two are here because what they
 * name is here: a name the content declared, and the key of a node in this
 * tree. Both build the same `weave::Selector`, so either composes with any
 * weave form as a plain intersection or union.
 */

#include <sigilweave/query/Selector.h>

#include <string>
#include <string_view>

namespace sigil::compose {

/** The selector forms whose subject is a compose description. */
namespace sel {

/** EVERY RUN DRESSED UNDER THIS NAME — the runs a `weave::rich()` value
 *  added with `add(utf8, styleName)`, addressed by the name rather than by
 *  the words they happen to contain.
 *
 *  This is the treatment as a handle: a glossary set in one registered
 *  style stays addressable when the copy changes, where naming the literal
 *  text means editing the selector every time an author edits a sentence.
 *
 *  It addresses the run's TEXT, so it survives everything that changes what
 *  that text looks like: re-registering the name against a different
 *  `weave::StyleSet` entry, or a `spanPaint`/`spanStyle` cutting across it,
 *  leaves the same runs selected.
 *
 *  ONLY A NAMED `rich()` RUN CARRIES A NAME. Plain `text(utf8, style)`, a
 *  `rich()` run given a style directly, and the `shared_ptr<Paragraph>`
 *  overload have none, so this addresses nothing there — as does a name no
 *  run was written with. Either way it selects nothing and warns once per
 *  name. */
[[nodiscard]] inline sigil::weave::Selector style(std::string_view name) {
  // A style name is ASCII-or-whatever the author typed, and the needle slot
  // holds UTF-8 bytes; the resolver compares it against the same bytes a
  // run's name was written with, so no transcoding is involved either way.
  return sigil::weave::Selector::of(
      {.kind = sigil::weave::Selector::Kind::Named,
       .pattern = std::u8string((const char8_t*)name.data(), name.size())});
}

/** EVERYTHING THE NAMED FRAME HOLDS — the frame-local address, since every
 *  other form numbers the story.
 *
 *      sel::inFrame("b") & weave::sel::line(0)  // no line: line 0 is in a
 *      sel::inFrame("b")                        // the text frame b got
 *
 *  Resolved on the leaf being addressed and nowhere else: it selects
 *  everything on the frame whose `key` it names and nothing anywhere else,
 *  so it composes with the story-wide forms as a plain intersection. A key
 *  no frame carries selects nothing and warns once — a frame-local address
 *  that quietly became story-wide is the silent no-op this vocabulary
 *  refuses. */
[[nodiscard]] inline sigil::weave::Selector inFrame(std::string_view key) {
  // The key rides the needle slot the same way a style name does, and for
  // the same reason: it is compared against the bytes the frame's key() was
  // written with, so no transcoding is involved either way.
  return sigil::weave::Selector::of(
      {.kind = sigil::weave::Selector::Kind::Scope,
       .pattern = std::u8string((const char8_t*)key.data(), key.size())});
}

}  // namespace sel

}  // namespace sigil::compose
