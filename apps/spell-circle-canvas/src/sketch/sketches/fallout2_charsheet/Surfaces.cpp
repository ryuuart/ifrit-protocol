#include "Fallout2CharSheet.h"

auto Fallout2CharSheet::plaqueType(float size, SkColor4f c, float condense,
                                   float track) const -> weave::TextStyle {
  return weave::textStyle({.face = fo::engraved(),
                           .size = size,
                           .color = c,
                           .track = fo::n(track),
                           .condense = condense});
}

auto Fallout2CharSheet::titleStyle() const -> weave::TextStyle {
  return weave::textStyle({.face = fo::titleFace(),
                           .size = fo::n(23.8f),
                           .color = fo::kInk,
                           .track = fo::n(-0.1f),
                           .condense = titleCondense});
}

auto Fallout2CharSheet::buildSurfaces() -> void {
  using namespace fo;
  // The plate: an olive ramp, a cast-metal tooth, and rust blotches. The
  // tooth and rust ride as separate LAYER ELEMENTS with node opacity.
  // `Paint::amount` thins a blend layer's own alpha before the mode
  // sees it, which is a different picture from compositing a whole
  // element at an opacity.
  // Sampled off the capture: the plate sits between #302820 and #383020 with
  // lit facets up to #483828 — a narrow band, so the ramp is shallow and the
  // grain does the work.
  // ON THE SAMPLED BAND, not a stop over it. The plate reads dark grimy
  // olive-steel on the capture and a fifth of a stop lighter turns it
  // into painted card, which is most of what made this sheet read warm
  // and flat beside the reference.
  plateMat = Paint::linearUnit({0, 0}, {0.15f, 1},
                               {{0.0f, hexColor(0x483828)},
                                {0.40f, hexColor(0x383020)},
                                {1.0f, hexColor(0x302820)}});
  plateTooth = Paint::recipe(field::grain(0.22f, 3, 11.0f, 0.65f, 1.0f));
  rustMat =
      Paint::blend({{Paint::solid(kRust), SkBlendMode::kSrcOver},
                    {Paint::recipe(field::grain(0.0075f, 3, 5.0f, 1.35f, 1.0f)),
                     SkBlendMode::kMultiply}});
  wellMat = Paint::blend({{Paint::solid(kWell), SkBlendMode::kSrcOver},
                          {Paint::recipe(field::grain(0.35f, 2, 17.0f, 0.10f)),
                           SkBlendMode::kOverlay}});
  // The odometer drum: a cylinder, bright just above centre, dark below,
  // lifting again at the bottom lip. Sampled off the capture at x=61.
  wheelMat = Paint::linearUnit({0, 0}, {0, 1},
                               {{0.0f, hexColor(0x3C3C3C)},
                                {0.22f, hexColor(0x545454)},
                                {0.58f, hexColor(0x282828)},
                                {0.80f, hexColor(0x1C1C1C)},
                                {1.0f, hexColor(0x383838)}});
  rivetMat = Paint::radialUnit({0.34f, 0.30f}, 1.15f,
                               {{0.0f, hexColor(0x6A5838)},
                                {0.55f, hexColor(0x3A3020)},
                                {1.0f, hexColor(0x140F08)}});
  tabMat = Paint::linearUnit(
      {0, 0}, {0, 1}, {{0.0f, kPlateLit}, {0.55f, kPlate}, {1.0f, kPlateDark}});
  // The parchment card: the richest surface here. Base ochre, a large-scale
  // mottle, a paper tooth, creases added as elements below.
  // Sampled across the reference card on a 44x32 grid: it sits between
  // #9C7434 and #BC9054 almost everywhere, with #8C6428 creases. Bright, not
  // moody — a vignette here turns the scrap into leather.
  parchMat =
      Paint::blend({{Paint::linearUnit({0.10f, 0}, {0.90f, 1},
                                       {{0.0f, kParchLit2},
                                        {0.30f, kParchLit},
                                        {0.66f, kParch},
                                        {1.0f, kParchDark}}),
                     SkBlendMode::kSrcOver},
                    {Paint::recipe(field::grain(0.013f, 4, 21.0f, 0.62f, 1.4f)),
                     SkBlendMode::kOverlay}});
  parchTooth = Paint::recipe(field::grain(0.40f, 2, 7.0f, 0.42f, 1.0f));
  canvasGrain = Paint::recipe(field::grain(0.9f, 1, 31.0f, 0.55f, 1.0f));
}

auto Fallout2CharSheet::well(fo::Rect r, float radius, bool rivets) -> Element {
  using namespace fo;
  Element e = atR(box(), r).corners(Corners{n(radius)}).fill(wellMat);
  e.background(styles::dropShadow(hexColor(0x000000, 0.55f), {0, n(1)}, n(2)));
  e.stroke(
      stroke(n(1), Fill::color(hexColor(0x0A0E06)), PathFormat::Align::Inner));
  e.foreground(
      inset(n(-1.5f), stroke(n(1.5f), Fill::color(hexColor(0x5C4C30, 0.85f)),
                             PathFormat::Align::Center)));
  e.foreground(
      stamp(1.2f, 1.6f, hexColor(0x8A7448, 0.55f), hexColor(0x000000, 0.60f)));
  if (rivets) {
    const float inset = 5.0f;
    for (int i = 0; i < 4; ++i) {
      const float rx = ((unsigned)i & 1u) ? r.w - inset : inset;
      const float ry = ((unsigned)i & 2u) ? r.h - inset : inset;
      e.child(
          sigil::compose::kit::disc(SkPoint{n(rx), n(ry)}, n(2.4f))
              .fill(rivetMat)
              .foreground(stroke(n(0.6f), Fill::color(hexColor(0x000000, 0.7f)),
                                 PathFormat::Align::Outer)));
    }
  }
  return e;
}

auto Fallout2CharSheet::raised(fo::Rect r, float radius) -> Element {
  using namespace fo;
  Element e = atR(box(), r).corners(Corners{n(radius)}).fill(tabMat);
  e.foreground(
      stamp(1.4f, 1.8f, hexColor(0xA08858, 0.60f), hexColor(0x0C0906, 0.65f)));
  e.stroke(stroke(n(1), Fill::color(hexColor(0x1A1610, 0.9f)),
                  PathFormat::Align::Inner));
  return e;
}

auto Fallout2CharSheet::centred(fo::Rect r, Element child) -> Element {
  Element e = fo::atR(box(), r);
  e.justify(Justify::Center).alignItems(Align::Center);
  e.child(std::move(child));
  return e;
}

auto Fallout2CharSheet::chrome() -> Element {
  using namespace fo;
  Element g = box().inset(0);

  // The outer frame and the dividers between the five panels — raised
  // facets lit from the top-left.
  g.child(at(box(), 0, 0, 640, 480)
              .foreground(stroke(n(3), Fill::color(hexColor(0x1E1810)),
                                 PathFormat::Align::Inner))
              .foreground(
                  inset(n(3), fo::stamp(2.0f, 2.5f, hexColor(0xA08858, 0.45f),
                                        hexColor(0x0C0906, 0.55f)))));
  // vertical divider between the left/middle block and the skills column
  g.child(at(box(), 328, 0, 4, 480)
              .fill(Paint::linearUnit(
                  {0, 0}, {1, 0},
                  {{0.0f, hexColor(0x554430)}, {1.0f, hexColor(0x241D12)}})));
  g.child(at(box(), 165, 30, 3, 240)
              .fill(Paint::linearUnit(
                  {0, 0}, {1, 0},
                  {{0.0f, hexColor(0x4E4030)}, {1.0f, hexColor(0x241D12)}})));
  g.child(at(box(), 5, 318, 320, 3)
              .fill(Paint::linearUnit(
                  {0, 0}, {0, 1},
                  {{0.0f, hexColor(0x554430)}, {1.0f, hexColor(0x241D12)}})));

  // Top plaques (measured bright runs at y = 1: 15..153, 155..236, 238..312).
  const char* plaqueText[3] = {"NARG", "AGE 20", "MALE"};
  const Rect plaques[3] = {
      {14, 0, 140, 26}, {155, 0, 82, 26}, {238, 0, 76, 26}};
  for (int i = 0; i < 3; ++i) {
    Element p = raised(plaques[i], 3.0f);
    p.justify(Justify::Center).alignItems(Align::Center);
    // A 26-unit run does not fit a 26-unit plaque: its line box is the caps
    // plus a rise above and a descent below that no capital uses, so it
    // overflows, pins to the top, and lands its caps on the lower bevel
    // with the whole leftover above them. Lifting the run by a third of
    // that rise puts the CAPS' own band in the middle of the plaque, which
    // is the only band on a run of capitals anyone reads as centred.
    p.child(engravedText(plaqueText[i], n(26.0f), kGold, engravedCondense, 0.6f)
                .margin(0, -engravedRise(n(26.0f)) / 3.0f, 0, 0));
    g.child(p);
  }

  // The SKILLS heading (font 103, #907824) at (380, 5), and SKILL POINTS at
  // (400, 233) with its own two-digit odometer at (522, 228).
  g.child(ink(engravedText("SKILLS", n(24.0f), kGold, 0.78f, 0.5f), 380, 4,
              engravedRise(n(24.0f))));
  // The SKILL POINTS bar: a raised strip carrying the label and the counter,
  // between the skills well and the card.
  g.child(raised({336, 226, 292, 30}, 3.0f));
  g.child(ink(engravedText("SKILL POINTS", n(24.0f), kGold, 0.78f, 0.5f), 400,
              232, engravedRise(n(24.0f))));
  g.child(at(box(), 520, 226, 34, 28)
              .fill(Fill::color(hexColor(0x120E08)))
              .corners(Corners{n(2)})
              .foreground(fo::stamp(1.0f, 1.4f, hexColor(0x8A7448, 0.45f),
                                    hexColor(0x000000, 0.6f), 300)));
  g.child(box().left(Dimension(0)).top(Dimension(0)).child(slot("points")));

  // PRINT / DONE / CANCEL at y = 454, each with a red button light. Lamp
  // rects sampled at x 344..355, 457..468, 553..564, y 455..466.
  const char* btn[3] = {"PRINT", "DONE", "CANCEL"};
  const float lampX[3] = {344, 457, 553};
  const float textX[3] = {364, 477, 573};
  for (int i = 0; i < 3; ++i) {
    Element lamp = at(box(), lampX[i], 455, 12, 12)
                       .corners(Corners{n(6)})
                       .fill(Paint::radialUnit({0.35f, 0.30f}, 1.1f,
                                               {{0.0f, hexColor(0xFF6A4A)},
                                                {0.45f, kLampOn},
                                                {1.0f, hexColor(0x600000)}}));
    lamp.foreground(stroke(n(1.2f), Fill::color(hexColor(0x1A1208)),
                           PathFormat::Align::Outer));
    if (i == 1)  // DONE dims and returns as the `+` is pressed
      lamp.child(box()
                     .inset(0)
                     .corners(Corners{n(6)})
                     .fill(Fill::color(kLampOff))
                     .opacity(&lampFlash));
    g.child(lamp);
    g.child(ink(engravedText(btn[i], n(22.0f), kGold, 0.80f, 0.3f), textX[i],
                454, engravedRise(n(22.0f))));
  }
  return g;
}
