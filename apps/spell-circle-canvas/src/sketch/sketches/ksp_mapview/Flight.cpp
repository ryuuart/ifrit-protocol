#include "KspMapView.h"

auto KspMapView::navball() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);

  // Bezel: silver ring, gently lit from the top-left.
  g.child(
      at(box()
             .shape(shapes::circle())
             .fill(Paint::linearUnit({0.15f, 0}, {0.85f, 1},
                                     {{0.0f, hexColor(0xC8CDD0)},
                                      {0.45f, hexColor(0x8B9296)},
                                      {1.0f, hexColor(0x5A6165)}}))
             .stroke(PathFormat{.width = 1.2f,
                                .strokeFill = Fill::color(hexColor(0x2A3034))}),
         kBall, kBezelR * 2, kBezelR * 2));
  g.child(
      at(box().shape(shapes::circle()).fill(Paint::solid(hexColor(0x171B1E))),
         kBall, (kBallR + 5) * 2, (kBallR + 5) * 2));

  // The sphere itself: the library's orthographic globe, at KSP's own
  // sky and ground, driven by the same three Outputs the ladder rides.
  Paint ball = Paint::recipe(matkit::globe({.sky = kSky,
                                            .skyPole = kSkyHi,
                                            .ground = kGround,
                                            .groundPole = kGroundLo})
                                 .bind("yaw", &yaw)
                                 .bind("pitch", &pitchOut)
                                 .bind("roll", &rollOut));
  g.child(at(box().fill(std::move(ball)).key("navball"), kBall, kBallR * 2,
             kBallR * 2));

  // THE PITCH LADDER. A navball is an ATTITUDE INDICATOR, and what makes
  // one readable is the numbered ladder: white rungs at 10, 20, 30, 45
  // and 60 either side of a bold horizon, carrying their own numerals.
  // Latitude and longitude bands say where a point on a sphere is; a
  // ladder says where the CRAFT is, which is the instrument's whole job.
  //
  // The deck rides the same two Outputs the sphere's shader does, so it
  // can never disagree with the ball under it: roll turns it, pitch
  // slides it. Over this scene's pitch range the small-angle map is
  // within two per cent of the sphere's own sine, and the deck is
  // clipped to the ball, so the error lands where nothing is drawn.
  {
    Element deck =
        box()
            .inset(0)
            .rotate(bind(&rollOut)
                        .source(-1.0f, 1.0f)
                        .target(57.29578f, -57.29578f))
            .transformOrigin(0.5f, 0.5f)
            .translateY(
                bind(&pitchOut).source(-1.0f, 1.0f).target(-kBallR, kBallR));
    static const int kRungs[5] = {10, 20, 30, 45, 60};
    const float mid = kBallR;
    auto rung = [&](int deg, int sign) {
      const float y =
          mid - (float)sign * kBallR * std::sin((float)deg * 0.0174532925f);
      const float halfW = (deg % 30 == 0) ? kBallR * 0.44f : kBallR * 0.26f;
      deck.child(
          box()
              .left(Dimension(mid - halfW))
              .top(Dimension(y - 0.9f))
              .width(Dimension(halfW * 2.0f))
              .height(Dimension(1.8f))
              .fill(Paint::solid(mskia::withAlpha(hexColor(0xEAF4F8), 0.88f))));
      const std::string num = std::to_string(deg);
      for (int e = 0; e < 2; ++e)
        deck.child(
            t(num.c_str(),
              bold(7.0f, mskia::withAlpha(hexColor(0xEAF4F8), 0.9f), 0.3f))
                .left(Dimension(e ? mid + halfW + 3.0f : mid - halfW - 13.0f))
                .top(Dimension(y - 5.0f)));
    };
    for (int r : kRungs) {
      rung(r, +1);
      rung(r, -1);
    }
    // The horizon: one bold white rule, which is the line every other
    // reading on the instrument is taken against.
    deck.child(box()
                   .left(Dimension(mid - kBallR * 0.92f))
                   .top(Dimension(mid - 1.4f))
                   .width(Dimension(kBallR * 1.84f))
                   .height(Dimension(2.8f))
                   .fill(Paint::solid(hexColor(0xFFFFFF))));
    g.child(at(box().shape(shapes::circle()).clip().child(std::move(deck)),
               kBall, kBallR * 2, kBallR * 2));
  }

  // Curved dial tapes on the bezel annulus — THROTTLE left, G FORCE right,
  // exactly as the reference draws them. sector() gives the closed,
  // fillable annular segment; onPath gives the curved lettering.
  auto tape = [&](float startDeg, float sweep, SkColor4f fillC) {
    return at(
        box()
            .shape(shapes::sector(startDeg, sweep, 0.845f))
            .fill(Paint::solid(fillC))
            .stroke(PathFormat{.width = 0.9f,
                               .strokeFill = Fill::color(hexColor(0x2A3034))}),
        kBall, kBezelR * 2, kBezelR * 2);
  };
  g.child(tape(150, 60, hexColor(0x23282B)));  // throttle body
  g.child(tape(143, 8, hexColor(0xC0392B)));   // hazard cap, top
  g.child(tape(210, 6, hexColor(0x2E7D32)));   // green foot
  g.child(tape(-30, 60, hexColor(0x23282B)));  // g-force body
  g.child(tape(-38, 9, hexColor(0xC0392B)));
  g.child(tape(30, 7, hexColor(0x2E7D32)));

  // THE TICK LADDERS. The reference's two tapes are read against a rung
  // ladder, not against their own ends: eleven ticks across each arc,
  // every fifth one long. Without them a needle on a plain band says
  // "somewhere in this arc" and nothing more.
  for (int side = 0; side < 2; ++side) {
    const float from = side ? -30.0f : 150.0f;
    const float sweep = 60.0f;
    for (int i = 0; i <= 10; ++i) {
      const bool major = i % 5 == 0;
      const float a = from + sweep * (float)i / 10.0f;
      g.child(at(
          box()
              .shape(shapes::sector(-0.42f, 0.84f, major ? 0.79f : 0.86f))
              .fill(Paint::solid(mskia::withAlpha(hexColor(0xC6CFD3), 0.85f)))
              .rotate(a)
              .transformOrigin(0.5f, 0.5f),
          kBall, kBezelR * 2, kBezelR * 2));
    }
  }

  // Moving needles. The Outputs stay in their OWN units — throttle and
  // g-force are both 0..1 — and bind() maps each onto its tape's arc at
  // the property, so no second Output carrying degrees has to be kept in
  // step with the first from the tick loop.
  g.child(at(box()
                 .shape(shapes::sector(-3.0f, 6.0f, 0.80f))
                 .fill(Paint::solid(hexColor(0xF2F4F5)))
                 .rotate(bind(&throttle).target(207, 148))
                 .transformOrigin(0.5f, 0.5f),
             kBall, kBezelR * 2, kBezelR * 2));
  g.child(at(box()
                 .shape(shapes::sector(-3.0f, 6.0f, 0.80f))
                 .fill(Paint::solid(hexColor(0xF2F4F5)))
                 .rotate(bind(&gforce).target(28, -33))
                 .transformOrigin(0.5f, 0.5f),
             kBall, kBezelR * 2, kBezelR * 2));

  auto arcLabel = [&](const char* s, float atFrac, float sz, SkColor4f c,
                      float off) {
    return at(t(s, body(sz, c, 1.1f))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = atFrac,
                                   .align = TextPath::Align::Center,
                                   .offset = off,
                                   .autoFlip = true}),
              kBall, kBezelR * 1.88f, kBezelR * 1.88f);
  };
  g.child(arcLabel("THROTTLE", 0.5f, 8.5f, hexColor(0xE8EDEF), 3));
  g.child(arcLabel("G FORCE", 0.0f, 8.5f, hexColor(0xE8EDEF), 3));
  g.child(arcLabel("100", 0.425f, 7.0f, hexColor(0xB9C2C6), -8));
  g.child(arcLabel("0", 0.575f, 7.0f, hexColor(0xB9C2C6), -8));
  g.child(arcLabel("+15", 0.075f, 7.0f, hexColor(0xB9C2C6), -8));
  g.child(arcLabel("0", 0.925f, 7.0f, hexColor(0xB9C2C6), -8));

  // A heading ring of numerals inside the bezel, rotating with the ball —
  // eight onPath runs sharing one rotating container.
  Element ring = at(box().rotate(&ringSpin).transformOrigin(0.5f, 0.5f), kBall,
                    kBallR * 1.66f, kBallR * 1.66f);
  static const char* kHdg[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i)
    ring.child(t(kHdg[i],
                 bold(9.0f, mskia::withAlpha(hexColor(0xEAF4F8), 0.85f), 0.6f))
                   .inset(0)
                   .onPath(TextPath{.path = shapes::circle(),
                                    .at = 0.75f + (float)i / 4.0f,
                                    .align = TextPath::Align::Center,
                                    .offset = 2.0f,
                                    .autoFlip = true}));
  g.child(std::move(ring));

  // The gold level chevron — screen-locked while everything under it turns.
  g.child(at(box()
                 .shape(shapes::chevron(0.20f, 0.34f, 0.16f, 0.20f))
                 .fill(Paint::solid(kGold)),
             kBall, 92, 26));

  // Readouts above and below.
  g.child(
      at(box()
             .column()
             .alignItems(Align::Center)
             .justify(Justify::Center)
             .corners({4})
             .fill(Paint::solid(kLcdBg))
             .stroke(PathFormat{.width = 1.2f,
                                .strokeFill = Fill::color(hexColor(0x9AA2A6))})
             .child(t("Orbit", lcd(11, kLcd)))
             .child(t("1140.0m/s", lcd(13, kLcdVal)).key("spd")),
         kBall.fX - 68, kBall.fY - kBezelR - 6, 136, 38));
  g.child(
      at(box()
             .row()
             .gap(6)
             .alignItems(Align::Center)
             .justify(Justify::Center)
             .corners({4})
             .fill(Paint::solid(kLcdBg))
             .stroke(PathFormat{.width = 1.2f,
                                .strokeFill = Fill::color(hexColor(0x9AA2A6))})
             .child(t("HDG", body(10, hexColor(0xA9B4B8))))
             .child(t("280°", lcd(12, kLcd)).key("hdg")),
         kBall.fX - 52, kBall.fY + kBezelR - 8, 104, 24));

  // RCS / SAS toggles, flanking the ball's shoulders.
  auto toggle = [&](const char* label, SkColor4f c, float x) {
    return at(
        box()
            .corners({3})
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .fill(Paint::linearUnit(
                {0, 0}, {0, 1}, {{0.0f, mskia::lighten(c, 0.14f)}, {1.0f, c}}))
            .stroke(
                PathFormat{.width = 1.0f,
                           .strokeFill = Fill::color(hexColor(0xE8EDEF, 0.5f)),
                           .align = PathFormat::Align::Inner})
            .opacity(&rcsPulse)
            .child(t(label, bold(10, hexColor(0xFFFFFF)))),
        x, kBall.fY - kBezelR + 4, 40, 20);
  };
  g.child(toggle("RCS", kRcs, kBall.fX - kBezelR + 6));
  g.child(toggle("SAS", kSas, kBall.fX + kBezelR - 46));

  // The manoeuvre Δv arc riding outside the bezel, plus its tag — this is
  // the flight-view frame's own composition, verbatim.
  g.child(
      at(box()
             .shape(shapes::arc(-72, 144))
             .stroke(spans::upTo(&dvSweep),
                     brush::presets::filament(mskia::withAlpha(kDvArc, 0.5f),
                                              hexColor(0xEBFFDA), 0.5f)),
         kBall, (kBezelR + 16) * 2, (kBezelR + 16) * 2));
  g.child(at(box()
                 .row()
                 .alignItems(Align::Center)
                 .justify(Justify::Center)
                 .corners({3})
                 .fill(Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0xA0A6AA)}, {1.0f, hexColor(0x6E767B)}}))
                 .gap(5)
                 .child(t("164.9m/s", body(11, hexColor(0x14181A))))
                 .child(box()
                            .width(Dimension(13))
                            .height(Dimension(13))
                            .corners({2})
                            .alignItems(Align::Center)
                            .justify(Justify::Center)
                            .fill(Paint::solid(kStageTab))
                            .child(t("×", bold(10, hexColor(0xFFFFFF))))),
             // must clear SAS, whose right edge is at
             // kBall.fX + kBezelR - 6
             kBall.fX + kBezelR + 6, kBall.fY - kBezelR - 2, 92, 20));
  // bezel index notch, top
  g.child(at(box()
                 .shape(shapes::polygon(3, 180))
                 .fill(Paint::solid(hexColor(0xD7DDE0))),
             {kBall.fX, kBall.fY - kBezelR - 4}, 16, 10));
  return g;
}

auto KspMapView::staging() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);
  const float x = 14;

  // The hazard stripe is an Sk2D lattice hatch clipped to the tab, not a
  // drawn texture. It has to be a foreground (a background hatch paints
  // UNDER the node's own fill and disappears), so the digit rides a
  // SIBLING drawn after the striped plate rather than a child of it.
  auto stageTab = [&](const char* n, float y) {
    return at(stack(), x, y, 58, 25)
        .child(
            box()
                .inset(0)
                .corners({2})
                .fill(Paint::solid(kStageTab))
                .foreground(lines::presets::hatch(
                    Fill::color(hexColor(0x101010, 0.45f)), 8.0f, 3.4f, -45.0f))
                .stroke(
                    PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x7A3703)),
                               .align = PathFormat::Align::Inner}))
        .child(box()
                   .inset(0)
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(t(n, bold(13, hexColor(0xFFFFFF)))));
  };
  auto partIcon = [&](float py, const char* badge, const char* count) {
    return at(
        stack()
            .corners({2})
            .fill(Paint::linearUnit({0, 0}, {0, 1},
                                    {{0.0f, hexColor(0x767F86)},
                                     {0.5f, hexColor(0x545D64)},
                                     {1.0f, hexColor(0x333A3F)}}))
            .stroke(PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x1D2226)),
                               .align = PathFormat::Align::Inner})
            .child(box()
                       .inset(0)
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .child(t(badge, body(14, hexColor(0xE3E9EC)))))
            .child(box()
                       .right(Dimension(1))
                       .bottom(Dimension(0))
                       .child(t(count, bold(8, hexColor(0xF6D488))))),
        x + 4, py, 27, 27);
  };
  // NOT `sketch::kit::meter`, and the difference is the artefact: that
  // component sets a bar's name and its reading in a row ABOVE the bar,
  // where a KSP resource gauge carries its name INSIDE the tank, over
  // the fuel. Everything else about it is the same bound level scaled
  // from its left edge over a cached bed.
  auto fuelBar = [&](float py, const ch::Output<float>* fill) {
    return at(
        box()
            .fill(Paint::solid(hexColor(0x14181B)))
            .stroke(PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x3B4147)),
                               .align = PathFormat::Align::Inner})
            .clip()
            // scaleX + transformOrigin: the drain is a transform,
            // not a re-laid-out width
            .child(
                box()
                    .inset(1)
                    .fill(Paint::linearUnit(
                        {0, 0}, {0, 1},
                        {{0.0f, mskia::lighten(kFuel, 0.10f)}, {1.0f, kFuel}}))
                    .scaleX(fill)
                    .transformOrigin(0.0f, 0.5f))
            .child(box()
                       .inset(4, 0, 0, 0)
                       .alignItems(Align::Center)
                       .child(t("LiquidFuel", body(9, hexColor(0xF0F3F0))))),
        x + 37, py, 96, 13);
  };

  g.child(stageTab("0", 528));
  g.child(partIcon(557, "▤", "4"));
  g.child(stageTab("1", 592));
  for (int i = 0; i < 4; ++i) {
    const float y = 620.0f + (float)i * 31.0f;
    const ch::Output<float>* f = i == 0   ? &fuel0
                                 : i == 1 ? &fuel1
                                 : i == 2 ? &fuel2
                                          : &fuel3;
    g.child(partIcon(y, "▥", i == 0 ? "4" : i == 1 ? "3" : i == 2 ? "2" : "1"));
    g.child(fuelBar(y + 7, f));
  }

  // STAGE cluster: hazard header, green go-button, the ONE inverted LCD.
  Element stage = at(box().column().corners({3}).clip(), x - 6, 756, 152, 38);
  stage.fill(Paint::solid(hexColor(0x2A2E31)))
      .stroke(PathFormat{.width = 1.0f,
                         .strokeFill = Fill::color(hexColor(0x4A5157)),
                         .align = PathFormat::Align::Inner});
  stage.child(
      box()
          .height(Dimension(9))
          .fill(Paint::solid(hexColor(0xE0B720)))
          .foreground(lines::presets::hatch(
              Fill::color(hexColor(0x141414, 0.9f)), 8.0f, 4.0f, -45.0f)));
  stage.child(
      box()
          .row()
          .grow(1)
          .gap(5)
          .padding(0, 6, 0, 6)
          .alignItems(Align::Center)
          .child(box()
                     .width(Dimension(15))
                     .height(Dimension(15))
                     .shape(shapes::circle())
                     .fill(Paint::radialUnit({0.38f, 0.30f}, 1.0f,
                                             {{0.0f, hexColor(0xE6FDD1)},
                                              {0.5f, kGo},
                                              {1.0f, hexColor(0x2E6E33)}}))
                     .opacity(&goPulse))
          .child(t("STAGE", bold(9, hexColor(0xE8ECEE))))
          .child(box().grow(1))
          .child(box().row().gap(2).children(std::vector<Element>{
              digitCell("0"), digitCell("0"), digitCell("1")})));
  g.child(std::move(stage));

  // ROLL / YAW linear tapes — a railway-tie track + a driven orange tick.
  auto tapeGauge = [&](const char* label, float py,
                       const ch::Output<float>* drive) {
    return at(
        stack()
            .corners({2})
            .fill(Paint::solid(hexColor(0x1B1F22)))
            .stroke(PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x454C51)),
                               .align = PathFormat::Align::Inner})
            .clip()
            .child(
                box()
                    .inset(0)
                    .shape(keyedShape(std::string_view("gauge-rail"),
                                      [](SkSize s) {
                                        SkPathBuilder b;
                                        b.moveTo(4, s.height() * 0.5f);
                                        b.lineTo(s.width() - 4,
                                                 s.height() * 0.5f);
                                        return b.detach();
                                      }))
                    .stroke(lines::Line{.width = 0.8f,
                                        .fill = Fill::color(hexColor(0x6C767C)),
                                        .tickSpacing = 6.0f,
                                        .tickLength = 9.0f}))
            .child(box()
                       .width(Dimension(9))
                       .height(Dimension(8))
                       .top(Dimension(0))
                       .left(Dimension(48))
                       .shape(shapes::polygon(3, 180))
                       .fill(Paint::solid(kStageTab))
                       .translateX(bind(drive).target(-42, 42)))
            .child(box()
                       .left(Dimension(4))
                       .top(Dimension(1))
                       .child(t(label, bold(8, hexColor(0xC7D0D5))))),
        x + 154, py, 106, 16);
  };
  g.child(tapeGauge("ROLL", 756, &rollTape));
  g.child(tapeGauge("YAW", 778, &yawTape));
  return g;
}

auto KspMapView::digitCell(const char* d) -> Element {
  using namespace ksp;
  return box()
      .width(Dimension(13))
      .height(Dimension(17))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .fill(Paint::linearUnit({0, 0}, {0, 1},
                              {{0.0f, hexColor(0xF2F2F2)}, {1.0f, kStageLcd}}))
      .child(ksp::t(d, lcd(13, hexColor(0x16181A))));
}

auto KspMapView::altimeter() -> Element {
  using namespace ksp;
  const float X = 430, Y = 6, W = 356, H = 82;
  Element g = at(stack().corners({4}).clip(), X, Y, W, H);
  g.fill(Paint::blend({{Paint::linearUnit({0, 0}, {0, 1},
                                          {{0.0f, hexColor(0xA8AFB4)},
                                           {0.45f, hexColor(0x848D93)},
                                           {1.0f, hexColor(0x4E565C)}}),
                        SkBlendMode::kSrcOver},
                       // worn-metal luminance: the ONE place grain belongs here
                       {Paint::recipe(field::grain(0.9f, 2, 9.0f, 0.3f, 1.0f)),
                        SkBlendMode::kSoftLight}}))
      .stroke(PathFormat{.width = 1.2f,
                         .strokeFill = Fill::color(hexColor(0x22282C)),
                         .align = PathFormat::Align::Inner});

  g.child(
      at(box()
             .fill(Paint::solid(hexColor(0xE0B720)))
             .foreground(lines::presets::hatch(
                 Fill::color(hexColor(0x141414, 0.9f)), 8.0f, 4.0f, -45.0f)),
         0, 0, 11, H));

  // odometer wheels
  auto wheel = [&](const char* d, float x, bool red) {
    return at(
        box()
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .fill(Paint::linearUnit(
                {0, 0}, {0, 1},
                red ? std::vector<Stop>{{0.0f, hexColor(0xE05B4A)},
                                        {0.5f, hexColor(0xC0392B)},
                                        {1.0f, hexColor(0x8E2A20)}}
                    : std::vector<Stop>{{0.0f, hexColor(0xFAFAFA)},
                                        {0.42f, hexColor(0xFFFFFF)},
                                        {1.0f, hexColor(0xBFBFBF)}}))
            .stroke(PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x50585E)),
                               .align = PathFormat::Align::Inner})
            .child(
                t(d, lcd(20, red ? hexColor(0xFFFFFF) : hexColor(0x101214)))),
        x, 8, 26, 34);
  };
  static const char* kDigits[6] = {"0", "0", "2", "1", "1", "3"};
  for (int i = 0; i < 6; ++i)
    g.child(wheel(kDigits[i], 18.0f + (float)i * 28.0f, false));
  g.child(wheel("K", 18.0f + 6 * 28.0f, true));

  // ATMOSPHERE tape
  g.child(at(
      stack()
          .fill(Paint::linearUnit({0, 0}, {0, 1},
                                  {{0.0f, hexColor(0x2E6E9E)},
                                   {0.5f, hexColor(0x4E9CC8)},
                                   {1.0f, hexColor(0x1E4E72)}}))
          .stroke(PathFormat{.width = 1.0f,
                             .strokeFill = Fill::color(hexColor(0x18333F)),
                             .align = PathFormat::Align::Inner})
          .clip()
          .child(box()
                     .inset(0)
                     .shape(keyedShape(std::string_view("atmosphere-rail"),
                                       [](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(2, s.height() * 0.62f);
                                         b.lineTo(s.width() - 2,
                                                  s.height() * 0.62f);
                                         return b.detach();
                                       }))
                     .stroke(lines::Line{
                         .width = 0.9f,
                         .fill = Fill::color(hexColor(0xE8F4FA, 0.85f)),
                         .tickSpacing = 5.0f,
                         .tickLength = 12.0f}))
          .child(box()
                     .left(Dimension(6))
                     .top(Dimension(1))
                     .child(t("ATMOSPHERE", bold(8, hexColor(0xEAF4FA), 1.4f))))
          .child(box()
                     .width(Dimension(9))
                     .height(Dimension(8))
                     .left(Dimension(30))
                     .top(Dimension(0))
                     .shape(shapes::polygon(3, 180))
                     .fill(Paint::solid(hexColor(0xFFFFFF)))
                     .translateX(bind(&yawTape).target(0, 190))),
      18, 48, 238, 22));

  // vertical-speed dial
  const SkPoint dc{300, 42};
  Element dial = stack().inset(0);
  dial.child(
      at(box()
             .shape(shapes::circle())
             .fill(Paint::radialUnit({0.4f, 0.32f}, 1.0f,
                                     {{0.0f, hexColor(0xF2F4F5)},
                                      {0.7f, hexColor(0xD3D8DB)},
                                      {1.0f, hexColor(0x9AA2A7)}}))
             .stroke(PathFormat{.width = 1.4f,
                                .strokeFill = Fill::color(hexColor(0x33393E))}),
         dc, 74, 74));
  for (int i = 0; i < 13; ++i)
    dial.child(at(box()
                      .shape(shapes::sector(-1.1f, 2.2f, i % 3 ? 0.82f : 0.72f))
                      .fill(Paint::solid(hexColor(0x3A4046)))
                      .rotate(-125.0f + (float)i * 20.8f),
                  dc, 68, 68));
  dial.child(at(t("VERT", bold(6.5f, hexColor(0x4A5157), 0.6f)),
                {dc.fX + 15, dc.fY - 6}, 26, 9));
  dial.child(at(t("SPD", bold(6.5f, hexColor(0x4A5157), 0.6f)),
                {dc.fX + 15, dc.fY + 3}, 26, 9));
  dial.child(at(t("100", body(6, hexColor(0x5A6167))), {dc.fX - 5, dc.fY - 26},
                20, 8));
  dial.child(at(t("-100", body(6, hexColor(0x5A6167))), {dc.fX - 5, dc.fY + 26},
                22, 8));
  dial.child(at(box()
                    .shape(shapes::sector(-2.2f, 4.4f, 0.0f))
                    .fill(Paint::solid(kGold))
                    .rotate(bind(&gforce).target(-118, 118)),
                dc, 62, 62));
  dial.child(
      at(box().shape(shapes::circle()).fill(Paint::solid(hexColor(0x33393E))),
         dc, 7, 7));
  g.child(std::move(dial));
  return g;
}

auto KspMapView::crewPlate() -> Element {
  using namespace ksp;
  const float X = 986, Y = 594, W = 178, H = 186;
  Element g = at(stack().corners({3}).clip(), X, Y, W, H);
  g.fill(Paint::linearUnit(
             {0, 0}, {0, 1},
             {{0.0f, hexColor(0x7F878C)}, {1.0f, hexColor(0x454D53)}}))
      .stroke(PathFormat{.width = 1.2f,
                         .strokeFill = Fill::color(hexColor(0x22282C)),
                         .align = PathFormat::Align::Inner});
  g.child(at(box().fill(Paint::radialUnit(
                 {0.5f, 0.35f}, 1.1f,
                 {{0.0f, hexColor(0x3E4A52)}, {1.0f, hexColor(0x1A2126)}})),
             5, 5, W - 10, H - 34));
  // helmet
  g.child(at(box()
                 .shape(shapes::circle())
                 .fill(Paint::radialUnit({0.36f, 0.28f}, 1.0f,
                                         {{0.0f, hexColor(0xFFFFFF)},
                                          {0.5f, hexColor(0xD3D8DB)},
                                          {1.0f, hexColor(0x7C858B)}})),
             46, 34, 92, 92));
  // face under the glass: green, because that is the one thing about a
  // kerbal nobody gets wrong
  g.child(at(box()
                 .shape(shapes::circle())
                 .fill(Paint::radialUnit(
                     {0.4f, 0.32f}, 1.0f,
                     {{0.0f, hexColor(0x9FC45C)}, {1.0f, hexColor(0x5F8330)}})),
             60, 48, 64, 64));
  g.child(at(box()
                 .shape(shapes::Circle{.uniform = true})
                 .fill(Paint::solid(hexColor(0xF4F4F0))),
             74, 62, 14, 17));
  g.child(at(box()
                 .shape(shapes::Circle{.uniform = true})
                 .fill(Paint::solid(hexColor(0xF4F4F0))),
             96, 62, 14, 17));
  g.child(at(box()
                 .shape(shapes::Circle{.uniform = true})
                 .fill(Paint::solid(hexColor(0x141414))),
             78, 68, 6, 7));
  g.child(at(box()
                 .shape(shapes::Circle{.uniform = true})
                 .fill(Paint::solid(hexColor(0x141414))),
             100, 68, 6, 7));
  g.child(at(box()
                 .shape(shapes::sector(20, 140, 0.0f))
                 .fill(Paint::solid(hexColor(0x2E3A18))),
             80, 84, 24, 14));
  // the glass itself, over the face
  g.child(
      at(box()
             .shape(shapes::sector(150, 240, 0.0f))
             .fill(Paint::linearUnit({0, 0}, {1, 1},
                                     {{0.0f, hexColor(0xBFE0D8, 0.34f)},
                                      {0.55f, hexColor(0x6E9A94, 0.10f)},
                                      {1.0f, hexColor(0x2E4A46, 0.26f)}}))
             .stroke(PathFormat{.width = 1.4f,
                                .strokeFill = Fill::color(hexColor(0xE8ECEA))}),
         54, 42, 76, 76));
  g.child(at(box()
                 .shape(shapes::blob(3u, 0.18f, 7))
                 .fill(Paint::linearUnit({0, 0}, {1, 1},
                                         {{0.0f, hexColor(0xFFFFFF, 0.42f)},
                                          {1.0f, hexColor(0xFFFFFF, 0.0f)}}))
                 .blend(SkBlendMode::kPlus),
             58, 44, 40, 34));
  // suit shoulders
  g.child(at(box().corners({26}).fill(Paint::linearUnit(
                 {0, 0}, {0, 1},
                 {{0.0f, hexColor(0xE7E8E4)}, {1.0f, hexColor(0x9AA0A2)}})),
             40, 124, 104, 40));
  g.child(at(box()
                 .corners({2})
                 .alignItems(Align::Center)
                 .justify(Justify::Center)
                 .fill(Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0x9AA2A7)}, {1.0f, hexColor(0x666E74)}}))
                 .child(t("Bill Kerman", body(11, hexColor(0x14181A)))),
             5, H - 26, W - 10, 21));
  return g;
}

auto KspMapView::cluster() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);
  g.child(at(box().fill(Paint::linearUnit({0, 0}, {0, 1},
                                          {{0.0f, hexColor(0x0A0C10, 0.30f)},
                                           {0.35f, hexColor(0x0A0C10, 0.62f)},
                                           {1.0f, hexColor(0x0A0C10, 0.74f)}})),
             0, 512, 528, 288));
  g.child(staging());
  g.child(navball());
  return g;
}
