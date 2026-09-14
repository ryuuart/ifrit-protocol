#pragma once

#include "Settings.h"

struct Minard1869 {
  // ---- the timeline: ONE Output, every beat a window onto it.
  // NOT from() — ease:: is not total, so a value outside a beat's window
  // would feed the curve outside its domain. bind().window(a, b) clamps
  // instead, which is what lets one clock drive every beat on the plate.
  ch::Output<float> T{0};
  ch::Output<float> mmScale{kMmPer10k};  // the 12.6% morph
  ch::Output<float> dimAmt{0};
  ch::Output<float> calAlpha{0};
  /** The caliper's discrete steps: an instrument walks in clicks, so its
   *  travel rides ticker.addFixed rather than the frame rate, and the
   *  reading it shows is TEXT — which no binding can carry, so it goes
   *  through slot()/renderSlot(). Four spot measurements, taken by hand on
   *  the BnF sheet with no cross-scan calibration at all. */
  int calStep = 0, calShown = -1;

  sk_sp<SkTypeface> faceScript, faceItalic, faceRoman, faceNum, faceUi,
      faceUiBold, faceMono;
  sigil::weave::FontContext* fonts = nullptr;  // the composer's, held for
                                               // metrics()/measureRun()

  Pattern paperPulp, laidLines, chainLines, foxing, tintSpeckle;
  Paint paperMat, vignette;

  // audits, computed once in setup()
  test::WidthAlong auditAdvance, auditRetreat;
  float advanceInk = 0, advanceArea = 0;
  float retreatArea = 0;
  float coverDoubled = 0;
  // the audit's own input, measured: what resolving the band's overlapping
  // steps into one outline leaves for a raycast to cross
  int outlineContours = 0, advSteps = 0;
  float outlineWalk = 0, advPerimeter = 0;
  std::string worstCorner;
  size_t advComponentsDrawn = 0, advComponentsWilkinson = 0, retComponents = 0;
  float riserArcErr = 0;   // arc-length indexed  — the right way
  float riserFracErr = 0;  // fraction indexed    — the trap
  std::string riserWorstCity;

  feed::TextRing colA{200}, colB{200}, colC{200}, colD{200}, colE{200};
  /** The look every kit component below this sketch is set in; built in
   *  setup, once the faces it names have been resolved. */
  sketch::kit::Theme sheetLook;
  /** THE AUDIT'S OWN LOOK, and it is deliberately another world: clean
   *  paper, crisp rules, the interface face, and the blue a measurement
   *  is printed in as the figure colour. Bound over the five cards. */
  sketch::kit::Theme cardLook;
  /** The sheet a card states: the card theme's classes, and the ones a
   *  card adds — `measured`, `amber`, `amberInk`, `grey`, `route`,
   *  `vector`, `cross`, `cardInk`, `claim`, `pass` — so a rule, a bar, a
   *  dot and the word that names it take one colour from one entry. */
  weave::StyleSheet cardSheet;

  // -----------------------------------------------------------------------
  // beat clock (seconds). Everything reads through bind(&T).window(a,b).
  static constexpr float tPaper = 0.0f, tPaperEnd = 1.2f;
  static constexpr float tHann = 1.2f, tHannEnd = 3.0f;
  static constexpr float tLegend = 3.0f, tLegendEnd = 4.2f;
  static constexpr float tAdv = 4.2f, tAdvEnd = 6.4f;
  static constexpr float tRet = 6.4f, tRetEnd = 8.4f;
  static constexpr float tTemp = 8.4f, tTempEnd = 10.2f;
  static constexpr float tScale = 10.2f, tScaleEnd = 13.0f;
  static constexpr float tMorph = 13.0f, tMorphEnd = 15.0f;
  static constexpr float tLigne = 15.0f, tLigneEnd = 16.4f;
  static constexpr float tGeo = 16.4f, tGeoEnd = 19.0f;
  static constexpr float tDistort = 19.0f, tDistortEnd = 21.0f;
  static constexpr float tBar = 21.0f, tBarEnd = 23.0f;
  static constexpr float tReaumur = 23.0f, tReaumurEnd = 25.0f;
  static constexpr float tTwo = 25.0f, tTwoEnd = 27.5f;
  static constexpr float tLoop = 30.0f;

  Animatable<float> beat(float a, float b) const {
    return bind(&T).window(a, b).clamp(0.0f, 1.0f);
  }

  // =======================================================================
  // THE SHEET

  /** THE DOCUMENT, or a null value where the file did not load: every
   *  reader below goes through this, so a plate without its content file
   *  draws without its lettering rather than not at all. */
  const data::Json& doc() const;

  /** ONE OF THE PLATE'S FIVE HANDS by the name the document calls it:
   *  the interface face and its bold, the fine sloped italic the place
   *  names are set in, the upright condensed face the numbers are, and
   *  the spaced roman MOSCOU alone takes. An unknown name inherits the
   *  engraver's script, which is what the sheet's root states. */
  sk_sp<SkTypeface> hand(const std::string& named) const;

  /** ONE LINE OF THE PLATE'S LETTERING as the document writes it: the
   *  words, where they stand, the register they are set in, and the beat
   *  they enter on. Everything on the sheet that is a line of type goes
   *  through here, so the document is the content and this is the
   *  template. */
  Element engraved(const Lettering& line) const;

  /** A WHOLE RUN OF LETTERING out of a record of the document, one
   *  element per entry, every beat offset by @p t0 and every baseline by
   *  @p dy — which is what lets a panel's or a card's lines be written in
   *  its own coordinates. */
  std::vector<Element> lettering(const data::Json& run, float dy,
                                 float t0) const;

  /** ONE STRING out of the document, empty where the key is not there. */
  std::string word(const char* panel, const char* named) const;

  /** AN ENGRAVED LINE: a cooked path stroked over the whole sheet and
   *  drawn on as the beat runs ALONG it. Every rule, coast, river,
   *  dropline and graduation on the plate is one of these, so none of
   *  them restates the box it is drawn in or how a line arrives. */
  template <class Nib>
  Element inked(SkPath path, const Nib& nib, float t0, float t1) const {
    return box()
        .inset(0)
        .shape(pathFn(std::move(path)))
        .stroke(spans::upTo(beat(t0, t1)), nib);
  }
  /** The same line, already printed: what the plate carries before the
   *  beat it is read at reaches it. */
  template <class Nib>
  Element inked(SkPath path, const Nib& nib) const {
    return box().inset(0).shape(pathFn(std::move(path))).stroke(nib);
  }

  Element paperGround();

  Element frames();

  /** Minard's own hand across the top margin, and the two donation
   *  stamps. This is the part of the object that makes it HIS copy. */
  Element provenance();

  /** A COAST OR A RIVER: a smooth path the placed points steer, stroked
   *  along as the beat runs. */
  Element river(const std::vector<SkPoint>& pts, float width, SkColor4f colour,
                const char* key, float t0);

  // =======================================================================
  // HANNIBAL — the panel nobody has seen

  /** The Mediterranean, as an engraved coast: the outline, then
   *  coast-parallel hatching whose spacing GROWS away from the shore,
   *  which is geometry::path::parallel called once per ring. */
  Element hannibalSea();

  /** Lehmann hachures: strokes down the line of steepest descent, drawn
   *  with the pen over a synthetic height field of gaussian ridges. */
  Element lehmann(const std::vector<std::array<float, 4>>& ridges, float x0,
                  float y0, float x1, float y1, const char* key, float t0);

  Element hannibalPanel();

  Element legendBox();

  // =======================================================================
  // NAPOLEON

  /** The brush every zone on both panels is painted with: the strength law
   *  on the width Profile seam, bevelled on the outside of each turn — the
   *  chord a lithographer's overlapping treads leave. The audit reads the
   *  same value, so what it measures is what the sheet shows. */
  brush::Ribbon flowRibbon(const WidthProfile& prof, SkColor4f colour);

  /** One band, on a node sized to the ROUTE's bounding box so that the
   *  profile's `max()` is actually load bearing (the band overflows that
   *  box by up to w/2 on each side). */
  Element bandElement(const SkPath& spine, const WidthProfile& prof,
                      SkColor4f colour, const std::string& key,
                      Animatable<float> reveal);

  /** A strength written ACROSS its zone — Minard's "écrits en travers des
   *  zônes", set in the French convention with a full stop for thousands
   *  (422.000, never 422,000). The baseline is a segment along the band's
   *  NORMAL, so TextPath::Orient::Tangent puts the type across the band.
   *  The segment reaches past the band's half-width — or past the run's
   *  own half-length, whichever is longer, because glyphs past a path's
   *  end are DROPPED, so a thin band under a long number needs the run to
   *  set the floor — by the face's cap slack at this size, so the
   *  clearance scales with the type instead of sitting at a guessed
   *  constant. */
  Element bandNumber(SkPoint at, SkVector tangent, float men, float size,
                     const std::string& key, float t0);

  Element advanceZones();

  Element napoleonPanel();

  /** A graduated bar: the rule, the ticks the plate numbers, and the unit
   *  it is measured in. `pxPerUnit` is px per lieue, `span` the last
   *  label, `step` the label interval. */
  Element scaleBar(float x, float y, float pxPerUnit, int span, int step,
                   const Utf8& label, const char* key, float t0);

  // =======================================================================
  // THE TEMPERATURE PANEL — and the nine droplines that ARE the joint
  // between the two panels.

  float tempY(float reaumur) const {
    return kDivNT + 26.0f + (-reaumur) / 30.0f * 92.0f;
  }

  Element temperaturePanel();

  Element imprints();

  struct SpotRead {
    float x, y, halfPx, mm, men;
    const char* where;
  };
  SpotRead spot(int i) const;

  /** The caliper: the only saturated object allowed on the plate, drawn
   *  OVER it with a small shadow so it reads as an instrument laid on
   *  paper rather than as ink. */
  Element caliper();

  Element sheet();

  // =======================================================================
  // THE AUDIT — five cards, a different world: clean paper, crisp rules,
  // no grain.

  /** ONE AUDIT CARD: the plate, its title and the rule under it are
   *  `compose::kit::panel`'s head and well, the body stands in that
   *  well's own coordinates, and the card's remarks are the run of
   *  lettering its own record carries. */
  Element card(const data::Json& spec, float t0, Element body);

  /** Card 1 — DOES THE PLATE OBEY ITS OWN LEGEND?  The eleven measured
   *  treads, the fit through them, and the two rules that matter. */
  Element cardScale(const data::Json& said);

  /** Card 2 — THE FLOOR. And the negative result: at the floor, the
   *  12,000 -> 14,000 anomaly is invisible in the ink. */
  Element cardFloor(const data::Json& said);

  /** Card 3 — THE MAP IS A REAL MAP. The received account is wrong. */
  Element cardGeo(const data::Json& said);

  /** Card 4 — WHAT HE DID DISTORT. Ten leg ratios against 1.00. */
  Element cardLegs(const data::Json& said);

  /** Card 5 — RÉAUMUR. °C = °R × 5/4 and °F = °R × 9/4 + 32, both exact,
   *  and the axis relabels itself live. */
  Element cardReaumur(const data::Json& said);

  Element auditColumn();

  // =======================================================================
  // THE TITLE STRIP and THE CONSOLE

  /** THE MASTHEAD: the plate's own title over its provenance, with the two
   *  remarks the sheet is read under ranged at the far edge — the scale it
   *  is drawn at, and the claim this sketch makes about it. The registers
   *  and the air between them are the sheet's theme's. */
  Element titleStrip();

  Element consoleStrip();

  // =======================================================================

  Element describe();

  // =======================================================================
  // THE PROOF — every number computed here, none copied.

  void runAudits(const sketch::SketchContext& ctx);

  // =======================================================================

  Sheet plate;

  void setup(sketch::SketchContext& ctx);

  /** The one thing on the sheet that cannot be a bound value: the caliper's
   *  reading is TYPE, and a string is not a property a binding can drive,
   *  so a new step re-describes. renderSlot() keeps that to one node. */
  void update(double, sketch::SketchContext& ctx);
};
