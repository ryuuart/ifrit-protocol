#include "KspMapView.h"

auto KspMapView::infoHead(const Utf8& label) -> Element {
  using namespace ksp;
  // A strip across the card, its word in the class a name INSIDE content
  // is set in. An absent head spends no room.
  if (label.empty()) return box();
  return box()
      .height(19)
      .justify(Justify::Center)
      .padding(0, 0, 0, 8)
      .fill(ksp::kCardStrip)
      .children({kit::section(label)});
}

auto KspMapView::infoCard() -> Element {
  using namespace ksp;
  // The card is set in the sans at 11 px in the card ink, and its three
  // registers are one sheet: a row's name is `captionNote`, the figure that
  // answers it `readout`, a strip head `section`. So every line below says
  // only its words, and the card's words are the document's.
  const data::Json& page = doc["info"];
  const kit::Rows how{.measure = 224, .gap = 7};
  const auto section = [&how, this](const data::Json& part) {
    return box().children(
        {infoHead(part["head"]), kit::readout(readings(part["rows"]), how)});
  };
  return at(
      box()
          .font({.face = sans(), .size = 11})
          .ink(kCardInk)
          .styleSheet(cardLook)
          .fill(kCardBody)
          .clip()
          .padding(0, 0, 0, 8)
          .translateX(animate(from(46.0f).to(0.0f), {380ms, ch::easeOutQuad}))
          .appear({380ms})
          .children(
              {box()
                   .height(26)
                   .justify(Justify::Center)
                   .fill(kOrange)
                   .children({text(page["vessel"])
                                  .font({.face = sansB(),
                                         .size = 14,
                                         .color = hexColor(0xFFFFFF)})}),
               box()
                   .height(19)
                   .justify(Justify::Center)
                   .fill(kCardSub)
                   .children({text(page["tab"])
                                  .font({.face = sansB(),
                                         .color = hexColor(0xE8E8EA)})}),
               infoHead(page["classification"]),
               // the part icon, and the three readings that name the ship
               box()
                   .row()
                   .padding(4, 0, 4, 0)
                   .gap(8)
                   .children({box()
                                  .width(34)
                                  .height(40)
                                  .shape(shapes::polygon(7, 12))
                                  .fill(Paint::linearUnit(
                                      {0, 0}, {1, 1},
                                      {{0.0f, hexColor(0xF7F7F8)},
                                       {1.0f, hexColor(0xB9BCC1)}}))
                                  .stroke(PathFormat{.width = 1.0f,
                                                     .strokeFill = Fill::color(
                                                         hexColor(0x8A8E93))}),
                              kit::readout(readings(page["ship"]),
                                           {.measure = 176, .gap = 7})
                                  .grow(1)}),
               each(page["sections"].items(), section), box().grow(1),
               box().height(6).fill(hexColor(0x9DA1A6))}),
      906, 40, 240, 318);
}

auto KspMapView::toolbar() -> Element {
  using namespace ksp;
  // Six buttons down the right edge, one per glyph the document names.
  return stack().inset(0).staggerChildren(45ms).children(
      {each(doc["toolbar"].items(), [](const data::Json& glyph, std::size_t i) {
        return at(
            kit::centred()
                .corners({5})
                .fill(Paint::linearUnit({0, 0}, {0, 1},
                                        {{0.0f, mskia::lighten(kGun, 0.10f)},
                                         {1.0f, hexColor(0x3E4750)}}))
                .stroke(
                    PathFormat{.width = 1.0f,
                               .strokeFill = Fill::color(hexColor(0x22282D)),
                               .align = PathFormat::Align::Inner})

                .appear({260ms})
                .scale(animate(from(0.7f).to(1.0f), {320ms, ease::outBack()}))
                .children({text(glyph, body(13, hexColor(0xD3DBE0)))}),
            1156, 34.0f + (float)i * 46.0f, 38, 38);
      })});
}

auto KspMapView::missionClock() -> Element {
  using namespace ksp;
  Element g = stack().inset(0);
  g.children(
      {at(kit::centred()
              .corners({4})

              .fill(Paint::solid(hexColor(0x26282C, 0.94f)))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(hexColor(0x4A5157))})
              .children({text(doc["clock"], lcd(13, kLcd)).key("met")}),
          18, 14, 200, 28),
       at(kit::centred()
              .corners({4})

              .fill(Paint::linearUnit({0, 0}, {0, 1},
                                      {{0.0f, mskia::lighten(kGun, 0.12f)},
                                       {1.0f, hexColor(0x3E4750)}}))
              .children({t("MET", bold(11, hexColor(0xE6EAEC)))}),
          222, 14, 40, 28)});
  static const char* kIcons[5] = {"◉", "▮▮", "▼", "◍", "◈"};
  g.children({each(kIcons, [](const char* icon, std::size_t i) {
    return at(kit::centred()
                  .corners({3})

                  .fill(hexColor(0x474F57))
                  .children({t(icon, body(10, hexColor(0x8CE07A)))}),
              272 + (float)i * 28, 16, 24, 24);
  })});
  return g;
}

auto KspMapView::chip(const char* glyph, const char* label, SkPoint p,
                      SkColor4f ink, float r) -> Element {
  using namespace ksp;
  Element g = stack();
  g.children(
      {at(kit::centred()
              .shape(shapes::circle())
              .fill(Paint::radialUnit({0.38f, 0.30f}, 1.0f,
                                      {{0.0f, hexColor(0xB8C0C6)},
                                       {0.55f, hexColor(0x66707A)},
                                       {1.0f, hexColor(0x2C3238)}}))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(hexColor(0x161A1E))})

              .children({t(glyph, body(r * 0.9f, hexColor(0x0F1316)))}),
          p, r * 2, r * 2)});
  if (label[0])
    g.children(
        {at(t(label, bold(9, ink, 0.5f)), {p.fX + r + 12, p.fY - 8}, 34, 12)});
  return g;
}
