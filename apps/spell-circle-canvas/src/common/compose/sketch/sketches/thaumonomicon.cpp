// thaumonomicon.cpp — THAUMCRAFT 6, THE THAUMONOMICON RESEARCH BROWSER
// =============================================================================
// Thaumcraft 6.1.BETA26 (Azanor, 2018), a Minecraft 1.12.2 Forge mod, PC.
// The screen: GuiResearchBrowser showing the ALCHEMY category — the research
// WEB, not a recipe grid: hand-drawn node plates scattered on a painted
// parallax backdrop, joined by inked elbow routes made of STAMPED TILES, all
// inside an ornamented frame with a tab rail running off the left edge.
//
// Authored at 640 x 400 GUI px (a 1280 x 800 window at Minecraft GUI scale 2),
// rebuilt here at EXACTLY 2x — canvas 1280 x 800. Every constant below is in
// GUI px and goes through g(); divide any canvas number by two to recover the
// mod's own pixel.
//
// -----------------------------------------------------------------------------
// SOURCES — fetched and read, not remembered
//
//  * github.com/TheDarkTower314/Thaumcraft-6-Source-Code (deobfuscated):
//      - client/gui/GuiResearchBrowser.java. drawLine():901-1011 is the whole
//        edge-routing grammar and is transcribed below verbatim into
//        thaumRoute(). :583-670 is the plate/badge/tint state machine.
//        :726-755 is the frame (22x22 corner at UV 13,13 + 64-px edge runs at
//        UV 48,13). :219-233 + :1089-1103 are the category tab rails.
//        :508-534 is the two-plane parallax and its 2.0 : 1.5 depth ratio.
//        :164-186 computes guiBounds — the scroll used here is the game's own
//        default for this category, derived, not chosen (see kLoc).
//      - resources/assets/thaumcraft/research/alchemy.json — all 22 nodes,
//        their [column,row], their meta flags and their parents, verbatim.
//        Nothing here is invented; nothing real is left out.
//      - resources/assets/thaumcraft/lang/en_us.lang — every string on screen.
//      - common/config/ConfigResearch.java:138-144 — the seven category tabs
//        in registration order and each one's AspectList.
//  * github.com/Azanor/thaumcraft-api aspects/Aspect.java:160-210 — the aspect
//    colours as hex ints. The tab glyphs are tinted by each category's
//    dominant aspect, taken from that AspectList.
//
// DOCUMENTED (not invented): the frame geometry and its tile UVs; the 24-px
//   lattice and the 32/16/20-px plate/icon/hover boxes; the four plate
//   silhouettes and which meta flag picks each; the badge offsets and their
//   0.5 scale; the entire drawLine tile walk including bigCorner; the four
//   edge colours and their z order; the 600 ms unlockable pulse and the
//   1.0 / 0.3 / 0.1 tint ladder; the parallax divisors; the tab rail pitch and
//   its 0.6,1.0,1.0 selected tint; every string and text colour; the whole
//   node graph.
// RECONSTRUCTED (and it is all of the pixels): gui_research_browser.png,
//   gui_research_back_3.jpg, BACK_OVER and every item icon are mod assets and
//   are in no repo. The UVs say where the cells are and how big they are; they
//   say nothing about what is drawn in them. So every tile, plate, ornament,
//   badge and 16x16 item glyph here is GENERATED — the route tiles and the
//   frame from the brush vocabulary (ops::Sketchy + lines::cased + a
//   ScatterBrush of ink spatter), the item icons as run-length pixel art on
//   the GUI grid, the way xcom_battlescape.cpp reconstructed ICONS.PCK. Also
//   reconstructed: the save state (which nodes are complete / unlockable /
//   locked) and the Minecraft bitmap face (see PixText).
//
// -----------------------------------------------------------------------------
// FOUR THINGS THE SOURCE SAID THAT THE SCREENSHOT DOES NOT
//
//  1. AN EDGE IS NOT A LINE, IT IS A TILE WALK, AND IT IS L-SHAPED THE OTHER
//     WAY ROUND. drawLine walks VERTICALLY out of the child first, turns once,
//     and runs HORIZONTALLY into the parent — so the bend sits at
//     (childColumn, parentRow), never at the midpoint an orthogonal router
//     would pick. The tiles are 24x24 straights, and the turn is a 24x24 elbow
//     UNLESS both deltas exceed one cell, in which case it is a 48x48 elbow
//     that EATS TWO CELLS OF EACH LEG (`bigCorner`, :904 and :970-1005). That
//     one flag is the entire reason this brief exists: a corner tile that
//     consumes leg length before the side tiles are fitted is precisely what
//     brushes::PatternBrush could not do until this study asked. See THE
//     CORNER REPORT below.
//  2. THE ARROWHEAD IS AT THE CHILD AND ITS GLYPH IS PICKED BY THE FIRST LEG,
//     NOT BY THE EDGE. :938-966 tests ym before xm, so a route with any
//     vertical component gets a vertical arrowhead even when it approaches the
//     child horizontally. REVERSE swaps which end the walk starts from, which
//     silently bends that edge the OTHER way — POTIONSPRAYER is the only node
//     in this category carrying it, and its elbow is the only one in the frame
//     that turns at the parent's column instead of the child's.
//  3. THE PULSE IS ABSOLUTE-TIME, NOT PER-NODE. :610 is
//     sin(systemTime % 600 / 600 * 2pi) * 0.25 + 0.75, read from the WALL
//     CLOCK inside the draw loop — so every unlockable node in the game
//     pulses in exact lockstep, with no phase offset and no per-node state.
//     Six nodes pulse here and they pulse together; staggering them would be
//     prettier and would be wrong.
//  4. THE DEFAULT SCROLL IS DERIVABLE, AND SO IS THE GUI SCALE.
//     updateResearch():246-262 accumulates guiBounds from the node columns and
//     rows and actionPerformed() centres the view on their midpoint, so the
//     scroll is not a choice: at 427x267 it is locX = -114, locY = -106.
//     ScaledResolution then settles the composition. The brief assumed GUI
//     scale 2, at which the 456 GUI px web sits inside a 608 px window with
//     room to spare and the screen reads as a diagram; but Minecraft's AUTO
//     scale at 1280x800 is 3 (1280/4 = 320 passes, 800/4 = 200 fails the 240
//     floor), the window becomes 395 px, and the web is WIDER than it. The
//     bleed is not a styling decision — it is what this artefact does at the
//     resolution it opens at.
//
// -----------------------------------------------------------------------------
// THE CORNER REPORT — brushes::PatternBrush, measured, and now closed
//
// The brief predicted PatternBrush would break on a 48 px corner tile against
// 24 px side tiles. It did. Probed with a scratch sketch (three L routes; a
// 48 px side tile with coloured end caps; a 96 px corner tile with its local
// +x axis drawn, so placement and rotation are both readable off the pixels):
//
//   a. IT RESERVED NO LENGTH FOR A CORNER. Runs were the spans BETWEEN corner
//      distances, both spans were tiled right up to the bend, and the corner
//      was then stamped on top — so a corner tile twice the advance covered,
//      and double-drew, exactly one side tile on each leg. Invisible when the
//      corner art is the same size as the side art, which is why it survived.
//   b. THE CORNER LANDS 3 PX PAST THE BEND, NOT ON IT. Detection scans at
//      step = clamp(advance/4, 1, 6) and records `d - step*0.5`, but the
//      tangent break is first SEEN one step AFTER the vertex, so the recorded
//      distance is (vertex + step/2). Measured: corner centre at bend + 3 px
//      along the OUTGOING leg on all three probes, box size exact
//      (52,193..145,286 for a bend at 96,240 — centre 98.5, 239.5).
//   c. AND IT IS ROTATED TO THE OUTGOING TANGENT, NOT THE BISECTOR. The
//      bisector is built from samples at d +/- 2; since d is already 3 px past
//      the vertex, BOTH land on the outgoing leg, so `before` and `after` are
//      the same vector. The seam corner of a CLOSED contour is the exception —
//      it is inserted at d = 0 and its probes wrap, so it alone gets a true
//      bisector. On a rect that means three corners rotate one way and the
//      fourth rotates 45 degrees off.
//
// (a) is FIXED: `PatternBrush::cornerLength` now reserves cornerLength/2 at
// each end of a corner's two runs and refits the side tiles over the shortened
// span. These edges are its first real consumer, and the numbers fall out
// exactly: reserve = kCell/2 for the small elbow and 1.5*kCell for the big
// one, the routed path is trimmed half a cell at each node, and the runs come
// out to kCell*(yd-1) / kCell*(xd-1) — or (yd-2) / (xd-2) with the big elbow —
// which is tile-for-tile what drawLine's `v` and `h` loops emit, at sx = 1.0.
//
// (b) and (c) ARE ALSO FIXED NOW, and the fix silently broke this study the
// same afternoon it shipped — which is the part worth writing down. Nothing
// in this file changed and no build failed; the render just went wrong, and
// stayed wrong through a review. The scanner learned to bisect
// its own bracket, so `hit.d` is the vertex (the 3 px is gone) and the two leg
// tangents come out of the search, so a corner can finally face a REAL
// bisector. `cornerAlign` was added at the same time and it defaults to
// `Bisector`, because that is what an ornamental elbow wants.
//
// This study's elbow is not ornamental. It is a piece of PIPE: it has an
// entry, an exit, and a handedness, and elbowTile() authors it in the frame
// (c) described — local +x along the OUTGOING leg. The moment the library
// started honouring the true bisector, that art was stamped 45 degrees off,
// and because a 2x2 route is ALL corner (see the arithmetic above: yd-2 = 0
// side tiles), whole edges turned into 45-degree chevrons hanging in space
// with nothing orthogonal left of them. Nothing here changed; the default
// under it did. So edgeEl() now asks for `cornerAlign = Outgoing` EXPLICITLY
// rather than inheriting it from a bug.
//
// The authoring cost (c) predicted is real and stands: an outgoing-aligned
// corner needs one art per turn direction. So does a bisector-aligned one, as
// it happens — the two arms sit at (turn/2, 180 - turn/2) off the bisector,
// which mirrors with the turn sign — so the "one art" hope in the original
// report was wrong. Four elbow arts per ink tier (two sizes x two hands) is
// the floor either way, and it is cheap: each connector has exactly one
// corner and picks its art up front.
//
// One more, found while getting under the 60 FPS gate and not obvious from
// either header: PatternBrush and ScatterBrush bake their art with snapshot(),
// which records DRAW CALLS. So an SkMaskFilter inside tile art is re-run on
// every stamp — a 1.1 px blurred ink bed cost 3.7 ms of a 15.5 ms frame across
// ~145 stamps. Art that will be stamped hundreds of times has to be flat
// geometry.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

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

namespace thaum {

// ---------------------------------------------------------------------------
// SCALE. One constant: GUI px -> canvas px. Nothing else scales.

// GUI scale 3, not 2 — and it is Minecraft's own answer, not a preference.
// ScaledResolution walks `while (i != guiScale && scaledWidth/(i+1) >= 320 &&
// scaledHeight/(i+1) >= 240) ++i`, and at 1280x800 that stops at 3 (1280/4 is
// 320, but 800/4 = 200 < 240). So AUTO gives 3, the GUI space is
// ceil(1280/3) x ceil(800/3) = 427 x 267, and screenX/screenY are 395 x 235.
//
// This is also the whole composition. The ALCHEMY web is 456 x 216 GUI px:
// at scale 2 it is smaller than a 608-px window and sits inside the frame
// like a diagram, and at scale 3 it is WIDER than the 395-px window and runs
// off both side edges — which is the screen everyone remembers. It is the
// same arithmetic either way; the mod just does not open at scale 2 here.
constexpr float U = 3.0f;
constexpr float g(float v) { return v * U; }

constexpr float kCanvasW = 1280.0f, kCanvasH = 800.0f;
constexpr float kGuiW = 427, kGuiH = 267;        // ceil(canvas / 3)
constexpr float kStartX = 16, kStartY = 16;      // GuiResearchBrowser ctor
constexpr float kScreenX = kGuiW - 32;           // width - 32  = 395
constexpr float kScreenY = kGuiH - 32;           // height - 32 = 235
constexpr float kCell = g(24);                   // the lattice cell, canvas px

// The scroll is DERIVED, not chosen: updateResearch():246-262 accumulates
// guiBounds over the category's columns/rows and the view centres on their
// midpoint. For ALCHEMY at this resolution that is
//   locX = floor((-6*24 - 395 + 48 + 12*24 - 24) / 2) = floor(-113.5) = -114
//   locY = floor((-4*24 - 235 + 48 +  4*24 - 24) / 2) = floor(-105.5) = -106
constexpr float kLocX = -114, kLocY = -106;

/** Node centre in canvas px. iconX = startX + col*24 - locX, icon is 16x16,
 *  so the centre is +8 (GuiResearchBrowser:596-600, :664). */
inline SkPoint centreOf(int col, int row) {
  return {g(kStartX + (float)col * 24 - kLocX + 8),
          g(kStartY + (float)row * 24 - kLocY + 8)};
}

/** :598 — the visibility test, and it is a CULL, not a clip: it is applied to
 *  the icon's top-left, so a node whose plate is more than half on screen is
 *  dropped whole. drawLine has no such test, so the EDGES to a culled node
 *  are drawn anyway and run off under the frame band. That asymmetry is what
 *  makes the browser read as a viewport onto a web that continues, and at
 *  this scroll it drops exactly three: BOTTLETAINT at column -6 and the two
 *  column-12 nodes. */
inline bool culled(int col, int row) {
  const float vx = (float)col * 24 - kLocX;
  const float vy = (float)row * 24 - kLocY;
  return !(vx >= -24 && vy >= -24 && vx <= kScreenX && vy <= kScreenY);
}

inline SkPoint centre(const SkRect &r) { return {r.centerX(), r.centerY()}; }

inline Decoration prog(PaintProgram p) { return Decoration(std::move(p)); }

inline SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16) & 255) / 255.0f, (float)((hex >> 8) & 255) / 255.0f,
          (float)(hex & 255) / 255.0f, a};
}
inline SkColor4f mul(SkColor4f c, float k, float a = -1) {
  return {c.fR * k, c.fG * k, c.fB * k, a < 0 ? c.fA : a};
}

/** Deterministic hash — every jitter, tear and speckle comes through here. */
inline uint32_t hash3(int a, int b, int c) {
  uint32_t h = (uint32_t)(a * 374761393 + b * 668265263 + c * 2147483647);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline float noise1(int a, int b, int c) {
  return (float)(hash3(a, b, c) & 0xFFFFu) / 32767.5f - 1.0f; // [-1,1]
}

// ---------------------------------------------------------------------------
// PALETTE. The aspect colours are Aspect.java's, verbatim. Everything else is
// reconstructed ink and parchment for textures that exist in no repo.

namespace aspect {
constexpr uint32_t kAer = 0xFFFF7E, kTerra = 0x56C000, kIgnis = 0xFF5A01;
constexpr uint32_t kAqua = 0x3CD4FC, kOrdo = 0xD5D4EC, kPerditio = 0x404040;
constexpr uint32_t kAlkimia = 0x23AC9D, kPraecantatio = 0xCF00FF;
constexpr uint32_t kVitium = 0x800080, kVictus = 0xDE0005;
constexpr uint32_t kMetallum = 0xB5B5CD, kDesiderium = 0xE6BE44;
constexpr uint32_t kAversio = 0xC05050, kAuram = 0xFFC0FF;
constexpr uint32_t kMachina = 0x8080A0, kHumanus = 0xFFD7C0;
constexpr uint32_t kAlienis = 0x805080, kHerba = 0x01AC00;
constexpr uint32_t kMortuus = 0x6A0005, kInstrumentum = 0x4040EE;
} // namespace aspect

// Reconstructed ink/parchment.
const SkColor4f kInkDeep = rgb(0x0E0A06);
const SkColor4f kInkBody = rgb(0xEADCBC);
const SkColor4f kPaper = rgb(0xA0865E);
const SkColor4f kPaperLit = rgb(0xCDAE7B);
const SkColor4f kPaperDark = rgb(0x2A2113);
const SkColor4f kBrass = rgb(0x8A6E38);
const SkColor4f kBrassLit = rgb(0xC9A860);
const SkColor4f kBrassDark = rgb(0x2A200F);

// Text colours, converted from the source's decimal literals.
const SkColor4f kTextGold = rgb(0xFFAA00);   // §6
const SkColor4f kTextRed = rgb(0xFF5555);    // §c
const SkColor4f kTextYellow = rgb(0xFFFF55); // §e
const SkColor4f kTextWhite = rgb(0xFFFFFF);

// ---------------------------------------------------------------------------
// THE GRAPH — alchemy.json, all 22 entries, verbatim.

enum Meta : uint8_t { kPlain = 0, kRound = 1, kHex = 2, kSpiky = 4,
                      kHidden = 8, kReverse = 16 };
enum State : uint8_t { kComplete, kUnlockable, kLocked };

struct Node {
  const char *key;
  int col, row;
  uint8_t meta;
  int icon;      // index into the reconstructed glyph set
  State state;
  const char *title; // en_us.lang research.<KEY>.title
  uint8_t warp;      // max stage warp — drives drawForbidden()
  bool flagResearch; // EnumResearchFlag.RESEARCH -> the UV(176,16) badge
  bool flagPage;     // EnumResearchFlag.PAGE     -> the UV(208,16) badge
};

// Icon glyph ids (reconstructed art; the names are the mod's own texture
// names from alchemy.json's `icons`).
enum Glyph {
  gAspect, gAlumentum, gIngot, gCluster, gTallow, gBucket, gBottle, gSalts,
  gSoap, gSpa, gSmelter, gJar, gTube, gSmelterThaum, gSmelterVoid, gSmelterAux,
  gVent, gCentrifuge, gThaumatorium, gInput, gUrn, gSprayer,
};

// The save state is RECONSTRUCTED — a mid-game player who has just finished
// the thaumium smelter and can see, but not yet take, the automation branch.
// It is chosen so the one hovered node's tooltip is self-consistent with the
// graph: THAUMATORIUM needs CENTRIFUGE, and CENTRIFUGE is not complete.
constexpr Node kNodes[] = {
    {"BASEALCHEMY", 0, 0, kRound | kHidden, gAspect, kComplete,
     "Basic Alchemy", 0, false, false},
    {"ALUMENTUM", 2, -1, kPlain, gAlumentum, kComplete, "Alumentum", 0, false,
     false},
    {"METALLURGY", 2, 2, kPlain, gIngot, kComplete, "Alchemical Metallurgy", 0,
     false, false},
    {"METALPURIFICATION", 2, 4, kPlain, gCluster, kUnlockable,
     "Metal Purification", 0, false, false},
    {"HEDGEALCHEMY", -2, 0, kPlain, gTallow, kComplete, "Hedge Alchemy", 0,
     false, false},
    {"LIQUIDDEATH", -4, 2, kPlain, gBucket, kComplete, "Liquid Death", 3, false,
     true},
    {"BOTTLETAINT", -6, 0, kPlain, gBottle, kUnlockable, "Bottled Taint", 2,
     true, false},
    {"BATHSALTS", -4, -2, kHidden, gSalts, kComplete, "Purifying Bath Salts", 0,
     false, false},
    {"SANESOAP", -3, -4, kPlain, gSoap, kUnlockable, "Sanity Soap", 0, false,
     false},
    {"ARCANESPA", -5, -4, kPlain, gSpa, kUnlockable, "Arcane Spa", 0, false,
     false},
    {"ESSENTIASMELTER", 4, 0, kSpiky, gSmelter, kComplete, "Essentia Smelting",
     0, false, false},
    {"WARDEDJARS", 4, -2, kRound, gJar, kComplete, "Warded Jars & labels", 0,
     false, false},
    {"TUBES", 6, -2, kPlain, gTube, kComplete, "Essentia Tubes", 0, false,
     false},
    {"ESSENTIASMELTERTHAUMIUM", 8, 0, kPlain, gSmelterThaum, kComplete,
     "Thaumium Essentia Smelter", 0, true, true},
    {"ESSENTIASMELTERVOID", 12, 0, kPlain, gSmelterVoid, kLocked,
     "Void Metal Essentia Smelter", 0, false, false},
    {"IMPROVEDSMELTING", 5, -4, kPlain, gSmelterAux, kLocked,
     "Improved Essentia Distillation (part 1)", 0, false, false},
    {"IMPROVEDSMELTING2", 3, -4, kPlain, gVent, kLocked,
     "Improved Essentia Distillation (part 2)", 0, false, false},
    {"CENTRIFUGE", 7, -4, kPlain, gCentrifuge, kUnlockable,
     "Essentia Centrifuge", 0, false, false},
    {"THAUMATORIUM", 10, -2, kSpiky, gThaumatorium, kLocked,
     "Alchemical Automation", 0, false, false},
    {"ESSENTIATRANSPORT", 12, -2, kPlain, gInput, kLocked,
     "Advanced Essentia Transport", 0, false, false},
    {"EVERFULLURN", -2, 2, kPlain, gUrn, kComplete, "Everfull Urn", 0, false,
     false},
    {"POTIONSPRAYER", -1, -2, kReverse, gSprayer, kUnlockable, "Potion Sprayer",
     0, false, false},
};
constexpr int kNodeCount = (int)(sizeof(kNodes) / sizeof(kNodes[0]));

inline const Node &nodeByKey(const char *k) {
  for (const Node &n : kNodes)
    if (std::string(n.key) == k)
      return n;
  return kNodes[0];
}
inline int indexByKey(const char *k) {
  for (int i = 0; i < kNodeCount; ++i)
    if (std::string(kNodes[i].key) == k)
      return i;
  return 0;
}

/** The four edge tiers of genResearchBackgroundZoomable:550-571 — the colour
 *  the white tile art is MULTIPLIED by, the z it is drawn at, and whether it
 *  carries an arrowhead. Only three of the four occur in this save: nothing in
 *  ALCHEMY has an unknown sibling, so the 0.1875 tier never fires. */
enum EdgeTier : uint8_t {
  kParentKnown,   // 0.6,0.6,0.6  z=3  arrow
  kParentUnknown, // 0.2,0.2,0.2  z=2  arrow
  kSiblingKnown,  // 0.3,0.3,0.4  z=1  no arrow
};
struct Edge {
  const char *child;
  const char *parent;
  EdgeTier tier;
  bool flipped; // source.hasMeta(REVERSE)
};

// Parent edges are drawn only when the parent is in the same category, and are
// SUPPRESSED when the parent lists the child as a sibling (:545) — which is why
// WARDEDJARS<-ESSENTIASMELTER appears once, as a sibling edge, and not twice.
// Parents outside ALCHEMY (UNLOCKALCHEMY, f_toomuchflux, BELLOWS, INFUSION,
// BASEELDRITCH) draw nothing; a `~` parent (POTIONSPRAYER's ~TUBES) is
// explicitly skipped at :547.
constexpr Edge kEdges[] = {
    {"ALUMENTUM", "BASEALCHEMY", kParentKnown, false},
    {"METALLURGY", "BASEALCHEMY", kParentKnown, false},
    {"HEDGEALCHEMY", "BASEALCHEMY", kParentKnown, false},
    {"METALPURIFICATION", "METALLURGY", kParentKnown, false},
    {"LIQUIDDEATH", "HEDGEALCHEMY", kParentKnown, false},
    {"BOTTLETAINT", "HEDGEALCHEMY", kParentKnown, false},
    {"BATHSALTS", "HEDGEALCHEMY", kParentKnown, false},
    {"EVERFULLURN", "HEDGEALCHEMY", kParentKnown, false},
    {"POTIONSPRAYER", "HEDGEALCHEMY", kParentKnown, true},
    {"SANESOAP", "BATHSALTS", kParentKnown, false},
    {"ARCANESPA", "BATHSALTS", kParentKnown, false},
    {"ESSENTIASMELTER", "ALUMENTUM", kParentKnown, false},
    {"ESSENTIASMELTER", "METALLURGY", kParentKnown, false},
    {"WARDEDJARS", "ESSENTIASMELTER", kSiblingKnown, false},
    {"TUBES", "WARDEDJARS", kParentKnown, false},
    {"IMPROVEDSMELTING", "TUBES", kParentKnown, false},
    {"CENTRIFUGE", "TUBES", kParentKnown, false},
    {"ESSENTIASMELTERTHAUMIUM", "TUBES", kParentKnown, false},
    {"ESSENTIASMELTERTHAUMIUM", "METALLURGY", kParentKnown, false},
    {"IMPROVEDSMELTING2", "IMPROVEDSMELTING", kParentUnknown, false},
    {"THAUMATORIUM", "CENTRIFUGE", kParentUnknown, false},
    {"THAUMATORIUM", "ESSENTIASMELTERTHAUMIUM", kParentKnown, false},
    {"ESSENTIATRANSPORT", "THAUMATORIUM", kParentUnknown, false},
    {"ESSENTIASMELTERVOID", "ESSENTIASMELTERTHAUMIUM", kParentKnown, false},
};
constexpr int kEdgeCount = (int)(sizeof(kEdges) / sizeof(kEdges[0]));

inline float tierMul(EdgeTier t) {
  return t == kParentKnown ? 0.6f : t == kParentUnknown ? 0.2f : 0.3f;
}
inline SkColor4f tierTint(EdgeTier t, SkColor4f base) {
  const float k = tierMul(t);
  SkColor4f c = mul(base, k);
  if (t == kSiblingKnown)
    c.fB = base.fB * 0.4f; // 0.3,0.3,0.4 — the one tier with a hue
  return c;
}
inline int tierZ(EdgeTier t) {
  return t == kParentKnown ? 3 : t == kParentUnknown ? 2 : 1;
}

// ---------------------------------------------------------------------------
// THE ROUTER — drawLine():901-1011, transcribed.
//
// The walk starts at the CHILD (or, when REVERSE, at the parent), runs
// VERTICALLY to the other end's row, turns exactly once, and runs
// HORIZONTALLY to one cell short of the other end. So the bend sits at
// (startColumn, endRow) — never at the midpoint an orthogonal router picks.
// Straight tiles are 24x24; the turn is a 24x24 elbow, or a 48x48 elbow that
// eats TWO cells of each leg when both deltas exceed one (`bigCorner`, :904).
//
// The path handed to the brush is ONE open contour with hard 90-degree
// breaks, trimmed half a cell at each end (the mod's first tile sits a whole
// cell from the node, and its last stops a cell short). PatternBrush::
// cornerLength then reserves the elbow's own room, and the side runs come out
// to an exact integer count with sx = 1.0:
//
//   reserve   = kCell/2 (small elbow)   or 1.5*kCell (big)
//   vertical  = kCell*yd - kCell/2 - reserve = kCell*(yd-1) or kCell*(yd-2)
//   horizontal= kCell*xd - kCell/2 - reserve = kCell*(xd-1) or kCell*(xd-2)
//
// which is tile-for-tile what the `v` and `h` loops at :970-1005 emit.

struct RouteShape {
  bool bigCorner = false;
  bool hasCorner = false;
  float handed = 0; // cross(incoming, outgoing): picks the elbow's mirror
};

/** Classify without building — the Brush must pick its elbow art before the
 *  path exists. */
inline RouteShape shapeOf(const Node &child, const Node &parent, bool flipped) {
  const int sx = flipped ? parent.col : child.col;
  const int sy = flipped ? parent.row : child.row;
  const int ex = flipped ? child.col : parent.col;
  const int ey = flipped ? child.row : parent.row;
  const int xd = std::abs(ex - sx), yd = std::abs(ey - sy);
  const float xm = xd == 0 ? 0.0f : (ex > sx ? 1.0f : -1.0f);
  const float ym = yd == 0 ? 0.0f : (ey > sy ? 1.0f : -1.0f);
  return {xd > 1 && yd > 1, xd > 0 && yd > 0, -ym * xm};
}

/** The elbow's reserved arc length on EACH adjoining run, canvas px. */
inline float cornerArm(bool big) { return big ? kCell * 1.5f : kCell * 0.5f; }

inline Router thaumRoute(bool flipped) {
  return [flipped](const SkRect &fromR, const SkRect &toR) {
    const SkPoint C = centre(fromR), P = centre(toR);
    const SkPoint S = flipped ? P : C; // the walk's start
    const SkPoint E = flipped ? C : P;
    const int xd = (int)std::lround(std::abs(E.fX - S.fX) / kCell);
    const int yd = (int)std::lround(std::abs(E.fY - S.fY) / kCell);
    const float xm = xd == 0 ? 0.0f : (E.fX > S.fX ? 1.0f : -1.0f);
    const float ym = yd == 0 ? 0.0f : (E.fY > S.fY ? 1.0f : -1.0f);
    const SkPoint bend{S.fX, S.fY + ym * kCell * (float)yd};
    const float h = kCell * 0.5f;

    SkPathBuilder b;
    if (yd == 0 && xd == 0)
      return b.detach();
    if (yd == 0) { // a pure horizontal run, no turn
      b.moveTo(S.fX + xm * h, S.fY);
      b.lineTo(E.fX - xm * h, E.fY);
      return b.detach();
    }
    if (xd == 0) { // a pure vertical run, no turn
      b.moveTo(S.fX, S.fY + ym * h);
      b.lineTo(E.fX, E.fY - ym * h);
      return b.detach();
    }
    b.moveTo(S.fX, S.fY + ym * h);
    b.lineTo(bend);
    b.lineTo(E.fX - xm * h, E.fY);
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// INK. Every tile in the route vocabulary is generated from the brush kit:
// ops::Sketchy jitter over lines::cased, plus a light ScatterBrush of spatter.
// Art elements are held by the sketch so their node identity is stable and the
// PatternBrush bake happens once.

/** A pen stroke as a Brush: the ink BED (wide, dark, blurred — ink soaking
 *  into paper), a triple rule whose bold spine carries the value and whose
 *  hairline outriders are the pen's own casing, and a scatter of spatter
 *  grains. State moves value, never width — the run-2 width law. */
inline Brush penBrush(SkColor4f tint, float k, const Element &spatter,
                      float bow = 0.0f) {
  Brush br;
  if (bow > 0)
    br.op(ops::Wave{.amplitude = g(bow), .wavelength = g(30)});
  br.op(ops::Sketchy{.segLength = g(4.5f), .deviation = g(0.45f), .seed = 21});
  // The bed: a crisp dark outline a GUI px wider than the body. NO blur —
  // and that is a measured constraint, not a taste. PatternBrush and
  // ScatterBrush bake their art with snapshot(), which records DRAW CALLS,
  // so an SkMaskFilter inside a tile is re-run on every stamp: 145 stamps of
  // a 1.1 px blurred bed cost 3.7 ms of a 15.5 ms frame. Art that will be
  // stamped hundreds of times has to be flat geometry.
  br.leg(LayeredBrush{{
      {g(4.0f) * k, mul(kInkDeep, 1.0f, 0.34f * tint.fA)},
      {g(2.6f) * k, mul(kInkDeep, 1.0f, 0.92f * tint.fA)},
  }});
  // the body: one rule — state moves VALUE, never width
  br.leg(lines::Line{.width = g(1.3f) * k, .fill = Fill::color(tint)});
  // the dry edge: a hairline offset to one side, where the nib lifted
  br.leg(lines::Line{.width = g(0.45f) * k,
                     .fill = Fill::color(mul(tint, 1.35f, 0.85f * tint.fA))},
         {ops::Offset{.px = -g(0.85f), .step = g(2)}});
  br.leg(brushes::ScatterBrush{.art = spatter,
                               .spacing = g(13),
                               .seed = 9,
                               .jitterAlong = g(4),
                               .jitterNormal = g(2.4f),
                               .jitterScale = 0.8f,
                               .jitterRotateDeg = 40,
                               .alignToPath = false,
                               .reach = g(6)});
  return br;
}

/** One ink grain — the ScatterBrush cell. */
inline Element spatterCell(SkColor4f tint) {
  return box().width(g(1.2f)).height(g(1.2f)).outline(shapes::circle()).fill(
      Fill::color(mul(tint, 0.85f, 0.7f * tint.fA)));
}

/** One 24x24 straight tile, authored 28 GUI px wide so successive stamps
 *  overlap by 2 px and the sketchy jitter never opens a seam. The KNOT at the
 *  tile's centre is what makes the 24-px repeat legible as a repeat — the run
 *  has to read as a chain of stamped cells, because that is what it is. */
inline Element straightTile(SkColor4f tint, const Element &spatter,
                            const Element &knot) {
  const float w = g(28), h = g(14);
  Brush br = penBrush(tint, 1.0f, spatter, 0.45f);
  br.leg(brushes::ScatterBrush{
      .art = knot,
      .place = {.mode = brushes::Placement::Mode::CentralPoint},
      .alignToPath = true,
      .reach = g(6)});
  return box()
      .width(w)
      .height(h)
      .outline([w, h](SkSize) {
        SkPathBuilder p;
        p.moveTo(0, h * 0.5f);
        p.lineTo(w, h * 0.5f);
        return p.detach();
      })
      .stroke(std::move(br));
}

/** The knot: a small inked lozenge, one per straight tile. */
inline Element knotCell(SkColor4f tint) {
  Element e = box()
                  .width(g(4.4f))
                  .height(g(3.2f))
                  .outline(shapes::polygon(4, 0))
                  .fill(Fill::color(mul(tint, 1.12f)));
  e.stroke(PathFormat{.width = g(1.1f),
                      .strokeFill =
                          Fill::color(mul(kInkDeep, 1, 0.9f * tint.fA))});
  return e;
}

/** The elbow tile, drawn in the frame PatternBrush stamps it in — local +x
 *  along the OUTGOING leg, which the brush only does when it is ASKED
 *  (`cornerAlign = Outgoing`, set in edgeEl; the default is the bisector, and
 *  under it this art lands 45 degrees off — see THE CORNER REPORT). In that
 *  frame:
 *
 *    the bend    is the art's own centre,
 *    the exit    is at local (+arm, 0),
 *    the entry   is at local (0, cross(u,w) * arm),
 *
 *  so a left turn and a right turn are MIRRORS and need two arts — which is
 *  cheap, because each connector has exactly one corner and picks its art up
 *  front. `arm` is the reserved length, kCell/2 for the 24x24 elbow and
 *  1.5*kCell for the 48x48 one. The turn is HARD: the mod stamps a cell, it
 *  does not round a join. */
inline Element elbowTile(float arm, float handed, SkColor4f tint,
                         const Element &spatter, const Element &knot) {
  const float pad = g(7);
  const float half = arm + pad;
  const float side = half * 2.0f;
  const SkPoint entry{half, half + handed * arm};
  const SkPoint exit{half + arm, half};
  Brush br = penBrush(tint, 1.0f, spatter, 0.0f);
  br.leg(brushes::ScatterBrush{
      .art = knot,
      .place = {.mode = brushes::Placement::Mode::InnerVertices},
      .alignToPath = true,
      .reach = g(6)});
  return box()
      .width(side)
      .height(side)
      .outline([entry, exit, half](SkSize) {
        SkPathBuilder p;
        p.moveTo(entry);
        p.lineTo(half, half);
        p.lineTo(exit);
        return p.detach();
      })
      .stroke(std::move(br));
}

/** The 32x32 arrowhead stamped at the child end. */
inline Element arrowCell(SkColor4f tint) {
  return box()
      .width(g(9))
      .height(g(7.5f))
      .outline(shapes::arrow(0.02f, 0.98f))
      .fill(Fill::color(tint))
      .stroke(PathFormat{.width = g(1),
                         .strokeFill =
                             Fill::color(mul(kInkDeep, 1, 0.6f * tint.fA))});
}

// ---------------------------------------------------------------------------
// THE PLATES. Four silhouettes chosen by meta (:621-637), each a torn paper
// plate with a sketched ink border. HIDDEN adds 32 to V — a second, tattered
// variant of every one of them.

/** A hand-torn square: the perimeter walked at 40 stations, each pushed out
 *  along its own normal by a seeded amount. Nothing here is a rounded rect. */
inline shapes::OutlineFn tornSquare(uint32_t seed, float amp) {
  return [seed, amp](SkSize s) {
    SkPathBuilder p;
    const int n = 40;
    const float w = s.width(), h = s.height();
    for (int i = 0; i < n; ++i) {
      const float t = (float)i / (float)n * 4.0f;
      const int e = (int)t;
      const float f = t - (float)e;
      SkPoint q;
      SkVector out;
      switch (e) {
      case 0: q = {w * f, 0}; out = {0, -1}; break;
      case 1: q = {w, h * f}; out = {1, 0}; break;
      case 2: q = {w * (1 - f), h}; out = {0, 1}; break;
      default: q = {0, h * (1 - f)}; out = {-1, 0}; break;
      }
      const float d = noise1((int)seed, i, 3) * amp;
      const SkPoint r{q.fX + out.fX * d, q.fY + out.fY * d};
      if (i == 0)
        p.moveTo(r);
      else
        p.lineTo(r);
    }
    p.close();
    return p.detach();
  };
}

/** The plate: silhouette + parchment + tooth + a sketched double rule. The
 *  border is a Brush, never a stroke width — standing order 3. */
inline Element plateArt(uint8_t meta, uint32_t seed, const Element &spatter) {
  const bool hidden = (meta & kHidden) != 0;
  shapes::OutlineFn shape = tornSquare(seed, g(hidden ? 2.4f : 1.3f));
  if (meta & kRound)
    shape = shapes::blob(seed, 0.055f, 9);
  else if (meta & kHex)
    shape = shapes::polygon(6, 90);

  const SkColor4f face = hidden ? mul(kPaper, 0.80f) : kPaper;
  const SkColor4f lit = hidden ? mul(kPaperLit, 0.80f) : kPaperLit;

  Element e = box()
                  .width(g(32))
                  .height(g(32))
                  .outline(shape)
                  .fill(Material::radialUnit({0.38f, 0.32f}, 1.05f,
                                             {{0.0f, lit},
                                              {0.55f, face},
                                              {1.0f, mul(face, 0.42f)}}))
                  .overlay(lines::Hatch{.strokeFill = Fill::color(
                                            mul(kPaperDark, 1, 0.13f)),
                                        .spacing = g(3.2f),
                                        .width = g(0.6f),
                                        .angleDeg = 32});
  // A doubled rule: a solid outer and a dotted inner that stops short.
  Brush rule;
  rule.op(ops::Sketchy{.segLength = g(5), .deviation = g(0.7f), .seed = seed});
  lines::Line outer;
  outer.width = g(1.6f);
  outer.fill = Fill::color(mul(kInkDeep, 1.0f, hidden ? 0.55f : 0.9f));
  rule.leg(outer);
  lines::Line inner;
  inner.width = g(0.8f);
  inner.fill = Fill::color(mul(kBrassLit, hidden ? 0.35f : 0.75f));
  inner.dashIntervals = {g(2.0f), g(hidden ? 4.0f : 2.5f)};
  rule.leg(inner, {ops::Offset{.px = g(2.4f), .step = g(2)}});
  e.stroke(rule);
  return e;
}

/** SPIKY stamps a SECOND plate over the first (:635) — an eight-pointed star
 *  with concave arms, which is the only place shapes::star's waist earns its
 *  keep. */
inline Element spikyOverlay(uint32_t seed) {
  Element e = box()
                  .inset(0)
                  .outline(shapes::star(8, 0.74f, 0.35f))
                  .fill(Fill::color(mul(kBrass, 1.0f, 0.30f)));
  Brush br;
  br.op(ops::Sketchy{.segLength = g(4), .deviation = g(0.6f), .seed = seed});
  lines::Line l;
  l.width = g(1.1f);
  l.fill = Fill::color(mul(kBrassLit, 0.9f, 0.85f));
  br.leg(l);
  e.stroke(br);
  return e;
}

// ---------------------------------------------------------------------------
// THE ITEM GLYPHS — 16x16 GUI px, drawn as whole GUI pixels so nothing lands
// off the grid. The mod's textures are in no repo; these are reconstructions
// named for the textures alchemy.json asks for.

inline uint32_t mulHex(uint32_t hex, float k) {
  const uint32_t r = (uint32_t)(((hex >> 16) & 255) * k);
  const uint32_t gg = (uint32_t)(((hex >> 8) & 255) * k);
  const uint32_t b = (uint32_t)((hex & 255) * k);
  return (r << 16) | (gg << 8) | b;
}

struct Ink {
  SkCanvas &c;
  void r(float x, float y, float w, float h, SkColor4f col) const {
    if (col.fA <= 0)
      return;
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor4f(col, nullptr);
    c.drawRect(SkRect::MakeXYWH(g(x), g(y), g(w), g(h)), p);
  }
  void px(float x, float y, SkColor4f col) const { r(x, y, 1, 1, col); }
};

/** A stoppered phial/bottle silhouette shared by several icons. */
inline void glassVessel(const Ink &k, SkColor4f liquid, float top = 4) {
  k.r(6, top - 1, 4, 2, rgb(0x6B5030));           // cork
  k.r(6, top + 1, 4, 1, rgb(0x8A8FA0));           // neck
  k.r(5, top + 2, 6, 11 - (top - 4), rgb(0xB6C6D6, 0.35f));
  k.r(5, top + 2, 1, 11 - (top - 4), rgb(0xE7F1F8, 0.55f));
  k.r(6, top + 6, 4, 7 - (top - 4), liquid);
  k.r(5, 14, 6, 1, rgb(0x2A3240));
}

inline void drawGlyph(SkCanvas &canvas, int glyph, float alpha) {
  const Ink k{canvas};
  auto a = [alpha](uint32_t hex, float mulA = 1.0f) {
    return rgb(hex, alpha * mulA);
  };
  switch (glyph) {
  case gAspect: { // cat_alchemy.png — the alkimia aspect medallion
    const SkColor4f c = a(aspect::kAlkimia);
    for (int i = 0; i < 6; ++i) {
      const float ang = (float)i * 60.0f * 0.0174533f;
      k.px(8 + 5 * std::cos(ang) - 0.5f, 8 + 5 * std::sin(ang) - 0.5f, c);
    }
    k.r(6, 4, 4, 1, c);
    k.r(5, 5, 1, 6, c);
    k.r(10, 5, 1, 6, c);
    k.r(6, 11, 4, 1, c);
    k.r(7, 7, 2, 2, a(aspect::kAlkimia, 0.75f));
    break;
  }
  case gAlumentum: // a burning nugget
    k.r(6, 6, 4, 6, a(0x241A12));
    k.r(7, 4, 2, 3, a(aspect::kIgnis));
    k.r(6, 7, 1, 3, a(0xFFB25A, 0.9f));
    k.r(9, 8, 1, 2, a(0xFF8020, 0.9f));
    k.r(5, 12, 6, 1, a(0x120C08));
    break;
  case gIngot: // ingot_brass
    k.r(3, 8, 10, 4, a(0xB98A32));
    k.r(4, 7, 8, 1, a(0xE0BE6A));
    k.r(3, 11, 10, 1, a(0x6B4E18));
    k.r(5, 9, 6, 1, a(0xE8D296, 0.7f));
    break;
  case gCluster: // cluster_iron
    k.r(4, 9, 3, 4, a(0x6E6E72));
    k.r(7, 6, 4, 7, a(0x9A9AA2));
    k.r(8, 4, 2, 3, a(0xC8C8D0));
    k.r(11, 10, 2, 3, a(0x55555A));
    k.r(8, 7, 1, 3, a(0xE6E6EE, 0.8f));
    break;
  case gTallow: // tallow
    k.r(5, 5, 6, 8, a(0xE3D8A8));
    k.r(5, 4, 6, 1, a(0xF6EFCC));
    k.r(5, 12, 6, 1, a(0x8C8358));
    k.r(7, 7, 1, 4, a(0xFFFCE2, 0.55f));
    break;
  case gBucket: // bucket_death
    k.r(4, 5, 8, 8, a(0x8D9299));
    k.r(5, 6, 6, 5, a(aspect::kMortuus));
    k.r(4, 4, 8, 1, a(0xB9BEC6));
    k.r(4, 12, 8, 1, a(0x4A4E55));
    k.r(3, 5, 1, 4, a(0x6E7278));
    break;
  case gBottle: // bottle_taint
    glassVessel(k, a(aspect::kVitium), 3);
    k.px(7, 9, a(0xC060FF, 0.9f));
    break;
  case gSalts: // bath_salts
    k.r(4, 8, 8, 5, a(0xD8CFE6));
    k.r(4, 7, 8, 1, a(0xF0E9F8));
    for (int i = 0; i < 7; ++i)
      k.px(4 + i, 5 + (float)(hash3(i, 3, 5) % 3u), a(0xEDE6FA, 0.85f));
    k.r(4, 12, 8, 1, a(0x807A90));
    break;
  case gSoap: // sanity_soap
    k.r(4, 7, 8, 5, a(0xE6E0C4));
    k.r(4, 6, 8, 1, a(0xF7F3DC));
    k.r(4, 11, 8, 1, a(0x8E8868));
    k.r(6, 8, 4, 2, a(0xC9C29A, 0.8f));
    k.px(11, 5, a(0xFFFFFF, 0.7f));
    k.px(12, 4, a(0xFFFFFF, 0.5f));
    break;
  case gSpa: // spa
    k.r(3, 9, 10, 4, a(0x9A7A50));
    k.r(3, 8, 10, 1, a(0xC29B66));
    k.r(4, 10, 8, 2, a(aspect::kAqua, 0.8f));
    for (int i = 0; i < 3; ++i)
      k.px(5 + i * 3, 5 + (float)(hash3(i, 7, 2) % 2u), a(0xDDF2FF, 0.6f));
    break;
  case gSmelter: // smelter_basic — a squat crucible on legs
  case gSmelterThaum:
  case gSmelterVoid: {
    const uint32_t body = glyph == gSmelter    ? 0x7E5A34
                          : glyph == gSmelterThaum ? 0x6C6AA8
                                                   : 0x2C2438;
    k.r(3, 5, 10, 7, a(body));
    k.r(3, 4, 10, 1, a(body + 0x181818));
    k.r(4, 6, 8, 3, a(aspect::kIgnis, 0.85f));
    k.r(5, 7, 6, 1, a(0xFFD27A, 0.9f));
    k.r(3, 12, 2, 2, a(mulHex(body, 0.6f)));
    k.r(11, 12, 2, 2, a(mulHex(body, 0.6f)));
    break;
  }
  case gJar: // jar_normal
    k.r(4, 4, 8, 2, a(0x8A6E38));
    k.r(4, 6, 8, 8, a(0xC5D9E4, 0.42f));
    k.r(4, 6, 1, 8, a(0xE9F4FA, 0.6f));
    k.r(5, 9, 6, 4, a(aspect::kAlkimia, 0.85f));
    k.r(4, 13, 8, 1, a(0x3A4450));
    break;
  case gTube: // tube
    k.r(2, 7, 12, 3, a(0x8A8FA0));
    k.r(2, 7, 12, 1, a(0xC0C6D6));
    k.r(6, 6, 4, 5, a(0x6B7080));
    k.r(2, 9, 12, 1, a(0x4A4E5A));
    break;
  case gSmelterAux: // smelter_aux
    k.r(4, 6, 8, 7, a(0x6E5A3A));
    k.r(4, 5, 8, 1, a(0x8E7448));
    k.r(6, 3, 4, 3, a(0x8A8FA0));
    k.r(5, 8, 6, 3, a(aspect::kIgnis, 0.7f));
    break;
  case gVent: // smelter_vent
    k.r(5, 8, 6, 5, a(0x6E6E72));
    k.r(4, 7, 8, 1, a(0x9A9AA2));
    for (int i = 0; i < 3; ++i)
      k.r(6 + i * 2, 3 + (float)(hash3(i, 2, 9) % 2u), 1, 3,
          a(0xCFE2EE, 0.55f));
    break;
  case gCentrifuge: // centrifuge
    k.r(4, 3, 8, 3, a(0x8A8FA0));
    k.r(5, 6, 6, 6, a(0x5E626C));
    k.r(6, 7, 4, 4, a(aspect::kAlkimia, 0.8f));
    k.r(3, 12, 10, 2, a(0x3E424A));
    k.px(5, 4, a(0xE0E6F0, 0.8f));
    break;
  case gThaumatorium: // thaumatorium
    k.r(3, 4, 10, 9, a(0x5A4A2E));
    k.r(3, 3, 10, 1, a(0x7E6A44));
    k.r(5, 6, 6, 5, a(0x1C1810));
    k.r(6, 7, 4, 3, a(aspect::kPraecantatio, 0.85f));
    k.r(2, 6, 1, 5, a(0x8A6E38));
    k.r(13, 6, 1, 5, a(0x8A6E38));
    break;
  case gInput: // essentia_input
    k.r(4, 4, 8, 8, a(0x6C6AA8));
    k.r(5, 5, 6, 6, a(0x2A2840));
    k.r(6, 6, 4, 4, a(aspect::kAlkimia, 0.9f));
    k.r(4, 12, 8, 1, a(0x3A3860));
    break;
  case gUrn: // everfull_urn
    k.r(5, 3, 6, 2, a(0x7E6242));
    k.r(4, 5, 8, 8, a(0x9A7A50));
    k.r(4, 5, 1, 8, a(0xC29B66));
    k.r(5, 7, 6, 4, a(aspect::kAqua, 0.75f));
    k.r(4, 13, 8, 1, a(0x50402A));
    break;
  default: // potion_sprayer
    k.r(4, 6, 5, 7, a(0x8A8FA0));
    k.r(5, 7, 3, 5, a(aspect::kVictus, 0.8f));
    k.r(9, 4, 3, 3, a(0x6B7080));
    for (int i = 0; i < 4; ++i)
      k.px(12 + (float)(i % 2), 3 + (float)i, a(0xFFB6C8, 0.55f));
    break;
  }
}

/** The icon element: a 16x16 GUI box whose paint program is the glyph. `bw`
 *  is drawResearchIcon's monochrome pass — a locked node's icon is drawn at
 *  0.1-0.2 grey (:639-643, :683). */
inline Element iconEl(int glyph, float alpha, bool bw) {
  return box().width(g(16)).height(g(16)).background(
      prog([glyph, alpha, bw](SkCanvas &c, const PaintContext &) {
        if (!bw) {
          drawGlyph(c, glyph, alpha);
          return;
        }
        c.saveLayer(nullptr, nullptr);
        drawGlyph(c, glyph, 1.0f);
        SkPaint dim;
        dim.setBlendMode(SkBlendMode::kSrcIn);
        dim.setColor4f({0.18f, 0.18f, 0.18f, alpha}, nullptr);
        c.drawPaint(dim);
        c.restore();
      }));
}

// ---------------------------------------------------------------------------
// THE BADGES. UV(176,16) "new research" at (iconX-9, iconY-9) and UV(208,16)
// "new page" at (iconX-9, iconY+9), both drawn through glScaled(0.5) — so a
// 32x32 cell lands as a 16x16 badge whose CENTRE is 1 px outside the icon's
// corner (:644-659).

inline Element researchBadge() {
  return box()
      .width(g(16))
      .height(g(16))
      .outline(shapes::star(4, 0.30f, 0.55f))
      .fill(Material::radialUnit({0.5f, 0.5f}, 0.9f,
                                 {{0, rgb(0xFFF3C0)},
                                  {0.45f, rgb(0xFFAA00)},
                                  {1, rgb(0xFFAA00, 0)}}));
}
inline Element pageBadge() {
  Element e = box()
                  .width(g(11))
                  .height(g(13))
                  .outline([](SkSize s) {
                    SkPathBuilder p;
                    const float w = s.width(), h = s.height(), c = w * 0.42f;
                    p.moveTo(0, 0);
                    p.lineTo(w - c, 0);
                    p.lineTo(w, c);
                    p.lineTo(w, h);
                    p.lineTo(0, h);
                    p.close();
                    return p.detach();
                  })
                  .fill(Fill::color(rgb(0xD9E8C6)));
  e.stroke(PathFormat{.width = g(1),
                      .strokeFill = Fill::color(rgb(0x2A3A1E))});
  e.overlay(lines::Hatch{.strokeFill = Fill::color(rgb(0x5A7A46, 0.75f)),
                         .spacing = g(2.4f),
                         .width = g(0.8f),
                         .angleDeg = 0});
  return e;
}

// ---------------------------------------------------------------------------
// drawForbidden (:1013-1020): a vitium-purple node swirl stamped over any
// research whose stages carry warp, at :603 — BEFORE the plate, so what a
// player actually sees is a corona bleeding out from under the node rather
// than a mark on it. LIQUIDDEATH (warp 3) and BOTTLETAINT (warp 2) are the
// two in this category. The mod animates it by walking 32 frames of
// nodeTexture and calls renderQuadCentered at 32x32; this is one generated
// lobed halo, drawn a little wider so the corona reads at all now the plate
// art fills more of its cell, on a bound rotation — paint-only volatility,
// so the node keeps its cached picture.

inline Element warpSwirl(const ch::Output<float> *spin, int strength) {
  const float a = 0.30f + 0.09f * (float)strength;
  Element e = box()
                  .width(g(44))
                  .height(g(44))
                  .outline(shapes::star(6, 0.50f, 0.62f))
                  .fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                             {{0.0f, rgb(0xC060FF, a)},
                                              {0.45f, rgb(0x7A0BA8, a * 0.8f)},
                                              {1.0f, rgb(0x2A0038, 0)}}))
                  .blend(SkBlendMode::kPlus)
                  .rotate(bind(spin).scale(360.0f));
  return e;
}

// ---------------------------------------------------------------------------
// THE FRAME (:726-755). A 22-px band whose outer edge is at -2 on every side,
// built from a 22x22 corner tile and 64-px edge runs. This is the one place
// the stock PatternBrush corner path is used: a closed rect has four hard
// 90-degree breaks, and 3 px of misregistration on a 128-px tile does not
// read. The inner rule beside it is the other idiom — four OPEN contours that
// stop short of the corners instead of mitring.

/** One 64x22 GUI edge run: a brass band with beading and rivets. Authored
 *  along +x; the brush rotates it onto each side. */
inline Element frameRun() {
  const float w = g(64), h = g(22);
  return box().width(w).height(h).background(
      prog([](SkCanvas &c, const PaintContext &in) {
        const Ink k{c};
        const float W = in.size.width() / U, H = in.size.height() / U;
        // the band
        k.r(0, 2, W, H - 4, kBrass);
        k.r(0, 2, W, 1, kBrassLit);
        k.r(0, H - 3, W, 1, kBrassDark);
        k.r(0, 5, W, 1, mul(kBrassDark, 1, 0.55f));
        k.r(0, H - 6, W, 1, mul(kBrassLit, 1, 0.35f));
        // beading: a lens every 8 px, and a rivet every 16
        for (int i = 0; i < (int)W; i += 8) {
          k.r((float)i + 2, 7, 4, H - 14, mul(kBrass, 1.22f));
          k.r((float)i + 3, 8, 2, H - 16, mul(kBrass, 0.72f));
        }
        for (int i = 8; i < (int)W; i += 16) {
          k.r((float)i - 1, H / 2 - 1, 2, 2, kBrassLit);
          k.px((float)i - 1, H / 2, mul(kBrassDark, 1, 0.8f));
        }
      }));
}

/** UV(13,13), 22x22 — and it is the SAME CELL twice. drawTexturedModalRect
 *  stamps it at the four frame corners (:751-754) and again, at (x-3, y-3),
 *  behind every category tab (:1105). So one art serves both, and the tab
 *  rail is literally the frame's own corner boss repeated down the margin.
 *  `tint` is the multiply the mod applies: 1,1,1 normally and 0.6,1.0,1.0 for
 *  the selected category (:1099-1103). */
inline Element cornerPlate(SkColor4f tint) {
  const float s = g(24);
  return box().width(s).height(s).background(
      prog([tint](SkCanvas &c, const PaintContext &in) {
        const float m = in.size.width() * 0.5f;
        auto T = [tint](SkColor4f a) {
          return SkColor4f{a.fR * tint.fR, a.fG * tint.fG, a.fB * tint.fB,
                           a.fA};
        };
        SkPaint p;
        p.setAntiAlias(true);
        // the seating shadow, then the boss body as a lit sphere
        p.setColor4f(T(mul(kBrassDark, 0.7f, 0.85f)), nullptr);
        c.drawCircle(m + g(0.6f), m + g(0.8f), g(10.6f), p);
        for (int i = 0; i < 9; ++i) {
          const float f = (float)i / 8.0f;
          p.setColor4f(T(mul(kBrass, 1.55f - 0.85f * f)), nullptr);
          c.drawCircle(m - g(1.5f) * (1 - f), m - g(1.8f) * (1 - f),
                       g(10.0f) * (1.0f - 0.62f * f), p);
        }
        // four studs on the diagonals — the cell's own ornament
        for (int i = 0; i < 4; ++i) {
          const float a = 0.7853982f + (float)i * 1.5707963f;
          const float x = m + std::cos(a) * g(7.2f);
          const float y = m + std::sin(a) * g(7.2f);
          p.setColor4f(T(mul(kBrassDark, 1.0f, 0.9f)), nullptr);
          c.drawCircle(x, y, g(1.9f), p);
          p.setColor4f(T(mul(kBrassLit, 1.15f)), nullptr);
          c.drawCircle(x - g(0.35f), y - g(0.4f), g(1.2f), p);
        }
        // the bevel: a bright arc up-left, a dark arc down-right
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(g(1.3f));
        const SkRect ring = SkRect::MakeLTRB(m - g(10), m - g(10), m + g(10),
                                            m + g(10));
        p.setColor4f(T(mul(kBrassLit, 1.25f, 0.9f)), nullptr);
        c.drawArc(ring, 150, 150, false, p);
        p.setColor4f(T(mul(kBrassDark, 1.0f, 0.9f)), nullptr);
        c.drawArc(ring, 330, 150, false, p);
        // the recess and its catchlight
        p.setStyle(SkPaint::kFill_Style);
        p.setColor4f(T(mul(kBrassDark, 1.1f)), nullptr);
        c.drawCircle(m, m, g(4.4f), p);
        p.setColor4f(T(mul(kBrass, 1.35f)), nullptr);
        c.drawCircle(m, m, g(3.1f), p);
        p.setColor4f(T(mul(kBrassLit, 1.3f, 0.85f)), nullptr);
        c.drawCircle(m - g(1.0f), m - g(1.1f), g(1.3f), p);
      }));
}

// ---------------------------------------------------------------------------
// PIXEL TYPE. Minecraft's ascii.png is a 1-bit bitmap face on an 8-px cell and
// is in no repo. SigilWeave has no bitmap-font path, so the substitute is
// shaped with ShapingStyle::aliased at the ORIGINAL 1x size, rasterised into
// an A8 mask, and presented at 2x with kNearest — which makes it a bitmap
// face again, on the grid, with no antialiasing anywhere. The drop shadow is
// the game's own: FontRenderer offsets by +1 and multiplies the colour by
// 0.25 ((color & 16579836) >> 2).
//
// THE SIZE IS 10, AND IT WAS 9, AND AT 9 EVERY LOWERCASE e WAS AN a.
// "Alchemical" read "Alchamical" and "research" read "rasaarch" — the most-read
// text on the plate, wrong in the one letter English uses most. The cause is
// x-height, not thresholding. Minecraft's own glyph body is 7 px tall on a 5 px
// x-height, and e's counter is one whole pixel of that. Menlo at 9 px has an
// x-height of 4.4 px, so under kAlias — which lights a pixel iff its CENTRE is
// inside the outline — the counter never contains a centre and closes; the
// glyph becomes a bowl with a notch, which is an a. At 10 px the x-height is
// 4.9 and rounds to the 5 the reference face has, and every e opens. 11 is
// worse again: m's two gaps close and it prints as a solid block.
//
// Note for anyone who reaches for the threshold below first — it was the
// obvious knob and it is INERT at these settings. kAlias hands back a mask
// that is already 0 or 255, so `>= 110` reclassifies nothing: 110 and 170
// render pixel-for-pixel identical (verified, 0 differing pixels over the whole
// 1280x800 frame). It only becomes a real control if `aliased` goes false, and
// the antialiased-then-threshold path fixes e at every threshold from 110 to
// 205 while ruining m, which is a worse trade than the one it solves.

struct PixText {
  sk_sp<SkImage> mask; // A8, one pixel per GUI px
  int w = 0, h = 0;
};

/** The substitute face's size, in GUI px. See the note above: 9 kills e. */
inline constexpr float kPixSizePx = 10.0f;

inline PixText bakeText(const std::string &s, weave::FontContext &fonts,
                        const sk_sp<SkTypeface> &face, float sizePx) {
  weave::TextStyle st;
  st.shaping.typeface = face;
  st.shaping.fontSize = sizePx;
  st.shaping.aliased = true;
  st.paint.foreground.setColor(SK_ColorWHITE);
  const std::u8string u8(reinterpret_cast<const char8_t *>(s.c_str()));
  Element tree = box().child(text(u8, st));
  const SkSize sz = measure(box().child(text(u8, st)), fonts);
  // +8, NOT +2. measure() gives the ADVANCE width, and the last glyph's ink
  // can sit outside its own advance — so the raster surface was ending inside
  // the final letter and the mask got cropped to what survived. The tooltip's
  // longest line printed "Centrifug" plus a two-pixel stub where its e should
  // be, at every font size tried, which is what gave it away: a rasterisation
  // fault moves with the size, a surface that is too small does not. The mask
  // is cropped to its lit bbox two dozen lines below, so slack here is free.
  const int w = std::max(1, (int)std::ceil(sz.width()) + 8);
  const int h = std::max(1, (int)std::ceil(sz.height()) + 4);
  sk_sp<SkSurface> surf =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surf)
    return {};
  surf->getCanvas()->clear(SK_ColorTRANSPARENT);
  if (sk_sp<SkPicture> pic = snapshot(std::move(tree), fonts))
    surf->getCanvas()->drawPicture(pic);
  SkBitmap read;
  read.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!surf->readPixels(read.pixmap(), 0, 0))
    return {};
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeA8(w, h));
  out.eraseColor(SK_ColorTRANSPARENT);
  int x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const bool lit = SkColorGetA(read.getColor(x, y)) >= 110; // 1-bit
      *out.getAddr8(x, y) = lit ? 255 : 0;
      if (lit) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
      }
    }
  if (x1 < 0)
    return {};
  SkBitmap crop;
  out.extractSubset(&crop, SkIRect::MakeLTRB(x0, y0, x1 + 1, y1 + 1));
  SkBitmap owned;
  owned.allocPixels(SkImageInfo::MakeA8(x1 - x0 + 1, y1 - y0 + 1));
  crop.readPixels(owned.pixmap(), 0, 0);
  owned.setImmutable();
  return {owned.asImage(), x1 - x0 + 1, y1 - y0 + 1};
}

/** Draw a baked mask at 2x, nearest, with the +1 GUI px 25% shadow. */
inline void blitText(SkCanvas &c, const PixText &t, float x, float y,
                     SkColor4f col, bool shadow = true) {
  if (!t.mask)
    return;
  const SkRect dst = SkRect::MakeXYWH(g(x), g(y), g((float)t.w), g((float)t.h));
  const SkSamplingOptions near(SkFilterMode::kNearest);
  SkPaint p;
  p.setAntiAlias(false);
  if (shadow) {
    p.setColor4f({col.fR * 0.25f, col.fG * 0.25f, col.fB * 0.25f, col.fA},
                 nullptr);
    c.drawImageRect(t.mask, dst.makeOffset(g(1), g(1)), near, &p);
  }
  p.setColor4f(col, nullptr);
  c.drawImageRect(t.mask, dst, near, &p);
}

} // namespace thaum

using namespace thaum;

// ===========================================================================

struct Thaumonomicon : sigil::compose::sketch::Sketch {
  // ---- motion --------------------------------------------------------------
  ch::Output<float> pulse{0};   // the 600 ms lockstep unlockable pulse
  ch::Output<float> spin{0};    // drawForbidden's swirl
  ch::Output<float> driftX{0};  // locX, in GUI px
  ch::Output<float> driftY{0};

  // ---- baked art (pointer-stable, so every brush bakes exactly once) -------
  std::array<Element, 3> spatter;
  std::array<Element, 3> knot;
  std::array<Element, 3> straight;
  // [tier][2*big + (handed>0)] — small/big elbow, left/right mirror
  std::array<std::array<Element, 4>, 3> elbows;
  Element runTile, cornerTile;

  // ---- baked type ----------------------------------------------------------
  sk_sp<SkTypeface> face;
  PixText tipTitle, tipMissing, tipParent;

  static sk_sp<SkTypeface> systemFace() {
    sk_sp<SkFontMgr> mgr = weave::ports::systemFontManager();
    if (!mgr)
      return nullptr;
    for (const char *name : {"Menlo", "Monaco", "Courier New", "Helvetica"})
      if (sk_sp<SkTypeface> f = mgr->matchFamilyStyle(name, SkFontStyle::Normal()))
        return f;
    return mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  }

  // -------------------------------------------------------------------------
  // The backdrop: two planes, translated at the source's 2.0 : 1.5 ratio. Both
  // are STATIC generated art baked with Cache::Texture — the parallax is a
  // bound transform, which is paint-only volatility, so the bakes replay under
  // it and no shader re-runs per frame.

  static Element backdropBase() {
    const float w = g(kScreenX + 4 + 44), h = g(kScreenY + 4 + 44);
    Element e = box()
                    .left(g(-22))
                    .top(g(-22))
                    .width(w)
                    .height(h)
                    .fill(Material::radialUnit({0.44f, 0.38f}, 1.20f,
                                               {{0.0f, rgb(0x3E3220)},
                                                {0.40f, rgb(0x241B10)},
                                                {0.78f, rgb(0x140E07)},
                                                {1.0f, rgb(0x070402)}}));
    // The painted plate under it: an alchemical wheel, a ruled margin, and
    // washes — the structure a photographed grimoire page carries and a noise
    // field never will.
    e.overlay(prog([](SkCanvas &c, const PaintContext &in) {
      SkPaint p;
      p.setAntiAlias(true);
      const SkPoint o{in.size.width() * 0.50f, in.size.height() * 0.47f};
      // washes first, so the linework sits on top of them
      for (int i = 0; i < 30; ++i) {
        const float x = (0.5f + 0.5f * noise1(i, 1, 5)) * in.size.width();
        const float y = (0.5f + 0.5f * noise1(i, 2, 5)) * in.size.height();
        const float r = g(22.0f + 58.0f * (0.5f + 0.5f * noise1(i, 3, 5)));
        p.setColor4f(rgb(0x060402, 0.16f), nullptr);
        c.drawCircle(x, y, r, p);
      }
      p.setStyle(SkPaint::kStroke_Style);
      // the wheel: four rules and three rings, plus a 72-tick limb
      for (int i = 0; i < 6; ++i) {
        p.setStrokeWidth(g(i % 3 == 0 ? 2.0f : 0.9f));
        p.setColor4f(mul(kBrassLit, 1.0f, 0.13f + 0.05f * (float)(i % 3)),
                     nullptr);
        c.drawCircle(o.fX, o.fY, g(46.0f + (float)i * 30.0f), p);
      }
      p.setStrokeWidth(g(1.0f));
      p.setColor4f(mul(kBrassLit, 1.0f, 0.17f), nullptr);
      SkPathBuilder t;
      for (int i = 0; i < 72; ++i) {
        const float aRad = (float)i * 5.0f * 0.0174533f;
        const float r0 = g(i % 6 == 0 ? 182.0f : 192.0f), r1 = g(200.0f);
        t.moveTo(o.fX + std::cos(aRad) * r0, o.fY + std::sin(aRad) * r0);
        t.lineTo(o.fX + std::cos(aRad) * r1, o.fY + std::sin(aRad) * r1);
      }
      // an inscribed pentagram and a hexagram, the plate's own furniture
      for (int k = 0; k < 2; ++k) {
        const int n = k ? 6 : 5;
        const float rad = g(k ? 106.0f : 166.0f);
        const float rot = k ? 0.26f : -1.57f;
        for (int i = 0; i < n; ++i) {
          const int j = (i + (k ? 2 : 2)) % n;
          const float a1 = rot + (float)i * 6.2831853f / (float)n;
          const float a2 = rot + (float)j * 6.2831853f / (float)n;
          t.moveTo(o.fX + std::cos(a1) * rad, o.fY + std::sin(a1) * rad);
          t.lineTo(o.fX + std::cos(a2) * rad, o.fY + std::sin(a2) * rad);
        }
      }
      c.drawPath(t.detach(), p);
      // strata: long diagonal scrapes across the plate
      p.setColor4f(rgb(0xD8C08A, 0.055f), nullptr);
      SkPathBuilder s2;
      for (int i = 0; i < 22; ++i) {
        const float y0 = (0.5f + 0.5f * noise1(i, 9, 4)) * in.size.height();
        p.setStrokeWidth(g(0.6f + 1.8f * (0.5f + 0.5f * noise1(i, 8, 4))));
        s2.moveTo(-g(20), y0);
        s2.lineTo(in.size.width() + g(20), y0 + g(46.0f * noise1(i, 7, 4)));
      }
      c.drawPath(s2.detach(), p);
    }));
    // Paper tooth: the luminance grain, held back so it shades instead of
    // shouting. Inside the Cache::Texture bake, so its pixels are paid once.
    e.child(box()
                .inset(0)
                .opacity(0.42f)
                .blend(SkBlendMode::kOverlay)
                .fill(patterns::grain(0.06f, 5, 3.0f)));
    return e;
  }

  static Element backdropOver() {
    const float w = g(kScreenX + 4 + 44), h = g(kScreenY + 4 + 44);
    return box()
        .left(g(-22))
        .top(g(-22))
        .width(w)
        .height(h)
        .blend(SkBlendMode::kScreen)
        .opacity(0.34f)
        .fill(Material::blend(
            {{patterns::grain(0.0075f, 4, 11.0f), SkBlendMode::kSrc},
             {Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                   {{0.0f, rgb(0xFFFFFF)},
                                    {0.7f, rgb(0x808080)},
                                    {1.0f, rgb(0x000000)}}),
              SkBlendMode::kMultiply}}));
  }

  // -------------------------------------------------------------------------
  // One edge: a connector on the transcribed router, dressed with ONE stock
  // PatternBrush — 24x24 side tiles, a 24x24 or 48x48 corner tile, and
  // cornerLength reserving the elbow's own room so the side run butts against
  // it instead of continuing underneath. Nothing here is a stroke.

  Element edgeEl(const Edge &e, int order) const {
    const Node &child = nodeByKey(e.child);
    const Node &parent = nodeByKey(e.parent);
    const RouteShape shape = shapeOf(child, parent, e.flipped);
    const int t = (int)e.tier;
    const int elbow =
        !shape.hasCorner ? -1
                         : (shape.bigCorner ? 2 : 0) + (shape.handed > 0 ? 1 : 0);

    brushes::PatternBrush pb{.side = straight[t],
                             .advance = kCell,
                             .cornerAngleDeg = 35.0f,
                             .stretchToFit = true,
                             .reach = g(56)};
    if (elbow >= 0) {
      pb.corner = elbows[t][(size_t)elbow];
      pb.cornerLength = 2.0f * cornerArm(shape.bigCorner);
      // An elbow of PIPE, not an ornament: entry, exit and a handedness, and
      // elbowTile() authors it with local +x along the outgoing leg. The
      // brush's default is the bisector, under which this art stamps 45
      // degrees off — and a 2x2 route is all corner and no side tiles, so the
      // whole edge becomes a chevron. Ask for the frame the art is drawn in.
      pb.cornerAlign = brushes::PatternBrush::CornerAlign::Outgoing;
    }
    Brush br;
    br.leg(std::move(pb));

    return connector(child.key, parent.key, thaumRoute(e.flipped))
        .inset(0)
        .key(std::string("edge:") + child.key + "<" + parent.key)
        .zIndex(tierZ(e.tier))
        .stroke(br)
        .trim(0.0f, withFrom(0.0f, 1.0f,
                             Transition{.duration = 620ms,
                                        .ease = ch::easeOutQuad,
                                        .delay = std::chrono::milliseconds(
                                            60 * order)}));
  }

  Element arrowEl(const Edge &e) const {
    const Node &child = nodeByKey(e.child);
    const Node &parent = nodeByKey(e.parent);
    const int dy = child.row - parent.row, dx = child.col - parent.col;
    SkVector travel{0, 0};
    if (dy != 0)
      travel = {0, dy > 0 ? 1.0f : -1.0f};
    else if (dx != 0)
      travel = {dx > 0 ? 1.0f : -1.0f, 0};
    else
      return box().width(0).height(0);
    const SkPoint c = centreOf(child.col, child.row);
    const SkColor4f tint = tierTint(e.tier, kInkBody);
    return arrowCell(tint)
        .centerAt({c.fX - travel.fX * g(20), c.fY - travel.fY * g(20)})
        .rotate(std::atan2(travel.fY, travel.fX) * 57.29578f)
        .zIndex(tierZ(e.tier));
  }

  // -------------------------------------------------------------------------

  Element nodePlate(const Node &n) const {
    const SkPoint c = centreOf(n.col, n.row);
    // Cache::Texture, not the automatic picture. A plate is a torn-square
    // outline + a radial + an Sk2D hatch + a sketchy double rule, and a
    // PICTURE replays the path effects — 22 of them measured 10.8 ms of a
    // 20 ms frame. Baked to a 64x64 texture they are 22 blits, and the
    // pulsing ones keep the bake because a bound opacity is paint-only.
    Element wrap = box()
                       .left(c.fX - g(16))
                       .top(c.fY - g(16))
                       .width(g(32))
                       .height(g(32))
                       .key(n.key);
    // :598 culls the whole node, art and all, but drawLine still draws its
    // edges — so a culled node keeps its KEYED, empty box (the connector
    // still needs somewhere to route to) and paints nothing.
    if (culled(n.col, n.row))
      return wrap;
    wrap.cache(Cache::Texture);
    if (n.state == kUnlockable)
      wrap.opacity(bind(&pulse).to(0.5f, 1.0f));
    else if (n.state == kLocked)
      wrap.opacity(0.3f);

    const uint32_t seed = (uint32_t)(n.col * 31 + n.row * 17 + 101);
    wrap.child(plateArt(n.meta, seed, spatter[0]).inset(g(2)));
    if (n.meta & kSpiky)
      wrap.child(spikyOverlay(seed + 7));
    wrap.child(iconEl(n.icon, n.state == kLocked ? 0.6f : 1.0f,
                      n.state == kLocked)
                   .left(g(8))
                   .top(g(8)));
    return wrap;
  }

  Element nodeBadges(const Node &n) const {
    const SkPoint c = centreOf(n.col, n.row);
    Element g0 = box().inset(0);
    if (culled(n.col, n.row))
      return g0;
    if (n.flagResearch)
      g0.child(researchBadge().centerAt({c.fX - g(9) + g(8), c.fY - g(9) + g(8)}));
    if (n.flagPage)
      g0.child(pageBadge().centerAt({c.fX - g(9) + g(6), c.fY + g(9) + g(7)}));
    return g0;
  }

  // -------------------------------------------------------------------------
  // The frame: the stock PatternBrush corner path (closed rect, four breaks).

  Element frameBand() const {
    const float in = g(9);
    return box()
        .inset(0)
        .outline([in](SkSize s) {
          SkPathBuilder p;
          p.addRect(SkRect::MakeLTRB(in, in, s.width() - in, s.height() - in));
          return p.detach();
        })
        .stroke(brushes::PatternBrush{.side = runTile,
                                      .corner = cornerTile,
                                      .advance = g(64),
                                      .cornerAngleDeg = 35.0f,
                                      .cornerLength = g(20),
                                      .stretchToFit = true,
                                      .reach = g(26)});
  }

  /** The inner rule: four OPEN contours that STOP SHORT of the corners — a
   *  doubled rule whose inner line is dotted. Not a rounded rect anywhere. */
  static Element innerRule() {
    const float m = g(22), cut = g(26);
    Element e = box().inset(0).outline([m, cut](SkSize s) {
      const float l = m, t = m, r = s.width() - m, b = s.height() - m;
      SkPathBuilder p;
      p.moveTo(l + cut, t); p.lineTo(r - cut, t);
      p.moveTo(r, t + cut); p.lineTo(r, b - cut);
      p.moveTo(r - cut, b); p.lineTo(l + cut, b);
      p.moveTo(l, b - cut); p.lineTo(l, t + cut);
      return p.detach();
    });
    Brush br;
    lines::Line outer;
    outer.width = g(1.2f);
    outer.fill = Fill::color(mul(kBrassDark, 1.0f, 0.85f));
    br.leg(outer);
    lines::Line dotted;
    dotted.width = g(0.8f);
    dotted.fill = Fill::color(mul(kBrassLit, 0.85f, 0.65f));
    dotted.dashIntervals = {g(1.2f), g(3.0f)};
    br.leg(dotted, {ops::Offset{.px = g(2.5f), .step = g(3)}});
    e.stroke(br);
    return e;
  }

  // -------------------------------------------------------------------------
  // The tab rail (:219-233, :1089-1103). Left column at x = 1, y = 10 + i*24;
  //
  // There is no right-hand rail here, and that is the source's answer rather
  // than an omission. updateResearch() splits categories with
  // `for (String tcc : ConfigResearch.TCCategories) if (tcc.equals(rcl))` —
  // anything in that array is added to categoriesTC and buttoned at x = 1,
  // everything else lands in categoriesOther at x = width-17. And
  // ConfigResearch:334 sets TCCategories to all seven vanilla categories, so
  // the right column and its two GuiScrollButtons only ever appear once an
  // ADDON registers one. Naming addon categories would be inventing text, so
  // the right margin here carries what the mod actually puts there at this
  // scroll: the web running off the edge under the frame band.
  // the 22x22 plate at (x-3, y-3) is UV(13,13) — the SAME cell as the frame's
  // four corners, which is why one art element serves both here.

  struct Category { const char *name; uint32_t aspect; int rune; };

  Element tabRail() const {
    static const Category kCats[7] = {
        {"Fundamentals", aspect::kHerba, 0},
        {"Auromancy", aspect::kAuram, 1},
        {"Alchemy", aspect::kAlkimia, 2},
        {"Artifice", aspect::kMachina, 3},
        {"Arcane Infusion", aspect::kPraecantatio, 4},
        {"Golemancy", aspect::kHumanus, 5},
        {"Eldritch", aspect::kAlienis, 6},
    };
    Element rail = box().inset(0);
    for (int i = 0; i < 7; ++i) {
      const float y = 10.0f + (float)(i + 1) * 24.0f;
      const bool selected = i == 2; // ALCHEMY
      rail.child(cornerPlate(selected ? SkColor4f{0.6f, 1.0f, 1.0f, 1}
                                      : SkColor4f{1, 1, 1, 1})
                     .left(g(-2 - 1))
                     .top(g(y - 3 - 1))
                     .opacity(selected ? 1.0f : 0.86f));
      const Category &cat = kCats[i];
      rail.child(box()
                     .left(g(1))
                     .top(g(y))
                     .width(g(16))
                     .height(g(16))
                     .opacity(selected ? 1.0f : 0.8f)
                     .background(prog([cat, selected](SkCanvas &c,
                                                      const PaintContext &) {
                       const Ink k{c};
                       const SkColor4f col =
                           rgb(cat.aspect, selected ? 1.0f : 0.66f);
                       // seven distinct runes, one per category
                       switch (cat.rune) {
                       case 0: k.r(7, 2, 2, 12, col); k.r(3, 6, 10, 2, col); break;
                       case 1: k.r(3, 4, 10, 2, col); k.r(3, 10, 10, 2, col);
                               k.r(7, 4, 2, 8, col); break;
                       case 2: k.r(4, 3, 8, 2, col); k.r(4, 3, 2, 10, col);
                               k.r(10, 3, 2, 10, col); k.r(4, 11, 8, 2, col);
                               k.r(7, 7, 2, 2, col); break;
                       case 3: k.r(3, 7, 10, 2, col); k.r(6, 3, 2, 10, col);
                               k.r(10, 3, 2, 10, col); break;
                       case 4: k.r(7, 2, 2, 12, col); k.r(3, 5, 10, 2, col);
                               k.r(5, 11, 6, 2, col); break;
                       case 5: k.r(6, 2, 4, 4, col); k.r(4, 7, 8, 4, col);
                               k.r(5, 11, 2, 3, col); k.r(9, 11, 2, 3, col); break;
                       default: k.r(3, 3, 2, 10, col); k.r(11, 3, 2, 10, col);
                                k.r(5, 7, 6, 2, col); k.r(7, 3, 2, 4, col); break;
                       }
                     })));
    }
    // the search button (:170, UV 160,16 at x=1, y=height-17), 0.8 grey
    rail.child(box()
                   .left(g(1))
                   .top(g(kGuiH - 17))
                   .width(g(16))
                   .height(g(16))
                   .opacity(0.8f)
                   .background(prog([](SkCanvas &c, const PaintContext &) {
                     SkPaint p;
                     p.setAntiAlias(true);
                     p.setStyle(SkPaint::kStroke_Style);
                     p.setStrokeWidth(g(1.6f));
                     p.setColor4f(kBrassLit, nullptr);
                     c.drawCircle(g(6.5f), g(6.5f), g(4.2f), p);
                     c.drawLine(g(9.5f), g(9.5f), g(13.5f), g(13.5f), p);
                   })));
    return rail;
  }

  // -------------------------------------------------------------------------
  // The tooltip. UtilsFX.drawCustomTooltip at (mx+3, my-3) for the hovered
  // node (:761-796): a gold title, then the missing-research block in red with
  // one yellow line per unknown parent. THAUMATORIUM is hovered; its parents
  // are CENTRIFUGE (not complete) and ESSENTIASMELTERTHAUMIUM (complete), so
  // exactly one line follows. It overlaps two edges and bleeds past the inner
  // rule, which is what a tooltip at the right-hand end of the tree does.

  Element tooltip() const {
    const PixText a = tipTitle, b = tipMissing, d = tipParent;
    const float wd = (float)std::max({a.w, b.w, d.w}) + 1.0f;
    const float ht = 3.0f * 10.0f;
    // THAUMATORIUM's hover box is 20x20 at (iconX-2, iconY-2); the cursor
    // sits inside it, and drawCustomTooltip anchors at (mx+3, my-3). At this
    // column that would run off the right edge, so it FLIPS to the cursor's
    // left — the vanilla clamp, and the reason the card ends up lying across
    // three edges and two neighbours instead of hanging in clear space.
    const SkPoint hc = centreOf(10, -2);
    const float mx = hc.fX / U + 2, my = hc.fY / U + 4;
    const float x = (mx + 3 + wd + 4 <= kGuiW) ? mx + 3 : mx - 3 - wd;
    const float y = my - 3;
    return box().inset(0).background(
        prog([a, b, d, x, y, wd, ht](SkCanvas &c, const PaintContext &) {
          SkPaint p;
          p.setAntiAlias(false);
          // Vanilla GuiScreen.drawHoveringText fills k1-3 .. k1+j1+3 under a
          // k1-4 .. k1-3 top strip, i.e. THREE px of sill below the last
          // line — not one. At +1 the descender of "Centrifuge" landed on
          // the inner border and its 1 px shadow crossed it.
          const SkRect r = SkRect::MakeLTRB(g(x - 4), g(y - 4), g(x + wd + 4),
                                            g(y + ht + 3));
          p.setColor4f(rgb(0x100010, 0.94f), nullptr);
          c.drawRect(r, p);
          // the vanilla two-tone inner border
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeWidth(g(1));
          p.setColor4f(rgb(0x5000FF, 0.31f), nullptr);
          c.drawRect(r.makeInset(g(1), g(1)), p);
          p.setColor4f(rgb(0x28007F, 0.31f), nullptr);
          c.drawRect(r.makeInset(g(2), g(2)), p);
          blitText(c, a, x, y, kTextGold);
          blitText(c, b, x, y + 10, kTextRed);
          blitText(c, d, x, y + 20, kTextYellow);
        }));
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(rgb(0x0B0906));

    face = systemFace();
    if (ctx.fonts) {
      // 10 px, NOT 9. See kPixSizePx below — 9 collapsed every lowercase e.
      tipTitle = bakeText("Alchemical Automation", *ctx.fonts, face, kPixSizePx);
      tipMissing =
          bakeText("Missing required research:", *ctx.fonts, face, kPixSizePx);
      tipParent = bakeText(" - Essentia Centrifuge", *ctx.fonts, face, kPixSizePx);
    }

    for (int t = 0; t < 3; ++t) {
      const SkColor4f tint = tierTint((EdgeTier)t, kInkBody);
      spatter[t] = spatterCell(tint);
      knot[t] = knotCell(tint);
      straight[t] = straightTile(tint, spatter[t], knot[t]);
      for (int q = 0; q < 4; ++q) {
        const bool big = q >= 2;
        const float handed = (q & 1) ? 1.0f : -1.0f;
        elbows[t][(size_t)q] =
            elbowTile(cornerArm(big), handed, tint, spatter[t], knot[t]);
      }
    }
    runTile = frameRun();
    cornerTile = cornerPlate({1, 1, 1, 1});

    // ---- motion ----------------------------------------------------------
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // :610 — sin(systemTime % 600 / 600 * 2pi) * 0.25 + 0.75, wall clock,
      // so every unlockable node is in lockstep with zero phase offset.
      pulse = (float)(std::sin(std::fmod(t, 0.6) / 0.6 * 6.2831853) * 0.25 +
                      0.75);
      spin = (float)std::fmod(t * 0.06, 1.0);
      driftX = (float)(std::sin(t * 0.17) * 26.0);
      driftY = (float)(std::cos(t * 0.11) * 16.0);
      return true;
    });

    // ---- the tree --------------------------------------------------------
    Element root = box().inset(0);

    // 0. drawDefaultBackground (the world under the GUI, then the vanilla
    //    0xC0101010 -> 0xD0101010 wash) is NOT drawn, and the reason is
    //    measured rather than assumed. At this resolution the painted
    //    backdrop plate covers GUI 14..413 x 14..253 opaquely and the frame
    //    band covers 0..20 and 407..427 / 247..267 — their union is the
    //    whole canvas, so every pixel of drawDefaultBackground is occluded.
    //    As a node it cost 3.91 ms of a 15.5 ms frame while contributing
    //    nothing: a full-canvas gradient is a SHADER, and a cached picture
    //    replays the draw call, not the pixels. The clear colour stands in
    //    for it wherever the frame's corner art leaves a hairline.

    // 1. the parallax pair, clipped to (startX-2, startY-2, screenX+4,
    //    screenY+4) and moved at the source's 2.0 : 1.5 divisors.
    Element plate = box()
                        .left(g(kStartX - 2))
                        .top(g(kStartY - 2))
                        .width(g(kScreenX + 4))
                        .height(g(kScreenY + 4))
                        .clip();
    plate.child(backdropBase()
                    .cache(Cache::Texture)
                    .translateX(bind(&driftX).scale(-U / 2.0f))
                    .translateY(bind(&driftY).scale(-U / 2.0f)));
    plate.child(backdropOver()
                    .cache(Cache::Texture)
                    .translateX(bind(&driftX).scale(-U / 1.5f))
                    .translateY(bind(&driftY).scale(-U / 1.5f)));
    root.child(std::move(plate));

    // 2. the web. NOT clipped — the mod culls whole nodes at the viewport
    //    edge (:598) and lets everything else run out under the frame band,
    //    which is drawn last and covers the margin. That is why the tree
    //    reads as a viewport onto something larger.
    Element inner = box().inset(0);

    // 2a. warp swirls go under everything (drawForbidden, :603).
    for (const Node &n : kNodes)
      if (n.warp > 0 && !culled(n.col, n.row)) {
        const SkPoint c = centreOf(n.col, n.row);
        inner.child(warpSwirl(&spin, n.warp).centerAt(c).zIndex(-1));
      }

    // 2b. the edges, in unlock order — staggerChildren cascades the
    //     withFrom() trim entrance outward from BASEALCHEMY.
    Element edges = box().inset(0).zIndex(0);
    std::vector<int> order = edgeOrder();
    int k = 0;
    for (int i : order)
      edges.child(edgeEl(kEdges[i], k++));
    for (int i : order)
      if (kEdges[i].tier != kSiblingKnown)
        edges.child(arrowEl(kEdges[i]));
    inner.child(std::move(edges));

    // 2c. the plates, then the badges at full brightness over them.
    Element plates = box().inset(0).zIndex(4);
    for (const Node &n : kNodes)
      plates.child(nodePlate(n));
    for (const Node &n : kNodes)
      if (n.flagResearch || n.flagPage)
        plates.child(nodeBadges(n));
    inner.child(std::move(plates));

    root.child(std::move(inner));

    // 3. the frame, drawn last (genResearchBackgroundFixedPost).
    root.child(innerRule());
    root.child(frameBand());
    root.child(tabRail());

    // 4. the hover tooltip, over everything including the frame.
    root.child(tooltip());

    ctx.composer.render(std::move(root));
  }

  /** BFS depth from BASEALCHEMY, so the draw-on cascade runs outward the way
   *  the research actually unlocks. */
  static std::vector<int> edgeOrder() {
    std::array<int, kNodeCount> depth{};
    depth.fill(99);
    depth[(size_t)indexByKey("BASEALCHEMY")] = 0;
    for (int pass = 0; pass < 8; ++pass)
      for (const Edge &e : kEdges) {
        const int ci = indexByKey(e.child), pi = indexByKey(e.parent);
        depth[(size_t)ci] = std::min(depth[(size_t)ci], depth[(size_t)pi] + 1);
      }
    std::vector<int> idx;
    for (int i = 0; i < kEdgeCount; ++i)
      idx.push_back(i);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
      return depth[(size_t)indexByKey(kEdges[a].child)] <
             depth[(size_t)indexByKey(kEdges[b].child)];
    });
    return idx;
  }
};

SIGIL_SKETCH(Thaumonomicon)
