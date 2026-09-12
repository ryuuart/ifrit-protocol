#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::specPair(const char* k, const char* v) -> Element {
  using namespace tav;
  return box()
      .column()
      .gap(2)
      .child(t(k, micro(9, mskia::withAlpha(hexColor(0x123B3D), 0.75f), 260)))
      .child(box().height(1).fill(mskia::withAlpha(kDate, 0.28f)))
      .child(t(v, sigil::weave::kit::tracked(blackFace(), 11,
                                             hexColor(0x0E3234), 40, 0.92f)));
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
          .child(box().inset(0).fill(mskia::Paint::radialUnit(
              {0.5f, 0.72f}, 0.95f,
              {{0.0f, mskia::withAlpha(kGlow, 0.8f)},
               {0.5f, mskia::withAlpha(kTealBar, 0.28f)},
               {1.0f, mskia::withAlpha(kTealBar, 0.0f)}})))
          .child(at(box().fill(hexColor(0x010A0C)), 18, 74, 30, 60))
          .child(at(box().fill(hexColor(0x02171B)), 52, 46, 44, 88))
          .child(at(box().fill(hexColor(0x010A0C)), 100, 62, 34, 72))
          .child(at(box().fill(mskia::withAlpha(kGlow, 0.6f)), 0, 108, 150, 1))
          .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.85f), 12, 2, 4,
                                       shapes::Corner::All})
          .foreground(styles::Scanlines{{0, 0, 0, 0.22f}, 3, 1});

  Element copy =
      box()
          .grow(1)
          .column()
          .gap(6)
          .child(
              box()
                  .row()
                  .gap(7)
                  .alignItems(Align::Center)
                  .child(box()
                             .width(9)
                             .height(9)
                             .shape(shapes::polygon(3, 90))
                             .fill(kDate))
                  .child(t("01.30.06", sigil::weave::kit::tracked(
                                           blackFace(), 14, kDate, 40, 0.95f)))
                  .child(box().grow(1).height(1).fill(
                      mskia::withAlpha(kDate, 0.35f))))
          .child(t("N.O.-XPLODE TV COMMERCIAL",
                   sigil::weave::kit::tracked(blackFace(), 17,
                                              hexColor(0x0E3234), 40, 0.92f)))
          .child(box()
                     .height(84)
                     .padding(9)
                     .fill(dither.material())
                     .foreground(stroke(
                         1, Fill::color(mskia::withAlpha(kPanelSh, 0.9f)),
                         PathFormat::Align::Inner))
                     // the ONE place this interface is not tracked caps
                     .child(t("2Advanced completes a broadcast spot for "
                              "BSN's N.O.-Xplode line \xe2\x80\x94 full CG "
                              "environment, character rig and compositing, "
                              "delivered in nine weeks on a Maxon pipeline "
                              "against a live-action plate.",
                              prose(13, hexColor(0x0B2C2E)))))
          // the related-work strip: four chamfered stills over the
          // dither ground, the way the FEATURE panel filled its slack
          .child(box()
                     .grow(1)
                     .row()
                     .gap(7)
                     .alignItems(Align::Stretch)
                     .children(relatedStills()))
          // the spec readout: dense, tabular, and never actually read
          .child(box()
                     .row()
                     .gap(14)
                     .child(specPair("CLIENT", "BSN / N.O.-XPLODE"))
                     .child(specPair("RUNTIME", "00:30 \xc2\xb7 NTSC"))
                     .child(specPair("TOOLS", "C4D R8 / AE 6.5"))
                     .child(specPair("DELIVERED", "01.24.06")))
          .child(box()
                     .row()
                     .gap(8)
                     .alignItems(Align::Center)
                     .child(t("\xe2\x80\xba VIEW CASE STUDY",
                              micro(11, hexColor(0x123B3D), 220)))
                     .child(box().grow(1))
                     .child(box()
                                .width(150)
                                .height(6)
                                .fill(mskia::withAlpha(kPanelSh, 0.7f))
                                .child(box()
                                           .left(Dimension(0))
                                           .top(Dimension(0))
                                           .width(112)
                                           .height(6)
                                           .fill(hexColor(0x0E3234))))
                     .child(t("74%", micro(10, hexColor(0x123B3D), 160)))
                     .child(box().width(126)));

  Element leftCol =
      box().width(150).shrink(0).column().gap(8).child(thumb).child(
          box()
              .grow(1)
              .column()
              .gap(4)
              .padding(8)
              .fill(dither.material())
              .foreground(stroke(1,
                                 Fill::color(mskia::withAlpha(kPanelSh, 0.9f)),
                                 PathFormat::Align::Inner))
              .child(
                  t("CREDITS",
                    micro(9, mskia::withAlpha(hexColor(0x123B3D), 0.8f), 260)))
              .child(box().height(1).fill(mskia::withAlpha(kDate, 0.28f)))
              .child(t("DIRECTION", micro(9, kDate, 200)))
              .child(t("ERIC JORDAN",
                       sigil::weave::kit::tracked(
                           blackFace(), 11, hexColor(0x0E3234), 40, 0.92f)))
              .child(box().height(3))
              .child(t("STUDIO", micro(9, kDate, 200)))
              .child(t("2ADVANCED",
                       sigil::weave::kit::tracked(
                           blackFace(), 11, hexColor(0x0E3234), 40, 0.92f)))
              .child(box().grow(1))
              .child(box()
                         .row()
                         .gap(4)
                         .alignItems(Align::Center)
                         .child(box().width(7).height(7).fill(
                             mskia::withAlpha(kDate, 0.8f)))
                         .child(t("ARCHIVED", micro(9, kDate, 200)))));

  Element bodyArea =
      monitorBody(316).row().padding(11).gap(11).child(leftCol).child(copy);
  // the hazard wedge, bottom-left — the STATIC baked-tile pattern path
  bodyArea.child(at(box()
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
                    0, 316 - 46, 150, 46));
  bodyArea.child(box()
                     .left(Dimension(694 - 11 - 116))
                     .top(Dimension(316 - 11 - 34))
                     .child(cta("LAUNCH", 116, 34, kPanelSh)));

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("feature")
      .area("feature")
      .translateX(animate(motion::from(90.0f).to(0.0f),
                          {500ms, &ch::easeOutQuint, 2600ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 2600ms}))
      .child(panelHeader("FEATURE", " SYSTEM", "LATEST TRANSMISSION", 1))
      .child(bodyArea);
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
    list.child(
        box()
            .column()
            .gap(4)
            .child(box()
                       .row()
                       .gap(7)
                       .alignItems(Align::Center)
                       .fill(mskia::withAlpha(kPanelSh, 0.55f))
                       .padding(6, 3)
                       .child(t(e.date, sigil::weave::kit::tracked(
                                            blackFace(), 13, kDate, 40, 0.95f)))
                       .child(box().grow(1).height(1).fill(
                           mskia::withAlpha(kDate, 0.3f)))
                       .child(t("\xe2\x96\xb8", micro(9, kDate, 0))))
            .child(t(e.headline,
                     sigil::weave::kit::tracked(blackFace(), 13,
                                                hexColor(0x0E3234), 50, 0.92f)))
            .child(t(e.body, prose(12.5f, hexColor(0x0C2E30)))));
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
        .child(t(up ? "\xe2\x96\xb4" : "\xe2\x96\xbe", micro(8, kBody, 0)));
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

  Element bodyArea =
      monitorBody(376)
          .column()
          .padding(11)
          .gap(9)
          .child(box()
                     .grow(1)
                     .row()
                     .gap(8)
                     .child(box()
                                .grow(1)
                                .clip()
                                .padding(9)
                                .fill(dither.material())
                                .foreground(stroke(1,
                                                   Fill::color(mskia::withAlpha(
                                                       kPanelSh, 0.9f)),
                                                   PathFormat::Align::Inner))
                                .child(list))
                     .child(scrollbar))
          .child(box()
                     .row()
                     .alignItems(Align::Center)
                     .gap(8)
                     .child(t("06 ENTRIES \xc2\xb7 PAGE 1/4",
                              micro(11, hexColor(0x123B3D), 220)))
                     .child(box().grow(1))
                     .child(cta("ARCHIVES", 116, 34, kPanelSh)));

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("press")
      .area("press")
      .translateY(animate(motion::from(60.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 3250ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3250ms}))
      .child(panelHeader("PRESS", " UPDATES", "STUDIO WIRE", 2))
      .child(bodyArea);
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
      .child(t("\xc2\xbb", micro(10, kCyan, 0)))
      .child(t(label, micro(11, kNear, 160)));
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
      .child(t("VIEW", label(11, kDate, 200)));
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
      {"\xe2\x96\xa0", "The Equipment store carries the latest",
       "2Advanced apparel and publications...", "\xe2\x80\xba VIEW"},
      {"\xe2\x96\xa3", "Chat live with a 2Advanced sales agent",
       "inorder to inquire about project pricing...", "\xe2\x80\xba OFFLINE"},
      {"\xe2\x9c\x89", "Subscribe to the 2Advanced Members",
       "List and receive exclusive news & press...", "\xe2\x80\xba SUBSCRIBE"},
  };
  Element supplementals = box()
                              .grow(1)
                              .basis(Dimension(0))
                              .column()
                              .gap(3)
                              .child(auxBar("SUPPLEMENTALS & "
                                            "ESSENTIALS"));
  for (const Item& it : items)
    supplementals.child(
        box()
            .row()
            .gap(8)
            .alignItems(Align::Center)
            .child(box()
                       .width(26)
                       .height(26)
                       .shrink(0)
                       .corners({4})
                       .fill(mskia::Paint::linearUnit(
                           {0, 0}, {0, 1},
                           {{0.0f, hexColor(0x8E2A2A)},
                            {1.0f, hexColor(0x3A0C0E)}}))
                       .stroke(
                           stroke(1, Fill::color(mskia::withAlpha(kNear, 0.4f)),
                                  PathFormat::Align::Inner))
                       .justify(Justify::Center)
                       .alignItems(Align::Center)
                       .child(t(it.glyph, micro(11, kPanelHi, 0))))
            .child(
                box()
                    .grow(1)
                    .column()
                    .child(t(it.l1, prose(11.5f, kCopy)))
                    .child(box()
                               .row()
                               .child(t(it.l2, prose(11.5f, kCopy)))
                               .child(box().grow(1))
                               .child(t(it.link,
                                        micro(9, mskia::withAlpha(kNear, 0.85f),
                                              160))))));

  // Column 2: the book plate is white — the one white rectangle on the
  // whole page — with the title set dark on it.
  Element photoshop =
      box()
          .grow(1)
          .basis(Dimension(0))
          .column()
          .gap(4)
          .child(auxBar("PHOTOSHOP: SECRETS OF THE PROS"))
          .child(
              box()
                  .row()
                  .gap(8)
                  .grow(1)
                  .child(box()
                             .width(118)
                             .shrink(0)
                             .fill(hexColor(0xF2F0EA))
                             .column()
                             .padding(7, 6)
                             .gap(2)
                             .child(t("Photoshop",
                                      sigil::weave::kit::tracked(
                                          arial(), 15, hexColor(0x2A4A7A), 0)))
                             .child(t("Secrets of the Pros",
                                      sigil::weave::kit::tracked(
                                          arial(), 10, hexColor(0x333333), 0))))
                  .child(t("Eric Jordan appears in \"Photoshop: Secrets "
                           "of the Pros\", a book featuring 20 top "
                           "designers with insights on their "
                           "techniques/methods.",
                           prose(11.5f, kCopy))))
          .child(auxView());

  // Column 3: the 2ADVANCED.NET plate — its angular mark is the only
  // amber on the interface.
  Element press =
      box()
          .grow(1)
          .basis(Dimension(0))
          .column()
          .gap(4)
          .child(auxBar("FEATURED PRESS"))
          .child(
              box()
                  .height(40)
                  .row()
                  .alignItems(Align::Center)
                  .padding(8, 0)
                  .gap(7)
                  .fill(mskia::Paint::linearUnit(
                      {0, 0}, {0, 1},
                      {{0.0f, hexColor(0x2A0A0C)}, {1.0f, hexColor(0x140404)}}))
                  .stroke(stroke(1, Fill::color(mskia::withAlpha(kDust, 0.4f)),
                                 PathFormat::Align::Inner))
                  .child(
                      box()
                          .width(20)
                          .height(20)
                          .shape(shapes::chamfered(6, shapes::Corner::Diagonal))
                          .fill(mskia::Paint::linearUnit(
                              {0, 0}, {0, 1},
                              {{0.0f, hexColor(0xE8A83C)},
                               {1.0f, hexColor(0x9A5E10)}})))
                  .child(box()
                             .column()
                             .gap(1)
                             .child(t("2ADVANCED.NET",
                                      heavy(13, hexColor(0xD9DDE0), 60)))
                             .child(t("PRECISION HOSTING PLATFORM",
                                      micro(8, kDust, 220)))))
          .child(t("2advanced Studios is pleased to announce the official "
                   "launch of 2advanced.net, a flexible and managed web "
                   "hosting platform.",
                   prose(11.5f, kCopy)))
          .child(box().grow(1))
          .child(auxView());

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("aux")
      .area("aux")
      .translateY(animate(motion::from(56.0f).to(0.0f),
                          {400ms, &ch::easeOutQuint, 3100ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 3100ms}))
      .child(panelHeader("AUXILIARY", " PANEL",
                         "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 3))
      .child(box()
                 .grow(1)
                 .row()
                 .gap(10)
                 .padding(8, 6)
                 .fill(hexColor(0x300B0E))
                 .child(supplementals)
                 .child(photoshop)
                 .child(press));
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
        .child(t(glyph, heavy(15, kCyan, 0)));
  };
  auto selector = [&](const char* lbl, const char* value) {
    return box()
        .row()
        .gap(8)
        .alignItems(Align::Center)
        .child(box()
                   .width(46)
                   .height(34)
                   .shape(shapes::chamfered(8, shapes::Corner::Diagonal))
                   .fill(mskia::Paint::radialUnit({0.5f, 0.76f}, 1.1f,
                                                  {{0.0f, hexColor(0x0A4148)},
                                                   {1.0f, hexColor(0x010D10)}}))
                   .stroke(stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.5f)),
                                  PathFormat::Align::Inner)))
        .child(box()
                   .column()
                   .gap(1)
                   .child(t(lbl, micro(10, kDustDim, 240)))
                   .child(box()
                              .row()
                              .gap(5)
                              .alignItems(Align::Center)
                              .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                              .child(t(value, label(13, kNear, 90)))
                              .child(t("\xe2\x96\xbe", micro(9, kDust, 0)))));
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
      .child(t("SUB", heavy(15, kNear, 40)))
      .child(t("SYSTEM",
               sigil::weave::kit::tracked(arial(), 14, kHeadDim, 40, 0.95f)))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(t("PARTNERS:", micro(11, kDust, 240)))
      .child(chip("A"))
      .child(chip("M"))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(selector("DESKTOPS", "'FIBERGLASS'"))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(selector("APP SKINS", "'PROPHECY PRIME'"))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(box()
                 .column()
                 .gap(3)
                 .child(t("SOUND", micro(10, kDustDim, 240)))
                 .child(box()
                            .row()
                            .gap(4)
                            .alignItems(Align::Center)
                            .child(toggle("ON", true))
                            .child(toggle("OFF", false))))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(box()
                 .column()
                 .gap(3)
                 .child(t("QUALITY", micro(10, kDustDim, 240)))
                 .child(box()
                            .row()
                            .gap(4)
                            .alignItems(Align::Center)
                            .child(toggle("LOW", false))
                            .child(toggle("MED", false))
                            .child(toggle("HIGH", true))))
      .child(box().width(1).height(30).fill(mskia::withAlpha(kDust, 0.35f)))
      .child(box()
                 .column()
                 .gap(3)
                 .child(t("RESOLUTION", micro(10, kDustDim, 240)))
                 .child(t("\xe2\x96\xb8 1024\xc3\x97"
                          "768 \xc2\xb7 32-BIT",
                          label(13, kNear, 90))))
      .child(box().grow(1))
      .child(box()
                 .column()
                 .alignItems(Align::End)
                 .gap(3)
                 .child(t("BANDWIDTH  \xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0"
                          "\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa1\xe2\x96\xa1",
                          micro(11, mskia::withAlpha(kCyan, 0.85f), 200)))
                 .child(t("UPTIME 118:24:07", micro(10, kDustDim, 200))))
      .child(box().width(70).height(40).foreground(
          styles::TickRail{mskia::withAlpha(kCyan, 0.45f), 6, 4, 10, 1, 3, 0.5f,
                           path::Edge::Bottom}));
  return row;
}
