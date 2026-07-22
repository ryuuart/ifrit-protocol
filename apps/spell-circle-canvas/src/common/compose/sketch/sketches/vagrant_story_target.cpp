// vagrant_story_target.cpp — VAGRANT STORY, THE BATTLE-MODE TARGETING OVERLAY
// =============================================================================
// Vagrant Story (Square Product Development Division 4 / Matsuno, 2000),
// PlayStation. The screen: battle mode, the instant the attack button is
// tapped — world time freezes, a wireframe REACH SPHERE blooms around Ashley,
// and every body part of the enemy inside it becomes selectable, each with its
// own HP, its own armour and its own hit percentage. Under it sit the HP / MP /
// RISK gauges; over it, the chain-ability timing ring.
//
// Canvas 1280 x 960 = 4x the 320 x 240 polygon-coordinate space. VS displays at
// a 512 x 240 hi-res framebuffer but computes polygon coordinates in 320 x 240
// regardless, so it draws 3D geometry on a 320-wide grid and UI text on a
// 512-wide one IN THE SAME FRAME. Practical rule, and it is honoured
// throughout: geometry snaps to multiples of 4 canvas px (snapG); text
// x-advances snap to 2.5 and y to 4 (snapX / snapY). That non-square text cell
// is why VS's font reads sharper than any other PS1 RPG.
//
// -----------------------------------------------------------------------------
// SOURCE — fetched and read, not remembered
//
//   github.com/ser-pounce/rood-reverse — a byte-matching VS decompilation.
//   Files pulled and quoted below: src/BATTLE/BATTLE.PRG/146C.{h,c} and
//   assets/MENU/MENU4.PRG/status.yaml.
//
// 1. THE SIX-LIMB MODEL IS LITERALLY A STRUCT. 146C.h:410-420
//
//      typedef struct {
//          short hp;  short maxHp;
//          signed char agilityDefenseBonus;
//          char unk5;  u_char nameIndex;  char chainEvasion;
//          short types[4];       // blunt / edged / piercing / +1
//          short affinities[8];  // air fire earth water light dark +2
//          vs_battle_uiArmor armor;                     // 0x18
//      } vs_battle_uiEquipment_limb;
//
//    and 146C.h:551 (inside vs_battle_actor2, at offset 0x388)
//      vs_battle_uiEquipment_limb limbs[6];
//    with 146C.c:315 carrying `short limbHp[6]` in the save-side struct.
//    SIX parts, each with its own HP, its own armour piece, four damage-type
//    resistances, eight affinities and a one-byte chain evasion.
//
// 2. THE HIT PERCENTAGE IS _getAgilityDifference, 146C.c:7110-7160, transcribed
//    verbatim into accuracy() below:
//
//      src = AGI + accessory.AGI + SUM(limbs[0..5].armor.AGI)
//      tgt = same, for the target
//      if (weapon drawn) src += weapon.AGI + shield.AGI      // and likewise tgt
//      src = src * (100 - risk_src) / 100                     // integer divide
//      tgt = (tgt + limbs[part].agilityDefenseBonus) * (100 - risk_tgt) / 100
//      agiDiff = src - tgt + 100
//      if (variance) agiDiff += getRandSmoothed(11) - 5
//      agiDiff += attackGem - defenseGem
//      clamp: < 0 -> 0 ; == 255 -> 254
//
//    THEN _doesAttackHit (146C.c:7294) clamps to 100 — and at :7334, AFTER the
//    clamp:
//
//        if ((source->unk40 == 0) && (source->actorId == 0)) {
//            threshold += 10;
//        }
//        return vs_main_getRand(100) < threshold;
//
//    Ashley is actor 0. HE SILENTLY GETS +10% TO HIT THAT THE PRINTED NUMBER
//    DOES NOT INCLUDE, and because the +10 lands after the clamp, a printed
//    100% is a true 110% — rand(100) < 110 can never fail. Both numbers are on
//    the render, and the divergence is printed, the way xcom_battlescape.cpp
//    printed the picker cheat.
//
// 3. RISK. _getRiskModifier (146C.c:7265):
//        rate = ((risk + 150) * 100) / 256;  if (rate == 255) rate = 254;
//    so RISK 0 already reads 58, and the rate hits 100 at risk = 106. RISK also
//    multiplies BOTH sides' agility by (100 - risk)/100 in the accuracy
//    formula, so raising your own RISK costs you exactly risk% of your
//    accuracy. _getChainEvasionModifier (146C.c:7276):
//        rate = (255 - limbs[part].chainEvasion) * 100 / 255;
//    — per-limb chain evasion, a byte. Both are on the render.
//
// 4. THE SPHERE AND THE WEDGE ARE BIT FIELDS. 146C.h:540-546:
//        u_int enemyClass : 3;  u_int reach : 5;  u_int currentRange : 8;
//        u_int unk39 : 8; u_int unk3A : 8; u_int unk3B_0 : 3;
//        u_int currentAttackShapeAngle : 5;
//    reach is a 5-bit radius (0-31) and the attack shape is a 5-bit angle:
//    32 divisions, 11.25 deg per step. That number IS the tick ladder under the
//    sphere. enemyClass : 3 is the six classes confirmed by MENU9's
//    statHeaders.yaml: human, beast, undead, phantom, dragon, evil.
//
// 5. TRUTHFUL TEXT. assets/MENU/MENU4.PRG/status.yaml gives hp / mp / risk, the
//    condition tiers critical / damaged / wounded / good / excellent, and the
//    ten defence rows physical, air, fire, earth, water, light, dark, blunt,
//    edged, piercing — all ten are in the right-hand ladder, in file order.
//
// DOCUMENTED: the six-limb struct, the accuracy formula and its clamps, the
//   +10 cheat and where it lands relative to the clamp, the RISK algebra, the
//   chain-evasion byte, the reach / attack-angle bit widths, the label
//   vocabulary and its order.
// RECONSTRUCTED: the exact HUD widget rectangles, the sphere's wire count, the
//   palette, the font, the humanoid part NAMING (the struct is limbs[6] with a
//   nameIndex; HEAD/BODY/R.ARM/L.ARM/R.LEG/L.LEG is the conventional
//   reconstruction), and every stat value. The decomp has not matched the
//   render code yet, so no pixel here comes from it — only the arithmetic.
//
// -----------------------------------------------------------------------------
// LINE VOCABULARY — the engineer's version of the screen
//
// VS's own rules are plain 1 px hairlines and teach nothing about stroke, so
// this is the drawing a draughtsman would have made had the PS1 let him:
//
//  * HIDDEN-LINE CONVENTION ON THE SPHERE. Every meridian and latitude ring is
//    ONE closed curve carrying TWO PathFormats: a solid one over the near arc
//    and a fine dotted one over the far arc, split at the TANGENCY by
//    PathFormat::trimStart / trimEnd — not by a clip, and not by two elements.
//    The split fractions are computed in arc length (see wire()), because the
//    projected ellipse's arc length is not linear in the parameter.
//  * DEPTH BY WEIGHT, NOT ALPHA. Near arcs run 1.4-2.6 px scaled by how
//    front-facing the circle is; far arcs are 1.0 px dotted; the silhouette is
//    lines::cased — two rails, the outer heavy.
//  * LEADER LINES ARE brushes::PatternBrush WITH START / SIDE / END TILES: a
//    small open ring as the START tile sitting on the limb, a hairline dash as
//    the SIDE tile, and a right-angle tick as the END tile the label sits on.
//    That is the drafting leader exactly.
//  * THE TICK LADDER IS THE 5-BIT FIELD: 32 divisions at 11.25 deg, two
//    lines::radialHatch passes (32 short + 4 long) on a foreshortened annulus.
//  * THE RISK GAUGE ENCODES VALUE AS HATCH DENSITY, not as a fill — spacing
//    tightens 7.0 -> 1.6 px as RISK climbs — inside a lines::dottedCore frame
//    (solid casing, dotted core).
//  * Bracket-only card frames: decorations::brackets and decorations::gappedRule
//    on chamfered outlines. No rounded rect anywhere on this canvas.
//  * ops::sketchy is deliberately absent. This is an instrument, not a page.
//    ops::Wave appears on exactly one thing: the damage-number pop.
//
// -----------------------------------------------------------------------------
// THE COMPOSITION — why there is no column on this canvas
//
// The first cut of this study put the six limb readouts in a right-hand margin
// and drew leaders back to the body. That is a stat sheet with a sphere behind
// it, and the giveaway is that the leaders were decorative: a column needs no
// leader, because a column IS the answer to "where does this readout go".
//
// So the placement was inverted. layoutCards() takes each limb's BEARING about
// the torso from the projection itself, re-spaces the six bearings into one
// 204-degree fan opening away from the title and gauge furniture, and hangs the
// panel at a radius no neighbour shares (268 / 392 / 300 / 430 / 320 / 300 px).
// HEAD's panel is above the skull, the arms' are out to either side, the legs'
// are below, and the selected R.ARM sits CLOSEST to its own limb. Re-spacing
// preserves order, which is what guarantees six leaders out of one cluster
// cannot cross — the property the brief asked for, obtained from the geometry
// rather than by hand-placing boxes until nothing overlapped.
//
// The figure is limbs[6] made visible: six drawables, one per struct entry,
// each on its limb's projected anchor and joined by drawn bones. The marker
// ring, the leader and the readout then all key off a shape you can point at.
//
// -----------------------------------------------------------------------------
// THE GEOMETRY REPORT — what animating generated geometry costs
//
// The sphere has to ROTATE, and its wires are shapes::parametric outlines. An
// Element's outline is a std::function<SkPath(SkSize)> resolved through
// Composer::Impl::resolveOutline (Paint.cpp:256), which memoizes on
// (description pointer, size) — so an OutlineFn that closes over a live value
// is evaluated ONCE and frozen into the node's picture. There is no
// `animated()` seam on OutlineFn the way there is on DecorationScheme.
//
// The only way to move generated geometry is therefore to re-describe it. This
// sketch does that through ONE slot: `slot("world")` holds the 12 sphere wires,
// the 6 limb rings and the 6 leader lines, and update() calls renderSlot()
// every frame with a fresh rotation. Everything else in the tree — chrome,
// gauges, cards, ladders, marginalia — is described once in setup() and keeps
// its caches. Measured cost of that decision is in the report; the natural API
// would be `outline(fn, Volatility::Live)` or an OutlineFn concept with the
// same optional `animated()` decorations already have.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint3.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace vs {

// ---------------------------------------------------------------------------
// THE TWO GRIDS. Geometry lives on 320x240 (x4 = 4 canvas px per unit); text
// lives on 512x240 (x2.5 horizontally, x4 vertically). Both are honoured.

constexpr float kW = 1280.0f, kH = 960.0f;
inline float snapG(float v) { return std::round(v / 4.0f) * 4.0f; }   // 320-grid
inline float snapX(float v) { return std::round(v / 2.5f) * 2.5f; }   // 512-grid
inline float snapY(float v) { return std::round(v / 4.0f) * 4.0f; }

inline SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16) & 255) / 255.0f, (float)((hex >> 8) & 255) / 255.0f,
          (float)(hex & 255) / 255.0f, a};
}
inline SkColor4f mul(SkColor4f c, float k, float a = -1) {
  return {c.fR * k, c.fG * k, c.fB * k, a < 0 ? c.fA : a};
}

// Reconstructed palette: pale bone rules and type over translucent slate, RISK
// amber -> blood, target highlight cold cyan.
const SkColor4f kBone = rgb(0xE8E4D8);
const SkColor4f kBoneDim = rgb(0xE8E4D8, 0.42f);
const SkColor4f kSlate = rgb(0x1A1E2A, 0.55f);
const SkColor4f kAmber = rgb(0xD8A038);
const SkColor4f kBlood = rgb(0xA81820);
const SkColor4f kCyan = rgb(0x7FD8E8);
const SkColor4f kVoid = rgb(0x090B11);

inline Decoration prog(PaintProgram p) { return Decoration(std::move(p)); }

// ---------------------------------------------------------------------------
// THE PROJECTION. Camera at the origin looking down +z; +x right, +y down
// (screen sense). A point projects to (f*X/Z, f*Y/Z) offset from the canvas
// centre. Nothing here is a squashed circle: every ellipse on the canvas is a
// real perspective image of a real circle, which is why the latitude rings'
// centres DRIFT off the sphere's centre instead of sharing it.

struct Cam {
  float f = 1700.0f;              // focal length, canvas px
  SkPoint3 C{0, 0, 1400};         // sphere centre, camera space
  float R = 400.0f;               // sphere radius, world units
  float pitchDeg = 22.0f;         // camera looks DOWN by this

  SkPoint project(SkPoint3 p) const {
    const float z = std::max(p.fZ, 1.0f);
    return {kW * 0.5f + f * p.fX / z, kH * 0.5f + f * p.fY / z};
  }
  /** The apparent silhouette radius: the tangency circle, projected. */
  float apparentRadius() const {
    const float d = std::sqrt(C.fX * C.fX + C.fY * C.fY + C.fZ * C.fZ);
    return f * R / std::sqrt(std::max(d * d - R * R, 1.0f));
  }
  /** World "up" tilted toward the viewer by the camera pitch, plus the two
   *  axes that complete the frame. A camera looking DOWN sees the world's up
   *  vector lean toward it, which is the -z component. */
  SkPoint3 axisY() const {
    const float p = pitchDeg * 0.017453293f;
    return {0, -std::cos(p), -std::sin(p)};
  }
  SkPoint3 axisX() const { return {1, 0, 0}; }
  SkPoint3 axisZ() const {
    const float p = pitchDeg * 0.017453293f;
    return {0, std::sin(p), -std::cos(p)};
  }
  /** Unit normal at (lat, long) in the sphere's own frame, spun by psi. */
  SkPoint3 normal(float latRad, float lonRad) const {
    const SkPoint3 X = axisX(), Y = axisY(), Z = axisZ();
    const float cl = std::cos(latRad), sl = std::sin(latRad);
    const float co = std::cos(lonRad), so = std::sin(lonRad);
    return {cl * co * X.fX + sl * Y.fX + cl * so * Z.fX,
            cl * co * X.fY + sl * Y.fY + cl * so * Z.fY,
            cl * co * X.fZ + sl * Y.fZ + cl * so * Z.fZ};
  }
  SkPoint3 surface(SkPoint3 n) const {
    return {C.fX + R * n.fX, C.fY + R * n.fY, C.fZ + R * n.fZ};
  }
  /** The tangency test, exact: with n the outward normal at P and the eye at
   *  the origin, P is on the near side iff dot(n, C) < -R. */
  float facing(SkPoint3 n) const {
    return n.fX * C.fX + n.fY * C.fY + n.fZ * C.fZ + R;  // < 0 == visible
  }
  SkPoint centre() const { return project(C); }
};

// ---------------------------------------------------------------------------
// A WIRE: one great circle or latitude ring, split at its tangency.
//
// shapes::parametric samples t uniformly and joins the samples with lines, so
// the SPLIT FRACTION IS AN ARC-LENGTH FRACTION OF THAT POLYLINE, not of t.
// We therefore build the same polyline ourselves, integrate its length, and
// hand the measured fraction to PathFormat::trimEnd. The curve is re-based so
// t0 is the moment the wire ENTERS visibility, which removes the wrap case
// entirely: near is always [0, split] and far is always [split, 1].

struct Wire {
  shapes::OutlineFn outline;
  float split = 1.0f;   ///< arc-length fraction where near becomes far
  float nearWeight = 2.0f;
  bool allNear = false, allFar = false;
};

/** @p at maps a curve parameter in [0, 2pi) to a unit normal. */
inline Wire buildWire(const Cam &cam, float halfExtent,
                      const std::function<SkPoint3(float)> &at,
                      int samples = 128) {
  constexpr float kTau = 6.28318530718f;
  // 1. find the entry tangency: facing() crossing from >= 0 to < 0.
  const int scan = 720;
  float t0 = 0;
  bool found = false, anyVis = false, anyHid = false;
  float prev = cam.facing(at(0));
  for (int i = 1; i <= scan; ++i) {
    const float t = kTau * (float)i / (float)scan;
    const float cur = cam.facing(at(t));
    (cur < 0 ? anyVis : anyHid) = true;
    if (!found && prev >= 0 && cur < 0) {
      const float k = prev / std::max(prev - cur, 1e-6f);
      t0 = kTau * ((float)(i - 1) + k) / (float)scan;
      found = true;
    }
    prev = cur;
  }
  Wire w;
  w.allNear = !anyHid;
  w.allFar = !anyVis;

  // 2. the polyline shapes::parametric will build, in UNIT space. The box is
  //    square, so unit-space arc length is proportional to canvas arc length
  //    and the fraction transfers exactly.
  const SkPoint sphereCentre = cam.centre();
  auto unitAt = [&](float t) {
    const SkPoint p = cam.project(cam.surface(at(t)));
    return SkPoint{(p.fX - sphereCentre.fX) / halfExtent,
                   (p.fY - sphereCentre.fY) / halfExtent};
  };
  std::vector<float> cum((size_t)samples + 1, 0.0f);
  std::vector<float> face((size_t)samples + 1, 0.0f);
  SkPoint last = unitAt(t0);
  face[0] = cam.facing(at(t0));
  float total = 0;
  for (int i = 1; i <= samples; ++i) {
    const float t = t0 + kTau * (float)i / (float)samples;
    const SkPoint p = unitAt(t);
    total += std::hypot(p.fX - last.fX, p.fY - last.fY);
    cum[(size_t)i] = total;
    face[(size_t)i] = cam.facing(at(t));
    last = p;
  }
  w.split = 1.0f;
  if (total > 1e-5f && !w.allNear && !w.allFar) {
    for (int i = 1; i <= samples; ++i)
      if (face[(size_t)(i - 1)] < 0 && face[(size_t)i] >= 0) {
        // a < 0 <= b, so (a - b) is NEGATIVE and clamping it to a positive
        // epsilon flips the fraction to a huge negative number. The whole
        // sphere painted as far-side dotted rule until this was a magnitude
        // guard instead of a max().
        const float a = face[(size_t)(i - 1)], b = face[(size_t)i];
        const float denom = a - b;
        const float k = std::abs(denom) > 1e-6f ? a / denom : 0.5f;
        const float d = cum[(size_t)(i - 1)] +
                        (cum[(size_t)i] - cum[(size_t)(i - 1)]) * k;
        w.split = std::clamp(d / total, 0.0f, 1.0f);
        break;
      }
  } else if (w.allFar) {
    w.split = 0.0f;
  }

  // 3. weight from how front-facing the circle gets: a wire that swings right
  //    up to the viewer reads heaviest. Depth is WEIGHT here, never alpha.
  float best = 0;
  for (int i = 0; i <= samples; ++i)
    best = std::min(best, face[(size_t)i]);
  const float depth = std::clamp(-best / (cam.R * 1.7f), 0.0f, 1.0f);
  w.nearWeight = 1.3f + 1.9f * depth;

  // Left in, opt-in: `#define VS_WIRE_DEBUG 1` above the includes prints every
  // wire's tangency numbers. It is how the split bug above was found, and a
  // wrong split is INVISIBLE in the render — it just looks like a flat mandala
  // rather than a sphere, which reads as a styling choice.
#ifdef VS_WIRE_DEBUG
  SkDebugf("[wire] t0=%.3f split=%.3f allNear=%d allFar=%d weight=%.2f total=%.3f\n",
           t0, w.split, (int)w.allNear, (int)w.allFar, w.nearWeight, total);
#endif
  w.outline = shapes::parametric(
      [at, cam, sphereCentre, halfExtent](float t) {
        const SkPoint p = cam.project(cam.surface(at(t)));
        return SkPoint{(p.fX - sphereCentre.fX) / halfExtent,
                       (p.fY - sphereCentre.fY) / halfExtent};
      },
      t0, t0 + kTau, samples, true);
  return w;
}

// ---------------------------------------------------------------------------
// THE BITMAP-CELL TYPE. SigilWeave has no bitmap-font path; ShapingStyle::
// aliased gives the hard-edged rasterisation, which is most of what a PS1 face
// looked like. Static labels go through text() elements with aliased shaping,
// condensed and snapped to the 512-grid. LIVE numbers (the hit percentage,
// RISK, the rate) cannot: text() takes no PropValue. So the digits and a few
// symbols are baked ONCE into 1-bit A8 cells at setup and blitted with
// kNearest inside custom() leaves that read the bound Output directly — the
// number IS the value, with no re-describe.

struct Cell {
  sk_sp<SkImage> mask;
  int w = 0, h = 0, adv = 0;
};

struct PixFont {
  std::array<Cell, 96> cells;   // ascii 32..127
  int lineHeight = 0;
  int digitAdv = 0;             // tabular: every digit shares one advance
};

inline Cell bakeCell(char32_t ch, weave::FontContext &fonts,
                     const weave::TextStyle &st) {
  std::string s;
  s.push_back((char)ch);
  const std::u8string u8(reinterpret_cast<const char8_t *>(s.c_str()));
  Element tree = box().child(text(u8, st));
  const SkSize sz = measure(box().child(text(u8, st)), fonts);
  const int w = std::max(1, (int)std::ceil(sz.width()) + 3);
  const int h = std::max(1, (int)std::ceil(sz.height()) + 3);
  sk_sp<SkSurface> surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
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
  SkBitmap a8;
  a8.allocPixels(SkImageInfo::MakeA8(w, h));
  a8.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      *a8.getAddr8(x, y) = SkColorGetA(read.getColor(x, y)) >= 100 ? 255 : 0;
  a8.setImmutable();
  Cell c;
  c.mask = a8.asImage();
  c.w = w;
  c.h = h;
  c.adv = std::max(1, (int)std::lround(sz.width()));
  return c;
}

inline PixFont bakeFont(weave::FontContext &fonts, const sk_sp<SkTypeface> &face,
                        float sizePx, float condense) {
  weave::TextStyle st;
  st.shaping.typeface = face;
  st.shaping.fontSize = sizePx;
  st.shaping.aliased = true;
  st.shaping.scaleX = condense;
  st.paint.foreground.setColor(SK_ColorWHITE);
  PixFont f;
  for (int i = 0; i < 96; ++i) {
    const char32_t ch = (char32_t)(32 + i);
    if (ch == 32) {
      f.cells[(size_t)i].adv = (int)std::lround(sizePx * 0.34f * condense);
      continue;
    }
    f.cells[(size_t)i] = bakeCell(ch, fonts, st);
    f.lineHeight = std::max(f.lineHeight, f.cells[(size_t)i].h);
  }
  for (int d = 0; d < 10; ++d)
    f.digitAdv = std::max(f.digitAdv, f.cells[(size_t)('0' - 32 + d)].adv);
  return f;
}

/** Blit a string at (x, y) top-left. Digits use one tabular advance so a
 *  rolling readout does not shiver; everything else uses its own. Positions
 *  land on the 512-grid — snapX horizontally, snapY vertically. */
inline float blit(SkCanvas &c, const PixFont &f, float x, float y,
                  const std::string &s, SkColor4f col, float track = 1.0f) {
  SkPaint p;
  p.setAntiAlias(false);
  p.setColor4f(col, nullptr);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  float cx = snapX(x);
  const float cy = snapY(y);
  for (char raw : s) {
    const int idx = (int)(unsigned char)raw - 32;
    if (idx < 0 || idx >= 96)
      continue;
    const Cell &cell = f.cells[(size_t)idx];
    const bool digit = raw >= '0' && raw <= '9';
    if (cell.mask)
      c.drawImageRect(cell.mask,
                      SkRect::MakeXYWH(cx, cy, (float)cell.w, (float)cell.h),
                      nearest, &p);
    cx = snapX(cx + (float)(digit ? f.digitAdv : cell.adv) + track);
  }
  return cx - snapX(x);
}

inline float widthOf(const PixFont &f, const std::string &s,
                     float track = 1.0f) {
  float w = 0;
  for (char raw : s) {
    const int idx = (int)(unsigned char)raw - 32;
    if (idx < 0 || idx >= 96)
      continue;
    const bool digit = raw >= '0' && raw <= '9';
    w += (float)(digit ? f.digitAdv : f.cells[(size_t)idx].adv) + track;
  }
  return w;
}

// ---------------------------------------------------------------------------
// THE RISK GAUGE'S DECORATION USED TO LIVE HERE, AND NO LONGER HAS TO.
//
// This file shipped a 24-line `RiskHatch` value decoration that composed
// `lines::Hatch`, held an `Output<float>*`, mapped RISK to a pitch at paint
// time and returned `animated() == true`. The reason was real when it was
// written: `Hatch::spacing` was a plain float with no binding seam, so a
// RISK-driven density would have forced a `render()` every frame purely to
// change one number.
//
// THAT GAP IS CLOSED. `Hatch::spacingBinding` and `Hatch::angleBinding`
// (Lines.h:1185-1186) take exactly the raw `Output<float>*` the workaround
// held, with `animated()` at :1188 and both in `operator==` at :1196 — which
// is the same shape the workaround argued for, so it is gone and the gauge
// binds the stock decoration. The one thing the field cannot do is MAP, so
// the RISK-to-pitch curve moved into the ticker as its own Output; that is
// where the study's other three shapings of the same value already live.

// ---------------------------------------------------------------------------
// THE ARITHMETIC. Every number below is the decompiled function, integer
// division and clamp order included.

struct Limb {
  const char *name;
  short hp, maxHp;
  signed char agilityDefenseBonus;
  unsigned char chainEvasion;
  const char *armour;
  short types[4];      // blunt / edged / piercing / +1
  short affinities[8]; // air fire earth water light dark +2
  /** status.yaml's first defence row. It is NOT in the limb struct — physical
   *  defence lives on the ARMOUR (vs_battle_uiArmor), which the limb owns at
   *  offset 0x18 — so it is carried alongside rather than faked into types[3],
   *  and the ladder below says where each of its ten rows comes from. */
  short physical;
};

/** status.yaml's ten rows mapped onto the two arrays the limb actually has:
 *  row 0 is the armour's physical defence, rows 1-6 are affinities[0..5]
 *  (air fire earth water light dark), rows 7-9 are types[0..2] (blunt edged
 *  piercing). affinities[6..7] and types[3] are the decomp's unnamed "+2" and
 *  "+1" slots and are deliberately not shown. */
inline short defenceValue(const struct Limb &l, int row);

// RECONSTRUCTED values in a DOCUMENTED struct. Part naming is the conventional
// humanoid reconstruction of limbs[0..5]; the struct itself carries only a
// nameIndex.
constexpr Limb kLimbs[6] = {
    // name      hp maxHp agiDef eva  armour        types[4]              affinities[8]                   physical
    {"HEAD",   88, 120,  6,  40, "hounskull",   { 12,  -8,  20,  0}, {  4, -6,  0, 2,  8, -10, 0, 0}, 18},
    {"BODY",  210, 260,  2,  18, "cuirass",     { 24,  10,  -6,  0}, {  0,  6,  8, 0,  2,  -4, 0, 0}, 31},
    {"R.ARM",  96, 140, -3,  92, "vambrace",    {  6,   4,   2,  0}, { -2,  0,  4, 6,  0,   0, 0, 0}, 14},
    {"L.ARM", 132, 140,  1,  55, "gauntlet",    {  8,  12,  -4,  0}, {  2,  4,  0, 0, -6,   2, 0, 0}, 16},
    {"R.LEG", 148, 180,  0,  30, "greave",      { 14,   6,   8,  0}, {  0,  2, 10, 4,  0,   0, 0, 0}, 21},
    {"L.LEG", 180, 180,  4,  12, "sabaton",     { 16,   2,  12,  0}, {  6,  0,  6, 0,  0,  -2, 0, 0}, 23},
};

// status.yaml's ten defence rows, in file order.
inline short defenceValue(const Limb &l, int row) {
  if (row == 0)
    return l.physical;
  if (row <= 6)
    return l.affinities[row - 1];       // air fire earth water light dark
  return l.types[row - 7];              // blunt edged piercing
}

constexpr const char *kDefenceRows[10] = {
    "PHYSICAL", "AIR", "FIRE", "EARTH", "WATER",
    "LIGHT",    "DARK", "BLUNT", "EDGED", "PIERCING"};
// status.yaml's condition tiers.
constexpr const char *kCondition[5] = {"CRITICAL", "DAMAGED", "WOUNDED", "GOOD",
                                       "EXCELLENT"};

/** Ashley and the Dullahan, in the fields the formula actually reads. */
struct Actor {
  int agility;
  int accessoryAgi;
  int armourAgiSum;   // SUM(limbs[0..5].armor.currentAgility)
  int weaponAgi, shieldAgi;
  bool drawn;         // vs_battle_actors[..]->unk20 & 1
};
constexpr Actor kAshley{42, 3, -7, 2, -4, true};
constexpr Actor kEnemy{30, 0, 5, 0, 0, false};

inline int actorAgi(const Actor &a) {
  int v = a.agility + a.accessoryAgi + a.armourAgiSum;
  if (a.drawn)
    v += a.weaponAgi + a.shieldAgi;
  return v;
}

/** _getAgilityDifference, 146C.c:7110 — variance and gems left at zero so the
 *  printed number is the deterministic part the HUD shows. */
inline int agilityDifference(int part, float riskSrc, float riskTgt) {
  const int rs = (int)std::lround(riskSrc), rt = (int)std::lround(riskTgt);
  int src = actorAgi(kAshley) * (100 - rs) / 100;
  int tgt = actorAgi(kEnemy) + kLimbs[part].agilityDefenseBonus;
  tgt = tgt * (100 - rt) / 100;
  int d = src - tgt + 100;
  if (d < 0)
    d = 0;
  else if (d == 255)
    d = 254;
  return d;
}
/** What the HUD prints: _doesAttackHit's clamp to 100, before the cheat. */
inline int printedHit(int part, float risk) {
  return std::min(agilityDifference(part, risk, 0.0f), 100);
}
/** What actually rolls: 146C.c:7334 adds 10 for actorId 0 AFTER the clamp. */
inline int trueHit(int part, float risk) { return printedHit(part, risk) + 10; }
/** _getRiskModifier, 146C.c:7265. */
inline int riskRate(float risk) {
  const int r = ((int)std::lround(risk) + 150) * 100 / 256;
  return r == 255 ? 254 : r;
}
/** _getChainEvasionModifier, 146C.c:7276. */
inline int chainEvasionRate(int part) {
  const int r = (255 - kLimbs[part].chainEvasion) * 100 / 255;
  return r == 255 ? 254 : r;
}
inline const char *conditionOf(int hp, int maxHp) {
  const float f = maxHp > 0 ? (float)hp / (float)maxHp : 0.0f;
  if (f >= 0.999f) return kCondition[4];
  if (f >= 0.70f) return kCondition[3];
  if (f >= 0.45f) return kCondition[2];
  if (f >= 0.20f) return kCondition[1];
  return kCondition[0];
}

// The five-bit fields, as data.
constexpr int kReach = 9;                    // u_int reach : 5
constexpr int kAttackShapeAngle = 12;        // u_int currentAttackShapeAngle : 5
constexpr float kAngleStep = 360.0f / 32.0f; // 11.25 deg per step
constexpr int kEnemyClass = 2;               // u_int enemyClass : 3 -> undead
constexpr const char *kClassNames[6] = {"HUMAN",  "BEAST",  "UNDEAD",
                                        "PHANTOM", "DRAGON", "EVIL"};
// ITEMNAME.BIN.yaml, in file order; index 20 is the drawn weapon.
constexpr const char *kWeaponName = "KATANA";
constexpr int kSelected = 2;                 // R.ARM

} // namespace vs

// =============================================================================

struct VagrantStoryTarget : sigil::compose::sketch::Sketch {
  using Cam = vs::Cam;

  // ---- bound values --------------------------------------------------------
  ch::Output<float> risk{58.0f};      // decays; drives FOUR consumers
  ch::Output<float> riskPitch{8.0f};  // ...one of which is the hatch's pitch
  ch::Output<float> chainSweep{0.0f}; // the timing ring, 0 -> 1
  ch::Output<float> select{1.0f};     // selection pulse
  ch::Output<float> damagePop{0.0f};  // the damage-number pop

  double clock = 0;
  float psi = 0;                      // sphere spin, radians

  vs::PixFont fontS, fontM, fontL;
  sk_sp<SkTypeface> face;

  Cam cam;
  float halfExtent = 560.0f;
  SkPoint sphereCentre{0, 0};
  std::array<SkPoint, 6> cardAt{};    // fixed card centres, canvas px
  std::array<int, 6> slotOf{};        // limb -> card slot, frozen at psi = 0
  std::array<SkRect, 6> cardBounds{}; // read back from Composer::bounds()

  // -------------------------------------------------------------------------
  // The enemy's six limb anchors, in the sphere's own rotating frame. Units of
  // the sphere radius; the whole cluster spins with the sphere, so the leader
  // lines re-route every frame while the cards stay put.

  static SkPoint3 limbOffset(int i) {
    static constexpr float kOff[6][3] = {
        { 0.00f,  0.34f,  0.00f},  // HEAD
        { 0.00f,  0.10f,  0.02f},  // BODY
        {-0.17f,  0.13f,  0.07f},  // R.ARM
        { 0.17f,  0.12f, -0.05f},  // L.ARM
        {-0.09f, -0.17f,  0.03f},  // R.LEG
        { 0.09f, -0.17f, -0.03f},  // L.LEG
    };
    return {kOff[i][0], kOff[i][1], kOff[i][2]};
  }

  /** The limb's canvas position: body offset + limb offset, spun about the
   *  sphere's polar axis by psi, then projected. */
  SkPoint limbPoint(int i, float spin) const {
    const SkPoint3 base{0.60f, 0.02f, -0.08f};
    const SkPoint3 o = limbOffset(i);
    const float x = base.fX + o.fX, y = base.fY + o.fY, z = base.fZ + o.fZ;
    const float cs = std::cos(spin), sn = std::sin(spin);
    const float rx = x * cs + z * sn, rz = -x * sn + z * cs;
    const SkPoint3 X = cam.axisX(), Y = cam.axisY(), Z = cam.axisZ();
    const SkPoint3 world{
        cam.C.fX + cam.R * (rx * X.fX + y * Y.fX + rz * Z.fX),
        cam.C.fY + cam.R * (rx * X.fY + y * Y.fY + rz * Z.fY),
        cam.C.fZ + cam.R * (rx * X.fZ + y * Y.fZ + rz * Z.fZ)};
    return cam.project(world);
  }

  // -------------------------------------------------------------------------
  // THE SPHERE. Six great circles through the poles (reading as twelve
  // meridians), five latitude rings, one silhouette — every one a real
  // perspective image, split at its tangency.

  Element sphereGroup(float spin) const {
    constexpr float kDeg = 0.017453293f;
    Element g = box().absolute().inset(0).key("sphere");

    auto wireEl = [&](const vs::Wire &w, const char *key, bool ring) {
      vs::Wire wire = w;
      Element e = box()
                      .absolute()
                      .width(halfExtent * 2)
                      .height(halfExtent * 2)
                      .centerAt(sphereCentre)
                      .key(key)
                      .outline(wire.outline)
                      // The draw-on: each wire reveals over 320 ms, and the
                      // group's staggerChildren(24ms) sweeps it round the
                      // equator — 12 x 24 ms is the full ring in ~290 ms.
                      // Settled at (0,1) the trim is a no-op (Paint.cpp:678
                      // skips the effect when s == 0 and e == 1).
                      .trim(0.0f, withFrom(0.0f, 1.0f,
                                           {.duration = 320ms,
                                            .ease = &ch::easeOutQuad}));
      if (!wire.allFar) {
        PathFormat nearRule;
        nearRule.width = wire.nearWeight * (ring ? 0.82f : 1.0f);
        nearRule.strokeFill =
            Fill::color(vs::mul(vs::kBone, 1.0f, ring ? 0.62f : 0.80f));
        nearRule.cap = SkPaint::kRound_Cap;
        nearRule.trimStart = 0.0f;
        nearRule.trimEnd = wire.allNear ? 1.0f : wire.split;
        e.foreground(nearRule);
      }
      if (!wire.allNear) {
        PathFormat farRule;
        farRule.width = 1.0f;
        farRule.strokeFill = Fill::color(vs::mul(vs::kBone, 1.0f, 0.46f));
        farRule.cap = SkPaint::kButt_Cap;
        // The drafting hidden-line is a SHORT DASH, not a dot: 3 on, 5 off
        // reads as "this arc is behind the solid" at every zoom, where a
        // 1 px dot every 10 px reads as noise.
        farRule.dashIntervals = {3.0f, 5.0f};
        farRule.trimStart = wire.allFar ? 0.0f : wire.split;
        farRule.trimEnd = 1.0f;
        e.foreground(farRule);
      }
      return e;
    };

    // 6 great circles through the poles == 12 meridians.
    static const char *kMerKeys[6] = {"m0", "m1", "m2", "m3", "m4", "m5"};
    for (int k = 0; k < 6; ++k) {
      const float lon = spin + (float)k * 30.0f * kDeg;
      const Cam c = cam;
      g.child(wireEl(vs::buildWire(cam, halfExtent,
                                   [c, lon](float t) {
                                     return c.normal(t, lon);
                                   }),
                     kMerKeys[k], false));
    }
    // 5 latitude rings.
    static const char *kLatKeys[5] = {"l0", "l1", "l2", "l3", "l4"};
    for (int k = 0; k < 5; ++k) {
      const float lat = (-60.0f + 30.0f * (float)k) * kDeg;
      const Cam c = cam;
      g.child(wireEl(vs::buildWire(cam, halfExtent,
                                   [c, lat, spin](float t) {
                                     return c.normal(lat, t + spin);
                                   }),
                     kLatKeys[k], true));
    }
    return g;
  }

  /** The silhouette: the tangency circle, projected. Static — the apparent
   *  contour of a sphere does not move when the sphere spins. lines::cased
   *  doubles it into the heavy edge that makes the wire ball read as solid. */
  Element silhouette() const {
    const float r = cam.apparentRadius();
    Element e = box()
                    .absolute()
                    .width(halfExtent * 2)
                    .height(halfExtent * 2)
                    .centerAt(sphereCentre)
                    .key("silhouette")
                    .outline(shapes::parametric(
                        [r, h = halfExtent](float t) {
                          return SkPoint{r / h * std::cos(t),
                                         r / h * std::sin(t)};
                        },
                        0, 6.28318530718f, 192, true));
    e.stroke(lines::Rails{.rails = {
                              {.offset = -2.5f, .width = 2.6f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.92f))},
                              {.offset = 2.5f, .width = 1.0f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.40f))},
                          }});
    return e;
  }

  // -------------------------------------------------------------------------
  // THE GROUND PLANE: the attack wedge and the 32-division tick ladder. Both
  // are foreshortened by the camera pitch — a horizontal circle under a camera
  // pitched down by p squashes to sin(p) of its width.

  /** The reach sphere is centred on Ashley's CHEST, so the ground plane cuts
   *  the sphere well above its lowest point — 0.30 R down the world-up axis.
   *  Every ground construction (the wedge, the 32-tick protractor, the rim)
   *  is inscribed in the ellipse that plane projects to. */
  SkPoint feet() const {
    const SkPoint3 up = cam.axisY();
    return cam.project({cam.C.fX - cam.R * up.fX * 0.30f,
                        cam.C.fY - cam.R * up.fY * 0.30f,
                        cam.C.fZ - cam.R * up.fZ * 0.30f});
  }

  Element groundPlate() const {
    const float squash = std::sin(cam.pitchDeg * 0.017453293f) * 1.62f;
    const float rx = vs::snapG(cam.apparentRadius() * 0.94f);
    const float ry = vs::snapG(rx * squash);
    const SkPoint at = feet();

    Element g = box().absolute().inset(0).key("ground");

    // The attack wedge: shapes::sector at currentAttackShapeAngle * 11.25 deg,
    // inscribed in a foreshortened box, so the wedge IS an ellipse sector.
    const float sweep = (float)vs::kAttackShapeAngle * vs::kAngleStep;
    const float start = -12.5f - sweep * 0.5f;   // aimed at the Dullahan
    Element wedge = box()
                        .absolute()
                        .width(rx * 2)
                        .height(ry * 2)
                        .centerAt(at)
                        .key("wedge")
                        .outline(shapes::sector(start, sweep))
                        .fill(Fill::color(vs::rgb(0x7FD8E8, 0.035f)));
    wedge.overlay(lines::Hatch{.strokeFill = Fill::color(vs::rgb(0x7FD8E8, 0.11f)),
                               .spacing = 15.0f,
                               .width = 1.0f,
                               .angleDeg = 118.0f});
    wedge.stroke(PathFormat{.width = 1.4f,
                            .strokeFill = Fill::color(vs::mul(vs::kCyan, 1, 0.55f)),
                            .dashIntervals = {9.0f, 6.0f},
                            .cap = SkPaint::kButt_Cap});
    g.child(std::move(wedge));

    // The tick ladder: 32 divisions from the 5-bit field, two radialHatch
    // passes on two annular bands — every 8th tick long.
    const float outerRx = rx * 1.06f, outerRy = ry * 1.06f;
    g.child(box()
                .absolute()
                .width(outerRx * 2)
                .height(outerRy * 2)
                .centerAt(at)
                .key("ticks32")
                .outline(shapes::annulus(0.90f))
                .foreground(lines::radialHatch(
                    Fill::color(vs::mul(vs::kBone, 1, 0.55f)), 32, 1.4f)));
    g.child(box()
                .absolute()
                .width(outerRx * 2)
                .height(outerRy * 2)
                .centerAt(at)
                .key("ticks4")
                .outline(shapes::annulus(0.78f))
                .foreground(lines::radialHatch(
                    Fill::color(vs::mul(vs::kBone, 1, 0.80f)), 4, 2.0f)));
    // The rim itself: a solid outer rule with a counter-dashed inner strand.
    g.child(box()
                .absolute()
                .width(outerRx * 2)
                .height(outerRy * 2)
                .centerAt(at)
                .key("rim")
                .outline(shapes::circle())
                .stroke(lines::Rails{
                    .rails = {{.offset = 0, .width = 1.4f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.62f))},
                              {.offset = 7.0f, .width = 1.0f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.30f)),
                               .dash = {3.0f, 7.0f}}}}));
    return g;
  }

  // -------------------------------------------------------------------------
  // THE FIGURES. Two generated silhouettes on the 320-grid: Ashley at the
  // sphere's near-bottom (the wedge runs UNDER him, which is what makes the
  // wedge read as ground), the Dullahan at the limb cluster.

  static SkPath figurePath(float w, float h, bool armsOut) {
    SkPathBuilder p;
    const float hx = w * 0.5f;
    p.moveTo(hx - w * 0.22f, h);
    p.lineTo(hx - w * 0.09f, h * 0.62f);
    p.lineTo(hx - w * 0.26f, h * 0.60f);
    p.lineTo(hx - (armsOut ? w * 0.50f : w * 0.36f), h * 0.50f);
    p.lineTo(hx - w * 0.42f, h * 0.28f);
    p.lineTo(hx - w * 0.22f, h * 0.24f);
    p.lineTo(hx - w * 0.13f, h * 0.14f);
    p.lineTo(hx + w * 0.13f, h * 0.14f);
    p.lineTo(hx + w * 0.22f, h * 0.24f);
    p.lineTo(hx + w * 0.42f, h * 0.28f);
    p.lineTo(hx + (armsOut ? w * 0.50f : w * 0.36f), h * 0.50f);
    p.lineTo(hx + w * 0.26f, h * 0.60f);
    p.lineTo(hx + w * 0.09f, h * 0.62f);
    p.lineTo(hx + w * 0.22f, h);
    p.lineTo(hx + w * 0.04f, h);
    p.lineTo(hx, h * 0.66f);
    p.lineTo(hx - w * 0.04f, h);
    p.close();
    p.addOval(SkRect::MakeXYWH(hx - w * 0.15f, 0, w * 0.30f, h * 0.17f));
    return p.detach();
  }

  Element ashley() const {
    const SkPoint at = feet();
    const float w = 104, h = 288;
    Element e = box()
                    .absolute()
                    .width(w)
                    .height(h)
                    .left(vs::snapG(at.fX - w * 0.5f))
                    .top(vs::snapG(at.fY - h))
                    .key("ashley")
                    .outline([w, h](SkSize) { return figurePath(w, h, false); })
                    .fill(Fill::color(vs::rgb(0x05070C, 1.0f)));
    e.stroke(PathFormat{.width = 1.8f,
                        .strokeFill = Fill::color(vs::mul(vs::kBone, 1, 0.72f))});
    e.foreground(lines::Hatch{
        .strokeFill = Fill::color(vs::mul(vs::kBone, 1, 0.13f)),
        .spacing = 7.0f, .width = 1.0f, .angleDeg = 62.0f});
    return e;
  }

  /** THE DULLAHAN, BUILT AS limbs[6]. The struct is six parts, so the figure
   *  is six parts: one drawable per limb, centred on that limb's projected
   *  anchor, joined by drawn bones. Nothing about the targeting is arbitrary
   *  then — the marker ring, the leader and the readout all key off a shape
   *  you can point at. Each part's silhouette is the shape that part is:
   *  a skull oval, a notched cuirass, tapered vambraces, chamfered greaves. */
  Element limbBody(float spin) const {
    // One projected unit of sphere radius, in canvas px — every part scales
    // with the projection instead of carrying a hardcoded size.
    const float s = cam.f / cam.C.fZ * cam.R;
    struct Part { float w, h; };
    static constexpr Part kPart[6] = {
        {0.115f, 0.140f},  // HEAD
        {0.200f, 0.300f},  // BODY
        {0.075f, 0.260f},  // R.ARM
        {0.075f, 0.260f},  // L.ARM
        {0.085f, 0.300f},  // R.LEG
        {0.085f, 0.300f},  // L.LEG
    };
    Element g = box().absolute().inset(0).key("dullahan");

    // The bones first, under the parts: BODY out to each of the other five.
    const SkPoint torso = limbPoint(1, spin);
    SkPathBuilder bones;
    for (int i = 0; i < 6; ++i) {
      if (i == 1)
        continue;
      const SkPoint p = limbPoint(i, spin);
      bones.moveTo(torso);
      bones.lineTo(p);
    }
    const SkPath bonePath = bones.detach();
    const SkRect bb = bonePath.getBounds().makeOutset(12, 12);
    g.child(box()
                .absolute()
                .left(bb.left())
                .top(bb.top())
                .width(bb.width())
                .height(bb.height())
                .key("bones")
                .outline([bonePath, bb](SkSize) {
                  return bonePath.makeTransform(
                      SkMatrix::Translate(-bb.left(), -bb.top()));
                })
                .stroke(lines::Rails{
                    .rails = {{.offset = 0, .width = 9.0f,
                               .fill = Fill::color(vs::rgb(0x0A0F18, 1.0f))},
                              {.offset = 0, .width = 1.6f,
                               .fill = Fill::color(
                                   vs::mul(vs::kCyan, 1, 0.70f)),
                               .dash = {7.0f, 4.0f}}}}));

    for (int i = 0; i < 6; ++i) {
      const SkPoint at = limbPoint(i, spin);
      const float w = s * kPart[i].w, h = s * kPart[i].h;
      shapes::OutlineFn shape;
      switch (i) {
      case 0: shape = shapes::circle(); break;
      case 1: shape = shapes::notched(w * 0.30f, h * 0.10f,
                                      shapes::Corner::All); break;
      case 2:
      case 3: shape = shapes::chamfered(w * 0.44f, shapes::Corner::All); break;
      default: shape = shapes::chamfered(w * 0.40f,
                                         shapes::Corner::Diagonal); break;
      }
      Element part = box()
                         .absolute()
                         .width(w)
                         .height(h)
                         .centerAt(at)
                         .key(std::string("part") + std::to_string(i))
                         .outline(shape)
                         .fill(Fill::color(vs::rgb(0x0A0F18, 0.94f)));
      part.foreground(lines::Hatch{
          .strokeFill = Fill::color(vs::mul(vs::kCyan, 1, 0.20f)),
          .spacing = 5.0f, .width = 1.0f,
          .angleDeg = (i == 2 || i == 4) ? 118.0f : 62.0f});
      part.stroke(PathFormat{
          .width = i == vs::kSelected ? 2.0f : 1.3f,
          .strokeFill = Fill::color(vs::mul(
              vs::kCyan, 1, i == vs::kSelected ? 0.95f : 0.62f))});
      g.child(std::move(part));
    }
    return g;
  }

  // -------------------------------------------------------------------------
  // THE LEADERS. A drafting leader is three tiles: an open RING where it
  // touches the part, a hairline dash repeated along the run, and a right-angle
  // TICK the label sits on. brushes::PatternBrush places exactly those, in
  // exactly those roles, on an open contour.

  Element ringTile() const {
    return box()
        .width(13)
        .height(13)
        .outline(shapes::circle())
        .stroke(PathFormat{.width = 1.6f,
                           .strokeFill = Fill::color(vs::kCyan)});
  }
  Element dashTile() const {
    return box()
        .width(14)
        .height(3)
        .outline([](SkSize s) {
          SkPathBuilder p;
          p.moveTo(0, s.height() * 0.5f);
          p.lineTo(s.width() * 0.62f, s.height() * 0.5f);
          return p.detach();
        })
        .stroke(PathFormat{.width = 1.2f,
                           .strokeFill =
                               Fill::color(vs::mul(vs::kBone, 1, 0.78f))});
  }
  Element tickTile() const {
    return box()
        .width(22)
        .height(16)
        .outline([](SkSize s) {
          SkPathBuilder p;
          p.moveTo(0, s.height() * 0.5f);
          p.lineTo(s.width() * 0.55f, s.height() * 0.5f);
          p.lineTo(s.width() * 0.55f, s.height());
          return p.detach();
        })
        .stroke(PathFormat{.width = 1.6f,
                           .strokeFill = Fill::color(vs::kBone),
                           .cap = SkPaint::kSquare_Cap,
                           .join = SkPaint::kMiter_Join});
  }

  // Tile arts AND the brush itself are held as members, and the second half
  // matters more than the first. PatternBrush owns its bake in a
  // shared_ptr<Cache> that lives IN THE VALUE, so a brush constructed inside
  // the per-frame describe gets a fresh empty cache and re-runs snapshot() on
  // all three tiles every frame — three full reconcile+layout+record passes
  // per leader, eighteen per frame. Holding one brush and copying it shares
  // the bake, which is what the art-pointer cache key was designed for.
  Element ringArt, dashArt, tickArt;
  brushes::PatternBrush leaderBrush;

  Element leaders(float spin) const {
    Element g = box().absolute().inset(0).key("leaders");
    for (int i = 0; i < 6; ++i) {
      const SkPoint from = limbPoint(i, spin);
      const SkPoint card = cardAt[(size_t)slotOf[(size_t)i]];
      // Where the leader lands is the CARD'S OWN resolved rect, read back off
      // the composer with bounds() — the cards are described once and laid
      // out by the tree, so this frame's geometry feeds the next frame's
      // route rather than a number copied from the card builder.
      const SkRect &cb = cardBounds[(size_t)i];
      // Land on whichever edge FACES the part — a panel to the left of its
      // limb takes the leader on its right side, and the shoulder runs the
      // other way. Nothing about a drafting leader is left-handed.
      const bool fromRight = (cb.isEmpty() ? card.fX : cb.centerX()) < from.fX;
      const float landX = cb.isEmpty()
                              ? card.fX + (fromRight ? 122.0f : -122.0f)
                              : (fromRight ? cb.right() + 10.0f
                                           : cb.left() - 10.0f);
      const float landY = cb.isEmpty() ? card.fY : cb.centerY();
      // The drafting leader proper: a short RADIAL stub off the part, a
      // straight run to the shoulder, then a level shoulder into the label.
      const SkPoint torso = limbPoint(1, spin);
      float dx = from.fX - torso.fX, dy = from.fY - torso.fY;
      if (i == 1) {
        dx = landX - from.fX;
        dy = landY - from.fY;
      }
      const float m = std::max(std::hypot(dx, dy), 1.0f);
      const SkPoint stub{from.fX + dx / m * 30.0f, from.fY + dy / m * 30.0f};
      const float bendX = landX + (fromRight ? 44.0f : -44.0f);
      SkPathBuilder pb;
      pb.moveTo(from.fX, from.fY);
      pb.lineTo(stub.fX, stub.fY);
      pb.lineTo(bendX, landY);
      pb.lineTo(landX, landY);
      const SkPath path = pb.detach();
      const SkRect b = path.getBounds().makeOutset(30, 30);

      Element e = box()
                      .absolute()
                      .left(b.left())
                      .top(b.top())
                      .width(b.width())
                      .height(b.height())
                      .key(std::string("leader") + std::to_string(i))
                      .outline([path, b](SkSize) {
                        return path.makeTransform(
                            SkMatrix::Translate(-b.left(), -b.top()));
                      })
                      .stroke(leaderBrush);
      g.child(std::move(e));

      // The part marker itself: an open ring, heavier and cyan on the
      // selected part, with a weightedCorners diamond around it.
      const bool sel = i == vs::kSelected;
      const float mr = sel ? 30.0f : 19.0f;
      Element mark = box()
                         .absolute()
                         .width(mr * 2)
                         .height(mr * 2)
                         .centerAt(from)
                         .key(std::string("mark") + std::to_string(i))
                         .outline(shapes::circle());
      if (sel)
        // The lock: a cased ring — two rails on one route, the engraver's
        // answer to "make this one louder without making it fatter".
        mark.stroke(lines::cased(2.0f, Fill::color(vs::kCyan), 6.0f));
      else
        mark.stroke(PathFormat{
            .width = 1.3f,
            .strokeFill = Fill::color(vs::mul(vs::kBone, 1, 0.70f)),
            .dashIntervals = {5.0f, 4.0f}});
      if (sel)
        mark.opacity(bind(&select).map(&ch::easeInOutQuad).to(0.45f, 1.0f));
      g.child(std::move(mark));
      if (sel)
        g.child(box()
                    .absolute()
                    .width(mr * 2.9f)
                    .height(mr * 2.9f)
                    .centerAt(from)
                    .key("marksel")
                    .outline(shapes::chamfered(18.0f, shapes::Corner::All))
                    .rotate(45.0f)
                    .foreground(decorations::brackets(
                        2.2f, Fill::color(vs::kCyan), 11.0f)));
    }
    return g;
  }

  // -------------------------------------------------------------------------
  // TYPE. Static labels are real text() leaves with ShapingStyle::aliased,
  // condensed to 0.86 and snapped to the 512-grid.

  weave::TextStyle style(float px, SkColor4f col, float condense = 0.86f,
                         float track = 0.0f) const {
    weave::TextStyle st;
    st.shaping.typeface = face;
    st.shaping.fontSize = px;
    st.shaping.aliased = true;
    st.shaping.scaleX = condense;
    st.shaping.letterSpacing = track;
    st.paint.foreground.setColor4f(col, nullptr);
    st.paint.foreground.setAntiAlias(false);
    return st;
  }

  Element label(const std::string &s, float px, SkColor4f col,
                float track = 0.0f) const {
    const std::u8string u8(reinterpret_cast<const char8_t *>(s.c_str()));
    return text(u8, style(px, col, 0.86f, track));
  }

  Element labelAt(const std::string &s, float x, float y, float px,
                  SkColor4f col, float track = 0.0f) const {
    return label(s, px, col, track)
        .absolute()
        .left(vs::snapX(x))
        .top(vs::snapY(y));
  }

  // -------------------------------------------------------------------------
  // THE CARDS. Bracket-only frames on chamfered silhouettes: corners present,
  // edges absent, plus a rule that stops 6 px short of every corner. The one
  // selected card overlaps two neighbours and carries a doubled frame.

  Element card(int limb, int slot) const {
    const vs::Limb &L = vs::kLimbs[limb];
    const bool sel = limb == vs::kSelected;
    const float w = sel ? 268.0f : 236.0f;
    const float h = sel ? 128.0f : 104.0f;
    const SkPoint at = cardAt[(size_t)slot];

    Element c = box()
                    .absolute()
                    .width(w)
                    .height(h)
                    .centerAt(at)
                    .key(std::string("card") + std::to_string(limb))
                    .zIndex(sel ? 6 : 4)
                    .outline(shapes::chamfered(
                        sel ? 20.0f : 13.0f,
                        sel ? shapes::Corner::All : shapes::Corner::Diagonal))
                    .fill(Fill::color(sel ? vs::rgb(0x101826, 0.86f)
                                          : vs::rgb(0x11141D, 0.72f)));
    c.foreground(decorations::gappedRule(
        1.0f, Fill::color(vs::mul(vs::kBone, 1, sel ? 0.42f : 0.26f)), 22.0f,
        6.0f));
    c.foreground(decorations::brackets(
        sel ? 2.6f : 1.8f, Fill::color(sel ? vs::kCyan : vs::kBone),
        sel ? 30.0f : 20.0f));
    if (sel)
      // An OUTSET bracket set 7 px proud of the plate: the selection reads as
      // a second frame standing off the first, not as a thicker line.
      c.foreground(decorations::brackets(
          3.4f, Fill::color(vs::mul(vs::kCyan, 1, 0.55f)), 13.0f, -7.0f));

    // The right-hand ~90 px is the hit-percentage column (a live custom leaf,
    // see hitReadout) — everything static keeps clear of it by construction
    // rather than by luck.
    const float gutter = sel ? 100.0f : 72.0f;
    // Row 1: the part name.
    c.child(labelAt(L.name, 16, 8, sel ? 24.0f : 20.0f,
                    sel ? vs::kCyan : vs::kBone, 1.4f));
    // Row 2: HP as a hatched bar, not a fill — an engraver reads density.
    const float bw = w - 32 - gutter, bh = 13;
    const float frac = (float)L.hp / (float)L.maxHp;
    c.child(box()
                .absolute()
                .left(16)
                .top(sel ? 48 : 40)
                .width(bw)
                .height(bh)
                .outline(shapes::chamfered(5.0f, shapes::Corner::AntiDiagonal))
                .foreground(lines::Hatch{
                    .strokeFill = Fill::color(vs::mul(vs::kBone, 1, 0.16f)),
                    .spacing = 5.0f,
                    .width = 1.0f,
                    .angleDeg = 45.0f})
                .foreground(decorations::border(
                    1.0f, Fill::color(vs::mul(vs::kBone, 1, 0.34f)))));
    c.child(box()
                .absolute()
                .left(16)
                .top(sel ? 48 : 40)
                .width(vs::snapG(bw * frac))
                .height(bh)
                .outline(shapes::chamfered(5.0f, shapes::Corner::AntiDiagonal))
                .foreground(lines::Hatch{
                    .strokeFill = Fill::color(sel ? vs::kCyan : vs::kBone),
                    .spacing = 3.0f,
                    .width = 1.2f,
                    .angleDeg = 45.0f}));
    // Row 3: hp / maxHp and the armour piece, both struct fields.
    const std::string hpText = std::to_string(L.hp) + "/" +
                               std::to_string(L.maxHp) + "  " +
                               upper(L.armour);
    c.child(labelAt(hpText, 16, sel ? 70 : 60, 15.0f,
                    vs::mul(vs::kBone, 1, 0.78f), 0.6f));
    // Row 4: the condition tier (status.yaml's own five) and the chain-evasion
    // byte with the rate _getChainEvasionModifier derives from it.
    c.child(labelAt(conditionOfStr(limb) + " \xC2\xB7 EVA " +
                        std::to_string(vs::chainEvasionRate(limb)) +
                        "% \xC2\xB7 BYTE " + std::to_string((int)L.chainEvasion),
                    16, sel ? 96 : 80, 13.0f, vs::mul(vs::kBone, 1, 0.50f),
                    0.6f));
    return c;
  }

  static std::string upper(const char *s) {
    std::string r(s);
    for (char &ch : r)
      ch = (char)std::toupper((unsigned char)ch);
    return r;
  }
  static std::string conditionOfStr(int limb) {
    return vs::conditionOf(vs::kLimbs[limb].hp, vs::kLimbs[limb].maxHp);
  }

  /** The live hit readouts: printed and true, side by side on every card, and
   *  the divergence spelled out on the selected one. A custom leaf reading the
   *  RISK Output, so a decaying RISK moves the number with no re-describe. */
  Element hitReadout(int limb, int slot) const {
    const bool sel = limb == vs::kSelected;
    const float w = sel ? 268.0f : 236.0f;
    const float h = sel ? 128.0f : 104.0f;
    const SkPoint at = cardAt[(size_t)slot];
    const vs::PixFont *big = sel ? &fontL : &fontM;
    const vs::PixFont *small = &fontS;
    const ch::Output<float> *r = &risk;
    return box()
        .absolute()
        .width(w)
        .height(h)
        .centerAt(at)
        .key(std::string("hit") + std::to_string(limb))
        .zIndex(sel ? 7 : 5)
        .cache(Cache::None)
        .background(vs::prog([limb, sel, w, big, small, r](SkCanvas &c,
                                                       const PaintContext &) {
          const float rv = r->value();
          const int p = vs::printedHit(limb, rv);
          const int t = vs::trueHit(limb, rv);
          const std::string ps = std::to_string(p) + "%";
          const float pw = vs::widthOf(*big, ps, 1.0f);
          vs::blit(c, *big, w - 16 - pw, sel ? 12.0f : 8.0f, ps,
                   sel ? vs::kCyan : vs::kBone);
          const std::string ts = "TRUE " + std::to_string(t) + "%";
          const float tw = vs::widthOf(*small, ts, 1.0f);
          vs::blit(c, *small, w - 16 - tw, sel ? 56.0f : 42.0f, ts,
                   vs::mul(vs::kAmber, 1, 0.92f));
        }));
  }

  // -------------------------------------------------------------------------
  // THE GAUGES, bleeding off the left edge. HP and MP are hatched bars; RISK
  // encodes its value as HATCH DENSITY inside a lines::dottedCore frame — solid
  // casing, dotted core — and drives two more bind() shapings besides.

  Element gauges() const {
    Element g = box().absolute().inset(0).key("gauges").zIndex(8);
    const float x0 = -54, w = 470, top = 664;
    // The cluster runs off the LEFT edge, plate and all — the gauge is not a
    // card, it is the frame of the screen, so its plate is nearly clear and
    // the wireframe reads straight through it.
    //
    // 248 TALL, NOT 268. At 268 the plate's bottom edge landed on y=888, and
    // marginalia() sets the CHAIN / DEFENSE ability rows at 882 — so a 1 px
    // panel rule ran through the x-height of "CHAIN 14" and straight along the
    // whole pip ladder, at full weight, for the width of the panel. The last
    // thing in the panel is the RISK note at ~845, so 248 keeps 23 px of
    // padding under it and clears the ability block by 14.
    g.child(panel(-70, 620, 512, 248, "gaugepanel", 22.0f,
                  vs::rgb(0x0B0E15, 0.42f)));

    auto bar = [&](float y, const char *name, float frac, SkColor4f col,
                   float dens) {
      Element row = box().absolute().left(x0).top(vs::snapG(y)).width(w)
                        .height(34);
      row.child(box()
                    .absolute()
                    .left(66)
                    .top(6)
                    .width(w - 84)
                    .height(20)
                    .outline(shapes::chamfered(8.0f, shapes::Corner::AntiDiagonal))
                    .foreground(lines::Hatch{
                        .strokeFill = Fill::color(vs::mul(vs::kBone, 1, 0.13f)),
                        .spacing = 6.0f,
                        .width = 1.0f,
                        .angleDeg = 45.0f})
                    .foreground(decorations::border(
                        1.2f, Fill::color(vs::mul(vs::kBone, 1, 0.40f)))));
      row.child(box()
                    .absolute()
                    .left(66)
                    .top(6)
                    .width(vs::snapG((w - 84) * frac))
                    .height(20)
                    .outline(shapes::chamfered(8.0f, shapes::Corner::AntiDiagonal))
                    .foreground(lines::Hatch{.strokeFill = Fill::color(col),
                                             .spacing = dens,
                                             .width = 1.3f,
                                             .angleDeg = 45.0f}));
      row.child(labelAt(name, 74, -14, 15.0f, vs::mul(vs::kBone, 1, 0.66f), 2.0f));
      return row;
    };

    g.child(bar(top, "HP", 0.71f, vs::kBone, 3.0f));
    g.child(labelAt("482/676", 300, top - 14, 15.0f,
                    vs::mul(vs::kBone, 1, 0.66f), 0.8f)
                .zIndex(2));
    g.child(bar(top + 46, "MP", 0.44f, vs::mul(vs::kCyan, 0.9f), 4.0f));
    g.child(labelAt("108/244", 300, top + 32, 15.0f,
                    vs::mul(vs::kBone, 1, 0.66f), 0.8f)
                .zIndex(2));

    // RISK. The track is a lines::dottedCore frame; inside it the hatch
    // DENSITY is the value. Both the needle and the penalty legend are bind()
    // shapings off the same Output that feeds the hatch.
    const float ry = vs::snapG(top + 100);
    Element track = box()
                        .absolute()
                        .left(x0 + 66)
                        .top(ry)
                        .width(w - 84)
                        .height(38)
                        .key("risktrack")
                        .outline(shapes::chamfered(11.0f, shapes::Corner::All));
    // 8.0 px at RISK 0 down to 2.4 px at RISK 100 — the floor is deliberately
    // not 1.6, because a hatch that closes to solid stops being a hatch and
    // the reader loses the one cue the whole widget is built on. The curve is
    // computed into `riskPitch` by the ticker; this binds the stock field.
    track.foreground(lines::Hatch{.strokeFill = Fill::color(vs::kAmber),
                                  .width = 1.3f,
                                  .angleDeg = 45.0f,
                                  .spacingBinding = &riskPitch});
    track.stroke(lines::dottedCore(1.6f, 1.0f,
                                   Fill::color(vs::mul(vs::kBone, 1, 0.55f)),
                                   4.0f, 6.0f));
    g.child(std::move(track));
    g.child(labelAt("RISK", x0 + 74, ry - 26, 15.0f,
                    vs::mul(vs::kAmber, 1, 0.85f), 2.0f));
    g.child(labelAt("DENSITY IS THE VALUE", x0 + 74, ry + 46, 12.0f,
                    vs::mul(vs::kBone, 1, 0.34f), 1.2f));

    // The needle: one bind() shaping, straight to px.
    g.child(box()
                .absolute()
                .left(x0 + 66)
                .top(ry - 9)
                .width(3)
                .height(56)
                .key("riskneedle")
                .fill(Fill::color(vs::kBlood))
                .translateX(bind(&risk).from(0, 100).to(0.0f, w - 87)));
    // The penalty legend: a SECOND shaping off the SAME Output — eased, then
    // remapped into opacity. Raising your own RISK costs you exactly risk% of
    // your accuracy, so this darkens as the number climbs.
    g.child(labelAt("RISK COSTS RISK% ACCURACY \xE2\x80\x94 BOTH SIDES", x0 + 74,
                    ry + 64, 13.0f,
                    vs::kBlood, 1.0f)
                .key("riskpenalty")
                .opacity(bind(&risk).from(0, 100).map(&ch::easeInOutQuad)
                             .to(0.20f, 1.0f)));

    // The live RISK / rate numbers.
    const vs::PixFont *fm = &fontM;
    const vs::PixFont *fs = &fontS;
    const ch::Output<float> *r = &risk;
    g.child(box()
                .absolute()
                .left(x0 + 66)
                .top(ry - 34)
                .width(w - 84)
                .height(32)
                .key("risknum")
                .cache(Cache::None)
                .background(vs::prog([fm, fs, r, w](SkCanvas &c,
                                                const PaintContext &) {
                  const int rv = (int)std::lround(r->value());
                  const std::string s = std::to_string(rv);
                  const float sw = vs::widthOf(*fm, s, 1.0f);
                  vs::blit(c, *fm, w - 84 - sw, 0, s, vs::kAmber);
                  const std::string rate =
                      "RATE " + std::to_string(vs::riskRate((float)rv));
                  const float rw = vs::widthOf(*fs, rate, 1.0f);
                  vs::blit(c, *fs, w - 84 - sw - rw - 14, 8, rate,
                           vs::mul(vs::kBone, 1, 0.55f));
                })));
    return g;
  }

  // -------------------------------------------------------------------------
  // THE ATTACK-SHAPE DIAL. The 5-bit field read as an instrument: 32 divisions
  // at 11.25 deg, every eighth long, the current 12/32 sector shaded, and the
  // four cardinal divisions numbered with TextPath::Orient::Radial — the type
  // radiates like a spoke, the way an astrolabe limb numbers its divisions,
  // which is one node per numeral instead of one rotated Element per glyph.
  //
  // Radial, not Upright, and on purpose: 24 at the bottom of this dial comes
  // out upside down, which is what an engraved instrument does — you turn it
  // to read the division you are on. TextPath's own docs make the same point
  // about Nightingale's 1858 plate. Upright is the modern-gauge convention and
  // would be the wrong register for a drawing that is otherwise all drafting.

  Element shapeDial() const {
    const SkPoint at{188, 448};
    const float r = 88;
    Element g = box().absolute().inset(0).key("dial").zIndex(8);

    const float sweep = (float)vs::kAttackShapeAngle * vs::kAngleStep;
    g.child(box()
                .absolute()
                .width(r * 1.5f)
                .height(r * 1.5f)
                .centerAt(at)
                .key("dialsector")
                .outline(shapes::sector(-90.0f - sweep * 0.5f, sweep))
                .fill(Fill::color(vs::rgb(0x7FD8E8, 0.05f)))
                .overlay(lines::Hatch{
                    .strokeFill = Fill::color(vs::mul(vs::kCyan, 1, 0.30f)),
                    .spacing = 5.0f, .width = 1.0f, .angleDeg = 45.0f})
                .stroke(PathFormat{
                    .width = 1.2f,
                    .strokeFill = Fill::color(vs::mul(vs::kCyan, 1, 0.75f))}));
    g.child(box()
                .absolute()
                .width(r * 2)
                .height(r * 2)
                .centerAt(at)
                .key("dial32")
                .outline(shapes::annulus(0.88f))
                .foreground(lines::radialHatch(
                    Fill::color(vs::mul(vs::kBone, 1, 0.60f)), 32, 1.4f,
                    {0.5f, 0.5f})));
    g.child(box()
                .absolute()
                .width(r * 2)
                .height(r * 2)
                .centerAt(at)
                .key("dial4")
                .outline(shapes::annulus(0.70f))
                .foreground(lines::radialHatch(
                    Fill::color(vs::mul(vs::kBone, 1, 0.88f)), 4, 2.2f,
                    {0.5f, 0.5f})));
    g.child(box()
                .absolute()
                .width(r * 2)
                .height(r * 2)
                .centerAt(at)
                .key("dialrim")
                .outline(shapes::circle())
                .stroke(lines::cased(1.4f, Fill::color(vs::mul(vs::kBone, 1,
                                                               0.55f)),
                                     5.0f)));
    // The four numbered divisions, radiating.
    static constexpr int kMark[4] = {0, 8, 16, 24};
    for (int i = 0; i < 4; ++i) {
      const float deg = -90.0f + (float)kMark[i] * vs::kAngleStep;
      float frac = std::fmod(deg + 360.0f, 360.0f) / 360.0f;
      g.child(label(std::to_string(kMark[i]), 17.0f,
                    vs::mul(vs::kBone, 1, 0.86f), 1.0f)
                  .absolute()
                  .width(r * 2)
                  .height(r * 2)
                  .centerAt(at)
                  .key(std::string("dialnum") + std::to_string(i))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = frac,
                                   .align = TextPath::Align::Center,
                                   .offset = -25.0f,
                                   .orient = TextPath::Orient::Radial}));
    }
    g.child(labelAt("ATTACK SHAPE " + std::to_string(vs::kAttackShapeAngle) +
                        "/32 \xC2\xB7 11.25\xC2\xB0 STEP",
                    at.fX - 96, at.fY + r + 14, 13.0f,
                    vs::mul(vs::kCyan, 1, 0.80f), 1.2f));
    return g;
  }

  // -------------------------------------------------------------------------
  // THE RIGHT MARGIN: the ten defence rows of status.yaml for the selected
  // limb, each a hatch-density strip, plus the four damage types.

  Element defenceLadder() const {
    Element g = box().absolute().inset(0).key("defence").zIndex(8);
    const float x = 1024, y0 = 92;
    g.child(labelAt("DEFENCE / R.ARM", x, y0 - 34, 15.0f,
                    vs::mul(vs::kBone, 1, 0.70f), 1.6f));
    g.child(box()
                .absolute()
                .left(x)
                .top(vs::snapG(y0 - 12))
                .width(192)
                .height(2)
                .key("defrule")
                .outline([](SkSize s) {
                  SkPathBuilder p;
                  p.moveTo(0, 1);
                  p.lineTo(s.width(), 1);
                  return p.detach();
                })
                .stroke(lines::heavyHairHeavy(
                    1.6f, 0.6f, Fill::color(vs::mul(vs::kBone, 1, 0.55f)),
                    3.0f)));
    const vs::Limb &L = vs::kLimbs[vs::kSelected];
    for (int i = 0; i < 10; ++i) {
      const int v = vs::defenceValue(L, i);
      const float y = vs::snapG(y0 + (float)i * 23.0f);
      g.child(labelAt(vs::kDefenceRows[i], x, y, 13.0f,
                      vs::mul(vs::kBone, 1, 0.62f), 0.8f));
      const float mag = std::clamp(std::abs((float)v) / 26.0f, 0.06f, 1.0f);
      g.child(box()
                  .absolute()
                  .left(vs::snapG(x + 104))
                  .top(y + 2)
                  .width(vs::snapG(20 + 72 * mag))
                  .height(12)
                  .key(std::string("def") + std::to_string(i))
                  .outline(shapes::chamfered(4.0f, shapes::Corner::AntiDiagonal))
                  .foreground(lines::Hatch{
                      .strokeFill = Fill::color(v < 0 ? vs::kBlood : vs::kBone),
                      .spacing = 8.0f - 6.0f * mag,
                      .width = 1.0f,
                      .angleDeg = v < 0 ? 135.0f : 45.0f}));
    }
    return g;
  }

  // -------------------------------------------------------------------------
  // THE TITLE BLOCK. The plate number, the two grids, the bit fields, and the
  // cheat printed as a divergence — the way xcom_battlescape printed the
  // picker's thumb on the scale.

  /** VS's chrome is translucent slate under everything it has to say. A panel
   *  here is a chamfered plate, a rule that stops short of every corner, and
   *  four brackets — never a rounded rect, and never an opaque box: the sphere
   *  reads THROUGH it, which is the whole reason the game's HUD works. */
  Element panel(float x, float y, float w, float h, const char *key,
                float cut = 18.0f, SkColor4f tint = vs::rgb(0x0B0E15, 0.62f))
      const {
    return box()
        .absolute()
        .left(vs::snapG(x))
        .top(vs::snapG(y))
        .width(vs::snapG(w))
        .height(vs::snapG(h))
        .key(key)
        .outline(shapes::chamfered(cut, shapes::Corner::All))
        .fill(Fill::color(tint))
        .foreground(decorations::gappedRule(
            1.0f, Fill::color(vs::mul(vs::kBone, 1, 0.26f)), 34.0f, 7.0f))
        .foreground(decorations::weightedCorners(
            1.2f, 2.6f, Fill::color(vs::mul(vs::kBone, 1, 0.62f)), 26.0f));
  }

  Element titleBlock() const {
    Element g = box().absolute().inset(0).key("title").zIndex(9);
    g.child(panel(24, 20, 600, 200, "titlepanel", 26.0f));
    g.child(labelAt("BATTLE MODE \xE2\x80\x94 TARGET SELECT", 40, 30, 24.0f,
                    vs::kBone, 2.6f));
    g.child(box()
                .absolute()
                .left(40)
                .top(66)
                .width(556)
                .height(3)
                .key("titlerule")
                .outline([](SkSize s) {
                  SkPathBuilder p;
                  p.moveTo(0, 1.5f);
                  p.lineTo(s.width(), 1.5f);
                  return p.detach();
                })
                .stroke(lines::Rails{
                    .rails = {{.offset = 0, .width = 2.0f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.85f))},
                              {.offset = 5.0f, .width = 0.8f,
                               .fill = Fill::color(vs::mul(vs::kBone, 1, 0.35f)),
                               .dash = {14.0f, 5.0f, 3.0f, 5.0f}}}}));
    const std::string cls = vs::kClassNames[vs::kEnemyClass];
    g.child(labelAt("DULLAHAN / CLASS " + cls + "  \xC2\xB7  ENEMYCLASS:3", 40,
                    78, 15.0f, vs::mul(vs::kBone, 1, 0.62f), 1.0f));
    g.child(labelAt("REACH:5 = " + std::to_string(vs::kReach) +
                        "   SHAPEANGLE:5 = " +
                        std::to_string(vs::kAttackShapeAngle) + " x 11.25\xC2\xB0 = " +
                        std::to_string((int)(vs::kAttackShapeAngle * 11.25f)) +
                        "\xC2\xB0",
                    40, 98, 15.0f, vs::mul(vs::kCyan, 1, 0.72f), 1.0f));
    g.child(labelAt("WEAPON " + std::string(vs::kWeaponName) + " DRAWN", 40, 118,
                    15.0f, vs::mul(vs::kBone, 1, 0.52f), 1.0f));

    // The divergence, live: both numbers and the sentence that explains them.
    const vs::PixFont *fs = &fontS;
    const vs::PixFont *fm = &fontM;
    const ch::Output<float> *r = &risk;
    g.child(box()
                .absolute()
                .left(40)
                .top(142)
                .width(590)
                .height(74)
                .key("divergence")
                .cache(Cache::None)
                .background(vs::prog([fs, fm, r](SkCanvas &c, const PaintContext &) {
                  const float rv = r->value();
                  const int p = vs::printedHit(vs::kSelected, rv);
                  const int t = vs::trueHit(vs::kSelected, rv);
                  float x = 0;
                  x += vs::blit(c, *fs, x, 4, "R.ARM PRINTED ",
                                vs::mul(vs::kBone, 1, 0.60f));
                  x += vs::blit(c, *fm, x, 0, std::to_string(p) + "%",
                                vs::kBone);
                  x += 14;
                  x += vs::blit(c, *fs, x, 4, "ROLLED AGAINST ",
                                vs::mul(vs::kBone, 1, 0.60f));
                  vs::blit(c, *fm, x, 0, std::to_string(t) + "%", vs::kAmber);
                  vs::blit(c, *fs, 0, 26,
                           "146C.c:7334  if (source->actorId == 0) threshold += 10;",
                           vs::mul(vs::kAmber, 1, 0.72f));
                  vs::blit(c, *fs, 0, 44,
                           "THE +10 LANDS AFTER THE CLAMP TO 100. A PRINTED 100 IS A TRUE 110.",
                           vs::mul(vs::kBone, 1, 0.45f));
                })));
    return g;
  }

  // -------------------------------------------------------------------------
  // MARGINALIA: registration marks, the plate number, the two-grid note, and
  // the chain / defense ability slots (14 of each, battleAbilities.yaml).

  Element marginalia() const {
    Element g = box().absolute().inset(0).key("margin").zIndex(9);
    // Registration marks: four bracket sets on a chamfered full-bleed box.
    g.child(box()
                .absolute()
                .left(20)
                .top(20)
                .width(vs::kW - 40)
                .height(vs::kH - 40)
                .key("reg")
                .outline(shapes::chamfered(34.0f, shapes::Corner::All))
                .foreground(decorations::brackets(
                    1.6f, Fill::color(vs::mul(vs::kBone, 1, 0.50f)), 46.0f))
                .foreground(decorations::gappedRule(
                    1.0f, Fill::color(vs::mul(vs::kBone, 1, 0.14f)), 120.0f)));
    g.child(labelAt("PLATE II \xC2\xB7 320x240 POLY GRID / 512x240 TEXT GRID", 880,
                    vs::kH - 60, 13.0f, vs::mul(vs::kBone, 1, 0.42f), 1.2f));
    g.child(labelAt("SER-POUNCE / ROOD-REVERSE \xC2\xB7 BATTLE.PRG 146C", 880,
                    vs::kH - 36, 13.0f, vs::mul(vs::kBone, 1, 0.30f), 1.2f));

    // battleAbilities.yaml: chainAbilityEffect0..13 and defenseAbilityEffect
    // 0..13 — fourteen of each, so fourteen ticks of each, lit ones filled.
    const float ax = 30, ay = 902;
    for (int k = 0; k < 2; ++k) {
      g.child(labelAt(k == 0 ? "CHAIN 14" : "DEFENSE 14", ax,
                      ay + (float)k * 30.0f - 20.0f, 13.0f,
                      vs::mul(vs::kBone, 1, 0.55f), 1.0f));
      for (int i = 0; i < 14; ++i) {
        const bool lit = k == 0 ? (i < 5) : (i < 3);
        g.child(box()
                    .absolute()
                    .left(vs::snapG(ax + 116 + (float)i * 15.0f))
                    .top(vs::snapG(ay + (float)k * 30.0f - 20.0f))
                    .width(9)
                    .height(lit ? 20.0f : 11.0f)
                    .key(std::string("ab") + std::to_string(k * 14 + i))
                    .fill(Fill::color(lit ? vs::mul(vs::kAmber, 1, 0.9f)
                                          : vs::mul(vs::kBone, 1, 0.22f))));
      }
    }
    return g;
  }

  // -------------------------------------------------------------------------
  // THE CHAIN PROMPT. A timing ring centred on the impact point, sweeping
  // 0 -> 360 deg over 400 ms: shapes::arc trimmed by a bound Output, over
  // everything. The damage number pops beside it on ops::Wave — the one place
  // a displaced rule is allowed on this canvas.

  Element chainPrompt(float spin) const {
    const SkPoint impact = limbPoint(vs::kSelected, spin);
    const float r = 56;
    Element g = box().absolute().inset(0).key("chain").zIndex(11);
    g.child(box()
                .absolute()
                .width(r * 2)
                .height(r * 2)
                .centerAt(impact)
                .key("chainring")
                .outline(shapes::arc(-90.0f, 359.9f))
                .trim(0.0f, bind(&chainSweep).clamp(0.0f, 1.0f))
                // A Brush with a per-LEG ops::Offset: the bright body on the
                // route, a counter-dashed strand 6 px outside it. Two legs,
                // one route, one value.
                .stroke(Brush{}
                            .leg(lines::Line{.width = 3.2f,
                                             .fill = Fill::color(vs::kCyan)})
                            .leg(lines::Line{.width = 1.0f,
                                             .fill = Fill::color(vs::mul(
                                                 vs::kCyan, 1, 0.45f)),
                                             .dashIntervals = {2.0f, 6.0f}},
                                 {ops::Offset{.px = 6.0f, .step = 2.0f}})));
    g.child(box()
                .absolute()
                .width(r * 2.5f)
                .height(r * 2.5f)
                .centerAt(impact)
                .key("chainticks")
                .outline(shapes::annulus(0.88f))
                .opacity(bind(&chainSweep).to(0.15f, 0.85f))
                .foreground(lines::radialHatch(
                    Fill::color(vs::mul(vs::kCyan, 1, 0.6f)), 16, 1.2f)));
    // The damage pop: the only ops::Wave on this canvas.
    Brush wavy;
    wavy.op(ops::Wave{.amplitude = 2.6f, .wavelength = 26.0f});
    wavy.leg(lines::Line{.width = 1.6f,
                         .fill = Fill::color(vs::mul(vs::kAmber, 1, 0.85f))});
    g.child(box()
                .absolute()
                .left(vs::snapG(impact.fX + 62))
                .top(vs::snapG(impact.fY - 96))
                .width(150)
                .height(3)
                .key("popRule")
                .outline([](SkSize s) {
                  SkPathBuilder p;
                  p.moveTo(0, 1.5f);
                  p.lineTo(s.width(), 1.5f);
                  return p.detach();
                })
                .stroke(std::move(wavy))
                .translateY(bind(&damagePop).to(0.0f, -26.0f))
                .opacity(bind(&damagePop).invert().clamp(0.0f, 1.0f)));
    g.child(labelAt("74", impact.fX + 74, impact.fY - 132, 34.0f, vs::kAmber,
                    2.0f)
                .key("popNum")
                .translateY(bind(&damagePop).to(0.0f, -26.0f))
                .opacity(bind(&damagePop).invert().clamp(0.0f, 1.0f)));
    return g;
  }

  // -------------------------------------------------------------------------

  static sk_sp<SkTypeface> pickFace() {
    sk_sp<SkFontMgr> mgr = weave::ports::systemFontManager();
    if (!mgr)
      return nullptr;
    for (const char *n : {"Charter", "Palatino", "Times New Roman", "Georgia",
                          "Baskerville", "Helvetica"})
      if (sk_sp<SkTypeface> f = mgr->matchFamilyStyle(n, SkFontStyle::Normal()))
        return f;
    return mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  }

  /** THE PANELS ARE PLACED BY THE BODY, NOT BY A MARGIN.
   *
   *  Each readout hangs off its own limb at the BEARING that limb has about
   *  the torso and at a radius no two of them share: HEAD's panel is above the
   *  skull, the arms' panels are out to either side, the legs' are below. The
   *  bearings are re-spaced into one 204-degree fan opening away from the
   *  title and gauge furniture, and re-spacing PRESERVES ORDER — which is
   *  what guarantees six leaders out of one cluster cannot cross.
   *
   *  So the composition is a fan around a figure, and the leader lines exist
   *  because the panel genuinely is somewhere else from the part. A right-hand
   *  column would need no leaders at all, which is how you know it is wrong. */
  void layoutCards() {
    const SkPoint torso = limbPoint(1, 0);
    std::array<float, 6> bear{};
    for (int i = 0; i < 6; ++i) {
      const SkPoint p = limbPoint(i, 0);
      float dx = p.fX - torso.fX, dy = p.fY - torso.fY;
      if (i == 1) {           // the torso's bearing about itself is undefined;
        dx = 1.0f;            // it takes the beam, straight out to the right
        dy = 0.0f;
      }
      bear[(size_t)i] = std::atan2(dy, dx);
    }
    std::array<int, 6> order{0, 1, 2, 3, 4, 5};
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return bear[(size_t)a] < bear[(size_t)b]; });
    // Radii by FAN POSITION, deliberately unequal and non-monotonic — two
    // neighbours at the same distance would read as a row again.
    //
    // FAN POSITION 4 IS 372, NOT 320. The fan steps 40.8 degrees, so two
    // adjacent panels sitting at ~310 are only 216 px apart centre to centre
    // and the panel is 236 WIDE: L.LEG (k4, 320) and R.LEG (k5, 300) were the
    // one pair close enough to interpenetrate, 24 px of frame into 56 px of
    // frame, and L.LEG's left bracket struck clean through R.LEG's "TRUE 100%"
    // readout. Every other adjacent pair clears — k0/k1 by only 10 px in y,
    // which is how narrow the margin is here. 372 puts L.LEG's left edge 8 px
    // clear of R.LEG's right and keeps it well above BODY's row at k3.
    static constexpr float kRadius[6] = {268, 392, 300, 430, 372, 300};
    constexpr float kFrom = -112.0f * 0.017453293f;
    constexpr float kTo = 92.0f * 0.017453293f;
    for (int k = 0; k < 6; ++k) {
      const float a = kFrom + (kTo - kFrom) * ((float)k / 5.0f);
      const int limb = order[(size_t)k];
      cardAt[(size_t)limb] = {
          vs::snapG(torso.fX + kRadius[k] * std::cos(a)),
          vs::snapG(torso.fY + kRadius[k] * std::sin(a))};
      slotOf[(size_t)limb] = limb;   // the panel IS the limb's; no slot table
    }
  }

  Element worldGroup(float spin) const {
    Element g = box().absolute().inset(0);
    g.child(sphereGroup(spin).staggerChildren(24ms));
    g.child(limbBody(spin).zIndex(2));
    g.child(leaders(spin).zIndex(3));
    g.child(chainPrompt(spin).zIndex(4));
    return g;
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(vs::kW, vs::kH);
    ctx.background(vs::kVoid);

    face = pickFace();
    if (ctx.fonts) {
      fontS = vs::bakeFont(*ctx.fonts, face, 13.0f, 0.86f);
      fontM = vs::bakeFont(*ctx.fonts, face, 19.0f, 0.86f);
      fontL = vs::bakeFont(*ctx.fonts, face, 34.0f, 0.86f);
    }

    // The camera. Ashley sits at (0.30, 0.66) of frame, so the sphere bleeds
    // off the LEFT and BOTTOM edges the way the game's does.
    cam.f = 1700.0f;
    cam.R = 372.0f;
    cam.pitchDeg = 22.0f;
    const SkPoint anchor{vs::snapG(0.32f * vs::kW), vs::snapG(0.62f * vs::kH)};
    cam.C = {(anchor.fX - vs::kW * 0.5f) * 1400.0f / cam.f,
             (anchor.fY - vs::kH * 0.5f) * 1400.0f / cam.f, 1400.0f};
    sphereCentre = cam.centre();
    halfExtent = cam.apparentRadius() * 1.06f;
    layoutCards();

    ringArt = ringTile();
    dashArt = dashTile();
    tickArt = tickTile();
    leaderBrush.side = dashArt;
    leaderBrush.start = ringArt;
    leaderBrush.end = tickArt;
    leaderBrush.advance = 15.0f;
    leaderBrush.stretchToFit = true;
    leaderBrush.reach = 26.0f;

    // ---- motion ----------------------------------------------------------
    ctx.ticker.add([this](double dt) {
      clock += dt;
      psi = (float)(clock * 6.0 * 0.017453293);   // 6 deg/s, the brief's rate
      // RISK: spikes on every swing, then decays — slower with the weapon
      // drawn. One Output, three consumers (hatch density, needle, legend).
      const double beat = std::fmod(clock, 6.0);
      risk = (float)std::clamp(24.0 + 62.0 * std::exp(-beat * 0.55), 0.0, 100.0);
      // the gauge hatch's PITCH, not its value: 8.0 px at RISK 0 closing to
      // 2.4 at RISK 100. Hatch::spacingBinding reads a pitch straight, so the
      // mapping lives here rather than inside a bespoke decoration.
      riskPitch = 8.0f + (2.4f - 8.0f) *
                             std::clamp(risk.value() / 100.0f, 0.0f, 1.0f);
      // The chain prompt's 400 ms sweep, once per beat.
      chainSweep = (float)std::clamp(beat / 0.4, 0.0, 1.0);
      select = (float)(0.5 + 0.5 * std::sin(clock * 3.4));
      damagePop = (float)std::clamp((beat - 2.0) / 1.2, 0.0, 1.0);
      return true;
    });

    // ---- the tree --------------------------------------------------------
    Element root = box().absolute().inset(0);

    // 0. the plate: a slate wash with grain, baked to PIXELS. A full-canvas
    //    generated material is a shader, and a picture replays the draw call,
    //    not the result — this is the Cache::Texture that keeps it free.
    root.child(box()
                   .absolute()
                   .inset(0)
                   .key("plate")
                   .cache(Cache::Texture)
                   .fill(Material::blend(
                       {{Material::radialUnit({0.32f, 0.62f}, 1.1f,
                                              {{0.0f, vs::rgb(0x141A26)},
                                               {0.62f, vs::rgb(0x0C0F16)},
                                               {1.0f, vs::rgb(0x070910)}}),
                         SkBlendMode::kSrcOver},
                        {patterns::grain(1.9f, 3, 11), SkBlendMode::kOverlay}}))
                   .overlay(decorations::wash(Material::solid(vs::kSlate),
                                              SkBlendMode::kSrcOver, 0.42f)));

    // 1. the ground plate, then Ashley over it (the wedge runs UNDER him,
    //    which is what makes it read as ground rather than as a decal).
    root.child(groundPlate().zIndex(1));
    root.child(ashley().zIndex(2));

    // 2. the moving world — sphere, enemy, limb rings, leaders. Re-described
    //    every frame through ONE slot; everything else keeps its caches.
    root.child(slot("world").zIndex(3));
    root.child(silhouette().zIndex(4));

    // 3. the cards, then their live readouts over them.
    for (int i = 0; i < 6; ++i)
      root.child(card(i, slotOf[(size_t)i]));
    for (int i = 0; i < 6; ++i)
      root.child(hitReadout(i, slotOf[(size_t)i]));

    // 4. chrome.
    root.child(gauges());
    root.child(shapeDial());
    root.child(defenceLadder());
    root.child(titleBlock());
    root.child(marginalia());

    ctx.composer.render(std::move(root));
    ctx.composer.renderSlot("world", worldGroup(psi));
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    (void)elapsed;
    // The derive step, across frames: read the cards' resolved rects off the
    // composer and let THEM decide where the leaders land. Information flows
    // forward within a frame, so last frame's layout feeds this frame's
    // description — the loop the API documents.
    for (int i = 0; i < 6; ++i)
      if (std::optional<SkRect> r =
              ctx.composer.bounds(std::string("card") + std::to_string(i)))
        cardBounds[(size_t)i] = *r;
    ctx.composer.renderSlot("world", worldGroup(psi));
  }
};

SIGIL_SKETCH(VagrantStoryTarget)
