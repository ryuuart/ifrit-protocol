/** @file
 * Arithmetic on lengths, as CSS's calc(): lengths in one unit combined in
 * that unit, lengths in several summed into an interned sum, and the
 * percentage refused wherever it would have to share a sum.
 */

#include "Calc.h"

#include <include/core/SkTypes.h>  // SkDebugf — the once-per-mixture warning
#include <sigilweave/style/Length.h>

#include <algorithm>
#include <bit>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace sigil::compose {
namespace detail {
namespace {

/** The process-wide table a sum is interned in: kept in a deque so a
 *  reference handed out stays valid when a later sum is added, and never
 *  freed, because a describe on another thread may still be reading one
 *  while the process exits. */
struct CalcTable {
  std::mutex mutex;
  std::deque<CalcLength> sums;  // id − 1 → sum
  boost::unordered_flat_map<std::string, uint32_t> ids;
};

CalcTable& calcTable() {
  static CalcTable* const table = new CalcTable;
  return *table;
}

/** The bytes a sum is keyed by. A zero of either sign is the one zero. */
std::string keyOf(const CalcLength& sum) {
  std::string key;
  const auto put = [&](float value) {
    value += 0.0f;
    char bytes[sizeof value];
    std::memcpy(bytes, &value, sizeof value);
    key.append(bytes, sizeof bytes);
  };
  for (const float term :
       {sum.px, sum.em, sum.rem, sum.lh, sum.ch, sum.pw, sum.ph})
    put(term);
  for (const auto& [id, coefficient] : sum.vars) {
    put(std::bit_cast<float>(id));
    put(coefficient);
  }
  return key;
}

const char* unitName(Dimension::Unit unit) {
  switch (unit) {
    case Dimension::Unit::Px:
      return "px";
    case Dimension::Unit::Pct:
      return "%";
    case Dimension::Unit::Auto:
      return "auto";
    case Dimension::Unit::Em:
      return "em";
    case Dimension::Unit::Rem:
      return "rem";
    case Dimension::Unit::Lh:
      return "lh";
    case Dimension::Unit::Var:
      return "var()";
    case Dimension::Unit::Pw:
      return "pw";
    case Dimension::Unit::Ph:
      return "ph";
    case Dimension::Unit::Ch:
      return "ch";
    case Dimension::Unit::Pt:
      return "pt";
    case Dimension::Unit::Calc:
      return "calc()";
  }
  return "?";
}

}  // namespace

void warnRefusedSum(Dimension::Unit left, Dimension::Unit right) {
  static thread_local boost::unordered_flat_map<uint16_t, bool> warned;
  const uint16_t pair = (uint16_t)(((uint16_t)left << 8) | (uint16_t)right);
  if (!warned.emplace(pair, true).second) return;
  SkDebugf(
      "[compose] a length in %s was combined with one in %s. A percentage "
      "is laid out by Yoga against the parent and cannot share a sum with "
      "another unit, and auto is no length at all, so the result is "
      "REFUSED and stands as auto. Use pw or ph to measure the canvas, or "
      "state the two on different properties. (warned once)\n",
      unitName(left), unitName(right));
}

namespace {

/** @p length times @p factor, as a sum. */
CalcLength termsOf(const Dimension& length, float factor) {
  CalcLength sum;
  switch (length.unit) {
    case Dimension::Unit::Px:
      sum.px = length.value * factor;
      break;
    case Dimension::Unit::Pt:
      sum.px = length.value * sigil::weave::Length::kPointPx * factor;
      break;
    case Dimension::Unit::Em:
      sum.em = length.value * factor;
      break;
    case Dimension::Unit::Rem:
      sum.rem = length.value * factor;
      break;
    case Dimension::Unit::Lh:
      sum.lh = length.value * factor;
      break;
    case Dimension::Unit::Ch:
      sum.ch = length.value * factor;
      break;
    case Dimension::Unit::Pw:
      sum.pw = length.value * factor;
      break;
    case Dimension::Unit::Ph:
      sum.ph = length.value * factor;
      break;
    case Dimension::Unit::Var:
      sum.vars.emplace_back(length.reference().id, factor);
      break;
    case Dimension::Unit::Calc: {
      sum = calcLength(length);
      for (float* term :
           {&sum.px, &sum.em, &sum.rem, &sum.lh, &sum.ch, &sum.pw, &sum.ph})
        *term *= factor;
      for (auto& [id, coefficient] : sum.vars) coefficient *= factor;
      break;
    }
    case Dimension::Unit::Pct:
    case Dimension::Unit::Auto:
      break;
  }
  return sum;
}

/** @p sum as a Dimension: in the one unit it holds where it holds one —
 *  so a sum that cancels back to a single unit is that unit again — and
 *  interned otherwise. */
Dimension fromSum(CalcLength sum) {
  std::erase_if(sum.vars, [](const auto& term) { return term.second == 0; });
  const std::pair<float, Dimension::Unit> units[] = {
      {sum.px, Dimension::Unit::Px},   {sum.em, Dimension::Unit::Em},
      {sum.rem, Dimension::Unit::Rem}, {sum.lh, Dimension::Unit::Lh},
      {sum.ch, Dimension::Unit::Ch},   {sum.pw, Dimension::Unit::Pw},
      {sum.ph, Dimension::Unit::Ph}};
  int stated = 0;
  Dimension single(0.0f);
  for (const auto& [value, unit] : units)
    if (value != 0.0f) {
      ++stated;
      single.unit = unit;
      single.value = value;
    }
  if (sum.vars.empty() && stated <= 1) return single;
  if (stated == 0 && sum.vars.size() == 1 && sum.vars.front().second == 1.0f)
    return Dimension(VarRef{sum.vars.front().first});
  CalcTable& table = calcTable();
  const std::lock_guard<std::mutex> lock(table.mutex);
  std::string key = keyOf(sum);
  uint32_t id = 0;
  if (const auto found = table.ids.find(key); found != table.ids.end()) {
    id = found->second;
  } else {
    table.sums.push_back(std::move(sum));
    id = (uint32_t)table.sums.size();
    table.ids.emplace(std::move(key), id);
  }
  Dimension interned;
  interned.unit = Dimension::Unit::Calc;
  interned.value = std::bit_cast<float>(id);
  return interned;
}

/** Whether @p length can stand in a sum: every unit but a percentage and
 *  auto. */
bool summable(const Dimension& length) {
  return length.unit != Dimension::Unit::Pct &&
         length.unit != Dimension::Unit::Auto;
}

bool zeroPixels(const Dimension& length) {
  return length.unit == Dimension::Unit::Px && length.value == 0.0f;
}

/** Whether both are the same plain unit, which combines in that unit. */
bool sameSimpleUnit(const Dimension& left, const Dimension& right) {
  return left.unit == right.unit && left.unit != Dimension::Unit::Var &&
         left.unit != Dimension::Unit::Calc &&
         left.unit != Dimension::Unit::Auto;
}

/** Says once that a length was divided by zero, which is no length, and
 *  that it stands as auto instead — as the calc() text refuses it. */
void warnDividedByZero() {
  static thread_local bool warned = false;
  if (std::exchange(warned, true)) return;
  SkDebugf(
      "[compose] a length was divided by zero, which is no length, so the "
      "result is REFUSED and stands as auto. (warned once)\n");
}

/** @p length times @p factor, or divided by it where @p dividing — a
 *  plain unit divided exactly, so a third of three ems is one em. */
Dimension scaled(const Dimension& length, float factor, bool dividing) {
  if (length.unit == Dimension::Unit::Auto) {
    warnRefusedSum(length.unit, Dimension::Unit::Px);
    return autoDimension();
  }
  if (dividing && factor == 0.0f) {
    warnDividedByZero();
    return autoDimension();
  }
  if (length.unit != Dimension::Unit::Var &&
      length.unit != Dimension::Unit::Calc) {
    Dimension out = length;
    out.value = dividing ? out.value / factor : out.value * factor;
    return out;
  }
  return fromSum(termsOf(length, dividing ? 1.0f / factor : factor));
}

Dimension summed(const Dimension& left, const Dimension& right, float sign) {
  if (sameSimpleUnit(left, right)) {
    Dimension out = left;
    out.value = left.value + sign * right.value;
    return out;
  }
  // A percentage stands only beside a zero of pixels, which adds nothing.
  if (left.unit == Dimension::Unit::Pct && zeroPixels(right)) return left;
  if (right.unit == Dimension::Unit::Pct && zeroPixels(left))
    return scaled(right, sign, false);
  if (!summable(left) || !summable(right)) {
    warnRefusedSum(left.unit, right.unit);
    return autoDimension();
  }
  CalcLength sum = termsOf(left, 1.0f);
  const CalcLength more = termsOf(right, sign);
  sum.px += more.px;
  sum.em += more.em;
  sum.rem += more.rem;
  sum.lh += more.lh;
  sum.ch += more.ch;
  sum.pw += more.pw;
  sum.ph += more.ph;
  for (const auto& [id, coefficient] : more.vars) {
    const auto found =
        std::find_if(sum.vars.begin(), sum.vars.end(),
                     [id](const auto& term) { return term.first == id; });
    if (found != sum.vars.end())
      found->second += coefficient;
    else
      sum.vars.emplace_back(id, coefficient);
  }
  std::sort(sum.vars.begin(), sum.vars.end());
  return fromSum(std::move(sum));
}

}  // namespace

const CalcLength& calcLength(const Dimension& length) {
  static const CalcLength* const nothing = new CalcLength;
  CalcTable& table = calcTable();
  const std::lock_guard<std::mutex> lock(table.mutex);
  const uint32_t id = std::bit_cast<uint32_t>(length.value);
  if (length.unit != Dimension::Unit::Calc || id == 0 || id > table.sums.size())
    return *nothing;
  return table.sums[id - 1];
}

bool readsCanvas(const Dimension& length) {
  if (length.unit == Dimension::Unit::Pw || length.unit == Dimension::Unit::Ph)
    return true;
  if (length.unit != Dimension::Unit::Calc) return false;
  const CalcLength& sum = calcLength(length);
  return sum.pw != 0.0f || sum.ph != 0.0f;
}

}  // namespace detail

Dimension operator+(Dimension left, Dimension right) {
  return detail::summed(left, right, 1.0f);
}
Dimension operator-(Dimension left, Dimension right) {
  return detail::summed(left, right, -1.0f);
}
Dimension operator-(Dimension length) {
  return detail::scaled(length, -1.0f, false);
}
Dimension operator*(Dimension length, float factor) {
  return detail::scaled(length, factor, false);
}
Dimension operator*(float factor, Dimension length) {
  return detail::scaled(length, factor, false);
}
Dimension operator/(Dimension length, float divisor) {
  return detail::scaled(length, divisor, true);
}

}  // namespace sigil::compose
