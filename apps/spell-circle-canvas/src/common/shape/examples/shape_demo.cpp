// Headless SigilShape catalog: every panel is one PNG, written to the
// out dir (argv[1], default shape_demo_out). Mirrors compose_demo's
// writePanel loop; no fonts, no GPU — raster Skia end to end.
// argv[2] names the fetched-asset dir (default "assets"): when the
// Poly Haven studio HDRI is present there, an extra materials panel
// renders under the REAL environment instead of the procedural bakes.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilcompose/Compose.h>
#include <sigilimage/Decode.h>
#include <sigilimage/ImageAsset.h>
#include <sigilloader/Loader.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweavekit/SigilWeaveKit.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>

#include "sigilshape/Blend.h"
#include "sigilshape/Curves.h"
#include "sigilshape/Geometry.h"
#include "sigilshape/Import.h"
#include "sigilshape/Materials.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Ops.h"
#include "sigilshape/Points.h"
#include "sigilshape/Pop.h"
#include "sigilshape/Space.h"

using namespace sigil::shape;

namespace {

bool writePanel(SkSize size, const std::filesystem::path& path,
                const std::function<void(SkCanvas&)>& draw,
                SkColor clear = 0xff101014) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height()));
  if (!surface) return false;
  surface->getCanvas()->clear(clear);
  draw(*surface->getCanvas());
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  if (!surface->readPixels(bm.pixmap(), 0, 0)) return false;
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bm.pixmap(), {});
}

// --- shape builders -------------------------------------------------------

SkPath star(int points, float outer, float inner, SkPoint center,
            float rotationDeg = -90) {
  SkPathBuilder b;
  const float step = (float)M_PI / (float)points;
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < points * 2; ++i) {
    const float r = i % 2 == 0 ? outer : inner;
    const float a = rot + step * (float)i;
    const SkPoint p = {center.fX + r * std::cos(a),
                       center.fY + r * std::sin(a)};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

SkPath circle(float radius, SkPoint center) {
  return SkPath::Circle(center.fX, center.fY, radius);
}

SkPath squircle(float radius, SkPoint center, float exponent = 4) {
  SkPathBuilder b;
  const int n = 128;
  for (int i = 0; i <= n; ++i) {
    const float t = (float)i / (float)n * 2.0f * (float)M_PI;
    const float c = std::cos(t), s = std::sin(t);
    auto shaped = [&](float x) {
      return (x < 0 ? -1.0f : 1.0f) * std::pow(std::abs(x), 2.0f / exponent);
    };
    const SkPoint p = {center.fX + radius * shaped(c),
                       center.fY + radius * shaped(s)};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

SkPath wave(SkPoint from, SkPoint to, float amplitude, int cycles) {
  SkPathBuilder b;
  const int n = 96;
  for (int i = 0; i <= n; ++i) {
    const float t = (float)i / (float)n;
    const float x = from.fX + (to.fX - from.fX) * t;
    const float y =
        from.fY + (to.fY - from.fY) * t +
        amplitude * std::sin(t * (float)cycles * 2.0f * (float)M_PI);
    if (i == 0)
      b.moveTo({x, y});
    else
      b.lineTo({x, y});
  }
  return b.detach();
}

void checker(SkCanvas& canvas, SkRect area, float cell, SkColor a, SkColor b) {
  SkPaint paint;
  for (int y = 0; (float)y * cell < area.height(); ++y)
    for (int x = 0; (float)x * cell < area.width(); ++x) {
      paint.setColor((x + y) % 2 == 0 ? a : b);
      canvas.drawRect(
          SkRect::MakeXYWH(area.left() + (float)x * cell,
                           area.top() + (float)y * cell, cell, cell),
          paint);
    }
}

// A font-free "UI card" for panel textures: header bar, gauge arc,
// sparkline, tick rows.
sk_sp<SkImage> uiCardImage(int w, int h, SkColor4f accent) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas* c = surface->getCanvas();
  c->clear(0xE60E1220);
  SkPaint p;
  p.setAntiAlias(true);

  p.setColor4f({accent.fR, accent.fG, accent.fB, 0.9f});
  c->drawRRect(
      SkRRect::MakeRectXY(SkRect::MakeXYWH(16, 16, (float)w - 32, 14), 7, 7),
      p);
  p.setColor4f({1, 1, 1, 0.25f});
  for (int i = 0; i < 4; ++i)
    c->drawRRect(
        SkRRect::MakeRectXY(SkRect::MakeXYWH(16, 48 + (float)i * 22,
                                             (float)(w - 60 - i * 40), 8),
                            4, 4),
        p);

  // gauge arc
  SkPaint arc;
  arc.setAntiAlias(true);
  arc.setStyle(SkPaint::kStroke_Style);
  arc.setStrokeWidth(12);
  arc.setStrokeCap(SkPaint::kRound_Cap);
  arc.setColor4f({1, 1, 1, 0.15f});
  const SkRect gauge =
      SkRect::MakeXYWH((float)w - 130, (float)h - 130, 100, 100);
  c->drawArc(gauge, 130, 280, false, arc);
  arc.setColor4f(accent);
  c->drawArc(gauge, 130, 200, false, arc);

  // sparkline
  SkPathBuilder spark;
  for (int i = 0; i <= 24; ++i) {
    const float t = (float)i / 24.0f;
    const float x = 16 + t * ((float)w - 180);
    const float y = (float)h - 60 -
                    28.0f * (0.5f + 0.5f * std::sin(t * 9.0f + 1.7f)) -
                    18.0f * t;
    if (i == 0)
      spark.moveTo({x, y});
    else
      spark.lineTo({x, y});
  }
  SkPaint line;
  line.setAntiAlias(true);
  line.setStyle(SkPaint::kStroke_Style);
  line.setStrokeWidth(3);
  line.setColor4f(accent);
  c->drawPath(spark.detach(), line);
  return surface->makeImageSnapshot();
}

// --- panels ---------------------------------------------------------------

void panelBlendMorph(SkCanvas& canvas) {
  // Row 1: star -> circle, the eight-step classic.
  {
    blend::Key from{star(5, 70, 30, {110, 130}), {1.0f, 0.42f, 0.30f, 1}};
    blend::Key to{circle(64, {1090, 130}), {0.30f, 0.62f, 1.0f, 1}};
    blend::Options options;
    options.steps = 8;
    blend::draw(canvas, blend::make(from, to, options));
  }
  // Row 2: three keys — star -> squircle -> gear-ish star.
  {
    blend::Key a{star(4, 70, 28, {110, 330}), {1.0f, 0.85f, 0.25f, 1}};
    blend::Key b{squircle(60, {600, 330}), {0.35f, 1.0f, 0.65f, 1}};
    blend::Key c{star(12, 66, 52, {1090, 330}), {0.75f, 0.4f, 1.0f, 1}};
    const blend::Key keys[3] = {a, b, c};
    blend::Options options;
    options.steps = 5;
    options.smoothOutlines = true;
    blend::draw(canvas, blend::make(keys, options));
  }
  // Row 3: outline-only blend (stroke interpolation).
  {
    blend::Key from{star(6, 72, 40, {110, 540}), {0, 0, 0, 0}};
    from.stroke = SkColor4f{0.2f, 0.9f, 1.0f, 1};
    from.strokeWidth = 6;
    blend::Key to{circle(64, {1090, 540}), {0, 0, 0, 0}};
    to.stroke = SkColor4f{1.0f, 0.35f, 0.75f, 1};
    to.strokeWidth = 1;
    blend::Options options;
    options.steps = 14;
    options.smoothOutlines = true;
    blend::draw(canvas, blend::make(from, to, options));
  }
}

void panelBlendColor(SkCanvas& canvas) {
  // Left: the smooth-color glow stack — one blob scaled down through
  // hue, Illustrator's Smooth Color choosing the step count.
  {
    SkPath outer = squircle(240, {320, 340}, 3.2f);
    SkPath inner = circle(36, {350, 300});
    blend::Key from{outer, {0.08f, 0.10f, 0.35f, 1}};
    blend::Key to{inner, {1.0f, 0.95f, 0.55f, 1}};
    blend::Options options;
    options.spacing = blend::Spacing::SmoothColor;
    options.smoothOutlines = true;
    blend::draw(canvas, blend::make(from, to, options));
  }
  // Right: 90s blend-art ribbon — two open waves, many steps.
  {
    blend::Key from{wave({700, 120}, {1180, 240}, 36, 3), {0, 0, 0, 0}};
    from.stroke = SkColor4f{0.15f, 0.85f, 1.0f, 0.9f};
    from.strokeWidth = 2.5f;
    blend::Key to{wave({660, 520}, {1160, 660}, 64, 2), {0, 0, 0, 0}};
    to.stroke = SkColor4f{1.0f, 0.3f, 0.75f, 0.9f};
    to.strokeWidth = 2.5f;
    blend::Options options;
    options.steps = 42;
    blend::draw(canvas, blend::make(from, to, options));
  }
}

void panelBlendSpine(SkCanvas& canvas) {
  // Blend riding a spiral spine, align-to-path.
  SkPathBuilder spiral;
  const SkPoint c = {620, 360};
  for (int i = 0; i <= 400; ++i) {
    const float t = (float)i / 400.0f;
    const float a = t * 4.4f * (float)M_PI;
    const float r = 40.0f + t * 260.0f;
    const SkPoint p = {c.fX + r * std::cos(a), c.fY + r * std::sin(a)};
    if (i == 0)
      spiral.moveTo(p);
    else
      spiral.lineTo(p);
  }
  blend::Key from{star(3, 34, 16, {0, 0}), {1.0f, 0.9f, 0.3f, 0.95f}};
  blend::Key to{star(7, 30, 12, {0, 0}), {0.4f, 0.5f, 1.0f, 0.95f}};
  blend::Options options;
  options.spacing = blend::Spacing::Distance;
  options.distance = 34;
  options.spine = spiral.detach();
  options.orientation = blend::Orientation::AlignToPath;
  options.smoothOutlines = true;
  blend::draw(canvas, blend::make(from, to, options));
}

void panelMaterials(SkCanvas& canvas) {
  const materials::Environment studio = materials::Environment::studio();
  const materials::Environment sunset = materials::Environment::sunset();

  // Backdrop: checker everywhere, with a bright zone and colored blobs
  // directly behind the glass column so refraction has something to
  // bend.
  checker(canvas, SkRect::MakeXYWH(0, 0, 1240, 720), 40, 0xff14141c,
          0xff1d1d28);
  {
    SkPaint bright;
    bright.setAntiAlias(true);
    bright.setColor4f({0.85f, 0.88f, 0.95f, 0.9f});
    canvas.drawRRect(
        SkRRect::MakeRectXY(SkRect::MakeXYWH(640, 40, 300, 640), 24, 24),
        bright);
  }
  checker(canvas, SkRect::MakeXYWH(650, 50, 280, 620), 35, 0xffdadde8,
          0xff9aa0b8);
  SkPaint blob;
  blob.setAntiAlias(true);
  blob.setColor4f({0.2f, 0.5f, 1.0f, 0.85f});
  canvas.drawCircle(760, 140, 70, blob);
  blob.setColor4f({1.0f, 0.4f, 0.3f, 0.85f});
  canvas.drawCircle(820, 400, 76, blob);
  blob.setColor4f({0.2f, 0.75f, 0.4f, 0.8f});
  canvas.drawCircle(750, 640, 62, blob);
  sk_sp<SkImage> backdrop;
  {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1240, 720));
    if (canvas.readPixels(bm.pixmap(), 0, 0)) {
      bm.setImmutable();
      backdrop = bm.asImage();
    }
  }

  // Column 1: gold — polished badge, foil star, rough coin.
  materials::GoldParams polished;
  polished.crinkle = 0.08f;
  polished.roughness = 0.1f;
  materials::drawGold(canvas, star(8, 90, 62, {180, 150}), studio, 10,
                      polished);
  materials::GoldParams foil;
  foil.crinkle = 0.55f;
  foil.sparkle = 0.8f;
  foil.roughness = 0.35f;
  materials::drawGold(canvas, star(5, 100, 46, {180, 390}), studio, 8, foil);
  materials::GoldParams rough;
  rough.roughness = 0.7f;
  rough.crinkle = 0.2f;
  materials::drawGold(canvas, circle(84, {180, 600}), studio, 16, rough);

  // Column 2: chrome — studio steel, brushed steel, sunset chrome.
  materials::drawChrome(canvas, squircle(92, {480, 150}, 3.4f), studio, 12);
  materials::ChromeParams brushed;
  brushed.brushed = 0.85f;
  brushed.roughness = 0.25f;
  brushed.contrast = 1.2f;
  materials::drawChrome(canvas, circle(88, {480, 390}), studio, 14, brushed);
  materials::ChromeParams y2k;
  y2k.contrast = 1.35f;
  y2k.roughness = 0.08f;
  materials::drawChrome(canvas, star(9, 98, 64, {480, 600}), sunset, 10, y2k);

  // Column 3: glass over the backdrop.
  if (backdrop) {
    materials::drawGlass(canvas, circle(92, {780, 170}), studio, backdrop, 16);
    materials::GlassParams deep;
    deep.tint = {0.75f, 0.9f, 0.8f, 1};
    deep.refractPx = 30;
    deep.edgeGlow = 0.5f;
    materials::drawGlass(canvas, squircle(90, {780, 410}, 3.0f), studio,
                         backdrop, 22, deep);
    materials::GlassParams shard;
    shard.tint = {0.9f, 0.85f, 1.0f, 1};
    shard.reflect = 0.75f;
    materials::drawGlass(canvas, star(6, 100, 52, {780, 630}), sunset, backdrop,
                         12, shard);
  }
}

void panelMeshPerspective(SkCanvas& canvas) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 260, 950};
  camera.target = {0, -30, 0};
  camera.fovYDeg = 42;

  space::MeshStyle steel;
  steel.baseColor = {0.72f, 0.75f, 0.82f, 1};
  steel.lights = {{{-0.55f, -0.7f, -0.45f}, {1.0f, 0.96f, 0.9f, 1}, 1.1f},
                  {{0.7f, -0.2f, -0.3f}, {0.4f, 0.55f, 0.9f, 1}, 0.5f}};
  steel.specular = 0.9f;
  steel.shininess = 64;

  // Extruded star, tilted.
  Mesh starMesh = mesh::extrude(star(5, 95, 44, {0, 0}), {.depth = 40});
  space::drawMesh(canvas, starMesh, space::place({-300, 60, 0}, 38, -18, 8),
                  camera, viewport, steel);

  // Torus.
  space::MeshStyle bronze = steel;
  bronze.baseColor = {0.85f, 0.55f, 0.3f, 1};
  space::drawMesh(canvas, mesh::torus(110, 40),
                  space::place({20, 40, -60}, 0, -32, 14), camera, viewport,
                  bronze);

  // Revolved vase.
  std::vector<glm::vec2> profile;
  for (int i = 0; i <= 24; ++i) {
    const float t = (float)i / 24.0f;
    const float r =
        46.0f + 34.0f * std::sin(t * 3.1f + 0.4f) + 18.0f * std::sin(t * 8.0f);
    profile.push_back({r, (t - 0.5f) * 240.0f});
  }
  space::MeshStyle jade = steel;
  jade.baseColor = {0.35f, 0.8f, 0.6f, 1};
  space::drawMesh(canvas, mesh::revolve(profile),
                  space::place({330, 30, -30}, 0, 0, 0), camera, viewport,
                  jade);

  // Superellipsoid pedestal under everything.
  space::MeshStyle slate = steel;
  slate.baseColor = {0.3f, 0.32f, 0.4f, 1};
  slate.specular = 0.4f;
  space::drawMesh(canvas, mesh::superellipsoid({420, 26, 200}, 6),
                  space::place({0, -150, 0}), camera, viewport, slate);
}

void panelMeshChrome(SkCanvas& canvas) {
  // The deferred bridge: 3D normal G-buffer -> per-pixel materials.
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 90, 820};
  camera.target = {0, 0, 0};

  space::MeshStyle normals;
  normals.mode = space::MeshStyle::Mode::Normals;

  auto renderNormals = [&](const Mesh& m, const glm::mat4& model) {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)viewport.width(), (int)viewport.height()));
    // Flat-normal clear: outside geometry stays (0,0,1).
    surface->getCanvas()->clear(SkColorSetARGB(255, 128, 128, 255));
    space::drawMesh(*surface->getCanvas(), m, model, camera, viewport, normals);
    return surface->makeImageSnapshot();
  };

  const materials::Environment sunset = materials::Environment::sunset();
  const materials::Environment studio = materials::Environment::studio();

  // Sunset-chrome blob — curvature everywhere, so the env sweeps
  // across the surface (a flat extruded cap would read as one sample).
  {
    Mesh m = mesh::superellipsoid({170, 150, 90}, 2.6f, 64, 48);
    const glm::mat4 model = space::place({-280, 0, 0}, 24, -10, -8);
    sk_sp<SkImage> g = renderNormals(m, model);
    materials::ChromeParams params;
    params.contrast = 1.35f;
    sk_sp<SkShader> shader = materials::chrome(g, sunset, {0, 0}, params);
    // Coverage mask: rasterize the mesh once more (any opaque mode),
    // then shade its pixels through kSrcIn.
    canvas.saveLayer(nullptr, nullptr);
    space::MeshStyle mask;
    mask.mode = space::MeshStyle::Mode::Uv;
    space::drawMesh(canvas, m, model, camera, viewport, mask);
    SkPaint shade;
    shade.setShader(shader);
    shade.setBlendMode(SkBlendMode::kSrcIn);
    canvas.drawPaint(shade);
    canvas.restore();
  }

  // Gold torus (studio).
  {
    Mesh m = mesh::torus(130, 46);
    const glm::mat4 model = space::place({280, 0, -40}, 0, -30, 18);
    sk_sp<SkImage> g = renderNormals(m, model);
    materials::GoldParams params;
    params.crinkle = 0.12f;
    sk_sp<SkShader> shader = materials::gold(g, studio, {0, 0}, params);
    canvas.saveLayer(nullptr, nullptr);
    space::MeshStyle mask;
    mask.mode = space::MeshStyle::Mode::Uv;
    space::drawMesh(canvas, m, model, camera, viewport, mask);
    SkPaint shade;
    shade.setShader(shader);
    shade.setBlendMode(SkBlendMode::kSrcIn);
    canvas.drawPaint(shade);
    canvas.restore();
  }
}

void panelSpace(SkCanvas& canvas) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 80, 900};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 38;

  // Faint floor plane far below the cards.
  space::MeshStyle wire;
  wire.baseColor = {0.16f, 0.3f, 0.5f, 0.22f};
  wire.mode = space::MeshStyle::Mode::Lit;
  wire.specular = 0;
  Mesh floor = mesh::grid(24, 24, [](float u, float v) -> glm::vec3 {
    return {(u - 0.5f) * 1400, -170, (v - 0.5f) * 1400};
  });
  space::drawMesh(canvas, floor, glm::mat4(1.0f), camera, viewport, wire);

  // Three floating UI cards, angled like a cockpit.
  sk_sp<SkImage> cardA = uiCardImage(360, 240, {0.2f, 0.85f, 1.0f, 1});
  sk_sp<SkImage> cardB = uiCardImage(360, 240, {1.0f, 0.6f, 0.25f, 1});
  sk_sp<SkImage> cardC = uiCardImage(360, 240, {0.7f, 0.45f, 1.0f, 1});
  space::drawImagePanel(canvas, cardA, 360, 240,
                        space::place({-350, 120, -80}, 34), camera, viewport,
                        0.95f);
  space::drawImagePanel(canvas, cardB, 360, 240,
                        space::place({0, 130, 30}, 0, -4), camera, viewport,
                        0.98f);
  space::drawImagePanel(canvas, cardC, 360, 240,
                        space::place({350, 110, -80}, -34), camera, viewport,
                        0.95f);

  // A curved panel: texture the cylinderPanel mesh.
  space::MeshStyle screen;
  screen.texture = uiCardImage(720, 200, {0.3f, 1.0f, 0.6f, 1});
  screen.baseColor = {1, 1, 1, 1};
  screen.ambient = {0.9f, 0.9f, 0.9f, 1};
  screen.lights = {};
  screen.specular = 0;
  space::drawMesh(canvas, mesh::cylinderPanel(680, 190, 420, 48, 10),
                  space::place({0, -160, 60}, 0, 10), camera, viewport, screen);
}

}  // namespace

// The Pathfinder panel and the Distort menu: booleans on a star+circle
// pair, offset rings, and the four distorts over one base star.
void panelPathfinder(SkCanvas& canvas) {
  const auto fill = [&](const SkPath& path, SkColor4f color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor4f(color);
    canvas.drawPath(path, paint);
  };
  const auto outline = [&](const SkPath& path, SkColor4f color, float width) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setColor4f(color);
    canvas.drawPath(path, paint);
  };

  // Row 1 — the booleans, each on the same star+circle pair.
  {
    const float y = 140;
    const SkColor4f ink = {0.85f, 0.9f, 1.0f, 1};
    struct Case {
      const char* name;
      SkPath (*op)(const SkPath&, const SkPath&);
    };
    const Case cases[] = {{"unite", ops::unite},
                          {"subtract", ops::subtract},
                          {"intersect", ops::intersect},
                          {"exclude", ops::exclude}};
    float x = 170;
    for (const Case& c : cases) {
      const SkPath a = star(5, 78, 34, {x - 18, y});
      const SkPath b = circle(52, {x + 34, y + 18});
      outline(a, {0.4f, 0.5f, 0.7f, 0.5f}, 1.5f);
      outline(b, {0.4f, 0.5f, 0.7f, 0.5f}, 1.5f);
      fill(c.op(a, b), ink);
      x += 300;
    }
  }
  // Row 2 — offset rings: the same blob, offset in steps both ways.
  {
    const SkPath base = squircle(70, {250, 430}, 3.0f);
    for (int i = -2; i <= 3; ++i) {
      const SkPath ring = ops::offset(base, (float)i * 22.0f);
      outline(ring,
              {0.3f + 0.12f * (float)(i + 2), 0.75f - 0.09f * (float)(i + 2),
               1.0f, 0.9f},
              i == 0 ? 4.0f : 2.0f);
    }
  }
  // Row 2, right — a non-destructive chain: offset -> zigzag -> roughen.
  {
    const SkPath base = circle(80, {700, 430});
    outline(base, {0.4f, 0.5f, 0.7f, 0.6f}, 1.5f);
    const ops::PathOp recipe =
        ops::chain({ops::offsetBy(18), ops::Zigzag{7, 30, true},
                    ops::Roughen{2.5f, 6, 11}});
    fill(recipe(base), {1.0f, 0.62f, 0.3f, 0.95f});
  }
  // Row 3 — the distorts over one base star.
  {
    const float y = 620;
    const SkPath base = star(6, 70, 38, {0, 0});
    struct Row {
      SkPath path;
      SkColor4f color;
    };
    const Row rows[] = {
        {ops::Roughen{5, 7, 3}.apply(base), {0.55f, 0.95f, 0.7f, 1}},
        {ops::Zigzag{6, 26, false}.apply(base), {0.95f, 0.85f, 0.4f, 1}},
        {ops::Zigzag{6, 26, true}.apply(base), {0.95f, 0.6f, 0.4f, 1}},
        {ops::PuckerBloat{-0.6f}.apply(base), {0.7f, 0.55f, 0.95f, 1}},
        {ops::PuckerBloat{0.7f}.apply(base), {0.45f, 0.75f, 0.95f, 1}},
        {ops::Twirl{100}.apply(base), {0.95f, 0.5f, 0.7f, 1}},
    };
    float x = 130;
    for (const Row& row : rows) {
      canvas.save();
      canvas.translate(x, y);
      fill(row.path, row.color);
      canvas.restore();
      x += 200;
    }
  }
}

// Splines crossing space: one knotted 3D curve carrying everything —
// tube geometry, a taper ribbon, arc-length beads as billboards, and
// instanced panels standing on its frames.
void panelSplines(SkCanvas& canvas) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {60, 500, 780};
  camera.target = {0, -20, 0};
  camera.fovYDeg = 44;

  Spline3 knot;
  knot.closed = true;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    knot.points.push_back(
        {std::cos(a) * 300, std::sin(a * 3.0f) * 110, std::sin(a) * 300});
  }

  // The tube, lit like brushed steel.
  space::MeshStyle steel;
  steel.baseColor = {0.6f, 0.68f, 0.8f, 1};
  steel.specular = 0.9f;
  steel.shininess = 48;
  space::drawMesh(
      canvas, curves::tube(knot, {.radius = 9, .segments = 220, .sides = 12}),
      glm::mat4(1.0f), camera, viewport, steel);

  // Instanced panels standing on the curve's frames, tilted like
  // solar panels (a cooked lane: binormal leaned toward the normal).
  Cloud stations = points::onSpline(knot, 14);
  {
    std::vector<glm::vec4>& tint = stations.color("tint");
    std::vector<glm::vec3>& facing = stations.vector("facing");
    const std::vector<float>& t = *stations.scalarIf("t");
    const std::vector<glm::vec3>& normal = *stations.vectorIf("normal");
    const std::vector<glm::vec3>& binormal = *stations.vectorIf("binormal");
    for (size_t i = 0; i < stations.size(); ++i) {
      tint[i] = {0.35f + 0.6f * t[i], 0.9f - 0.5f * t[i], 1.0f, 0.85f};
      const glm::vec3 lean = binormal[i] + normal[i] * 1.2f;
      const float len = glm::length(lean);
      facing[i] = len > 1e-6f ? lean * (1.0f / len) : normal[i];
    }
  }
  points::InstanceOptions cards;
  cards.orientLane = "facing";
  cards.tintLane = "tint";
  space::MeshStyle screen;
  screen.baseColor = {1, 1, 1, 1};
  screen.ambient = {0.85f, 0.85f, 0.9f, 1};
  screen.specular = 0;
  space::drawMesh(canvas, points::quads(stations, 96, 64, cards),
                  glm::mat4(1.0f), camera, viewport, screen);

  // Particles: a drifting halo around the wire, additive.
  Cloud sparks = points::onSpline(knot, 320);
  {
    std::vector<glm::vec4>& tint = sparks.color("tint");
    std::vector<float>& size = sparks.scalar("size", 1);
    const std::vector<float>& t = *sparks.scalarIf("t");
    for (size_t i = 0; i < sparks.size(); ++i) {
      tint[i] = {0.4f + 0.6f * t[i], 0.75f, 1.0f - 0.5f * t[i], 0.28f};
      size[i] = 0.4f + 0.8f * std::abs(std::sin(t[i] * 40.0f));
    }
  }
  points::displaceNoise(sparks, 34, 0.012f, 9);
  points::BillboardStyle glow;
  glow.size = 13;
  glow.sizeLane = "size";
  glow.tintLane = "tint";
  points::drawBillboards(canvas, sparks, camera, viewport, glow);

  // The same spline as a drawn overlay — proof one value feeds both
  // the mesh world and the vector world.
  SkPaint wire;
  wire.setAntiAlias(true);
  wire.setStyle(SkPaint::kStroke_Style);
  wire.setStrokeWidth(1.2f);
  wire.setColor4f({1, 1, 1, 0.35f});
  canvas.drawPath(curves::project(knot, camera, viewport, 400), wire);
}

// Materials under a fetched HDRI: SigilLoader -> OIIO -> F32 equirect
// SkImage -> Environment::fromEquirect. Gold/chrome/glass badges lit by
// a real studio.
void panelMaterialsHdri(SkCanvas& canvas, const materials::Environment& env) {
  checker(canvas, SkRect::MakeXYWH(0, 0, 1240, 720), 40, 0xff191920,
          0xff232330);
  SkPaint blob;
  blob.setAntiAlias(true);
  blob.setColor4f({0.95f, 0.6f, 0.2f, 0.8f});
  canvas.drawCircle(1000, 240, 90, blob);
  blob.setColor4f({0.25f, 0.55f, 0.95f, 0.8f});
  canvas.drawCircle(1060, 500, 76, blob);
  sk_sp<SkImage> backdrop;
  {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1240, 720));
    if (canvas.readPixels(bm.pixmap(), 0, 0)) {
      bm.setImmutable();
      backdrop = bm.asImage();
    }
  }
  materials::GoldParams gold;
  gold.crinkle = 0.3f;
  gold.roughness = 0.2f;
  materials::drawGold(canvas, star(6, 110, 60, {210, 200}), env, 12, gold);
  materials::GoldParams coin;
  coin.roughness = 0.55f;
  coin.crinkle = 0.1f;
  materials::drawGold(canvas, circle(92, {210, 520}), env, 18, coin);
  materials::ChromeParams mirror;
  mirror.exposure = 2.8f;
  mirror.contrast = 1.25f;
  materials::drawChrome(canvas, squircle(104, {560, 200}, 3.2f), env, 14,
                        mirror);
  materials::ChromeParams brushed;
  brushed.brushed = 0.8f;
  brushed.roughness = 0.3f;
  brushed.contrast = 1.1f;
  brushed.exposure = 2.8f;
  materials::drawChrome(canvas, circle(98, {560, 520}), env, 16, brushed);
  if (backdrop) {
    materials::drawGlass(canvas, circle(102, {920, 240}), env, backdrop, 18);
    materials::GlassParams deep;
    deep.refractPx = 26;
    materials::drawGlass(canvas, squircle(98, {940, 520}, 2.8f), env, backdrop,
                         20, deep);
  }
}

// Imported models: whatever assets/models/ holds (fetch_assets ships
// the CC0 Khronos Avocado GLB; drop any OBJ/glTF/STL beside it and it
// joins the lineup). Each part draws with its own base color and, when
// the file carries one, its base-color texture — SigilImage decodes
// the encoded bytes the importer hands over.
void panelImportedModels(SkCanvas& canvas,
                         const std::vector<import::Model>& models) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 170, 860};
  camera.target = {0, -10, 0};
  camera.fovYDeg = 40;

  // The faint floor grid from the panels scene grounds the lineup.
  space::MeshStyle grid;
  grid.baseColor = {0.16f, 0.3f, 0.5f, 0.22f};
  grid.specular = 0;
  Mesh floor = mesh::grid(24, 24, [](float u, float v) -> glm::vec3 {
    return {(u - 0.5f) * 1400, -180, (v - 0.5f) * 1400};
  });
  space::drawMesh(canvas, floor, glm::mat4(1.0f), camera, viewport, grid);

  const float slots[3] = {-370, 0, 370};
  const int count = std::min((int)models.size(), 3);
  for (int i = 0; i < count; ++i) {
    const import::Model& model = models[(size_t)i];
    const glm::mat4 place = space::place({slots[count == 1 ? 1 : i], 0, 0},
                                         -34 + 26.0f * (float)i, -6) *
                            model.fitTransform(290);
    for (const import::Part& part : model.parts) {
      space::MeshStyle style;
      style.baseColor = {part.baseColor.r, part.baseColor.g, part.baseColor.b,
                         part.baseColor.a};
      style.lights = {
          {{-0.5f, -0.75f, -0.4f}, {1.0f, 0.97f, 0.92f, 1}, 1.05f},
          {{0.65f, -0.15f, -0.35f}, {0.45f, 0.55f, 0.85f, 1}, 0.4f}};
      style.ambient = {0.38f, 0.38f, 0.42f, 1};
      style.specular = 0.25f;
      style.shininess = 24;
      if (!part.textureBytes.empty()) {
        if (auto decoded = sigil::image::decodeImage(part.textureBytes.data(),
                                                     part.textureBytes.size(),
                                                     {}, part.textureUri))
          style.texture = decoded->frameAt(0).image;
      }
      space::drawMesh(canvas, part.mesh, place, camera, viewport, style);
    }
  }
}

// A 2x2 sprite atlas: four motifs, one texture — the pop Atlas op
// picks a cell per point, so one stamps() call scatters variety.
sk_sp<SkImage> spriteAtlas() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256, 256));
  SkCanvas* c = surface->getCanvas();
  c->clear(SK_ColorTRANSPARENT);
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f({0.4f, 0.85f, 1.0f, 1});
  c->drawCircle(64, 64, 44, p);  // dot
  p.setColor4f({1.0f, 0.6f, 0.3f, 1});
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(16);
  c->drawCircle(192, 64, 38, p);  // ring
  p.setStyle(SkPaint::kFill_Style);
  p.setColor4f({0.6f, 1.0f, 0.6f, 1});
  SkPathBuilder diamond;
  diamond.moveTo({64, 148})
      .lineTo({108, 192})
      .lineTo({64, 236})
      .lineTo({20, 192});
  diamond.close();
  c->drawPath(diamond.detach(), p);  // diamond
  p.setColor4f({1.0f, 0.8f, 0.3f, 1});
  c->drawPath(star(4, 46, 16, {192, 192}), p);  // sparkle
  return surface->makeImageSnapshot();
}

// The pop combinators, spoken as an artist: chains form models, sinks
// pick the former, every edit is a value. This panel IS the example —
// copy any stanza into a sketch and turn the dials.
void panelPop(SkCanvas& canvas) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 260, 980};
  camera.target = {0, 20, 0};
  camera.fovYDeg = 42;

  std::vector<glm::vec3> ring;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    ring.push_back(
        {std::cos(a) * 230, std::sin(a * 2.0f) * 60, std::sin(a) * 230});
  }

  space::MeshStyle steel;
  steel.baseColor = {0.62f, 0.7f, 0.82f, 1};
  steel.specular = 0.8f;
  steel.shininess = 48;

  // A noised ring becomes a wobbly tube: three verbs and a sink.
  space::drawMesh(
      canvas, pop::on(ring).count(220).noise(26, 0.004f).tube(11, 14, true),
      space::place({-330, 40, 0}, 24, -10), camera, viewport, steel);

  // The same loop, stamped: scattered plates facing the camera,
  // scale varied, tint faded around the ring.
  space::MeshStyle plates;
  plates.baseColor = {1, 1, 1, 1};
  plates.ambient = {0.85f, 0.85f, 0.9f, 1};
  plates.specular = 0;
  plates.texture = spriteAtlas();  // .atlas() picks each stamp's cell
  space::drawMesh(canvas,
                  pop::on(ring)
                      .count(900)
                      .spread(26)
                      .vary(0.6f)
                      .fade({1.0f, 0.7f, 0.55f, 1}, {0.6f, 0.85f, 1.0f, 1})
                      .atlas(2, 2)
                      .lookAt(camera.eye)
                      .stamps(mesh::quad(11, 11)),
                  space::place({330, 60, 0}, -16), camera, viewport, plates);

  // A ribbon on a lifted window of the loop — the chain is open:
  // reach in, move it, re-form.
  // Any outline sweeps along a chain: a star cross-section riding a
  // smoothed noised ring — extrusion, the artist way.
  space::MeshStyle gold = steel;
  gold.baseColor = {0.95f, 0.72f, 0.3f, 1};
  const Mesh crown = pop::on(ring)
                         .count(140)
                         .noise(20, 0.004f)
                         .smooth(0.5f, 2)
                         .sweep(star(5, 30, 14, {0, 0}), true);
  const glm::mat4 crownPlace = space::place({0, 255, -140}, 14, -10, 0, 0.85f);
  space::drawMesh(canvas, crown, crownPlace, camera, viewport, gold);
  // ...and pops seed from FORMED models: glints scattered on the
  // crown's own surface, facing along its normals.
  space::MeshStyle glint;
  glint.baseColor = {1.0f, 0.95f, 0.8f, 1};
  glint.ambient = {0.85f, 0.8f, 0.7f, 1};
  glint.specular = 0;
  space::drawMesh(canvas,
                  pop::on(crown, 600).jitter(1.5f).stamps(mesh::quad(3, 3)),
                  crownPlace, camera, viewport, glint);

  pop::Chain wave = pop::on(ring)
                        .count(120)
                        .window(0.5f, 0.5f)
                        .noise(16, 0.004f)
                        .smooth(0.6f, 3);  // heal the kinks before the sweep
  space::MeshStyle jade = steel;
  jade.baseColor = {0.4f, 0.85f, 0.6f, 1};
  jade.backfaceCull = false;  // a band twists; show both faces
  space::drawMesh(canvas, popops::cookRibbon(wave, 42, {.segments = 120}),
                  space::place({0, -60, 140}, 0, 14), camera, viewport, jade);
}

// The PRIMITIVE class: attributes that live on TRIANGLES, the sibling
// of the point lanes. Three stanzas, left to right:
//
//  1. facets — a prim "Color" lane written per triangle and read
//     natively by MeshStyle::primColorLane. The two triangles of each
//     quad alternate brightness, which is the whole point: no point or
//     vertex attribute can say that.
//  2. baked — the SAME lane through mesh::bakePrimColor, which unwelds
//     into per-vertex colours and needs no lane support at all. This is
//     how the layer reaches SigilWorld's vertex-only pipelines; both
//     styles here run flat (no specular/rim) so the match is honest.
//  3. pieces — the point class promoted INTO the prim class:
//     .promote("Id") stamps each triangle with its owning point's
//     index, and the demo colours by it. A stamp instance is a run of
//     triangles sharing an Id value, not a second container.
void panelPopPrims(SkCanvas& canvas) {
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 210, 900};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 42;

  const auto wheel = [](float h, float value) -> glm::vec4 {
    const auto channel = [&](float offset) {
      return value * (0.55f + 0.45f * std::cos(6.2831853f * (h + offset)));
    };
    return {channel(0.0f), channel(0.33f), channel(0.67f), 1.0f};
  };

  space::MeshStyle flat;
  flat.baseColor = {1, 1, 1, 1};
  flat.ambient = {0.34f, 0.34f, 0.38f, 1};
  flat.specular = 0;  // keep 1 and 2 comparable: no view-dependent terms
  flat.rim = 0;

  // 1. A prim lane written straight onto a formed model.
  Mesh facets = mesh::torus(130, 46, 34, 14);
  std::vector<glm::vec4>& color = facets.prim("Color");
  for (size_t t = 0; t < color.size(); ++t)
    color[t] = wheel((float)(t / 2) / (float)(color.size() / 2),
                     t % 2 == 0 ? 1.0f : 0.55f);
  space::MeshStyle lit = flat;
  lit.primColorLane = "Color";
  space::drawMesh(canvas, facets, space::place({-380, 10, 0}, 0, -28), camera,
                  viewport, lit);

  // 2. The same lane, baked into vertices for renderers without one.
  space::drawMesh(canvas, mesh::bakePrimColor(facets, "Color"),
                  space::place({0, 10, 0}, 0, -28), camera, viewport, flat);

  // 3. The promote: point class -> prim class, addressed by name.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.push_back({160.0f * std::cos(a), 40.0f * std::sin(a * 3.0f),
                    160.0f * std::sin(a)});
  }
  const int kPieces = 64;
  Mesh pieces = pop::on(loop)
                    .count(kPieces)
                    .spread(30)
                    .vary(0.45f)
                    .lookAt(camera.eye)
                    .promote("Id")
                    .stamps(mesh::quad(46, 46));
  if (const std::vector<glm::vec4>* ids = pieces.primIf("Id")) {
    std::vector<glm::vec4>& tint = pieces.prim("Color");
    for (size_t t = 0; t < tint.size(); ++t) {
      const int id = (int)(*ids)[t].x;
      tint[t] = wheel((float)(id * 19 % kPieces) / (float)kPieces,
                      t % 2 == 0 ? 1.0f : 0.62f);
    }
  }
  space::MeshStyle stamped = lit;
  stamped.ambient = {0.9f, 0.9f, 0.95f, 1};
  space::drawMesh(canvas, pieces, space::place({380, 10, 0}), camera, viewport,
                  stamped);
}

// The Skia yarn marquee: the SAME idea as SigilWorld's — a ball
// winding painted end to end with one compose column, perpendicular
// text — but formed by curves::banner and drawn by the PAINTER
// (space::drawMesh), no GPU device anywhere. Arcs draw back-to-front
// by centroid depth; interpenetrating wraps accept painter honesty.
void panelYarnMarquee(SkCanvas& canvas) {
  namespace sc = sigil::compose;
  namespace weave = sigil::weave;
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {40, 140, 980};
  camera.target = {0, 10, 0};
  camera.fovYDeg = 46;

  Spline3 yarn;
  yarn.closed = true;
  for (int i = 0; i < 96; ++i) {
    const float t = (float)i / 96.0f;
    const float lat = std::sin(2.0f * (float)M_PI * 3.0f * t);
    const float azi = -2.0f * (float)M_PI * 2.0f * t;
    yarn.points.push_back({340.0f * std::cos(lat) * std::cos(azi),
                           10.0f + 200.0f * std::sin(lat),
                           280.0f * std::cos(lat) * std::sin(azi)});
  }
  const float loopLen = yarn.length(512);
  const float kWidth = 100;
  const int kAcrossPx = 300;
  const int kTiles = 4;
  const int kTilePx = 4096;
  const float total = (float)kTiles * kTilePx;

  // One compose column, perpendicular orientation, packed sectors.
  weave::FontContext fonts(weave::ports::systemFontManager());
  const SkColor kInk = SkColorSetARGB(255, 236, 244, 254);
  const SkColor kAccent = SkColorSetARGB(255, 116, 224, 190);
  const auto para = [](const char8_t* s, float size, SkColor c) {
    auto p = std::make_shared<weave::Paragraph>();
    p->appendText(s, weave::kit::makeStyle(size, c));
    return p;
  };
  weave::ParagraphLayoutOptions centered;
  centered.alignment = weave::TextAlignment::kCenter;
  sc::Element column =
      sc::box()
          .column()
          .width((float)kAcrossPx)
          .height(total)
          .padding(18, 60)
          .child(sc::box().left(4).top(0).bottom(0).width(3).fill(
              sc::Fill::color({0.455f, 0.878f, 0.745f, 0.9f})))
          .child(sc::box().right(4).top(0).bottom(0).width(2).fill(
              sc::Fill::color({0.455f, 0.878f, 0.745f, 0.5f})));
  const char8_t* pool[6] = {
      u8"the same winding, no GPU anywhere",
      u8"curves::banner forms the band",
      u8"space::drawMesh paints the cloth",
      u8"text reads across, the column climbs",
      u8"one compose column, sliced to tiles",
      u8"skia end to end, the artist's backend",
  };
  column.child(sc::text(para(u8"THE PAINTER'S YARN", 52, kAccent), centered));
  for (int s = 0; s < 48; ++s) {
    column.child(sc::box().grow());
    char numeral[16];
    std::snprintf(numeral, sizeof(numeral), "- %02d -", s + 1);
    std::u8string label((const char8_t*)numeral);
    auto n = std::make_shared<weave::Paragraph>();
    n->appendText(
        label, weave::kit::makeStyle(30, SkColorSetARGB(255, 150, 168, 196)));
    column.child(sc::text(n, centered));
    column.child(sc::text(para(pool[s % 6], 38, kInk), centered));
  }
  column.child(sc::box().grow());
  column.child(
      sc::text(para(u8"and back to its own beginning", 42, kAccent), centered));
  const sk_sp<SkPicture> art = sc::snapshot(column, fonts);

  // Slice into tiles (mirror-x: the wall samples u right-to-left).
  std::vector<sk_sp<SkImage>> tiles;
  for (int k = 0; k < kTiles; ++k) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kAcrossPx, kTilePx));
    SkCanvas* c = surface->getCanvas();
    c->clear(SkColorSetARGB(120, 8, 12, 22));
    c->translate((float)kAcrossPx, 0);
    c->scale(-1, 1);
    c->translate(0, -(float)k * kTilePx);
    c->drawPicture(art);
    tiles.push_back(surface->makeImageSnapshot());
  }

  // One banner arc per tile, painted farthest-first.
  struct Arc {
    Mesh mesh;
    float depth = 0;
    int tile = 0;
  };
  std::vector<Arc> arcs;
  for (int k = 0; k < kTiles; ++k) {
    Arc arc;
    arc.mesh = curves::banner(yarn, {.width = kWidth,
                                     .head = (float)(k + 1) / (float)kTiles,
                                     .span = 1.0f / (float)kTiles,
                                     .sections = 160});
    glm::vec3 lo, hi;
    arc.mesh.bounds(&lo, &hi);
    const glm::vec3 mid = (lo + hi) * 0.5f;
    arc.depth = glm::length(mid - camera.eye);
    arc.tile = k;
    arcs.push_back(std::move(arc));
  }
  std::sort(arcs.begin(), arcs.end(),
            [](const Arc& a, const Arc& b) { return a.depth > b.depth; });
  for (const Arc& arc : arcs) {
    space::MeshStyle cloth;
    cloth.texture = tiles[(size_t)arc.tile];
    cloth.baseColor = {1, 1, 1, 1};
    cloth.ambient = {1, 1, 1, 1};
    cloth.lights = {};
    cloth.specular = 0;
    cloth.backfaceCull = false;  // the back of the cloth shows, honest
    space::drawMesh(canvas, arc.mesh, glm::mat4(1.0f), camera, viewport, cloth);
  }
  static_cast<void>(loopLen);
}

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-') {
    const bool help =
        std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0;
    std::fprintf(help ? stdout : stderr,
                 "usage: shape_demo [outdir] [assetdir]\n");
    return help ? 0 : 1;
  }
  const std::filesystem::path outDir = argc > 1 ? argv[1] : "shape_demo_out";
  const std::filesystem::path assetDir = argc > 2 ? argv[2] : "assets";
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);

  struct Panel {
    const char* name;
    void (*draw)(SkCanvas&);
    SkColor clear;
  };
  const Panel panels[] = {
      {"blend_morph.png", panelBlendMorph, 0xff101014},
      {"blend_color.png", panelBlendColor, 0xff0c0c12},
      {"blend_spine.png", panelBlendSpine, 0xff101014},
      {"materials.png", panelMaterials, 0xff101014},
      {"mesh_perspective.png", panelMeshPerspective, 0xff0d0d13},
      {"mesh_chrome.png", panelMeshChrome, 0xff0d0d13},
      {"panels_space.png", panelSpace, 0xff07070c},
      {"pathfinder.png", panelPathfinder, 0xff101014},
      {"splines_particles.png", panelSplines, 0xff07070c},
      {"pop_models.png", panelPop, 0xff0d0d13},
      {"pop_prims.png", panelPopPrims, 0xff0d0d13},
      {"yarn_marquee.png", panelYarnMarquee, 0xff08080d},
  };

  int written = 0;
  int total = (int)std::size(panels);
  for (const Panel& panel : panels) {
    if (writePanel(
            {1240, 720}, outDir / panel.name,
            [&](SkCanvas& c) { panel.draw(c); }, panel.clear)) {
      ++written;
    } else {
      std::fprintf(stderr, "failed: %s\n", panel.name);
    }
  }

  // Optional extra panel under the fetched HDRI environment.
  const std::filesystem::path hdri = "hdri/studio_small_09_1k.hdr";
  if (std::filesystem::exists(assetDir / hdri)) {
    sigil::loader::Hub hub;
    hub.mount("res://", assetDir);
    if (std::shared_ptr<const sigil::image::ImageAsset> equirect =
            hub.image("res://" + hdri.string())) {
      const materials::Environment env =
          materials::Environment::fromEquirect(equirect->frameAt(0).image);
      ++total;
      if (writePanel(
              {1240, 720}, outDir / "materials_hdri.png",
              [&](SkCanvas& c) { panelMaterialsHdri(c, env); }, 0xff101014))
        ++written;
      else
        std::fprintf(stderr, "failed: materials_hdri.png\n");
    }
  }

  // Imported models, up to three, alphabetical: fetch_assets ships the
  // Khronos Avocado; any OBJ/glTF/GLB/STL dropped into assets/models/
  // joins the panel.
  std::vector<import::Model> models;
  const std::filesystem::path modelDir = assetDir / "models";
  if (std::filesystem::exists(modelDir)) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(modelDir))
      if (entry.is_regular_file()) files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& file : files) {
      if (models.size() == 3) break;
      if (auto model = import::model(file)) models.push_back(std::move(*model));
      // non-model files (licenses) simply don't import; no complaint
    }
  }
  if (!models.empty()) {
    ++total;
    if (writePanel(
            {1240, 720}, outDir / "imported_models.png",
            [&](SkCanvas& c) { panelImportedModels(c, models); }, 0xff0d0d13))
      ++written;
    else
      std::fprintf(stderr, "failed: imported_models.png\n");
  }

  std::printf("wrote %d/%d panels to %s\n", written, total,
              outDir.string().c_str());
  return written == total ? 0 : 1;
}
