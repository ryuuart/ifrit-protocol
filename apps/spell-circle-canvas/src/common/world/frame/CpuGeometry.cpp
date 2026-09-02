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

/** An emitter as the mesh painter takes it: the one directional reading
 *  every tier that shades without a per-pixel position works from. */
render::Light painterLight(const Light& light) {
  const light::Directional value = light::directional(light);
  render::Light out;
  out.direction = value.direction;
  out.color = SkColor4f{value.color.r, value.color.g, value.color.b, 1.0f};
  out.intensity = value.intensity;
  return out;
}

render::MeshStyle litStyle(const View& view) {
  render::MeshStyle style;
  style.runtime = render::Runtime::cpu();
  if (!view.lights.empty()) {
    style.lights.clear();
    for (const Light& light : view.lights)
      style.lights.push_back(painterLight(light));
  }
  // THE SET'S PANORAMA, once for the whole list: it is a property of the
  // frame and not of a body, and building it per draw would re-copy the
  // chain for every triangle set in the scene.
  style.environment = paintedEnvironment(view.environment, view.orientation);
  return style;
}

/** Flat white and nothing else: coverage is where the selection is, not
 *  what it looks like. */
render::MeshStyle coverageStyle() {
  render::MeshStyle style;
  style.runtime = render::Runtime::cpu();
  style.lights.clear();
  // A COVERAGE MASK IS NOT A PICTURE, so nothing shades it: no emitter
  // reaches it, and the tone curve every lit sum ends at does not touch
  // it either. Flat white is what a mask must be, and the device draws
  // it with the scaffold's unlit build for the same reason.
  style.lit = false;
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

/** The map a body is dressed with and whether the emitters reach it, put
 *  on the style — and taken off it again for a body carrying neither,
 *  since one style is reused across the whole list. */
void dress(render::MeshStyle& style, const Draw& draw) {
  const Sampling sampling =
      draw.texture ? samplingOf(*draw.texture) : Sampling{};
  style.texture = sampling.image;
  style.uvTransform = sampling.uv;
  style.tileTexture = sampling.tile;
  style.filter = sampling.filter;
  style.lit = draw.lit;
  const SurfaceTerms terms = surfaceTermsOf(draw.material);
  style.metallic = terms.metallic;
  style.roughness = terms.roughness;
}

void drawSelection(SkCanvas& canvas, const View& view, const Selector& selector,
                   render::MeshStyle& style, bool flat) {
  for (const Draw& draw : view.draws) {
    if (!draw.mesh) continue;
    if (!selector.matches(subjectOf(draw))) continue;
    if (!flat) {
      style.baseColor = colourOf(draw.baseColor);
      dress(style, draw);
    }
    render::drawMesh(canvas, *draw.mesh, draw.world, view.camera,
                     viewportOf(view), style);
  }
}

/** The stamps of every point set the pass reads. A compute pass writes
 *  points; this is what makes them visible, and a pass with no stamp
 *  draws none of them.
 *
 *  The stamping is FORMED IN THE STORE and not here: a set that has not
 *  moved between two frames is one stamping, and re-forming it inside
 *  the rasteriser would instance the whole cloud again every frame
 *  however still the pass is. */
void drawStamps(SkCanvas& canvas, const Pass& pass, const View& view,
                Targets& targets, render::MeshStyle& style) {
  if (pass.stamp().positions.empty()) return;
  for (const std::string& name : pass.reads()) {
    const Cloud* cloud = targets.points(name);
    if (!cloud || cloud->positions.empty()) continue;
    const Mesh* stamped = targets.stamped(*cloud, pass.stamp());
    if (!stamped || stamped->indices.empty()) continue;
    style.baseColor = colourOf({0.9f, 0.9f, 0.95f, 1.0f});
    style.texture = nullptr;
    // A stamp wears no material, so it says nothing of its own about
    // the map or the light and takes the pass's reading of both.
    style.filter = SkFilterMode::kLinear;
    style.lit = true;
    render::drawMesh(canvas, *stamped, glm::mat4(1.0f), view.camera,
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
    // THE SKY FIRST, where the set shows one: it stands behind every
    // body in the frame, so it is painted before the first of them and
    // nothing about it is a body's business.
    render::drawBackdrop(*canvas, style.environment,
                         view.camera.projection(viewportOf(view).width() > 0 ? viewportOf(view).width() / viewportOf(view).height() : 1.0f),
                         view.camera.view(), viewportOf(view));
    const bool cull = work.realisation == Selection::Cull;
    for (const Draw& draw : view.draws) {
      if (!draw.mesh) continue;
      if (cull && !pass.selector().matches(subjectOf(draw))) continue;
      style.baseColor = colourOf(draw.baseColor);
      dress(style, draw);
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
      // The variant surface is a colour laid over the bodies a selector
      // names, and the pass's own lights are what it stands under —
      // whatever the last body drawn happened to say about its own.
      over.lit = true;
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
