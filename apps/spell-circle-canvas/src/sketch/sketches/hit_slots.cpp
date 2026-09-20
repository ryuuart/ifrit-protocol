/** @file
 * hit_slots — the read-back queries, and the two update domains they
 * drive over one scene.
 *
 * A slot is a named mount point in a described tree, and `renderSlot`
 * replaces what is under it WITHOUT re-describing anything else. That is
 * the frame of the sketch: the world below is described ONCE and its
 * caches stay warm for the life of the run, while two small regions above
 * it churn at completely different rates.
 *
 *   THE WORLD   nine keyed targets standing on the probe's own path, and
 *               eight keyed wires between them, described once in setup
 *               and never again. Nothing in it is a rectangle, so the hit
 *               test has to answer against real outlines rather than
 *               bounds.
 *   THE PROBE   a slot re-rendered EVERY frame, because a marker that
 *               follows a moving point is new content every frame and
 *               there is nothing to compare.
 *   THE ANSWER  a slot re-rendered ONLY when the answer changes. Most
 *               frames the probe is still over what it was over last
 *               frame, and re-describing then would be work with no
 *               different pixels at the end of it.
 *
 * The two together are the point. A slot is not a cheap re-render; it is
 * a BOUNDARY, and what it buys is that the expensive tree behind it never
 * hears about either of them. Whether a given slot re-renders per frame
 * or per change is then an ordinary question about that slot's content,
 * answered twice here with two different answers.
 *
 * THE THREE QUERIES, all in the answer slot, all asked of the RESOLVED
 * tree rather than of the numbers that described it:
 *   hitTest(pt)     walks back to front and returns the KEY of the
 *                   topmost node whose OUTLINE contains the point — so a
 *                   blob's concave bay reads as a miss and a star's arm
 *                   reads as a hit, and a node with no key is transparent
 *                   to it.
 *   bounds(key)     the resolved rect of that node. The ring is placed
 *                   FROM it, never from the numbers that described the
 *                   target: a second copy of the arithmetic is a second
 *                   thing to keep in step.
 *   routesAt(key)   the edge store's back-index — which keyed wires are
 *                   anchored on that node. The wires it names light up.
 *
 * The order of operations is the shape of all three: queries answer
 * against the resolved tree, so the scene is described first, the answers
 * are read on the next frame, and the overlay is described from them —
 * the data path, not a paint-time peek.
 *
 * EDIT THESE FIRST
 *   the Lissajous rates in walk() — where the probe goes, and therefore
 *                                   how often the answer changes.
 *   kTargets — more keyed outlines under the same one-pass hit test.
 */

// TAGS: Runtime/Interaction

#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {900, 600};
constexpr int kTargets = 9;
/** When the probe stands on the first target, and how long it takes to
 *  reach the next one. */
constexpr double kFirstStation = 0.8;
constexpr double kStationGap = 1.05;

constexpr SkColor4f kWire{0.42f, 0.46f, 0.62f, 0.55f};
constexpr SkColor4f kLit{1.0f, 0.71f, 0.42f, 1};

/** The house sheet warmed to this one's own ink, so every line on it is
 *  set from the theme rather than from a colour restated beside it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ink = {0.94f, 0.88f, 0.69f, 1};
  look.palette.ash = {0.58f, 0.60f, 0.70f, 1};
  return look;
}

std::string targetKey(int i) { return "target-" + std::to_string(i); }
std::string wireKey(int i) { return "wire-" + std::to_string(i); }

/** Where the probe is at scene time @p seconds: a Lissajous figure, so
 *  it crosses the field at an angle that never repeats over the run. */
SkPoint walk(double seconds) {
  const float w = kCanvas.width(), h = kCanvas.height();
  return {w * 0.5f + w * 0.36f * (float)std::sin(seconds * 0.9),
          h * 0.46f + h * 0.30f * (float)std::sin(seconds * 0.53 + 1.0)};
}

/** THE TARGETS STAND ON THE PROBE'S OWN PATH, one per station. A hit
 *  test only says something when there is something under the point, so
 *  the crossings are placed rather than hoped for: at station i the
 *  probe is exactly on target i's centre, and between stations it is
 *  somewhere in between and usually on nothing. */
double stationTime(int i) { return kFirstStation + (double)i * kStationGap; }

}  // namespace

struct HitSlots {
  std::string hitLabel = "—";
  std::optional<SkRect> hitBounds;
  std::vector<std::string> hitRoutes;
  SkPoint probe{0, 0};

  /** THE WORLD: nine keyed outlines standing on the probe's path, eight
   *  keyed wires threading them, plus the two slots the churn happens
   *  in. Described once. */
  Element describe() const {
    // ONE TARGET PER STATION, standing where the probe will be at that
    // station: a hit test only says something when there is something
    // under the point, so every crossing is placed rather than hoped for.
    const auto target = [](int i) {
      const float hue = (float)i / (float)kTargets;
      const SkPoint at = walk(stationTime(i));
      const float w = 108.0f + (float)(i % 3) * 24.0f;
      const float h = 100.0f + (float)(i % 4) * 20.0f;
      return box()
          .key(targetKey(i))
          .width(w)
          .height(h)
          .inset(at.x() - w * 0.5f, at.y() - h * 0.5f, 0, 0)
          .shape(i % 2 == 1 ? Shape(shapes::star(5 + i % 3, 0.5f))
                            : Shape(shapes::blob((uint32_t)(60 + i), 0.32f, 6)))
          .fill(Fill::color(
              {0.22f + 0.45f * hue, 0.30f + 0.22f * hue, 0.62f, 0.85f}))
          .foreground(stroke(1.6f, Fill::color({0.85f, 0.95f, 1, 0.45f})));
    };
    const auto stations = std::views::iota(0, kTargets);

    return stack()
        .fill(linearGradient(
            {0, 0}, {0, kCanvas.height()},
            {{0.07f, 0.06f, 0.13f, 1}, {0.16f, 0.09f, 0.20f, 1}}, {0.0f, 1.0f}))
        .children(
            {stack().inset(0).zIndex(0).children({each(stations, target)}),
             // The wires: keyed, so `routesAt` can name them. A keyless
             // route is anchored but unaddressable, and the back-index
             // omits it.
             stack().inset(0).zIndex(1).hitTestable(false).children(
                 {each(stations | std::views::take(kTargets - 1),
                       [this](int i) {
                         return wire(i, 1.6f, kWire).key(wireKey(i));
                       })}),
             // Both slots opt OUT of the hit test. A slot's name is its
             // key, and a full-canvas marker slot would otherwise be the
             // topmost keyed node under every point the probe visits — the
             // readout would answer with the probe's own name, for ever.
             slot("probe").inset(0).zIndex(6).hitTestable(false),
             slot("answer").inset(0).zIndex(5).hitTestable(false)});
  }

  /** ONE WIRE between two neighbouring stations, arcing to whichever side
   *  the parity of its index gives it. The LIT copy in the answer slot is
   *  described exactly like the dim one, which is what lands it on top of
   *  it: a route is a derivation of the two nodes' resolved bounds. */
  Element wire(int i, float width, SkColor4f ink) const {
    return connector(targetKey(i), targetKey(i + 1),
                     routers::arc(i % 2 == 0 ? 0.22f : -0.22f), 6)
        .hitTestable(false)
        .stroke(stroke(width, Fill::color(ink)));
  }

  /** The probe marker: new content on every frame, which is why its
   *  slot is re-rendered on every frame. */
  Element probeDot() const {
    return stack().inset(0).hitTestable(false).children(
        {kit::dot(probe, 5, Fill::color(SkColors::kWhite)),
         kit::ring(probe, 10, stroke(1.5f, Fill::color(SkColors::kWhite)))});
  }

  /** THE ANSWER: the ring placed from `bounds`, the wires `routesAt`
   *  named drawn again lit, and the three answers printed. Re-described
   *  only when `hitTest` returns something new. */
  Element answer() const {
    Element root = stack().inset(0).hitTestable(false);

    // THE RING IS PLACED FROM `bounds`, never from the numbers that
    // described the target: a second copy of the arithmetic is a second
    // thing to keep in step.
    if (hitBounds)
      root.children({box()
                         .rect(hitBounds->makeOutset(10, 10))
                         .borderRadius({16})
                         .foreground(stroke(2.5f, Fill::color(kLit)))
                         .hitTestable(false)});
    // The named routes, drawn again over their own dim selves.
    root.children({each(hitRoutes, [this](const std::string& route) {
      const size_t dash = route.rfind('-');
      return dash == std::string::npos
                 ? box()
                 : wire(std::stoi(route.substr(dash + 1)), 2.6f, kLit);
    })});

    std::string routes;
    for (const std::string& route : hitRoutes)
      routes += (routes.empty() ? "" : ", ") + route;
    if (routes.empty()) routes = "—";

    const std::string rect =
        hitBounds ? kit::formatted("%d, %d, %d × %d", (int)hitBounds->x(),
                                   (int)hitBounds->y(), (int)hitBounds->width(),
                                   (int)hitBounds->height())
                  : "—";

    // The three answers as a reading each: the name at the left in the
    // theme's quiet register, what the query came back with beside it in
    // the figure register, and the hit itself lit in the sheet's own ink.
    return root.children(
        {sketch::kit::readout(
             {{.name = "hitTest(probe)",
               .value = hitLabel,
               .ink = sketch::kit::theme().palette.ink},
              {.name = "bounds(\"" + hitLabel + "\")", .value = rect},
              {.name = "routesAt(\"" + hitLabel + "\")", .value = routes}},
             {.nameMeasure = 196})
             .absolute()
             .left(20)
             .bottom(14)
             .hitTestable(false)});
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(
        ctx, {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
              .captureAt = stationTime(4),
              .background = SkColor4f{0, 0, 0, 1}});  // standing on target-4
    hitLabel = "—";
    hitBounds.reset();
    hitRoutes.clear();
    probe = walk(0.0);
    ctx.composer.render(describe());
    ctx.composer.renderSlot("probe", probeDot());
    ctx.composer.renderSlot("answer", answer());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    // The answer is described AGAIN whenever it changes, and a describe
    // outside the scope setup opened has no theme bound: the look must be
    // in scope wherever the tree is built, not only where it is first
    // built.
    const sketch::kit::Provide look(sheetTheme());
    Composer& composer = ctx.composer;
    probe = walk(elapsed);
    // Per frame: the marker moved, so its content is different.
    composer.renderSlot("probe", probeDot());
    // Per change: most frames the answer is the one already on screen.
    std::string found = composer.hitTest(probe).value_or("—");
    if (found == hitLabel) return;
    hitLabel = std::move(found);
    hitBounds = composer.bounds(hitLabel);
    hitRoutes = composer.routesAt(hitLabel);
    composer.renderSlot("answer", answer());
  }
};

SIGIL_SKETCH(HitSlots, "Kit · API",
             "the read-back queries over one scene described once "
             "— hitTest naming a target, bounds placing the ring "
             "on it and routesAt lighting its wires, in a slot that "
             "re-renders only when the answer changes")
