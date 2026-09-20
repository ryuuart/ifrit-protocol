// Section artwork and the lower content modules.

#include "TwoAdvancedV3.h"

Element TwoAdvancedV3::stageArt() {
  using namespace tv3;
  return at(box().clip().children({slot("stage")}), kStageX, kArtY, kStageW,
            kArtH)
      .mask(by::edge(0, animate(motion::from(0.0f).to(1.0f),
                                {650ms, &ch::easeOutQuint, 1900ms})));
}

Element TwoAdvancedV3::sectionArt(int sec, float settle) {
  using namespace tv3;
  // Keyed per section AND per settle bucket: structurally different
  // content must REPLACE in the slot, not be patched over — a text
  // leaf patched onto a container trips the layout engine.
  const std::string key =
      kit::formatted("sec:%d:%d", sec, settle >= 1.0f ? 1 : (int)(settle * 14));
  Element art = box().key(key).width(kStageW).height(kArtH).clip();
  const ImagePtr& bg = sec < 0 ? homeBg : sectionBg[(size_t)sec];
  if (bg)
    art.fill(stretchFill(bg, kStageW, kArtH));
  else
    art.fill(mskia::Paint::linearUnit(
        {0, 0}, {0, 1}, {{0.0f, hexColor(0x2A3A58)}, {1.0f, kDeep}}));

  if (sec < 0) {
    if (!clouds.empty() && gapMask) {
      // The 62-frame loop is FULL sky footage — blue sky, clouds and
      // all — so it REPLACES the opening rather than tinting it:
      // drawn behind the structure through the feathered opening
      // coverage, slightly desaturated and dimmed so the footage
      // sits in the plate's own exposure.
      art.children({box()
                        .inset(0)
                        .children({at(box().clip().children({slot("clouds")}),
                                      415, 35, 310, 255)})
                        .mask(by::alpha(mskia::Paint::image(
                            gapMask, SkTileMode::kClamp, SkTileMode::kClamp,
                            SkMatrix::Scale(kStageW / (float)gapMask->width(),
                                            kArtH / (float)gapMask->height()))))
                        .filter(mskia::Effect::filter(cloudLook))
                        .opacity(0.95f)});
    }
    // Idle beacon on the art's readout cluster: the one light that
    // never stops blinking.
    art.children({at(box().borderRadius({3}), kStageW - 116, kArtH - 62, 6, 6)
                      .fill(mskia::withAlpha(kSteelHi, 0.9f))
                      .opacity(&beaconAlpha)});
    return art;
  }

  const SectionSpec& spec = kSections[(size_t)sec];
  // Title spreader, top right: the letterform block settles from
  // stretched-wide to rest as the section engages.
  art.children({at(box()
                       .row()
                       .justifyContent(Justify::End)
                       .alignItems(Align::Center)
                       .gap(10)
                       .children({box().flexGrow(1).height(1).fill(
                                      mskia::withAlpha(kSteelHi, 0.55f)),
                                  t(spec.tab, micro(14, kNear, 600)),
                                  box().width(24).height(8).fill(
                                      mskia::withAlpha(kSteelHi, 0.8f))}),
                   kStageW - 560, 12, 540, 22)
                    .opacity(0.25f + 0.75f * settle)
                    .scaleX(1.5f - 0.5f * settle)
                    .transformOrigin(pct(100), pct(50))});
  // Sub-nav tabs, centre top — sections without them page by arrows.
  if (spec.subnav[0]) {
    Element tabs = box().row().gap(2);
    for (const char* s : spec.subnav)
      if (s)
        tabs.children(
            {kit::centred()
                 .height(17)
                 .padding(10, 0)
                 .fill(mskia::withAlpha(kSeam, 0.92f))
                 .stroke(stroke(1,
                                Fill::color(mskia::withAlpha(kSteelHi, 0.45f)),
                                PathFormat::Align::Inner))

                 .children({t(s, micro(9, kNear, 200))})});
    art.children(
        {at(box().row().justifyContent(Justify::Center).children({tabs}),
            kStageW / 2 - 220, 26, 440, 17)
             .opacity(settle)});
  }
  // RETURN TO MAIN, bottom right.
  const data::Json& voice = doc["art"];
  art.children(
      {at(t(voice["return"], micro(9, mskia::withAlpha(kNear, 0.85f), 200)),
          kStageW - 190, kArtH - 30, 180, 14)
           .opacity(settle),
       // MODULE.ENGAGED tick, bottom left — the riv's load-state voice.
       at(t(voice[settle >= 1.0f ? "engaged" : "loading"],
            micro(9, mskia::withAlpha(kSteelHi, 0.8f), 240)),
          18, kArtH - 30, 220, 14)});
  return art;
}

Element TwoAdvancedV3::transitionArt(int fromSec, int toSec, int step) {
  using namespace tv3;
  const float f = (float)step / 14.0f;
  const std::string key = kit::formatted("trans:%d:%d", fromSec, toSec);
  Element out = box().key(key).width(kStageW).height(kArtH).clip();
  out.children(
      {box().inset(0).children({sectionArt(fromSec, 1.0f)}),
       box().inset(0).children({sectionArt(toSec, f)}).mask(by::edge(0, f))});
  // the leading band, one step wide, brightest at mid-sweep
  const float x = kStageW * f;
  out.children(
      {at(box().fill(mskia::withAlpha(kSteelHi, 0.85f)), x - 5, 0, 10, kArtH)
           .blend(SkBlendMode::kScreen)
           .opacity(0.28f + 0.5f * std::sin(f * 3.14159f))});
  return out;
}

Element TwoAdvancedV3::module(const Utf8& glyph, const Utf8& barLabel,
                              Element body, int order) {
  using namespace tv3;
  return box()
      .width(kPanelW)
      .column()
      .children(
          {moduleBar(glyph, barLabel, kPanelW),
           box()
               .flexGrow(1)
               .fill(mskia::withAlpha(hexColor(0x4A5872), 0.80f))
               .stroke(stroke(1, Fill::color(mskia::withAlpha(kSteelHi, 0.55f)),
                              PathFormat::Align::Inner))
               .children({body.inset(0)})})
      .translateY(animate(motion::from(46.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint,
                           std::chrono::milliseconds(2300 + 120 * order)}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {320ms, &ch::easeOutQuad,
                        std::chrono::milliseconds(2300 + 120 * order)}));
}

Element TwoAdvancedV3::thumbPlate(Element content, const Utf8& btn) {
  using namespace tv3;
  return box()
      .width(150)
      .flexShrink(0)
      .column()
      .gap(2)
      // the toolbar strip: a small lit segment on a dark rail
      .children(
          {box()
               .height(8)
               .fill(hexColor(0x2A3550))
               .row()
               .alignItems(Align::Center)
               .padding(3, 0)
               .children({box().width(28).height(4).fill(
                   mskia::withAlpha(kSteelHi, 0.85f))}),
           kit::centred()
               .height(96)
               .shape(shapes::chamfered(20, shapes::Corner::TopRight))
               .fill(hexColor(0x232E48))
               .stroke(stroke(1, Fill::color(mskia::withAlpha(kSteelHi, 0.5f)),
                              PathFormat::Align::Inner))

               .children({std::move(content)}),
           kit::centred()
               .height(22)
               .shape(shapes::chamfered(14, shapes::Corner::BottomLeft))
               .fill(hexColor(0x313D5A))
               .stroke(stroke(1, Fill::color(mskia::withAlpha(kSteelHi, 0.45f)),
                              PathFormat::Align::Inner))

               .children({text(btn).font(micro(11, kNear, 140))})});
}

Element TwoAdvancedV3::riveLockup() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["partner"];
  Element row = box().row().gap(7).alignItems(Align::Center);
  if (logoMark)
    row.children({box().width(34).height(34).fill(kNear).mask(
        by::alpha(stretchFill(logoMark, 34, 34)))});
  row.children({text(page["plus"],
                     sigil::weave::kit::tracked(
                         grotBold(), 13, mskia::withAlpha(kNear, 0.9f), 0))});
  if (riveLogo)
    row.children(
        {box().width(44).height(44).fill(stretchFill(riveLogo, 44, 44))});
  else
    row.children({text(page["rive"],
                       sigil::weave::kit::tracked(grotBold(), 26, kNear, 0))});
  return row;
}

Element TwoAdvancedV3::featuredPartner() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["partner"];
  Element body = box().row().padding(12).gap(12).children(
      {thumbPlate(riveLockup(), page["button"]),
       box().flexGrow(1).column().gap(8).children(
           {box()
                .row()
                .gap(7)
                .alignItems(Align::Center)
                .children({meter(2), t(page["head"], micro(12, kNear, 20))}),
            t(page["copy"], prose(12, hexColor(0xC7D0DD)))})});
  return module(page["glyph"], page["bar"], std::move(body), 0);
}

Element TwoAdvancedV3::subData() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["discord"];
  Element icon = box().width(56).height(56);
  if (!discordSeq.empty())
    icon.children({slot("discord")});
  else
    icon.borderRadius({32}).fill(mskia::withAlpha(kSteel, 0.5f));
  Element body =
      box()
          .column()
          .padding(12)
          .gap(7)
          .alignItems(Align::Center)
          .children({box()
                         .row()
                         .gap(6)
                         .alignItems(Align::Center)
                         .children({meter(3),
                                    t(page["head"], micro(13.5f, kNear, 60))}),
                     icon, t(page["copy"], prose(12, hexColor(0xC7D0DD))),
                     box().flexGrow(1),
                     box()
                         .row()
                         .gap(8)
                         .alignItems(Align::Center)
                         .children({t(page["call"], micro(12, kNear, 120)),
                                    meter(2)})});
  return module(page["glyph"], page["bar"], std::move(body), 1);
}

Element TwoAdvancedV3::updates() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["updates"];
  Element body = box().row().padding(12).gap(12).children(
      {thumbPlate(dddLogo ? box().width(56).height(72).fill(
                                stretchFill(dddLogo, 56, 72))
                          : text(page["mark"], sigil::weave::kit::tracked(
                                                   grotBold(), 20, kNear, 100)),
                  page["button"]),
       box().flexGrow(1).column().gap(8).children(
           {box()
                .row()
                .gap(7)
                .alignItems(Align::Center)
                .children({meter(2), t(page["head"], micro(12, kNear, 20))}),
            t(page["copy"], prose(12, hexColor(0xC7D0DD)))})});
  return module(page["glyph"], page["bar"], std::move(body), 2);
}

Element TwoAdvancedV3::mailingList() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["mailing"];
  Element body = box().padding(12, 8).gap(6).children(
      {t(page["prompt"], micro(11, hexColor(0xC7D0DD), 100)),
       box()
           .row()
           .gap(8)
           .alignItems(Align::Center)
           .children({box()
                          .flexGrow(1)
                          .height(22)
                          .fill(mskia::withAlpha(kPage, 0.9f))
                          .stroke(stroke(
                              1, Fill::color(mskia::withAlpha(kSteel, 0.6f)),
                              PathFormat::Align::Inner))
                          .row()
                          .alignItems(Align::Center)
                          .padding(7, 0)
                          .children({t(
                              page["placeholder"],
                              micro(9, mskia::withAlpha(kBody, 0.7f), 100))}),
                      button(page["button"], 64)})});
  return module(page["glyph"], page["bar"], std::move(body), 3);
}

Element TwoAdvancedV3::support2a() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["support"];
  const auto half = [&](const data::Json& side) {
    return box()
        .flexGrow(1)
        .flexBasis(0)
        .column()
        .gap(4)
        .alignItems(Align::Center)
        .children({t(side["head"], micro(11, kNear, 60)),
                   t(side["copy"],
                     prose(9, mskia::withAlpha(hexColor(0xC7D0DD), 0.95f))),
                   box().flexGrow(1),
                   box()
                       .row()
                       .gap(6)
                       .alignItems(Align::Center)
                       .children({meter(2),
                                  t(side["call"], micro(10, kNear, 120))})});
  };
  Element body = box().row().padding(12, 4).gap(14).children(
      {each(page["halves"].items(), half)});
  return module(page["glyph"], page["bar"], std::move(body), 4);
}

Element TwoAdvancedV3::follow2a() {
  using namespace tv3;
  const data::Json& page = doc["modules"]["follow"];
  Element icons = box().height(16);
  if (socialSprite) {
    // The sprite is authored @2x (436×32); the layout shows it at 1×.
    const float iw = (float)socialSprite->width() * 0.5f;
    icons.width(iw).fill(stretchFill(socialSprite, iw, 16));
  } else {
    icons.row().gap(12);
    for (int i = 0; i < 7; ++i)
      icons.children({box().width(16).height(16).borderRadius({8}).fill(
          mskia::withAlpha(kSteelHi, 0.8f))});
  }
  Element body = kit::centred()
                     .column()
                     .padding(12, 8)

                     .children({icons});
  return module(page["glyph"], page["bar"], std::move(body), 5);
}
