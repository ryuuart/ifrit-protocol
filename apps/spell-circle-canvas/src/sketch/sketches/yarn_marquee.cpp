/** @file
 * yarn_marquee — THE HUNG RAIL: `curve::hangFrames` against
 * `curve::frames`, on one closed loop.
 *
 * A rail is a sequence of moving frames, and a band swept on it is that
 * sequence made visible: the profile is a two-point line, so every ring
 * is one segment laid along the frame's BINORMAL — the across-vector —
 * and the surface between two rings is the cloth. Which way the cloth
 * faces at each station is therefore entirely a question about the rail,
 * and the two rails here answer it differently.
 *
 *   PARALLEL TRANSPORT — `frames(loop, n)`. The first normal starts
 *     nearest `up` and every later frame rotates MINIMALLY from the one
 *     before it, so the rail never flips at an inflection the way a
 *     Frenet frame does. What it does not promise is any relation to the
 *     world: carried round a loop that climbs and dives, the across-
 *     vector ends up pointing wherever transport left it, and the band
 *     rolls onto its edge and past it. A banner on this rail reads
 *     sideways and then upside down.
 *   THE HUNG RAIL — `hangFrames(loop, n, head, span)`. Each frame's
 *     binormal is instead the world-vertical HANG direction: the
 *     component of straight down perpendicular to the tangent, carried
 *     unchanged through stretches too near vertical to define one. The
 *     band is then always as flat to the ground as the tangent allows,
 *     which is what a towed banner does and what a marquee needs. `head`
 *     is the leading edge and `span` the length trailing it, so advancing
 *     `head` alone tows the window round the loop.
 *
 * THE ACROSS-VECTORS ARE DRAWN. At `kStations` stations on each rail a
 * tick is struck from the curve along the frame's binormal, scaled to the
 * band's own half-width, and projected with the same camera the cloth is
 * drawn with. That is the whole difference between the two panels, said
 * twice: once as a rolling banner and once as a comb of ticks.
 *
 * The loop is `world::kit::winding` — a closed winding on a shell whose
 * wrap and turn counts share no factor, so it crosses in front of and
 * behind itself and every part of it is seen once per lap. The banner is
 * one compose column baked once and used by both panels, so nothing but
 * the rail differs between them.
 *
 * PAINTER ORDER IS THE DEPTH TEST. There is none of the other kind here:
 * the mesh painter sorts triangles back to front and the cull is off, so
 * both faces of a cloth show and wraps that pass through one another
 * accept that honestly.
 *
 * EDIT THESE FIRST
 *   kSections — rings along each rail; also how finely the roll is seen.
 *   kStations — ticks per panel.
 *   kWidth    — the band's world width.
 *   the Winding's wraps / turns — how often the loop crosses itself, and
 *               therefore how hard the transported rail is pushed.
 */

#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Sweep.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace camera = sigil::geometry::mesh::camera;
namespace curve = sigil::geometry::mesh::curve;
namespace mesh = sigil::geometry::mesh;
namespace render = sigil::geometry::mesh::render;
namespace wkit = sigil::world::kit;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kPanel = 552;    // one panel's drawn width, px
constexpr float kPanelH = 470;   // …and its height
constexpr float kWidth = 132;    // the band's world width
constexpr int kAcrossPx = 300;   // the banner column's pixel width
constexpr int kBannerPx = 3072;  // the banner column's pixel length
constexpr int kSections = 220;   // rings along a rail
constexpr int kStations = 34;    // across-vector ticks per panel
constexpr int kSectors = 16;     // numbered sectors down the banner

constexpr SkColor4f kGround{0.031f, 0.031f, 0.051f, 1};
constexpr SkColor4f kCellGround{0.055f, 0.055f, 0.085f, 1};
constexpr SkColor4f kInk{0.925f, 0.957f, 0.996f, 1};
constexpr SkColor4f kAccent{0.455f, 0.878f, 0.745f, 1};
constexpr SkColor4f kNumeral{0.588f, 0.659f, 0.769f, 1};
constexpr SkColor4f kAsh{0.56f, 0.58f, 0.66f, 1};
constexpr SkColor4f kRule{0.17f, 0.18f, 0.24f, 1};
constexpr SkColor4f kTick{1.0f, 0.72f, 0.36f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(13, kInk, 0.5f),
          .note = label(11, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kPanel};
}

/** THE BANNER, as one column laid out at the full strip length. Sector
 *  spacing is `grow()` boxes rather than fixed gaps: the column is told
 *  how long it is and distributes the slack itself. */
Element banner(float length) {
  const char8_t* pool[4] = {
      u8"the across-vector is the whole difference",
      u8"a hung rail keeps the cloth flat to the ground",
      u8"transport only promises the SMALLEST turn",
      u8"one loop, one banner, two rails",
  };
  Element column = box()
                       .column()
                       .alignItems(Align::Center)
                       .width((float)kAcrossPx)
                       .height(length)
                       .padding(14, 48)
                       .fill(Fill::color({0.031f, 0.047f, 0.086f, 0.62f}));
  column.child(box().absolute().inset(3, 0, (float)kAcrossPx - 6, 0).fill(
      Fill::color({kAccent.fR, kAccent.fG, kAccent.fB, 0.9f})));
  column.child(box().absolute().inset((float)kAcrossPx - 5, 0, 3, 0).fill(
      Fill::color({kAccent.fR, kAccent.fG, kAccent.fB, 0.5f})));
  column.child(text(u8"THE HUNG RAIL", label(60, kAccent)));
  for (int s = 0; s < kSectors; ++s) {
    column.child(box().grow());
    char numeral[16];
    std::snprintf(numeral, sizeof(numeral), "- %02d -", s + 1);
    column.child(text(toU8(numeral), label(34, kNumeral)));
    column.child(text(pool[(size_t)s % 4], label(40, kInk)));
  }
  column.child(box().grow());
  column.child(text(u8"and back to its own beginning", label(46, kAccent)));
  return column;
}

/** The loop both panels ride: a winding whose 3 wraps and 2 turns share
 *  no factor, so no later wrap retraces an earlier one. */
curve::Spline3 loop() {
  return wkit::winding({.shell = {320, 150, 250}, .wraps = 3, .turns = 2});
}

camera::Camera view() {
  camera::Camera c;
  c.eye = {40, 190, 900};
  c.target = {0, 0, 0};
  c.fovYDeg = 44;
  return c;
}

/** One panel: the cloth on its rail, then the across-vector at every
 *  station, struck from the curve and projected with the same camera. */
void paintRail(SkCanvas& canvas, const std::vector<curve::Frame3>& rail,
               const sk_sp<SkImage>& art) {
  const SkSize viewport = {kPanel, kPanelH};
  const camera::Camera camera = view();

  const mesh::Mesh cloth =
      curve::sweep(rail, curve::profile::line(),
                   {.scale = kWidth,
                    .normals = curve::SweepOptions::Normals::Frame});
  render::MeshStyle style;
  style.texture = art;
  style.baseColor = {1, 1, 1, 1};
  style.ambient = {1, 1, 1, 1};
  style.lights = {};
  style.specular = 0;
  style.backfaceCull = false;
  render::drawMesh(canvas, cloth, glm::mat4(1.0f), camera, viewport, style);

  // The ticks. Projected exactly as the painter projects a vertex:
  // clip = viewProjection * p, then the perspective divide.
  const glm::mat4 vp = camera.viewProjection(viewport);
  const auto project = [&vp](glm::vec3 p, SkPoint* out) {
    const glm::vec4 clip = vp * glm::vec4{p, 1};
    if (clip.w <= 1e-4f) return false;
    *out = {clip.x / clip.w, clip.y / clip.w};
    return true;
  };
  SkPaint tick;
  tick.setAntiAlias(true);
  tick.setStyle(SkPaint::kStroke_Style);
  tick.setStrokeWidth(1.6f);
  tick.setColor4f(kTick);
  const int stride = std::max(1, (int)rail.size() / kStations);
  for (size_t i = 0; i < rail.size(); i += (size_t)stride) {
    const curve::Frame3& f = rail[i];
    SkPoint a, b;
    if (!project(f.position - f.binormal * (kWidth * 0.5f), &a)) continue;
    if (!project(f.position + f.binormal * (kWidth * 0.5f), &b)) continue;
    canvas.drawLine(a, b, tick);
  }
}

}  // namespace

struct YarnMarquee final : sketch::Sketch {
  sk_sp<SkImage> art;
  std::vector<curve::Frame3> transported;
  std::vector<curve::Frame3> hung;

  Element panel(const char* call, const char* note, std::string key,
                const std::vector<curve::Frame3>* rail) const {
    return kit::cell(voice(), toU8(call), toU8(note),
                     custom(std::move(key),
                            [this, rail](SkCanvas& canvas,
                                         const PaintContext&) {
                              paintRail(canvas, *rail, art);
                            })
                         .width(kPanel)
                         .height(kPanelH)
                         .fill(Fill::color(kCellGround)));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1200, 660);
    ctx.background(kGround);
    // Both rails are computed from the loop and nothing reads the clock.
    ctx.captureAt(0.05);

    // The banner, once — both panels wear the same cloth, so the only
    // difference between them is which rail carries it.
    if (ctx.fonts) {
      const sk_sp<SkPicture> picture =
          snapshot(box().child(banner((float)kBannerPx)), *ctx.fonts,
                   {(float)kAcrossPx, (float)kBannerPx});
      sk_sp<SkSurface> surface = SkSurfaces::Raster(
          SkImageInfo::MakeN32Premul(kAcrossPx, kBannerPx));
      SkCanvas* c = surface->getCanvas();
      c->clear(SK_ColorTRANSPARENT);
      // A swept band charts u ACROSS the profile from the side the wall
      // is read from, so an unmirrored tile reads backwards.
      c->translate((float)kAcrossPx, 0);
      c->scale(-1, 1);
      if (picture) c->drawPicture(picture);
      art = surface->makeImageSnapshot();
    }

    const curve::Spline3 rail = loop();
    transported = curve::frames(rail, kSections);
    hung = curve::hangFrames(rail, kSections, 1.0f, 1.0f);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE HUNG RAIL \xc2\xb7 curve::hangFrames against "
                           "curve::frames"),
             .subtitle = toU8("one closed winding, one banner, a two-point "
                              "line profile \xe2\x80\x94 the ticks are each "
                              "frame's across-vector at the band's own "
                              "width"),
             .footer = toU8("painter order is the depth test \xc2\xb7 the "
                            "cull is off, so both faces of a cloth show"),
             .titleStyle = label(15, kInk, 2.0f),
             .subtitleStyle = label(11, kAsh, 0.6f),
             .footerStyle = label(10.5f, kAsh, 0.2f),
             .marginX = 30,
             .marginTop = 22,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {panel("curve::frames(loop, 220)",
                            "parallel transport \xe2\x80\x94 the smallest "
                            "turn from one frame to the next, and no "
                            "relation to the world: the ticks tilt and the "
                            "banner rolls onto its edge",
                            "transported", &transported),
                      panel("curve::hangFrames(loop, 220, head 1, span 1)",
                            "the hang direction \xe2\x80\x94 straight down, "
                            "made perpendicular to the tangent: the ticks "
                            "stay level and the banner never turns over",
                            "hung", &hung)},
                 .gap = 20}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(YarnMarquee, "Kit \xc2\xb7 API",
             "the hung rail \xe2\x80\x94 one closed winding swept twice, "
             "over parallel-transport frames and over hangFrames, with "
             "each rail's across-vector struck at every station")
