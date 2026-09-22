/** @file
 * The outline operators: a band swept along one node's resolved outline
 * and attached to it, and the hull of a set of nodes' outlines attached
 * to the scope.
 */

#include "sigilcompose/kit/Outline.h"

#include <glm/vec2.hpp>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Shape.h>
#include <sigilgeometry/path/Hull.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Polyline.h>

#include <vector>

namespace sigil::compose::outline {

void Around::add(Scope& scope) const {
  const Scope::Node* node = scope.find(key);
  if (!node) return;
  // The band is the node's own outline swept `across` wide, in the
  // node's coordinates, laid over the node's box so it moves with it.
  Band ring = band(heldPath(node->toLocal(node->outline)), across);
  ring.bandAlignment(formation)
      .key(key + "-outline")
      .rect(SkRect::MakeWH(node->bounds.width(), node->bounds.height()));
  if (fill) ring.fill(*fill);
  node->attach(std::move(ring));
}

void Hull::add(Scope& scope) const {
  const std::vector<const Scope::Node*> nodes =
      lane.empty() ? scope.withClass(styleClass) : scope.having(lane);
  std::vector<glm::vec2> points;
  for (const Scope::Node* node : nodes)
    for (const geometry::path::Polyline& line :
         geometry::path::flatten(node->outline))
      points.insert(points.end(), line.points.begin(), line.points.end());
  const std::vector<geometry::path::Polyline> rings =
      geometry::path::hull(points, alpha);
  if (rings.empty()) return;
  SkPathBuilder builder;
  for (const geometry::path::Polyline& ring : rings)
    builder.addPath(geometry::path::toPath(ring));
  builder.setFillType(SkPathFillType::kEvenOdd);
  SkPath figure = builder.detach();
  if (margin > 0) figure = geometry::path::operations::offset(figure, margin);
  Element enclosure = pathFigure(figure, 1.0f);
  enclosure.key("hull:" + (lane.empty() ? styleClass : lane));
  if (fill) enclosure.fill(*fill);
  scope.attach(std::move(enclosure));
}

}  // namespace sigil::compose::outline
