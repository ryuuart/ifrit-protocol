#include "RotaConvocationis.h"

auto RotaConvocationis::pulse(double from, double to, double edge)
    -> motion::Animatable<float> {
  const float l = (float)loopSecs;
  return motion::bind(&cycle)
      .source(0.0f, l)
      .trapezoid((float)from / l, (float)(from + edge) / l,
                 (float)(to - edge * 1.6) / l, (float)to / l)
      .map(&ch::easeInOutQuad);
}

auto RotaConvocationis::envelope() -> motion::Animatable<float> {
  const float l = (float)loopSecs;
  return motion::bind(&cycle)
      .source(0.0f, l)
      .trapezoid(0.05f / l, 0.55f / l, (l - 1.9f) / l, (l - 0.55f) / l)
      .map(&ch::easeInOutQuad);
}

auto RotaConvocationis::fitToRing(sketch::SketchContext& ctx,
                                  const std::string& s,
                                  const sigil::weave::TextStyle& style,
                                  float radius, float fill) -> float {
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

auto RotaConvocationis::rule(const char* key, int chalkIndex, float width,
                             SkColor4f color, double from, double dur)
    -> Element {
  return box()
      .key(key)
      .absolute()
      .inset(0)
      .hitTestable(false)
      .shape(heldPath(chalk[(size_t)chalkIndex]))
      .fill(Fill::none())
      .stroke(spans::upTo(beat(from, from + dur)),
              stroke(width, Fill::color(color)));
}

auto RotaConvocationis::line(const char* key, const SkPath& path, float width,
                             SkColor4f color, double from, double dur)
    -> Element {
  return box()
      .key(key)
      .absolute()
      .inset(0)
      .hitTestable(false)
      .shape(heldPath(path))
      .fill(Fill::none())
      .stroke(spans::upTo(beat(from, from + dur)),
              stroke(width, Fill::color(color)));
}

auto RotaConvocationis::emissive(const std::string& key, const Glow& g,
                                 const ch::Output<float>* gain) -> Element {
  // ONE BAKE FOR THE WHOLE STACK, and the gain rides its blit. Four
  // grades painted live are four additive fills of four complex unions
  // over the sheet, every frame, for a stack whose shape never changes —
  // the gain is the only thing that moves, and it moves OUTSIDE the
  // pixels. Baked once at full strength and blitted at the gain, the
  // stack costs one image add. Each grade is bounded by its own region
  // too, so the bake is the light's own box and not the sheet's.
  //
  // THE ONE THING THAT MOVES WITH IT: the grades add inside the bake, so
  // where all four overlap they saturate there rather than against the
  // sheet, and a gain part way up prints that saturated sum scaled down
  // instead of four scaled terms summed. The difference is confined to
  // the hottest core of a group while its gain is between 0 and 1 — the
  // grades are nested, so the outer three never reach it — and it reads
  // as the core coming up a shade later, which is what a filament does.
  const SkRect groupBox = g.bloom.box;
  const auto inside = [&](const std::string& name, const Grade& r,
                          SkColor4f ink, float alpha) {
    return box()
        .key(name)
        .absolute()
        .rect(SkRect::MakeXYWH(r.box.left() - groupBox.left(),
                               r.box.top() - groupBox.top(), r.box.width(),
                               r.box.height()))
        .hitTestable(false)
        .shape(heldPath(r.local))
        .fill(Fill::color({ink.fR, ink.fG, ink.fB, alpha}))
        .blend(SkBlendMode::kPlus);
  };
  return box()
      .key(key)
      .absolute()
      .rect(groupBox)
      .hitTestable(false)
      .cache(Cache::Texture)
      .blend(SkBlendMode::kPlus)
      .opacity(gain)
      .child(inside(key + "-bloom", g.bloom, kBloom, 0.085f))
      .child(inside(key + "-mid", g.mid, kBloom, 0.16f))
      .child(inside(key + "-halo", g.halo, kHalo, 0.42f))
      .child(inside(key + "-core", g.core, kCore, 0.96f));
}

auto RotaConvocationis::ladder(const char* key, int divisions, int skipEvery,
                               float outer, float inner, float width,
                               SkColor4f color, double from, double dur,
                               float fromDeg) -> Element {
  shapes::Ticks t{.divisions = divisions,
                  .from = fromDeg,
                  .mark = {inner / outer, 1.0f},
                  .longEvery = skipEvery,
                  .longMark = {1.0f, 1.0f}};
  return box()
      .key(key)
      .absolute()
      .inset((1.0f - outer) * kR + (kEye.x() - kR))
      .hitTestable(false)
      .shape(shapes::ticks(t))
      .fill(Fill::none())
      .stroke(spans::upTo(beat(from, from + dur)),
              stroke(width, Fill::color(color)));
}

auto RotaConvocationis::invocatio() -> Element {
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
           .unit = weave::Unit::Word,
           .innerUnit = weave::Unit::Cluster,
           .progress = beat(tVox, tVox + voxSpanS)})
      .fx({.effect = fx::tint(kEmber, kBone),
           .stagger = voxCascade(),
           .unit = weave::Unit::Word,
           .innerUnit = weave::Unit::Cluster,
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
           .unit = weave::Unit::Word,
           .innerUnit = weave::Unit::Cluster,
           .progress = beat(tVox, tVox + voxSpanS)});
}

auto RotaConvocationis::registrum() -> Element {
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
           .stagger = {.eachMs = 7,
                       .durationMs = 420,
                       .from = motion::Spread::From::Random,
                       .seed = 17},
           .progress = beat(tRune, tRune + runeSpanS)})
      .fx({.effect = fx::keys(
               {{0.00f, {}}, {0.35f, {.colorScreen = kHalo}}, {1.00f, {}}}),
           .stagger = shimmerCascade(),
           .progress = &runePhase});
}

auto RotaConvocationis::nomina() -> Element {
  motion::Spread form = {.amountMs = 1900};
  form.then({.eachMs = 24, .durationMs = 420});

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
          // TURNED AS A BODY, not advanced along its path — the same
          // rule the seals are built on, and for the same reason: a
          // circular baseline rotated about its own centre is the same
          // picture as the run advanced along it, so the ring is
          // recorded once and blitted under a bound rotation where a
          // driven phase re-places every glyph, re-runs the glow and
          // re-rasterizes the whole band on every frame.
          .rotate(motion::bind(&nomDrift).target(0.0f, 360.0f))
          .cache(Cache::Texture)
          .onPath({.path = shapes::circle(),
                   .at = 0.0f,
                   .align = TextPath::Align::Start,
                   .offset = -nomSize * 0.34f,
                   .autoFlip = false})
          .fx({.effect = fx::hold(fx::rise(nomSize * 0.8f)),
               .stagger = form,
               .unit = weave::Unit::Word,
               .innerUnit = weave::Unit::Cluster,
               .progress = beat(tNames, tNames + nomSpanS)})
          .fx({.effect = std::move(swell),
               .stagger = {.eachMs = 130, .durationMs = 900},
               .unit = weave::Unit::Word,
               .progress = beat(tIgnite, tIgnite + 2.6)})
          // The charge reaching a name flashes it, on the pass's own
          // cascade so the letters and the shader open together.
          .fx({.effect = fx::keys(
                   {{0.00f, {}}, {0.30f, {.colorScreen = kCore}}, {1.00f, {}}},
                   &ch::easeInOutQuad),
               .stagger = {.eachMs = 170, .durationMs = 820},
               .unit = weave::Unit::Word,
               .progress = beat(tIgnite, tIgnite + 2.6)});
  names.fx({.effect = fx::pass(mskia::Paint::recipe(
                                   sigil::material::Material(chargeRecipe()))
                                   .uniform("uGold", kGold))
                          .restsAt(0.0f, 1.0f),
            .stagger = {.eachMs = 170, .durationMs = 820},
            .unit = weave::Unit::Word,
            .progress = beat(tIgnite, tIgnite + 2.6),
            .reach = kReach});
  return names;
}

auto RotaConvocationis::textura() -> Element {
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
           .stagger = {.eachMs = 4,
                       .durationMs = 300,
                       .from = motion::Spread::From::Random,
                       .seed = 61},
           .progress = beat(tTex, tTex + texSpanS)});
}
