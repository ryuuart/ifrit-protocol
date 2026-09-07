#pragma once

/** @file
 * A tool that records what reaches its tip instead of drawing: every dab
 * the executor hands it, in order, with the pen's fill at the moment.
 */

#include <include/core/SkColor.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Tool.h>

#include <memory>
#include <vector>

namespace sigil::draw::brush::testing {

struct Recording {
  std::vector<Dab> dabs;
  std::vector<SkColor4f> fills;
};

/** A marker whose tip is a callback into @p recording, with every jitter
 *  and dynamic off so the dabs are exactly the sampled ones. */
inline Tool recorder(std::shared_ptr<Recording> recording,
                     float spacing = 4.0f) {
  Tool tool = marker(SkColors::kBlack, 2.0f);
  tool.tip = Tip::Custom;
  tool.spacing = spacing;
  tool.scatter = 0.0f;
  tool.markerTip = false;
  tool.pressure = {1, 1, 1};
  tool.pressure.variation.reset();
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.spacingJitter = 0.0f;
  tool.noise = 0.0f;
  tool.customTip = [recording](Pen& pen, const Dab& dab) {
    recording->dabs.push_back(dab);
    if (const SkPaint* paint = pen.fillPaint())
      recording->fills.push_back(paint->getColor4f());
  };
  return tool;
}

}  // namespace sigil::draw::brush::testing
