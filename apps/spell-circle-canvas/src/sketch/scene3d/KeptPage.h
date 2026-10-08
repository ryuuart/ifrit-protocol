#pragma once

/** @file
 * The one way a context makes a page: what both runtimes' contexts hand a
 * body when it asks for one, so a page painted for a baked frame and a page
 * painted for a scene3d frame stand on the same device and are kept the same
 * way.
 */

#include <sigilsketch/scene3d/Page.h>

#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace sigil::weave {
class FontContext;
}
namespace sigil::compose {
class SurfaceScene;
}

namespace sigil::sketch::detail {

/** A PAGE @p size pixels across, painting with @p fonts, and the scene it
 *  paints into put in @p pages when the session lent somewhere to keep it.
 *  A live page paints on the device the sketch is drawn on; a capture that
 *  will be diffed, @p deterministic, keeps the CPU's pixels. The first page
 *  in @p pages is named "page" and each one after it is numbered, so two
 *  pages on one frame are two passes an error can tell apart. No page when
 *  the scene cannot be made. */
[[nodiscard]] Page keptPage(
    glm::ivec2 size, weave::FontContext& fonts, bool deterministic,
    std::vector<std::shared_ptr<compose::SurfaceScene>>* pages);

}  // namespace sigil::sketch::detail
