#pragma once

#include "Settings.h"

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
  /** The fringe shader, compiled once per declaration and held here. */
  sk_sp<SkRuntimeEffect> fringeFx;

  sk_sp<SkTypeface> faceRing, faceRingBold, faceMono;

  // Geometry baked once, before the first describe, and never moved: the
  // chalk's wobble, each lighting group's emissive stack, and the star
  // compound's morph ladder.
  std::vector<SkPath> chalk;
  std::vector<Glow> glows;
  std::vector<SkPath> starSteps;
  SkPath cageLadder, cageBeads, cageFenceOut, cageFenceIn, hubMotes;
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
  [[nodiscard]] motion::Animatable<float> beat(double from, double to) {
    return motion::bind(&cycle).window((float)from, (float)to);
  }
  /** A one-shot swell inside the loop — up, held, gone — for the flashes
   *  ignition throws on things that are not text. */
  [[nodiscard]] motion::Animatable<float> pulse(double from, double to,
                                                double edge);
  /** The sheet's own envelope: up at the head, out before the wrap, so
   *  the loop cuts on a dark sheet. */
  [[nodiscard]] motion::Animatable<float> envelope();

  // ------------------------------------------------------------------
  // type

  [[nodiscard]] sigil::weave::TextStyle ring(float size, SkColor4f color,
                                             float track = 3.0f) const {
    return weave::textStyle(
        {.face = faceRingBold, .size = size, .color = color, .track = track});
  }
  /** THE REGISTER'S TYPE takes no face. The invented alphabet is not in
   *  the interface family, and asking for it by name would be asking for
   *  a font this study does not ship: the shaper's own fallback finds the
   *  letterforms, which is the mechanism that puts them on the plate. */
  [[nodiscard]] static sigil::weave::TextStyle rune(float size, SkColor4f color,
                                                    float track) {
    return weave::textStyle({.size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle mono(float size, SkColor4f color,
                                             float track = 1.0f) const {
    return weave::textStyle(
        {.face = faceMono, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle label(float size, SkColor4f color,
                                              float track = 2.6f) const {
    return weave::textStyle(
        {.face = faceRing, .size = size, .color = color, .track = track});
  }

  /** The size at which @p s girds a circle of radius @p radius: measured
   *  straight, refined once because tracking is px and does not scale
   *  with the type. */
  [[nodiscard]] float fitToRing(sketch::SketchContext& ctx,
                                const std::string& s,
                                const sigil::weave::TextStyle& style,
                                float radius, float fill = 0.985f);

  // ------------------------------------------------------------------
  // the drawn lines

  /** A stroked line of the figure, revealed as an arc sweep — the compass
   *  stroke itself. The outline is the baked chalk path, so what the
   *  sweep lays down carries the hand's wobble; the box is the whole panel
   *  because the path is in the panel's own px. */
  [[nodiscard]] Element rule(const char* key, int chalkIndex, float width,
                             SkColor4f color, double from, double dur);

  /** The same, for a path this sketch holds directly rather than in the
   *  chalk table — the spoke sets and the node rings, which are perfect
   *  by construction because a node is a bead and not a rule. */
  [[nodiscard]] Element line(const char* key, const SkPath& path, float width,
                             SkColor4f color, double from, double dur);

  /** THE IGNITED LINE — white-hot core, saturated halo, two grades of
   *  bloom. The value hierarchy is the drawing: the core carries the
   *  shape, the halo carries the hue, the bloom carries the reach. */
  [[nodiscard]] Element emissive(const std::string& key, const Glow& g,
                                 const ch::Output<float>* gain);

  /** A DIVISION LADDER at one length class. `skipEvery` leaves a hole
   *  where a heavier class stands, because two passes stamping the same
   *  mark print it darker than either weight meant to. */
  [[nodiscard]] Element ladder(const char* key, int divisions, int skipEvery,
                               float outer, float inner, float width,
                               SkColor4f color, double from, double dur,
                               float fromDeg = 0.0f);

  // ------------------------------------------------------------------
  // the bands

  /** The invocation's cascade: one cue per word — writing pace, with the
   *  pen resting at every cross — and the letters of each word beating
   *  inside its cue. Built as a named value because `then` mutates in
   *  place. */
  [[nodiscard]] motion::Spread voxCascade() const {
    motion::Spread cascade = motion::Spread{}.cues(voxCues);
    cascade.then({.eachMs = 22, .durationMs = 300});
    return cascade;
  }

  /** THE INVOCATION — Latin majuscule between two rules, the one band on
   *  the plate that is meant to be read. Its entrance composes with the
   *  baseline: the hold vetoes a glyph until its beat, the rise then lifts
   *  it onto the ring along the ring's own local perpendicular, and the
   *  tint warms it from ember to bone as it lands. After ignition the
   *  whole run becomes a slow marquee. */
  [[nodiscard]] Element invocatio();

  /** THE SHIMMER — a crest of light that re-opens on every glyph of the
   *  register forever. `Spread::loopMs` folds each glyph's beat onto its
   *  own cycle of one period, phase-offset by the glyph's start, so one
   *  sweep of the master IS one cycle and a wrapping phase drives it
   *  seamlessly with no seam to hide. It is the reading that the circle
   *  is not a finished picture: something is still running round it. */
  [[nodiscard]] motion::Spread shimmerCascade() const {
    motion::Spread s = {.eachMs = 26, .durationMs = 620};
    s.loopMs = (uint32_t)(shimmerS * 1000.0f);
    return s;
  }

  /** THE REGISTER — the outer rune band, the widest ring of script on the
   *  plate and the one that says what idiom this is. It forms as a
   *  scatter (the letters do not arrive in reading order, because nobody
   *  reads them), then never settles. */
  [[nodiscard]] Element registrum();

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
  [[nodiscard]] Element nomina();

  /** THE TEXTURE — the small register, tracked shut and set at a size
   *  that stops it being letters and makes it grain. It is what makes the disc
   * continuous between the arcs and the stars instead of leaving a dark annulus
   *  there, and it is the band the whole idiom depends on: the plate
   *  looks like a mechanism because the innermost script is too small to
   *  resolve. Every glyph is still a real glyph; a baked tile would be
   *  cheaper and would be a lie about what this plate is. */
  [[nodiscard]] Element textura();

  // ------------------------------------------------------------------
  // the turning layers

  /** THE ARC LAYER: two rings of broken arcs half a pitch apart, the
   *  spokes that tie them, the nodes that stand between them and a dashed
   *  ring outside the pair — one layer, so one baked union, so one
   *  rotation, and the whole assembly sweeps past the bands either side of
   *  it as a mechanism rather than as a picture being spun. */
  [[nodiscard]] Element arcus();

  /** THE OFF-ORDER MARK — one medallion straddling the outermost rule at
   *  twelve o'clock, carrying a single letterform. It obeys none of the
   *  plate's symmetries and interrupts two bands to exist, which is
   *  exactly its job: the eye takes a perfectly regular figure for a
   *  pattern and a figure with one exception for a drawing. */
  [[nodiscard]] Element spur();

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
  [[nodiscard]] Element stella();

  /** THE INNER FIGURE: the {12/4} compound — four triangles — nested so
   *  that its vertices sit just inside the circle the squares above are
   *  tangent to, and its own chords run tangent to the hub's rule. It
   *  turns against the compound outside it, which is the whole reason
   *  there are two. */
  [[nodiscard]] Element stellaInterior();

  /** THE CAPTIONS on the outer compound's chords. They ride a SECOND path
   *  — the same twelve chords as OPEN contours — so chord k's midpoint is
   *  at exactly (k+0.5)/12 of one arc-length coordinate, and each caption
   *  is a leaf addressed by fraction alone. Only six of the twelve chords
   *  are written; the other six carry the ladder's silence, which is the
   *  asymmetry the idiom allows itself. They do not turn with the figure,
   *  and they do not travel with the rim: a caption is read, and the
   *  mechanism is what moves. */
  [[nodiscard]] Element limina();

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
  [[nodiscard]] Element sigillum(int k);

  // ------------------------------------------------------------------
  // the emblem

  /** THE EMBLEM'S DISC of light — the SDF's one pass carrying fill and
   *  glow together, sized by the reserve the style declares so the box
   *  cannot crop its own falloff. */
  [[nodiscard]] Element emblemDisc();

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
  [[nodiscard]] Element emblema();

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
  [[nodiscard]] Element monogramma();

  // ------------------------------------------------------------------

  [[nodiscard]] Element wheel();

  /** THE COLOPHON — two lines under the figure and nothing else. A circle
   *  of this kind is the whole picture; a panel of commentary beside it
   *  would be a diagram of a spell circle rather than one. */
  [[nodiscard]] Element colophon();

  [[nodiscard]] Element describe();

  // ------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override;

  // ------------------------------------------------------------------
  /** Everything the figure draws that is a PATH rather than a box, cooked
   *  once before the first describe and never touched again: the chalk's
   *  wobble, each lighting group's unioned emissive stack, and the outer
   *  compound's morph ladder. All of it is content-static, which is what
   *  entitles the nodes that wear it to record once and replay under
   *  bound transforms and bound gains for the rest of the loop. */
  void bakeGeometry();

  /** THE STRIKE, and what it leaves burning. Three readings compose into
   *  one scalar per lighting group, which is why they are stepped here
   *  rather than declared as a window each: the cross-fade that lands the
   *  light on a struck rule, the flash the strike itself throws — up in
   *  two frames, down over half a second, the shape a hot thing has and a
   *  fade does not — and, once the circle is charged, the breath that
   *  keeps it from ever reading as a finished picture again. */
  float strike(double tc, double at, float rest) const;

  /** The ignition's own envelope: a fast rise to the crest and a long
   *  decay, so what follows the crest is an AFTERGLOW and not a cut.
   *  `motion::flash` is that shape at its origin — a linear rise to
   *  exactly 1 at the crest and an exponential fall from it. */
  static float burst(double tc, double at, double rise, double fall) {
    return motion::flash((float)(tc - at), (float)rise, (float)fall);
  }

  /** Every gain the fire drives, plus the ember pool. */
  void stepFire(double tc);

  /** THE EMBERS: seeded on the rim, thrown up at ignition, then a sparse
   *  drizzle for as long as the circle is charged. Each spark is a pure
   *  function of its index and the time since ignition, so the pool needs
   *  no per-frame state and the loop's wrap re-deals it exactly. */
  void stepEmbers(double tc);

  void update(double elapsed, sketch::SketchContext& ctx) override;
};
