// yarn_marquee.cpp — ONE COLUMN OF TYPE, WOUND ROUND A BALL.
// =============================================================================
// A banner that reads continuously around a winding is one long picture
// and a rail to hang it on, and neither half needs a device.
//
//   THE PICTURE  a single compose column — a rule down each side, a
//     heading, then numbered sectors with `grow()` boxes between them so
//     the spacing distributes itself over whatever height the column is
//     given. It is baked ONCE, whole, and each arc cuts its own quarter
//     out of it with `MeshStyle::uvTransform` — which also carries the
//     mirror in x, because a swept ribbon charts u ACROSS the profile
//     and the wall therefore samples it from the far side. Slicing the
//     picture into four surfaces would say the same thing with four
//     rasters and a canvas transform; a uv matrix says it with neither.
//   THE RAIL     `curve::hangFrames` over one quarter of a closed
//     spline, four windows covering it end to end. Hang frames put the
//     binormal along world-down, so the band never rolls upside down
//     where the winding turns back on itself — which is exactly where a
//     parallel-transport rail would.
//   THE CLOTH    `curve::sweep` with a two-point line profile. A ribbon
//     is the same primitive a tube is, with a different cross-section.
//
// PAINTER ORDER IS THE DEPTH TEST. There is none of the other kind
// here: the arcs are sorted by the distance from the camera to each
// mesh's own bounds centre and drawn farthest first. Wraps that pass
// through one another accept that honestly, which is the trade a
// painter makes and the reason the cull is off — both faces of a cloth
// show.
//
// EDIT THESE FIRST
//   kTiles     — arcs around the ball; each is one window of the rail.
//   kWidth     — the band's width in world units.
//   the winding's lat/azi factors — how many times the yarn crosses
//                itself, which is what makes the sort do any work.

#include <include/core/SkMatrix.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace camera = sigil::geometry::mesh::camera;
namespace curve = sigil::geometry::mesh::curve;
namespace mesh = sigil::geometry::mesh;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};
constexpr float kWidth = 100;   // the band's world width
constexpr int kAcrossPx = 300;  // the column's pixel width
constexpr int kTilePx = 4096;   // one tile's pixel height
constexpr int kTiles = 4;       // arcs, and slices of the column
constexpr int kSectors = 48;
/** Where the winding is watched from. The arcs are sorted by how far
 *  each one's bounds centre stands from it, so the sort and the camera
 *  must be the same point. */
constexpr glm::vec3 kEye{40, 140, 980};

const SkColor4f kInk = {0.925f, 0.957f, 0.996f, 1};
const SkColor4f kAccent = {0.455f, 0.878f, 0.745f, 1};
const SkColor4f kNumeral = {0.588f, 0.659f, 0.769f, 1};

/** The banner's text, as one column laid out at the full strip height.
 *  Sector spacing is `grow()` boxes rather than fixed gaps: the column
 *  is told how tall it is and distributes the slack itself. */
Element strip(float height) {
  const char8_t* pool[6] = {
      u8"the same winding, no device anywhere",
      u8"a line profile on a hung rail forms the band",
      u8"the painter puts the cloth on the mesh",
      u8"text reads across, the column climbs",
      u8"one compose column, sliced to tiles",
      u8"raster end to end, the artist's backend",
  };
  auto column = box()
                    .column()
                    .alignItems(Align::Center)
                    .width((float)kAcrossPx)
                    .height(height)
                    .padding(18, 60)
                    .fill(Fill::color({0.031f, 0.047f, 0.086f, 0.47f}));
  column.child(
      box()
          .absolute()
          .inset(4, 0, (float)kAcrossPx - 7, 0)
          .fill(Fill::color({kAccent.fR, kAccent.fG, kAccent.fB, 0.9f})));
  column.child(
      box()
          .absolute()
          .inset((float)kAcrossPx - 6, 0, 4, 0)
          .fill(Fill::color({kAccent.fR, kAccent.fG, kAccent.fB, 0.5f})));
  column.child(
      text(u8"THE PAINTER'S YARN", type({.size = 52, .color = kAccent})));
  for (int s = 0; s < kSectors; ++s) {
    column.child(box().grow());
    char numeral[16];
    std::snprintf(numeral, sizeof(numeral), "- %02d -", s + 1);
    column.child(text(toU8(numeral), type({.size = 30, .color = kNumeral})));
    column.child(text(pool[s % 6], type({.size = 38, .color = kInk})));
  }
  column.child(box().grow());
  column.child(text(u8"and back to its own beginning",
                    type({.size = 42, .color = kAccent})));
  return column;
}

curve::Spline3 winding() {
  curve::Spline3 yarn;
  yarn.closed = true;
  for (int i = 0; i < 96; ++i) {
    const float t = (float)i / 96.0f;
    const float lat = std::sin(6.2831853f * 3.0f * t);
    const float azi = -6.2831853f * 2.0f * t;
    yarn.points.emplace_back(340.0f * std::cos(lat) * std::cos(azi),
                             10.0f + 200.0f * std::sin(lat),
                             280.0f * std::cos(lat) * std::sin(azi));
  }
  return yarn;
}

}  // namespace

struct YarnMarquee final : sketch::Sketch {
  struct Arc {
    mesh::Mesh cloth;
    float depth = 0;
    int tile = 0;
  };
  /** The scene behind the column, and the one picture it painted. The
   *  scene owns the surface, so it is held for as long as the image. */
  std::shared_ptr<TextureScene> column;
  sk_sp<SkImage> banner;
  std::vector<Arc> arcs;

  /** ONE ARC'S QUARTER OF THE COLUMN, in uv: mirrored in x, and the kth
   *  of kTiles bands down the picture. */
  static SkMatrix window(int k) {
    return SkMatrix::Translate(1.0f, (float)k / (float)kTiles) *
           SkMatrix::Scale(-1.0f, 1.0f / (float)kTiles);
  }

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = kEye;
    view.target = {0, 10, 0};
    view.fovYDeg = 46;
    for (const Arc& arc : arcs) {
      render::MeshStyle cloth;
      cloth.texture = banner;
      cloth.uvTransform = window(arc.tile);
      cloth.baseColor = {1, 1, 1, 1};
      cloth.lit = false;
      cloth.backfaceCull = false;
      render::drawMesh(canvas, arc.cloth, glm::mat4(1.0f), view, kCanvas,
                       cloth);
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.031f, 0.031f, 0.051f, 1});
    ctx.captureAt(1.0);

    // The picture, once and whole. What each arc shows is a window onto
    // it, and the window is a uv matrix rather than a slice.
    if (ctx.fonts) {
      column = TextureScene::make({kAcrossPx, kTiles * kTilePx}, *ctx.fonts);
      column->render(strip((float)kTiles * kTilePx));
      banner = column->image();
    }

    const curve::Spline3 yarn = winding();
    for (int k = 0; k < kTiles; ++k) {
      Arc arc;
      arc.cloth = curve::sweep(
          curve::hangFrames(yarn, 160, (float)(k + 1) / (float)kTiles,
                            1.0f / (float)kTiles),
          curve::profile::line(),
          {.scale = kWidth, .normals = curve::SweepOptions::Normals::Frame});
      glm::vec3 lo, hi;
      arc.cloth.bounds(&lo, &hi);
      arc.depth = glm::length((lo + hi) * 0.5f - kEye);
      arc.tile = k;
      arcs.push_back(std::move(arc));
    }
    std::sort(arcs.begin(), arcs.end(),
              [](const Arc& a, const Arc& b) { return a.depth > b.depth; });

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(YarnMarquee, "Kit",
             "one compose column sliced into tiles and hung on four "
             "windows of a ball winding — a marquee formed and painted "
             "with no device anywhere")
