#pragma once
/** @file
 * @ingroup geometry-path
 *
 * POINTS WHERE A SHAPE SAYS: one verb, `points(where, pattern)`, over
 * every way of placing them — at random, on a grid, jittered, by Poisson
 * disc, dealt round a centre, or along an outline at a spacing. The
 * heading a point laid along a curve is turned by is `arrange::heading`.
 */
#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Radial.h"

namespace sigil::geometry::path {

/** How points are placed: one of the patterns below, as a comparable
 *  value. Build one with its function rather than by hand. */
struct Pattern {
  enum class Kind : uint8_t { Random, Poisson, Grid, Radial, Along };
  Kind kind = Kind::Random;
  /** The count a random or radial pattern places. */
  int count = 0;
  /** The spacing of a grid or a walk along, the radius of a Poisson
   *  disc. */
  float spacing = 0;
  /** A grid's jitter, as a fraction of the spacing. */
  float jitter = 0;
  /** Where a walk along starts and stops, in px of arc length; a
   *  negative `to` runs to the end. */
  float from = 0;
  float to = -1;
  uint64_t seed = 1;
  RadialOptions radial{};
  bool operator==(const Pattern&) const = default;
};

/** @p count points uniformly at random inside the shape. */
Pattern random(int count, uint64_t seed = 1);
/** Points no two closer than @p radius, as many as fit — blue noise. */
Pattern poisson(float radius, uint64_t seed = 1);

/** The dials of `grid()`. */
struct GridOptions {
  /** Each point nudged by up to this fraction of the spacing. */
  float jitter = 0;
  uint64_t seed = 1;
  bool operator==(const GridOptions&) const = default;
};
/** A square grid at @p spacing, cut to the shape. */
Pattern grid(float spacing, GridOptions options = {});

/** @p count points dealt round the centre of the shape's bounds, as
 *  `shapes::radial` deals its vertices — on a circle of half the bounds'
 *  shorter side. A golden-angle step with square-root growth is
 *  phyllotaxis. */
Pattern radial(int count, RadialOptions options = {});

/** The dials of `along()`. */
struct AlongOptions {
  /** Where the walk starts, in px of arc length. */
  float from = 0;
  /** Where it stops; negative runs to the end. */
  float to = -1;
  bool operator==(const AlongOptions&) const = default;
};
/** A point every @p spacing px along the outline, its contours walked as
 *  one run. */
Pattern along(float spacing, AlongOptions options = {});

/** The points @p pattern places in or on @p where: inside it for the
 *  area patterns, on it for `along`. */
std::vector<glm::vec2> points(const Outline& where, const Pattern& pattern);
/** The same over a rectangle. */
std::vector<glm::vec2> points(const Rect& where, const Pattern& pattern);

}  // namespace sigil::geometry::path
