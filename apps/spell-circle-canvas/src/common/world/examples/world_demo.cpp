// Headless SigilWorld scene: diegetic UI panels and SigilShape
// procedural props rendered by Diligent (Vulkan/MoltenVK), one PNG per
// camera angle.
// Usage: world_demo [outdir] [assetdir]  (defaults world_demo_out,
// assets). With fetch_assets run, the tiger SVG lands on a poster panel
// through SigilLoader's SVG decode; without it the scene simply omits
// the poster.

#include "sigilworld/Components.h"
#include "sigilworld/Easel.h"
#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

#include <sigilimage/ImageAsset.h>
#include <sigilloader/Loader.h>
#include <sigilshape/Curves.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>
#include <sigilshape/Save.h>

#include <sigilcompose/Compose.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweavekit/SigilWeaveKit.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSurface.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <vector>

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

// --- the marquee's type ---------------------------------------------------
// THE YARN, fully painted, PERPENDICULAR orientation, FILLED end to
// end: the entire ball winding is the ribbon, its full length ONE
// SigilCompose COLUMN — every line of type reads ACROSS the band's
// width and the stack advances ALONG the winding. The column is
// packed with numbered SECTORS: a big numeral, a narrow-column
// paragraph, and a graphics stretch (ruler / waveform / swatch run /
// dot ellipsis, each parameterized by its sector index so no two
// render alike), with only thin grow gaps between — no empty
// stretches anywhere on the loop. Snapshotted ONCE as a vector
// SkPicture, SLICED straight down into GPU tiles (texture x = u,
// y = v — no transpose), drawn mirrored in x so the wall's u-mapping
// restores unmirrored glyphs. Arcs share boundary texels: seamless.
struct StripArt {
  std::vector<sk_sp<SkImage>> tiles;
  float acrossPx = 0; ///< column width = the band's texel width
  float totalAlongPx = 0;
};

StripArt yarnStrip(sigil::weave::FontContext &fonts, int tileCount,
                   int tileAlongPx, int acrossPx) {
  namespace sc = sigil::compose;
  namespace weave = sigil::weave;
  const SkColor kInk = SkColorSetARGB(255, 236, 244, 254);
  const SkColor kAccent = SkColorSetARGB(255, 116, 224, 190);
  const SkColor kDim = SkColorSetARGB(255, 158, 176, 202);
  const float total = (float)tileCount * (float)tileAlongPx;

  const auto para = [](const char8_t *string, float size, SkColor color) {
    auto p = std::make_shared<weave::Paragraph>();
    p->appendText(string, weave::kit::makeStyle(size, color));
    return p;
  };
  weave::ParagraphLayoutOptions centered;
  centered.alignment = weave::TextAlignment::kCenter;
  const auto label = [&](std::u8string string, float size,
                         SkColor color) {
    auto p = std::make_shared<weave::Paragraph>();
    p->appendText(std::move(string), weave::kit::makeStyle(size, color));
    return sc::text(p, centered);
  };

  // Sector graphics, parameterized so no two stretches render alike.
  const auto ruler = [&](int s) {
    sc::Element row = sc::box().row().gap(8.0f + (float)(s % 3) * 3.0f)
                          .alignItems(sc::Align::Center)
                          .alignSelf(sc::Align::Center)
                          .height(150);
    const int major = 5 + s % 4;
    for (int i = 0; i < 30 + (s % 3) * 5; ++i)
      row.child(sc::box()
                    .width(i % major == 0 ? 5.0f : 3.0f)
                    .height(i % major == 0 ? 140.0f : 60.0f)
                    .fill(sc::Fill::color(
                        {0.58f, 0.66f, 0.77f,
                         i % major == 0 ? 0.95f : 0.5f})));
    return row;
  };
  const auto wave = [&](int s) {
    sc::Element row = sc::box().row().gap(5).alignItems(sc::Align::End)
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
                    .fill(sc::Fill::color({0.455f, 0.878f, 0.745f,
                                           0.45f + 0.55f * beat})));
    }
    return row;
  };
  const auto swatches = [&](int s) {
    sc::Element row =
        sc::box().row().gap(13).alignSelf(sc::Align::Center);
    for (int i = 0; i < 12; ++i) {
      const float f = (float)((i + s * 5) % 12) / 11.0f;
      row.child(sc::box().width(24).height(24).corners({12}).fill(
          sc::Fill::color(
              {0.4f + 0.45f * f, 0.878f, 0.745f, 0.35f + 0.65f * f})));
    }
    return row;
  };
  const auto dots = [&](int s) {
    sc::Element row = sc::box().row().gap(15).alignItems(sc::Align::Center)
                          .alignSelf(sc::Align::Center)
                          .height(60);
    for (int i = 0; i < 14; ++i) {
      const float f =
          0.5f + 0.5f * std::sin((float)(i + s * 3) * 0.55f);
      row.child(
          sc::box()
              .width(10 + 22 * f)
              .height(10 + 22 * f)
              .corners({(10 + 22 * f) * 0.5f})
              .fill(sc::Fill::color(
                  {0.72f, 0.82f, 0.95f, 0.3f + 0.7f * f})));
    }
    return row;
  };

  const char8_t *pool[10] = {
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
      u8"geometry by SigilShape, type by SigilWeave, authored in "
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
  const int kSectors = 44; // fills ~90% of the column; thin grow gaps
                           // absorb the rest evenly
  for (int s = 0; s < kSectors; ++s) {
    root.child(sc::box().grow());
    char numeral[8];
    std::snprintf(numeral, sizeof(numeral), "%02d", s + 1);
    root.child(label(std::u8string(u8"— ") +
                         (const char8_t *)numeral + u8" —",
                     64, kDim));
    root.child(label(pool[s % 10], 44,
                     s % 10 == 4 || s % 10 == 9 ? kDim : kInk));
    switch (s % 4) {
    case 0: root.child(ruler(s)); break;
    case 1: root.child(wave(s)); break;
    case 2: root.child(swatches(s)); break;
    default: root.child(dots(s)); break;
    }
  }
  root.child(sc::box().grow());
  root.child(label(u8"— and back to its own beginning", 72, kAccent));

  const sk_sp<SkPicture> art = sc::snapshot(root, fonts);
  StripArt out;
  out.acrossPx = (float)acrossPx;
  out.totalAlongPx = total;
  for (int k = 0; k < tileCount; ++k) {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(acrossPx, tileAlongPx));
    SkCanvas *canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    // Mirror in x (the wall's u-mapping mirrors back), then step to
    // this tile's window of the column.
    canvas->translate((float)acrossPx, 0);
    canvas->scale(-1, 1);
    canvas->translate(0, -(float)k * (float)tileAlongPx);
    canvas->drawPicture(art);
    out.tiles.push_back(surface->makeImageSnapshot());
  }
  return out;
}

float wrap01(float t) { return t - std::floor(t); }

/** The dart that tows the flag: revolve's +y nose aimed down the
 *  flight tangent, keeping the flag's own up convention. */
glm::mat4 dartTransform(const sigil::shape::Spline3 &loop, float head) {
  const glm::vec3 p = loop.position(wrap01(head));
  const glm::vec3 ahead = loop.position(wrap01(head + 0.004f));
  const glm::vec3 behind = loop.position(wrap01(head - 0.004f));
  glm::vec3 t = ahead - behind;
  const float len = glm::length(t);
  t = len > 1e-6f ? t * (1.0f / len) : glm::vec3{1, 0, 0};
  glm::vec3 n =
      glm::vec3{0, 0, 1} - t * glm::dot(t, glm::vec3{0, 0, 1});
  const float nLen = glm::length(n);
  n = nLen > 1e-6f ? n * (1.0f / nLen) : glm::vec3{0, 1, 0};
  const glm::vec3 b = glm::cross(t, n);
  // Basis columns (binormal, tangent, normal | position) — glm's
  // constructor takes columns directly.
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
    w->addSurface(slab, glm::translate(glm::mat4(1.0f), {0, -190, 0}),
                  floor);
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
  shape::Spline3 arc;
  arc.points = {{-820, 260, -320},
                {-300, 420, 60},
                {260, 300, 220},
                {820, 430, -260}};
  {
    shape::Cloud stations = shape::points::onSpline(arc, 9);
    const glm::vec3 eye = {0, 200, 1150}; // the stream shot's camera
    std::vector<glm::vec3> &facing = stations.vector("facing");
    for (size_t i = 0; i < stations.size(); ++i) {
      const glm::vec3 to = eye - stations.positions[i];
      const float len = glm::length(to);
      facing[i] = len > 1e-6f ? to * (1.0f / len) : glm::vec3{0, 0, 1};
    }
    shape::points::InstanceOptions cardOptions;
    cardOptions.orientLane = "facing";

    world::Material wireMat;
    wireMat.baseColor = {0.9f, 0.93f, 1.0f, 1};
    wireMat.metallic = 1;
    wireMat.roughness = 0.15f;

    // The wire carries a baked color lane — cool chrome at the start
    // warming to rose by the end. Tube rings are ordered along the
    // curve, so a vertex-index ramp IS an arc-length ramp.
    shape::Mesh wire = shape::curves::tube(
        arc, {.radius = 7, .segments = 180, .sides = 10});
    wire.colors.resize(wire.positions.size());
    for (size_t i = 0; i < wire.positions.size(); ++i) {
      const float f =
          wire.positions.size() > 1
              ? (float)i / (float)(wire.positions.size() - 1)
              : 0.0f;
      wire.colors[i] = {0.75f + 0.25f * f, 0.9f - 0.35f * f,
                        1.0f - 0.25f * f, 1};
    }
    world::Material cardMat;
    cardMat.unlit = true;
    cardMat.texture = uiCard(384, 256, {0.45f, 0.95f, 0.85f, 1}, 0.62f);
    cardMat.baseColor = {1, 1, 1, 0.92f};

    stream.render(
        world::scene::group().key("stream")
            .child(world::scene::surface(wire, wireMat).key("wire"))
            .child(world::scene::surface(
                       shape::points::panels(stations, 170, 112,
                                             cardOptions),
                       cardMat)
                       .key("cards")));
  }

  // The set dressing, declared through the world easel (Easel.h): two
  // colored point lights pooling on the floor by the props, and a
  // 3000-spark swarm riding the stream arc, GPU-instanced as ONE draw
  // — tint ramps along "t", size varies through the scale lane.
  {
    shape::Cloud sparks = shape::points::onSpline(arc, 3000);
    shape::points::jitter(sparks, 30, 11);
    shape::points::displaceNoise(sparks, 70, 0.006f, 12);
    const std::vector<float> &t = sparks.scalar("t");
    std::vector<glm::vec4> &tint = sparks.color("tint");
    std::vector<float> &size = sparks.scalar("size", 1);
    for (size_t i = 0; i < sparks.size(); ++i) {
      const float f = t[i];
      tint[i] = {0.45f + 0.55f * f, 0.95f - 0.55f * f,
                 1.0f - 0.05f * f, 1};
      size[i] = 0.55f + 0.75f * (0.5f + 0.5f * std::sin(f * 61.0f));
    }
    world::Material sparkMat;
    sparkMat.unlit = true;
    sparkMat.baseColor = {1, 1, 1, 0.85f}; // blended pass, one flock
    world::InstanceLanes sparkLanes;
    sparkLanes.tintLane = "tint";
    sparkLanes.scaleLane = "size";

    world::easel::Stage dressing = world::easel::stage(*w);
    dressing.light({-520, 60, -80}, {1.0f, 0.25f, 0.85f, 1}, 7, 760)
        .light({540, 80, -50}, {0.2f, 0.85f, 1.0f, 1}, 7, 760)
        .swarm(std::move(sparks), shape::mesh::quad(6, 6), sparkMat,
               sparkLanes)
        .key("sparks");
    const world::scene::Scene::Stats stats = dressing.commit();
    std::printf("easel dressing: %d added\n", stats.added);
  }

  // THE YARN, painted end to end: the sparse ball winding wraps the
  // whole scene (latitude swings seven times while the winding plane
  // precesses twice — coprime, so the wraps spread), and every unit
  // of it carries the infinite-canvas strip above. One surface per
  // GPU tile; per frame every arc re-sweeps one step forward so the
  // whole canvas marches around the winding behind the chrome dart.
  std::vector<uint32_t> stripIds; // one surface per strip tile
  uint32_t dartId = 0, cometId = 0, guideId = 0;
  const float kCometSpan = 0.34f;
  world::World::pop::Chain guideChain;
  shape::Spline3 flightLoop;
  float bandWidth = 300;            // set from the strip's texel density
  const int kTiles = 10;            // GPU tiles the vector strip slices to
  const int kSectionsPerTile = 200; // ribbon cross-sections per arc
  const float kFlagHome = 0.91f;    // stills: strip start on a front pass
  {
    flightLoop.closed = true;
    const int kKnots = 96;
    const float kWraps = 7;   // latitude oscillations
    const float kPrecess = 2; // turns of the winding plane
    const float kTilt = 1.0f; // latitude amplitude, radians
    const glm::vec3 shell = {1250, 620, 950};
    const glm::vec3 center = {0, 380, 0};
    for (int i = 0; i < kKnots; ++i) {
      const float t = (float)i / (float)kKnots;
      const float lat = kTilt * std::sin(2.0f * (float)M_PI * kWraps * t);
      const float azi = -2.0f * (float)M_PI * kPrecess * t; // front: face out
      flightLoop.points.push_back(
          {center.x + shell.x * std::cos(lat) * std::cos(azi),
           center.y + shell.y * std::sin(lat),
           center.z + shell.z * std::cos(lat) * std::sin(azi)});
    }
    float loopLen = 0;
    {
      shape::Cloud rail = shape::points::onSpline(flightLoop, 1024);
      for (size_t i = 1; i < rail.size(); ++i)
        loopLen +=
            glm::length(rail.positions[i] - rail.positions[i - 1]);
    }

    sigil::weave::FontContext fonts(
        sigil::weave::ports::systemFontManager());

    // Square texels: 10 x 4096 px over ~24k wu ≈ 1.68 px/wu, so a
    // 506 px column stands ~300 wu wide on the winding — a real
    // narrow-column measure reading across the band.
    const StripArt strip = yarnStrip(fonts, kTiles, 4096, 506);
    const float pxPerWu = strip.totalAlongPx / loopLen;
    bandWidth = strip.acrossPx / pxPerWu;
    // Each arc is a GPU sweep: the loop's control points live in a
    // device buffer and a compute pass writes the ribbon's vertices —
    // no CPU mesh exists for the band at all.
    for (int k = 0; k < kTiles; ++k) {
      world::Material segment;
      segment.unlit = true;
      segment.texture = strip.tiles[(size_t)k];
      segment.baseColor = {1, 1, 1, 0.98f}; // alpha < 1: blended pass
      world::World::SweepDesc arc;
      arc.loop = flightLoop.points;
      arc.width = bandWidth;
      arc.sections = kSectionsPerTile;
      arc.head = wrap01(kFlagHome + (float)(k + 1) / (float)kTiles);
      arc.span = 1.0f / (float)kTiles;
      stripIds.push_back(w->addSweep(arc, segment));
    }

    world::Material chromeDart;
    chromeDart.baseColor = {0.92f, 0.95f, 1.0f, 1};
    chromeDart.metallic = 1;
    chromeDart.roughness = 0.12f;
    const std::vector<glm::vec2> dartProfile = {
        {0, 95}, {26, 30}, {34, -20}, {18, -52}, {0, -60}};
    dartId = w->addSurface(shape::mesh::revolve(dartProfile),
                           dartTransform(flightLoop, kFlagHome),
                           chromeDart);

    // The comet, COMPOSED ON DEVICE: a small guide chain rides the
    // yarn's window, and the comet chain rides the GUIDE — its
    // generator reads the guide's cooked arena directly. Animating
    // the guide (two floats) cascades through the comet in compute;
    // the CPU never touches a point of either.
    guideChain = shape::pop::on(flightLoop.points)
                     .count(64)
                     .window(kFlagHome, kCometSpan)
                     .noise(26, 0.003f)
                     .smooth(0.5f, 2);
    world::Material guideMat;
    guideMat.unlit = true;
    guideMat.baseColor = {0.5f, 0.9f, 0.8f, 0.25f}; // faint beads
    guideId = w->addPoints(shape::mesh::quad(4, 4), guideChain,
                           guideMat);
    const world::World::pop::Chain cometChain =
        shape::pop::on(std::vector<glm::vec3>{}) // loop = the guide's arena
            .count(300000)
            .window(0.984f, 0.984f) // skip the closing return segment
            .spread(48)
            .noise(20, 0.004f)
            .vary(0.5f)
            .fade({1.0f, 0.45f, 0.85f, 0.0f}, // tail
                  {0.65f, 0.95f, 1.0f, 0.5f}); // head
    world::Material sparkle;
    sparkle.unlit = true;
    sparkle.baseColor = {1, 1, 1, 0.8f}; // blended
    cometId = w->addPointsOn(guideId, shape::mesh::quad(2.6f, 2.6f),
                             cometChain, sparkle);
    std::printf("comet: %d GPU particles riding a %d-point guide "
                "chain, composed on device\n",
                std::get<shape::pop::SplineScatter>(cometChain[0]).count,
                std::get<shape::pop::SplineScatter>(guideChain[0]).count);

    std::printf("yarn: %.0f wu wound, band %.0f wu wide, %d tiles of "
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
    const char *name;
    shape::space::Camera camera;
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
  // --- the interop door, demonstrated -------------------------------------
  // The comet's 300k particles exist ONLY as GPU lanes — cooked by
  // compute, never touched by the CPU. readPoints() pulls the arena
  // back as a Cloud and save::ply() writes a binary PLY (a third the
  // ascii size, bit-exact floats) Houdini or Blender opens directly:
  // positions, t/size scalars, dir vectors, tint colors — the
  // attributes ride along.
  if (cometId) {
    const shape::Cloud comet = w->readPoints(cometId);
    const auto file = outDir / "comet_points.ply";
    if (shape::save::ply(file, comet, {.binary = true}))
      std::printf("comet_points.ply: %zu GPU-cooked points exported "
                  "(binary_little_endian, %.1f MB), "
                  "%zu scalar / %zu vector / %zu color lanes\n",
                  comet.size(),
                  (double)std::filesystem::file_size(file) / 1e6,
                  comet.scalars.size(), comet.vectors.size(),
                  comet.colors.size());
  }

  // --- the flight, and its numbers ----------------------------------------
  // Per frame every strip arc re-sweeps one step forward along the
  // winding (setSurfaceMesh per tile — same topology, an UpdateBuffer
  // each), so the whole painted yarn marches behind the dart. Timed
  // two ways: submit+flush throughput (drained at the end) and fully
  // synced frames (render + GPU readback each time).
  if (!stripIds.empty()) {
    w->setCamera(shots[5].camera);
    // One circumnavigation every 2400 frames (40 s at 60).
    const auto animate = [&](int frame) {
      const float shift =
          wrap01(kFlagHome + (float)frame / 2400.0f);
      for (int k = 0; k < kTiles; ++k)
        w->setSweepWindow(
            stripIds[(size_t)k],
            wrap01(shift + (float)(k + 1) / (float)kTiles),
            1.0f / (float)kTiles);
      w->setTransform(dartId, dartTransform(flightLoop, shift));
      // Two floats on the GUIDE; the comet re-cooks by cascade.
      w->setPointsWindow(guideId, shift, kCometSpan);
    };

    const int kTimed = 240;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kTimed; ++frame) {
      animate(frame);
      w->render();
    }
    w->readback(); // drain, so the mean owns every submitted frame
    const auto flushed = std::chrono::steady_clock::now();

    const int kSynced = 30;
    for (int frame = 0; frame < kSynced; ++frame) {
      animate(frame);
      w->render();
      w->readback();
    }
    const auto synced = std::chrono::steady_clock::now();

    const double renderMs =
        std::chrono::duration<double, std::milli>(flushed - start)
            .count() /
        kTimed;
    const double syncedMs =
        std::chrono::duration<double, std::milli>(synced - flushed)
            .count() /
        kSynced;
    std::printf("flight: %.2f ms/frame submitted+flushed (%.0f fps), "
                "%.2f ms/frame with GPU readback (%.0f fps) — the re-sweeps "
                "run as compute on the GPU\n",
                renderMs, 1000.0 / renderMs, syncedMs,
                1000.0 / syncedMs);

    int flightWritten = 0;
    for (int i = 0; i < 6; ++i) {
      animate(i * 400); // six stations around one full loop
      char name[40];
      std::snprintf(name, sizeof(name), "world_marquee_flight_%d.png",
                    i);
      if (w->render() && w->savePng(outDir / name))
        ++flightWritten;
    }
    std::printf("flight frames: %d/6\n", flightWritten);

    // Showcase mode: argv[3] = a frame count dumps a continuous
    // animation sequence (world_anim_0000.png ...) for ffmpeg.
    const int animFrames = argc > 3 ? std::atoi(argv[3]) : 0;
    int animWritten = 0;
    for (int i = 0; i < animFrames; ++i) {
      animate(i * 4); // 4 loop-steps per frame: one lap in 600 frames
      char name[40];
      std::snprintf(name, sizeof(name), "world_anim_%04d.png", i);
      if (w->render() && w->savePng(outDir / name))
        ++animWritten;
    }
    if (animFrames > 0)
      std::printf("anim frames: %d/%d\n", animWritten, animFrames);
  }

  std::printf("wrote %d/%d shots to %s\n", written, total,
              outDir.string().c_str());
  return written == total ? 0 : 1;
}
