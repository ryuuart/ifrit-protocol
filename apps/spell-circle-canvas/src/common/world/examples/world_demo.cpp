// Headless SigilWorld scene: diegetic UI panels and SigilShape
// procedural props rendered by Diligent (Vulkan/MoltenVK), one PNG per
// camera angle.
// Usage: world_demo [outdir] [assetdir]  (defaults world_demo_out,
// assets). With fetch_assets run, the tiger SVG lands on a poster panel
// through SigilLoader's SVG decode; without it the scene simply omits
// the poster.

#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

#include <sigilimage/ImageAsset.h>
#include <sigilloader/Loader.h>
#include <sigilshape/Curves.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>

using namespace sigil;

namespace {

// Font-free UI card texture (header, rules, gauge, sparkline).
sk_sp<SkImage> uiCard(int w, int h, SkColor4f accent, float gaugeFrac) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas *c = surface->getCanvas();
  c->clear(SkColorSetARGB(235, 12, 16, 30));
  SkPaint p;
  p.setAntiAlias(true);

  p.setColor4f({accent.fR, accent.fG, accent.fB, 0.95f});
  c->drawRRect(SkRRect::MakeRectXY(
                   SkRect::MakeXYWH(18, 18, (float)w - 36, 16), 8, 8),
               p);
  p.setColor4f({1, 1, 1, 0.22f});
  for (int i = 0; i < 5; ++i)
    c->drawRRect(SkRRect::MakeRectXY(
                     SkRect::MakeXYWH(18, 56 + (float)i * 24,
                                      (float)(w - 70 - i * 36), 9),
                     4.5f, 4.5f),
                 p);

  SkPaint arc;
  arc.setAntiAlias(true);
  arc.setStyle(SkPaint::kStroke_Style);
  arc.setStrokeWidth(14);
  arc.setStrokeCap(SkPaint::kRound_Cap);
  arc.setColor4f({1, 1, 1, 0.14f});
  const SkRect gauge =
      SkRect::MakeXYWH((float)w - 140, (float)h - 140, 104, 104);
  c->drawArc(gauge, 130, 280, false, arc);
  arc.setColor4f(accent);
  c->drawArc(gauge, 130, 280 * gaugeFrac, false, arc);

  SkPathBuilder spark;
  for (int i = 0; i <= 30; ++i) {
    const float t = (float)i / 30.0f;
    const float x = 18 + t * ((float)w - 190);
    const float y = (float)h - 52 -
                    30.0f * (0.5f + 0.5f * std::sin(t * 8.0f + 1.2f)) -
                    16.0f * t;
    if (i == 0)
      spark.moveTo({x, y});
    else
      spark.lineTo({x, y});
  }
  SkPaint line;
  line.setAntiAlias(true);
  line.setStyle(SkPaint::kStroke_Style);
  line.setStrokeWidth(3.5f);
  line.setColor4f(accent);
  c->drawPath(spark.detach(), line);
  return surface->makeImageSnapshot();
}

SkPath starPath(int points, float outer, float inner) {
  SkPathBuilder b;
  const float step = (float)M_PI / (float)points;
  for (int i = 0; i < points * 2; ++i) {
    const float r = i % 2 == 0 ? outer : inner;
    const float a = -0.5f * (float)M_PI + step * (float)i;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

} // namespace

int main(int argc, char **argv) {
  const std::filesystem::path outDir =
      argc > 1 ? argv[1] : "world_demo_out";
  const std::filesystem::path assetDir = argc > 2 ? argv[2] : "assets";
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);

  world::WorldConfig config;
  config.width = 1440;
  config.height = 810;
  std::string error;
  std::unique_ptr<world::World> w = world::World::create(config, &error);
  if (!w) {
    std::fprintf(stderr, "world_demo: %s\n", error.c_str());
    return 1;
  }
  std::printf("backend: %s\n", w->backendName());

  // --- the set ------------------------------------------------------------
  // Floor: brushed slab.
  {
    world::Material floor;
    floor.baseColor = {0.16f, 0.17f, 0.2f, 1};
    floor.metallic = 0.85f;
    floor.roughness = 0.4f;
    shape::Mesh slab = shape::mesh::superellipsoid({900, 24, 620}, 8, 64, 24);
    w->addSurface(slab, SkM44::Translate(0, -190, 0), floor);
  }

  // Three emissive UI cards, cockpit arc.
  {
    world::Material screen;
    screen.unlit = true;
    screen.texture = uiCard(512, 340, {0.25f, 0.85f, 1.0f, 1}, 0.72f);
    w->addSurface(shape::mesh::quad(380, 252),
                  shape::space::place({-420, 60, -40}, 30), screen);
    screen.texture = uiCard(512, 340, {1.0f, 0.62f, 0.22f, 1}, 0.45f);
    w->addSurface(shape::mesh::quad(380, 252),
                  shape::space::place({0, 70, 30}, 0, -4), screen);
    screen.texture = uiCard(512, 340, {0.72f, 0.5f, 1.0f, 1}, 0.9f);
    w->addSurface(shape::mesh::quad(380, 252),
                  shape::space::place({420, 55, -40}, -30), screen);
  }

  // Curved ticker panel below the cards.
  {
    world::Material screen;
    screen.unlit = true;
    screen.texture = uiCard(1024, 220, {0.3f, 1.0f, 0.6f, 1}, 0.6f);
    w->addSurface(shape::mesh::cylinderPanel(880, 170, 560, 64, 12),
                  shape::space::place({0, -96, 90}, 0, 8), screen);
  }

  // Props: gold star (extruded), chrome blob, glass pane.
  {
    world::Material gold;
    gold.baseColor = {1.0f, 0.78f, 0.34f, 1};
    gold.metallic = 1;
    gold.roughness = 0.3f;
    shape::Mesh star =
        shape::mesh::extrude(starPath(5, 95, 44), {.depth = 34});
    w->addSurface(star, shape::space::place({-560, 280, -220}, 36, -10),
                  gold);

    world::Material chrome;
    chrome.baseColor = {0.95f, 0.97f, 1.0f, 1};
    chrome.metallic = 1;
    chrome.roughness = 0.08f;
    w->addSurface(shape::mesh::superellipsoid({110, 95, 80}, 2.4f, 64, 48),
                  shape::space::place({590, 300, -200}, 15, 0, -6), chrome);

    world::Material glass;
    glass.baseColor = {0.75f, 0.9f, 0.95f, 0.32f};
    glass.metallic = 0;
    glass.roughness = 0.05f;
    w->addSurface(shape::mesh::quad(360, 240),
                  shape::space::place({210, 40, 150}, -12, -3), glass);
  }

  // Fetched-asset poster: the Ghostscript tiger, decoded from SVG at
  // panel resolution through SigilLoader (run the fetch_assets target
  // to populate the asset dir; the scene degrades to no poster).
  if (std::filesystem::exists(assetDir / "svg/tiger.svg")) {
    loader::Hub hub;
    hub.mount("res://", assetDir);
    loader::ImageOptions options;
    options.width = 900;
    if (std::shared_ptr<const image::ImageAsset> tiger =
            hub.image("res://svg/tiger.svg", options)) {
      world::Material poster;
      poster.unlit = true;
      poster.texture = tiger->frameAt(0).image;
      w->addSurface(shape::mesh::quad(300, 300),
                    shape::space::place({-780, 60, 120}, 42), poster);
      std::printf("tiger poster: %dx%d\n", poster.texture->width(),
                  poster.texture->height());
    }
  }

  // The stream: a spline crossing the space above the set, carrying a
  // chrome wire and camera-facing UI cards instanced on its arc-length
  // points — declared through the scene layer (describe + reconcile),
  // not imperative addSurface calls.
  world::scene::Scene stream(*w);
  {
    shape::Spline3 arc;
    arc.points = {{-820, 260, -320},
                  {-300, 420, 60},
                  {260, 300, 220},
                  {820, 430, -260}};

    shape::Cloud stations = shape::points::onSpline(arc, 9);
    const SkV3 eye = {0, 200, 1150}; // the stream shot's camera
    std::vector<SkV3> &facing = stations.vector("facing");
    for (size_t i = 0; i < stations.size(); ++i) {
      const SkV3 to = eye - stations.positions[i];
      const float len = to.length();
      facing[i] = len > 1e-6f ? to * (1.0f / len) : SkV3{0, 0, 1};
    }
    shape::points::InstanceOptions cardOptions;
    cardOptions.orientLane = "facing";

    world::Material wireMat;
    wireMat.baseColor = {0.9f, 0.93f, 1.0f, 1};
    wireMat.metallic = 1;
    wireMat.roughness = 0.15f;
    world::Material cardMat;
    cardMat.unlit = true;
    cardMat.texture = uiCard(384, 256, {0.45f, 0.95f, 0.85f, 1}, 0.62f);
    cardMat.baseColor = {1, 1, 1, 0.92f};

    stream.render(
        world::scene::group().key("stream")
            .child(world::scene::surface(
                       shape::curves::tube(arc, {.radius = 7,
                                                 .segments = 180,
                                                 .sides = 10}),
                       wireMat)
                       .key("wire"))
            .child(world::scene::surface(
                       shape::points::panels(stations, 170, 112,
                                             cardOptions),
                       cardMat)
                       .key("cards")));
  }

  world::Lighting lighting;
  lighting.sunDirection = {-0.4f, -0.8f, -0.45f};
  lighting.sunIntensity = 2.4f;
  lighting.skyColor = {0.38f, 0.46f, 0.68f, 1};
  lighting.groundColor = {0.08f, 0.07f, 0.1f, 1};
  w->setLighting(lighting);

  struct Shot {
    const char *name;
    shape::space::Camera camera;
  };
  Shot shots[5];
  shots[4].name = "world_stream.png";
  shots[4].camera.eye = {0, 200, 1150};
  shots[4].camera.target = {0, 330, -60};
  shots[4].camera.fovYDeg = 46;
  shots[0].name = "world_cockpit.png";
  shots[0].camera.eye = {0, 90, 900};
  shots[0].camera.target = {0, 20, 0};
  shots[0].camera.fovYDeg = 46;
  shots[1].name = "world_low_orbit.png";
  shots[1].camera.eye = {-620, 300, 820};
  shots[1].camera.target = {60, -20, 0};
  shots[1].camera.fovYDeg = 40;
  shots[2].name = "world_close_panel.png";
  shots[2].camera.eye = {330, 60, 470};
  shots[2].camera.target = {120, 30, -40};
  shots[2].camera.fovYDeg = 38;
  shots[3].name = "world_poster.png";
  shots[3].camera.eye = {-840, 130, 660};
  shots[3].camera.target = {-700, 40, 80};
  shots[3].camera.fovYDeg = 42;

  const int total = (int)std::size(shots);
  int written = 0;
  for (const Shot &shot : shots) {
    w->setCamera(shot.camera);
    if (!w->render()) {
      std::fprintf(stderr, "render failed: %s\n", shot.name);
      continue;
    }
    if (w->savePng(outDir / shot.name))
      ++written;
    else
      std::fprintf(stderr, "write failed: %s\n", shot.name);
  }
  std::printf("wrote %d/%d shots to %s\n", written, total,
              outDir.string().c_str());
  return written == total ? 0 : 1;
}
