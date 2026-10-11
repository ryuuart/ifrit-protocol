#pragma once

/** @file
 * A material painted over an outline as the unit it reads: what a relief
 * and a ribbon both paint with.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/core/Material.h>

#include <optional>

namespace sigil::draw {
class Pen;
}

namespace sigil::compose::detail {

class UnitSurfaces;

/** @p material painted over @p outline, between its coverage effects and
 *  under its pixel effects. On a page painted as surface maps the outline
 *  is the unit the material reads — the relief cut from its coverage, the
 *  brushing along @p direction where one is painted and along the
 *  outline's long axis where none is — with @p units holding the prepared
 *  maps; every other draw paints the material's colours flat. */
void paintUnit(const material::Material& material, UnitSurfaces& units,
               draw::Pen& pen, const PaintContext& context,
               const SkPath& outline,
               const std::optional<material::Material>& direction);

}  // namespace sigil::compose::detail
