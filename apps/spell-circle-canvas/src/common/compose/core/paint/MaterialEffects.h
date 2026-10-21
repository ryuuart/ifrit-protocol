#pragma once

/** @file
 * THE EFFECTS STAGE OF A MATERIAL on a node: each coverage step of the
 * material's filter chain as a mark around the node's own shape — shadows
 * and glows beneath the fill, strokes and bevels over it — and a hard
 * shadow of the whole node (fill and text) as a misprint echo.
 */

#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>

#include <memory>
#include <optional>
#include <vector>

class SkCanvas;

namespace sigil::compose::detail {

struct ElementNode;

/** One coverage step, painted around the node's outline. */
struct CoverageMark {
  material::CoverageEffect effect;
  bool operator==(const CoverageMark&) const = default;
  /** How far past the node's box the mark paints. */
  float bleed() const;
  /** Every step is cast from the outline it is handed: a shadow or a glow
   *  around it, a stroke along it, a bevel and an inner shadow cut to it. */
  bool staysOnOutline() const { return true; }
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
  StrokeOptions options;
  std::optional<material::Material> surfaced;
  bool operator==(const MaterialStroke&) const = default;
  bool isRunning() const;
  bool usesWorldSpace() const;
  /** How far the keyline reaches past the node's box. One set inside a
   *  closed contour stays within it, but an open contour has no inside and
   *  takes one band centred on it, half of it past a box edge the curve
   *  runs along; the outline is not known here, so inside answers that
   *  half. A square cap reaches its half width past the end on either
   *  side of it, and a mitred corner up to its limit. */
  float bleed() const;
  /** The keyline is the outline it is handed, widened. */
  bool staysOnOutline() const { return true; }
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
