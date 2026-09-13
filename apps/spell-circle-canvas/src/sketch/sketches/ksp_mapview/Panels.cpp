#include "KspMapView.h"

auto KspMapView::infoRow(const char* label, const char* value) -> Element {
  using namespace ksp;
  return box()
      .row()
      .height(Dimension(19))
      .alignItems(Align::Center)
      .padding(0, 0, 0, 8)
      .children({text(label), box().grow(1),
                 t(value, {.face = sansB(), .color = kOrange})});
}

auto KspMapView::infoHead(const char* label) -> Element {
  using namespace ksp;
  return box()
      .height(Dimension(19))
      .justify(Justify::Center)
      .padding(0, 0, 0, 8)
      .fill(Paint::solid(ksp::kCardStrip))
      .children({t(label, {.face = sansB(), .color = kOrange, .track = 0.2f})});
}

auto KspMapView::infoCard() -> Element {
  using namespace ksp;
  // The card is set in the sans at 11 px in the card ink: a row's label
  // says nothing, a figure and a head name the bold cut and the orange.
  return at(
      box()
          .column()
          .font({.face = sans(), .size = 11})
          .ink(kCardInk)
          .fill(Paint::solid(kCardBody))
          .clip()
          .translateX(animate(from(46.0f).to(0.0f), {380ms, ch::easeOutQuad}))
          .opacity(animate(from(0.0f).to(1.0f), {380ms}))
          .children(
              {box()
                   .height(Dimension(26))
                   .justify(Justify::Center)
                   .padding(0, 0, 0, 9)
                   .fill(Paint::solid(kOrange))
                   .children({t("Kerbal X", {.face = sansB(),
                                             .size = 14,
                                             .color = hexColor(0xFFFFFF)})}),
               box()
                   .height(Dimension(19))
                   .justify(Justify::Center)
                   .padding(0, 0, 0, 9)
                   .fill(Paint::solid(kCardSub))
                   .children({t("Info", {.face = sansB(),
                                         .color = hexColor(0xE8E8EA)})}),
               infoHead("Vessel classification"),
               box()
                   .row()
                   .padding(4, 0, 4, 8)
                   .gap(8)
                   .children({box()
                                  .width(Dimension(34))
                                  .height(Dimension(40))
                                  .shape(shapes::polygon(7, 12))
                                  .fill(Paint::linearUnit(
                                      {0, 0}, {1, 1},
                                      {{0.0f, hexColor(0xF7F7F8)},
                                       {1.0f, hexColor(0xB9BCC1)}}))
                                  .stroke(PathFormat{.width = 1.0f,
                                                     .strokeFill = Fill::color(
                                                         hexColor(0x8A8E93))})})
                   .children(
                       {box()
                            .column()
                            .grow(1)
                            .padding(2, 0, 0, 0)
                            .children({infoRow("Ship:", "Rocket")})
                            .children({infoRow("Partcount:", "71")})
                            .children({infoRow("Total mass:", "130.54 t")})}),
               infoRow("Sphere of influence", "Kerbin"),
               infoRow("Situation", "ORBITING"),
               infoRow("Flight time", "T+ 00:05:10"),
               infoHead("Orbital Characteristics"),
               infoRow("Velocity", "2276.9 m/s"),
               infoRow("Altitude", "90,834 m"),
               infoRow("Apoapsis", "213,904 m"),
               infoRow("Periapsis", "88,012 m"),
               infoRow("Inclination", "6.4 °"), infoHead("Craft Stats"),
               infoRow("Max. Acceleration", "21.4 m/s²"),
               infoRow("Thrust / Weight", "1.63"), box().grow(1),
               box()
                   .height(Dimension(6))
                   .fill(Paint::solid(hexColor(0x9DA1A6)))}),
      906, 40, 240, 318);
}

auto KspMapView::toolbar() -> Element {
  using namespace ksp;
  Element g = stack().inset(0).staggerChildren(45ms);
  static const char* kGlyphs[6] = {"◉", "◆", "▤", "✱", "▲", "◍"};
  for (int i = 0; i < 6; ++i) {
    const float y = 34.0f + (float)i * 46.0f;
    g.children({at(
        box()
            .corners({5})
            .fill(Paint::linearUnit({0, 0}, {0, 1},
                                    {{0.0f, mskia::lighten(kGun, 0.10f)},
                                     {1.0f, hexColor(0x3E4750)}}))
            .stroke(PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x22282D)),
                               .align = PathFormat::Align::Inner})
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .opacity(animate(from(0.0f).to(1.0f), {260ms}))
            .scale(animate(from(0.7f).to(1.0f), {320ms, ease::outBack()}))
            .children({t(kGlyphs[i], body(13, hexColor(0xD3DBE0)))}),
        1156, y, 38, 38)});
  }
  return g;
}

auto KspMapView::missionClock() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);
  g.children(
      {at(box()
              .corners({4})
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .fill(Paint::solid(hexColor(0x26282C, 0.94f)))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(hexColor(0x4A5157))})
              .children({t("T+ 0y, 0d, 00:05:10", lcd(13, kLcd)).key("met")}),
          18, 14, 200, 28)});
  g.children(
      {at(box()
              .corners({4})
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .fill(Paint::linearUnit({0, 0}, {0, 1},
                                      {{0.0f, mskia::lighten(kGun, 0.12f)},
                                       {1.0f, hexColor(0x3E4750)}}))
              .children({t("MET", bold(11, hexColor(0xE6EAEC)))}),
          222, 14, 40, 28)});
  static const char* kIcons[5] = {"◉", "▮▮", "▼", "◍", "◈"};
  for (int i = 0; i < 5; ++i)
    g.children({at(box()
                       .corners({3})
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .fill(Paint::solid(hexColor(0x474F57)))
                       .children({t(kIcons[i], body(10, hexColor(0x8CE07A)))}),
                   272 + (float)i * 28, 16, 24, 24)});
  return g;
}

auto KspMapView::chip(const char* glyph, const char* label, SkPoint p,
                      SkColor4f ink, float r) -> Element {
  using namespace ksp;
  Element g = stack();
  g.children(
      {at(box()
              .shape(shapes::circle())
              .fill(Paint::radialUnit({0.38f, 0.30f}, 1.0f,
                                      {{0.0f, hexColor(0xB8C0C6)},
                                       {0.55f, hexColor(0x66707A)},
                                       {1.0f, hexColor(0x2C3238)}}))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(hexColor(0x161A1E))})
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .children({t(glyph, body(r * 0.9f, hexColor(0x0F1316)))}),
          p, r * 2, r * 2)});
  if (label[0])
    g.children(
        {at(t(label, bold(9, ink, 0.5f)), {p.fX + r + 12, p.fY - 8}, 34, 12)});
  return g;
}
