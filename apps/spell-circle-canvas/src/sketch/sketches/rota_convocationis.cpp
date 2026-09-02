// rota_convocationis.cpp — A MAGIC CIRCLE THAT ASSEMBLES ITSELF AND THEN
// IGNITES: an invented summoning circle in the ANIME idiom — a dense,
// twelve-fold mandala that fills its disc edge to centre, built band by
// band on one declared schedule and lit at the end, run as the text
// engine's heaviest single sheet.
// =============================================================================
// SUBJECT  The spell circle as a CONSTRUCTION SEQUENCE. The screen circles
//          are drawn as machinery — concentric rules that group into bands,
//          a script register that girds each band, star polygons nested
//          inside one another, a ring of small seals riding the rim and
//          carried round it, everything turning at its own rate — and this
//          study performs the order that machinery would be assembled in
//          rather than presenting the finished plate: the rules are
//          struck, each band is written, the stars are inscribed, the fire
//          runs round the rim seal by seal, and the emblem at the centre
//          resolves last. The circle is INVENTED. It copies no specific
//          circle and claims none.
//
// -----------------------------------------------------------------------------
// FROM THE RECORD — what was studied, and what each thing showed
//
//   The references here are screen circles, studied for their GEOMETRY —
//   how the parts are placed relative to one another — not for their
//   lighting, which is a separate register recorded further down.
//
//   * FULLMETAL ALCHEMIST's transmutation circles are the clearest
//     statement of the FRAMING RULE: every band of script or symbol sits
//     SANDWICHED between two ring rules, never floating on the ground.
//     The array is a small number of concentric circles whose gaps do the
//     work, and the figure inside — a triangle, a hexagram, a double
//     square — is inscribed so that its vertices touch a circle and its
//     chords are tangent to the next circle in. Nothing is placed by eye;
//     every radius is a consequence of the figure above it.
//   * AKASHIC RECORDS' spell circles show what DENSITY means in this
//     idiom: several script bands at once, at three or four different
//     sizes, so the disc reads as continuous texture from the rim inward
//     with no empty annulus anywhere. The bands are thin relative to the
//     radius and there are many of them; the script is script-shaped
//     rather than readable, and the small size of the innermost register
//     is what makes the plate look like a mechanism rather than a poster.
//   * FULLMETAL ALCHEMIST again, for two moves this study takes whole.
//     PARTIAL ARCS: crescents of a pair of concentric arcs, spanning a
//     sixth of the circle, three of them, laid across the figure and
//     belonging to no ring. And the OFF-ORDER MARK: on the flame array a
//     perfect hexagram is broken by exactly one medallion straddling the
//     rim and one glyph in the lower crescent. The exception is what makes
//     the rest read as a drawing rather than as a pattern, and one is
//     enough.
//   * THE COMMON GEOMETRY of the genre, which both of those and the
//     procedural "magic circle generator" culture agree on: rotational
//     symmetry of twelve or a divisor of it, with DIFFERENT BANDS ON
//     DIFFERENT ORDERS and every ladder a multiple of the base; SEVERAL
//     star polygons rather than one, nested at different radii, each
//     inscribed a little inside the ring it belongs to, and turning
//     against each other; a copy of a figure rotated half a step as the
//     standard way to double it; SMALL CIRCLES at every star vertex, of
//     the order of a hundredth of the radius, and SATELLITE SEALS an
//     order larger riding the OUTERMOST ring — the sub-circles sit at the
//     edge of the disc, not in the middle of it, and each is a small
//     magic circle carrying its own ring of script; radial spokes that stop
//     short of both ends of the annulus they cross; tick ladders offset
//     half a step from the nodes so the two interleave; broken and dashed
//     rings at something under half duty; two rings of script, the inner
//     one running the other way; and a single emblem at the centre inside
//     a fifth of the radius, nested down in two or three steps rather than
//     stopping dead.
//
//   The TREATMENT — light standing where ink would stand — is quoted from
//   a different set: Ufotable's summoning circles for the white-hot core
//   inside a saturated halo, Fullmetal Alchemist again for chalk that is
//   inert until it ignites, the Clow emblem for a centre that is the
//   brightest stable thing on the plate, and the compositing craft of the
//   motion-graphics tutorial genre for the way a glow is built by adding
//   a blurred copy of the line to the line.
//
// THIS STUDY'S OWN, flagged rather than smuggled:
//   * The circle itself. This arrangement of those principles is not any
//     circle that has been drawn: the radius table, the two nested star
//     compounds ({12/3} outside {12/4}, three squares outside four
//     triangles), the three twelve-fold spoke sets, the twelve seals on
//     the rim, the arc pair offset half a pitch, and every colour, radius
//     and millisecond are composition. FOUR RADII ARE NOT FREE — the
//     circle under each compound is where that compound's chords run
//     tangent, and a seal's size and station radius are the span from the
//     register's inner rule to a lip past the greatest circle. Both are
//     the reference rule that a figure decides the radius beneath it
//     rather than being fitted to one.
//   * The CONTENT is invented outright and quotes nothing. The Latin of
//     the invocation is this study's; the nine names are made up; the
//     rune register is an invented alphabet dealt from a seeded walk over
//     Tifinagh letterforms, chosen because their vocabulary of circles,
//     crosses and bars is the vocabulary a spell circle already speaks.
//     It is not Tamazight and does not spell anything.
//   * The ASSEMBLY is the subject and is invented: nothing animates on a
//     reference plate. The order performed here — rules, invocation,
//     register, names, texture, arcs, stars, seals, emblem, ignition — is
//     a plausible working order, not a documented one.
//
// -----------------------------------------------------------------------------
// THE MACHINE, and what it puts under load
//
//   * TWENTY-THREE TEXT-ON-PATH LEAVES at once: four full rings, twelve
//     seal rings, and seven leaves whose baseline is a POLYGON'S CHORDS as
//     separate OPEN contours, so caption k is addressed by (k+0.5)/n of
//     one arc-length coordinate and needs no placement of its own. Ring
//     sizes are FITTED, not guessed: each ring's string is measured
//     straight and its type sized so the run girds its own circumference —
//     which is why a seal's ring is written to a MARK COUNT: on a circle
//     that small the number of marks is the only handle on their size.
//   * THE SCHEDULE IS DECLARED, THEN READ BACK, never restated: every
//     stage's window is computed from `Stagger::spanMs` at declare time —
//     the seals chain each start off the span of the cascade before it —
//     and the scribe point that leads the invocation is placed every
//     frame from `Composer::beatsOf`, so the dot and the letters cannot
//     disagree about where the pen is.
//   * THE INVOCATION'S CASCADE IS A CUE TABLE: one start per word,
//     stepping at writing pace and PAUSING at every cross — irregular
//     timing no even spread expresses — with a cluster cascade nested
//     inside each word's beat and `fx::hold` vetoing every glyph whose
//     beat has not opened.
//   * ONE BAND NEVER SETTLES: the register's shimmer is a LOOPING
//     cascade, so a crest of light re-opens on every glyph's own cycle
//     forever, driven by a wrapping phase — declared continuity rather
//     than a one-shot re-run.
//   * THE CHARGE IS ONE PASS, not per-glyph paint: `fx::pass` renders the
//     nine names into a layer and one SkSL pass blooms each name on its
//     own cascade clock, riding a baseline that is itself a moving
//     marquee. The pass DECLARES WHERE IT RESTS, so it stays mounted for
//     the whole loop and costs nothing outside its window on a ring that
//     repaints for the whole loop because it orbits.
//   * THE LIGHT IS UNIONED BEFORE IT IS ADDED: each lighting group's
//     whole geometry is stroke-expanded and merged into one region at
//     four widths, so an additive stack cannot print a crossing at twice
//     the brightness of the lines that cross there — and a circle of this
//     density is nearly all crossings. SigilGeometry is the geometry
//     PRODUCER here — booleans, roughening and a resample-and-lerp morph
//     — and compose consumes what it produces as ordinary comparable
//     silhouette values. That boundary does not move: compose does not
//     link SigilGeometry, and this file does.
//   * A LIGHTING GROUP IS A LAYER THAT TURNS TOGETHER. Nothing in one
//     group crosses anything in another, which is what lets seven groups
//     rotate at seven rates over one baked union each.
//   * MIXED REGISTERS on one wheel: majuscule Latin on the grotesque that
//     carries GRAD (so the ignition can swell the names' weight with no
//     reshape), and two invented rune registers running from a monogram
//     fifty pixels tall down to a grain too small to read as letters.
//   * A TURNING RING OF TURNING SEALS, which is the one arrangement the
//     framework's placement rule has to be right about: the carrier is a
//     bound rotation ABOVE twelve text leaves, each seal a second bound
//     rotation above its own, so every glyph on the rim lands somewhere
//     else next frame. A run declares that from the transforms over it
//     and takes subpixel origins for as long as they are live, which is
//     what keeps a ring of moving letterforms from standing still and
//     then hopping a whole pixel letter by letter.
//   * THE LOAD, deliberately: per-glyph alpha, rotation, colour
//     multiplier, colour screen and axis coordinate all varying at once
//     across every glyph of the twenty-three curved baselines; ten layers
//     counter-turning at ten rates over cached recordings, and two more
//     inside each seal; an ember pool stamped as one draw; and — for
//     the ignition's own window — a full-band shader pass, a radial-ray
//     material, a chromatic backdrop and a screened flood, every one of
//     them gated so that a gain of zero is a node the painter never
//     reaches.
//
// EDIT THESE FIRST
//   kStepMs / kCrossMs — the writing pace of the invocation, and the
//               scribe's pause at each cross. The whole timeline
//               downstream moves with them, because every later window is
//               chained off this cascade's span.
//   kSealBeat — how much of a seal's forming overlaps the next one's
//               start. 1.0 is strictly one at a time; lower hands the eye
//               on earlier. Twelve seals want a lower number than six
//               did: at 1.0 a ring of twelve is a roll call, and what the
//               rim should read as is a fuse.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/rota_convocationis.cpp \
//       --frame /tmp/rota_convocationis.png --at 14.3
//
//   ~1.7 s  the compass: the rules and the rim's serration sweep on
//   ~3.6 s  the invocation mid-write, the scribe point leading the letters
//   ~5.5 s  the register lands and begins its endless shimmer
//   ~7.0 s  the twelve arcs and the dashed ring struck
//   ~9.2 s  the star compounds — a scribble of light resolving onto them
//  ~14.3 s  the seal cycle: the fire half way round the rim, seven lit,
//           one forming, and the ring already off its stations
//  ~17.0 s  the emblem resolved, the hub burning as the brightest thing
//  ~18.8 s  ignition's crest — rays, flood, embers, a colour fringe
//  ~23.5 s  the hum: twelve lit seals riding a turning rim, each turning
//           on its own centre, over a circle breathing and drizzling
//  loops on a dark sheet at 25.4 s.

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/core/Sdf.h>
#include <sigilcompose/instances/Instances.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/core/Material.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Style.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace ch = choreograph;

namespace {

// A magic circle wants a square field: the figure is the picture, and a
// panoramic sheet would only be two dark wings either side of it.
constexpr float kW = 1280.0f;
constexpr float kH = 1280.0f;

// ---- palette: chalk by candlelight, then one hue of light -----------------
constexpr SkColor4f kNight = hex(0x0A0812);
constexpr SkColor4f kNightLift = hex(0x141021);
constexpr SkColor4f kGold = hex(0xD8A94E);
constexpr SkColor4f kBone = hex(0xE9DFC8);
constexpr SkColor4f kEmber = hex(0x8A4A26);
constexpr SkColor4f kIron = hex(0x3B3554);     // construction lines
constexpr SkColor4f kIronDim = hex(0x262238);  // faint construction
constexpr SkColor4f kAsh = hex(0x8A8299);      // secondary type
constexpr SkColor4f kAshDim = hex(0x8A8299, 0.62f);
constexpr SkColor4f kRuneInk = hex(0x9C8FB8);  // the register's ink

// ---- the ignited palette: ONE hue family, value doing the drawing --------
// Not a second scheme beside the candlelit one. It is the state the circle
// changes INTO: chalk is pigment on a surface and reflects what the room
// gives it, light is emitted and collapses to one hue with a white core.
constexpr SkColor4f kCore = hex(0xFFF6E2);   // the white-hot core
constexpr SkColor4f kHalo = hex(0xFFC152);   // the saturated halo
constexpr SkColor4f kBloom = hex(0xC96F1E);  // the wide dim bloom

// ---- the circle's frame ---------------------------------------------------
constexpr SkPoint kEye{640.0f, 640.0f};  // centre in canvas px
constexpr float kR = 545.0f;             // the greatest circle

// THE RADIUS TABLE, in units of the greatest circle. Read it as the bands
// it makes, because that is how it was built: a band is a PAIR OF RULES
// with something written or drawn between them, and no rule stands alone.
// Two radii are not free — they are where a star's chords run tangent, so
// the figure inside decides where the circle under it goes. Two more are
// the seal ride below, which the same rule sets from this table's own
// fences.
constexpr float rEdge = 1.000f;      // ─┐ the serration: 240 teeth  ─┐
constexpr float rEdgeIn = 0.980f;    // ─┘                            │ THE
constexpr float rRuneOut = 0.958f;   // ─┐ THE REGISTER — the rune    │ SEAL
constexpr float rRune = 0.934f;      //   │ band, and its baseline    │ RIDE
constexpr float rRuneIn = 0.910f;    // ─┘                           ─┘
constexpr float rVoxOut = 0.898f;    // ─┐ THE INVOCATION — Latin majuscule
constexpr float rVox = 0.872f;       //   │
constexpr float rVoxIn = 0.846f;     // ─┘
constexpr float rTickOut = 0.836f;   // ─┐ the division ladder, three classes
constexpr float rTickMid = 0.818f;   // ─┤ …fenced like any other band, with
constexpr float rTickIn = 0.800f;    // ─┘ the turning ladder in its own half
constexpr float rNomOut = 0.790f;    // ─┐ THE NINE NAMES — the charged band
constexpr float rNom = 0.752f;       //   │
constexpr float rNomIn = 0.708f;     // ─┘
constexpr float rNomCase = 0.694f;   //   its hairline companion
constexpr float rArcOut = 0.676f;    // ─┐ the arc layer: two broken rings,
constexpr float rArcIn = 0.640f;     // ─┘ offset half a pitch, spoked
constexpr float rTexOut = 0.622f;    // ─┐ THE TEXTURE — the small register
constexpr float rTex = 0.598f;       //   │
constexpr float rTexIn = 0.572f;     // ─┘
constexpr float rTexCase = 0.560f;   // ─┐ a fine serration, the texture
constexpr float rSerrIn = 0.534f;    // ─┘ band's own companion ladder
constexpr float rStar = 0.520f;      // {12/3} vertices
constexpr float rCrescOut = 0.492f;  // ─┐ THREE crescents, 120° apart, laid
constexpr float rCrescIn = 0.454f;   // ─┘ ACROSS the compound: the one
                                     //   three-fold mark on the plate
constexpr float rEnv = 0.368f;       // ─┐ where the {12/3} chords run
constexpr float rEnvIn = 0.356f;     // ─┘ tangent: a circle it decides
constexpr float rInner = 0.348f;     // {12/4} vertices
constexpr float rHubOut = 0.174f;    // ─┐ where the {12/4} chords run
constexpr float rHubIn = 0.160f;     // ─┘ tangent: the hub's own rule
constexpr float rHubCase = 0.150f;   //   the emblem's outermost line
constexpr float rEmblem = 0.140f;    //   the disc of light
constexpr float rHexagram = 0.118f;  //   the emblem's own figure
constexpr float rHubKern = 0.104f;   //   …and the ring its chords enclose:
                                     //   three nested circles, so the centre
                                     //   steps down as the plate does rather
                                     //   than stopping dead at one line
constexpr float kSpurR = 23.0f;      // the one off-order mark, px

// THE SEAL RIDE — where the twelve sub-seals stand, and how big they are.
// Both numbers are read off the bands rather than chosen: a seal's inner
// edge lands on the register's inner rule and its outer edge stands proud
// of the greatest circle, so ONE SEAL IS EXACTLY AS TALL AS THE GROUND IT
// STANDS ON — the register and the serration together — and breaks the
// rim on its way out. That is the reason the seals are out here and not
// at mid radius: the rim is where a small circle can cover a whole band
// group without touching anything that is meant to be read, and the band
// it covers is script-shaped rather than readable. The invocation, one
// fence further in, is never crossed.
constexpr float rSealLip = 1.032f;  // how far a seal stands past rEdge
constexpr float rSealRide = (rRuneIn + rSealLip) * 0.5f;    // seal centres
constexpr float kSealR = (rSealLip - rRuneIn) * 0.5f * kR;  // ≈33 px
constexpr float kSealRing = kSealR * 0.78f;  // its ring-text baseline

// ---- the writing pace -----------------------------------------------------
constexpr float kStepMs = 55.0f;    // word to word round the band
constexpr float kCrossMs = 190.0f;  // the scribe's extra pause at a cross
constexpr float kSealBeat = 0.55f;  // fraction of a seal's span before the
                                    // next one starts forming

constexpr float kDeg = 3.14159265358979f / 180.0f;
constexpr int kStations = 12;        // the circle's rotational symmetry
constexpr int kSeals = kStations;    // …and the seals', one per station
constexpr int kLimens = kSeals / 2;  // the thresholds named on the chords
constexpr int kEmbers = 96;  // the rim's rising sparks, then the drizzle

// ---- content: invented, and invented in the open --------------------------

/** THE REGISTER — an invented alphabet, not a language.
 *
 *  The letterforms are Tifinagh's (U+2D30), taken for their shapes alone:
 *  a vocabulary of rings, crosses, bars and dotted figures, which is the
 *  vocabulary a circle of this kind already speaks. The subset below
 *  drops the forms that read as Latin letters, because a stray E in a
 *  rune band is the one thing that breaks it.
 *
 *  Words are DEALT rather than written: a seeded walk gives every band its
 *  own text, deterministically, so the plate is the same plate on every
 *  run and on every machine. An invented script has no orthography to be
 *  wrong about; a plate that re-dealt itself would not be one plate. */
const char32_t kRegister[] = {
    U'ⴰ', U'ⴱ', U'ⴳ', U'ⴴ', U'ⴵ', U'ⴶ', U'ⴷ', U'ⴹ', U'ⴺ', U'ⴻ', U'ⴼ',
    U'ⴽ', U'ⴿ', U'ⵀ', U'ⵁ', U'ⵂ', U'ⵃ', U'ⵄ', U'ⵅ', U'ⵆ', U'ⵇ', U'ⵉ',
    U'ⵊ', U'ⵌ', U'ⵍ', U'ⵎ', U'ⵏ', U'ⵑ', U'ⵓ', U'ⵔ', U'ⵕ', U'ⵖ', U'ⵗ',
    U'ⵙ', U'ⵚ', U'ⵛ', U'ⵜ', U'ⵝ', U'ⵟ', U'ⵡ', U'ⵢ', U'ⵣ', U'ⵥ',
};
constexpr int kRegisterN = (int)(sizeof(kRegister) / sizeof(kRegister[0]));

/** THE SMALL REGISTER — a second alphabet for the texture band, so the
 *  two innermost script rings are visibly not the same script. Canadian
 *  Aboriginal syllabics (U+1400) read as triangles, arcs and dotted
 *  wedges at four points, which is exactly what a texture band wants: a
 *  rhythm of marks rather than a run of letters. */
const char32_t kSmallRegister[] = {
    U'ᐁ', U'ᐃ', U'ᐅ', U'ᐊ', U'ᐍ', U'ᐐ', U'ᐓ', U'ᐠ', U'ᐯ', U'ᐱ', U'ᐺ', U'ᑉ',
    U'ᑌ', U'ᑐ', U'ᑦ', U'ᑭ', U'ᑲ', U'ᒃ', U'ᒋ', U'ᒣ', U'ᒻ', U'ᓀ', U'ᓓ', U'ᓪ',
    U'ᔁ', U'ᔦ', U'ᕋ', U'ᕕ', U'ᕝ', U'ᕰ', U'ᖏ', U'ᖦ', U'ᗞ', U'ᘁ', U'ᘔ', U'ᘢ',
};
constexpr int kSmallRegisterN =
    (int)(sizeof(kSmallRegister) / sizeof(kSmallRegister[0]));

/** Append @p cp to @p out as UTF-8. The bands are built as bytes because
 *  everything downstream — measuring, fitting, glyph counting — speaks
 *  the same encoding the content literals do. */
void appendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0u | (cp >> 6u));
    out += (char)(0x80u | (cp & 0x3Fu));
  } else {
    out += (char)(0xE0u | (cp >> 12u));
    out += (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    out += (char)(0x80u | (cp & 0x3Fu));
  }
}

/** @p words words of @p lo…@p hi letters, space-separated, dealt from one
 *  of the two registers. The draw is fixed-width integer arithmetic, so
 *  the same seed gives the same band wherever this runs; the `| 1u`
 *  keeps the stream off the state every shift fixes. */
std::string deal(uint32_t seed, int words, int lo, int hi, bool small = false) {
  const char32_t* set = small ? kSmallRegister : kRegister;
  const int n = small ? kSmallRegisterN : kRegisterN;
  std::string out;
  uint32_t s = seed | 1u;
  for (int w = 0; w < words; ++w) {
    sigil::core::noise::xorshiftNext(s);
    const int len = lo + (int)(s % (uint32_t)(hi - lo + 1));
    for (int i = 0; i < len; ++i) {
      sigil::core::noise::xorshiftNext(s);
      appendUtf8(out, set[s % (uint32_t)n]);
    }
    out += ' ';
  }
  return out;
}

/** THE NINE NAMES the charge runs. Invented, and built to be pronounced —
 *  a name band is read as a name band because the runs look like words a
 *  mouth could close on, whatever they mean. */
const char* kNames[9] = {"AZRAEVN", "VELMOTH",  "SIRAKEL",
                         "OMBRIAX", "THELVNE",  "KARANDIS",
                         "VOXIMER", "HALDRETH", "ZEPHARIN"};

/** THE INVOCATION — this study's own Latin, in the band-broken-by-crosses
 *  form. Eight phrases; the leading cross closes the ring at the seam,
 *  and each says something the circle then does. */
const char* kInvocatio[8] = {
    "IN PRINCIPIO CIRCVLVS SCRIBITVR", "LITTERA SVRGIT IN ORBEM",
    "DVODECIM RADII CONVENIVNT",       "BIS SENA SIGILLA VIGILANT",
    "STELLA IN STELLA VOLVITVR",       "NOMEN IN CORONA FRANGITVR",
    "ORDO EX ORDINE NASCITVR",         "ET SIGILLVM VIVIT"};

/** THE TWELVE SEALS riding the rim, one to a station. Each is a small
 *  circle of its own: two words of the register — the band the seal
 *  stands on, said back inside it — and an ordinal that decodes at its
 *  centre. The polygon ladder runs three to eight and then repeats, so a
 *  seal and the seal opposite it carry the same figure and the twelve
 *  read as two sixes, which is this study's arithmetic and nobody
 *  else's. */
struct Seal {
  const char* ordo;  // the ordinal, as the seal's centre writes it
  int order;         // …and the spinning polygon's sides
};
constexpr Seal kSealTable[kSeals] = {
    {"I", 3},   {"II", 4},   {"III", 5}, {"IV", 6}, {"V", 7},  {"VI", 8},
    {"VII", 3}, {"VIII", 4}, {"IX", 5},  {"X", 6},  {"XI", 7}, {"XII", 8},
};

/** THE THRESHOLDS, named on the outer compound's chords: six of the
 *  twelve, the odd ones. A caption no longer stands beside the seal it
 *  names — the seals ride the rim and the chords are bands deeper in —
 *  so the correspondence is carried by the numbering alone, which is how
 *  a plate says two things belong together without drawing a line between
 *  them. */
constexpr const char* kLimina[kLimens] = {
    "LIMEN PRIMVM",   "LIMEN TERTIVM", "LIMEN QVINTVM",
    "LIMEN SEPTIMVM", "LIMEN NONVM",   "LIMEN VNDECIMVM",
};

// ---- the charge -----------------------------------------------------------

/** THE CHARGE, against the pass contract: `uContent`, `uUnitRect`,
 *  `uUnitPhase` and `kUnitCount` arrive from the runtime; everything else
 *  is this material's own. Each unit is one NAME on the ring. A name's
 *  flash is sin(π·phase) — nothing before its beat, white-gold at the
 *  crest, settled after — lifting the letters' own pixels and laying a
 *  soft radial wash around the unit's box, so the charge visibly RUNS the
 *  ring as the cascade opens name after name. At every phase 0 the pass
 *  is an exact pass-through, which is what lets it stay mounted for the
 *  whole loop. */
constexpr const char* kChargeSksl = R"(
half4 main(float2 xy) {
  half4 c = uContent.eval(xy);
  float flash = 0.0;
  float wash = 0.0;
  for (int i = 0; i < kUnitCount; ++i) {
    float4 r = uUnitRect[i];
    float f = sin(clamp(uUnitPhase[i].x, 0.0, 1.0) * 3.14159265);
    float inside = step(r.x, xy.x) * step(xy.x, r.x + r.z) *
                   step(r.y, xy.y) * step(xy.y, r.y + r.w);
    flash = max(flash, f * inside);
    float2 mid = float2(r.x + r.z * 0.5, r.y + r.w * 0.5);
    float2 q = (xy - mid) / max(float2(r.z, r.w) * 0.85, float2(1.0));
    wash = max(wash, f * exp(-dot(q, q) * 2.6));
  }
  float cov = float(c.a);
  float a = clamp(cov + 0.42 * wash, 0.0, 1.0);
  float3 col = float3(c.rgb) * (1.0 + 2.1 * flash) +
               uGold.rgb * (0.55 * flash * cov + 0.30 * wash);
  return half4(half3(min(col, float3(a))), half(a));
})";

/** The charge's ABI: one colour, the gold the flash lays on the names. */
struct ChargeParams {
  sigil::material::Color uGold;
};

/** The definition, made once for the process — a recipe's identity is the
 *  object, so a fresh one per describe would compile a fresh program and
 *  never compare equal to itself. */
std::shared_ptr<const sigil::material::Recipe> chargeRecipe() {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      std::make_shared<const sigil::material::Recipe>(
          sigil::material::Recipe::of<ChargeParams>("rota.charge")
              .body(sigil::material::Target::SkSL, kChargeSksl));
  return recipe;
}

// ---- helpers --------------------------------------------------------------

/** Circle-frame polar → canvas px. θ clockwise from 12 o'clock, the
 *  direction every band here is written in. */
SkPoint P(float thDeg, float rNorm) {
  const float a = thDeg * kDeg;
  return {kEye.x() + rNorm * kR * std::sin(a),
          kEye.y() - rNorm * kR * std::cos(a)};
}

/** The pitch of the twelve stations, and the angle of station k. */
constexpr float kPitch = 360.0f / (float)kStations;
float station(int k) { return kPitch * (float)k; }

// Slots in the two baked tables. The chalk table holds one wobbled
// outline per construction line; the glow table one emissive stack per
// LIGHTING GROUP — a set of lines that ignite together, may cross each
// other freely, and TURN TOGETHER, because a group is baked as one union
// and a union can only be rotated whole.
enum : int {
  kChalkEdge,
  kChalkEdgeIn,
  kChalkRuneOut,
  kChalkRuneIn,
  kChalkVoxOut,
  kChalkVoxIn,
  kChalkTickMid,
  kChalkNomOut,
  kChalkNomIn,
  kChalkNomCase,
  kChalkTexOut,
  kChalkTexIn,
  kChalkTexCase,
  kChalkSerrIn,
  kChalkEnv,
  kChalkEnvIn,
  kChalkHubOut,
  kChalkHubIn,
  kChalkHubCase,
  kChalkHubKern,
  kChalkArcs,
  kChalkDash,
  kChalkCresc,
  kChalkStar,
  kChalkInner,
  kChalkHexagram,
  kChalkCount,
};
enum : int {
  kGlowRim,    // the outer rules and the register's frame
  kGlowNom,    // the invocation's and the names' frames
  kGlowArc,    // the broken arcs, their spokes, nodes and the dashed ring
  kGlowStar,   // the {12/3} compound, its spokes, nodes and crescents
  kGlowInner,  // the {12/4} compound and its spokes
  kGlowHub,    // the hub's rules and the emblem's figures
  kGlowSpur,   // the one off-order medallion, straddling the rim
  kGlowCount,
};

/** A path baked once at setup, wrapped as a comparable silhouette: the
 *  ADDRESS is the identity, so the node prunes and its recording caches
 *  exactly as a `shapes::` generator's would. The sketch owns the storage
 *  and fills it before the first describe; nothing moves it afterwards. */
struct Baked {
  const SkPath* held = nullptr;
  bool operator==(const Baked&) const = default;
  SkPath path(SkSize) const { return held ? *held : SkPath(); }
};

/** THE EMISSIVE OUTLINE of one lighting group, as four nested regions.
 *  Every line of the group is expanded to a region and the regions are
 *  UNIONED, so a place where two lines cross is covered ONCE; the four
 *  grades are that union grown three more times. Painting them additively
 *  is then safe, which is the whole reason for the union: stroke over
 *  stroke under `kPlus` prints every junction at twice the brightness of
 *  the lines running through it, and a figure this dense is mostly
 *  junctions — it would light as a constellation of bright knots instead
 *  of one circuit of even light. */
struct Glow {
  SkPath core, halo, mid, bloom;
};

/** A circle of the figure as a path in canvas px. */
SkPath ringPath(float rNorm) {
  SkPathBuilder b;
  b.addOval(SkRect::MakeLTRB(kEye.x() - rNorm * kR, kEye.y() - rNorm * kR,
                             kEye.x() + rNorm * kR, kEye.y() + rNorm * kR));
  return b.detach();
}

/** N ARC SEGMENTS on one ring, each spanning @p spanDeg of the station
 *  pitch and centred on a station offset by @p fromDeg. An arc that stops
 *  short of its neighbour is what makes a ring read as a mechanism rather
 *  than as another circle, and offsetting one ring half a pitch from the
 *  ring beside it is what makes the pair read as two. */
SkPath arcRing(float rNorm, int count, float spanDeg, float fromDeg) {
  SkPathBuilder b;
  const float rad = rNorm * kR;
  const SkRect oval = SkRect::MakeLTRB(kEye.x() - rad, kEye.y() - rad,
                                       kEye.x() + rad, kEye.y() + rad);
  const float pitch = 360.0f / (float)count;
  for (int k = 0; k < count; ++k) {
    // Skia measures from due east; the table above is measured from
    // twelve o'clock, which is the whole difference.
    const float mid = fromDeg + pitch * (float)k - 90.0f;
    b.addArc(oval, mid - spanDeg * 0.5f, spanDeg);
  }
  return b.detach();
}

/** N RADIAL SPOKES between two radii. Three sets of these cross the
 *  figure at three depths, each set living in the layer it crosses, so a
 *  spoke never runs from one turning layer into another. */
SkPath spokeRing(int count, float r0, float r1, float fromDeg) {
  SkPathBuilder b;
  const float pitch = 360.0f / (float)count;
  for (int k = 0; k < count; ++k) {
    const float th = fromDeg + pitch * (float)k;
    b.moveTo(P(th, r0));
    b.lineTo(P(th, r1));
  }
  return b.detach();
}

/** N CRESCENTS: a pair of concentric arcs closed at both ends by a radial
 *  tie, with a short ladder of @p rungs across the gap. A crescent spans a
 *  fraction of the circle and belongs to no ring — which is what makes it
 *  read as an applied mark rather than as another band, and what lets a
 *  plate that counts twelve everywhere else carry three of something. The
 *  fenced gap is the plate's own rule applied to a mark: even here, the
 *  ladder runs BETWEEN two lines. */
SkPath crescentRing(float rOut, float rIn, int count, float spanDeg,
                    float fromDeg, int rungs) {
  SkPathBuilder b;
  const float pitch = 360.0f / (float)count;
  const float ro = rOut * kR;
  const float ri = rIn * kR;
  const SkRect ovalOut = SkRect::MakeLTRB(kEye.x() - ro, kEye.y() - ro,
                                          kEye.x() + ro, kEye.y() + ro);
  const SkRect ovalIn = SkRect::MakeLTRB(kEye.x() - ri, kEye.y() - ri,
                                         kEye.x() + ri, kEye.y() + ri);
  for (int k = 0; k < count; ++k) {
    const float mid = fromDeg + pitch * (float)k;
    const float lo = mid - spanDeg * 0.5f;
    b.addArc(ovalOut, lo - 90.0f, spanDeg);
    b.addArc(ovalIn, lo - 90.0f, spanDeg);
    for (int r = 0; r <= rungs; ++r) {
      const float th = lo + spanDeg * (float)r / (float)rungs;
      // The two ends are full ties; the rungs between them are stubs off
      // the inner arc, so the mark reads as a bracket and not as a grid.
      const bool end = r == 0 || r == rungs;
      b.moveTo(P(th, rIn));
      b.lineTo(P(th, end ? rOut : rIn + (rOut - rIn) * 0.45f));
    }
  }
  return b.detach();
}

/** N SMALL CIRCLES standing on a ring — the furniture that sits at every
 *  star vertex and at every arc's station. They are drawn as one path so
 *  twelve nodes cost one node. */
SkPath nodeRing(int count, float rNorm, float px, float fromDeg) {
  SkPathBuilder b;
  const float pitch = 360.0f / (float)count;
  for (int k = 0; k < count; ++k) {
    const SkPoint c = P(fromDeg + pitch * (float)k, rNorm);
    b.addOval(SkRect::MakeLTRB(c.fX - px, c.fY - px, c.fX + px, c.fY + px));
  }
  return b.detach();
}

/** THE HAND-DRAWN RULE: the compass wobbles, the pen has a nib. A rule
 *  struck in chalk is never a perfect circle, and the eye reads the
 *  difference immediately — which is what makes the clean emissive circle
 *  landing on top of it at the strike say "ignited" rather than
 *  "brighter". The seed is the line's own index, so two neighbouring
 *  rules wobble differently and no two share a wave. */
SkPath chalked(const SkPath& line, uint32_t seed, float amplitude = 1.15f) {
  return sigil::geometry::path::ops::Roughen{
      .amplitude = amplitude, .segmentPx = 34.0f, .seed = seed, .smooth = true}(
      line);
}

/** A LINE as a region of the given half-width. Stroke expansion, and not
 *  `ops::offset`, because offset unites its source with the expansion —
 *  correct for growing a region, and for a CLOSED line (a circle, a star
 *  compound's rings) it would hand back the interior as well. A rule is a
 *  line and never a disc. */
SkPath expand(const SkPath& line, float halfWidth) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halfWidth * 2.0f);
  p.setStrokeJoin(SkPaint::kRound_Join);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(line, p);
}

/** The group's lines widened four times and unioned at each width. */
Glow bakeGlow(const std::vector<SkPath>& lines, float coreHalf) {
  namespace ops = sigil::geometry::path::ops;
  auto at = [&](float k) {
    std::vector<SkPath> regions;
    regions.reserve(lines.size());
    for (const SkPath& line : lines)
      regions.push_back(expand(line, coreHalf * k));
    return ops::unite(regions);
  };
  return Glow{at(1.0f), at(2.8f), at(7.0f), at(18.0f)};
}

/** THE RAYS: radial streaks thrown past the figure at ignition, sampled
 *  on angle so the count is a number and not a hundred nodes. The falloff
 *  is a window in radius — nothing at the hub, nothing past the sheet —
 *  and the streaks' own widths come from two beats of a hashed angle, so
 *  the fan reads as unequal spokes of light rather than a gear. */
constexpr const char* kRaysSksl = R"(
half4 main(float2 xy) {
  float2 c = uResolution * 0.5;
  float2 d = xy - c;
  float r = length(d) / max(c.x, 1.0);
  float a = atan(d.y, d.x);
  float k = sin(a * 19.0) * 0.5 + 0.5;
  float j = sin(a * 47.0 + 1.7) * 0.5 + 0.5;
  float streak = pow(k, 6.0) * 0.75 + pow(j, 14.0) * 0.55;
  float band = smoothstep(0.15, 0.40, r) * (1.0 - smoothstep(0.44, 0.95, r));
  float v = streak * band;
  return half4(half3(uInk.rgb * v), half(v));
})";

/** The rays' ABI. The body reads the node's box, which the recipe declares
 *  as a frame input rather than a field: the runtime fills it. */
struct RaysParams {
  sigil::material::Color uInk;
};

std::shared_ptr<const sigil::material::Recipe> raysRecipe() {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      std::make_shared<const sigil::material::Recipe>(
          sigil::material::Recipe::of<RaysParams>("rota.rays")
              .frame(sigil::material::FrameInput::Resolution)
              .body(sigil::material::Target::SkSL, kRaysSksl));
  return recipe;
}

/** THE CREST'S COLOUR FRINGE: the picture beneath re-sampled with its red
 *  and blue pulled apart ALONG THE RADIUS, which is where a lens throws
 *  them on a light this bright. Attached as a backdrop and gated by the
 *  node's own opacity, so it is a crossfade to the fringed frame at the
 *  crest and literally not painted at any other moment. */
constexpr const char* kFringeSksl = R"(
uniform shader content;
uniform float uSpread;
uniform float uCx;
uniform float uCy;
half4 main(float2 xy) {
  float2 d = xy - float2(uCx, uCy);
  float r = length(d);
  float2 u = r > 0.001 ? d / r : float2(0.0);
  float k = uSpread * (0.5 + r * 0.0022);
  half4 lo = content.eval(xy + u * k);
  half4 mid = content.eval(xy);
  half4 hi = content.eval(xy - u * k);
  return half4(lo.r, mid.g, hi.b, mid.a);
})";

/** The fringe compiled once for the process. */
sk_sp<SkRuntimeEffect> fringeEffect() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(kFringeSksl));
    if (!effect) std::fprintf(stderr, "[rota] fringe: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

int glyphsOf(const std::string& s) {
  int n = 0;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = (unsigned char)s[i];
    const int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (c != ' ' && c != '\n') ++n;
    i += (size_t)len;
  }
  return n;
}

}  // namespace

// ===========================================================================

struct RotaConvocationis : sketch::Sketch {
  // The two hand-stepped scalars, plus the drifts and spins that are pure
  // shapes of them. Everything scheduled is a window on `cycle`.
  ch::Output<float> cycle{0};  // seconds within one loop, wrapping
  ch::Output<float> secs{0};   // monotonic — breaths and orbits

  // NINE LAYERS TURNING AT NINE RATES, and twelve more inside the seals.
  // The periods are set where they are stepped; what belongs here is the
  // list, which is also the reading: no band on this plate is still.
  ch::Output<float> voxDrift{0};   // the invocation, after ignition
  ch::Output<float> runeDrift{0};  // the register, the other way
  ch::Output<float> nomDrift{0};   // the names' counter-orbit
  ch::Output<float> texDrift{0};   // the texture band
  ch::Output<float> tickSpin{0};   // the division ladder — the fast layer
  ch::Output<float> arcSpin{0};    // the broken arcs and their spokes
  ch::Output<float> starSpin{0};   // the {12/3} compound
  ch::Output<float> innerSpin{0};  // the {12/4} compound, counter to it
  ch::Output<float> hexSpin{0};    // the emblem's own figure, slowest
  // THE RIM'S THREE MOTIONS, which is the reading the sub-seals exist for:
  // the carrier brings the whole ring of stations round, each seal turns
  // about its own centre inside it, and the polygon in each seal turns
  // against its seal. Only the ordinal is held out of all three.
  ch::Output<float> sealOrbit{0};    // the carrier: the station ring
  ch::Output<float> sealUpright{0};  // …its negation, gimballing the ordinals
  ch::Output<float> sealSpin[kSeals] = {};  // each seal about its own centre
  ch::Output<float> sealCog[kSeals] = {};   // its polygon, against the seal
  ch::Output<float> scribeX{0}, scribeY{0}, scribeA{0};
  // The register's shimmer never lands. It is a LOOPING cascade, so the
  // master is a phase mod 1 and one sweep is one cycle: this output wraps
  // forever once the band is written, and pins at 0 before that, where
  // every unit rests at its landed deviation and the effect is neutral.
  ch::Output<float> runePhase{0};

  // How hard each lighting group is burning. One scalar carries three
  // things a single declared window cannot say together — the strike's
  // cross-fade, the flash the strike throws, and the breath the charged
  // circle keeps afterwards — and it is bound, so the four cached
  // recordings of the group's emissive stack replay under it untouched.
  ch::Output<float> litRim{0}, litNom{0}, litArc{0}, litStar{0}, litInner{0},
      litHub{0}, litSpur{0};
  ch::Output<float> litSeal[kSeals] = {};
  ch::Output<float> morphStep{0};  // the star compound's strike-morph ladder
  ch::Output<float> humScale{1};   // the charged circle's scale breath
  ch::Output<float> floodA{0}, raysA{0}, emberA{0};
  // The fringe is a BACKDROP: it re-samples what is already painted, and a
  // backdrop is not blended back through the node's alpha the way a fill
  // is — an opacity of a half would not be half a fringe. So the ramp
  // lives in the shader's own spread and the opacity is a hard gate,
  // exactly zero outside the crest, which is what keeps the effect from
  // being paid for on any frame that does not want it.
  ch::Output<float> fringeK{0}, fringeA{0};

  sk_sp<SkTypeface> faceRing, faceRingBold, faceMono;

  // Geometry baked once, before the first describe, and never moved: the
  // chalk's wobble, each lighting group's emissive stack, and the star
  // compound's morph ladder.
  std::vector<SkPath> chalk;
  std::vector<Glow> glows;
  std::vector<SkPath> starSteps;
  SkPath arcNodes, starNodes, arcSpokes, starSpokes, innerSpokes, hubDots,
      spurRules;
  std::shared_ptr<instancing::Atlas> emberAtlas;
  std::shared_ptr<instancing::Pool> embers;
  int emberFrame = 0;

  // Fitted content: each ring's text and the size that girds its band.
  std::string voxText, runeText, nomText, texText, hubRuneText, emblemText,
      spurText;
  std::string sealText[kSeals];
  float voxSize = 18, runeSize = 20, nomSize = 30, texSize = 9,
        sealSize[kSeals] = {};
  std::vector<float> voxCues;  // one start per word; pauses at crosses
  int voxWords = 0, voxMaxWord = 1;

  // The computed timeline, seconds. Every value is chained from a span.
  double tVox = 0, tRune = 0, tNames = 0, tTex = 0, tArc = 0, tStar = 0,
         tInner = 0, limenAt[kLimens] = {}, tSeal[kSeals] = {}, tHub = 0,
         tIgnite = 0, loopSecs = 30;
  double lastElapsed = 0;  // the scribe's decay reads real dt
  float voxSpanS = 0, runeSpanS = 0, nomSpanS = 0, texSpanS = 0, limenSpanS = 0,
        sealSpanS = 0, hubSpanS = 0, shimmerS = 3.4f;

  int totalGlyphs = 0;

  // ------------------------------------------------------------------
  // schedule verbs

  /** A beat on the circle's timeline, in seconds of the loop: clamped
   *  outside its range, so an unstarted track reads 0 and a finished one
   *  reads 1, and the whole assembly re-performs on the wrap. */
  [[nodiscard]] Animatable<float> beat(double from, double to) {
    return bind(&cycle).window((float)from, (float)to);
  }
  /** A one-shot swell inside the loop — up, held, gone — for the flashes
   *  ignition throws on things that are not text. */
  [[nodiscard]] Animatable<float> pulse(double from, double to, double edge) {
    const float l = (float)loopSecs;
    return bind(&cycle)
        .source(0.0f, l)
        .trapezoid((float)from / l, (float)(from + edge) / l,
                   (float)(to - edge * 1.6) / l, (float)to / l)
        .map(&ch::easeInOutQuad);
  }
  /** The sheet's own envelope: up at the head, out before the wrap, so
   *  the loop cuts on a dark sheet. */
  [[nodiscard]] Animatable<float> envelope() {
    const float l = (float)loopSecs;
    return bind(&cycle)
        .source(0.0f, l)
        .trapezoid(0.05f / l, 0.55f / l, (l - 1.9f) / l, (l - 0.55f) / l)
        .map(&ch::easeInOutQuad);
  }

  // ------------------------------------------------------------------
  // type

  [[nodiscard]] sigil::weave::TextStyle ring(float size, SkColor4f color,
                                             float track = 3.0f) const {
    return type(
        {.face = faceRingBold, .size = size, .color = color, .track = track});
  }
  /** THE REGISTER'S TYPE takes no face. The invented alphabet is not in
   *  the interface family, and asking for it by name would be asking for
   *  a font this study does not ship: the shaper's own fallback finds the
   *  letterforms, which is the mechanism that puts them on the plate. */
  [[nodiscard]] static sigil::weave::TextStyle rune(float size, SkColor4f color,
                                                    float track) {
    return type({.size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle mono(float size, SkColor4f color,
                                             float track = 1.0f) const {
    return type(
        {.face = faceMono, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle label(float size, SkColor4f color,
                                              float track = 2.6f) const {
    return type(
        {.face = faceRing, .size = size, .color = color, .track = track});
  }

  /** The size at which @p s girds a circle of radius @p radius: measured
   *  straight, refined once because tracking is px and does not scale
   *  with the type. */
  [[nodiscard]] float fitToRing(sketch::SketchContext& ctx,
                                const std::string& s,
                                const sigil::weave::TextStyle& style,
                                float radius, float fill = 0.985f) {
    const float target = 2.0f * 3.14159265f * radius * fill;
    float size = style.shaping.fontSize;
    for (int pass = 0; pass < 2; ++pass) {
      sigil::weave::TextStyle probe = style;
      probe.shaping.fontSize = size;
      const SkSize m = ctx.measure(text(toU8(s), probe));
      if (m.width() > 1.0f) size *= target / m.width();
    }
    return size;
  }

  // ------------------------------------------------------------------
  // the drawn lines

  /** A stroked line of the figure, revealed as an arc sweep — the compass
   *  stroke itself. The outline is the baked chalk path, so what the
   *  sweep lays down carries the hand's wobble; the box is the whole panel
   *  because the path is in the panel's own px. */
  [[nodiscard]] Element rule(const char* key, int chalkIndex, float width,
                             SkColor4f color, double from, double dur) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .shape(Baked{&chalk[(size_t)chalkIndex]})
        .fill(Fill::none())
        .stroke(spans::upTo(beat(from, from + dur)),
                stroke(width, Fill::color(color)));
  }

  /** The same, for a path this sketch holds directly rather than in the
   *  chalk table — the spoke sets and the node rings, which are perfect
   *  by construction because a node is a bead and not a rule. */
  [[nodiscard]] Element line(const char* key, const SkPath& path, float width,
                             SkColor4f color, double from, double dur) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .shape(Baked{&path})
        .fill(Fill::none())
        .stroke(spans::upTo(beat(from, from + dur)),
                stroke(width, Fill::color(color)));
  }

  /** ONE GRADE of a lighting group's emissive stack. Each grade is a flat
   *  fill of an already-unioned region and carries the group's own gain,
   *  so a fill-only leaf takes the blend and the opacity straight onto its
   *  paint: no layer opens, and the four grades add onto the sheet
   *  directly, which is what makes the pile read as light rather than as
   *  four translucent rings. */
  [[nodiscard]] Element grade(const std::string& key, const SkPath& region,
                              SkColor4f ink, float alpha,
                              const ch::Output<float>* gain) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .shape(Baked{&region})
        .fill(Fill::color({ink.fR, ink.fG, ink.fB, alpha}))
        .blend(SkBlendMode::kPlus)
        .opacity(gain);
  }

  /** THE IGNITED LINE — white-hot core, saturated halo, two grades of
   *  bloom. The value hierarchy is the drawing: the core carries the
   *  shape, the halo carries the hue, the bloom carries the reach. */
  [[nodiscard]] Element emissive(const std::string& key, const Glow& g,
                                 const ch::Output<float>* gain) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .child(grade(key + "-bloom", g.bloom, kBloom, 0.085f, gain))
        .child(grade(key + "-mid", g.mid, kBloom, 0.16f, gain))
        .child(grade(key + "-halo", g.halo, kHalo, 0.42f, gain))
        .child(grade(key + "-core", g.core, kCore, 0.96f, gain));
  }

  /** A DIVISION LADDER at one length class. `skipEvery` leaves a hole
   *  where a heavier class stands, because two passes stamping the same
   *  mark print it darker than either weight meant to. */
  [[nodiscard]] Element ladder(const char* key, int divisions, int skipEvery,
                               float outer, float inner, float width,
                               SkColor4f color, double from, double dur,
                               float fromDeg = 0.0f) {
    kit::Ticks t{.divisions = divisions,
                 .from = fromDeg,
                 .mark = {inner / outer, 1.0f},
                 .longEvery = skipEvery,
                 .longMark = {1.0f, 1.0f}};
    return box()
        .key(key)
        .absolute()
        .inset((1.0f - outer) * kR + (kEye.x() - kR))
        .hitTestable(false)
        .shape(kit::ticks(t))
        .fill(Fill::none())
        .stroke(spans::upTo(beat(from, from + dur)),
                stroke(width, Fill::color(color)));
  }

  // ------------------------------------------------------------------
  // the bands

  /** The invocation's cascade: one cue per word — writing pace, with the
   *  pen resting at every cross — and the letters of each word beating
   *  inside its cue. Built as a named value because `then` mutates in
   *  place. */
  [[nodiscard]] Stagger voxCascade() const {
    Stagger cascade = stagger(unit::Word, cues(voxCues));
    cascade.then(unit::Cluster, {.eachMs = 22, .durationMs = 300});
    return cascade;
  }

  /** THE INVOCATION — Latin majuscule between two rules, the one band on
   *  the plate that is meant to be read. Its entrance composes with the
   *  baseline: the hold vetoes a glyph until its beat, the rise then lifts
   *  it onto the ring along the ring's own local perpendicular, and the
   *  tint warms it from ember to bone as it lands. After ignition the
   *  whole run becomes a slow marquee. */
  [[nodiscard]] Element invocatio() {
    return text(toU8(voxText), ring(voxSize, kBone, 2.2f))
        .key("vox")
        .centerAt(kEye)
        .width(2 * rVox * kR)
        .height(2 * rVox * kR)
        .hitTestable(false)
        .onPath({.path = shapes::circle(),
                 .at = &voxDrift,
                 .align = TextPath::Align::Start,
                 .offset = -voxSize * 0.34f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::rise(voxSize * 1.1f)),
             .stagger = voxCascade(),
             .progress = beat(tVox, tVox + voxSpanS)})
        .fx({.effect = fx::tint(kEmber, kBone),
             .stagger = voxCascade(),
             .progress = beat(tVox, tVox + voxSpanS)})
        // THE STRIKE, per word: a letter does not fade up, it arrives lit
        // and cools. The screen term lifts each channel by the headroom it
        // has left rather than adding into a clip, so the flash reads as
        // the glyph glowing white for a beat and not as a white rectangle
        // where the glyph was.
        .fx({.effect = fx::keys({{0.00f, {.colorScreen = kHalo}},
                                 {0.18f, {.colorScreen = kCore}},
                                 {1.00f, {}}},
                                &ch::easeOutQuad),
             .stagger = voxCascade(),
             .progress = beat(tVox, tVox + voxSpanS)});
  }

  /** THE SHIMMER — a crest of light that re-opens on every glyph of the
   *  register forever. `Stagger::loopMs` folds each glyph's beat onto its
   *  own cycle of one period, phase-offset by the glyph's start, so one
   *  sweep of the master IS one cycle and a wrapping phase drives it
   *  seamlessly with no seam to hide. It is the reading that the circle
   *  is not a finished picture: something is still running round it. */
  [[nodiscard]] Stagger shimmerCascade() const {
    Stagger s = stagger(unit::Cluster, {.eachMs = 26, .durationMs = 620});
    s.loopMs = (uint32_t)(shimmerS * 1000.0f);
    return s;
  }

  /** THE REGISTER — the outer rune band, the widest ring of script on the
   *  plate and the one that says what idiom this is. It forms as a
   *  scatter (the letters do not arrive in reading order, because nobody
   *  reads them), then never settles. */
  [[nodiscard]] Element registrum() {
    return text(toU8(runeText), rune(runeSize, kRuneInk, 2.0f))
        .key("registrum")
        .centerAt(kEye)
        .width(2 * rRune * kR)
        .height(2 * rRune * kR)
        .hitTestable(false)
        .onPath({.path = shapes::circle(),
                 .at = &runeDrift,
                 .align = TextPath::Align::Start,
                 .offset = -runeSize * 0.34f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::pop(0.55f)),
             .stagger = stagger(unit::Cluster, {.eachMs = 7,
                                                .durationMs = 420,
                                                .from = Stagger::From::Random,
                                                .seed = 17}),
             .progress = beat(tRune, tRune + runeSpanS)})
        .fx({.effect = fx::keys(
                 {{0.00f, {}}, {0.35f, {.colorScreen = kHalo}}, {1.00f, {}}}),
             .stagger = shimmerCascade(),
             .progress = &runePhase});
  }

  /** THE NINE NAMES, one text leaf whose baseline is a circle it orbits.
   *  Three tracks and the marquee compose: the forming cascade (held
   *  rise), the ignition swell on GRAD — up and back down, two phases
   *  driving one axis, which lerp — and the charge pass, whose SkSL holds
   *  the whole ignition schedule as per-unit uniforms.
   *
   *  THE PASS DECLARES ITS OWN REST, which is what lets it stay mounted
   *  for the whole loop on a ring that is volatile for the whole loop
   *  because it orbits. The charge's flash is sin(π·phase): exactly zero
   *  at phase 0 and at phase 1, where the material returns its input
   *  pixels untouched — so `restsAt(0, 1)` is a true promise and the
   *  runtime skips both the layer and the shader on every frame outside
   *  the charge's window. The one-shot cascade clamps a unit to exactly 0
   *  before its beat and exactly 1 after, which is the comparison the
   *  skip makes; both ends therefore engage, where under a looping
   *  cascade only the settled end would. */
  [[nodiscard]] Element nomina() {
    Stagger form = stagger(unit::Word, {.amountMs = 1900});
    form.then(unit::Cluster, {.eachMs = 24, .durationMs = 420});

    TextEffect swell = fx::seq(
        fx::variableAxisSweep("GRAD", 400.0f, 860.0f).until(0.45f).xfade(0.25f),
        fx::variableAxisSweep("GRAD", 860.0f, 400.0f));

    // How far past the ring's snug box the pass may paint: the glyphs
    // straddle the baseline circle and stand proud of the box at its four
    // extremes, the forming rise carries them further, and the charge's
    // wash spreads around each name's rect. Over-reporting is safe;
    // under-reporting shears the outer halves off at the layer's edge.
    constexpr float kReach = 90.0f;
    Element names =
        text(toU8(nomText), ring(nomSize, kGold, 4.2f))
            .key("nomina")
            .effect(styles::textGlow(kHalo, 6.0f))
            .centerAt(kEye)
            .width(2 * rNom * kR)
            .height(2 * rNom * kR)
            .hitTestable(false)
            .onPath({.path = shapes::circle(),
                     .at = &nomDrift,
                     .align = TextPath::Align::Start,
                     .offset = -nomSize * 0.34f,
                     .autoFlip = false})
            .fx({.effect = fx::hold(fx::rise(nomSize * 0.8f)),
                 .stagger = form,
                 .progress = beat(tNames, tNames + nomSpanS)})
            .fx({.effect = std::move(swell),
                 .stagger =
                     stagger(unit::Word, {.eachMs = 130, .durationMs = 900}),
                 .progress = beat(tIgnite, tIgnite + 2.6)})
            // The charge reaching a name flashes it, on the pass's own
            // cascade so the letters and the shader open together.
            .fx({.effect = fx::keys({{0.00f, {}},
                                     {0.30f, {.colorScreen = kCore}},
                                     {1.00f, {}}},
                                    &ch::easeInOutQuad),
                 .stagger =
                     stagger(unit::Word, {.eachMs = 170, .durationMs = 820}),
                 .progress = beat(tIgnite, tIgnite + 2.6)});
    names.fx(
        {.effect = fx::pass(Material::recipe(
                                sigil::material::Material(chargeRecipe()))
                                .uniform("uGold", kGold))
                       .restsAt(0.0f, 1.0f),
         .stagger = stagger(unit::Word, {.eachMs = 170, .durationMs = 820}),
         .progress = beat(tIgnite, tIgnite + 2.6),
         .reach = kReach});
    return names;
  }

  /** THE TEXTURE — the small register, tracked shut and set at a size
   *  that stops it being letters and makes it grain. It is what makes the disc
   * continuous between the arcs and the stars instead of leaving a dark annulus
   *  there, and it is the band the whole idiom depends on: the plate
   *  looks like a mechanism because the innermost script is too small to
   *  resolve. Every glyph is still a real glyph; a baked tile would be
   *  cheaper and would be a lie about what this plate is. */
  [[nodiscard]] Element textura() {
    return text(toU8(texText), rune(texSize, kAsh, 0.0f))
        .key("textura")
        .centerAt(kEye)
        .width(2 * rTex * kR)
        .height(2 * rTex * kR)
        .hitTestable(false)
        .onPath({.path = shapes::circle(),
                 .at = &texDrift,
                 .align = TextPath::Align::Start,
                 .offset = -texSize * 0.30f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::rise(texSize * 0.9f)),
             .stagger = stagger(unit::Cluster, {.eachMs = 4,
                                                .durationMs = 300,
                                                .from = Stagger::From::Random,
                                                .seed = 61}),
             .progress = beat(tTex, tTex + texSpanS)});
  }

  // ------------------------------------------------------------------
  // the turning layers

  /** THE ARC LAYER: two rings of broken arcs half a pitch apart, the
   *  spokes that tie them, the nodes that stand between them and a dashed
   *  ring outside the pair — one layer, so one baked union, so one
   *  rotation, and the whole assembly sweeps past the bands either side of
   *  it as a mechanism rather than as a picture being spun. */
  [[nodiscard]] Element arcus() {
    Element turn =
        box().key("arcus").absolute().inset(0).hitTestable(false).rotate(
            bind(&arcSpin).target(0.0f, 360.0f));
    turn.child(rule("arc-chalk", kChalkArcs, 1.7f, kIron, tArc, 1.1));
    turn.child(rule("arc-dash", kChalkDash, 1.0f, kIron, tArc - 0.3, 1.0));
    turn.child(line("arc-spokes", arcSpokes, 0.9f, kIron, tArc + 0.2, 0.9));
    turn.child(line("arc-nodes", arcNodes, 1.0f, kIron, tArc + 0.35, 0.8));
    turn.child(emissive("arc-lit", glows[kGlowArc], &litArc));
    return turn;
  }

  /** THE OFF-ORDER MARK — one medallion straddling the outermost rule at
   *  twelve o'clock, carrying a single letterform. It obeys none of the
   *  plate's symmetries and interrupts two bands to exist, which is
   *  exactly its job: the eye takes a perfectly regular figure for a
   *  pattern and a figure with one exception for a drawing. */
  [[nodiscard]] Element spur() {
    const SkPoint c = P(0.0f, rEdge);
    return box()
        .key("spur")
        .absolute()
        .inset(0)
        .hitTestable(false)
        .child(kit::disc(c, kSpurR)
                   .key("spur-ground")
                   .hitTestable(false)
                   .fill(Fill::color(hex(0x0D0A16, 0.92f)))
                   .opacity(beat(0.9, 1.4)))
        .child(line("spur-rules", spurRules, 1.2f, kIron, 1.0, 0.9))
        .child(text(toU8(spurText), rune(19.0f, kBone, 0.0f))
                   .key("spur-glyph")
                   .centerAt(c)
                   .hitTestable(false)
                   .fx({.effect = fx::hold(fx::pop(0.5f)),
                        .stagger = stagger(unit::Cluster, {.durationMs = 360}),
                        .progress = beat(1.3, 1.8)}))
        .child(emissive("spur-lit", glows[kGlowSpur], &litSpur));
  }

  /** THE OUTER FIGURE: the {12/3} compound — three squares turned 30°
   *  from each other, which is what a step of three across twelve
   *  stations gives — with a small circle at every vertex and spokes
   *  running from the vertices down to the circle its chords are tangent
   *  to. The chalk is swept on at the strike; the morph ladder resolves
   *  the light from a scribble to the true figure; the emissive stack is
   *  what stays.
   *
   *  THE MORPH IS A LADDER OF BAKED STEPS, not one interpolated path. A
   *  path re-cooked every frame is content nothing can cache, and this
   *  one is a resample-and-lerp over hundreds of points. Baked, each step
   *  is an ordinary comparable silhouette whose recording is built once,
   *  and a bound scalar walks the ladder: neighbouring steps cross-fade,
   *  so what the eye follows is continuous while what the library records
   *  is a bounded set. */
  [[nodiscard]] Element stella() {
    Element turn =
        box().key("stella").absolute().inset(0).hitTestable(false).rotate(
            bind(&starSpin).target(0.0f, 360.0f));
    turn.child(rule("star-chalk", kChalkStar, 1.5f, kIron, tStar, 1.3));
    // THE CRESCENTS: three double-arcs laid ACROSS the compound, 120°
    // apart, each closed at both ends by a radial tie and carrying its
    // own short ladder. They are the plate's single THREE-FOLD mark —
    // applied over the mechanism rather than being a band of it — and
    // they ride the compound's rotation, so three things visibly sweep
    // past twelve.
    turn.child(rule("cresc-chalk", kChalkCresc, 1.3f, kIron, tStar + 0.5, 1.0));
    turn.child(
        line("star-spokes", starSpokes, 0.8f, kIronDim, tStar + 0.2, 1.0));
    turn.child(line("star-nodes", starNodes, 1.0f, kIron, tStar + 0.4, 0.9));
    for (int i = 0; i < (int)starSteps.size(); ++i)
      turn.child(box()
                     .key("star-morph" + std::to_string(i))
                     .absolute()
                     .inset(0)
                     .hitTestable(false)
                     .shape(Baked{&starSteps[(size_t)i]})
                     .fill(Fill::none())
                     .stroke(stroke(1.6f, Fill::color(kCore)))
                     .blend(SkBlendMode::kPlus)
                     .opacity(bind(&morphStep)
                                  .window((float)i - 1.0f, (float)i + 1.0f)
                                  .pingPong()));
    turn.child(emissive("star-lit", glows[kGlowStar], &litStar));
    return turn;
  }

  /** THE INNER FIGURE: the {12/4} compound — four triangles — nested so
   *  that its vertices sit just inside the circle the squares above are
   *  tangent to, and its own chords run tangent to the hub's rule. It
   *  turns against the compound outside it, which is the whole reason
   *  there are two. */
  [[nodiscard]] Element stellaInterior() {
    Element turn = box()
                       .key("stella-int")
                       .absolute()
                       .inset(0)
                       .hitTestable(false)
                       .rotate(bind(&innerSpin).target(0.0f, 360.0f));
    turn.child(rule("inner-chalk", kChalkInner, 1.2f, kIron, tInner, 1.2));
    turn.child(
        line("inner-spokes", innerSpokes, 0.7f, kIronDim, tInner + 0.2, 1.0));
    turn.child(emissive("inner-lit", glows[kGlowInner], &litInner));
    return turn;
  }

  /** THE CAPTIONS on the outer compound's chords. They ride a SECOND path
   *  — the same twelve chords as OPEN contours — so chord k's midpoint is
   *  at exactly (k+0.5)/12 of one arc-length coordinate, and each caption
   *  is a leaf addressed by fraction alone. Only six of the twelve chords
   *  are written; the other six carry the ladder's silence, which is the
   *  asymmetry the idiom allows itself. They do not turn with the figure,
   *  and they do not travel with the rim: a caption is read, and the
   *  mechanism is what moves. */
  [[nodiscard]] Element limina() {
    Element fig = kit::disc(kEye, rStar * kR).key("limina").hitTestable(false);
    const Shape chordPath =
        kit::chords({.sides = kStations, .step = 3, .inset = 74.0f});
    for (int k = 0; k < kLimens; ++k) {
      fig.child(
          text(toU8(kLimina[k]), label(11.0f, kAsh, 2.0f))
              .key("limen" + std::to_string(k))
              .absolute()
              .inset(0)
              .hitTestable(false)
              .onPath({.path = chordPath,
                       .at = ((float)(k * 2) + 0.5f) / (float)kStations,
                       .align = TextPath::Align::Center,
                       .offset = 5.0f,
                       .autoFlip = true})
              .fx({.effect = fx::typeOn(),
                   .stagger = stagger(unit::Cluster,
                                      {.eachMs = 30, .durationMs = 120}),
                   .progress = beat(limenAt[k], limenAt[k] + limenSpanS)}));
    }
    return fig;
  }

  /** ONE SEAL — a sub-circle that is a small magic circle of its own,
   *  standing on the rim at one of the twelve stations: rules struck, a
   *  ring of lettering tumbling on, an ordinal decoding at the centre,
   *  and an order-sided polygon spinning behind it. Seal k's whole window
   *  starts where seal k−1's span says, so the twelve ignite in turn as
   *  the carrier brings them round, and what runs the rim is a fuse
   *  rather than a ceremony of one seal at a time.
   *
   *  THE SEAL TURNS AS A BODY, and its ring text turns with it because it
   *  IS the body: a circular baseline rotated about its own centre is the
   *  same picture as the same run advanced along its path, so the letters
   *  need no placement of their own: the seal is recorded once and
   *  replayed under a bound transform, where a driven path phase would
   *  re-place every glyph of every seal on every frame. That is what
   *  makes the count affordable — a rim of twelve, not a handful. */
  [[nodiscard]] Element sigillum(int k) {
    const Seal& s = kSealTable[k];
    const SkPoint c = P(station(k), rSealRide);
    const double at = tSeal[k];
    const std::string id = "seal" + std::to_string(k);

    Element seal = kit::disc(c, kSealR).key(id);
    // The ground: occludes the bands under the seal — a seal SITS ON the
    // plate rather than being drawn into it — and wears an aura for a
    // breath at ignition.
    seal.child(box()
                   .key(id + "-ground")
                   .absolute()
                   .inset(0)
                   .corners({kSealR})
                   .hitTestable(false)
                   .fill(Fill::color(hex(0x0D0A16, 0.94f)))
                   // The ground is dressed rather than shaded: an inner glow is
                   // a blurred band hugging its own edge, a value decoration
                   // that records once with the disc it sits on. It gives the
                   // seal a lip of light without a second node and without a
                   // shader.
                   .overlay(styles::innerGlow(hex(0xE79A32, 0.30f), 8.0f))
                   .opacity(beat(at, at + 0.4)));
    // The seal's own emissive rule. A seal is a small magic circle, so it
    // lights like one — but its two rules are concentric and cross
    // nothing, which is the case the SDF answers in one pass: silhouette,
    // core and halo are three uniforms of one shader rather than a union
    // and four fills.
    {
      const sdf::Style lit{.borderWidth = 1.1f,
                           .borderColor = kCore,
                           .glowRadius = 6.0f,
                           .glowColor = hex(0xFFC152, 0.42f)};
      const float side = sdf::minBoxFor(lit, 2.0f * kSealR);
      seal.child(box()
                     .key(id + "-lit")
                     .absolute()
                     .inset(kSealR - side * 0.5f)
                     .hitTestable(false)
                     .fill(sdf::material(sdf::circle(), lit))
                     .blend(SkBlendMode::kPlus)
                     .opacity(&litSeal[k]));
    }
    // The rules, struck as sweeps.
    seal.child(box()
                   .key(id + "-rule-out")
                   .absolute()
                   .inset(0)
                   .corners({kSealR})
                   .hitTestable(false)
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(at, at + 0.55)),
                           stroke(1.2f, Fill::color(kIron))))
        .child(box()
                   .key(id + "-rule-in")
                   .absolute()
                   .inset(kSealR - kSealRing + 7.0f)
                   .corners({kSealRing - 7.0f})
                   .hitTestable(false)
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(at + 0.15, at + 0.7)),
                           stroke(0.7f, Fill::color(kIronDim))));
    // THE TURNING BODY: everything in the seal that is not a circle and
    // not the ordinal. The rules and the ground are concentric discs and
    // a disc under rotation is the same disc, so they stay outside this
    // node and nothing pays for turning them.
    Element body = box()
                       .key(id + "-body")
                       .absolute()
                       .inset(0)
                       .hitTestable(false)
                       .rotate(bind(&sealSpin[k]).target(0.0f, 360.0f));
    // The order-sided polygon, turning AGAINST its own seal once lit, so
    // the figure inside a seal and the seal around it are visibly two
    // mechanisms and not one drawing.
    body.child(box()
                   .key(id + "-poly")
                   .absolute()
                   .inset(kSealR - 16.0f)
                   .hitTestable(false)
                   .shape(shapes::polygon(s.order))
                   .fill(Fill::none())
                   .stroke(stroke(0.9f, Fill::color(kIron)))
                   .rotate(bind(&sealCog[k]).target(0.0f, 360.0f))
                   .opacity(beat(at + 0.3, at + 0.9)));
    // The ring: two words of the register, tumbling onto the circle and
    // then carried round by the body it belongs to. Its phase is a plain
    // number — a twelfth per station, so no two seals open their text at
    // the same clock angle — because the turning is the body's and a run
    // that is not driving its own placement can rest at whole pixels
    // until an ancestor moves it.
    body.child(text(toU8(sealText[k]), ring(sealSize[k], kBone, 1.4f))
                   .key(id + "-ring")
                   .absolute()
                   .inset(kSealR - kSealRing)
                   .hitTestable(false)
                   .onPath({.path = shapes::circle(),
                            .at = (float)k / (float)kSeals,
                            .align = TextPath::Align::Start,
                            .offset = -sealSize[k] * 0.34f,
                            .autoFlip = false})
                   .fx({.effect = fx::hold(fx::spinIn(70.0f, 9.0f)),
                        .stagger = stagger(unit::Cluster,
                                           {.eachMs = 30, .durationMs = 480}),
                        .progress = beat(at + 0.25, at + 0.25 + sealSpanS)}));
    seal.child(std::move(body));
    // THE ORDINAL at the centre, decoding — held, so a numeral waiting
    // its beat is absent rather than churning wrong — and GIMBALLED: it
    // carries the carrier's rotation backwards, so the one mark on the
    // seal that has to be read stands upright at every station the rim
    // brings it to while everything around it turns.
    seal.child(
        text(toU8(s.ordo), mono(12.0f, kGold, 1.0f))
            .key(id + "-ordo")
            .centerAt({kSealR, kSealR})
            .hitTestable(false)
            .rotate(bind(&sealUpright).target(0.0f, 360.0f))
            .effect(styles::textGlow(kHalo, 3.0f))
            .fx({.effect = fx::hold(fx::scramble(U"IVXLC", 12)),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 90, .durationMs = 620}),
                 .progress = beat(at + 0.55, at + 0.55 + sealSpanS * 0.9)})
            .fx({.effect = fx::keys({{0.00f, {}},
                                     {0.80f, {}},
                                     {0.90f, {.colorAdd = kCore}},
                                     {1.00f, {}}}),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 90, .durationMs = 620}),
                 .progress = beat(at + 0.55, at + 0.55 + sealSpanS * 0.9)}));
    return seal;
  }

  // ------------------------------------------------------------------
  // the emblem

  /** THE EMBLEM'S DISC of light — the SDF's one pass carrying fill and
   *  glow together, sized by the reserve the style declares so the box
   *  cannot crop its own falloff. */
  [[nodiscard]] Element emblemDisc() {
    const sdf::Style emblem{.fill = hex(0x1A1008, 0.66f),
                            .glowRadius = 34.0f,
                            .glowColor = hex(0xFFB13A, 0.5f)};
    const float side = sdf::minBoxFor(emblem, 2.0f * rEmblem * kR);
    return kit::disc(kEye, side * 0.5f)
        .key("emblem-disc")
        .hitTestable(false)
        .fill(sdf::material(sdf::circle(), emblem))
        .blend(SkBlendMode::kPlus)
        .opacity(&litHub);
  }

  /** THE EMBLEM at the centre — a monogram of the register, three
   *  letterforms set large, resolving out of a churn.
   *
   *  IT IS THE EMBLEM, and an emblem is the brightest stable thing on the
   *  plate: the one place the eye returns to between events, and the
   *  reason everything else reads as arranged AROUND something. So it is
   *  set bone rather than gold (the value hierarchy, not the hue, is what
   *  makes a centre dominant), it takes its own flash as each letterform
   *  lands, and it sits in a disc of light with a glow of its own. The
   *  blur is affordable HERE and nowhere else on the plate: this node is
   *  a hundred pixels across and does not orbit.
   *
   *  Six more letterforms ring it, riding the emblem's own hexagram as
   *  SIX OPEN CHORDS — the same addressed-by-fraction trick the captions
   *  use, one node for six marks. */
  [[nodiscard]] Element emblema() {
    Element hub =
        box().key("emblem").absolute().inset(0).hitTestable(false).rotate(
            bind(&hexSpin).target(0.0f, 360.0f));
    hub.child(rule("hex-chalk", kChalkHexagram, 1.1f, kIron, tInner, 0.8));
    hub.child(line("hub-dots", hubDots, 1.0f, kIron, tInner + 0.3, 0.7));
    hub.child(emissive("hub-lit", glows[kGlowHub], &litHub));
    hub.child(
        text(toU8(hubRuneText), rune(13.0f, kAsh, 0.0f))
            .key("hub-ring")
            .absolute()
            .inset(0)
            .hitTestable(false)
            .onPath({.path = kit::chords(
                         {.sides = 6,
                          .step = 2,
                          .radius = rHexagram * kR / (std::min(kW, kH) * 0.5f),
                          .inset = 14.0f}),
                     .at = 0.0f,
                     .align = TextPath::Align::Start,
                     .offset = 4.0f,
                     .autoFlip = true})
            .fx({.effect = fx::hold(fx::typeOn()),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 26, .durationMs = 260}),
                 .progress = beat(tHub - 0.3, tHub + 0.9)}));
    return hub;
  }

  /** The monogram itself, off the turning figure so it stands still while
   *  the emblem's hexagram revolves under it.
   *
   *  IT ARRIVES RATHER THAN DECODES, and the reason is a contract worth
   *  knowing: a code-point substitution is honoured only where the
   *  replacement carries the original's advance along the axis its run
   *  advances on, because a swap that differs there would move every
   *  letter after it — a reshape, not a redraw. The register's letterforms
   *  are proportional, so a churn through them is refused with a warning
   *  and the glyphs draw at rest. A decode belongs on an equal-advance
   *  charset, which is where the seals' numerals put it. */
  [[nodiscard]] Element monogramma() {
    Stagger letters =
        stagger(unit::Cluster, {.eachMs = 240, .durationMs = 760});
    return text(toU8(emblemText), rune(52.0f, kBone, 6.0f))
        .key("monogramma")
        .centerAt(kEye)
        .hitTestable(false)
        .effect(styles::textGlow(kHalo, 7.0f))
        .fx({.effect = fx::hold(fx::spinIn(90.0f, 14.0f)),
             .stagger = letters,
             .progress = beat(tHub, tHub + hubSpanS)})
        .fx({.effect = fx::keys({{0.00f, {}},
                                 {0.74f, {}},
                                 {0.88f, {.colorAdd = kHalo}},
                                 {1.00f, {}}}),
             .stagger = letters,
             .progress = beat(tHub, tHub + hubSpanS)});
  }

  // ------------------------------------------------------------------

  [[nodiscard]] Element wheel() {
    Element panel = box().key("rota").absolute().inset(0).hitTestable(false);

    // The ground wash under the figure.
    panel.child(
        box()
            .key("rota-wash")
            .absolute()
            .inset(0)
            .hitTestable(false)
            .fill(Material::glowUnit(
                {0.5f, 0.5f}, 0.62f,
                {{0.0f, kNightLift}, {0.66f, hex(0x0D0A18)}, {1.0f, kNight}})));

    // THE FLOOD: light thrown at the whole sheet from behind the figure.
    // It screens, so it lifts what is already there toward white instead
    // of laying a wash over it, and it is worth its full-panel gradient
    // only while it is on — at gain zero the node is not painted at all.
    panel.child(box()
                    .key("flood")
                    .absolute()
                    .inset(-120)
                    .hitTestable(false)
                    .fill(Material::glowUnit({0.5f, 0.5f}, 0.86f,
                                             {{0.0f, hex(0xFFD98A, 0.55f)},
                                              {0.42f, hex(0xE79A32, 0.30f)},
                                              {1.0f, hex(0xC96F1E, 0.0f)}}))
                    .blend(SkBlendMode::kScreen)
                    .opacity(&floodA));

    // THE RAYS, thrown past the figure at ignition — the reading that
    // makes an ignition a whole-frame event and not a brighter drawing.
    // They pass BEHIND the lettering: light coming out from the figure is
    // occluded by the figure, and rays laid over the type would only be a
    // veil across the words.
    panel.child(
        box()
            .key("rays")
            .absolute()
            .inset(-170)
            .hitTestable(false)
            .fill(Material::recipe(sigil::material::Material(raysRecipe()))
                      .uniform("uInk", kHalo))
            .blend(SkBlendMode::kPlus)
            .opacity(&raysA));

    // THE COMPASS WORK. Every rule below is half of a PAIR, and the pair
    // is what makes a band: no line on this plate stands on its own, and
    // nothing is written anywhere except between two of them.
    panel.child(rule("r-edge", kChalkEdge, 2.1f, kIron, 0.35, 1.3));
    panel.child(rule("r-edge-in", kChalkEdgeIn, 0.6f, kIronDim, 0.45, 1.2));
    panel.child(rule("r-rune-out", kChalkRuneOut, 1.1f, kIron, 0.55, 1.3));
    panel.child(rule("r-rune-in", kChalkRuneIn, 1.1f, kIron, 0.62, 1.3));
    panel.child(rule("r-vox-out", kChalkVoxOut, 0.5f, kIronDim, 0.70, 1.2));
    panel.child(rule("r-vox-in", kChalkVoxIn, 1.0f, kIron, 0.76, 1.3));
    panel.child(rule("r-nom-out", kChalkNomOut, 1.2f, kIron, 0.86, 1.3));
    panel.child(rule("r-nom-in", kChalkNomIn, 1.2f, kIron, 0.94, 1.3));
    panel.child(rule("r-nom-case", kChalkNomCase, 0.5f, kIronDim, 1.02, 1.2));

    // THE DIVISION LADDERS: the serration at the rim, and three length
    // classes inside it at two pitches — the density an engraved figure
    // carries and a single ladder cannot. Different bands take different
    // symmetry orders on purpose; a plate whose every ring counted twelve
    // would read as one drawing rather than as several mechanisms.
    panel.child(
        ladder("teeth", 240, 12, rEdge, rEdgeIn, 0.7f, kIron, 0.5, 1.7));
    // The ladder band is fenced like every other band and split in two:
    // the still classes in its outer half, the turning one in its inner
    // half, so two ladders never stamp the same mark. Every count is a
    // multiple of the twelve the plate is built on, and the mid class is
    // offset HALF A STEP from the long one so the two interleave instead
    // of doubling up.
    panel.child(rule("r-tick-mid", kChalkTickMid, 0.5f, kIronDim, 0.86, 1.2));
    panel.child(ladder("ticks-long", kStations, 0, rTickOut, rTickMid - 0.028f,
                       1.3f, kAshDim, 0.9, 1.4));
    panel.child(ladder("ticks-mid", 24, 2, rTickOut, rTickMid - 0.010f, 0.9f,
                       kIron, 0.9, 1.6, kPitch * 0.5f));
    panel.child(ladder("ticks-short", 144, 6, rTickOut, rTickMid, 0.7f, kIron,
                       0.9, 2.1));

    // THE FAST LAYER: a hairline ladder in the band's inner half, turning
    // at seconds per revolution against everything inside it. It is the
    // layer that can afford the rate — cached geometry replayed under a
    // bound transform, where the lettering it sits beside would have to
    // re-place every glyph.
    panel.child(box()
                    .key("ladder-turn")
                    .absolute()
                    .inset(0)
                    .hitTestable(false)
                    .rotate(bind(&tickSpin).target(0.0f, 360.0f))
                    .child(ladder("ticks-fine", 288, 6, rTickMid - 0.003f,
                                  rTickIn, 0.6f, kIron, 1.1, 2.3)));

    // The bands the rules frame.
    panel.child(invocatio());
    panel.child(registrum());
    panel.child(nomina());

    // The texture band and its own frame.
    panel.child(rule("r-tex-out", kChalkTexOut, 0.9f, kIron, tTex - 0.5, 1.0));
    panel.child(rule("r-tex-in", kChalkTexIn, 0.9f, kIron, tTex - 0.35, 1.0));
    panel.child(rule("r-tex-case", kChalkTexCase, 0.5f, kIronDim, tTex, 1.0));
    panel.child(
        rule("r-serr-in", kChalkSerrIn, 0.5f, kIronDim, tTex + 0.15, 1.0));
    panel.child(ladder("serration", 48, 4, rTexCase, rSerrIn, 0.7f, kIron,
                       tTex + 0.1, 1.4, kPitch * 0.25f));
    panel.child(textura());

    // The turning layers, outside in.
    panel.child(arcus());
    panel.child(rule("r-env", kChalkEnv, 1.0f, kIron, tStar + 0.3, 1.0));
    panel.child(
        rule("r-env-in", kChalkEnvIn, 0.5f, kIronDim, tStar + 0.45, 1.0));
    panel.child(stella());
    panel.child(stellaInterior());
    panel.child(limina());

    // THE HUB'S OWN FRAME is struck with the inner compound, not with the
    // emblem it will hold: the centre of a figure like this is never a
    // hole waiting to be filled, and a plate whose middle is empty for
    // eight seconds reads as unfinished rather than as forming.
    panel.child(
        rule("r-hub-out", kChalkHubOut, 1.6f, kIron, tInner - 0.4, 0.8));
    panel.child(
        rule("r-hub-in", kChalkHubIn, 0.6f, kIronDim, tInner - 0.3, 0.8));
    panel.child(
        rule("r-hub-case", kChalkHubCase, 0.9f, kIron, tInner - 0.2, 0.8));
    panel.child(ladder("hub-teeth", 72, 6, rHubOut, rHubIn, 0.6f, kIronDim,
                       tInner - 0.35, 0.9));
    panel.child(
        rule("r-hub-kern", kChalkHubKern, 0.7f, kIron, tInner + 0.1, 0.8));
    panel.child(emblemDisc());
    panel.child(emblema());
    panel.child(monogramma());

    // The light that lands on the struck rules, group by group. The ones
    // that do not turn are painted here; the ones that do are painted
    // inside the layers that turn them.
    panel.child(emissive("rim-lit", glows[kGlowRim], &litRim));
    panel.child(emissive("nom-lit", glows[kGlowNom], &litNom));

    // THE CARRIER: one node holding all twelve seals, turning them about
    // the circle's own centre. It is painted here, after the rim's light,
    // for the same reason each seal carries an opaque ground — a seal
    // SITS ON the plate, and a rule that runs under one does not print
    // across it however hard the rule is burning.
    //
    // A ring of stations that turns is what makes the seals read as
    // MOUNTED rather than as drawn at twelve places: the whole rim is one
    // mechanism, and the exception that proves it is the medallion below,
    // which is at the rim and does not travel — the seals pass behind it.
    Element ferrum =
        box().key("ferrum").absolute().inset(0).hitTestable(false).rotate(
            bind(&sealOrbit).target(0.0f, 360.0f));
    for (int k = 0; k < kSeals; ++k) ferrum.child(sigillum(k));
    panel.child(std::move(ferrum));

    panel.child(spur());

    // The embers: a live pool stamped as one draw, rising off the rim at
    // ignition and drizzling for as long as the circle is charged.
    panel.child(box()
                    .key("embers")
                    .absolute()
                    .inset(0)
                    .hitTestable(false)
                    .opacity(&emberA)
                    .child(instancing::instances(emberAtlas, embers,
                                                 instancing::Mode::Live,
                                                 SkBlendMode::kPlus)));

    // THE CREST'S FRINGE: half a second of the frame beneath re-sampled
    // with its channels pulled apart along the radius.
    panel.child(
        box()
            .key("fringe")
            .absolute()
            .inset(0)
            .hitTestable(false)
            .backdrop(Effect::shader(fringeEffect(),
                                     {{"uCx", kEye.x()}, {"uCy", kEye.y()}})
                          .uniform("uSpread", &fringeK))
            .opacity(&fringeA));

    // The scribe: the point of the pen, led round the band by the writing
    // cascade — placed every frame from the schedule read back, so it
    // cannot drift from the letters it appears to write.
    panel.child(box()
                    .key("scribe")
                    .left(-9)
                    .top(-9)
                    .width(18)
                    .height(18)
                    .hitTestable(false)
                    .fill(Material::glowUnit({0.5f, 0.5f}, 0.5f,
                                             {{0.0f, hex(0xFFE9B0)},
                                              {0.35f, hex(0xD8A94E, 0.55f)},
                                              {1.0f, hex(0xD8A94E, 0.0f)}}))
                    .translateX(&scribeX)
                    .translateY(&scribeY)
                    .opacity(&scribeA));
    return panel;
  }

  /** THE COLOPHON — two lines under the figure and nothing else. A circle
   *  of this kind is the whole picture; a panel of commentary beside it
   *  would be a diagram of a spell circle rather than one. */
  [[nodiscard]] Element colophon() {
    return box()
        .key("colophon")
        .absolute()
        .left(0)
        .right(0)
        .bottom(26)
        .column()
        .alignItems(Align::Center)
        .gap(7)
        .hitTestable(false)
        .child(text(toU8("ROTA CONVOCATIONIS"), label(12.0f, kAshDim, 5.2f))
                   .key("titulus")
                   // A lozenge stands at the word the whole figure
                   // converges on, anchored to the rect the selector
                   // resolves rather than to a number a caller measured.
                   .mark(sel::text(u8"ROTA"),
                         box()
                             .key("m-rota")
                             .left(pct(50))
                             .top(pct(126))
                             .width(5)
                             .height(5)
                             .shape(shapes::polygon(4))
                             .fill(Fill::color(kGold))
                             .opacity(beat(tIgnite + 0.9, tIgnite + 1.5)))
                   .fx({.effect = fx::rise(10.0f),
                        .stagger = {.eachMs = 0,
                                    .amountMs = 420,
                                    .durationMs = 520},
                        .progress = beat(0.35, 1.8)}))
        .child(text(toU8(std::to_string(totalGlyphs) +
                         " GLYPHS \xc2\xb7 23 CVRVED BASELINES \xc2\xb7 10 "
                         "TVRNING LAYERS \xc2\xb7 EVERY START CHAINED FROM A "
                         "SPAN, NONE FITTED BY HAND"),
                    label(8.5f, hex(0x8A8299, 0.42f), 2.4f))
                   .key("colophon-2")
                   .opacity(beat(tIgnite + 0.4, tIgnite + 1.2)));
  }

  [[nodiscard]] Element describe() {
    return stack()
        .fill(Material::glowUnit({0.5f, 0.5f}, 0.9f,
                                 {{0.0f, kNightLift}, {1.0f, kNight}}))
        .child(box()
                   .absolute()
                   .inset(0)
                   .opacity(envelope())
                   // The charged circle breathes: under a percent of
                   // scale, which is not a size change so much as the
                   // reading that a finished circle is holding something
                   // in.
                   .child(wheel().scale(&humScale))
                   .child(colophon()));
  }

  // ------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kNight);

    faceRing = pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 500);
    faceRingBold = pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 600);
    faceMono = pickFace({"Menlo", "SF Mono", "Courier New"}, 500);

    // ---- content, fitted to its own bands --------------------------------
    voxText = "+ ";
    for (int i = 0; i < 8; ++i) {
      voxText += kInvocatio[i];
      voxText += i < 7 ? " + " : " ";
    }
    nomText.clear();
    for (const char* n : kNames) {
      nomText += n;
      nomText += " \xc2\xb7 ";
    }
    // THREE SIZES OF SCRIPT, and the ratio between them is the plate's
    // depth: the names are the main register, the invocation and the
    // rune band flank it at roughly half, and the texture band runs at
    // roughly a quarter — small enough that it stops being letters and
    // becomes the grain the disc is made of. Every band is FITTED to its
    // own circumference, so the word counts below are how the sizes are
    // set: fewer, longer words make bigger letters.
    runeText = deal(0x5EED1, 34, 3, 6);
    texText = deal(0x5EED2, 84, 2, 5, true);
    hubRuneText = deal(0x5EED3, 6, 2, 2);
    emblemText = deal(0x5EED4, 1, 3, 3);
    spurText = deal(0x5EED5, 1, 1, 1);
    spurText.pop_back();  // one letterform, and no trailing space
    // A SEAL'S RING is two words of the register and a separator, and the
    // COUNT IS WHAT SETS THE SIZE, because the ring is fitted to its own
    // circumference like every other band here. Sixteen marks or so is
    // what a circle this small wants: fewer and the letters come out
    // taller than the annulus that fences them, more and the ring becomes
    // grain, which is the texture band's job and not a seal's.
    for (int k = 0; k < kSeals; ++k)
      sealText[k] = deal(0x5EE00u + (uint32_t)k, 2, 7, 8) + "\xc2\xb7 ";

    voxSize = fitToRing(ctx, voxText, ring(18, kBone, 2.2f), rVox * kR);
    runeSize = fitToRing(ctx, runeText, rune(20, kRuneInk, 2.0f), rRune * kR);
    nomSize = fitToRing(ctx, nomText, ring(30, kGold, 4.2f), rNom * kR);
    texSize = fitToRing(ctx, texText, rune(9, kAsh, 0.0f), rTex * kR, 0.995f);
    for (int k = 0; k < kSeals; ++k)
      sealSize[k] =
          fitToRing(ctx, sealText[k], ring(9, kBone, 1.4f), kSealRing, 0.97f);

    // ---- the writing cue table: pace, pausing at each cross --------------
    voxCues.clear();
    voxWords = 0;
    voxMaxWord = 1;
    {
      float t = 0.0f;
      size_t i = 0;
      while (i < voxText.size()) {
        while (i < voxText.size() && voxText[i] == ' ') ++i;
        if (i >= voxText.size()) break;
        size_t j = i;
        while (j < voxText.size() && voxText[j] != ' ') ++j;
        voxCues.push_back(t);
        ++voxWords;
        voxMaxWord = std::max(voxMaxWord, (int)(j - i));
        t += kStepMs;
        if (voxText[i] == '+') t += kCrossMs;
        i = j;
      }
    }

    // ---- the timeline, every window chained from a span ------------------
    voxSpanS =
        voxCascade().spanMs((uint32_t)voxWords, (uint32_t)voxMaxWord) / 1000.0f;
    runeSpanS = stagger(unit::Cluster, {.eachMs = 7,
                                        .durationMs = 420,
                                        .from = Stagger::From::Random,
                                        .seed = 17})
                    .spanMs((uint32_t)glyphsOf(runeText)) /
                1000.0f;
    {
      Stagger form = stagger(unit::Word, {.amountMs = 1900});
      form.then(unit::Cluster, {.eachMs = 24, .durationMs = 420});
      nomSpanS = form.spanMs(18, 14) / 1000.0f;
    }
    texSpanS = stagger(unit::Cluster, {.eachMs = 4,
                                       .durationMs = 300,
                                       .from = Stagger::From::Random,
                                       .seed = 61})
                   .spanMs((uint32_t)glyphsOf(texText)) /
               1000.0f;
    limenSpanS =
        stagger(unit::Cluster, {.eachMs = 30, .durationMs = 120}).spanMs(15) /
        1000.0f;
    sealSpanS = stagger(unit::Cluster, {.eachMs = 30, .durationMs = 480})
                    .spanMs((uint32_t)glyphsOf(sealText[0])) /
                1000.0f;
    hubSpanS =
        stagger(unit::Cluster, {.eachMs = 240, .durationMs = 760}).spanMs(3) /
        1000.0f;
    // THE SHIMMER'S PERIOD is the span the same ladder would take as a
    // one-shot — read off a NON-looping copy, because a looping cascade
    // answers its own period when asked for its span and would only tell
    // this back what it was already given.
    shimmerS = stagger(unit::Cluster, {.eachMs = 26, .durationMs = 620})
                   .spanMs((uint32_t)glyphsOf(runeText)) /
               1000.0f;

    tVox = 1.2;
    tRune = tVox + voxSpanS * 0.46;
    tNames = tRune + runeSpanS * 0.62;
    tTex = tNames + nomSpanS * 0.52;
    tArc = tTex + texSpanS * 0.55;
    tStar = tArc + 1.5;
    tInner = tStar + 1.4;
    for (int k = 0; k < kLimens; ++k)
      limenAt[k] = tInner + 0.5 + k * limenSpanS * 0.62;
    tSeal[0] = limenAt[2];
    for (int k = 1; k < kSeals; ++k)
      tSeal[k] = tSeal[k - 1] + sealSpanS * kSealBeat;
    tHub = tSeal[kSeals - 1] + sealSpanS * 0.85;
    tIgnite = tHub + hubSpanS + 0.5;
    loopSecs = tIgnite + 2.6 + 2.8 + 1.6;

    // Mid-cycle among the seals: every band written, both compounds
    // struck, the fire round the rim about half way — seven seals lit,
    // one forming, the rest dark, and the carrier far enough round that
    // the ring is visibly off its stations.
    ctx.captureAt(tSeal[7] + sealSpanS * 0.55);

    // The chained timeline, printed: every number below came out of a
    // span, and this is where to read what the chaining resolved to.
    std::fprintf(stderr,
                 "[rota] vox %.2f+%.2fs  register %.2f+%.2fs (shimmer %.2fs)  "
                 "nomina %.2f+%.2fs  textura %.2f+%.2fs  arcs %.2f  stars "
                 "%.2f/%.2f  seals %.2f..%.2f (+%.2fs each)  emblem %.2f+%.2fs "
                 " ignition %.2f  loop %.2fs\n",
                 tVox, voxSpanS, tRune, runeSpanS, shimmerS, tNames, nomSpanS,
                 tTex, texSpanS, tArc, tStar, tInner, tSeal[0],
                 tSeal[kSeals - 1], sealSpanS, tHub, hubSpanS, tIgnite,
                 loopSecs);

    totalGlyphs = glyphsOf(voxText) + glyphsOf(runeText) + glyphsOf(nomText) +
                  glyphsOf(texText) + glyphsOf(hubRuneText) +
                  glyphsOf(emblemText) + glyphsOf(spurText);
    for (int k = 0; k < kSeals; ++k)
      totalGlyphs += glyphsOf(sealText[k]) + glyphsOf(kSealTable[k].ordo);
    for (const char* l : kLimina) totalGlyphs += glyphsOf(l);

    bakeGeometry();
    ctx.composer.render(describe());
  }

  // ------------------------------------------------------------------
  /** Everything the figure draws that is a PATH rather than a box, cooked
   *  once before the first describe and never touched again: the chalk's
   *  wobble, each lighting group's unioned emissive stack, and the outer
   *  compound's morph ladder. All of it is content-static, which is what
   *  entitles the nodes that wear it to record once and replay under
   *  bound transforms and bound gains for the rest of the loop. */
  void bakeGeometry() {
    namespace ops = sigil::geometry::path::ops;
    namespace geom = sigil::geometry::path;

    chalk.assign((size_t)kChalkCount, SkPath());
    auto chalkRing = [&](int slot, float rNorm, uint32_t seed) {
      chalk[(size_t)slot] = chalked(ringPath(rNorm), seed);
    };
    chalkRing(kChalkEdge, rEdge, 11);
    chalkRing(kChalkEdgeIn, rEdgeIn, 23);
    chalkRing(kChalkRuneOut, rRuneOut, 37);
    chalkRing(kChalkRuneIn, rRuneIn, 53);
    chalkRing(kChalkVoxOut, rVoxOut, 71);
    chalkRing(kChalkVoxIn, rVoxIn, 89);
    chalkRing(kChalkTickMid, rTickMid, 97);
    chalkRing(kChalkNomOut, rNomOut, 101);
    chalkRing(kChalkNomIn, rNomIn, 113);
    chalkRing(kChalkNomCase, rNomCase, 127);
    chalkRing(kChalkTexOut, rTexOut, 139);
    chalkRing(kChalkTexIn, rTexIn, 151);
    chalkRing(kChalkTexCase, rTexCase, 163);
    chalkRing(kChalkSerrIn, rSerrIn, 167);
    chalkRing(kChalkEnv, rEnv, 173);
    chalkRing(kChalkEnvIn, rEnvIn, 181);
    chalkRing(kChalkHubOut, rHubOut, 191);
    chalkRing(kChalkHubIn, rHubIn, 197);
    chalkRing(kChalkHubCase, rHubCase, 211);
    chalkRing(kChalkHubKern, rHubKern, 217);

    // THE FIGURES. A star compound is `kit::chords` with a step: twelve
    // stations stepped by three is three squares, stepped by four is four
    // triangles, and the library returns exactly gcd(sides, step) closed
    // rings rather than treating the non-coprime case as an error.
    const kit::Frame frame{.centre = kEye, .radius = kR};
    const SkPath star = kit::chords(
        frame,
        {.sides = kStations, .step = 3, .radius = rStar, .closed = true});
    const SkPath inner = kit::chords(
        frame,
        {.sides = kStations, .step = 4, .radius = rInner, .closed = true});
    const SkPath hexagram = kit::chords(
        frame, {.sides = 6, .step = 2, .radius = rHexagram, .closed = true});
    // The arcs: two rings of twelve, each arc short of its own pitch, the
    // inner ring turned half a pitch so the pair reads as a mechanism and
    // not as two circles.
    const SkPath arcs = [&] {
      SkPathBuilder b;
      b.addPath(arcRing(rArcOut, kStations, kPitch * 0.80f, 0.0f));
      b.addPath(arcRing(rArcIn, kStations, kPitch * 0.62f, kPitch * 0.5f));
      return b.detach();
    }();
    arcSpokes = spokeRing(kStations, rArcIn, rArcOut, kPitch * 0.5f);
    arcNodes = nodeRing(kStations, (rArcIn + rArcOut) * 0.5f, 5.0f, 0.0f);
    starSpokes = spokeRing(kStations, rEnv, rStar, 0.0f);
    starNodes = nodeRing(kStations, rStar, 9.0f, 0.0f);
    innerSpokes = spokeRing(kStations, rHubOut, rInner, kPitch * 0.5f);
    hubDots = nodeRing(6, rHubKern, 4.0f, kPitch);
    // A DASHED RING is a ring at a duty cycle: sixty marks of a little
    // under half a pitch each, which the eye reads as broken rather than
    // as sixty things.
    const SkPath dashed =
        arcRing(rNomCase - 0.010f, 60, (360.0f / 60.0f) * 0.44f, 0.0f);
    // THE CRESCENTS: three, where everything else counts twelve.
    const SkPath crescents =
        crescentRing(rCrescOut, rCrescIn, 3, 66.0f, kPitch * 2.5f, 6);
    // THE OFF-ORDER MARK: one medallion straddling the outermost rule at
    // twelve o'clock, cutting the serration and the register alike. A
    // plate that is perfectly regular everywhere reads as a pattern; one
    // mark that obeys nothing is what makes the rest read as a drawing.
    {
      const SkPoint c = P(0.0f, rEdge);
      SkPathBuilder b;
      b.addOval(SkRect::MakeLTRB(c.fX - kSpurR, c.fY - kSpurR, c.fX + kSpurR,
                                 c.fY + kSpurR));
      const float ri = kSpurR * 0.60f;
      b.addOval(SkRect::MakeLTRB(c.fX - ri, c.fY - ri, c.fX + ri, c.fY + ri));
      spurRules = b.detach();
    }

    chalk[kChalkArcs] = chalked(arcs, 223, 0.9f);
    chalk[kChalkDash] = chalked(dashed, 239, 0.6f);
    chalk[kChalkCresc] = chalked(crescents, 241, 0.8f);
    chalk[kChalkStar] = chalked(star, 227, 0.9f);
    chalk[kChalkInner] = chalked(inner, 229, 0.8f);
    chalk[kChalkHexagram] = chalked(hexagram, 233, 0.6f);

    // THE LIGHTING GROUPS. Each is a set of lines that ignite together
    // and turn together; nothing in one crosses anything in another,
    // which is the invariant that lets seven unions be rotated
    // independently and still add cleanly.
    glows.assign((size_t)kGlowCount, Glow{});
    glows[kGlowRim] = bakeGlow({ringPath(rEdge), ringPath(rEdgeIn),
                                ringPath(rRuneOut), ringPath(rRuneIn)},
                               0.85f);
    glows[kGlowNom] =
        bakeGlow({ringPath(rVoxIn), ringPath(rNomOut), ringPath(rNomIn)}, 0.7f);
    glows[kGlowArc] = bakeGlow({arcs, arcSpokes, arcNodes, dashed}, 0.7f);
    glows[kGlowStar] =
        bakeGlow({star, starSpokes, starNodes, crescents}, 0.85f);
    glows[kGlowInner] = bakeGlow({inner, innerSpokes}, 0.75f);
    glows[kGlowHub] = bakeGlow({ringPath(rHubOut), ringPath(rHubCase),
                                ringPath(rHubKern), hexagram, hubDots},
                               0.8f);
    glows[kGlowSpur] = bakeGlow({spurRules}, 0.8f);

    // THE STRIKE-MORPH: the light arrives as a scribble and resolves onto
    // the true figure. Both ends are resampled to the same point count on
    // the same arc-length parameterisation and the ladder is the lerp
    // between them, so step 0 is the scribble, the last step is the
    // compound exactly, and nothing in between has to be computed again.
    constexpr int kSteps = 10;
    constexpr int kSamples = 200;
    const SkPath scribble =
        ops::Roughen{.amplitude = 15.0f, .segmentPx = 16.0f, .seed = 211}(star);
    const std::vector<geom::Sampled> from = geom::resample(scribble, kSamples);
    const std::vector<geom::Sampled> to = geom::resample(star, kSamples);
    starSteps.assign((size_t)kSteps, SkPath());
    for (int i = 0; i < kSteps; ++i) {
      const float t = (float)i / (float)(kSteps - 1);
      SkPathBuilder b;
      for (size_t c = 0; c < to.size() && c < from.size(); ++c)
        b.addPath(geom::toPath(geom::lerp(from[c], to[c], t), true));
      starSteps[(size_t)i] = b.detach();
    }

    // THE EMBERS: one cell baked from a soft dot, and a pool the ticker
    // owns. The stamp is one draw whatever the count, which is what lets
    // the drizzle keep running for the whole charged idle.
    emberAtlas = std::make_shared<instancing::Atlas>(2.0f);
    emberFrame = emberAtlas->cell(
        box().fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                      {{0.0f, hex(0xFFF3D2)},
                                       {0.22f, hex(0xFFD277, 0.85f)},
                                       {0.55f, hex(0xE79A32, 0.30f)},
                                       {1.0f, hex(0xC96F1E, 0.0f)}})),
        {22, 22});
    embers = std::make_shared<instancing::Pool>();
    embers->resize(kEmbers);
  }

  /** THE STRIKE, and what it leaves burning. Three readings compose into
   *  one scalar per lighting group, which is why they are stepped here
   *  rather than declared as a window each: the cross-fade that lands the
   *  light on a struck rule, the flash the strike itself throws — up in
   *  two frames, down over half a second, the shape a hot thing has and a
   *  fade does not — and, once the circle is charged, the breath that
   *  keeps it from ever reading as a finished picture again. */
  float strike(double tc, double at, float rest) const {
    if (tc < at) return 0.0f;
    const double since = tc - at;
    const float onset = (float)std::min(1.0, since / 0.30);
    const float flash = (float)std::exp(-since / 0.34) * 1.0f;
    const float charged =
        tc > tIgnite ? 0.10f + 0.05f * (float)std::sin((tc - tIgnite) * 1.35)
                     : 0.0f;
    return std::min(1.0f,
                    onset * (rest + charged) + flash * (1.0f - rest * 0.4f));
  }

  /** The ignition's own envelope: a fast rise to the crest and a long
   *  decay, so what follows the crest is an AFTERGLOW and not a cut. */
  static float burst(double tc, double at, double rise, double fall) {
    if (tc < at) return 0.0f;
    const double since = tc - at;
    if (since < rise) return (float)(since / rise);
    return (float)std::exp(-(since - rise) / fall);
  }

  /** Every gain the fire drives, plus the ember pool. */
  void stepFire(double tc) {
    litRim = strike(tc, tVox + voxSpanS, 0.34f);
    litNom = strike(tc, tNames + nomSpanS, 0.30f);
    litArc = strike(tc, tArc + 1.1, 0.30f);
    litSpur = strike(tc, tVox + voxSpanS * 0.5, 0.30f);
    litStar = strike(tc, tStar + 1.1, 0.42f);
    litInner = strike(tc, tInner + 1.1, 0.36f);
    litHub = strike(tc, tHub + hubSpanS * 0.9, 0.46f);
    for (int k = 0; k < kSeals; ++k)
      litSeal[k] = strike(tc, tSeal[k] + 0.25 + sealSpanS, 0.30f);

    // The morph walks its ladder over the strike's own half-second. The
    // walk runs from one step BEFORE the first to one PAST the last, so
    // the ladder is dark at both ends: nothing of the scribble stands on
    // the plate before the strike, and nothing of it is left behind when
    // the emissive stack takes the figure over.
    const double morphAt = tStar + 1.1;
    morphStep = -1.0f + (float)(std::clamp((tc - morphAt) / 0.78, 0.0, 1.0) *
                                (double)(starSteps.size() + 1));

    // IGNITION IS FULL-FRAME. The rays are the widest and the shortest,
    // the flood the deepest, the fringe half a second at the crest alone.
    raysA = 0.85f * burst(tc, tIgnite + 0.15, 0.22, 0.85);
    floodA = 0.9f * burst(tc, tIgnite + 0.1, 0.30, 1.5) +
             (tc > tIgnite + 2.0
                  ? 0.06f + 0.03f * (float)std::sin((tc - tIgnite) * 1.35)
                  : 0.0f);
    const float fringe = 1.7f * burst(tc, tIgnite + 0.26, 0.09, 0.20);
    fringeK = fringe;
    fringeA = fringe > 0.02f ? 1.0f : 0.0f;
    humScale =
        tc > tIgnite
            ? 1.0f + 0.005f * (float)std::sin((tc - tIgnite) * 1.35 - 1.57)
            : 1.0f;

    stepEmbers(tc);
  }

  /** THE EMBERS: seeded on the rim, thrown up at ignition, then a sparse
   *  drizzle for as long as the circle is charged. Each spark is a pure
   *  function of its index and the time since ignition, so the pool needs
   *  no per-frame state and the loop's wrap re-deals it exactly. */
  void stepEmbers(double tc) {
    if (!embers) return;
    const double since = tc - tIgnite;
    emberA = since < 0 ? 0.0f
                       : std::min(1.0f, (float)(since / 0.25)) *
                             (0.30f + 0.70f * burst(tc, tIgnite, 0.25, 1.1));
    if (emberA.value() <= 0.0f) return;
    auto positions = embers->positions();
    auto scales = embers->scales();
    auto tints = embers->tints();
    auto frames = embers->frames();
    for (int i = 0; i < kEmbers; ++i) {
      const float u = (float)i / (float)kEmbers;
      const float seed = std::fmod(u * 7919.0f, 1.0f);
      const float life = 2.2f + seed * 2.4f;
      const float age = (float)std::fmod(std::max(0.0, since) + u * life, life);
      const float t = age / life;
      const float th = (u * 360.0f + seed * 53.0f) * kDeg;
      // Struck off the RIM and rising: the sparks belong to the edge of
      // the figure, not to the field it encloses, and lettering is not
      // improved by fireflies over it.
      const float r = kR * (0.90f + 0.10f * seed);
      const float sway = std::sin(age * 1.9f + seed * 6.28f) * 13.0f;
      positions[(size_t)i] = {kEye.x() + r * std::sin(th) + sway,
                              kEye.y() - r * std::cos(th) - t * 190.0f};
      scales[(size_t)i] = (0.45f + seed * 0.75f) * (1.0f - t * 0.5f);
      const float a = std::pow(1.0f - t, 1.4f);
      tints[(size_t)i] = {1.0f, 0.93f - 0.12f * seed, 0.74f, a};
      frames[(size_t)i] = emberFrame;
    }
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    const double tc = std::fmod(elapsed, loopSecs);
    cycle = (float)tc;
    secs = (float)elapsed;

    // THE TURNING, phased so nothing moves until it has formed, and RATED
    // so the counter-rotation is legible: every period below is SECONDS
    // per revolution and not minutes, because a rate the eye cannot
    // resolve in one glance is not mechanism, it is drift. No two layers
    // share a period and NEIGHBOURS TURN OPPOSITE WAYS — that pairing, not
    // the absolute speed, is what makes the plate read as a train of gears
    // rather than as a picture being spun. Geometry takes the fast
    // periods, lettering the slow ones: a ladder is cached geometry
    // replayed under a bound transform, where a ring of type has to
    // re-place every glyph.
    auto turn = [&](double on, double period) {
      if (tc <= on) return 0.0f;
      const double p = (tc - on) / period;
      return (float)(p - std::floor(p));
    };
    const double runeOn = tRune + runeSpanS;
    const double namesOn = tNames + nomSpanS;
    tickSpin = turn(2.3, 19.0);
    runeDrift = turn(runeOn, -96.0);
    voxDrift = turn(tIgnite, 132.0);
    nomDrift = turn(namesOn, -84.0);
    texDrift = turn(tTex + texSpanS, 52.0);
    arcSpin = turn(tArc + 1.2, -44.0);
    starSpin = turn(tStar + 1.3, 31.0);
    innerSpin = turn(tInner + 1.3, -17.0);
    hexSpin = turn(tHub + hubSpanS, 62.0);
    // The shimmer is pinned at exactly 0 until the register is written,
    // where every glyph rests at its landed deviation and the crest is
    // nowhere; after that it wraps forever on the cascade's own period.
    runePhase = tc > runeOn ? turn(runeOn, shimmerS) : 0.0f;
    // THE RIM'S OWN TRAIN, three rates deep and each about four times the
    // one that carries it, so what the eye reads is nested mechanism and
    // not one speed applied three times. The CARRIER is the slowest
    // turning thing on the plate that is not lettering — a wheel moving
    // rather than a spin — and it runs AGAINST the register band beneath
    // it, so the seals visibly cross the script they stand on. Each SEAL
    // turns about its own centre four times faster, and NEIGHBOURS TURN
    // OPPOSITE WAYS: the rim reads as a gear train because adjacent teeth
    // must. Each polygon turns faster again and against its own seal. The
    // carrier starts when the first seal lights: a ring of stations is a
    // fixed array until there is something mounted on it to carry.
    sealOrbit = turn(tSeal[0], 68.0);
    sealUpright = 1.0f - sealOrbit.value();
    for (int k = 0; k < kSeals; ++k) {
      sealSpin[k] = turn(tSeal[k] + 0.3, k % 2 ? 17.0 : -17.0);
      sealCog[k] = turn(tSeal[k] + 0.3, k % 2 ? -9.0 : 9.0);
    }

    stepFire(tc);

    // The scribe, placed from the schedule read back: the most recently
    // opened beat of the writing cascade is where the pen is.
    float bestStart = -1.0f;
    SkRect at = SkRect::MakeEmpty();
    for (const Beat& b : ctx.composer.beatsOf("vox", 0))
      if (b.localT > 0.0f && b.localT < 1.0f && b.startMs > bestStart) {
        bestStart = b.startMs;
        at = b.rect;
      }
    const bool writing =
        bestStart >= 0.0f && tc > tVox && tc < tVox + (double)voxSpanS;
    if (writing) {
      scribeX = at.centerX();
      scribeY = at.centerY();
    }
    // The dying glow decays in TIME, not per frame, so a jittering frame
    // interval cannot change how long the pen's light lingers.
    const double dt = std::max(0.0, elapsed - lastElapsed);
    lastElapsed = elapsed;
    scribeA = writing ? 1.0f : std::max(0.0, scribeA.value() - 4.5 * dt);
  }
};

SIGIL_SKETCH(RotaConvocationis, "Study \xc2\xb7 Type",
             "An invented conjuring wheel in the Solomonic idiom \xe2\x80\x94 "
             "fourteen curved baselines assembling ring by ring, every start "
             "chained from a span")
