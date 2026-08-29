// A headless SigilWorld scene: UI panels and procedural props rendered
// through Vulkan/MoltenVK, written out as one PNG per camera angle.
//
// Usage: world_demo [outdir] [assetdir] [frameCount]
//   outdir     where the PNGs go (default world_demo_out)
//   assetdir   optional fetched assets (default assets); when it holds
//              the SVG below, an extra poster panel joins the scene
//   frameCount if given, also dumps that many sequential animation
//              frames for encoding into a video
//
// Every shot but the last aims its camera by hand through setCamera().
// The last one exercises the declared camera instead — an AnimatedCamera
// with a CameraPath and a wiggled roll lane — and it lives here rather
// than in a live-coding sketch because that host has no GPU device. See
// the block at the bottom of main().

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilimage/Decode.h>
#include <sigilimage/ImageAsset.h>
#include <sigilloader/Loader.h>
#include <sigilmotion/Ticker.h>
#include <sigilgeometry/Curves.h>
#include <sigilgeometry/Import.h>
#include <sigilgeometry/Mesh.h>
#include <sigilgeometry/Points.h>
#include <sigilgeometry/Save.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweavekit/SigilWeaveKit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <iterator>
#include <optional>
#include <vector>

#include "sigilworld/Animation.h"
#include "sigilworld/Components.h"
#include "sigilworld/Easel.h"
#include "sigilworld/Scene.h"
#include "sigilworld/TextureSet.h"
#include "sigilworld/World.h"
#ifdef SIGIL_WORLD_DEMO_SUBSTANCE_ASSETS
#include <sigilsubstance/Substance.h>
#endif
#ifdef SIGIL_WORLD_DEMO_USD
#include <sigilusd/Usd.h>
#endif

using namespace sigil;

namespace {

// Font-free UI card texture (header, rules, gauge, sparkline).
sk_sp<SkImage> uiCard(int w, int h, SkColor4f accent, float gaugeFrac) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas* c = surface->getCanvas();
  c->clear(SkColorSetARGB(235, 12, 16, 30));
  SkPaint p;
  p.setAntiAlias(true);

  p.setColor4f({accent.fR, accent.fG, accent.fB, 0.95f});
  c->drawRRect(
      SkRRect::MakeRectXY(SkRect::MakeXYWH(18, 18, (float)w - 36, 16), 8, 8),
      p);
  p.setColor4f({1, 1, 1, 0.22f});
  for (int i = 0; i < 5; ++i)
    c->drawRRect(
        SkRRect::MakeRectXY(SkRect::MakeXYWH(18, 56 + (float)i * 24,
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

// --- the ribbon's artwork -------------------------------------------------
// One long strip of type and graphics, wound end to end around the
// scene. The whole winding is a single tall column of laid-out content:
// every line of type reads ACROSS the band's width, and the column
// advances ALONG the winding. It is packed with numbered sectors — a
// numeral, a narrow-column paragraph, and one of four graphic stretches,
// each parameterized by its sector index so that no two render alike —
// separated only by thin growing gaps, so no stretch of the loop is
// blank.
//
// The column is laid out and snapshotted ONCE as a vector picture, then
// sliced straight down into GPU tiles: texture x is u and y is v, with
// no transpose. Each tile is drawn mirrored in x, because the ribbon
// wall's own u mapping mirrors it back. Adjacent arcs share their
// boundary texels, so the seams are invisible.
struct StripArt {
  std::vector<sk_sp<SkImage>> tiles;
  float acrossPx = 0;  ///< column width = the band's texel width
  float totalAlongPx = 0;
};

StripArt yarnStrip(sigil::weave::FontContext& fonts, int tileCount,
                   int tileAlongPx, int acrossPx) {
  namespace sc = sigil::compose;
  namespace weave = sigil::weave;
  const SkColor kInk = SkColorSetARGB(255, 236, 244, 254);
  const SkColor kAccent = SkColorSetARGB(255, 116, 224, 190);
  const SkColor kDim = SkColorSetARGB(255, 158, 176, 202);
  const float total = (float)tileCount * (float)tileAlongPx;

  const auto para = [](const char8_t* string, float size, SkColor color) {
    auto p = std::make_shared<weave::Paragraph>();
    p->appendText(string, weave::kit::makeStyle(size, color));
    return p;
  };
  weave::ParagraphLayoutOptions centered;
  centered.alignment = weave::TextAlignment::kCenter;
  const auto label = [&](std::u8string string, float size, SkColor color) {
    auto p = std::make_shared<weave::Paragraph>();
    p->appendText(std::move(string), weave::kit::makeStyle(size, color));
    return sc::text(p, centered);
  };

  // Sector graphics, parameterized so no two stretches render alike.
  const auto ruler = [&](int s) {
    sc::Element row = sc::box()
                          .row()
                          .gap(8.0f + (float)(s % 3) * 3.0f)
                          .alignItems(sc::Align::Center)
                          .alignSelf(sc::Align::Center)
                          .height(150);
    const int major = 5 + s % 4;
    for (int i = 0; i < 30 + (s % 3) * 5; ++i)
      row.child(sc::box()
                    .width(i % major == 0 ? 5.0f : 3.0f)
                    .height(i % major == 0 ? 140.0f : 60.0f)
                    .fill(sc::Fill::color(
                        {0.58f, 0.66f, 0.77f, i % major == 0 ? 0.95f : 0.5f})));
    return row;
  };
  const auto wave = [&](int s) {
    sc::Element row = sc::box()
                          .row()
                          .gap(5)
                          .alignItems(sc::Align::End)
                          .alignSelf(sc::Align::Center)
                          .height(170);
    for (int i = 0; i < 44; ++i) {
      const float beat = std::clamp(
          0.5f +
              0.35f * std::sin((float)i * (0.29f + 0.03f * (float)(s % 5)) +
                               (float)s * 1.7f) +
              0.3f * std::sin((float)i * 0.11f + (float)s),
          0.0f, 1.0f);
      row.child(sc::box()
                    .width(6)
                    .height(28 + 134 * beat)
                    .corners({3})
                    .fill(sc::Fill::color(
                        {0.455f, 0.878f, 0.745f, 0.45f + 0.55f * beat})));
    }
    return row;
  };
  const auto swatches = [&](int s) {
    sc::Element row = sc::box().row().gap(13).alignSelf(sc::Align::Center);
    for (int i = 0; i < 12; ++i) {
      const float f = (float)((i + s * 5) % 12) / 11.0f;
      row.child(
          sc::box().width(24).height(24).corners({12}).fill(sc::Fill::color(
              {0.4f + 0.45f * f, 0.878f, 0.745f, 0.35f + 0.65f * f})));
    }
    return row;
  };
  const auto dots = [&](int s) {
    sc::Element row = sc::box()
                          .row()
                          .gap(15)
                          .alignItems(sc::Align::Center)
                          .alignSelf(sc::Align::Center)
                          .height(60);
    for (int i = 0; i < 14; ++i) {
      const float f = 0.5f + 0.5f * std::sin((float)(i + s * 3) * 0.55f);
      row.child(
          sc::box()
              .width(10 + 22 * f)
              .height(10 + 22 * f)
              .corners({(10 + 22 * f) * 0.5f})
              .fill(sc::Fill::color({0.72f, 0.82f, 0.95f, 0.3f + 0.7f * f})));
    }
    return row;
  };

  const char8_t* pool[10] = {
      u8"a paragraph can behave like yarn: one description wound seven "
      "times around the stage while its plane turns twice, painted end "
      "to end.",
      u8"every line reads across the band and the column climbs the "
      "winding — the hanging-scroll orientation riding the yarn, "
      "perpendicular to the flight.",
      u8"nothing here tiles and nothing repeats: each sector is a "
      "different neighborhood of the same element tree, numbered as it "
      "passes.",
      u8"the strip is snapshotted once as a vector picture and sliced "
      "into GPU tiles, one per arc — the seams share their texels and "
      "vanish.",
      u8"geometry by SigilGeometry, type by SigilWeave, authored in "
      "SigilCompose, lit by SigilWorld on Diligent and Vulkan.",
      u8"the cloth hangs from its own tangent like a towed banner, so "
      "the words never roll upside down anywhere on the ball.",
      u8"each frame every arc re-sweeps one step forward — the same "
      "vertices, updated in place — and the whole canvas marches.",
      u8"the dart is the pilot: a chrome revolve riding the head of the "
      "winding, always one sector ahead of the seam.",
      u8"the band is wide enough for a real measure, so the paragraphs "
      "set as narrow columns, centered, sector after sector.",
      u8"read it the long way: twenty-four thousand units of continuous "
      "surface, and back to its own beginning.",
  };

  sc::Element root =
      sc::box()
          .column()
          .width((float)acrossPx)
          .height(total)
          .padding(52, 110)
          .child(sc::box().left(10).top(0).bottom(0).width(6).fill(
              sc::Fill::color({0.455f, 0.878f, 0.745f, 0.95f})))
          .child(sc::box().right(10).top(0).bottom(0).width(4).fill(
              sc::Fill::color({0.455f, 0.878f, 0.745f, 0.5f})));

  root.child(label(u8"THE YARN", 96, kAccent));
  // Enough sectors to fill most of the column; the growing gaps between
  // them absorb whatever height is left, evenly.
  const int kSectors = 44;
  for (int s = 0; s < kSectors; ++s) {
    root.child(sc::box().grow());
    char numeral[8];
    std::snprintf(numeral, sizeof(numeral), "%02d", s + 1);
    root.child(label(std::u8string(u8"— ") + (const char8_t*)numeral + u8" —",
                     64, kDim));
    root.child(
        label(pool[s % 10], 44, s % 10 == 4 || s % 10 == 9 ? kDim : kInk));
    switch (s % 4) {
      case 0:
        root.child(ruler(s));
        break;
      case 1:
        root.child(wave(s));
        break;
      case 2:
        root.child(swatches(s));
        break;
      default:
        root.child(dots(s));
        break;
    }
  }
  root.child(sc::box().grow());
  root.child(label(u8"— and back to its own beginning", 72, kAccent));

  const sk_sp<SkPicture> art = sc::snapshot(root, fonts);
  // Re-record the picture behind a bounding-box hierarchy, so each
  // tile's replay visits only the drawing operations that meet it
  // instead of the whole column. The tiles come out pixel-identical to
  // replaying the original.
  const sk_sp<SkPicture> sliced = sc::tiles::sliceable(art);
  StripArt out;
  out.acrossPx = (float)acrossPx;
  out.totalAlongPx = total;
  for (int k = 0; k < tileCount; ++k) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(acrossPx, tileAlongPx));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    // Step down the column to tile k, mirrored across it because the
    // ribbon wall's own u mapping mirrors back. The transform comes from
    // sc::tiles::window() rather than being spelled out here: it is easy
    // to get the mirror and the step the wrong way round, and the band's
    // legibility on the wall rests entirely on it.
    canvas->concat(sc::tiles::window({acrossPx, tileAlongPx}, k,
                                     sc::tiles::Flow::Down,
                                     sc::tiles::Facing::Mirrored));
    canvas->drawPicture(sliced);
    out.tiles.push_back(surface->makeImageSnapshot());
  }
  return out;
}

float wrap01(float t) { return t - std::floor(t); }

/** Place the dart at @p head on the loop, with the revolved mesh's +y
 *  nose aimed down the flight tangent and its up axis matching the
 *  ribbon's. */
glm::mat4 dartTransform(const sigil::geometry::Spline3& loop, float head) {
  const glm::vec3 p = loop.position(wrap01(head));
  const glm::vec3 ahead = loop.position(wrap01(head + 0.004f));
  const glm::vec3 behind = loop.position(wrap01(head - 0.004f));
  glm::vec3 t = ahead - behind;
  const float len = glm::length(t);
  t = len > 1e-6f ? t * (1.0f / len) : glm::vec3{1, 0, 0};
  glm::vec3 n = glm::vec3{0, 0, 1} - t * glm::dot(t, glm::vec3{0, 0, 1});
  const float nLen = glm::length(n);
  n = nLen > 1e-6f ? n * (1.0f / nLen) : glm::vec3{0, 1, 0};
  const glm::vec3 b = glm::cross(t, n);
  // Basis columns (binormal, tangent, normal | position): glm's matrix
  // constructor takes columns, so this needs no transpose.
  return glm::mat4(glm::vec4(b, 0), glm::vec4(t, 0), glm::vec4(n, 0),
                   glm::vec4(p, 1));
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

}  // namespace

namespace {

/** Decode an image file through SigilImage; null when unreadable. */
sk_sp<SkImage> decodeFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return nullptr;
  const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  std::optional<image::ImageAsset> asset = image::decodeImage(
      reinterpret_cast<const std::byte*>(bytes.data()), bytes.size(), {}, path);
  if (!asset) return nullptr;
  return asset->frameAt(0).image;
}

/** The pop lab: a point chain seeded from an existing point set — the
 *  fetched Avocado's own vertices when the model is there, a scattered
 *  torus otherwise — selected into a band, the band twisted, the rest
 *  peaked along its normals and coloured by height, cooked on the GPU
 *  and drawn as one instanced surface. Written as world_pops.png. */
void renderPopLab(const std::filesystem::path& outDir,
                  const std::filesystem::path& assetDir) {
  world::WorldConfig config;
  config.width = 1440;
  config.height = 810;
  config.clearColor = {0.03f, 0.032f, 0.045f, 1};
  std::string error;
  std::unique_ptr<world::World> w = world::World::create(config, &error);
  if (!w) return;

  // The seed: an imported model's vertices as a cloud, fitted to the
  // stage — every lane it carries rides into the chain — or, without
  // the asset, points scattered on a torus.
  geometry::Cloud seed;
  std::string seedName = "torus scatter";
  if (std::optional<geometry::import::Model> avocado =
          geometry::import::model(assetDir / "models/Avocado.glb")) {
    const glm::mat4 fit = avocado->fitTransform(520);
    geometry::Mesh merged = avocado->merged();
    merged.transform(fit);
    // Densify: scatter on the fitted surface, keeping its normals.
    seed = geometry::points::onMesh(merged, 14000, 3);
    seedName = "Avocado.glb, scattered";
  } else {
    seed = geometry::points::onMesh(geometry::mesh::torus(180, 70, 96, 48), 14000, 3);
  }
  const std::vector<glm::vec4> heightStops = {
      {0.05f, 0.15f, 0.7f, 1}, {0.95f, 0.25f, 0.1f, 1}, {1.0f, 0.85f, 0.2f, 1}};
  glm::vec3 lo, hi;
  lo = hi = seed.positions.empty() ? glm::vec3{0} : seed.positions[0];
  for (const glm::vec3& p : seed.positions) {
    lo = glm::min(lo, p);
    hi = glm::max(hi, p);
  }
  const geometry::pop::Chain chain =
      geometry::pop::on(seed)
          .select("band", geometry::pop::Select::Shape::Box,
                  {0, (lo.y + hi.y) * 0.5f, 0},
                  {2000, (hi.y - lo.y) * 0.18f, 2000}, 0.4f)
          .rampBy(geometry::pop::Lane::P, 1, heightStops, lo.y, hi.y)
          .peak(14)
          .select("band", geometry::pop::Select::Shape::Box,
                  {0, (lo.y + hi.y) * 0.5f, 0},
                  {2000, (hi.y - lo.y) * 0.18f, 2000}, 0.4f,
                  geometry::pop::Select::Combine::Replace, true)
          .masked("band")  // the peak: everyone OUTSIDE the band
          .select("band", geometry::pop::Select::Shape::Box,
                  {0, (lo.y + hi.y) * 0.5f, 0},
                  {2000, (hi.y - lo.y) * 0.18f, 2000}, 0.4f)
          .twist(90, {0, 1, 0}, lo.y, hi.y, {60, 0, 0})
          .masked("band")
          .peak(-40)
          .masked("band")
          .vary(0.45f, 1.0f)
          .lookAt({120, 260, 900});
  world::Material flake;
  flake.baseColor = {1, 1, 1, 1};
  flake.roughness = 0.85f;
  const uint32_t id = w->placeChain(geometry::mesh::quad(5, 5), chain, flake);
  if (id == 0) {
    std::fprintf(stderr, "pop lab: the executor declined the chain\n");
    return;
  }
  // A quiet floor and the studio light.
  world::Material floor;
  floor.baseColor = {0.12f, 0.12f, 0.14f, 1};
  floor.roughness = 0.5f;
  floor.metallic = 0.3f;
  w->place(geometry::mesh::grid(2, 2,
                             [&](float u, float v) -> glm::vec3 {
                               return {(u - 0.5f) * 1800, lo.y - 40,
                                       (v - 0.5f) * 1200};
                             }),
           glm::mat4(1.0f), floor);
  world::Lighting lighting;
  lighting.sunDirection = {-0.35f, -0.75f, -0.55f};
  lighting.sunIntensity = 2.6f;
  lighting.ambient = 0.45f;
  if (sk_sp<SkImage> hdri =
          decodeFile(assetDir / "hdri/studio_small_09_1k.hdr")) {
    lighting.environment = hdri;
    lighting.environmentRotationDeg = 200;
  }
  w->setLighting(lighting);
  geometry::space::Camera camera;
  camera.eye = {120, 260, 900};
  camera.target = {0, (lo.y + hi.y) * 0.5f, 0};
  camera.fovYDeg = 44;
  w->setCamera(camera);
  if (w->render() && w->savePng(outDir / "world_pops.png"))
    std::printf("wrote world_pops.png (%s, %zu points, %zu ops)\n",
                seedName.c_str(), seed.size(), chain.size());
}

/** The material lab: one shot, several props, each dressed by a
 *  different door into Material — a fetched texture set read by its
 *  file names, a Substance archive rendered on the fly (twice, with a
 *  parameter moved), and plain scalar materials for reference. Written
 *  as world_materials.png. */
void renderMaterialLab(const std::filesystem::path& outDir,
                       const std::filesystem::path& assetDir, bool dark) {
  world::WorldConfig config;
  config.width = 1440;
  config.height = 810;
  config.clearColor = {0.03f, 0.032f, 0.045f, 1};
  std::string error;
  std::unique_ptr<world::World> w = world::World::create(config, &error);
  if (!w) return;

  // Every prop placed here is also remembered as the VALUES it was made
  // of, so the lit lab can be written out as USD after the shot.
  struct Placed {
    std::string name;
    geometry::Mesh mesh;
    glm::mat4 model;
    std::vector<world::Material> slots;
  };
  std::vector<Placed> placed;
  int propIndex = 0;
  const auto add = [&](const geometry::Mesh& mesh, const glm::mat4& model,
                       auto materialOrSlots) {
    std::vector<world::Material> slots;
    if constexpr (std::is_same_v<std::decay_t<decltype(materialOrSlots)>,
                                 world::Material>)
      slots = {materialOrSlots};
    else
      slots = std::vector<world::Material>(materialOrSlots.begin(),
                                           materialOrSlots.end());
    placed.push_back(
        {"prop" + std::to_string(propIndex++), mesh, model, slots});
    return w->place(mesh, model, slots);
  };

  world::Lighting lighting;
  lighting.sunDirection = {-0.35f, -0.75f, -0.55f};
  lighting.sunIntensity = dark ? 0.0f : 2.4f;
  lighting.skyColor = {0.42f, 0.5f, 0.7f, 1};
  lighting.groundColor = {0.09f, 0.08f, 0.1f, 1};
  lighting.ambient = dark ? 0.06f : 0.9f;
  // The fetched studio HDRI lights the lab when it is there: decoded
  // float, so the panorama keeps its range through the upload; the
  // same image, flattened, dresses a backdrop sphere so the reflections
  // have a visible source. Absent, the hemisphere stands in. The dark
  // shot takes neither: what light there is comes from the emissive
  // props and a faint hemisphere.
  if (sk_sp<SkImage> hdri =
          dark ? nullptr
               : decodeFile(assetDir / "hdri/studio_small_09_1k.hdr")) {
    lighting.environment = hdri;
    lighting.environmentIntensity = 1.0f;
    lighting.environmentRotationDeg = 200;
    world::Material sky;
    sky.unlit = true;
    sky.texture = hdri;  // flattened to 8-bit here: the backdrop clips
    sky.baseColor = {0.55f, 0.55f, 0.55f, 1};
    sky.uvScale = {-1, 1};  // seen from inside: mirror u
    sky.uvOffset = {1, 0};
    sky.tile = true;
    // Inside the camera's far plane all round.
    geometry::Mesh dome =
        geometry::mesh::superellipsoid({2400, 2400, 2400}, 2, 96, 48);
    add(dome, geometry::space::place({0, 0, 0}, 200 + 180), sky);
    std::printf("environment: %dx%d\n", hdri->width(), hdri->height());
  }
  w->setLighting(lighting);
  if (!dark) {
    world::LightComponent fill;
    fill.type = world::LightComponent::Type::Point;
    fill.position = {520, 380, 520};
    fill.color = {1.0f, 0.85f, 0.7f, 1};
    fill.intensity = 1.6f;
    fill.range = 1500;
    w->addLight(fill);
  }

  // 1. A texture set from the fetched assets, found by its file names.
  world::Material plate;
  bool havePlate = false;
  for (const world::textures::TextureSet& set :
       world::textures::discover(assetDir / "textures/metal_plate")) {
    plate = world::textures::material(set, decodeFile);
    havePlate = plate.texture != nullptr;
    std::printf("texture set %s: %zu maps\n", set.name.c_str(),
                set.files.size());
  }
  if (!havePlate) {
    plate.baseColor = {0.5f, 0.5f, 0.55f, 1};
    plate.metallic = 1;
    plate.roughness = 0.35f;
  }
  // The floor: the plate laid down four times across.
  world::Material floor = plate;
  floor.uvScale = {4, 4};
  add(geometry::mesh::grid(2, 2,
                        [](float u, float v) -> glm::vec3 {
                          return {(u - 0.5f) * 1800, -150, (v - 0.5f) * 1200};
                        }),
      glm::mat4(1.0f), floor);
  // A sphere wearing it once around — LAYERED: rust where the plate's
  // own occlusion runs deep (the AO map, inverted and fitted, as the
  // mask), then moss on the faces that look up. One material, read
  // top-down: steel, over that rust where the mask says, over that moss
  // where the slope says.
  world::Material once = plate;
  once.uvScale = {2, 1};
  world::Material rust;
  rust.baseColor = {0.42f, 0.16f, 0.06f, 1};
  rust.roughness = 0.95f;
  rust.metallic = 0;
  world::Material moss;
  moss.baseColor = {0.16f, 0.32f, 0.08f, 1};
  moss.roughness = 1;
  world::Material weathered = once;
  if (plate.occlusionMap)
    weathered = weathered.over(
        rust, world::Mask::fromMap(plate.occlusionMap, plate.occlusionChannel)
                  .window(once.uvScale, once.uvOffset)
                  .invert()
                  .fit(0.35f, 0.75f));
  weathered = weathered.over(moss, world::Mask::slope({0, 1, 0}, 0.55f, 0.9f));
  add(geometry::mesh::superellipsoid({150, 150, 150}, 2, 96, 64),
      geometry::space::place({-470, -10, 200}, 20), weathered);
  // The torus wears TWO material slots: its "Material" prim lane
  // alternates around the ring, so every other segment is the plate and
  // the rest a plain dark rubber — one prop, one transform, per-face
  // materials.
  world::Material band = plate;
  band.uvScale = {6, 1.5f};
  world::Material rubber2;
  rubber2.baseColor = {0.09f, 0.09f, 0.1f, 1};
  rubber2.roughness = 0.85f;
  geometry::Mesh torus = geometry::mesh::torus(150, 60, 96, 48);
  std::vector<glm::vec4>& slotLane = torus.prim("Material", {0, 0, 0, 0});
  for (size_t t = 0; t < slotLane.size(); ++t)
    slotLane[t] = {(float)((t / (48 * 2 * 8)) % 2), 0, 0, 0};
  add(torus, geometry::space::place({420, 20, 0}, 0, -20),
      std::vector<world::Material>{band, rubber2});

#ifdef SIGIL_WORLD_DEMO_SUBSTANCE_ASSETS
  // 2. A Substance archive rendered here and now: the SDK's own sample,
  // its normal format switched to OpenGL through the graph's standard
  // $normalformat input, once at the authored season and once moved.
  const std::filesystem::path sbsar =
      std::filesystem::path(SIGIL_WORLD_DEMO_SUBSTANCE_ASSETS) /
      "Autumn_Leaves.sbsar";
  std::string sbsError;
  if (std::unique_ptr<substance::Package> package =
          substance::Package::load(sbsar, &sbsError)) {
    substance::Graph& graph = package->graph(0);
    graph.setResolution(9, 9);
    graph.set("$normalformat", 1.0f);  // OpenGL
    const auto leaves = [&](float season, float density) {
      graph.set("Season", season);
      graph.set("Density", density);
      graph.render();
      world::Material m = world::textures::material(
          graph.outputsByUsage(), world::Material{}, graph.normalsAreDirectX());
      m.roughness = 0.75f;
      m.metallic = 0;
      m.normalScale = 1.4f;
      return m;
    };
    add(geometry::mesh::superellipsoid({150, 150, 150}, 2, 96, 64),
        geometry::space::place({0, 20, 0}), leaves(0.5f, 0.7f));
    // ...and a leaning panel wearing the late-season cook, tiled twice.
    world::Material late = leaves(1.0f, 1.0f);
    late.uvScale = {2, 2};
    if (dark) {
      // In the dark the panel is backlit by its own base colour: the
      // same image in the emissive slot, warm and dim.
      late.emissiveMap = late.texture;
      late.emissive = {0.9f, 0.55f, 0.3f, 1};
      late.emissiveStrength = 1.4f;
    }
    add(geometry::mesh::quad(560, 340), geometry::space::place({0, -30, 330}, 0, -62),
        late);
    std::printf("substance: %s rendered (%s)\n", graph.label().c_str(),
                substance::Package::engineVersion().c_str());
  } else {
    std::fprintf(stderr, "substance: %s\n", sbsError.c_str());
  }
#endif

  // The emissive slot on its own: a dark, rough sphere whose emissive
  // MAP is a drawn circuit — bright traces on black — tinted amber by the
  // emissive colour and pushed past 1 by the strength, under no light of
  // its own. Where the map is black nothing glows; the map is the shape,
  // the colour the hue, the strength the intensity.
  {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256, 256));
    SkCanvas* c = surface->getCanvas();
    c->clear(SK_ColorBLACK);
    SkPaint trace;
    trace.setAntiAlias(true);
    trace.setColor(SK_ColorWHITE);
    trace.setStyle(SkPaint::kStroke_Style);
    trace.setStrokeWidth(5);
    trace.setStrokeCap(SkPaint::kRound_Cap);
    for (int i = 0; i < 6; ++i) {
      const float y = 24 + (float)i * 40;
      SkPathBuilder path;
      path.moveTo(0, y);
      path.lineTo(60 + (float)(i % 3) * 30, y);
      path.lineTo(90 + (float)(i % 3) * 30, y + 20);
      path.lineTo(256, y + 20);
      c->drawPath(path.detach(), trace);
    }
    SkPaint pad;
    pad.setAntiAlias(true);
    pad.setColor(SK_ColorWHITE);
    for (int i = 0; i < 6; ++i)
      c->drawCircle(60 + (float)(i % 3) * 30, 24 + (float)i * 40, 9, pad);
    world::Material circuit;
    circuit.baseColor = {0.03f, 0.03f, 0.035f, 1};
    circuit.roughness = 0.9f;
    circuit.emissiveMap = surface->makeImageSnapshot();
    circuit.emissive = {1.0f, 0.5f, 0.12f, 1};
    circuit.emissiveStrength = 3.5f;
    circuit.uvScale = {4, 2};
    circuit.tile = true;
    add(geometry::mesh::superellipsoid({110, 110, 110}, 2, 96, 64),
        geometry::space::place({-460, 250, -320}, 20), circuit);
    if (dark) {
      // Emission does not light its neighbours by itself; in the dark a
      // dim point light of the same hue sits inside the sphere so the
      // props around it pick up its glow.
      world::LightComponent glow;
      glow.type = world::LightComponent::Type::Point;
      glow.position = {-460, 250, -320};
      glow.color = {1.0f, 0.5f, 0.12f, 1};
      glow.intensity = 2.2f;
      glow.range = 900;
      w->addLight(glow);
    }
  }

  // Glass: a clear sphere and a frosted pane, both transmission 1 —
  // the sphere bends what is behind it (ior 1.5, a thick slab), the
  // pane is thin, faintly blue and rough enough to blur.
  {
    world::Material clear;
    clear.baseColor = {0.98f, 0.99f, 1.0f, 1};
    clear.roughness = 0.04f;
    clear.transmission = 1;
    clear.ior = 1.5f;
    clear.thickness = 140;
    add(geometry::mesh::superellipsoid({120, 120, 120}, 2, 96, 64),
        geometry::space::place({-150, 40, 260}), clear);
    world::Material frosted;
    frosted.baseColor = {0.85f, 0.93f, 1.0f, 1};
    frosted.roughness = 0.45f;
    frosted.transmission = 1;
    frosted.ior = 1.45f;
    frosted.thickness = 12;
    add(geometry::mesh::quad(300, 210), geometry::space::place({120, 300, -260}, -12),
        frosted);

    // Fluted (reeded) glass: refraction goes through the shaded normal,
    // so a normal map of vertical half-cylinders ribbons whatever is
    // behind the pane — no geometry. Drawn here: each flute's normal
    // swings from -60 to +60 degrees across its width (red encodes x,
    // blue z; green stays flat), OpenGL convention. A faint warm
    // emission makes it edge-lit glass, which is why it still reads in
    // the dark shot.
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256, 4));
    {
      SkCanvas* c = surface->getCanvas();
      const int flute = 32;
      for (int x = 0; x < 256; ++x) {
        const float u =
            ((float)(x % flute) + 0.5f) / (float)flute * 2.0f - 1.0f;
        const float angle = u * 60.0f * (float)M_PI / 180.0f;
        const float nx = std::sin(angle), nz = std::cos(angle);
        SkPaint p;
        p.setColor(SkColorSetARGB(255, (U8CPU)(nx * 127.5f + 127.5f), 128,
                                  (U8CPU)(nz * 127.5f + 127.5f)));
        c->drawRect(SkRect::MakeXYWH((float)x, 0, 1, 4), p);
      }
    }
    world::Material fluted;
    fluted.baseColor = {0.96f, 0.98f, 1.0f, 1};
    fluted.roughness = 0.12f;
    fluted.transmission = 1;
    fluted.ior = 1.5f;
    fluted.thickness = 30;
    fluted.normalMap = surface->makeImageSnapshot();
    fluted.normalScale = 1;
    fluted.uvScale = {2, 1};
    fluted.tile = true;
    fluted.emissive = {1.0f, 0.75f, 0.45f, 1};
    fluted.emissiveStrength = 0.12f;
    add(geometry::mesh::quad(300, 230), geometry::space::place({-380, 190, -40}, 26),
        fluted);
  }

  // 3. An imported model wearing the material its file carries: base
  // colour, normal, packed metallicRoughness and occlusion, decoded from
  // the bytes the importer kept.
  if (std::optional<geometry::import::Model> avocado =
          geometry::import::model(assetDir / "models/Avocado.glb")) {
    const auto decodeBytes = [](const std::vector<std::byte>& bytes,
                                std::string_view hint) -> sk_sp<SkImage> {
      std::optional<image::ImageAsset> asset = image::decodeImage(
          bytes.data(), bytes.size(), {}, std::filesystem::path(hint));
      return asset ? asset->frameAt(0).image : nullptr;
    };
    // Merged into one mesh, placed with the file's material slots — the
    // per-triangle "Material" lane picks each triangle's.
    const glm::mat4 fit = avocado->fitTransform(260);
    const std::vector<world::Material> slots =
        world::textures::materials(*avocado, decodeBytes);
    add(avocado->merged(), geometry::space::place({420, -60, 320}, 30) * fit,
        slots);
    std::printf("avocado: %zu parts, %zu material slots\n",
                avocado->parts.size(), slots.size());
  }

  // 4. Scalar reference materials on the back row.
  world::Material rubber;
  rubber.baseColor = {0.85f, 0.2f, 0.15f, 1};
  rubber.roughness = 0.9f;
  add(geometry::mesh::superellipsoid({90, 90, 90}, 2, 64, 48),
      geometry::space::place({-200, 260, -300}), rubber);
  world::Material chrome;
  chrome.baseColor = {0.95f, 0.97f, 1.0f, 1};
  chrome.metallic = 1;
  chrome.roughness = 0.08f;
  add(geometry::mesh::superellipsoid({90, 90, 90}, 2, 64, 48),
      geometry::space::place({200, 260, -300}), chrome);

  geometry::space::Camera camera;
  camera.eye = {60, 330, 1150};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 44;
  w->setCamera(camera);
  const char* name = dark ? "world_materials_dark.png" : "world_materials.png";
  if (w->render() && w->savePng(outDir / name)) std::printf("wrote %s\n", name);
#ifdef SIGIL_WORLD_DEMO_USD
  // The lit lab as a USD crate: every prop with its slots and materials
  // (images written beside it), the sun and a light, the camera.
  if (!dark) {
    usd::Writer writer(outDir / "world_materials.usdc");
    for (const Placed& p : placed)
      writer.mesh(p.name, p.mesh, p.model, p.slots);
    writer.sun("sun", lighting);
    writer.camera("camera", camera);
    std::string usdError;
    if (writer.save(&usdError))
      std::printf("wrote world_materials.usdc\n");
    else
      std::fprintf(stderr, "usd: %s\n", usdError.c_str());
  }
#endif
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-') {
    const bool help =
        std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0;
    std::fprintf(help ? stdout : stderr,
                 "usage: world_demo [outdir] [assetdir]\n");
    return help ? 0 : 1;
  }
  const std::filesystem::path outDir = argc > 1 ? argv[1] : "world_demo_out";
  const std::filesystem::path assetDir = argc > 2 ? argv[2] : "assets";
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  renderMaterialLab(outDir, assetDir, /*dark=*/false);
  renderMaterialLab(outDir, assetDir, /*dark=*/true);
  renderPopLab(outDir, assetDir);

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
    geometry::Mesh slab = geometry::mesh::superellipsoid({900, 24, 620}, 8, 64, 24);
    w->place(slab, glm::translate(glm::mat4(1.0f), {0, -190, 0}), floor);
  }

  // Three self-lit UI cards, arranged on an arc facing the viewer.
  {
    world::Material screen;
    screen.unlit = true;
    screen.texture = uiCard(512, 340, {0.25f, 0.85f, 1.0f, 1}, 0.72f);
    w->place(geometry::mesh::quad(380, 252),
             geometry::space::place({-420, 60, -40}, 30), screen);
    screen.texture = uiCard(512, 340, {1.0f, 0.62f, 0.22f, 1}, 0.45f);
    w->place(geometry::mesh::quad(380, 252),
             geometry::space::place({0, 70, 30}, 0, -4), screen);
    screen.texture = uiCard(512, 340, {0.72f, 0.5f, 1.0f, 1}, 0.9f);
    w->place(geometry::mesh::quad(380, 252),
             geometry::space::place({420, 55, -40}, -30), screen);
  }

  // Curved ticker panel below the cards.
  {
    world::Material screen;
    screen.unlit = true;
    screen.texture = uiCard(1024, 220, {0.3f, 1.0f, 0.6f, 1}, 0.6f);
    w->place(geometry::mesh::cylinderPanel(880, 170, 560, 64, 12),
             geometry::space::place({0, -96, 90}, 0, 8), screen);
  }

  // Props: gold star (extruded), chrome blob, glass pane.
  {
    world::Material gold;
    gold.baseColor = {1.0f, 0.78f, 0.34f, 1};
    gold.metallic = 1;
    gold.roughness = 0.3f;
    geometry::Mesh star = geometry::mesh::extrude(starPath(5, 95, 44), {.depth = 34});
    w->place(star, geometry::space::place({-560, 280, -220}, 36, -10), gold);

    world::Material chrome;
    chrome.baseColor = {0.95f, 0.97f, 1.0f, 1};
    chrome.metallic = 1;
    chrome.roughness = 0.08f;
    w->place(geometry::mesh::superellipsoid({110, 95, 80}, 2.4f, 64, 48),
             geometry::space::place({590, 300, -200}, 15, 0, -6), chrome);

    world::Material glass;
    glass.baseColor = {0.75f, 0.9f, 0.95f, 0.32f};
    glass.metallic = 0;
    glass.roughness = 0.05f;
    w->place(geometry::mesh::quad(360, 240),
             geometry::space::place({210, 40, 150}, -12, -3), glass);
  }

  // An optional poster, decoded from SVG at panel resolution through
  // SigilLoader. Present only when the asset directory has been
  // populated; otherwise the scene simply omits it.
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
      w->place(geometry::mesh::quad(300, 300),
               geometry::space::place({-780, 60, 120}, 42), poster);
      std::printf("tiger poster: %dx%d\n", poster.texture->width(),
                  poster.texture->height());
    }
  }

  // The stream: a spline crossing the space above the set, carrying a
  // chrome wire and camera-facing UI cards at its arc-length stations.
  // Declared through the scene layer — describe and reconcile — rather
  // than through imperative place calls.
  //
  // The cards are live billboards: every shot re-describes them through
  // space::faceCamera() against that shot's actual camera eye, so the
  // reconciler sees transform-only changes and each card costs a
  // setTransform rather than a re-upload. Baking the facing into one
  // merged mesh instead would freeze the billboards toward whichever eye
  // was used when the mesh was built.
  world::scene::Scene stream(*w);
  std::function<world::scene::Scene::Stats(glm::vec3)> faceStream;
  geometry::Spline3 arc;
  arc.points = {
      {-820, 260, -320}, {-300, 420, 60}, {260, 300, 220}, {820, 430, -260}};
  {
    const geometry::Cloud stations = geometry::points::onSpline(arc, 9);

    world::Material wireMat;
    wireMat.baseColor = {0.9f, 0.93f, 1.0f, 1};
    wireMat.metallic = 1;
    wireMat.roughness = 0.15f;

    // The wire carries a baked colour lane, cool at the start and warm
    // by the end. A tube's rings are generated in order along the curve,
    // so ramping by vertex index ramps along the curve.
    geometry::Mesh wire =
        geometry::curves::tube(arc, {.radius = 7, .segments = 180, .sides = 10});
    wire.colors.resize(wire.positions.size());
    for (size_t i = 0; i < wire.positions.size(); ++i) {
      const float f = wire.positions.size() > 1
                          ? (float)i / (float)(wire.positions.size() - 1)
                          : 0.0f;
      wire.colors[i] = {0.75f + 0.25f * f, 0.9f - 0.35f * f, 1.0f - 0.25f * f,
                        1};
    }
    world::Material cardMat;
    cardMat.unlit = true;
    cardMat.texture = uiCard(384, 256, {0.45f, 0.95f, 0.85f, 1}, 0.62f);
    cardMat.baseColor = {1, 1, 1, 0.92f};

    // The wire mesh is held by shared_ptr so every re-describe presents
    // the same pointer and keeps its surface; the card quads are
    // identity-stable through the Scene's per-size quad cache. Together
    // that makes re-facing the stream cost one setTransform per card and
    // nothing else.
    auto wireMesh = std::make_shared<const geometry::Mesh>(std::move(wire));
    faceStream = [&stream, wireMesh, wireMat, cardMat,
                  positions = stations.positions](glm::vec3 eye) {
      world::scene::Node root = world::scene::group().key("stream");
      root.child(world::scene::place(wireMesh, wireMat).key("wire"));
      for (size_t i = 0; i < positions.size(); ++i)
        root.child(world::scene::panel(cardMat.texture, 170, 112)
                       .material(cardMat)
                       .key("card" + std::to_string(i))
                       .transform(geometry::space::faceCamera(eye, positions[i])));
      return stream.render(root);
    };
    // A first describe, so the surfaces exist before any shot runs; the
    // eye here matches the stream shot's.
    faceStream({0, 200, 1150});
  }

  // The set dressing, declared through the world easel (Easel.h): two
  // coloured point lights pooling on the floor by the props, and stamps
  // of sparks riding the stream arc, GPU-instanced as one draw call with
  // tint ramping along the "t" lane and size varying through the scale
  // lane.
  {
    geometry::Cloud sparks = geometry::points::onSpline(arc, 3000);
    geometry::points::jitter(sparks, 30, 11);
    geometry::points::displaceNoise(sparks, 70, 0.006f, 12);
    const std::vector<float>& t = sparks.scalar("t");
    std::vector<glm::vec4>& tint = sparks.color("tint");
    std::vector<float>& size = sparks.scalar("size", 1);
    for (size_t i = 0; i < sparks.size(); ++i) {
      const float f = t[i];
      tint[i] = {0.45f + 0.55f * f, 0.95f - 0.55f * f, 1.0f - 0.05f * f, 1};
      size[i] = 0.55f + 0.75f * (0.5f + 0.5f * std::sin(f * 61.0f));
    }
    world::Material sparkMat;
    sparkMat.unlit = true;
    sparkMat.baseColor = {1, 1, 1, 0.85f};  // alpha < 1: the blended pass
    world::StampLanes sparkLanes;
    sparkLanes.tintLane = "tint";
    sparkLanes.scaleLane = "size";

    world::easel::Stage dressing = world::easel::stage(*w);
    dressing.light({-520, 60, -80}, {1.0f, 0.25f, 0.85f, 1}, 7, 760)
        .light({540, 80, -50}, {0.2f, 0.85f, 1.0f, 1}, 7, 760)
        .placeStamps(std::move(sparks), geometry::mesh::quad(6, 6), sparkMat,
                     sparkLanes)
        .key("sparks");
    const world::scene::Scene::Stats stats = dressing.commit();
    std::printf("easel dressing: %d added\n", stats.added);
  }

  // The ribbon, painted end to end: a loose ball winding that wraps the
  // whole scene, carrying the strip artwork above along its entire
  // length. One surface per GPU tile, and each frame every arc re-sweeps
  // one step forward so the whole canvas marches around the winding
  // behind the chrome dart.
  std::vector<uint32_t> stripIds;  // one surface per strip tile
  uint32_t dartId = 0, cometId = 0, guideId = 0;
  const float kCometSpan = 0.34f;
  world::World::pop::Chain guideChain;
  geometry::Spline3 flightLoop;
  float bandWidth = 300;             // recomputed from the strip's density
  const int kTiles = 10;             // GPU tiles the vector strip slices to
  const int kSectionsPerTile = 200;  // ribbon cross-sections per arc
  const float kFlagHome = 0.91f;     // puts the strip's start on a front
                                     // pass in the still shots
  {
    flightLoop.closed = true;
    const int kKnots = 96;
    // Coprime, so successive wraps land beside each other instead of
    // retracing the same great circle.
    const float kWraps = 7;    // latitude oscillations
    const float kPrecess = 2;  // turns of the winding plane
    const float kTilt = 1.0f;  // latitude amplitude, radians
    const glm::vec3 shell = {1250, 620, 950};
    const glm::vec3 center = {0, 380, 0};
    for (int i = 0; i < kKnots; ++i) {
      const float t = (float)i / (float)kKnots;
      const float lat = kTilt * std::sin(2.0f * (float)M_PI * kWraps * t);
      // Negative, so the winding faces outward at the front of the ball.
      const float azi = -2.0f * (float)M_PI * kPrecess * t;
      flightLoop.points.push_back(
          {center.x + shell.x * std::cos(lat) * std::cos(azi),
           center.y + shell.y * std::sin(lat),
           center.z + shell.z * std::cos(lat) * std::sin(azi)});
    }
    float loopLen = 0;
    {
      geometry::Cloud rail = geometry::points::onSpline(flightLoop, 1024);
      for (size_t i = 1; i < rail.size(); ++i)
        loopLen += glm::length(rail.positions[i] - rail.positions[i - 1]);
    }

    sigil::weave::FontContext fonts(sigil::weave::ports::systemFontManager());

    // The band's world width is DERIVED from the strip's pixel density
    // rather than chosen, which keeps the texels square: the strip's
    // total pixel length over the loop's world length gives px-per-unit,
    // and the column's pixel width divided by that is how wide the band
    // stands on the winding.
    const StripArt strip = yarnStrip(fonts, kTiles, 4096, 506);
    const float pxPerWu = strip.totalAlongPx / loopLen;
    bandWidth = strip.acrossPx / pxPerWu;
    // Each arc is a GPU sweep: the loop's control points live in a
    // device buffer and a compute pass writes the ribbon's vertices, so
    // no CPU mesh for the band exists at all.
    for (int k = 0; k < kTiles; ++k) {
      world::Material segment;
      segment.unlit = true;
      segment.texture = strip.tiles[(size_t)k];
      segment.baseColor = {1, 1, 1, 0.98f};  // alpha < 1: the blended pass
      world::World::SweepDesc arc;
      arc.loop = flightLoop.points;
      arc.width = bandWidth;
      arc.sections = kSectionsPerTile;
      arc.head = wrap01(kFlagHome + (float)(k + 1) / (float)kTiles);
      arc.span = 1.0f / (float)kTiles;
      stripIds.push_back(w->placeSweep(arc, segment));
    }

    world::Material chromeDart;
    chromeDart.baseColor = {0.92f, 0.95f, 1.0f, 1};
    chromeDart.metallic = 1;
    chromeDart.roughness = 0.12f;
    const std::vector<glm::vec2> dartProfile = {
        {0, 95}, {26, 30}, {34, -20}, {18, -52}, {0, -60}};
    dartId = w->place(geometry::mesh::revolve(dartProfile),
                      dartTransform(flightLoop, kFlagHome), chromeDart);

    // The comet, COMPOSED ON DEVICE: a small guide chain rides the
    // ribbon's window, and the comet chain rides the guide — its
    // generator reads the guide's cooked lanes directly. Animating the
    // guide costs two floats and cascades through the comet in compute;
    // the CPU never touches a point of either.
    guideChain = geometry::pop::on(flightLoop.points)
                     .count(64)
                     .window(kFlagHome, kCometSpan)
                     .noise(26, 0.003f)
                     .smooth(0.5f, 2);
    world::Material guideMat;
    guideMat.unlit = true;
    guideMat.baseColor = {0.5f, 0.9f, 0.8f, 0.25f};  // faint beads
    guideId = w->placeChain(geometry::mesh::quad(4, 4), guideChain, guideMat);
    const world::World::pop::Chain cometChain =
        geometry::pop::on(std::vector<glm::vec3>{})  // loop comes from the guide
            .count(300000)
            // Just short of the whole guide, to skip the segment that
            // closes the loop back on itself.
            .window(0.984f, 0.984f)
            .spread(48)
            .noise(20, 0.004f)
            .vary(0.5f)
            .fade({1.0f, 0.45f, 0.85f, 0.0f},   // tail
                  {0.65f, 0.95f, 1.0f, 0.5f});  // head
    world::Material sparkle;
    sparkle.unlit = true;
    sparkle.baseColor = {1, 1, 1, 0.8f};  // blended
    cometId = w->placeChainOn(guideId, geometry::mesh::quad(2.6f, 2.6f),
                              cometChain, sparkle);
    std::printf(
        "comet: %d GPU particles riding a %d-point guide "
        "chain, composed on device\n",
        std::get<geometry::pop::SplineScatter>(cometChain[0]).count,
        std::get<geometry::pop::SplineScatter>(guideChain[0]).count);

    std::printf(
        "yarn: %.0f wu wound, band %.0f wu wide, %d tiles of "
        "%dx%d (%.0fk px of unique canvas)\n",
        loopLen, bandWidth, kTiles, strip.tiles[0]->width(),
        strip.tiles[0]->height(), strip.totalAlongPx / 1000);
  }

  world::Lighting lighting;
  lighting.sunDirection = {-0.4f, -0.8f, -0.45f};
  lighting.sunIntensity = 2.4f;
  lighting.skyColor = {0.38f, 0.46f, 0.68f, 1};
  lighting.groundColor = {0.08f, 0.07f, 0.1f, 1};
  w->setLighting(lighting);

  struct Shot {
    const char* name;
    geometry::space::Camera camera;
  };
  Shot shots[6];
  shots[5].name = "world_marquee.png";
  shots[5].camera.eye = {180, 520, 2400};
  shots[5].camera.target = {-40, 380, 0};
  shots[5].camera.fovYDeg = 46;
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
  for (const Shot& shot : shots) {
    // Re-face the stream's billboards toward THIS shot's eye. It must
    // reconcile as transform-only: the cards move, the wire is kept, and
    // nothing is added or removed.
    const world::scene::Scene::Stats faced = faceStream(shot.camera.eye);
    if (faced.added + faced.removed != 0)
      std::fprintf(stderr,
                   "stream re-face re-uploaded: +%d -%d (expected "
                   "transform-only)\n",
                   faced.added, faced.removed);
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
  // --- reading GPU-cooked points back out ---------------------------------
  // The comet's particles exist only as GPU lanes, cooked by compute and
  // never touched by the CPU. readChain() pulls those lanes back as a
  // Cloud, and save::ply() writes a binary PLY that a DCC application
  // opens directly — positions plus the scalar, vector and colour lanes
  // the chain named, all riding along.
  if (cometId) {
    const geometry::Cloud comet = w->readChain(cometId);
    const auto file = outDir / "comet_points.ply";
    if (geometry::save::ply(file, comet, {.binary = true}))
      std::printf(
          "comet_points.ply: %zu GPU-cooked points exported "
          "(binary_little_endian, %.1f MB), "
          "%zu scalar / %zu vector / %zu color lanes\n",
          comet.size(), (double)std::filesystem::file_size(file) / 1e6,
          comet.scalars.size(), comet.vectors.size(), comet.colors.size());
  }

  // --- the flight, and its timings ----------------------------------------
  // Each frame slides every strip arc one step forward along the
  // winding, so the whole painted ribbon marches behind the dart. Timed
  // two ways, because they answer different questions: submit-and-flush
  // throughput, drained once at the end, and fully synced frames that
  // read the GPU back every time.
  if (!stripIds.empty()) {
    w->setCamera(shots[5].camera);
    // One full circumnavigation per 2400 frames.
    const auto animate = [&](int frame) {
      const float shift = wrap01(kFlagHome + (float)frame / 2400.0f);
      for (int k = 0; k < kTiles; ++k)
        w->setSweepWindow(stripIds[(size_t)k],
                          wrap01(shift + (float)(k + 1) / (float)kTiles),
                          1.0f / (float)kTiles);
      w->setTransform(dartId, dartTransform(flightLoop, shift));
      // Two floats on the GUIDE only; the comet re-cooks by cascade.
      w->setChainWindow(guideId, shift, kCometSpan);
    };

    const int kTimed = 240;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kTimed; ++frame) {
      animate(frame);
      w->render();
    }
    // Drain the queue, so the mean below covers every submitted frame
    // rather than stopping while work is still in flight.
    w->readback();
    const auto flushed = std::chrono::steady_clock::now();

    const int kSynced = 30;
    for (int frame = 0; frame < kSynced; ++frame) {
      animate(frame);
      w->render();
      w->readback();
    }
    const auto synced = std::chrono::steady_clock::now();

    const double renderMs =
        std::chrono::duration<double, std::milli>(flushed - start).count() /
        kTimed;
    const double syncedMs =
        std::chrono::duration<double, std::milli>(synced - flushed).count() /
        kSynced;
    std::printf(
        "flight: %.2f ms/frame submitted+flushed (%.0f fps), "
        "%.2f ms/frame with GPU readback (%.0f fps) — the re-sweeps "
        "run as compute on the GPU\n",
        renderMs, 1000.0 / renderMs, syncedMs, 1000.0 / syncedMs);

    int flightWritten = 0;
    for (int i = 0; i < 6; ++i) {
      animate(i * 400);  // six evenly spaced stations around one lap
      char name[40];
      std::snprintf(name, sizeof(name), "world_marquee_flight_%d.png", i);
      if (w->render() && w->savePng(outDir / name)) ++flightWritten;
    }
    std::printf("flight frames: %d/6\n", flightWritten);

    // Optional third argument: a frame count, which dumps a continuous
    // numbered sequence suitable for encoding into a video.
    const int animFrames = argc > 3 ? std::atoi(argv[3]) : 0;
    int animWritten = 0;
    for (int i = 0; i < animFrames; ++i) {
      animate(i * 4);  // four loop-steps per frame: one lap in 600
      char name[40];
      std::snprintf(name, sizeof(name), "world_anim_%04d.png", i);
      if (w->render() && w->savePng(outDir / name)) ++animWritten;
    }
    if (animFrames > 0)
      std::printf("anim frames: %d/%d\n", animWritten, animFrames);
  }

  // --- the DECLARED camera: a flight path and a wiggled lane --------------
  // Everything above aims the camera imperatively, with setCamera() and
  // two vectors per shot, which is still the right tool for a still.
  // This shot uses the declared door instead, and it lives here rather
  // than in a live-coding sketch because that host has no GPU device.
  //
  //  · An AnimatedCamera needs no special home: a camera is already a
  //    registry entity, and an ACTIVE CameraComponent outranks
  //    World::setCamera while it exists.
  //  · CameraPath flies the EYE along a spline. The lane is `t`, where
  //    along the curve, so the bind() chain shapes the SCHEDULE while
  //    the curve supplies the shape. A non-zero lookAhead is the
  //    caller's way of saying "aim it for me", so the framing follows
  //    the tangent and the target lanes are ignored outright.
  //  · rollDeg is the one wiggled lane, a handheld dutch tilt. wiggle()
  //    reads NO CLOCK — it is a pure function of the normalised input —
  //    so this frame depends on the frame index and nothing else, which
  //    is what lets a wiggled artifact be byte-reproducible at all.
  //
  // The camera entity is created only after every artifact above is on
  // disk, so an active camera cannot reframe a shot already taken. This
  // PNG is deliberately outside the `written`/`total` count.
  {
    entt::registry& registry = w->registry();
    const entt::entity cam = registry.create();
    registry.emplace<world::CameraComponent>(cam);

    // A closed loop threaded THROUGH the set: dive past the panels,
    // sweep the poster wall, climb out over the ribbon ball. Aiming down
    // the tangent only frames anything if the curve goes somewhere.
    geometry::Spline3 flight;
    flight.closed = true;
    flight.points = {{1650, 520, 1450},   {320, 150, 780},   {-1080, 360, 340},
                     {-1500, 880, -1050}, {180, 1020, -760}, {1700, 700, -260}};

    // The caller owns the clock; world has none.
    motion::Ticker ticker;
    choreograph::Output<float> along{0.0f};
    ticker.timeline().apply(&along).then<choreograph::RampTo>(1.0f, 8.0f);

    world::AnimatedCamera& lens = registry.emplace<world::AnimatedCamera>(cam);
    lens.fovYDeg = 54.0f;
    world::CameraPath& path = lens.path.emplace();
    path.path = flight;
    path.t = world::bind(&along)
                 .map(&choreograph::easeInOutQuad)  // eased flight…
                 .target(0.0f, 1.0f);               // …one lap of the loop
    path.lookAhead = 0.05f;                         // aim down the tangent
    lens.rollDeg = world::wiggle(&along, 3.5f, 24.0f, 5, 2);

    const int kCameraFrame = 140;  // part-way into the eased lap
    for (int i = 0; i < kCameraFrame; ++i) ticker.tick(1.0 / 60.0);
    // Resolve the lanes first so the flown eye can be read back, then
    // face the stream's billboards at it — the same per-frame
    // re-describe a live loop would run. render()'s own resolve then
    // finds every lane already parked and writes nothing.
    world::resolveAnimation(*w);
    faceStream(registry.get<world::CameraComponent>(cam).camera.eye);
    if (w->render() && w->savePng(outDir / "world_camera_flight.png")) {
      const geometry::space::Camera& c =
          registry.get<world::CameraComponent>(cam).camera;
      std::printf(
          "camera flight: t=%.3f -> eye (%.0f %.0f %.0f) aimed down "
          "the tangent, roll wiggled +-3.5 deg at 24 Hz\n",
          along.value(), c.eye.x, c.eye.y, c.eye.z);
    } else {
      std::fprintf(stderr, "write failed: world_camera_flight.png\n");
    }
  }

  std::printf("wrote %d/%d shots to %s\n", written, total,
              outDir.string().c_str());
  return written == total ? 0 : 1;
}
