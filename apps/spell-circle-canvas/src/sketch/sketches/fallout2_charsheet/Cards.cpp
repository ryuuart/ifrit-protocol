#include "Fallout2CharSheet.h"

auto Fallout2CharSheet::poseFor(int skill) const -> fo::Pose {
  if (skill == 0)
    return {48, 44, 0.28f, 0.55f, 1};  // Small Guns — rifle, narrow
  if (skill == 7)
    return {31, 70, 0.62f, 1.15f, 2};  // Doctor — one arm raised, wide
  return {39, 62, 0.42f, 0.85f, 0};    // Melee Weapons
}

auto Fallout2CharSheet::cardContent(int skill) -> Element {
  using namespace fo;
  const SkillDef& d = skills()[(size_t)skill];
  const Pose pose = poseFor(skill);
  Element g = box().inset(0);

  // ---- title + formula on a shared bottom edge -------------------------
  // Fallout puts the formula at 268 + lineHeight(102) - lineHeight(101), a
  // hand-computed baseline alignment across a 28 px and a 10 px face. Same
  // arithmetic here, over the substituted faces' own measured metrics —
  // alignItems(Align::Baseline) would be the kernel spelling, but the two
  // runs are absolutely positioned at documented x/y, not laid out in a row.
  g.child(ink(t(d.name, titleStyle()), 348 - 345, 272 - 267, titleRise));
  const int walkIdx = skill == 0 ? 0 : (skill == 7 ? 1 : 2);
  const float advance = titleAdvance[walkIdx] / kScale;
  g.child(bodyAt(d.formula, kInk, 348 - 345 + advance + 8, 286 - 267));

  // ---- the rule: two 1-px lines at y = 300, 301 ------------------------
  g.child(
      at(box(), 348 - 345, 300 - 267, 613 - 348, 2).fill(Fill::color(kInk)));

  // ---- the illustration, and the FLOAT the copy clears ----------------
  // The exclusion is the keyed node's resolved BOX. Fallout's exclusion is
  // one number — the leftmost inked column, taken as a global minimum over
  // the whole bitmap — which makes its "silhouette float" a constant wrap
  // width for every line, i.e. exactly a rectangle from that column to the
  // right margin. So the keyed node is that rectangle: first ink -> the
  // column's right edge, full illustration height. With margin 8 the copy
  // stops at 484 + inkLeft - 8 and the two rules agree to the pixel.
  // Flowing around the FIGURE's own box instead strands words in the gap to
  // its right, which is correct DTP behaviour and wrong for 1998.
  const float inkX = 484 - 345 + pose.inkLeft;
  g.child(at(box(), inkX, 309 - 267, (613 - 345) - inkX, 128).key("card-ink"));
  Element figNode = at(box(), inkX, 309 - 267, pose.inkWidth, 128);
  // The silhouette is a function of the pose and of nothing else, so the
  // pose is what tells one drawing from another: keyed, the node settles
  // between describes where a bare callable compares equal to nothing.
  figNode.shape(keyedShape(pose, figure(pose)));
  // PathFormat exposes no cap or join, so these open contours end square and
  // mitre at the joints — fine for a 1998 blit.
  figNode.stroke(
      stroke(n(1.15f), Fill::color(kInk), PathFormat::Align::Center));
  g.child(figNode);

  // ---- the body. Greedy first-fit, ragged right, forced 11 px pitch. ---
  // Fallout's wordWrap() is greedy with no hyphenation and the ragged edge
  // is what the screen looks like, so this is deliberately NOT justified and
  // NOT hyphenated. What the screen tests is forced leading, a computed wrap
  // width, a silhouette float, and mixed-size baseline alignment.
  weave::ParagraphBuilder pb(body(kInk));
  pb.addText(toUtf8(d.blurb));
  cardPara = std::make_shared<weave::Paragraph>(pb.build());
  weave::ParagraphLayoutOptions opts;
  opts.alignment = weave::TextAlignment::kStart;
  opts.lineBreakStrategy = weave::LineBreakStrategy::kGreedy;
  opts.hyphenation.enabled = false;
  opts.lineMetrics.height = n(kRowPitch11);  // the forced 11 px pitch, x2
  g.child(text(cardPara, opts)
              .left(Dimension(n(348 - 345)))
              .top(Dimension(n(315 - 267) - n(1.5f)))
              .width(Dimension(n(613 - 348)))
              .flowAround("card-ink", n(8)));
  return g;
}

auto Fallout2CharSheet::card() -> Element {
  using namespace fo;
  Element c = at(box(), 345, 267, 277, 170)
                  .fill(parchMat)
                  .clip()
                  .corners(Corners{n(1)});
  c.overlay(styles::Overlay{parchTooth, SkBlendMode::kSoftLight, 0.55f});
  // creases: two diagonal slivers and one bottom-right scuff. The creases
  // are what sell the card as a stuck-on scrap.
  c.child(
      at(box(), -40, -20, 60, 260)
          .rotate(-16.0f)
          .translateX(n(120))
          .fill(Paint::linearUnit({0, 0}, {1, 0},
                                  {{0.0f, mskia::withAlpha(kRust, 0.0f)},
                                   {0.5f, mskia::withAlpha(kRust, 0.16f)},
                                   {1.0f, mskia::withAlpha(kRust, 0.0f)}})));
  c.child(at(box(), -40, -20, 34, 260)
              .rotate(9.0f)
              .translateX(n(232))
              .fill(Paint::linearUnit(
                  {0, 0}, {1, 0},
                  {{0.0f, mskia::withAlpha(hexColor(0x7C581C), 0.0f)},
                   {0.5f, mskia::withAlpha(hexColor(0x6A4A18), 0.20f)},
                   {1.0f, mskia::withAlpha(hexColor(0x7C581C), 0.0f)}})));
  c.child(at(box(), 150, 120, 130, 55)
              .fill(Paint::radialUnit(
                  {0.55f, 0.75f}, 1.0f,
                  {{0.0f, mskia::withAlpha(kParchScuff, 0.30f)},
                   {1.0f, mskia::withAlpha(kParchScuff, 0.0f)}})));
  c.child(at(box(), -6, -10, 60, 190)
              .fill(Paint::linearUnit(
                  {0, 0}, {1, 0},
                  {{0.0f, mskia::withAlpha(hexColor(0x5A3C10), 0.28f)},
                   {1.0f, mskia::withAlpha(hexColor(0x5A3C10), 0.0f)}})));
  // the scrap's own soiling — kept light: the reference card is bright ochre
  // right into its corners
  c.child(box().inset(0).fill(Paint::radialUnit(
      {0.46f, 0.42f}, 1.35f,
      {{0.0f, mskia::withAlpha(hexColor(0x2A1C08), 0.0f)},
       {0.70f, mskia::withAlpha(hexColor(0x2A1C08), 0.04f)},
       {1.0f, mskia::withAlpha(hexColor(0x2A1C08), 0.22f)}})));
  c.child(at(box(), 178, 118, 110, 60)
              .fill(Paint::radialUnit(
                  {0.60f, 0.85f}, 1.0f,
                  {{0.0f, mskia::withAlpha(hexColor(0x3A2A12), 0.18f)},
                   {1.0f, mskia::withAlpha(hexColor(0x3A2A12), 0.0f)}})));
  c.stroke(stroke(n(1.5f), Fill::color(hexColor(0x2A1C08, 0.75f)),
                  PathFormat::Align::Inner));
  c.child(box().inset(0).child(slot("card")));
  return c;
}

auto Fallout2CharSheet::failureCard() const -> Element {
  using namespace fo;
  sketch::kit::Theme look;
  look.palette.ash = kGreen;
  look.palette.figure = hexColor(0xE04020);
  look.type.sans = bodyFace();
  look.type.mono = bodyFace();
  look.type.captionNote = {17.0f, 0.1f};
  look.type.captionLabel = {17.0f, 0.1f, true};
  look.spacing.rowGap = 6;
  std::vector<sketch::kit::Row> rows;
  for (const sigil::measure::Check& c : sheetAudit.rows) {
    if (c.pass || !c.judged()) continue;
    rows.push_back({{toUtf8(c.label), toUtf8(c.actual),
                     toUtf8("shipped sheet says " + c.expected)},
                    Fill::color(hexColor(0xE04020))});
  }
  sketch::kit::Provide bound(look);
  return box()
      .left(Dimension(120))
      .top(Dimension(160))
      .width(Dimension(kScreenW - 240))
      .height(Dimension(120.0f + 24.0f * (float)rows.size()))
      .fill(Fill::color(hexColor(0x0B0D08, 0.96f)))
      .foreground(stroke(3.0f, Fill::color(hexColor(0xE04020)),
                         PathFormat::Align::Inner))
      .column()
      .padding(28)
      .gap(14)
      .child(text(toUtf8("THE ARITHMETIC DOES NOT MATCH THE SHIPPED "
                         "PREMADES"),
                  sheetType(bodyBold(), 22.0f, hexColor(0xE04020), 1.2f)))
      .child(sketch::kit::table(
          std::move(rows),
          {.columns = {{420}, {90, true}, {}}, .gap = 16, .swatchSide = 11}));
}

auto Fallout2CharSheet::captionBand() -> Element {
  using namespace fo;
  Element band = box()
                     .left(Dimension(0))
                     .top(Dimension(kScreenH))
                     .width(Dimension(kScreenW))
                     .height(Dimension(kCaptionH))
                     .fill(Paint::linearUnit({0, 0}, {0, 1},
                                             {{0.0f, hexColor(0x0B0D08)},
                                              {1.0f, hexColor(0x050604)}}));
  band.foreground(onEdges(
      path::Edge::Top,
      stroke(2.0f, Fill::color(hexColor(0x3A3020)), PathFormat::Align::Inner)));
  const std::string audited = kit::formatted(
      "SEVEN NUMBERS BECOME SIXTY \xc2\xb7 %d/%d derived values "
      "match the shipped sheets (Narg, Mingan, Chitsa), trait "
      "corrections included",
      sheetAudit.checks() - sheetAudit.failures(), sheetAudit.checks());
  auto line = [&](const char* s, float size, SkColor4f c, float y,
                  float track) {
    return text(toUtf8(s), fo::sheetType(bodyFace(), size, c, track))
        .left(Dimension(30))
        .top(Dimension(y));
  };
  band.child(text(toUtf8("FALLOUT 2 \xc2\xb7 CHARACTER SCREEN \xc2\xb7 BLACK "
                         "ISLE STUDIOS, 1998 \xc2\xb7 640\xc3\x97"
                         "480 8-BIT "
                         "INDEXED, REBUILT AT 2\xc3\x97"),
                  fo::sheetType(bodyBold(), 17.0f, kGold, 1.8f))
                 .left(Dimension(30))
                 .top(Dimension(14)));
  band.child(line(audited.c_str(), 14.5f, kGreen, 41, 0.2f));
  band.child(
      line("_colorTable[992] REQUESTS #00FF00; the 256-colour VGA "
           "palette has no pure green, so what reached the CRT is "
           "#3CF800.",
           13.0f, hexColor(0x8A8A78), 64, 0.1f));
  band.child(
      line("Chrome, plaques, rivets, tabs and parchment are "
           "procedural; the originals are raster FRMs (intrface art "
           "id 177). The sheet is RE-SET in real faces.",
           13.0f, hexColor(0x6A6A5A), 84, 0.1f));
  band.child(
      line("The screen above is exactly 1280\xc3\x97"
           "960 \xe2\x80\x94 "
           "halve it and it overlays the 1998 capture. This band is "
           "not part of the artefact.",
           13.0f, hexColor(0x55554A), 104, 0.1f));
  return band;
}
