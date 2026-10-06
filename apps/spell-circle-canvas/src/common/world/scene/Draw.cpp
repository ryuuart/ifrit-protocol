/** @file
 * The draw: what the last render left, put on a canvas. A frame that
 * declared passes has already run them and the picture is one of its
 * resources, so the draw is a blit. It reads the plan and the targets —
 * the node tree is not reachable from here.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <sigilworld/advanced/Skia.h>

#include "SceneImpl.h"

namespace sigil::world {

void draw(Scene& scene, SkCanvas& canvas, Presentation presentation) {
  Scene::Impl& impl = SceneAccess::of(scene);
  if (impl.frame.passes().empty() || !impl.error.empty()) return;
  if (impl.plan.present().empty()) return;
  sk_sp<SkImage> picture;
  if (presentation == Presentation::OnDevice)
    picture = impl.targets.onDevice(impl.plan.present(), canvas);
  if (picture) {
    ++impl.stats.presentedOnDevice;
  } else {
    picture = impl.targets.image(impl.plan.present());
    if (!picture) return;
    ++impl.stats.readbacks;
  }
  // A frame formed over a PART of its picture lands where that part
  // stands in it, so the canvas is addressed in the whole picture's
  // pixels either way.
  const ViewOffset& offset = impl.frame.viewOffset();
  const glm::ivec2 corner =
      isPart(offset, impl.frame.extent()) ? offset.origin : glm::ivec2{0, 0};
  canvas.drawImage(picture, (float)corner.x, (float)corner.y);
}

}  // namespace sigil::world
