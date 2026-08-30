/** @file
 * A body as a selector reads it.
 */

#include <sigilworld/frame/View.h>

namespace sigil::world {

Subject subjectOf(const Draw& draw) {
  return Subject{draw.key, draw.tags, draw.ancestors, draw.material};
}

}  // namespace sigil::world
