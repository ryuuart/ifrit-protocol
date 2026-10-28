/** @file
 * Lengths against the font: a length in any unit as pixels at a node — in
 * the font, the root's font, the line and the zero's advance in force
 * there, on the canvas, or read from a custom property — the first-line
 * indent resolved the same way, and the line height and zero advance a
 * font sets.
 */

#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "CascadePass.h"
#include "description/Calc.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The extent of @p canvas a viewport unit is a percentage of: its width,
 *  its height, or the smaller or the larger of the two. */
float viewportExtent(const SkSize& canvas, Dimension::Unit unit) {
  switch (unit) {
    case Dimension::Unit::Vw:
      return canvas.width();
    case Dimension::Unit::Vh:
      return canvas.height();
    case Dimension::Unit::Vmin:
      return std::min(canvas.width(), canvas.height());
    default:
      return std::max(canvas.width(), canvas.height());
  }
}

}  // namespace

float Composer::Impl::resolveLength(const Instance& inst,
                                    const Dimension& length, bool& relative,
                                    float percentBasis) const {
  switch (length.unit) {
    case Dimension::Unit::Px:
    case Dimension::Unit::Percent:
      return length.value;
    case Dimension::Unit::Auto:
      return YGUndefined;
    case Dimension::Unit::Em:
      relative = true;
      return length.value * fontSizePx(inst.font);
    case Dimension::Unit::Rem:
      relative = true;
      return length.value * remPx;
    case Dimension::Unit::Lh:
      relative = true;
      return length.value * inst.lineHeight;
    case Dimension::Unit::Ch:
      relative = true;
      return length.value * inst.zeroAdvance;
    case Dimension::Unit::Pt:
      return length.value * sigil::weave::Length::kPointPx;
    case Dimension::Unit::Vw:
    case Dimension::Unit::Vh:
    case Dimension::Unit::Vmin:
    case Dimension::Unit::Vmax: {
      // THE CANVAS, never the parent: the box the composer renders into,
      // which is why this is resolved here and not handed to Yoga as a
      // percent. Under an intrinsic root — a snapshot, which has no
      // viewport — the root's own laid-out extent is the honest stand-in.
      relative = true;
      const SkSize canvas = size.isEmpty() ? rootLayoutSize : size;
      return length.value * 0.01f * viewportExtent(canvas, length.unit);
    }
    case Dimension::Unit::Var:
      // Dereferenced by the caller, which decides between a pixel and a
      // percent setter; a reference that reaches here names nothing.
      relative = true;
      return 0.0f;
    case Dimension::Unit::Calc: {
      // Every term in pixels here, where the font, the properties and the
      // canvas in force are known. A property read inside a sum must hold
      // a length a sum can hold: not a percentage, not auto, and not
      // another property.
      relative = true;
      const CalcLength& sum = calcLength(length);
      const SkSize canvas = size.isEmpty() ? rootLayoutSize : size;
      float total =
          sum.px + sum.em * fontSizePx(inst.font) + sum.rem * remPx +
          sum.lh * inst.lineHeight + sum.ch * inst.zeroAdvance +
          sum.vw * 0.01f * canvas.width() + sum.vh * 0.01f * canvas.height() +
          sum.vmin * 0.01f * viewportExtent(canvas, Dimension::Unit::Vmin) +
          sum.vmax * 0.01f * viewportExtent(canvas, Dimension::Unit::Vmax);
      // The percentage is of the extent the property reading the sum
      // measures against, which only that property knows.
      if (sum.percent != 0.0f) {
        if (std::isfinite(percentBasis))
          total += sum.percent * 0.01f * percentBasis;
        else
          warnPercentageSumUnresolved(
              "a property measured without a "
              "containing extent",
              "the percentage term is left out");
      }
      for (const auto& [id, coefficient] : sum.vars) {
        const VarValue* value =
            inst.vars ? inst.vars->find(VarRef{id}) : nullptr;
        const Dimension* found =
            value ? std::get_if<Dimension>(value) : nullptr;
        if (found == nullptr || found->unit == Dimension::Unit::Var) {
          if (inst.cascadeResolved) warnNoSuchVar(VarRef{id}, false);
          continue;
        }
        if (found->unit == Dimension::Unit::Percent ||
            found->unit == Dimension::Unit::Auto) {
          warnRefusedSum(found->unit, Dimension::Unit::Calc);
          continue;
        }
        bool inner = false;
        total += coefficient * resolveLength(inst, *found, inner);
      }
      return total;
    }
  }
  return length.value;
}

void Composer::Impl::resolveTextIndent(const Instance& inst,
                                       const Dimension& indent,
                                       sigil::weave::ParagraphBlock& block,
                                       std::optional<float>& percent) const {
  // A property is read once, into the length it holds, and that length is
  // resolved as if it had been written here.
  const Dimension* length = &indent;
  if (indent.unit == Dimension::Unit::Var) {
    const VarValue* value =
        inst.vars ? inst.vars->find(indent.reference()) : nullptr;
    length = value ? std::get_if<Dimension>(value) : nullptr;
    if (length == nullptr || length->unit == Dimension::Unit::Var ||
        length->unit == Dimension::Unit::Auto) {
      warnNoSuchVar(indent.reference(), false);
      return;
    }
  }
  if (length->unit == Dimension::Unit::Percent) {
    percent = length->value;
    block.firstLineIndent.reset();
    return;
  }
  // A sum holding a percentage of the measure is not one the paragraph
  // can take: its first-line indent is a pixel length or a percentage,
  // never both.
  if (percentageSum(*length))
    warnPercentageSumUnresolved("textIndent",
                                "its percentage term is left out");
  bool relative = false;
  block.firstLineIndent =
      resolveLength(inst, *length, relative, /*percentBasis=*/0.0f);
  percent.reset();
}

SkRRect Composer::Impl::cornersRRect(const Instance& inst,
                                     const SkRect& bounds) const {
  const Corners& corners = inst.computed.corners;
  const auto radius = [&](const Dimension& stated) -> SkVector {
    const Dimension* length = &stated;
    if (stated.unit == Dimension::Unit::Var) {
      const VarValue* value =
          inst.vars ? inst.vars->find(stated.reference()) : nullptr;
      length = value ? std::get_if<Dimension>(value) : nullptr;
      if (length == nullptr || length->unit == Dimension::Unit::Var)
        return {0, 0};
    }
    switch (length->unit) {
      case Dimension::Unit::Px:
        return {length->value, length->value};
      case Dimension::Unit::Auto:
        return {0, 0};
      case Dimension::Unit::Percent:
        return {length->value * 0.01f * bounds.width(),
                length->value * 0.01f * bounds.height()};
      default: {
        // A sum's percentage term is of the width across and of the height
        // down, as a percentage standing alone is.
        bool relative = false;
        return {resolveLength(inst, *length, relative, bounds.width()),
                resolveLength(inst, *length, relative, bounds.height())};
      }
    }
  };
  const SkVector radii[4] = {radius(corners.topLeft), radius(corners.topRight),
                             radius(corners.bottomRight),
                             radius(corners.bottomLeft)};
  SkRRect rrect;
  rrect.setRectRadii(bounds, radii);
  return rrect;
}

void Composer::Impl::resolveLineHeight(
    const Instance& inst, const Dimension& length,
    sigil::weave::ParagraphBlock& block) const {
  const Dimension* stated = &length;
  if (length.unit == Dimension::Unit::Var) {
    const VarValue* value =
        inst.vars ? inst.vars->find(length.reference()) : nullptr;
    stated = value ? std::get_if<Dimension>(value) : nullptr;
    if (stated == nullptr || stated->unit == Dimension::Unit::Var ||
        stated->unit == Dimension::Unit::Auto) {
      warnNoSuchVar(length.reference(), false);
      return;
    }
  }
  // CSS's percentage of a line height is of the element's own type size,
  // which is what an em is here.
  const float sizePx = fontSizePx(inst.font);
  bool relative = false;
  const float pitch =
      stated->unit == Dimension::Unit::Percent
          ? stated->value / 100.0f * sizePx
          : resolveLength(inst, *stated, relative, /*percentBasis=*/sizePx);
  block.lineHeight = sigil::weave::LineHeight::absolute(pitch);
}

float Composer::Impl::lineHeightAt(const sigil::weave::Type& font) {
  return sigil::weave::lineHeightOf(sigil::weave::toTextStyle(font), fonts);
}

float Composer::Impl::zeroAdvanceAt(const sigil::weave::Type& font) {
  return sigil::weave::zeroAdvanceOf(sigil::weave::toTextStyle(font), fonts);
}

}  // namespace sigil::compose
