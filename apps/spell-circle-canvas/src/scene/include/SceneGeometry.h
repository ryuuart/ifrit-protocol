#pragma once

// Qt-free geometry resolution: turns a SceneDocument's author-space entities
// into absolute, native-scaled canvas positions ready for drawing. This is
// the only place scaling and point-on-circle math happens; both scene
// backends (Skia and QCanvasPainter) and the native macOS app draw from the
// ResolvedScene it produces.

#include "SceneModel.h"

#include <string>
#include <vector>

namespace spellcircle {

/** A 2D position/direction in native canvas coordinates. */
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

/** A circle resolved to absolute, native-scaled canvas coordinates. */
struct ResolvedCircle {
  std::string name; // UTF-8
  Vec2 center;
  float radius = 0.0f;
  float textStart = 0.0f;
  float active = 0.0f; // background fill alpha/intensity [0, 1]
};

/** A straight connector between two resolved point positions. */
struct ResolvedEdge {
  Vec2 first;
  Vec2 second;
};

/** A labelled box anchored at a resolved point position, offset outward
 *  along the ray from the canvas center through that point. */
struct ResolvedBox {
  std::string value; // UTF-8
  Vec2 anchor;
  // Unit vector from the canvas center through `anchor`, used both to push
  // the box outward by the configured distance and to pick which of its
  // edges (the one facing the center) sits at that offset.
  Vec2 direction;
  float active = 0.0f; // background fill alpha/intensity [0, 1]
};

/** Every entity of a SceneDocument resolved into drawable geometry. */
struct ResolvedScene {
  std::vector<ResolvedCircle> circles;
  std::vector<ResolvedEdge> edges;
  std::vector<ResolvedBox> boxes;
  // Point.value labels: positioned the same way as boxes (anchor pushed
  // outward along the ray from the canvas center) and sourced from every
  // PointComponent with a non-empty value rather than from BoxComponent — a
  // Point already anchoring a Box or Edge can still carry its own label —
  // but drawn as plain text with no surrounding rect/stroke/fill, since a
  // point isn't a box. `active` is unused here (Point has no fill concept).
  std::vector<ResolvedBox> pointLabels;

  void clear();
};

/**
 * Resolves every entity of @p document into absolute canvas coordinates for
 * a native canvas of @p canvasWidth x @p canvasHeight pixels. Scenes may be
 * authored in a smaller coordinate space and scaled up to the native canvas;
 * document width/height of 0 mean coordinates are already native.
 */
ResolvedScene resolveScene(const SceneDocument &document, float canvasWidth,
                           float canvasHeight);

} // namespace spellcircle
