#pragma once

#include "Settings.h"

struct SigillumAemeth {
  sk_sp<SkTypeface> faceSeal, faceRing, faceQuill, faceSerif, faceItalic,
      faceMono, faceDisplay;

  // ONE Output turns three systems that cannot agree except at 12 o'clock
  ch::Output<float> settle{0.0f};

  feed::TextRing logA{64}, logB{64}, logC{64}, logD{64};
  Weave weave;
  std::array<Solved, 7> solved;
  std::array<bool, 40> visited{};
  int usedCells = 0;
  double clockT = 0;
  int solvePhase = -2;

  Paint waxGrain;
  Pattern waxSpeck;

  // --- the words -----------------------------------------------------------

  /** data/content.json, read once in setup, carrying the figures the walk
   *  measured under the names its sentences call them by. */
  sketch::kit::Document doc;

  /** THE MARGIN'S WHOLE LOOK, as classes on the panel: the terminal face at
   *  the body size in the note ink is the panel's own font, so a line that
   *  names no class is the note and a class states only what differs — the
   *  rubric of a heading, the display cut of a Name, the quill of an
   *  archangel, the seal face of a letter on the fan. */
  [[nodiscard]] weaveNs::StyleSheet voices() const {
    return weaveNs::StyleSheet{
        {"h1",
         weaveNs::Type{
             .face = faceDisplay, .size = 46, .color = kVellum, .track = 2.6f}},
        {"lead",
         weaveNs::Type{
             .face = faceItalic, .size = 19, .color = hexColor(0xc7ab74)}},
        {"serif", weaveNs::Type{.face = faceSerif}},
        {"italic", weaveNs::Type{.face = faceItalic}},
        {"gloss",
         weaveNs::Type{
             .face = faceItalic, .size = 14, .color = hexColor(0x6f5f45)}},
        {"legend",
         weaveNs::Type{.face = faceSerif, .color = hexColor(0x9d8a66)}},
        {"index", weaveNs::Type{.size = 17}},
        {"name",
         weaveNs::Type{
             .face = faceDisplay, .size = 30, .color = kVellum, .track = 1.2f}},
        {"raw", weaveNs::Type{.face = faceItalic, .color = hexColor(0x6f5f45)}},
        {"chain", weaveNs::Type{.size = 14, .color = kTrace}},
        {"archangel",
         weaveNs::Type{
             .face = faceQuill, .size = 21, .color = hexColor(0xd8c08a)}},
        {"fan", weaveNs::Type{.face = faceSeal, .size = 23, .color = kVellum}},
        {"rubric", weaveNs::Type{.color = kRubric}},
        {"heading", weaveNs::Type{.size = 15, .color = kRubric, .track = 1.6f}},
    };
  }

  // --- the reading order, in seconds ---------------------------------------
  static constexpr float tPlate = 0.05f;
  static constexpr float tCells = 0.35f;
  static constexpr float tInner = 1.05f;
  static constexpr float tSolve = 2.00f;
  static constexpr float tSolveEach = 0.92f;
  static constexpr float tDark = tSolve + 7 * tSolveEach + 0.35f;  // ~8.8
  static constexpr float tBirds = 9.6f;
  static constexpr float tSpin = 15.4f;

  // =========================================================================
  // THE WAX

  Element waxGround();

  // =========================================================================
  // THE CIRCUMFERENCE — 40 cells of 9°, division 1 at 12 o'clock, running
  // clockwise "toward the right hand".

  /** The rules and the radial hatch field: invariant under rotation about
   *  their own centre, so they stay OUT of the turning layer. Inside it they
   *  would be resampled every frame of the settle and look identical. */
  Element circumferenceRules();

  /** What the 40 divisions actually are: the cells, and only the cells. */
  Element circumferenceCells();

  // =========================================================================
  // THE SEVEN ANGLES — 7 rows of 7 letters on the sides of a heptagon, with
  // a little cross at every corner of the segments they sit in.

  Element angles();

  // =========================================================================
  // THE HEPTAGON and the seven Names of God, written along its sides with a
  // quill — the seven sides come out at seven weights from one nib.

  Element heptagonNames();

  // =========================================================================
  // THE HEPTAGRAM — an interlaced band over seven DISCOVERED crossings.
  //
  // The over/under is the geometry library's crossing rule: the crossings
  // are found by path intersection, the order is `crossing::alternateAlong()`
  // prepared against them — see Weave — and the region
  // repainted at each knot is `crossingPatch`, the EXACT lens where the two
  // marks overlap. A disc is the wrong shape there: two bands meeting at a
  // shallow angle overlap in a long lens whose extent goes as
  // reach/sin(theta), and a disc sized for the perpendicular case leaves
  // the under-band showing straight across the over-band's cut.

  Element heptagram();

  // =========================================================================
  // the concentric cell rules, the four orders of the Children of Light,
  // ZABATHIEL's heptagon, the pentagram, the cross.

  /** Concentric circles and a crosshatched annular recess: also invariant
   *  under rotation, also lifted out of the turning layer. */
  Element innerRings();

  Element inner();

  static constexpr float kHp = 0.245f * kR;  // the pentagram's own half-box

  Element pentagram();

  static constexpr float kHc = 0.118f * kR;  // the cross's own half-box

  Element centreCross();

  // =========================================================================
  // THE SOLVER, ON THE PLATE. Seven names; each hop is an arc that leaps the
  // rim, drawn on by a trim window.

  Element solverOverlay();

  // =========================================================================
  // THE MARGIN — title, the seven Names assembling, the 7×7 square, legend.

  Element margin();

  /** The seven Names as flow rows, printing as the walk finds them: the
   *  index, the Name, the raw reading where it differs, and the chain of
   *  cells it came off. */
  Element nameRows();

  /** THE SEVEN ANGLES UNROLLED, not tabulated. On the plate these rows lie
   *  along seven sides of a heptagon; here they lie on seven nested arcs of
   *  the same fan, so a "column" is a RADIAL RAY and reading down a column is
   *  reading outward — which is what the columns do on the object. A leader
   *  curves from each ray to the archangel it spells. */
  Element basketFan();

  // =========================================================================

  feed::TextOptions logStyle();

  Element consolePanel();

  Element colophon();

  // =========================================================================

  Element describe(sketch::SketchContext&);

  // =========================================================================
  // THE CHECKS

  void runChecks();

  // =========================================================================

  void setup(sketch::SketchContext& ctx);

  /** The one discrete state this sketch has: which Name the solver is
   *  currently walking. Everything else on the plate is continuous and rides
   *  the ticker's Outputs, so it is described once and never re-described.
   *
   *  The walk cannot: each Name is a different path with a different contour
   *  count, so advancing it means new geometry. That is what slot() is for —
   *  `renderSlot` rebuilds ONLY the solver overlay, a handful of nodes, and
   *  leaves the rest of the tree (and its baked layers) untouched. A full
   *  render() to advance one hop would rebuild the whole seal. */
  void update(double, sketch::SketchContext& ctx);
};
