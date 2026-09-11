#include "KspMapView.h"

auto KspMapView::infoRow(const char* label, const char* value) -> Element {
  using namespace ksp;
  return box()
      .row()
      .height(Dim(19))
      .alignItems(Align::Center)
      .padding(0, 0, 0, 8)
      .child(t(label, body(11, kCardInk)))
      .child(box().grow(1))
      .child(t(value, bold(11, kOrange)));
}

auto KspMapView::infoHead(const char* label) -> Element {
  using namespace ksp;
  return box()
      .height(Dim(19))
      .justify(Justify::Center)
      .padding(0, 0, 0, 8)
      .fill(Paint::solid(ksp::kCardStrip))
      .child(t(label, bold(11, kOrange, 0.2f)));
}

auto KspMapView::infoCard() -> Element {
  using namespace ksp;
  return at(
      box()
          .column()
          .fill(Paint::solid(kCardBody))
          .clip()
          .translateX(animate(from(46.0f).to(0.0f), {380ms, ch::easeOutQuad}))
          .opacity(animate(from(0.0f).to(1.0f), {380ms}))
          .child(box()
                     .height(Dim(26))
                     .justify(Justify::Center)
                     .padding(0, 0, 0, 9)
                     .fill(Paint::solid(kOrange))
                     .child(t("Kerbal X", bold(14, hexColor(0xFFFFFF)))))
          .child(box()
                     .height(Dim(19))
                     .justify(Justify::Center)
                     .padding(0, 0, 0, 9)
                     .fill(Paint::solid(kCardSub))
                     .child(t("Info", bold(11, hexColor(0xE8E8EA)))))
          .child(infoHead("Vessel classification"))
          .child(box()
                     .row()
                     .padding(4, 0, 4, 8)
                     .gap(8)
                     .child(box()
                                .width(Dim(34))
                                .height(Dim(40))
                                .shape(shapes::polygon(7, 12))
                                .fill(Paint::linearUnit(
                                    {0, 0}, {1, 1},
                                    {{0.0f, hexColor(0xF7F7F8)},
                                     {1.0f, hexColor(0xB9BCC1)}}))
                                .stroke(PathFormat{.width = 1.0f,
                                                   .strokeFill = Fill::color(
                                                       hexColor(0x8A8E93))}))
                     .child(box()
                                .column()
                                .grow(1)
                                .padding(2, 0, 0, 0)
                                .child(infoRow("Ship:", "Rocket"))
                                .child(infoRow("Partcount:", "71"))
                                .child(infoRow("Total mass:", "130.54 t"))))
          .child(infoRow("Sphere of influence", "Kerbin"))
          .child(infoRow("Situation", "ORBITING"))
          .child(infoRow("Flight time", "T+ 00:05:10"))
          .child(infoHead("Orbital Characteristics"))
          .child(infoRow("Velocity", "2276.9 m/s"))
          .child(infoRow("Altitude", "90,834 m"))
          .child(infoRow("Apoapsis", "213,904 m"))
          .child(infoRow("Periapsis", "88,012 m"))
          .child(infoRow("Inclination", "6.4 °"))
          .child(infoHead("Craft Stats"))
          .child(infoRow("Max. Acceleration", "21.4 m/s²"))
          .child(infoRow("Thrust / Weight", "1.63"))
          .child(box().grow(1))
          .child(box().height(Dim(6)).fill(Paint::solid(hexColor(0x9DA1A6)))),
      906, 40, 240, 318);
}

auto KspMapView::toolbar() -> Element {
  using namespace ksp;
  Element g = stack().inset(0).staggerChildren(45ms);
  static const char* kGlyphs[6] = {"◉", "◆", "▤", "✱", "▲", "◍"};
  for (int i = 0; i < 6; ++i) {
    const float y = 34.0f + (float)i * 46.0f;
    g.child(
        at(box()
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
               .child(t(kGlyphs[i], body(13, hexColor(0xD3DBE0)))),
           1156, y, 38, 38));
  }
  return g;
}

auto KspMapView::missionClock() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);
  g.child(
      at(box()
             .corners({4})
             .alignItems(Align::Center)
             .justify(Justify::Center)
             .fill(Paint::solid(hexColor(0x26282C, 0.94f)))
             .stroke(PathFormat{.width = 1.0f,
                                .strokeFill = Fill::color(hexColor(0x4A5157))})
             .child(t("T+ 0y, 0d, 00:05:10", lcd(13, kLcd)).key("met")),
         18, 14, 200, 28));
  g.child(at(box()
                 .corners({4})
                 .alignItems(Align::Center)
                 .justify(Justify::Center)
                 .fill(Paint::linearUnit({0, 0}, {0, 1},
                                         {{0.0f, mskia::lighten(kGun, 0.12f)},
                                          {1.0f, hexColor(0x3E4750)}}))
                 .child(t("MET", bold(11, hexColor(0xE6EAEC)))),
             222, 14, 40, 28));
  static const char* kIcons[5] = {"◉", "▮▮", "▼", "◍", "◈"};
  for (int i = 0; i < 5; ++i)
    g.child(at(box()
                   .corners({3})
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .fill(Paint::solid(hexColor(0x474F57)))
                   .child(t(kIcons[i], body(10, hexColor(0x8CE07A)))),
               272 + (float)i * 28, 16, 24, 24));
  return g;
}

auto KspMapView::chip(const char* glyph, const char* label, SkPoint p,
                      SkColor4f ink, float r) -> Element {
  using namespace ksp;
  Element g = stack();
  g.child(
      at(box()
             .shape(shapes::circle())
             .fill(Paint::radialUnit({0.38f, 0.30f}, 1.0f,
                                     {{0.0f, hexColor(0xB8C0C6)},
                                      {0.55f, hexColor(0x66707A)},
                                      {1.0f, hexColor(0x2C3238)}}))
             .stroke(PathFormat{.width = 1.0f,
                                .strokeFill = Fill::color(hexColor(0x161A1E))})
             .alignItems(Align::Center)
             .justify(Justify::Center)
             .child(t(glyph, body(r * 0.9f, hexColor(0x0F1316)))),
         p, r * 2, r * 2));
  if (label[0])
    g.child(
        at(t(label, bold(9, ink, 0.5f)), {p.fX + r + 12, p.fY - 8}, 34, 12));
  return g;
}
