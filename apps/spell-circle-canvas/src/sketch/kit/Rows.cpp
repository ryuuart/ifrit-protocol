#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Rows.h>

#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element labelRow(const Reading& reading, float measure) {
  const Theme& look = theme();
  Element row = box().row().alignItems(Align::Baseline);
  if (measure > 0)
    row.width(Dim(measure));
  else
    row.gap(look.spacing.labelGap);
  if (!reading.name.empty())
    row.child(text(reading.name,
                   look.style(look.type.captionNote, look.palette.ash)));
  // With a measure the space between is what grows, which is what makes a
  // stack of rows line up on their figures rather than on their names.
  if (measure > 0) row.child(box().grow(1));
  if (!reading.value.empty())
    row.child(text(reading.value,
                   look.style(look.type.captionLabel, look.palette.figure)));
  if (!reading.note.empty()) {
    Element note =
        text(reading.note, look.style(look.type.captionNote, look.palette.ash));
    if (!reading.value.empty()) note.margin(look.spacing.labelGap, 0, 0, 0);
    row.child(std::move(note));
  }
  return row;
}

compose::Element readout(const Readout& table) {
  const Theme& look = theme();
  Element column = box().column().gap(look.spacing.rowGap);
  if (table.measure > 0) column.width(Dim(table.measure));
  bool first = true;
  for (const Reading& reading : table.rows) {
    if (!first && table.ruled)
      column.child(box()
                       .height(Dim(1))
                       .alignSelf(Align::Stretch)
                       .fill(Fill::color(look.palette.rule)));
    first = false;
    column.child(labelRow(reading, table.measure));
  }
  return column;
}

}  // namespace sigil::sketch::kit
