#pragma once

/** @file
 * Document components with semantic roles and a stock type hierarchy. A role
 * supplies fallback type; a rule of a sheet in force naming the role styles
 * every element that carries it. Rules for authored classes and direct
 * font/paragraph declarations stand above the role's. Every result is an
 * Element.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilweave/paragraph/RichText.h>

#include <initializer_list>
#include <string_view>

namespace sigil::compose::document {

/** Inherited layout properties, read where each component lands. A document
 *  supplies fallback values; Element::var on any ancestor overrides them.
 *  Measure is 38 em, flow gap is 1 em, list gap is 0.4 em, and quote inset
 *  is 1 em. These are lengths, so a theme can use pixels or font units. */
inline constexpr std::string_view measure = "document.measure";
inline constexpr std::string_view gap = "document.gap";
inline constexpr std::string_view listGap = "document.listGap";
inline constexpr std::string_view quoteInset = "document.quoteInset";

/** A reading column with a bounded measure and inherited paragraph leading.
 *  The role is `article`; its gap and maximum width read the document's
 *  layout properties. Children can also be appended through children(). */
[[nodiscard]] Element article(std::initializer_list<Children> children = {});
/** A content group using the document's flow gap, without another measure
 *  limit or a new font. The role is `section`. */
[[nodiscard]] Element section(std::initializer_list<Children> children = {});

/** Heading levels 1 through 6. Their roles are `h1` through `h6` and their
 *  fallback sizes are 2, 1.5, 1.25, 1.1, 1 and 0.875 times the inherited
 *  type size. An invalid level throws std::out_of_range. */
[[nodiscard]] Text heading(int level, Utf8 words);
[[nodiscard]] Text h1(Utf8 words);
[[nodiscard]] Text h2(Utf8 words);
[[nodiscard]] Text h3(Utf8 words);
[[nodiscard]] Text h4(Utf8 words);
[[nodiscard]] Text h5(Utf8 words);
[[nodiscard]] Text h6(Utf8 words);

/** Prose in the inherited font and paragraph setting, with role `paragraph`.
Rich text
 *  stays one shaped passage; inline styles retain their own precedence. */
[[nodiscard]] Text paragraph(Utf8 words);
[[nodiscard]] Text paragraph(const weave::RichText& words);
/** Introductory prose at 1.125 em, role `lead`. */
[[nodiscard]] Text lead(Utf8 words);
/** Supporting prose at 0.875 em, role `caption`. */
[[nodiscard]] Text caption(Utf8 words);
/** A short identifying line at 0.875 em, role `label`. */
[[nodiscard]] Text label(Utf8 words);
/** An introductory label at 0.75 em with light tracking, role `eyebrow`. */
[[nodiscard]] Text eyebrow(Utf8 words);
/** A closing note at 0.875 em, role `footer`. */
[[nodiscard]] Text footer(Utf8 words);
/** Literal text in a monospace face, role `code`. Line breaks stay in the
 *  passage; its font, paragraph and measure are ordinary fluent overrides. */
[[nodiscard]] Text code(Utf8 words);

/** An indented content group, role `quote`. */
[[nodiscard]] Element quote(std::initializer_list<Children> children = {});
/** One quoted paragraph. */
[[nodiscard]] Element quote(Utf8 words);
/** A sequence of items, role `list`. Each item owns its marker; the list
 *  uses the inherited list gap. */
[[nodiscard]] Element list(std::initializer_list<Children> children = {});
/** A marker beside a body, with both aligned on their first baseline.
 *  The marker defaults to a bullet and retains role `marker`; the prose
 *  retains role `paragraph`. Nested content can use the Element overload. */
[[nodiscard]] Element item(Utf8 words, Utf8 marker = u8"\u2022");
[[nodiscard]] Element item(Element body, Utf8 marker = u8"\u2022");
/** A body followed by its caption, role `figure`. Empty captions are absent. */
[[nodiscard]] Element figure(Element body, Utf8 note = {});
/** A one-pixel rule in the inherited ink, role `rule`. */
[[nodiscard]] Element rule();

}  // namespace sigil::compose::document
