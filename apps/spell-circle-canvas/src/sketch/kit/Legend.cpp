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
namespace document = compose::document;

namespace {

/** The label and the gloss beside a mark: the label in what it names
 *  where the entry says so, the note in the quiet ash either way. */
void words(Element& line, const LegendEntry& entry) {
  const Theme& look = theme();
  if (!entry.label.empty()) {
    Element label = document::label(entry.label)
                        .role(weave::rule("label").font(look.font(
                            look.type.captionNote, look.palette.ink)));
    if (entry.ink) detail::documentInk(label, *entry.ink);
    line.children({std::move(label)});
  }
  if (!entry.note.empty())
    line.children({document::caption(entry.note)
                       .role(weave::rule("caption").font(look.font(
                           look.type.captionNote, look.palette.ash)))});
}

/** The beat the entry rides in on, where it has one. */
void enter(Element& line, const LegendEntry& entry) {
  if (entry.opacity) line.opacity(*entry.opacity);
  if (entry.slide) line.translateX(*entry.slide);
}

/** The patch a key's mark is where the entry drew none of its own: the
 *  entry's colour at the key's own side, dressed as the key says. */
Element swatchOf(const Legend& key, const LegendEntry& entry, float side) {
  Element mark = box().width(side).height(side).shrink(0);
  if (key.strokeWidth > 0) {
    compose::PathFormat outline =
        compose::stroke(key.strokeWidth, entry.swatch);
    mark.stroke(std::move(outline));
  } else {
    entry.swatch.apply(mark);
  }
  if (key.corners > 0) mark.borderRadius(Corners{key.corners});
  if (entry.keyline)
    mark.foreground(compose::stroke(key.keylineWidth, *entry.keyline));
  return mark;
}

}  // namespace

compose::Element legend(const Legend& key) {
  const Theme& look = theme();
  const float side = key.swatchSide.value_or(look.spacing.swatchSide);
  const float gap = key.gap.value_or(key.column ? look.spacing.rowGap
                                                : look.spacing.labelGap);
  Element run =
      box().gap(gap).alignItems(key.column ? Align::Start : Align::Center);
  if (key.column)
    run.column();
  else
    run.row();
  if (key.wrap) run.flexWrap();
  const float labelGap = key.labelGap.value_or(look.spacing.captionNoteGap);
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
  const Theme& look = theme();
  Element run = box()
                    .row()
                    .gap(strip.gap.value_or(look.spacing.rowGap))
                    .alignItems(Align::Start);
  for (size_t i = 0; i < strip.swatches.size(); ++i) {
    Element patch = box();
    strip.swatches[i].apply(patch);
    if (strip.width.unit != Dimension::Unit::Auto) patch.width(strip.width);
    if (strip.height.unit != Dimension::Unit::Auto) patch.height(strip.height);
    if (strip.corners > 0) patch.borderRadius(Corners{strip.corners});
    const bool named = i < strip.labels.size() && !strip.labels[i].empty();
    if (!named) {
      if (strip.appear) patch.appear(*strip.appear);
      run.children({std::move(patch)});
      continue;
    }
    // The word stands under its own swatch and takes the swatch's width,
    // so a strip that names only its ends keeps its steps butted; a step
    // whose number is the measurement says so with its own ink.
    Element label = document::eyebrow(strip.labels[i])
                        .role(weave::rule("eyebrow").font(
                            look.font(look.type.eyebrow, look.palette.ash)));
    if (i < strip.inks.size() && strip.inks[i].kind != Fill::Kind::None)
      detail::documentInk(label, strip.inks[i]);
    Element step = box()
                       .column()
                       .alignItems(Align::Center)
                       .children({std::move(patch)})
                       .children({std::move(label.margin(
                           0, look.spacing.captionNoteGap, 0, 0))});
    if (strip.appear) step.appear(*strip.appear);
    run.children({std::move(step)});
  }
  return run;
}

compose::Element chip(const Chip& tag) {
  const Theme& look = theme();
  Element plate =
      box().padding(look.spacing.chipPaddingX, look.spacing.chipPaddingY);
  tag.ground.value_or(Fill::color(look.palette.figure)).apply(plate);
  Element label = document::eyebrow(tag.label).role(weave::rule("eyebrow").font(
      look.font(look.type.eyebrow, look.palette.ground)));
  if (tag.ink) detail::documentInk(label, *tag.ink);
  plate.children({std::move(label)});
  if (const float round = tag.corners.value_or(look.spacing.chipCorners);
      round > 0)
    plate.borderRadius(Corners{round});
  return plate;
}

}  // namespace sigil::sketch::kit
