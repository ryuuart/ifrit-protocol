#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::panelHeader(const char* boldHalf, const char* restHalf,
                                const char* flavor, int cluster) -> Element {
  using namespace tav;
  return box()
      .height(28)
      .row()
      .alignItems(Align::Center)
      .padding({.top = 0, .right = 10, .bottom = 0, .left = 10})
      .fill(stripesLive)
      .foreground(onEdges(path::Edge::Bottom,
                          stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.35f)),
                                 PathFormat::Align::Inner)))
      .children(
          {t(boldHalf, heavy(17, kNear, 40)),
           t(restHalf, cut(arial(), 15, kHeadDim, 40, 0.95f)), box().width(12),
           kit::line({.length = Dimension(12),
                      .column = true,
                      .fill = Fill::color(mskia::withAlpha(kCyan, 0.4f))}),
           box().width(10),
           t(flavor, micro(11, mskia::withAlpha(kDust, 1.0f), 260)),
           box().flexGrow(1), tickDots(cluster), box().width(8),
           box().width(34).height(10).foreground(
               styles::TickRail{mskia::withAlpha(kCyan, 0.55f), 4, 3, 7, 1, 3,
                                0.5f, path::Edge::Bottom})});
}

auto TwoAdvancedV4::cta(const char* lbl, float w, float h, SkColor4f hairline)
    -> Element {
  using namespace tav;
  return kit::centred()
      .width(w)
      .height(h)
      .shape(shapes::chamfered(9, shapes::Corner::Diagonal))
      .fill(mskia::Paint::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, kCtaHi}, {0.42f, kCta}, {1.0f, hexColor(0x3A0000)}}))
      .stroke(stroke(1, Fill::color(kChrome), PathFormat::Align::Outer))
      .foreground(kit::gloss(mskia::withAlpha(kCtaHi, 0.55f), h * 0.30f,
                             {0, -h * 0.26f}, 0.62f, 0.30f))
      .foreground(stroke(1, Fill::color(mskia::withAlpha(hairline, 0.45f)),
                         PathFormat::Align::Inner))
      .row()

      .children({t(lbl, label(15, kNear, 110))});
}

auto TwoAdvancedV4::readout(float w, float h, SkColor4f ground) -> Element {
  using namespace tav;
  return box()
      .width(w)
      .height(h)
      .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
      .fill(ground)
      .foreground(styles::BevelPair{
          mskia::withAlpha(kChromeHi, 0.6f), {0, 0, 0, 0.5f}, 1, 1})
      .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.55f), 8, 2, 3,
                                   shapes::Corner::All});
}

auto TwoAdvancedV4::navBar() -> Element {
  using namespace tav;
  Element bar =
      bevelPanel(box()
                     .row()
                     .justifyContent(Justify::SpaceEvenly)
                     .alignItems(Align::Center)
                     .padding({.top = 0, .right = 6, .bottom = 0, .left = 6}),
                 kChrome);
  bar.key("nav").gridArea("nav").fill(stripesLive).staggerChildren(40ms);
  // ONE ITEM PER NAME IN THE DOCUMENT'S TAXONOMY, with the hairline that
  // stands each off the one before it interleaved by the run itself.
  bar.children({each(
      doc()["nav"].items(),
      [](const sigil::data::Json& name) {
        return kit::centred()
            .column()
            .gap(3)
            .translateY(animate(motion::from(16.0f).to(0.0f),
                                {240ms, &ch::easeOutQuint, 2250ms}))
            .appear({240ms, &ch::easeOutQuad, 2250ms})
            .children(
                {t(name, label(13, kNear, 80)),
                 box().width(8).height(2).fill(mskia::withAlpha(kDust, 0.6f))});
      },
      kit::line(
          {.length = Dimension(20),
           .column = true,
           .fill = Fill::color(mskia::withAlpha(hexColor(0x2A0A0C), 0.9f))}))});
  // The GLOBAL NAVIGATOR's live selection mark: one cyan bar whose X is
  // a single bound value, gliding between items as the section cycle
  // walks the taxonomy.
  bar.children(
      {box()
           .left(0)
           .top(38)
           .width(24)
           .height(3)
           .fill(kCyan)
           .background(styles::OuterGlow{mskia::withAlpha(kGlow, 0.5f), 6, 0})
           .translateX(&navIndX)});
  return bar;
}

auto TwoAdvancedV4::masthead() -> Element {
  using namespace tav;
  Element emblem = kit::centred().width(78).height(78).background(
      styles::OuterGlow{mskia::withAlpha(kGlow, 0.45f), 14, 0});
  if (logoBugSvg) {
    // The production mark itself, recoloured to the wordmark cyan: a
    // solid fill masked by the SVG raster's coverage, so the vector
    // art contributes shape only and the palette stays sampled.
    emblem.children({box().width(62).height(62).fill(kCyan).mask(
        by::alpha(stretchFill(logoBugSvg, 62, 62)))});
  } else {
    emblem
        .fill(mskia::Paint::recipe(msdf::material(
            msdf::circle(), {.fill = {0, 0, 0, 0},
                             .borderWidth = 4,
                             .borderColor = mskia::toColor(kCyan)})))
        .children(
            {kit::centred()
                 .width(50)
                 .height(50)
                 .shape(shapes::polygon(6, 0))
                 .stroke(
                     stroke(1, Fill::color(mskia::withAlpha(kCyanRing, 0.75f))))

                 .children({t("2", cut(blackFace(), 32, kCyan, 0, 0.85f))})});
  }

  return box()
      .key("masthead")
      .gridArea("masthead")
      .column()
      .translateX(animate(motion::from(320.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 1850ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 1850ms}))
      .children(
          {box()
               .flexGrow(1)
               .row()
               .alignItems(Align::Center)
               .padding({.top = 0, .right = 8, .bottom = 0, .left = 26})
               .gap(18)
               .children(
                   {emblem,
                    box().column().gap(6).children(
                        {t("2ADVANCED STUDIOS",
                           cut(blackFace(), 25, kCyan, 80, 0.90f))
                             .filter(styles::textGlow(
                                 mskia::withAlpha(kGlow, 0.55f), 6)),
                         t("PROGRESSIVE DESIGN TECHNOLOGY",
                           micro(12, kDust, 240)),
                         box()
                             .row()
                             .gap(6)
                             .alignItems(Align::Center)
                             .children({kit::line({.length = Dimension(30),
                                                   .fill = Fill::color(
                                                       mskia::withAlpha(
                                                           kCyan, 0.5f))}),
                                        t("EST. 1999 · IRVINE CA",
                                          micro(10, kDustDim, 200))})}),
                    box().flexGrow(1),
                    box()
                        .column()
                        .alignItems(Align::End)
                        .gap(4)
                        .children(
                            {t("BUILD 4.0.7", micro(10, kDustDim, 200)),
                             t("FLASH 6 REQ.", micro(10, kDustDim, 200)),
                             t("1024×768 MIN", micro(10, kDustDim, 200))})}),
           // the glowing 2px cyan divider under the whole masthead panel
           box().height(2).fill(kCyan).background(
               styles::OuterGlow{mskia::withAlpha(kGlow, 0.55f), 10, 1})});
}

auto TwoAdvancedV4::toggle(const char* lbl, bool on) -> Element {
  using namespace tav;
  return kit::centred()
      .height(18)
      .padding({.top = 0, .right = 7, .bottom = 0, .left = 7})
      .shape(shapes::chamfered(5, shapes::Corner::Diagonal))
      .fill(on ? mskia::Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0x0A4148)}, {1.0f, hexColor(0x02181C)}})
               : mskia::Paint::solid(hexColor(0x220608)))
      .stroke(stroke(1,
                     Fill::color(on ? mskia::withAlpha(kCyan, 0.7f)
                                    : mskia::withAlpha(kDust, 0.35f)),
                     PathFormat::Align::Inner))

      .children({t(lbl, micro(10, on ? kCyan : kDustDim, 160))});
}

auto TwoAdvancedV4::linkRun(const sigil::data::Json& names, float size,
                            float rule) -> std::vector<Element> {
  using namespace tav;
  return each(
      names.items(),
      [size](const sigil::data::Json& name) {
        return t(name, micro(size, kDustDim, 200));
      },
      kit::line({.length = Dimension(rule),
                 .column = true,
                 .fill = Fill::color(mskia::withAlpha(kDust, 0.32f))}));
}

auto TwoAdvancedV4::legalStrip() -> Element {
  using namespace tav;
  return box()
      .column()
      .padding({.top = 8, .right = 6, .bottom = 8, .left = 6})
      .alignItems(Align::Center)
      .gap(4)
      .key("legal")
      .gridArea("legal")
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {400ms, &ch::easeOutQuad, 3750ms}))
      .children(
          {box()
               .alignSelf(Align::Stretch)
               .row()
               .alignItems(Align::Center)
               .children(
                   {box()
                        .row()
                        .gap(6)
                        .alignItems(Align::Center)
                        .children(
                            {kit::line({.length = Dimension(60),
                                        .fill = Fill::color(
                                            mskia::withAlpha(kDust, 0.35f))}),
                             t(doc()["legal"]["requires"],
                               micro(10, kDustDim, 200))}),
                    box().flexGrow(1),
                    box()
                        .row()
                        .gap(7)
                        .alignItems(Align::Center)
                        .children(
                            {t(doc()["legal"]["archived"],
                               micro(11, kDust, 240)),
                             box()
                                 .height(24)
                                 .padding({.top = 0,
                                           .right = 8,
                                           .bottom = 0,
                                           .left = 8})
                                 .shape(shapes::chamfered(
                                     7, shapes::Corner::Diagonal))
                                 .fill(hexColor(0x2A0A0C))
                                 .stroke(stroke(1,
                                                Fill::color(mskia::withAlpha(
                                                    kDust, 0.45f)),
                                                PathFormat::Align::Inner))
                                 .row()
                                 .gap(6)
                                 .alignItems(Align::Center)
                                 .children({t("▸", micro(9, kCyan, 0)),
                                            t(doc()["legal"]["version"],
                                              label(12, kNear, 90)),
                                            t("▾", micro(9, kDust, 0))})})}),
           box().flexGrow(1),
           box()
               .row()
               .gap(9)
               .alignItems(Align::Center)
               .children({kit::line({.length = Dimension(40),
                                     .fill = Fill::color(
                                         mskia::withAlpha(kDust, 0.3f))}),
                          linkRun(doc()["footer"], 10, 9),
                          kit::line({.length = Dimension(40),
                                     .fill = Fill::color(
                                         mskia::withAlpha(kDust, 0.3f))})}),
           box().height(4),
           t(doc()["legal"]["copyright"], micro(12, kDust, 240)),
           box()
               .row()
               .gap(10)
               .alignItems(Align::Center)
               .children({linkRun(doc()["legal"]["links"], 11, 10)})});
}

auto TwoAdvancedV4::dockBars() -> std::vector<Element> {
  using namespace tav;
  std::vector<Element> bars;
  for (int i = 0; i < 56; ++i) {
    const float v = 0.14f + 0.82f * std::abs(std::sin(i * 0.51f) *
                                             std::cos(i * 0.19f + 0.7f));
    bars.push_back(box().flexGrow(1).flexShrink(0).height(72 * v).fill(
        mskia::Paint::linearUnit({0, 0}, {0, 1},
                                 {{0.0f, mskia::withAlpha(kD7, 1.0f)},
                                  {1.0f, mskia::withAlpha(kD4, 0.9f)}})));
  }
  return bars;
}

auto TwoAdvancedV4::footerDock() -> Element {
  using namespace tav;
  if (footerGif) {
    // The real 970×110 dock bitmap at ×2 — on the page it sat BELOW
    // the SWF as a plain image, monochrome oxblood, no live states.
    // Everything the procedural fallback rebuilds is already in it.
    return box()
        .fill(stretchFill(footerGif, 1892, 220))
        .key("dock")
        .gridArea("dock")
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {400ms, &ch::easeOutQuad, 3850ms}))
        .foreground(onEdges(path::Edge::Top, stroke(2, Fill::color(kD5),
                                                    PathFormat::Align::Inner)));
  }
  Element strip =
      box()
          .fill(mskia::Paint::blend(
              {{mskia::Paint::linearUnit(
                    {0, 0}, {0, 1}, {{0.0f, kD5}, {0.45f, kD2}, {1.0f, kD1}}),
                SkBlendMode::kSrcOver},
               {hatchA.material(), SkBlendMode::kSrcOver},
               {hatchB.material(), SkBlendMode::kSrcOver}}))
          .row()
          .alignItems(Align::Center)
          .padding({.top = 12, .right = 14, .bottom = 12, .left = 14})
          .gap(12)
          .key("dock")
          .gridArea("dock")
          .opacity(animate(motion::from(0.0f).to(1.0f),
                           {400ms, &ch::easeOutQuad, 3850ms}))
          .foreground(
              onEdges(path::Edge::Top,
                      stroke(2, Fill::color(kD5), PathFormat::Align::Inner)));

  // THE DOCK'S WINDOWS, one per record in the document: a title with its
  // rule, two readings under it and a tick rail at the foot.
  const auto window = [&](const sigil::data::Json& w) {
    return readout((float)w["width"].number(280), 150, hexColor(0x140404))
        .column()
        .padding(10)
        .gap(5)
        .children(
            {box()
                 .row()
                 .alignItems(Align::Center)
                 .gap(6)
                 .children({t(w["title"], cut(blackFace(), 12, kD7, 60, 0.92f)),
                            kit::line({.fill = Fill::color(kD4)}).flexGrow(1),
                            t("»", micro(11, kD5, 0))}),
             t(w["first"], micro(10, kD6, 220)),
             t(w["second"], micro(10, mskia::withAlpha(kD6, 0.7f), 220)),
             box().flexGrow(1),
             box()
                 .row()
                 .gap(5)
                 .alignItems(Align::Center)
                 .children(
                     {box().width(58).height(8).foreground(styles::TickRail{
                          kD6, 5, 3, 7, 1, 3, 0.5f, path::Edge::Top}),
                      box().flexGrow(1), t("v v", micro(10, kD6, 200))})});
  };

  strip.children(
      {each(doc()["dock"].items(), window),
       // the instanced chevron array — one atlas cell, one stamp
       box()
           .width(260)
           .height(150)
           .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
           .fill(hexColor(0x110303))
           .foreground(styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 1, 1})
           .children({box().left(12).top(12).width(236).height(96).children(
                          {instancing::instances(dockAtlas, dockPool,
                                                 instancing::Mode::Data)}),
                      box().left(12).top(122).children(
                          {t("ARRAY 6×14 · IDLE", micro(10, kD6, 220))})}),
       box()
           .flexGrow(1)
           .height(150)
           .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
           .fill(hexColor(0x140404))
           .column()
           .padding(10)
           .gap(5)
           .foreground(styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 1, 1})
           .foreground(styles::Brackets{kD6, 9, 2, 3, shapes::Corner::All})
           .children(
               {box()
                    .row()
                    .alignItems(Align::Center)
                    .gap(6)
                    .children(
                        {t("SIGNAL", cut(blackFace(), 12, kD7, 60, 0.92f)),
                         kit::line({.fill = Fill::color(kD4)}).flexGrow(1),
                         t("»", micro(11, kD5, 0))}),
                box()
                    .flexGrow(1)
                    .fill(hexColor(0x0D0202))
                    .foreground(styles::BevelPair{kD4, {0, 0, 0, 0.5f}, 1, 1})
                    .row()
                    .alignItems(Align::End)
                    .gap(2)
                    .padding(6)
                    .children(dockBars()),
                box()
                    .row()
                    .gap(6)
                    .alignItems(Align::Center)
                    .children(
                        {t("GAIN 0.42 · SWEEP 20 MS", micro(10, kD6, 220)),
                         box().flexGrow(1),
                         box().width(70).height(8).foreground(styles::TickRail{
                             kD5, 5, 3, 7, 1, 3, 0.5f, path::Edge::Top})})})});

  Element cluster =
      kit::centred()
          .width(310)
          .height(150)
          .shape(shapes::chamfered(9, shapes::Corner::Diagonal))
          .fill(mskia::Paint::linearUnit(
              {0, 0}, {0, 1}, {{0.0f, kD3}, {1.0f, hexColor(0x0C0202)}}))
          .foreground(inset(5, styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 2, 1}))
          .foreground(styles::Brackets{kD6, 12, 2, 5, shapes::Corner::All})
          .row()

          .gap(14);
  for (int i = 0; i < 3; ++i)
    cluster.children(
        {kit::centred()
             .width(80)
             .height(80)
             .fill(mskia::Paint::recipe(msdf::material(
                 msdf::circle(), {.fill = mskia::toColor(hexColor(0x0A0202)),
                                  .borderWidth = 3,
                                  .borderColor = mskia::toColor(kD6)})))

             .children({radarSweep(i, hexColor(0xB65050), 0.42f),
                        box()
                            .inset(26)
                            .borderRadius({16})
                            .fill(mskia::withAlpha(kD1, 0.92f))
                            .stroke(stroke(1, Fill::color(kD4),
                                           PathFormat::Align::Inner)),
                        t(i == 0 ? "01" : (i == 1 ? "02" : "03"),
                          micro(11, kD7, 140))})});
  strip.children({cluster});
  return strip;
}

auto TwoAdvancedV4::rail(bool right) -> Element {
  using namespace tav;
  Element r = box()
                  .left(right ? 1916.0f : 0.0f)
                  .top(0)
                  .width(24)
                  .height(1560)
                  .cache(Cache::None);
  const auto& gif = right ? railRightGif : railLeftGif;
  if (gif) {
    // The production rail bitmap, held to the shell's own display
    // geometry: the page shows the 26×780 GIF at 12×780 CSS px, and
    // this frame is ×2 of that page, so the node is 24×1560.
    r.fill(stretchFill(gif, 24, 1560));
    // The flare highlight stays live on top — the bitmap carries the
    // flare ART, and the travelling sheen is drawn over it.
    r.foreground(RailFlares{hexColor(0x99AAAA), 6.0f, right ? 3.0f : 0.0f});
    return r;
  }
  return r
      .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                     {{0.00f, hexColor(0x6A1B21)},
                                      {0.22f, kChrome},
                                      {0.70f, hexColor(0x2A0708)},
                                      {1.00f, hexColor(0x0A0000)}}))
      .foreground(onEdges(
          right ? path::Edge::Left : path::Edge::Right,
          stroke(1, Fill::color(mskia::withAlpha(hexColor(0x99AAAA), 0.35f)),
                 PathFormat::Align::Inner)))
      .foreground(RailFlares{hexColor(0x99AAAA), 6.0f, right ? 3.0f : 0.0f});
}

auto TwoAdvancedV4::bootOverlay() -> Element {
  using namespace tav;
  const float cx = 970, cy = 760;

  auto hair = [&](float x, float y, float w, float h, float dx, float dy,
                  int delayMs) {
    return at(
        box()
            .shape(ray(dx, dy))
            .stroke(spans::upTo(animate(motion::from(0.0f).to(1.0f),
                                        {400ms, &ch::easeOutQuint,
                                         std::chrono::milliseconds(delayMs)})),
                    stroke(1.5f, Fill::color(kCyan))),
        x, y, w, h);
  };

  Element o = stack().inset(0).zIndex(90);
  o.children(
      {box()
           .inset(0)
           .fill(hexColor(0x120303))
           .opacity(animate(
               motion::through({{0ms, 1.0f}, {1400ms, 1.0f}, {1560ms, 0.0f}}))),
       // 1. the single cyan pixel-dot
       at(box().fill(kCyan), cx - 3, cy - 3, 6, 6)
           .opacity(animate(motion::through(
               {{0ms, 0.0f}, {150ms, 1.0f}, {1350ms, 1.0f}, {1450ms, 0.0f}}))),
       // 2. the reticle drawing OUTWARD from it on four trimmed rays
       hair(cx - 470, cy, 470, 1, -1, 1, 150), hair(cx, cy, 470, 1, 1, 1, 150),
       hair(cx, cy - 300, 1, 300, 1, -1, 220), hair(cx, cy, 1, 300, 1, 1, 220),
       at(box()
              .shape(shapes::arc(-90, 359))
              .stroke(spans::upTo(animate(motion::from(0.0f).to(1.0f),
                                          {500ms, &ch::easeOutQuint, 260ms})),
                      stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.7f)))),
          cx - 92, cy - 92, 184, 184),
       // 3. the 0→100 readout (a slot: TEXT, so it cannot be a binding)
       at(box().column().alignItems(Align::Center).gap(9), cx - 260, cy + 120,
          520, 110)
           .opacity(animate(motion::through(
               {{520ms, 0.0f}, {620ms, 1.0f}, {1350ms, 1.0f}, {1450ms, 0.0f}})))
           .children(
               {slot("bootpct"),
                box()
                    .width(420)
                    .height(2)
                    .fill(mskia::withAlpha(kCyan, 0.18f))
                    .children({box().inset(0).shape(ray(1, 1)).stroke(
                        spans::upTo(animate(motion::from(0.0f).to(1.0f),
                                            {800ms, &ch::easeNone, 550ms})),
                        stroke(2, Fill::color(kCyan)))}),
                t("LOADING PROPHECY INTERFACE · 970×655",
                  micro(11, mskia::withAlpha(kCyan, 0.6f), 240))}),
       // 4. the boot-complete flash
       box()
           .inset(0)
           .fill(SkColor4f{1, 1, 1, 1})
           .opacity(animate(motion::through(
               {{1330ms, 0.0f}, {1390ms, 0.7f}, {1460ms, 0.0f}})))
           .blendMode(SkBlendMode::kPlus)});
  o.opacity(animate(motion::through({{1440ms, 1.0f}, {1480ms, 0.0f}})));
  return o;
}

auto TwoAdvancedV4::bootReadout() -> Element {
  using namespace tav;
  const std::string buf = kit::formatted("%03d", bootPct);
  return box()
      .row()
      .alignItems(Align::Baseline)
      .gap(6)
      .children({t(buf.c_str(), cut(blackFace(), 46, kCyan, 40, 0.9f)),
                 t("%", cut(blackFace(), 20, mskia::withAlpha(kCyan, 0.6f), 40,
                            0.9f))});
}
