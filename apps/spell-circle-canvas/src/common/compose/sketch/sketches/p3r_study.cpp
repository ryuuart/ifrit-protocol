// p3r_study.cpp — a STUDY of the Persona 3 Reload pause-menu grammar
// (REFERENCES.md §1, the ATLUS "sexy UI" grammar). Round 2: the study now
// exercises the landed API surface instead of hand-building it —
//
//   ground .......... abyss navy vertical ramp #061030→#0B2A5E
//   halftone field .. patterns::halftoneRamp — ONE SkSL pass: 45° staggered
//                     dot grid whose radius swells top→bottom (§1 "radius
//                     modulated 0→0.45·spacing by a gradient across the
//                     field"), cyan-on-navy 26% alpha; the shader's
//                     uDriftX/uDriftY uniforms are BOUND to ch::Outputs so
//                     the grid drifts ~10px/s along its diagonal forever —
//                     no oversized box, no kDstIn blend stack
//   menu battery .... layout(layouts::Diagonal{−12°}) places the cards down
//                     ONE shear line; each card is a plain rect .skewX(−12)
//                     — the type shears with it (the ATLUS oblique voice).
//                     "never center — anchor corners, let the diagonal
//                     carry the eye"
//   sticker stack ... zero-blur only: solid ink duplicate at +8/+8
//                     (util::shadow blur 0) < white outer stroke
//                     (util::stroke) < fill; one light direction (upper-left)
//   selection ....... PERSONA flooded #1269CC→#51EEFC, cyan shadow
//                     BREATHING ±2px (bound-translate duplicate box), plus a
//                     white trim window marching around the card outline —
//                     trim(0, w, &phase, TrimMode::Wrap), the offset now a
//                     live PropValue
//   entrance ........ withFrom() MOUNT transitions: cards slide in along
//                     the diagonal with a stagger FAKED by growing each
//                     card's distance+duration (withFrom has no delay);
//                     date block slides from the right; ghost fades up.
//                     Capture mid-entrance with --at 0.18
//   date block ...... pinned with .top()/.right() per-side Dim pins — no
//                     stretching, unpinned sides stay auto
//
// Headless: ComposeSketch <this> --frame p3r.png --at 2.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Layouts.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>

#include <chrono>
#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
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

constexpr float kSkew = -12.0f; // the P3R slash angle
constexpr float kCardH = 58;
constexpr float kRowGap = 16;
constexpr float kDotSpacing = 11.0f; // §1: 8–14px

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
sigil::weave::TextStyle verbType(float size, SkColor4f fill, float strokeW,
                                 SkColor4f strokeColor = kInk) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = heavyFace();
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = size * 0.06f;
  s.paint.foreground.setColor(fill.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  if (strokeW > 0)
    s.paint.addUnderlay(sigil::weave::PaintLayer::outline(
        strokeColor.toSkColor(), strokeW, SkPaint::kMiter_Join));
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
  // Idle motion: field drift (bound straight into the halftoneRamp SkSL),
  // the selected card's breathing cyan shadow, and the marching trim window.
  ch::Output<float> driftX{0}, driftY{0};
  ch::Output<float> selDx{8}, selDy{8};
  ch::Output<float> trimPhase{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kNavyTop);

    // Drift ~10px/s along the 45° grid diagonal: at 45° a (d,d) offset is
    // pure grid-y motion of d·√2, so wrapping d at 2·spacing·√2 lands on an
    // exact stagger-tile period — seamless forever.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const double period = 2.0 * kDotSpacing * 1.41421356; // 31.11px
      const float d = (float)std::fmod(t * 7.07, period);
      driftX = d;
      driftY = d;
      // Selection breath: offset 8→10px, one light direction kept (§1:
      // "selection animates offset +2px").
      selDx = 8.0f + 2.0f * (float)std::sin(t * 2.4);
      selDy = 8.0f + 2.0f * (float)std::sin(t * 2.4);
      // The white sliver orbiting the selected card, one lap every 4s.
      trimPhase = (float)std::fmod(t * 0.25, 1.0);
      return true;
    });

    ctx.composer.render(describe());
  }

  /** §1's dotted field as ONE material: 45° staggered grid, radius swelling
   *  rMin→rMax down the node (0→0.43·spacing), drift uniforms bound live. */
  Material dotField() {
    Material m = patterns::halftoneRamp(
        kDotSpacing, 0.4f, 4.7f,
        {kCyan.fR, kCyan.fG, kCyan.fB, 0.26f}, 45.0f);
    m.uniform("uDriftX", &driftX).uniform("uDriftY", &driftY);
    return m;
  }

  /** The sticker stack, §1 order: ink duplicate +8/+8, white outer stroke,
   *  fill. The card is a PLAIN RECT sheared by skewX — layouts::Diagonal
   *  marches the rows down the same shear line. */
  Element card(int i) {
    const Verb &v = kVerbs[i];
    const bool sel = (i == kSelected);

    Element face =
        box().absolute().inset(0)
            .background(shadow(sel ? kCyan
                                   : SkColor4f{0.016f, 0.020f, 0.043f, 1},
                               {8, 8}, 0))
            .fill(sel ? Material::linear({0, 0}, {v.width, 0},
                                         {{0.0f, kBlue}, {1.0f, kCyan}})
                      : Material::solid(kInk))
            .stroke(stroke(9, Fill::color(kPaper)))
            .row().alignItems(Align::Center).padding(30, 0, 0, 0)
            .child(text(toU8(v.label),
                        verbType(30, kPaper, 4.5f, sel ? kCyan : kInk))
                       .margin(0, 0, 0, 2));

    // Mount entrance, staggered BY HAND: withFrom() has no delay, so the
    // cascade is faked with per-card distance+duration growth (constant-ish
    // velocity so the cascade reads as offset starts, not slow-motion).
    const auto slideDur = std::chrono::milliseconds(300 + 120 * i);
    const auto fadeDur = std::chrono::milliseconds(200 + 85 * i);
    const float slideFrom = -(70.0f + 34.0f * (float)i);

    Element c =
        box().key(v.label).width(v.width).height(kCardH)
            .skewX(kSkew)
            .translateX(withFrom(slideFrom, 0.0f,
                                 {slideDur, &ch::easeOutQuint}))
            .opacity(withFrom(0.0f, 1.0f, {fadeDur, &ch::easeOutQuad}));
    if (sel) {
      // The selected shadow BREATHES: a bound-translate duplicate box
      // beneath the face. Same rect (shear inherited), cyan flood, zero-blur.
      c.child(box().absolute().inset(0)
                  .fill(Material::solid(kCyan))
                  .translateX(&selDx)
                  .translateY(&selDy));
    }
    c.child(face);
    if (sel) {
      // The misprint sliver, alive: a cyan window marching around the card
      // outline (over the white stroke band) — animatable trim OFFSET under
      // TrimMode::Wrap.
      c.child(box().absolute().inset(0)
                  .stroke(stroke(4, Fill::color(kCyan)))
                  .trim(0.0f, 0.16f, &trimPhase, TrimMode::Wrap));
    }
    return c;
  }

  Element describe() {
    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, kNavyTop}, {1.0f, kNavyBot}}))
        // ---- the dotted field: one live material, full bleed ----
        .child(box().absolute().inset(0).fill(dotField()))
        // ---- ghost echo of the selection (the oversized background type
        //      ATLUS floats behind every menu), on the same −12° axis ----
        .child(text(toU8("PERSONA"),
                    verbType(168, {kSea.fR, kSea.fG, kSea.fB, 0.14f}, 0))
                   .absolute().inset(348, 236, 0, 0).zIndex(1)
                   .rotate(kSkew)
                   .opacity(withFrom(0.0f, 1.0f, {700ms})))
        // ---- the menu battery: ONE oblique line via layouts::Diagonal ----
        .child(layout(layouts::Diagonal{.skewDeg = kSkew, .gap = kRowGap})
                   .left(85).top(122).width(340).height(360).zIndex(2)
                   .child(card(0))
                   .child(card(1))
                   .child(card(2))
                   .child(card(3))
                   .child(card(4)))
        // ---- date/time block, PINNED top-right (no stretching) ----
        .child(
            box().row().top(34).right(52).height(136).zIndex(3)
                .alignItems(Align::End)
                .translateX(withFrom(48.0f, 0.0f, {420ms, &ch::easeOutQuint}))
                .opacity(withFrom(0.0f, 1.0f, {320ms}))
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
        // ---- locator caption, pinned bottom-left ----
        .child(text(toU8("tartarus — block 2 · thebel"),
                    smallType(14, kSea, 2))
                   .left(98).bottom(34).zIndex(3));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(P3RStudy)
