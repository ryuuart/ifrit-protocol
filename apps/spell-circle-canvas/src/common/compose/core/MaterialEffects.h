#pragma once

/** @file
 * THE EFFECTS STAGE OF A MATERIAL on a node: each coverage step of the
 * material's filter chain as a mark around the node's own shape — shadows
 * and glows beneath the fill, strokes and bevels over it — and a hard
 * shadow of the whole node (fill and text) as a misprint echo.
 */

#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/filter/Filter.h>

class SkCanvas;

namespace sigil::compose::detail {

struct ElementNode;

/** One coverage step, painted around the node's outline. */
struct CoverageMark {
  material::CoverageEffect effect;
  bool operator==(const CoverageMark&) const = default;
  /** How far past the node's box the mark paints. */
  float bleed() const;
  void paint(SkCanvas& canvas, const PaintContext& context) const;
};

/** A keyline around the node's outline painted with a material. */
struct MaterialStroke {
  material::Paint source;
  material::StrokeOptions options;
  bool operator==(const MaterialStroke&) const = default;
  float bleed() const {
    return options.position == material::StrokePosition::Inside ? 0
                                                                : options.width;
  }
  void paint(SkCanvas& canvas, const PaintContext& context) const;
};

/** Splices @p effects onto @p node: its coverage steps as marks and
 *  echoes, and the passes that read pixels as the node's filter, which
 *  runs over the node and its subtree. */
void applyEffects(ElementNode& node, const material::Filter& effects);

}  // namespace sigil::compose::detail
