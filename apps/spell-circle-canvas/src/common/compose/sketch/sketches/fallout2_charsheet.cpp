// fallout2_charsheet.cpp — FALLOUT 2, THE CHARACTER SCREEN
// =============================================================================
// Black Isle Studios / Interplay, 1998 (PC-DOS/Windows). The full-screen sheet
// opened with `C` during play — `character_editor.cc` in non-creation mode.
// Authored at 640x480 in 8-bit indexed colour, ONE fixed resolution; rebuilt
// here at exactly 2x. Every constant below is in original 640x480 units and
// goes through n() — divide any canvas number by 2 to recover the 1998 pixel.
//
// This is the programme's first TYPE-SET screen. No shader carries it: it is
// ~60 rows of small type in five different two-column regimes plus a parchment
// card of real body copy wrapping around an illustration. The paragraph engine
// and the grid do all the work.
//
// CANVAS 1280 x 1088. The SCREEN is exactly 1280x960 at the origin — halve the
// capture and it overlays the reference pixel-for-pixel. The 128 px band below
// is a plate caption (the Penrose study's move) carrying the arithmetic audit,
// which is not part of the 1998 artefact and is drawn outside it on purpose.
//
// -----------------------------------------------------------------------------
// SOURCES — read, not remembered
//
//  * github.com/alexbatalov/fallout2-ce — a C++ decompilation of the shipped
//    game. `character_editor.cc` (7,306 lines) is the entire screen: window
//    size, every draw coordinate, every row pitch, every colour index, the
//    folder view, the card, the big-number renderer and its animation delay.
//    `stat.cc` gives critterUpdateDerivedStats (:555), pcGetExperienceForLevel
//    (:654) and the per-level HP gain (~:770); `skill.cc` gives
//    gSkillDescriptions (:76) and skillGetValue (:230); `trait.cc` gives
//    traitGetStatModifier / traitGetSkillModifier (:180, :284); `color.cc`
//    gives the 15-bit -> 256-entry palette table.
//  * github.com/BGforgeNet/Fallout2_Unofficial_Patch data/text/english/game/ —
//    stat.msg, skill.msg, editor.msg, proto.msg. Every label and every line of
//    body copy quoted below is verbatim from those files. CAVEAT, stated
//    plainly: this is the Unofficial Patch's copy, not a vanilla 1.02 dump. For
//    the strings used here it matches the reference capture character for
//    character, but it is a patched repository.
//  * lparchive.org/Fallout-2-(by-ddegenha)/Update 02/ — a LOSSLESS 640x480 PNG
//    of the screen itself (character "Luke", level 1) plus the three premade
//    selection screens. Because it is PNG and not JPEG the sampled colours are
//    the game's actual palette entries. Every hex marked "sampled" below was
//    read off that file's pixels with PIL in this session; every panel rect was
//    found by scanning it for its near-black wells (results in the table under
//    "Geometry"), which is why a few of them differ by 1-3 px from the brief.
//
// DOCUMENTED vs RECONSTRUCTED, since the difference is the whole point:
//   DOCUMENTED — the 640x480 window; every draw x/y; row pitches; the colour
//     indices and their sampled results; every label and body string; the
//     derived-stat and skill formulas; the XP table; BIG_NUM_WIDTH 14,
//     BIG_NUM_HEIGHT 24, BIG_NUM_ANIMATION_DELAY 123 ms; the kills-folder
//     leader-rule geometry; the card's wrap-width rule.
//   RECONSTRUCTED — (1) ALL chrome art. The plate, plaques, wells, rivets,
//     tabs, buttons and parchment are raster FRMs (intrface art id 177); their
//     layout is documented, their pixels are not recoverable, so they are
//     generated here from Material::blend / patterns::grain / shapes::inset.
//     (2) The typeface: Fallout draws with bitmap fonts (font101/102/103.aaf).
//     SigilWeave has no bitmap-font path, so the sheet is RE-SET in real faces
//     at 2x with real kerning. This is a typographic restoration, not a pixel
//     copy, and it is the single largest departure. (3) The character: Narg at
//     level 6 — his level-1 sheet is documented, levels 2..6 are a scenario,
//     but every number in that scenario is DERIVED below, never typed.
//     (4) The card illustration: a neutral black line-art figure of the same
//     footprint, not the licensed Vault Boy.
//
// -----------------------------------------------------------------------------
// TWO THINGS THAT SURPRISED THE RESEARCH, AND THEY BELONG IN THE HEADER
//
//  1. FALLOUT'S GREEN IS NOT #00FF00. The engine asks for colours as a 15-bit
//     RGB555 triple and looks it up in _colorTable[32768], which maps it to the
//     NEAREST entry of a 256-colour VGA palette. _colorTable[992] is RGB555
//     (0,31,0) — pure green — and the palette has no such entry. What actually
//     reached the CRT is a chartreuse #3CF800 (60,248,0), sampled off the
//     lossless capture. Everything ever written about "Fallout green" describes
//     a colour the game never drew. The received account is wrong because it
//     reads the SOURCE, and the source is a request, not a result.
//  2. THE GAME'S OWN PRINTED CHARACTER SHEET IS BROKEN. characterPrintToFile()
//     formats every skills-and-kills line — name, dots to 16 columns, %.3d%% —
//     into a buffer and never writes it to the file (:4637-4671); four lines
//     earlier the decompilation carries a FIXME noting the "Sequence" field
//     prints Strength. So the dot-leader function (_AddDots, :4745) that
//     motivates the whole two-column problem below lives in a code path whose
//     output nobody has ever seen.
//
// Also worth one line: editor.msg ships the derived-stat labels with TRAILING
// SPACES ("Action Points ", "Damage Res.  ", "Poison Res.   ") — vestigial
// padding from an even older fixed-x layout. They are trimmed here.
//
// -----------------------------------------------------------------------------
// SEVEN NUMBERS BECOME SIXTY — the reconstruction IS the arithmetic
//
// Ten derived statistics and eighteen skill percentages fall out of
// S.P.E.C.I.A.L. by published integer formulas. NOTHING on this screen is
// typed in; `derive()` and `skillValue()` below compute all of it, and
// `audit()` re-runs the same functions against the three shipped premade
// characters (Narg, Mingan, Chitsa, whose sheets the selection screens print)
// for 21 independent checks INCLUDING the two trait corrections. The result is
// printed on the caption band as a fraction, so a wrong formula shows up as
// 19/21 rather than as a plausible drawing.
//
// The XP curve is written in the decompilation as two integer-division
// branches to dodge a divide; it is 1000 * T(L-1) either way, i.e. 1000 times
// the triangular numbers: 1000, 3000, 6000, 10000, 15000, 21000, 28000.
//
// -----------------------------------------------------------------------------
// WHAT THIS EXERCISES, AND THE ONE HARD THING
//
// ~60 absolutely-positioned text leaves; text(paragraph, opts) with a FORCED
// line pitch (LineMetricsOptions.height = 22) and greedy breaking; flowAround()
// — the derive phase's only user-facing feature, previously untouched by this
// programme; renderSlot() as the counter/content idiom; measure() for the
// leader rules and the card's title advance; patterns::grain, Material::blend,
// Material::linearUnit/radialUnit, shapes::inset, shapes::circle, util::disc,
// styles::BevelEmboss, PathFormat hairlines, Element::overlay(), bind().
//
// THE HARD THING: every row here is a two-column table and the library has no
// table. Five regimes on one screen. The kills folder is the dense case — it
// measures the name, measures the count, and draws a hairline between them at
// y + lineHeight/2. TabStopOptions carries `positions` and `interval` and NO
// LEADER, so that is hand-built from two measure() calls per row. See the
// report; this is now the programme's most concrete text-side ask.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;
namespace weave = sigil::weave;

namespace fo {

// ---------------------------------------------------------------------------
// Scale. ORIGINAL 640x480 px -> canvas px. Nothing else scales anything.

constexpr float kScale = 2.0f;
constexpr float n(float v) { return v * kScale; }
constexpr float kScreenW = n(640), kScreenH = n(480);
constexpr float kCaptionH = 128.0f;

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f, a};
}
inline SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// PALETTE. Left column = what the code REQUESTS through _colorTable; right =
// what the 6-bit VGA palette actually delivered, sampled off the lossless PNG.
// The gap is the finding (see header).

constexpr SkColor4f kGreen = C(0x3CF800);    // _colorTable[992],  req #00FF00
constexpr SkColor4f kSelected = C(0xFCFC7C); // _colorTable[32747], req #F8F858
constexpr SkColor4f kInactive = C(0x183018); // _colorTable[1313],  req #084808
constexpr SkColor4f kTagged = C(0xA0A0A0);   // _colorTable[21140], exact
constexpr SkColor4f kGold = C(0x907824);     // _colorTable[18979], req #908818
constexpr SkColor4f kGoldDim = C(0x7C6818);  // engraving shadow
constexpr SkColor4f kInk = C(0x000000);      // ALL card text and its rule

constexpr SkColor4f kWell = C(0x040C00);     // inset interior: near-black,
                                             // GREEN-cast, not pure black
constexpr SkColor4f kPlate = C(0x383020);    // metal plate, base olive
constexpr SkColor4f kPlateLit = C(0x483828); // lit facet
constexpr SkColor4f kPlateDark = C(0x302820);// shadowed facet
constexpr SkColor4f kRust = C(0x7C581C);
constexpr SkColor4f kParch = C(0x9C7434);    // parchment base ochre
constexpr SkColor4f kParchLit = C(0xAC8044);
constexpr SkColor4f kParchLit2 = C(0xBC9054);
constexpr SkColor4f kParchDark = C(0x8C6428);
constexpr SkColor4f kParchScuff = C(0x947C60);
constexpr SkColor4f kLampOff = C(0x580000);
constexpr SkColor4f kLampOn = C(0xF80000);   // _colorTable[31744]
constexpr SkColor4f kDigit = C(0xFFFFFF);    // the odometer sprite sheet's
                                             // first half (the second half is
                                             // red, for stats above 10)

// ---------------------------------------------------------------------------
// GEOMETRY. Source tag: (code) = character_editor.cc; (measured) = scanned off
// lp_1-FO2_1_2.png in this session. Where they disagree, measured wins and the
// difference is noted.

// The five inset wells (measured bboxes of the near-black regions).
struct Rect { float x, y, w, h; };
constexpr Rect kWellStatus{188, 37, 130, 118};  // code: (194,46) 118x108
constexpr Rect kWellDerived{188, 171, 130, 143};// code: (194,179) 116x130
constexpr Rect kWellLevel{25, 275, 132, 40};    // code: (32,280) 124x32
constexpr Rect kWellSkills{368, 23, 249, 202};  // code: (370,27) region
constexpr Rect kWellFolder{25, 359, 291, 112};  // code list body 34..314
constexpr Rect kWellSpecial{5, 34, 152, 232};   // the S.P.E.C.I.A.L. column

// S.P.E.C.I.A.L. rows (code, gCharacterEditorPrimaryStatY) — pitch 33.
constexpr std::array<float, 7> kStatY{37, 70, 103, 136, 169, 202, 235};
constexpr float kAbbrX = 20;      // measured ink 20..55, 21 px tall
constexpr float kOdoX = 58;       // code; cell 14 x 24
constexpr float kOdoW = 14, kOdoH = 24;
constexpr float kPlaqueX = 100, kPlaqueW = 58; // measured 100..157
constexpr float kDescX = 103;     // code (x = 103, y = row + 8)

constexpr float kRowPitch13 = 13; // fontGetLineHeight(101) + 3
constexpr float kRowPitch11 = 11; // fontGetLineHeight(101) + 1

// ---------------------------------------------------------------------------
// TYPE. Fallout sets everything in 8-bit bitmap faces at 10 px and 28 px.
// Substitutes are resolved through the system font manager with a fallback
// chain; the BODY size is derived from a MEASURED advance rather than assumed,
// because the whole reason the game's fixed value columns work is that its
// small font is near-monospaced (winamp_base's lesson, reapplied).

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              SkFontStyle::Width width,
                              const char *fb1 = nullptr,
                              const char *fb2 = nullptr) {
  auto mgr = weave::ports::systemFontManager();
  const SkFontStyle style(weight, width, SkFontStyle::kUpright_Slant);
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(family, style);
  if (!f && fb1)
    f = mgr->matchFamilyStyle(fb1, style);
  if (!f && fb2)
    f = mgr->matchFamilyStyle(fb2, style);
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
/** font 101 substitute — a squarish near-monospace. */
inline const sk_sp<SkTypeface> &bodyFace() {
  static sk_sp<SkTypeface> f = face("Andale Mono", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kNormal_Width, "Menlo",
                                    "PT Mono");
  return f;
}
inline const sk_sp<SkTypeface> &bodyBold() {
  static sk_sp<SkTypeface> f = face("Andale Mono", SkFontStyle::kBold_Weight,
                                    SkFontStyle::kNormal_Width, "Menlo",
                                    "PT Mono");
  return f;
}
/** The engraved gold: plaques, tabs, buttons, S.P.E.C.I.A.L. caps. */
inline const sk_sp<SkTypeface> &engraved() {
  static sk_sp<SkTypeface> f = face("Impact", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kCondensed_Width,
                                    "Haettenschweiler", "Copperplate");
  return f;
}
/** font 102 substitute — the card title, a condensed heavy grotesque. */
inline const sk_sp<SkTypeface> &titleFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::kBlack_Weight,
                                    SkFontStyle::kCondensed_Width, "Impact",
                                    "Helvetica");
  return f;
}
/** The odometer digits — tabular figures on a hard 14 px cell. */
inline const sk_sp<SkTypeface> &digitFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::kBold_Weight,
                                    SkFontStyle::kCondensed_Width, "Menlo",
                                    "Helvetica");
  return f;
}

inline weave::TextStyle type(const sk_sp<SkTypeface> &tf, float size,
                             SkColor4f color, float track = 0,
                             float condense = 1.0f) {
  weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** Probed once in setup(): px of advance per em for the body face, and the
 *  scaleX that lands "EN-" on the measured 36 original px. */
inline float &bodyEm() {
  static float v = 0.6f;
  return v;
}
inline float &titleCondense() {
  static float v = 0.6f;
  return v;
}
/** Original px the body face advances per character.
 *
 *  Fallout's font 101 is PROPORTIONAL, not monospaced — fontGetStringWidth()
 *  sums glyphWidth + letterSpacing per glyph — so no single advance can match
 *  it. Measured off the capture's ten derived-stat labels: 752 px of ink over
 *  119 characters, i.e. ~6.4 px of advance. But "Critical Chance" is 15
 *  characters and its VALUE column starts at x = 288, only 94 px away, so a
 *  6.4 px monospace collides with its own value. 6.15 is the widest advance
 *  that keeps every row inside its column — the compromise a monospaced
 *  substitute forces, stated rather than hidden. */
constexpr float kBodyAdvance = 6.15f;
inline float bodySize() { return n(kBodyAdvance) / bodyEm(); }

/** The distance from a laid-out line's TOP to the top of its capitals, in
 *  ORIGINAL px — Fallout's draw y is the glyph cell's top, SigilWeave's node
 *  top is the line box's top, and the two differ by the ascent slack. Derived
 *  from the substituted face's own metrics in setup(). */
inline float &bodyRise() {
  static float v = 2.0f;
  return v;
}
inline float &titleRise() {
  static float v = 5.0f;
  return v;
}
inline float &engravedRise() {
  static float v = 3.0f;
  return v;
}

inline Element t(const std::string &s, weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}
/** Place at a DOCUMENTED (x, y) in original screen px. `y` is Fallout's draw
 *  y — the top of the glyph cell — so the rise correction lands here, once. */
inline Element ink(Element e, float x, float y, float rise) {
  e.absolute().left(Dim(n(x))).top(Dim(n(y) - rise));
  return e;
}
inline Element at(Element e, float x, float y, float w, float h) {
  e.absolute().left(Dim(n(x))).top(Dim(n(y))).width(Dim(n(w))).height(Dim(n(h)));
  return e;
}
inline Element atR(Element e, Rect r) { return at(std::move(e), r.x, r.y, r.w, r.h); }

// ---------------------------------------------------------------------------
// THE MECHANISM — stat.cc / skill.cc / trait.cc, transcribed.

enum Stat { ST = 0, PE, EN, CH, IN, AG, LK };

struct Special {
  std::array<int, 7> v{};
  int operator[](int i) const { return v[(size_t)i]; }
};

struct Traits {
  bool gifted = false;      // +1 every stat, -10 EVERY skill, -5 skill pts/lvl
  bool heavyHanded = false; // +4 melee damage
  bool smallFrame = false;  // +1 AG, carry weight -10*ST
  bool bruiser = false;     // +2 ST, -2 action points
  bool skilled = false;     // +5 skill points/level, a perk every 4 levels
};

/** critterUpdateDerivedStats, stat.cc:555. All integer arithmetic. */
struct Derived {
  int hitPoints, actionPoints, armorClass, meleeDamage, carryWeight;
  int sequence, healingRate, criticalChance;
  int damageResist, poisonResist, radResist;
};
inline Derived derive(const Special &s, const Traits &tr, int level,
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
  d.damageResist = 0 + 10 * toughnessRank; // perk.msg {1113}: +10% general DR
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
  const char *name;
  int base;
  int mult;
  int s1;
  int s2; // -1 = single-stat
  const char *formula;
  const char *blurb; // skill.msg 200-217, verbatim
};
inline const std::array<SkillDef, 18> &skills() {
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
inline int skillValue(int idx, const Special &s, const Traits &tr, bool tagged,
                      int invested) {
  const SkillDef &d = skills()[(size_t)idx];
  int v = d.base + d.mult * (s[d.s1] + (d.s2 >= 0 ? s[d.s2] : 0));
  v += invested * (tagged ? 2 : 1);
  if (tagged)
    v += 20;
  if (tr.gifted)
    v -= 10; // traitGetSkillModifier: -10 to EVERY skill
  return std::min(v, 300);
}

/** skillPointsPerLevel, character_editor.cc / stat.cc. */
inline int skillPointsPerLevel(const Special &s, const Traits &tr,
                               int educatedRank) {
  return 5 + 2 * s[IN] + 2 * educatedRank + (tr.skilled ? 5 : 0) -
         (tr.gifted ? 5 : 0);
}

/** stat.msg 301-310, indexed by the stat's VALUE — this is why the descriptor
 *  column is data, not a per-row string. */
inline const char *descriptor(int value) {
  static const char *d[11] = {"Very Bad", "Very Bad", "Bad",      "Poor",
                              "Fair",     "Average",  "Good",     "Very Good",
                              "Great",    "Excellent","Heroic"};
  return d[(size_t)std::clamp(value, 0, 10)];
}

/** The game's own _itostndn(): thousands separators. */
inline std::string thousands(int v) {
  std::string s = std::to_string(v);
  for (int i = (int)s.size() - 3; i > 0; i -= 3)
    s.insert((size_t)i, ",");
  return s;
}

// ---------------------------------------------------------------------------
// THE AUDIT. The same derive()/skillValue() run against the three shipped
// premades, whose sheets the character-selection screens print. 21 independent
// numbers. If a formula above is wrong, the caption band says so.

struct Audit {
  int passed = 0, total = 0;
};
inline Audit audit() {
  Audit a;
  auto chk = [&](int got, int want) {
    a.total++;
    if (got == want)
      a.passed++;
  };
  // Narg — Heavy Handed, Gifted.
  {
    Special s{{9, 6, 10, 4, 5, 8, 5}};
    Traits tr;
    tr.heavyHanded = tr.gifted = true;
    Derived d = derive(s, tr, 1, 0);
    chk(d.hitPoints, 44);
    chk(d.armorClass, 8);
    chk(d.actionPoints, 9);
    chk(d.meleeDamage, 8);
    chk(skillValue(4, s, tr, true, 0), 64);  // Melee Weapons
    chk(skillValue(0, s, tr, true, 0), 47);  // Small Guns
    chk(skillValue(5, s, tr, true, 0), 42);  // Throwing
  }
  // Mingan — Skilled, Small Frame.
  {
    Special s{{5, 8, 4, 4, 5, 10, 5}};
    Traits tr;
    tr.skilled = tr.smallFrame = true;
    Derived d = derive(s, tr, 1, 0);
    chk(d.hitPoints, 28);
    chk(d.armorClass, 10);
    chk(d.actionPoints, 10);
    chk(d.meleeDamage, 1);
    chk(skillValue(8, s, tr, true, 0), 55);  // Sneak
    chk(skillValue(9, s, tr, true, 0), 48);  // Lockpick
    chk(skillValue(10, s, tr, true, 0), 50); // Steal
  }
  // Chitsa — One Hander, Sex Appeal (neither touches these numbers).
  {
    Special s{{4, 5, 4, 10, 7, 6, 4}};
    Traits tr;
    Derived d = derive(s, tr, 1, 0);
    chk(d.hitPoints, 27);
    chk(d.armorClass, 6);
    chk(d.actionPoints, 8);
    chk(d.meleeDamage, 1);
    chk(skillValue(14, s, tr, true, 0), 70); // Speech
    chk(skillValue(15, s, tr, true, 0), 60); // Barter
    chk(skillValue(6, s, tr, true, 0), 44);  // First Aid
  }
  return a;
}

// ---------------------------------------------------------------------------
// THE CARD ILLUSTRATION. A neutral black line-art figure, GENERATED from a
// pose: head radius, shoulder span, arm swing, leg spread, prop. Three poses
// with three different INK insets (48 / 31 / 39 original px) so the card's
// computed wrap width visibly changes between selections — that is the moving
// flowAround test.

struct Pose {
  float inkLeft;  // original px from the illustration blit x to first ink
  float inkWidth; // original px of inked span
  float armSwing; // radians of arm splay
  float legSpread;
  int prop; // 0 none, 1 rifle across the body, 2 raised arm
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
    const float handX = std::min(w * 0.435f, elbowX + std::sin(a * 0.5f) * w * 0.11f);
    const float handY = raise ? h * 0.055f : h * 0.535f;
    const float armT = w * 0.070f; // arm half-thickness
    const float kneeX = w * (0.115f + p.legSpread * 0.055f);
    const float ankX = w * (0.130f + p.legSpread * 0.090f);
    struct P { float x, y; };
    const P half[] = {
        {w * 0.062f, neckY},           // neck
        {shX, shY},                    // shoulder
        {elbowX, elbowY},              // outer elbow
        {handX, handY},                // outer hand
        {handX - armT * 0.55f, handY + h * 0.030f}, // fingertips
        {elbowX - armT, elbowY + h * 0.018f},       // inner elbow
        {w * 0.245f, h * 0.345f},      // armpit
        {w * 0.190f, h * 0.450f},      // waist
        {w * 0.262f, h * 0.565f},      // hip
        {kneeX + w * 0.055f, h * 0.735f},
        {ankX, h * 0.895f},
        {ankX + w * 0.085f, h * 0.930f}, // toe
        {ankX - w * 0.058f, h * 0.930f}, // heel
        {w * 0.050f, h * 0.600f},        // crotch
    };
    const int nHalf = (int)(sizeof half / sizeof half[0]);
    b.moveTo(cx + half[0].x, half[0].y);
    for (int i = 1; i < nHalf; ++i)
      b.lineTo(cx + half[i].x, half[i].y);
    for (int i = nHalf - 1; i >= 0; --i)
      b.lineTo(cx - half[i].x, half[i].y);
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
    b.moveTo(cx - w * 0.205f, h * 0.548f); // belt
    b.lineTo(cx + w * 0.205f, h * 0.548f);
    b.moveTo(cx - w * 0.225f, h * 0.612f);
    b.lineTo(cx + w * 0.225f, h * 0.612f);
    // knees
    for (int sgn = -1; sgn <= 1; sgn += 2) {
      const float x = cx + (float)sgn * (kneeX + w * 0.015f);
      b.moveTo(x - w * 0.045f, h * 0.740f);
      b.quadTo(x, h * 0.756f, x + w * 0.045f, h * 0.740f);
    }

    if (p.prop == 1) { // a long arm slung across the body, one closed outline
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
      b.lineTo(pt(len * 0.30f, t * 3.4f)); // magazine
      b.lineTo(pt(len * 0.16f, t * 3.4f));
      b.lineTo(pt(len * 0.10f, t));
      b.lineTo(pt(-len * 0.24f, t * 2.6f)); // stock
      b.lineTo(pt(-len * 0.30f, -t * 0.4f));
      b.close();
    }
    if (raise) { // something held aloft in the right hand
      b.addCircle(cx + handX - w * 0.01f, handY - h * 0.045f, w * 0.070f);
    }
    return b.detach();
  };
  return shapes::rounded(std::move(raw), 6.0f);
}

} // namespace fo

// ===========================================================================

struct Fallout2CharSheet : sigil::compose::sketch::Sketch {
  using Out = ch::Output<float>;

  // ---- the character. Seven numbers; everything else is computed. --------
  fo::Special narg{{9, 6, 10, 4, 5, 8, 5}}; // includes Gifted's +1
  fo::Traits nargTraits;
  static constexpr int kLevel = 6;
  static constexpr int kToughness = 1;   // one rank, +10% damage resistance
  fo::Derived stats{};
  std::array<int, 18> skillPct{};
  std::array<bool, 18> tagged{};
  std::array<int, 18> invested{};
  int pointsEarned = 0, pointsSpent = 0;
  fo::Audit sheetAudit;

  // ---- interaction state (the only motion this screen has) ---------------
  static constexpr std::array<int, 3> kWalk{0, 7, 4}; // Small Guns, Doctor,
                                                      // Melee Weapons
  static constexpr double kDwell = 1.30;   // seconds per selection
  static constexpr double kSpend0 = 3.40;  // first `+` press
  static constexpr double kSpendGap = 0.60;
  static constexpr double kLoop = 9.0;
  static constexpr double kBlank = 0.123;  // BIG_NUM_ANIMATION_DELAY

  int selected = 4;
  int presses = 0;      // 0..3 skill-point presses on the selected skill
  std::string odoTens = "0", odoOnes = "4";
  Out plusFlash{0}, lampFlash{0};
  double now = 0;

  // ---- generated surfaces, held so their identity prunes ------------------
  Material plateMat, plateTooth, rustMat, wellMat, parchMat, parchTooth,
      wheelMat, rivetMat, tabMat, canvasGrain;

  // ---- measured advances for the kills leaders (setup-time) ---------------
  struct Kill {
    const char *name;
    int count;
  };
  static const std::array<Kill, 9> &kills() {
    // proto.msg 1450-1468; counts are scenario; sorted case-insensitively by
    // name (characterEditorKillsCompare -> compat_stricmp) and only species
    // with a non-zero count appear.
    static const std::array<Kill, 9> v = {{{"Brahmin", 2},
                                           {"Dogs", 4},
                                           {"Geckos", 11},
                                           {"Giant Ants", 6},
                                           {"Men", 9},
                                           {"Plants", 3},
                                           {"Radscorpions", 5},
                                           {"Rats", 23},
                                           {"Women", 1}}};
    return v;
  }
  std::array<float, 9> killNameW{}, killCountW{};
  float titleAdvance[3] = {0, 0, 0}; // the card title's measured advance

  // paragraph identity for the card body, held so shaping caches stay warm
  std::shared_ptr<weave::Paragraph> cardPara;

  // =========================================================================
  // Type helpers, once the probes have run.

  /** font 101 at 10 px is a BITMAP face: 1-px stems at 640 wide, which is a
   *  heavy colour. An outline face at the same size reads much lighter, so the
   *  substitute runs BOLD — and because Andale Mono is monospaced, bold has
   *  identical advances and every column stays where the game put it. */
  weave::TextStyle body(SkColor4f c) const {
    return fo::type(fo::bodyBold(), fo::bodySize(), c);
  }
  weave::TextStyle heading(SkColor4f c) const {
    return fo::type(fo::bodyBold(), fo::n(14.0f), c, fo::n(0.6f));
  }
  weave::TextStyle plaqueType(float size, SkColor4f c) const {
    return fo::type(fo::engraved(), size, c, fo::n(0.9f));
  }
  /** font 102: cap height 17 original px, advance ~6.8 — a heavily condensed
   *  heavy grotesque (measured off the reference: "Strength" is 55 px wide
   *  with 17 px capitals, a 0.40 advance/cap ratio no stock face has). The
   *  condense factor is derived in setup() from the substitute's own advance
   *  rather than guessed. */
  weave::TextStyle titleStyle() const {
    return fo::type(fo::titleFace(), fo::n(23.8f), fo::kInk, fo::n(-0.1f),
                    fo::titleCondense());
  }

  Element bodyAt(const std::string &s, SkColor4f c, float x, float y) {
    return fo::ink(fo::t(s, body(c)), x, y, fo::bodyRise());
  }

  // =========================================================================
  // Chrome. Everything on this screen that is not type is a raster FRM in the
  // game's data; all of it is generated here.

  void buildSurfaces() {
    using namespace fo;
    // The plate: an olive ramp, a cast-metal tooth, and rust blotches. The
    // tooth and rust ride as separate LAYER ELEMENTS with node opacity because
    // Material::blend has no per-layer amount (ROADMAP §5).
    // Sampled off the capture: the plate sits between #302820 and #383020 with
    // lit facets up to #483828 — a narrow band, so the ramp is shallow and the
    // grain does the work.
    plateMat = Material::linearUnit({0, 0}, {0.15f, 1},
                                    {{0.0f, C(0x4A4030)},
                                     {0.40f, C(0x3A3222)},
                                     {1.0f, C(0x322A1C)}});
    plateTooth = patterns::grain(0.22f, 3, 11.0f, 0.65f, 1.0f);
    rustMat = Material::blend(
        {{Material::solid(kRust), SkBlendMode::kSrcOver},
         {patterns::grain(0.0075f, 3, 5.0f, 1.35f, 1.0f),
          SkBlendMode::kMultiply}});
    wellMat = Material::blend(
        {{Material::solid(kWell), SkBlendMode::kSrcOver},
         {patterns::grain(0.35f, 2, 17.0f, 0.10f), SkBlendMode::kOverlay}});
    // The odometer drum: a cylinder, bright just above centre, dark below,
    // lifting again at the bottom lip. Sampled off the capture at x=61.
    wheelMat = Material::linearUnit({0, 0}, {0, 1},
                                    {{0.0f, C(0x3C3C3C)},
                                     {0.22f, C(0x545454)},
                                     {0.58f, C(0x282828)},
                                     {0.80f, C(0x1C1C1C)},
                                     {1.0f, C(0x383838)}});
    rivetMat = Material::radialUnit({0.34f, 0.30f}, 1.15f,
                                    {{0.0f, C(0x6A5838)},
                                     {0.55f, C(0x3A3020)},
                                     {1.0f, C(0x140F08)}});
    tabMat = Material::linearUnit({0, 0}, {0, 1},
                                  {{0.0f, kPlateLit},
                                   {0.55f, kPlate},
                                   {1.0f, kPlateDark}});
    // The parchment card: the richest surface here. Base ochre, a large-scale
    // mottle, a paper tooth, creases added as elements below.
    // Sampled across the reference card on a 44x32 grid: it sits between
    // #9C7434 and #BC9054 almost everywhere, with #8C6428 creases. Bright, not
    // moody — the first pass here vignetted it into leather and was wrong.
    parchMat = Material::blend(
        {{Material::linearUnit({0.10f, 0}, {0.90f, 1},
                               {{0.0f, kParchLit2},
                                {0.30f, kParchLit},
                                {0.66f, kParch},
                                {1.0f, kParchDark}}),
          SkBlendMode::kSrcOver},
         {patterns::grain(0.013f, 4, 21.0f, 0.62f, 1.4f),
          SkBlendMode::kOverlay}});
    parchTooth = patterns::grain(0.40f, 2, 7.0f, 0.42f, 1.0f);
    canvasGrain = patterns::grain(0.9f, 1, 31.0f, 0.55f, 1.0f);
  }

  /** A recessed well: the black interior, a hard inner keyline, a warm outer
   *  bevel, and rivets at the corners. shapes::inset is literally "the same
   *  keyline again N px further in", which is this chrome's whole vocabulary. */
  Element well(fo::Rect r, float radius = 3.0f, bool rivets = true) {
    using namespace fo;
    Element e = atR(box(), r).corners(Corners{n(radius)}).fill(wellMat);
    e.background(styles::dropShadow(C(0x000000, 0.55f), {0, n(1)}, n(2)));
    e.stroke(util::stroke(n(1), Fill::color(C(0x0A0E06)),
                          PathFormat::Align::Inner));
    e.foreground(shapes::inset(
        n(-1.5f), util::stroke(n(1.5f), Fill::color(C(0x5C4C30, 0.85f)),
                               PathFormat::Align::Center)));
    e.foreground(styles::BevelEmboss{n(1.2f), n(1.6f), 118,
                                     C(0x8A7448, 0.55f), C(0x000000, 0.60f)});
    if (rivets) {
      const float inset = 5.0f;
      for (int i = 0; i < 4; ++i) {
        const float rx = (i & 1) ? r.w - inset : inset;
        const float ry = (i & 2) ? r.h - inset : inset;
        e.child(util::disc({n(rx), n(ry)}, n(2.4f))
                    .fill(rivetMat)
                    .foreground(util::stroke(n(0.6f),
                                             Fill::color(C(0x000000, 0.7f)),
                                             PathFormat::Align::Outer)));
      }
    }
    return e;
  }

  /** A raised plate button — the top plaques, the folder tabs, the bottom
   *  buttons. Same two-stroke bevel everywhere, which is the argument for it
   *  being a primitive rather than a one-screen hack. */
  Element raised(fo::Rect r, float radius = 2.0f) {
    using namespace fo;
    Element e = atR(box(), r).corners(Corners{n(radius)}).fill(tabMat);
    e.foreground(styles::BevelEmboss{n(1.4f), n(1.8f), 118,
                                     C(0xA08858, 0.60f), C(0x0C0906, 0.65f)});
    e.stroke(util::stroke(n(1), Fill::color(C(0x1A1610, 0.9f)),
                          PathFormat::Align::Inner));
    return e;
  }

  /** Engraved gold lettering: the bright face over a 1 px shadow stamp. The
   *  library has no glyph-level stroke (ROADMAP §9), so the doubling is
   *  echo() — one misprint stamp UNDER the run, which is exactly the effect. */
  Element engravedText(const std::string &s, float size, SkColor4f c) {
    return fo::t(s, plaqueType(size, c))
        .echo({fo::n(0.7f), fo::n(0.7f)}, fo::kGoldDim);
  }

  Element centred(fo::Rect r, Element child) {
    Element e = fo::atR(box(), r);
    e.justify(Justify::Center).alignItems(Align::Center);
    e.child(std::move(child));
    return e;
  }

  // =========================================================================
  // 1. The S.P.E.C.I.A.L. column

  Element specialColumn() {
    using namespace fo;
    Element g = box().absolute().inset(0);
    static const char *abbr[7] = {"ST", "PE", "EN", "CH", "IN", "AG", "LK"};
    for (int i = 0; i < 7; ++i) {
      const float y = kStatY[(size_t)i];
      const int value = narg[i];

      // The two-letter gold abbreviation and its dash: measured ink 20..55,
      // 21 px tall, i.e. 2 px below the row's y. A worn engraved stencil.
      g.child(ink(engravedText(std::string(abbr[(size_t)i]) + "-", n(15.5f),
                               kGold)
                      .opacity(0.94f),
                  kAbbrX, y + 1.0f, engravedRise() * 1.42f));

      // The two-digit odometer: BIG_NUM_WIDTH 14 x BIG_NUM_HEIGHT 24, white
      // digits on counter wheels with a seam across the digit. That seam is
      // what makes them read as wheels rather than as a readout, which is why
      // they animate the way they do.
      g.child(odometer(kOdoX, y, value, "", ""));

      // The value descriptor on its dark plaque with the gold bevel along the
      // bottom-left. stat.msg 301-310, indexed by the VALUE.
      Element plaque = at(box(), kPlaqueX, y + 4, kPlaqueW, 17)
                           .fill(wellMat)
                           .corners(Corners{n(1.5f)});
      plaque.stroke(util::stroke(n(1), Fill::color(C(0x14100A)),
                                 PathFormat::Align::Inner));
      g.child(plaque);
      // the gold L: a bar under the plaque running 4 px past its left edge,
      // and a short riser (sampled at y = 59..60, x from 96)
      g.child(at(box(), kPlaqueX - 4, y + 22, kPlaqueW + 6, 2)
                  .fill(Material::linearUnit({0, 0}, {0, 1},
                                             {{0.0f, kParchLit2},
                                              {1.0f, C(0x8C6428)}})));
      g.child(at(box(), kPlaqueX - 4, y + 15, 2, 7).fill(fade(kParchLit, 0.85f)));
      g.child(bodyAt(descriptor(value), kGreen, kDescX, y + 8));
    }
    return g;
  }

  /** A two-digit odometer at (x, y), cell 14 x 24. `blankTens`/`blankOnes`
   *  carry the mechanical blank: an empty string draws the sprite sheet's
   *  blank cell for 123 ms and then the new digit — a counter, not a fade. */
  Element odometer(float x, float y, int value, std::string tensOverride,
                   std::string onesOverride) {
    using namespace fo;
    Element g = at(box(), x - 1, y - 1, kOdoW * 2 + 2, kOdoH + 2)
                    .fill(Fill::color(C(0x000000)))
                    .corners(Corners{n(2)});
    g.foreground(util::stroke(n(1), Fill::color(C(0x0A0A0A)),
                              PathFormat::Align::Inner));
    const std::string digits =
        (value < 10 ? "0" : "") + std::to_string(std::clamp(value, 0, 99));
    for (int c = 0; c < 2; ++c) {
      Element wheel = at(box(), 1 + kOdoW * (float)c, 1, kOdoW - 1, kOdoH)
                          .fill(wheelMat)
                          .corners(Corners{n(2.5f)})
                          .clip();
      std::string glyph(1, digits[(size_t)c]);
      if (c == 0 && !tensOverride.empty())
        glyph = tensOverride == " " ? "" : tensOverride;
      if (c == 1 && !onesOverride.empty())
        glyph = onesOverride == " " ? "" : onesOverride;
      if (!glyph.empty())
        wheel.child(box()
                        .absolute()
                        .inset(0)
                        .justify(Justify::Center)
                        .alignItems(Align::Center)
                        .child(t(glyph, fo::type(digitFace(), n(15.0f), kDigit,
                                                 0, 0.92f))
                                   .translateY(n(0.6f))));
      // The seam ACROSS the digit — the counter-wheel tell, and the reason
      // these things blank for 123 ms instead of cross-fading. It has to paint
      // over the glyph, so it is a foreground, not a background: the exact
      // slot Element::overlay() does NOT cover.
      wheel.foreground(shapes::onEdges(
          shapes::Edge::All,
          util::stroke(n(0.7f), Fill::color(C(0x000000, 0.75f)),
                       PathFormat::Align::Inner)));
      wheel.child(at(box(), 0, kOdoH * 0.5f - 0.6f, kOdoW - 1, 1.2f)
                      .fill(Fill::color(C(0x000000, 0.60f)))
                      .zIndex(3));
      wheel.child(at(box(), 0, kOdoH * 0.5f + 0.6f, kOdoW - 1, 0.7f)
                      .fill(Fill::color(C(0xC0C0C0, 0.22f)))
                      .zIndex(3));
      g.child(wheel);
    }
    return g;
  }

  // =========================================================================
  // 2. Status block — label at x=194, value at x=263, pitch 13. The seven
  // condition rows are INACTIVE (#183018) and that dead grey-green covering
  // most of the panel is a big part of how the screen reads.

  Element statusBlock() {
    using namespace fo;
    Element g = box().absolute().inset(0);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d/%d", stats.hitPoints, stats.hitPoints);
    g.child(bodyAt("Hit Points", kGreen, 194, 46));
    g.child(bodyAt(buf, kGreen, 263, 46));
    static const char *cond[7] = {"Poisoned",           "Radiated",
                                  "Eye Damage",         "Crippled Right Arm",
                                  "Crippled Left Arm",  "Crippled Right Leg",
                                  "Crippled Left Leg"};
    for (int i = 0; i < 7; ++i)
      g.child(bodyAt(cond[(size_t)i], kInactive, 194,
                     46 + kRowPitch13 * (float)(i + 1)));
    return g;
  }

  // =========================================================================
  // 3. Derived block — label at x=194, value at x=288, both LEFT-aligned,
  // pitch 13. Ten rows in gCharacterEditorDerivedStatsMap order.

  Element derivedBlock() {
    using namespace fo;
    Element g = box().absolute().inset(0);
    struct Row {
      const char *label;
      std::string value;
    };
    const std::array<Row, 10> rows = {{
        {"Armor Class", std::to_string(stats.armorClass)},
        {"Action Points", std::to_string(stats.actionPoints)},
        {"Carry Weight", std::to_string(stats.carryWeight)},
        {"Melee Damage", std::to_string(stats.meleeDamage)},
        {"Damage Res.", std::to_string(stats.damageResist) + "%"},
        {"Poison Res.", std::to_string(stats.poisonResist) + "%"},
        {"Radiation Res.", std::to_string(stats.radResist) + "%"},
        {"Sequence", std::to_string(stats.sequence)},
        {"Healing Rate", std::to_string(stats.healingRate)},
        {"Critical Chance", std::to_string(stats.criticalChance) + "%"},
    }};
    for (int i = 0; i < 10; ++i) {
      const float y = 179 + kRowPitch13 * (float)i;
      g.child(bodyAt(rows[(size_t)i].label, kGreen, 194, y));
      g.child(bodyAt(rows[(size_t)i].value, kGreen, 288, y));
    }
    return g;
  }

  // =========================================================================
  // 4. Level / Exp / Next Level — "%s %d" with the label ending in a colon,
  // pitch 11. The thousands separators come from the game's own _itostndn().

  Element levelBlock() {
    using namespace fo;
    Element g = box().absolute().inset(0);
    g.child(bodyAt("Level: " + std::to_string(kLevel), kGreen, 32, 280));
    g.child(bodyAt("Exp: " + thousands(experienceForLevel(kLevel)), kGreen, 32,
                   291));
    g.child(bodyAt("Next Level: " + thousands(experienceForLevel(kLevel + 1)),
                   kGreen, 32, 302));
    return g;
  }

  // =========================================================================
  // 5. The folder, KILLS tab selected. THE typographically interesting row
  // renderer (characterEditorFolderViewDrawKillsEntry):
  //
  //   name  at (34, y)                          left-aligned
  //   count at (314 - width(count), y)          right-aligned to x = 314
  //   rule  from 34 + width(name) + gap
  //           to 314 - width(count) - gap       a 1 px hairline
  //         at  y + lineHeight/2                the row's vertical midline
  //
  // Three nodes and two setup-time measure() calls per row, because
  // TabStopOptions has no leader. See the report.

  Element folder() {
    using namespace fo;
    Element g = box().absolute().inset(0);

    // The tab strip, blitted at (11, 327). Hit split points x<110 -> PERKS,
    // 110..208 -> KARMA, >208 -> KILLS, so those ARE the tab boundaries.
    static const char *tabs[3] = {"PERKS", "KARMA", "KILLS"};
    const float tabX[4] = {25, 110, 208, 300};
    for (int i = 0; i < 3; ++i) {
      const bool sel = i == 2;
      const float x0 = tabX[i], x1 = tabX[i + 1];
      Element tab = raised({x0, sel ? 327.0f : 330.0f, x1 - x0,
                            sel ? 33.0f : 29.0f},
                           2.5f);
      if (!sel)
        tab.overlay(styles::colorOverlay(C(0x000000, 0.30f)));
      tab.justify(Justify::Center).alignItems(Align::Center);
      tab.child(engravedText(tabs[(size_t)i], n(11.0f),
                             sel ? kGold : C(0x6E5A20))
                    .translateY(sel ? n(-1.0f) : n(0.0f)));
      g.child(tab);
    }

    // Nine leader rows from y = 364, pitch 11, between x = 34 and x = 314.
    for (int i = 0; i < 9; ++i) {
      const Kill &k = kills()[(size_t)i];
      const float y = 364 + kRowPitch11 * (float)i;
      const std::string count = std::to_string(k.count);
      const float nameW = killNameW[(size_t)i] / kScale;   // back to orig px
      const float countW = killCountW[(size_t)i] / kScale;
      const float gap = 1.0f; // fontGetLetterSpacing() at this size
      g.child(bodyAt(k.name, kGreen, 34, y));
      g.child(bodyAt(count, kGreen, 314 - countW, y));
      const float x0 = 34 + nameW + gap * 2;
      const float x1 = 314 - countW - gap * 2;
      if (x1 > x0)
        g.child(at(box(), x0, y + kRowPitch11 * 0.5f - 1.0f, x1 - x0, 1)
                    .fill(Fill::color(fade(kGreen, 0.85f))));
    }

    // The scroll arrows at x = 317 (characterEditorFolderViewClear).
    for (int i = 0; i < 2; ++i) {
      const bool up = i == 0;
      Element a = at(box(), 317, up ? 361.0f : 456.0f, 11, 12)
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0x50432E)},
                                                  {1.0f, C(0x2C2418)}}))
                      .corners(Corners{n(1)});
      a.foreground(util::stroke(n(0.8f), Fill::color(C(0x1A1610)),
                                PathFormat::Align::Inner));
      a.child(at(box(), 2, 3, 7, 6)
                  .fill(Fill::color(fade(kGold, 0.9f)))
                  .outline([up](SkSize s) {
                    SkPathBuilder b;
                    if (up) {
                      b.moveTo(s.width() * 0.5f, 0);
                      b.lineTo(s.width(), s.height());
                      b.lineTo(0, s.height());
                    } else {
                      b.moveTo(0, 0);
                      b.lineTo(s.width(), 0);
                      b.lineTo(s.width() * 0.5f, s.height());
                    }
                    b.close();
                    return b.detach();
                  }));
      g.child(a);
    }
    return g;
  }

  // =========================================================================
  // 6. The skills column. Eighteen rows, pitch 11: name at x = 380, value at
  // x = 573, "%d%%" LEFT-ALIGNED at 573 — so 6% and 71% start at the same x
  // and the column is ragged on the right. That is authentic; do not
  // right-align it. Tagged skills are #A0A0A0, untagged #3CF800, the selected
  // row #FCFC7C with NO tween — the game has none.

  Element skillsColumn() {
    using namespace fo;
    Element g = box().absolute().inset(0);
    for (int i = 0; i < 18; ++i) {
      const float y = 27 + kRowPitch11 * (float)i;
      const SkColor4f c = i == selected ? kSelected
                          : tagged[(size_t)i] ? kTagged
                                              : kGreen;
      int pct = skillPct[(size_t)i];
      if (i == selected)
        pct += 2 * presses; // 1 point = 2% on a tagged skill (editor.msg {500})
      g.child(bodyAt(skills()[(size_t)i].name, c, 380, y));
      g.child(bodyAt(std::to_string(pct) + "%", c, 573, y));
    }
    // The +/- slider graphic, blitted at (592, selectedIndex*11 + 27 + 16),
    // its buttons at x = 614.
    const float sy = (float)selected * kRowPitch11 + 27 + 16;
    Element slider = at(box(), 592, sy - 12, 36, 24)
                         .fill(tabMat)
                         .corners(Corners{n(2)});
    slider.foreground(styles::BevelEmboss{n(1.2f), n(1.4f), 118,
                                          C(0xA08858, 0.5f),
                                          C(0x0C0906, 0.6f)});
    for (int k = 0; k < 2; ++k) {
      Element btn = at(box(), 22, 2 + 11.0f * (float)k, 12, 9)
                        .fill(Fill::color(k == 0 ? C(0x3A3020) : C(0x2A2418)))
                        .corners(Corners{n(1.5f)});
      btn.foreground(util::stroke(n(0.8f), Fill::color(C(0x8A7448, 0.7f)),
                                  PathFormat::Align::Inner));
      if (k == 0) // the `+` lamp flashes on each spend
        btn.child(box().absolute().inset(0).fill(Fill::color(kLampOn))
                      .corners(Corners{n(1.5f)})
                      .opacity(&plusFlash));
      btn.child(box()
                    .absolute()
                    .inset(0)
                    .justify(Justify::Center)
                    .alignItems(Align::Center)
                    .child(t(k == 0 ? "+" : "-",
                             fo::type(bodyBold(), n(7.0f), kGold))));
      slider.child(btn);
    }
    slider.child(at(box(), 2, 6, 16, 12)
                     .fill(Fill::color(C(0x141008)))
                     .corners(Corners{n(1)}));
    g.child(slider);
    return g;
  }

  // =========================================================================
  // 7. The description card — the typeset hero.
  //
  //   title    font 102 at (348, 272)
  //   formula  font 101 at (348 + width(title) + 8, 286) — the two runs share
  //            a bottom edge at ~296. Fallout hand-computed that as
  //            268 + lineHeight(102) - lineHeight(101); done here by the same
  //            arithmetic over the substituted faces' measured advances.
  //   rule     TWO 1-px lines at y = 300 and 301, x 348..613
  //   figure   blitted at (484, 309), ~101x128
  //   body     font 101, first line top y = 315, pitch 11
  //
  // THE WRAP WIDTH IS A SILHOUETTE FLOAT (character_editor.cc):
  //     inkLeft = min over the illustration's rows of the leftmost dark column
  //     extra   = max(inkLeft - 8, 0)
  //     wrap    = 136 + extra
  // With inkLeft = 39 that is 167, so the column runs 348..515 and stops 8 px
  // short of the first ink. flowAround(key, margin) is the library's
  // equivalent: it excludes the keyed node's resolved BOX, so the keyed node
  // here is sized to the INK, not to the illustration — and then the two rules
  // agree EXACTLY, because Fallout's scan takes a single global minimum and
  // therefore also produces one constant wrap width for every line. See the
  // report for where they stop agreeing.

  fo::Pose poseFor(int skill) const {
    if (skill == 0)
      return {48, 44, 0.28f, 0.55f, 1};  // Small Guns — rifle, narrow
    if (skill == 7)
      return {31, 70, 0.62f, 1.15f, 2};  // Doctor — one arm raised, wide
    return {39, 62, 0.42f, 0.85f, 0};    // Melee Weapons
  }

  Element cardContent(int skill) {
    using namespace fo;
    const SkillDef &d = skills()[(size_t)skill];
    const Pose pose = poseFor(skill);
    Element g = box().absolute().inset(0);

    // ---- title + formula on a shared bottom edge -------------------------
    // Fallout puts the formula at 268 + lineHeight(102) - lineHeight(101), a
    // hand-computed baseline alignment across a 28 px and a 10 px face. Same
    // arithmetic here, over the substituted faces' own measured metrics —
    // alignItems(Align::Baseline) would be the kernel spelling, but the two
    // runs are absolutely positioned at documented x/y, not laid out in a row.
    g.child(ink(t(d.name, titleStyle()), 348 - 345, 272 - 267, titleRise()));
    const int walkIdx = skill == 0 ? 0 : (skill == 7 ? 1 : 2);
    const float advance = titleAdvance[walkIdx] / kScale;
    g.child(bodyAt(d.formula, kInk, 348 - 345 + advance + 8, 286 - 267));

    // ---- the rule: two 1-px lines at y = 300, 301 ------------------------
    g.child(at(box(), 348 - 345, 300 - 267, 613 - 348, 2)
                .fill(Fill::color(kInk)));

    // ---- the illustration, and the FLOAT the copy clears ----------------
    // The exclusion is the keyed node's resolved BOX. Fallout's exclusion is
    // one number — the leftmost inked column, taken as a global minimum over
    // the whole bitmap — which makes its "silhouette float" a constant wrap
    // width for every line, i.e. exactly a rectangle from that column to the
    // right margin. So the keyed node is that rectangle: first ink -> the
    // column's right edge, full illustration height. With margin 8 the copy
    // stops at 484 + inkLeft - 8 and the two rules agree to the pixel.
    // Flowing around the FIGURE's own box instead strands words in the gap to
    // its right, which is correct DTP behaviour and wrong for 1998 — see the
    // report.
    const float inkX = 484 - 345 + pose.inkLeft;
    g.child(at(box(), inkX, 309 - 267, (613 - 345) - inkX, 128).key("card-ink"));
    Element figNode =
        at(box(), inkX, 309 - 267, pose.inkWidth, 128);
    figNode.outline(figure(pose));
    // PathFormat exposes no cap or join, so these open contours end square and
    // mitre at the joints — fine for a 1998 blit, and noted in the report.
    figNode.stroke(util::stroke(n(1.15f), Fill::color(kInk),
                                PathFormat::Align::Center));
    g.child(figNode);

    // ---- the body. Greedy first-fit, ragged right, forced 11 px pitch. ---
    // Fallout's wordWrap() is greedy with no hyphenation and the ragged edge
    // is what the screen looks like, so this is deliberately NOT justified and
    // NOT hyphenated. What the screen tests is forced leading, a computed wrap
    // width, a silhouette float, and mixed-size baseline alignment.
    weave::ParagraphBuilder pb(body(kInk));
    pb.addText(toU8(d.blurb));
    cardPara = std::make_shared<weave::Paragraph>(pb.build());
    weave::ParagraphLayoutOptions opts;
    opts.alignment = weave::TextAlignment::kStart;
    opts.lineBreakStrategy = weave::LineBreakStrategy::kGreedy;
    opts.hyphenation.enabled = false;
    opts.lineMetrics.height = n(kRowPitch11); // the forced 11 px pitch, x2
    g.child(text(cardPara, opts)
                .absolute()
                .left(Dim(n(348 - 345)))
                .top(Dim(n(315 - 267) - n(1.5f)))
                .width(Dim(n(613 - 348)))
                .flowAround("card-ink", n(8)));
    return g;
  }

  Element card() {
    using namespace fo;
    Element c = at(box(), 345, 267, 277, 170)
                    .fill(parchMat)
                    .clip()
                    .corners(Corners{n(1)});
    c.overlay(styles::Overlay{parchTooth, SkBlendMode::kSoftLight, 0.55f});
    // creases: two diagonal slivers and one bottom-right scuff. Study
    // crop_card.png — the creases are what sell it as a stuck-on scrap.
    c.child(at(box(), -40, -20, 60, 260)
                .rotate(-16.0f)
                .translateX(n(120))
                .fill(Material::linearUnit({0, 0}, {1, 0},
                                           {{0.0f, fade(kRust, 0.0f)},
                                            {0.5f, fade(kRust, 0.16f)},
                                            {1.0f, fade(kRust, 0.0f)}})));
    c.child(at(box(), -40, -20, 34, 260)
                .rotate(9.0f)
                .translateX(n(232))
                .fill(Material::linearUnit({0, 0}, {1, 0},
                                           {{0.0f, fade(C(0x7C581C), 0.0f)},
                                            {0.5f, fade(C(0x6A4A18), 0.20f)},
                                            {1.0f, fade(C(0x7C581C), 0.0f)}})));
    c.child(at(box(), 150, 120, 130, 55)
                .fill(Material::radialUnit({0.55f, 0.75f}, 1.0f,
                                           {{0.0f, fade(kParchScuff, 0.30f)},
                                            {1.0f, fade(kParchScuff, 0.0f)}})));
    c.child(at(box(), -6, -10, 60, 190)
                .fill(Material::linearUnit({0, 0}, {1, 0},
                                           {{0.0f, fade(C(0x5A3C10), 0.28f)},
                                            {1.0f, fade(C(0x5A3C10), 0.0f)}})));
    // the scrap's own soiling — kept light: the reference card is bright ochre
    // right into its corners
    c.child(box().absolute().inset(0).fill(
        Material::radialUnit({0.46f, 0.42f}, 1.35f,
                             {{0.0f, fade(C(0x2A1C08), 0.0f)},
                              {0.70f, fade(C(0x2A1C08), 0.04f)},
                              {1.0f, fade(C(0x2A1C08), 0.22f)}})));
    c.child(at(box(), 178, 118, 110, 60)
                .fill(Material::radialUnit({0.60f, 0.85f}, 1.0f,
                                           {{0.0f, fade(C(0x3A2A12), 0.18f)},
                                            {1.0f, fade(C(0x3A2A12), 0.0f)}})));
    c.stroke(util::stroke(n(1.5f), Fill::color(C(0x2A1C08, 0.75f)),
                          PathFormat::Align::Inner));
    c.child(box().absolute().inset(0).child(slot("card")));
    return c;
  }

  // =========================================================================
  // 8. Bezel, plaques, buttons

  Element chrome() {
    using namespace fo;
    Element g = box().absolute().inset(0);

    // The outer frame and the dividers between the five panels — raised
    // facets lit from the top-left.
    g.child(at(box(), 0, 0, 640, 480)
                .foreground(util::stroke(n(3), Fill::color(C(0x1E1810)),
                                         PathFormat::Align::Inner))
                .foreground(shapes::inset(
                    n(3), styles::BevelEmboss{n(2.0f), n(2.5f), 118,
                                              C(0xA08858, 0.45f),
                                              C(0x0C0906, 0.55f)})));
    // vertical divider between the left/middle block and the skills column
    g.child(at(box(), 328, 0, 4, 480)
                .fill(Material::linearUnit({0, 0}, {1, 0},
                                           {{0.0f, C(0x554430)},
                                            {1.0f, C(0x241D12)}})));
    g.child(at(box(), 165, 30, 3, 240)
                .fill(Material::linearUnit({0, 0}, {1, 0},
                                           {{0.0f, C(0x4E4030)},
                                            {1.0f, C(0x241D12)}})));
    g.child(at(box(), 5, 318, 320, 3)
                .fill(Material::linearUnit({0, 0}, {0, 1},
                                           {{0.0f, C(0x554430)},
                                            {1.0f, C(0x241D12)}})));

    // Top plaques (measured bright runs at y = 1: 15..153, 155..236, 238..312).
    const char *plaqueText[3] = {"NARG", "AGE 20", "MALE"};
    const Rect plaques[3] = {{14, 0, 140, 26}, {155, 0, 82, 26},
                             {238, 0, 76, 26}};
    for (int i = 0; i < 3; ++i) {
      Element p = raised(plaques[i], 3.0f);
      p.justify(Justify::Center).alignItems(Align::Center);
      p.child(engravedText(plaqueText[i], n(11.5f), kGold));
      g.child(p);
    }

    // The SKILLS heading (font 103, #907824) at (380, 5), and SKILL POINTS at
    // (400, 233) with its own two-digit odometer at (522, 228).
    g.child(ink(engravedText("SKILLS", n(13.0f), kGold), 380, 5,
                engravedRise() * 1.18f));
    // The SKILL POINTS bar: a raised strip carrying the label and the counter,
    // between the skills well and the card.
    g.child(raised({336, 226, 292, 30}, 3.0f));
    g.child(ink(engravedText("SKILL POINTS", n(13.0f), kGold), 400, 233,
                engravedRise() * 1.18f));
    g.child(at(box(), 520, 226, 34, 28)
                .fill(Fill::color(C(0x120E08)))
                .corners(Corners{n(2)})
                .foreground(styles::BevelEmboss{n(1.0f), n(1.4f), 300,
                                                C(0x8A7448, 0.45f),
                                                C(0x000000, 0.6f)}));
    g.child(box().absolute().left(Dim(0)).top(Dim(0)).child(slot("points")));

    // PRINT / DONE / CANCEL at y = 454, each with a red button light. Lamp
    // rects sampled at x 344..355, 457..468, 553..564, y 455..466.
    const char *btn[3] = {"PRINT", "DONE", "CANCEL"};
    const float lampX[3] = {344, 457, 553};
    const float textX[3] = {364, 477, 573};
    for (int i = 0; i < 3; ++i) {
      Element lamp = at(box(), lampX[i], 455, 12, 12)
                         .corners(Corners{n(6)})
                         .fill(Material::radialUnit({0.35f, 0.30f}, 1.1f,
                                                    {{0.0f, C(0xFF6A4A)},
                                                     {0.45f, kLampOn},
                                                     {1.0f, C(0x600000)}}));
      lamp.foreground(util::stroke(n(1.2f), Fill::color(C(0x1A1208)),
                                   PathFormat::Align::Outer));
      if (i == 1) // DONE dims and returns as the `+` is pressed
        lamp.child(box().absolute().inset(0).corners(Corners{n(6)})
                       .fill(Fill::color(kLampOff))
                       .opacity(&lampFlash));
      g.child(lamp);
      g.child(ink(engravedText(btn[i], n(11.0f), kGold), textX[i], 454,
                  engravedRise()));
    }
    return g;
  }

  // =========================================================================

  Element describe() {
    using namespace fo;
    Element root = stack().width(Dim(kScreenW)).height(Dim(kScreenH + kCaptionH));

    // ---- the screen -----------------------------------------------------
    Element screen = box()
                         .absolute()
                         .left(Dim(0))
                         .top(Dim(0))
                         .width(Dim(kScreenW))
                         .height(Dim(kScreenH))
                         .clip()
                         .fill(plateMat);
    // the cast-metal tooth and the rust, as layer elements because
    // Material::blend has no per-layer amount
    screen.child(box().absolute().inset(0).fill(plateTooth)
                     .blend(SkBlendMode::kOverlay).opacity(0.30f)
                     .cache(Cache::Texture));
    screen.child(box().absolute().inset(0).fill(rustMat)
                     .blend(SkBlendMode::kSoftLight).opacity(0.55f)
                     .cache(Cache::Texture));

    screen.child(chrome());

    // The S.P.E.C.I.A.L. column is NOT a well — sampled at (45,60) the
    // reference reads #483828, a LIT metal facet. It is a raised panel with
    // the odometers and plaques recessed into it, and reading it as black was
    // the first pass's biggest error.
    {
      Element sp = atR(box(), kWellSpecial)
                       .corners(Corners{n(4)})
                       .fill(Material::linearUnit({0, 0}, {0.2f, 1},
                                                  {{0.0f, C(0x54462E)},
                                                   {0.35f, kPlateLit},
                                                   {1.0f, C(0x3A3020)}}));
      sp.child(box().absolute().inset(0).fill(plateTooth)
                   .blend(SkBlendMode::kOverlay).opacity(0.34f)
                   .cache(Cache::Texture));
      sp.foreground(styles::BevelEmboss{n(1.6f), n(2.0f), 118,
                                        C(0xB09868, 0.55f),
                                        C(0x080604, 0.70f)});
      sp.stroke(util::stroke(n(1), Fill::color(C(0x1A1610)),
                             PathFormat::Align::Inner));
      for (int i = 0; i < 4; ++i)
        sp.child(util::disc({n((i & 1) ? kWellSpecial.w - 6 : 6.0f),
                             n((i & 2) ? kWellSpecial.h - 6 : 6.0f)},
                            n(2.6f))
                     .fill(rivetMat)
                     .foreground(util::stroke(n(0.6f),
                                              Fill::color(C(0x000000, 0.7f)),
                                              PathFormat::Align::Outer)));
      screen.child(sp);
    }
    screen.child(well(kWellStatus));
    screen.child(well(kWellDerived));
    screen.child(well(kWellLevel));
    screen.child(well(kWellSkills, 3.0f));
    screen.child(well(kWellFolder, 3.0f));

    screen.child(specialColumn());
    screen.child(statusBlock());
    screen.child(derivedBlock());
    screen.child(levelBlock());
    screen.child(folder());
    screen.child(box().absolute().inset(0).child(slot("skills")));
    screen.child(card());

    // POST: none. No bloom, no scanlines, no vignette, no CRT — a 1998 VGA
    // screen captured off a framebuffer has none of that and the restraint is
    // the point. At most a 4% grain so the metal does not read as vector-flat.
    screen.child(box().absolute().inset(0).fill(canvasGrain)
                     .blend(SkBlendMode::kOverlay).opacity(0.04f)
                     .cache(Cache::Texture));
    root.child(screen);

    // ---- the plate caption. NOT part of the artefact: the screen above is
    // exactly 1280x960 and this band sits below it, carrying the audit.
    root.child(captionBand());
    return root;
  }

  Element captionBand() {
    using namespace fo;
    Element band = box()
                       .absolute()
                       .left(Dim(0))
                       .top(Dim(kScreenH))
                       .width(Dim(kScreenW))
                       .height(Dim(kCaptionH))
                       .fill(Material::linearUnit({0, 0}, {0, 1},
                                                  {{0.0f, C(0x0B0D08)},
                                                   {1.0f, C(0x050604)}}));
    band.foreground(shapes::onEdges(
        shapes::Edge::Top,
        util::stroke(2.0f, Fill::color(C(0x3A3020)),
                     PathFormat::Align::Inner)));
    char buf[192];
    std::snprintf(buf, sizeof buf,
                  "SEVEN NUMBERS BECOME SIXTY \xc2\xb7 %d/%d derived values "
                  "match the shipped sheets (Narg, Mingan, Chitsa), trait "
                  "corrections included",
                  sheetAudit.passed, sheetAudit.total);
    auto line = [&](const char *s, float size, SkColor4f c, float y,
                    float track) {
      return text(toU8(s), fo::type(bodyFace(), size, c, track))
          .absolute()
          .left(Dim(30))
          .top(Dim(y));
    };
    band.child(text(toU8("FALLOUT 2 \xc2\xb7 CHARACTER SCREEN \xc2\xb7 BLACK "
                         "ISLE STUDIOS, 1998 \xc2\xb7 640\xc3\x97""480 8-BIT "
                         "INDEXED, REBUILT AT 2\xc3\x97"),
                    fo::type(bodyBold(), 17.0f, kGold, 1.8f))
                    .absolute()
                    .left(Dim(30))
                    .top(Dim(14)));
    band.child(line(buf, 14.5f, kGreen, 41, 0.2f));
    band.child(line("_colorTable[992] REQUESTS #00FF00; the 256-colour VGA "
                    "palette has no pure green, so what reached the CRT is "
                    "#3CF800.",
                    13.0f, C(0x8A8A78), 64, 0.1f));
    band.child(line("Chrome, plaques, rivets, tabs and parchment are "
                    "procedural; the originals are raster FRMs (intrface art "
                    "id 177). The sheet is RE-SET in real faces.",
                    13.0f, C(0x6A6A5A), 84, 0.1f));
    band.child(line("The screen above is exactly 1280\xc3\x97""960 \xe2\x80\x94 "
                    "halve it and it overlays the 1998 capture. This band is "
                    "not part of the artefact.",
                    13.0f, C(0x55554A), 104, 0.1f));
    return band;
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    using namespace fo;
    ctx.canvas(kScreenW, kScreenH + kCaptionH);
    ctx.background(C(0x050604));

    nargTraits.gifted = true;
    nargTraits.heavyHanded = true;

    // ---- the arithmetic, before anything is drawn ----------------------
    sheetAudit = audit();
    stats = derive(narg, nargTraits, kLevel, kToughness);
    tagged[0] = tagged[4] = tagged[5] = true; // Small Guns, Melee, Throwing
    invested[0] = 12;  // 24% on a tagged skill
    invested[4] = 15;  // 30%
    invested[5] = 7;   // 14%
    invested[6] = 6;   // First Aid, untagged: 6 points = 6%
    invested[7] = 6;   // Doctor
    for (int i = 0; i < 18; ++i)
      skillPct[(size_t)i] =
          skillValue(i, narg, nargTraits, tagged[(size_t)i],
                     invested[(size_t)i]);
    pointsEarned = (kLevel - 1) * skillPointsPerLevel(narg, nargTraits, 0);
    pointsSpent = 0;
    for (int i = 0; i < 18; ++i)
      pointsSpent += invested[(size_t)i];

    buildSurfaces();

    // ---- measured type. The body size comes from the face's real advance,
    // not from an assumed em: the reason Fallout's fixed value columns work is
    // that its small font is near-monospaced, so the substitute has to land on
    // the same 5.85 original px per character.
    {
      const float w =
          ctx.measure(t("MMMMMMMMMMMMMMMMMMMM",
                        fo::type(bodyFace(), 100.0f, kGreen)))
              .width();
      if (w > 1.0f)
        bodyEm() = w / 2000.0f;
      // The line-top -> cap-top slack. Fallout's draw y is the top of the
      // glyph cell; SigilWeave's node top is the line box's top, and the two
      // differ by (ascent - capHeight). Taken as a fraction of the measured
      // line height, which lands every row on the reference's own y.
      bodyRise() = std::max(0.0f, ctx.measure(t("H", body(kGreen))).height() *
                                      0.20f);
      // font 102: squeeze the substitute until "Melee Weapons" lands on the
      // reference's ~6.8 original px per character.
      const float raw =
          ctx.measure(t("Melee Weapons",
                        fo::type(titleFace(), n(23.8f), kInk, n(-0.1f))))
              .width();
      if (raw > 1.0f)
        titleCondense() = std::clamp(n(6.8f * 13.0f) / raw, 0.30f, 1.0f);
      titleRise() =
          std::max(0.0f, ctx.measure(t("H", titleStyle())).height() * 0.20f);
      engravedRise() = std::max(
          0.0f, ctx.measure(t("H", plaqueType(n(11.0f), kGold))).height() *
                    0.20f);
    }

    // the leader-row advances: two measures per row, nine rows.
    for (int i = 0; i < 9; ++i) {
      killNameW[(size_t)i] =
          ctx.measure(t(kills()[(size_t)i].name, body(kGreen))).width();
      killCountW[(size_t)i] =
          ctx.measure(t(std::to_string(kills()[(size_t)i].count),
                        body(kGreen)))
              .width();
    }
    // the card title advances, for the formula's hand-computed x
    {
      const int idx[3] = {0, 7, 4};
      for (int k = 0; k < 3; ++k)
        titleAdvance[k] =
            ctx.measure(t(skills()[(size_t)idx[k]].name, titleStyle())).width();
    }

    ctx.ticker.add([this](double dt) {
      step(dt);
      return true;
    });

    ctx.composer.render(describe());
    pushSlots(ctx, true);
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    now = elapsed;
    pushSlots(ctx, false);
  }

  // ---- the three discrete-state slots -------------------------------------
  // Everything continuous is bound; these three change CONTENT, which no
  // PropValue can carry (ROADMAP §9, last bullet).
  int shownSelected = -1, shownPresses = -1;
  std::string shownTens = "?", shownOnes = "?";

  void pushSlots(sketch::SketchContext &ctx, bool force) {
    if (force || selected != shownSelected || presses != shownPresses) {
      const bool cardChanged = force || selected != shownSelected;
      shownSelected = selected;
      shownPresses = presses;
      ctx.composer.renderSlot("skills", skillsColumn());
      if (cardChanged)
        ctx.composer.renderSlot("card", cardContent(selected));
    }
    if (force || odoTens != shownTens || odoOnes != shownOnes) {
      shownTens = odoTens;
      shownOnes = odoOnes;
      ctx.composer.renderSlot(
          "points", odometer(522, 228, pointsEarned - pointsSpent - presses,
                             odoTens, odoOnes));
    }
  }

  // =========================================================================
  // The frame loop. THE REFERENCE DOES NOT IDLE — the only motion on this
  // screen is interaction, and holding that line is part of the study.

  void step(double) {
    const double t = std::fmod(now, kLoop);

    // ---- the selection walk. The row recolours INSTANTLY: there is no tween
    // in the game, and the card's title, formula, body and figure swap with
    // it at 0 ms (Fallout plays a sound here and draws no transition).
    int sel = kWalk[2];
    if (t < kDwell)
      sel = kWalk[0];
    else if (t < 2 * kDwell)
      sel = kWalk[1];
    if (sel != selected) {
      selected = sel;
      presses = 0;
    }

    // ---- three `+` presses on Melee Weapons: +2% each, SKILL POINTS 04 -> 01
    int p = 0;
    if (selected == kWalk[2])
      for (int i = 0; i < 3; ++i)
        if (t >= kSpend0 + kSpendGap * (double)i)
          p = i + 1;
    presses = p;

    // ---- the odometer flip. On each change the ONES digit is replaced by
    // the sprite sheet's blank cell, held 123 ms, then the new digit; then, if
    // the tens digit changed, the same for tens. A mechanical counter.
    const int value = pointsEarned - pointsSpent - presses;
    const int prevValue = pointsEarned - pointsSpent - std::max(0, presses - 1);
    double sinceChange = 1e9;
    if (presses > 0 && selected == kWalk[2])
      sinceChange = t - (kSpend0 + kSpendGap * (double)(presses - 1));
    const int tensNow = (value / 10) % 10, onesNow = value % 10;
    const int tensPrev = (prevValue / 10) % 10;
    odoTens = std::to_string(tensNow);
    odoOnes = std::to_string(onesNow);
    if (sinceChange >= 0 && sinceChange < kBlank)
      odoOnes = " "; // blank cell
    else if (tensNow != tensPrev && sinceChange >= kBlank &&
             sinceChange < 2 * kBlank)
      odoTens = " ";

    // ---- the `+` lamp and the DONE lamp: 90 ms, invented and minimal
    const float flash =
        (sinceChange >= 0 && sinceChange < 0.09) ? 1.0f : 0.0f;
    plusFlash = flash;
    lampFlash = flash;
  }
};

SIGIL_SKETCH(Fallout2CharSheet)
