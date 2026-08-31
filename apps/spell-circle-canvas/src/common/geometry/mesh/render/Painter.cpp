/** @file
 * The doors: each hands the work to the runtime it was given, so the
 * call site spells one thing whichever executor performs it.
 */

#include "sigilgeometry/mesh/render/Painter.h"

#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>

namespace sigil::geometry::mesh::render {

void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
              const camera::Camera& camera, SkSize viewport,
              const MeshStyle& style) {
  if (!style.runtime) return;
  style.runtime->drawMesh(canvas, mesh, model, camera, viewport, style);
}

void drawPanel(SkCanvas& canvas, const glm::mat4& model,
               const camera::Camera& camera, SkSize viewport,
               const std::function<void(SkCanvas&)>& draw,
               const Runtime& runtime) {
  if (!runtime) return;
  runtime->drawPanel(canvas, model, camera, viewport, draw);
}

void drawImagePanel(SkCanvas& canvas, sk_sp<SkImage> image, float width,
                    float height, const glm::mat4& model,
                    const camera::Camera& camera, SkSize viewport,
                    float opacity, const Runtime& runtime) {
  if (!image) return;
  drawPanel(
      canvas, model, camera, viewport,
      [&](SkCanvas& local) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setAlphaf(opacity);
        local.drawImageRect(
            image,
            SkRect::MakeXYWH(-width * 0.5f, -height * 0.5f, width, height),
            SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
            &paint);
      },
      runtime);
}

}  // namespace sigil::geometry::mesh::render
