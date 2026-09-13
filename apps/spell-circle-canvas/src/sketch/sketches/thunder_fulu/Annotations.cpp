#include "ThunderFulu.h"

auto ThunderFulu::rail(float w) const -> Element {
  // THE PLATE'S ONE DIVIDER: a gold rule with a dashed hairline held off
  // under it, the way an engraved sheet rules a section off. Every head on
  // this plate stands over one.
  return box()
      .width(w)
      .height(3)
      .shape(keyedShape(w,
                        [w] {
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
}

auto ThunderFulu::sung(const data::Json& said, float t0, float each, float hold,
                       const char* tag) const -> std::vector<Element> {
  // A CHANT IS SUNG WHILE THE STROKE GOES DOWN: each line is windowed on
  // the same Output the ink is, so the phrase and the stroke it belongs to
  // cannot drift apart.
  return ::each(
      said.items(),
      [this, t0, each, hold, tag](const data::Json& n, size_t i) -> Element {
        const float t = t0 + (float)i * each;
        const std::string named(n["style"].text("chant"));
        return text(spoken(n))
            .styleClass(named)
            .opacity(bind(&scribe).window(t, t + hold).target(0.14f, 0.98f))
            .key(kit::formatted("%s%d", tag, (int)i));
      });
}

auto ThunderFulu::chantPanel() -> Element {
  const data::Json& said = doc()["chant"];
  // the chant's lines are the terminal face in chalk; the head is the
  // section heading, larger
  return box()
      .column()
      .at({718, 796})
      .width(474)
      .gap(8)
      .key("chant")
      .font({.size = 11.5f})
      .ink(kChalk)
      .children({text(std::string(said["heading"].text()))
                     .styleClass("heading")
                     .font({.size = 17.0f, .track = 1.4f}),
                 rail(468),
                 box().column().width(468).gap(7).children(
                     {sung(said["lines"], tGall, 0.7f, 0.4f, "chant")}),
                 text(std::string(said["gloss"].text()))
                     .styleClass("gloss")
                     .width(468)});
}

auto ThunderFulu::logStyle() -> feed::TextOptions {
  feed::TextOptions s;
  // One voice, and the levels are CLASSES over it: a colour each, the
  // face and the size the base's.
  s.styles
      .base(weave::textStyle(
          {.face = faceMono, .size = 9.6f, .color = hexColor(0x9a8a68)}))
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
  const data::Json& said = doc()["tempo"];
  // THE FIVE COMPONENTS AND WHAT EACH IS WRITTEN AT: a table of five
  // columns — the component, what it is, how many strokes, the tempo, and
  // what happens there. The last row is the foot, and its tempo is the
  // whole point: 0.034 s a stroke against the body's 0.240.
  std::vector<sketch::kit::Row> rows;
  for (const data::Json& r : said["rows"].items())
    rows.push_back({.cells = wordsOf(r)});
  return box()
      .column()
      .at({718, 972})
      .width(474)
      .gap(8)
      .key("tempo")
      .font({.size = 10.5f})
      .ink(hexColor(0x9a8a68))
      .children({text(std::string(said["heading"].text()))
                     .styleClass("heading")
                     .font({.size = 13.0f, .track = 1.2f}),
                 sketch::kit::table(std::move(rows),
                                    {.columns = {{62}, {72}, {50}, {112}, {}}})
                     .width(468)
                     .key("tempoRows"),
                 text(std::string(said["gloss"].text()))
                     .styleClass("gloss")
                     .width(468)});
}

auto ThunderFulu::widthLawPlot(float w) const -> Element {
  // WHAT THE TWO AXES MEAN: arc length across, the width law's multiple of
  // w₀ up, with 1.0 ruled so the belly and the two swells are read against
  // it. The curve is the same widthLaw() the ink is painted by, so this is
  // a specimen of the law and not an illustration of one.
  const data::Json& said = doc()["margin"]["law"];
  std::vector<sketch::kit::Layer> layers{
      sketch::kit::rules({.y = {0.0, 1.0}}),
      sketch::kit::trace([](double s) { return widthLaw((float)s); },
                         {.width = 1.5f})};
  for (const data::Json& m : said["marks"].items())
    layers.push_back(sketch::kit::label(std::string(m["words"].text()),
                                        m["s"].number(),
                                        widthLaw((float)m["s"].number()),
                                        {.anchor = {.across = Align::Start,
                                                    .down = Align::End,
                                                    .offset = {2, -4}},
                                         .styleClass = "lawMark"}));
  return sketch::kit::plot(
             "law",
             {.x = {.domain = {0, 1}}, .y = {.domain = {0, 2}}, .pad = 2},
             std::move(layers))
      .width(w)
      .height(62);
}

auto ThunderFulu::marginColumn() -> Element {
  const data::Json& said = doc()["margin"];
  const float X = 718, Wc = 282;
  auto g = box().at({X, 0}).width(Wc).key("margin");

  // --- 踏符頭: one chant line per hook, as the hook goes down -----------
  g.children({box().column().at({0, 126}).width(Wc).gap(8).children(
      {text(std::string(said["head"]["heading"].text())).styleClass("heading"),
       rail(Wc),
       box().column().width(Wc).gap(13).children(
           {sung(said["head"]["lines"], tHead, tHeadEach + tHeadGap, 0.3f,
                 "hc")})})});

  // --- the width law: the band it paints, and the law itself, plotted ---
  const float py = 260;
  g.children(
      {box()
           .column()
           .at({0, py - 22})
           .width(Wc)
           .gap(6)
           .children(
               {text(std::string(said["law"]["heading"].text()))
                    .styleClass("heading"),
                rail(Wc),
                // THE BAND THE LAW ACTUALLY PAINTS, by the same Ribbon that
                // paints the plate.
                box()
                    .width(Wc)
                    .height(46)
                    .shape(keyedShape(Wc,
                                      [Wc] {
                                        SkPathBuilder axis;
                                        axis.moveTo(2, 23);
                                        axis.lineTo(Wc - 2, 23);
                                        return axis.detach();
                                      }))
                    .fill(Fill::none())
                    .stroke(brush::Ribbon{
                        .fill = Fill::color(hexColor(0xcf3018, 0.92f)),
                        .step = 1.5f,
                        .width = LawBand{21.0f}})
                    .key("lawband"),
                widthLawPlot(Wc),
                text(std::string(said["law"]["note"].text()))
                    .font({.size = 8.5f, .color = hexColor(0x6f6047)})})});

  // --- the six recovered classes, as specimens -------------------------
  // Each specimen runs in its OWN class's direction, at the class's own
  // w₀, so the key reads as the taxonomy and not as six copies of one
  // curve. Two columns of three; the cell is a 126 × 54 em window, and the
  // caption hangs under it.
  static const Poly kSpec[CLSN] = {
      {{4, 30}, {60, 22}, {118, 28}},           // HENG
      {{58, 2}, {64, 26}, {57, 52}},            // SHU
      {{112, 4}, {74, 22}, {6, 52}},            // PIE
      {{6, 4}, {50, 22}, {118, 52}},            // NA
      {{42, 8}, {58, 22}, {72, 38}},            // DIAN
      {{6, 8}, {96, 5}, {106, 16}, {100, 52}},  // TURN
  };
  const float ky = 412;
  g.children({text(std::string(said["classes"]["heading"].text()))
                  .styleClass("heading")
                  .at({0, ky})
                  .width(Wc),
              rail(Wc).at({0, ky + 18}),
              each(kSpec, [&](const Poly& spec, size_t c) -> Element {
                const arrange::Cell at = arrange::cellAt(c, 2);
                const float cx = (at.column == 0) ? 0.0f : 148.0f;
                const float y = ky + 28 + arrange::cellRect(at, {0, 62}).fTop;
                const float w0 = w0ForClass((int)c) * 128.0f;
                return box().at({cx, y}).width(140).children(
                    {box()
                         .rect(SkRect::MakeXYWH(4, 0, 126, 56))
                         .shape(heldPath(smoothPath(spec)))
                         .fill(Fill::none())
                         .stroke(brush::Ribbon{.fill = Fill::color(kCinnabar),
                                               .step = 1.2f,
                                               .width = LawBand{w0}})
                         .key(kit::formatted("spec%d", (int)c)),
                     text(kit::formatted("%s  %.3f em", kClsName[c],
                                         (double)w0ForClass((int)c)))
                         .font({.size = 9.0f, .color = hexColor(0xa48c5c)})
                         .at({0, 56})
                         .width(140)});
              })});

  // --- the six phrases sung while 罡 is drawn ---------------------------
  // The block above ends at 412 and this heading stands at 646: the gap is
  // tight by construction, since the last specimen's caption baseline sits
  // just above it and a section rule follows this heading immediately.
  const float gy = 646;
  g.children({box().column().at({0, gy}).width(Wc).gap(8).children(
      {text(std::string(said["gall"]["heading"].text())).styleClass("heading"),
       rail(Wc),
       box().column().width(Wc).gap(5).children(
           // the chant must finish exactly as the tenth stroke
           // lands, so its step is ten strokes over six phrases
           {sung(said["gall"]["lines"], tGall, 10.0f * tGallEach / 6.0f, 0.28f,
                 "gc")}),
       text(std::string(said["gall"]["note"].text()))
           .font({.size = 9.0f, .color = hexColor(0x6f6047)})})});
  return g;
}
