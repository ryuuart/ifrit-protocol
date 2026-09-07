/** @file
 * The one read of the pen's stream a weighted choice needs.
 */

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Choice.h>

namespace sigil::draw::brush {

float randomBelow(Pen& pen, float total) { return pen.random(total); }

}  // namespace sigil::draw::brush
