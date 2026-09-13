// The explanatory panels and particle census.

#include "GenesisFire.h"

Element GenesisFire::generationPanel() {
  const sigil::data::Json& law = doc()["generation"];
  return panel(kPanelH[0], 1)
      .gap(3)
      .children({panelHead(said(law["head"])),
                 each(law["equations"].items(),
                      [this](const sigil::data::Json& line) {
                        return eqn(said(line));
                      }),
                 box().grow(1), note(said(law["note"])).ink(kSteelDim)});
}

Element GenesisFire::censusBar(float frac, SkColor4f c, const char* key) {
  sketch::kit::Meter bar{.width = Dimension(kCensusW[4]),
                         .height = Dimension(7),
                         .track = Fill::color(hexColor(0x171B24)),
                         .bar = Fill::color(c)};
  bar.level = animate(
      from(0.0f).to(frac),
      {.duration = 420ms, .ease = ease::outBack(1.2f), .delay = 1200ms});
  if (key) bar.level = bind(&liveFrac).clamp(0.02f, 1.0f);
  Element rail = sketch::kit::meter(bar).shrink(0);
  if (key) rail.key(key);
  return rail;
}

Element GenesisFire::censusRow(const sigil::data::Json& row) {
  return box()
      .row()
      .height(14)
      .shrink(0)
      .alignItems(Align::Center)
      .styleClass("cell")
      .children({censusCell(said(row["fig"]), 0, kSteel),
                 censusCell(said(row["systems"]), 1, kBone),
                 censusCell(said(row["particles"]), 2, kBone)
                     .font({.face = monoBoldFace()}),
                 censusCell(said(row["per"]), 3, kSteel),
                 censusBar((float)row["share"].number(), hexColor(0x6D5A3F),
                           nullptr)});
}

Element GenesisFire::liveRow() {
  char parts_[32], per_[24], sys_[16];
  std::snprintf(parts_, sizeof parts_, "%zu,%03zu", liveCount / 1000,
                liveCount % 1000);
  std::snprintf(per_, sizeof per_, "%d",
                (int)std::lround((double)liveCount / (33.0 * kDepth)));
  std::snprintf(sys_, sizeof sys_, "33×%d", kDepth);
  return box()
      .row()
      .height(14)
      .shrink(0)
      .alignItems(Align::Center)
      .styleClass("cell")
      .children({censusCell("THIS", 0, kCyan).font({.face = monoBoldFace()}),
                 censusCell(sys_, 1, kCyan),
                 censusCell(parts_, 2, kCyan).font({.face = monoBoldFace()}),
                 censusCell(per_, 3, kCyan),
                 censusBar(0.0f, kCyan, "livebar")});
}

Element GenesisFire::censusPanel() {
  const sigil::data::Json& census = doc()["census"];
  return panel(kPanelH[1], 2)
      .gap(4)
      .children(
          {panelHead(said(census["head"])),
           box().row().height(11).shrink(0).styleClass("colhead").children(
               {each(census["columns"].items(),
                     [this](const sigil::data::Json& name, size_t i) {
                       return censusCell(said(name), i, kSteelDim);
                     })}),
           box().column().gap(3).shrink(0).staggerChildren(70ms).children(
               {each(census["rows"].items(),
                     [this](const sigil::data::Json& row) {
                       return censusRow(row);
                     }),
                liveRow()}),
           box().grow(1), note(said(census["footnote"])).ink(kSteelDim),
           note(said(census["note"]))});
}

Element GenesisFire::rampPanel() {
  // ONE SWATCH AND ONE NUMBER PER OVERLAP COUNT, the number lit where the
  // count is one of the three a channel saturates at.
  const auto swatch = [](int n) {
    return box()
        .width(28)
        .height(26)
        .shrink(0)
        .fill(Paint::solid(overlap(n)))
        .transformOrigin(0.5f, 1.0f)
        .scaleY(animate(
            from(0.0f).to(1.0f),
            {.duration = 220ms, .ease = ease::outBack(), .delay = 1500ms}));
  };
  const auto count = [](int n) {
    return text(std::to_string(n))
        .styleClass("label")
        .ink(n == 5 || n == 20 || n == 111 ? kBone : kSteelDim)
        .width(28)
        .shrink(0)
        .block({.alignment = sigil::weave::TextAlignment::kCenter});
  };
  const sigil::data::Json& ramp = doc()["ramp"];
  return panel(kPanelH[2], 3)
      .gap(3)
      .children({panelHead(said(ramp["head"])),
                 box().row().gap(2).shrink(0).staggerChildren(26ms).children(
                     {each(kRampN, swatch)}),
                 box().row().gap(2).shrink(0).children({each(kRampN, count)}),
                 box().grow(1), note(said(ramp["note"]))});
}

Element GenesisFire::benchCell(Element content, const Utf8& caption,
                               SkColor4f cc) {
  return box().column().gap(3).width(130).shrink(0).children(
      {box()
           .width(130)
           .height(52)
           .shrink(0)
           .clip(true)
           .fill(hexColor(0x05060A))
           .stroke(stroke(1.0f, Fill::color(hexColor(0x1B2029)),
                          PathFormat::Align::Inner))
           .children({std::move(content)}),
       text(caption).styleClass("label").ink(cc).width(130).block(
           {.alignment = sigil::weave::TextAlignment::kCenter})});
}

Element GenesisFire::renderModelPanel() {
  // THE SAME POOL THREE WAYS: two instanced cells that differ only in
  // blend, and an empty third the pen fills with the field's own quads.
  const auto instanced = [this](SkBlendMode blend) {
    return box().inset(0).children({instancing::instances(
        abAtlas, abPool, instancing::Mode::Live, blend)});
  };
  const sigil::data::Json& model = doc()["render"];
  const sigil::data::Json& cells = model["cells"];
  return panel(kPanelH[3], 4)
      .gap(4)
      .children(
          {panelHead(said(model["head"])),
           box().row().gap(15).shrink(0).children(
               {benchCell(instanced(SkBlendMode::kSrcOver), said(cells[0]),
                          hexColor(0x8A93A8)),
                benchCell(instanced(SkBlendMode::kPlus), said(cells[1]),
                          hexColor(0xFFB672)),
                benchCell(box().inset(0), said(cells[2]), hexColor(0xFFB672))}),
           box().grow(1), note(said(model["note"]))});
}

Element GenesisFire::productionPanel() {
  const sigil::data::Json& made = doc()["production"];
  return panel(kPanelH[4], 5)
      .gap(1)
      .children({panelHead(said(made["head"])),
                 each(made["lines"].items(),
                      [this](const sigil::data::Json& line) {
                        return prodLine(
                            said(line["words"]),
                            line["loud"].boolean() ? kBone : kSteel);
                      }),
                 box().grow(1), note(said(made["note"])).ink(kSteelDim)});
}

Element GenesisFire::header() {
  Track rise{.effect = fx::rise(22),
             .stagger = {.eachMs = 26, .durationMs = 460},
             .progress = animate(
                 from(0.0f).to(1.0f),
                 {.duration = 850ms, .ease = &ch::easeNone, .delay = 120ms})};
  // The masthead is set in the interface face and steel; the title alone
  // takes the black cut and the bone.
  const sigil::data::Json& head = doc()["header"];
  return box()
      .column()
      .height(kHeaderH)
      .shrink(0)
      .gap(4)
      .font({.face = uiFace()})
      .ink(kSteel)
      .children(
          {text(said(head["eyebrow"]))
               .font({.size = 11.5f, .track = 2.7f})
               .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
               .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})),
           text(said(head["title"]))
               .font({.face = heavyFace(), .size = 46, .track = -0.4f})
               .ink(kBone)
               .key("title")
               .fx(std::move(rise)),
           text(said(head["credit"]))
               .font({.size = 11.0f, .track = 0.1f})
               .opacity(animate(from(0.0f).to(1.0f),
                                {.duration = 240ms, .delay = 420ms})),
           box().grow(1),
           kit::line({.fill = Fill::color(kKeyline)})
               .shrink(0)
               .opacity(animate(from(0.0f).to(1.0f),
                                {.duration = 400ms, .delay = 320ms}))});
}
