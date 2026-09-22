#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::specPair(const sigil::data::Json& spec) -> Element {
  using namespace tav;
  return box().column().gap(2).children(
      {t(spec["name"],
         micro(9, sigil::material::withAlpha(hexColor(0x123B3D), 0.75f), 260)),
       kit::line(
           {.fill = Fill::color(sigil::material::withAlpha(kDate, 0.28f))}),
       t(spec["value"], cut(blackFace(), 11, hexColor(0x0E3234), 40, 0.92f))});
}

auto TwoAdvancedV4::featureSystem() -> Element {
  using namespace tav;
  const sigil::data::Json& feature = doc()["feature"];
  Element thumb =
      box()
          .width(150)
          .height(150)
          .flexShrink(0)
          .shape(shapes::chamfered(12, shapes::Corner::Diagonal))
          .fill(mskia::Paint::linearUnit(
              {0, 0}, {0, 1},
              {{0.0f, hexColor(0x06232A)}, {1.0f, hexColor(0x011114)}}))
          .stroke(stroke(
              1,
              Fill::color(sigil::material::withAlpha(hexColor(0x0B3B40), 0.9f)),
              PathFormat::Align::Inner))
          .children({box().inset(0).fill(mskia::Paint::radialUnit(
                         {0.5f, 0.72f}, 0.95f,
                         {{0.0f, sigil::material::withAlpha(kGlow, 0.8f)},
                          {0.5f, sigil::material::withAlpha(kTealBar, 0.28f)},
                          {1.0f, sigil::material::withAlpha(kTealBar, 0.0f)}})),
                     at(box().fill(hexColor(0x010A0C)), 18, 74, 30, 60),
                     at(box().fill(hexColor(0x02171B)), 52, 46, 44, 88),
                     at(box().fill(hexColor(0x010A0C)), 100, 62, 34, 72),
                     at(box().fill(sigil::material::withAlpha(kGlow, 0.6f)), 0,
                        108, 150, 1)})
          .foreground(styles::Brackets{sigil::material::withAlpha(kCyan, 0.85f),
                                       12, 2, 4, shapes::Corner::All})
          .foreground(styles::Scanlines{{0, 0, 0, 0.22f}, 3, 1});

  Element copy = box().flexGrow(1).column().gap(6).children(
      {box()
           .row()
           .gap(7)
           .alignItems(Align::Center)
           .children(
               {box()
                    .width(9)
                    .height(9)
                    .shape(shapes::polygon(3, 90))
                    .fill(kDate),
                t(feature["date"], cut(blackFace(), 14, kDate, 40, 0.95f)),
                kit::line({.fill = Fill::color(
                               sigil::material::withAlpha(kDate, 0.35f))})
                    .flexGrow(1)}),
       t(feature["headline"],
         cut(blackFace(), 17, hexColor(0x0E3234), 40, 0.92f)),
       box()
           .height(84)
           .padding(9)
           .fill(dither.material())
           .foreground(stroke(
               1, Fill::color(sigil::material::withAlpha(kPanelSh, 0.9f)),
               PathFormat::Align::Inner))
           // the ONE place this interface is not tracked caps
           .children({t(feature["body"], prose(13, hexColor(0x0B2C2E)))}),
       // the related-work strip: four chamfered stills over the
       // dither ground, the way the FEATURE panel filled its slack
       box()
           .flexGrow(1)
           .row()
           .gap(7)
           .alignItems(Align::Stretch)
           .children(relatedStills()),
       // the spec readout: dense, tabular, and never actually read
       box().row().gap(14).children({each(feature["specs"].items(), specPair)}),
       box()
           .row()
           .gap(8)
           .alignItems(Align::Center)
           .children(
               {t("› VIEW CASE STUDY", micro(11, hexColor(0x123B3D), 220)),
                box().flexGrow(1),
                box()
                    .width(150)
                    .height(6)
                    .fill(sigil::material::withAlpha(kPanelSh, 0.7f))
                    .children({box().left(0).top(0).width(112).height(6).fill(
                        hexColor(0x0E3234))}),
                t("74%", micro(10, hexColor(0x123B3D), 160)),
                box().width(126)})});

  Element leftCol = box().width(150).flexShrink(0).column().gap(8).children(
      {thumb,
       box()
           .flexGrow(1)
           .column()
           .gap(4)
           .padding(8)
           .fill(dither.material())
           .foreground(stroke(
               1, Fill::color(sigil::material::withAlpha(kPanelSh, 0.9f)),
               PathFormat::Align::Inner))
           .children(
               {t("CREDITS",
                  micro(9, sigil::material::withAlpha(hexColor(0x123B3D), 0.8f),
                        260)),
                kit::line({.fill = Fill::color(
                               sigil::material::withAlpha(kDate, 0.28f))}),
                t("DIRECTION", micro(9, kDate, 200)),
                t("ERIC JORDAN",
                  cut(blackFace(), 11, hexColor(0x0E3234), 40, 0.92f)),
                box().height(3), t("STUDIO", micro(9, kDate, 200)),
                t("2ADVANCED",
                  cut(blackFace(), 11, hexColor(0x0E3234), 40, 0.92f)),
                box().flexGrow(1),
                box()
                    .row()
                    .gap(4)
                    .alignItems(Align::Center)
                    .children({box().width(7).height(7).fill(
                                   sigil::material::withAlpha(kDate, 0.8f)),
                               t("ARCHIVED", micro(9, kDate, 200))})})});

  Element bodyArea =
      monitorBody(316).row().padding(11).gap(11).children({leftCol, copy});
  // the hazard wedge, bottom-left — the STATIC baked-tile pattern path
  bodyArea.children({at(box()
                            .shape(keyedShape(std::string_view("hazard-wedge"),
                                              [](SkSize s) {
                                                SkPathBuilder b;
                                                b.moveTo(0, 0);
                                                b.lineTo(s.width(), s.height());
                                                b.lineTo(0, s.height());
                                                b.close();
                                                return b.detach();
                                              }))
                            .fill(hazard.material())
                            .opacity(0.45f),
                        0, 316 - 46, 150, 46),
                     box()
                         .left(694 - 11 - 116)
                         .top(316 - 11 - 34)
                         .children({cta("LAUNCH", 116, 34, kPanelSh)})});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("feature")
      .gridArea("feature")
      .translateX(animate(motion::from(90.0f).to(0.0f),
                          {500ms, &ch::easeOutQuint, 2600ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 2600ms}))
      .children({panelHeader("FEATURE", " SYSTEM", "LATEST TRANSMISSION", 1),
                 bodyArea});
  return panel;
}

auto TwoAdvancedV4::pressList() -> Element {
  using namespace tav;
  // ONE ENTRY PER RECORD IN THE WIRE: the date on its own plate with the
  // rule that fills the rest of it, the headline under that, the body
  // under that.
  const auto entry = [](const sigil::data::Json& e) {
    return box().column().gap(4).children(
        {box()
             .row()
             .gap(7)
             .alignItems(Align::Center)
             .fill(sigil::material::withAlpha(kPanelSh, 0.55f))
             .padding(3, 6)
             .children(
                 {t(e["date"], cut(blackFace(), 13, kDate, 40, 0.95f)),
                  kit::line({.fill = Fill::color(
                                 sigil::material::withAlpha(kDate, 0.3f))})
                      .flexGrow(1),
                  t("▸", micro(9, kDate, 0))}),
         t(e["headline"], cut(blackFace(), 13, hexColor(0x0E3234), 50, 0.92f)),
         t(e["body"], prose(12.5f, hexColor(0x0C2E30)))});
  };
  return box().column().gap(9).children({each(doc()["press"].items(), entry)});
}

auto TwoAdvancedV4::pressUpdates() -> Element {
  using namespace tav;
  Element list = pressList().translateY(&pressScroll);

  // The thumb is the well's share of the entries it scrolls, and it
  // rides the same Output the list does: a bar drawn at a guessed
  // length beside a list scrolled over a measured one says the wrong
  // thing twice, about how much is below and about where the reader is.
  auto stepper = [&](bool up) {
    return kit::centred()
        .width(16)
        .height(16)
        .fill(kPanelSh)

        .children({t(up ? "▴" : "▾", micro(8, kBody, 0))});
  };
  const sketch::kit::Scrolled well = pressScrolled();
  Element scrollbar =
      sketch::kit::scrollbar(
          {.leading = stepper(true),
           .trailing = stepper(false),
           .thumb =
               box()
                   .fill(mskia::Paint::linearUnit(
                       {0, 0}, {1, 0},
                       {{0.0f, hexColor(0xCFEFEC)}, {1.0f, kPanelHi}}))
                   .stroke(stroke(
                       1, Fill::color(sigil::material::withAlpha(kDate, 0.4f)),
                       PathFormat::Align::Inner)),
           .scrolled = well,
           // The list's own scroll, in the thumb's units. A list that
           // fits has no travel and the target collapses to nothing;
           // the divisor is only there to stay finite.
           .position = motion::bind(&pressScroll)
                           .source(0.0f, -std::max(pressOverflow, 1.0f))
                           .target(0.0f, well.thumb().travel),
           .thumbInset = 2,
           .track = Fill::color(sigil::material::withAlpha(kPanelSh, 0.6f))})
          .width(16)
          .gap(3);

  Element bodyArea = monitorBody(376).column().padding(11).gap(9).children(
      {box().flexGrow(1).row().gap(8).children(
           {box()
                .flexGrow(1)
                .overflow(Overflow::Clip)
                .padding(9)
                .fill(dither.material())
                .foreground(stroke(
                    1, Fill::color(sigil::material::withAlpha(kPanelSh, 0.9f)),
                    PathFormat::Align::Inner))
                .children({list}),
            scrollbar}),
       box()
           .row()
           .alignItems(Align::Center)
           .gap(8)
           .children(
               {t("06 ENTRIES · PAGE 1/4", micro(11, hexColor(0x123B3D), 220)),
                box().flexGrow(1), cta("ARCHIVES", 116, 34, kPanelSh)})});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("press")
      .gridArea("press")
      .translateY(animate(motion::from(60.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 3250ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3250ms}))
      .children({panelHeader("PRESS", " UPDATES", "STUDIO WIRE", 2), bodyArea});
  return panel;
}

auto TwoAdvancedV4::auxBar(const Utf8& label) -> Element {
  using namespace tav;
  return box()
      .height(18)
      .row()
      .alignItems(Align::Center)
      .padding(0, 6)
      .gap(6)
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, hexColor(0x5A1A20)}, {1.0f, hexColor(0x2E0A0C)}}))
      .children({t("»", micro(10, kCyan, 0)),
                 text(label).font(micro(11, kNear, 160))});
}

auto TwoAdvancedV4::auxView() -> Element {
  using namespace tav;
  return kit::centred()
      .height(17)
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1}, {{0.0f, kPanelHi}, {0.5f, kPanel}, {1.0f, kPanelSh}}))
      .stroke(stroke(
          1, Fill::color(sigil::material::withAlpha(hexColor(0xCFEFEC), 0.6f)),
          PathFormat::Align::Inner))

      .children({t("VIEW", label(11, kDate, 200))});
}

auto TwoAdvancedV4::auxiliary() -> Element {
  using namespace tav;
  const sigil::material::Color kCopy =
      hexColor(0x7FD4D0);  // the module copy teal
  const sigil::data::Json& aux = doc()["aux"];

  // Column 1: one row per record in the document — the glyph on its own
  // rounded plate, two lines of copy, and the link ranged at the far edge
  // of the second.
  const auto module = [kCopy](const sigil::data::Json& it) {
    return box()
        .row()
        .gap(8)
        .alignItems(Align::Center)
        .children(
            {kit::centred()
                 .width(26)
                 .height(26)
                 .flexShrink(0)
                 .borderRadius({4})
                 .fill(mskia::Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0x8E2A2A)}, {1.0f, hexColor(0x3A0C0E)}}))
                 .stroke(stroke(
                     1, Fill::color(sigil::material::withAlpha(kNear, 0.4f)),
                     PathFormat::Align::Inner))

                 .children({t(it["glyph"], micro(11, kPanelHi, 0))}),
             box().flexGrow(1).column().children(
                 {t(it["first"], prose(11.5f, kCopy)),
                  box().row().children(
                      {t(it["second"], prose(11.5f, kCopy)), box().flexGrow(1),
                       t(it["link"],
                         micro(9, sigil::material::withAlpha(kNear, 0.85f),
                               160))})})});
  };
  Element supplementals =
      box().flexGrow(1).flexBasis(0).column().gap(3).children(
          {auxBar("SUPPLEMENTALS & ESSENTIALS"),
           each(aux["items"].items(), module)});

  // Column 2: the book plate is white — the one white rectangle on the
  // whole page — with the title set dark on it.
  const sigil::data::Json& book = aux["book"];
  Element photoshop = box().flexGrow(1).flexBasis(0).column().gap(4).children(
      {auxBar(book["bar"].text()),
       box().row().gap(8).flexGrow(1).children(
           {box()
                .width(118)
                .flexShrink(0)
                .fill(hexColor(0xF2F0EA))
                .column()
                .padding(6, 7)
                .gap(2)
                .children(
                    {t(book["title"], cut(arial(), 15, hexColor(0x2A4A7A), 0)),
                     t(book["subtitle"],
                       cut(arial(), 10, hexColor(0x333333), 0))}),
            t(book["body"], prose(11.5f, kCopy))}),
       auxView()});

  // Column 3: the 2ADVANCED.NET plate — its angular mark is the only
  // amber on the interface.
  const sigil::data::Json& wire = aux["wire"];
  Element press = box().flexGrow(1).flexBasis(0).column().gap(4).children(
      {auxBar(wire["bar"].text()),
       box()
           .height(40)
           .row()
           .alignItems(Align::Center)
           .padding(0, 8)
           .gap(7)
           .fill(mskia::Paint::linearUnit(
               {0, 0}, {0, 1},
               {{0.0f, hexColor(0x2A0A0C)}, {1.0f, hexColor(0x140404)}}))
           .stroke(stroke(1,
                          Fill::color(sigil::material::withAlpha(kDust, 0.4f)),
                          PathFormat::Align::Inner))
           .children({box()
                          .width(20)
                          .height(20)
                          .shape(shapes::chamfered(6, shapes::Corner::Diagonal))
                          .fill(mskia::Paint::linearUnit(
                              {0, 0}, {0, 1},
                              {{0.0f, hexColor(0xE8A83C)},
                               {1.0f, hexColor(0x9A5E10)}})),
                      box().column().gap(1).children(
                          {t(wire["name"], heavy(13, hexColor(0xD9DDE0), 60)),
                           t(wire["tagline"], micro(8, kDust, 220))})}),
       t(wire["body"], prose(11.5f, kCopy)), box().flexGrow(1), auxView()});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("aux")
      .gridArea("aux")
      .translateY(animate(motion::from(56.0f).to(0.0f),
                          {400ms, &ch::easeOutQuint, 3100ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3100ms}))
      .children({panelHeader("AUXILIARY", " PANEL",
                             "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 3),
                 box()
                     .flexGrow(1)
                     .row()
                     .gap(10)
                     .padding(6, 8)
                     .fill(hexColor(0x300B0E))
                     .children({supplementals, photoshop, press})});
  return panel;
}

auto TwoAdvancedV4::subSystem() -> Element {
  using namespace tav;
  const sigil::data::Json& sub = doc()["subsystem"];
  // A PARTNER CHIP: one letter on a round plate.
  const auto chip = [](const char* glyph) {
    return kit::centred()
        .width(40)
        .height(40)
        .borderRadius({20})
        .fill(mskia::Paint::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, hexColor(0x6A1B21)}, {1.0f, hexColor(0x220608)}}))
        .stroke(stroke(1, Fill::color(sigil::material::withAlpha(kDust, 0.5f)),
                       PathFormat::Align::Inner))

        .children({t(glyph, heavy(15, kCyan, 0))});
  };
  // A BANK: its quiet name over whatever answers it. Every station along
  // this row is one, which is why the row is a run rather than a layout.
  const auto bank = [](const Utf8& name, Element under) {
    return box().column().gap(3).children(
        {text(name).font(micro(10, kDustDim, 240)), std::move(under)});
  };
  // A SELECTOR: the dark chamfered well, then the name over the value the
  // document carries, dressed as a drop-down.
  const auto selector = [&bank](const sigil::data::Json& one) {
    return box()
        .row()
        .gap(8)
        .alignItems(Align::Center)
        .children(
            {box()
                 .width(46)
                 .height(34)
                 .shape(shapes::chamfered(8, shapes::Corner::Diagonal))
                 .fill(mskia::Paint::radialUnit(
                     {0.5f, 0.76f}, 1.1f,
                     {{0.0f, hexColor(0x0A4148)}, {1.0f, hexColor(0x010D10)}}))
                 .stroke(stroke(
                     1, Fill::color(sigil::material::withAlpha(kCyan, 0.5f)),
                     PathFormat::Align::Inner)),
             bank(one["name"].text(),
                  box()
                      .row()
                      .gap(5)
                      .alignItems(Align::Center)
                      .children({t("▸", micro(9, kCyan, 0)),
                                 t(one["value"], label(13, kNear, 90)),
                                 t("▾", micro(9, kDust, 0))}))});
  };
  const auto keys = [](std::vector<Element> on) {
    return box().row().gap(4).alignItems(Align::Center).children(std::move(on));
  };

  // THE STATIONS, in order. The hairline that stands between every pair is
  // written once, below.
  std::vector<Element> stations;
  stations.push_back(box()
                         .row()
                         .gap(8)
                         .alignItems(Align::Center)
                         .children({t("PARTNERS:", micro(11, kDust, 240)),
                                    chip("A"), chip("M")}));
  for (const sigil::data::Json& one : sub["selectors"].items())
    stations.push_back(selector(one));
  stations.push_back(
      bank("SOUND", keys({toggle("ON", true), toggle("OFF", false)})));
  stations.push_back(
      bank("QUALITY", keys({toggle("LOW", false), toggle("MED", false),
                            toggle("HIGH", true)})));
  stations.push_back(
      bank("RESOLUTION", t(sub["resolution"], label(13, kNear, 90))));

  std::vector<Element> run;
  for (Element& station : stations) {
    run.push_back(kit::line(
        {.length = Dimension(30),
         .column = true,
         .fill = Fill::color(sigil::material::withAlpha(kDust, 0.35f))}));
    run.push_back(std::move(station));
  }

  Element row =
      bevelPanel(box()
                     .row()
                     .alignItems(Align::Center)
                     .padding(0, 14)
                     .gap(18),
                 hexColor(0x2E0B0D));
  row.key("subsys")
      .gridArea("subsys")
      .background(
          styles::Overlay{hazard.material(), SkBlendMode::kSrcOver, 0.16f})
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {400ms, &ch::easeOutQuad, 3650ms}))
      .foreground(styles::TickRail{sigil::material::withAlpha(kDust, 0.35f), 9,
                                   4, 8, 1, 4, 0.5f, path::Edge::Top})
      .children(
          {t("SUB", heavy(15, kNear, 40)),
           t("SYSTEM", cut(arial(), 14, kHeadDim, 40, 0.95f)), std::move(run),
           box().flexGrow(1),
           box()
               .column()
               .alignItems(Align::End)
               .gap(3)
               .children(
                   {t(sub["bandwidth"],
                      micro(11, sigil::material::withAlpha(kCyan, 0.85f), 200)),
                    t(sub["uptime"], micro(10, kDustDim, 200))}),
           box().width(70).height(40).foreground(
               styles::TickRail{sigil::material::withAlpha(kCyan, 0.45f), 6, 4,
                                10, 1, 3, 0.5f, path::Edge::Bottom})});
  return row;
}
