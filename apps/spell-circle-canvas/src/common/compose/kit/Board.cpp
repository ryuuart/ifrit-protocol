#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Board.h>

namespace sigil::compose::kit {

Element board(const Board& plate) {
  // Pinned at its parent's origin, and stretched on whichever axis was
  // left open: a size on an axis is the board's own extent, and no size
  // is the parent's far edge, which is the canvas for a root.
  Element root = stack().absolute().left(Dimension(0)).top(Dimension(0));
  if (plate.size.width() > 0)
    root.width(Dimension(plate.size.width()));
  else
    root.right(Dimension(0));
  if (plate.size.height() > 0)
    root.height(Dimension(plate.size.height()));
  else
    root.bottom(Dimension(0));
  if (!plate.ground.none()) root.fill(plate.ground);
  return root;
}

}  // namespace sigil::compose::kit
