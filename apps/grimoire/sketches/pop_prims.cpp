// pop_prims.cpp — THE PRIMITIVE CLASS: attributes that live on TRIANGLES.
// =============================================================================
// A mesh carries lanes on its points and lanes on its PRIMITIVES, and
// the second class is not a convenience for the first. The two triangles
// of a quad can hold different values; no point or vertex attribute can
// say that, because the two triangles share every point they have.
// Three stanzas, left to right, and the middle one is the reason the
// class needs a bake at all.
//
//   1. FACETS  a primitive "Color" lane written straight onto a formed body
//      and read by the world body that names it, `.primitiveColor("Color")`.
//      The two triangles of every quad alternate brightness — that
//      alternation IS the demonstration.
//   2. BAKED   the SAME lane through `mesh::bakePrimitiveColor`, which
//      unwelds each triangle into three of its own vertices and writes
//      the value per vertex. That is how the class reaches a consumer
//      that reads vertex colours and nothing else. Both bodies wear one
//      matte finish under one light, so the two pictures are comparable
//      and any difference is the bake's, not the lighting's.
//   3. PIECES  the point class promoted INTO the primitive class.
//      `.promote("Id")` stamps every triangle with its owning point's
//      index, so a stamp instance is a RUN OF TRIANGLES sharing an Id
//      value rather than a second container the renderer must know
//      about. Colouring by that Id is what makes the runs visible.
//
// The colours come off a perceptual ramp rather than a hue formula:
// interpolating in OKLab is what keeps a run of tints from collapsing
// into mud in its middle, and the alternation in stanzas 1 and 2 is a
// lightness step on the same ramp.
//
// EDIT THESE FIRST
//   the torus segment counts — how many facets the alternation runs over.
//   kPieces                  — instances in stanza 3; the Id stride
//                              below scatters neighbouring runs apart.

// TAGS: Geometry/Points

#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace pop = sigil::geometry::mesh::pop;
namespace material = sigil::material;
namespace world = sigil::world;

namespace {

constexpr SkSize kCanvas = {1240, 780};
constexpr int kPieces = 64;
/** How many picture pixels a bake takes per page pixel, so a figure stays
 *  sharp on a plate captured at twice the page's size. */
constexpr int kBakeDensity = 2;
constexpr float kFigureWidth = 372;
constexpr float kFigureHeight = 360;

/** A four-stop ramp walked in OKLab, at a lightness multiplier. @p f
 *  wraps, so the ramp closes on itself the way a loop of facets needs
 *  it to.
 *
 *  Named for the space it walks: `ramp` alone is compose's transition
 *  spelled in milliseconds, and both take two floats. */
glm::vec4 oklabRamp(float f, float value) {
  static const material::Color stops[5] = {{0.98f, 0.42f, 0.30f, 1},
                                           {0.98f, 0.85f, 0.35f, 1},
                                           {0.35f, 0.88f, 0.62f, 1},
                                           {0.40f, 0.55f, 0.98f, 1},
                                           {0.98f, 0.42f, 0.30f, 1}};
  f -= std::floor(f);
  const float scaled = f * 4.0f;
  const int i = std::min((int)scaled, 3);
  const material::Color c =
      material::lerpOklab(stops[i], stops[i + 1], scaled - (float)i);
  return {c.r * value, c.g * value, c.b * value, 1.0f};
}

}  // namespace

struct PopPrims {
  /** Every figure is a frame baked on the device, so the sheet needs one
   *  and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  mesh::Mesh facets, baked, pieces;

  /** One figure: @p body under one light, matte and white so what shows
   *  is its colour, baked at the pixels its well will have. The stamped
   *  pieces are their own light, so their runs read flat. */
  static Element figure(sketch::Scene2DContext& ctx, world::Mesh body,
                        bool stamps = false) {
    const camera::Camera view{
        .eye = {0, 210, 900}, .target = {0, 0, 0}, .fovYDeg = 42};
    body.key("subject").transform(camera::place({}, 0, stamps ? 0 : -28));
    if (stamps)
      body.fill(material::surface::unlit({.baseColor = {0.9f, 0.9f, 0.93f, 1}}));
    else
      body.fill(material::surface::program(
          {.baseColor = {1, 1, 1, 1}, .roughness = 1}));
    const world::Frame frame(world::group(
        {world::camera(view, {.toneCurve = world::ToneCurve::Neutral}),
         world::directionalLight({-0.5f, -0.8f, -0.4f}, {.intensity = 2.6f}),
         world::ambientLight({.intensity = 0.9f}), std::move(body)}));
    return sketch::kit::well(
        {.width = kFigureWidth, .height = kFigureHeight},
        image(ctx.bakeScene3D(frame,
                              {(int)kFigureWidth * kBakeDensity,
                               (int)kFigureHeight * kBakeDensity},
                              {.camera = view}),
              material::Fit::Stretch));
  }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(
        ctx, {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
              .captureAt = 1.0,
              .background = material::Color{0.051f, 0.051f, 0.075f, 1}});

    // 1 — a primitive lane written straight onto a formed body.
    facets = mesh::torus(130, 46, 34, 14);
    {
      std::vector<glm::vec4>& color = facets.primitive("Color");
      const size_t quads = color.size() / 2;
      for (size_t t = 0; t < color.size(); ++t) {
        // The two triangles of a quad share its ramp position and differ
        // only in lightness — which is the alternation this stanza is for.
        const size_t quad = t / 2;
        color[t] =
            oklabRamp((float)quad / (float)quads, t % 2 == 0 ? 1.0f : 0.55f);
      }
    }

    // 2 — the same lane unwelded into per-vertex colours.
    baked = mesh::bakePrimitiveColor(facets, "Color");

    // 3 — the point class promoted into the primitive class.
    std::vector<glm::vec3> loop;
    for (int i = 0; i < 12; ++i) {
      const float a = (float)i / 12.0f * 6.2831853f;
      loop.emplace_back(160.0f * std::cos(a), 40.0f * std::sin(a * 3.0f),
                        160.0f * std::sin(a));
    }
    pieces = pop::on(loop)
                 .count(kPieces)
                 .spread(30)
                 .vary(0.45f)
                 .lookAt({0, 210, 900})
                 .promote("Id")
                 .stamps(mesh::quad(46, 46));
    if (const std::vector<glm::vec4>* ids = pieces.primitiveIf("Id")) {
      std::vector<glm::vec4>& tint = pieces.primitive("Color");
      for (size_t t = 0; t < tint.size(); ++t) {
        const int id = (int)(*ids)[t].x;
        tint[t] = oklabRamp((float)(id * 19 % kPieces) / (float)kPieces,
                            t % 2 == 0 ? 1.0f : 0.62f);
      }
    }

    ctx.composer.render(sketch::kit::page(
        {.title = "Colour belongs to a triangle",
         .subtitle = "A primitive lane, its vertex-only equivalent, and the "
                     "identity carried by each stamped piece",
         .footer = "The first two views share the mesh, camera and matte "
                   "lighting. Baking duplicates vertices so neighbouring "
                   "triangles can retain different colours."},
        box().column().gap(22).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "NATIVE PRIMITIVE LANE",
                             .control = "primitiveColor(\"Color\")",
                             .figure = figure(ctx, world::mesh(facets)
                                                       .primitiveColor("Color")),
                             .note = "Two triangles share points but carry "
                                     "different brightness values."},
                            {.title = "BAKED INTO VERTICES",
                             .control = "bakePrimitiveColor(Color)",
                             .figure = figure(ctx, world::mesh(baked)),
                             .note = "The same appearance, expressed through "
                                     "per-vertex colour."},
                            {.title = "IDENTITY PER PIECE",
                             .control = "promote(Id) → stamps(quad)",
                             .figure = figure(
                                 ctx,
                                 world::mesh(pieces).primitiveColor("Color"),
                                 /*stamps=*/true),
                             .note = "Each stamped point gives all of its "
                                     "triangles the same Id."}},
                  .measure = 1160,
                  .gap = 22}),
             sketch::kit::comparison(
                 {.cases =
                      {{.figure = sigil::compose::kit::readout(
                                      std::vector<sigil::compose::kit::Reading>{
                                          {.name = "Vertices",
                                           .value = kit::formatted(
                                               "%zu", facets.vertexCount())},
                                          {.name = "Triangles",
                                           .value = kit::formatted(
                                               "%zu", facets.triangleCount())}},
                                      {.nameMeasure = 88,
                                       .gap =
                                           sketch::kit::theme().spacing.rowGap,
                                       .labelGap = sketch::kit::theme()
                                                       .spacing.labelGap})
                                      .width(372)},
                       {.figure = sigil::compose::kit::readout(
                                      std::vector<sigil::compose::kit::Reading>{
                                          {.name = "Vertices",
                                           .value = kit::formatted(
                                               "%zu", baked.vertexCount())},
                                          {.name = "Triangles",
                                           .value = kit::formatted(
                                               "%zu", baked.triangleCount())}},
                                      {.nameMeasure = 88,
                                       .gap =
                                           sketch::kit::theme().spacing.rowGap,
                                       .labelGap = sketch::kit::theme()
                                                       .spacing.labelGap})
                                      .width(372)},
                       {.figure = sigil::compose::kit::readout(
                                      std::vector<sigil::compose::kit::Reading>{
                                          {.name = "Pieces",
                                           .value =
                                               kit::formatted("%d", kPieces)},
                                          {.name = "Triangles",
                                           .value = kit::formatted(
                                               "%zu", pieces.triangleCount())}},
                                      {.nameMeasure = 88,
                                       .gap =
                                           sketch::kit::theme().spacing.rowGap,
                                       .labelGap = sketch::kit::theme()
                                                       .spacing.labelGap})
                                      .width(372)}},
                  .measure = 1160,
                  .gap = 22})})));
  }
};

SIGIL_SKETCH(PopPrims, "Kit · API",
             "attributes on triangles — a primitive colour lane read natively, "
             "the same lane baked into vertices, and a point lane "
             "promoted so stamp runs are addressable")
