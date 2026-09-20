#pragma once

/** @file
 * @ingroup draw-brush
 *
 * Deposition: a tool laid along dabs or a stroke through the pen.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Field.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>

#include <span>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** Which ends of a run of dabs are the ends of a stroke, for the endpoint
 *  buildup of `markerTip`; a live stroke deposited in several runs marks
 *  only its first and last. */
struct DepositOptions {
  bool start = true;
  bool end = true;
};

/** Deposits already sampled dabs — THE EXECUTOR SEAM shared by stored
 *  paths, live stylus input, shape tips and caller-defined tips. Round
 *  dust, nib and scatter dabs go down as one sprite batch per stroke;
 *  fibres, shape and custom tips and the SUBTRACT blend draw through the
 *  pen's verbs dab by dab. The pen's style and transform are restored
 *  afterwards, the transform still applying to the mark. */
void deposit(Pen& pen, const Tool& tool, std::span<const Dab> dabs,
             DepositOptions options = {});

/** Deposits @p tool along @p stroke, rolling the stroke's randomness
 *  first. The dabs carry no speed: a stored path has no clock. */
void paint(Pen& pen, const Tool& tool, std::span<const Sample> stroke);

/** Straight and smoothed conveniences over the stroke constructors. */
void line(Pen& pen, const Tool& tool, SkPoint from, SkPoint to,
          float startPressure = 1.0f, float endPressure = 1.0f);
/** The curve through @p controls at @p curvature, sampled and then
 *  deposited. */
void spline(Pen& pen, const Tool& tool, std::span<const Sample> controls,
            float curvature = 0.5f);

/** Traces a field at the tool's spacing and paints the path. */
template <DirectionField Field>
void flowLine(Pen& pen, const Tool& tool, SkPoint start, float length,
              float seconds, const Field& field, float pressure = 1.0f) {
  paint(pen, tool,
        trace(start, length, spacingOf(tool), seconds, field, pressure));
}

}  // namespace sigil::draw::brush
