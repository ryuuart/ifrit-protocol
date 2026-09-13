#pragma once

#include "Settings.h"

struct DunhuangStarChart : sketch::Sketch {
  sk_sp<SkTypeface> faceSerif, faceItalic, faceMono, faceDisplay, faceHan;

  /** THE PLATE'S WHOLE LOOK, AS CLASSES. The running voice is the root's own
   *  font, so a line that names no class is the plate's note and a class
   *  states only what differs from it: a colour for the registers that are a
   *  colour, a size and a colour for the small hands, a face for the Han
   *  graphs. `heading` is the one named voice — the display face in the
   *  chart's gold, tracked, over whatever size the heading states. Bound on
   *  the description and again on the audit panel, which is described into
   *  its slot from the clock. */
  [[nodiscard]] weave::StyleSheet voices() const {
    return weave::StyleSheet{
        {"heading",
         weave::Type{
             .face = faceDisplay, .color = hexColor(0xc9a35c), .track = 1.0f}},
        // THE SIZES FIRST, THE COLOURS AFTER, because a later entry stands over
        // an earlier one by name: `styleClass("caption dim")` is then the small
        // hand in the quiet ink, which is what a plate of notes is set in.
        {"caption", weave::Type{.size = 8.4f, .color = hexColor(0x9a8a68)}},
        {"paper", weave::Type{.size = 8.0f, .color = hexColor(0x4a3b28, 0.9f)}},
        {"tick", weave::Type{.size = 7.4f, .color = hexColor(0x8a7458, 0.6f)}},
        {"ruler",
         weave::Type{.size = 7.6f, .color = hexColor(0xc9a35c, 0.85f)}},
        {"han", weave::Type{.face = faceHan ? faceHan : faceSerif,
                            .size = 11.5f,
                            .color = hexColor(0x2a2118, 0.88f)}},
        {"brush", weave::Type{.face = faceHan ? faceHan : faceSerif,
                              .size = 12.5f,
                              .color = hexColor(0x241d15, 0.92f)}},
        {"mapno", weave::Type{.face = faceDisplay,
                              .size = 13.0f,
                              .color = hexColor(0x4a3b28, 0.82f)}},
        {"xiu", weave::Type{.size = 7.0f, .color = hexColor(0x5d4c37, 0.75f)}},
        {"note", weave::Type{.color = hexColor(0x9a8a68)}},
        {"dim", weave::Type{.color = hexColor(0x6d6249)}},
        {"chalk", weave::Type{.color = kChalk}},
        {"number", weave::Type{.color = hexColor(0xcf6a4a)}},
        {"flag", weave::Type{.color = hexColor(0xb4531f)}},
        {"pass", weave::Type{.color = hexColor(0x6ba87e)}},
        {"gold", weave::Type{.color = hexColor(0xc9a35c)}},
        {"vermilion", weave::Type{.color = hexColor(0x8a3020, 0.95f)}},
        // THE THREE SCHOOLS ARE CLASSES, so a name and the dot beside it take
        // one statement: S.3326 is the first document to colour them, and
        // cinnabar, carbon and oxidised lead white are what it colours them
        // with.
        {"shishi", weave::Type{.color = kCinnabar}},
        {"ganshi", weave::Type{.color = kInk}},
        {"wuxian", weave::Type{.color = kLead}},
        {"undeclared", weave::Type{.color = kInkFaint}},
        // THE PARTS A CHART DRAWS, in this plate's own inks rather than the
        // theme's: the pole wheel's rings, its track, its dots and its
        // captions, the departure curves, and the hand's own residual ruled
        // across them.
        {"plotRule", weave::Type{.color = hexColor(0x8a7458, 0.30f)}},
        {"plotTrace", weave::Type{.color = kTrace}},
        {"plotMark", weave::Type{.color = kCinnabar}},
        {"plotLabel", weave::Type{.size = 7.4f, .color = hexColor(0x9a8a68)}},
        {"ghost", weave::Type{.color = hexColor(0x8a7458, 0.45f)}},
        {"hand", weave::Type{.color = hexColor(0xa8382a, 0.45f)}},
    };
  }

  // --- the words -----------------------------------------------------------

  /** data/content.json, read once in setup, carrying the figures this
   *  study measured under the names its sentences call them by. */
  sketch::kit::Document doc;

  /** THE DOCUMENT'S LINES AS A COLUMN — the shape every note on this plate
   *  takes, each line in the class the file gave it. */
  [[nodiscard]] Element noteStack(std::string_view key) const {
    return box().column().gap(2.4f).children(
        each(doc.run(key), sketch::kit::lineOf));
  }

  // ONE Output writes the plate: the score position in seconds.
  ch::Output<float> scribe{0.0f};
  double clockT = 0;

  feed::TextRing logA{72}, logB{72}, logC{72};

  /** THE ONE DISCRETE STATE, AND IT IS A CACHING STATE. Every reveal on this
   *  plate is a window() on `scribe`, which is a BOUND property and therefore
   *  counts as live volatility for ever — even once its value has been 1.0
   *  for ten seconds. Past the score's end the same reveal is described as a
   *  plain constant instead, so the several hundred nodes that carry one fall
   *  back into their parents' recordings. The cost is the two render() calls
   *  per loop that flip the state, and they are cheap here because nothing
   *  below re-measures text. */
  bool settled = false;
  Animatable<float> gate(float t0, float t1) const {
    if (settled) return Animatable<float>(1.0f);
    return Animatable<float>(bind(&scribe).window(t0, t1));
  }
  /** A mark that COMES AND GOES: it ramps 0 → 1 across [t0, t1], then
   *  falls back to 0 by t2, so the mark is a visit rather than a residue.
   *  One binding carries the whole visit — the window normalises [t0, t2]
   *  onto [0,1] and the map folds that into a rise that peaks where t1
   *  lands and a fall that reaches zero at the window's end. Past the
   *  score, `settled` describes it as a plain 0 so the node drops out of
   *  the live set entirely. */
  Animatable<float> flash(float t0, float t1, float t2) const;

  Paint paperGrain;
  Pattern paperSpeck;

  // --- the star field -----------------------------------------------------
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> pool;
  int cellRed = 0, cellBlack = 1, cellWhite = 2, cellOpen = 3, cellBare = 4;

  struct Placed {          // where each star lands once the fold completes
    SkPoint sky{0, 0};     // the equatorial plate carrée
    SkPoint paper{0, 0};   // the scroll
    bool onPaper = false;  // survived the DEC window and the visible segments
    int cell = 3;
    float raNow = 0, decNow = 0;
    float fold0 = 0;  // its own beat in the fold's stagger
  };
  std::vector<Placed> placed;
  float lastEpoch = 1e9f, lastFold = -1;

  // --- the joins, counted -------------------------------------------------
  Catalogue cat;
  Concordance conc;
  int nStars = 0, nAst = 0;
  int nOnDisc = 0, nOnMaps = 0, nInGap = 0, nTooSouth = 0, nUnattested = 0;
  int nSchooled = 0;
  int m5ChenZhuo = 0, m5Sxc = 0, m5Map = 0;
  Departure depMerc{}, depStereo{};

  // asterism → school, resolved from Tables 4 and 5 by Chen Zhuo id
  std::vector<char> astSchool;
  std::vector<int> astMap;   // 5, 13, or 0
  std::vector<float> astRa;  // mean RA at +700, for the reveal stagger

  /** The asterism line art, built ONCE in setup(). describe() runs twice per
   *  loop (the settle flip and the loop reset), so 1,747 precessions and 317
   *  path builds inside it would land whole on those two frames. */
  struct AstArt {
    SkPath local;
    SkRect box;
    float t0;
    SkColor4f ink;
  };
  std::vector<AstArt> astArt;
  /** Map 5's twenty centroids and the 28 determinative RAs, resolved ONCE in
   *  setup(). Both are O(asterisms x vertices), so leaving them inside
   *  describe() would put that whole walk on each of the two re-describes. */
  std::array<SkPoint, 20> m5Cent{};
  std::array<int, 20> m5Region{};
  std::array<float, 28> xiuRa{};

  // =========================================================================
  // GEOMETRY

  /** The equatorial plate carrée the stars arrive in — RA increasing to the
   *  LEFT, exactly as on the scroll, so the fold is a fold and not a flip. */
  static SkPoint skyPoint(float ra, float dec) {
    const float u = wrap360(ra - 296.0f) / 360.0f;
    return {2452.0f - u * 2344.0f, 632.0f - dec * (382.0f / 90.0f)};
  }

  /** Scroll coordinate s (mm, from the atlas's right edge, growing leftward)
   *  to canvas x — through whichever of the two segments holds it. Returns
   *  false when s falls in the omitted stretch or off the plate. */
  static bool scrollX(float s, float& x);
  // the two scroll segments, each its own clipping window: a map cut by the
  // drafting break must be drawn CUT, not skipped
  static float segLo(int seg) { return seg == 0 ? -92.0f : kBreakR; }
  static float segHi(int seg) { return seg == 0 ? kBreakL : kW + 92.0f; }
  static float segX(int seg, float s) {
    return (seg == 0 ? kOriginL : kOriginR) - s * kPxMm;
  }

  /** A star at (ra, dec) of epoch +700 → the paper. Twelve cylindrical maps
   *  for |dec| ≤ 45, the azimuthal disc for dec ≥ +52. Between them is the
   *  chart's own uncovered band, and it is real. */
  static bool paperPoint(float ra, float dec, SkPoint& out, int& region);

  /** The disc's own frame, for the ornament and the mansion spokes. */
  static bool discFrame(SkPoint& c, float& rOuter);

  // =========================================================================
  // THE POOL — rebuilt only when the epoch or the fold actually moved.

  void rebuild(float epoch, float fold);

  // =========================================================================
  // BUILDING

  void computeJoins();

  // --- the paper ----------------------------------------------------------

  Element ground();

  /** The equatorial graticule the stars arrive in, fading as the fold runs. */
  Element graticule();

  /** ONE LEAF FOR 1,460 DOTS. */
  Element starField();

  /** One of the two scroll segments, each its own clipping window: a map cut
   *  by the drafting break must be drawn CUT, not skipped. */
  Element segment(int seg);

  /** The scroll band: the mulberry sheet, the Kraft lining showing at the
   *  edges, the roll's contact replication marks, and two rules of UNEQUAL
   *  weight along the top and bottom — a scroll's edges are not a rect. */
  Element scrollBand(float x0, float x1, const char* keyName, float tilt);

  // --- the map furniture --------------------------------------------------

  /** One hour-angle map: its ruled frame, its RA tick ladder (whose numbers
   *  JUMP BACK 18° at every boundary — the contradiction, drawn), its
   *  equator, and the interrupted mansion rules that fall inside it. */
  Element mapFrame(int k, int seg);

  /** The 50 columns of text, drawn as columns of marks rather than a grid.
   *  Real Tang manuscript columns are UNRULED — the discipline is in the
   *  hand — so what is drawn is the drift, not a lattice. */
  Element columnBand(int k, int seg);

  // --- the circumpolar disc -----------------------------------------------

  Element discPlate(int seg);

  /** The disc's captions, on the paper below it — a note written beside the
   *  drawing, which is where a note goes. */
  Element discNotes(int seg);

  // --- the asterism line art ----------------------------------------------

  /** One asterism, as line art: a polyline through real dot centres, bowed
   *  by shapers::Jitter because a hand-drawn join is not a rule, revealed by
   *  trim() on its own beat. */
  void buildAsterismArt();

  Element asterismLines();

  /** Where an asterism's stars land on the paper, and its own box. */
  bool astCentroid(std::string_view cid, SkPoint& out, int& region) const;

  /** MAP 5'S TWENTY, LABELLED AS THE SCRIBE LABELLED THEM. Two labels are
   *  interchanged, two asterisms carry none at all, and one sits where it
   *  does not belong — all six defects drawn, none corrected, each flagged.
   *  The flags are the audit's, not the chart's: the chart just has them. */
  void buildFixtures();

  Element map5Labels();

  // --- the archer, and the title nobody can read --------------------------

  Element archer();

  /** The closing title. The circulating reading 'Qi jie meng ji dian jing yi
   *  juan' ignores the first character and reads a non-standard third
   *  character as 夢; 蔑 is likelier. The paper's verdict is quoted, and the
   *  graphs are drawn ILLEGIBLE on purpose. */
  Element unreadTitle();

  // =========================================================================
  // THE APPARATUS

  Element locator();

  /** WHERE THE POLE WAS. The paper dates the chart partly from the pole:
   *  "the measured shift in polar distance of the pole reference point
   *  between the S.3326 map and the sky at date +700 is only marginally
   *  significant with a difference of (3.9±2.9)°." This panel draws the
   *  pole's own track, computed from the same IAU 1976 matrix the stars
   *  ride, and marks the four epochs the paper names. */
  Element poleDrift();

  Element poleText();

  /** THE THIRD QUESTION, DRAWN. Twelve maps of 48 deg on a 30 deg pitch: the
   *  frames butt on the paper but their RA windows OVERLAP by 18 deg, so the
   *  scroll's own RA axis jumps BACKWARD at every map boundary. This ruler
   *  is the two readings side by side. */
  Element raRuler(int seg);

  Element breakMark();

  feed::TextOptions logStyle();

  Element projectionPanel();

  /** ONE AUDITED ROW: the index, the name, the school's dot and the name in
   *  the school's own ink, the three censuses side by side, the confidence
   *  index as five cells, and the documented defect where there is one. */
  Element auditRow(int i);

  /** Map 5's twenty asterisms, audited one at a time. Six of them carry a
   *  documented defect and every one is drawn AS FOUND. */
  Element auditPanel();

  /** THE DISC'S OWN ERRATA. Table 5 is 34 asterisms and 142 stars — and its
   *  n(map) column sums to 141. Everything here is quoted, nothing resolved. */
  Element map13Panel();

  Element consolePanel();

  Element ruleNote();

  Element headings();

  // =========================================================================

  Element describe(sketch::SketchContext&);

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  /** Two render()s per loop, at the score's end and at its start. */
  void update(double, sketch::SketchContext& ctx) override;
};
