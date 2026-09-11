// The page shell, navigation and boot sequence.

#include "TwoAdvancedV3.h"

Element TwoAdvancedV3::moduleBar(const char* glyph, const char* label,
                                 float w) {
  using namespace tv3;
  return box()
      .width(Dim(w))
      .height(26)
      .row()
      .alignItems(Align::Center)
      .padding(7, 0)
      .gap(8)
      .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, hexColor(0x8B98B2)},
                                      {0.55f, hexColor(0x64738F)},
                                      {1.0f, hexColor(0x4C5A73)}}))
      .foreground(onEdges(path::Edge::Bottom,
                          stroke(1, Fill::color(mskia::withAlpha(kInk, 0.6f)),
                                 PathFormat::Align::Inner)))
      .foreground(
          onEdges(path::Edge::Top,
                  stroke(1, Fill::color(mskia::withAlpha(kSteelHi, 0.7f)),
                         PathFormat::Align::Inner)))
      .child(box()
                 .width(16)
                 .height(16)
                 .fill(kInk)
                 .justify(Justify::Center)
                 .alignItems(Align::Center)
                 .child(t(glyph, micro(9, kSteelHi, 0))))
      .child(t(label, micro(14.5f, kInk, 140)))
      .child(box().width(6))
      .child(box().grow(1).height(16).fill(dots.material()).opacity(0.85f))
      .child(box().width(4).height(4).fill(mskia::withAlpha(kInk, 0.8f)))
      .child(box().width(4).height(4).fill(mskia::withAlpha(kInk, 0.5f)))
      .child(box().width(4).height(4).fill(mskia::withAlpha(kInk, 0.3f)));
}

Element TwoAdvancedV3::button(const char* label, float w, float h) {
  using namespace tv3;
  return box()
      .width(Dim(w))
      .height(Dim(h))
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, kSteelHi}, {0.5f, kSteel}, {1.0f, kSteelDim}}))
      .stroke(stroke(1, Fill::color(mskia::withAlpha(kInk, 0.7f)),
                     PathFormat::Align::Inner))
      .justify(Justify::Center)
      .alignItems(Align::Center)
      .child(t(label, micro(11, kInk, 140)));
}

Element TwoAdvancedV3::meter(int lit) {
  using namespace tv3;
  Element m = box().row().gap(2).alignItems(Align::Center);
  for (int i = 0; i < 5; ++i)
    m.child(box().width(9).height(7).fill(
        i < lit ? mskia::withAlpha(kSteelHi, 0.9f)
                : mskia::withAlpha(kSteelDim, 0.4f)));
  return m;
}

Element TwoAdvancedV3::bevelBar() {
  using namespace tv3;
  return at(box().fill(mskia::Paint::linearUnit(
                {0, 0}, {0, 1},
                {{0.0f, hexColor(0x98A3BA)}, {1.0f, hexColor(0x66738F)}})),
            kStageX, 0, kStageW, 8)
      .translateY(animate(motion::from(-10.0f).to(0.0f),
                          {300ms, &ch::easeOutQuint, 1450ms}));
}

Element TwoAdvancedV3::headerStrip() {
  using namespace tv3;
  Element strip = at(box().clip(), kStageX, 8, kStageW, 74);
  if (topHeader) {
    // Drawn at the bitmap's own half-res size and CROPPED at the stage
    // edge, exactly as the page shows it — squeezing it to fit reads
    // measurably lighter than the reference.
    strip.child(
        at(box().fill(stretchFill(topHeader, 1381, 77)), 0, 0, 1381, 77));
  } else {
    strip.fill(mskia::Paint::linearUnit(
        {0, 0}, {1, 0.4f},
        {{0.0f, hexColor(0x2E3F5D)}, {1.0f, hexColor(0x25334C)}}));
    strip.child(
        at(box().fill(diag.material()).opacity(0.18f), 0, 0, kStageW, 74));
  }
  return strip
      .translateY(animate(motion::from(-84.0f).to(0.0f),
                          {380ms, &ch::easeOutQuint, 1500ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {280ms, &ch::easeOutQuad, 1500ms}));
}

Element TwoAdvancedV3::wordmark() {
  using namespace tv3;
  // The mark and BOTH text lines are near-white on the steel — the
  // panel carries all the contrast, the lockup none of it.
  Element mark = box().width(46).height(46);
  if (logoMark) {
    mark.fill(kNear).mask(by::alpha(stretchFill(logoMark, 46, 46)));
  } else {
    mark.corners({23})
        .stroke(stroke(3, Fill::color(kNear), PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t("2a",
                 sigil::weave::kit::tracked(grotBold(), 18, kNear, 0, 1.0f)));
  }
  Element panel =
      at(box().row().alignItems(Align::Center).padding(30, 0).gap(16), kStageX,
         82, kStageW, 86)
          .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                         {{0.0f, hexColor(0x8C99B4)},
                                          {0.6f, kSteel},
                                          {1.0f, hexColor(0x67748E)}}))
          .foreground(
              onEdges(path::Edge::Bottom,
                      stroke(2, Fill::color(mskia::withAlpha(kInk, 0.5f)),
                             PathFormat::Align::Inner)))
          .child(mark)
          .child(box()
                     .column()
                     .gap(2)
                     .child(box()
                                .row()
                                .alignItems(Align::Start)
                                .gap(4)
                                .child(t("2 A D V A N C E D",
                                         sigil::weave::kit::tracked(
                                             grotBold(), 27, kNear, 80, 1.02f)))
                                .child(t("\xc2\xae", micro(9, kNear, 0))))
                     .child(t("S T U D I O S",
                              sigil::weave::kit::tracked(grotBold(), 12, kNear,
                                                         560, 1.0f))))
          .child(box().grow(1));
  return panel
      .translateY(animate(motion::from(-60.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 1600ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 1600ms}));
}

Element TwoAdvancedV3::navBar() {
  using namespace tv3;
  Element bar =
      at(box().row().alignItems(Align::Center), kStageX, 174, kStageW, 33);
  if (navbarBg)
    // Half-res native size, cropped at the stage edge (see the header
    // strip note — squeezing lightens the render).
    bar.clip().fill(stretchFill(navbarBg, 1338, 33));
  else
    bar.fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                      {{0.0f, hexColor(0x5A6A88)},
                                       {0.5f, kNavbar},
                                       {1.0f, hexColor(0x3C4A63)}}));

  // Left: the section label window (dark, baked into the bitmap).
  bar.child(
      box()
          .width(230)
          .height(33)
          .row()
          .alignItems(Align::Center)
          .padding(12, 0)
          .gap(7)
          .fill(mskia::withAlpha(hexColor(0x39445C), 0.92f))
          .foreground(
              onEdges(path::Edge::Right,
                      stroke(1, Fill::color(mskia::withAlpha(kInk, 0.8f)),
                             PathFormat::Align::Inner)))
          .child(t("\xe2\x86\x92", micro(11, kSteelHi, 0)))
          .child(t("2A.V3..2024 // EXPANSIONS", micro(11.5f, kNear, 80))));
  // Right: the six tab slots live in a slot so the active-section
  // indicator can move without re-describing the bar.
  bar.child(slot("navtabs"));
  return bar
      .translateY(animate(motion::from(-40.0f).to(0.0f),
                          {380ms, &ch::easeOutQuint, 1750ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {280ms, &ch::easeOutQuad, 1750ms}));
}

Element TwoAdvancedV3::navTabs(int active) {
  using namespace tv3;
  Element row = box().width(Dim(kStageW - 230)).height(33).row();
  for (int i = 0; i < 6; ++i) {
    const bool on = i == active;
    row.child(
        box()
            .grow(1)
            .height(33)
            .column()
            .justify(Justify::Center)
            .alignItems(Align::Center)
            .gap(2)
            // THE NAV IS THE PAGE'S LOUDEST TYPE. On the studio's
            // own capture the six section names are set larger
            // than the module headers under them; at eleven they
            // sat under, and the whole page read thin.
            .child(t(
                kSections[i].tab,
                micro(13.5f, on ? kNear : mskia::withAlpha(kNear, 0.88f), 170)))
            .child(box().width(46).height(2).fill(
                on ? mskia::withAlpha(kSteelHi, 0.95f)
                   : SkColor4f{0, 0, 0, 0})));
  }
  return row;
}

Element TwoAdvancedV3::hairlines() {
  using namespace tv3;
  return at(box()
                .column()
                .gap(2)
                .child(box().height(2).fill(mskia::withAlpha(kSteelHi, 0.8f)))
                .child(box()
                           .height(3)
                           .fill(mskia::withAlpha(kSeam, 0.95f))
                           .child(box()
                                      .inset(0)
                                      .fill(vticks.material())
                                      .opacity(0.55f))),
            kStageX, 210, kStageW, 7)
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {280ms, &ch::easeOutQuad, 1800ms}));
}

Element TwoAdvancedV3::scrollStrip() {
  using namespace tv3;
  return at(box()
                .row()
                .alignItems(Align::Center)
                .padding(10, 0)
                .gap(6)
                .fill(hexColor(0x4B5870))
                .foreground(onEdges(
                    path::Edge::Top,
                    stroke(1, Fill::color(mskia::withAlpha(kSteelHi, 0.55f)),
                           PathFormat::Align::Inner)))
                .child(t("\xe2\x86\x93", micro(9, kSteelHi, 0)))
                .child(t("SCROLL.EXTENDED.CONTENT",
                         micro(9, mskia::withAlpha(kSteelHi, 0.85f), 180)))
                .child(box().grow(1))
                .child(t("AMBIENCE.MUTE",
                         micro(9, mskia::withAlpha(kSteel, 0.9f), 180))),
            kStageX, 617, kStageW, 16)
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 2200ms}));
}

Element TwoAdvancedV3::footerRail() {
  using namespace tv3;
  return at(box()
                .row()
                .alignItems(Align::Center)
                .padding(10, 0)
                .gap(8)
                .fill(mskia::Paint::linearUnit(
                    {0, 0}, {0, 1},
                    {{0.0f, hexColor(0x5A6880)}, {1.0f, hexColor(0x49556C)}}))
                .child(t("(C) 2024 2ADVANCED STUDIOS", micro(9, kInk, 140)))
                .child(t("//", micro(9, mskia::withAlpha(kInk, 0.5f), 0)))
                .child(t("CONDITIONS OF USE", micro(9, kInk, 140)))
                .child(t("//", micro(9, mskia::withAlpha(kInk, 0.5f), 0)))
                .child(t("PRIVACY POLICY", micro(9, kInk, 140)))
                .child(box().grow(1))
                .child(t("HOSTING PARTNER:", micro(9, kInk, 140)))
                .child(box().width(12).height(12).corners({6}).fill(kHost)),
            kStageX, 1045, kStageW, 20)
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {320ms, &ch::easeOutQuad, 2900ms}));
}

Element TwoAdvancedV3::bootOverlay() {
  using namespace tv3;
  const SkColor4f kPreBg = hexColor(0x2A3753), kPreInk = hexColor(0x7183A5);
  Element lockup = box().width(197).height(94);
  if (pageLogo)
    // The bitmap is near-black art; the page shows it inverted. A
    // white fill through its coverage is that filter's visible result.
    lockup.fill(kNear).mask(by::alpha(stretchFill(pageLogo, 197, 94)));
  else
    lockup.justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t("2ADVANCED",
                 sigil::weave::kit::tracked(grotBold(), 24, kNear, 200, 1.0f)));

  Element o = stack().inset(0).zIndex(90);
  o.child(box().inset(0).fill(kPreBg).opacity(
      animate(motion::through({{0ms, 1.0f}, {1250ms, 1.0f}, {1450ms, 0.0f}}))));
  o.child(
      at(box().column().alignItems(Align::Center).gap(18), kW / 2 - 300,
         kH / 2 - 170, 600, 360)
          .opacity(animate(motion::through(
              {{0ms, 0.0f}, {150ms, 1.0f}, {1200ms, 1.0f}, {1350ms, 0.0f}})))
          .child(lockup)
          .child(t("SOLACE IN TECHNOLOGY. BELIEF IN THE FUTURE.",
                   sigil::weave::kit::tracked(grot(), 10, kPreInk, 400, 1.0f)))
          .child(slot("bootpct")));
  o.opacity(animate(motion::through({{1400ms, 1.0f}, {1450ms, 0.0f}})));
  return o;
}

Element TwoAdvancedV3::bootReadout() {
  using namespace tv3;
  const std::string buf = kit::formatted("%d", bootPct);
  return t(buf.c_str(), sigil::weave::kit::tracked(
                            grot(), 150, hexColor(0x7183A5), 0, 1.0f));
}
