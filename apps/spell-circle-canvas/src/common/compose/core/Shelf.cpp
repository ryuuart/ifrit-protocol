#include <sigilcompose/core/Shelf.h>

#include <algorithm>
#include <cmath>

namespace sigil::compose {

Shelved shelve(std::span<const SkSize> boxes, const ShelfOptions& options) {
  Shelved out;
  out.cells.assign(boxes.size(), SkRect::MakeEmpty());
  const float pad = std::max(0.0f, options.padding);
  float penX = pad, penY = pad, shelfH = 0, sheetW = 0;
  for (size_t i = 0; i < boxes.size(); ++i) {
    const float w = boxes[i].width(), h = boxes[i].height();
    if (w <= 0 || h <= 0) continue;
    // The wrap tests the pen against the width a shelf ends at, and only
    // once something is already on the shelf: a box wider than the whole
    // sheet would otherwise wrap forever, one empty shelf per box.
    if (penX > pad && penX + w + pad > options.maxWidth) {
      penY += shelfH + pad;
      penX = pad;
      shelfH = 0;
    }
    out.cells[i] = SkRect::MakeXYWH(penX, penY, w, h);
    penX += w + pad;
    shelfH = std::max(shelfH, h);
    sheetW = std::max(sheetW, penX);
  }
  if (sheetW <= 0) return out;
  // The pen already carries the gap after the last box on its shelf, so
  // `sheetW` is the right margin as well as the content; the bottom margin
  // is the one that has to be added.
  out.sheet = {std::max(1, (int)std::ceil(sheetW)),
               std::max(1, (int)std::ceil(penY + shelfH + pad))};
  return out;
}

}  // namespace sigil::compose
