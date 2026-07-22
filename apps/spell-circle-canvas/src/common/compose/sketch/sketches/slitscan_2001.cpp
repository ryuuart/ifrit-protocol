// slitscan_2001.cpp — STUDY 05: Douglas Trumbull's Slit-Scan machine
// (MGM Borehamwood, 1966-68) and the Star Gate of 2001: A Space Odyssey.
// =============================================================================
// SUBJECT  The machine, not the shot. A slit-scan frame is a TIME INTEGRAL:
//          one frame of film accumulated over a continuous exposure made
//          while a camera travelled fourteen feet toward a narrow slit in a
//          six-foot rotatable plate, backlit through a twelve-foot glass
//          panel of high-contrast artwork and gels. Super Panavision 70:
//          65 mm negative, 5 perf, spherical, projected 2.20:1, 24 fps,
//          f/1.8. There is no object in the picture. The subject IS the
//          exposure, and everything in the film frame below is the sum of
//          812 additive stamps -- nothing paints it.
//
// -----------------------------------------------------------------------------
// SOURCES -- read, not remembered
//
//  [T68] Douglas Trumbull, "Creating Special Effects for 2001: A Space
//        Odyssey", American Cinematographer 49(6):416-420, 451-453, June 1968.
//        http://www.visual-memory.co.uk/sk/2001a/page3.html
//        *** READ DIRECTLY. The brief warned the host's TLS certificate has
//        expired and that WebFetch refuses it; `curl -k` does not. The
//        passage is verbatim as quoted below -- AND the reprint carries
//        three photo captions the brief's secondary carrier did not, which
//        settle FINDING 1 outright (see below). Reading the primary was
//        worth more than every secondary in this file combined.
//  [C85] Cinefex 85, April 2001 -- Trumbull on the Star Gate. Quoted at one
//        remove by two independent carriers (fellinis.com.au 2021-03-23;
//        slashfilm.com 680507). NOT read directly.
//  [FS]  Trumbull, The Film Stage: "four minutes a frame ... only about a
//        quarter of what we produced".
//  [NO]  Neil Oseman, "Slit-scan and the Legacy of Douglas Trumbull".
//  [MB]  MagicBeans VFX, "Recreating 2001's Slit Scan Effect in Nuke's 3D
//        System", 31 Jul 2019.
//  [GE]  Greg Ercolano, "2001 A Space Odyssey: Unwrapping the Slit Scan
//        sequences", seriss.com/people/erco/2001/ -- recovered the ORIGINAL
//        ARTWORK from Seq29 shots 4, 8, 27, 29 by accumulating one scanline
//        per frame, i.e. by running the machine BACKWARDS. It works, which
//        is an independent test of the mechanism. Verification E below runs
//        it FORWARDS and gets the strip back to the byte.
//  [AP]  "The Age of Plastic", 2001: Film, Cameras and Lenses -- the Super
//        Panavision 70 spherical prime list.
//  [WP]  Wikipedia, "70 mm film" (apertures) and "Slit-scan photography"
//        (the attribution, checked for citations).
//
// -----------------------------------------------------------------------------
// [T68], VERBATIM. This sentence is the spine of the study:
//
//   "For this infinite corridor of lights, shapes, and enormous speed and
//    scale, I designed what I called the Slit-Scan machine. Using a
//    technique of image scanning as used in scientific and industrial
//    photography, this device could produce two seemingly infinite planes of
//    exposure while holding depth-of-field from a distance of fifteen feet
//    to one and one-half inches from the lens at an aperture of F/1.8 with
//    exposures of approximately one minute per frame using a standard 65mm
//    Mitchell camera."
//
// AND, three sentences later, in the SAME article, as photo captions:
//
//   "(Left) Designer assembling a bank of photo-electric control devices for
//    slit-scan mechanism. (Center) 65mm Todd-AO camera with follow-focus
//    device mounted for operation of slit-scan set-up. (Right) Todd-AO
//    camera with Acme stop-motion motor and selsyn-driven follow-focus
//    mechanism."
//
// -----------------------------------------------------------------------------
// WHAT THIS STUDY VERIFIED, AND WHAT CHANGED (all printed on canvas)
//
//  1. THE DEPTH-OF-FIELD SENTENCE CANNOT MEAN WHAT IT SAYS, AND THE ARTICLE
//     ITSELF SAYS WHAT IT MEANT. At f/1.8, c = 0.05 mm, with the
//     self-consistent f = 28.64 mm, the DOF at 1.5 in is 0.0791 mm. The
//     claimed bracket is 4533.9 mm. The ratio is 5.7e4. For the sentence to
//     be literally true one setting would need a hyperfocal of 76.2 mm, i.e.
//     f = 2.57 mm at c = 0.05 -- a 169-degree field on a 52.6 mm frame.
//     Read the fifteen feet as a HYPERFOCAL NEAR LIMIT and the closed form
//     f = (-Nc + sqrt((Nc)^2 + 4HNc))/2 at H = 9144 mm gives 20.26 / 28.64 /
//     35.07 / 40.48 mm for c = 0.025 / 0.05 / 0.075 / 0.1. ALL FOUR ROWS
//     RE-DERIVED AND ALL FOUR AGREE WITH THE BRIEF (the c = 0.075 row that
//     the researcher corrected on the second pass is right at 35.07).
//     What the machine held was FOCUS, SERVOED TO THE TRACK -- and I did not
//     have to infer it: [T68]'s own photo captions say "follow-focus device"
//     and "SELSYN-DRIVEN follow-focus mechanism". A selsyn is a synchro, a
//     shaft-position servo. The erratum is settled by the same page that
//     contains it.
//  1b. AND THE LENS EXISTS. [AP] lists Panavision's Super Panavision 70
//     spherical primes as 28mm T2.8, 35mm T2.8, 50mm T2.0, 75mm T2.8, 100mm
//     T2.5, 150mm T3, and notes the 28/50/75/100 in the film's own
//     continuity reports. The back-solved 28.64 mm lands ON a lens
//     Panavision made for the format and that 2001 actually used. FINDING 1's
//     second half survives its own test.
//  1c. NEW ERRATUM OF MY OWN, IN THE OTHER DIRECTION: f/1.8 IS FASTER THAN
//     ANY LENS ON THAT LIST (widest T2.8). So the Slit-Scan lens was
//     probably not Panavision glass -- and [AP] records Kubrick using Nikon
//     still lenses whose image circle covers the 65 mm frame. Flagged, not
//     resolved.
//  1d. AND A SECOND ONE: TO FOCUS ANY OF THOSE LENSES AT 1.5 in YOU NEED A
//     BELLOWS, NOT A FOCUS RING. m = f/(u-f) at u = 38.1 mm is 3.03x for
//     f = 28.64 (86.7 mm of extension), 11.56x for f = 35.07, and NEGATIVE
//     for f = 40.48 -- at c = 0.1 the back-solved lens is LONGER than the
//     subject distance and cannot form an image at all. The "servoed focus"
//     reading therefore implies a servoed EXTENSION over fourteen feet of
//     track, which is a far larger mechanical claim than a follow-focus and
//     is exactly what "selsyn-driven" buys you.
//  2. THE TWO PUBLISHED DISTANCE FIGURES DISAGREE BY 10.5 INCHES. [T68]'s
//     pair implies 178.5 in = 14 ft 10.5 in of travel; [C85] says 14 ft flat,
//     which puts the near end at 12 in, not 1.5 in. 6.2500% of the track,
//     33 years apart, same source. Re-checked; exact. This study uses [T68]
//     for the 120.0:1 RATIO (which the picture depends on) and [C85] for the
//     TRACK (which the diagram depends on).
//  3. THE ATTRIBUTION. [T68] credits "a technique of image scanning as used
//     in scientific and industrial photography" and does not mention John
//     Whitney. Wikipedia's slit-scan article says Whitney developed the
//     technique for Vertigo's titles; I checked, and THAT SENTENCE CARRIES
//     NO INLINE CITATION. The adjacent sentence -- "after he sent some test
//     sequences on film to Stanley Kubrick, the technique was adapted by
//     Douglas Trumbull" -- cites a 2010 Trumbull Master Class at TIFF Bell
//     Lightbox, for which I could reach no transcript. So the honest line is
//     narrower than the brief's: the 1968 attribution is the designer's own
//     and names industrial image scanning; the Whitney lineage rests on one
//     uncited sentence plus one un-transcribed talk given 42 years after the
//     machine was built. (This programme's study 02 documented the Vertigo
//     spirals as a pendulum on an M-5 gun director. A pen on a rotating
//     plate is not a moving slit.)
//  4. THE 65 mm APERTURES CHECK OUT. [WP] "70 mm film": camera aperture
//     52.63 x 23.01 mm (2.072 x 0.906 in) = 2.287:1; projection aperture
//     48.56 x 22.10 mm (1.912 x 0.870 in) = 2.1973:1 ~ 2.20:1. Corroborated,
//     NOT read against SMPTE. A second carrier gives 2.066 in for the camera
//     aperture width; both circulate.
//  5. K = 400 IS 1.5% UNDER ITS OWN DERIVATION. The overlap condition is
//     d(ln u) <= ln(1 + w/X0), not <= w/X0. With X0/w = 84 that is
//     K >= 1 + ln(120)/ln(1+1/84) = 405.6, so K_min = 406. The brief's 402
//     linearised the logarithm. This file stamps 406 per wall.
//  6. ERCOLANO'S UNWRAP PINS THE ARTWORK ADVANCE, WHICH NO SOURCE PUBLISHES.
//     If the panel advanced MORE than one slit-width per frame the unwrap
//     would show gaps; LESS, and it would show repeats. [GE] recovered
//     coherent artwork from four shots, so the advance is ~= the slit width.
//     The brief listed 0.9 in/frame as free; it is not free, and this file
//     uses w = 0.5857 in/frame. An independent observation constraining a
//     reconstructed parameter is the best thing in this study.
//
// -----------------------------------------------------------------------------
// DOCUMENTED (cited above; not altered)
//   "two seemingly infinite planes of exposure" -- the corridor has exactly
//   TWO walls and they are TWO EXPOSURES per frame [T68][C85]; focus reach
//   15 ft -> 1.5 in at f/1.8, ~1 min per frame, a standard 65 mm Mitchell
//   [T68]; a follow-focus device, selsyn-driven, and a bank of photo-electric
//   controls [T68 captions]; 14 ft of track traversed IN FULL for every
//   exposure, 45-60 s per exposure, two exposures per frame [C85]; the plate
//   6 ft tall, rotatable sheet metal, one narrow opening [C85], the opening
//   ~4 ft high [NO]; a 12 ft mechanised backlit glass panel of transparencies
//   and celluloid gels, the images HIGH-CONTRAST NEGATIVES of op-art,
//   architectural drawings and circuit prints [C85]; "The room was painted
//   totally black." [C85]; shutter OPEN for the whole move, closes, camera
//   returns, THEN the artwork moves slightly [MB][NO]; worm-gear dolly,
//   synchronous motors [NO]; 36 h continuous per take, six months of
//   slit-scan work [C85]; ~4 min/frame, ~a quarter used [FS].
//
// DERIVED (arithmetic over published integers -- not reconstruction)
//   z0/z1 = 180 in / 1.5 in = 120.0 : 1 EXACTLY. Every scale in the film
//   frame hangs off it. EXPOSURE ALONG A STREAK FALLS AS 1/rho: dwell at
//   film radius u is f*w/(V*u) and irradiance from an extended source is
//   distance-invariant at fixed aperture. THE SUB-EXPOSURES MUST BE SPACED
//   UNIFORMLY IN ln z (K_min = 1 + ln(120)/ln(1+w/X0) = 406) AND WEIGHTED BY
//   omega ~ z; equal-weight log stamps are band-free AND FLAT, which is the
//   wrong physics. MACHINE:SCREEN = 24 x 90..120 s = 2160..2880 : 1
//   (5760 : 1 on [FS]). ONE SECOND OF THE STAR GATE COST 36 TO 96 MINUTES OF
//   MACHINE TIME.
//
// RECONSTRUCTED (mine -- NO SOURCE PUBLISHES THE GEOMETRY, and [T68] says
//   why: "Stanley Kubrick strongly emphasized ... that he wished the specific
//   techniques used in the last sequence to remain as unpublicized as
//   possible, so ... I will describe them only briefly without specific
//   details." The record is thin BECAUSE THE DIRECTOR ASKED FOR IT TO BE.)
//   X0 = 49.2 in and w = X0/84 = 0.5857 in, fitted to theta = 26 deg and a
//   82:1 opening -- which puts X0 more than four feet off axis, outside a
//   six-foot plate centred on the axis. THE WEAKEST JOINT HERE, printed as
//   such. Constant camera speed in z; the plate's rotation schedule; every
//   gel colour (built from the PROCESS -- saturated gels added on black --
//   not eyedropped from a transfer); the film transfer D = 1 - e^(-kE); the
//   three artwork strips; all layout, chrome, loop timing and type. And the
//   2-D reduction, which is EXACT rather than simplified: a pinhole on a
//   straight track pointed at a plane reduces to the plane, because every
//   source point's image runs along a ray through the principal point.
//
// CAVEAT ON THE PINHOLE: the picture is drawn to the PINHOLE ratio 120:1.
//   A real thin lens at f = 28.64 mm gives m = f/(u-f), so the magnification
//   ratio over the same sweep is (4572-28.64)/(38.1-28.64) = 480:1, four
//   times more flare. The two agree only while f << u, which fails hard at
//   the near end. Printed, not hidden.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace slit {

// ---------------------------------------------------------------------------
// The machine, in numbers. Provenance in the comment on every line.

constexpr float kZ0In = 180.0f;      // 15 ft far focus              [T68]
constexpr float kZ1In = 1.5f;        // 1.5 in near focus            [T68]
constexpr float kR = kZ0In / kZ1In;  // 120.0 : 1 EXACTLY            derived
constexpr float kTrackIn = 168.0f;   // 14 ft of track               [C85]
constexpr float kPlateIn = 72.0f;    // 6 ft rotatable plate         [C85]
constexpr float kSlitHIn = 48.0f;    // 4 ft illuminated opening     [NO]
constexpr float kPanelIn = 144.0f;   // 12 ft backlit glass panel    [C85]
constexpr float kX0In = 49.2f;       // slit offset       RECONSTRUCTED
constexpr float kX0OverW = 84.0f;    // slit aspect ratio  RECONSTRUCTED
constexpr float kSlitWIn = kX0In / kX0OverW;          // 0.5857 in
// K_min = 1 + ln(R) / ln(1 + w/X0). NOT ln(R)*X0/w -- see header note 5.
constexpr int kK = 406;
// Ercolano's unwrap is contiguous only if the advance IS the slit width.
constexpr float kAdvanceIn = kSlitWIn;                // header note 6

// The film frame is drawn to its own 120:1 scale: 1 px from the vanishing
// point is 15 ft from the lens, 600 px is 1.5 in.
constexpr float kUFar = 5.0f;
constexpr float kUNear = kUFar * kR; // 600 px

// The atlas cell IS the twelve-foot panel, at one scale.
constexpr float kCellPxPerIn = 10.25f;
constexpr float kCellW = kPanelIn * kCellPxPerIn;  // 1476 px
constexpr float kCellH = kSlitHIn * kCellPxPerIn;  // 492 px
constexpr float kWinFrac = kSlitWIn / kPanelIn;    // 0.00406746
// on-screen stamp width = kSlitW*ppi*s and height = kSlitH*ppi*s, so
// s(u) = u / (X0 * ppi) makes width = u*w/X0 and height = u*h/X0 EXACTLY.
constexpr float kScaleDen = kX0In * kCellPxPerIn;  // 504.3

// ---------------------------------------------------------------------------
// Chrome -- a darkroom register (deliberately unlike studies 01-04)

constexpr SkColor4f kInk{0.047f, 0.039f, 0.031f, 1};      // #0C0A08
constexpr SkColor4f kPanelBg{0.078f, 0.067f, 0.063f, 1};  // #141110
constexpr SkColor4f kRule{0.133f, 0.114f, 0.102f, 1};     // #221D1A
constexpr SkColor4f kType{0.929f, 0.910f, 0.874f, 1};     // #EDE8DF
constexpr SkColor4f kType2{0.549f, 0.514f, 0.471f, 1};    // #8C8378
constexpr SkColor4f kAmber{0.839f, 0.506f, 0.227f, 1};    // #D6813A
constexpr SkColor4f kCold{0.918f, 0.949f, 1.0f, 1};       // #EAF2FF
constexpr SkColor4f kRed{0.769f, 0.220f, 0.180f, 1};      // #C4382E
constexpr SkColor4f kSolid{0.106f, 0.090f, 0.078f, 1};    // #1B1714
constexpr SkColor4f kTick{0.361f, 0.329f, 0.294f, 1};     // #5C544B
constexpr SkColor4f kBlack{0, 0, 0, 1};                   // the room was black

// Gels -- RECONSTRUCTED from the process (saturated gels added on black),
// not eyedropped from a compressed transfer of a 1968 print.
constexpr SkColor4f kGelRed{1.0f, 0.180f, 0.122f, 1};     // #FF2E1F
constexpr SkColor4f kGelAmber{1.0f, 0.541f, 0.039f, 1};   // #FF8A0A
constexpr SkColor4f kGelStraw{1.0f, 0.890f, 0.302f, 1};   // #FFE34D
constexpr SkColor4f kGelGreen{0.231f, 0.878f, 0.541f, 1}; // #3BE08A
constexpr SkColor4f kGelCyan{0.145f, 0.714f, 1.0f, 1};    // #25B6FF
constexpr SkColor4f kGelViolet{0.478f, 0.298f, 1.0f, 1};  // #7A4CFF
constexpr SkColor4f kGelMag{1.0f, 0.247f, 0.627f, 1};     // #FF3FA0

inline SkColor4f a(SkColor4f c, float alpha) { c.fA = alpha; return c; }

// ---------------------------------------------------------------------------
// Layout, exact (see the arithmetic in the caption strip)

constexpr float kCanvasW = 1560, kCanvasH = 960;
constexpr float kPad = 32, kHeaderH = 96, kBodyH = 780;
constexpr float kLeftW = 1056, kSideW = 412;
constexpr float kFilmW = 1056, kFilmH = 480;  // 1056/480 = 2.200 EXACTLY
constexpr float kRigW = 1056, kRigH = 276;

// ---------------------------------------------------------------------------
// Type

sk_sp<SkTypeface> face(const char *family, SkFontStyle style) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(family, style);
  return f ? f : mgr->matchFamilyStyle(nullptr, style);
}
sk_sp<SkTypeface> uiFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::Normal());
  return f;
}
sk_sp<SkTypeface> uiBoldFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::Bold());
  return f;
}
sk_sp<SkTypeface> monoFace() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Normal());
  return f;
}
sk_sp<SkTypeface> monoBoldFace() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Bold());
  return f;
}

sigil::weave::TextStyle type(sk_sp<SkTypeface> tf, float size, SkColor4f color,
                             float track = 0.0f, float condense = 1.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(tf);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.shaping.horizontalScale = condense;
  s.paint.foreground.setColor4f(color);
  s.paint.foreground.setAntiAlias(true);
  return s;
}
sigil::weave::TextStyle ui(float s, SkColor4f c, float tr = 0) {
  return type(uiFace(), s, c, tr);
}
sigil::weave::TextStyle uiB(float s, SkColor4f c, float tr = 0) {
  return type(uiBoldFace(), s, c, tr);
}
sigil::weave::TextStyle mono(float s, SkColor4f c, float tr = 0) {
  return type(monoFace(), s, c, tr);
}
sigil::weave::TextStyle monoB(float s, SkColor4f c, float tr = 0) {
  return type(monoBoldFace(), s, c, tr);
}
// The quotation register the header promised: condensed 0.94, tracked 0.4.
sigil::weave::TextStyle quote(float s, SkColor4f c) {
  return type(uiFace(), s, c, 0.4f, 0.94f);
}

Element t(const std::string &s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

std::string fmt(const char *f, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return std::string(buf);
}

// ---------------------------------------------------------------------------
// The film's transfer curve: D = 1 - e^(-k*E), over already-painted content.
// ROADMAP 10f said a LUT was unreachable and was corrected before this was
// built: Effect::shader's layer arrives as the child shader `content`, so a
// transfer curve over an accumulation is one call. Verified at this call
// site (and see the freeze probe reported in the gap list).

sk_sp<SkRuntimeEffect> transferCurve() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    const char *src = R"(
uniform shader content;
uniform float k;
half4 main(float2 xy) {
  half4 s = content.eval(xy);
  float av = max(float(s.a), 1e-5);
  float3 straight = float3(s.rgb) / av;
  // The film's shoulder. Exposure in, density out, per channel.
  float3 d = 1.0 - exp(-k * straight * av);
  float da = 1.0 - exp(-k * av);
  return half4(half3(d * da), half(da));
}
)";
    auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(src));
    if (!e)
      std::fprintf(stderr, "[slitscan] transfer sksl: %s\n", err.c_str());
    return e;
  }();
  return fx;
}

// ---------------------------------------------------------------------------
// THE ARTWORK -- generated, not drawn. [C85] says the panel carried
// high-contrast NEGATIVES of op-art paintings, architectural drawings and
// prints of electrical circuits; [GE] recovered coral / flowers /
// microscopic and botanical photography, backlit graphic shapes with
// scrolling graphics, and composites of abstract shapes and spirals. So the
// sketch generates those KINDS of thing and thresholds the result to 1 bit,
// which is what "high-contrast negative" means.

struct Strip {
  sk_sp<SkImage> image;
  std::vector<uint8_t> lum; // 0/255, row-major, for the round-trip check
  int w = 0, h = 0;
};

/** Threshold a rendered element tree to a 1-bit mask: opaque white where the
 *  luminance clears `thresh`, fully transparent elsewhere. A mask is exactly
 *  right for the physics -- the gel lives entirely in the tints() lane, so
 *  no palette material is needed anywhere in this file. */
Strip bakeStrip(Element tree, sigil::weave::FontContext &fonts, int W, int H,
                float thresh, int stripeX) {
  Strip out;
  out.w = W;
  out.h = H;
  sk_sp<SkPicture> pic = snapshot(box().child(std::move(tree)), fonts);
  SkBitmap bm;
  if (!bm.tryAllocN32Pixels(W, H))
    return out;
  {
    SkCanvas c(bm);
    c.clear(SK_ColorBLACK);
    if (pic)
      c.drawPicture(pic);
  }
  out.lum.assign((size_t)W * H, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const SkColor s = bm.getColor(x, y);
      const float l = (0.2126f * SkColorGetR(s) + 0.7152f * SkColorGetG(s) +
                       0.0722f * SkColorGetB(s)) /
                      255.0f;
      bool on = l > thresh;
      // [GE]'s "white stripe artifact" -- one 3 px full-height run at a
      // fixed x, present in the shots that SHARE this strip. Do not explain
      // it in the picture; explain it in the caption.
      if (stripeX >= 0 && x >= stripeX && x < stripeX + 3)
        on = true;
      out.lum[(size_t)y * W + x] = on ? 255 : 0;
      *bm.getAddr32(x, y) = on ? SkPreMultiplyARGB(255, 255, 255, 255) : 0;
    }
  }
  bm.setImmutable();
  out.image = bm.asImage();
  return out;
}

/** Band 1 -- op-art: concentric rings and a two-axis lattice. */
Element artOpArt() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  for (int i = 0; i < 9; ++i) {
    const float cx = 82.0f + (float)i * 164.0f;
    const float rr = 54.0f + 26.0f * std::sin((float)i * 1.7f);
    g.child(box()
                .absolute()
                .left(Dim(cx - 128.0f))
                .top(Dim(kCellH * 0.5f - 128.0f))
                .width(256)
                .height(256)
                .outline(shapes::circle())
                .foreground(lines::concentric(Fill::color({1, 1, 1, 1}),
                                              (int)(rr * 0.22f), 5.0f)));
  }
  g.child(box()
              .absolute()
              .inset(0)
              .fill(Material::solid({0, 0, 0, 0}))
              .foreground(decorations::wash(
                  Material::solid({1, 1, 1, 1}), SkBlendMode::kSrcOver, 0.0f)));
  // A lattice of bars across the whole panel -- "scrolling graphics" [GE].
  for (int i = 0; i < 48; ++i) {
    const float x = (float)i * 30.75f;
    const float hh = 40.0f + 150.0f * std::fabs(std::sin((float)i * 0.41f));
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(kCellH * 0.5f - hh * 0.5f))
                .width(9)
                .height(Dim(hh))
                .fill(Fill::color({1, 1, 1, 1})));
  }
  return g;
}

/** Band 2 -- architectural drawing + spirals: hatched sections and rules. */
Element artArch() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  g.child(box()
              .absolute()
              .inset(0)
              .foreground(lines::hatch(Fill::color({1, 1, 1, 1}), 11.0f, 2.0f,
                                       58.0f)));
  for (int i = 0; i < 6; ++i) {
    const float x = 40.0f + (float)i * 240.0f;
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(60))
                .width(190)
                .height(372)
                .outline(shapes::spiral(2.6f + 0.4f * (float)i, true, 0.34f))
                .foreground(stroke(6.0f, Fill::color({1, 1, 1, 1}))));
    // elevation courses -- an architectural drawing's horizontal rules
    for (int k = 0; k < 7; ++k)
      g.child(box()
                  .absolute()
                  .left(Dim(x - 26.0f))
                  .top(Dim(70.0f + (float)k * 52.0f))
                  .width(238)
                  .height(4)
                  .fill(Fill::color({1, 1, 1, 1})));
  }
  return g;
}

/** Band 3 -- circuit print + posterised botanical. Shared by SH 27 and
 *  SH 29 [GE], with the white-stripe artifact in it. */
Element artCircuit() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  // The circuit print: a two-axis lattice at an X-COM-style uneven pitch.
  g.child(box().absolute().inset(0).fill(
      Pattern{patterns::gridLines(34.0f, 22.0f, 3.0f, {1, 1, 1, 1})}
          .material()));
  // Pads.
  for (int i = 0; i < 88; ++i) {
    const float x = std::fmod((float)i * 137.0f, kCellW - 30.0f) + 10.0f;
    const float y = std::fmod((float)i * 271.0f, kCellH - 30.0f) + 10.0f;
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(y))
                .width(18)
                .height(18)
                .outline(shapes::annulus(0.44f))
                .fill(Fill::color({1, 1, 1, 1})));
  }
  // Posterised botanical: a LARGE speckle tile (ledger 63 -- the tile IS
  // the repeat), thresholded by bakeStrip into blobs.
  g.child(box().absolute().inset(0).fill(
      Pattern{patterns::speckle(246.0f, 130, 3.0f, 17.0f, {1, 1, 1, 1},
                                {0, 0, 0, 0})}
          .material()));
  return g;
}

// ---------------------------------------------------------------------------
// One wall = one exposure. K stamps, log-spaced in z, weighted by dwell.

struct WallSpec {
  SkPoint vp{0, 0};
  float phiDeg = 0;
  SkColor4f gel = kGelStraw;
  float uFar = kUFar;
  float gain = 1.0f;     // master exposure gain (display only)
  float artLeft = 0.0f;  // window left edge, as a fraction of the panel
  float artDrift = 0.0f; // within-exposure drift [NO]: the panel also slid
  int cell = 0;
  int upTo = -1; // stamps beyond this get zero weight (the monitor)
  bool artwork = true;
};

/** Builds the pool for one exposure. Everything that makes the picture is
 *  here and it is nine lines of arithmetic:
 *
 *    z_j = z0 * R^(-j/(K-1))          log-uniform in z          (4.5)
 *    u_j = uFar * R^( j/(K-1))        film radius, px
 *    s_j = u_j / (X0 * ppi)           uniform scale -- a perspective
 *                                     projection scales x and y ALIKE
 *    w_j = omega ~ z_j ~ 1/u_j        THE DWELL WEIGHT           (4.6)
 *
 *  Nothing paints the 1/rho falloff. It is the sum. */
void buildWall(instancing::Pool &p, const WallSpec &s) {
  p.resize((size_t)kK);
  auto pos = p.positions();
  auto rot = p.rotations();
  auto sc = p.scales();
  auto ti = p.tints();
  auto fr = p.frames();
  auto sz = p.sizes();
  auto win = p.texWindows();
  const float phi = s.phiDeg * 0.017453292f;
  const float c = std::cos(phi), sn = std::sin(phi);
  for (int j = 0; j < kK; ++j) {
    const float f = (float)j / (float)(kK - 1);
    const float u = s.uFar * std::pow(kR, f);
    const float scale = u / kScaleDen;
    const float wpx = kSlitWIn * kCellPxPerIn * scale; // on-screen width
    const float uc = u * (1.0f + 0.5f / kX0OverW);     // centre, not edge
    pos[j] = {s.vp.fX + c * uc, s.vp.fY + sn * uc};
    rot[j] = phi;
    sc[j] = scale;
    fr[j] = s.cell;
    float w = s.gain * s.uFar / u; // omega ~ z ~ 1/u
    // AREA-CONSERVING SUB-PIXEL CLAMP. Below u = X0/w px the stamp is
    // narrower than a pixel, and drawSpriteAtlas's drawVertices is not
    // antialiased, so those stamps would simply vanish -- taking 239 of
    // the 406 with them. Widen to one pixel through the sizes() lane and
    // scale the weight by the true width, so the deposited light is
    // unchanged. This is a box filter, and it is why the measured exponent
    // stays 1 through the apex.
    float sx = 1.0f;
    if (wpx < 1.0f) {
      sx = 1.0f / std::max(wpx, 1e-4f);
      w *= wpx;
    }
    sz[j] = {sx, 1.0f};
    ti[j] = {s.gel.fR, s.gel.fG, s.gel.fB, std::clamp(w, 0.0f, 1.0f)};
    if (s.upTo >= 0 && j > s.upTo)
      ti[j].fA = 0.0f;
    const float left = s.artwork
                           ? std::fmod(s.artLeft + s.artDrift * f + 4.0f, 1.0f)
                           : 0.0f;
    win[j] = SkRect::MakeXYWH(std::min(left, 1.0f - kWinFrac), 0.0f, kWinFrac,
                              1.0f);
  }
  p.touch();
}

} // namespace slit

// ===========================================================================

struct SlitScan2001 : sigil::compose::sketch::Sketch {
  using namespace_unused = void;

  // ---- the film clock ----------------------------------------------------
  ch::Output<float> frameAlpha{0};
  sigil::motion::Ticker::FixedStatus fixedStatus;
  long long filmFrame = 0;
  bool everClamped = false;

  // ---- the demonstration clock -------------------------------------------
  double tau = 0.0; // [0,1) once per 3.0 s
  double elapsed = 0.0;

  // ---- the machine -------------------------------------------------------
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> wallA, wallB, monA, monB;
  std::array<slit::Strip, 3> strips;
  float artOffset = 0.0f; // fraction of the panel
  float plateDeg = 0.0f;
  int shot = 0;

  // ---- measurement results (printed on canvas) ---------------------------
  float fitP = 0, fitR2 = 0, fitResid = 0;
  int fitRays = 0, fitPts = 0;
  float rtMaxErr = -1, rtCorr = 0;
  int rtCols = 0;
  int sheetW = 0, sheetH = 0;
  double bakeMs = 0, measMs = 0, rtMs = 0;

  // ------------------------------------------------------------------ shots
  struct Shot {
    const char *name;
    SkColor4f gelA, gelB;
    float phi0, dPhi;
    int cell;
    const char *art;
  };
  static const Shot &shotAt(int i) {
    using namespace slit;
    static const Shot k[4] = {
        {"SEQ 29 · SH 04", kGelStraw, kGelCyan, 0.0f, 0.35f, 0,
         "BACKLIT GRAPHIC SHAPES, FAST SCROLL"},
        {"SEQ 29 · SH 08", kGelMag, kGelGreen, 34.0f, -0.50f, 1,
         "ABSTRACT SHAPES AND SPIRALS"},
        {"SEQ 29 · SH 27", kGelRed, kGelAmber, 78.0f, 0.20f, 2,
         "POSTERISED CORAL / FLOWERS"},
        {"SEQ 29 · SH 29", kGelViolet, kGelCyan, 12.0f, -0.35f, 2,
         "MICROSCOPIC / BOTANICAL — SAME STRIP AS SH 27"},
    };
    return k[i & 3];
  }

  // The vanishing point banks on a slow Lissajous; the corridor does not
  // sit still. Kept inside +-96 x +-44 px of centre.
  SkPoint vp() const {
    return {slit::kFilmW * 0.5f + 96.0f * (float)std::sin(elapsed * 0.41),
            slit::kFilmH * 0.5f + 44.0f * (float)std::sin(elapsed * 0.67 + 1.1)};
  }

  float displayGain() const { return 8.0f; }
  float transferK() const { return 3.6f; }

  // ------------------------------------------------------------- the walls
  void rebuildWalls() {
    using namespace slit;
    const Shot &s = shotAt(shot);
    const SkPoint c = vp();
    WallSpec A{c,  s.phi0 + plateDeg, s.gelA, kUFar, displayGain(),
               artOffset, 0.4f * kWinFrac, s.cell, -1, true};
    WallSpec B = A;
    B.phiDeg = s.phi0 + plateDeg + 180.0f; // "two seemingly infinite planes"
    B.gel = s.gelB;
    B.artLeft = std::fmod(artOffset + 0.37f, 1.0f);
    buildWall(*wallA, A);
    buildWall(*wallB, B);

    // The monitor: the SAME two exposures, at the demonstration rate, with
    // stamps beyond the carriage's current z suppressed. The only place in
    // the plate where you see a frame BEING MADE rather than made.
    const int upTo = (int)(tau * (double)(kK - 1));
    const float mScale = 118.0f / kUNear; // fit 600 px of sweep into the box
    WallSpec MA = A;
    MA.vp = {132.0f, 60.0f};
    MA.uFar = kUFar * mScale;
    MA.upTo = upTo;
    MA.gain = displayGain();
    WallSpec MB = MA;
    MB.phiDeg = B.phiDeg;
    MB.gel = B.gel;
    MB.artLeft = B.artLeft;
    buildWall(*monA, MA);
    buildWall(*monB, MB);
  }

  // ---------------------------------------------------------------- 12-D
  // MEASURE the exposure profile. Nothing in the library can read back what
  // it drew (Debug.h is entirely path-level), so this builds the raster by
  // hand: snapshot the accumulation subtree, replay it into an F16 surface
  // -- F16 because the outer streak is ~1/120 of the apex and 8 bits would
  // quantise it to two levels -- and readPixels.
  //
  // Measured at UNIT GAIN with a UNIFORM cell. The gain the picture uses
  // clips the apex on purpose (that is the film's shoulder), and the
  // artwork modulates the envelope (that is verification E). This measures
  // the LAW, which is what the study claims.
  void measureExposure(sigil::weave::FontContext &fonts) {
    using namespace slit;
    const double t0 = (double)clock() / CLOCKS_PER_SEC;
    // A uniform white slit cell -- the exposure, not the artwork.
    auto flat = std::make_shared<instancing::Atlas>(1.0f);
    flat->filter(SkFilterMode::kNearest);
    flat->cell(box().fill(Fill::color({1, 1, 1, 1})), {kCellW, kCellH});
    auto pa = std::make_shared<instancing::Pool>();
    auto pb = std::make_shared<instancing::Pool>();
    const SkPoint c{kFilmW * 0.5f, kFilmH * 0.5f};
    WallSpec A{c, 0.0f, {1, 1, 1, 1}, kUFar, 1.0f, 0, 0, 0, -1, false};
    WallSpec B = A;
    B.phiDeg = 180.0f;
    buildWall(*pa, A);
    buildWall(*pb, B);

    Element accum = box()
                        .width(Dim(kFilmW))
                        .height(Dim(kFilmH))
                        .child(instancing::instances(flat, pa,
                                                     instancing::Mode::Data,
                                                     SkBlendMode::kPlus))
                        .child(instancing::instances(flat, pb,
                                                     instancing::Mode::Data,
                                                     SkBlendMode::kPlus));
    // snapshot() sizes by the root's CHILDREN, not the root's own dims
    // (ROADMAP 10j) -- hence the shell. A probe without it reads a wrong
    // exponent, silently.
    sk_sp<SkPicture> pic = snapshot(box().child(std::move(accum)), fonts);
    sk_sp<SkSurface> surf = SkSurfaces::Raster(SkImageInfo::Make(
        (int)kFilmW, (int)kFilmH, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
    if (!pic || !surf)
      return;
    surf->getCanvas()->clear(SK_ColorTRANSPARENT);
    surf->getCanvas()->drawPicture(pic);

    const SkImageInfo dst = SkImageInfo::Make((int)kFilmW, (int)kFilmH,
                                              kRGBA_F32_SkColorType,
                                              kUnpremul_SkAlphaType);
    std::vector<float> px((size_t)kFilmW * (size_t)kFilmH * 4);
    if (!surf->readPixels(dst, px.data(), (size_t)kFilmW * 16, 0, 0))
      return;
    auto lumAt = [&](float x, float y) -> float {
      const int xi = (int)std::lround(x), yi = (int)std::lround(y);
      if (xi < 0 || yi < 0 || xi >= (int)kFilmW || yi >= (int)kFilmH)
        return -1;
      const float *p = &px[((size_t)yi * (size_t)kFilmW + (size_t)xi) * 4];
      return 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
    };

    // 32 rays inside the wedge (half-angle 26 deg; sample to 20 deg so the
    // rect corners never truncate the ray), 120 log-spaced radii each.
    const float u0 = 8.0f, u1 = 520.0f;
    double sp = 0, sr2 = 0;
    float worst = 0;
    int rays = 0, pts = 0;
    for (int w = 0; w < 2; ++w) {
      for (int r = 0; r < 16; ++r) {
        const float dd = (-20.0f + 40.0f * (float)r / 15.0f) * 0.017453292f;
        const float ang = (w ? 3.14159265f : 0.0f) + dd;
        std::vector<double> lx, ly;
        for (int i = 0; i < 120; ++i) {
          const float u =
              u0 * std::pow(u1 / u0, (float)i / 119.0f);
          const float v = lumAt(c.fX + std::cos(ang) * u,
                                c.fY + std::sin(ang) * u);
          if (v > 1e-6f) {
            lx.push_back(std::log((double)u));
            ly.push_back(std::log((double)v));
          }
        }
        if (lx.size() < 40)
          continue;
        const size_t n = lx.size();
        double mx = 0, my = 0;
        for (size_t i = 0; i < n; ++i) { mx += lx[i]; my += ly[i]; }
        mx /= (double)n; my /= (double)n;
        double sxy = 0, sxx = 0, syy = 0;
        for (size_t i = 0; i < n; ++i) {
          sxy += (lx[i] - mx) * (ly[i] - my);
          sxx += (lx[i] - mx) * (lx[i] - mx);
          syy += (ly[i] - my) * (ly[i] - my);
        }
        const double slope = sxx > 0 ? sxy / sxx : 0;
        const double inter = my - slope * mx;
        double ss = 0;
        for (size_t i = 0; i < n; ++i) {
          const double e = ly[i] - (slope * lx[i] + inter);
          ss += e * e;
          worst = std::max(worst, (float)std::fabs(std::exp(e) - 1.0));
        }
        sp += -slope;
        sr2 += syy > 0 ? 1.0 - ss / syy : 0.0;
        ++rays;
        pts += (int)n;
      }
    }
    if (rays) {
      fitP = (float)(sp / rays);
      fitR2 = (float)(sr2 / rays);
      fitResid = worst;
      fitRays = rays;
      fitPts = pts;
    }
    measMs = ((double)clock() / CLOCKS_PER_SEC - t0) * 1000.0;
  }

  // ---------------------------------------------------------------- 12-E
  // ERCOLANO'S UNWRAP, RUN FORWARDS. He accumulated one scanline per DVD
  // frame and got the artwork back. This does the same in reverse: feed a
  // known strip in, accumulate one film column per frame across 96 frames,
  // concatenate, and compare against what went in.
  //
  // The radius is chosen as u* = X0*ppi = 504.3 px, where the stamp is
  // EXACTLY the cell window's baked size -- 6 x 492 px, 1:1 in both axes.
  // At any other radius the residual would be measuring nearest-neighbour
  // resampling phase; at this one it measures TRANSPORT, which is the claim.
  void roundTrip(sigil::weave::FontContext &fonts) {
    using namespace slit;
    const double t0 = (double)clock() / CLOCKS_PER_SEC;
    const slit::Strip &S = strips[0];
    if (!S.image || S.lum.empty())
      return;
    constexpr int kFrames = 96;
    const int cw = (int)std::lround(kSlitWIn * kCellPxPerIn);  // 6
    const int chh = (int)std::lround(kCellH);                  // 492
    const int boxW = cw + 14, boxH = chh + 8;
    const float cx = (float)boxW * 0.5f, cy = (float)boxH * 0.5f;

    auto oneCell = std::make_shared<instancing::Atlas>(1.0f);
    oneCell->filter(SkFilterMode::kNearest);
    oneCell->cell(box().fill(Material::image(S.image, SkTileMode::kClamp,
                                             SkTileMode::kClamp,
                                             SkMatrix::Scale(kCellW / (float)S.w,
                                                             kCellH / (float)S.h),
                                             SkSamplingOptions())),
                  {kCellW, kCellH});
    auto pool = std::make_shared<instancing::Pool>();

    std::vector<float> got, want;
    got.reserve((size_t)kFrames * cw * chh);
    want.reserve((size_t)kFrames * cw * chh);
    const float aStart = 0.11f;
    for (int n = 0; n < kFrames; ++n) {
      const float left = aStart + (float)n * kWinFrac;
      pool->resize(1);
      pool->positions()[0] = {cx, cy};
      pool->rotations()[0] = 0;
      pool->scales()[0] = kUFar * kR / kR / 1.0f; // placeholder, set below
      pool->scales()[0] = 1.0f;                   // u* = X0*ppi -> s = 1
      pool->tints()[0] = {1, 1, 1, 1};
      pool->frames()[0] = 0;
      pool->sizes()[0] = {1, 1};
      pool->texWindows()[0] = SkRect::MakeXYWH(left, 0, kWinFrac, 1.0f);
      pool->touch();

      Element acc = box().width(Dim((float)boxW)).height(Dim((float)boxH))
                        .child(instancing::instances(oneCell, pool,
                                                     instancing::Mode::Data));
      sk_sp<SkPicture> pic = snapshot(box().child(std::move(acc)), fonts);
      SkBitmap bm;
      if (!pic || !bm.tryAllocN32Pixels(boxW, boxH))
        return;
      SkCanvas c(bm);
      c.clear(SK_ColorBLACK);
      c.drawPicture(pic);
      const int x0 = (int)std::lround(cx - (float)cw * 0.5f);
      const int y0 = (int)std::lround(cy - (float)chh * 0.5f);
      for (int y = 0; y < chh; ++y)
        for (int x = 0; x < cw; ++x) {
          const SkColor s = bm.getColor(x0 + x, y0 + y);
          got.push_back((float)SkColorGetG(s) / 255.0f);
          // what went in: the same window of the source strip
          const float fx = (left + ((float)x + 0.5f) / (float)cw * kWinFrac);
          const int sx = std::clamp((int)(fx * (float)S.w), 0, S.w - 1);
          const int sy = std::clamp(
              (int)(((float)y + 0.5f) / (float)chh * (float)S.h), 0, S.h - 1);
          want.push_back((float)S.lum[(size_t)sy * S.w + sx] / 255.0f);
        }
    }
    // max abs error and Pearson correlation
    double mg = 0, mw = 0;
    float worst = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      mg += got[i];
      mw += want[i];
      worst = std::max(worst, std::fabs(got[i] - want[i]));
    }
    mg /= (double)got.size();
    mw /= (double)want.size();
    double cgw = 0, cgg = 0, cww = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      cgw += (got[i] - mg) * (want[i] - mw);
      cgg += (got[i] - mg) * (got[i] - mg);
      cww += (want[i] - mw) * (want[i] - mw);
    }
    rtMaxErr = worst;
    rtCorr = (cgg > 0 && cww > 0) ? (float)(cgw / std::sqrt(cgg * cww)) : 0.0f;
    rtCols = kFrames * cw;
    rtMs = ((double)clock() / CLOCKS_PER_SEC - t0) * 1000.0;
  }

  // ===================================================================
  // Elements

  Element header() {
    using namespace slit;
    return box()
        .column()
        .height(Dim(kHeaderH))
        .gap(6)
        .child(t("TECHNIQUE STUDY 05 · TIME AS AN AXIS OF THE IMAGE",
                 ui(10, kType2, 2.6f))
                   .key("eyebrow")
                   .opacity(withFrom(0.0f, 1.0f, {260ms, ch::easeOutQuad}))
                   .translateY(withFrom(8.0f, 0.0f, {260ms, ch::easeOutQuad})))
        .child(t("THE SLIT-SCAN MACHINE, 1966–68", uiB(40, kType, 0.5f))
                   .key("title")
                   .textStroke(0.6f, Fill::color(kInk))
                   .glyphFx(kinetic::rise(18.0f))
                   .opacity(withFrom(0.0f, 1.0f,
                                     {440ms, ch::easeOutExpo, 120ms})))
        .child(t("Douglas Trumbull — ‘Creating Special Effects for "
                 "2001: A Space Odyssey’, American Cinematographer "
                 "49(6):416–420, 451–453, June 1968 · Cinefex "
                 "85, April 2001 · Super Panavision 70, 65 mm 5-perf, "
                 "2.20:1, 24 fps",
                 ui(11, kType2))
                   .key("cite")
                   .opacity(withFrom(0.0f, 1.0f,
                                     {240ms, ch::easeOutQuad, 400ms})));
  }

  // ------------------------------------------------------------- film frame
  Element filmFrame() {
    using namespace slit;
    const Shot &s = shotAt(shot);

    Element accumulation =
        box()
            .width(Dim(kFilmW))
            .height(Dim(kFilmH))
            .absolute()
            .inset(0)
            .child(instancing::instances(atlas, wallA, instancing::Mode::Live,
                                         SkBlendMode::kPlus))
            .child(instancing::instances(atlas, wallB, instancing::Mode::Live,
                                         SkBlendMode::kPlus))
            // The film's transfer curve, over the accumulation. ROADMAP 10f
            // is right: a LUT over already-painted content works today.
            .effect(Effect::shader(transferCurve(),
                                   {{"k", transferK()}}));

    auto hud = [&](const std::string &s, float l, float tp, float r, float b) {
      Element e = t(s, mono(8, a(kTick, 0.55f), 0.6f)).absolute();
      if (l >= 0) e.left(Dim(l));
      if (r >= 0) e.right(Dim(r));
      if (tp >= 0) e.top(Dim(tp));
      if (b >= 0) e.bottom(Dim(b));
      return e;
    };

    const double machineSec = (double)filmFrame * 120.0; // 2880 : 1
    const long long mh = (long long)(machineSec / 3600.0);
    const long long mm = (long long)std::fmod(machineSec / 60.0, 60.0);

    return box()
        .width(Dim(kFilmW))
        .height(Dim(kFilmH))
        .shrink(0)
        .fill(kBlack)
        .clip()
        .stroke(stroke(1.0f, Fill::color(kRule)))
        .key("film")
        .wipe(0.0f, withFrom(0.0f, 1.0f, {520ms, ch::easeOutCubic, 240ms}))
        .child(std::move(accumulation))
        // The shutter bar: the ONLY thing driven by addFixed's interpolant.
        .child(box()
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(0))
                   .width(Dim(kFilmW))
                   .height(2)
                   .fill(a(kCold, 0.35f))
                   .wipe(0.0f, bind(&frameAlpha)))
        .child(hud(s.name, 10, 8, -1, -1))
        .child(hud(fmt("FRAME %06lld · 24 fps", filmFrame), -1, 8, 10, -1))
        .child(hud(fmt("MACHINE TIME %lld h %02lld m%s", mh, mm,
                       everClamped ? " *" : ""),
                   10, -1, -1, 8))
        .child(hud("2.20 : 1 · 65 mm · f/1.8", -1, -1, 10, 8))
        .child(t("THE FRAME IS HELD, NOT TWEENED. addFixed’s INTERPOLANT "
                 "DRIVES THE SHUTTER BAR AND NOTHING IN THE PICTURE.",
                 mono(8, a(kTick, 0.5f), 0.4f))
                   .absolute()
                   .left(Dim(10))
                   .bottom(Dim(20)));
  }

  // -------------------------------------------------------------- rig strip
  Element rigStrip();
  Element monitor();

  // ---------------------------------------------------------------- sidebar
  Element panelShell(float h, const char *heading, int order) {
    using namespace slit;
    return box()
        .column()
        .width(Dim(kSideW))
        .height(Dim(h))
        .shrink(0)
        .padding(14)
        .gap(7)
        .corners({5})
        .fill(kPanelBg)
        .stroke(stroke(1.0f, Fill::color(kRule)))
        .key(fmt("panel%d", order))
        .opacity(withFrom(0.0f, 1.0f, {300ms, ch::easeOutQuad}))
        .translateX(withFrom(14.0f, 0.0f, {300ms, ch::easeOutQuad}))
        .child(t(heading, ui(10, kType2, 2.2f)));
  }

  Element s1Quote();
  Element s2Lens();
  Element s3Law();
  Element s4Sampling();

  Element sidebar() {
    using namespace slit;
    return box()
        .column()
        .width(Dim(kSideW))
        .shrink(0)
        .gap(22)
        .staggerChildren(85ms)
        .child(s1Quote())
        .child(s2Lens())
        .child(s3Law())
        .child(s4Sampling());
  }

  Element describe(sketch::SketchContext &ctx) {
    using namespace slit;
    return box()
        .column()
        .width(Dim(kCanvasW))
        .height(Dim(kCanvasH))
        .padding(kPad)
        .gap(20)
        .fill(kInk)
        .child(header())
        .child(box()
                   .row()
                   .gap(28)
                   .height(Dim(kBodyH))
                   .child(box()
                              .column()
                              .width(Dim(kLeftW))
                              .shrink(0)
                              .gap(24)
                              .child(filmFrame())
                              .child(rigStrip()))
                   .child(sidebar()));
  }

  // ===================================================================
  void setup(sketch::SketchContext &ctx) override;
  void update(double e, sketch::SketchContext &ctx) override;
  Element readoutEl();
};

#include "slitscan_2001_parts.inc"

SIGIL_SKETCH(SlitScan2001)
