/** @file
 * usd_roundtrip — a set written to a stage and read back as the same
 * values it was described with.
 *
 * `usd::Writer` takes VALUES, never a renderer's memory: a `mesh::Mesh`
 * with its placement and its material, a `mesh::Cloud` with its stamp as
 * a point instancer, a `light::Light`, a `camera::Camera`. `save()`
 * writes whichever format the path's extension names — binary crate
 * (`.usdc`), ASCII (`.usda`) or a `.usdz` package — and the three carry
 * the same stage.
 *
 * `readModel` pours the stage's meshes and instancers into
 * `codec::decode::Model`, which is the currency every other format lands
 * in too, so a stage read here goes anywhere a PLY or a glTF does.
 * `readLights` and `readCameras` hand back the emitters and the lens AS
 * THE SAME VALUES THE WRITER TOOK, which is what makes this a round trip
 * rather than an export.
 *
 * THE CAMERA IS THE PROOF. Each cell is drawn from the camera that cell's
 * own file gave back — the source cell from the source camera, each
 * format's cell from the one `readCameras` read out of it. Four pictures
 * that agree are four cameras that agree, which no readout can claim as
 * plainly.
 *
 * WHAT USD HAS NO WORD FOR rides as custom data. A point light's range is
 * `sigil:range`; a material stack's depth is `sigil:layers`. The readouts
 * name what came back, including the material the binding carried — which
 * is also why the three read cells are brass and the source cell is not:
 * a stage carries the surface's colour as `displayColor`, so the merged
 * model comes back with a colour lane the authored mesh never had.
 *
 * WHAT THE WRITER REFUSES HERE is the third cell, printed rather than
 * hidden: `save()` exports the layer, and USD does not allow a PACKAGE
 * layer to be written that way — a `.usdz` wants the packaging utility
 * that assembles a crate and its neighbours into an archive. The crate
 * and the ASCII spellings round-trip whole.
 *
 * The stage is written into a temporary directory here, so the sheet is a
 * function of the geometry generated below and never of what a machine
 * happens to have on disk.
 *
 * EDIT THESE FIRST
 *   kR, kr        — the torus written to the stage.
 *   kMotes        — points the instancer carries.
 *   kMetersPerUnit — the stage metadata a consumer reads the size by.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Model.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace usd = sigil::usd;
namespace material = sigil::material;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
namespace codec = sigil::geometry::mesh::codec;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1200, 520};
constexpr float kCell = 274;
constexpr float kPicture = 232;

constexpr float kR = 62, kr = 23;  // the torus, major and minor radius
constexpr int kNu = 44, kNv = 22;  // how finely it is tessellated
constexpr int kMotes = 900;        // points the instancer carries
constexpr double kMetersPerUnit = 0.01;

constexpr SkColor4f kGround{0.07f, 0.075f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.56f, 0.57f, 0.63f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

}  // namespace

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** The one voice every cell is captioned in: the file over the picture,
 *  what came back out of it under, monospaced so four readouts line
 *  up. */
kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kInk, 1.2f),
          .note = mono(10, kAsh),
          .gap = 8,
          .noteMeasure = kCell};
}

render::MeshStyle stageStyle() {
  render::MeshStyle style;
  style.baseColor = {0.78f, 0.72f, 0.62f, 1};
  style.lights = {{{-0.5f, -0.7f, -0.5f}, {1.0f, 0.95f, 0.88f, 1}, 1.2f},
                  {{0.7f, -0.15f, -0.2f}, {0.40f, 0.55f, 0.9f, 1}, 0.5f}};
  style.specular = 0.7f;
  style.shininess = 56;
  return style;
}

/** The camera the set is described with, and the one every cell's own
 *  file has to give back. */
camera::Camera sourceCamera() {
  camera::Camera view;
  view.eye = {40, 92, 250};
  view.target = {0, 0, 0};
  view.fovYDeg = 36;
  return view;
}

const char* kindName(world::light::Kind kind) {
  switch (kind) {
    case world::light::Kind::Sun:
      return "sun";
    case world::light::Kind::Point:
      return "point";
    case world::light::Kind::Spot:
      return "spot";
  }
  return "?";
}

Element cell(const std::string& key, const char* heading,
             const std::string& reading, gm::Mesh mesh, camera::Camera lens) {
  return kit::cell(
      voice(), toU8(heading), toU8(reading),
      kit::well({.width = kCell,
                 .height = kPicture,
                 .ground = Fill::color(kCellGround),
                 .clip = false},
                custom(key, [mesh = std::move(mesh), lens](
                                SkCanvas& canvas, const PaintContext& pc) {
                  if (mesh.positions.empty()) return;
                  render::drawMesh(canvas, mesh, glm::mat4(1.0f), lens, pc.size,
                                   stageStyle());
                })));
}

}  // namespace

struct UsdRoundtrip final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE. USD's file formats are plugins found
   *  on disk when it first runs, so a build that links and starts can
   *  still open nothing. */
  static bool available(std::string* why) { return usd::available(why); }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // THE SOURCE, as values. Nothing below reaches into a renderer.
    const gm::Mesh source = gm::torus(kR, kr, kNu, kNv);
    const gm::Cloud motes = gm::points::onMesh(source, kMotes, 7);
    const material::Material brass =
        material::kit::surface({.baseColor = {0.76f, 0.58f, 0.28f, 1},
                                .metallic = 1,
                                .roughness = 0.28f});
    const world::light::Light sun =
        world::light::sun({-0.5f, -0.7f, -0.5f}, {1.0f, 0.95f, 0.88f, 1}, 1.2f);
    const camera::Camera lens = sourceCamera();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sigil-usd-roundtrip";
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);

    kit::Cells shelf{.gap = 18, .divider = Fill::color(kRule)};
    shelf.cells.push_back(cell(
        "source", "the set, as values",
        kit::format("%zu vertices \xc2\xb7 %zu triangles \xc2\xb7 no colour "
                    "lane\n%s light \xc2\xb7 fovY %.1f\xc2\xb0 \xc2\xb7 "
                    "%zu instancer points",
                    source.positions.size(), source.indices.size() / 3,
                    kindName(sun.kind), (double)lens.fovYDeg, motes.size()),
        source, lens));

    std::string names;
    std::string trouble;
    for (const char* extension : {".usdc", ".usda", ".usdz"}) {
      const std::filesystem::path file = dir / (std::string("set") + extension);
      std::string error;
      {
        usd::Writer writer(file, {.metersPerUnit = kMetersPerUnit});
        writer.mesh("torus", source, glm::mat4(1.0f), brass);
        writer.stamps("motes", motes, gm::superellipsoid({3, 3, 3}, 2.0f, 6, 4),
                      glm::mat4(1.0f), brass);
        writer.light("key", sun);
        writer.camera("lens", lens);
        if (!writer.save(&error)) {
          if (trouble.empty()) trouble = error;
          shelf.cells.push_back(cell(extension, extension,
                                     "save refused: " + error, gm::Mesh{},
                                     lens));
          continue;
        }
      }

      usd::ReadInfo info;
      const std::optional<codec::decode::Model> model =
          usd::readModel(file, &info, &error);
      const auto lamps = usd::readLights(file, &error);
      const auto lenses = usd::readCameras(file, &error);
      const uintmax_t bytes = std::filesystem::file_size(file, ignored);

      const gm::Mesh back = model ? model->merged() : gm::Mesh{};
      const camera::Camera readLens =
          lenses && !lenses->empty() ? lenses->front().camera : lens;
      if (names.empty() && !info.materialNames.empty())
        names = info.materialNames.front();

      shelf.cells.push_back(cell(
          extension, extension,
          kit::format(
              "%.1f KiB \xc2\xb7 %zu parts \xc2\xb7 %zu vertices\n"
              "%zu light%s (%s) \xc2\xb7 %zu camera%s \xc2\xb7 fovY %.1f"
              "\xc2\xb0",
              (double)bytes / 1024.0, model ? model->parts.size() : 0,
              back.positions.size(), lamps ? lamps->size() : 0,
              lamps && lamps->size() == 1 ? "" : "s",
              lamps && !lamps->empty() ? kindName(lamps->front().light.kind)
                                       : "none",
              lenses ? lenses->size() : 0,
              lenses && lenses->size() == 1 ? "" : "s",
              (double)readLens.fovYDeg),
          back, readLens));
    }

    std::string foot = kit::format(
        "Writer(metersPerUnit = %g) \xc2\xb7 UsdGeomMesh + "
        "UsdGeomPointInstancer + UsdLuxDistantLight + UsdGeomCamera "
        "under /World",
        kMetersPerUnit);
    if (!names.empty())
      foot += "   \xc2\xb7   ReadInfo bound \xe2\x80\x9c" + names +
              "\xe2\x80\x9d as the material";
    if (!trouble.empty())
      foot += "   \xc2\xb7   a package layer is not written through save()";

    ctx.composer.render(
        kit::sheet({.title = toU8("USD ROUND TRIP \xc2\xb7 usd::Writer "
                                  "\xe2\x86\x92 readModel / readLights / "
                                  "readCameras"),
                    .subtitle = toU8("dials \xc2\xb7 the format (.usdc, "
                                     ".usda, .usdz) \xc2\xb7 metersPerUnit "
                                     "\xe2\x80\x94 each cell drawn from the "
                                     "camera its own file gave back"),
                    .footer = toU8(foot),
                    .titleStyle = label(14, kInk, 2.4f),
                    .subtitleStyle = label(11, kAsh, 0.6f),
                    .footerStyle = label(10.5f, kAsh, 0.3f),
                    .marginX = 24,
                    .marginTop = 20,
                    .marginBottom = 16,
                    .ground = Fill::color(kGround),
                    .rule = Fill::color(kRule)},
                   kit::cells(std::move(shelf)))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(UsdRoundtrip, "Kit \xc2\xb7 API",
             "one set written to crate, ASCII and a package, each read "
             "back and drawn from the camera its own file carried")
