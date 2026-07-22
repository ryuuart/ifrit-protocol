// astral_tome.cpp — ASTRAL SORCERY, THE ASTRAL TOME CONSTELLATION CLUSTER PAGE
// =============================================================================
// Astral Sorcery 1.12.2 (HellFirePvP, 2016-2019), a Minecraft 1.12.2 Forge mod.
// The screen: GuiJournalConstellationCluster page 0 — four constellations drawn
// as live star charts on one open spread, before you click through to any one
// of them. A chart of charts, and the only screen in the mod where four
// independent star fields overlap on one page.
//
// The tome is 420 x 270 GUI px (GuiScreenJournal calls super(270, 420) and
// GuiWHScreen's ctor is (guiHeight, guiWidth) — so 270 TALL, 420 WIDE). Rebuilt
// here at EXACTLY 3x: 1260 x 810, on a 1200 x 750 canvas, so the tome bleeds
// 30 canvas px off all four edges. That is not a styling choice — it is what
// falls out of 3x, and it lands the dark page plate (drawCstBackground fills
// GUI 15,10 .. 405,260) at canvas x 15..1185, y 0..750: full-height bleed top
// and bottom, 15 px of leather left and right. Every constant below is in GUI
// px and goes through g(); divide any canvas number by three to recover the
// mod's own pixel.
//
// -----------------------------------------------------------------------------
// SOURCE — fetched and read, not remembered
//
//   github.com/HellFirePvP/AstralSorcery, branch 1.12.2. Files pulled:
//     client/gui/GuiJournalConstellationCluster.java
//     client/gui/GuiJournalConstellationDetails.java
//     client/gui/journal/GuiScreenJournal.java
//     client/gui/base/GuiWHScreen.java
//     client/util/RenderConstellation.java
//     client/util/Blending.java
//     client/sky/RenderAstralSkybox.java
//     client/ClientProxy.java
//     common/registry/RegistryConstellations.java
//     common/constellation/IConstellation.java
//     common/data/config/Config.java
//     resources/assets/astralsorcery/textures/environment/star1.png
//     resources/assets/astralsorcery/textures/effect/connectionperks.png
//     resources/assets/astralsorcery/textures/gui/{guijspacebook,guijarrow,
//                                                  guiresbgcst}.png
//     resources/assets/astralsorcery/lang/en_US.lang
//
// -----------------------------------------------------------------------------
// FIVE CORRECTIONS TO MY OWN BRIEF, ALL FROM THE CODE
//
//  1. THE CHART IS NOT STRETCHED. THE CELL IS.  The brief's headline finding
//     was that a 31x31 square star grid goes into an 80x110 cell and every
//     constellation on this page is therefore squeezed 1.375x vertically. It
//     is not. GuiJournalConstellationCluster:238 reads
//
//         RenderConstellation.renderConstellationIntoGUI(display,
//                 0, 0, 0,
//                 95, 95, 2F, new BrightnessFunction() { … }, true, false);
//
//     — the render box is 95 x 95, SQUARE, so ulength == vlength == 95/31 ==
//     3.0645 and the aspect is exactly 1.000. `width = 80, height = 110` are
//     used for three other things and never for the render: the hit
//     Rectangle (:219), the hover pivot (:222), and the label centring (:252).
//     The detail page (GuiJournalConstellationDetails:566) renders 150 x 150,
//     also square. NOTHING in this mod stretches a constellation.
//
//     What IS true, and is the better finding, is that THE CHART OVERFLOWS ITS
//     OWN CELL. 95 > 80, so a chart whose stars reach grid x = 27 lands at
//     GUI x = 82.7 + one ulength of quad = 85.8, five px past the 80-wide hit
//     box that decides whether you clicked it. The four cells are pitched 80
//     apart with 95-wide charts in them: they interleave by 15 px on top of
//     the zero gutter the brief expected. Both are drawn here — the cell plate
//     is the 80 x 110 hit box, the chart is the 95 x 95 render, and the
//     difference is visible as furniture rather than asserted in a caption.
//
//  2. THE LABEL IS OFF-CENTRE, BY 7.5 GUI PX, FOREVER.  :252 is
//         float fullLength = (width / 2) - (fr.getStringWidth(trName) / 2F);
//     — `width` is 80, so the name centres on x = 40. The chart it labels is
//     95 wide and centres on 47.5. Every constellation name on this screen
//     sits 7.5 GUI px (22.5 canvas px) LEFT of the chart above it. Same
//     constant, same class, second consequence.
//
//  3. THE PIVOT BUG IS REAL AND IT IS WORSE THAN THE BRIEF SAID.  :222 is
//         GlStateManager.translate(offsetX + (width / 2), offsetY + (width / 2), …)
//     — width/2 on BOTH axes. Against the 110-tall hit box that is 15 px above
//     centre, which is the brief's reading; against the 95 x 95 chart actually
//     being scaled it is 7.5 px above AND 7.5 px left of centre. So a hovered
//     chart grows down and to the RIGHT, not just down. Shipped, un-fixed, at
//     the source's own 1.1 scale (:225 — the brief said 1.05).
//
//  4. IT IS NOT ADDITIVE.  The double pass is real — renderConstellationIntoGUI
//     :243 is `for (int j = 0; j < 2; j++)` around the whole connection set —
//     but Blending.java:23 is
//         DEFAULT(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA)
//     i.e. ordinary alpha compositing. ADDITIVE is a different enum member
//     (:27, GL_ONE/GL_ONE) and this path never selects it. So the double pass
//     does not bloom past the constellation's colour; it DEEPENS the link from
//     a toward 1-(1-a1)(1-a2), and because the two passes draw two separate
//     getBrightness() calls (two different divisors — see 5), the composite
//     beats between two periods instead of doubling one. That is reproduced
//     exactly here: every link is TWO sibling elements at two divisors,
//     alpha-composited, which is the source's own construction.
//     The one place kPlus appears on this canvas is the star cores, where it
//     is a declared departure and is labelled as one on the plate.
//
//  5. THERE IS NO MAJOR-TIER PAGE.  getConstellationScreen() (:82) is
//         return new GuiJournalConstellationCluster(20, "no.title", constellations);
//     over `ConstellationRegistry.resolve(client.getSeenConstellations())`,
//     with the whole tier-splitting branch COMMENTED OUT below it. The shipped
//     build has one cluster screen over everything you have seen, in registry
//     order, four to a page — and `unlocTitle` is stored, passed between pages,
//     and never drawn. So "major tier page 1" is a screen state the 1.12.2
//     build cannot produce; page 1 is simply the first four you have seen,
//     which on a full save is discidia / armara / vicio / aevitas because that
//     is registration order (RegistryConstellations:296-350). The brief's four
//     survive; its reason does not. Sixteen seen at four to a page is FOUR
//     pages, so this is page 1 of 4 and page 2 opens with evorsio — which is
//     why the "next" arrow is drawn (:173, `size() >= nextIndex + 1`) and
//     "prev" is not (:151, `cIndex > 0`). Both are on the canvas, and the
//     one that is absent is absent for a reason read off the source.
//
// -----------------------------------------------------------------------------
// THE NUMBERS, ALL FROM THE FILES
//
//   STAR_GRID_SIZE = 31                       IConstellation:32
//   CONSTELLATIONS_PER_PAGE = 4               Cluster:54  (the offsetMap comment
//   width = 80, height = 110                  Cluster:59   says "we put 6 on '1'
//   offsetMap 0..3 = (45,55) (125,105)        Cluster:288  page/screen". Four
//                    (200,45) (280,110)                    offsets exist. The
//   render box 95 x 95, linebreadth 2F        Cluster:240  constant is right.)
//   hover scale 1.1                           Cluster:225
//   arrow idle scale sin(t/4)/32 + 1          Cluster:163  (+/-3.1%, 25.13 tick
//   arrow hover scale 1.1                     Cluster:160   period = 1.257 s)
//   rectNext (367,125,30,15)                  Cluster:176
//   rectBack (197,230,30,15)                  Cluster:198
//   page plate (15,10)..(w-15,h-10)           Cluster:119  tinted 0.8,0.8,1,0.7
//   bookmark rail x = guiWidth - 17.25        Journal:113  67x15, gap 18, +5 px
//   bookmark labels                           ClientProxy:196-205  when unselected
//
//   stars           star.x, star.y on the 31-grid, quad 2*ulength square,
//                   CENTRED (starVec = origin + (x*u - u, y*v - v), extent 2u)
//                                                          Render:300-311
//   connections     quad of half-width `linebreadth` on the NORMALISED cross
//                   product — so the band is 2*linebreadth wide in FINAL
//                   screen space, uniform in every direction  Render:258-262
//   brightness      0.3 + 0.7 * conCFlicker(tick, partial, 12 + rand.nextInt(10))
//                   conCFlicker = sin(((t % (dayLength/2)) + partial)/d)/4 + 0.375
//                   dayLength = 24000 (Config:96), so the modulus is 12000 ticks
//                   => brightness = 0.5625 + 0.175*sin(...) in [0.3875, 0.7375]
//                   The stars never reach white and never go out.
//                                             Render:327-335, Cluster:243
//   seed            new Random(0x4196A15C91A5E199L), constructed INSIDE
//                   drawConstellationRect — so the sequence RESTARTS for every
//                   constellation and all four charts share one divisor list.
//                                             Cluster:228
//
// The divisor list, computed by running java.util.Random's LCG (see javaRand
// below) — the first 30 draws of 12 + nextInt(10):
//
//   12 16 15 16 15 20 12 18 20 19 18 17 13 20 14 18 15 13 20 19 14 18 21 15 …
//
// consumed in draw order: 2*C connection calls (pass 0 then pass 1), then S
// star calls. Periods are 2*pi*d ticks at 20 Hz = 3.77 s (d=12) to 6.60 s
// (d=21). Only TEN divisors exist, so this canvas needs exactly TEN live
// Outputs to twinkle 31 stars and 62 connection passes — which is the perf
// story below, and it is the source's structure, not a shortcut.
//
// -----------------------------------------------------------------------------
// LINE VOCABULARY — read off the sprite, not invented
//
// The brief called Astral Sorcery's connection "a flat constant-width quad
// drawn twice" and sent me to the Bayer/Bode engraved register for something
// better. Half of that turned out to be unnecessary: connectionperks.png is
// 64 x 64, white, and its ALPHA is a vertical profile —
//
//   v      0    8   16   24   28   30   32   34   36   44   52   60   63
//   alpha  1   14   52   95  134  205  248  194  124   72   26    4    1
//
// — a 0.5-px bright core inside a 4-px halo. That is a THREE-RAIL RULE, in the
// artefact, and lines::Rails is the exact shape of it. Integrating the profile
// in three bands and solving the alpha-over composite backwards gives the three
// rail alphas used below (0.097 / 0.309 / 0.580) — so the halo weights on this
// page are measured off the mod's own texture rather than chosen.
//
// What IS the departure, and is labelled as such on the plate:
//   * two DOTTED FLANK RAILS at +/-7.5 px, one phase-shifted half a period
//     against the other, so the two rows of dots interleave down the link.
//     Per-rail offset, width, fill, dash AND dashPhase in one value — the
//     whole point of Rails and the reason nothing before it could do this.
//   * a brushes::Ribbon body under the rails whose widthFn takes the band to
//     40% at both endpoints, so a link reads as drawn FROM star TO star. The
//     mod's quad cannot taper; a printed chart always does.
//   * shapes::star(4, 0.24, 0.16) for the glyphs, which is not a departure at
//     all — star1.png is a four-pointed star with concave arms reaching the
//     full 32 px of its cell, and the waist parameter is what draws that.
//     Radius varies by LINK DEGREE (armara's hub has 4, discidia's leaves 1),
//     which is real data off the link list.
//   * the cell plates are decorations::brackets + a gappedRule on the top edge
//     only (shapes::onEdges(Edge::Top, …)); the mod draws no cell border at
//     all, so the plate is entirely the study's apparatus and is drawn on the
//     80 x 110 HIT box so you can see the chart overflow it.
//   * the spread frame is doubleBorder + weightedCorners over a chamfered
//     inner, and the 31-tick declination ladder runs the key cell's flanks.
//
// -----------------------------------------------------------------------------
// THE ANISOTROPY REPORT — the predicted break, measured, and it is real
//                          (just not on this artefact)
//
// The brief predicted brushes::Ribbon would compute its normal in the parent's
// transformed space and horizontal links would come out THINNER than vertical
// ones under the 1.375x cell. There is no 1.375x cell (correction 1), so the
// question moved to a scratch probe — eight identical 12 px bands, four plain
// and four under `.scaleY(1.375f)`, two of each pair Ribbon and two Rails:
//
//     run2/probe/ribbon_aniso.{cpp,png}
//
//   Ribbon::paint (Brushes.h:1041-1088) samples ctx.outline, takes
//   n = (-tan.y, tan.x) and emits an explicit filled band of vertices. All of
//   that is in the NODE'S LOCAL SPACE; the element transform is applied to the
//   finished band. So under scaleY(k):
//       a HORIZONTAL run has a VERTICAL normal   -> band scales to k * w
//       a VERTICAL   run has a HORIZONTAL normal -> band stays at w
//
//   MEASURED off the probe PNG (longest lit run across the band, k = 1.375,
//   w = 12):
//                        plain      scaleY(1.375)
//       Ribbon  horiz     12 px        17 px        (12 x 1.375 = 16.5 + AA)
//       Ribbon  vert      12 px        12 px
//       Rails   horiz     12 px        17 px
//       Rails   vert      12 px        12 px
//
//   So: CONFIRMED as a real behaviour, REFUTED as stated. Horizontal runs come
//   out THICKER, not thinner — the prediction has the right mechanism and the
//   wrong sign, because it is the NORMAL that gets stretched, not the tangent.
//   lines::Rails behaves identically (offsetAlong and the stroke width are both
//   local-space), so the whole rail vocabulary shares the property.
//
// This sketch does not hit it, because it does what the mod does: positions are
// computed into final space by hand (u = v = 95/31) and every glyph is placed
// with centerAt() on a computed point, so no element on this canvas carries a
// non-uniform scale. The source is doing the same thing for the same reason —
// Render:259 normalises the cross product BEFORE multiplying by linebreadth,
// which is a device-space width by construction.
//
// The natural API, and the gap filed: `Ribbon::widthSpace = Local | Device`
// (and the same on Rail::width / Line::width), because the moment a chart is
// placed under a non-uniform scale — which any anisotropic cell does — a
// stroke vocabulary that measures in local space is measuring the wrong thing,
// and there is no way to say so short of un-scaling the container and
// pre-transforming every point yourself. Which is what this sketch does.
// Skia already has the concept: a hairline is device-space by definition, and
// SkPaint has no other way to spell it, so the seam belongs at ours.
//
// -----------------------------------------------------------------------------
// THE PERF STORY — ten Outputs, and a saveLayer the size of a star
//
// 31 stars and 62 connection passes all twinkle independently, which reads like
// 93 live bindings. It is not, and the reason is in the source: brightness is a
// pure function of (divisor, t), and the divisor is `12 + rand.nextInt(10)` —
// TEN values. So the sketch allocates ten ch::Output<float> and every star and
// every link pass binds to the one for ITS divisor. The plate, the field, the
// frame and the marginalia are two Cache::Texture bakes.
//
// The first cut then did the obvious thing with those ten Outputs: TWENTY
// GROUPS — ten full-canvas boxes holding the links, ten holding the stars, one
// .opacity(bind(&bright[d])) each, static geometry inside picture-caching once
// and replaying under a live alpha. That is the documented "paint-only
// volatility keeps the node's own content picture" contract and it is correct.
// It measured p50 16.51 / p99 18.38 ms — a FAIL — and --bench named the twenty
// groups as the six worst nodes, ~1.1 ms each, "box 1200x750".
//
//     A BOUND OPACITY IS A saveLayer OVER THE NODE'S BOX.
//
// Twenty of them at 1200 x 750 is 18 megapixels of layer allocated, cleared and
// composited every frame, for content occupying maybe 2% of it. Moving the
// binding off the groups and onto each primitive's own TIGHT box — the link's
// segment bbox, the star's 84 px halo square — is the same ten Outputs, the
// same draw order, the same pixels, and takes the layer budget to about a
// megapixel:
//
//     p50 16.51 / p99 18.38 ms   FAIL   20 groups, layer = the canvas
//     p50  7.57 / p99  7.99 ms   PASS   93 primitives, layer = the sprite
//
// The star elements had to be rebuilt for it: `starEl` was an .inset(0) group
// with centerAt() children, which is a full-canvas box however small its
// contents are. Grouping by divisor is otherwise safe here for a reason worth
// stating — within a constellation every link is the same colour and both
// passes are the same colour, and alpha-over of equal colours is
// order-independent in coverage (1-(1-a)(1-b) is symmetric) — but grouping is
// not what pays for itself; box size is.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace at {

// ---------------------------------------------------------------------------
// THE GRID. One scale for the whole page: 3 canvas px per GUI px.

constexpr float kU = 3.0f;               ///< canvas px per GUI px
constexpr float kCanvasW = 1200.0f, kCanvasH = 750.0f;
constexpr float kGuiW = 420.0f, kGuiH = 270.0f;   // GuiScreenJournal(270, 420)
constexpr float kGuiLeft = (kCanvasW - kGuiW * kU) * 0.5f;   // -30
constexpr float kGuiTop = (kCanvasH - kGuiH * kU) * 0.5f;    // -30

inline float g(float gui) { return gui * kU; }
inline float gx(float gui) { return kGuiLeft + gui * kU; }
inline float gy(float gui) { return kGuiTop + gui * kU; }

constexpr int kGrid = 31;                 // IConstellation:32
constexpr float kRenderBox = 95.0f;       // Cluster:240 — SQUARE
constexpr float kCellW = 80.0f, kCellH = 110.0f;  // Cluster:59 — the HIT box
constexpr float kUlen = kRenderBox / (float)kGrid;   // 3.0645 GUI px
constexpr float kLineBreadth = 2.0f;      // Cluster:240

using studio::hex;   // the same four lines as twenty-three other files
using studio::mul;
inline Decoration prog(PaintProgram p) { return Decoration(std::move(p)); }

// Palette, sampled out of the mod's own PNGs (see the header).
const SkColor4f kLeatherDark = hex(0x0A0800);   // guijspacebook, darkest bulk
const SkColor4f kLeatherMid = hex(0x2C1602);    // its commonest opaque colour
const SkColor4f kLeatherWarm = hex(0x634913);
const SkColor4f kGilt = hex(0x9B7A2D);          // its brightest
const SkColor4f kOlive = hex(0x7D6C00);         // guijarrow
const SkColor4f kOliveDim = hex(0x574E25);
const SkColor4f kNebula = hex(0x0B080B);        // guiresbgcst mean * (.8,.8,1)*.7
const SkColor4f kFieldStar = hex(0x8F8FB3);     // its white points, same tint
const SkColor4f kInk = hex(0xDDDDDD);           // Cluster:253 text 0xBBDDDDDD
constexpr float kInkAlpha = 0xBB / 255.0f;

// ---------------------------------------------------------------------------
// java.util.Random, exactly — the divisor sequence has to be byte-identical to
// the mod's or the twinkle is a different pattern that merely looks similar.
// 48-bit LCG, multiplier 0x5DEECE66D, addend 0xB; nextInt(bound) for a
// non-power-of-two bound is `next(31) % bound` with the overflow rejection.

struct JavaRand {
  uint64_t s;
  explicit JavaRand(uint64_t seed) : s((seed ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1)) {}
  int32_t next(int bits) {
    s = (s * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
    return (int32_t)(s >> (48 - bits));
  }
  int nextInt(int bound) {
    int32_t r = next(31);
    const int32_t m = bound - 1;
    if ((bound & m) == 0)
      return (int)(((int64_t)bound * (int64_t)r) >> 31);
    for (int32_t u = r; u - (r = u % bound) + m < 0; u = next(31))
      ;
    return (int)r;
  }
};

/** The list `new Random(0x4196A15C91A5E199L)` produces, restarted per
 *  constellation exactly as Cluster:228 does. */
inline std::vector<int> divisorSequence(int n) {
  JavaRand r(0x4196A15C91A5E199ULL);
  std::vector<int> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i)
    out.push_back(12 + r.nextInt(10));
  return out;
}

constexpr int kDivMin = 12, kDivCount = 10;   // 12 + nextInt(10) -> [12, 21]

// ---------------------------------------------------------------------------
// THE DATA. RegistryConstellations:296-368, verbatim, in registration order.

struct Con {
  const char *name;
  uint32_t color;
  int starCount;
  std::array<std::pair<int, int>, 9> stars;
  int linkCount;
  std::array<std::pair<int, int>, 10> links;   // 1-based, as addConnection reads
};

const std::array<Con, 4> kPage0 = {{
    {"DISCIDIA", 0xE01903, 8,
     {{{7, 2}, {3, 6}, {5, 12}, {20, 11}, {15, 17}, {26, 21}, {23, 27}, {15, 25}, {0, 0}}},
     7,
     {{{1, 2}, {2, 3}, {2, 4}, {4, 5}, {5, 7}, {6, 7}, {7, 8}, {0, 0}, {0, 0}, {0, 0}}}},
    {"ARMARA", 0xB7BBB8, 7,
     {{{8, 4}, {9, 15}, {11, 26}, {19, 25}, {23, 14}, {23, 4}, {15, 7}, {0, 0}, {0, 0}}},
     10,
     {{{1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 1}, {2, 5}, {2, 7}, {5, 7}}}},
    {"VICIO", 0x00BDAD, 7,
     {{{3, 8}, {13, 9}, {6, 23}, {14, 16}, {23, 24}, {22, 16}, {24, 4}, {0, 0}, {0, 0}}},
     6,
     {{{1, 2}, {2, 7}, {3, 4}, {4, 7}, {5, 6}, {6, 7}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}}},
    {"AEVITAS", 0x2EE400, 9,
     {{{15, 14}, {7, 12}, {3, 6}, {21, 8}, {25, 2}, {13, 21}, {9, 26}, {17, 28}, {27, 17}}},
     8,
     {{{1, 2}, {2, 3}, {1, 4}, {4, 5}, {1, 6}, {6, 7}, {6, 8}, {4, 9}, {0, 0}, {0, 0}}}},
}};

/** Cluster:288 — the hand-placed zig-zag. High, low, high, low; 80 px pitch,
 *  a 50/60 px swing. Authored, not computed; do not tidy it into a grid. */
const std::array<SkPoint, 4> kOffsets = {
    {{45, 55}, {125, 105}, {200, 45}, {280, 110}}};

/** The remaining twelve, for the tier rail. RegistryConstellations:370-553. */
struct Tier {
  const char *name;
  uint32_t color;
  const char *band;
};
const std::array<Tier, 12> kRest = {{
    {"EVORSIO", 0xA00100, "BRIGHT"},   {"LUCERNA", 0xFFE709, "DIM"},
    {"MINERALIS", 0xCB7D0A, "DIM"},    {"HOROLOGIUM", 0x7D16B4, "DIM"},
    {"OCTANS", 0x706EFF, "DIM"},       {"BOOTES", 0xD41CD6, "DIM"},
    {"FORNAX", 0xFF4E1B, "DIM"},       {"PELOTRIO", 0xEC006B, "DIM"},
    {"GELU", 0x758BA8, "FAINT"},       {"ULTERIA", 0x347463, "FAINT"},
    {"ALCARA", 0x802952, "FAINT"},     {"VORUX", 0xA8881E, "FAINT"},
}};

/** Star centre in the chart's LOCAL canvas space. Render:300 offsets the quad
 *  by -1 grid unit and spans 2 units, so the quad is centred on the grid point
 *  and the CENTRE is simply (x*u, y*u). */
inline SkPoint starAt(const Con &c, int index1) {
  const auto &s = c.stars[(size_t)(index1 - 1)];
  return {g(kUlen * (float)s.first), g(kUlen * (float)s.second)};
}

inline int degreeOf(const Con &c, int index1) {
  int d = 0;
  for (int i = 0; i < c.linkCount; ++i)
    if (c.links[(size_t)i].first == index1 || c.links[(size_t)i].second == index1)
      ++d;
  return d;
}

} // namespace at

// =============================================================================

struct AstralTome : sigil::compose::sketch::Sketch {
  // TEN Outputs for 93 twinkling primitives — see the perf story.
  std::array<ch::Output<float>, at::kDivCount> bright;
  ch::Output<float> arrowScale{1.0f};

  std::vector<int> divisors;     // the seeded list, long enough for any chart
  sk_sp<SkTypeface> serif, mono;
  double clock = 0;

  // ------------------------------------------------------------------ type
  Element label(const char *s, float x, float y, float size, SkColor4f col,
                float track = 0.0f, bool useMono = false) const {
    weave::TextStyle st;
    st.shaping.typeface = useMono ? mono : serif;
    st.shaping.fontSize = size;
    st.shaping.letterSpacing = track;
    st.shaping.aliased = useMono;
    st.paint.foreground.setColor4f(col, nullptr);
    return box().at({x, y}).child(text(toU8(s), st));
  }
  Element label(const std::string &s, float x, float y, float size, SkColor4f col,
                float track = 0.0f, bool useMono = false) const {
    return label(s.c_str(), x, y, size, col, track, useMono);
  }
  /** The tome's own tooltip idiom: type that has to sit ON the star field gets
   *  a dark sill under it (GuiScreen.drawHoveringText fills a plate before it
   *  prints). Without one, an annotation lands on a link and is gone. */
  Element scrimLabel(const std::string &s, float x, float y, float size,
                     SkColor4f col, float track = 0.0f) const {
    return label(s, x, y, size, col, track, true)
        .padding(3.0f)
        .fill(Fill::color({0.02f, 0.02f, 0.035f, 0.74f}));
  }

  // ------------------------------------------------------------- the plate
  /** Layer 0: the tome's leather, bleeding off every edge. guijspacebook is
   *  57% transparent (the window onto the page) over a gilt-brown ground; the
   *  opaque part is #0A0800 .. #9B7A2D. Generated as a radial ground plus
   *  grain, baked to PIXELS — a full-canvas material is a shader, and a
   *  picture replays the draw call, not the result. */
  Element leather() const {
    Element e = box()
                    .inset(0)
                    .key("leather")
                    .cache(Cache::Texture)
                    .fill(Material::blend(
                        {{Material::radialUnit({0.5f, 0.5f}, 0.95f,
                                               {{0.0f, at::kLeatherWarm},
                                                {0.55f, at::kLeatherMid},
                                                {1.0f, at::kLeatherDark}}),
                          SkBlendMode::kSrcOver},
                         {patterns::grain(2.6f, 4, 21), SkBlendMode::kOverlay}}));
    return e;
  }

  /** Layer 1: drawCstBackground (Cluster:113). texBlack over GUI 15,10 ..
   *  405,260 at full white, then guiresbgcst over the same rect at
   *  (0.8, 0.8, 1.0, 0.7) with UV cropped to 0.1..0.9. The measured product
   *  is a #0B080B ground with white points reaching #8F8FB3 over ~0.95% of
   *  the area — reconstructed as a nebula ramp plus four ScatterBrush passes
   *  on lissajous routes, seeded so the field is identical every frame. */
  Element pagePlate() const {
    const float x = at::gx(15), y = at::gy(10);
    const float w = at::g(at::kGuiW - 30), h = at::g(at::kGuiH - 20);
    Element p = box()
                    .rect(SkRect::MakeXYWH(x, y, w, h))
                    .key("page")
                    .cache(Cache::Texture)
                    .fill(Material::blend(
                        {{Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
                         {Material::radialUnit({0.42f, 0.38f}, 0.85f,
                                               {{0.0f, at::mul(at::kNebula, 2.2f)},
                                                {0.5f, at::kNebula},
                                                {1.0f, {0, 0, 0, 1}}}),
                          SkBlendMode::kPlus},
                         {Material::radialUnit({0.78f, 0.74f}, 0.55f,
                                               {{0.0f, at::mul(at::kNebula, 1.6f)},
                                                {1.0f, {0, 0, 0, 0}}}),
                          SkBlendMode::kPlus}}));
    // The field. Six scatter runs on lissajous routes with a wide normal
    // jitter — a brush, seeded, not a table of hand-placed dots. The measured
    // budget is the constraint: ~0.95% of the plate's pixels exceed L = 60, so
    // the runs are deliberately sparse and mostly sub-pixel-faint, with one
    // bright pass carrying the visible points.
    struct Run { float a, b, delta; float spacing; float mag; float alpha; };
    static constexpr Run kRuns[6] = {
        {3, 4, 40, 78, 0.9f, 0.30f},  {5, 6, 90, 96, 0.6f, 0.20f},
        {7, 5, 20, 112, 0.5f, 0.14f}, {2, 9, 65, 134, 1.5f, 0.55f},
        {9, 8, 15, 88, 0.7f, 0.24f},  {4, 11, 75, 168, 2.1f, 0.75f}};
    for (int i = 0; i < 6; ++i) {
      const Run &r = kRuns[i];
      Element art = box()
                        .width(1.5f * r.mag)
                        .height(1.5f * r.mag)
                        .outline(shapes::star(4, 0.26f, 0.14f))
                        .fill(Fill::color(at::mul(at::kFieldStar, 1.0f, r.alpha)));
      p.child(box()
                  .inset(-60)
                  .key(std::string("field") + std::to_string(i))
                  .outline(shapes::lissajous(r.a, r.b, r.delta, 1600))
                  .stroke(brushes::ScatterBrush{
                      .art = std::move(art),
                      .spacing = r.spacing,
                      .seed = 8000u + (uint32_t)i * 37u,
                      .jitterAlong = r.spacing * 0.49f,
                      .jitterNormal = 300.0f,
                      .jitterScale = 0.85f,
                      .alignToPath = false,
                      .reach = 330.0f}));
    }
    return p;
  }

  // -------------------------------------------------------------- the links
  /** One connection pass, as the mod draws it: a band of half-width
   *  `linebreadth` about the segment, at the constellation's colour, at the
   *  pass's own brightness. Rendered as a Ribbon body (tapered to 40% at each
   *  end — the departure) under a five-rail engraved rule whose three inner
   *  alphas are integrated off connectionperks.png. */
  Element linkPass(const at::Con &c, int li, int pass, int key) const {
    const SkPoint a = at::starAt(c, c.links[(size_t)li].first);
    const SkPoint b = at::starAt(c, c.links[(size_t)li].second);
    const SkColor4f col = at::hex(c.color);
    const float half = at::g(at::kLineBreadth);      // 6 canvas px
    const float band = half * 2.0f;                  // 12 canvas px

    // The three measured bands of connectionperks.png, solved back through
    // alpha-over so the composite matches the sprite's own profile. The
    // sprite's v-profile is 0.097 over the full band, 0.309 over the middle
    // 25/64, 0.580 over the middle 9/64 — those three numbers, verbatim.
    lines::Rails rails;
    rails.rails = {
        {.offset = 0,
         .width = band * (25.0f / 64.0f),
         .fill = Fill::color(at::mul(col, 1.0f, 0.309f))},
        {.offset = 0,
         .width = band * (9.0f / 64.0f),
         .fill = Fill::color(at::mul(col, 1.45f, 0.580f))},
        // the two dotted flanks — the departure, and the per-rail phase test:
        // same width, same fill, same dash, HALF A PERIOD apart, so the two
        // rows of dots interleave down the link the way a plate's register
        // marks do. Nothing before Rails could spell this.
        // NOTE the 2.2 px "on" interval. The first cut asked for round DOTS
        // with .dash = {0.01f, 11.0f} and nothing appeared on the flanks.
        //
        // CORRECTED 2026-07-22, AND THE OLD REASON WAS NEVER TRUE. This
        // comment used to say the OFFSET was the casualty: that a 0.01-long
        // contour has no usable tangent, so offsetAlong displaced the dashes
        // by nothing and both flanks drew inside the band. Measured at these
        // exact numbers — offset +-11.4, width 1.4, centreline y = 100 — the
        // 0.01 dash lands on y = 88 and y = 111. That IS the full offset, to
        // the pixel. The shipped 2.2 dash lands on the SAME TWO ROWS, and on a
        // curve the 0.01 dash puts ink at 68.6 and 91.4 about an apex of 80,
        // which is again +-11.4. offsetAlong (Lines.h:148) samples getPosTan at
        // two distances and displaces both along the normal; a 0.01 dash is a
        // real segment with a well-defined tangent and nothing in that loop
        // degenerates.
        //
        // What changed with the longer dash was INK, not position. A 0.01
        // contour under a 1.4 px round cap is a disc whose peak coverage never
        // reaches 1 — measured peak 191 against 2.2's 255, and half the lit
        // pixels — laid over a band already carrying two brighter rails. It
        // was FAINT, not displaced. Widening the SAME 0.01 dash to w = 4.0
        // reaches 255 on those same rows, which isolates the cause.
        //
        // So the remedy is more ink: a wider rail, or an "on" interval long
        // enough to build full coverage. 2.2 px is the latter. This matters
        // beyond one sketch, because lines::dottedCore ships
        // .dash = {0.01f, dotGap} as its DEFAULT (Lines.h:1048) — the old
        // comment was indicting a stock helper for a defect it does not have.
        {.offset = half * 1.9f,
         .width = 1.4f,
         .fill = Fill::color(at::mul(col, 1.35f, 0.52f)),
         .dash = {2.2f, 9.4f},
         .cap = SkPaint::kRound_Cap},
        {.offset = -half * 1.9f,
         .width = 1.4f,
         .fill = Fill::color(at::mul(col, 1.35f, 0.52f)),
         .dash = {2.2f, 9.4f},
         .dashPhase = 5.8f,
         .cap = SkPaint::kRound_Cap},
    };

    // The band's own soft shoulder, tapered to 40% at both ends so the link
    // reads as drawn FROM star TO star. Two Ribbons: a wide bloom and the
    // sprite's own 12-px body.
    brushes::Ribbon bloom;
    bloom.fill = Fill::color(at::mul(col, 1.0f, 0.055f));
    bloom.widthStart = bloom.widthEnd = band * 2.1f;
    bloom.step = 6.0f;
    bloom.widthMax = band * 2.1f;
    bloom.widthFn = [band](const PathSample &s) {
      const float k = std::sin(3.14159265f * std::clamp(s.fraction, 0.0f, 1.0f));
      return band * 2.1f * (0.40f + 0.60f * std::sqrt(k));
    };

    brushes::Ribbon body;
    body.fill = Fill::color(at::mul(col, 1.0f, 0.135f));
    body.widthStart = body.widthEnd = band;
    body.step = 4.0f;
    body.widthMax = band;
    body.widthFn = [band](const PathSample &s) {
      const float k = std::sin(3.14159265f * std::clamp(s.fraction, 0.0f, 1.0f));
      return band * (0.40f + 0.60f * std::sqrt(k));
    };

    const SkRect box2 = SkRect::MakeLTRB(std::min(a.fX, b.fX), std::min(a.fY, b.fY),
                                         std::max(a.fX, b.fX), std::max(a.fY, b.fY));
    const SkPoint p0{a.fX - box2.left(), a.fY - box2.top()};
    const SkPoint p1{b.fX - box2.left(), b.fY - box2.top()};
    return box()
        .rect(SkRect::MakeXYWH(box2.left(), box2.top(), std::max(box2.width(), 1.0f), std::max(box2.height(), 1.0f)))
        .key(std::string("lk") + std::to_string(key) + "_" + std::to_string(pass))
        .outline([p0, p1](SkSize) {
          SkPathBuilder p;
          p.moveTo(p0);
          p.lineTo(p1);
          return p.detach();
        })
        .stroke(std::move(bloom))
        .stroke(std::move(body))
        .stroke(std::move(rails));
  }

  // -------------------------------------------------------------- the stars
  /** star1.png is a four-pointed star with concave arms reaching the full
   *  32 px of its cell: shapes::star(4, 0.24, 0.16). The quad the mod draws is
   *  2*ulength square; the radius here is modulated by LINK DEGREE, which is
   *  the one piece of magnitude information the graph actually carries. */
  Element starEl(const at::Con &c, int si, int key) const {
    const SkPoint p = at::starAt(c, si);
    const SkColor4f col = at::hex(c.color);
    const int deg = at::degreeOf(c, si);
    const float base = at::g(at::kUlen * 2.0f);          // 18.39 canvas px
    const float r = base * (0.74f + 0.15f * (float)std::min(deg, 4));
    const float side = r * 3.4f;

    // A TIGHT box. The first cut of this hung every star off a full-canvas
    // group and let a shared .opacity() carry the twinkle; a bound opacity is
    // a saveLayer over the node's box, so twenty of those was twenty
    // 1200x750 layers a frame and 18.4 ms of an 16.6 ms budget. The layer has
    // to be the size of the thing that twinkles. See the perf story.
    Element grp = box()
                      .width(side)
                      .height(side)
                      .centerAt(p)
                      .key(std::string("st") + std::to_string(key));
    // the halo — a gradient, which lowers to a SIMD blitter, not a blur
    grp.child(box()
                  .inset(0)
                  .fill(Material::glowUnit({0.5f, 0.5f}, 0.5f,
                                           {{0.0f, at::mul(col, 1.0f, 0.42f)},
                                            {0.34f, at::mul(col, 1.0f, 0.13f)},
                                            {1.0f, at::mul(col, 1.0f, 0.0f)}})));
    // the glyph
    grp.child(box()
                  .rect(SkRect::MakeXYWH((side - r) * 0.5f, (side - r) * 0.5f, r, r))
                  .outline(shapes::star(4, 0.24f, 0.16f))
                  .fill(Fill::color(at::mul(col, 1.35f, 0.92f))));
    // the white-hot core. The one kPlus on this canvas, declared as a
    // departure on the plate: the source is GL_SRC_ALPHA/ONE_MINUS_SRC_ALPHA
    // throughout (Blending.java:23).
    const float cr = r * 0.34f;
    grp.child(box()
                  .rect(SkRect::MakeXYWH((side - cr) * 0.5f, (side - cr) * 0.5f, cr, cr))
                  .blend(SkBlendMode::kPlus)
                  .outline(shapes::circle())
                  .fill(Fill::color({0.92f, 0.94f, 1.0f, 0.85f})));
    return grp;
  }

  // -------------------------------------------------------------- cell plate
  /** The 80 x 110 HIT box (Cluster:219) as a chart plate: brackets at the four
   *  corners and a gapped rule on the TOP EDGE ONLY. The mod draws no cell
   *  border, so this is the study's apparatus — and it is drawn on the hit box
   *  precisely so the 95-wide chart can be seen overflowing it. */
  Element cellPlate(int i, bool key) const {
    const SkPoint o = at::kOffsets[(size_t)i];
    Element e = box()
                    .rect(SkRect::MakeXYWH(at::gx(o.fX), at::gy(o.fY), at::g(at::kCellW), at::g(at::kCellH)))
                    .key(std::string("cell") + std::to_string(i))
                    .outline(shapes::chamfered(at::g(4.0f), shapes::Corner::All))
                    .foreground(decorations::brackets(
                        1.5f, Fill::color(at::mul(at::kGilt, 1.0f, 0.62f)),
                        at::g(14.0f)))
                    .foreground(shapes::onEdges(
                        shapes::Edge::Top,
                        decorations::gappedRule(
                            1.0f, Fill::color(at::mul(at::kGilt, 1.0f, 0.30f)),
                            at::g(16.0f))));
    if (key) {
      // The declination ladder: 31 divisions of the cell, every 5th long —
      // the grid the star coordinates actually live on, drawn once.
      const float pitch = at::g(at::kCellH) / (float)at::kGrid;
      Element tick = box().width(1.0f).height(at::g(2.6f)).fill(
          Fill::color(at::mul(at::kGilt, 1.0f, 0.55f)));
      Element longTick = box().width(1.2f).height(at::g(6.0f)).fill(
          Fill::color(at::mul(at::kGilt, 1.0f, 0.85f)));
      e.foreground(shapes::onEdges(
          shapes::Edge::Left,
          Decoration(brushes::ScatterBrush{.art = std::move(tick),
                                           .spacing = pitch,
                                           .alignToPath = true,
                                           .reach = at::g(4.0f)})));
      e.foreground(shapes::onEdges(
          shapes::Edge::Left,
          Decoration(brushes::ScatterBrush{.art = std::move(longTick),
                                           .spacing = pitch * 5.0f,
                                           .alignToPath = true,
                                           .reach = at::g(8.0f)})));
    }
    return e;
  }

  // ---------------------------------------------------------------- arrows
  /** guijarrow is a 45 x 26 sheet read as a 2x2 UV grid, so one arrow is a
   *  22.5 x 13 cell drawn into a 30 x 15 rect. Olive, #7D6C00 over #574E25.
   *  Idle scale sin(t/4)/32 + 1 (Cluster:163); hover snaps to a flat 1.1. */
  Element arrow(float guiX, float guiY, bool flip, bool hovered,
                const char *k) const {
    Element e = box()
                    .rect(SkRect::MakeXYWH(at::gx(guiX), at::gy(guiY), at::g(30), at::g(15)))
                    .key(k)
                    .transformOrigin(0.5f, 0.5f)
                    .outline(shapes::arrow(0.34f, 0.42f))
                    .rotate(flip ? 180.0f : 0.0f)
                    .fill(Material::linearUnit({0, 0}, {0, 1},
                                               {{0.0f, at::kOlive},
                                                {1.0f, at::kOliveDim}}))
                    .foreground(decorations::border(
                        1.2f, Fill::color(at::mul(at::kGilt, 1.0f, 0.7f))));
    if (hovered)
      e.scale(1.1f);
    else
      e.scale(bind(&arrowScale));
    return e;
  }

  // ------------------------------------------------------------ bookmarks
  /** GuiScreenJournal:113 — the rail hangs at guiLeft + guiWidth - 17.25 and
   *  runs DOWN THE RIGHT EDGE at an 18 px pitch, 67 x 15 each, five px WIDER
   *  when not selected. Almost all of it is off the page: the tab sticks out
   *  past the tome's own right edge, which is why it bleeds off this canvas.
   *  Labels from ClientProxy:196-205. */
  Element bookmarkRail() const {
    static constexpr const char *kNames[4] = {"RESEARCH", "CONSTELLATIONS",
                                              "PERKS", "KNOWLEDGE"};
    Element rail = box().inset(0).key("bm").zIndex(12);
    for (int i = 0; i < 4; ++i) {
      const bool sel = i == 1;              // bookmarkIndex 20 = Constellations
      const float w = 67.0f + (sel ? 0.0f : 5.0f);
      const float y = 20.0f + 18.0f * (float)i;
      rail.child(box()
                     .rect(SkRect::MakeXYWH(at::gx(at::kGuiW - 17.25f), at::gy(y), at::g(w), at::g(15)))
                     .key(std::string("bmk") + std::to_string(i))
                     .outline(shapes::notched(at::g(9.0f), at::g(4.0f),
                                              shapes::Corner::TopRight |
                                                  shapes::Corner::BottomRight))
                     .fill(Material::linearUnit(
                         {0, 0}, {1, 0},
                         {{0.0f, sel ? at::kLeatherWarm : at::kLeatherMid},
                          {0.6f, at::mul(at::kLeatherMid, 0.8f)},
                          {1.0f, at::kLeatherDark}}))
                     .foreground(decorations::border(
                         1.2f,
                         Fill::color(at::mul(at::kGilt, 1.25f, sel ? 1.0f : 0.6f)),
                         1.0f)));
      (void)kNames;   // the label rides 15 GUI px into a tab that starts
                      // 2.75 px from the tome's right edge: at 3x it is
                      // entirely off-canvas, so the tab bleeds and the name
                      // does not print as a clipped fragment.
    }
    return rail;
  }

  // --------------------------------------------------------- the apparatus
  /** THE KEY CELL. Chart 0 gets the 31 x 31 lattice its star coordinates
   *  actually live on, drawn once, on the 95 x 95 RENDER box — which is how
   *  you see it stand 15 px proud of the 80-wide cell plate under it. */
  Element keyLattice(int ci) const {
    const SkPoint o = at::kOffsets[(size_t)ci];
    const float side = at::g(at::kRenderBox);
    Element e = box()
                    .rect(SkRect::MakeXYWH(at::gx(o.fX), at::gy(o.fY), side, side))
                    .key("lattice")
                    .foreground(lines::crosshatch(
                        Fill::color(at::mul(at::kGilt, 1.0f, 0.11f)),
                        at::g(at::kUlen), 0.5f, 0.0f))
                    .foreground(decorations::border(
                        0.9f, Fill::color(at::mul(at::kGilt, 1.0f, 0.34f))));
    return e;
  }

  /** Every star on the key chart carries its own grid coordinate, small, the
   *  way a plate labels the positions it was set from. */
  Element keyCoords(int ci) const {
    const at::Con &c = at::kPage0[(size_t)ci];
    const SkPoint o = at::kOffsets[(size_t)ci];
    Element g2 = box().inset(0).key("coords").zIndex(7);
    for (int si = 1; si <= c.starCount; ++si) {
      const SkPoint p = at::starAt(c, si);
      const auto &s = c.stars[(size_t)(si - 1)];
      const std::string t = std::to_string(s.first) + "," + std::to_string(s.second);
      // SCRIMMED, like every other annotation that has to sit on the field.
      // These went out plain and the plate's own rule caught up with them: a
      // coordinate is pinned +9,-15 from its star, which is exactly where an
      // edge leaving up-and-right goes, so DISCIDIA's "3,6" and "23,27" each
      // had a 12 px link through their x-height. scrimLabel() is this file's
      // answer to that and was already carrying "pivot 40,40" and
      // "centre 47.5"; the coordinates just never got it.
      g2.child(scrimLabel(t, at::gx(o.fX) + p.fX + 9.0f,
                          at::gy(o.fY) + p.fY - 16.0f, 10.0f,
                          at::mul(at::hex(c.color), 1.25f, 0.86f), 0.6f));
    }
    return g2;
  }

  /** THE MAGNITUDE KEY, as a specimen ROW rather than a column — four glyphs
   *  on one rule in the pocket between armara, vicio and aevitas. The mod
   *  draws every star at one size; this plate does not, and the variable is
   *  LINK DEGREE off the connection list, so each specimen names the star it
   *  was measured from. */
  Element magnitudeKey(float x, float y) const {
    static constexpr const char *kWho[4] = {"discidia sl1", "vicio sl4",
                                            "aevitas sl1", "armara sl2"};
    Element k = box().inset(0).key("magkey").zIndex(14);
    k.child(label("GLYPH RADIUS \xC3\x97 LINK DEGREE", x, y - 22.0f, 10.5f,
                  at::mul(at::kGilt, 1.0f, 0.68f), 2.0f, true));
    k.child(box()
                .rect(SkRect::MakeXYWH(x, y - 6.0f, 216.0f, 2.0f))
                .key("magrule")
                .outline([](SkSize s2) {
                  SkPathBuilder p;
                  p.moveTo(0, 1);
                  p.lineTo(s2.width(), 1);
                  return p.detach();
                })
                .stroke(lines::Rails{
                    .rails = {{.offset = 0,
                               .width = 1.0f,
                               .fill = Fill::color(at::mul(at::kGilt, 1.0f, 0.5f))},
                              {.offset = 4.0f,
                               .width = 0.7f,
                               .fill = Fill::color(at::mul(at::kGilt, 1.0f, 0.24f)),
                               .dash = {2.0f, 5.0f}}}}));
    const float base = at::g(at::kUlen * 2.0f);
    for (int d = 1; d <= 4; ++d) {
      const float r = base * (0.74f + 0.15f * (float)d);
      const float cx = x + 22.0f + (float)(d - 1) * 58.0f;
      k.child(box()
                  .width(r)
                  .height(r)
                  .centerAt({cx, y + 26.0f})
                  .key(std::string("mk") + std::to_string(d))
                  .outline(shapes::star(4, 0.24f, 0.16f))
                  .fill(Fill::color(at::mul(at::kInk, 1.0f, 0.88f))));
      k.child(label(std::to_string(d), cx - 3.0f, y + 46.0f, 11.0f,
                    at::mul(at::kGilt, 1.05f, 0.78f), 0.6f, true));
    }
    k.child(label(kWho[0], x, y + 64.0f, 8.5f,
                  at::mul(at::kGilt, 0.9f, 0.45f), 0.3f, true));
    k.child(label(kWho[3], x + 122.0f, y + 64.0f, 8.5f,
                  at::mul(at::kGilt, 0.9f, 0.45f), 0.3f, true));
    return k;
  }

  /** THE LIVE DIVISOR STRIP. Ten bars, ten Outputs — the whole twinkle of this
   *  page, as a 250 px rule under the plate head rather than a panel in a
   *  corner. Each bar is bound to the SAME Output that drives every star and
   *  every link filed under that divisor, so the strip is a direct read on the
   *  structure, not a legend for it. */
  Element divisorStrip(float x, float y) {
    Element k = box().inset(0).key("divstrip").zIndex(14);
    k.child(label("conCFlicker \xC2\xB7 seed 0x4196A15C91A5E199 \xC2\xB7 d = 12 "
                  "\xE2\x80\xA6 21 \xC2\xB7 3.77 \xE2\x80\x93 6.60 s",
                  x, y + 24.0f, 10.0f, at::mul(at::kGilt, 0.95f, 0.52f), 0.8f,
                  true));
    for (int i = 0; i < at::kDivCount; ++i) {
      const float bx = x + (float)i * 25.0f;
      k.child(box()
                  .rect(SkRect::MakeXYWH(bx, y, 17.0f, 18.0f))
                  .key(std::string("fb") + std::to_string(i))
                  .transformOrigin(0.5f, 1.0f)
                  .scaleY(bind(&bright[(size_t)i])
                              .from(0.3875f, 0.7375f)
                              .scale(0.78f)
                              .offset(0.22f))
                  .fill(Fill::color(at::mul(at::kInk, 1.0f, 0.72f))));
      k.child(box()
                  .rect(SkRect::MakeXYWH(bx, y + 19.0f, 17.0f, 1.0f))
                  .key(std::string("fu") + std::to_string(i))
                  .fill(Fill::color(at::mul(at::kGilt, 1.0f, 0.45f))));
    }
    return k;
  }

  /** A LEADER CALLOUT on one star of each chart: the drafting leader, a dot on
   *  the star, a bent rule out to a caption that names the star's grid
   *  coordinate, its link degree, and the divisor the seeded sequence handed
   *  it. Marginalia standing next to what it describes — four of them,
   *  radiating into four different pockets of the page. */
  Element callout(int ci, int si, SkPoint to, bool flip) const {
    const at::Con &c = at::kPage0[(size_t)ci];
    const SkPoint o = at::kOffsets[(size_t)ci];
    const SkPoint p{at::gx(o.fX) + at::starAt(c, si).fX,
                    at::gy(o.fY) + at::starAt(c, si).fY};
    const SkColor4f col = at::hex(c.color);
    const auto &s = c.stars[(size_t)(si - 1)];
    const int deg = at::degreeOf(c, si);
    const int d = divisors[(size_t)(2 * c.linkCount + si - 1)];

    const SkRect bb = SkRect::MakeLTRB(std::min(p.fX, to.fX) - 2,
                                       std::min(p.fY, to.fY) - 2,
                                       std::max(p.fX, to.fX) + 2,
                                       std::max(p.fY, to.fY) + 2);
    const SkPoint a0{p.fX - bb.left(), p.fY - bb.top()};
    const SkPoint a1{to.fX - bb.left(), to.fY - bb.top()};
    Element g2 = box().inset(0).key(std::string("co") + std::to_string(ci)).zIndex(13);
    g2.child(box()
                 .rect(SkRect::MakeXYWH(bb.left(), bb.top(), bb.width(), bb.height()))
                 .key(std::string("col") + std::to_string(ci))
                 .outline([a0, a1](SkSize) {
                   // an elbow: out along the bearing, then flat into the
                   // caption — the leader a draughtsman draws, not a chord
                   SkPathBuilder pb;
                   pb.moveTo(a0);
                   pb.lineTo(a1.fX + (a0.fX < a1.fX ? -26.0f : 26.0f), a1.fY);
                   pb.lineTo(a1);
                   return pb.detach();
                 })
                 .stroke(lines::Line{.width = 0.9f,
                                     .fill = Fill::color(at::mul(col, 1.2f, 0.55f)),
                                     .startCap = lines::Cap::Dot,
                                     .capSize = 6.0f}));
    char buf[96];
    std::snprintf(buf, sizeof buf, "sl%d (%d,%d) \xC2\xB7 deg %d \xC2\xB7 d=%d \xC2\xB7 %.2f s",
                  si, s.first, s.second, deg, d, 6.2831853f * (float)d / 20.0f);
    g2.child(scrimLabel(buf, flip ? to.fX - 182.0f : to.fX + 8.0f, to.fY - 13.0f,
                        10.5f, at::mul(col, 1.3f, 0.95f), 0.5f));
    return g2;
  }

  /** THE SPRITE PROFILE. connectionperks.png is 64 x 64, white, and its ALPHA
   *  is a vertical ramp with a narrow bright core — which is a three-rail rule
   *  in the artefact, not a departure from it. The 64 measured samples are
   *  plotted here with the three integration bands ruled across them and the
   *  three solved rail alphas printed, so the numbers in linkPass() have their
   *  derivation on the same page. */
  Element spriteProfile(float x, float y) const {
    static constexpr int kProf[64] = {
        1,   1,   2,   4,   5,   6,   9,   11,  14,  17,  21,  25,  29,
        34,  40,  45,  52,  57,  63,  69,  76,  82,  89,  95,  101, 107,
        113, 119, 134, 172, 205, 233, 248, 224, 194, 159, 124, 116, 109,
        104, 98,  91,  84,  78,  72,  65,  59,  52,  48,  41,  36,  31,
        26,  22,  19,  14,  12,  9,   7,   5,   4,   2,   2,   1};
    const float w = 176.0f, h = 74.0f;
    const SkColor4f ink = at::mul(at::kGilt, 1.15f, 0.9f);
    const SkColor4f dim = at::mul(at::kGilt, 1.0f, 0.30f);
    Element m = box().inset(0).key("sprofile").zIndex(14);
    m.child(label("connectionperks.png \xC2\xB7 alpha by v", x, y - 20.0f, 10.0f,
                  at::mul(at::kGilt, 1.0f, 0.66f), 1.4f, true));
    m.child(box()
                .rect(SkRect::MakeXYWH(x, y, w, h))
                .key("sprof")
                .foreground(decorations::brackets(1.0f, Fill::color(dim), 12.0f))
                .background(at::prog([=](SkCanvas &c, const PaintContext &) {
                  SkPaint band;
                  band.setAntiAlias(false);
                  // the three integration bands, as the ink they became
                  const int lo[3] = {0, 20, 28}, hi[3] = {63, 43, 36};
                  const float al[3] = {0.097f, 0.309f, 0.580f};
                  for (int b = 0; b < 3; ++b) {
                    band.setColor4f({ink.fR, ink.fG, ink.fB, 0.09f + 0.07f * (float)b},
                                    nullptr);
                    c.drawRect(SkRect::MakeLTRB(w * (float)lo[b] / 63.0f, 0,
                                                w * (float)hi[b] / 63.0f, h),
                               band);
                    (void)al[b];
                  }
                  SkPathBuilder pb;
                  for (int i = 0; i < 64; ++i) {
                    const float px = w * (float)i / 63.0f;
                    const float py = h - h * (float)kProf[i] / 255.0f;
                    if (i == 0)
                      pb.moveTo(px, py);
                    else
                      pb.lineTo(px, py);
                  }
                  SkPaint line;
                  line.setAntiAlias(true);
                  line.setStyle(SkPaint::kStroke_Style);
                  line.setStrokeWidth(1.3f);
                  line.setColor4f(ink, nullptr);
                  c.drawPath(pb.detach(), line);
                })));
    m.child(label("0.097 \xC2\xB7 0.309 \xC2\xB7 0.580  \xE2\x86\x92  three rails",
                  x, y + h + 5.0f, 9.0f, at::mul(at::kGilt, 1.0f, 0.55f), 0.4f,
                  true));
    return m;
  }

  /** THE OFFSET MAP, at 1/5.45 scale: the four cells on their authored
   *  zig-zag, numbered, with the offsets printed. A key to the composition,
   *  the way a plate carries a key to its own frame — and the fastest way to
   *  see that (45,55) (125,105) (200,45) (280,110) is high-low-high-low at an
   *  80 px pitch and NOT a grid. */
  Element offsetKey(float x, float y) const {
    const float k = 0.55f;   // canvas px per GUI px, ~1/5.45 of the plate
    Element m = box().inset(0).key("offkey").zIndex(14);
    m.child(label("offsetMap \xC2\xB7 4 PER PAGE", x, y - 22.0f,
                  10.5f, at::mul(at::kGilt, 1.0f, 0.68f), 1.6f, true));
    m.child(box()
                .rect(SkRect::MakeXYWH(x, y, at::kGuiW * k, at::kGuiH * k))
                .key("offframe")
                .outline(shapes::chamfered(7.0f, shapes::Corner::All))
                .foreground(decorations::weightedCorners(
                    0.7f, 2.0f, Fill::color(at::mul(at::kGilt, 1.0f, 0.42f)),
                    16.0f)));
    for (int i = 0; i < 4; ++i) {
      const SkPoint o = at::kOffsets[(size_t)i];
      // the 80 x 110 hit cell
      m.child(box()
                  .rect(SkRect::MakeXYWH(x + o.fX * k, y + o.fY * k, at::kCellW * k, at::kCellH * k))
                  .key(std::string("ok") + std::to_string(i))
                  .foreground(decorations::brackets(
                      0.9f, Fill::color(at::hex(at::kPage0[(size_t)i].color, 0.8f)),
                      9.0f)));
      // the 95 x 95 render box standing proud of it
      m.child(box()
                  .rect(SkRect::MakeXYWH(x + o.fX * k, y + o.fY * k, at::kRenderBox * k, at::kRenderBox * k))
                  .key(std::string("or") + std::to_string(i))
                  .foreground(lines::Line{
                      .width = 0.7f,
                      .fill = Fill::color(at::hex(at::kPage0[(size_t)i].color, 0.32f)),
                      .dashIntervals = {2.0f, 4.0f}}));
      m.child(label(std::to_string(i), x + o.fX * k + 3.0f, y + o.fY * k + 2.0f,
                    9.5f, at::hex(at::kPage0[(size_t)i].color, 0.95f), 0.4f,
                    true));
    }
    m.child(label("0 45,55    1 125,105", x, y + at::kGuiH * k + 5.0f, 8.5f,
                  at::mul(at::kGilt, 0.95f, 0.58f), 0.4f, true));
    m.child(label("2 200,45   3 280,110", x, y + at::kGuiH * k + 16.0f, 8.5f,
                  at::mul(at::kGilt, 0.95f, 0.58f), 0.4f, true));
    m.child(label("hit cell 80\xC3\x97" "110 \xC2\xB7 render 95\xC3\x97" "95",
                  x, y + at::kGuiH * k + 27.0f, 8.5f,
                  at::mul(at::kGilt, 0.9f, 0.5f), 0.4f, true));
    return m;
  }

  /** THE PIVOT MARK. Cluster:222 scales the hovered chart about
   *  (offsetX + width/2, offsetY + width/2) — width/2 on BOTH axes. Drawn:
   *  the ghost of the un-hovered 95 x 95 render box, the pivot cross, and the
   *  chart's true centre 7.5 GUI px down-right of it. */
  Element pivotMark(int ci) const {
    const SkPoint o = at::kOffsets[(size_t)ci];
    const float side = at::g(at::kRenderBox);
    const SkPoint pivot{at::gx(o.fX) + at::g(at::kCellW * 0.5f),
                        at::gy(o.fY) + at::g(at::kCellW * 0.5f)};
    const SkPoint centre{at::gx(o.fX) + side * 0.5f, at::gy(o.fY) + side * 0.5f};
    Element m = box().inset(0).key("pivot").zIndex(13);
    // the ghost: where the chart sits when NOT hovered
    m.child(box()
                .rect(SkRect::MakeXYWH(at::gx(o.fX), at::gy(o.fY), side, side))
                .key("ghost")
                .foreground(lines::Line{
                    .width = 1.1f,
                    .fill = Fill::color(at::mul(at::kInk, 1.0f, 0.40f)),
                    .dashIntervals = {5.0f, 7.0f}}));
    m.child(scrimLabel("95\xC3\x97" "95 UNHOVERED  \xC2\xB7  hover \xC3\x97" "1.1 "
                       "grows DOWN and RIGHT",
                       at::gx(o.fX) + 4.0f, at::gy(o.fY) - 18.0f, 9.5f,
                       at::mul(at::kInk, 1.0f, 0.72f), 0.9f));
    m.child(box()
                .width(26.0f)
                .height(26.0f)
                .centerAt(pivot)
                .key("pivotx")
                .outline(shapes::star(4, 0.06f, 0.0f))
                .fill(Fill::color(at::mul(at::kGilt, 1.4f, 0.95f))));
    m.child(box()
                .width(11.0f)
                .height(11.0f)
                .centerAt(centre)
                .key("truec")
                .outline(shapes::circle())
                .foreground(decorations::border(
                    1.0f, Fill::color(at::mul(at::kInk, 1.0f, 0.45f)))));
    m.child(scrimLabel("pivot 40,40", pivot.fX - 102.0f, pivot.fY - 26.0f, 10.0f,
                       at::mul(at::kGilt, 1.3f, 0.98f), 0.6f));
    m.child(scrimLabel("centre 47.5", centre.fX - 102.0f, centre.fY + 16.0f,
                       10.0f, at::mul(at::kInk, 1.0f, 0.72f), 0.6f));
    return m;
  }

  // ----------------------------------------------------------- marginalia
  Element marginalia() const {
    Element m = box().inset(0).key("margin").zIndex(14);

    // The plate frame: a double border whose outer rule THICKENS into the
    // corner and whose inner rule stops short of it, over a chamfered
    // silhouette. A frame is not a 1 px rounded rect.
    m.child(box()
                .rect(SkRect::MakeXYWH(at::gx(15) + 5, 5, at::g(at::kGuiW - 30) - 10, at::kCanvasH - 10))
                .key("plateframe")
                .outline(shapes::chamfered(26.0f, shapes::Corner::All))
                .style(decorations::doubleBorder(
                    decorations::weightedCorners(
                        1.0f, 3.2f, Fill::color(at::mul(at::kGilt, 1.0f, 0.55f)),
                        44.0f),
                    decorations::gappedRule(
                        0.8f, Fill::color(at::mul(at::kGilt, 1.0f, 0.22f)), 62.0f,
                        7.0f))));

    // Plate head — kept inside x < 560 so nothing in the top-right pocket can
    // collide with it.
    m.child(label("ASTRAL TOME \xC2\xB7 CONSTELLATIONS \xC2\xB7 PAGE 1 OF 4", 34.0f,
                  16.0f, 17.0f, at::mul(at::kGilt, 1.15f, 0.92f), 3.2f));
    m.child(label("GuiJournalConstellationCluster \xC2\xB7 AstralSorcery 1.12.2",
                  35.0f, 40.0f, 11.5f, at::mul(at::kGilt, 0.85f, 0.6f), 0.4f,
                  true));
    m.child(box()
                .rect(SkRect::MakeXYWH(34.0f, 56.0f, 500.0f, 3.0f))
                .key("headrule")
                .outline([](SkSize s2) {
                  SkPathBuilder p;
                  p.moveTo(0, 1.5f);
                  p.lineTo(s2.width(), 1.5f);
                  return p.detach();
                })
                .stroke(lines::heavyHairHeavy(
                    1.6f, 0.5f, Fill::color(at::mul(at::kGilt, 1.0f, 0.5f)),
                    3.0f)));

    // The evidence. Bottom-left pocket: x 34..545, clear of rectBack
    // (Cluster:198 -> canvas 561,660) and below armara's cell plate.
    const float ex = 34.0f, ey = 638.0f;
    static const char *kNotes[5] = {
        "renderConstellationIntoGUI(\xE2\x80\xA6, 95, 95, 2F)  \xE2\x80\x94  the "
        "render box is SQUARE. u = v = 3.0645",
        "width=80 height=110  \xE2\x80\x94  hit box, pivot, label. never the render",
        "translate(x + width/2, y + width/2)  \xE2\x80\x94  width/2 on BOTH axes",
        "Blending.DEFAULT = SRC_ALPHA / ONE_MINUS_SRC_ALPHA \xE2\x80\x94 alpha, not add.",
        "getConstellationScreen() \xE2\x80\x94 the tier split is commented out"};
    for (int i = 0; i < 5; ++i)
      m.child(label(kNotes[i], ex, ey + (float)i * 19.0f, 11.0f,
                    at::mul(at::kGilt, 0.95f, i >= 3 ? 0.82f : 0.6f), 0.2f, true));
    m.child(label("CORRECTIONS FROM THE SOURCE", ex, ey - 24.0f, 10.5f,
                  at::mul(at::kGilt, 1.1f, 0.72f), 2.2f, true));
    m.child(box()
                .rect(SkRect::MakeXYWH(ex, ey - 8.0f, 496.0f, 2.0f))
                .key("noterule")
                .outline([](SkSize s2) {
                  SkPathBuilder p;
                  p.moveTo(0, 1);
                  p.lineTo(s2.width(), 1);
                  return p.detach();
                })
                .stroke(lines::Rails{
                    .rails = {{.offset = 0,
                               .width = 1.0f,
                               .fill = Fill::color(at::mul(at::kGilt, 1.0f, 0.42f))},
                              {.offset = 3.5f,
                               .width = 0.6f,
                               .fill = Fill::color(at::mul(at::kGilt, 1.0f, 0.2f)),
                               .dash = {1.5f, 4.5f}}}}));

    // The twelve NOT on this page: a horizontal swatch strip in the
    // bottom-right pocket, two rows of six, each in its own colour.
    const float tx = 684.0f, ty = 642.0f;
    m.child(label("NOT ON THIS PAGE \xC2\xB7 PAGES 2\xE2\x80\x93" "4", tx, ty - 22.0f,
                  10.5f, at::mul(at::kGilt, 1.0f, 0.68f), 2.0f, true));
    m.child(box()
                .rect(SkRect::MakeXYWH(tx, ty - 8.0f, 486.0f, 1.0f))
                .key("tierrule")
                .fill(Fill::color(at::mul(at::kGilt, 1.0f, 0.36f))));
    for (int i = 0; i < 12; ++i) {
      const SkColor4f c = at::hex(at::kRest[(size_t)i].color);
      const float cx = tx + (float)(i % 6) * 81.0f;
      const float cy = ty + (float)(i / 6) * 46.0f;
      m.child(box()
                  .rect(SkRect::MakeXYWH(cx, cy + 4.0f, 26.0f, 3.0f))
                  .key(std::string("tier") + std::to_string(i))
                  .fill(Fill::color(at::mul(c, 1.0f, 0.92f))));
      m.child(label(at::kRest[(size_t)i].name, cx, cy + 10.0f, 9.5f,
                    at::mul(c, 1.0f, 0.8f), 0.5f, true));
      m.child(label(at::kRest[(size_t)i].band, cx, cy + 24.0f, 8.0f,
                    at::mul(at::kGilt, 0.9f, 0.42f), 0.6f, true));
    }

    // Registration marks on the page plate.
    m.child(box()
                .rect(SkRect::MakeXYWH(at::gx(15), 0, at::g(at::kGuiW - 30), at::kCanvasH))
                .key("reg")
                .foreground(decorations::brackets(
                    1.4f, Fill::color(at::mul(at::kGilt, 1.0f, 0.4f)), 30.0f,
                    4.0f)));
    return m;
  }

  // --------------------------------------------------------------- setup
  static sk_sp<SkTypeface> pick(std::initializer_list<const char *> names) {
    sk_sp<SkFontMgr> mgr = weave::ports::systemFontManager();
    if (!mgr)
      return nullptr;
    for (const char *n : names)
      if (sk_sp<SkTypeface> f = mgr->matchFamilyStyle(n, SkFontStyle::Normal()))
        return f;
    return mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(at::kCanvasW, at::kCanvasH);
    ctx.background({0, 0, 0, 1});

    serif = pick({"Baskerville", "Charter", "Palatino", "Times New Roman",
                  "Georgia", "Helvetica"});
    mono = pick({"Menlo", "SF Mono", "Monaco", "Courier New", "Helvetica"});
    divisors = at::divisorSequence(64);

    // ---- motion ---------------------------------------------------------
    ctx.ticker.add([this](double dt) {
      clock += dt;
      // Cluster:243 / Render:331. clientTick runs at 20 Hz; partialTicks is
      // the sub-tick fraction, so (tick + partial) is simply t*20.
      const double t = std::fmod(clock * 20.0, 12000.0);   // dayLength/2
      for (int i = 0; i < at::kDivCount; ++i)
        bright[(size_t)i] =
            (float)(0.3 + 0.7 * (std::sin(t / (double)(at::kDivMin + i)) / 4.0 +
                                 0.375));
      // Cluster:163 — sin(t/4)/32 + 1 on the client tick.
      arrowScale = (float)(std::sin(t / 4.0) / 32.0 + 1.0);
      return true;
    });

    // ---- the tree -------------------------------------------------------
    Element root = box().inset(0);
    root.child(leather().zIndex(0));
    root.child(pagePlate().zIndex(1));

    // The cell plates first — they are apparatus, under the charts. Chart 0
    // is the KEY cell and additionally carries the 31 x 31 lattice, so the
    // 95-vs-80 overhang is a thing you can see rather than a caption.
    for (int i = 0; i < 4; ++i)
      root.child(cellPlate(i, i == 0).zIndex(2));
    root.child(keyLattice(0).zIndex(2));

    // THE TWINKLE. Ten divisors is the whole of it (12 + rand.nextInt(10)),
    // so ten ch::Output<float> drive 31 stars and 62 connection passes — but
    // the BINDING goes on each primitive's own tight box, not on ten shared
    // full-canvas groups. A bound opacity is a saveLayer over the node's box;
    // grouping put twenty 1200x750 layers in every frame and cost 18.4 ms.
    // Same ten Outputs, same draw order, layers the size of a star.
    // The draw-on: each chart is its own container so staggerChildren can
    // cascade the four in offsetMap order — the zig-zag reveals as a zig-zag —
    // and the links inside a chart cascade again at 25 ms. Containers with no
    // opacity of their own allocate no layer, so this costs nothing at rest.
    Element links = box()
                        .inset(0)
                        .key("links")
                        .zIndex(3)
                        .staggerChildren(160ms);
    Element stars = box()
                        .inset(0)
                        .key("stars")
                        .zIndex(4)
                        .staggerChildren(160ms);

    int lkKey = 0, stKey = 0;
    for (int ci = 0; ci < 4; ++ci) {
      const at::Con &c = at::kPage0[(size_t)ci];
      const SkPoint o = at::kOffsets[(size_t)ci];
      const SkPoint org{at::gx(o.fX), at::gy(o.fY)};
      // Cluster:222 — the pivot is (width/2, width/2) = (40, 40) GUI, i.e.
      // 7.5 px up and left of the 95x95 chart's own centre. Chart 1 (armara,
      // the dense one) is the hovered chart, and it grows down-right off it.
      const bool hovered = ci == 1;
      const float pivot = at::g(at::kCellW * 0.5f);
      Element chartLinks = box()
                               .inset(0)
                               .key(std::string("cl") + std::to_string(ci))
                               .staggerChildren(25ms);
      Element chartStars = box()
                               .inset(0)
                               .key(std::string("cs") + std::to_string(ci))
                               .staggerChildren(25ms);
      auto place = [&](Element e) {
        e.translateX(org.fX).translateY(org.fY);
        if (hovered) {
          e.scale(1.1f);
          e.transformOriginPx({org.fX + pivot, org.fY + pivot});
        }
        return e;
      };

      // Connections. TWO sibling passes per link, at the two divisors the
      // seeded sequence hands them — Render:243's `for (j = 0; j < 2; j++)`
      // calls getBrightness() again on the second lap, so the two passes do
      // NOT share an alpha and the composite beats between two periods.
      for (int li = 0; li < c.linkCount; ++li)
        for (int pass = 0; pass < 2; ++pass) {
          const int d = divisors[(size_t)(pass * c.linkCount + li)] - at::kDivMin;
          chartLinks.child(place(linkPass(c, li, pass, lkKey++)
                                     .trim(0.0f, withFrom(0.0f, 1.0f, {520ms}))
                                     .opacity(bind(&bright[(size_t)d]))));
        }

      // Stars. The star loop runs after both connection laps, so it picks up
      // at index 2*C in the same sequence.
      for (int si = 1; si <= c.starCount; ++si) {
        const int d =
            divisors[(size_t)(2 * c.linkCount + si - 1)] - at::kDivMin;
        chartStars.child(place(starEl(c, si, stKey++)
                                   .scale(withFrom(0.0f, 1.0f, {380ms}))
                                   .opacity(bind(&bright[(size_t)d]))));
      }
      links.child(std::move(chartLinks));
      stars.child(std::move(chartStars));
    }
    root.child(std::move(links));
    root.child(std::move(stars));

    // The names. Cluster:252-253 centres on x = 40 of an 80-wide cell — 7.5
    // GUI px left of the 95-wide chart — and drops the baseline at y = 90,
    // inside the chart's own star field.
    for (int ci = 0; ci < 4; ++ci) {
      const at::Con &c = at::kPage0[(size_t)ci];
      const SkPoint o = at::kOffsets[(size_t)ci];
      const float w = (float)std::char_traits<char>::length(c.name) * 8.6f;
      root.child(label(c.name, at::gx(o.fX + at::kCellW * 0.5f) - w * 0.5f,
                       at::gy(o.fY + 90.0f), 19.0f,
                       at::mul(at::kInk, 1.0f, kInkAlphaOf()), 2.4f)
                     .key(std::string("nm") + std::to_string(ci))
                     .zIndex(6));
    }

    root.child(keyCoords(0));
    root.child(pivotMark(1));

    // The apparatus, distributed into the page's own pockets rather than
    // stacked into a column: the specimen row between three charts, the
    // offsetMap key in the top-right, the live divisor strip under the head,
    // and four leader callouts standing next to the stars they describe.
    root.child(magnitudeKey(634.0f, 478.0f));
    root.child(offsetKey(896.0f, 56.0f));
    root.child(divisorStrip(34.0f, 74.0f));
    root.child(spriteProfile(404.0f, 132.0f));
    root.child(callout(0, 6, {132.0f, 476.0f}, false));    // discidia sl6
    root.child(callout(1, 3, {170.0f, 560.0f}, false));    // armara sl3
    root.child(callout(2, 7, {636.0f, 82.0f}, false));     // vicio sl7
    root.child(callout(3, 5, {1112.0f, 268.0f}, true));    // aevitas sl5

    root.child(arrow(367, 125, false, false, "arrowNext").zIndex(10));
    root.child(arrow(197, 230, true, false, "arrowBack").zIndex(10));
    root.child(bookmarkRail());
    root.child(marginalia());

    ctx.composer.render(std::move(root));
  }

  static float kInkAlphaOf() { return at::kInkAlpha; }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(AstralTome)
