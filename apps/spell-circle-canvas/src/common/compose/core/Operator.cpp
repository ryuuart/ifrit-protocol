/** @file
 * The adapter between the two shapes an arranging operator takes: the
 * table a `place(const LayoutInput&)` scheme reads, built from the
 * arrangement's records, and the rectangles it answers written back.
 */

#include "sigilcompose/core/Operator.h"

#include <algorithm>

namespace sigil::compose::detail {

LayoutInput layoutInputOf(const Arrangement& arrangement) {
  LayoutInput input;
  input.container = {arrangement.box.width(), arrangement.box.height()};
  input.childSizes.reserve(arrangement.children.size());
  input.childBaselines.reserve(arrangement.children.size());
  input.childCells.reserve(arrangement.children.size());
  input.childAreas.reserve(arrangement.children.size());
  input.childAttributes.reserve(arrangement.children.size());
  for (const Arrangement::Child& child : arrangement.children) {
    input.childSizes.push_back(child.size);
    input.childBaselines.push_back(child.baseline);
    input.childCells.push_back(child.cells);
    input.childAreas.push_back(child.area);
    input.childAttributes.push_back(child.attributes);
  }
  // The minima are filled only for a scheme that asked, and the table
  // says so by their presence: a scheme reading them where none were
  // measured would read an empty list, which is the contract it declares
  // against.
  if (arrangement.minSizesMeasured)
    for (const Arrangement::Child& child : arrangement.children)
      input.childMinSizes.push_back(child.minSize);
  return input;
}

void placeFromRects(Arrangement& arrangement, const std::vector<SkRect>& rects) {
  const size_t count = std::min(rects.size(), arrangement.children.size());
  for (size_t i = 0; i < count; ++i) arrangement.children[i].rect = rects[i];
}

}  // namespace sigil::compose::detail
