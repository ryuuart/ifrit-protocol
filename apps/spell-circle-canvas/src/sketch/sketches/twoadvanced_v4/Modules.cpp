#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::specPair(const char* k, const char* v) -> Element {
  using namespace tav;
  return box().column().gap(2).children(
      {t(k, micro(9, mskia::withAlpha(hexColor(0x123B3D), 0.75f), 260)),
       box().height(1).fill(mskia::withAlpha(kDate, 0.28f)),
       t(v, sigil::weave::kit::tracked(blackFace(), 11, hexColor(0x0E3234), 40,
                                       0.92f))});
}

auto TwoAdvancedV4::featureSystem() -> Element {
  using namespace tav;
  Element thumb =
      box()
          .width(150)
          .height(150)
          .shrink(0)
          .shape(shapes::chamfered(12, shapes::Corner::Diagonal))
          .fill(mskia::Paint::linearUnit(
              {0, 0}, {0, 1},
              {{0.0f, hexColor(0x06232A)}, {1.0f, hexColor(0x011114)}}))
          .stroke(
              stroke(1, Fill::color(mskia::withAlpha(hexColor(0x0B3B40), 0.9f)),
                     PathFormat::Align::Inner))
          .children(
              {box().inset(0).fill(mskia::Paint::radialUnit(
                   {0.5f, 0.72f}, 0.95f,
                   {{0.0f, mskia::withAlpha(kGlow, 0.8f)},
                    {0.5f, mskia::withAlpha(kTealBar, 0.28f)},
                    {1.0f, mskia::withAlpha(kTealBar, 0.0f)}})),
               at(box().fill(hexColor(0x010A0C)), 18, 74, 30, 60),
               at(box().fill(hexColor(0x02171B)), 52, 46, 44, 88),
               at(box().fill(hexColor(0x010A0C)), 100, 62, 34, 72),
               at(box().fill(mskia::withAlpha(kGlow, 0.6f)), 0, 108, 150, 1)})
          .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.85f), 12, 2, 4,
                                       shapes::Corner::All})
          .foreground(styles::Scanlines{{0, 0, 0, 0.22f}, 3, 1});

  Element copy =
      box()
          .grow(1)
          .column()
          .gap(6)
          .children(
              {box()
                   .row()
                   .gap(7)
                   .alignItems(Align::Center)
                   .children({box()
                                  .width(9)
                                  .height(9)
                                  .shape(shapes::polygon(3, 90))
                                  .fill(kDate)})
                   .children(
                       {t("01.30.06", sigil::weave::kit::tracked(
                                          blackFace(), 14, kDate, 40, 0.95f))})
                   .children({box().grow(1).height(1).fill(
                       mskia::withAlpha(kDate, 0.35f))}),
               t("N.O.-XPLODE TV COMMERCIAL",
                 sigil::weave::kit::tracked(blackFace(), 17, hexColor(0x0E3234),
                                            40, 0.92f)),
               box()
                   .height(84)
                   .padding(9)
                   .fill(dither.material())
                   .foreground(
                       stroke(1, Fill::color(mskia::withAlpha(kPanelSh, 0.9f)),
                              PathFormat::Align::Inner))
                   // the ONE place this interface is not tracked caps
                   .children({t("2Advanced completes a broadcast spot for "
                                "BSN's N.O.-Xplode line — full CG "
                                "environment, character rig and compositing, "
                                "delivered in nine weeks on a Maxon pipeline "
                                "against a live-action plate.",
                                prose(13, hexColor(0x0B2C2E)))})})
          // the related-work strip: four chamfered stills over the
          // dither ground, the way the FEATURE panel filled its slack
          .children({box()
                         .grow(1)
                         .row()
                         .gap(7)
                         .alignItems(Align::Stretch)
                         .children(relatedStills())})
          // the spec readout: dense, tabular, and never actually read
          .children(
              {box()
                   .row()
                   .gap(14)
                   .children({specPair("CLIENT", "BSN / N.O.-XPLODE")})
                   .children({specPair("RUNTIME", "00:30 · NTSC")})
                   .children({specPair("TOOLS", "C4D R8 / AE 6.5")})
                   .children({specPair("DELIVERED", "01.24.06")}),
               box()
                   .row()
                   .gap(8)
                   .alignItems(Align::Center)
                   .children({t("› VIEW CASE STUDY",
                                micro(11, hexColor(0x123B3D), 220))})
                   .children({box().grow(1)})
                   .children({box()
                                  .width(150)
                                  .height(6)
                                  .fill(mskia::withAlpha(kPanelSh, 0.7f))
                                  .children({box()
                                                 .left(0)
                                                 .top(0)
                                                 .width(112)
                                                 .height(6)
                                                 .fill(hexColor(0x0E3234))})})
                   .children({t("74%", micro(10, hexColor(0x123B3D), 160))})
                   .children({box().width(126)})});

  Element leftCol = box().width(150).shrink(0).column().gap(8).children(
      {thumb,
       box()
           .grow(1)
           .column()
           .gap(4)
           .padding(8)
           .fill(dither.material())
           .foreground(stroke(1, Fill::color(mskia::withAlpha(kPanelSh, 0.9f)),
                              PathFormat::Align::Inner))
           .children(
               {t("CREDITS",
                  micro(9, mskia::withAlpha(hexColor(0x123B3D), 0.8f), 260))})
           .children({box().height(1).fill(mskia::withAlpha(kDate, 0.28f))})
           .children({t("DIRECTION", micro(9, kDate, 200))})
           .children({t("ERIC JORDAN",
                        sigil::weave::kit::tracked(
                            blackFace(), 11, hexColor(0x0E3234), 40, 0.92f))})
           .children({box().height(3)})
           .children({t("STUDIO", micro(9, kDate, 200))})
           .children({t("2ADVANCED",
                        sigil::weave::kit::tracked(
                            blackFace(), 11, hexColor(0x0E3234), 40, 0.92f))})
           .children({box().grow(1)})
           .children({box()
                          .row()
                          .gap(4)
                          .alignItems(Align::Center)
                          .children({box().width(7).height(7).fill(
                              mskia::withAlpha(kDate, 0.8f))})
                          .children({t("ARCHIVED", micro(9, kDate, 200))})})});

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
                        0, 316 - 46, 150, 46)});
  bodyArea.children({box()
                         .left(694 - 11 - 116)
                         .top(316 - 11 - 34)
                         .children({cta("LAUNCH", 116, 34, kPanelSh)})});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("feature")
      .area("feature")
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
  struct Entry {
    const char *date, *headline, *body;
  };
  static const Entry entries[6] = {
      {"+ 04.05.06", "PROPHECY PRIME SKIN RELEASED",
       "The v4 desktop suite ships with the new application skin, the "
       "wallpaper set and the FIBERGLASS icon pack."},
      {"+ 03.11.06", "NAMED FWA SITE OF THE MONTH",
       "Prophecy takes the month for interface design and sound "
       "integration."},
      {"+ 01.30.06", "N.O.-XPLODE SPOT NOW AIRING",
       "Nine weeks, a full CG environment, delivered on a Maxon pipeline."},
      {"+ 01.07.06", "EQUIPMENT PAGE UPDATED",
       "New render nodes online; the studio moves to a dual-Xeon farm."},
      {"+ 12.14.05", "EXPERIMENTAL SECTION REOPENS",
       "Six new motion studies posted under the experimental banner."},
      {"+ 11.02.05", "HOLIDAY DESKTOP SET",
       "Three widescreen wallpapers in the Prophecy palette."},
  };

  Element list = box().column().gap(9);
  for (const Entry& e : entries)
    list.children(
        {box()
             .column()
             .gap(4)
             .children({box()
                            .row()
                            .gap(7)
                            .alignItems(Align::Center)
                            .fill(mskia::withAlpha(kPanelSh, 0.55f))
                            .padding(6, 3)
                            .children({t(e.date, sigil::weave::kit::tracked(
                                                     blackFace(), 13, kDate, 40,
                                                     0.95f))})
                            .children({box().grow(1).height(1).fill(
                                mskia::withAlpha(kDate, 0.3f))})
                            .children({t("▸", micro(9, kDate, 0))})})
             .children({t(e.headline,
                          sigil::weave::kit::tracked(
                              blackFace(), 13, hexColor(0x0E3234), 50, 0.92f))})
             .children({t(e.body, prose(12.5f, hexColor(0x0C2E30)))})});
  return list;
}

auto TwoAdvancedV4::pressUpdates() -> Element {
  using namespace tav;
  Element list = pressList().translateY(&pressScroll);

  // The thumb is the well's share of the entries it scrolls, and it
  // rides the same Output the list does: a bar drawn at a guessed
  // length beside a list scrolled over a measured one says the wrong
  // thing twice, about how much is below and about where the reader is.
  auto stepper = [&](bool up) {
    return box()
        .width(16)
        .height(16)
        .fill(kPanelSh)
        .justify(Justify::Center)
        .alignItems(Align::Center)
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
                   .stroke(stroke(1, Fill::color(mskia::withAlpha(kDate, 0.4f)),
                                  PathFormat::Align::Inner)),
           .scrolled = well,
           // The list's own scroll, in the thumb's units. A list that
           // fits has no travel and the target collapses to nothing;
           // the divisor is only there to stay finite.
           .position = motion::bind(&pressScroll)
                           .source(0.0f, -std::max(pressOverflow, 1.0f))
                           .target(0.0f, well.thumb().travel),
           .thumbInset = 2,
           .track = Fill::color(mskia::withAlpha(kPanelSh, 0.6f))})
          .width(16)
          .gap(3);

  Element bodyArea = monitorBody(376).column().padding(11).gap(9).children(
      {box()
           .grow(1)
           .row()
           .gap(8)
           .children({box()
                          .grow(1)
                          .clip()
                          .padding(9)
                          .fill(dither.material())
                          .foreground(stroke(
                              1, Fill::color(mskia::withAlpha(kPanelSh, 0.9f)),
                              PathFormat::Align::Inner))
                          .children({list})})
           .children({scrollbar}),
       box()
           .row()
           .alignItems(Align::Center)
           .gap(8)
           .children(
               {t("06 ENTRIES · PAGE 1/4", micro(11, hexColor(0x123B3D), 220))})
           .children({box().grow(1)})
           .children({cta("ARCHIVES", 116, 34, kPanelSh)})});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("press")
      .area("press")
      .translateY(animate(motion::from(60.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 3250ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3250ms}))
      .children({panelHeader("PRESS", " UPDATES", "STUDIO WIRE", 2), bodyArea});
  return panel;
}

auto TwoAdvancedV4::auxBar(const char* label) -> Element {
  using namespace tav;
  return box()
      .height(18)
      .row()
      .alignItems(Align::Center)
      .padding(6, 0)
      .gap(6)
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, hexColor(0x5A1A20)}, {1.0f, hexColor(0x2E0A0C)}}))
      .children({t("»", micro(10, kCyan, 0)), t(label, micro(11, kNear, 160))});
}

auto TwoAdvancedV4::auxView() -> Element {
  using namespace tav;
  return box()
      .height(17)
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1}, {{0.0f, kPanelHi}, {0.5f, kPanel}, {1.0f, kPanelSh}}))
      .stroke(stroke(1, Fill::color(mskia::withAlpha(hexColor(0xCFEFEC), 0.6f)),
                     PathFormat::Align::Inner))
      .justify(Justify::Center)
      .alignItems(Align::Center)
      .children({t("VIEW", label(11, kDate, 200))});
}

auto TwoAdvancedV4::auxiliary() -> Element {
  using namespace tav;
  const SkColor4f kCopy = hexColor(0x7FD4D0);  // the module copy teal

  // Column 1: three icon rows, copy and link verbatim (including the
  // interface's own "inorder").
  struct Item {
    const char *glyph, *l1, *l2, *link;
  };
  static const Item items[3] = {
      {"■", "The Equipment store carries the latest",
       "2Advanced apparel and publications...", "› VIEW"},
      {"▣", "Chat live with a 2Advanced sales agent",
       "inorder to inquire about project pricing...", "› OFFLINE"},
      {"✉", "Subscribe to the 2Advanced Members",
       "List and receive exclusive news & press...", "› SUBSCRIBE"},
  };
  Element supplementals = box()
                              .grow(1)
                              .basis(0)
                              .column()
                              .gap(3)
                              .children({auxBar("SUPPLEMENTALS & "
                                                "ESSENTIALS")});
  for (const Item& it : items)
    supplementals.children(
        {box()
             .row()
             .gap(8)
             .alignItems(Align::Center)
             .children({box()
                            .width(26)
                            .height(26)
                            .shrink(0)
                            .corners({4})
                            .fill(mskia::Paint::linearUnit(
                                {0, 0}, {0, 1},
                                {{0.0f, hexColor(0x8E2A2A)},
                                 {1.0f, hexColor(0x3A0C0E)}}))
                            .stroke(stroke(
                                1, Fill::color(mskia::withAlpha(kNear, 0.4f)),
                                PathFormat::Align::Inner))
                            .justify(Justify::Center)
                            .alignItems(Align::Center)
                            .children({t(it.glyph, micro(11, kPanelHi, 0))})})
             .children(
                 {box()
                      .grow(1)
                      .column()
                      .children({t(it.l1, prose(11.5f, kCopy))})
                      .children(
                          {box()
                               .row()
                               .children({t(it.l2, prose(11.5f, kCopy))})
                               .children({box().grow(1)})
                               .children(
                                   {t(it.link,
                                      micro(9, mskia::withAlpha(kNear, 0.85f),
                                            160))})})})});

  // Column 2: the book plate is white — the one white rectangle on the
  // whole page — with the title set dark on it.
  Element photoshop =
      box()
          .grow(1)
          .basis(0)
          .column()
          .gap(4)
          .children(
              {auxBar("PHOTOSHOP: SECRETS OF THE PROS"),
               box()
                   .row()
                   .gap(8)
                   .grow(1)
                   .children({box()
                                  .width(118)
                                  .shrink(0)
                                  .fill(hexColor(0xF2F0EA))
                                  .column()
                                  .padding(7, 6)
                                  .gap(2)
                                  .children({t(
                                      "Photoshop",
                                      sigil::weave::kit::tracked(
                                          arial(), 15, hexColor(0x2A4A7A), 0))})
                                  .children({t("Secrets of the Pros",
                                               sigil::weave::kit::tracked(
                                                   arial(), 10,
                                                   hexColor(0x333333), 0))})})
                   .children({t("Eric Jordan appears in \"Photoshop: Secrets "
                                "of the Pros\", a book featuring 20 top "
                                "designers with insights on their "
                                "techniques/methods.",
                                prose(11.5f, kCopy))}),
               auxView()});

  // Column 3: the 2ADVANCED.NET plate — its angular mark is the only
  // amber on the interface.
  Element press =
      box()
          .grow(1)
          .basis(0)
          .column()
          .gap(4)
          .children(
              {auxBar("FEATURED PRESS"),
               box()
                   .height(40)
                   .row()
                   .alignItems(Align::Center)
                   .padding(8, 0)
                   .gap(7)
                   .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                                  {{0.0f, hexColor(0x2A0A0C)},
                                                   {1.0f, hexColor(0x140404)}}))
                   .stroke(stroke(1, Fill::color(mskia::withAlpha(kDust, 0.4f)),
                                  PathFormat::Align::Inner))
                   .children({box()
                                  .width(20)
                                  .height(20)
                                  .shape(shapes::chamfered(
                                      6, shapes::Corner::Diagonal))
                                  .fill(mskia::Paint::linearUnit(
                                      {0, 0}, {0, 1},
                                      {{0.0f, hexColor(0xE8A83C)},
                                       {1.0f, hexColor(0x9A5E10)}}))})
                   .children(
                       {box()
                            .column()
                            .gap(1)
                            .children({t("2ADVANCED.NET",
                                         heavy(13, hexColor(0xD9DDE0), 60))})
                            .children({t("PRECISION HOSTING PLATFORM",
                                         micro(8, kDust, 220))})}),
               t("2advanced Studios is pleased to announce the official "
                 "launch of 2advanced.net, a flexible and managed web "
                 "hosting platform.",
                 prose(11.5f, kCopy)),
               box().grow(1), auxView()});

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("aux")
      .area("aux")
      .translateY(animate(motion::from(56.0f).to(0.0f),
                          {400ms, &ch::easeOutQuint, 3100ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3100ms}))
      .children({panelHeader("AUXILIARY", " PANEL",
                             "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 3),
                 box()
                     .grow(1)
                     .row()
                     .gap(10)
                     .padding(8, 6)
                     .fill(hexColor(0x300B0E))
                     .children({supplementals})
                     .children({photoshop})
                     .children({press})});
  return panel;
}

auto TwoAdvancedV4::subSystem() -> Element {
  using namespace tav;
  auto chip = [&](const char* glyph) {
    return box()
        .width(40)
        .height(40)
        .corners({20})
        .fill(mskia::Paint::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, hexColor(0x6A1B21)}, {1.0f, hexColor(0x220608)}}))
        .stroke(stroke(1, Fill::color(mskia::withAlpha(kDust, 0.5f)),
                       PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .children({t(glyph, heavy(15, kCyan, 0))});
  };
  auto selector = [&](const char* lbl, const char* value) {
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
                 .stroke(stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.5f)),
                                PathFormat::Align::Inner)),
             box()
                 .column()
                 .gap(1)
                 .children({t(lbl, micro(10, kDustDim, 240))})
                 .children({box()
                                .row()
                                .gap(5)
                                .alignItems(Align::Center)
                                .children({t("▸", micro(9, kCyan, 0))})
                                .children({t(value, label(13, kNear, 90))})
                                .children({t("▾", micro(9, kDust, 0))})})});
  };

  Element row =
      bevelPanel(box().row().alignItems(Align::Center).padding(14, 0).gap(18),
                 hexColor(0x2E0B0D));
  row.key("subsys")
      .area("subsys")
      .background(
          styles::Overlay{hazard.material(), SkBlendMode::kSrcOver, 0.16f})
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {400ms, &ch::easeOutQuad, 3650ms}))
      .foreground(styles::TickRail{mskia::withAlpha(kDust, 0.35f), 9, 4, 8, 1,
                                   4, 0.5f, path::Edge::Top})
      .children(
          {t("SUB", heavy(15, kNear, 40)),
           t("SYSTEM",
             sigil::weave::kit::tracked(arial(), 14, kHeadDim, 40, 0.95f)),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           t("PARTNERS:", micro(11, kDust, 240)), chip("A"), chip("M"),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           selector("DESKTOPS", "'FIBERGLASS'"),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           selector("APP SKINS", "'PROPHECY PRIME'"),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           box()
               .column()
               .gap(3)
               .children({t("SOUND", micro(10, kDustDim, 240))})
               .children({box()
                              .row()
                              .gap(4)
                              .alignItems(Align::Center)
                              .children({toggle("ON", true)})
                              .children({toggle("OFF", false)})}),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           box()
               .column()
               .gap(3)
               .children({t("QUALITY", micro(10, kDustDim, 240))})
               .children({box()
                              .row()
                              .gap(4)
                              .alignItems(Align::Center)
                              .children({toggle("LOW", false)})
                              .children({toggle("MED", false)})
                              .children({toggle("HIGH", true)})}),
           box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)),
           box()
               .column()
               .gap(3)
               .children({t("RESOLUTION", micro(10, kDustDim, 240))})
               .children({t("▸ 1024×768 · 32-BIT", label(13, kNear, 90))}),
           box().grow(1),
           box()
               .column()
               .alignItems(Align::End)
               .gap(3)
               .children({t("BANDWIDTH  ■■■"
                            "■■□□",
                            micro(11, mskia::withAlpha(kCyan, 0.85f), 200))})
               .children({t("UPTIME 118:24:07", micro(10, kDustDim, 200))}),
           box().width(70).height(40).foreground(
               styles::TickRail{mskia::withAlpha(kCyan, 0.45f), 6, 4, 10, 1, 3,
                                0.5f, path::Edge::Bottom})});
  return row;
}
