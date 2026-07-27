// Headless SigilShape catalog: every panel is one PNG, written to the
// out dir (argv[1], default shape_demo_out). Mirrors compose_demo's
// writePanel loop; no fonts, no GPU — raster Skia end to end.
// argv[2] names the fetched-asset dir (default "assets"): when the
// Poly Haven studio HDRI is present there, an extra materials panel
// renders under the REAL environment instead of the procedural bakes.

#include "sigilshape/Blend.h"
#include "sigilshape/Geometry.h"
#include "sigilshape/Materials.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Space.h"

#include <sigilimage/ImageAsset.h>
#include <sigilloader/Loader.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>

using namespace sigil::shape;

namespace {

bool writePanel(SkSize size, const std::filesystem::path &path,
                const std::function<void(SkCanvas &)> &draw,
                SkColor clear = 0xff101014) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
      (int)size.width(), (int)size.height()));
  if (!surface)
    return false;
  surface->getCanvas()->clear(clear);
  draw(*surface->getCanvas());
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  if (!surface->readPixels(bm.pixmap(), 0, 0))
    return false;
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
      return (x < 0 ? -1.0f : 1.0f) *
             std::pow(std::abs(x), 2.0f / exponent);
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
    const float y = from.fY + (to.fY - from.fY) * t +
                    amplitude * std::sin(t * (float)cycles * 2.0f *
                                         (float)M_PI);
    if (i == 0)
      b.moveTo({x, y});
    else
      b.lineTo({x, y});
  }
  return b.detach();
}

void checker(SkCanvas &canvas, SkRect area, float cell, SkColor a,
             SkColor b) {
  SkPaint paint;
  for (int y = 0; (float)y * cell < area.height(); ++y)
    for (int x = 0; (float)x * cell < area.width(); ++x) {
      paint.setColor((x + y) % 2 == 0 ? a : b);
      canvas.drawRect(SkRect::MakeXYWH(area.left() + (float)x * cell,
                                       area.top() + (float)y * cell, cell,
                                       cell),
                      paint);
    }
}

// A font-free "UI card" for panel textures: header bar, gauge arc,
// sparkline, tick rows.
sk_sp<SkImage> uiCardImage(int w, int h, SkColor4f accent) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas *c = surface->getCanvas();
  c->clear(0xE60E1220);
  SkPaint p;
  p.setAntiAlias(true);

  p.setColor4f({accent.fR, accent.fG, accent.fB, 0.9f});
  c->drawRRect(SkRRect::MakeRectXY(
                   SkRect::MakeXYWH(16, 16, (float)w - 32, 14), 7, 7),
               p);
  p.setColor4f({1, 1, 1, 0.25f});
  for (int i = 0; i < 4; ++i)
    c->drawRRect(SkRRect::MakeRectXY(
                     SkRect::MakeXYWH(16, 48 + (float)i * 22,
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
  const SkRect gauge = SkRect::MakeXYWH((float)w - 130, (float)h - 130,
                                        100, 100);
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

void panelBlendMorph(SkCanvas &canvas) {
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

void panelBlendColor(SkCanvas &canvas) {
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

void panelBlendSpine(SkCanvas &canvas) {
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

void panelMaterials(SkCanvas &canvas) {
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
    canvas.drawRRect(SkRRect::MakeRectXY(
                         SkRect::MakeXYWH(640, 40, 300, 640), 24, 24),
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
  materials::drawGold(canvas, star(5, 100, 46, {180, 390}), studio, 8,
                      foil);
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
  materials::drawChrome(canvas, circle(88, {480, 390}), studio, 14,
                        brushed);
  materials::ChromeParams y2k;
  y2k.contrast = 1.35f;
  y2k.roughness = 0.08f;
  materials::drawChrome(canvas, star(9, 98, 64, {480, 600}), sunset, 10,
                        y2k);

  // Column 3: glass over the backdrop.
  if (backdrop) {
    materials::drawGlass(canvas, circle(92, {780, 170}), studio, backdrop,
                         16);
    materials::GlassParams deep;
    deep.tint = {0.75f, 0.9f, 0.8f, 1};
    deep.refractPx = 30;
    deep.edgeGlow = 0.5f;
    materials::drawGlass(canvas, squircle(90, {780, 410}, 3.0f), studio,
                         backdrop, 22, deep);
    materials::GlassParams shard;
    shard.tint = {0.9f, 0.85f, 1.0f, 1};
    shard.reflect = 0.75f;
    materials::drawGlass(canvas, star(6, 100, 52, {780, 630}), sunset,
                         backdrop, 12, shard);
  }
}

void panelMeshPerspective(SkCanvas &canvas) {
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
  std::vector<SkPoint> profile;
  for (int i = 0; i <= 24; ++i) {
    const float t = (float)i / 24.0f;
    const float r = 46.0f + 34.0f * std::sin(t * 3.1f + 0.4f) +
                    18.0f * std::sin(t * 8.0f);
    profile.push_back({r, (t - 0.5f) * 240.0f});
  }
  space::MeshStyle jade = steel;
  jade.baseColor = {0.35f, 0.8f, 0.6f, 1};
  space::drawMesh(canvas, mesh::revolve(profile),
           space::place({330, 30, -30}, 0, 0, 0), camera, viewport, jade);

  // Superellipsoid pedestal under everything.
  space::MeshStyle slate = steel;
  slate.baseColor = {0.3f, 0.32f, 0.4f, 1};
  slate.specular = 0.4f;
  space::drawMesh(canvas, mesh::superellipsoid({420, 26, 200}, 6),
           space::place({0, -150, 0}), camera, viewport, slate);
}

void panelMeshChrome(SkCanvas &canvas) {
  // The deferred bridge: 3D normal G-buffer -> per-pixel materials.
  const SkSize viewport = {1240, 720};
  space::Camera camera;
  camera.eye = {0, 90, 820};
  camera.target = {0, 0, 0};

  space::MeshStyle normals;
  normals.mode = space::MeshStyle::Mode::Normals;

  auto renderNormals = [&](const Mesh &m, const SkM44 &model) {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul((int)viewport.width(),
                                   (int)viewport.height()));
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
    const SkM44 model = space::place({-280, 0, 0}, 24, -10, -8);
    sk_sp<SkImage> g = renderNormals(m, model);
    materials::ChromeParams params;
    params.contrast = 1.35f;
    sk_sp<SkShader> shader =
        materials::chrome(g, sunset, {0, 0}, params);
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
    const SkM44 model = space::place({280, 0, -40}, 0, -30, 18);
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

void panelSpace(SkCanvas &canvas) {
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
  Mesh floor = mesh::grid(24, 24, [](float u, float v) -> SkV3 {
    return {(u - 0.5f) * 1400, -170,
            (v - 0.5f) * 1400};
  });
  space::drawMesh(canvas, floor, SkM44(), camera, viewport, wire);

  // Three floating UI cards, angled like a cockpit.
  sk_sp<SkImage> cardA = uiCardImage(360, 240, {0.2f, 0.85f, 1.0f, 1});
  sk_sp<SkImage> cardB = uiCardImage(360, 240, {1.0f, 0.6f, 0.25f, 1});
  sk_sp<SkImage> cardC = uiCardImage(360, 240, {0.7f, 0.45f, 1.0f, 1});
  space::drawImagePanel(canvas, cardA, 360, 240,
                        space::place({-350, 120, -80}, 34), camera,
                        viewport, 0.95f);
  space::drawImagePanel(canvas, cardB, 360, 240,
                        space::place({0, 130, 30}, 0, -4), camera,
                        viewport, 0.98f);
  space::drawImagePanel(canvas, cardC, 360, 240,
                        space::place({350, 110, -80}, -34), camera,
                        viewport, 0.95f);

  // A curved panel: texture the cylinderPanel mesh.
  space::MeshStyle screen;
  screen.texture = uiCardImage(720, 200, {0.3f, 1.0f, 0.6f, 1});
  screen.baseColor = {1, 1, 1, 1};
  screen.ambient = {0.9f, 0.9f, 0.9f, 1};
  screen.lights = {};
  screen.specular = 0;
  space::drawMesh(canvas, mesh::cylinderPanel(680, 190, 420, 48, 10),
                  space::place({0, -160, 60}, 0, 10), camera, viewport,
                  screen);
}

} // namespace

// Materials under a fetched HDRI: SigilLoader -> OIIO -> F32 equirect
// SkImage -> Environment::fromEquirect. Gold/chrome/glass badges lit by
// a real studio.
void panelMaterialsHdri(SkCanvas &canvas,
                        const materials::Environment &env) {
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
  materials::drawGold(canvas, star(6, 110, 60, {210, 200}), env, 12,
                      gold);
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
    materials::drawGlass(canvas, circle(102, {920, 240}), env, backdrop,
                         18);
    materials::GlassParams deep;
    deep.refractPx = 26;
    materials::drawGlass(canvas, squircle(98, {940, 520}, 2.8f), env,
                         backdrop, 20, deep);
  }
}

int main(int argc, char **argv) {
  const std::filesystem::path outDir =
      argc > 1 ? argv[1] : "shape_demo_out";
  const std::filesystem::path assetDir = argc > 2 ? argv[2] : "assets";
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);

  struct Panel {
    const char *name;
    void (*draw)(SkCanvas &);
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
  };

  int written = 0;
  int total = (int)std::size(panels);
  for (const Panel &panel : panels) {
    if (writePanel({1240, 720}, outDir / panel.name,
                   [&](SkCanvas &c) { panel.draw(c); }, panel.clear)) {
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
      if (writePanel({1240, 720}, outDir / "materials_hdri.png",
                     [&](SkCanvas &c) { panelMaterialsHdri(c, env); },
                     0xff101014))
        ++written;
      else
        std::fprintf(stderr, "failed: materials_hdri.png\n");
    }
  }

  std::printf("wrote %d/%d panels to %s\n", written, total,
              outDir.string().c_str());
  return written == total ? 0 : 1;
}
