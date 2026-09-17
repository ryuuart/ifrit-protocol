/** @file
 * blur_falloff — `Effect::blur(Paint sigmaMap, float maxSigma)`: a blur
 * whose sigma is a FUNCTION OF POSITION.
 *
 * Every panel holds the same content and the same maximum sigma; only the
 * MAP differs, and the map is an ordinary paint — which is the whole
 * point. Unit-space ramps are authored against whatever box the layout
 * decides, so a focal falloff is one line and needs no pixel arithmetic.
 *
 *   1 a constant blur, for contrast: all legible or none of it.
 *   2 DEPTH OF FIELD — a 3-stop linearUnit down the box.
 *   3 A LENS EDGE — glowUnit from the centre: sharp on axis, soft at the
 *     rim.
 *   4 RACK FOCUS — the same map, maxSigma BOUND inside the declared
 *     range: no re-describe, and the Gaussian passes are held while only
 *     the mix between them moves.
 *
 * The content is the library's own repeating tile rather than a shader
 * written here: fine rules are what a blur destroys visibly, and
 * `pattern::sequence` is the generator for a run of coloured bands.
 *
 * EDIT THESE FIRST
 *   kMaxSigma  — the blur radius the map's 1.0 end reaches. Every panel
 *                shares it, so raising it separates the panels further and
 *                lowering it makes panel 1 and panel 2 converge.
 *   kFocal     — where the sharp line sits down panel 2's box, 0..1. At 0
 *                the falloff becomes a plain top-to-bottom ramp.
 *   kRackHz    — how fast panel 4 breathes.
 *   the map in dofMap() — swap linearUnit for radialUnit, or add stops,
 *                and only the FALLOFF changes: the effect, the content and
 *                the node are untouched.
 *
 * Of the three ways a scene can move, panel 4 is the driven one: nothing
 * re-describes, an Output does the moving. The bound Output IS the
 * volatility declaration — a live map or a live maxSigma lifts the whole
 * effect to live, so no bake can sample the parameter once and freeze it.
 */

// TAGS: Materials/Compositing

#include <include/effects/SkImageFilters.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace mat = sigil::material;
namespace mskia = sigil::material::skia;
namespace ptn = sigil::material::pattern;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 240.0f;
constexpr float kMaxSigma = 14.0f;  // the map's 1.0 end, in px of sigma
constexpr float kFocal = 0.42f;     // panel 2's sharp line, 0..1 down the box
constexpr double kRackHz = 0.18;    // panel 4's breathing rate

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.92f, 0.94f, 0.98f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 40;
  look.spacing.captionGap = 6;
  return look;
}

/** THE CONTENT, one function so every panel blurs the SAME picture: fine
 *  rules 7 px apart (detail a blur destroys visibly) under three discs.
 *  The tile is baked once and repeated; the rotation only remaps the
 *  sampling, so the run stays seamless. */
mskia::Paint rules() {
  return mskia::Paint::shader(
      ptn::sequence({{3.5f, mat::rgb(0x293147)}, {3.5f, mat::rgb(0x9eb3db)}})
          .rotate(90)
          .texture()
          .shader());
}

Element subject() {
  return stack().width(kPanel).height(kPanel).fill(rules()).children(
      {kit::dot({64, 68}, 38, mskia::Paint::solid({0.98f, 0.44f, 0.34f, 1})),
       kit::dot({122, 122}, 26, mskia::Paint::solid({0.42f, 0.86f, 0.72f, 1})),
       kit::dot({166, 180}, 48,
                mskia::Paint::solid({0.96f, 0.82f, 0.36f, 1}))});
}

/** DEPTH OF FIELD: three stops down the unit square — max sigma at the
 *  top edge, zero at the focal line, max again at the bottom. */
mskia::Paint dofMap() {
  return mskia::Paint::linearUnit(
      {0, 0}, {0, 1},
      {{0.0f, {1, 1, 1, 1}}, {kFocal, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

/** A LENS EDGE: zero on axis, max at the inscribed circle. glowUnit is
 *  the one that means "fills this box" (radialUnit reaches the
 *  corners). */
mskia::Paint lensMap() {
  return mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

sketch::kit::ComparisonCase panel(const char* caseTitle, const char* call,
                                  const char* note, mskia::Paint map,
                                  mskia::Effect e, std::string key) {
  return {.title = caseTitle,
          .control = call,
          .figure = box().column().gap(12).children(
              {box().width(kPanel).height(74).fill(std::move(map)),
               subject().key(std::move(key)).effect(std::move(e))}),
          .note = note};
}

}  // namespace

struct BlurFalloff {
  choreograph::Output<float> rack{0.0f};  // panel 4's bound maxSigma

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // the top of panel 4's breath: 1 / (2 kRackHz)
    sketch::kit::stage(ctx, {.size = {1100, 960}, .captureAt = 2.78});

    ctx.composer.render(sketch::kit::page(
        {.title = "Where does the blur fall?",
         .subtitle = "One source, four spatial controls",
         .footer = "The last result is live: a bound parameter changes the "
                   "blur without rebuilding the picture."},
        box().column().gap(28).children(
            {box()
                 .row()
                 .shrink(0)
                 .alignItems(Align::Start)
                 .gap(40)
                 .children(
                     {box().column().gap(12).children(
                          {sketch::kit::sectionHeader(
                               {.label = "THE SOURCE", .note = ""}),
                           subject()}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "READ THE MAP",
                                .note =
                                    "Dark means sharp; light means blurred."}),
                           text("The stripes reveal lost detail. Every output "
                                "uses this same picture and a maximum sigma of "
                                "14 pixels. The narrow map above each result "
                                "shows where that blur is applied.")
                               .width(660)
                               .styleClass("captionNote"),
                           box().width(660).height(42).fill(
                               mskia::Paint::linearUnit(
                                   {0, 0}, {1, 0},
                                   {{0, {0, 0, 0, 1}}, {1, {1, 1, 1, 1}}})),
                           box()
                               .row()
                               .alignItems(Align::Start)
                               .gap(20)
                               .children({text("BLACK  ·  sigma 0")
                                              .width(320)
                                              .styleClass("captionNote"),
                                          text("WHITE  ·  sigma 14")
                                              .width(320)
                                              .styleClass("captionNote")})})}),
             sketch::kit::sectionHeader(
                 {.label = "FOUR FALLOFFS",
                  .note = "Map above · resulting image below"}),
             sketch::kit::comparison(
                 {.cases =
                      {panel("UNIFORM", "filter(Blur(14, 14))",
                             "Every position receives the same blur.",
                             mskia::Paint::solid({1, 1, 1, 1}),
                             mskia::Effect::filter(SkImageFilters::Blur(
                                 kMaxSigma, kMaxSigma, nullptr)),
                             "flat"),
                       panel("DEPTH OF FIELD", "blur(linearUnit 3 stops, 14)",
                             "The dark horizon stays sharp; distance from it "
                             "increases blur.",
                             dofMap(), mskia::Effect::blur(dofMap(), kMaxSigma),
                             "dof"),
                       panel("LENS EDGE", "blur(glowUnit, 14)",
                             "The centre stays sharp while the edge softens.",
                             lensMap(),
                             mskia::Effect::blur(lensMap(), kMaxSigma), "lens"),
                       panel("RACK FOCUS", "blur(dofMap, 14) · live",
                             "The depth map stays fixed. Its maximum blur "
                             "breathes with time.",
                             dofMap(),
                             mskia::Effect::blur(dofMap(), kMaxSigma)
                                 .uniform("maxSigma", &rack),
                             "rack")},
                  .measure = 1020,
                  .gap = 20})})));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    (void)ctx;
    // Derived from `elapsed`, not accumulated: a still at a declared time
    // is then the same still every run. No re-describe — the bound
    // parameter makes the effect live all by itself.
    const double phase = elapsed * kRackHz * 6.2831853;
    rack = kMaxSigma * 0.5f * (float)(1.0 - std::cos(phase));
  }
};

SIGIL_SKETCH(BlurFalloff, "Kit · API",
             "Effect::blur(Paint, maxSigma) — one effect, four "
             "falloffs: constant, depth of field, a lens edge, a rack focus")
