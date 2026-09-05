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

compose::Element legend(const Legend& key) {
  const Theme& look = theme();
  const float side = key.swatch.value_or(look.spacing.swatch);
  const float gap = key.gap.value_or(key.column ? look.spacing.rowGap
                                                : look.spacing.labelGap);
  Element run = box().gap(gap).alignItems(key.column ? Align::Start
                                                     : Align::Center);
  if (key.column)
    run.column();
  else
    run.row();
  if (key.wrap) run.wrapLines();
  for (const LegendEntry& entry : key.entries) {
    Element mark = box().width(Dim(side)).height(Dim(side)).shrink(0);
    if (key.strokeWidth > 0)
      mark.stroke(compose::stroke(key.strokeWidth, entry.swatch));
    else
      mark.fill(entry.swatch);
    if (key.corners > 0) mark.corners(Corners{key.corners});
    Element line = box()
                       .row()
                       .alignItems(Align::Center)
                       .gap(look.spacing.captionNoteGap)
                       .child(std::move(mark));
    if (!entry.label.empty())
      line.child(text(entry.label,
                      look.style(look.type.captionNote, look.palette.ink)));
    if (!entry.note.empty())
      line.child(text(entry.note,
                      look.style(look.type.captionNote, look.palette.ash)));
    run.child(std::move(line));
  }
  return run;
}

compose::Element swatchStrip(const SwatchStrip& strip) {
  const Theme& look = theme();
  Element run = box().row().gap(strip.gap.value_or(look.spacing.rowGap)).
                alignItems(Align::Start);
  for (size_t i = 0; i < strip.swatches.size(); ++i) {
    Element patch = box().fill(strip.swatches[i]);
    if (strip.width.unit != Dim::Unit::Auto) patch.width(strip.width);
    if (strip.height.unit != Dim::Unit::Auto) patch.height(strip.height);
    if (strip.corners > 0) patch.corners(Corners{strip.corners});
    const bool named =
        i < strip.labels.size() && !strip.labels[i].empty();
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
      box()
          .padding(look.spacing.chipPaddingX, look.spacing.chipPaddingY)
          .fill(tag.ground.value_or(Fill::color(look.palette.figure)))
          .child(text(tag.label,
                      look.style(look.type.eyebrow,
                                 tag.ink.value_or(look.palette.ground))));
  if (tag.corners > 0) plate.corners(Corners{tag.corners});
  return plate;
}

}  // namespace sigil::sketch::kit
