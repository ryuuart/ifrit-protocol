/** @file
 * The executor seam: dabs, or a stroke, laid down with a tool.
 */

#include "DabStyle.h"
#include "Executors.h"

#include <include/core/SkCanvas.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Sampler.h>

#include <algorithm>
#include <vector>

namespace sigil::draw::brush {

namespace {

/** The endpoint buildup of `markerTip`: the layers stacked at each end of
 *  the stroke, and how much each layer's opacity is raised. A nib pools
 *  more than an image or custom tip. */
constexpr int kNibBuildupLayers = 9;
constexpr int kTipBuildupLayers = 4;
constexpr float kNibBuildup = 8.0f;
constexpr float kTipBuildup = 2.0f;

/** Whether the grain has to be taken out of the whole deposit at once
 *  rather than out of each stamp. Only a shape tip can carry a grain
 *  with it, so every other tip takes its grain standing still, and a
 *  stroke-space grain always does — pigment accumulates first and the
 *  texture cuts the accumulated mark, which is what makes two strokes
 *  crossing meet one surface instead of two. */
bool grainStandsStill(const Tool& tool) {
  return tool.grain && tool.grain->image &&
         !(tool.tip == Tip::Image && tool.grain->space == GrainSpace::Dab);
}

}  // namespace

void deposit(Pen& pen, const Tool& tool, std::span<const Dab> dabs,
             DepositOptions options) {
  if (!(tool.width > 0.0f) || !(tool.opacity > 0.0f)) return;
  pen.push();
  pen.blendMode(tool.blend);
  // The standing grain wants the whole run of dabs in one place before
  // it takes anything away, so the deposit goes into a layer of its own
  // and the texture is drawn over that layer's coverage.
  const bool standingGrain = grainStandsStill(tool);
  SkCanvas* canvas = pen.canvas();
  if (standingGrain && canvas) canvas->saveLayer(nullptr, nullptr);
  const auto finish = [&] {
    if (standingGrain && canvas) {
      layerGrain(pen, *tool.grain);
      canvas->restore();
    }
    pen.pop();
  };
  if (tool.tip == Tip::Fibres) {
    depositFibres(pen, tool, dabs);
    finish();
    return;
  }
  std::vector<Stamp> stamps;
  if (tool.tip == Tip::Dust || tool.tip == Tip::Nib ||
      tool.tip == Tip::Scatter)
    stamps.reserve(dabs.size() * (tool.tip == Tip::Scatter
                                      ? (size_t)std::max(1, tool.bristles)
                                      : 1u));
  const SkPaint shapePaint =
      tool.tip == Tip::Image ? shapeTipPaint(pen, tool) : SkPaint();
  for (size_t index = 0; index < dabs.size(); ++index) {
    const Dab& dab = dabs[index];
    const DabStyle style = styleDab(
        pen, tool, dab, tool.tip != Tip::Scatter && tool.tip != Tip::Dust);
    if (!(style.size > 0.0f) || !(style.opacity > 0.0f)) continue;
    const auto put = [&](const DabStyle& mark) {
      switch (tool.tip) {
        case Tip::Dust:
          depositDust(pen, tool, dab, mark, stamps);
          break;
        case Tip::Nib:
          depositNib(tool, mark, stamps);
          break;
        case Tip::Fibres:
          break;
        case Tip::Scatter:
          depositScatter(pen, tool, dab, mark, stamps);
          break;
        case Tip::Image:
          depositShape(pen, tool, shapePaint, mark);
          break;
        case Tip::Custom:
          depositCustom(pen, tool, dab, mark);
          break;
      }
    };
    put(style);
    const bool endpoint = (options.start && index == 0) ||
                          (options.end && index + 1 == dabs.size());
    if (tool.markerTip && endpoint &&
        (tool.tip == Tip::Nib || tool.tip == Tip::Image ||
         tool.tip == Tip::Custom)) {
      const int layers =
          tool.tip == Tip::Nib ? kNibBuildupLayers : kTipBuildupLayers;
      const float buildup = tool.tip == Tip::Nib ? kNibBuildup : kTipBuildup;
      for (int layer = 1; layer <= layers; ++layer) {
        DabStyle end = styleDab(pen, tool, dab, true);
        end.size *= (float)layer * 0.1f;
        end.opacity = std::min(1.0f, end.opacity * buildup);
        put(end);
      }
    }
  }
  drawStamps(pen, tool.blend, stamps);
  finish();
}

void paint(Pen& pen, const Tool& tool, std::span<const Sample> stroke) {
  if (stroke.size() < 2 || !(tool.width > 0.0f) || !(tool.opacity > 0.0f))
    return;
  const Tool rolled = prepareStroke(pen, tool);
  std::vector<Input> input;
  input.reserve(stroke.size());
  for (const Sample& sample : stroke)
    input.push_back({.position = sample.position, .pressure = sample.pressure});
  deposit(pen, rolled, dabs(input, spacingOf(rolled)));
}

void line(Pen& pen, const Tool& tool, SkPoint from, SkPoint to,
          float startPressure, float endPressure) {
  paint(pen, tool,
        segment(from, to, spacingOf(tool), startPressure, endPressure));
}

void spline(Pen& pen, const Tool& tool, std::span<const Sample> controls,
            float curvature) {
  paint(pen, tool, brush::spline(controls, spacingOf(tool), curvature));
}

}  // namespace sigil::draw::brush
