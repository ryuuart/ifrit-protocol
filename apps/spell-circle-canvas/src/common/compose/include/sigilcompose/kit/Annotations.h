#pragma once

/** @file
 * SigilCompose KIT — things that stand BESIDE a text, one per unit it
 * addresses: `annotate`, which places a caller's element at every word,
 * cluster, line or sentence a selector names, and the two placements that
 * say where each one goes — `Beside`, which does the arithmetic of the
 * reading direction, and `Anchored`, which puts the object where the
 * caller says.
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
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/Units.h>

#include <functional>
#include <optional>
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


/** WHERE AN ANCHORED OBJECT STANDS when the caller states the position
 *  rather than a side: an object still TIED to a text position — it moves
 *  when the text reflows, because it is placed off a unit the layout
 *  reports — that neither sits in the line nor stands in a reserved band.
 *
 *  Three things say where it lands. `horizontal` and `vertical` name WHICH
 *  RECT each axis is measured from, and they are named separately because
 *  the commonest anchored object in print measures them from different
 *  things: a margin figure takes its x from the frame's edge and its y
 *  from the word it belongs to. `at` says WHICH POINT of those rects the
 *  measurement starts from, as fractions of each — {0,0} their left-top,
 *  {1,1} their right-bottom, {0.5,0.5} their centre. `offset` is how far
 *  from there, in pixels.
 *
 *  THE OFFSET IS IN THE COMPOSITION'S AXES, not the reading direction's: a
 *  custom position is a place on the sheet, and +x is right on a page of
 *  Arabic and on a column of Japanese exactly as it is here. An object
 *  that follows the reading direction instead wants `Beside`, which does
 *  that arithmetic and nothing else.
 *
 *  WHAT LANDS ON THE POINT IS THE OBJECT'S OWN LEFT-TOP CORNER. The
 *  object's size is decided by its content after this places it, so there
 *  is nothing here to measure it against — an object hung by its right
 *  edge is given a width and an offset carrying that width, the same way
 *  `Beside::measure` is stated rather than measured. */
struct Anchored {
  /** WHAT one axis is measured from: the unit the selector named, the
   *  whole flow line (or column) that unit landed on, or the text node's
   *  own box. A reference the layout cannot answer for falls back to the
   *  unit, which is the one rect a placed unit always has. */
  enum class From { Unit, Line, Frame };
  From horizontal = From::Unit;
  From vertical = From::Unit;
  SkPoint at = {0.0f, 0.0f};
  SkVector offset = {0.0f, 0.0f};
};

/** ONE ELEMENT PER UNIT, AT THE POSITION THE CALLER STATES — the same
 *  read-back as the placement above, with the arithmetic handed over.
 *
 *      root.child(kit::annotate(composer, "verse", sel::text(u8"Ishmael"),
 *                               unit::Word,
 *                               {.horizontal = kit::Anchored::From::Frame,
 *                                .offset = {-44, 0}},
 *                               [&](const TextUnit &u) { return figure(u); })
 *                     .absolute().inset(0));
 *
 *  Anchored to the text and positioned by the caller: the object moves
 *  with the word when the copy or the measure changes, and stands exactly
 *  where the offset puts it while it is there. Everything the placement
 *  above says about the space the rects are in, the describe-time
 *  read-back and the silent empty answer holds here word for word — it is
 *  one mechanism with two ways of saying where. */
[[nodiscard]] inline Element annotate(
    const Composer& composer, std::string_view baseKey, const Selector& where,
    Unit unit, Anchored anchored,
    const std::function<Element(const TextUnit&)>& make) {
  Element overlay = positioned();
  const std::vector<TextUnit> units = composer.units(baseKey, where, unit);
  if (units.empty()) return overlay;
  const bool wantsLine = anchored.horizontal == Anchored::From::Line ||
                         anchored.vertical == Anchored::From::Line;
  const bool wantsFrame = anchored.horizontal == Anchored::From::Frame ||
                          anchored.vertical == Anchored::From::Frame;
  // The lines are read once for the whole overlay, and UNSLICED so each one
  // reports the whole line rather than the part a selector addressed: an
  // object measured from the line is measured from all of it.
  const std::vector<TextUnit> lines =
      wantsLine ? composer.units(baseKey, sel::each(Unit::Line), Unit::Line)
                : std::vector<TextUnit>{};
  const std::optional<SkRect> frame =
      wantsFrame ? composer.bounds(baseKey) : std::nullopt;
  for (const TextUnit& entry : units) {
    const SkRect* line = nullptr;
    for (const TextUnit& candidate : lines)
      if (candidate.lineIndex == entry.lineIndex) {
        line = &candidate.rect;
        break;
      }
    const auto rectFor = [&](Anchored::From from) -> const SkRect& {
      if (from == Anchored::From::Line && line) return *line;
      if (from == Anchored::From::Frame && frame) return *frame;
      return entry.rect;
    };
    const SkRect& across = rectFor(anchored.horizontal);
    const SkRect& down = rectFor(anchored.vertical);
    const float left =
        across.left() + anchored.at.x() * across.width() + anchored.offset.x();
    const float top =
        down.top() + anchored.at.y() * down.height() + anchored.offset.y();
    overlay.child(box()
                      .key(std::string(baseKey) + "-anchored" +
                           std::to_string(entry.index))
                      .left(left)
                      .top(top)
                      .child(make(entry)));
  }
  return overlay;
}

}  // namespace sigil::compose::kit
