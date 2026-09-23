#pragma once

/** @file
 * @ingroup compose-core
 *
 * What a node hands DOWN the tree, as verbs: the paragraph setting and
 * its longhands, the custom properties, how image leaves under it sample, and
 * the three keywords. The font and the ink are the font family's.
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
  /** HOW EVERY PARAGRAPH under this node is set, as a PARTIAL, in the
   *  same way the font is: the fields it names override the inherited
   *  setting and the rest inherit. The one spelling of every paragraph
   *  field — the leading, the alignment, the indents, the writing mode
   *  and the rest; the five longhands below each write one field of it,
   *  CSS's names for the fields CSS has. */
  Derived& paragraph(sigil::weave::ParagraphBlock partial);
  /** THE PITCH OF THE LINES — CSS `line-height`: the face's own, a
   *  multiple of the size, an absolute pitch, or a baseline grid. The
   *  `leading` field of `paragraph()`. The face's own when nothing states
   *  one. */
  Derived& lineHeight(sigil::weave::Leading leading);
  /** WHERE THE LINES SIT ACROSS THE MEASURE — CSS `text-align`: start,
   *  centre, end or justified. The `alignment` field of `paragraph()`. Start
   *  when nothing states one. */
  Derived& textAlign(sigil::weave::TextAlignment alignment);
  /** THE FIRST LINE OF EVERY BLOCK INDENTED by @p px — CSS
   *  `text-indent`; negative hangs it out. The `firstLineIndent` field of
   *  `paragraph()`. Zero when nothing states one. */
  Derived& textIndent(float px);
  /** WHICH WAY THE LINES RUN — CSS `writing-mode`: horizontal, or
   *  vertical columns right to left. The `writingMode` field of
   *  `paragraph()`. Horizontal when nothing states one. */
  Derived& writingMode(sigil::weave::WritingMode mode);
  /** WHETHER AND WHERE A WORD MAY BREAK WITH A HYPHEN — CSS `hyphens`:
   *  `enabled = false` is `none`, soft hyphens alone are `manual`, and a
   *  pattern set is `auto`. The `hyphenation` field of `paragraph()`. */
  Derived& hyphens(sigil::weave::HyphenationOptions hyphenation);
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

  /** @p property TAKES THE PARENT'S COMPUTED VALUE, whether or not it is
   *  one that inherits on its own: `inherit(Property::PaddingLeft)` gives
   *  this node the padding its parent ended up with. On a property that
   *  DOES inherit it drops the role default, the rule and this node's own
   *  verb that were folded over the inherited value. The root, which has
   *  no parent, reads it as `initial`.
   *  @trap A property the description keeps and no fold reads — the
   *  depth lanes, the shape, the grid area, the decoration outline — has
   *  nowhere to resolve a keyword, so one written about it is refused and
   *  said once. `answersKeyword` is the table. */
  Derived& inherit(Property property);
  /** @p property TAKES ITS OWN INITIAL VALUE — the one it has where
   *  nothing anywhere states it — whatever an ancestor or a rule says.
   *  This is how an inheriting property is stopped: `initial(Property::Font)`
   *  sets this node and its subtree in the default face at the default
   *  size. */
  Derived& initial(Property property);
  /** @p property TAKES WHICHEVER OF THE TWO its own behaviour asks for:
   *  the parent's value where it inherits, its initial value where it does
   *  not. CSS's `unset`, and the honest way to say "as if I had not
   *  written this" when a rule might have — including when this node's own
   *  verb said it. */
  Derived& unset(Property property);

 private:
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
