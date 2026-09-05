#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Rows.h>

#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element labelRow(const Reading& reading, const Readout& how) {
  const Theme& look = theme();
  Element row = box().row().alignItems(Align::Center).gap(
      look.spacing.labelGap);
  if (how.measure > 0) row.width(Dim(how.measure));
  if (reading.swatch.kind != Fill::Kind::None) {
    const float side = how.swatch.value_or(look.spacing.swatch);
    Element mark =
        box().width(Dim(side)).height(Dim(side)).fill(reading.swatch).shrink(0);
    if (how.swatchCorners > 0) mark.corners(Corners{how.swatchCorners});
    row.child(std::move(mark));
  }
  if (!reading.name.empty()) {
    Element name =
        text(reading.name, look.style(look.type.captionNote, look.palette.ash));
    if (how.nameMeasure > 0) name.width(Dim(how.nameMeasure));
    row.child(std::move(name));
  }
  // With a measure the space between is what grows, which is what puts
  // every figure on one edge however long the names are.
  if (how.measure > 0) row.child(box().grow(1));
  if (!reading.value.empty())
    row.child(text(reading.value,
                   look.style(look.type.captionLabel, look.palette.figure)));
  if (!reading.note.empty())
    row.child(
        text(reading.note, look.style(look.type.captionNote, look.palette.ash)));
  return row;
}

compose::Element readout(std::vector<Reading> rows, const Readout& how) {
  const Theme& look = theme();
  Element column = box().column().gap(look.spacing.rowGap);
  if (how.measure > 0) column.width(Dim(how.measure));
  bool first = true;
  for (const Reading& reading : rows) {
    if (!first && how.ruled)
      column.child(box()
                       .height(Dim(1))
                       .alignSelf(Align::Stretch)
                       .fill(Fill::color(look.palette.rule)));
    first = false;
    column.child(labelRow(reading, how));
  }
  return column;
}

}  // namespace sigil::sketch::kit
