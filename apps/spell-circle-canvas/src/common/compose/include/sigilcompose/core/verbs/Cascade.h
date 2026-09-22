#pragma once

/** @file
 * @ingroup compose-core
 *
 * What a node hands DOWN the tree, as verbs: the font, the block, the
 * ink, the custom properties, and how image leaves under it sample.
 */

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/PaintAnchor.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Var.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Style.h>

#include <string_view>

namespace sigil::compose {

/** WHAT FLOWS DOWN THE TREE, from a node to everything under it,
 *  wherever the code that built a child ran. A node that leaves one of
 *  these unset takes the nearest ancestor's, and the root's are the
 *  composer's `setInherited` defaults — CSS's own split between the
 *  properties that inherit and the ones that do not. */
template <class Derived>
class CascadeVerbs {
 public:
  /** THE FONT EVERYTHING UNDER THIS NODE IS SET IN, as a PARTIAL: the
   *  fields @p partial names override the inherited font and the rest
   *  inherit. Written twice, the later call wins field by field. A
   *  relative size resolves against the PARENT's font. */
  Derived& font(sigil::weave::Type partial);
  /** THE BLOCK everything under this node is set in, as a PARTIAL, in
   *  the same way the font is: the fields it names override the
   *  inherited block and the rest inherit. This is the one spelling of
   *  every block field — the leading, the alignment, the indents, the
   *  writing mode and the rest. */
  Derived& block(sigil::weave::Block partial);
  /** THE INK: the colour text under this node is set in, and every
   *  mark that names no colour is painted in — CSS's `color`.
   *  `Fill::currentInk()` reads it back. A node whose ink changes under
   *  a `transition()` eases it, and everything under it follows. */
  Derived& ink(material::Color colour);
  /** The ink read from a custom property in force here. A property
   *  nobody set, or one holding a length, leaves the inherited ink
   *  standing and says so once. */
  Derived& ink(VarRef reference);
  /** THE INK AS A WHOLE PAINT — a ramp, a sprite, a recipe, SkSL —
   *  taking everything `fill` takes. A plain colour behaves as the
   *  colour form above does; any other paint inherits the same way but
   *  SNAPS under a transition rather than easing, as a fill does. @p
   *  anchor is the box the paint's unit square maps onto, own box by
   *  default, which for a text leaf is its text-metric box.
   *  @trap An empty paint clears an ancestor's ink paint and leaves the
   *  inherited colour standing. */
  Derived& ink(SurfacePaint paint, PaintAnchor anchor = PaintAnchor::OwnBox);
  /** A CUSTOM PROPERTY set on this node and inherited by everything
   *  under it, read back through `var(name)`, `Fill::var` or
   *  `ink(var(name))`. The nearest ancestor that set a name wins. */
  Derived& var(std::string_view name, material::Color colour);
  /** The same, holding a LENGTH rather than a colour. A property is one
   *  or the other, and reading one as the other leaves the target
   *  standing and says so once. */
  Derived& var(std::string_view name, Dimension length);
  /** FALLBACK CUSTOM PROPERTIES for this node and its descendants.
   *  An inherited property overrides these, and one this node sets with
   *  `var()` overrides both, explicit zeros included. A later call
   *  replaces the table. */
  Derived& varDefaults(VarTable defaults);
  /** HOW IMAGE LEAVES UNDER THIS NODE SAMPLE THEIR SOURCE. Linear when
   *  nothing states it, which is right for photographs and wrong for
   *  every pixel grid, and inherited as CSS inherits `image-rendering`,
   *  so a panel of pixel art states nearest once. */
  Derived& imageRendering(SkSamplingOptions options);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
