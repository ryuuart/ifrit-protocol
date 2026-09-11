#pragma once

#include "Settings.h"

struct Fallout2CharSheet : sketch::Sketch {
  using Out = ch::Output<float>;

  // ---- the character. Seven numbers; everything else is computed. --------
  fo::Special narg{{9, 6, 10, 4, 5, 8, 5}};  // includes Gifted's +1
  fo::Traits nargTraits;
  static constexpr int kLevel = 6;
  static constexpr int kToughness = 1;  // one rank, +10% damage resistance
  fo::Derived stats{};
  std::array<int, 18> skillPct{};
  std::array<bool, 18> tagged{};
  std::array<int, 18> invested{};
  int pointsEarned = 0, pointsSpent = 0;
  sigil::measure::Table sheetAudit;

  // ---- interaction state (the only motion this screen has) ---------------
  static constexpr std::array<int, 3> kWalk{0, 7, 4};  // Small Guns, Doctor,
                                                       // Melee Weapons
  static constexpr double kDwell = 1.30;               // seconds per selection
  static constexpr double kSpend0 = 3.40;              // first `+` press
  static constexpr double kSpendGap = 0.60;
  static constexpr double kLoop = 9.0;
  static constexpr double kBlank = 0.123;  // BIG_NUM_ANIMATION_DELAY

  int selected = 4;
  int presses = 0;  // 0..3 skill-point presses on the selected skill
  std::string odoTens = "0", odoOnes = "4";
  Out plusFlash{0}, lampFlash{0};
  double now = 0;

  // ---- generated surfaces, held so their identity prunes ------------------
  Paint plateMat, plateTooth, rustMat, wellMat, parchMat, parchTooth, wheelMat,
      rivetMat, tabMat, canvasGrain;

  // ---- measured advances for the kills leaders (setup-time) ---------------
  struct Kill {
    const char* name;
    int count;
  };
  static const std::array<Kill, 9>& kills();
  std::array<float, 9> killNameW{}, killCountW{};
  float titleAdvance[3] = {0, 0, 0};  // the card title's measured advance

  // paragraph identity for the card body, held so shaping caches stay warm
  std::shared_ptr<weave::Paragraph> cardPara;

  // ---- the measured type, probed once per instance in setup() ------------
  // Each is a property of the faces THIS instance resolved, so it lives on
  // the instance: two live sessions of this sketch measure independently and
  // neither may see the other's numbers. The values below are the fallbacks
  // a probe that measures nothing leaves in place.

  /** Px of advance per em for the body face. */
  float bodyEm = 0.6f;
  /** The scaleX that condenses the card-title face onto its measured
   *  original advance. */
  float titleCondense = 0.6f;
  /** The engraved face's condense for the S.P.E.C.I.A.L. abbreviations, the
   *  top plaques and the folder tabs: "ST-" measures 36 original px of ink
   *  on the capture, so the substitute is squeezed until its own advance
   *  lands there. scaleX is a ratio of the face's proportions, not of a
   *  size, so the one probe serves all three sites' sizes. */
  float engravedCondense = 0.84f;
  /** The distance from a laid-out line's TOP to the top of its capitals, in
   *  ORIGINAL px — Fallout's draw y is the glyph cell's top, SigilWeave's
   *  node top is the line box's top, and the two differ by the ascent slack.
   *  Derived from the substituted face's own metrics. */
  float bodyRise = 2.0f;
  float titleRise = 5.0f;
  /** The engraved face's line-top -> cap-top slack as a FRACTION of its font
   *  size, so one probe serves the five sizes the plaques, tabs, headings,
   *  buttons and S.P.E.C.I.A.L. caps are set in. */
  float engravedRiseFrac = 0.20f;

  /** The body size that lands the substitute on kBodyAdvance original px per
   *  character, given this instance's measured em. */
  float bodySize() const { return fo::n(fo::kBodyAdvance) / bodyEm; }
  /** The engraved slack at @p size. */
  float engravedRise(float size) const { return engravedRiseFrac * size; }

  // =========================================================================
  // Type helpers, once the probes have run.

  /** font 101 at 10 px is a BITMAP face: 1-px stems at 640 wide, which is a
   *  heavy colour. An outline face at the same size reads much lighter, so the
   *  substitute runs BOLD — and because Andale Mono is monospaced, bold has
   *  identical advances and every column stays where the game put it. */
  weave::TextStyle body(SkColor4f c) const {
    return weave::textStyle(
        {.face = fo::bodyBold(), .size = bodySize(), .color = c});
  }
  /** The engraved gold. Sizes are DERIVED from measured ink on the capture:
   *  "ST-" is 36x21 original px, "SKILLS" 51x18, "PRINT" 47x16, "LUKE" 58x20.
   *  Those cap-to-advance ratios (0.40-0.47) are narrower than any stock face,
   *  so the substitute is condensed to land on them. The S.P.E.C.I.A.L.
   *  abbreviations, the top plaques and the folder tabs take
   *  engravedCondense, probed in setup() from "ST-"'s measured ink — the
   *  same discipline the body advance and the card title use; the headings
   *  and buttons state their factors at the call, tuned to their own runs. */
  weave::TextStyle plaqueType(float size, SkColor4f c, float condense = 0.95f,
                              float track = 0.5f) const;
  /** font 102: cap height 17 original px, advance ~6.8 — a heavily condensed
   *  heavy grotesque (measured off the reference: "Strength" is 55 px wide
   *  with 17 px capitals, a 0.40 advance/cap ratio no stock face has). The
   *  condense factor is derived in setup() from the substitute's own advance
   *  rather than guessed. */
  weave::TextStyle titleStyle() const;

  Element bodyAt(const std::string& s, SkColor4f c, float x, float y,
                 float condense = 1.0f) {
    return fo::ink(
        fo::t(s, fo::sheetType(fo::bodyBold(), bodySize(), c, 0, condense)), x,
        y, bodyRise);
  }

  // =========================================================================
  // Chrome. Everything on this screen that is not type is a raster FRM in the
  // game's data; all of it is generated here.

  void buildSurfaces();

  /** A recessed well: the black interior, a hard inner keyline, a warm outer
   *  bevel, and rivets at the corners. shapes::inset is literally "the same
   *  keyline again N px further in", which is this chrome's whole vocabulary.
   */
  /** THE RIVETS ARE THE PLATE'S, NOT EVERY WELL'S. On the capture only the
   *  outer plate carries corner rivets; a well is a recess cut into it and
   *  a recess has no fasteners of its own. Five wells wearing four rivets
   *  each is twenty studs the sheet does not have, and they are the
   *  brightest small marks on it. */
  Element well(fo::Rect r, float radius = 3.0f, bool rivets = false);

  /** A raised plate button — the top plaques, the folder tabs, the bottom
   *  buttons. One two-stroke bevel serves all three. */
  Element raised(fo::Rect r, float radius = 2.0f);

  /** Engraved gold lettering: the bright face over a 1 px shadow stamp. There
   *  is no glyph-level stroke to reach for, so the doubling is echo() — one
   *  misprint stamp UNDER the run, which is exactly the effect. */
  Element engravedText(const std::string& s, float size, SkColor4f c,
                       float condense = 0.95f, float track = 0.5f) {
    return fo::t(s, plaqueType(size, c, condense, track))
        .echo({fo::n(1.0f), fo::n(1.0f)}, fo::kGoldDim);
  }

  Element centred(fo::Rect r, Element child);

  // =========================================================================
  // 1. The S.P.E.C.I.A.L. column

  Element specialColumn();

  /** A two-digit odometer at (x, y), cell 14 x 24. `blankTens`/`blankOnes`
   *  carry the mechanical blank: an empty string draws the sprite sheet's
   *  blank cell for 123 ms and then the new digit — a counter, not a fade. */
  Element odometer(float x, float y, int value, const std::string& tensOverride,
                   const std::string& onesOverride);

  // =========================================================================
  // 2. Status block — label at x=194, value at x=263, pitch 13. The seven
  // condition rows are INACTIVE (#183018) and that dead grey-green covering
  // most of the panel is a big part of how the screen reads.

  Element statusBlock();

  // =========================================================================
  // 3. Derived block — label at x=194, value at x=288, both LEFT-aligned,
  // pitch 13. Ten rows in gCharacterEditorDerivedStatsMap order.

  Element derivedBlock();

  // =========================================================================
  // 4. Level / Exp / Next Level — "%s %d" with the label ending in a colon,
  // pitch 11. The thousands separators come from the game's own _itostndn().

  Element levelBlock();

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
  // Three nodes and two setup-time measure() calls per row: a tab stop's
  // leader is a repeated string, and this gap holds a rule.

  Element folder();

  // =========================================================================
  // 6. The skills column. Eighteen rows, pitch 11: name at x = 380, value at
  // x = 573, "%d%%" LEFT-ALIGNED at 573 — so 6% and 71% start at the same x
  // and the column is ragged on the right. That is authentic; do not
  // right-align it. Tagged skills are #A0A0A0, untagged #3CF800, the selected
  // row #FCFC7C with NO tween — the game has none.

  Element skillsColumn();

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
  // therefore also produces one constant wrap width for every line. The two
  // part company only for an illustration whose leftmost ink is not a single
  // straight column.

  fo::Pose poseFor(int skill) const;

  Element cardContent(int skill);

  Element card();

  // =========================================================================
  // 8. Bezel, plaques, buttons

  Element chrome();

  // =========================================================================

  Element describe();

  /** THE FAILING CLAIMS, PAINTED — and only when there are any. The screen
   *  above is the artefact and carries no drafting chrome, so a sheet whose
   *  arithmetic holds shows the arithmetic and nothing else; a formula that
   *  disagrees with the shipped premades is dealt over the card where
   *  nobody can miss it. */
  Element failureCard() const;

  Element captionBand();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  void update(double elapsed, sketch::SketchContext& ctx) override {
    now = elapsed;
    pushSlots(ctx, false);
  }

  // ---- the three discrete-state slots -------------------------------------
  // Everything continuous is bound; these three change CONTENT, which no
  // Animatable can carry, so they are republished through slots instead.
  int shownSelected = -1, shownPresses = -1;
  std::string shownTens = "?", shownOnes = "?";

  void pushSlots(sketch::SketchContext& ctx, bool force);

  // =========================================================================
  // The frame loop. THE REFERENCE DOES NOT IDLE — the only motion on this
  // screen is interaction, and holding that line is part of the study.

  void step(double);
};
