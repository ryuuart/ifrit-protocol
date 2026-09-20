#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/kit/Meter.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dimension;
using compose::Element;
using compose::Fill;
namespace document = compose::document;

compose::Element meter(const Meter& bar) {
  const Theme& look = theme();
  const float filled = std::clamp(bar.fraction, 0.0f, 1.0f);
  const compose::SurfacePaint trackPaint =
      bar.track.value_or(Fill::color(look.palette.cellGround));
  const compose::SurfacePaint barPaint =
      bar.bar.value_or(Fill::color(look.palette.figure));

  Element rail = box();
  trackPaint.apply(rail);
  rail.clip();
  if (bar.width.unit != Dimension::Unit::Auto) rail.width(bar.width);
  rail.height(bar.height.value_or(Dimension(look.spacing.barHeight)));
  if (bar.corners > 0) rail.borderRadius(Corners{bar.corners});
  if (bar.keyline)
    rail.stroke(compose::stroke(bar.keylineWidth, *bar.keyline,
                                compose::PathFormat::Align::Inner));
  if (bar.inset && *bar.inset > 0) rail.padding(*bar.inset);
  if (bar.level) {
    // Scaled from the left edge rather than sized: the bed keeps its
    // recording and only the transform moves.
    Element run = box().absolute().inset(bar.inset.value_or(0.0f));
    barPaint.apply(run);
    run.transformOrigin(compose::pct(0), compose::pct(50)).scaleX(*bar.level);
    if (bar.corners > 0) run.borderRadius(Corners{bar.corners});
    rail.children({std::move(run)});
  } else if (filled > 0) {
    // The height is stated rather than left to the cross-axis stretch:
    // a rail is laid out in whichever direction its caller's tree runs,
    // and a fill that took its height from that would be a hairline on
    // half of them.
    Element run =
        box().width(compose::pct(filled * 100)).height(compose::pct(100));
    barPaint.apply(run);
    run.alignSelf(Align::Stretch);
    if (bar.corners > 0) run.borderRadius(Corners{bar.corners});
    rail.children({std::move(run)});
  }

  if (bar.label.empty() && bar.reading.empty()) return rail;

  Element column = box().column();
  if (bar.width.unit != Dimension::Unit::Auto) column.width(bar.width);
  Element head = box().row().alignItems(Align::Baseline);
  if (!bar.label.empty())
    head.children(
        {document::caption(bar.label).role(weave::rule("caption").font(
            look.font(look.type.captionNote, look.palette.ash)))});
  head.children({box().grow(1)});
  if (!bar.reading.empty())
    head.children({document::paragraph(bar.reading)
                       .role(weave::rule("paragraph")
                                 .font(look.font(look.type.captionLabel,
                                                 look.palette.figure)))
                       .styleClass("readout")});
  column.children({std::move(head)});
  column.children(
      {std::move(rail.margin(0, look.spacing.captionNoteGap, 0, 0))});
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
  const auto ring = [&](float sweep, const compose::SurfacePaint& paint) {
    Element band = box().absolute().inset(0).shape(
        geometry::shapes::sector(dial.startDeg, sweep, inner));
    paint.apply(band);
    return band;
  };

  Element face = box().width(diameter).height(diameter);
  face.children({ring(dial.sweepDeg, dial.track.value_or(Fill::color(
                                         look.palette.cellGround)))});
  if (swept > 0)
    face.children({ring(dial.sweepDeg * swept,
                        dial.bar.value_or(Fill::color(look.palette.figure)))});
  if (!dial.reading.empty())
    face.children(
        {box()
             .absolute()
             .inset(0)
             .alignItems(Align::Center)
             .justifyContent(compose::Justify::Center)
             .children({document::paragraph(dial.reading)
                            .role(weave::rule("paragraph")
                                      .font(look.font(look.type.captionLabel,
                                                      look.palette.figure)))
                            .styleClass("readout")})});
  return face;
}

}  // namespace sigil::sketch::kit
