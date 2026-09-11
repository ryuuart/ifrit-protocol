// rota convocationis: scene assembly and animation.

#include "RotaConvocationis.h"

auto RotaConvocationis::describe() -> Element {
  return stack()
      .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.9f,
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

auto RotaConvocationis::setup(sketch::SketchContext& ctx) -> void {
  ctx.canvas(kW, kH);
  ctx.background(kNight);

  fringeFx = fringeEffect();

  faceRing = sketch::kit::houseFace(sketch::kit::Voice::Interface, 500);
  faceRingBold = sketch::kit::houseFace(sketch::kit::Voice::Interface, 600);
  faceMono = weave::ports::face({"Menlo", "SF Mono", "Courier New"}, 500);

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
  runeSpanS = motion::Spread{.eachMs = 7,
                             .durationMs = 420,
                             .from = motion::Spread::From::Random,
                             .seed = 17}
                  .spanMs((uint32_t)glyphsOf(runeText)) /
              1000.0f;
  {
    motion::Spread form = {.amountMs = 1900};
    form.then({.eachMs = 24, .durationMs = 420});
    nomSpanS = form.spanMs(18, 14) / 1000.0f;
  }
  texSpanS = motion::Spread{.eachMs = 4,
                            .durationMs = 300,
                            .from = motion::Spread::From::Random,
                            .seed = 61}
                 .spanMs((uint32_t)glyphsOf(texText)) /
             1000.0f;
  limenSpanS =
      motion::Spread{.eachMs = 30, .durationMs = 120}.spanMs(15) / 1000.0f;
  sealSpanS = motion::Spread{.eachMs = 30, .durationMs = 480}.spanMs(
                  (uint32_t)glyphsOf(sealText[0])) /
              1000.0f;
  hubSpanS =
      motion::Spread{.eachMs = 240, .durationMs = 760}.spanMs(3) / 1000.0f;
  // THE SHIMMER'S PERIOD is the span the same ladder would take as a
  // one-shot — read off a NON-looping copy, because a looping cascade
  // answers its own period when asked for its span and would only tell
  // this back what it was already given.
  shimmerS = motion::Spread{.eachMs = 26, .durationMs = 620}.spanMs(
                 (uint32_t)glyphsOf(runeText)) /
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

  totalGlyphs = glyphsOf(voxText) + glyphsOf(runeText) + glyphsOf(nomText) +
                glyphsOf(texText) + glyphsOf(hubRuneText) +
                glyphsOf(emblemText) + glyphsOf(spurText);
  for (int k = 0; k < kSeals; ++k)
    totalGlyphs += glyphsOf(sealText[k]) + glyphsOf(kSealTable[k].ordo);
  for (const char* l : kLimina) totalGlyphs += glyphsOf(l);

  bakeGeometry();
  ctx.composer.render(describe());
}

auto RotaConvocationis::update(double elapsed, sketch::SketchContext& ctx)
    -> void {
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

SIGIL_SKETCH(RotaConvocationis, "Study \xc2\xb7 Type",
             "An invented conjuring wheel in the anime idiom \xe2\x80\x94 "
             "twenty-three curved baselines assembling ring by ring, every "
             "start chained from a span")
