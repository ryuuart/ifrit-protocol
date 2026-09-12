// The explanatory panels and particle census.

#include "GenesisFire.h"

Element GenesisFire::generationPanel() {
  return panel(kPanelH[0], 1)
      .gap(3)
      .child(panelHead("GENERATION LAW"))
      .child(eqn("NParts_f    = MeanParts_f + Rand() \xc3\x97 VarParts_f"))
      .child(eqn("MeanParts_f = InitialMeanParts + \xce\x94Mean \xc3\x97 "
                 "(f \xe2\x88\x92 f\xe2\x82\x80)"))
      .child(eqn("InitialSpeed = MeanSpeed + Rand() \xc3\x97 VarSpeed"))
      .child(box().grow(1))
      .child(t("Rand() \xe2\x86\x92 UNIFORM [\xe2\x88\x92"
               "1.0, +1.0] "
               "\xe2\x80\x94 REEVES 1983 \xc2\xa7"
               "2.1\xe2\x80\x93"
               "2.2",
               mono(7.5f, kSteelDim, 0.4f))
                 .shrink(0));
}

Element GenesisFire::censusBar(float frac, SkColor4f c, const char* key) {
  sketch::kit::Meter bar{.width = Dimension(96),
                         .height = Dimension(7),
                         .track = Fill::color(hexColor(0x171B24)),
                         .bar = Fill::color(c)};
  bar.level = animate(
      from(0.0f).to(frac),
      {.duration = 420ms, .ease = ease::outBack(1.2f), .delay = 1200ms});
  if (key) bar.level = bind(&liveFrac).clamp(0.02f, 1.0f);
  Element rail = sketch::kit::meter(bar).shrink(0);
  if (key) rail.key(key);
  return rail;
}

Element GenesisFire::censusRow(const char* fig, const char* sys,
                               const char* particles, const char* per,
                               float frac, bool live) {
  const SkColor4f c = live ? kCyan : kBone;
  const SkColor4f cd = live ? kCyan : kSteel;
  return box()
      .row()
      .height(14)
      .shrink(0)
      .alignItems(Align::Center)
      .child(censusCell(fig, 46, mono(9.5f, cd, 0.4f)))
      .child(censusCell(sys, 62, mono(9.5f, c, 0.4f)))
      .child(censusCell(particles, 108, monoB(9.5f, c, 0.4f)))
      .child(censusCell(per, 76, mono(9.5f, cd, 0.4f)))
      .child(censusBar(frac, live ? kCyan : hexColor(0x6D5A3F),
                       live ? "livebar" : nullptr));
}

Element GenesisFire::liveRow() {
  char parts_[32], per_[24], sys_[16];
  std::snprintf(parts_, sizeof parts_, "%zu,%03zu", liveCount / 1000,
                liveCount % 1000);
  std::snprintf(per_, sizeof per_, "%d",
                (int)std::lround((double)liveCount / (33.0 * kDepth)));
  std::snprintf(sys_, sizeof sys_,
                "33\xc3\x97"
                "%d",
                kDepth);
  return box()
      .row()
      .height(14)
      .shrink(0)
      .alignItems(Align::Center)
      .child(censusCell("THIS", 46, monoB(9.5f, kCyan, 0.4f)))
      .child(censusCell(sys_, 62, mono(9.5f, kCyan, 0.4f)))
      .child(censusCell(parts_, 108, monoB(9.5f, kCyan, 0.4f)))
      .child(censusCell(per_, 76, mono(9.5f, kCyan, 0.4f)))
      .child(censusBar(0.0f, kCyan, "livebar"));
}

Element GenesisFire::censusPanel() {
  return panel(kPanelH[1], 2)
      .gap(4)
      .child(panelHead("PARTICLE CENSUS \xe2\x80\x94 REEVES 1983 \xc2\xa7"
                       "3"))
      .child(
          box()
              .row()
              .height(11)
              .shrink(0)
              .child(censusCell("FIG", 46, mono(7.5f, kSteelDim, 0.9f)))
              .child(censusCell("SYSTEMS", 62, mono(7.5f, kSteelDim, 0.9f)))
              .child(censusCell("PARTICLES", 108, mono(7.5f, kSteelDim, 0.9f)))
              .child(censusCell("PER SYS", 76, mono(7.5f, kSteelDim, 0.9f)))
              .child(censusCell("LOG SCALE", 96, mono(7.5f, kSteelDim, 0.9f))))
      .child(
          box()
              .column()
              .gap(3)
              .shrink(0)
              .staggerChildren(70ms)
              .child(censusRow("4", "~21", "25,000", "1,190*", 0.273f, false))
              .child(censusRow("5", "~200", "75,000", "375", 0.491f, false))
              .child(censusRow("6", "~200", "85,000", "425", 0.514f, false))
              .child(censusRow("7\xe2\x80\x93"
                               "8",
                               "~400", ">750,000", ">1,875", 0.945f, false))
              .child(liveRow()))
      .child(box().grow(1))
      .child(t("* FIG. 4 IS \"ONE VERY LARGE PARTICLE SYSTEM AND ABOUT 20 "
               "SMALLER ONES\" \xe2\x80\x94 THAT MEAN IS MEANINGLESS.",
               mono(6.5f, kSteelDim, 0.2f))
                 .shrink(0))
      .child(t("NO PARAMETER VALUE IS PUBLISHED ANYWHERE. EVERY CONSTANT "
               "HERE IS ARITHMETIC ON TWO PUBLISHED INTEGERS: 85,000 "
               "\xc3\xb7 200 = 425 ALIVE PER EXPLOSION (FIG. 6), AND "
               "POPULATION = BIRTH RATE \xc3\x97 LIFETIME \xe2\x80\x94 "
               "PICK MeanLife = 34 f AND THE RATE FOLLOWS. SYSTEMS IGNITE "
               "EVERY 18.5/168 = 0.110 s: 20 GENERATING + 13 BURNING OUT "
               "= 24 FULLY-LIT EQUIVALENTS. A LIMB VIEW STACKS THE RING "
               "IN DEPTH (FIG. 6 IS ~200 SYSTEMS; THIS SLICE ANCHORS 53 "
               "COLUMNS), SO EACH COLUMN CARRIES 3: 72 \xc3\x97 425 = "
               "30,600 PREDICTED. THE \"THIS\" ROW IS MEASURED.",
               mono(6.5f, kSteel, 0.2f))
                 .shrink(0));
}

Element GenesisFire::rampPanel() {
  std::vector<Element> swatches, labels;
  swatches.reserve(14);
  labels.reserve(14);
  for (int n : kRampN) {
    swatches.push_back(
        box()
            .width(28)
            .height(26)
            .shrink(0)
            .fill(Paint::solid(overlap(n)))
            .transformOrigin(0.5f, 1.0f)
            .scaleY(animate(from(0.0f).to(1.0f), {.duration = 220ms,
                                                  .ease = ease::outBack(),
                                                  .delay = 1500ms})));
    const bool key = n == 5 || n == 20 || n == 111;
    labels.push_back(
        t(std::to_string(n).c_str(), mono(7.0f, key ? kBone : kSteelDim, 0.2f))
            .width(28)
            .shrink(0)
            .textAlign(sigil::weave::TextAlignment::kCenter));
  }
  return panel(kPanelH[2], 3)
      .gap(3)
      .child(panelHead("COLOUR IS OVERLAP COUNT"))
      .child(box().row().gap(2).shrink(0).staggerChildren(26ms).children(
          std::move(swatches)))
      .child(box().row().gap(2).shrink(0).children(std::move(labels)))
      .child(box().grow(1))
      .child(t("LIGHT ADDS AND CLAMPS (\xc2\xa7"
               "2.5) \xe2\x80\x94 RED "
               "SATURATES AT n=5, GREEN AT n=20, BLUE AT n=111. "
               "e\xe2\x82\x80 = (0.220, 0.050, 0.009) IS THE ONE "
               "RECONSTRUCTED SEED.",
               mono(6.5f, kSteel, 0.2f))
                 .shrink(0));
}

Element GenesisFire::benchCell(Element content, const char* caption,
                               SkColor4f cc) {
  return box()
      .column()
      .gap(3)
      .width(130)
      .shrink(0)
      .child(box()
                 .width(130)
                 .height(52)
                 .shrink(0)
                 .clip(true)
                 .fill(hexColor(0x05060A))
                 .stroke(stroke(1.0f, Fill::color(hexColor(0x1B2029)),
                                PathFormat::Align::Inner))
                 .child(std::move(content)))
      .child(t(caption, mono(7.0f, cc, 0.2f))
                 .width(130)
                 .textAlign(sigil::weave::TextAlignment::kCenter));
}

Element GenesisFire::renderModelPanel() {
  return panel(kPanelH[3], 4)
      .gap(4)
      .child(panelHead("RENDER MODEL \xe2\x80\x94 THREE PATHS, ONE POOL"))
      .child(box()
                 .row()
                 .gap(15)
                 .shrink(0)
                 .child(benchCell(box().inset(0).child(instancing::instances(
                                      abAtlas, abPool, instancing::Mode::Live,
                                      SkBlendMode::kSrcOver)),
                                  "instances() \xc2\xb7 kSrcOver",
                                  hexColor(0x8A93A8)))
                 .child(benchCell(box().inset(0).child(instancing::instances(
                                      abAtlas, abPool, instancing::Mode::Live,
                                      SkBlendMode::kPlus)),
                                  "instances() \xc2\xb7 kPlus",
                                  hexColor(0xFFB672)))
                 .child(benchCell(box().inset(0), "pen quads \xc2\xb7 kPlus",
                                  hexColor(0xFFB672))))
      .child(box().grow(1))
      .child(t("SAME 700 PARTICLES, ONE POOL. LEFT AND CENTRE DIFFER ONLY "
               "IN BLEND: kSrcOver CANNOT ACCUMULATE, SO ITS WHOLE PALETTE "
               "IS LUT ENTRY n=1. ALL THREE ARE STREAKED SPHERICAL "
               "\xe2\x80\x94 LENGTH 0.5\xc2\xb7|v|, WIDTH size. THE TWO "
               "POOLS TAKE IT FROM Pool::sizes(), THE OPT-IN NON-UNIFORM "
               "LANE THAT STRETCHES ONE BAKED CELL PER INSTANCE.",
               mono(6.5f, kSteel, 0.2f))
                 .shrink(0));
}

Element GenesisFire::productionPanel() {
  return panel(kPanelH[4], 5)
      .gap(1)
      .child(panelHead("PRODUCTION \xe2\x80\x94 SMITH 1982"))
      .child(prodLine("67-SECOND SHOT \xc2\xb7 250,000 PX/FRAME \xc2\xb7 "
                      "500-LINE VIDEO MONITOR",
                      kBone))
      .child(prodLine("2 MAN-YEARS OVER AN 80-SECOND PIECE (60 s GENESIS + "
                      "20 s RETINA ID)",
                      kBone))
      .child(prodLine("FRAMES: 5 MINUTES TO 5 HOURS \xc2\xb7 ~1 MONTH OF "
                      "VAX TIME FOR THE FRACTALS",
                      kSteel))
      .child(prodLine("E&S PICTURE SYSTEM II \xc2\xb7 2\xc3\x97 IKONAS "
                      "\xc2\xb7 BARCO \xc2\xb7 HITACHI TABLET",
                      kSteel))
      .child(prodLine("DELIVERED MARCH 19, 1982 \xc2\xb7 SHOT TO "
                      "VISTAVISION BY ILM",
                      kSteel))
      .child(box().grow(1))
      .child(t("Am. Cinematographer 63(10) \xe2\x80\x94 caption: 67 s; "
               "body text: 60 s. Both printed.",
               mono(6.5f, kSteelDim, 0.2f))
                 .shrink(0));
}

Element GenesisFire::header() {
  Track rise{.effect = fx::rise(22),
             .stagger = {.eachMs = 26, .durationMs = 460},
             .progress = animate(
                 from(0.0f).to(1.0f),
                 {.duration = 850ms, .ease = &ch::easeNone, .delay = 120ms})};
  return box()
      .column()
      .height(kHeaderH)
      .shrink(0)
      .gap(4)
      .child(t("STOCHASTIC PARTICLE SYSTEMS", ui(11.5f, kSteel, 2.7f))
                 .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
                 .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})))
      .child(t("THE GENESIS DEMO, 1982", faced(heavyFace(), 46, kBone, -0.4f))
                 .key("title")
                 .fx(std::move(rise)))
      .child(t("W. T. Reeves, Lucasfilm Ltd \xe2\x80\x94 \"Particle "
               "Systems: A Technique for Modeling a Class of Fuzzy "
               "Objects\", SIGGRAPH '83 / ACM TOG 2(2) \xc2\xb7 sequence "
               "dir. Alvy Ray Smith \xc2\xb7 Star Trek II, Paramount, "
               "June 4, 1982",
               ui(11.0f, kSteel, 0.1f))
                 .opacity(animate(from(0.0f).to(1.0f),
                                  {.duration = 240ms, .delay = 420ms})))
      .child(box().grow(1))
      .child(box().height(1).shrink(0).fill(kKeyline).opacity(
          animate(from(0.0f).to(1.0f), {.duration = 400ms, .delay = 320ms})));
}
