// p3r_study.cpp — a STUDY of the Persona 3 Reload pause-menu grammar
// (REFERENCES.md §1, the ATLUS "sexy UI" grammar). Not an invented
// aesthetic — every construction below is that section's rule:
//
//   ground .......... abyss navy vertical ramp #061030→#0B2A5E
//   halftone field .. 45°-rotated dot grid (patterns::halftone, rotated as a
//                     Pattern mapping), alpha-modulated across the field by a
//                     kDstIn ramp inside ONE Material::blend shader, drifting
//                     along the grid diagonal ~10px/s forever via bound
//                     translateX/Y on an OVERSIZED box (wrap = exact tile
//                     translation, seamless)
//   menu cards ...... PARALLELOGRAMS via outline() at −12° (no skew
//                     transform in this study); rows x-offset by
//                     rowPitch·tan(12°) so the left edges form the slanted
//                     rule ("never center — anchor corners, let the diagonal
//                     carry the eye")
//   sticker stack ... zero-blur only: solid ink duplicate at +8/+8
//                     (util::shadow blur 0) < white outer stroke
//                     (util::stroke) < fill; one light direction (upper-left)
//   selection ....... ONE card flooded #1269CC→#51EEFC, white text, shadow
//                     recolored cyan — and BREATHING (offset ±2px): the cyan
//                     shadow is a bound-translate duplicate box, so the idle
//                     motion is two Outputs, zero re-describes
//   menu verbs ...... heavy condensed all-caps white over an ink stroked
//                     underlay (PaintLayer::outline — the second-text-layer
//                     trick, one paragraph)
//   date block ...... huge condensed day numeral + cyan slash quad
//                     (steeper parallelogram) top-right, P3R calendar style
//
// Headless: ComposeSketch <this> --frame p3r.png --at 1.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

namespace {

constexpr float W = 960, H = 640;

// REFERENCES.md §1 palette, verbatim.
constexpr SkColor4f kNavyTop{0.0235f, 0.0627f, 0.1882f, 1};  // #061030
constexpr SkColor4f kNavyBot{0.0431f, 0.1647f, 0.3686f, 1};  // #0B2A5E
constexpr SkColor4f kBlue{0.0706f, 0.4118f, 0.8000f, 1};     // #1269CC
constexpr SkColor4f kCyan{0.3176f, 0.9333f, 0.9882f, 1};     // #51EEFC
constexpr SkColor4f kSea{0.4275f, 0.6039f, 0.7804f, 1};      // #6D9AC7
constexpr SkColor4f kPaper{1, 1, 1, 1};                      // #FFFFFF
constexpr SkColor4f kInk{0.0784f, 0.0863f, 0.1216f, 1};      // #14161F

constexpr float kTan12 = 0.212557f; // tan(12°) — the P3R slash angle
constexpr float kCardH = 58;
constexpr float kRowGap = 16;
constexpr float kRowPitch = kCardH + kRowGap;

/** The card shape: a parallelogram leaning −12° (both verticals slanted). */
SkPath parallelogram(SkSize sz) {
  const float s = sz.height() * kTan12;
  SkPathBuilder b;
  b.moveTo(s, 0);
  b.lineTo(sz.width(), 0);
  b.lineTo(sz.width() - s, sz.height());
  b.lineTo(0, sz.height());
  b.close();
  return b.detach();
}

/** The calendar slash: a steeper slanted bar (the "/" between month/day). */
SkPath slashQuad(SkSize sz) {
  const float t = sz.width() * 0.34f; // bar thickness along x
  SkPathBuilder b;
  b.moveTo(sz.width() - t, 0);
  b.lineTo(sz.width(), 0);
  b.lineTo(t, sz.height());
  b.lineTo(0, sz.height());
  b.close();
  return b.detach();
}

/** Heavy condensed grotesque — the ATLUS menu voice (CoreText match). */
sk_sp<SkTypeface> heavyFace() {
  static sk_sp<SkTypeface> face = [] {
    auto mgr = sigil::weave::ports::systemFontManager();
    sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
        "Helvetica Neue",
        SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kCondensed_Width,
                    SkFontStyle::kUpright_Slant));
    if (!f)
      f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
    return f;
  }();
  return face;
}

/** Menu verb type: white fill over a stroked ink underlay (one paragraph —
 *  the "second text layer in ink behind white text" as a paint pass). */
sigil::weave::TextStyle verbType(float size, SkColor4f fill,
                                 float inkStroke) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = heavyFace();
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = size * 0.06f;
  s.paint.foreground.setColor(fill.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  if (inkStroke > 0)
    s.paint.addUnderlay(sigil::weave::PaintLayer::outline(
        kInk.toSkColor(), inkStroke, SkPaint::kMiter_Join));
  return s;
}

sigil::weave::TextStyle smallType(float size, SkColor4f c, float track = 1) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor(c.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

struct Verb {
  const char *label;
  float width;
};
constexpr Verb kVerbs[] = {{"SKILL", 208},
                           {"ITEM", 186},
                           {"EQUIP", 214},
                           {"PERSONA", 268},
                           {"SYSTEM", 236}};
constexpr int kSelected = 3; // PERSONA floods blue→cyan

} // namespace

struct P3RStudy : sketch::Sketch {
  // 45° halftone, cyan-on-navy (§1: spacing 8–14, 15–35% alpha). Held as a
  // member — the Pattern bake is the identity.
  Pattern dots = patterns::halftone(11.0f, 2.6f, {kCyan.fR, kCyan.fG,
                                                  kCyan.fB, 0.30f});

  // Idle motion: field drift + the selected card's breathing cyan shadow.
  ch::Output<float> driftX{0}, driftY{0};
  ch::Output<float> selDx{8}, selDy{8};
  // Entrance: cards slide along the diagonal, Hold-staggered.
  std::array<ch::Output<float>, 5> slide;
  std::array<ch::Output<float>, 5> fade;

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kNavyTop);
    dots.rotate(45.0f);

    auto &tl = ctx.ticker.timeline();
    for (int i = 0; i < 5; ++i) {
      slide[i] = -64.0f;
      fade[i] = 0.0f;
      const float delay = 0.06f + 0.07f * (float)i;
      tl.apply(&slide[i])
          .then<ch::Hold>(-64.0f, delay)
          .then<ch::RampTo>(0.0f, 0.42f, &ch::easeOutQuint);
      tl.apply(&fade[i])
          .then<ch::Hold>(0.0f, delay)
          .then<ch::RampTo>(1.0f, 0.30f, &ch::easeOutQuad);
    }

    // Drift ~10px/s along the grid diagonal; wrap at an exact tile
    // translation (d·√2 = 4·spacing) so the loop is seamless forever.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const double period = 4.0 * 11.0 / 1.41421356; // 31.11px of drift
      const float d = (float)std::fmod(t * 7.07, period);
      driftX = d;
      driftY = d;
      // Selection breath: offset 8→10px, one light direction kept.
      selDx = 8.0f + 2.0f * (float)std::sin(t * 2.4);
      selDy = 8.0f + 2.0f * (float)std::sin(t * 2.4);
      return true;
    });

    ctx.composer.render(describe());
  }

  /** The sticker stack, §1 order: ink duplicate +8/+8, white outer stroke,
   *  fill. Text rides inside the fill layer. */
  Element card(int i) {
    const Verb &v = kVerbs[i];
    const bool sel = (i == kSelected);
    const float x = 96.0f - (float)i * kRowPitch * kTan12; // the slanted rule
    const SkColor4f textFill = kPaper;

    Element face =
        box().absolute().inset(0).outline(parallelogram)
            .background(shadow(sel ? kCyan : SkColor4f{0.016f, 0.020f,
                                                       0.043f, 1},
                               {8, 8}, 0))
            .fill(sel ? Material::linear({0, 0}, {v.width, 0},
                                         {{0.0f, kBlue}, {1.0f, kCyan}})
                      : Material::solid(kInk))
            .stroke(stroke(9, Fill::color(kPaper)))
            .row().alignItems(Align::Center).padding(36, 0, 0, 0)
            .child(text(toU8(v.label), verbType(30, textFill, 4.5f))
                       .margin(0, 0, 0, 2));

    Element c = box().key(v.label).width(v.width).height(kCardH)
                    .margin(x, i == 0 ? 0 : kRowGap, 0, 0)
                    .alignSelf(Align::Start)
                    .translateX(&slide[i])
                    .opacity(&fade[i]);
    if (sel) {
      // The selected shadow BREATHES: a bound-translate duplicate box
      // beneath the face (util::shadow is a static value; motion needs a
      // node). Same parallelogram, cyan flood, zero-blur.
      c.child(box().absolute().inset(0).outline(parallelogram)
                  .fill(Material::solid(kCyan))
                  .translateX(&selDx)
                  .translateY(&selDy));
      // face's own static shadow is redundant under the live one — but it
      // keeps the stack identical when unselected; harmless (covered).
    }
    return c.child(face);
  }

  Element describe() {
    // The halftone field: dots masked by a diagonal alpha ramp (radius-
    // modulation approximated as coverage modulation — one flat shader),
    // on an oversized box so the drift never reveals an edge.
    const float FW = W + 320, FH = H + 320;
    Material field = Material::blend(
        {{dots.material(), SkBlendMode::kSrcOver},
         {Material::linear({FW * 0.15f, 0}, {FW, FH},
                           {{0.0f, {1, 1, 1, 0.10f}}, {1.0f, {1, 1, 1, 1}}}),
          SkBlendMode::kDstIn}});

    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, kNavyTop}, {1.0f, kNavyBot}}))
        .child(box().absolute().inset(-160, -160, -160, -160)
                   .fill(field)
                   .translateX(&driftX)
                   .translateY(&driftY))
        // ---- ghost echo of the selection (the oversized background type
        //      ATLUS floats behind every menu), on the same −12° axis ----
        .child(text(toU8("PERSONA"),
                    verbType(168, {kSea.fR, kSea.fG, kSea.fB, 0.14f}, 0))
                   .absolute().inset(348, 236, 0, 0).zIndex(1)
                   .rotate(-12.0f))
        // ---- the menu stack, anchored left, sliding down the diagonal ----
        .child(box().column().absolute().inset(52, 122, 0, 0).zIndex(2)
                   .alignItems(Align::Start)
                   .child(card(0))
                   .child(card(1))
                   .child(card(2))
                   .child(card(3))
                   .child(card(4)))
        // ---- date/time block, top-right ----
        .child(
            box().row().absolute().inset(586, 34, 0, 0).height(136).zIndex(3)
                .alignItems(Align::End)
                .child(text(toU8("07"), verbType(44, kSea, 3))
                           .margin(0, 0, 0, 14))
                .child(box().width(56).height(84).outline(slashQuad)
                           .background(shadow(kInk, {5, 5}, 0))
                           .fill(Material::solid(kCyan))
                           .margin(10, 0, 10, 2))
                .child(text(toU8("21"), verbType(96, kPaper, 6)))
                .child(box().column().margin(12, 0, 0, 10)
                           .child(text(toU8("MON"), verbType(24, kCyan, 3)))
                           .child(text(toU8("evening"),
                                       smallType(13, kSea, 1.5f))
                                      .margin(2, 4, 0, 0))))
        // ---- locator caption, bottom-left ----
        .child(text(toU8("tartarus — block 2 · thebel"),
                    smallType(14, kSea, 2))
                   .absolute().inset(98, H - 52, 0, 0).zIndex(3));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(P3RStudy)
