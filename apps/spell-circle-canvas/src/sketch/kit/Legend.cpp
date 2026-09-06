#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Legend.h>

#include <cstddef>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

namespace {

/** The label and the gloss beside a mark: the label in what it names
 *  where the entry says so, the note in the quiet ash either way. */
void words(Element& line, const LegendEntry& entry) {
  const Theme& look = theme();
  if (!entry.label.empty())
    line.child(
        text(entry.label,
             look.style(look.type.captionNote,
                        entry.ink.value_or(Fill::color(look.palette.ink)))));
  if (!entry.note.empty())
    line.child(
        text(entry.note, look.style(look.type.captionNote, look.palette.ash)));
}

/** The beat the entry rides in on, where it has one. */
void enter(Element& line, const LegendEntry& entry) {
  if (entry.opacity) line.opacity(*entry.opacity);
  if (entry.slide) line.translateX(*entry.slide);
}

/** The patch a key's mark is where the entry drew none of its own: the
 *  entry's colour at the key's own side, dressed as the key says. */
Element swatchOf(const Legend& key, const LegendEntry& entry, float side) {
  Element mark = box().width(Dim(side)).height(Dim(side)).shrink(0);
  if (key.strokeWidth > 0) {
    // An outlined key draws the swatch as a line, so the ground goes
    // into the stroke's own two slots rather than onto the node.
    compose::PathFormat outline =
        compose::stroke(key.strokeWidth, entry.swatch.fill());
    if (const material::skia::Paint* m = entry.swatch.material())
      outline.strokeMaterial = *m;
    mark.stroke(std::move(outline));
  } else {
    entry.swatch.paint(mark);
  }
  if (key.corners > 0) mark.corners(Corners{key.corners});
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
  if (key.wrap) run.wrapLines();
  const float labelGap = key.labelGap.value_or(look.spacing.captionNoteGap);
  for (const LegendEntry& entry : key.entries) {
    // A mark the caller drew IS the mark: it carries its own extent and
    // its own edge, so none of the key's dressing is read for it.
    Element line =
        box()
            .row()
            .alignItems(Align::Center)
            .gap(labelGap)
            .child(entry.mark ? *entry.mark : swatchOf(key, entry, side));
    words(line, entry);
    enter(line, entry);
    run.child(std::move(line));
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
    strip.swatches[i].paint(patch);
    if (strip.width.unit != Dim::Unit::Auto) patch.width(strip.width);
    if (strip.height.unit != Dim::Unit::Auto) patch.height(strip.height);
    if (strip.corners > 0) patch.corners(Corners{strip.corners});
    const bool named = i < strip.labels.size() && !strip.labels[i].empty();
    if (!named) {
      run.child(std::move(patch));
      continue;
    }
    // The word stands under its own swatch and takes the swatch's width,
    // so a strip that names only its ends keeps its steps butted.
    run.child(box()
                  .column()
                  .alignItems(Align::Center)
                  .child(std::move(patch))
                  .child(text(strip.labels[i],
                              look.style(look.type.eyebrow, look.palette.ash))
                             .margin(0, look.spacing.captionNoteGap, 0, 0)));
  }
  return run;
}

compose::Element chip(const Chip& tag) {
  const Theme& look = theme();
  Element plate =
      box().padding(look.spacing.chipPaddingX, look.spacing.chipPaddingY);
  tag.ground.value_or(Fill::color(look.palette.figure)).paint(plate);
  plate.child(text(tag.label,
                   look.style(look.type.eyebrow, tag.ink.value_or(Fill::color(
                                                     look.palette.ground)))));
  if (const float round = tag.corners.value_or(look.spacing.chipCorners);
      round > 0)
    plate.corners(Corners{round});
  return plate;
}

}  // namespace sigil::sketch::kit
