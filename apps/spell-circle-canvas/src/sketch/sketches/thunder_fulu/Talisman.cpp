#include "ThunderFulu.h"

auto ThunderFulu::voidWriting() -> Element {
  auto g = box().inset(0).key("xushu");
  const SkColor4f cols[3] = {kVoidBlue, kVoidRed, kVoidWhite};
  const char* how[3] = {"left eye", "right eye", "tongue"};
  const SkPoint at[3] = {{kCol - 116, 214}, {kCol + 116, 214}, {kCol, 330}};
  const int src[3] = {WU, GANG, LING};
  for (int k = 0; k < 3; ++k) {
    const std::vector<Poly> ms = medians(font, src[k]);
    SkPathBuilder b;
    for (const auto& m : ms) {
      Poly q = place(m, {at[k].fX - 30.0f, at[k].fY - 30.0f}, 60.0f, 60.0f);
      SkPath sp = cloud(smoothPath(q), 2.2f, 22.0f);
      b.addPath(sp);
    }
    auto hump = [](float t) { return std::sin(t * SK_ScalarPI); };
    g.child(box()
                .left(at[k].fX - 46)
                .top(at[k].fY - 46)
                .width(92)
                .height(92)
                .shape(heldPath(
                    b.detach().makeOffset(-(at[k].fX - 46), -(at[k].fY - 46))))
                .fill(Fill::none())
                .stroke(brush::presets::taper(3.6f, 1.0f, Fill::color(cols[k])))
                .foreground(PathFormat{
                    .width = 7.0f,
                    .strokeFill = Fill::color(
                        {cols[k].fR, cols[k].fG, cols[k].fB, 0.20f})})
                .opacity(bind(&scribe)
                             .window(tVoid + (float)k * 0.16f,
                                     tVoid + tVoidDur + (float)k * 0.16f)
                             .map(hump)
                             .scale(0.92f))
                .key(kit::formatted("void%d", k)));
    g.child(
        text(toUtf8(kit::formatted("%s  \xe2\x80\x94  no mark", how[k])),
             type(faceMono, 10.0f, {cols[k].fR, cols[k].fG, cols[k].fB, 0.85f}))
            .left(at[k].fX - 72)
            .top(at[k].fY + 50)
            .width(168)
            .opacity(bind(&scribe)
                         .window(tVoid + (float)k * 0.16f,
                                 tVoid + tVoidDur + (float)k * 0.16f)
                         .map(hump))
            .key(kit::formatted("voidlbl%d", k)));
  }
  return g;
}

auto ThunderFulu::sealBlock() -> Element {
  const float S = 104.0f, x = 474.0f, y = 786.0f;
  auto g =
      box()
          .left(x)
          .top(y)
          .width(Dimension(S))
          .height(Dimension(S))
          .rotate(-6.0f)
          .transformOrigin(0.5f, 0.5f)
          .opacity(bind(&scribe).window(tSeal, tSeal + 0.45f))
          .scale(bind(&scribe).window(tSeal, tSeal + 0.45f).target(1.5f, 1.0f))
          .key("seal");
  g.child(box()
              .inset(0)
              .shape(shapes::chamfered(6.0f))
              .fill(Fill::color(hexColor(0xb52a17, 0.90f)))
              .foreground(lines::presets::crosshatch(
                  Fill::color(hexColor(0x6d1409, 0.25f)), 5.0f, 0.8f, 18.0f))
              .stroke(PathFormat{
                  .width = 5.0f,
                  .strokeFill = Fill::color(hexColor(0xc23520, 1.0f)),
                  .align = PathFormat::Align::Inner})
              .key("sealground"));
  // 五 above, 雷 below — each STRETCHED to fill its half cell, which is
  // what 篆書 does inside a seal.
  const int half[2] = {WU, LEI};
  for (int k = 0; k < 2; ++k) {
    const std::vector<Poly> ms = medians(font, half[k]);
    SkPathBuilder b;
    for (const Poly& m : ms) {
      Poly q = place(m, {12.0f, 11.0f + (float)k * 41.0f}, S - 24.0f, 40.0f);
      b.addPath(smoothPath(q));
    }
    g.child(box()
                .inset(0)
                .shape(heldPath(b.detach()))
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 4.4f,
                    .strokeFill = Fill::color(hexColor(0xf2e2cf, 0.95f)),
                    .cap = SkPaint::kSquare_Cap,
                    .join = SkPaint::kMiter_Join})
                .key(kit::formatted("sealglyph%d", k)));
  }
  return g;
}

auto ThunderFulu::plate() -> Element {
  auto g = box()
               .left(kPL)
               .top(kPT)
               .width(Dimension(kPW))
               .height(Dimension(kPH))
               .opacity(bind(&scribe).window(tPlate, tPlateEnd))
               .key("plate");
  g.child(ironGround());

  // the spine every component is strung on — a fu is a COLUMN
  g.child(box()
              .inset(0)
              .shape(keyedShape(std::string_view("plate-spine"),
                                [](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(kCol, 28);
                                  b.lineTo(kCol, kPH - 24);
                                  return b.detach();
                                }))
              .fill(Fill::none())
              .stroke(PathFormat{
                  .width = 0.8f,
                  .strokeFill = Fill::color(hexColor(0x0e0d0c, 0.26f)),
                  .dashIntervals = {2.0f, 9.0f}})
              .key("spine"));

  // the four registers, ruled faintly in the margin the way a plate is
  // laid out before it is written
  struct Reg {
    float y;
    const char* label;
  };
  const Reg regs[5] = {{34, "FU TOU  head  \xc2\xb7 3 hooks, 3 Pure Ones"},
                       {184, "FU QIAO  aperture \xc2\xb7 one revolution"},
                       {296, "FU SHEN  body \xc2\xb7 cloud-seal, 33 strokes"},
                       {566, "FU DAN  gall \xc2\xb7 GANG, 10 = 10 stems"},
                       {782, "FU JIAO  foot \xc2\xb7 one breath, 38 strokes"}};
  for (int i = 0; i < 5; ++i) {
    g.child(box()
                .left(18)
                .top(regs[i].y)
                .width(Dimension(kPW - 36))
                .height(1)
                .shape(keyedShape(kPW - 36,
                                  [w = kPW - 36](SkSize) {
                                    SkPathBuilder b;
                                    b.moveTo(0, 0.5f);
                                    b.lineTo(w, 0.5f);
                                    return b.detach();
                                  }))
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 0.7f,
                    .strokeFill = Fill::color(hexColor(0x0e0d0c, 0.28f)),
                    .dashIntervals = {1.5f, 6.0f}})
                .key(kit::formatted("reg%d", i)));
    g.child(text(toUtf8(regs[i].label),
                 type(faceMono, 8.5f, hexColor(0x0b0a09, 0.60f)))
                .left(20)
                .top(regs[i].y + 3)
                .width(360)
                .key(kit::formatted("reglbl%d", i)));
  }

  g.child(inkLayer());
  g.child(voidWriting());
  g.child(sealBlock());
  g.child(ironWash());
  return g;
}

auto ThunderFulu::tread() -> Element {
  const float x0 = 1012, y0 = 108, W = 738, H = 738 * 0.5302f;
  // Where each station's plate stands relative to its star. Six step right
  // and three step left, so the tread's own line stays legible where the
  // handle folds back on itself. KAI YANG (index 5) steps well ABOVE its
  // star instead: the handle turns there, and the plates of its two
  // neighbours already stand in the space below it.
  // JIANG BAO (index 6) hangs 24 px lower than the common 26: at 26 its
  // plate top overlaps JUAN WU's caption band, and the first letters of
  // "roll up fog" are lost against its own lit top edge.
  static const float kOffX[9] = {-56, 48, 48, -56, 48, -58, 50, 54, 52};
  static const float kOffY[9] = {26, 26, 26, 26, 26, -104, 50, 26, 34};
  auto g = box()
               .left(x0)
               .top(y0)
               .width(Dimension(W))
               .height(Dimension(H + 130))
               .key("tread");

  auto S = [&](int i) { return SkPoint{kDipper[i].x * W, kDipper[i].y * W}; };

  // the priest's path between the stations, drawn as LINE ART — the
  // connective tissue is the walk, not a gap. 禹步 「三步九跡」.
  SkPathBuilder walk;
  walk.moveTo(S(0));
  for (int i = 1; i < 7; ++i) walk.lineTo(S(i));
  walk.lineTo(S(7));
  walk.lineTo(S(8));
  const SkPath walkPath = walk.detach();

  g.child(box()
              .inset(0)
              .shape(heldPath(walkPath))
              .fill(Fill::none())
              .stroke(lines::rails(
                  {{.across = 0.0f,
                    .width = 2.6f,
                    .fill = Fill::color(hexColor(0x8b6f36, 0.50f))},
                   {.across = 0.0f,
                    .width = 1.0f,
                    .fill = Fill::color(hexColor(0xd8bd7c, 0.75f)),
                    .dash = {2.0f, 7.0f}}}))
              .foreground(brush::Scatter{.art = footPrint,
                                         .spacing = 46.0f,
                                         .alignToPath = true,
                                         .bleedPx = 18.0f})
              .opacity(bind(&scribe).window(tStars - 0.4f, tStars + 0.5f))
              .key("walkpath"));

  // the nine stations
  for (int i = 0; i < 9; ++i) {
    const SkPoint p = S(i);
    const bool invisible = i == 8;
    const float t = tStars + (float)i * 0.16f;
    auto st = box()
                  .left(p.fX - 13)
                  .top(p.fY - 13)
                  .width(26)
                  .height(26)
                  .opacity(bind(&scribe).window(t, t + 0.45f))
                  .key(kit::formatted("star%d", i));
    st.child(
        box()
            .inset(0)
            .shape(shapes::star(6, 0.30f))
            .fill(invisible ? Fill::none()
                            : Fill::color(hexColor(0xe4c98a, 0.92f)))
            .stroke(PathFormat{
                .width = 1.0f,
                .strokeFill =
                    Fill::color(hexColor(0xe4c98a, invisible ? 0.7f : 0.4f)),
                .dashIntervals = invisible ? std::vector<SkScalar>{2.0f, 2.6f}
                                           : std::vector<SkScalar>{}}));
    g.child(std::move(st));
    // 天璇 is the one station the walk reaches from straight overhead — the
    // Dubhe–Merak leg is 8 px of run over 169 of rise — so at the common
    // 44 px left of the star the tread comes straight down through the
    // ritual name. It hangs to the star's right instead, where that leg
    // has already stopped. Every other ritual name clears its own
    // segments, which is why the rest of the table is 0.
    static const float kRitualDodge[9] = {0, 54, 0, 0, 0, 0, 0, 0, 0};
    g.child(text(toUtf8(kit::formatted("%d %s", i + 1, kDipper[i].ritual)),
                 type(faceMono, 9.5f, hexColor(0xa48c5c, 0.9f)))
                .left(p.fX - 44 + kRitualDodge[i])
                .top(p.fY - 34)
                .width(140)
                .opacity(bind(&scribe).window(t, t + 0.45f))
                .key(kit::formatted("starlbl%d", i)));
    // THE BAYER NAME DODGES THE WALK. It hangs 12 px under its own star,
    // which is clear for the stations the tread leaves sideways and not
    // for the three it leaves downward — at 天樞, 開陽 and 左輔 the walk
    // would cross "Dubhe", "Mizar" and "Alcor" at their x-height. A plate
    // sets the name clear of the line, not on it, so those three carry a
    // dodge: left at 天樞 and 左輔, right at 開陽, where the segment falls
    // the other way. The other six are 0 and stay under their star.
    // 左輔's dodge is 30 and no more: the margin column ends at x = 1000
    // and "Alcor" is only 38 px wide, so a larger left dodge would push it
    // over the margin's own rules.
    static const float kNameDodge[9] = {-34, 0, 0, 0, 0, 30, 0, -30, 0};
    g.child(text(toUtf8(kDipper[i].name),
                 type(faceItalic, 9.0f, hexColor(0x6f6047, 0.85f)))
                .left(p.fX - 22 + kNameDodge[i])
                .top(p.fY + 12)
                .width(140)
                .opacity(bind(&scribe).window(t, t + 0.45f))
                .key(kit::formatted("starnm%d", i)));
  }

  // The eleven other plates. Nine stand on stations; the last two bleed
  // off the sheet, one walking off the right edge and one off the top,
  // because the tread does not stop at the frame. Neither of those two is
  // labelled — a caption cut mid-word reads as an accident, a plate cut
  // mid-edge reads as a frame.
  const SkPoint bleed[2] = {{W + 150, H * 0.16f}, {W * 0.52f, -150.0f}};
  for (int i = 0; i < 11; ++i) {
    SkPoint at = i < 9 ? S(i) : bleed[i - 9];
    if (i < 9) {
      at.fX += kOffX[i];
      at.fY += kOffY[i];
    }
    const float pw = 52.0f, ph = pw * 5.0f / 3.0f;
    const float t = tStars + (float)i * 0.15f + 0.2f;
    auto mp = box()
                  .left(at.fX - pw * 0.5f)
                  .top(at.fY)
                  .width(Dimension(pw))
                  .height(Dimension(ph))
                  .rotate(((i * 37) % 11 - 5) * 0.62f)
                  .transformOrigin(0.5f, 0.0f)
                  .opacity(bind(&scribe).window(t, t + 0.5f))
                  .key(kit::formatted("mini%d", i));
    mp.child(box()
                 .inset(0)
                 .shape(shapes::shaped(shapes::chamfered(5.0f),
                                       shapers::Jitter{14.0f, 1.4f, 7}))
                 .fill(Paint::linearUnit({0.18f, 0.0f}, {0.88f, 1.0f},
                                         {{0.0f, hexColor(0x4a443b)},
                                          {0.55f, hexColor(0x272522)},
                                          {1.0f, hexColor(0x161514)}}))
                 .stroke(PathFormat{
                     .width = 1.2f,
                     .strokeFill = Fill::color(hexColor(0x5b5449, 0.85f)),
                     .align = PathFormat::Align::Inner}));
    // a miniature fu: the same grammar, four marks, generated
    SkPathBuilder b;
    const int srcs[4] = {YU, WU, LEI, YUN};
    const std::vector<Poly> ms = medians(font, srcs[i % 4]);
    const int take = 3 + (i % 3);
    // The rows must FIT the plate for every `take`, and `take` varies from
    // 3 to 5 with i % 3. A fixed pitch cannot do that: at five rows the
    // last ones fall past the bottom edge and land on the plate's own
    // caption, striking a red mark through the gloss. So the band is
    // pitched BETWEEN the head hook and the foot tick and divided by
    // `take`, which contains any count.
    const float rowTop = 24.0f, rowBot = ph - 19.0f;
    const float rowH = std::min(16.0f, (rowBot - rowTop) / (float)take);
    const float pitch =
        take > 1 ? (rowBot - rowTop - rowH) / (float)(take - 1) : 0.0f;
    for (int k = 0; k < take && k < (int)ms.size(); ++k) {
      Poly q = place(ms[(size_t)((k * 5 + i) % ms.size())],
                     {9.0f, rowTop + (float)k * pitch}, pw - 18.0f, rowH);
      b.addPath(cloud(smoothPath(q), 1.8f, 13.0f));
    }
    // the head: one hook. the foot: one tick.
    b.moveTo(14, 12);
    b.quadTo(pw * 0.6f, 10, pw - 16, 20);
    b.moveTo(pw * 0.30f, ph - 16);
    b.quadTo(pw * 0.5f, ph - 8, pw * 0.72f, ph - 18);
    mp.child(
        box()
            .inset(0)
            .shape(heldPath(b.detach()))
            .fill(Fill::none())
            .stroke(brush::presets::taper(3.6f, 1.4f, Fill::color(kCinnaWet))));
    if (i < 9) {
      mp.child(text(toUtf8(kOthers[i].pinyin),
                    type(faceMono, 8.0f, hexColor(0xa89264, 0.95f)))
                   .left(-16)
                   .top(ph + 10)
                   .width(124));
      mp.child(text(toUtf8(kOthers[i].gloss),
                    type(faceItalic, 8.0f, hexColor(0x776953, 0.9f)))
                   .left(-16)
                   .top(ph + 22)
                   .width(124));
    }
    g.child(std::move(mp));
  }
  return g;
}

auto ThunderFulu::furniture() -> Element {
  auto g = box().inset(0).key("furn");
  // title block
  g.child(
      text(toUtf8("WU LEI HAO LING \xc2\xb7 A THUNDER-RITE COMMAND TALISMAN, "
                  "WRITTEN"),
           type(faceDisplay, 22.0f, kChalk, 2.6f))
          .left(76)
          .top(34)
          .width(1400));
  g.child(
      text(toUtf8("DAOFA HUIYUAN DZ 1220, juan 46 \xc2\xb7 iron plate, five "
                  "cun by three \xc2\xb7 written in cinnabar \xc2\xb7 stroke "
                  "medians from makemeahanzi, classes recovered from "
                  "geometry"),
           type(faceMono, 10.5f, kGoldDim))
          .left(76)
          .top(62)
          .width(1500));
  // registration marks at the four corners of the sheet
  for (int i = 0; i < 4; ++i) {
    const float rx = ((unsigned)i & 1u) ? kW - 46 : 46;
    const float ry = ((unsigned)i & 2u) ? kH - 46 : 46;
    g.child(box()
                .left(rx - 11)
                .top(ry - 11)
                .width(22)
                .height(22)
                .shape(keyedShape(std::string_view("register-mark"),
                                  [](SkSize s) {
                                    SkPathBuilder b;
                                    b.moveTo(s.width() * 0.5f, 0);
                                    b.lineTo(s.width() * 0.5f, s.height());
                                    b.moveTo(0, s.height() * 0.5f);
                                    b.lineTo(s.width(), s.height() * 0.5f);
                                    b.addCircle(s.width() * 0.5f,
                                                s.height() * 0.5f,
                                                s.width() * 0.30f);
                                    return b.detach();
                                  }))
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 0.8f,
                    .strokeFill = Fill::color(hexColor(0xb2914f, 0.42f))})
                .key(kit::formatted("reg%d", i + 10)));
  }
  // tick ladder down the plate's left margin — cun and fen
  g.child(box()
              .left(kPL - 26)
              .top(kPT)
              .width(20)
              .height(Dimension(kPH))
              .shape(keyedShape(
                  std::string_view("tick-ladder"),
                  [](SkSize s) {
                    SkPathBuilder b;
                    for (int i = 0; i <= 50; ++i) {
                      const float y = s.height() * (float)i / 50.0f;
                      const float len =
                          (i % 10 == 0) ? 17.0f : (i % 5 == 0 ? 10.0f : 5.0f);
                      b.moveTo(s.width(), y);
                      b.lineTo(s.width() - len, y);
                    }
                    return b.detach();
                  }))
              .fill(Fill::none())
              .stroke(PathFormat{
                  .width = 0.9f,
                  .strokeFill = Fill::color(hexColor(0xb2914f, 0.40f))})
              .key("ladder"));
  for (int i = 0; i <= 5; ++i)
    g.child(text(toUtf8(kit::formatted("%d", i)),
                 type(faceMono, 8.5f, hexColor(0x8b7644)))
                .left(kPL - 46)
                .top(kPT + kPH * (float)i / 5.0f - 5)
                .width(16)
                .key(kit::formatted("ladlbl%d", i)));
  g.child(text(toUtf8("CUN"), type(faceMono, 8.0f, hexColor(0x8b7644)))
              .left(kPL - 52)
              .top(kPT + kPH + 8)
              .width(40));
  // colophon
  g.child(text(toUtf8("BU GANG TA DOU \xc2\xb7 THE TREAD, ON THE REAL DIPPER"),
               type(faceDisplay, 12.0f, kGold, 1.1f))
              .left(1046)
              .top(706)
              .width(830));
  g.child(box()
              .left(1046)
              .top(724)
              .width(830)
              .height(3)
              .shape(keyedShape(std::string_view("rule-830"),
                                [](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(0, 1.5f);
                                  b.lineTo(830, 1.5f);
                                  return b.detach();
                                }))
              .fill(Fill::none())
              .stroke(lines::rails(
                  {{.across = 0.0f,
                    .width = 1.3f,
                    .fill = Fill::color(hexColor(0xb2914f, 0.48f))},
                   {.across = -3.4f,
                    .width = 0.6f,
                    .fill = Fill::color(hexColor(0xb2914f, 0.26f)),
                    .dash = {1.3f, 4.2f}}})));
  g.child(text(toUtf8("Nine stations: J2000 right ascension and declination, "
                      "gnomonically projected about the asterism's own "
                      "centroid. Yu bu is \"three steps, nine prints\"."),
               type(faceItalic, 10.0f, hexColor(0x8d7f60)))
              .left(1046)
              .top(734)
              .width(830));
  g.child(text(toUtf8("Zuo Fu (Alcor) lies 0.008 of the asterism's span from "
                      "Kai Yang (Mizar) on the real sky \xe2\x80\x94 every bu "
                      "gang plate separates the pair by hand, and so does this "
                      "one. You Bi is invisible: its station is doctrine, and "
                      "it is drawn open."),
               type(faceItalic, 10.0f, hexColor(0x6d6047)))
              .left(1046)
              .top(752)
              .width(830));
  g.child(text(toUtf8("Never invert the brush and tap for a pregnant woman or "
                      "a patient with eye disease. \xc2\xb7 SigilCompose study "
                      "\xc2\xb7 no CJK font is loaded: every Han glyph here is "
                      "stroke geometry"),
               type(faceMono, 9.5f, hexColor(0x5d5341)))
              .left(76)
              .top(kH - 34)
              .width(1600));
  return g;
}
