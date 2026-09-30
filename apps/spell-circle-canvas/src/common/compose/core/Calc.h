#pragma once

/** @file
 * Internal to the kernel — a length summed over several units, as CSS's
 * calc() holds one: a coefficient per unit and per custom property,
 * interned once, counted by the Dimensions whose value carries its id,
 * and dropped with the last of them.
 */

#include <sigilcompose/core/Layout.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace sigil::compose::detail {

/** A SUM OF LENGTHS IN SEVERAL UNITS: pixels (points already counted in
 *  them), a percentage, the four font units, the two canvas units, and
 *  custom properties by id, each with its coefficient.
 *
 *  THE PERCENTAGE IS THE CONSUMER'S. Everything else resolves where the
 *  node lands, from the font, the properties and the canvas in force; a
 *  percentage is of an extent only the property that reads the sum knows
 *  — a positioned inset measures its containing box, a mark's inset the
 *  rect its selector resolved — so the sum keeps it as a term until that
 *  consumer adds it against its own extent. */
struct CalcLength {
  float px = 0.0f;
  float pct = 0.0f;
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

/** The sum a `Dimension::Unit::Calc` length stands for, valid while
 *  @p length holds it. */
[[nodiscard]] const CalcLength& calcLength(const Dimension& length);

/** Says once, per pair of units, that a sum was asked to hold something
 *  no sum here can — auto, or a percentage read through a custom property
 *  inside a sum — and that it stands as auto instead. */
void warnRefusedSum(Dimension::Unit left, Dimension::Unit right);

/** The percentage @p length holds: a percent's own value, a sum's
 *  percentage term, and zero for everything else. */
[[nodiscard]] float percentOf(const Dimension& length);

/** Whether @p length is a sum with a percentage in it — a length only a
 *  consumer that knows its containing extent can resolve. */
[[nodiscard]] bool percentageSum(const Dimension& length);

/** Says once per @p consumer that a sum holding a percentage reached a
 *  property with no extent to measure the percentage against, and what
 *  that property did with it instead; the sum itself is left as it was
 *  written. */
void warnPercentageSumUnresolved(const char* consumer, const char* instead);

/** Whether @p length is measured against the canvas, wholly or in part —
 *  which only a canvas that changes size moves. */
[[nodiscard]] bool readsCanvas(const Dimension& length);

}  // namespace sigil::compose::detail
