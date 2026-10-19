/** @file
 * mesh_normal_bridge — ONE REFLECTION, TWO KINDS OF SHAPE: a lit body in
 * space beside a flat outline wearing the same finish.
 *
 * A reflective finish needs two things: which way the surface faces at
 * every pixel, and an environment to look that direction up in. Where the
 * facing comes from is the only difference between the panels.
 *
 *   FROM A MESH. The superellipsoid and the torus stand in a world frame
 *     each, in the environment the panel names, and the world shades them:
 *     a metal at a low roughness mirrors the panorama, its colour tinting
 *     what it mirrors. The bodies are baked into the sheet's picture
 *     through `ctx.bakeScene3D`.
 *   FROM AN OUTLINE. `bevelNormals(path, bevelPx)` derives a rounded
 *     shoulder from a flat path's coverage, and the chrome recipe reads
 *     the sunset through it. No mesh exists anywhere in that panel; the
 *     shoulder is a distance field over the silhouette.
 *
 * WHY CURVATURE, NOT FLATNESS. Both meshed bodies curve in every
 * direction. An extruded cap would present one normal over its whole face
 * and mirror the environment at one point, which is a flat colour.
 *
 * EDIT THESE FIRST
 *   the superellipsoid's exponent — rounder bodies sweep more of the
 *                 environment across the silhouette.
 *   kBevelPx      — the shoulder's width on the third panel, px. Wide
 *                 enough and the squircle reads as the same kind of body
 *                 the mesh on the left is; narrow, and its interior is a
 *                 flat face mirroring the sky at one point.
 *   the environments — studio and sunset are differently coloured skies,
 *                 and chrome shows the difference hardest.
 */

// TAGS: Geometry/Meshes, Materials/Lighting

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/kit/Document.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <string>
#include <utility>

#include "shapeworks_lab/Environments.h"
#include "shapeworks_lab/Reflections.h"

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace world = sigil::world;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace material = sigil::material;

namespace {

constexpr SkSize kCanvas = {1320, 720};
/** The bevelled panel's shoulder, px. */
constexpr float kBevelPx = 118.0f;
/** Where the three bodies stand across the canvas. */
constexpr float kStations[3] = {-320, 0, 380};

/** How many picture pixels a bake takes per page pixel, so the bodies
 *  stay sharp on a plate captured at twice the page's size. */
constexpr int kBakeDensity = 2;

camera::Camera lens() { return {.eye = {0, 90, 820}, .target = {0, 0, 0}}; }

/** One body alone in @p surroundings, wearing @p finish, baked at the
 *  canvas's size so it lands where its placement puts it on the sheet. */
sk_sp<SkImage> bakeBody(sketch::Scene2DContext& ctx, mesh::Mesh body,
                        const glm::mat4& placement,
                        material::Material finish,
                        const material::EnvironmentMap& surroundings) {
  world::Frame frame(world::group(
      {world::camera(lens(), {.toneCurve = world::ToneCurve::Neutral}),
       world::mesh(std::move(body))
           .key("body")
           .transform(placement)
           .fill(std::move(finish))}));
  frame.environment(surroundings, {.intensity = 1.6f});
  return ctx.bakeScene3D(frame,
                         {(int)kCanvas.width() * kBakeDensity,
                          (int)kCanvas.height() * kBakeDensity},
                         {.camera = lens()});
}

/** The third panel's silhouette: a squircle, in CANVAS coordinates,
 *  because `bevelNormals` places its map so a shader's device xy reads
 *  the normal under it. */
SkPath squircle() {
  const float side = 300;
  return sigil::geometry::path::toSk(
             shapes::squircle(3.4f).outline({side, side}))
      .makeTransform(SkMatrix::Translate(
          kCanvas.width() * 0.5f + kStations[2] - side * 0.5f,
          kCanvas.height() * 0.5f - side * 0.5f));
}

}  // namespace

struct MeshNormalBridge {
  /** The meshed bodies are frames baked on the device, so the sheet needs
   *  one and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  material::EnvironmentMap sunset;
  sk_sp<SkImage> chromeBody, goldBody;

  void draw(SkCanvas& canvas) const {
    const SkSamplingOptions linear(SkFilterMode::kLinear);
    for (const sk_sp<SkImage>& body : {chromeBody, goldBody})
      if (body)
        canvas.drawImageRect(body, SkRect::MakeSize(kCanvas), linear);
    // THE OTHER SOURCE. No mesh and no frame: the normals are derived from
    // the path's own coverage and the path is its own stencil, so one
    // drawPath is the whole panel.
    const SkPath outline = squircle();
    SkPaint shade;
    shade.setAntiAlias(true);
    shade.setShader(material::skia::shader(
        shapeworks_lab::chrome(material::skia::bevelNormals(outline, kBevelPx),
                               sunset, {.contrast = 1.35f}),
        {}));
    canvas.drawPath(outline, shade);
  }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 1.0,
                        .background = sketch::kit::theme().palette.ground});
    const material::EnvironmentMap studio = shapeworks_lab::studioEnvironment();
    sunset = shapeworks_lab::sunsetEnvironment();
    // Chrome in the sunset, gold in the studio: metals at a low roughness,
    // so what they show is the panorama tinted by their own colour.
    chromeBody = bakeBody(
        ctx, mesh::superellipsoid({150, 138, 90}, 2.6f, 64, 48),
        camera::place({kStations[0], 0, 0}, 24, -10, -8),
        material::surface::program({.baseColor = {0.95f, 0.95f, 0.97f, 1},
                                    .metallic = 1,
                                    .roughness = 0.08f}),
        sunset);
    goldBody = bakeBody(
        ctx, mesh::torus(116, 40),
        camera::place({kStations[1], 0, -40}, 0, -30, 18),
        material::surface::program({.baseColor = {1.0f, 0.78f, 0.34f, 1},
                                    .metallic = 1,
                                    .roughness = 0.22f}),
        studio);
    const auto caption = [&](const char* call, const char* note, float x) {
      return box()
          .column()
          .gap(4)
          .width(370)
          .absolute()
          .inset(kCanvas.height() - 92, 0, 0, x)
          .children(
              {document::label(call), document::caption(note).width(370)});
    };
    ctx.composer.render(
        // Every line is set in the bright ink unless it says otherwise;
        // the quiet notes name the dim one.
        stack()
            .applyStyleSheet(sketch::kit::theme().styleSheet())
            .ink(sketch::kit::theme().palette.ink)
            // Keyed on the sink's own name: everything `draw` reads is
            // cooked above, in this setup, and nothing after it moves.
            .children(
                {custom("mesh.normal.bridge",
                        [this](sigil::draw::Pen& pen) {
                          SkCanvas& canvas = *pen.canvas();
                          draw(canvas);
                        })
                     .inset(0),
                 sketch::kit::page(
                     {.title = "One reflection, two kinds of shape",
                      .subtitle = "Compare a body the world shades with the "
                                  "bevel of a flat silhouette.",
                      .footer =
                          "A body faces where its mesh says; an outline faces "
                          "where its bevel says. Both mirror an environment.",
                      .ground = Fill::none()},
                     box()),
                 caption("A mesh in chrome",
                         "A superellipsoid in a world frame, mirroring a "
                         "sunset environment.",
                         30),
                 caption("A mesh in gold",
                         "A torus in a world frame, a gold metal mirroring a "
                         "studio environment.",
                         462),
                 caption("A flat outline in chrome",
                         "A bevel supplies the normals, using the same chrome "
                         "recipe and sunset as the first body.",
                         894)}));
  }
};

SIGIL_SKETCH(MeshNormalBridge, "Kit · API",
             "one reflection, two kinds of shape — chrome and gold bodies "
             "the world shades in their environments, and a flat path's "
             "shoulder through bevelNormals under the chrome recipe")
