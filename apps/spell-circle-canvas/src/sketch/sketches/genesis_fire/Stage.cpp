// The planet, star field and layered stage artwork.

#include "GenesisFire.h"

void GenesisFire::seedStars() {
  starAtlas = std::make_shared<instancing::Atlas>(2.0f);
  // Five magnitude cells: a soft disc each, 10/7/5/4/3 px logical.
  const float sizes[5] = {7.0f, 5.4f, 4.2f, 3.2f, 2.4f};
  for (float s : sizes)
    starAtlas->cell(box().width(s).height(s).fill(
                        Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                          {{0.0f, {1, 1, 1, 1}},
                                           {0.22f, {1, 1, 1, 0.78f}},
                                           {0.58f, {1, 1, 1, 0.14f}},
                                           {1.0f, {1, 1, 1, 0.0f}}})),
                    {s, s});
  // N(m) ~ 10^(0.6m): 1 / 5 / 19 / 76 / 319 = 420 stars.
  const int bin[5] = {1, 5, 19, 76, 319};
  // B-V ramp [S82]: Carpenter "deduced the colors of the individual
  // stars" from the Yale Bright Star Catalogue.
  const SkColor4f bv[6] = {hexColor(0xAEC6FF), hexColor(0xD6E2FF),
                           hexColor(0xFFFFFF), hexColor(0xFFE9B8),
                           hexColor(0xFFC48A), hexColor(0xFF9E6E)};
  const int bvWeight[6] = {6, 12, 20, 26, 24, 12};
  starPool = std::make_shared<instancing::Pool>();
  rng.reseed(0x5EED1982u);
  for (int m = 0; m < 5; ++m) {
    for (int k = 0; k < bin[m]; ++k) {
      float x = 0, y = 0;
      for (int guard = 0; guard < 24; ++guard) {
        x = rng.unit() * kStageW;
        y = rng.unit() * (kStageH * 0.86f);
        if (y < limbY(x) - 8.0f) break;
      }
      int pick = (int)(rng.unit() * 100.0f), c = 0, acc = 0;
      for (; c < 6; ++c) {
        acc += bvWeight[c];
        if (pick < acc) break;
      }
      SkColor4f col = bv[std::min(c, 5)];
      col.fA = 0.30f + 0.55f * rng.unit();
      starPool->add({x, y}, m, 0.0f, 0.60f + 0.45f * rng.unit(), col);
    }
  }
}

Element GenesisFire::starField() {
  return box()
      .inset(0)
      .opacity(
          animate(from(0.0f).to(1.0f), {.duration = 700ms, .delay = 340ms}))
      .child(instancing::instances(starAtlas, starPool, instancing::Mode::Data,
                                   SkBlendMode::kPlus));
}

Element GenesisFire::dipper() {
  // Real relative geometry, framed into x[430,860] y[40,250].
  static constexpr DipStar kStars[8] = {
      {"ALKAID", 0.05f, 0.10f, 1.85f}, {"MIZAR", 0.24f, 0.26f, 2.23f},
      {"ALIOTH", 0.42f, 0.30f, 1.77f}, {"MEGREZ", 0.58f, 0.36f, 3.31f},
      {"PHECDA", 0.62f, 0.52f, 2.44f}, {"MERAK", 0.86f, 0.44f, 2.37f},
      {"DUBHE", 0.86f, 0.18f, 1.79f},  {"SOL", 0.40f, 0.14f, 2.63f}};
  constexpr float bx = 430, by = 40, bw = 430, bh = 210;
  auto at = [&](int i) {
    return SkPoint{bx + kStars[i].u * bw, by + kStars[i].v * bh};
  };

  Element g = box().inset(0);

  // the asterism, drawn on
  g.child(
      box()
          .inset(0)
          .shape(keyedShape(std::string_view("asterism"),
                            [](SkSize) {
                              SkPathBuilder b;
                              auto P = [](int i) {
                                return SkPoint{bx + kStars[i].u * bw,
                                               by + kStars[i].v * bh};
                              };
                              b.moveTo(P(0));
                              b.lineTo(P(1));
                              b.lineTo(P(2));
                              b.lineTo(P(3));
                              b.lineTo(P(4));
                              b.lineTo(P(5));
                              b.lineTo(P(6));
                              b.lineTo(P(3));
                              return b.detach();
                            }))
          .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                      {.duration = 620ms, .delay = 1300ms})),
                  stroke(1.0f, Fill::color(hexColor(0x4FB8D8, 0.35f))))
          .key("asterism"));

  for (int i = 0; i < 8; ++i) {
    const SkPoint p = at(i);
    const float rad = std::max(1.4f, 4.6f - 0.85f * kStars[i].mag);
    const bool sol = i == 7;
    g.child(kit::disc(p, rad * 2.0f)
                .fill(Paint::radialUnit(
                    {0.5f, 0.5f}, 0.707f,
                    {{0.0f, sol ? hexColor(0xFFFFFF) : hexColor(0xEFF3FF)},
                     {0.22f, sol ? hexColor(0xFFF4D8, 0.9f)
                                 : hexColor(0xD9E4FF, 0.85f)},
                     {1.0f, {1, 1, 1, 0}}}))
                .blend(SkBlendMode::kPlus)
                .opacity(animate(from(0.0f).to(1.0f),
                                 {.duration = 500ms, .delay = 1200ms})));
    g.child(t(kStars[i].name,
              mono(7.0f, sol ? kCyan : hexColor(0x9FB0CC, 0.85f), 1.1f))
                .left(p.fX + rad + 5.0f)
                .top(p.fY - 5.0f)
                .opacity(animate(from(0.0f).to(1.0f),
                                 {.duration = 400ms, .delay = 1500ms})));
  }
  // Smith's joke, verified in the header block.
  const SkPoint s = at(7);
  g.child(box().left(s.fX + 4).top(s.fY + 6).width(1).height(16).fill(
      hexColor(0x4FB8D8, 0.5f)));
  g.child(box()
              .left(s.fX + 9)
              .top(s.fY + 12)
              .column()
              .gap(1)
              .opacity(animate(from(0.0f).to(1.0f),
                               {.duration = 400ms, .delay = 1600ms}))
              .child(t("m = 2.63 FROM \xce\xb5 INDI (3.64 pc)",
                       mono(7.0f, kCyan, 0.9f)))
              .child(t("\"OUR SUN WOULD APPEAR AS AN EXTRA STAR\"",
                       mono(7.0f, hexColor(0x4FB8D8, 0.7f), 0.9f))));
  return g;
}

Element GenesisFire::regolith() {
  // A generated surface, plus the ONE hand-added light in the shot
  // (Tom Duff's), riding the wavefront.
  Paint ground =
      Paint::blend({{Paint::radialUnit({0.5f, 0.723f}, 0.50f,
                                       {{0.0f, hexColor(0x3B3933)},
                                        {0.42f, hexColor(0x232119)},
                                        {1.0f, hexColor(0x0A0A0C)}}),
                     SkBlendMode::kSrc},
                    {Paint::recipe(field::grain(0.022f, 4, 7.0f, 0.5f, 1.0f)),
                     SkBlendMode::kSoftLight},
                    {Pattern(patterns::speckle(170, 17, 0.9f, 3.4f,
                                               {toColor(hexColor(0x6A655B)),
                                                toColor(hexColor(0x171512))}))
                         .material(),
                     SkBlendMode::kOverlay}});

  return box()
      .inset(0)
      .shape(limbOutline())
      .clip(true)
      .fill(std::move(ground))
      .opacity(
          animate(from(0.0f).to(1.0f), {.duration = 520ms, .delay = 420ms}))
      .translateY(animate(
          from(12.0f).to(0.0f),
          {.duration = 520ms, .ease = &ch::easeOutCubic, .delay = 420ms}))
      // Duff's local light. ONE Output (loopU) shaped into px.
      .child(kit::disc(SkPoint{0, 0}, 132)
                 .fill(Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                         {{0.0f, hexColor(0xFF8A3A, 0.62f)},
                                          {0.38f, hexColor(0xC24E14, 0.24f)},
                                          {1.0f, hexColor(0xFF8A3A, 0.0f)}}))
                 .blend(SkBlendMode::kPlus)
                 .translateX(bind(&loopU).scale(1680.0f).offset(-80.0f))
                 .translateY(limbY(444.0f) + 26.0f)
                 .opacity(bind(&loopU).map([](float v) {
                   const float t = v * 10.0f;
                   return std::clamp(t / 0.4f, 0.0f, 1.0f) *
                          std::clamp((9.6f - t) / 0.8f, 0.0f, 1.0f);
                 })));
}

Element GenesisFire::shockwave() {
  // The impact flash and Carpenter/Cole's shockwave, at the impact point
  // (off-frame left). Documented as elements; timing is reconstruction.
  const SkPoint impact{kX0, limbY(kX0 < 0 ? 0.0f : kX0) + 8.0f};
  Element g = box().inset(0);
  g.child(kit::disc(impact, 170)
              .fill(Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                      {{0.0f, {1, 1, 1, 0.95f}},
                                       {0.25f, hexColor(0xFFE7B0, 0.6f)},
                                       {1.0f, hexColor(0xFF7A20, 0.0f)}}))
              .blend(SkBlendMode::kPlus)
              .opacity(bind(&loopU).map([](float v) {
                const float t = v * 10.0f;
                if (t < 0.06f) return t / 0.06f;
                if (t < 0.45f) {
                  const float k = 1.0f - (t - 0.06f) / 0.39f;
                  return k * k;
                }
                return 0.0f;
              })));
  g.child(kit::disc(impact, 520)
              .shape(shapes::circle())
              .stroke(stroke(2.0f, Fill::color(hexColor(0xFFB070, 0.85f))))
              .blend(SkBlendMode::kPlus)
              .scale(bind(&loopU)
                         .map([](float v) {
                           return choreograph::easeOutCubic(
                               std::clamp(v * 10.0f / 1.1f, 0.0f, 1.0f));
                         })
                         .clamp(0.001f, 1.0f))
              .opacity(bind(&loopU).map([](float v) {
                const float t = v * 10.0f;
                if (t > 1.1f) return 0.0f;
                const float k = 1.0f - t / 1.1f;
                return k * k;
              })));
  return g;
}

Element GenesisFire::stageBelow() {
  return stack()
      .width(kStageW)
      .height(kStageH)
      .clip()
      .fill(Paint::linearUnit({0.5f, 0.0f}, {0.5f, 0.85f},
                              {{0.0f, hexColor(0x03040A)},
                               {0.55f, hexColor(0x05060D)},
                               {1.0f, hexColor(0x0A0B13)}}))
      .child(starField().zIndex(1))
      .child(dipper().zIndex(2))
      .child(regolith().zIndex(3))
      .child(shockwave().zIndex(4));
}

Element GenesisFire::planInset() {
  // Fig. 2: the distribution of second-level systems on the planet's
  // surface, live.
  Element inner =
      box()
          .left(12)
          .top(12)
          .width(184)
          .height(184)
          .shape(shapes::circle())
          .clip(true)
          .stroke(stroke(1.0f, Fill::color(hexColor(0x4FB8D8, 0.55f)),
                         PathFormat::Align::Inner))
          // the expanding wavefront ring — same Output, unit scale
          .child(
              kit::disc(SkPoint{34, 106}, 124)
                  .shape(shapes::circle())
                  .stroke(stroke(1.0f, Fill::color(hexColor(0x4FB8D8, 0.75f))))
                  .scale(bind(&loopU)
                             .scale(10.0f / (float)kFrontCrossSeconds)
                             .clamp(0.004f, 1.0f)))
          .child(box().inset(0).child(
              instancing::instances(planAtlas, planPool, instancing::Mode::Live,
                                    SkBlendMode::kPlus)));

  return box()
      .left(24)
      .top(24)
      .width(208)
      .height(208)
      .corners({6})
      .fill(hexColor(0x0B0D14, 0.86f))
      .stroke(stroke(1.5f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(
          animate(from(0.0f).to(1.0f), {.duration = 340ms, .delay = 900ms}))
      .scale(
          animate(from(0.94f).to(1.0f),
                  {.duration = 340ms, .ease = ease::outBack(), .delay = 900ms}))
      .child(std::move(inner))
      // the impact point itself
      .child(box()
                 .left(12 + 34 - 2)
                 .top(12 + 106 - 2)
                 .width(4)
                 .height(4)
                 .shape(shapes::circle())
                 .fill(hexColor(0xFFFFFF, 0.95f)))
      // rim caption on a curved baseline
      .child(t("IMPACT \xc2\xb7 KETI BANDAR \xc2\xb7 \xce\xb5 INDI",
               mono(8.0f, kCyan, 1.4f))
                 .left(12)
                 .top(12)
                 .width(184)
                 .height(184)
                 .onPath(TextPath{.path = shapes::circle(),
                                  .at = 0.75f,
                                  .align = TextPath::Align::Center,
                                  .offset = 8.0f}));
}

Element GenesisFire::stageAbove() {
  return stack()
      .width(kStageW)
      .height(kStageH)
      .clip()
      .child(planInset())
      .child(t("FIG. 2 \xe2\x80\x94 DISTRIBUTION OF PARTICLE SYSTEMS ON THE "
               "PLANET'S SURFACE",
               mono(8.5f, kSteel, 0.6f))
                 .left(24)
                 .top(236)
                 .width(300)
                 .opacity(animate(from(0.0f).to(1.0f),
                                  {.duration = 300ms, .delay = 1050ms})));
}

void GenesisFire::blurCallout(Pen& pen, float x0, float y0, float w, float h,
                              float a) {
  // the panel
  pen.noStroke();
  pen.fill(hexColor(0x0B0D14, 0.86f * a));
  pen.rect(x0, y0, w, h, 6);
  pen.noFill();
  pen.stroke(hexColor(0x242A36, a));
  pen.strokeWeight(1.5f);
  pen.rect(x0 + 0.75f, y0 + 0.75f, w - 1.5f, h - 1.5f, 6);
  pen.noStroke();

  float cy = y0 + 11;
  pen.textFont(weave::Type{.face = uiFace(), .size = 8.5f, .track = 1.7f});
  pen.fill(hexColor(0x4FB8D8, a));
  pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
  pen.text(
      "MOTION BLUR \xe2\x80\x94 REEVES 1983 \xc2\xa7"
      "3",
      x0 + 11, cy);
  cy += 14;

  // the streak itself: a 3x-magnified quad, one shape, colour ramped
  // across the cross-section by the fills between its vertices
  const float sx = x0 + 11, sy = cy + h * 0.30f;
  const float x1 = sx + 5, x2 = sx + 141;
  const SkColor4f hot = overlap(9);
  const SkColor4f edge = {hot.fR, hot.fG, hot.fB, 0.0f};
  pen.noStroke();
  pen.beginShape(sigil::draw::TRIANGLE_STRIP);
  pen.fill(edge);
  pen.vertex(x1, sy - 5);
  pen.vertex(x2, sy - 5);
  pen.fill(hexColor(hot.toSkColor() & 0xFFFFFFu, a));
  pen.vertex(x1, sy);
  pen.vertex(x2, sy);
  pen.fill(edge);
  pen.vertex(x1, sy + 5);
  pen.vertex(x2, sy + 5);
  pen.endShape();

  // the two sample positions
  pen.fill(hexColor(0xFFFFFF, 0.95f * a));
  pen.circle(x2, sy, 5.2f);
  pen.fill(hexColor(0xFFFFFF, 0.45f * a));
  pen.circle(x1, sy, 4.0f);
  // cyan dimension bracket
  pen.noFill();
  pen.stroke(hexColor(0x4FB8D8, 0.9f * a));
  pen.strokeWeight(1.0f);
  const float by = sy + 13;
  pen.line(x1, by - 3, x1, by + 3);
  pen.line(x2, by - 3, x2, by + 3);
  pen.line(x1, by, x2, by);
  pen.noStroke();
  // labels
  penMono(pen, 7.0f, hexColor(0x9FB0CC, a));
  pen.text("pos(f + 1/2)", x1 - 8, sy - 18);
  pen.text("pos(f)", x2 - 14, sy - 18);
  penMono(pen, 7.0f, hexColor(0x4FB8D8, a));
  pen.text("0.5 \xc2\xb7 |v|", (x1 + x2) * 0.5f - 16, by + 4);

  cy = y0 + h - 46;
  penMono(pen, 7.5f, fadeTo(kBone, a), 0.4f);
  pen.text("SHUTTER 1/50 s @ 24 fps \xe2\x89\x88 \xc2\xbd FRAME OF MOTION",
           x0 + 11, cy);
  cy += 11;
  penMono(pen, 7.5f, fadeTo(kSteel, a), 0.4f);
  pen.text(
      "STREAK = pos(f) \xe2\x86\x92 pos(f+\xc2\xbd), ANTIALIASED, "
      "ADDITIVE",
      x0 + 11, cy);
  cy += 11;
  penMono(pen, 6.5f, fadeTo(kSteelDim, a), 0.2f);
  pen.text(
      "FN.4: \"A PARTICLE'S TRAJECTORY IS ACTUALLY PARABOLIC, BUT\n"
      "THE STRAIGHT-LINE APPROXIMATION HAS SO FAR PROVED SUFFICIENT\"",
      x0 + 11, cy);
}
