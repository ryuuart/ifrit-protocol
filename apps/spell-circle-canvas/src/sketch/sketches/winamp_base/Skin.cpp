#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/color/Color.h>

#include "WinampBase.h"

auto WinampBase::buildMaterials() -> void {
  using namespace wa;
  // THE BODY IS SPECKLE, NOT BRUSH. Read off MAIN.BMP the surface is a
  // fine isotropic speckle with a soft radial lightening toward the
  // middle-left, and at viewing size it is essentially flat. A stretch
  // of five turns the same grain into long striations across every
  // window, which is then the loudest texture in the picture and is not
  // in the skin. One recipe, three windows, two sizes — the unit square
  // is what makes 275x116 and 400x377 share it.
  steel = mskia::Paint::blend(
      {{mskia::Paint::linearUnit({0, 0}, {0, 1},
                                 {{0.0f, kBodyTop}, {1.0f, kBodyBot}}),
        SkBlendMode::kSrcOver},
       {mskia::Paint::radialUnit(
            {0.34f, 0.42f}, 1.15f,
            {{0.0f, {1, 1, 1, 0.055f}}, {1.0f, {1, 1, 1, 0.0f}}}),
        SkBlendMode::kSrcOver},
       {mskia::Paint::recipe(field::grain(0.34f, 2, 3.0f, 0.20f, 1.0f)),
        SkBlendMode::kOverlay}});

  // The desktop: flat teal plus ONE low-octave dither, baked once.
  deskMat = mskia::Paint::blend(
      {{mskia::Paint::solid(kDesk), SkBlendMode::kSrcOver},
       {mskia::Paint::recipe(field::grain(0.45f, 1, 3.0f, 0.055f, 1.0f)),
        SkBlendMode::kOverlay}});

  // CRT glass: the flat screen colour plus a soft off-centre catch-light.
  lcdMat = mskia::Paint::blend(
      {{mskia::Paint::solid(kLcd), SkBlendMode::kSrcOver},
       {mskia::Paint::radialUnit({0.28f, 0.22f}, 1.25f,
                                 {{0.0f, hexColor(0x2A2A46, 0.75f)},
                                  {1.0f, hexColor(0x2A2A46, 0.0f)}}),
        SkBlendMode::kSrcOver}});

  // ONE fader-track value shared by all eleven faders (preamp + 10
  // bands), and it is a LADDER rather than a ramp. EQMAIN.BMP carries
  // twenty-eight slider frames and each one is a SINGLE colour picked
  // off the green-yellow-orange-red run by position; a continuous
  // gradient is the one thing a sprite sheet of flat frames cannot be,
  // and this file already quantises the volume slider for exactly that
  // reason. Hard stops at each twenty-eighth make the same picture the
  // sheet does.
  {
    std::vector<mskia::Stop> steps;
    constexpr int kFrames = 28;
    const auto ramp = [](float u) {
      return u < 0.46f ? sigil::material::mixLinear(kEqTop, kEqMid, u / 0.46f)
                       : sigil::material::mixLinear(kEqMid, kEqBot,
                                                    (u - 0.46f) / 0.54f);
    };
    for (int i = 0; i < kFrames; ++i) {
      const float lo = (float)i / (float)kFrames;
      const float hi = (float)(i + 1) / (float)kFrames;
      const sigil::material::Color c = ramp((lo + hi) * 0.5f);
      steps.push_back({lo, c});
      steps.push_back({hi, c});
    }
    faderTrack = mskia::Paint::linearUnit({0, 0}, {0, 1}, steps);
  }

  graphMat = mskia::Paint::solid(kGraph);

  // The title-bar grip: horizontal hairlines, as a rotated stripe tile.
  // TITLEBAR.BMP's grip rails are CREAM, not the body's blue-grey — the
  // one warm thing on an otherwise cold window.
  gripTile = patterns::stripes(n(1), n(1), hexColor(0xC8BC98));
  gripTile.rotate(90.0f);
  // The preview-visualiser swatch's default checkerboard art.
  previewCheck =
      patterns::checker(n(2), hexColor(0x2B2B44), hexColor(0x14141F));
  // The visualiser well's baked dot grid (MAIN.BMP paints these under the
  // bars, in VISCOLOR's own "grey for dots").
  visDots = patterns::halftone(n(2), n(0.5f), kUnlit, false);
  // The EQ graph's dashed rules.
  graphGrid = Pattern::tile({n(4), n(4)}, [](SkCanvas& c, SkSize, uint32_t) {
    SkPaint p;
    p.setColor4f(sigil::material::skia::toSkColor(kGrid), nullptr);
    c.drawRect(SkRect::MakeWH(n(2), n(1)), p);
  });
}

auto WinampBase::key(float x, float y, float w, float h, Element glyph)
    -> Element {
  using namespace wa;
  Element e = at(box(), x, y, w, h);
  e.fill(mskia::Paint::linearUnit(
      {0, 0}, {0, 1},
      {{0.0f, sigil::material::lighten(kBtnFace, 0.10f)},
       {0.55f, kBtnFace},
       {1.0f, dark(kBtnFace, 0.22f)}}));
  // WINAMP'S DOUBLED BUTTON EDGE: the skin's bevel with a second ring
  // one native px in, and that ring a HALF one — the two shaded sides
  // with no light opposite them, because the key's second line is a
  // deepened shadow and not a second bevel; a whole ring there lights
  // the top of a 3 px key. Its ends are SLICED, so the shadow turns the
  // bottom-right corner and stops rather than running up into the two
  // corners the outer ring lights.
  kit::Bevel edge = kit::bevels::skin(kBtnHi, kBtnLo, n(1));
  edge.inner =
      kit::BevelInner{.gap = n(1),
                      .shadow = sigil::material::withAlpha(kBtnLo, 0.45f),
                      .depth = n(1),
                      .edges = path::Edge::Bottom | path::Edge::Right,
                      .ends = styles::BevelEnds::Sliced};
  kit::bevelled(e, edge);
  // the lettering on any key is set in the one dark ink
  e.ink(kLabel);
  e.children({std::move(glyph)});
  return e;
}

auto WinampBase::part(float x, float y, float w, float h, Shape shape)
    -> Element {
  using namespace wa;
  Element e = at(box(), x, y, w, h).fill(kGlyph);
  if (shape) e.shape(std::move(shape));
  return e;
}

auto WinampBase::textKey(float x, float y, float w, float h, const char* label,
                         float cell) -> Element {
  using namespace wa;
  return key(x, y, w, h, box())
      .justifyContent(Justify::Center)
      .alignItems(Align::Center)
      .children({t(label, pix(cell))});
}

auto WinampBase::titleBar(float wN, const char* label, bool wide, bool hasMin,
                          float hN) -> Element {
  using namespace wa;
  // Window buttons, native 9x9, at the right-hand offsets the SDK pins them
  // to; each is centred in whatever bar height it is given.
  const auto wbtn = [this, hN](float x, const char* g) {
    return raised(at(box(), x, (hN - 9) * 0.5f, 9, 9)
                      .fill(dark(kTitle, 0.35f))
                      .justifyContent(Justify::Center)
                      .alignItems(Align::Center),
                  sigil::material::withAlpha(hexColor(0x5A5A82), 0.8f),
                  hexColor(0x0E0E16))
        .children({t(g, pix(3.6f))});
  };
  // The grip rails share the title's row so both captions reserve their
  // actual text width, including the longer animated message.
  const auto caption = [this, hN, wN](Element run) {
    const auto grip = [this] {
      return box().flexGrow(1).flexBasis(0).height(n(7)).fill(
          gripTile.material());
    };
    return at(box(), 24, (hN - 8) * 0.5f, wN - 48, 8)
        .row()
        .gap(n(5))
        .alignItems(Align::Center)
        .children({grip(), std::move(run.flexShrink(0)), grip()});
  };
  // the wordmark, the egg and the window buttons' glyphs: one gold
  return raised(at(box(), 0, 0, wN, hN)
                    .fill(mskia::Paint::linearUnit(
                        {0, 0}, {0, 1},
                        {{0.0f, sigil::material::lighten(kTitle, 0.06f)},
                         {1.0f, dark(kTitle, 0.25f)}}))
                    .ink(kGold),
                sigil::material::withAlpha(hexColor(0x5A5A82), 0.85f),
                hexColor(0x101018))
      .children({caption(document::label(label).font(pix(6.6f, true, 1.7f)))
                     .opacity(motion::bind(&llama).invert()),
                 caption(document::label("IT REALLY WHIPS THE LLAMA'S ASS!")
                             .font(pix(5.2f, true, 0.7f))
                             .scale(&llamaPop))
                     .opacity(&llama),
                 // the option/context menu, native 9x9 at x=6
                 wide ? box() : wbtn(6, "-"),
                 hasMin ? wbtn(wN - 31, "_") : box(), wbtn(wN - 21, "="),
                 wbtn(wN - 11, "x")});
}
