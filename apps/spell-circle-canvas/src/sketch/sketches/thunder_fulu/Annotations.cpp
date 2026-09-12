#include "ThunderFulu.h"

auto ThunderFulu::chantPanel() -> Element {
  auto g = box().left(718).top(796).width(474).key("chant");
  g.child(text(toUtf8("ZHAN HONG FU \xc2\xb7 SEVER THE RAINBOW"),
               type(faceDisplay, 17.0f, kGold, 1.4f))
              .left(0)
              .top(0)
              .width(468));
  g.child(box()
              .left(0)
              .top(24)
              .width(468)
              .height(3)
              .shape(keyedShape(std::string_view("rule-468"),
                                [](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(0, 1.5f);
                                  b.lineTo(468, 1.5f);
                                  return b.detach();
                                }))
              .fill(Fill::none())
              .stroke(lines::rails(
                  {{.across = 0.0f,
                    .width = 1.6f,
                    .fill = Fill::color(hexColor(0xb2914f, 0.55f))},
                   {.across = -4.0f,
                    .width = 0.7f,
                    .fill = Fill::color(hexColor(0xb2914f, 0.30f)),
                    .dash = {1.4f, 4.6f}}})));
  const char* lines_[6] = {
      "SHANG DI YOU LING      The High Emperor has commanded:",
      "YAO NI MIE XING        demon-rainbow, annihilate its form.",
      "FENG DAO CUN ZHAN      The wind-blade cuts it inch by inch.",
      "QING KE LIU XING       In an instant, a shooting star.",
      "JI JI RU LU LING       Swiftly, swiftly, per the statutes.",
      "",
  };
  for (int i = 0; i < 5; ++i) {
    const float t = tGall + (float)i * 0.7f;
    g.child(text(toUtf8(lines_[i]), type(faceMono, 11.5f, kChalk))
                .left(0)
                .top(36 + (float)i * 19)
                .width(468)
                .opacity(bind(&scribe).window(t, t + 0.4f).target(0.22f, 1.0f))
                .key(kit::formatted("chant%d", i)));
  }
  g.child(
      text(toUtf8("\xe2\x80\x9c\xe6\x80\xa5\xe6\x80\xa5\xe5\xa6\x82\xe5\xbe"
                  "\x8b\xe4\xbb\xa4\xe2\x80\x9d is the Han imperial-document "
                  "closing formula, borrowed whole. It ends"),
           type(faceItalic, 10.5f, hexColor(0x7d6f52)))
          .left(0)
          .top(136)
          .width(468));
  g.child(text(toUtf8("almost every fu, and it goes at the FOOT."),
               type(faceItalic, 10.5f, hexColor(0x7d6f52)))
              .left(0)
              .top(150)
              .width(468));
  return g;
}

auto ThunderFulu::logStyle() -> feed::TextOptions {
  feed::TextOptions s;
  // One voice, and the levels are CLASSES over it: a colour each, the
  // face and the size the base's.
  s.styles.base(type(faceMono, 9.6f, hexColor(0x9a8a68)))
      .set("dim", weave::Type{.color = hexColor(0x6d6249)})
      .set("heading", weave::Type{.color = kGold})
      .set("pass", weave::Type{.color = hexColor(0x5fae7f)})
      .set("number", weave::Type{.color = hexColor(0xcf6a4a)})
      .set("fail", weave::Type{.color = hexColor(0xc4483a)});
  s.window.gap = 1.0f;
  s.window.visible = 13;
  return s;
}

auto ThunderFulu::consolePanel() -> Element {
  const feed::TextOptions style = logStyle();
  return kit::console(
             {.feeds = {&logA, &logB, &logC},
              .style = style,
              .plate = {.column = true,
                        .paddingX = 11,
                        .paddingY = 8,
                        .gap = 7,
                        .fill = Fill::color(hexColor(0x131215, 0.88f)),
                        .border = Fill::color(hexColor(0xb2914f, 0.20f)),
                        .divider = Fill::color(hexColor(0xb2914f, 0.14f))}})
      .rect(SkRect::MakeXYWH(1228, 768, 638, 420))
      .key("console");
}

auto ThunderFulu::tempoPanel() -> Element {
  auto g = box().left(718).top(972).width(474).key("tempo");
  g.child(text(toUtf8("YI QI LI DUAN \xc2\xb7 CUT OFF IN ONE BREATH"),
               type(faceDisplay, 13.0f, kGold, 1.2f))
              .left(0)
              .top(0)
              .width(468));
  const char* rows[5] = {
      "FU TOU   head        3 str   1.000 s/stroke   tap fu tou",
      "FU QIAO  aperture    1 rev   1.150 s          ends not meeting",
      "FU SHEN  body       33 str   0.240 s/stroke   cloud-seal",
      "FU DAN   gall       10 str   0.360 s/stroke   chant lands on 10",
      "FU JIAO  foot       38 str   0.034 s/stroke   NO LIFT, FLYING WHITE",
  };
  for (int i = 0; i < 5; ++i)
    g.child(text(toUtf8(rows[i]),
                 type(faceMono, 10.5f,
                      i == 4 ? hexColor(0xcf6a4a) : hexColor(0x9a8a68)))
                .left(0)
                .top(20 + (float)i * 15)
                .width(468));
  g.child(
      text(toUtf8("the foot is 7.1x the body \xe2\x80\x94 doctrine, measured: "
                  "\"the foot is the last"),
           type(faceItalic, 10.5f, hexColor(0x7d6f52)))
          .left(0)
          .top(100)
          .width(468));
  g.child(text(toUtf8("step; total concentration, cut off in a single breath, "
                      "no slowing"),
               type(faceItalic, 10.5f, hexColor(0x7d6f52)))
              .left(0)
              .top(114)
              .width(468));
  g.child(text(toUtf8("or dragging.\"  A fu written at one tempo is not a fu."),
               type(faceItalic, 10.5f, hexColor(0x7d6f52)))
              .left(0)
              .top(128)
              .width(468));
  return g;
}

auto ThunderFulu::marginColumn() -> Element {
  const float X = 718, Wc = 282;
  auto g = box().left(X).top(0).width(Dimension(Wc)).key("margin");

  auto rule = [&](float y, float w) {
    return box()
        .left(0)
        .top(y)
        .width(Dimension(w))
        .height(3)
        .shape(keyedShape(w,
                          [w](SkSize) {
                            SkPathBuilder b;
                            b.moveTo(0, 1.5f);
                            b.lineTo(w, 1.5f);
                            return b.detach();
                          }))
        .fill(Fill::none())
        .stroke(lines::rails({{.across = 0.0f,
                               .width = 1.3f,
                               .fill = Fill::color(hexColor(0xb2914f, 0.48f))},
                              {.across = -3.4f,
                               .width = 0.6f,
                               .fill = Fill::color(hexColor(0xb2914f, 0.26f)),
                               .dash = {1.3f, 4.2f}}}));
  };

  // --- 踏符頭: one chant line per hook, as the hook goes down -----------
  g.child(text(toUtf8("TA FU TOU \xc2\xb7 TREADING THE HEAD"),
               type(faceDisplay, 11.5f, kGold, 1.1f))
              .left(0)
              .top(126)
              .width(Dimension(Wc)));
  g.child(rule(144, Wc));
  for (int k = 0; k < 3; ++k) {
    const float t = tHead + (float)k * (tHeadEach + tHeadGap);
    g.child(text(toUtf8(kHeadChant[k]), type(faceItalic, 11.0f, kChalk))
                .left(0)
                .top(154 + (float)k * 26)
                .width(Dimension(Wc))
                .opacity(bind(&scribe).window(t, t + 0.3f).target(0.14f, 0.98f))
                .key(kit::formatted("hc%d", k)));
  }

  // --- the width law, PLOTTED. 起 · 行 · 收 as one curve ---------------
  const float py = 260, ph = 126, pw = Wc;
  g.child(text(toUtf8("QI / XING / SHOU \xc2\xb7 w(s) OVER ARC LENGTH"),
               type(faceDisplay, 11.5f, kGold, 1.1f))
              .left(0)
              .top(py - 22)
              .width(Dimension(Wc)));
  g.child(rule(py - 5, Wc));
  // STRIP 1 — the band the law actually paints, by the same Ribbon that
  // paints the plate. This is the specimen, not an illustration of one.
  {
    const float bh = 46.0f;
    SkPathBuilder axis;
    axis.moveTo(2, bh * 0.5f);
    axis.lineTo(pw - 2, bh * 0.5f);
    g.child(box()
                .left(0)
                .top(py + 6)
                .width(Dimension(pw))
                .height(Dimension(bh))
                .shape(heldPath(axis.detach()))
                .fill(Fill::none())
                .stroke(brush::Ribbon{
                    .fill = Fill::color(hexColor(0xcf3018, 0.92f)),
                    .step = 1.5f,
                    .width = LawBand{21.0f}})
                .key("lawband"));
    // STRIP 2 — w(s) plotted from a baseline, with the 1.0 reference
    const float cy = py + 6 + bh + 12, chh = ph - bh - 18;
    const float sc = chh / 2.0f;
    g.child(box()
                .left(0)
                .top(cy)
                .width(Dimension(pw))
                .height(Dimension(chh))
                .shape(keyedShape(std::tuple{pw, chh, sc},
                                  [pw, chh, sc](SkSize) {
                                    SkPathBuilder b;
                                    b.moveTo(0, chh - sc);
                                    b.lineTo(pw, chh - sc);
                                    b.moveTo(0, chh);
                                    b.lineTo(pw, chh);
                                    return b.detach();
                                  }))
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 0.7f,
                    .strokeFill = Fill::color(hexColor(0x8b7f66, 0.5f)),
                    .dashIntervals = {1.6f, 4.4f}})
                .key("lawaxis"));
    g.child(box()
                .left(0)
                .top(cy)
                .width(Dimension(pw))
                .height(Dimension(chh))
                // shapes::parametric returns UNIT coordinates (+-1
                // spans the box), so with the baseline at the box's
                // bottom and the 1.0 reference at its middle the plot is
                // exactly v = 1 - w(s). Returning pixels here drew a
                // 40 000 px diagonal across the whole sheet.
                .shape(shapes::parametric(
                    [](float t) {
                      return SkPoint{2.0f * t - 1.0f, 1.0f - widthLaw(t)};
                    },
                    0.0f, 1.0f, 180))
                .fill(Fill::none())
                .stroke(lines::rails(
                    {{.across = 0.0f,
                      .width = 1.5f,
                      .fill = Fill::color(hexColor(0xe6d7ae, 0.95f))},
                     {.across = -3.0f,
                      .width = 0.6f,
                      .fill = Fill::color(hexColor(0xcf3018, 0.55f))}}))
                .key("lawcurve"));
    const char* marks[3] = {"ni feng 1.77", "belly 0.73", "dun 1.42"};
    const float mx[3] = {0.0f, 0.28f, 0.88f};
    const float off[3] = {2, -18, -52};
    for (int i = 0; i < 3; ++i)
      g.child(text(toUtf8(marks[i]), type(faceMono, 8.5f, hexColor(0xa89778)))
                  .left(mx[i] * pw + off[i])
                  .top(cy + chh - widthLaw(mx[i]) * sc + (i == 1 ? 4 : -13))
                  .width(120)
                  .key(kit::formatted("lawmk%d", i)));
    g.child(text(toUtf8("s = distance / fullLength, NOT PathSample::fraction"),
                 type(faceMono, 8.5f, hexColor(0x6f6047)))
                .left(0)
                .top(cy + chh + 4)
                .width(Dimension(pw)));
  }

  // --- the six recovered classes, as specimens -------------------------
  // The block starts at 412 and the chant heading below it at 646, and the
  // gap between them is tight by construction: the grid is three rows at a
  // 62 pitch with each caption hung at cell + 56, which puts the last
  // caption's baseline just above that heading. Close it further and the
  // specimen key reads as the first line of the next section, since a
  // section rule follows the heading immediately. The grid cannot be
  // compressed to buy room — SHU's specimen starts at cell y = 2, already
  // under its own caption — so any change here has to move the heading.
  const float ky = 412;
  g.child(text(toUtf8("SIX CLASSES \xc2\xb7 w0, RECOVERED"),
               type(faceDisplay, 11.5f, kGold, 1.1f))
              .left(0)
              .top(ky)
              .width(Dimension(Wc)));
  g.child(rule(ky + 18, Wc));
  // Each specimen runs in its OWN class's direction, at the class's own
  // w₀, so the key reads as the taxonomy and not as six copies of one
  // curve. Two columns of three; the cell is a 120 × 54 em window.
  static const Poly kSpec[CLSN] = {
      {{4, 30}, {60, 22}, {118, 28}},           // HENG
      {{58, 2}, {64, 26}, {57, 52}},            // SHU
      {{112, 4}, {74, 22}, {6, 52}},            // PIE
      {{6, 4}, {50, 22}, {118, 52}},            // NA
      {{42, 8}, {58, 22}, {72, 38}},            // DIAN
      {{6, 8}, {96, 5}, {106, 16}, {100, 52}},  // TURN
  };
  for (int c = 0; c < CLSN; ++c) {
    const arrange::Cell at = arrange::cellAt((size_t)c, 2);
    const float cx = (at.column == 0) ? 0.0f : 148.0f;
    const float y = ky + 28 + arrange::cellRect(at, {0, 62}).fTop;
    const float w0 = w0ForClass(c) * 128.0f;
    g.child(box()
                .left(cx + 4)
                .top(y)
                .width(126)
                .height(56)
                .shape(heldPath(smoothPath(kSpec[c])))
                .fill(Fill::none())
                .stroke(brush::Ribbon{.fill = Fill::color(kCinnabar),
                                      .step = 1.2f,
                                      .width = LawBand{w0}})
                .key(kit::formatted("spec%d", c)));
    g.child(text(toUtf8(kit::formatted("%s  %.3f em", kClsName[c],
                                       (double)w0ForClass(c))),
                 type(faceMono, 9.0f, hexColor(0xa48c5c)))
                .left(cx)
                .top(y + 56)
                .width(140)
                .key(kit::formatted("speclbl%d", c)));
  }

  // --- the six phrases sung while 罡 is drawn ---------------------------
  const float gy = 646;  // see the note above ky
  g.child(text(toUtf8("SUNG WHILE GANG IS DRAWN"),
               type(faceDisplay, 11.5f, kGold, 1.1f))
              .left(0)
              .top(gy)
              .width(Dimension(Wc)));
  g.child(rule(gy + 18, Wc));
  for (int k = 0; k < 6; ++k) {
    // the chant must finish exactly as the tenth stroke lands
    const float t = tGall + (float)k * (10.0f * tGallEach / 6.0f);
    g.child(
        text(toUtf8(kit::formatted("%d/6  %s", k + 1, kGallChant[k])),
             type(faceItalic, 11.0f, k == 5 ? hexColor(0xe07a52) : kChalk))
            .left(0)
            .top(gy + 28 + (float)k * 18)
            .width(Dimension(Wc))
            .opacity(bind(&scribe).window(t, t + 0.28f).target(0.14f, 0.98f))
            .key(kit::formatted("gc%d", k)));
  }
  g.child(text(toUtf8("the sixth phrase lands on the tenth stroke"),
               type(faceMono, 9.0f, hexColor(0x6f6047)))
              .left(0)
              .top(gy + 132)
              .width(Dimension(Wc)));
  return g;
}
