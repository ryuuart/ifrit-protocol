#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/color/Color.h>

#include "ThunderFulu.h"

auto ThunderFulu::voidWriting() -> Element {
  auto g = box().inset(0).key("xushu");
  const sigil::material::Color cols[3] = {kVoidBlue, kVoidRed, kVoidWhite};
  const std::vector<Utf8> how = wordsOf(doc()["void"]);
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
    g.children(
        {box()
             .rect(SkRect::MakeXYWH(at[k].fX - 46, at[k].fY - 46, 92, 92))
             .shape(heldPath(
                 b.detach().makeOffset(-(at[k].fX - 46), -(at[k].fY - 46))))
             .fill(Fill::none())
             .stroke(brush::presets::taper(3.6f, 1.0f, Fill::color(cols[k])))
             .foreground(PathFormat{
                 .width = 7.0f,
                 .strokeFill =
                     Fill::color({cols[k].r, cols[k].g, cols[k].b, 0.20f})})
             .opacity(bind(&scribe)
                          .window(tVoid + (float)k * 0.16f,
                                  tVoid + tVoidDur + (float)k * 0.16f)
                          .map(hump)
                          .scale(0.92f))
             .key(kit::formatted("void%d", k)),
         text(how[(size_t)k])
             .font({.size = 10.0f,
                    .color =
                        sigil::material::skia::toSkColor(sigil::material::Color{
                            cols[k].r, cols[k].g, cols[k].b, 0.85f})})
             .at({at[k].fX - 72, at[k].fY + 50})
             .width(168)
             .opacity(bind(&scribe)
                          .window(tVoid + (float)k * 0.16f,
                                  tVoid + tVoidDur + (float)k * 0.16f)
                          .map(hump))
             .key(kit::formatted("voidlbl%d", k))});
  }
  return g;
}

auto ThunderFulu::sealBlock() -> Element {
  const float S = 104.0f, x = 474.0f, y = 786.0f;
  auto g =
      box()
          .rect(SkRect::MakeXYWH(x, y, S, S))
          .rotate(-6.0f)
          .transformOrigin(pct(50), pct(50))
          .opacity(bind(&scribe).window(tSeal, tSeal + 0.45f))
          .scale(bind(&scribe).window(tSeal, tSeal + 0.45f).target(1.5f, 1.0f))
          .key("seal");
  g.children(
      {box()
           .inset(0)
           .shape(shapes::chamfered(6.0f))
           .fill(Fill::color(hexColor(0xb52a17, 0.90f)))
           .foreground(lines::presets::crosshatch(
               Fill::color(hexColor(0x6d1409, 0.25f)), 5.0f, 0.8f, 18.0f))
           .stroke(
               PathFormat{.width = 5.0f,
                          .strokeFill = Fill::color(hexColor(0xc23520, 1.0f)),
                          .align = PathFormat::Align::Inner})
           .key("sealground")});
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
    g.children({box()
                    .inset(0)
                    .shape(heldPath(b.detach()))
                    .fill(Fill::none())
                    .stroke(PathFormat{
                        .width = 4.4f,
                        .strokeFill = Fill::color(hexColor(0xf2e2cf, 0.95f)),
                        .cap = SkPaint::kSquare_Cap,
                        .join = SkPaint::kMiter_Join})
                    .key(kit::formatted("sealglyph%d", k))});
  }
  return g;
}

auto ThunderFulu::plate() -> Element {
  auto g = box()
               .rect(SkRect::MakeXYWH(kPL, kPT, kPW, kPH))
               .opacity(bind(&scribe).window(tPlate, tPlateEnd))
               .key("plate");
  // the spine every component is strung on — a fu is a COLUMN
  // the five registers, ruled faintly in the margin the way a plate is
  // laid out before it is written, each naming what is written on it
  g.children(
      {ironGround(),
       box()
           .inset(0)
           .shape(keyedShape(std::string_view("plate-spine"),
                             [] {
                               SkPathBuilder b;
                               b.moveTo(kCol, 28);
                               b.lineTo(kCol, kPH - 24);
                               return b.detach();
                             }))
           .fill(Fill::none())
           .stroke(
               PathFormat{.width = 0.8f,
                          .strokeFill = Fill::color(hexColor(0x0e0d0c, 0.26f)),
                          .dashIntervals = {2.0f, 9.0f}})
           .key("spine"),
       each(doc()["registers"].items(),
            [this](const data::Json& n, size_t i) -> Element {
              const float y = (float)n["y"].number();
              return box().at({18, y}).width(kPW - 36).children(
                  {box()
                       .width(kPW - 36)
                       .height(1)
                       .shape(keyedShape(kPW - 36,
                                         [w = kPW - 36] {
                                           SkPathBuilder b;
                                           b.moveTo(0, 0.5f);
                                           b.lineTo(w, 0.5f);
                                           return b.detach();
                                         }))
                       .fill(Fill::none())
                       .stroke(PathFormat{
                           .width = 0.7f,
                           .strokeFill = Fill::color(hexColor(0x0e0d0c, 0.28f)),
                           .dashIntervals = {1.5f, 6.0f}}),
                   text(n["words"])
                       .styleClass("register")
                       .at({2, 3})
                       .width(360)});
            }),
       inkLayer(), voidWriting(), sealBlock(), ironWash()});
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
  auto g = box().rect(SkRect::MakeXYWH(x0, y0, W, H + 130)).key("tread");

  auto S = [&](int i) { return SkPoint{kDipper[i].x * W, kDipper[i].y * W}; };

  // the priest's path between the stations, drawn as LINE ART — the
  // connective tissue is the walk, not a gap. 禹步 「三步九跡」.
  SkPathBuilder walk;
  walk.moveTo(S(0));
  for (int i = 1; i < 7; ++i) walk.lineTo(S(i));
  walk.lineTo(S(7));
  walk.lineTo(S(8));
  const SkPath walkPath = walk.detach();

  g.children({box()
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
                  .key("walkpath")});

  // the nine stations. Two dodge tables sit here because they are facts
  // about where the WALK falls, not about the stars: 天璇 is reached from
  // straight overhead, so its ritual name hangs to the star's right where
  // the Dubhe–Merak leg has already stopped; and at 天樞, 開陽 and 左輔 the
  // walk would cross "Dubhe", "Mizar" and "Alcor" at their x-height, so
  // those three names step aside. 左輔's step is 30 and no more — the
  // margin column ends at x = 1000 and a larger one would push "Alcor"
  // over its rules. Every other entry is 0 and stays under its star.
  static const float kRitualDodge[9] = {0, 54, 0, 0, 0, 0, 0, 0, 0};
  static const float kNameDodge[9] = {-34, 0, 0, 0, 0, 30, 0, -30, 0};
  g.children({each(kDipper, [&](const Star& star, size_t i) -> Element {
    const SkPoint p = S((int)i);
    const bool invisible = i == 8;
    const float t = tStars + (float)i * 0.16f;
    return box()
        .rect(SkRect::MakeXYWH(p.fX, p.fY, 0, 0))
        .opacity(bind(&scribe).window(t, t + 0.45f))
        .key(kit::formatted("star%d", (int)i))
        .children({box()
                       .rect(SkRect::MakeXYWH(-13, -13, 26, 26))
                       .shape(shapes::star(6, 0.30f))
                       .fill(invisible ? Fill::none()
                                       : Fill::color(hexColor(0xe4c98a, 0.92f)))
                       .stroke(PathFormat{
                           .width = 1.0f,
                           .strokeFill = Fill::color(
                               hexColor(0xe4c98a, invisible ? 0.7f : 0.4f)),
                           .dashIntervals =
                               invisible ? std::vector<SkScalar>{2.0f, 2.6f}
                                         : std::vector<SkScalar>{}}),
                   text(kit::formatted("%d %s", (int)i + 1, star.ritual))
                       .styleClass("station")
                       .at({-44 + kRitualDodge[i], -34})
                       .width(140),
                   text(star.name)
                       .styleClass("bayer")
                       .at({-22 + kNameDodge[i], 12})
                       .width(140)});
  })});

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
                  .rect(SkRect::MakeXYWH(at.fX - pw * 0.5f, at.fY, pw, ph))
                  .rotate(((i * 37) % 11 - 5) * 0.62f)
                  .transformOrigin(pct(50), pct(0))
                  .opacity(bind(&scribe).window(t, t + 0.5f))
                  .key(kit::formatted("mini%d", i));
    mp.children({box()
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
                         .align = PathFormat::Align::Inner})});
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
    mp.children({box()
                     .inset(0)
                     .shape(heldPath(b.detach()))
                     .fill(Fill::none())
                     .stroke(brush::presets::taper(3.6f, 1.4f,
                                                   Fill::color(kCinnaWet)))});
    if (i < 9)
      mp.children({text(kOthers[i].pinyin)
                       .styleClass("miniName")
                       .at({-16, ph + 10})
                       .width(124),
                   text(kOthers[i].gloss)
                       .styleClass("miniGloss")
                       .at({-16, ph + 22})
                       .width(124)});
    g.children({std::move(mp)});
  }
  return g;
}

auto ThunderFulu::furniture() -> Element {
  auto g = box().inset(0).key("furn");
  // the title block
  g.children({box().column().at({76, 34}).width(1500).gap(6).children(
      {text(std::string(doc()["title"].text()))
           .font({.face = faceDisplay,
                  .size = 22.0f,
                  .color = sigil::material::skia::toSkColor(kChalk),
                  .track = 2.6f}),
       text(std::string(doc()["subtitle"].text()))
           .font({.size = 10.5f,
                  .color = sigil::material::skia::toSkColor(kGoldDim)})})});
  // registration marks at the four corners of the sheet
  const std::array<SkPoint, 4> corners{
      {{46, 46}, {kW - 46, 46}, {46, kH - 46}, {kW - 46, kH - 46}}};
  // the tick ladder down the plate's left margin — cun and fen
  g.children(
      {each(corners,
            [](SkPoint at, size_t i) -> Element {
              return box()
                  .rect(SkRect::MakeXYWH(at.fX - 11, at.fY - 11, 22, 22))
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
                  .key(kit::formatted("reg%d", (int)i + 10));
            }),
       box()
           .rect(SkRect::MakeXYWH(kPL - 26, kPT, 20, kPH))
           .shape(keyedShape(std::string_view("tick-ladder"),
                             [](SkSize s) {
                               SkPathBuilder b;
                               for (int i = 0; i <= 50; ++i) {
                                 const float y = s.height() * (float)i / 50.0f;
                                 const float len =
                                     (i % 10 == 0)
                                         ? 17.0f
                                         : (i % 5 == 0 ? 10.0f : 5.0f);
                                 b.moveTo(s.width(), y);
                                 b.lineTo(s.width() - len, y);
                               }
                               return b.detach();
                             }))
           .fill(Fill::none())
           .stroke(
               PathFormat{.width = 0.9f,
                          .strokeFill = Fill::color(hexColor(0xb2914f, 0.40f))})
           .key("ladder")});
  const std::array<int, 6> cun{0, 1, 2, 3, 4, 5};
  g.children({each(cun,
                   [](int i) -> Element {
                     return text(kit::formatted("%d", i))
                         .styleClass("ladder")
                         .at({kPL - 46, kPT + kPH * (float)i / 5.0f - 5})
                         .width(16);
                   }),
              text("CUN")
                  .styleClass("ladder")
                  .font({.size = 8.0f})
                  .at({kPL - 52, kPT + kPH + 8})
                  .width(40)});
  // the colophon over the tread
  const data::Json& colophon = doc()["colophon"];
  g.children({box()
                  .column()
                  .at({1046, 706})
                  .width(830)
                  .gap(6)
                  .children({document::h2(colophon["heading"])
                                 .styleClass("heading")
                                 .font({.size = 12.0f}),
                             rail(830),
                             box().column().width(830).gap(6).children(
                                 {each(colophon["lines"].items(),
                                       [](const data::Json& n) -> Element {
                                         return text(std::string(n.text()))
                                             .styleClass("colophon");
                                       })})}),
              text(std::string(doc()["footer"].text()))
                  .styleClass("note")
                  .at({76, kH - 34})
                  .width(1600)});
  return g;
}
