#pragma once

/** @file
 * Internal to the kernel — a length summed over several units, as CSS's
 * calc() holds one: a coefficient per unit and per custom property,
 * interned once so a Dimension carries it in the id its value holds.
 */

#include <sigilcompose/core/Layout.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace sigil::compose::detail {

/** A SUM OF LENGTHS IN SEVERAL UNITS: pixels (points already counted in
 *  them), the four font units, the two canvas units, and custom
 *  properties by id, each with its coefficient. Never a percentage. */
struct CalcLength {
  float px = 0.0f;
  float em = 0.0f;
  float rem = 0.0f;
  float lh = 0.0f;
  float ch = 0.0f;
  float pw = 0.0f;
  float ph = 0.0f;
  /** Property id and coefficient, ordered by id, none twice. */
  std::vector<std::pair<uint32_t, float>> vars;

  bool operator==(const CalcLength&) const = default;
};

/** The sum a `Dimension::Unit::Calc` length stands for. */
[[nodiscard]] const CalcLength& calcLength(const Dimension& length);

/** Says once, per pair of units, that a sum was asked to hold something
 *  no sum here can — a percentage beside another unit, or auto — and
 *  that it stands as auto instead. */
void warnRefusedSum(Dimension::Unit left, Dimension::Unit right);

/** Whether @p length is measured against the canvas, wholly or in part —
 *  which only a canvas that changes size moves. */
[[nodiscard]] bool readsCanvas(const Dimension& length);

}  // namespace sigil::compose::detail
