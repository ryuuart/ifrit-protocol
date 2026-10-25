#pragma once

/** @file
 * @ingroup compose-core
 *
 * The font and the ink, as verbs: the partial every passage under a node
 * is set in, CSS's longhands for the fields CSS names, and the colour or
 * paint the text and every mark naming no colour is painted in.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/FontStyle.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/PaintBox.h>
#include <sigilcompose/core/Var.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/Type.h>

#include <glm/vec2.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose {

/** ONE SHADOW EACH GLYPH CASTS — one entry of CSS `text-shadow`. It is
 *  drawn with its glyph, beneath it, so it fades, tints, moves and
 *  deforms exactly as a `textFx()` track takes that glyph, and a glyph's
 *  blurred shadow is rasterised once per glyph and size rather than once
 *  over the passage's box. */
struct TextShadow {
  /** How far the shadow falls from its glyph, in local pixels. */
  glm::vec2 offset = {0, 0};
  /** CSS's blur radius in local pixels, as every shadow reads it: a
   *  Gaussian whose standard deviation is half of it; 0 is a hard copy of
   *  the glyph. A blurred shadow with no offset is a glow. */
  float blur = 0;
  /** What the shadow is painted in: a colour, a custom property read
   *  where the shadow is stated, or — unless stated — the ink in force,
   *  the colour each glyph is painted in, as CSS's `currentcolor`. */
  ColorTerm color = ColorTerm::currentInk();
  bool operator==(const TextShadow&) const = default;
};

/** CSS'S `text-shadow` READ FROM ITS TEXT: a comma-separated list, each
 *  shadow two or three lengths — the offset and a blur radius — with a
 *  colour before or after them, the first drawn nearest the glyph —
 *  `readTextShadow("1px 1px 2px black, 0 0 4px blue")`. A length is in
 *  `px` or `pt`, or a bare zero; the colour is any CSS colour
 *  `readColor` reads, or `currentcolor`, the ink in force, which is also
 *  what an unnamed colour is. `none` is the empty list. Nothing where any
 *  shadow does not read, a blur radius is negative, or a length is in a
 *  unit measured against the font or the viewport. */
std::optional<std::vector<TextShadow>> readTextShadow(std::string_view text);

/** THE FONT AND THE INK, which flow down the tree from the node that
 *  states them to every passage under it. `font()` is the shorthand over
 *  the whole partial; each longhand writes one field of it, so
 *  `fontSize(18)` and `font({.size = 18})` are one statement. */
template <class Derived>
class FontVerbs {
 public:
  /** THE FONT EVERYTHING UNDER THIS NODE IS SET IN, as a PARTIAL: the
   *  fields @p partial names override the inherited font and the rest
   *  inherit. Written twice, the later call wins field by field. A
   *  relative size resolves against the PARENT's font. */
  Derived& font(sigil::weave::Type partial);
  /** THE FAMILY — CSS `font-family`, as its list of names:
   *  `fontFamily("Inter, Helvetica Neue, sans-serif")`. The run is set in
   *  the FIRST family of the list the composer's font context has — an
   *  installed family, or one a font file brought that the context
   *  adopted, as a sketch session's does for every font its hub loads —
   *  its face found at the weight and the style in force, and again wherever
   *  either moves below it; the generic names `serif`, `sans-serif`,
   *  `monospace` and `system-ui` are the families the context's font
   *  manager draws them in. The list chooses one face for the run: a
   *  character that face lacks falls back through the context as ever,
   *  never down the list. An empty name is the context's default family.
   *  It and the `face` field of `font()` are one statement, so the later
   *  wins.
   *  @trap A list naming no family the context has leaves the inherited
   *  face standing and says so once, naming the whole list. */
  Derived& fontFamily(std::string family);
  /** THE FAMILY AND ONE OF ITS STYLES BY THE FONT'S OWN NAME for it —
   *  `fontFamily("Avenir", "Book")` — for the faces a weight and a style
   *  cannot tell apart: Avenir's Book and Roman are both an upright 400.
   *  The name is found when the cascade runs, through the composer's font
   *  context, so a family the context adopted answers by its own names, and
   *  it is compared with ASCII case and spaces aside. The face found is
   *  written into the font as `fontFamily(face)` writes a held one, its own
   *  weight and style standing; a `fontWeight` or a `fontStyle` written
   *  after it, or below it in the tree, overrides that trait as CSS would,
   *  the named face's other one standing, and the family chooses its face
   *  at them. One written before it on the same node gives way to it. An
   *  empty name is `fontFamily(family)`.
   *  @trap A name no family of the list has is said once, naming both, and
   *  the family is set at the weight and style in force. */
  Derived& fontFamily(std::string family, std::string styleName);
  /** THE FAMILY AS ONE FACE THE AUTHOR HOLDS — a face a hub loaded from a
   *  font file, or one a style already carries: the run is set in exactly
   *  @p face, with no list and no lookup by name. It is the `face` field of
   *  `font()`, so the later of the two wins; no face is the context's
   *  default family. */
  Derived& fontFamily(sigil::weave::Face face);
  /** THE TYPE SIZE — CSS `font-size`, in any length a font holds (see
   *  `fontLength`): pixels when bare, `em` and `rem` against the parent's
   *  and the root's size, `pt`, and a `percent` of the parent's size, as
   *  CSS reads one. The `size` field of `font()`. 16 px when nothing
   *  states one.
   *  @trap A length a font cannot hold — a viewport unit, a custom
   *  property, a sum, auto — leaves the size in force standing and says
   *  so once. */
  Derived& fontSize(Dimension size);
  /** THE WEIGHT — CSS `font-weight`, as the face's `wght` axis: 400
   *  regular, 700 bold. 0 is the face's own weight, stated. The `weight`
   *  field of `font()`. */
  Derived& fontWeight(float weight);
  /** THE STYLE — CSS `font-style`: `FontStyle::Normal`, `Italic`, or
   *  `FontStyle::oblique(degrees)`, POSITIVE leaning right, 14 when no
   *  angle is given; a bare number is that oblique. An oblique is the
   *  `slant` field of `font()`, negated onto the face's `slnt` axis as
   *  OpenType counts it. Upright when nothing states one.
   *  @trap Italic is the family's italic face, else its `ital` axis, else
   *  an oblique of 14 degrees, which is said once. */
  Derived& fontStyle(FontStyle style);
  /** THE TRACKING added after each cluster — CSS `letter-spacing`, in any
   *  length a font holds: pixels when bare; `em` is a fraction of the
   *  size the type resolves to, and a `percent` is of that size too, as
   *  CSS reads one. The `track` field of `font()`.
   *  @trap A length a font cannot hold leaves the tracking in force
   *  standing and says so once. */
  Derived& letterSpacing(Dimension tracking);
  /** THE SHADOWS EACH GLYPH CASTS — CSS `text-shadow`, the first entry
   *  drawn nearest the glyph. They are the `underlays` field of `font()`,
   *  so the later statement replaces the list and a passage with none of
   *  its own inherits the one in force. An empty list states none. */
  Derived& textShadow(std::vector<TextShadow> shadows);
  /** One shadow, as the one-entry list. */
  Derived& textShadow(TextShadow shadow);
  /** THE INK: the colour text under this node is set in, and every
   *  mark that names no colour is painted in — CSS's `color`.
   *  `Fill::currentInk()` reads it back. A node whose ink changes under
   *  a `transition()` eases it however the change was written — this
   *  verb, a class, a rule, a custom property. A node under it with no
   *  transition of its own follows the ramp; one with its own eases the
   *  inherited ink over its own duration. */
  Derived& ink(material::Color colour);
  /** The ink read from a custom property in force here. A property
   *  nobody set, or one holding a length, leaves the inherited ink
   *  standing and says so once. */
  Derived& ink(VarRef reference);
  /** THE INK AS A WHOLE PAINT — a ramp, a sprite, a recipe, a program —
   *  as a `Fill`, carrying a material's base paint and layers. Its surface
   *  response and effects require the Material overload. A plain colour
   *  behaves as the colour form above does; any other paint inherits the
   *  same way but SNAPS under a transition rather than easing, as a fill
   *  does. @p box is the rectangle the paint's unit square is stretched
   *  over: `Element` by default, which for a passage is its text box;
   *  `Subtree` or `Canvas` for one field the tree shows slices of; or a
   *  text unit, `Glyph` to `Sentence`, restarting the paint on each.
   *  @trap An empty fill clears an ancestor's ink paint and leaves the
   *  inherited colour standing; the ink in force and a custom property
   *  leave the ink where it was. A text unit on a node that is no
   *  passage, and `Padding` or `Content`, which are a fill's, read as
   *  `Element` and say so once. */
  Derived& ink(Fill paint, PaintBox box = PaintBox::Element);
  /** THE INK AS A MATERIAL: its base and layers paint the glyphs, flat,
   *  and a stated surface paints its maps on a page painted as surface
   *  maps (`Composer::setSurfaceMap`).
   *  Its effects dress the node's own layer as fill() places them — a hard
   *  shadow echoes the whole node; the shadow each glyph casts, which moves
   *  with its glyph, is `textShadow`. */
  Derived& ink(material::Material material, PaintBox box = PaintBox::Element);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
