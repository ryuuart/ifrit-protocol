#pragma once

/** @file
 * SigilCompose KIT — things that stand BESIDE a text, one per unit it
 * addresses: `annotate`, which places a caller's element at every word,
 * cluster, line or sentence a selector names, and the placements that say
 * where beside a unit each one goes.
 *
 * It is `Composer::units` handed to an element factory, and its whole
 * argument is that the arithmetic between a unit and the thing standing
 * next to it — which side, how far off, aligned where, and what a column
 * makes of all three — should be written once rather than per artefact.
 * The unit itself is the factory's argument, so an annotation may take its
 * size from the type it names rather than from a fraction somebody chose.
 *
 * A marginal note, a word label, a callout, a pronunciation guide beside a
 * phrase: all of them are this. RUBY AND KENTEN ARE NOT — those reserve
 * space in the line they annotate, which is a layout input and belongs on
 * the text leaf itself (`Element::annotate`), not on a sibling that reads
 * the finished result.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Text.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::kit {

/** WHERE AN ANNOTATION STANDS RELATIVE TO THE UNIT IT NAMES, in that
 *  unit's own writing frame.
 *
 *  `Before` and `After` are ACROSS the reading direction — above and below
 *  a line, right and left of a column, which is the side each writing mode
 *  reads its furniture on. `Start` and `End` are ALONG it, so a label
 *  before a word and a note after a column both say `Start` and `End` and
 *  mean what a reader of that mode means by them.
 *
 *  `gap` is the standoff from the unit's band. `measure` is how wide — or,
 *  down a column, how deep — the annotation itself is, and a side that
 *  stands BEFORE the unit needs it: a note ending where a line begins has
 *  to know its own extent to know where to start. A side that stands after
 *  the unit ignores it and lets the content decide. */
struct Beside {
  enum class Side { Before, After, Start, End };
  Side side = Side::Before;
  float gap = 2.0f;
  float measure = 0.0f;
};

/** ONE ELEMENT PER UNIT a selector addresses on a keyed text node, placed
 *  beside it.
 *
 *      root.child(kit::annotate(composer, "verse", sel::each(unit::Word),
 *                               unit::Word, {.side = Beside::Side::End,
 *                                            .gap = 14},
 *                               [&](const TextUnit &u) {
 *                                 return text(gloss(u.range), small);
 *                               })
 *                     .absolute().inset(0));
 *
 *  THE RECTS ARE IN THE COMPOSER'S SPACE, because that is the space
 *  `Composer::units` answers in, so the result belongs over the whole
 *  composition rather than inside the text it annotates. Each child is
 *  given the unit's own extent along the reading direction and is sized by
 *  its own content across it, so a note that wraps grows away from the
 *  text rather than into it.
 *
 *  It resolves at DESCRIBE time from the layout the last draw left
 *  standing — the same terms as the instruments in `Instruments.h`. A text
 *  that reflows therefore wants a re-describe for its annotations to
 *  follow, and one that reflows every frame wants one every frame. What it
 *  buys for that is that no annotation can disagree with the placement:
 *  the units are read off the glyphs rather than measured again.
 *
 *  An unknown key, a node that is not text, and a selector that addresses
 *  nothing all give an EMPTY overlay, silently, exactly as
 *  `Composer::units` answers empty. */
[[nodiscard]] inline Element annotate(
    const Composer& composer, std::string_view baseKey, const Selector& where,
    Unit unit, Beside beside,
    const std::function<Element(const TextUnit&)>& make) {
  Element overlay = positioned();
  const std::vector<TextUnit> units = composer.units(baseKey, where, unit);
  for (const TextUnit& entry : units) {
    const bool column =
        entry.writingMode == sigil::weave::WritingMode::kVerticalRL;
    const SkRect& rect = entry.rect;
    // ALONG the reading direction the annotation takes the unit's own
    // extent; ACROSS it, its own content decides, and the gap stands it off
    // the unit's band.
    const float along = column ? rect.height() : rect.width();
    float left = rect.left();
    float top = rect.top();
    // A side that stands BEFORE the unit ends where the unit begins, so it
    // starts one measure back from there; a side that stands after it
    // simply begins past the gap.
    switch (beside.side) {
      case Beside::Side::Before:
        if (column)
          left = rect.right() + beside.gap;  // a column reads its furniture
        else                                 // on the right; a line above
          top = rect.top() - beside.gap - beside.measure;
        break;
      case Beside::Side::After:
        if (column)
          left = rect.left() - beside.gap - beside.measure;
        else
          top = rect.bottom() + beside.gap;
        break;
      case Beside::Side::Start:
        if (column)
          top = rect.top() - beside.gap - beside.measure;
        else
          left = rect.left() - beside.gap - beside.measure;
        break;
      case Beside::Side::End:
        if (column)
          top = rect.bottom() + beside.gap;
        else
          left = rect.right() + beside.gap;
        break;
    }
    const bool acrossOnly = beside.side == Beside::Side::Before ||
                            beside.side == Beside::Side::After;
    Element child = make(entry);
    const std::string key =
        std::string(baseKey) + "-note" + std::to_string(entry.index);
    Element cell = box().key(key).left(left).top(top);
    if (acrossOnly) {
      // Across the reading direction the annotation is as long as its unit
      // and slides along it; along the other axis its content decides.
      if (column)
        cell.height(along);
      else
        cell.width(along);
    }
    overlay.child(cell.child(std::move(child)));
  }
  return overlay;
}

}  // namespace sigil::compose::kit
