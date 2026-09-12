#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::panelHeader(const char* boldHalf, const char* restHalf,
                                const char* flavor, int cluster) -> Element {
  using namespace tav;
  return box()
      .height(28)
      .row()
      .alignItems(Align::Center)
      .padding(10, 0)
      .fill(stripesLive)
      .foreground(onEdges(path::Edge::Bottom,
                          stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.35f)),
                                 PathFormat::Align::Inner)))
      .child(t(boldHalf, heavy(17, kNear, 40)))
      .child(t(restHalf,
               sigil::weave::kit::tracked(arial(), 15, kHeadDim, 40, 0.95f)))
      .child(box().width(12))
      .child(box().width(1).height(12).fill(mskia::withAlpha(kCyan, 0.4f)))
      .child(box().width(10))
      .child(t(flavor, micro(11, mskia::withAlpha(kDust, 1.0f), 260)))
      .child(box().grow(1))
      .child(tickDots(cluster))
      .child(box().width(8))
      .child(box().width(34).height(10).foreground(
          styles::TickRail{mskia::withAlpha(kCyan, 0.55f), 4, 3, 7, 1, 3, 0.5f,
                           path::Edge::Bottom}));
}

auto TwoAdvancedV4::cta(const char* lbl, float w, float h, SkColor4f hairline)
    -> Element {
  using namespace tav;
  return box()
      .width(Dimension(w))
      .height(Dimension(h))
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
      .justify(Justify::Center)
      .alignItems(Align::Center)
      .child(t(lbl, label(15, kNear, 110)));
}

auto TwoAdvancedV4::readout(float w, float h, SkColor4f ground) -> Element {
  using namespace tav;
  return box()
      .width(Dimension(w))
      .height(Dimension(h))
      .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
      .fill(ground)
      .foreground(styles::BevelPair{
          mskia::withAlpha(kChromeHi, 0.6f), {0, 0, 0, 0.5f}, 1, 1})
      .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.55f), 8, 2, 3,
                                   shapes::Corner::All});
}

auto TwoAdvancedV4::navBar() -> Element {
  using namespace tav;
  Element bar = bevelPanel(box()
                               .row()
                               .justify(Justify::SpaceEvenly)
                               .alignItems(Align::Center)
                               .padding(6, 0),
                           kChrome);
  bar.key("nav").area("nav").fill(stripesLive).staggerChildren(40ms);
  for (int i = 0; i < 7; ++i) {
    bar.child(box()
                  .column()
                  .alignItems(Align::Center)
                  .gap(3)
                  .translateY(animate(motion::from(16.0f).to(0.0f),
                                      {240ms, &ch::easeOutQuint, 2250ms}))
                  .opacity(animate(motion::from(0.0f).to(1.0f),
                                   {240ms, &ch::easeOutQuad, 2250ms}))
                  .child(t(kNavItems[i], label(13, kNear, 80)))
                  .child(box().width(8).height(2).fill(
                      mskia::withAlpha(kDust, 0.6f))));
    if (i < 6)
      bar.child(box().width(1).height(20).fill(
          mskia::withAlpha(hexColor(0x2A0A0C), 0.9f)));
  }
  // The GLOBAL NAVIGATOR's live selection mark: one cyan bar whose X is
  // a single bound value, gliding between items as the section cycle
  // walks the taxonomy.
  bar.child(
      box()
          .left(Dimension(0))
          .top(Dimension(38))
          .width(24)
          .height(3)
          .fill(kCyan)
          .background(styles::OuterGlow{mskia::withAlpha(kGlow, 0.5f), 6, 0})
          .translateX(&navIndX));
  return bar;
}

auto TwoAdvancedV4::masthead() -> Element {
  using namespace tav;
  Element emblem =
      box()
          .width(78)
          .height(78)
          .background(styles::OuterGlow{mskia::withAlpha(kGlow, 0.45f), 14, 0})
          .justify(Justify::Center)
          .alignItems(Align::Center);
  if (logoBugSvg) {
    // The production mark itself, recoloured to the wordmark cyan: a
    // solid fill masked by the SVG raster's coverage, so the vector
    // art contributes shape only and the palette stays sampled.
    emblem.child(box().width(62).height(62).fill(kCyan).mask(
        by::alpha(stretchFill(logoBugSvg, 62, 62))));
  } else {
    emblem
        .fill(mskia::Paint::recipe(msdf::material(
            msdf::circle(), {.fill = {0, 0, 0, 0},
                             .borderWidth = 4,
                             .borderColor = mskia::toColor(kCyan)})))
        .child(box()
                   .width(50)
                   .height(50)
                   .shape(shapes::polygon(6, 0))
                   .stroke(stroke(
                       1, Fill::color(mskia::withAlpha(kCyanRing, 0.75f))))
                   .justify(Justify::Center)
                   .alignItems(Align::Center)
                   .child(t("2", sigil::weave::kit::tracked(blackFace(), 32,
                                                            kCyan, 0, 0.85f))));
  }

  return box()
      .key("masthead")
      .area("masthead")
      .column()
      .translateX(animate(motion::from(320.0f).to(0.0f),
                          {420ms, &ch::easeOutQuint, 1850ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 1850ms}))
      .child(box()
                 .grow(1)
                 .row()
                 .alignItems(Align::Center)
                 .padding(26, 0, 8, 0)
                 .gap(18)
                 .child(emblem)
                 .child(box()
                            .column()
                            .gap(6)
                            .child(t("2ADVANCED STUDIOS",
                                     sigil::weave::kit::tracked(
                                         blackFace(), 25, kCyan, 80, 0.90f))
                                       .effect(styles::textGlow(
                                           mskia::withAlpha(kGlow, 0.55f), 6)))
                            .child(t("PROGRESSIVE DESIGN TECHNOLOGY",
                                     micro(12, kDust, 240)))
                            .child(box()
                                       .row()
                                       .gap(6)
                                       .alignItems(Align::Center)
                                       .child(box().width(30).height(1).fill(
                                           mskia::withAlpha(kCyan, 0.5f)))
                                       .child(t("EST. 1999 \xc2\xb7 IRVINE CA",
                                                micro(10, kDustDim, 200)))))
                 .child(box().grow(1))
                 .child(box()
                            .column()
                            .alignItems(Align::End)
                            .gap(4)
                            .child(t("BUILD 4.0.7", micro(10, kDustDim, 200)))
                            .child(t("FLASH 6 REQ.", micro(10, kDustDim, 200)))
                            .child(t("1024\xc3\x97"
                                     "768 MIN",
                                     micro(10, kDustDim, 200)))))
      // the glowing 2px cyan divider under the whole masthead panel
      .child(box().height(2).fill(kCyan).background(
          styles::OuterGlow{mskia::withAlpha(kGlow, 0.55f), 10, 1}));
}

auto TwoAdvancedV4::toggle(const char* lbl, bool on) -> Element {
  using namespace tav;
  return box()
      .height(18)
      .padding(7, 0)
      .shape(shapes::chamfered(5, shapes::Corner::Diagonal))
      .fill(on ? mskia::Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0x0A4148)}, {1.0f, hexColor(0x02181C)}})
               : mskia::Paint::solid(hexColor(0x220608)))
      .stroke(stroke(1,
                     Fill::color(on ? mskia::withAlpha(kCyan, 0.7f)
                                    : mskia::withAlpha(kDust, 0.35f)),
                     PathFormat::Align::Inner))
      .justify(Justify::Center)
      .alignItems(Align::Center)
      .child(t(lbl, micro(10, on ? kCyan : kDustDim, 160)));
}

auto TwoAdvancedV4::footerLinks() -> std::vector<Element> {
  using namespace tav;
  static const char* l[8] = {"HOME",      "COMPANY",   "SERVICES",
                             "PORTFOLIO", "ACCOLADES", "EXPERIMENTAL",
                             "EQUIPMENT", "CONTACT"};
  std::vector<Element> out;
  for (int i = 0; i < 8; ++i) {
    if (i)
      out.push_back(
          box().width(1).height(9).fill(mskia::withAlpha(kDust, 0.3f)));
    out.push_back(t(l[i], micro(10, kDustDim, 200)));
  }
  return out;
}

auto TwoAdvancedV4::legalStrip() -> Element {
  using namespace tav;
  return box()
      .column()
      .padding(6, 8)
      .alignItems(Align::Center)
      .gap(4)
      .key("legal")
      .area("legal")
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {400ms, &ch::easeOutQuad, 3750ms}))
      .child(
          box()
              .alignSelf(Align::Stretch)
              .row()
              .alignItems(Align::Center)
              .child(box()
                         .row()
                         .gap(6)
                         .alignItems(Align::Center)
                         .child(box().width(60).height(1).fill(
                             mskia::withAlpha(kDust, 0.35f)))
                         .child(t("SITE REQUIRES MACROMEDIA FLASH "
                                  "PLAYER 6",
                                  micro(10, kDustDim, 200))))
              .child(box().grow(1))
              .child(
                  box()
                      .row()
                      .gap(7)
                      .alignItems(Align::Center)
                      .child(t("ARCHIVED VERSIONS:", micro(11, kDust, 240)))
                      .child(
                          box()
                              .height(24)
                              .padding(8, 0)
                              .shape(shapes::chamfered(
                                  7, shapes::Corner::Diagonal))
                              .fill(hexColor(0x2A0A0C))
                              .stroke(stroke(
                                  1,
                                  Fill::color(mskia::withAlpha(kDust, 0.45f)),
                                  PathFormat::Align::Inner))
                              .row()
                              .gap(6)
                              .alignItems(Align::Center)
                              .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                              .child(t("V3 'EXPANSIONS'", label(12, kNear, 90)))
                              .child(t("\xe2\x96\xbe", micro(9, kDust, 0))))))
      .child(box().grow(1))
      .child(box()
                 .row()
                 .gap(9)
                 .alignItems(Align::Center)
                 .child(box().width(40).height(1).fill(
                     mskia::withAlpha(kDust, 0.3f)))
                 .children(footerLinks())
                 .child(box().width(40).height(1).fill(
                     mskia::withAlpha(kDust, 0.3f))))
      .child(box().height(4))
      .child(t("COPYRIGHT (C) 2003 2ADVANCED STUDIOS, LLC.  ALL RIGHTS "
               "RESERVED.",
               micro(12, kDust, 240)))
      .child(box()
                 .row()
                 .gap(10)
                 .alignItems(Align::Center)
                 .child(t("LEGAL", micro(11, kDustDim, 200)))
                 .child(box().width(1).height(10).fill(
                     mskia::withAlpha(kDust, 0.35f)))
                 .child(t("PRIVACY POLICY", micro(11, kDustDim, 200)))
                 .child(box().width(1).height(10).fill(
                     mskia::withAlpha(kDust, 0.35f)))
                 .child(t("SITE MAP", micro(11, kDustDim, 200))));
}

auto TwoAdvancedV4::dockBars() -> std::vector<Element> {
  using namespace tav;
  std::vector<Element> bars;
  for (int i = 0; i < 56; ++i) {
    const float v = 0.14f + 0.82f * std::abs(std::sin(i * 0.51f) *
                                             std::cos(i * 0.19f + 0.7f));
    bars.push_back(box()
                       .grow(1)
                       .shrink(0)
                       .height(Dimension(72 * v))
                       .fill(mskia::Paint::linearUnit(
                           {0, 0}, {0, 1},
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
        .area("dock")
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
          .padding(14, 12)
          .gap(12)
          .key("dock")
          .area("dock")
          .opacity(animate(motion::from(0.0f).to(1.0f),
                           {400ms, &ch::easeOutQuad, 3850ms}))
          .foreground(
              onEdges(path::Edge::Top,
                      stroke(2, Fill::color(kD5), PathFormat::Align::Inner)));

  auto window = [&](const char* title, const char* a, const char* b, float w) {
    return readout(w, 150, hexColor(0x140404))
        .column()
        .padding(10)
        .gap(5)
        .child(box()
                   .row()
                   .alignItems(Align::Center)
                   .gap(6)
                   .child(t(title, sigil::weave::kit::tracked(blackFace(), 12,
                                                              kD7, 60, 0.92f)))
                   .child(box().grow(1).height(1).fill(kD4))
                   .child(t("\xc2\xbb", micro(11, kD5, 0))))
        .child(t(a, micro(10, kD6, 220)))
        .child(t(b, micro(10, mskia::withAlpha(kD6, 0.7f), 220)))
        .child(box().grow(1))
        .child(box()
                   .row()
                   .gap(5)
                   .alignItems(Align::Center)
                   .child(box().width(58).height(8).foreground(styles::TickRail{
                       kD6, 5, 3, 7, 1, 3, 0.5f, path::Edge::Top}))
                   .child(box().grow(1))
                   .child(t("v v", micro(10, kD6, 200))));
  };

  strip.child(window("NAVIGATION", "SECTOR / PROPHECY", "NODE 04.11.22", 300));
  strip.child(
      window("EQUIPMENT", "RENDER FARM  08/08", "STORAGE  4.2 TB", 300));
  strip.child(window("DISPATCH", "QUEUE  00114", "LAST  04.05.06", 280));

  // the instanced chevron array — one atlas cell, one stamp
  strip.child(box()
                  .width(260)
                  .height(150)
                  .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
                  .fill(hexColor(0x110303))
                  .foreground(styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 1, 1})
                  .child(box()
                             .left(Dimension(12))
                             .top(Dimension(12))
                             .width(236)
                             .height(96)
                             .child(instancing::instances(
                                 dockAtlas, dockPool, instancing::Mode::Data)))
                  .child(box()
                             .left(Dimension(12))
                             .top(Dimension(122))
                             .child(t("ARRAY 6\xc3\x97"
                                      "14 \xc2\xb7 IDLE",
                                      micro(10, kD6, 220)))));

  strip.child(
      box()
          .grow(1)
          .height(150)
          .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
          .fill(hexColor(0x140404))
          .column()
          .padding(10)
          .gap(5)
          .foreground(styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 1, 1})
          .foreground(styles::Brackets{kD6, 9, 2, 3, shapes::Corner::All})
          .child(box()
                     .row()
                     .alignItems(Align::Center)
                     .gap(6)
                     .child(t("SIGNAL", sigil::weave::kit::tracked(
                                            blackFace(), 12, kD7, 60, 0.92f)))
                     .child(box().grow(1).height(1).fill(kD4))
                     .child(t("\xc2\xbb", micro(11, kD5, 0))))
          .child(box()
                     .grow(1)
                     .fill(hexColor(0x0D0202))
                     .foreground(styles::BevelPair{kD4, {0, 0, 0, 0.5f}, 1, 1})
                     .row()
                     .alignItems(Align::End)
                     .gap(2)
                     .padding(6)
                     .children(dockBars()))
          .child(
              box()
                  .row()
                  .gap(6)
                  .alignItems(Align::Center)
                  .child(
                      t("GAIN 0.42 \xc2\xb7 SWEEP 20 MS", micro(10, kD6, 220)))
                  .child(box().grow(1))
                  .child(box().width(70).height(8).foreground(styles::TickRail{
                      kD5, 5, 3, 7, 1, 3, 0.5f, path::Edge::Top}))));

  Element cluster =
      box()
          .width(310)
          .height(150)
          .shape(shapes::chamfered(9, shapes::Corner::Diagonal))
          .fill(mskia::Paint::linearUnit(
              {0, 0}, {0, 1}, {{0.0f, kD3}, {1.0f, hexColor(0x0C0202)}}))
          .foreground(inset(5, styles::BevelPair{kD5, {0, 0, 0, 0.6f}, 2, 1}))
          .foreground(styles::Brackets{kD6, 12, 2, 5, shapes::Corner::All})
          .row()
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .gap(14);
  for (int i = 0; i < 3; ++i)
    cluster.child(
        box()
            .width(80)
            .height(80)
            .fill(mskia::Paint::recipe(msdf::material(
                msdf::circle(), {.fill = mskia::toColor(hexColor(0x0A0202)),
                                 .borderWidth = 3,
                                 .borderColor = mskia::toColor(kD6)})))
            .justify(Justify::Center)
            .alignItems(Align::Center)
            .child(radarSweep(i, hexColor(0xB65050), 0.42f))
            .child(box()
                       .inset(26)
                       .corners({16})
                       .fill(mskia::withAlpha(kD1, 0.92f))
                       .stroke(stroke(1, Fill::color(kD4),
                                      PathFormat::Align::Inner)))
            .child(t(i == 0 ? "01" : (i == 1 ? "02" : "03"),
                     micro(11, kD7, 140))));
  strip.child(cluster);
  return strip;
}

auto TwoAdvancedV4::rail(bool right) -> Element {
  using namespace tav;
  Element r = box()
                  .left(Dimension(right ? 1916.0f : 0.0f))
                  .top(Dimension(0))
                  .width(24)
                  .height(Dimension(1560))
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
  o.child(box()
              .inset(0)
              .fill(hexColor(0x120303))
              .opacity(animate(motion::through(
                  {{0ms, 1.0f}, {1400ms, 1.0f}, {1560ms, 0.0f}}))));
  // 1. the single cyan pixel-dot
  o.child(
      at(box().fill(kCyan), cx - 3, cy - 3, 6, 6)
          .opacity(animate(motion::through(
              {{0ms, 0.0f}, {150ms, 1.0f}, {1350ms, 1.0f}, {1450ms, 0.0f}}))));
  // 2. the reticle drawing OUTWARD from it on four trimmed rays
  o.child(hair(cx - 470, cy, 470, 1, -1, 1, 150));
  o.child(hair(cx, cy, 470, 1, 1, 1, 150));
  o.child(hair(cx, cy - 300, 1, 300, 1, -1, 220));
  o.child(hair(cx, cy, 1, 300, 1, 1, 220));
  o.child(
      at(box()
             .shape(shapes::arc(-90, 359))
             .stroke(spans::upTo(animate(motion::from(0.0f).to(1.0f),
                                         {500ms, &ch::easeOutQuint, 260ms})),
                     stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.7f)))),
         cx - 92, cy - 92, 184, 184));
  // 3. the 0→100 readout (a slot: TEXT, so it cannot be a binding)
  o.child(
      at(box().column().alignItems(Align::Center).gap(9), cx - 260, cy + 120,
         520, 110)
          .opacity(animate(motion::through(
              {{520ms, 0.0f}, {620ms, 1.0f}, {1350ms, 1.0f}, {1450ms, 0.0f}})))
          .child(slot("bootpct"))
          .child(box()
                     .width(420)
                     .height(2)
                     .fill(mskia::withAlpha(kCyan, 0.18f))
                     .child(box().inset(0).shape(ray(1, 1)).stroke(
                         spans::upTo(animate(motion::from(0.0f).to(1.0f),
                                             {800ms, &ch::easeNone, 550ms})),
                         stroke(2, Fill::color(kCyan)))))
          .child(t("LOADING PROPHECY INTERFACE \xc2\xb7 970\xc3\x97"
                   "655",
                   micro(11, mskia::withAlpha(kCyan, 0.6f), 240))));
  // 4. the boot-complete flash
  o.child(box()
              .inset(0)
              .fill(SkColor4f{1, 1, 1, 1})
              .opacity(animate(motion::through(
                  {{1330ms, 0.0f}, {1390ms, 0.7f}, {1460ms, 0.0f}})))
              .blend(SkBlendMode::kPlus));
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
      .child(t(buf.c_str(),
               sigil::weave::kit::tracked(blackFace(), 46, kCyan, 40, 0.9f)))
      .child(t("%",
               sigil::weave::kit::tracked(
                   blackFace(), 20, mskia::withAlpha(kCyan, 0.6f), 40, 0.9f)));
}
