#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;

namespace field = sigil::material::field;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::material::skia::Paint;
namespace ch = choreograph;

namespace fo {

// ---------------------------------------------------------------------------
// THE UNIT MAP. ORIGINAL 640x480 px -> canvas px. Nothing else scales
// anything, and every number below the palette is in the artefact's own
// pixels: divide any canvas number by the scale to recover the 1998 pixel.
//
// A LENGTH IS NOT A POSITION. `s()` takes no origin and `x()`/`y()` do, so
// a width run through a position map is the one arithmetic error this
// sheet's 200 placements cannot make.

constexpr path::Grid kUnits{.scale = 2.0f};
constexpr float kScale = kUnits.scale;
constexpr float n(float v) { return kUnits.s(v); }
constexpr float kScreenW = n(640), kScreenH = n(480);
constexpr float kCaptionH = 128.0f;

// ---------------------------------------------------------------------------
// PALETTE. Left column = what the code REQUESTS through _colorTable; right =
// what the 6-bit VGA palette actually delivered, sampled off the lossless PNG.
// The gap is the finding (see header).

constexpr SkColor4f kGreen =
    hexColor(0x3CF800);  // _colorTable[992],  req #00FF00
constexpr SkColor4f kSelected =
    hexColor(0xFCFC7C);  // _colorTable[32747], req #F8F858
constexpr SkColor4f kInactive =
    hexColor(0x183018);  // _colorTable[1313],  req #084808
constexpr SkColor4f kTagged = hexColor(0xA0A0A0);  // _colorTable[21140], exact
constexpr SkColor4f kGold =
    hexColor(0x907824);  // _colorTable[18979], req #908818
constexpr SkColor4f kGoldDim = hexColor(0x7C6818);  // engraving shadow
constexpr SkColor4f kInk = hexColor(0x000000);  // ALL card text and its rule

constexpr SkColor4f kWell = hexColor(0x040C00);   // inset interior: near-black,
                                                  // GREEN-cast, not pure black
constexpr SkColor4f kPlate = hexColor(0x383020);  // metal plate, base olive
constexpr SkColor4f kPlateLit = hexColor(0x483828);   // lit facet
constexpr SkColor4f kPlateDark = hexColor(0x302820);  // shadowed facet
constexpr SkColor4f kRust = hexColor(0x7C581C);
constexpr SkColor4f kParch = hexColor(0x9C7434);  // parchment base ochre
constexpr SkColor4f kParchLit = hexColor(0xAC8044);
constexpr SkColor4f kParchLit2 = hexColor(0xBC9054);
constexpr SkColor4f kParchDark = hexColor(0x8C6428);
constexpr SkColor4f kParchScuff = hexColor(0x947C60);
constexpr SkColor4f kLampOff = hexColor(0x580000);
constexpr SkColor4f kLampOn = hexColor(0xF80000);  // _colorTable[31744]
constexpr SkColor4f kDigit = hexColor(0xFFFFFF);  // the odometer sprite sheet's
                                                  // first half (the second half
                                                  // is red, for stats above 10)

// ---------------------------------------------------------------------------
// GEOMETRY. Source tag: (code) = character_editor.cc; (measured) = scanned off
// the lossless capture lp_1-FO2_1_2.png. Where they disagree, measured wins
// and the difference is noted.

// The five inset wells (measured bboxes of the near-black regions).
struct Rect {
  float x, y, w, h;
};
constexpr Rect kWellStatus{188, 37, 130, 118};    // code: (194,46) 118x108
constexpr Rect kWellDerived{188, 171, 130, 143};  // code: (194,179) 116x130
constexpr Rect kWellLevel{25, 275, 132, 40};      // code: (32,280) 124x32
constexpr Rect kWellSkills{368, 23, 249, 202};    // code: (370,27) region
constexpr Rect kWellFolder{25, 359, 291, 112};    // code list body 34..314
constexpr Rect kWellSpecial{5, 34, 152, 232};     // the S.P.E.C.I.A.L. column

// S.P.E.C.I.A.L. rows (code, gCharacterEditorPrimaryStatY) — pitch 33.
constexpr std::array<float, 7> kStatY{37, 70, 103, 136, 169, 202, 235};
constexpr float kAbbrX = 20;  // measured ink 20..55, 21 px tall
constexpr float kOdoX = 58;   // code; cell 14 x 24
constexpr float kOdoW = 14, kOdoH = 24;
constexpr float kPlaqueX = 100, kPlaqueW = 58;  // measured 100..157
constexpr float kDescX = 103;                   // code (x = 103, y = row + 8)

constexpr float kRowPitch13 = 13;  // fontGetLineHeight(101) + 3
constexpr float kRowPitch11 = 11;  // fontGetLineHeight(101) + 1

// ---------------------------------------------------------------------------
// TYPE. Fallout sets everything in 8-bit bitmap faces at 10 px and 28 px.
// Substitutes are resolved through the system font manager with a fallback
// chain; the BODY size is derived from a MEASURED advance rather than assumed,
// because the whole reason the game's fixed value columns work is that its
// small font is near-monospaced.

/** The one shape this sheet asks a face in: a family list at a weight and
 *  a width, held for the process by the port that answers it. */
inline sk_sp<SkTypeface> sheetFace(std::initializer_list<const char*> families,
                                   int weight, SkFontStyle::Width width) {
  return weave::ports::face(
      families, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
}
/** font 101 substitute — a squarish PROPORTIONAL face.
 *
 *  font101.aaf is proportional: `fontGetStringWidth()` sums glyphWidth +
 *  letterSpacing per glyph, and no monospace can match that. A monospace
 *  substitute runs every label column about forty per cent wide — "Energy
 *  Weapons" stops short of the middle of the card in the capture and
 *  nearly reaches it when every glyph takes an M's advance — and that
 *  changes the whole density of the sheet, which is the one thing a
 *  reconstruction of a LAYOUT cannot afford. It costs nothing in geometry:
 *  every value column is at an absolute x, so a shorter label simply ends
 *  sooner. */
inline sk_sp<SkTypeface> bodyFace() {
  return sheetFace({"Verdana", "DejaVu Sans", "Helvetica"},
                   SkFontStyle::kNormal_Weight, SkFontStyle::kNormal_Width);
}
inline sk_sp<SkTypeface> bodyBold() {
  return sheetFace({"Verdana", "DejaVu Sans", "Helvetica"},
                   SkFontStyle::kBold_Weight, SkFontStyle::kNormal_Width);
}
/** The engraved gold: plaques, tabs, buttons, S.P.E.C.I.A.L. caps. */
inline sk_sp<SkTypeface> engraved() {
  return sheetFace({"Impact", "Haettenschweiler", "Copperplate"},
                   SkFontStyle::kNormal_Weight, SkFontStyle::kCondensed_Width);
}
/** font 102 substitute — the card title, a condensed heavy grotesque. */
inline sk_sp<SkTypeface> titleFace() {
  return sheetFace({"Helvetica Neue", "Impact", "Helvetica"},
                   SkFontStyle::kBlack_Weight, SkFontStyle::kCondensed_Width);
}
/** The odometer digits — tabular figures on a hard 14 px cell. */
inline sk_sp<SkTypeface> digitFace() {
  return sheetFace({"Helvetica Neue", "Menlo", "Helvetica"},
                   SkFontStyle::kBold_Weight, SkFontStyle::kCondensed_Width);
}

// A positional shorthand over weave's designated-init `textStyle()`: the
// sheet has one type signature and names its own five parameters over it.
inline weave::TextStyle sheetType(const sk_sp<SkTypeface>& tf, float size,
                                  SkColor4f color, float track = 0,
                                  float condense = 1.0f) {
  return weave::textStyle({.face = tf,
                           .size = size,
                           .color = color,
                           .track = track,
                           .condense = condense});
}

/** Original px the body face advances per character, ON AVERAGE.
 *
 *  Measured off the capture's ten derived-stat labels: 752 px of ink over
 *  119 characters, i.e. 6.32 px per character. With a proportional
 *  substitute that is what it should be — a MEAN, not a cell — and the
 *  size is solved so the same sample string measures that width. The
 *  figure sits a little under the measurement because the substitute's
 *  own letterforms are wider than font101's at the same mean: 5.80 is
 *  where "Critical Chance" clears its value column at x = 288 and the
 *  skill rows clear the +/- buttons, which is the constraint the
 *  measurement has to survive. */
constexpr float kBodyAdvance = 5.80f;

inline Element t(const std::string& s, weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}
/** Place at a DOCUMENTED (x, y) in original screen px. `y` is Fallout's draw
 *  y — the top of the glyph cell — so the rise correction lands here, once. */
inline Element ink(Element e, float x, float y, float rise) {
  e.left(Dim(kUnits.x(x))).top(Dim(kUnits.y(y) - rise));
  return e;
}
/** Absolute placement in the SHEET'S OWN pixels — the numbers read off
 *  the capture — through the unit map on the way to the canvas. */
inline Element at(Element e, float x, float y, float w, float h) {
  return kit::at(std::move(e), kUnits.x(x), kUnits.y(y), kUnits.s(w),
                 kUnits.s(h));
}
inline Element atR(Element e, Rect r) {
  return at(std::move(e), r.x, r.y, r.w, r.h);
}

// ---------------------------------------------------------------------------
// THE MECHANISM — stat.cc / skill.cc / trait.cc, transcribed.

enum Stat { ST = 0, PE, EN, CH, IN, AG, LK };

struct Special {
  std::array<int, 7> v{};
  int operator[](int i) const { return v[(size_t)i]; }
};

struct Traits {
  bool gifted = false;       // +1 every stat, -10 EVERY skill, -5 skill pts/lvl
  bool heavyHanded = false;  // +4 melee damage
  bool smallFrame = false;   // +1 AG, carry weight -10*ST
  bool bruiser = false;      // +2 ST, -2 action points
  bool skilled = false;      // +5 skill points/level, a perk every 4 levels
};

/** critterUpdateDerivedStats, stat.cc:555. All integer arithmetic. */
struct Derived {
  int hitPoints, actionPoints, armorClass, meleeDamage, carryWeight;
  int sequence, healingRate, criticalChance;
  int damageResist, poisonResist, radResist;
};
inline Derived derive(const Special& s, const Traits& tr, int level,
                      int toughnessRank) {
  Derived d{};
  d.hitPoints = s[ST] + 2 * s[EN] + 15;
  // hitPointsPerLevel = EN/2 + 2 (+4 per rank of Lifegiver), stat.cc:~770
  d.hitPoints += (level - 1) * (s[EN] / 2 + 2);
  d.actionPoints = s[AG] / 2 + 5 - (tr.bruiser ? 2 : 0);
  d.armorClass = s[AG];
  d.meleeDamage = std::max(s[ST] - 5, 1) + (tr.heavyHanded ? 4 : 0);
  d.carryWeight = 25 * s[ST] + 25 - (tr.smallFrame ? 10 * s[ST] : 0);
  d.sequence = 2 * s[PE];
  d.healingRate = std::max(s[EN] / 3, 1);
  d.criticalChance = s[LK];
  d.damageResist = 0 + 10 * toughnessRank;  // perk.msg {1113}: +10% general DR
  d.poisonResist = 5 * s[EN];
  d.radResist = 2 * s[EN];
  return d;
}

/** pcGetExperienceForLevel, stat.cc:654 — written as two integer-division
 *  branches in the decompilation to dodge a divide; it is 1000*T(L-1). */
inline int experienceForLevel(int L) { return 1000 * L * (L - 1) / 2; }

/** gSkillDescriptions, skill.cc:76ff. `formula` is what the game PRINTS on
 *  the card beside the skill's name (skill.msg 300-317) — the UI shows its
 *  own maths, so the same row drives both the number and the caption. */
struct SkillDef {
  const char* name;
  int base;
  int mult;
  int s1;
  int s2;  // -1 = single-stat
  const char* formula;
  const char* blurb;  // skill.msg 200-217, verbatim
};
inline const std::array<SkillDef, 18>& skills() {
  static const std::array<SkillDef, 18> v = {{
      {"Small Guns", 5, 4, AG, -1, "+ (4 x AG)",
       "The use, care and general knowledge of small firearms - pistols, SMGs "
       "and rifles."},
      {"Big Guns", 0, 2, AG, -1, "+ (2 x AG)",
       "The operation and maintenance of really big guns - miniguns, rocket "
       "launchers, flamethrowers and such."},
      {"Energy Weapons", 0, 2, AG, -1, "+ (2 x AG)",
       "The care and feeding of energy-based weapons. How to arm and operate "
       "weapons that use laser or plasma technology."},
      {"Unarmed", 30, 2, AG, ST, "+ (2 x (AG + ST))",
       "A combination of martial arts, boxing and other hand-to-hand martial "
       "arts. Combat with your hands and feet."},
      {"Melee Weapons", 20, 2, AG, ST, "+ (2 x (AG + ST))",
       "Using non-ranged weapons in hand-to-hand, or melee combat - knives, "
       "sledgehammers, spears, clubs and so on."},
      {"Throwing", 0, 4, AG, -1, "+ (4 x AG)",
       "The skill of muscle-propelled ranged weapons, such as throwing "
       "knives, spears and grenades."},
      {"First Aid", 0, 2, PE, IN, "+ (2 x (PE + IN))",
       "General healing skill. Used to heal small cuts, abrasions and other "
       "minor ills."},
      {"Doctor", 5, 1, PE, IN, "+ (PE + IN)",
       "The healing of major wounds and crippled limbs. Without this skill, "
       "it will take a much longer period of time to restore crippled limbs "
       "to use."},
      {"Sneak", 5, 3, AG, -1, "+ (3 x AG)",
       "Quiet movement, and the ability to remain unnoticed. If successful, "
       "you will be much harder to locate."},
      {"Lockpick", 10, 1, PE, AG, "+ (PE + AG)",
       "The ability to pick padlocks and combination locks, and to disarm "
       "mechanical traps."},
      {"Steal", 0, 3, AG, -1, "+ (3 x AG)",
       "The ability to make things belong to you. Use this skill to steal "
       "from another, or to plant an item on them."},
      {"Traps", 10, 1, PE, AG, "+ (PE + AG)",
       "The finding and removal of traps. Also the setting of explosives for "
       "demolition purposes."},
      {"Science", 0, 4, IN, -1, "+ (4 x IN)",
       "Covers a variety of high-technology skills, such as computers, "
       "biology, physics and geology."},
      {"Repair", 0, 3, IN, -1, "+ (3 x IN)",
       "The practical application of the Science skill for fixing damaged "
       "equipment, machinery and electronics."},
      {"Speech", 0, 5, CH, -1, "+ (5 x CH)",
       "The ability to communicate in a practical and efficient manner. The "
       "skill of convincing others that your position is correct."},
      {"Barter", 0, 4, CH, -1, "+ (4 x CH)",
       "Trading and trade-related tasks. The lower the skill, the higher the "
       "price you will pay for an item."},
      {"Gambling", 0, 5, LK, -1, "+ (5 x LK)",
       "The knowledge and practical skills related to wagering. The skill at "
       "cards, dice and other games."},
      {"Outdoorsman", 0, 2, EN, IN, "+ (2 x (EN + IN))",
       "Practical knowledge of the outdoors, and the ability to live off the "
       "land. The knowledge of plants and animals."},
  }};
  return v;
}

/** skillGetValue, skill.cc:230. `invested` is SKILL POINTS spent, which for a
 *  TAGGED skill count twice (editor.msg {500}: 1 point per 2%). */
inline int skillValue(int idx, const Special& s, const Traits& tr, bool tagged,
                      int invested) {
  const SkillDef& d = skills()[(size_t)idx];
  int v = d.base + d.mult * (s[d.s1] + (d.s2 >= 0 ? s[d.s2] : 0));
  v += invested * (tagged ? 2 : 1);
  if (tagged) v += 20;
  if (tr.gifted) v -= 10;  // traitGetSkillModifier: -10 to EVERY skill
  return std::min(v, 300);
}

/** skillPointsPerLevel, character_editor.cc / stat.cc. */
inline int skillPointsPerLevel(const Special& s, const Traits& tr,
                               int educatedRank) {
  return 5 + 2 * s[IN] + 2 * educatedRank + (tr.skilled ? 5 : 0) -
         (tr.gifted ? 5 : 0);
}

/** stat.msg 301-310, indexed by the stat's VALUE — this is why the descriptor
 *  column is data, not a per-row string. */
inline const char* descriptor(int value) {
  static const char* d[11] = {"Very Bad", "Very Bad",  "Bad",   "Poor",
                              "Fair",     "Average",   "Good",  "Very Good",
                              "Great",    "Excellent", "Heroic"};
  return d[(size_t)std::clamp(value, 0, 10)];
}

/** The game's own _itostndn(): thousands separators. */
inline std::string thousands(int v) {
  std::string s = std::to_string(v);
  for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
  return s;
}

// ---------------------------------------------------------------------------
// THE AUDIT. The same derive()/skillValue() run against the three shipped
// premades, whose sheets the character-selection screens print. 21 independent
// numbers, each row's verdict COMPUTED from the shipped value and the one this
// file derives, so the fraction on the caption band cannot disagree with the
// arithmetic behind it. If a formula above is wrong, the caption band says so.

inline sigil::measure::Table audit() {
  namespace measure = sigil::measure;
  measure::Table t;
  auto chk = [&](const char* premade, const char* what, int got, int want) {
    t.add(measure::check(kit::formatted("%s  %s", premade, what), want, got));
  };
  // Narg — Heavy Handed, Gifted.
  {
    Special s{{9, 6, 10, 4, 5, 8, 5}};
    Traits tr;
    tr.heavyHanded = tr.gifted = true;
    Derived d = derive(s, tr, 1, 0);
    t.add(measure::heading("NARG  Heavy Handed, Gifted"));
    chk("Narg", "hit points", d.hitPoints, 44);
    chk("Narg", "armor class", d.armorClass, 8);
    chk("Narg", "action points", d.actionPoints, 9);
    chk("Narg", "melee damage", d.meleeDamage, 8);
    chk("Narg", "Melee Weapons %", skillValue(4, s, tr, true, 0), 64);
    chk("Narg", "Small Guns %", skillValue(0, s, tr, true, 0), 47);
    chk("Narg", "Throwing %", skillValue(5, s, tr, true, 0), 42);
  }
  // Mingan — Skilled, Small Frame.
  {
    Special s{{5, 8, 4, 4, 5, 10, 5}};
    Traits tr;
    tr.skilled = tr.smallFrame = true;
    Derived d = derive(s, tr, 1, 0);
    t.add(measure::heading("MINGAN  Skilled, Small Frame"));
    chk("Mingan", "hit points", d.hitPoints, 28);
    chk("Mingan", "armor class", d.armorClass, 10);
    chk("Mingan", "action points", d.actionPoints, 10);
    chk("Mingan", "melee damage", d.meleeDamage, 1);
    chk("Mingan", "Sneak %", skillValue(8, s, tr, true, 0), 55);
    chk("Mingan", "Lockpick %", skillValue(9, s, tr, true, 0), 48);
    chk("Mingan", "Steal %", skillValue(10, s, tr, true, 0), 50);
  }
  // Chitsa — One Hander, Sex Appeal (neither touches these numbers).
  {
    Special s{{4, 5, 4, 10, 7, 6, 4}};
    Traits tr;
    Derived d = derive(s, tr, 1, 0);
    t.add(measure::heading("CHITSA  One Hander, Sex Appeal"));
    chk("Chitsa", "hit points", d.hitPoints, 27);
    chk("Chitsa", "armor class", d.armorClass, 6);
    chk("Chitsa", "action points", d.actionPoints, 8);
    chk("Chitsa", "melee damage", d.meleeDamage, 1);
    chk("Chitsa", "Speech %", skillValue(14, s, tr, true, 0), 70);
    chk("Chitsa", "Barter %", skillValue(15, s, tr, true, 0), 60);
    chk("Chitsa", "First Aid %", skillValue(6, s, tr, true, 0), 44);
  }
  return t;
}

// ---------------------------------------------------------------------------
// THE CARD ILLUSTRATION. A neutral black line-art figure, GENERATED from a
// pose: head radius, shoulder span, arm swing, leg spread, prop. Three poses
// with three different INK insets (48 / 31 / 39 original px) so the card's
// computed wrap width visibly changes between selections — that is the moving
// flowAround test.

struct Pose {
  float inkLeft;   // original px from the illustration blit x to first ink
  float inkWidth;  // original px of inked span
  float armSwing;  // radians of arm splay
  float legSpread;
  int prop;  // 0 none, 1 rifle across the body, 2 raised arm
  bool operator==(const Pose&) const = default;
};

/** The figure, drawn in the INK box's own [0,w]x[0,h].
 *
 *  A real SILHOUETTE, not a stick figure: one closed contour walks the neck ->
 *  shoulder -> outer arm -> hand -> inner arm -> armpit -> waist -> hip ->
 *  outer leg -> foot -> inner leg -> crotch and mirrors back, so the body
 *  reads as a body. Everything is a function of the pose, and the corners are
 *  softened by shapes::rounded() rather than by hand-tuned control points. */
inline shapes::OutlineFn figure(Pose p) {
  auto raw = [p](SkSize s) {
    const float w = s.width(), h = s.height();
    const float cx = w * 0.5f;
    const bool raise = p.prop == 2;
    SkPathBuilder b;

    // --- head, ear, hair -------------------------------------------------
    const float headR = h * 0.082f;
    const float headY = h * 0.105f;
    b.addCircle(cx, headY, headR);
    b.moveTo(cx - headR * 0.96f, headY - headR * 0.30f);
    b.quadTo(cx - headR * 0.15f, headY - headR * 1.80f, cx + headR * 0.92f,
             headY - headR * 0.50f);
    b.moveTo(cx + headR * 1.00f, headY - headR * 0.05f);
    b.quadTo(cx + headR * 1.42f, headY + headR * 0.22f, cx + headR * 0.90f,
             headY + headR * 0.55f);

    // --- the closed body silhouette, right half then mirrored ------------
    const float neckY = headY + headR * 0.94f;
    const float shY = h * 0.205f;
    const float shX = w * (raise ? 0.34f : 0.36f);
    const float a = p.armSwing;
    const float elbowX = std::min(w * 0.40f, shX + std::sin(a) * w * 0.14f);
    const float elbowY = raise ? h * 0.185f : h * 0.375f;
    const float handX =
        std::min(w * 0.435f, elbowX + std::sin(a * 0.5f) * w * 0.11f);
    const float handY = raise ? h * 0.055f : h * 0.535f;
    const float armT = w * 0.070f;  // arm half-thickness
    const float kneeX = w * (0.115f + p.legSpread * 0.055f);
    const float ankX = w * (0.130f + p.legSpread * 0.090f);
    struct P {
      float x, y;
    };
    const P half[] = {
        {w * 0.062f, neckY},                         // neck
        {shX, shY},                                  // shoulder
        {elbowX, elbowY},                            // outer elbow
        {handX, handY},                              // outer hand
        {handX - armT * 0.55f, handY + h * 0.030f},  // fingertips
        {elbowX - armT, elbowY + h * 0.018f},        // inner elbow
        {w * 0.245f, h * 0.345f},                    // armpit
        {w * 0.190f, h * 0.450f},                    // waist
        {w * 0.262f, h * 0.565f},                    // hip
        {kneeX + w * 0.055f, h * 0.735f},
        {ankX, h * 0.895f},
        {ankX + w * 0.085f, h * 0.930f},  // toe
        {ankX - w * 0.058f, h * 0.930f},  // heel
        {w * 0.050f, h * 0.600f},         // crotch
    };
    const int nHalf = (int)(sizeof half / sizeof half[0]);
    b.moveTo(cx + half[0].x, half[0].y);
    for (int i = 1; i < nHalf; ++i) b.lineTo(cx + half[i].x, half[i].y);
    for (int i = nHalf - 1; i >= 0; --i) b.lineTo(cx - half[i].x, half[i].y);
    b.close();

    // --- interior engraving: neck, pectorals, abdomen, belt --------------
    b.moveTo(cx - w * 0.062f, neckY);
    b.lineTo(cx - w * 0.062f, shY - h * 0.012f);
    b.moveTo(cx + w * 0.062f, neckY);
    b.lineTo(cx + w * 0.062f, shY - h * 0.012f);
    for (int i = 0; i < 3; ++i) {
      const float y = shY + h * (0.085f + 0.052f * (float)i);
      const float sp = w * (0.150f - 0.030f * (float)i);
      b.moveTo(cx - sp, y);
      b.quadTo(cx, y + h * 0.020f, cx + sp, y);
    }
    b.moveTo(cx, shY + h * 0.055f);
    b.lineTo(cx, h * 0.520f);
    b.moveTo(cx - w * 0.205f, h * 0.548f);  // belt
    b.lineTo(cx + w * 0.205f, h * 0.548f);
    b.moveTo(cx - w * 0.225f, h * 0.612f);
    b.lineTo(cx + w * 0.225f, h * 0.612f);
    // knees
    for (int sgn = -1; sgn <= 1; sgn += 2) {
      const float x = cx + (float)sgn * (kneeX + w * 0.015f);
      b.moveTo(x - w * 0.045f, h * 0.740f);
      b.quadTo(x, h * 0.756f, x + w * 0.045f, h * 0.740f);
    }

    if (p.prop == 1) {  // a long arm slung across the body, one closed outline
      const float t = h * 0.020f;
      const SkPoint muzzle{w * 0.92f, h * 0.375f};
      const SkPoint breech{w * 0.30f, h * 0.535f};
      const SkVector d{muzzle.fX - breech.fX, muzzle.fY - breech.fY};
      const float len = std::hypot(d.fX, d.fY);
      const SkVector u{d.fX / len, d.fY / len};
      const SkVector nvec{-u.fY, u.fX};
      auto pt = [&](float along, float across) {
        return SkPoint{breech.fX + u.fX * along + nvec.fX * across,
                       breech.fY + u.fY * along + nvec.fY * across};
      };
      b.moveTo(pt(0, -t));
      b.lineTo(pt(len, -t * 0.55f));
      b.lineTo(pt(len, t * 0.55f));
      b.lineTo(pt(len * 0.42f, t));
      b.lineTo(pt(len * 0.30f, t * 3.4f));  // magazine
      b.lineTo(pt(len * 0.16f, t * 3.4f));
      b.lineTo(pt(len * 0.10f, t));
      b.lineTo(pt(-len * 0.24f, t * 2.6f));  // stock
      b.lineTo(pt(-len * 0.30f, -t * 0.4f));
      b.close();
    }
    if (raise) {  // something held aloft in the right hand
      b.addCircle(cx + handX - w * 0.01f, handY - h * 0.045f, w * 0.070f);
    }
    return b.detach();
  };
  return shapes::rounded(std::move(raw), 6.0f);
}

/** THE STAMP this card's chrome is pressed with: the moulded plane pair a
 *  cast metal plate has, lit from the upper left at the angle the
 *  capture's own shadows fall at. Every plaque, tab, button, frame and
 *  well on the sheet is this one token set — what changes between them is
 *  how deep the press went and which two golds it went into, never the
 *  treatment. The one recess lit from below states its own angle: a press
 *  from the other side is the same stamp with the light moved. */
inline kit::Bevel stamp(float depth, float softness, SkColor4f lit,
                        SkColor4f shade, float angleDeg = 118) {
  kit::Bevel b = kit::bevels::plate(lit, shade, n(depth), n(softness));
  b.angleDeg = angleDeg;
  return b;
}

}  // namespace fo

// ===========================================================================
