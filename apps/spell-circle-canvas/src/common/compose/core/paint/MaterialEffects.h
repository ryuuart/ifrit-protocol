#pragma once

/** @file
 * THE EFFECTS STAGE OF A MATERIAL on a node: each coverage step of the
 * material's filter chain as a mark around the node's own shape — shadows
 * and glows beneath the fill, strokes and bevels over it — and a hard
 * shadow of the whole node (fill and text) as a misprint echo.
 */

#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>

#include <memory>
#include <optional>
#include <vector>

class SkCanvas;

namespace sigil::compose::detail {

struct ElementNode;
struct MaterialStrokeCache;
struct BevelFaceCache;

/** One coverage step, painted around the node's outline. */
struct CoverageMark {
  material::CoverageEffect effect;
  // The shoulder a bevel shaded from its face last cut; paint-time state
  // does not participate in the mark's value equality.
  mutable std::shared_ptr<BevelFaceCache> face = {};
  bool operator==(const CoverageMark& other) const {
    return effect == other.effect;
  }
  /** How far past the node's box the mark paints. */
  float bleed() const;
  void paint(draw::Pen& pen, const PaintContext& context) const;

 private:
  /** A bevel whose face is shaded from its shoulder, over the shape. */
  void paintBevelFace(draw::Pen& pen, const PaintContext& context) const;
};

/** A keyline around the node's outline painted with a material's colours;
 *  one whose material states a lit surface paints that surface's map on a
 *  page painted as surface maps. */
struct MaterialStroke {
  material::Paint source;
  material::StrokeOptions options;
  std::optional<material::Material> surfaced;
  bool operator==(const MaterialStroke& other) const {
    return source == other.source && options == other.options &&
           surfaced == other.surfaced;
  }
  // One prepared material and its surface map passes; paint-time state
  // does not participate in the mark's value equality.
  mutable std::shared_ptr<MaterialStrokeCache> cache = {};
  bool isRunning() const;
  bool usesWorldSpace() const;
  float bleed() const {
    return options.position == material::StrokePosition::Inside ? 0
                                                                : options.width;
  }
  void paint(draw::Pen& pen, const PaintContext& context) const;
};

/** WHAT A MATERIAL'S EFFECTS PUT ON A NODE: its coverage steps as marks
 *  beneath the fill (an outer shadow or glow) and over it (a stroke, a
 *  bevel, an inner shadow), a hard outer shadow as an echo, and the passes
 *  that read pixels as a filter over the node and its subtree. */
struct EffectMarks {
  std::vector<Decoration> beneath;
  std::vector<Decoration> over;
  std::vector<Echo> echoes;
  std::optional<material::Filter> pixels;
};

/** @p effects split into what each puts on a node. */
[[nodiscard]] EffectMarks effectMarksOf(const material::Filter& effects);

/** Splices @p effects onto @p node: its coverage steps as marks and
 *  echoes, and the passes that read pixels as the node's filter, which
 *  runs over the node and its subtree. */
void applyEffects(ElementNode& node, const material::Filter& effects);

}  // namespace sigil::compose::detail
