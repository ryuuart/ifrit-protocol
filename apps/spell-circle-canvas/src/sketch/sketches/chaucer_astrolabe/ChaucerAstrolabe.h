#pragma once

#include "Settings.h"

struct ChaucerAstrolabe : sketch::Sketch {
  // ONE Output for the rete's rotation, remapped at every call site with
  // bind(): the rete's rotate(), the label's angle, the sun mark's position,
  // the readouts and the highlighted letter are all the same number in
  // different units, which is exactly what bind() is for.
  ch::Output<float> reteRot{0};
  ch::Output<float> hourAngle{-46.550124f};
  ch::Output<float> sunLam{1.0f};
  ch::Output<float> sunAlt{25.5f};
  ch::Output<float> projDec{0};
  ch::Output<float> trace{0};  // the construction overlay's opacity
  std::array<ch::Output<float>, 24> letterGlow;
  ch::Output<float> signMark{0};

  double now = 0;
  int nightHour = 0;
  float latHours = 8.8967f;

  Rete rete;
  std::vector<Piece> pieces;
  std::shared_ptr<instancing::Atlas> tickAtlas;
  std::shared_ptr<instancing::Pool> tickPool;
  Paint brassGrain, vellumGrain;
  Pattern verdigris;
  sk_sp<SkTypeface> faceEngrave, faceLimb, faceSerif, faceItalic, faceMono,
      faceBold;

  feed::TextRing logA{64}, logB{64}, logC{64}, logD{64};
  /** The look every kit component below this sketch is set in; built in
   *  setup, once the faces it names have been resolved. */
  sketch::kit::Theme sheetLook;
  std::string chaucerH, chaucerA, chaucerDelta;

  // =========================================================================
  // LATTEN — one sheet of brass, one light, two hundred nodes
  //
  // The instrument is a single piece of metal, and every face on it differs
  // only in HOW LIT it is. So there is one recipe, `kit::latten` — a
  // shadow/body/light ladder with a sheen laid along a run and the tooling's
  // own grain over it — and a `level` per face saying where on that ladder
  // the face sits.
  //
  // THE RUN IS STATED IN THE CANVAS'S COORDINATES and the paint is anchored
  // to the root, so the light crosses two hundred nodes without any of them
  // converting it into its own box, and a node that TURNS keeps the light
  // where it was rather than carrying it round. The bank folds the levels
  // into kLevels buckets, so the whole instrument costs at most that many
  // materials however many faces are cut into it.
  static constexpr int kLevels = 48;
  mat::Bank latten{kLevels};

  /** The ladder and the light, with the level left at the bottom: the bank
   *  keys on these bytes, so the run, the tones and the grain are the
   *  species and the level alone is the bucket. */
  static matkit::LattenParameters sheet();

  /** The sheet at @p level, banked. */
  Paint brass(float level = 0.5f);

  /** The sheen as a node-local gradient, in the node's own px. The two
   *  consumers left cannot take the material — a `Ribbon`'s fill and a
   *  `PathFormat`'s `strokeFill` both take a `Fill` — so the sheet's own
   *  ladder is read at the two ends of the run and its middle, exactly
   *  where that is unavoidable and nowhere else. */
  Fill brassStroke(SkRect r, float level = 0.5f) const;

  // =========================================================================
  // THE PLATE — "compowned after the latitude of Oxenford"

  Element plate();

  // =========================================================================
  // THE CONSTRUCTION OVERLAY — the only saturated thing on the object, and
  // it reads as a line drawn ON the brass, never engraved into it.

  Element construction();

  // =========================================================================
  // The rete is a PHYSICALLY RAISED, PIERCED SHEET, so it casts onto the
  // plate — and the plate's engraving is visibly darker and lower through
  // its cutouts. This is the most convincing single cue that the thing is an
  // object and not a drawing, and it costs one duplicate geometry pass: the
  // same skeleton, flat dark, in a wrapper that rotates WITH the rete and is
  // then displaced in the parent's frame (a shadow offset must not turn with
  // the object).

  Element reteShadow();

  // =========================================================================
  // THE RETE — "shapen in maner of a net" (I.21), a single pierced sheet

  Element reteGroup();

  // =========================================================================
  // THE LIMB — degrees outside, the 24 hour letters inside
  //
  // Chaucer I.7–8 puts the 90°-per-quadrant altitude scale on the BACK; II.3
  // has him take the altitude there, turn the instrument over, set the rete,
  // lay the label and read the letter "in the bordure". That sequence puts
  // the letters on the FRONT limb. I have not seen a photograph of the 1326
  // instrument's front limb to confirm it. Both rings are drawn.

  Element limb();

  // =========================================================================
  // the label (a thin rule from the pin to the limb), the sun's mark, the pin

  Element rule();

  /** The sun's mark is positioned every frame from the same Output the rete
   *  turns on, so it stays welded to the ecliptic. Rendered through a slot
   *  because its POSITION is data, not a transform of a fixed point. */
  Element sunMark();

  // =========================================================================
  // THE PROJECTION — the panel that explains the whole instrument

  Element projectionPanel();

  /** The live half of the projection panel: a point P walking the meridian
   *  from +ε to −ε, the ray from the south pole through it, and where that
   *  ray lands on the plane. r = R_eq·tan((90−δ)/2), and the landing radius
   *  sweeps R_can → R_eq → R_cap as it goes. */
  Element projRay();

  Element projRead();

  // =========================================================================
  // panels

  /** A VELLUM CARD: `kit::sheet`'s header — the title, the gloss under it
   *  and the rule under both — on a bordered ground, laid at (x, y, w, h).
   *  The header FLOWS, so a two-line gloss pushes its own rule down instead
   *  of running through a rule ruled at a fixed distance.
   *
   *  The container spans the WHOLE canvas so that everything in this sketch
   *  — instrument, panels, overlays — is authored in one coordinate frame;
   *  the card itself is just the first child. */
  Element panel(float x, float y, float w, float h, const char* title,
                const char* sub);

  Element familiesPanel();

  // --- the back: where Chaucer's example starts -----------------------------
  Element backPanel();

  // --- specification card ---------------------------------------------------
  Element specCard();

  // --- the star table -------------------------------------------------------
  Element starPanel();

  // --- Chaucer's worked example --------------------------------------------
  Element chaucerPanel();

  Element chaucerBody();

  // --- the zodiac is not uniform -------------------------------------------
  Element zodiacPanel();

  // --- the feed: the checks, printed as they run ----------------------------
  feed::TextOptions logStyle();

  Element consolePanel();

  // --- the title strip ------------------------------------------------------
  /** THE MASTHEAD: what the instrument is over where it was compowned,
   *  with its museum provenance ranged at the far edge and the engraved
   *  hairline under the block. The registers, the inks and the air between
   *  them are the plate's theme's. */
  Element titleStrip();

  // --- the live readout laid over the case ---------------------------------
  Element readout();

  // =========================================================================

  Element describe(sketch::SketchContext&);

  // =========================================================================
  // THE VERIFICATION — run at setup, printed into the feed, and a failure
  // is visible on screen rather than swallowed.

  /** THE VERIFICATION. Every row below is a `measure::Check`: its printed
   *  line is COMPUTED from the two values it reports, so a claim and its
   *  evidence cannot drift apart the way a hand-typed "PASS" can. A row
   *  that is a measurement rather than a claim is a `reading`, and the one
   *  row that is a statement about the SUBJECT rather than about this
   *  construction — the published azimuth parameterisation, which does not
   *  hold — is a `finding`, printed with a verdict and never counted
   *  against the run. */
  void verify();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  void update(double, sketch::SketchContext& ctx) override;
};
