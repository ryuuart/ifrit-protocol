#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontTypes.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/stats/Fit.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "../genesis_fire/Instrument.h"

namespace sketch = sigil::sketch;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::material::skia::Effect;
using sigil::material::skia::Paint;
using sigil::material::skia::toColor;
namespace ch = choreograph;

namespace slit {

// ---------------------------------------------------------------------------
// The machine, in numbers. Provenance on every line.

constexpr float kZ0In = 180.0f;      // 15 ft far focus              [T68]
constexpr float kZ1In = 1.5f;        // 1.5 in near focus            [T68]
constexpr float kR = kZ0In / kZ1In;  // 120.0 : 1 EXACTLY         derived
constexpr float kTrackIn = 168.0f;   // 14 ft of track               [C85]
constexpr float kPlateIn = 72.0f;    // 6 ft rotatable plate         [C85]
constexpr float kSlitHIn = 48.0f;    // 4 ft illuminated opening      [NO]
constexpr float kPanelIn = 144.0f;   // 12 ft backlit glass panel    [C85]
constexpr float kX0In = 49.2f;       // slit offset        RECONSTRUCTED
constexpr float kX0OverW = 84.0f;    // slit aspect        RECONSTRUCTED
constexpr float kSlitWIn = kX0In / kX0OverW;  // 0.585714 in
constexpr float kAdvanceIn = kSlitWIn;        // pinned by [GE], note 6

// K_min = 1 + ln(R)/ln(1 + w/X0) = 405.6 -> 406: the fewest stamps whose
// adjacent images still touch across the whole sweep. Linearising the
// logarithm gives 402, which is under and leaves gaps.
constexpr int kK = 406;
// K_min removes GAPS, not RIPPLE. drawSpriteAtlas's drawVertices is not
// antialiased, so at exactly K_min -- where adjacent stamps only just touch
// -- the covering count beats between one and two on the pixel grid.
// Verification D below fits the exposure law at both counts, which is what
// separates the ripple from the exponent. The picture stamps four times the
// minimum so the ripple is not what the frame shows.
constexpr int kOversample = 4;
constexpr int kKDisplay = kK * kOversample;

// The film frame is drawn to its own 120:1 scale: the sweep starts 5 px
// from the vanishing point, which is 15 ft from the lens, and ends at
// 600 px, which is one and a half inches.
constexpr float kUFar = 5.0f;
constexpr float kUNear = kUFar * kR;  // 600 px

// The atlas cell IS the twelve-foot panel, at one scale.
constexpr float kCellPxPerIn = 10.25f;
constexpr float kCellW = kPanelIn * kCellPxPerIn;  // 1476 px
constexpr float kCellH = kSlitHIn * kCellPxPerIn;  // 492 px
constexpr float kWinFrac = kSlitWIn / kPanelIn;    // 0.00406746
// on-screen width = w*ppi*s and height = h*ppi*s, so s(u) = u/(X0*ppi)
// makes width = u*w/X0 and height = u*h/X0 EXACTLY. ONE scale, both axes,
// because a perspective projection scales both the same way; anisotropic
// stamp scaling here would be a drawing choice, not a projection.
constexpr float kScaleDen = kX0In * kCellPxPerIn;  // 504.3

// ---------------------------------------------------------------------------
// Chrome -- a darkroom / optical-bench register

constexpr SkColor4f kInk{0.047f, 0.039f, 0.031f, 1};      // #0C0A08
constexpr SkColor4f kPanelBg{0.078f, 0.067f, 0.063f, 1};  // #141110
constexpr SkColor4f kRule{0.133f, 0.114f, 0.102f, 1};     // #221D1A
constexpr SkColor4f kType{0.929f, 0.910f, 0.874f, 1};     // #EDE8DF
// THE SECONDARY REGISTER READS AT PLATE SCALE. At #8C8378 on
// #100E0C a seven-point line is a texture rather than a
// sentence, and the right column is nearly all seven-point
// lines. The pair goes up a step each, which keeps the
// hierarchy and makes the lower half of it legible.
constexpr SkColor4f kType2{0.694f, 0.655f, 0.604f, 1};
constexpr SkColor4f kAmber{0.839f, 0.506f, 0.227f, 1};  // #D6813A
constexpr SkColor4f kCold{0.918f, 0.949f, 1.0f, 1};     // #EAF2FF
constexpr SkColor4f kRed{0.769f, 0.220f, 0.180f, 1};    // #C4382E
constexpr SkColor4f kSolid{0.106f, 0.090f, 0.078f, 1};  // #1B1714
constexpr SkColor4f kTick{0.494f, 0.455f, 0.408f, 1};
constexpr SkColor4f kBlack{0, 0, 0, 1};  // "the room was painted
                                         //  totally black" [C85]
constexpr SkColor4f kWhite{1, 1, 1, 1};

// The gels. RECONSTRUCTED from the process -- saturated subtractive filters
// on black, ADDED -- never eyedropped from a transfer of a 1968 print.
constexpr SkColor4f kGelRed{1.0f, 0.180f, 0.122f, 1};      // #FF2E1F
constexpr SkColor4f kGelAmber{1.0f, 0.541f, 0.039f, 1};    // #FF8A0A
constexpr SkColor4f kGelStraw{1.0f, 0.890f, 0.302f, 1};    // #FFE34D
constexpr SkColor4f kGelGreen{0.231f, 0.878f, 0.541f, 1};  // #3BE08A
constexpr SkColor4f kGelCyan{0.145f, 0.714f, 1.0f, 1};     // #25B6FF
constexpr SkColor4f kGelViolet{0.478f, 0.298f, 1.0f, 1};   // #7A4CFF
constexpr SkColor4f kGelMag{1.0f, 0.247f, 0.627f, 1};      // #FF3FA0

inline SkColor4f al(SkColor4f c, float a) {
  c.fA = a;
  return c;
}

// ---------------------------------------------------------------------------
// Layout, exact: 32 + 96 + 20 + 780 + 32 = 960; 1056 + 28 + 412 = 1496;
// 480 + 24 + 276 = 780; and 1056 / 480 = 2.200, the projected aspect.

constexpr float kCanvasW = 1560, kCanvasH = 960;
constexpr float kPad = 32, kHeaderH = 96, kBodyH = 780;
constexpr float kLeftW = 1056, kSideW = 412;
constexpr float kFilmW = 1056, kFilmH = 480;
constexpr float kRigW = 1056, kRigH = 276;
constexpr float kRigPxPerIn = 3.0f;  // the rig strip's ONE scale
constexpr float kElevW = 596;        // 8 + 540 (focus reach) + 48 of air
constexpr float kPanelStripW = 432;  // 144 in at 3.0 px/in, exact

// ---------------------------------------------------------------------------
// Type

using instrument::faced;
using instrument::mono;
using instrument::monoB;
using instrument::monoBoldFace;
using instrument::monoFace;
using instrument::t;
using instrument::ui;
using instrument::uiB;
using instrument::uiBoldFace;
using instrument::uiFace;

/** The quotation register: condensed 0.94 with 0.4 of tracking. */
inline weave::TextStyle quo(float s, SkColor4f c) {
  weave::TextStyle st = faced(uiFace(), s, c, 0.4f);
  st.condense(0.94f);
  return st;
}

inline Element rule(float w, SkColor4f c, float h = 1.0f) {
  return box().width(Dim(w)).height(Dim(h)).shrink(0).fill(c);
}

// ---------------------------------------------------------------------------
// The film's transfer curve: D = 1 - e^(-kE), applied to the ACCUMULATION.
//
// Effect::shader hands the node's already-painted layer to the SkSL as a
// child shader named `content`, so a tone curve over painted content is one
// call and needs no palette lookup and no read-back. Applied here over a
// Mode::Live instancing leaf.

inline sk_sp<SkRuntimeEffect> transferCurve() {
  const char* src = R"(
uniform shader content;
uniform float k;
half4 main(float2 xy) {
  // The layer arrives PREMULTIPLIED, and the premultiplied colour IS the
  // accumulated exposure. Density is the saturating response to it -- the
  // shoulder that lets a 120:1 brightness range onto a display at all.
  half4 s = content.eval(xy);
  float3 e = float3(s.rgb);
  float3 d = float3(1.0) - exp(-k * e);
  float da = 1.0 - exp(-k * float(s.a));
  return half4(half3(d), half(da));
}
)";
  auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(src));
  if (!e) std::fprintf(stderr, "[slitscan] transfer sksl: %s\n", err.c_str());
  return e;
}

// ---------------------------------------------------------------------------
// THE ARTWORK -- generated, not drawn.
//
// [C85]: high-contrast NEGATIVES of op-art paintings, architectural drawings
// and prints of electrical circuits. [GE], independently, recovered coral /
// flowers, microscopic and botanical photography, backlit graphic shapes
// with scrolling graphics, and composites of abstract shapes and spirals. So
// the sketch generates those KINDS of thing and thresholds the result to one
// bit, which is what "high-contrast negative" means. Colour then lives
// entirely in the pools' tints() lane: the artwork is one-bit, the gel is a
// per-stamp tint, and no palette material is needed anywhere in this file.

struct Strip {
  sk_sp<SkImage> image;
  std::vector<uint8_t> lum;  // 0/255, row-major -- fuel for verification E
  int w = 0, h = 0;
};

inline Strip bakeStrip(Element tree, sigil::weave::FontContext& fonts, int W,
                       int H, float thresh, int stripeX) {
  Strip out;
  out.w = W;
  out.h = H;
  sk_sp<SkPicture> pic = snapshot(box().child(std::move(tree)), fonts);
  SkBitmap bm;
  if (!bm.tryAllocN32Pixels(W, H)) return out;
  {
    SkCanvas c(bm);
    c.clear(SK_ColorBLACK);
    if (pic) c.drawPicture(pic);
  }
  out.lum.assign((size_t)W * (size_t)H, 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const SkColor s = bm.getColor(x, y);
      const float l =
          (0.2126f * (float)SkColorGetR(s) + 0.7152f * (float)SkColorGetG(s) +
           0.0722f * (float)SkColorGetB(s)) /
          255.0f;
      bool on = l > thresh;
      // [GE]'s "white stripe artifact" -- one 3 px full-height run at a
      // fixed x, present in BOTH shots that share this strip. He found the
      // same defect in Shot 27 and Shot 29 and concluded the same physical
      // artwork was reused with different filtering. Not explained in the
      // picture; explained in the caption.
      if (stripeX >= 0 && x >= stripeX && x < stripeX + 7) on = true;
      out.lum[(size_t)y * (size_t)W + (size_t)x] = on ? 255 : 0;
      *bm.getAddr32(x, y) = on ? SkPreMultiplyARGB(255, 255, 255, 255) : 0u;
    }
  bm.setImmutable();
  out.image = bm.asImage();
  return out;
}

/** Op-art: concentric rings over a dense bar lattice -- "backlit graphic
 *  shapes with scrolling graphics" [GE]. */
inline Element artOpArt() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  for (int i = 0; i < 122; ++i) {
    const float x = (float)i * 12.1f;
    const float hh = 30.0f + 220.0f * std::fabs(std::sin((float)i * 0.2131f));
    g.child(box()
                .left(Dim(x))
                .top(Dim(kCellH * 0.5f - hh * 0.5f))
                .width(5)
                .height(Dim(hh))
                .fill(Fill::color(kWhite)));
  }
  for (int i = 0; i < 13; ++i) {
    const float cx = 56.0f + (float)i * 114.0f;
    g.child(box()
                .left(Dim(cx - 170.0f))
                .top(Dim(kCellH * 0.5f - 170.0f))
                .width(340)
                .height(340)
                .shape(shapes::circle())
                .foreground(lines::presets::concentric(Fill::color(kWhite),
                                                       9 + (i % 5) * 4, 6.0f)));
  }
  return g;
}

/** Architectural drawing + spirals: a hatched section, elevation courses,
 *  logarithmic spirals. */
inline Element artArch() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  g.child(box().inset(0).foreground(
      lines::presets::hatch(Fill::color(kWhite), 9.0f, 2.6f, 58.0f)));
  for (int i = 0; i < 8; ++i) {
    const float x = 20.0f + (float)i * 182.0f;
    g.child(box()
                .left(Dim(x))
                .top(Dim(30))
                .width(176)
                .height(432)
                .shape(shapes::spiral(2.4f + 0.35f * (float)i, true, 0.36f))
                .foreground(stroke(8.0f, Fill::color(kWhite))));
    for (int k = 0; k < 11; ++k)
      g.child(box()
                  .left(Dim(x - 16.0f))
                  .top(Dim(14.0f + (float)k * 44.0f))
                  .width(208)
                  .height(4)
                  .fill(Fill::color(kWhite)));
  }
  return g;
}

/** Circuit print + posterised botanical. Shared by SH 27 and SH 29 [GE],
 *  and it carries the white-stripe defect. */
inline Element artCircuit(Pattern& grid, Pattern& spek) {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  g.child(box().inset(0).fill(grid.material()));
  g.child(box().inset(0).fill(spek.material()));
  for (int i = 0; i < 110; ++i) {
    const float x = std::fmod((float)i * 137.31f, kCellW - 34.0f) + 12.0f;
    const float y = std::fmod((float)i * 271.7f, kCellH - 34.0f) + 12.0f;
    g.child(box()
                .left(Dim(x))
                .top(Dim(y))
                .width(26)
                .height(26)
                .shape(shapes::annulus(0.52f))
                .fill(Fill::color(kWhite)));
  }
  return g;
}

// ---------------------------------------------------------------------------
// One wall = one exposure.

struct WallSpec {
  SkPoint vp{0, 0};
  float phiDeg = 0;
  SkColor4f gel = kGelStraw;
  float uFar = kUFar;
  float gain = 1.0f;
  float artLeft = 0.0f;
  float artDrift = 0.0f;  // the panel also slides DURING the exposure [NO]
  int cell = 0;
  float upTo = -1.0f;  // suppress stamps past this fraction (the monitor)
  bool artwork = true;
  int K = kKDisplay;
  bool logSpaced = true;  // false = uniform in z, the S4 counter-example
};

/** The whole picture is these lines of arithmetic:
 *
 *    z_j = z0 * R^(-j/(K-1))     log-uniform in z
 *    u_j = uFar * R^( j/(K-1))   film radius, px
 *    s_j = u_j / (X0 * ppi)      ONE uniform scale
 *    w_j = omega ~ z_j ~ 1/u_j   THE DWELL WEIGHT
 *
 *  With log-uniform positions the covering count is CONSTANT in u, so
 *  equal-weight stamps integrate to a FLAT wall -- the one place the physics
 *  is easy to get backwards. The weight must be the camera travel each stamp
 *  stands for. Nothing paints the 1/rho falloff; it is the sum. */
inline void buildWall(instancing::Pool& p, const WallSpec& s) {
  const int K = std::max(2, s.K);
  p.resize((size_t)K);
  auto pos = p.positions();
  auto rot = p.rotations();
  auto sc = p.scales();
  auto ti = p.tints();
  auto fr = p.frames();
  auto sz = p.sizes();
  auto win = p.texWindows();
  const float phi = s.phiDeg * 0.017453292f;
  const float cs = std::cos(phi), sn = std::sin(phi);
  // Normalising by (K-1) keeps the TOTAL deposited light independent of how
  // finely the sweep is sampled, so K is a sampling choice and never a
  // brightness knob. (For the uniform-z rule the weight is CONSTANT -- the
  // camera travel per stamp is literally constant -- and the 1/u law still
  // falls out, from the covering count instead. That is the row of the
  // sampling table that is correct-but-gappy.)
  const float norm = (float)(kK - 1) / (float)(K - 1);
  // The uniform-z rule's equal weight, matched to the log rule's total.
  const float uniformW = norm * (kZ0In - kZ1In) / (std::log(kR) * kZ0In);
  for (int j = 0; j < K; ++j) {
    const float f = (float)j / (float)(K - 1);
    float u, w;
    if (s.logSpaced) {
      u = s.uFar * std::pow(kR, f);
      w = s.gain * norm * s.uFar / u;
    } else {
      const float z = kZ0In - (kZ0In - kZ1In) * f;
      u = s.uFar * kZ0In / z;
      w = s.gain * uniformW;  // equal weight: the travel IS constant
    }
    const float scale = u / kScaleDen;
    const float wpx = kSlitWIn * kCellPxPerIn * scale;  // on-screen width
    const float uc = u * (1.0f + 0.5f / kX0OverW);      // CENTRE, not edge
    pos[j] = {s.vp.fX + cs * uc, s.vp.fY + sn * uc};
    rot[j] = phi;
    sc[j] = scale;
    fr[j] = s.cell;
    // AREA-CONSERVING SUB-PIXEL CLAMP. Below u = X0/w px the stamp is
    // narrower than a pixel, and drawSpriteAtlas's drawVertices is NOT
    // antialiased -- so most of the sweep would silently vanish. Widen to
    // one pixel through the sizes() lane and scale the weight by the true
    // width, leaving the deposited light unchanged. That is a box filter;
    // without it the fitted exposure exponent rolls off near the apex,
    // where the stamps go sub-pixel, instead of holding at 1.
    float sx = 1.0f;
    if (wpx < 1.0f) {
      sx = 1.0f / std::max(wpx, 1e-4f);
      w *= wpx;
    }
    sz[j] = {sx, 1.0f};
    ti[j] = {s.gel.fR, s.gel.fG, s.gel.fB, std::clamp(w, 0.0f, 1.0f)};
    if (s.upTo >= 0.0f && f > s.upTo) ti[j].fA = 0.0f;
    // Pool::texWindows() -- ONE bake of the twelve-foot panel, addressed at
    // a different sub-rect per stamp, so the artwork crawl is continuous.
    // The alternative is pre-registering a cell per crawl position, which
    // quantises the crawl to that cell count and cannot be repaired later:
    // Atlas::cell() drops the whole baked sheet when you re-register.
    const float left =
        s.artwork ? std::fmod(s.artLeft + s.artDrift * f + 8.0f, 1.0f) : 0.0f;
    win[j] =
        SkRect::MakeXYWH(std::min(left, 1.0f - kWinFrac), 0.0f, kWinFrac, 1.0f);
  }
  p.commit();
}

}  // namespace slit

// ===========================================================================
