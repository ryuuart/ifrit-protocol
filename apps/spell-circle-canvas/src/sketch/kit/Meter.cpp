#include <sigilcompose/core/Factories.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/kit/Meter.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element meter(const Meter& bar) {
  const Theme& look = theme();
  const float filled = std::clamp(bar.fraction, 0.0f, 1.0f);
  const Fill trackFill =
      bar.track.value_or(Fill::color(look.palette.cellGround));
  const Fill barFill = bar.bar.value_or(Fill::color(look.palette.figure));

  Element rail = box().fill(trackFill).clip();
  if (bar.width.unit != Dim::Unit::Auto) rail.width(bar.width);
  rail.height(bar.height.value_or(Dim(look.spacing.barHeight)));
  if (bar.corners > 0) rail.corners(Corners{bar.corners});
  if (filled > 0) {
    Element run = box().width(compose::pct(filled * 100)).fill(barFill).
                  alignSelf(Align::Stretch);
    if (bar.corners > 0) run.corners(Corners{bar.corners});
    rail.child(std::move(run));
  }

  if (bar.label.empty() && bar.reading.empty()) return rail;

  Element column = box().column();
  if (bar.width.unit != Dim::Unit::Auto) column.width(bar.width);
  Element head = box().row().alignItems(Align::Baseline);
  if (!bar.label.empty())
    head.child(
        text(bar.label, look.style(look.type.captionNote, look.palette.ash)));
  head.child(box().grow(1));
  if (!bar.reading.empty())
    head.child(text(bar.reading,
                    look.style(look.type.captionLabel, look.palette.figure)));
  column.child(std::move(head));
  column.child(std::move(rail.margin(0, look.spacing.captionNoteGap, 0, 0)));
  return column;
}

compose::Element gauge(const Gauge& dial) {
  const Theme& look = theme();
  const float swept = std::clamp(dial.fraction, 0.0f, 1.0f);
  const float diameter = std::max(dial.diameter, 1.0f);
  // `Sector`'s inner ratio is a fraction of the RADIUS, so a ring of
  // `thickness` px on a dial of `diameter` px leaves this much of it.
  const float inner =
      std::clamp(1.0f - (2.0f * dial.thickness) / diameter, 0.0f, 0.999f);
  const auto ring = [&](float sweep, Fill paint) {
    return box()
        .absolute()
        .inset(0)
        .shape(geometry::shapes::sector(dial.startDeg, sweep, inner))
        .fill(paint);
  };

  Element face = box().width(Dim(diameter)).height(Dim(diameter));
  face.child(ring(dial.sweepDeg,
                  dial.track.value_or(Fill::color(look.palette.cellGround))));
  if (swept > 0)
    face.child(ring(dial.sweepDeg * swept,
                    dial.bar.value_or(Fill::color(look.palette.figure))));
  if (!dial.reading.empty())
    face.child(box()
                   .absolute()
                   .inset(0)
                   .alignItems(Align::Center)
                   .justify(compose::Justify::Center)
                   .child(text(dial.reading,
                               look.style(look.type.captionLabel,
                                          look.palette.figure))));
  return face;
}

}  // namespace sigil::sketch::kit
