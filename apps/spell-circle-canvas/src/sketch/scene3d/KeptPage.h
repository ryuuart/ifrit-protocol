#pragma once

/** @file
 * The one way a context makes a page: what both runtimes' contexts hand a
 * body when it asks for one, so a page painted for a baked frame and a page
 * painted for a scene3d frame stand on the same device and are kept the same
 * way.
 */

#include <sigilcompose/texture/SurfaceRange.h>
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

/** A PAGE @p size pixels across, its maps read against @p range, painting
 *  with @p fonts, and the scene it
 *  paints into put in @p pages when the session lent somewhere to keep it.
 *  It paints on the device the process draws on, in a window and in a
 *  capture alike, and on the CPU where the process holds none. The first page
 *  in @p pages is named "page" and each one after it is numbered, so two
 *  pages on one frame are two passes an error can tell apart. No page when
 *  the scene cannot be made. */
[[nodiscard]] Page keptPage(
    glm::ivec2 size, compose::SurfaceRange range, weave::FontContext& fonts,
    std::vector<std::shared_ptr<compose::SurfaceScene>>* pages);

}  // namespace sigil::sketch::detail
