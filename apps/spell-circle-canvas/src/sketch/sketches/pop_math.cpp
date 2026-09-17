/** @file
 * pop_math — the point operators that write a lane, and the two that
 * change what the set IS.
 *
 * Every operator here addresses attributes BY NAME. The conventional
 * lanes — P, T, Dir, Scale, Color, Tex — are only well-known names; any
 * other name creates a custom float4 on first write, flows through every
 * filter after it, and comes out on the cooked cloud. That is what makes
 * a mask a lane rather than a parameter: `Select` writes one from a
 * region of space, and every per-point filter's `mask` field reads a
 * lane's .x as how much of its write each point receives.
 *
 * Three of the eight are not per-point maps, and the boundary is worth
 * seeing on one sheet. `Delete` changes the COUNT, which no map can do.
 * `Normal` reads a point's own position to decide a sense. `Mix` reads
 * two lanes and writes a third, which is still per-point but is the op
 * that makes one attribute a function of another.
 *
 * EDIT THESE FIRST
 *   kMotes   — points in the cloud every cell starts from.
 *   kFactor  — the Mix weight.
 *   kFeather — the fraction of Select's extent that fades.
 */

// TAGS: Geometry/Points

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;

using namespace sigil::compose;
namespace pop = sigil::geometry::mesh::pop;

namespace {

constexpr SkSize kCanvas = {1200, 920};
constexpr float kCell = 268;
constexpr float kPicture = 218;

constexpr int kMotes = 4200;      // points every cell starts from
constexpr float kFactor = 0.55f;  // the Mix weight
constexpr float kFeather = 0.6f;  // the fraction of Select's extent that fades

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  look.type.captionLabel = {.size = 11.5f, .track = 0.8f};
  return look;
}

camera::Camera stage() {
  camera::Camera view;
  view.eye = {0, 120, 330};
  view.target = {0, 0, 0};
  view.fovYDeg = 40;
  return view;
}

points::BillboardStyle splat() {
  points::BillboardStyle style;
  style.size = 2.4f;
  // The lanes a cook exports, named: a chain that varied either shows it
  // whether the cloud is splatted the moment it is cooked or long after.
  style.sizeLane = "size";
  style.tintLane = "tint";
  style.additive = false;
  return style;
}

/** The cloud every cell starts from: points on a torus, tinted along
 *  their own T so a rewrite of any other lane is visible against a
 *  colouring that did not change. */
pop::Builder base() {
  return pop::on(gm::torus(74, 24, 64, 32), kMotes)
      .seed(3)
      .rampBy({{0.34f, 0.60f, 0.96f, 1}, {0.98f, 0.68f, 0.32f, 1}});
}

/** ONE CELL: a cloud the sheet cooked once, splatted once, held as an
 *  image.
 *
 *  The sink closes over the COOKED points rather than over the chain that
 *  describes them. Nothing on this sheet moves, so the chain answers the
 *  same cloud every time it is asked, and a chain cooked inside the paint
 *  program would be cooked again on every frame the well is painted.
 *
 *  TEXTURE, NOT PICTURE, and at this count that is the whole cost. Every
 *  point in the cloud is one image draw, and a recording replays every one
 *  of them on every frame — thousands of canvas calls to arrive at a
 *  picture that has not changed since the cloud was cooked. A texture is
 *  the same pixels reached once and blitted after. */
Element cloudFigure(const char* key, gm::Cloud cloud) {
  return sketch::kit::well(
      {.width = kCell, .height = kPicture},
      custom(key, [cloud = std::move(cloud)](SkCanvas& canvas,
                                             const PaintContext& pc) {
        points::drawBillboards(canvas, cloud, stage(), pc.size, splat());
      }).cache(Cache::Texture));
}

}  // namespace

struct PopMath {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // The selection every masked cell reads, and the two halves Delete
    // cuts the set into — counted here so the captions can say so.
    const auto selected = [] {
      return base().select("core", pop::Select::Shape::Sphere, {0, 0, 74},
                           {70, 70, 70}, kFeather);
    };
    const size_t kept = selected().keep("core").cloud().size();
    const size_t dropped = selected().drop("core").cloud().size();

    ctx.composer.render(sketch::kit::page(
        {.title = "One cloud, different questions",
         .subtitle = "4,200 points on a torus · one seed, one camera, one "
                     "scale throughout",
         .footer = "Selection writes a continuous lane. A masked operator "
                   "weights its edit by that lane; keep() changes the "
                   "membership of the cloud."},
        box().column().gap(26).children(
            {text("01 / REWRITE A VALUE").styleClass("section"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SOURCE",
                        .control = "colour follows T",
                        .figure = cloudFigure("source", base().cloud()),
                        .note = "The unchanged torus is the reference for "
                                "every edit."},
                       {.title = "STRETCH",
                        .control = "P.y × 2.4",
                        .figure = cloudFigure(
                            "stretch", base()
                                           .operation(pop::Math{
                                               pop::Lane::P, {1, 2.4f, 1, 1}})
                                           .cloud()),
                        .note = "Multiply one component. The colour lane stays "
                                "intact."},
                       {.title = "SHEAR + ROTATE",
                        .control = "Affine · rotate × shear",
                        .figure = cloudFigure(
                            "affine",
                            base()
                                .affine(glm::rotate(glm::mat4(1.0f), 0.5f,
                                                    glm::vec3{0, 0, 1}) *
                                        glm::mat4{1, 0, 0, 0, 0.55f, 1, 0, 0, 0,
                                                  0, 1, 0, 0, 0, 0, 1})
                                .cloud()),
                        .note = "One matrix acts on position. Translation is "
                                "excluded for directions."},
                       {.title = "RECOLOUR",
                        .control = "Lookup · height −24…24",
                        .figure = cloudFigure(
                            "lookup", base()
                                          .rampBy(pop::Lane::P, 1,
                                                  {{0.10f, 0.14f, 0.30f, 1},
                                                   {0.30f, 0.85f, 0.72f, 1},
                                                   {1.00f, 0.95f, 0.55f, 1}},
                                                  -24, 24)
                                          .cloud()),
                        .note = "Only tint changes. Low, middle and high "
                                "points find new colours."}},
                  .measure = 1120,
                  .gap = 16}),
             text("02 / CHOOSE WHERE THE EDIT LANDS").styleClass("section"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "FEATHER A MOVE",
                        .control = "Select core → move +58y",
                        .figure = cloudFigure(
                            "masked",
                            selected()
                                .masked("core")
                                .operation(pop::Math{
                                    pop::Lane::P, {1, 1, 1, 1}, {0, 58, 0, 0}})
                                .cloud()),
                        .note = "A soft selection blends the displacement into "
                                "untouched points."},
                       {.title = "DRAW TO AN ANCHOR",
                        .control = "Fill anchor → Mix 55%",
                        .figure = cloudFigure(
                            "mix", base()
                                       .fill("anchor", {0, 86, 0, 1})
                                       .mix(pop::Lane::P, "anchor",
                                            pop::Lane::P, kFactor)
                                       .cloud()),
                        .note = "Every position travels the same fraction "
                                "toward a named value."},
                       {.title = "PUSH OUTWARD",
                        .control = "Normal outward → Peak 34",
                        .figure = cloudFigure(
                            "peak",
                            base().normal(1.0f, {0, 0, 0}).peak(34).cloud()),
                        .note = "Normalize the direction before pushing each "
                                "point along it."},
                       {.title = "KEEP THE SELECTION",
                        .control = "Delete · keep core",
                        .figure = cloudFigure("keep",
                                              selected().keep("core").cloud()),
                        .note =
                            kit::formatted("%zu kept · %zu removed. Every lane "
                                           "follows the same permutation.",
                                           kept, dropped)}},
                  .measure = 1120,
                  .gap = 16})})));
  }
};

SIGIL_SKETCH(PopMath, "Kit · API",
             "eight point operators over one scattered torus: the lane "
             "rewrites, the mask a Select writes, and the two that change "
             "what the set is")
