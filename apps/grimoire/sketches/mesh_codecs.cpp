/** @file
 * mesh_codecs — geometry through a file, and what comes back.
 *
 *   01  A MESH. A generated torus written through `encode::ply` as ASCII
 *       and as binary, each read back by `decode::model` and baked
 *       beside the source under one camera and one pair of lights, with
 *       the topology and byte count each file returned.
 *   02  A CLOUD. The torus's points, carrying a tint and a size lane,
 *       written as a faceless PLY: the lanes return as named properties,
 *       the decoded model's bounds give the one transform that fits it
 *       to a stated size, and the file's own header says how.
 *   03  A GROUP. A grid with a 0/1 lane "ring" written as Houdini's JSON
 *       `.geo` and read back: the reader turns a point group into a 0/1
 *       lane under its own name, so `pop::on(part.asCloud())` seeds a
 *       chain whose mask arrived in the file. Drop a real Houdini save
 *       with a `ring` group in place of the encoded text and the row is
 *       unchanged.
 *
 * Every fixture is generated here; nothing is read from disk.
 *
 * EDIT THESE FIRST
 *   kMajorRadius / kMinorRadius — the torus.
 *   kAround / kAcross           — how finely it is tessellated.
 *   kMotes                      — points in the cloud round trip.
 *   kRingRadius / kRingWidth    — which grid points the group holds.
 */

// TAGS: Geometry/Meshes, Media/Models

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/codec/Encode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/World.h>

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace mesh = sigil::geometry::mesh;
namespace codec = sigil::geometry::mesh::codec;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;
namespace pop = sigil::geometry::mesh::pop;
namespace world = sigil::world;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1120, 1290};
constexpr float kCell = 330.66f;
constexpr float kPicture = 208;
constexpr float kMeasure = 1040;

constexpr float kMajorRadius = 62, kMinorRadius = 23;
constexpr int kAround = 48, kAcross = 24;
constexpr int kMotes = 2600;

constexpr int kSide = 28;
constexpr float kSpacing = 14;
constexpr float kRingRadius = 120;
constexpr float kRingWidth = 34;

constexpr material::Color kFigure{0.98f, 0.78f, 0.36f, 1};

weave::TextStyle mono(float size, material::Color color) {
  const weave::Face face =
      weave::ports::face({"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

const camera::Camera kStageView{
    .eye = {0, 96, 250}, .target = {0, 0, 0}, .fovYDegrees = 38};
const camera::Camera kGridView{
    .eye = {0, 380, 440}, .target = {0, 0, 0}, .fovYDegrees = 40};

/** The stage every mesh figure stands on: its lens, a warm key and a cool
 *  fill over a dim light from everywhere, and @p subject in a soft stone
 *  finish turned to show its hole. */
world::Frame stage(mesh::Mesh subject) {
  return world::Frame(world::group(
      {world::camera(kStageView, {.toneCurve = world::ToneCurve::Neutral}),
       world::directionalLight({-0.5f, -0.7f, -0.5f},
                               {.color = material::Color{1.0f, 0.95f, 0.88f, 1},
                                .intensity = 2.9f}),
       world::directionalLight({0.7f, -0.15f, -0.2f},
                               {.color = material::Color{0.42f, 0.56f, 0.9f, 1},
                                .intensity = 1.3f}),
       world::ambientLight(
           {.color = material::Color{0.8f, 0.8f, 1.0f, 1}, .intensity = 0.3f}),
       world::mesh(std::move(subject))
           .key("subject")
           .transform(camera::place({0, 0, 0}, 24, 0))
           .fill(material::surface::program(
               {.baseColor = {0.76f, 0.72f, 0.66f, 1}, .roughness = 0.35f}))}));
}

/** One splat of a cooked @p cloud through @p view, painted once and held
 *  as a texture. */
Element splat(const char* key, mesh::Cloud cloud, camera::Camera view,
              float size) {
  return sketch::kit::well(
      {.width = kCell, .height = kPicture},
      custom(key, [cloud = std::move(cloud), view, size,
                   sprite = kit::dotSprite()](sigil::draw::Pen& pen,
                                              const PaintContext& paint) {
        points::drawBillboards(*pen.canvas(), cloud, view, paint.size,
                               {.sprite = sprite->frames().front().image,
                                .size = size,
                                .sizeLane = "size",
                                .tintLane = "tint",
                                .additive = false});
      }).cache(Cache::Texture));
}

/** The lanes @p cloud carries, one line each, by width. */
std::string lanes(const mesh::Cloud& cloud) {
  std::string listed =
      kit::formatted("positions        %zu points\n", cloud.size());
  for (const auto& [name, values] : cloud.vectors)
    listed += kit::formatted("vector  %-8s %zu\n", name.c_str(), values.size());
  for (const auto& [name, values] : cloud.colors)
    listed += kit::formatted("colour  %-8s %zu\n", name.c_str(), values.size());
  for (const auto& [name, values] : cloud.scalars)
    listed += kit::formatted("scalar  %-8s %zu\n", name.c_str(), values.size());
  return listed;
}

/** A file's text in a well, set in a monospaced face. */
Element listing(const std::string& text) {
  return sketch::kit::well({.width = kCell, .height = 254, .padding = 16})
      .children({sigil::compose::text(text, mono(11, kFigure))
                     .width(kCell - 32)});
}

/** The grid the `.geo` row writes: a lattice spanned z then x so its
 *  normal faces up, a tint across x, and the 0/1 lane "ring" — a point
 *  group's own spelling on this side of the file. */
mesh::Cloud grid() {
  const float half = (float)(kSide - 1) * 0.5f * kSpacing;
  mesh::Cloud cloud = points::grid({-half, 0, -half}, {0, 0, 2 * half},
                                   {2 * half, 0, 0}, kSide, kSide);
  std::vector<glm::vec4>& tint = cloud.color("tint");
  std::vector<float>& ring = cloud.scalar("ring");
  for (size_t index = 0; index < cloud.size(); ++index) {
    const glm::vec3 position = cloud.positions[index];
    const float across = (position.x + half) / (2 * half);
    tint[index] = {0.2f + 0.7f * across, 0.45f, 0.9f - 0.7f * across, 1};
    ring[index] = std::abs(std::hypot(position.x, position.z) - kRingRadius) <
                          kRingWidth * 0.5f
                      ? 1.0f
                      : 0.0f;
  }
  return cloud;
}

}  // namespace

struct MeshCodecs {
  /** Every mesh figure is a frame baked on the device, so the sheet needs
   *  one and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE SOURCE. Rows 01 and 02 are this mesh, or its points, after a
    // trip through a file.
    const mesh::Mesh source =
        mesh::torus(kMajorRadius, kMinorRadius, kAround, kAcross);

    /** One mesh figure: @p subject baked on its stage at the size the well
     *  will have, and painted as the well's picture. */
    const auto meshFigure = [&ctx](mesh::Mesh subject) {
      return sketch::kit::well(
          {.width = kCell, .height = kPicture},
          image(ctx.bakeScene3D(stage(std::move(subject)),
                                {(int)kCell, (int)kPicture},
                                {.camera = kStageView}),
                material::Fit::Stretch));
    };

    // 01 — A MESH.
    const std::string ascii = codec::encode::ply(source);
    const std::string binary = codec::encode::ply(source, {.binary = true});
    const std::optional<codec::decode::Model> fromAscii =
        codec::decode::model(ascii.data(), ascii.size(), "torus.ply");
    const std::optional<codec::decode::Model> fromBinary =
        codec::decode::model(binary.data(), binary.size(), "torus.ply");
    const auto returned = [](const std::optional<codec::decode::Model>& model,
                             size_t bytes) {
      return kit::formatted("%zu vertices · %zu triangles · %.1f KiB",
                            model ? model->vertexCount() : 0,
                            model ? model->triangleCount() : 0,
                            (double)bytes / 1024.0);
    };

    // 02 — A CLOUD: a lane written per point, encoded faceless and read
    // back, which is the whole reason PLY is the carrier.
    mesh::Cloud motes = points::onMesh(source, kMotes, 5);
    std::vector<glm::vec4>& tint = motes.color("tint");
    std::vector<float>& size = motes.scalar("size", 1.0f);
    for (size_t index = 0; index < motes.size(); ++index) {
      const float up = 0.5f + 0.5f * motes.positions[index].y / kMinorRadius;
      tint[index] = {0.42f + 0.55f * up, 0.62f - 0.10f * up, 0.95f - 0.5f * up,
                     1};
      size[index] = 0.7f + 0.6f * up;
    }
    const std::string cloudPly = codec::encode::ply(motes);
    const std::optional<codec::decode::Model> cloudBack =
        codec::decode::model(cloudPly.data(), cloudPly.size(), "motes.ply");
    const mesh::Cloud read = cloudBack ? cloudBack->mergedCloud() : mesh::Cloud{};
    // Bounds over every part, then the one transform that centres a model
    // and scales its longest extent to a stated size.
    glm::vec3 low{0}, high{0};
    if (fromAscii) fromAscii->bounds(&low, &high);
    mesh::Mesh fitted = fromAscii ? fromAscii->merged() : mesh::Mesh{};
    if (fromAscii) fitted.transform(fromAscii->fitTransform(120.0f));
    const auto headerEnd = cloudPly.find("end_header");
    const std::string header = cloudPly.substr(
        0, headerEnd == std::string::npos ? 0 : headerEnd + 10);

    // 03 — A GROUP. asCloud(): positions, "normal" from N, "tint" from Cd,
    // and every group under its own name.
    const std::string geo = codec::encode::geo(grid());
    const std::optional<codec::decode::Model> geoBack =
        codec::decode::model(geo.data(), geo.size(), "grid.geo");
    const mesh::Cloud seed = geoBack && !geoBack->parts.empty()
                                 ? geoBack->parts.front().asCloud()
                                 : mesh::Cloud{};
    size_t inRing = 0;
    if (const std::vector<float>* ring = seed.scalarIf("ring"))
      for (const float member : *ring) inRing += member > 0.5f;
    // The ring drawn larger: a Math on Scale, masked by the returned lane.
    const pop::Math ringLarger{"Scale", {2.2f, 2.2f, 2.2f, 2.2f}, {}, "ring"};

    ctx.composer.render(sketch::kit::page(
        {.title = "Geometry through a file",
         .subtitle = "A mesh, a cloud and a group written out and read back: "
                     "match each before and after the trip.",
         .footer = "decode::model chooses its reader from the path hint. "
                   "ASCII stores decimal text; binary little-endian values; "
                   ".geo Houdini's JSON, its groups as lanes."},
        box().column().gap(26).children(
            {document::h2("01 / A MESH · PLY, ASCII AND BINARY"),
             sketch::kit::comparison(
                 {.cases = {{.title = "GENERATED TORUS",
                             .control = "R 62 · r 23 · 48 × 24",
                             .figure = meshFigure(source),
                             .note = kit::formatted("%zu vertices · %zu "
                                                    "triangles",
                                                    source.vertexCount(),
                                                    source.triangleCount())},
                            {.title = "ASCII ROUND TRIP",
                             .control = "encode::ply(mesh)",
                             .figure = meshFigure(fromAscii
                                                      ? fromAscii->merged()
                                                      : mesh::Mesh{}),
                             .note = returned(fromAscii, ascii.size())},
                            {.title = "BINARY ROUND TRIP",
                             .control = "binary = true",
                             .figure = meshFigure(fromBinary
                                                      ? fromBinary->merged()
                                                      : mesh::Mesh{}),
                             .note = returned(fromBinary, binary.size())}},
                  .measure = kMeasure,
                  .gap = 24}),
             document::h2("02 / A CLOUD · LANES AS NAMED PROPERTIES"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "POINT ATTRIBUTES",
                        .control = "faceless PLY → mergedCloud()",
                        .figure = splat("cloud", read, kStageView, 3.4f),
                        .note = kit::formatted(
                            "%zu points · %zu colour / %zu scalar / %zu "
                            "vector lanes",
                            read.size(), read.colors.size(),
                            read.scalars.size(), read.vectors.size())},
                       {.title = "FIT THE DECODED MODEL",
                        .control = "fitTransform(120)",
                        .figure = meshFigure(fitted),
                        .note = kit::formatted(
                            "bounds (%.0f %.0f %.0f)–(%.0f %.0f %.0f) → "
                            "longest extent 120",
                            (double)low.x, (double)low.y, (double)low.z,
                            (double)high.x, (double)high.y, (double)high.z)},
                       {.title = "THE FILE'S HEADER",
                        .control = "position + named properties",
                        .figure = listing(header),
                        .note = "Lane names are written as PLY properties and "
                                "read back into lanes."}},
                  .measure = kMeasure,
                  .gap = 24}),
             document::h2(kit::formatted(
                 "03 / A GROUP · HOUDINI .geo · %zu POINTS, %zu IN \"ring\"",
                 seed.size(), inRing)),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "THE GROUP RETURNS AS A LANE",
                        .control = "pop::on(part.asCloud())",
                        .figure = splat(
                            "group",
                            pop::on(seed).operation(ringLarger).cloud(),
                            kGridView, 6),
                        .note = "Cd from the file; the ring group, scaled up "
                                "by a Math masked with it."},
                       {.title = "A MASK ON ARRIVAL",
                        .control = "twist(70).masked(\"ring\")",
                        .figure = splat("mask",
                                        pop::on(seed)
                                            .twist(70, {0, 1, 0}, -1, 1)
                                            .masked("ring")
                                            .peak(30)
                                            .masked("ring")
                                            .operation(ringLarger)
                                            .cloud(),
                                        kGridView, 6),
                        .note = "Only the group the file named turns and "
                                "lifts."},
                       {.title = "WHAT CAME BACK",
                        .control = "the lanes asCloud() names",
                        .figure = listing(lanes(seed)),
                        .note = kit::formatted(
                            "From %.1f KiB of JSON: the group is one more "
                            "scalar lane.",
                            (double)geo.size() / 1024.0)}},
                  .measure = kMeasure,
                  .gap = 24})})));
  }
};

SIGIL_SKETCH(MeshCodecs, "Kit · API",
             "geometry through a file: a torus written as ascii and binary "
             "PLY, a faceless cloud whose lanes return as properties, and a "
             "Houdini .geo whose point group comes back as a mask")
