/** @file
 * A mesh, point instancer, light and camera written as USD values.
 * Successful formats are drawn using the cameras read from their stages;
 * readouts report the decoded topology and scene contents. Failed writes
 * produce a labelled result and a diagnostic in the export summary.
 * The writer exports layers directly and assembles packages with USD's archive
 * utility.
 */

// TAGS: Media/Models

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Model.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace usd = sigil::usd;
namespace material = sigil::material;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
namespace codec = sigil::geometry::mesh::codec;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1200, 740};
constexpr float kCell = 262;
constexpr float kPicture = 232;

constexpr float kR = 62, kr = 23;  // the torus, major and minor radius
constexpr int kNu = 44, kNv = 22;  // how finely it is tessellated
constexpr int kMotes = 900;        // points the instancer carries
constexpr double kMetersPerUnit = 0.01;

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.07f, 0.075f, 0.085f, 1};
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.spacing.captionGap = 8;
  return look;
}

}  // namespace

using namespace sigil::compose;

namespace {

/** The one voice every cell is captioned in: the file over the picture,
 *  what came back out of it under, monospaced so four readouts line
 *  up. */
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

sketch::kit::ComparisonCase specimen(const std::string& key,
                                     const char* heading,
                                     const std::string& reading, gm::Mesh mesh,
                                     std::optional<camera::Camera> lens) {
  return {.title = heading,
          .control =
              lens ? kit::formatted("%s fovY %.1f°",
                                    key == "source" ? "Authored" : "Returned",
                                    (double)lens->fovYDeg)
                   : std::string("Camera unavailable"),
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(key,
                     [mesh = std::move(mesh), lens](SkCanvas& canvas,
                                                    const PaintContext& pc) {
                       if (lens && !mesh.positions.empty())
                         render::drawMesh(canvas, mesh, glm::mat4(1), *lens,
                                          pc.size, stageStyle());
                     })),
          .note = reading};
}

}  // namespace

struct UsdRoundtrip {
  /** WHAT THIS MACHINE MUST HAVE. USD's file formats are plugins found
   *  on disk when it first runs, so a build that links and starts can
   *  still open nothing. */
  static bool available(std::string* why) { return usd::available(why); }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

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

    std::vector<sketch::kit::ComparisonCase> cases;
    cases.push_back(specimen(
        "source", "AUTHORED VALUES",
        kit::formatted("%zu vertices · %zu triangles · no colour "
                       "lane\n%s light · fovY %.1f° · "
                       "%zu instancer points",
                       source.positions.size(), source.indices.size() / 3,
                       kindName(sun.kind), (double)lens.fovYDeg, motes.size()),
        source, lens));

    std::string names;
    std::string trouble;
    bool packaged = false;
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
          trouble += std::string(extension) + ": " + error + "\n";
          cases.push_back(
              {.title = extension,
               .control = "Save failed",
               .figure =
                   sketch::kit::well(
                       {.width = kCell, .height = kPicture, .padding = 20})
                       .children({text("No exported stage").width(kCell - 40)}),
               .note = error});
          continue;
        }
      }

      if (std::string_view(extension) == ".usdz") packaged = true;
      usd::ReadInfo info;
      std::string modelError, lightError, cameraError;
      const std::optional<codec::decode::Model> model =
          usd::readModel(file, &info, &modelError);
      const auto lamps = usd::readLights(file, &lightError);
      const auto lenses = usd::readCameras(file, &cameraError);
      for (const auto& failure : {modelError, lightError, cameraError})
        if (!failure.empty())
          trouble += std::string(extension) + ": " + failure + "\n";
      const uintmax_t bytes = std::filesystem::file_size(file, ignored);

      const gm::Mesh back = model ? model->merged() : gm::Mesh{};
      const std::optional<camera::Camera> readLens =
          lenses && !lenses->empty()
              ? std::optional<camera::Camera>(lenses->front().camera)
              : std::nullopt;
      if (names.empty() && !info.materialNames.empty())
        names = info.materialNames.front();

      cases.push_back(specimen(
          extension, extension,
          kit::formatted(
              "%.1f KiB · %zu parts · %zu vertices\n"
              "%zu light%s (%s) · %zu camera%s · fovY %.1f"
              "°",
              (double)bytes / 1024.0, model ? model->parts.size() : 0,
              back.positions.size(), lamps ? lamps->size() : 0,
              lamps && lamps->size() == 1 ? "" : "s",
              lamps && !lamps->empty() ? kindName(lamps->front().light.kind)
                                       : "none",
              lenses ? lenses->size() : 0,
              lenses && lenses->size() == 1 ? "" : "s",
              readLens ? (double)readLens->fovYDeg : 0.0),
          back, readLens));
    }

    std::string foot = kit::formatted(
        "Writer(metersPerUnit = %g) · UsdGeomMesh + "
        "UsdGeomPointInstancer + UsdLuxDistantLight + UsdGeomCamera "
        "under /World",
        kMetersPerUnit);
    if (!names.empty())
      foot += "   ·   ReadInfo bound “" + names + "” as the material";
    if (!trouble.empty())
      foot += "   ·   read/write diagnostics are shown below";

    ctx.composer.render(sketch::kit::page(
        {.title = "A scene leaves the renderer",
         .subtitle = "Mesh, instancer, light and lens travel as values. Every "
                     "returned scene uses the camera read from its own stage.",
         .footer = foot},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = std::move(cases), .measure = 1120, .gap = 24}),
             sketch::kit::sectionHeader(
                 {.label = "PACKAGE EXPORT", .note = ".usdz"}),
             sketch::kit::well({.width = 1120, .padding = 20})
                 .row()
                 .gap(28)
                 .children(
                     {box().column().gap(12).width(286).children(
                          {document::label(packaged ? "PACKAGE WRITTEN"
                                                    : "PACKAGE UNAVAILABLE"),
                           text("A package needs an archive containing its "
                                "crate and dependencies.")
                               .width(286)}),
                      text(trouble.empty()
                               ? "All three stages were written and read. USDZ "
                                 "packages a crate and its dependencies into "
                                 "one archive."
                               : trouble)
                          .width(480)
                          .styleClass("readout")})})));
  }
};

SIGIL_SKETCH(UsdRoundtrip, "Kit · API",
             "one set written to crate, ASCII and a package, each read "
             "back and drawn from the camera its own file carried")
