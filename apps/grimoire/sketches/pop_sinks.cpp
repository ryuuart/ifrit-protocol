/** @file
 * pop_sinks — every sink a point chain ends in.
 *
 * A pop chain is a value: a list of edits over points, cooked only when
 * something asks for the result. What it is cooked INTO is the sink at
 * the end, and the sinks are where a chain stops being points.
 *
 *   01  THE SPLAT, the one sink that forms no geometry: a billboard faces
 *       the EYE, so it is answered where the eye is. Four styles over one
 *       torus — a soft additive dot, a hollow ring, constant pixels with
 *       perspective off, and a window of a sprite sheet per point read
 *       from the "Tex" lane `.atlas()` writes.
 *   02  THE SWEEPS. `.sweep()` makes a tube whose rail IS the cooked
 *       points, so noise before the sweep is a wobbly pipe; any outline
 *       sweeps, and a star section on a smoothed ring is a crown whose
 *       own surface seeds a second chain of glints (`pop::on(mesh)`: the
 *       sink's output is a legal source); `pop::cookSweep` on a WINDOW
 *       of the ring is an open ribbon, drawn on both of its sides.
 *   03  THE STAMPS, one small mesh at every point. `.atlas(2, 2)` picks a
 *       cell of the shared texture per point, `.vary()` sizes them apart
 *       and `.lookAt()` turns every one to the camera; `.promote()`
 *       carries a point's lane onto the triangles its stamp forms, and
 *       "Id" stamps every triangle with its owning point's index, so an
 *       instance is a RUN of triangles rather than a second container.
 *
 * Every formed figure is one world frame under one light, baked into its
 * well through `ctx.bakeScene3D`; every splat is cooked once and held as
 * a texture. The sprite sheet and the ring stamp are compose trees
 * painted once through the texture door.
 *
 * EDIT THESE FIRST
 *   kMotes                — points in the splatted torus.
 *   the .count() values   — how densely each forming chain resolves.
 *   .noise(amplitude, frequency) — the wobble, on the rail not the skin.
 *   .smooth(strength, iterations) — drop it and the star sweep kinks.
 *   kPieces               — stamps in the identity figure.
 */

// TAGS: Geometry/Points

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/Picture.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <array>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace sections = sigil::geometry::sections;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;
namespace pop = sigil::geometry::mesh::pop;
namespace world = sigil::world;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1240, 1250};
constexpr float kMeasure = 1160;
constexpr int kMotes = 5200;
constexpr int kPieces = 64;

constexpr material::Color kWhite{1, 1, 1, 1};

/** The cool steel the formed bodies are made of, in @p colour. */
material::Material steel(material::Color colour) {
  return material::surface::program({.baseColor = colour, .roughness = 0.35f});
}

/** A 2 x 2 sprite sheet, described rather than painted: four motifs in
 *  four cells, which is exactly what `.atlas(2, 2)` indexes. Each motif
 *  stands inside its cell with a gutter, because a cell is sampled half a
 *  texel inside its window. */
Element atlasSheet(float cell) {
  const std::array<std::pair<Shape, material::Color>, 4> motifs{
      {{shapes::circle(), {0.4f, 0.85f, 1.0f, 1}},
       {shapes::annulus(0.62f), {1.0f, 0.6f, 0.3f, 1}},
       {shapes::polygon(4), {0.6f, 1.0f, 0.6f, 1}},
       {shapes::star(4, 0.35f), {1.0f, 0.8f, 0.3f, 1}}}};
  return stack().width(cell * 2).height(cell * 2).children(
      each(motifs, [cell](const std::pair<Shape, material::Color>& motif,
                          std::size_t index) {
        return kit::at((float)(index % 2) * cell, (float)(index / 2) * cell,
                       cell, cell)
            .padding(cell * 0.12f)
            .children({box()
                           .flexGrow()
                           .shape(motif.first)
                           .fill(Fill::color(motif.second))});
      }));
}

/** Ten control points on a ring with a twofold vertical wave: the rail
 *  every formed figure starts from. */
std::vector<glm::vec3> ringPoints() {
  std::vector<glm::vec3> ring;
  for (int index = 0; index < 10; ++index) {
    const float angle = (float)index / 10.0f * 6.2831853f;
    ring.emplace_back(std::cos(angle) * 230, std::sin(angle * 2.0f) * 60,
                      std::sin(angle) * 230);
  }
  return ring;
}

const glm::vec3 kEye{0, 260, 980};
const camera::Camera kFormedView{
    .eye = kEye, .target = {0, 20, 0}, .verticalFieldOfViewDegrees = 24};
const camera::Camera kSplatView{
    .eye = {0, 74, 235}, .target = {0, 0, 0}, .verticalFieldOfViewDegrees = 40};

/** The splatted subject: points scattered on a torus, sized and tinted by
 *  their own lanes so the sink has lanes to read. */
pop::Builder motes() {
  return pop::on(mesh::torus(74, 26, 64, 32), kMotes)
      .vary(0.7f, 1.0f)
      .rampBy({{0.30f, 0.62f, 0.98f, 1}, {1.00f, 0.62f, 0.28f, 1}});
}

/** ONE SPLAT FIGURE: @p cloud, cooked once, in @p style, painted with the
 *  well's own ground and held as a texture — an additive splat keeps the
 *  backdrop it was blended over. */
Element splatFigure(const char* key, mesh::Cloud cloud,
                    points::BillboardStyle style) {
  return sketch::kit::well(
             {.width = 274, .height = 200},
             custom(key,
                    [cloud = std::move(cloud), style = std::move(style)](
                        sigil::draw::Pen& pen, const PaintContext& paint) {
                      points::drawBillboards(*pen.canvas(), cloud, kSplatView,
                                             paint.size, style);
                    }))
      .cache(Cache::Texture);
}

}  // namespace

struct PopSinks {
  /** The formed figures are frames baked on the device, so the sheet needs
   *  one and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  /** One formed figure: @p bodies under one light, baked at the size its
   *  well will have. */
  static Element figure(sketch::Scene2DContext& ctx, SkISize size,
                        std::vector<world::Node> bodies) {
    bodies.push_back(
        world::camera(kFormedView, {.toneCurve = world::ToneCurve::Neutral}));
    bodies.push_back(
        world::directionalLight({-0.5f, -0.8f, -0.4f}, {.intensity = 2.9f}));
    bodies.push_back(world::ambientLight({.intensity = 0.35f}));
    return sketch::kit::well(
        {.width = (float)size.width(), .height = (float)size.height()},
        image(ctx.bakeScene3D(world::Frame(world::group(std::move(bodies))),
                              size, {.camera = kFormedView}),
              material::Fit::Stretch));
  }

  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 1.0,
         .background = material::Color{0.051f, 0.051f, 0.075f, 1}});

    const material::Texture atlas = ctx.texture(atlasSheet(128), {256, 256});
    const std::vector<glm::vec3> ring = ringPoints();

    // 01 — THE SPLAT.
    const mesh::Cloud torus = motes().cloud();
    const media::Picture ringStamp =
        ctx.texture(box().width(64).height(64).padding(7).children(
                        {box()
                             .flexGrow()
                             .shape(shapes::annulus(0.72f))
                             .fill(Fill::color(kWhite))}),
                    {64, 64})
            .frameAt()
            .image;
    // Every splat but the sheet reads the lanes the chain wrote.
    const auto styled = [](points::BillboardStyle style) {
      style.sizeLane = "size";
      style.tintLane = "tint";
      return style;
    };

    // 02 — THE SWEEPS.
    const mesh::Mesh tube =
        pop::on(ring)
            .count(220)
            .noise(26, 0.004f)
            .sweep(sections::circle(14), true, {.segments = 160, .scale = 11});
    const mesh::Mesh crown =
        pop::on(ring)
            .count(140)
            .noise(20, 0.004f)
            .smooth(0.5f, 2)
            .sweep(pop::profile::fromPath(
                       shapes::star(5, 14.0f / 30.0f).at({0, 0}, 30)),
                   true,
                   {.segments = 160,
                    .normals = pop::SweepOptions::Normals::Geometric});
    const mesh::Mesh glints =
        pop::on(crown, 600).jitter(1.5f).stamps(mesh::quad(3, 3));
    const mesh::Mesh ribbon =
        pop::cookSweep(pop::on(ring)
                           .count(120)
                           .window(0.5f, 0.5f)
                           .noise(16, 0.004f)
                           .smooth(0.6f, 3),
                       sections::line(), false,
                       {.segments = 120,
                        .scale = 42,
                        .normals = pop::SweepOptions::Normals::Frame});

    // 03 — THE STAMPS.
    const mesh::Mesh plates =
        pop::on(ring)
            .count(900)
            .spread(26)
            .vary(0.6f)
            .fade({1.0f, 0.7f, 0.55f, 1}, {0.6f, 0.85f, 1.0f, 1})
            .atlas(2, 2)
            .lookAt(kEye)
            .stamps(mesh::quad(11, 11));
    // Each point's colour and its index carried onto the triangles its
    // stamp forms: one run of triangles per piece.
    const mesh::Mesh pieces = pop::on(ring)
                                  .count(kPieces)
                                  .spread(30)
                                  .vary(0.45f)
                                  .lookAt(kEye)
                                  .rampBy({{0.98f, 0.42f, 0.30f, 1},
                                           {0.98f, 0.85f, 0.35f, 1},
                                           {0.35f, 0.88f, 0.62f, 1},
                                           {0.40f, 0.55f, 0.98f, 1}})
                                  .promote(pop::Attribute::Color)
                                  .promote("Id")
                                  .stamps(mesh::quad(34, 34));
    std::set<int> runs;
    if (const std::vector<glm::vec4>* ids = pieces.primitiveIf("Id"))
      for (const glm::vec4& id : *ids) runs.insert((int)id.x);

    // The stamps wear the atlas as a surface that is its own light, each
    // multiplying the cell it shows by the colour the chain faded along
    // it; a stamp writes no depth, so a motif's clear corners show what
    // stands behind them.
    material::Material motifs =
        material::surface::unlit({.baseColor = {1, 1, 1, 1}, .depthWrite = 0});
    if (atlas.valid()) motifs.slot(material::surface::kBaseColorSlot, atlas);
    const glm::mat4 crowned = camera::place({}, 14, -10, 0, 0.85f);
    constexpr SkISize kSweepFigure{372, 200};
    constexpr SkISize kStampFigure{568, 238};

    ctx.composer.render(sketch::kit::page(
        {.title = "Where a chain stops being points",
         .subtitle = "One torus splatted four ways, then one ring of ten "
                     "control points swept and stamped into bodies",
         .footer = "A splat projects one sprite per point and forms nothing. "
                   "A sweep threads a frame through the cooked points; a "
                   "stamp places a mesh at each."},
        box().column().gap(24).children(
            {document::h2("01 / THE SPLAT · 5,200 POINTS, NO GEOMETRY"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SOFT LIGHT",
                        .control = "soft dot · additive · size 2",
                        .figure =
                            splatFigure("soft", torus, styled({.size = 2.0f})),
                        .note = "Overlapping light builds a glow. Point lanes "
                                "supply size and tint."},
                       {.title = "HOLLOW RINGS",
                        .control = "ring stamp · opaque · size 7",
                        .figure = splatFigure("rings", torus,
                                              styled({.sprite = ringStamp,
                                                      .size = 7,
                                                      .additive = false})),
                        .note = "A crisp rim shows the projected size of each "
                                "mark."},
                       {.title = "CONSTANT PIXELS",
                        .control = "perspective = false · size 4",
                        .figure = splatFigure("pixels", torus,
                                              styled({.size = 4,
                                                      .additive = false,
                                                      .perspective = false})),
                        .note = "Near and far share a pixel size; depth order "
                                "still separates them."},
                       {.title = "A SHEET OF SPRITES",
                        .control = "atlas(2, 2) → textureLane \"Tex\"",
                        .figure =
                            splatFigure("sheet", motes().atlas(2, 2).cloud(),
                                        {.sprite = atlas.frameAt().image,
                                         .size = 5,
                                         .sizeLane = "size",
                                         .textureLane = "Tex",
                                         .additive = false}),
                        .note = "Each point draws the cell its lane names, "
                                "untinted."}},
                  .measure = kMeasure,
                  .gap = 18}),
             document::h2("02 / THE SWEEPS · A RAIL OF COOKED POINTS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SWEEP A CIRCLE",
                        .control = "220 points → noise → circle",
                        .figure =
                            figure(ctx, kSweepFigure,
                                   {world::mesh(tube)
                                        .key("tube")
                                        .transform(camera::place({}, 24, -10))
                                        .fill(steel({0.62f, 0.7f, 0.82f, 1}))}),
                        .note = kit::formatted(
                            "A displaced rail makes a wobbly tube. %zu "
                            "triangles.",
                            tube.triangleCount())},
                       {.title = "SWEEP AN OUTLINE",
                        .control = "noise → smooth → star section",
                        .figure = figure(
                            ctx,
                            kSweepFigure,
                            {world::mesh(crown)
                                 .key("crown")
                                 .transform(crowned)
                                 .fill(steel({0.95f, 0.72f, 0.3f, 1})),
                             world::mesh(glints)
                                 .key("glints")
                                 .transform(crowned)
                                 .fill(material::surface::unlit(
                                     {.baseColor = {1.0f, 0.95f, 0.8f, 1}}))}),
                        .note = "The crown's own surface seeds 600 glints: a "
                                "formed body is a source again."},
                       {.title = "OPEN A WINDOW",
                        .control = "half-window → line section",
                        .figure =
                            figure(ctx, kSweepFigure,
                                   {world::mesh(ribbon)
                                        .key("ribbon")
                                        .transform(camera::place({}, 0, 14))
                                        .backface(material::Backface::Visible)
                                        .fill(steel({0.4f, 0.85f, 0.6f, 1}))}),
                        .note = "An open section on a partial rail is a "
                                "ribbon, drawn on both sides."}},
                  .measure = kMeasure,
                  .gap = 22}),
             document::h2("03 / THE STAMPS · ONE MESH AT EVERY POINT"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "STAMP AN ATLAS",
                        .control = "900 points → spread → atlas(2, 2)",
                        .figure =
                            figure(ctx, kStampFigure,
                                   {world::mesh(plates)
                                        .key("plates")
                                        .transform(camera::place({}, -16))
                                        .backface(material::Backface::Visible)
                                        .fill(motifs)}),
                        .note = "Four texture motifs, varied in size and "
                                "turned toward the camera."},
                       {.title = "CARRY A LANE ONTO THE PIECES",
                        .control = "promote(Color) · promote(\"Id\")",
                        .figure =
                            figure(ctx, kStampFigure,
                                   {world::mesh(pieces)
                                        .key("pieces")
                                        .transform(camera::place({}, -16))
                                        .backface(material::Backface::Visible)
                                        .primitiveColor("Color")
                                        .fill(material::surface::unlit(
                                            {.baseColor = {1, 1, 1, 1}}))}),
                        .note = kit::formatted(
                            "%zu triangles in %zu runs: every triangle of a "
                            "stamp carries its point's colour and Id.",
                            pieces.triangleCount(), runs.size())}},
                  .measure = kMeasure,
                  .gap = 24})})));
  }
};

SIGIL_SKETCH(PopSinks, "Kit · API",
             "every sink a point chain ends in: the splat in four styles, a "
             "swept tube, an outline and an open ribbon, atlas stamps, and "
             "stamp runs carrying their point's colour and Id")
