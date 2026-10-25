#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilsketch/kit/Legend.h>

#include <cstddef>
#include <utility>

#include "DocumentInk.h"

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dimension;
using compose::Element;
using compose::Fill;
using compose::Text;
using compose::var;
namespace document = compose::document;

namespace {

/** The label and the gloss beside a mark: the label in what it names
 *  where the entry says so, the note in the quiet ash either way. */
void words(Element& line, const LegendEntry& entry) {
  const Theme& house = houseTheme();
  if (!entry.label.empty()) {
    Text label = document::label(entry.label)
                     .role("label", house.font(house.type.captionNote),
                           var("palette-ink"));
    if (entry.ink) detail::documentInk(label, *entry.ink);
    line.children({std::move(label)});
  }
  if (!entry.note.empty())
    line.children({document::caption(entry.note)
                       .role("caption", house.font(house.type.captionNote),
                             var("palette-ash"))});
}

/** The beat the entry rides in on, where it has one. */
void enter(Element& line, const LegendEntry& entry) {
  if (entry.opacity) line.opacity(*entry.opacity);
  if (entry.slide) line.translateX(*entry.slide);
}

/** The patch a key's mark is where the entry drew none of its own: the
 *  entry's colour at the key's own side, dressed as the key says. */
Element swatchOf(const Legend& key, const LegendEntry& entry, Dimension side) {
  Element mark = box().width(side).height(side).flexShrink(0);
  if (key.strokeWidth > 0) {
    compose::PathFormat outline =
        compose::stroke(key.strokeWidth, entry.swatch);
    mark.stroke(std::move(outline));
  } else {
    mark.fill(entry.swatch);
  }
  if (key.corners > 0) mark.borderRadius(Corners{key.corners});
  if (entry.keyline)
    mark.foreground(compose::stroke(key.keylineWidth, *entry.keyline));
  return mark;
}

}  // namespace

compose::Element legend(const Legend& key) {
  const Dimension side =
      key.swatchSide ? Dimension(*key.swatchSide) : var("spacing-swatch-side");
  const Dimension gap = key.gap      ? Dimension(*key.gap)
                        : key.column ? var("spacing-row-gap")
                                     : var("spacing-label-gap");
  Element run =
      box().gap(gap).alignItems(key.column ? Align::Start : Align::Center);
  if (key.column)
    run.column();
  else
    run.row();
  if (key.wrap) run.flexWrap();
  const Dimension labelGap =
      key.labelGap ? Dimension(*key.labelGap) : var("spacing-caption-note-gap");
  for (const LegendEntry& entry : key.entries) {
    // A mark the caller drew IS the mark: it carries its own extent and
    // its own edge, so none of the key's dressing is read for it.
    Element line =
        box()
            .row()
            .alignItems(Align::Center)
            .gap(labelGap)
            .children({entry.mark ? *entry.mark : swatchOf(key, entry, side)});
    words(line, entry);
    enter(line, entry);
    run.children({std::move(line)});
  }
  return run;
}

compose::Element swatchStrip(const SwatchStrip& strip) {
  const Theme& house = houseTheme();
  Element run =
      box()
          .row()
          .gap(strip.gap ? Dimension(*strip.gap) : var("spacing-row-gap"))
          .alignItems(Align::Start);
  for (size_t i = 0; i < strip.swatches.size(); ++i) {
    Element patch = box();
    patch.fill(strip.swatches[i]);
    if (strip.width.unit != Dimension::Unit::Auto) patch.width(strip.width);
    if (strip.height.unit != Dimension::Unit::Auto) patch.height(strip.height);
    if (strip.corners > 0) patch.borderRadius(Corners{strip.corners});
    const bool named = i < strip.labels.size() && !strip.labels[i].empty();
    if (!named) {
      if (strip.opacity) patch.opacity(*strip.opacity);
      run.children({std::move(patch)});
      continue;
    }
    // The word stands under its own swatch and takes the swatch's width,
    // so a strip that names only its ends keeps its steps butted; a step
    // whose number is the measurement says so with its own ink.
    Text label = document::eyebrow(strip.labels[i])
                     .role("eyebrow", house.font(house.type.eyebrow),
                           var("palette-ash"));
    if (i < strip.inks.size() && strip.inks[i].kind != Fill::Kind::None)
      detail::documentInk(label, strip.inks[i]);
    Element step = box()
                       .column()
                       .alignItems(Align::Center)
                       .children({std::move(patch)})
                       .children({std::move(label.margin(
                           var("spacing-caption-note-gap"), 0, 0, 0))});
    if (strip.opacity) step.opacity(*strip.opacity);
    run.children({std::move(step)});
  }
  return run;
}

compose::Element chip(const Chip& tag) {
  const Theme& house = houseTheme();
  Element plate = box().padding(var("spacing-chip-padding-vertical"),
                                var("spacing-chip-padding-horizontal"));
  plate.fill(tag.ground.value_or(compose::var("palette-figure")));
  Text label = document::eyebrow(tag.label).role(
      "eyebrow", house.font(house.type.eyebrow), var("palette-ground"));
  if (tag.ink) detail::documentInk(label, *tag.ink);
  plate.children({std::move(label)});
  if (!tag.corners)
    plate.borderRadius(Corners{var("spacing-chip-corners")});
  else if (*tag.corners > 0)
    plate.borderRadius(Corners{*tag.corners});
  return plate;
}

}  // namespace sigil::sketch::kit
