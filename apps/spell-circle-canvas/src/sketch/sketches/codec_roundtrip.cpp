/** @file
 * codec_roundtrip — geometry out through encode::ply and back in through
 * decode::model.
 *
 * PLY is the carrier because it is the one widely read format where
 * arbitrary per-vertex attributes are first-class: a Mesh writes its
 * vertices and triangles, a Cloud writes its positions and EVERY lane,
 * and the reader folds the suffixed properties back into lanes under
 * their own names. So a round trip is lossless up to the uchar colour
 * quantisation, and the last cell shows the header that says so.
 *
 * Ascii and binary carry the same properties in the same order. Ascii is
 * readable and diffable; binary writes rows as raw little-endian bytes,
 * so floats survive exactly instead of through a decimal spelling, and
 * the file is a third of the size. Both are read by the same
 * `decode::model` call, which picks its reader off the path hint.
 *
 * `decode::model` is also the door for OBJ, glTF, STL, Alembic and
 * Houdini .geo. Nothing is mounted at `res://` in this repository, so a
 * plate here is a function of the geometry generated below and never of
 * what a machine happens to have on disk.
 *
 * EDIT THESE FIRST
 *   kR, kr, kNu, kNv — the torus and how finely it is tessellated, which
 *                      is what every byte count below is a count of.
 *   kMotes           — points scattered on it for the cloud round trip.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/codec/Encode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <cstdio>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace gm = sigil::geometry::mesh;
namespace codec = sigil::geometry::mesh::codec;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1120, 700};
constexpr float kCell = 348;
constexpr float kPicture = 214;

constexpr float kR = 62, kr = 23;  // the torus, major and minor radius
constexpr int kNu = 48, kNv = 24;  // how finely it is tessellated
constexpr int kMotes = 2600;       // points for the cloud round trip

constexpr SkColor4f kGround{0.07f, 0.075f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.56f, 0.57f, 0.63f, 1};
constexpr SkColor4f kFigure{0.98f, 0.78f, 0.36f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
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
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%zu %s", n, unit);
  return buffer;
}

std::string kib(size_t bytes) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.1f KiB", (double)bytes / 1024.0);
  return buffer;
}

/** The one voice every cell on this sheet is captioned in: the call over
 *  the picture, the readout it produced under it, set monospaced so the
 *  counts of six cells line up. */
kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kInk, 1.2f),
          .note = mono(11, kAsh),
          .gap = 8,
          .noteMeasure = kCell};
}

/** A drawn cell: a fixed picture between the call and its readout, so six
 *  of them line up whatever each one drew. */
Element cell(const char* heading, const std::string& reading, Element picture) {
  return kit::cell(voice(), toU8(heading), toU8(reading),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround),
                              .clip = false},
                             std::move(picture)));
}

Element meshCell(const char* heading, const std::string& reading,
                 gm::Mesh mesh) {
  return cell(heading, reading,
              custom(heading, [mesh = std::move(mesh)](SkCanvas& canvas,
                                                       const PaintContext& pc) {
                render::drawMesh(canvas, mesh, camera::place({0, 0, 0}, 24, 0),
                                 stageCamera(), pc.size, stageStyle());
              }));
}

}  // namespace

struct CodecRoundtrip final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

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
    const gm::Cloud read =
        cloudBack ? cloudBack->mergedCloud() : gm::Cloud{};

    // THE FIT. Bounds over every part, then the one transform that
    // centres a model and scales its longest extent to a stated size.
    glm::vec3 lo{0}, hi{0};
    if (fromAscii) fromAscii->bounds(&lo, &hi);
    gm::Mesh fitted = fromAscii ? fromAscii->merged() : gm::Mesh{};
    if (fromAscii) fitted.transform(fromAscii->fitTransform(120.0f));

    char boundsLine[160];
    std::snprintf(boundsLine, sizeof boundsLine,
                  "bounds (%.0f %.0f %.0f)-(%.0f %.0f %.0f) \xc2\xb7 "
                  "fitTransform(120) \xe2\x86\x92 longest extent 120",
                  (double)lo.x, (double)lo.y, (double)lo.z, (double)hi.x,
                  (double)hi.y, (double)hi.z);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("CODEC ROUND TRIP \xc2\xb7 encode::ply "
                           "\xe2\x86\x92 decode::model"),
             .subtitle = toU8("dials \xc2\xb7 the format (ascii, binary, "
                              "faceless cloud) \xc2\xb7 the generator "
                              "(torus R 62 r 23, 48 by 24)"),
             .footer = toU8("one decode::model call reads all of them "
                            "\xe2\x80\x94 the reader is picked off the "
                            "path hint, and OBJ, glTF, STL, Alembic and "
                            ".geo come through the same door"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {meshCell("shapes::torus \xc2\xb7 never "
                                         "written",
                                         count(source.positions.size(),
                                               "vertices") +
                                             " \xc2\xb7 " +
                                             count(source.indices.size() / 3,
                                                   "triangles"),
                                         source),
                                meshCell("encode::ply(mesh)",
                                         kib(ascii.size()) + " ascii \xc2\xb7 " +
                                             count(fromAscii
                                                       ? fromAscii->vertexCount()
                                                       : 0,
                                                   "vertices") +
                                             " \xc2\xb7 " +
                                             count(fromAscii
                                                       ? fromAscii->triangleCount()
                                                       : 0,
                                                   "triangles") + " back",
                                         fromAscii ? fromAscii->merged()
                                                   : gm::Mesh{}),
                                meshCell("encode::ply(mesh, {.binary = true})",
                                         kib(binary.size()) + " \xc2\xb7 " +
                                             count(fromBinary
                                                       ? fromBinary->vertexCount()
                                                       : 0,
                                                   "vertices") +
                                             " \xc2\xb7 floats exact, not "
                                             "decimal",
                                         fromBinary ? fromBinary->merged()
                                                    : gm::Mesh{})},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("encode::ply(cloud) \xc2\xb7 faceless",
                                     count(read.size(), "points") +
                                         " \xc2\xb7 " +
                                         std::to_string(read.colors.size()) +
                                         " colour, " +
                                         std::to_string(read.scalars.size()) +
                                         " scalar, " +
                                         std::to_string(read.vectors.size()) +
                                         " vector lanes read back",
                                     custom("cloud",
                                            [read](SkCanvas& canvas,
                                                   const PaintContext& pc) {
                                              gm::points::BillboardStyle splat;
                                              splat.size = 3.4f;
                                              splat.sizeLane = "size";
                                              splat.tintLane = "tint";
                                              splat.additive = false;
                                              gm::points::drawBillboards(
                                                  canvas, read, stageCamera(),
                                                  pc.size, splat);
                                            })),
                                meshCell("Model::bounds + Model::fitTransform",
                                         boundsLine, fitted),
                                cell("the header encode::ply wrote",
                                     "positions, then a property per lane "
                                     "\xe2\x80\x94 nx/ny/nz, uchar rgba, and "
                                     "each scalar under its own name",
                                     box().padding(12, 10).child(
                                         text(toU8(cloudPly.substr(
                                                  0,
                                                  cloudPly.find("end_header") +
                                                      (cloudPly.find(
                                                           "end_header") ==
                                                               std::string::npos
                                                           ? 0
                                                           : 10))),
                                              mono(9.5f, kFigure))
                                             .width(kCell - 24)))},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(CodecRoundtrip, "Kit \xc2\xb7 API",
             "a generated torus written through encode::ply as ascii, as "
             "binary and as a faceless cloud, each read back through "
             "decode::model and drawn beside its byte count")
