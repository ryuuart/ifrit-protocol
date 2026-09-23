/** @file
 * A generated torus serialized as ASCII and binary PLY, then decoded.
 * The first comparison draws the source and both decoded meshes with one
 * camera and reports measured topology and byte counts. The lower comparison
 * covers point attributes, fitting the decoded bounds and the actual PLY
 * header. Every fixture is generated locally.
 */

// TAGS: Geometry/Meshes, Media/Models

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/codec/Encode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <cstdio>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace gm = sigil::geometry::mesh;
namespace codec = sigil::geometry::mesh::codec;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1120, 910};
constexpr float kCell = 330.66f;
constexpr float kPicture = 208;

constexpr float kR = 62, kr = 23;  // the torus, major and minor radius
constexpr int kNu = 48, kNv = 24;  // how finely it is tessellated
constexpr int kMotes = 2600;       // points for the cloud round trip

constexpr material::Color kFigure{0.98f, 0.78f, 0.36f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.07f, 0.075f, 0.085f, 1};
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.spacing.captionGap = 8;
  return look;
}

weave::TextStyle mono(float size, material::Color color) {
  const sk_sp<SkTypeface> face =
      weave::ports::face({"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

camera::Camera stageCamera() {
  camera::Camera view;
  view.eye = {0, 96, 250};
  view.target = {0, 0, 0};
  view.fovYDeg = 38;
  return view;
}

render::MeshStyle stageStyle() {
  render::MeshStyle style;
  style.baseColor = {0.76f, 0.72f, 0.66f, 1};
  style.lights = {{{-0.5f, -0.7f, -0.5f}, {1.0f, 0.95f, 0.88f, 1}, 1.15f},
                  {{0.7f, -0.15f, -0.2f}, {0.42f, 0.56f, 0.9f, 1}, 0.5f}};
  style.specular = 0.7f;
  style.shininess = 56;
  return style;
}

std::string count(size_t n, const char* unit) {
  const std::string buffer = kit::formatted("%zu %s", n, unit);
  return buffer;
}

std::string kib(size_t bytes) {
  const std::string buffer = kit::formatted("%.1f KiB", (double)bytes / 1024.0);
  return buffer;
}

Element meshFigure(const char* key, gm::Mesh mesh) {
  return sketch::kit::well(
      {.width = kCell, .height = kPicture},
      custom(key, [mesh = std::move(mesh)](SkCanvas& canvas,
                                           const PaintContext& pc) {
        render::drawMesh(canvas, mesh, camera::place({0, 0, 0}, 24, 0),
                         stageCamera(), pc.size, stageStyle());
      }));
}

}  // namespace

struct CodecRoundtrip {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE SOURCE. Everything below is this mesh, or this mesh's points,
    // after a trip through a file.
    const gm::Mesh source = gm::torus(kR, kr, kNu, kNv);

    const std::string ascii = codec::encode::ply(source);
    const std::string binary = codec::encode::ply(source, {.binary = true});
    const std::optional<codec::decode::Model> fromAscii =
        codec::decode::model(ascii.data(), ascii.size(), "torus.ply");
    const std::optional<codec::decode::Model> fromBinary =
        codec::decode::model(binary.data(), binary.size(), "torus.ply");

    // THE CLOUD. A lane written per point, encoded faceless and read
    // back — the whole reason PLY is the carrier.
    gm::Cloud motes = gm::points::onMesh(source, kMotes, 5);
    std::vector<glm::vec4>& tint = motes.color("tint");
    std::vector<float>& size = motes.scalar("size", 1.0f);
    for (size_t i = 0; i < motes.size(); ++i) {
      const float up = 0.5f + 0.5f * motes.positions[i].y / kr;
      tint[i] = {0.42f + 0.55f * up, 0.62f - 0.10f * up, 0.95f - 0.5f * up, 1};
      size[i] = 0.7f + 0.6f * up;
    }
    const std::string cloudPly = codec::encode::ply(motes);
    const std::optional<codec::decode::Model> cloudBack =
        codec::decode::model(cloudPly.data(), cloudPly.size(), "motes.ply");
    const gm::Cloud read = cloudBack ? cloudBack->mergedCloud() : gm::Cloud{};

    // THE FIT. Bounds over every part, then the one transform that
    // centres a model and scales its longest extent to a stated size.
    glm::vec3 lo{0}, hi{0};
    if (fromAscii) fromAscii->bounds(&lo, &hi);
    gm::Mesh fitted = fromAscii ? fromAscii->merged() : gm::Mesh{};
    if (fromAscii) fitted.transform(fromAscii->fitTransform(120.0f));

    const std::string boundsLine = kit::formatted(
        "bounds (%.0f %.0f %.0f)-(%.0f %.0f %.0f) · "
        "fitTransform(120) → longest extent 120",
        (double)lo.x, (double)lo.y, (double)lo.z, (double)hi.x, (double)hi.y,
        (double)hi.z);

    const auto returned = [&](const auto& model, size_t bytes) {
      return kit::formatted(
          "%zu vertices · %zu triangles · %s", model ? model->vertexCount() : 0,
          model ? model->triangleCount() : 0, kib(bytes).c_str());
    };
    Element meshes = sketch::kit::comparison(
        {.cases = {{.title = "GENERATED TORUS",
                    .control = "R 62 · r 23 · 48 × 24",
                    .figure = meshFigure("source", source),
                    .note = count(source.positions.size(), "vertices") + " · " +
                            count(source.indices.size() / 3, "triangles")},
                   {.title = "ASCII ROUND TRIP",
                    .control = "encode::ply(mesh)",
                    .figure = meshFigure(
                        "ascii", fromAscii ? fromAscii->merged() : gm::Mesh{}),
                    .note = returned(fromAscii, ascii.size())},
                   {.title = "BINARY ROUND TRIP",
                    .control = "binary = true",
                    .figure =
                        meshFigure("binary", fromBinary ? fromBinary->merged()
                                                        : gm::Mesh{}),
                    .note = returned(fromBinary, binary.size())}},
         .measure = 1040,
         .gap = 24});
    const auto end = cloudPly.find("end_header");
    const std::string header =
        cloudPly.substr(0, end == std::string::npos ? 0 : end + 10);
    Element cloud = sketch::kit::well(
        {.width = kCell, .height = kPicture},
        custom("cloud", [read](SkCanvas& canvas, const PaintContext& pc) {
          gm::points::BillboardStyle splat;
          splat.size = 3.4f;
          splat.sizeLane = "size";
          splat.tintLane = "tint";
          splat.additive = false;
          gm::points::drawBillboards(canvas, read, stageCamera(), pc.size,
                                     splat);
        }));
    Element structure = sketch::kit::comparison(
        {.cases =
             {{.title = "POINT ATTRIBUTES",
               .control = "Faceless PLY → mergedCloud()",
               .figure = std::move(cloud),
               .note = kit::formatted(
                   "%zu points · %zu colour / %zu scalar / %zu vector lanes",
                   read.size(), read.colors.size(), read.scalars.size(),
                   read.vectors.size())},
              {.title = "FIT THE DECODED MODEL",
               .control = "fitTransform(120)",
               .figure = meshFigure("fitted", fitted),
               .note = boundsLine},
              {.title = "THE ACTUAL FILE HEADER",
               .control = "Position + named properties",
               .figure =
                   sketch::kit::well(
                       {.width = kCell, .height = 254, .padding = 16})
                       .children(
                           {text(header, mono(11, kFigure)).width(kCell - 32)}),
               .note = "Attribute names are encoded as PLY properties and "
                       "reconstructed into lanes."}},
         .measure = 1040,
         .gap = 24});
    ctx.composer.render(sketch::kit::page(
        {.title = "Geometry through a file",
         .subtitle = "Match the torus before and after serialization, then "
                     "inspect the attributes and bounds the decoder recovered.",
         .footer = "decode::model chooses its reader from the path hint. ASCII "
                   "stores decimal text; binary stores little-endian values."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  MESH TOPOLOGY AND APPEARANCE"}),
             std::move(meshes),
             sketch::kit::sectionHeader(
                 {.label = "02  WHAT ELSE SURVIVES THE TRIP"}),
             std::move(structure)})));
  }
};

SIGIL_SKETCH(CodecRoundtrip, "Kit · API",
             "a generated torus written through encode::ply as ascii, as "
             "binary and as a faceless cloud, each read back through "
             "decode::model and drawn beside its byte count")
