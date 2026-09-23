#pragma once

/** @file
 * @ingroup compose-core
 *
 * The marks laid under, over and around what the node paints, and what
 * outline they dress.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>

#include <cstddef>
#include <string>

namespace sigil::compose {

/** THE DECORATION SLOTS. Backgrounds paint below the fill, overlays
 *  above it and below the content and children, foregrounds above
 *  everything; `fill()` is the transitionable first background.
 *
 *  Repeated calls APPEND — two `stroke()` calls are two rings — and
 *  every slot takes an optional LOCAL name, which is what
 *  `mask(parts::named(name), …)` addresses and is never a query key.
 *  Decorations dress the OUTLINE, so `overflow(Overflow::Clip)` does not
 *  clip them. */
template <class Derived>
class DecorationVerbs {
 public:
  /** A decoration painted OVER the fill and UNDER the content and
   *  children — hazard stripes over a surface but under the digit,
   *  scanlines under a readout, bevelled chrome. */
  Derived& overlay(Decoration d, std::string name = {});
  /** A decoration painted BENEATH the fill, the CSS box-shadow
   *  ordering — shadows, ground textures, anything the surface sits on
   *  top of. An opaque fill covers it completely. */
  Derived& background(Decoration d, std::string name = {});
  /** The background slot, span-qualified: the twin of
   *  `stroke(where, what)` in the other z-half. One list of passes, one
   *  claim ledger and one no-overlap rule across both halves. */
  Derived& background(Spans where, Decoration what, std::string name = {});
  /** A decoration painted OVER the children. */
  Derived& foreground(Decoration d, std::string name = {});
  /** Dress the node's whole BOUNDARY with a brush. This form does not
   *  CLAIM: it overlays the whole boundary, so repeated calls stack and
   *  never collide. It appends to the foregrounds, which is why the
   *  unqualified strokes always paint under the span passes. */
  Derived& stroke(Decoration brush, std::string name = {});
  /** THE STROKE SLOT: `where` on the boundary, painted by `what`.
   *  Span-qualified passes CLAIM the runs they resolve to, and two
   *  claims that overlap are reported out loud, naming both passes and
   *  the run. */
  Derived& stroke(Spans where, Decoration what, std::string name = {});
  /** WHICH OUTLINE THIS NODE'S DECORATIONS FOLLOW — its own shape (the
   *  default, as CSS `box-shadow` follows the box), the outline of its
   *  GLYPHS on a text leaf (`text-shadow`), or the silhouette of what it
   *  DREW (`filter: drop-shadow`). @p coverage is read under
   *  `Boundary::Coverage` alone: how much paint counts as ink, as a
   *  fraction of full opacity clamped to [0, 1]. Half when unstated,
   *  which is the rule an unantialiased rasteriser uses. */
  Derived& decorationOutline(Boundary source, float coverage = 0.5f);
  /** Apply a whole `LayerStyle`: its `under` layers append as
   *  backgrounds, its `over` layers as foregrounds and its `echoes` as
   *  misprint re-stamps beneath the real pass, so one call dresses the
   *  node in a bundled treatment. An echo is not applied to text
   *  carrying `textFx()` tracks, nor to image or custom content. */
  Derived& layerStyle(LayerStyle s);

 private:
  /** Register whatever a decoration says it borrows so the derive pass
   *  resolves it. EVERY slot that accepts a Decoration must route
   *  through here: a borrow honoured on some slots and not others
   *  draws nothing and says why on no channel. */
  void claimBorrows(const Decoration& d);
  /** Bind the optional LOCAL label an unqualified slot took to the mark
   *  it just appended. `slot` is a detail::MarkSlot as an int, so the
   *  exported header does not have to name an internal enum. */
  void labelMark(int slot, size_t index, std::string name);
  /** The shared body of `stroke(Spans,…)` and `background(Spans,…)`.
   *  `half` is a detail::StrokePass::Half, passed as an int for the
   *  same reason. */
  Derived& addSpanPass(Spans where, Decoration what, std::string name,
                       int half);

  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
  detail::ElementNode* declare(Property property) {
    return detail::NodeAccess::declare(self(), property);
  }
  detail::ElementNode* declare(std::initializer_list<Property> properties) {
    detail::ElementNode* node = detail::NodeAccess::declarations(self());
    for (Property property : properties) detail::markDeclared(node, property);
    return node;
  }
};

}  // namespace sigil::compose
