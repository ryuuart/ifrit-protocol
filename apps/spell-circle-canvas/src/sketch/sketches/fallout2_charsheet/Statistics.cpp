#include "Fallout2CharSheet.h"

auto Fallout2CharSheet::kills() -> const std::array<Kill, 9>& {
  // proto.msg 1450-1468; counts are scenario; sorted case-insensitively by
  // name (characterEditorKillsCompare -> compat_stricmp) and only species
  // with a non-zero count appear.
  static const std::array<Kill, 9> v = {{{"Brahmin", 2},
                                         {"Dogs", 4},
                                         {"Geckos", 11},
                                         {"Giant Ants", 6},
                                         {"Men", 9},
                                         {"Plants", 3},
                                         {"Radscorpions", 5},
                                         {"Rats", 23},
                                         {"Women", 1}}};
  return v;
}

auto Fallout2CharSheet::specialColumn() -> Element {
  using namespace fo;
  Element g = box().inset(0);
  static const char* abbr[7] = {"ST", "PE", "EN", "CH", "IN", "AG", "LK"};
  for (int i = 0; i < 7; ++i) {
    const float y = kStatY[(size_t)i];
    const int value = narg[i];

    // The two-letter gold abbreviation and its dash: measured ink 20..55,
    // 21 px tall, i.e. 2 px below the row's y. A worn engraved stencil.
    g.child(ink(engravedText(std::string(abbr[(size_t)i]) + "-", n(29.0f),
                             engravedCondense, 0.2f)
                    .opacity(0.94f),
                kAbbrX, y + 2.0f, engravedRise(n(29.0f))));

    // The two-digit odometer: BIG_NUM_WIDTH 14 x BIG_NUM_HEIGHT 24, white
    // digits on counter wheels with a seam across the digit. That seam is
    // what makes them read as wheels rather than as a readout, which is why
    // they animate the way they do.
    g.child(odometer(kOdoX, y, value, "", ""));

    // The value descriptor on its dark plaque with the gold bevel along the
    // bottom-left. stat.msg 301-310, indexed by the VALUE.
    Element plaque = at(box(), kPlaqueX, y + 4, kPlaqueW, 17)
                         .fill(Fill::color(kWell))
                         .corners(Corners{n(1.5f)});
    plaque.background(
        styles::dropShadow(hexColor(0x000000, 0.6f), {0, n(1)}, n(2)));
    plaque.foreground(
        styles::InnerShadow{hexColor(0x000000, 0.75f), {0, n(1.2f)}, n(2.0f)});
    plaque.stroke(stroke(n(1), Fill::color(hexColor(0x14100A)),
                         PathFormat::Align::Inner));
    g.child(plaque);
    // the gold L: a bar under the plaque running 4 px past its left edge,
    // and a short riser (sampled at y = 59..60, x from 96)
    g.child(at(box(), kPlaqueX - 4, y + 22, kPlaqueW + 6, 2)
                .fill(Paint::linearUnit(
                    {0, 0}, {0, 1},
                    {{0.0f, kParchLit2}, {1.0f, hexColor(0x8C6428)}})));
    g.child(at(box(), kPlaqueX - 4, y + 15, 2, 7)
                .fill(mskia::withAlpha(kParchLit, 0.85f)));
    g.child(bodyAt(descriptor(value), kDescX, y + 8));
  }
  return g;
}

auto Fallout2CharSheet::odometer(float x, float y, int value,
                                 const std::string& tensOverride,
                                 const std::string& onesOverride) -> Element {
  using namespace fo;
  Element g = at(box(), x - 1, y - 1, kOdoW * 2 + 2, kOdoH + 2)
                  .fill(Fill::color(hexColor(0x000000)))
                  .corners(Corners{n(2)});
  g.foreground(
      stroke(n(1), Fill::color(hexColor(0x0A0A0A)), PathFormat::Align::Inner));
  const std::string digits =
      (value < 10 ? "0" : "") + std::to_string(std::clamp(value, 0, 99));
  for (int c = 0; c < 2; ++c) {
    Element wheel = at(box(), 1 + kOdoW * (float)c, 1, kOdoW - 1, kOdoH)
                        .fill(wheelMat)
                        .corners(Corners{n(2.5f)})
                        .clip();
    std::string glyph(1, digits[(size_t)c]);
    if (c == 0 && !tensOverride.empty())
      glyph = tensOverride == " " ? "" : tensOverride;
    if (c == 1 && !onesOverride.empty())
      glyph = onesOverride == " " ? "" : onesOverride;
    if (!glyph.empty())
      wheel.child(box()
                      .inset(0)
                      .justify(Justify::Center)
                      .alignItems(Align::Center)
                      .child(t(glyph, fo::sheetType(digitFace(), n(20.0f),
                                                    kDigit, 0, 0.98f))
                                 .translateY(n(0.7f))));
    // NO SEAM ACROSS THE DIGIT. The wheels are the mechanism and the
    // 123 ms blank is how it reads on screen, but the capture's BIG_NUM
    // glyphs are solid beige bitmaps on a dark inset with nothing across
    // them — a rule through the middle turns a 14x24 numeral into a
    // flip-clock digit at this size. What stays is the well's own inner
    // keyline, which the sprite sheet does have.
    wheel.foreground(onEdges(
        path::Edge::All, stroke(n(0.7f), Fill::color(hexColor(0x000000, 0.75f)),
                                PathFormat::Align::Inner)));
    g.child(wheel);
  }
  return g;
}

auto Fallout2CharSheet::statusBlock() -> Element {
  using namespace fo;
  Element g = box().inset(0);
  g.child(bodyAt("Hit Points", 194, 46));
  g.child(bodyAt(kit::formatted("%d/%d", stats.hitPoints, stats.hitPoints), 263,
                 46));
  static const char* cond[7] = {"Poisoned",          "Radiated",
                                "Eye Damage",        "Crippled Right Arm",
                                "Crippled Left Arm", "Crippled Right Leg",
                                "Crippled Left Leg"};
  for (int i = 0; i < 7; ++i)
    g.child(bodyAt(cond[(size_t)i], 194, 46 + kRowPitch13 * (float)(i + 1))
                .ink(kInactive));
  return g;
}

auto Fallout2CharSheet::derivedBlock() -> Element {
  using namespace fo;
  Element g = box().inset(0);
  struct Row {
    const char* label;
    std::string value;
  };
  const std::array<Row, 10> rows = {{
      {"Armor Class", std::to_string(stats.armorClass)},
      {"Action Points", std::to_string(stats.actionPoints)},
      {"Carry Weight", std::to_string(stats.carryWeight)},
      {"Melee Damage", std::to_string(stats.meleeDamage)},
      {"Damage Res.", std::to_string(stats.damageResist) + "%"},
      {"Poison Res.", std::to_string(stats.poisonResist) + "%"},
      {"Radiation Res.", std::to_string(stats.radResist) + "%"},
      {"Sequence", std::to_string(stats.sequence)},
      {"Healing Rate", std::to_string(stats.healingRate)},
      {"Critical Chance", std::to_string(stats.criticalChance) + "%"},
  }};
  // The labels are CONDENSED, the values are not. 194 -> 288 is 94 original
  // px and "Critical Chance" is 15 characters: at the block's 6.15 px
  // monospaced advance the label ends 1.7 px short of the value column and
  // its final `e` sits against the `5`, which is not what a proportional
  // font 101 does. 0.96 buys 3.7 px of gap and is the same discipline the
  // plaque type already uses — condense the substitute onto the measured
  // original width rather than let the advance decide the column.
  constexpr float kLabelCondense = 0.96f;
  for (int i = 0; i < 10; ++i) {
    const float y = 179 + kRowPitch13 * (float)i;
    g.child(bodyAt(rows[(size_t)i].label, 194, y, kLabelCondense));
    g.child(bodyAt(rows[(size_t)i].value, 288, y));
  }
  return g;
}

auto Fallout2CharSheet::levelBlock() -> Element {
  using namespace fo;
  Element g = box().inset(0);
  g.child(bodyAt("Level: " + std::to_string(kLevel), 32, 280));
  g.child(bodyAt("Exp: " + thousands(experienceForLevel(kLevel)), 32, 291));
  g.child(bodyAt("Next Level: " + thousands(experienceForLevel(kLevel + 1)), 32,
                 302));
  return g;
}

auto Fallout2CharSheet::folder() -> Element {
  using namespace fo;
  Element g = box().inset(0);

  // The tab strip, blitted at (11, 327). Hit split points x<110 -> PERKS,
  // 110..208 -> KARMA, >208 -> KILLS, so those ARE the tab boundaries.
  static const char* tabs[3] = {"PERKS", "KARMA", "KILLS"};
  const float tabX[4] = {25, 110, 208, 300};
  for (int i = 0; i < 3; ++i) {
    const bool sel = i == 2;
    const float x0 = tabX[i], x1 = tabX[i + 1];
    Element tab =
        raised({x0, sel ? 327.0f : 330.0f, x1 - x0, sel ? 33.0f : 29.0f}, 2.5f);
    if (!sel) tab.overlay(styles::colorOverlay(hexColor(0x000000, 0.30f)));
    tab.justify(Justify::Center).alignItems(Align::Center);
    tab.child(engravedText(tabs[(size_t)i], n(23.0f), engravedCondense, 0.3f)
                  .ink(sel ? kGold : hexColor(0x6E5A20))
                  .translateY(sel ? n(-1.0f) : n(0.0f)));
    g.child(tab);
  }

  // Nine leader rows from y = 364, pitch 11, between x = 34 and x = 314.
  for (int i = 0; i < 9; ++i) {
    const Kill& k = kills()[(size_t)i];
    const float y = 364 + kRowPitch11 * (float)i;
    const std::string count = std::to_string(k.count);
    const float nameW = killNameW[(size_t)i] / kScale;  // back to orig px
    const float countW = killCountW[(size_t)i] / kScale;
    const float gap = 1.0f;  // fontGetLetterSpacing() at this size
    g.child(bodyAt(k.name, 34, y));
    g.child(bodyAt(count, 314 - countW, y));
    const float x0 = 34 + nameW + gap * 2;
    const float x1 = 314 - countW - gap * 2;
    if (x1 > x0)
      g.child(at(box(), x0, y + kRowPitch11 * 0.5f - 1.0f, x1 - x0, 1)
                  .fill(Fill::color(mskia::withAlpha(kGreen, 0.85f))));
  }

  // The scroll arrows at x = 317 (characterEditorFolderViewClear).
  for (int i = 0; i < 2; ++i) {
    const bool up = i == 0;
    Element a = at(box(), 317, up ? 361.0f : 456.0f, 11, 12)
                    .fill(Paint::linearUnit({0, 0}, {0, 1},
                                            {{0.0f, hexColor(0x50432E)},
                                             {1.0f, hexColor(0x2C2418)}}))
                    .corners(Corners{n(1)});
    a.foreground(stroke(n(0.8f), Fill::color(hexColor(0x1A1610)),
                        PathFormat::Align::Inner));
    // The arrowhead spans the whole cell, so it is not the inscribed
    // `shapes::polygon(3)` — it is a triangle keyed on the one thing it
    // is a function of, which is the direction it points.
    a.child(at(box(), 2, 3, 7, 6)
                .fill(Fill::color(mskia::withAlpha(kGold, 0.9f)))
                .shape(keyedShape(up, [up](SkSize s) {
                  SkPathBuilder b;
                  if (up) {
                    b.moveTo(s.width() * 0.5f, 0);
                    b.lineTo(s.width(), s.height());
                    b.lineTo(0, s.height());
                  } else {
                    b.moveTo(0, 0);
                    b.lineTo(s.width(), 0);
                    b.lineTo(s.width() * 0.5f, s.height());
                  }
                  b.close();
                  return b.detach();
                })));
    g.child(a);
  }
  return g;
}

auto Fallout2CharSheet::skillsColumn() -> Element {
  using namespace fo;
  Element g = box().inset(0);
  for (int i = 0; i < 18; ++i) {
    const float y = 27 + kRowPitch11 * (float)i;
    const SkColor4f c = i == selected       ? kSelected
                        : tagged[(size_t)i] ? kTagged
                                            : kGreen;
    int pct = skillPct[(size_t)i];
    if (i == selected)
      pct += 2 * presses;  // 1 point = 2% on a tagged skill (editor.msg {500})
    g.child(bodyAt(skills()[(size_t)i].name, 380, y).ink(c));
    g.child(bodyAt(std::to_string(pct) + "%", 573, y).ink(c));
  }
  // The +/- slider graphic, blitted at (592, selectedIndex*11 + 27 + 16),
  // its buttons at x = 614.
  const float sy = (float)selected * kRowPitch11 + 27 + 16;
  Element slider =
      at(box(), 592, sy - 12, 36, 24).fill(tabMat).corners(Corners{n(2)});
  slider.foreground(fo::stamp(1.2f, 1.4f, hexColor(0xA08858, 0.5f),
                              hexColor(0x0C0906, 0.6f)));
  for (int k = 0; k < 2; ++k) {
    Element btn =
        at(box(), 22, 2 + 11.0f * (float)k, 12, 9)
            .fill(Fill::color(k == 0 ? hexColor(0x3A3020) : hexColor(0x2A2418)))
            .corners(Corners{n(1.5f)});
    btn.foreground(stroke(n(0.8f), Fill::color(hexColor(0x8A7448, 0.7f)),
                          PathFormat::Align::Inner));
    if (k == 0)  // the `+` lamp flashes on each spend
      btn.child(box()
                    .inset(0)
                    .fill(Fill::color(kLampOn))
                    .corners(Corners{n(1.5f)})
                    .opacity(&plusFlash));
    btn.child(box()
                  .inset(0)
                  .justify(Justify::Center)
                  .alignItems(Align::Center)
                  .child(text(toUtf8(k == 0 ? "+" : "-"))
                             .font({.size = n(7.0f), .color = kGold})));
    slider.child(btn);
  }
  slider.child(at(box(), 2, 6, 16, 12)
                   .fill(Fill::color(hexColor(0x141008)))
                   .corners(Corners{n(1)}));
  g.child(slider);
  return g;
}
