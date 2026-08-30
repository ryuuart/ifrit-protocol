/** @file
 * A geometry pass on the CPU: the bodies the pass's realisation leaves
 * it, the stamps of the point sets it reads, and the coverage a masked
 * pass downstream reads its selection from.
 */

#include <include/core/SkCanvas.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/render/Painter.h>

#include <glm/mat4x4.hpp>
#include <vector>

#include "Cpu.h"

namespace sigil::world::cpu {

namespace {

namespace render = ::sigil::geometry::mesh::render;

/** An emitter in the terms this tier shades in, which are directional.
 *  A sun already is one. A light that stands somewhere reaches the tier
 *  as the direction from where it stands toward the origin, at the
 *  strength it has there; the full falloff is `light::attenuation`. */
render::Light directional(const Light& light) {
  render::Light out;
  out.color = SkColor4f{light.color.r, light.color.g, light.color.b, 1.0f};
  out.intensity = light.intensity;
  if (light.kind == light::Kind::Sun) {
    out.direction = light.direction;
    return out;
  }
  const glm::vec3 toward = -light.position;
  out.direction = glm::dot(toward, toward) > 0.0f ? glm::normalize(toward)
                                                  : light.direction;
  out.intensity = light.intensity * light::attenuation(light, glm::vec3(0.0f));
  return out;
}

render::MeshStyle litStyle(const View& view) {
  render::MeshStyle style;
  style.runtime = render::Runtime::cpu();
  if (!view.lights.empty()) {
    style.lights.clear();
    for (const Light& light : view.lights)
      style.lights.push_back(directional(light));
  }
  return style;
}

/** Flat white and nothing else: coverage is where the selection is, not
 *  what it looks like. */
render::MeshStyle coverageStyle() {
  render::MeshStyle style;
  style.runtime = render::Runtime::cpu();
  style.lights.clear();
  style.ambient = {1.0f, 1.0f, 1.0f, 1.0f};
  style.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
  style.specular = 0.0f;
  style.rim = 0.0f;
  return style;
}

SkSize viewportOf(const View& view) {
  return SkSize::Make((float)view.extent.width(), (float)view.extent.height());
}

/** The base colour a material was resolved to, as the mesh painter
 *  takes it. */
SkColor4f colourOf(const glm::vec4& colour) {
  return SkColor4f{colour.r, colour.g, colour.b, colour.a};
}

void drawSelection(SkCanvas& canvas, const View& view, const Selector& selector,
                   render::MeshStyle& style, bool flat) {
  for (const Draw& draw : view.draws) {
    if (!draw.mesh) continue;
    if (!selector.matches(subjectOf(draw))) continue;
    if (!flat) style.baseColor = colourOf(draw.baseColor);
    render::drawMesh(canvas, *draw.mesh, draw.world, view.camera,
                     viewportOf(view), style);
  }
}

/** The stamps of every point set the pass reads. A compute pass writes
 *  points; this is what makes them visible, and a pass with no stamp
 *  draws none of them. */
void drawStamps(SkCanvas& canvas, const Pass& pass, const View& view,
                const Targets& targets, render::MeshStyle& style) {
  if (pass.stamp().positions.empty()) return;
  for (const std::string& name : pass.reads()) {
    const Cloud* cloud = targets.points(name);
    if (!cloud || cloud->positions.empty()) continue;
    const Cooked cooked = cook(Stamped{*cloud, pass.stamp()});
    if (cooked.mesh.indices.empty()) continue;
    style.baseColor = colourOf({0.9f, 0.9f, 0.95f, 1.0f});
    render::drawMesh(canvas, cooked.mesh, glm::mat4(1.0f), view.camera,
                     viewportOf(view), style);
  }
}

}  // namespace

void paintGeometry(const PassWork& work, const View& view, Targets& targets) {
  const Pass& pass = *work.pass;
  const std::string* name = target(pass);
  SkCanvas* canvas = name ? targets.canvas(*name) : nullptr;
  if (!canvas) return;
  canvas->clear(pass.clear());

  render::MeshStyle style = litStyle(view);
  const bool asCoverage = work.realisation == Selection::Mask;
  if (asCoverage) {
    style = coverageStyle();
    drawSelection(*canvas, view, pass.selector(), style, /*flat=*/true);
  } else {
    const bool cull = work.realisation == Selection::Cull;
    for (const Draw& draw : view.draws) {
      if (!draw.mesh) continue;
      if (cull && !pass.selector().matches(subjectOf(draw))) continue;
      style.baseColor = colourOf(draw.baseColor);
      render::drawMesh(*canvas, *draw.mesh, draw.world, view.camera,
                       viewportOf(view), style);
    }
    drawStamps(*canvas, pass, view, targets, style);
    // The selection, drawn again in the surface the pass named — which
    // is what makes it visible in a pass that paints everything.
    if (work.realisation == Selection::Variant && pass.variant()) {
      const material::Field* field =
          pass.variant()->recipe().params().find("baseColor");
      render::MeshStyle over = style;
      over.baseColor =
          field && field->floats == 4
              ? colourOf(pass.variant()->get<glm::vec4>("baseColor"))
              : SkColor4f{1.0f, 1.0f, 1.0f, 1.0f};
      drawSelection(*canvas, view, pass.selector(), over, /*flat=*/true);
    }
  }

  if (work.coverageOut.empty()) return;
  SkCanvas* coverage = targets.canvas(work.coverageOut);
  if (!coverage) return;
  coverage->clear(SkColor4f{0.0f, 0.0f, 0.0f, 0.0f});
  render::MeshStyle flat = coverageStyle();
  drawSelection(*coverage, view, work.coverageOf, flat, /*flat=*/true);
}

void cookPoints(const PassWork& work, Targets& targets) {
  const Pass& pass = *work.pass;
  const std::string* name = target(pass);
  if (!name || pass.chain().empty()) return;
  *targets.points(*name) =
      ::sigil::geometry::mesh::pop::cook(pass.chain(), pass.popRuntime());
}

}  // namespace sigil::world::cpu
