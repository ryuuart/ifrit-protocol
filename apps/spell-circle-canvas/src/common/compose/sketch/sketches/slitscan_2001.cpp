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
//          exposure, and every pixel of the film frame below is the sum of
//          812 additive stamps -- nothing paints it.
//
// -----------------------------------------------------------------------------
// SOURCES -- read, not remembered
//
//  [T68] Douglas Trumbull, "Creating Special Effects for 2001: A Space
//        Odyssey", American Cinematographer 49(6):416-420, 451-453, June 1968.
//        http://www.visual-memory.co.uk/sk/2001a/page3.html
//        *** READ DIRECTLY. The brief warned the host's TLS certificate had
//        expired and that WebFetch refuses it; `curl -k` does not. The
//        passage is verbatim as the brief had it -- AND the reprint carries
//        three photo captions no secondary carrier reproduced, which settle
//        FINDING 1 outright. Reading the primary was worth more than every
//        secondary in this file.
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
//        is an independent test of the mechanism. Verification E runs it
//        FORWARDS.
//  [AP]  "The Age of Plastic", 2001: Film, Cameras and Lenses -- the Super
//        Panavision 70 spherical prime list.
//  [WP]  Wikipedia, "70 mm film" (apertures); "Slit-scan photography"
//        (the attribution -- checked for citations, see 3 below).
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
// AND, in the SAME article, three sentences later, as photo captions:
//
//   "(Left) Designer assembling a bank of photo-electric control devices for
//    slit-scan mechanism. (Center) 65mm Todd-AO camera with follow-focus
//    device mounted for operation of slit-scan set-up. (Right) Todd-AO
//    camera with Acme stop-motion motor and SELSYN-DRIVEN FOLLOW-FOCUS
//    mechanism."
//
// -----------------------------------------------------------------------------
// WHAT THIS STUDY VERIFIED, AND WHAT CHANGED (all printed on canvas)
//
//  1. THE DEPTH-OF-FIELD SENTENCE CANNOT MEAN WHAT IT SAYS -- AND THE
//     ARTICLE ITSELF SAYS WHAT IT MEANT. At f/1.8, c = 0.05 mm, with the
//     self-consistent f = 28.64 mm, the DOF at 1.5 in is 0.0791 mm against
//     a claimed bracket of 4533.9 mm: a ratio of 5.7e4. (The brief's 5.1e4
//     is right for a flat f = 28.0 mm; both are ~5e4.) For the sentence to
//     be literally true, one setting would need a hyperfocal of 76.2 mm,
//     i.e. f = 1.83 / 2.57 / 3.14 / 3.61 mm for c = 0.025 / 0.05 / 0.075 /
//     0.1 -- a 169-degree field on a 52.6 mm frame. Read the fifteen feet
//     as a HYPERFOCAL NEAR LIMIT and the closed form
//     f = (-Nc + sqrt((Nc)^2 + 4HNc))/2 at H = 9144 mm gives 20.26 / 28.64 /
//     35.07 / 40.48 mm. ALL EIGHT ROWS RE-DERIVED; all eight agree with the
//     brief, INCLUDING the c = 0.075 row the researcher corrected on his
//     second pass (35.07, not 26.1).
//     What the machine held was FOCUS, SERVOED TO THE TRACK -- and I did
//     not have to infer it. [T68]'s own captions say "follow-focus device"
//     and "SELSYN-DRIVEN follow-focus mechanism". A selsyn is a synchro: a
//     shaft-position servo. The erratum is settled by the same page that
//     carries it.
//  1b. AND THE LENS EXISTS. [AP] lists Panavision's Super Panavision 70
//     spherical primes as 28mm T2.8, 35mm T2.8, 50mm T2.0, 75mm T2.8,
//     100mm T2.5, 150mm T3, and notes 28/50/75/100 in the film's own
//     continuity reports. The back-solved 28.64 mm lands ON a lens
//     Panavision made for this format and that 2001 actually used.
//  1c. A NEW ERRATUM, IN THE OTHER DIRECTION: f/1.8 IS FASTER THAN ANY LENS
//     ON THAT LIST (widest T2.8). So the Slit-Scan lens was probably not
//     Panavision glass -- and [AP] records Kubrick using Nikon still lenses
//     whose image circle covers the 65 mm frame. Flagged, not resolved.
//  1d. AND ANOTHER: TO FOCUS ANY OF THOSE LENSES AT 1.5 in YOU NEED A
//     BELLOWS, NOT A FOCUS RING. m = f/(u-f) at u = 38.1 mm is 3.03x for
//     f = 28.64 (86.7 mm of extension), 11.56x for f = 35.07, and NEGATIVE
//     for f = 40.48 -- at c = 0.1 the back-solved lens is LONGER than the
//     subject distance and forms no image at all. So the servoed-focus
//     reading implies a servoed EXTENSION over fourteen feet of track,
//     which is a much larger mechanical claim than a follow-focus, and is
//     exactly what "selsyn-driven" buys.
//  2. THE TWO PUBLISHED DISTANCE FIGURES DISAGREE BY 10.5 INCHES. [T68]'s
//     pair implies 178.5 in = 14 ft 10.5 in of travel; [C85] says 14 ft
//     flat, putting the near end at 12 in, not 1.5 in. 6.2500% of the
//     track, 33 years apart, same source. Re-checked; exact. This study
//     uses [T68] for the 120.0:1 RATIO (which the picture depends on) and
//     [C85] for the TRACK (which the diagram depends on).
//  3. THE ATTRIBUTION. [T68] credits "a technique of image scanning as used
//     in scientific and industrial photography" and never mentions John
//     Whitney. Wikipedia says Whitney developed the technique for Vertigo's
//     titles -- I checked, and THAT SENTENCE CARRIES NO INLINE CITATION.
//     The adjacent sentence ("after he sent some test sequences on film to
//     Stanley Kubrick, the technique was adapted by Douglas Trumbull")
//     cites a 2010 Trumbull Master Class at TIFF Bell Lightbox, for which I
//     could reach no transcript. So the honest line is narrower than the
//     brief's: the 1968 attribution is the designer's own and names
//     industrial image scanning; the Whitney lineage rests on one uncited
//     sentence plus one un-transcribed talk given 42 years after the fact.
//     (Study 02 of this programme documented the Vertigo spirals as a
//     pendulum on an M-5 gun director. A pen on a rotating plate is not a
//     moving slit.)
//  4. THE 65 mm APERTURES CHECK OUT. [WP] "70 mm film": camera aperture
//     52.63 x 23.01 mm (2.072 x 0.906 in) = 2.287:1; projection aperture
//     48.56 x 22.10 mm (1.912 x 0.870 in) = 2.1973:1 ~ 2.20:1. Corroborated,
//     NOT read against SMPTE; a second carrier gives 2.066 in for the
//     camera width. Both circulate.
//  5. K = 400 IS UNDER ITS OWN DERIVATION. The overlap condition is
//     d(ln u) <= ln(1 + w/X0), not <= w/X0. With X0/w = 84 that is
//     K >= 1 + ln(120)/ln(1 + 1/84) = 405.6, so K_min = 406. The brief's
//     402 linearised the logarithm. This file stamps 406 per wall, 812 in
//     the frame.
//  6. [GE]'s UNWRAP PINS THE ARTWORK ADVANCE, WHICH NO SOURCE PUBLISHES. If
//     the panel advanced MORE than one slit-width per frame the unwrap
//     would show gaps; LESS, and repeats. Ercolano recovered coherent
//     artwork from four shots, so the advance is ~= the slit width. The
//     brief listed 0.9 in/frame as a free parameter; it is not free, and
//     this file uses w = 0.5857 in/frame. An independent observation
//     constraining a reconstructed parameter is the best thing here.
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
//   z0/z1 = 180 in / 1.5 in = 120.0 : 1 EXACTLY; every scale in the film
//   frame hangs off it. EXPOSURE ALONG A STREAK FALLS AS 1/rho: dwell at
//   film radius u is f*w/(V*u), and irradiance from an extended source is
//   distance-invariant at fixed aperture. THE SUB-EXPOSURES MUST BE SPACED
//   UNIFORMLY IN ln z (K_min = 406) AND WEIGHTED BY omega ~ z; equal-weight
//   log stamps are band-free AND FLAT, which is the wrong physics.
//   MACHINE:SCREEN = 24 x 90..120 s = 2160..2880 : 1 (5760 : 1 on [FS]).
//   ONE SECOND OF THE STAR GATE COST 36 TO 96 MINUTES OF MACHINE TIME.
//
// RECONSTRUCTED (mine -- NO SOURCE PUBLISHES THE GEOMETRY, and [T68] says
//   why: "Stanley Kubrick strongly emphasized ... that he wished the
//   specific techniques used in the last sequence to remain as unpublicized
//   as possible, so ... I will describe them only briefly without specific
//   details." The record is thin BECAUSE THE DIRECTOR ASKED FOR IT TO BE.)
//   X0 = 49.2 in and w = X0/84 = 0.5857 in, fitted to theta = 26 deg and an
//   82:1 opening -- which puts X0 more than four feet off axis, outside a
//   six-foot plate centred on the axis. THE WEAKEST JOINT HERE, printed as
//   such. Constant camera speed in z; the plate's rotation schedule; every
//   gel colour (built from the PROCESS -- saturated gels added on black --
//   never eyedropped from a transfer); the transfer D = 1 - e^(-kE); the
//   three artwork strips; all layout, chrome, loop timing and type. And the
//   2-D reduction, which is EXACT rather than simplified: a pinhole on a
//   straight track pointed at a plane reduces to the plane, because every
//   source point's image runs along a ray through the principal point.
//
// CAVEAT ON THE PINHOLE, printed rather than hidden: the picture is drawn to
//   the PINHOLE ratio 120:1. A real thin lens at f = 28.64 mm has
//   m = f/(u-f), so the magnification ratio over the same sweep is
//   (4572-28.64)/(38.1-28.64) = 480:1 -- four times more flare. The two
//   agree only while f << u, which fails hard at the near end.
// =============================================================================


#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Pattern.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

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

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
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

// K_min = 1 + ln(R)/ln(1 + w/X0) = 405.6 -> 406. The brief said 402; it
// linearised the logarithm.
constexpr int kK = 406;
// AND K_min REMOVES GAPS, NOT RIPPLE -- a finding of this build, measured
// below. drawSpriteAtlas's drawVertices is not antialiased, so at exactly
// K_min, where adjacent stamps only just touch, the covering count beats
// between one and two on the pixel grid. Verification D prints the residual
// at both K, and the fitted exponent is untouched by it: the LAW survives
// its own quantisation. The picture stamps four times the minimum.
constexpr int kOversample = 4;
constexpr int kKDisplay = kK * kOversample;

// The film frame is drawn to its own 120:1 scale: 1 px from the vanishing
// point is 15 ft from the lens; 600 px is one and a half inches.
constexpr float kUFar = 5.0f;
constexpr float kUNear = kUFar * kR; // 600 px

// The atlas cell IS the twelve-foot panel, at one scale.
constexpr float kCellPxPerIn = 10.25f;
constexpr float kCellW = kPanelIn * kCellPxPerIn;  // 1476 px
constexpr float kCellH = kSlitHIn * kCellPxPerIn;  // 492 px
constexpr float kWinFrac = kSlitWIn / kPanelIn;    // 0.00406746
// on-screen width = w*ppi*s and height = h*ppi*s, so s(u) = u/(X0*ppi)
// makes width = u*w/X0 and height = u*h/X0 EXACTLY. ONE scale, both axes:
// a perspective projection is uniform by construction, and this is the
// first study in the programme whose geometry is uniform by construction
// rather than by luck.
constexpr float kScaleDen = kX0In * kCellPxPerIn;  // 504.3

// ---------------------------------------------------------------------------
// Chrome -- a darkroom / optical-bench register

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
constexpr SkColor4f kBlack{0, 0, 0, 1};        // "the room was painted
                                               //  totally black" [C85]
constexpr SkColor4f kWhite{1, 1, 1, 1};

// The gels. RECONSTRUCTED from the process -- saturated subtractive filters
// on black, ADDED -- never eyedropped from a transfer of a 1968 print.
constexpr SkColor4f kGelRed{1.0f, 0.180f, 0.122f, 1};     // #FF2E1F
constexpr SkColor4f kGelAmber{1.0f, 0.541f, 0.039f, 1};   // #FF8A0A
constexpr SkColor4f kGelStraw{1.0f, 0.890f, 0.302f, 1};   // #FFE34D
constexpr SkColor4f kGelGreen{0.231f, 0.878f, 0.541f, 1}; // #3BE08A
constexpr SkColor4f kGelCyan{0.145f, 0.714f, 1.0f, 1};    // #25B6FF
constexpr SkColor4f kGelViolet{0.478f, 0.298f, 1.0f, 1};  // #7A4CFF
constexpr SkColor4f kGelMag{1.0f, 0.247f, 0.627f, 1};     // #FF3FA0

inline SkColor4f al(SkColor4f c, float a) { c.fA = a; return c; }

// ---------------------------------------------------------------------------
// Layout, exact: 32 + 96 + 20 + 780 + 32 = 960; 1056 + 28 + 412 = 1496;
// 480 + 24 + 276 = 780; and 1056 / 480 = 2.200, the projected aspect.

constexpr float kCanvasW = 1560, kCanvasH = 960;
constexpr float kPad = 32, kHeaderH = 96, kBodyH = 780;
constexpr float kLeftW = 1056, kSideW = 412;
constexpr float kFilmW = 1056, kFilmH = 480;
constexpr float kRigW = 1056, kRigH = 276;
constexpr float kRigPxPerIn = 3.0f;   // the rig strip's ONE scale
constexpr float kElevW = 596;         // 8 + 540 (focus reach) + 48 of air
constexpr float kPanelStripW = 432;   // 144 in at 3.0 px/in, exact

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
                             float track = 0.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(tf);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
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
/** The quotation register: condensed 0.94 with 0.4 of tracking. */
sigil::weave::TextStyle quo(float s, SkColor4f c) {
  sigil::weave::TextStyle st = type(uiFace(), s, c, 0.4f);
  st.condense(0.94f);
  return st;
}

Element t(const std::string &s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

std::string fmt(const char *f, ...) {
  char buf[640];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return std::string(buf);
}

Element rule(float w, SkColor4f c, float h = 1.0f) {
  return box().width(Dim(w)).height(Dim(h)).shrink(0).fill(c);
}

// ---------------------------------------------------------------------------
// The film's transfer curve: D = 1 - e^(-kE), applied to the ACCUMULATION.
//
// ROADMAP 10f used to say a palette LUT was unreachable, and was corrected
// before this was built: Effect::shader's layer arrives as the child shader
// named `content`, so a transfer curve over already-painted content is one
// call. Exercised here at a real call site, over a Mode::Live leaf.

sk_sp<SkRuntimeEffect> transferCurve() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    const char *src = R"(
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
    if (!e)
      std::fprintf(stderr, "[slitscan] transfer sksl: %s\n", err.c_str());
    return e;
  }();
  return fx;
}

// ---------------------------------------------------------------------------
// THE ARTWORK -- generated, not drawn.
//
// [C85]: high-contrast NEGATIVES of op-art paintings, architectural drawings
// and prints of electrical circuits. [GE], independently, recovered coral /
// flowers, microscopic and botanical photography, backlit graphic shapes
// with scrolling graphics, and composites of abstract shapes and spirals. So
// the sketch generates those KINDS of thing and thresholds the result to one
// bit, which is what "high-contrast negative" means. The gel then lives
// entirely in the tints() lane and no palette material is needed anywhere in
// this file -- which is exactly why this study ranks the two-source-Material
// item DOWN rather than citing it.

struct Strip {
  sk_sp<SkImage> image;
  std::vector<uint8_t> lum; // 0/255, row-major -- fuel for verification E
  int w = 0, h = 0;
};

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
  out.lum.assign((size_t)W * (size_t)H, 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const SkColor s = bm.getColor(x, y);
      const float l = (0.2126f * (float)SkColorGetR(s) +
                       0.7152f * (float)SkColorGetG(s) +
                       0.0722f * (float)SkColorGetB(s)) /
                      255.0f;
      bool on = l > thresh;
      // [GE]'s "white stripe artifact" -- one 3 px full-height run at a
      // fixed x, present in BOTH shots that share this strip. He found the
      // same defect in Shot 27 and Shot 29 and concluded the same physical
      // artwork was reused with different filtering. Not explained in the
      // picture; explained in the caption.
      if (stripeX >= 0 && x >= stripeX && x < stripeX + 7)
        on = true;
      out.lum[(size_t)y * (size_t)W + (size_t)x] = on ? 255 : 0;
      *bm.getAddr32(x, y) = on ? SkPreMultiplyARGB(255, 255, 255, 255) : 0u;
    }
  bm.setImmutable();
  out.image = bm.asImage();
  return out;
}

/** Op-art: concentric rings over a dense bar lattice -- "backlit graphic
 *  shapes with scrolling graphics" [GE]. */
Element artOpArt() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  for (int i = 0; i < 122; ++i) {
    const float x = (float)i * 12.1f;
    const float hh = 30.0f + 220.0f * std::fabs(std::sin((float)i * 0.2131f));
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(kCellH * 0.5f - hh * 0.5f))
                .width(5)
                .height(Dim(hh))
                .fill(Fill::color(kWhite)));
  }
  for (int i = 0; i < 13; ++i) {
    const float cx = 56.0f + (float)i * 114.0f;
    g.child(box()
                .absolute()
                .left(Dim(cx - 170.0f))
                .top(Dim(kCellH * 0.5f - 170.0f))
                .width(340)
                .height(340)
                .outline(shapes::circle())
                .foreground(lines::concentric(Fill::color(kWhite),
                                              9 + (i % 5) * 4, 6.0f)));
  }
  return g;
}

/** Architectural drawing + spirals: a hatched section, elevation courses,
 *  logarithmic spirals. */
Element artArch() {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  g.child(box().absolute().inset(0).foreground(
      lines::hatch(Fill::color(kWhite), 9.0f, 2.6f, 58.0f)));
  for (int i = 0; i < 8; ++i) {
    const float x = 20.0f + (float)i * 182.0f;
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(30))
                .width(176)
                .height(432)
                .outline(shapes::spiral(2.4f + 0.35f * (float)i, true, 0.36f))
                .foreground(stroke(8.0f, Fill::color(kWhite))));
    for (int k = 0; k < 11; ++k)
      g.child(box()
                  .absolute()
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
Element artCircuit(Pattern &grid, Pattern &spek) {
  Element g = box().width(Dim(kCellW)).height(Dim(kCellH));
  g.child(box().absolute().inset(0).fill(grid.material()));
  g.child(box().absolute().inset(0).fill(spek.material()));
  for (int i = 0; i < 110; ++i) {
    const float x = std::fmod((float)i * 137.31f, kCellW - 34.0f) + 12.0f;
    const float y = std::fmod((float)i * 271.7f, kCellH - 34.0f) + 12.0f;
    g.child(box()
                .absolute()
                .left(Dim(x))
                .top(Dim(y))
                .width(26)
                .height(26)
                .outline(shapes::annulus(0.52f))
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
  float artDrift = 0.0f; // the panel also slides DURING the exposure [NO]
  int cell = 0;
  float upTo = -1.0f;    // suppress stamps past this fraction (the monitor)
  bool artwork = true;
  int K = kKDisplay;
  bool logSpaced = true; // false = uniform in z, the S4 counter-example
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
void buildWall(instancing::Pool &p, const WallSpec &s) {
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
      w = s.gain * uniformW; // equal weight: the travel IS constant
    }
    const float scale = u / kScaleDen;
    const float wpx = kSlitWIn * kCellPxPerIn * scale; // on-screen width
    const float uc = u * (1.0f + 0.5f / kX0OverW);     // CENTRE, not edge
    pos[j] = {s.vp.fX + cs * uc, s.vp.fY + sn * uc};
    rot[j] = phi;
    sc[j] = scale;
    fr[j] = s.cell;
    // AREA-CONSERVING SUB-PIXEL CLAMP, and the one thing nothing predicted.
    // Below u = X0/w px the stamp is narrower than a pixel, and
    // drawSpriteAtlas's drawVertices is NOT antialiased -- so most of the
    // sweep would silently vanish. Widen to one pixel through the sizes()
    // lane and scale the weight by the true width, leaving the deposited
    // light unchanged. That is a box filter, and it is why the measured
    // exponent holds at 1 straight through the apex.
    float sx = 1.0f;
    if (wpx < 1.0f) {
      sx = 1.0f / std::max(wpx, 1e-4f);
      w *= wpx;
    }
    sz[j] = {sx, 1.0f};
    ti[j] = {s.gel.fR, s.gel.fG, s.gel.fB, std::clamp(w, 0.0f, 1.0f)};
    if (s.upTo >= 0.0f && f > s.upTo)
      ti[j].fA = 0.0f;
    // Pool::texWindows() -- ONE bake of the twelve-foot panel, addressed at
    // a different window per stamp. Continuous, not quantised: the brief's
    // planned workaround was 64 pre-registered cells and a stepped crawl,
    // because Atlas::cell() drops the whole sheet when you re-register.
    const float left =
        s.artwork ? std::fmod(s.artLeft + s.artDrift * f + 8.0f, 1.0f) : 0.0f;
    win[j] = SkRect::MakeXYWH(std::min(left, 1.0f - kWinFrac), 0.0f, kWinFrac,
                              1.0f);
  }
  p.touch();
}

} // namespace slit

// ===========================================================================

struct SlitScan2001 : sigil::compose::sketch::Sketch {
  // ---- the film clock: 24 Hz because the film runs at 24 fps -------------
  ch::Output<float> frameAlpha{0};
  sigil::motion::Ticker::FixedStatus fixedStatus;
  long long filmNo = 0;
  bool everClamped = false;

  // ---- the demonstration clock: one sweep per 3.0 s ----------------------
  double tau = 0.0, elapsed = 0.0;

  // ---- the machine -------------------------------------------------------
  std::shared_ptr<instancing::Atlas> atlas, flatAtlas;
  std::shared_ptr<instancing::Pool> wallA, wallB, monA, monB;
  std::array<std::shared_ptr<instancing::Pool>, 6> s4;
  std::array<slit::Strip, 3> strips;
  Pattern gridPat, spekPat;
  float artOffset = 0.0f;
  float plateDeg = 0.0f;
  int shot = 0;

  // ---- what the sketch measured about itself -----------------------------
  float fitP = 0, fitR2 = 0, fitResid = 0, fitP95 = 0;
  std::array<float, 120> profX{}, profY{};  // the measured profile
  int profN = 0;
  float fitPMin = 0, fitResidMin = 0; // the same fit at K = K_min
  int fitRays = 0, fitPts = 0;
  float rtMaxErr = -1, rtCorr = 0, rtMismatch = 0;
  int rtCols = 0;
  int sheetW = 0, sheetH = 0;
  double bakeMs = 0, measMs = 0, rtMs = 0;

  struct Shot {
    const char *name;
    SkColor4f gelA, gelB;
    float phi0, dPhi;
    int cell;
    const char *art;
  };
  /** 96 film frames = 4.0 s at 24 fps. The shot index is derived from the
   *  FRAME COUNTER, never from the wall clock -- verification F caught the
   *  difference: `addFixed` is exact across draw rates (frame 148 and the
   *  artwork offset matched to the last digit at 60 and 30 fps), but the
   *  plate's rotation was accumulated inside the fixed step from a `shot`
   *  that update() set at the DRAW rate, so the two runs charged different
   *  steps to different dPhi and diverged by 0.85 degrees. A fixed-rate
   *  step must not read a variable-rate value. */
  static int shotFor(long long frame) { return (int)((frame / 96) & 3); }

  static const Shot &shotAt(int i) {
    using namespace slit;
    static const Shot k[4] = {
        {"SEQ 29 · SH 04", kGelStraw, kGelCyan, 0.0f, 0.35f, 0,
         "BACKLIT GRAPHIC SHAPES, FAST SCROLL [GE]"},
        {"SEQ 29 · SH 08", kGelMag, kGelGreen, 34.0f, -0.50f, 1,
         "ABSTRACT SHAPES AND SPIRALS [GE]"},
        {"SEQ 29 · SH 27", kGelRed, kGelAmber, 78.0f, 0.20f, 2,
         "POSTERISED CORAL / FLOWERS [GE]"},
        {"SEQ 29 · SH 29", kGelViolet, kGelCyan, 12.0f, -0.35f, 2,
         "MICROSCOPIC / BOTANICAL — SAME STRIP AS SH 27 [GE]"},
    };
    return k[i & 3];
  }

  // The corridor banks; it does not sit still. Within +-96 x +-44 px.
  SkPoint vp() const {
    return {slit::kFilmW * 0.5f + 96.0f * (float)std::sin(elapsed * 0.41),
            slit::kFilmH * 0.5f +
                44.0f * (float)std::sin(elapsed * 0.67 + 1.1)};
  }
  static float displayGain() { return 30.0f; }
  static float transferK() { return 1.35f; }

  // ------------------------------------------------------------- the walls
  void rebuildWalls() {
    using namespace slit;
    const Shot &s = shotAt(shot);
    const SkPoint c = vp();
    WallSpec A;
    A.vp = c;
    A.phiDeg = s.phi0 + plateDeg;
    A.gel = s.gelA;
    A.gain = displayGain();
    A.artLeft = artOffset;
    // [NO]: "the artwork slid slowly left or right DURING the exposure".
    // Eighteen slit widths across one sweep, so the artwork a streak reads
    // changes along its length -- which is what gives the corridor its
    // rungs rather than a clean radial fan.
    A.artDrift = 18.0f * kWinFrac;
    A.cell = s.cell;
    WallSpec B = A;
    B.phiDeg = A.phiDeg + 180.0f; // "two seemingly infinite planes" [T68]
    B.gel = s.gelB;
    B.artLeft = std::fmod(artOffset + 0.37f, 1.0f);
    B.artDrift = -18.0f * kWinFrac;
    buildWall(*wallA, A);
    buildWall(*wallB, B);

    // The monitor: the SAME two exposures at the demonstration rate, with
    // stamps beyond the carriage's current z suppressed. It is the only
    // place in the plate where you see a frame BEING MADE rather than made.
    const float mScale = 108.0f / kUNear;
    WallSpec MA = A;
    MA.vp = {124.0f, 60.0f};
    MA.uFar = kUFar * mScale;
    MA.upTo = (float)tau;
    MA.K = kK;
    WallSpec MB = MA;
    MB.phiDeg = B.phiDeg;
    MB.gel = B.gel;
    MB.artLeft = B.artLeft;
    MB.artDrift = B.artDrift;
    buildWall(*monA, MA);
    buildWall(*monB, MB);
  }

  // ==================================================================== 12-D
  // MEASURE the exposure profile; do not assume it. Nothing in the library
  // can read back what it drew -- Debug.h is entirely path-level, every
  // check it offers is a check on geometry you already hold -- so this
  // builds the raster by hand: snapshot the accumulation subtree, replay it
  // into an F16 surface (F16 because the outer streak is 1/120 of the apex
  // and eight bits would quantise it to two levels), and readPixels.
  //
  // Measured at UNIT GAIN through a UNIFORM slit cell: the LAW, not the
  // picture. The gain the frame uses clips the apex on purpose -- that IS
  // the film's shoulder -- and the artwork modulates the envelope, which is
  // verification E's business. Measuring the drawn frame would measure all
  // three at once and prove none of them.
  struct Fit {
    float p = 0, r2 = 0, worst = 0, p95 = 0;
    int rays = 0, pts = 0;
    std::array<double, 120> sum{};
    std::array<int, 120> cnt{};
  };

  Fit fitAtK(sigil::weave::FontContext &fonts, int K) {
    using namespace slit;
    Fit out;
    auto pa = std::make_shared<instancing::Pool>();
    auto pb = std::make_shared<instancing::Pool>();
    const SkPoint c{kFilmW * 0.5f, kFilmH * 0.5f};
    WallSpec A;
    A.vp = c;
    A.gel = kWhite;
    A.gain = 1.0f;
    A.artwork = false;
    A.K = K;
    WallSpec B = A;
    B.phiDeg = 180.0f;
    buildWall(*pa, A);
    buildWall(*pb, B);

    Element accum = box()
                        .width(Dim(kFilmW))
                        .height(Dim(kFilmH))
                        .child(instancing::instances(flatAtlas, pa,
                                                     instancing::Mode::Data,
                                                     SkBlendMode::kPlus))
                        .child(instancing::instances(flatAtlas, pb,
                                                     instancing::Mode::Data,
                                                     SkBlendMode::kPlus));
    // snapshot() sizes by the root's CHILDREN, not the root's own dims
    // (ROADMAP 10j) -- hence the shell. Without it the read-back is a
    // silently wrong answer, which is exactly what that entry warns about.
    sk_sp<SkPicture> pic = snapshot(box().child(std::move(accum)), fonts);
    sk_sp<SkSurface> surf = SkSurfaces::Raster(SkImageInfo::Make(
        (int)kFilmW, (int)kFilmH, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
    if (!pic || !surf)
      return out;
    surf->getCanvas()->clear(SK_ColorTRANSPARENT);
    surf->getCanvas()->drawPicture(pic);

    const int W = (int)kFilmW, H = (int)kFilmH;
    const SkImageInfo dst = SkImageInfo::Make(W, H, kRGBA_F32_SkColorType,
                                              kUnpremul_SkAlphaType);
    std::vector<float> px((size_t)W * (size_t)H * 4);
    if (!surf->readPixels(dst, px.data(), (size_t)W * 16, 0, 0))
      return out;
    auto lumAt = [&](float x, float y) -> float {
      const int xi = (int)std::lround(x), yi = (int)std::lround(y);
      if (xi < 0 || yi < 0 || xi >= W || yi >= H)
        return -1;
      const float *p = &px[((size_t)yi * (size_t)W + (size_t)xi) * 4];
      return (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) * p[3];
    };

    // 32 rays inside the wedge (half-angle 26 deg; sampled to 20 deg so the
    // stamp rectangles' corners never truncate a ray), 120 log-spaced radii.
    const float u0 = 8.0f, u1 = 520.0f;
    double sp = 0, sr2 = 0;
    std::vector<float> resid;
    for (int w = 0; w < 2; ++w)
      for (int r = 0; r < 16; ++r) {
        const float dd = (-20.0f + 40.0f * (float)r / 15.0f) * 0.017453292f;
        const float ang = (w ? 3.14159265f : 0.0f) + dd;
        std::vector<double> lx, ly;
        std::array<int, 120> bin{};
        for (int i = 0; i < 120; ++i) {
          const float u = u0 * std::pow(u1 / u0, (float)i / 119.0f);
          const float v =
              lumAt(c.fX + std::cos(ang) * u, c.fY + std::sin(ang) * u);
          bin[(size_t)i] = -1;
          if (v > 1e-6f) {
            bin[(size_t)i] = (int)lx.size();
            lx.push_back(std::log((double)u));
            ly.push_back(std::log((double)v));
          }
        }
        if (lx.size() < 40)
          continue;
        const size_t n = lx.size();
        double mx = 0, my = 0;
        for (size_t i = 0; i < n; ++i) { mx += lx[i]; my += ly[i]; }
        mx /= (double)n;
        my /= (double)n;
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
          resid.push_back((float)std::fabs(std::exp(e) - 1.0));
        }
        // Keep the profile itself, each ray levelled by its own intercept,
        // so the plot shows MEASURED samples rather than a replay of the fit.
        for (int i = 0; i < 120; ++i)
          if (bin[(size_t)i] >= 0) {
            out.sum[(size_t)i] += ly[(size_t)bin[(size_t)i]] - inter;
            out.cnt[(size_t)i] += 1;
          }
        sp += -slope;
        sr2 += syy > 0 ? 1.0 - ss / syy : 0.0;
        ++out.rays;
        out.pts += (int)n;
      }
    if (out.rays) {
      out.p = (float)(sp / (double)out.rays);
      out.r2 = (float)(sr2 / (double)out.rays);
      std::sort(resid.begin(), resid.end());
      out.worst = resid.empty() ? 0 : resid.back();
      out.p95 = resid.empty() ? 0 : resid[(size_t)(0.95 * (double)resid.size())];
    }
    return out;
  }

  void measureExposure(sigil::weave::FontContext &fonts) {
    const double t0 = (double)std::clock() / CLOCKS_PER_SEC;
    const Fit big = fitAtK(fonts, slit::kKDisplay);
    const Fit small = fitAtK(fonts, slit::kK);
    fitP = big.p; fitR2 = big.r2; fitResid = big.worst; fitP95 = big.p95;
    fitRays = big.rays; fitPts = big.pts;
    fitPMin = small.p; fitResidMin = small.worst;
    // Normalise the measured profile onto the same axes the analytic C/u
    // uses: an exact 1/u law is then the box diagonal, and every departure
    // -- including the +-1-stamp quantisation ripple -- is a visible wiggle.
    const double span = std::log(520.0 / 8.0);
    profN = 0;
    double anchor = 0;
    int anchorN = 0;
    for (int i = 0; i < 120; ++i)
      if (big.cnt[(size_t)i]) {
        const double le = big.sum[(size_t)i] / (double)big.cnt[(size_t)i];
        anchor += le + span * (double)i / 119.0;
        ++anchorN;
      }
    if (anchorN)
      anchor /= (double)anchorN; // zero-mean against the analytic diagonal
    for (int i = 0; i < 120; ++i) {
      if (!big.cnt[(size_t)i])
        continue;
      const double le = big.sum[(size_t)i] / (double)big.cnt[(size_t)i];
      profX[(size_t)profN] = (float)((double)i / 119.0);
      profY[(size_t)profN] = (float)((anchor - le) / span);
      ++profN;
    }
    measMs = ((double)std::clock() / CLOCKS_PER_SEC - t0) * 1000.0;
  }

  // ==================================================================== 12-E
  // ERCOLANO'S UNWRAP, RUN FORWARDS. He accumulated one scanline per DVD
  // frame and got the artwork back for four production shots. This feeds a
  // known strip in, accumulates one film column per frame across 96 frames,
  // concatenates them, and compares against what went in.
  //
  // The radius is u* = X0*ppi = 504.3 px, where the stamp is EXACTLY the
  // window's baked size -- 6 x 492 px, 1:1 on both axes. At any other radius
  // the residual would be measuring nearest-neighbour resampling phase; at
  // this one it measures TRANSPORT, which is the claim under test.
  void roundTrip(sigil::weave::FontContext &fonts) {
    using namespace slit;
    const double t0 = (double)std::clock() / CLOCKS_PER_SEC;
    const Strip &S = strips[0];
    if (!S.image || S.lum.empty())
      return;
    constexpr int kFrames = 96;
    const int cw = (int)std::lround(kSlitWIn * kCellPxPerIn); // 6
    const int chh = (int)std::lround(kCellH);                 // 492
    const int boxW = cw + 16, boxH = chh + 8;
    const float cx = (float)boxW * 0.5f, cy = (float)boxH * 0.5f;

    auto one = std::make_shared<instancing::Atlas>(1.0f);
    one->filter(SkFilterMode::kNearest);
    one->cell(box().fill(Material::image(
                  S.image, SkTileMode::kClamp, SkTileMode::kClamp,
                  SkMatrix::Scale(kCellW / (float)S.w, kCellH / (float)S.h),
                  SkSamplingOptions())),
              {kCellW, kCellH});
    auto pool = std::make_shared<instancing::Pool>();
    pool->resize(1);

    double sg = 0, sw = 0, sgg = 0, sww = 0, sgw = 0;
    size_t n = 0, bad = 0;
    float worst = 0;
    std::vector<float> got, want;
    const float aStart = 0.11f;
    for (int f = 0; f < kFrames; ++f) {
      const float left = aStart + (float)f * kWinFrac;
      pool->positions()[0] = {cx, cy};
      pool->rotations()[0] = 0;
      pool->scales()[0] = 1.0f; // u* = X0*ppi  =>  s = 1
      pool->tints()[0] = {1, 1, 1, 1};
      pool->frames()[0] = 0;
      pool->sizes()[0] = {1, 1};
      pool->texWindows()[0] = SkRect::MakeXYWH(left, 0, kWinFrac, 1.0f);
      pool->touch();

      Element acc = box()
                        .width(Dim((float)boxW))
                        .height(Dim((float)boxH))
                        .child(instancing::instances(one, pool,
                                                     instancing::Mode::Data));
      sk_sp<SkPicture> pic = snapshot(box().child(std::move(acc)), fonts);
      SkBitmap bm;
      if (!pic || !bm.tryAllocN32Pixels(boxW, boxH))
        return;
      {
        SkCanvas c(bm);
        c.clear(SK_ColorBLACK);
        c.drawPicture(pic);
      }
      const int x0 = (int)std::lround(cx - (float)cw * 0.5f);
      const int y0 = (int)std::lround(cy - (float)chh * 0.5f);
      for (int y = 0; y < chh; ++y)
        for (int x = 0; x < cw; ++x) {
          const SkColor s = bm.getColor(x0 + x, y0 + y);
          const float g = (float)SkColorGetG(s) / 255.0f;
          const float fx = left + ((float)x + 0.5f) / (float)cw * kWinFrac;
          const int sx = std::clamp((int)(fx * (float)S.w), 0, S.w - 1);
          const int sy = std::clamp(
              (int)(((float)y + 0.5f) / (float)chh * (float)S.h), 0, S.h - 1);
          const float wv =
              (float)S.lum[(size_t)sy * (size_t)S.w + (size_t)sx] / 255.0f;
          sg += g; sw += wv; sgg += g * g; sww += wv * wv; sgw += g * wv;
          worst = std::max(worst, std::fabs(g - wv));
          if (std::fabs(g - wv) > 0.5f)
            ++bad;
          ++n;
        }
    }
    if (!n)
      return;
    const double mg = sg / (double)n, mw = sw / (double)n;
    const double cgg = sgg / (double)n - mg * mg;
    const double cww = sww / (double)n - mw * mw;
    const double cgw = sgw / (double)n - mg * mw;
    rtMaxErr = worst;
    rtCorr = (cgg > 0 && cww > 0) ? (float)(cgw / std::sqrt(cgg * cww)) : 0.0f;
    rtMismatch = (float)bad / (float)n;
    rtCols = kFrames * cw;
    rtMs = ((double)std::clock() / CLOCKS_PER_SEC - t0) * 1000.0;
  }

  // =====================================================================
  // Header

  Element header() {
    using namespace slit;
    GlyphFx fx;
    fx.effect = glyphfx::rise(18.0f);
    fx.stagger = {.eachMs = 22};
    fx.progress = with(1.0f, {440ms, ch::easeOutExpo, 120ms});
    return box()
        .column()
        .height(Dim(kHeaderH))
        .gap(4)
        .child(t("TECHNIQUE STUDY 05 · TIME AS AN AXIS OF THE IMAGE",
                 ui(10, kType2, 2.6f))
                   .key("eyebrow")
                   .opacity(withFrom(0.0f, 1.0f, {260ms, ch::easeOutQuad}))
                   .translateY(withFrom(8.0f, 0.0f, {260ms, ch::easeOutQuad})))
        .child(t("THE SLIT-SCAN MACHINE, 1966–68", uiB(40, kType, 0.4f))
                   .key("title")
                   .textStroke(0.6f, Fill::color(kInk))
                   .glyphFx(std::move(fx)))
        .child(t("Douglas Trumbull — ‘Creating Special Effects for 2001: A "
                 "Space Odyssey’, American Cinematographer 49(6):416–420, "
                 "451–453, June 1968 (READ DIRECTLY) · Cinefex 85, April "
                 "2001 · Super Panavision 70, 65 mm 5-perf spherical, "
                 "2.20:1, 24 fps, f/1.8",
                 ui(11, kType2))
                   .key("cite")
                   .opacity(withFrom(0.0f, 1.0f,
                                     {240ms, ch::easeOutQuad, 400ms})));
  }

  // ---------------------------------------------------------- the film frame
  Element filmFrame() {
    using namespace slit;
    const Shot &s = shotAt(shot);

    // THE ACCUMULATION. Two exposures, kPlus, one atlas stamp each.
    auto raw = [this] {
      return box()
          .absolute()
          .inset(0)
          .child(instancing::instances(atlas, wallA, instancing::Mode::Live,
                                       SkBlendMode::kPlus))
          .child(instancing::instances(atlas, wallB, instancing::Mode::Live,
                                       SkBlendMode::kPlus));
    };
    Element accumulation =
        raw().effect(Effect::shader(transferCurve(), {{"k", transferK()}}));
    // HALATION. Film's own bloom: light scattering back off the base. The
    // SAME two pools read a second time, tone-curved softer, blurred and
    // added -- so it is still the accumulation, not a painted glow.
    Element halation =
        raw()
            .effect(Effect::shader(transferCurve(),
                                   {{"k", transferK() * 0.55f}})
                        .then(Effect::filter(
                            SkImageFilters::Blur(9.0f, 9.0f, nullptr))))
            .blend(SkBlendMode::kPlus)
            .opacity(0.55f);

    auto hud = [&](const std::string &str, float l, float tp, float r, float b,
                   SkColor4f col) {
      Element e = t(str, mono(8, col, 0.6f)).absolute();
      if (l >= 0) e.left(Dim(l));
      if (r >= 0) e.right(Dim(r));
      if (tp >= 0) e.top(Dim(tp));
      if (b >= 0) e.bottom(Dim(b));
      return e;
    };

    const double machineSec = (double)filmNo * 120.0; // 2880 : 1
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
        .child(std::move(halation))
        // The shutter bar -- the ONLY thing in the plate driven by
        // addFixed's interpolant, and the caption says why.
        .child(box()
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(0))
                   .width(Dim(kFilmW))
                   .height(2)
                   .fill(al(kCold, 0.4f))
                   .wipe(0.0f, bind(&frameAlpha)))
        .child(hud(s.name, 10, 10, -1, -1, al(kCold, 0.75f)))
        .child(hud(fmt("FRAME %06lld · 24 fps · %d STAMPS/WALL · kPLUS",
                       filmNo, kKDisplay),
                   -1, 10, 10, -1, al(kTick, 0.9f)))
        .child(hud(fmt("MACHINE TIME %lld h %02lld m  @ 2880 : 1%s", mh, mm,
                       everClamped ? "  *" : ""),
                   10, -1, -1, 30, al(kTick, 0.95f)))
        .child(hud("2.20 : 1 · 65 mm 5-PERF · f/1.8", -1, -1, 10, 30,
                   al(kTick, 0.95f)))
        .child(hud("THE FRAME IS HELD, NOT TWEENED. addFixed’s INTERPOLANT "
                   "DRIVES THE SHUTTER BAR AND NOTHING IN THE PICTURE.",
                   10, -1, -1, 18, al(kTick, 0.8f)))
        .child(hud("1 px FROM THE VANISHING POINT = 15 FEET FROM THE LENS · "
                   "600 px = 1½ INCHES · THE FRAME IS THE SCAN, 120 : 1, "
                   "DRAWN TO ITS OWN SCALE — AND THE 5 px HOLE AT THE APEX "
                   "IS WHERE THE SWEEP BEGINS",
                   10, -1, -1, 6, al(kType2, 0.95f)));
  }

  // ------------------------------------------------------------- the rig
  void drawRig(SkCanvas &c, const PaintContext &ctx);
  void drawArtworkPanel(SkCanvas &c, const PaintContext &ctx);
  void drawMeasuredPoints(SkCanvas &c, const PaintContext &ctx);

  Element rigStrip() {
    using namespace slit;
    return box()
        .width(Dim(kRigW))
        .height(Dim(kRigH))
        .shrink(0)
        .key("rig")
        .child(custom([this](SkCanvas &c, const PaintContext &p) {
                 drawRig(c, p);
               })
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(0))
                   .width(Dim(kElevW))
                   .height(Dim(kRigH))
                   .clip()
                   .cache(Cache::None))
        .child(custom([this](SkCanvas &c, const PaintContext &p) {
                 drawArtworkPanel(c, p);
               })
                   .absolute()
                   .left(Dim(kRigW - kPanelStripW))
                   .top(Dim(0))
                   .width(Dim(kPanelStripW))
                   .height(Dim(kRigH))
                   .cache(Cache::None))
        // The "THIS EXPOSURE" monitor, in the elevation's upper-left where
        // there is nothing but sky. The only place you see a frame BEING
        // MADE rather than made, so it gets the good corner.
        .child(box()
                   .absolute()
                   .left(Dim(18))
                   .top(Dim(10))
                   .width(264)
                   .height(116)
                   .corners({4})
                   .fill(al(kPanelBg, 0.92f))
                   .stroke(stroke(1.0f, Fill::color(kRule)))
                   .clip()
                   .child(box()
                              .absolute()
                              .inset(0)
                              .child(instancing::instances(
                                  atlas, monA, instancing::Mode::Live,
                                  SkBlendMode::kPlus))
                              .child(instancing::instances(
                                  atlas, monB, instancing::Mode::Live,
                                  SkBlendMode::kPlus))
                              .effect(Effect::shader(transferCurve(),
                                                     {{"k", 2.4f}})))
                   .child(t("THIS EXPOSURE", mono(8, al(kCold, 0.85f), 1.4f))
                              .absolute()
                              .left(Dim(8))
                              .top(Dim(5)))
                   .child(slot("expo").absolute().left(Dim(8)).bottom(Dim(19)))
                   .child(t("ONE SWEEP / 3.0 s. THE MACHINE TOOK 45–60 s "
                            "[C85]. ×18.",
                            mono(7, al(kTick, 0.95f)))
                              .absolute()
                              .left(Dim(8))
                              .bottom(Dim(6))))
        .child(slot("readout").absolute().left(Dim(20)).top(Dim(130)));
  }

  // ------------------------------------------------------------- sidebar
  /** Every text child inside a panel is shrink(0): a flex column will
   *  otherwise squash a text leaf below its measured height and the run
   *  silently overlaps its neighbour -- the height-axis twin of ROADMAP
   *  14's "a fixed width() flex child still shrinks". */
  Element pl(const std::string &str, sigil::weave::TextStyle st) {
    return slit::t(str, std::move(st)).shrink(0);
  }

  Element panelShell(const char *heading, int order) {
    using namespace slit;
    return box()
        .column()
        .width(Dim(kSideW))
        .shrink(0)
        .padding(11)
        .gap(4)
        .corners({5})
        .fill(kPanelBg)
        .stroke(stroke(1.0f, Fill::color(kRule)))
        .clip()
        .key(fmt("panel%d", order))
        .opacity(withFrom(0.0f, 1.0f, {300ms, ch::easeOutQuad}))
        .translateX(withFrom(14.0f, 0.0f, {300ms, ch::easeOutQuad}))
        .child(pl(heading, ui(9.5f, kType2, 2.2f)))
        .child(rule(390, kRule));
  }

  Element s1Quote() {
    using namespace slit;
    Element p = panelShell("THE MACHINE, IN ITS OWN WORDS", 0);
    p.child(pl("“… this device could produce two seemingly infinite planes "
              "of exposure while holding depth-of-field from a distance of "
              "fifteen feet to one and one-half inches from the lens at an "
              "aperture of F/1.8 with exposures of approximately one minute "
              "per frame using a standard 65mm Mitchell camera.”",
               quo(9.1f, kType)));
    p.child(box()
                .row()
                .gap(5)
                .shrink(0)
                .child(pl("▸", ui(9, kRed)).width(7))
                .child(pl("“holding depth-of-field” IS THE PHRASE THAT "
                         "CANNOT BE TRUE — SEE BELOW",
                         mono(7.1f, kRed))
                           .grow(1)));
    p.child(pl("“… we moved the camera along fourteen feet of track toward "
               "the slit — a full fourteen feet for each exposure. It took "
               "about forty-five seconds to a minute per exposure, and each "
               "frame was made up of two exposures.” [C85]",
               quo(8.4f, al(kType, 0.82f))));
    p.child(rule(390, kRule));
    p.child(pl("[T68] Am. Cinematographer 49(6):416–420, 451–453, Jun 1968 · "
               "[C85] Cinefex 85, Apr 2001, at one remove through two "
               "agreeing carriers · [FS] The Film Stage · [NO] Oseman · "
               "[MB] MagicBeans · [GE] Ercolano · [AP] Age of Plastic · "
               "[WP] Wikipedia.",
               mono(6.5f, kTick)));
    return p;
  }

  Element s2Lens() {
    using namespace slit;
    Element p = panelShell("120 : 1, AND A LENS", 1);
    p.child(pl("z0 = 15 ft = 180.0 in   z1 = 1.5 in   z0/z1 = 120.0 : 1",
              mono(7.9f, al(kCold, 0.95f))));
    p.child(pl("DOF @ 1.5 in, f/1.8, 28.64 mm, c 0.05 = 0.0791 mm   ⇒   "
               "CLAIMED BRACKET 4533.9 mm / ACTUAL DOF = 5.7 × 10⁴",
               mono(7.1f, kRed)));
    p.child(pl("READ THE 15 ft AS A HYPERFOCAL NEAR LIMIT, f²/(Nc)+f = 2×4572:",
               mono(6.9f, kType2)));
    p.child(pl("c .025→20.26   .050→28.64   .075→35.07   .100→40.48 mm",
               mono(7.8f, kAmber)));
    p.child(pl("28 mm T2.8 IS ON PANAVISION’S SUPER PANAVISION 70 LIST AND IN "
               "2001’S OWN CONTINUITY REPORTS [AP] — THE LENS EXISTS. (f/1.8 "
               "IS FASTER THAN ANY OF THEM; 1.5 in NEEDS 87 mm OF BELLOWS.)",
               mono(6.5f, al(kCold, 0.78f))));
    p.child(pl("[C85]’S 14 ft TRACK ⇒ NEAR END 12 in, NOT 1½ in — THE TWO "
               "PUBLISHED FIGURES DISAGREE BY 10.5 in, 6.2500% OF THE TRACK.",
               mono(6.5f, kType2)));
    p.child(pl("THE NUMBER IN THE SENTENCE IS NOT A DEPTH OF FIELD. "
              "IT IS A LENS.",
               uiB(10.5f, kType, 0.2f)));
    p.child(pl("AND [T68]’S OWN CAPTION SAYS SO: “SELSYN-DRIVEN FOLLOW-FOCUS "
               "MECHANISM”. WHAT IT HELD WAS FOCUS, SERVOED TO THE TRACK.",
               mono(6.5f, kAmber)));
    return p;
  }

  Element s3Law() {
    using namespace slit;
    Element p = panelShell("THE 1/ρ LAW — MEASURED, NOT ASSUMED", 2);
    p.child(box()
                .width(386)
                .height(62)
                .shrink(0)
                .fill(al(kBlack, 0.6f))
                .stroke(stroke(1.0f, Fill::color(kRule)))
                .child(box()
                           .absolute()
                           .inset(4)
                           .outline(shapes::parametric(
                               [](float s) {
                                 // log-log axes: exact C/u is a straight
                                 // line of slope -1 in this frame.
                                 return SkPoint{s, s};
                               },
                               0.0f, 1.0f, 240, false))
                           .stroke(stroke(1.6f, Fill::color(kAmber)))
                           .trim(0.0f, with(1.0f, {520ms, ch::easeOutCubic,
                                                   1500ms})))
                .child(custom([this](SkCanvas &c, const PaintContext &p2) {
                         drawMeasuredPoints(c, p2);
                       })
                           .absolute()
                           .inset(4)
                           .cache(Cache::None)));
    p.child(slot("fit").height(Dim(21)).shrink(0));
    p.child(pl("DWELL AT FILM RADIUS u IS f·w/(V·u), AND IRRADIANCE FROM AN "
               "EXTENDED SOURCE IS DISTANCE-INVARIANT AT FIXED APERTURE — SO "
               "EXPOSURE ∝ 1/u. NOTHING PAINTS IT; IT IS THE SUM OF 1624 "
               "STAMPS PER WALL WEIGHTED BY THE CAMERA TRAVEL EACH STANDS "
               "FOR, MEASURED BACK OUT OF AN F16 RASTER OF THE ACCUMULATION "
               "SUBTREE ALONE. Debug.h IS ENTIRELY PATH-LEVEL.",
               mono(6.5f, kType2)));
    p.child(pl("WHAT THE FILM SHOWS IS DENSITY. WHAT THE MACHINE MADE IS "
               "EXPOSURE. THE 1/ρ LAW IS IN THE SECOND; THE CURVE BETWEEN "
               "THEM IS RECONSTRUCTED.",
               mono(6.5f, kAmber)));
    return p;
  }

  Element s4Sampling() {
    using namespace slit;
    Element p = panelShell("SAMPLING: 406 IS NOT ARBITRARY", 3);
    const char *rowName[2] = {"uniform in  z", "uniform in ln z"};
    for (int r = 0; r < 2; ++r) {
      Element row = box().row().gap(5).alignItems(Align::Center);
      row.child(pl(rowName[r], mono(7.0f, kType2)).width(80));
      for (int k = 0; k < 3; ++k) {
        const int idx = r * 3 + k;
        row.child(box()
                      .width(98)
                      .height(18)
                      .shrink(0)
                      .fill(kBlack)
                      .clip()
                      .key(fmt("s4_%d", idx))
                      .scaleX(withFrom(0.0f, 1.0f,
                                       {220ms, ease::outBack(1.70158f)}))
                      .transformOrigin(0.0f, 0.5f)
                      .child(instancing::instances(flatAtlas, s4[(size_t)idx],
                                                   instancing::Mode::Data,
                                                   SkBlendMode::kPlus)));
      }
      p.child(row.shrink(0));
    }
    p.child(box()
                .row()
                .gap(5)
                .shrink(0)
                .child(box().width(80).shrink(0))
                .child(pl("K = 12", mono(6.6f, kTick)).width(98))
                .child(pl("K = 48", mono(6.6f, kTick)).width(98))
                .child(pl("K = 406", mono(6.6f, kTick)).width(98)));
    p.child(pl("EACH STRIP IS ONE REAL WALL, STAMPED BY THE SAME CODE AS "
               "THE FRAME: FILM RADIUS 0 → 600 px, LEFT TO RIGHT, LINEAR.",
               mono(6.5f, kTick)));
    p.child(rule(390, kRule));
    p.child(pl("Δ(ln u) ≤ ln(1 + w/X0)   K_min = 1 + ln 120 / ln(1+1/84) = "
               "405.6 → 406",
               mono(7.1f, al(kCold, 0.9f))));
    p.child(pl("THE BRIEF SAID 402 — IT LINEARISED THE LOGARITHM (4.7875 × "
               "84); AT K = 400 THE STAMPS NO LONGER QUITE TOUCH.",
               mono(6.5f, kAmber)));
    p.child(slot("ripple").height(Dim(19)).shrink(0));
    p.child(pl("X0/w IS THE ONLY NUMBER THAT SETS THIS, AND X0 = 49.2 in PUTS "
               "THE SLIT 4 ft OFF AXIS — OUTSIDE A 6 ft PLATE. THE WEAKEST "
               "JOINT IN THIS RECONSTRUCTION, PRINTED RATHER THAN HIDDEN.",
               mono(6.5f, kType2)));
    p.child(pl("EQUAL-WEIGHT LOG STAMPS ARE BAND-FREE AND FLAT — WHICH IS "
               "WRONG. THE WEIGHT MUST BE THE CAMERA TRAVEL: ω ∝ z.",
               mono(6.5f, kRed)));
    return p;
  }

  Element sidebar() {
    using namespace slit;
    return box()
        .column()
        .width(Dim(kSideW))
        .height(Dim(kBodyH))
        .shrink(0)
        .justify(Justify::SpaceBetween)
        .staggerChildren(85ms)
        .child(s1Quote())
        .child(s2Lens())
        .child(s3Law())
        .child(s4Sampling());
  }

  // ---------------------------------------------------------- live readouts
  Element readoutEl() {
    using namespace slit;
    const float z = kZ0In * std::pow(kR, -(float)tau);
    const int stampIdx = (int)(tau * (double)(kKDisplay - 1));
    const float u = kUFar * std::pow(kR, (float)tau);
    const float omega = kUFar / std::max(u, 1e-3f);
    return box()
        .column()
        .gap(2)
        .width(Dim(262))
        .child(box()
                   .row()
                   .gap(12)
                   .child(t(fmt("z = %06.2f in", z), monoB(9, kAmber)))
                   .child(t(fmt("m = ×%0.3f", kZ0In / std::max(z, 1e-3f)),
                            mono(9, kType2))))
        .child(box()
                   .row()
                   .gap(12)
                   .child(t(fmt("stamp %04d / %d", stampIdx, kKDisplay),
                            mono(9, kType2)))
                   .child(t(fmt("ω = %0.4f", omega), mono(9, al(kCold, 0.9f)))))
        .child(t(fmt("ONE ATLAS · %d×%d SHEET · ONE BAKE %.0f ms · "
                     "texWindows()",
                     sheetW, sheetH, bakeMs),
                 mono(6.8f, kTick)));
  }
  Element expoEl() {
    using namespace slit;
    return t(fmt("SWEEP %3d%%  ·  z %06.2f in  ·  %d / %d STAMPS LAID",
                 (int)(tau * 100.0), kZ0In * std::pow(kR, -(float)tau),
                 (int)(tau * (double)kK), kK),
             mono(7.2f, al(kCold, 0.8f)));
  }
  Element fitEl() {
    using namespace slit;
    if (fixedStatus.clamped)
      return t("FIT SUPPRESSED — THIS FRAME DROPPED SIMULATED TIME",
               mono(8.2f, kRed));
    return box()
        .column()
        .gap(1)
        .child(t(fmt("FIT  E(u) = C / u^p     p = %0.4f     R² = %0.5f",
                     fitP, fitR2),
                 monoB(8.2f, al(kCold, 0.95f))))
        .child(t(fmt("RESIDUAL u ∈ [8, 520] px  p95 %0.2f%%  max %0.2f%%  "
                     "(%d rays, %d pts)",
                     fitP95 * 100.0f, fitResid * 100.0f, fitRays, fitPts),
                 mono(7.2f, kType2)));
  }
  Element rippleEl() {
    using namespace slit;
    return box()
        .column()
        .gap(1)
        .child(t(fmt("AND K_min REMOVES GAPS, NOT RIPPLE: AT K = 406 THE "
                     "MEASURED MAX RESIDUAL IS %0.0f%%,",
                     fitResidMin * 100.0f),
                 mono(7.0f, al(kCold, 0.85f))))
        .child(t(fmt("AT 4× IT IS %0.0f%% — AND p MOVES ONLY %0.4f → %0.4f. "
                     "THE LAW SURVIVES ITS OWN QUANTISATION.",
                     fitResid * 100.0f, fitPMin, fitP),
                 mono(7.0f, al(kCold, 0.85f))));
  }

  // ---------------------------------------------------------------- describe
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

  void setup(sketch::SketchContext &ctx) override;
  void update(double e, sketch::SketchContext &ctx) override;
};

// ===========================================================================
// The rig elevation. ONE SCALE: 3.0 px = 1 inch, throughout.
//
// Drawn as one custom() leaf -- custom() measures ZERO on the main axis, so
// it carries explicit width/height -- with every brushed mark going through
// decorations::paintOn(), which is how the whole vocabulary reaches
// hand-built geometry.

void SlitScan2001::drawRig(SkCanvas &c, const PaintContext &ctx) {
  using namespace slit;
  const float S = kRigPxPerIn;
  const float plateX = 548.0f;         // the slit; z = 0 for focus
  const float trackY = 194.0f;
  const float plateCY = 116.0f;
  const float plateH = kPlateIn * S;   // 216 px
  const float benchT = 232.0f, benchB = 244.0f;

  SkPaint p;
  p.setAntiAlias(true);

  // The bench, as a drafting section: a solid with 45-degree hatching.
  SkPathBuilder benchB2;
  benchB2.addRect(SkRect::MakeLTRB(4, benchT, kElevW - 6, benchB));
  const SkPath benchPath = benchB2.detach();
  p.setColor4f(kSolid);
  c.drawPath(benchPath, p);
  decorations::paintOn(c, ctx, benchPath,
                       lines::hatch(Fill::color(al(kAmber, 0.20f)), 6.0f, 1.0f,
                                    45.0f));
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(al(kAmber, 0.55f));
  c.drawPath(benchPath, p);
  p.setStyle(SkPaint::kFill_Style);

  // THE TRACK: 14 ft = 168 in = 504 px [C85], from z = 180 in down to
  // z = 12 in -- which is where FINDING 2 becomes a picture. A double rule,
  // with the worm gear as a micro-pitch lattice along it [NO].
  const float trackL = plateX - kZ0In * S;             // z = 180 in, x = 8
  const float trackR = trackL + kTrackIn * S;          // z = 12 in,  x = 512
  p.setColor4f(kAmber);
  c.drawRect(SkRect::MakeLTRB(trackL, trackY - 1, trackR, trackY), p);
  c.drawRect(SkRect::MakeLTRB(trackL, trackY + 5, trackR, trackY + 6), p);
  p.setColor4f(al(kAmber, 0.5f));
  for (float x = trackL; x <= trackR; x += 4.0f)
    c.drawRect(SkRect::MakeLTRB(x, trackY + 1, x + 1.7f, trackY + 5), p);
  // …and the 36 px of reach the published track does NOT cover.
  p.setColor4f(al(kRed, 0.85f));
  for (float x = trackR; x < plateX - 2; x += 5.0f)
    c.drawRect(SkRect::MakeLTRB(x, trackY + 2, x + 2.0f, trackY + 4), p);

  // THE PLATE: 6 ft = 216 px tall, rotatable [C85], turning live.
  c.save();
  c.translate(plateX, plateCY);
  c.rotate(plateDeg * 0.4f);
  const SkRect plate = SkRect::MakeLTRB(-9, -plateH * 0.5f, 9, plateH * 0.5f);
  p.setColor4f(kSolid);
  c.drawRect(plate, p);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(kAmber);
  c.drawRect(plate, p);
  p.setStyle(SkPaint::kFill_Style);
  const float slitH = kSlitHIn * S; // 4 ft of the 6 ft plate [NO] vs [C85]
  p.setColor4f(al(kCold, 0.26f));
  c.drawRect(SkRect::MakeLTRB(-5.0f, -slitH * 0.5f, 5.0f, slitH * 0.5f), p);
  p.setColor4f(kCold);
  c.drawRect(SkRect::MakeLTRB(-0.9f, -slitH * 0.5f, 0.9f, slitH * 0.5f), p);
  c.restore();

  // THE LOG TICKS. z halves eight times across the sweep, and because
  // u = f*X0/z the SAME eight steps are equal intervals in the film frame.
  // Drawing them on the track is the clearest statement of why a corridor
  // shot at constant speed reads as acceleration.
  {
    SkPaint q;
    q.setAntiAlias(true);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.9f);
    SkFont f7(monoFace(), 6.8f);
    SkPaint qt;
    qt.setAntiAlias(true);
    for (int i = 0; i <= 7; ++i) {
      const float zz = kZ0In * std::pow(kR, -(float)i / 7.0f);
      const float x = plateX - zz * S;
      const float a2 = 0.30f + 0.09f * (float)i;
      q.setColor4f(al(kAmber, a2));
      c.drawLine(x, trackY - 8, x, trackY - 2, q);
      qt.setColor4f(al(kAmber, a2));
      const std::string lab = fmt("%.4g", zz);
      const float tw =
          f7.measureText(lab.c_str(), lab.size(), SkTextEncoding::kUTF8);
      c.drawString(lab.c_str(), x - tw * 0.5f, trackY - 11, f7, qt);
    }
  }

  // The twelve-foot panel, edge on, immediately behind the plate: [T68]'s
  // 0.09 mm of depth at 1.5 in means the artwork must be pressed against
  // the glass or the near end of every streak is a blur of it. [C85] says
  // the plate "stood in front of" the panel and says nothing about the gap.
  p.setColor4f(al(kCold, 0.5f));
  c.drawRect(SkRect::MakeLTRB(plateX + 12, plateCY - kSlitHIn * S * 0.5f,
                              plateX + 15, plateCY + kSlitHIn * S * 0.5f),
             p);

  // THE CARRIAGE at z(tau) = z0 * 120^(-tau) -- log, so it visibly
  // decelerates in FILM terms as it closes, which is the whole reason the
  // corridor reads as acceleration.
  const float z = kZ0In * std::pow(kR, -(float)tau);
  const float camX = plateX - z * S;
  p.setColor4f(kSolid);
  const SkRect body = SkRect::MakeXYWH(camX - 30, trackY - 32, 42, 26);
  c.drawRect(body, p);
  p.setStyle(SkPaint::kStroke_Style);
  p.setColor4f(kAmber);
  c.drawRect(body, p);
  p.setStyle(SkPaint::kFill_Style);
  c.drawCircle(camX - 17, trackY - 38, 7, p);                     // the mag
  c.drawRect(SkRect::MakeXYWH(camX + 12, trackY - 24, 10, 11), p); // lens
  p.setColor4f(al(kAmber, 0.85f));
  c.drawRect(SkRect::MakeXYWH(camX - 28, trackY - 6, 38, 5), p);  // carriage

  // The light path, 1 px, opacity falling as it lengthens.
  const float beamA = std::clamp(0.9f - 0.62f * (float)tau, 0.1f, 0.9f);
  p.setColor4f(al(kCold, beamA));
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  c.drawLine(camX + 22, trackY - 19, plateX, plateCY + slitH * 0.5f, p);
  c.drawLine(camX + 22, trackY - 19, plateX, plateCY - slitH * 0.5f, p);
  p.setStyle(SkPaint::kFill_Style);

  SkFont f75(monoFace(), 7.5f);
  SkPaint tp;
  tp.setAntiAlias(true);

  auto leader = [&](float ax, float bx, float y, const char *label,
                    SkColor4f col) {
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(col);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.8f);
    c.drawLine(ax, y, bx, y, q);
    c.drawLine(ax, y - 4, ax, y + 4, q);
    c.drawLine(bx, y - 4, bx, y + 4, q);
    q.setStyle(SkPaint::kFill_Style);
    SkPathBuilder h;
    h.moveTo(ax, y).lineTo(ax + 6, y - 2.4f).lineTo(ax + 6, y + 2.4f).close();
    h.moveTo(bx, y).lineTo(bx - 6, y - 2.4f).lineTo(bx - 6, y + 2.4f).close();
    c.drawPath(h.detach(), q);
    tp.setColor4f(col);
    const float tw =
        f75.measureText(label, std::strlen(label), SkTextEncoding::kUTF8);
    c.drawString(label, (ax + bx) * 0.5f - tw * 0.5f, y - 3.5f, f75, tp);
  };
  leader(trackL, plateX, trackY + 20.0f,
         "15 ft = 180 in = 540 px  FOCUS REACH [T68]", kAmber);
  leader(trackL, trackR, trackY + 34.0f,
         "14 ft = 168 in = 504 px  TRACK [C85]", kAmber);

  // The 1.5 in near focus: 4.5 px, dimensioned anyway. The whole erratum of
  // FINDING 2 is visible as a dimension you can barely see -- and as the
  // 36 px of red dashes the published track never reaches.
  {
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(kRed);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.9f);
    const float ax = plateX - kZ1In * S, bx = plateX;
    c.drawLine(ax, trackY - 50, bx, trackY - 50, q);
    c.drawCircle((ax + bx) * 0.5f, trackY - 50, 11, q);
    c.drawLine((ax + bx) * 0.5f - 11, trackY - 50, 466, trackY - 22, q);
    // HALOED. The carriage travels the whole rail over tau, so this caption
    // is printed through by the camera body at some phase of every sweep —
    // and there is no clear band to move it to: above is the clock block,
    // below is the leader pair, the worm-gear rail and the scale note. A
    // 2 px knockout in the panel colour is what a drafting plate does when
    // a note has to cross the drawing.
    SkPaint halo;
    halo.setAntiAlias(true);
    halo.setColor4f(kPanelBg);
    halo.setStyle(SkPaint::kStroke_Style);
    halo.setStrokeWidth(2.2f);
    halo.setStrokeJoin(SkPaint::kRound_Join);
    const char *r1 = "1½ in = 4.5 px [T68] — AND [C85]’s TRACK";
    const char *r2 = "STOPS 12 in SHORT: THE RED DASHES.";
    c.drawString(r1, 304, trackY - 26, f75, halo);
    c.drawString(r2, 304, trackY - 16, f75, halo);
    tp.setColor4f(kRed);
    c.drawString(r1, 304, trackY - 26, f75, tp);
    c.drawString(r2, 304, trackY - 16, f75, tp);
  }

  // The machine's own clock, which is the reason this study is called
  // "time as an axis of the image". Arithmetic over published numbers.
  {
    SkFont f8(monoFace(), 8.0f);
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(al(kCold, 0.92f));
    c.drawString("2 EXPOSURES / FRAME × 45–60 s  [T68][C85]", 304, 44, f8, q);
    q.setColor4f(kType2);
    c.drawString("= 90–120 s PER FILM FRAME  ·  ×24 fps", 304, 56, f8, q);
    c.drawString("MACHINE : SCREEN  =  2160 – 2880 : 1", 304, 68, f8, q);
    c.drawString("[FS]’s 4 min/frame GIVES 5760 : 1", 304, 80, f8, q);
    c.drawString("ONE 36-HOUR TAKE [C85] = 45–60 s OF SCREEN", 304, 92, f8, q);
    q.setColor4f(kAmber);
    c.drawString("ONE SECOND OF THE STAR GATE COST", 304, 110, f8, q);
    c.drawString("36 TO 96 MINUTES OF MACHINE TIME.", 304, 122, f8, q);
    q.setColor4f(kTick);
    c.drawString("SIX MONTHS OF SLIT-SCAN WORK [C85], OF WHICH", 304, 140, f8, q);
    c.drawString("ABOUT A QUARTER REACHED THE FILM [FS].", 304, 152, f8, q);
  }

  // Two lines, not one: the caption column runs out of room at the plate,
  // and set as a single run the last nineteen characters were drawn UNDER
  // the glass panel and never read.
  tp.setColor4f(al(kAmber, 0.9f));
  c.drawString("6 ft PLATE [C85], 4 ft OPENING [NO] —", 292, 10, f75, tp);
  c.drawString("PROBABLY NOT A CONTRADICTION, AND NOTHING SAYS SO", 292, 21,
               f75, tp);
  tp.setColor4f(al(kCold, 0.85f));
  c.drawString("SLIT · X0 = 49.2 in OFF AXIS, w = 0.586 in, 82 : 1", 292, 32,
               f75, tp);
  tp.setColor4f(kTick);
  c.drawString("ONE SCALE: 3.0 px = 1 INCH.  180 in = 540 px · 168 in = 504 px "
               "· 72 in = 216 px · 144 in = 432 px  ·  THE z TICKS ARE EIGHT",
               6, 258, f75, tp);
  c.drawString("EQUAL STEPS IN ln z, WHICH ARE EIGHT EQUAL STEPS IN THE FILM "
               "FRAME  ·  WORM-GEAR DOLLY [NO] · SELSYN FOLLOW-FOCUS [T68]",
               6, 270, f75, tp);
}

// The artwork panel, face on, at the same 3.0 px = 1 inch.
void SlitScan2001::drawArtworkPanel(SkCanvas &c, const PaintContext &ctx) {
  using namespace slit;
  const Shot &s = shotAt(shot);
  const Strip &S = strips[(size_t)s.cell];
  if (!S.image)
    return;
  const float pw = kPanelIn * kRigPxPerIn;  // 432 px, exact
  const float ph = kSlitHIn * kRigPxPerIn;  // 144 px -- 3:1, undistorted
  const float top = 40.0f;

  SkPaint p;
  p.setAntiAlias(false);
  c.save();
  c.clipRect(SkRect::MakeXYWH(0, top, pw, ph));
  // The panel crawls left by one slit-width per film frame -- pinned by
  // [GE]'s unwrap, not chosen.
  const float ox = -artOffset * pw;
  for (int k = 0; k < 2; ++k) {
    c.save();
    c.translate(ox + (float)k * pw, top);
    c.scale(pw / (float)S.w, ph / (float)S.h);
    c.drawImage(S.image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), &p);
    c.restore();
  }
  c.restore();

  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(al(kAmber, 0.7f));
  c.drawRect(SkRect::MakeXYWH(0, top, pw, ph), p);

  // The slit's window over the panel, with a cased leader back to the rig.
  const float sw = std::max(2.5f, kSlitWIn * kRigPxPerIn);
  const float sx = pw * 0.5f;
  p.setColor4f(kCold);
  p.setStrokeWidth(1.2f);
  c.drawRect(SkRect::MakeXYWH(sx - sw * 0.5f, top - 5, sw, ph + 10), p);
  SkPathBuilder lb;
  lb.moveTo(sx, top + ph + 8);
  lb.lineTo(sx, top + ph + 16);
  lb.lineTo(-38, top + ph + 16);
  decorations::paintOn(c, ctx, lb.detach(),
                       lines::cased(1.2f, Fill::color(al(kCold, 0.5f)), 3.0f));

  SkFont f76(monoFace(), 7.6f);
  SkPaint tp;
  tp.setAntiAlias(true);
  tp.setColor4f(al(kCold, 0.9f));
  c.drawString(s.art, 0, top - 24, f76, tp);
  tp.setColor4f(kType2);
  c.drawString("THE 12-FOOT PANEL, FACE ON, AT 3.0 px = 1 INCH", 0, top - 12,
               f76, tp);
  tp.setColor4f(kTick);
  c.drawString("12′ MECHANISED BACKLIT GLASS PANEL · TRANSPARENCIES + "
               "CELLULOID GELS · HIGH-CONTRAST",
               0, top + ph + 26, f76, tp);
  c.drawString("NEGATIVES OF OP-ART, ARCHITECTURAL DRAWINGS, CIRCUIT "
               "PRINTS [C85]",
               0, top + ph + 37, f76, tp);
  tp.setColor4f(al(kAmber, 0.95f));
  c.drawString("ADVANCE 0.586 in/FRAME = EXACTLY ONE SLIT WIDTH — PINNED BY "
               "[GE]’S UNWRAP,",
               0, top + ph + 55, f76, tp);
  c.drawString("NOT CHOSEN: MORE WOULD GAP, LESS WOULD REPEAT.", 0,
               top + ph + 66, f76, tp);
  tp.setColor4f(al(kCold, 0.8f));
  c.drawString(fmt("ROUND TRIP OVER 96 FRAMES: %d COLUMNS BACK, r = %0.5f, "
                   "%0.3f%% OF PIXELS WRONG",
                   rtCols, rtCorr, rtMismatch * 100.0f)
                   .c_str(),
               0, top + ph + 84, f76, tp);
  if (s.cell == 2) {
    tp.setColor4f(kRed);
    c.drawString("THIS STRIP CARRIES [GE]’S WHITE-STRIPE DEFECT — FOUND IN "
                 "BOTH SH 27 AND SH 29",
                 0, top + ph + 14, f76, tp);
  }
}

// The measured 1/rho profile against the analytic C/u, on log-log axes.
// The frame is normalised so an exact 1/u law is the box diagonal, which
// makes any departure from p = 1 a visible bow rather than a number.
void SlitScan2001::drawMeasuredPoints(SkCanvas &c, const PaintContext &ctx) {
  using namespace slit;
  if (profN <= 0)
    return;
  const float W = ctx.size.width(), H = ctx.size.height();
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(al(kCold, 0.92f));
  // THE MEASURED SAMPLES. Every ray levelled by its own fitted intercept,
  // then averaged: on these axes an exact 1/u law is the diagonal, so the
  // visible wiggle IS the +-1-stamp quantisation the sampling panel names.
  for (int i = 0; i < profN; i += 2) {
    const float x = profX[(size_t)i] * W;
    const float y = std::clamp(profY[(size_t)i], 0.0f, 1.0f) * H;
    c.drawCircle(x, y, 1.5f, p);
  }
  SkFont f(monoFace(), 6.4f);
  SkPaint tp;
  tp.setAntiAlias(true);
  tp.setColor4f(al(kTick, 0.95f));
  c.drawString("log u  8 → 520 px", 3, H - 3, f, tp);
  c.drawString("log 1/E", 3, 9, f, tp);
  tp.setColor4f(al(kAmber, 0.95f));
  c.drawString("ANALYTIC C/u", W - 66, 9, f, tp);
  tp.setColor4f(al(kCold, 0.95f));
  c.drawString("MEASURED", W - 50, H - 3, f, tp);
}

// ===========================================================================

void SlitScan2001::setup(sketch::SketchContext &ctx) {
  using namespace slit;
  ctx.canvas(kCanvasW, kCanvasH);
  ctx.background(kInk);

  // ---- bake the artwork ONCE. NOT a Material::buffer case (ROADMAP 4):
  // this is a static image made at setup and never mutated, and three
  // studies have now over-cited that entry. The distinction is the useful
  // part, so it is said plainly rather than cited.
  const double b0 = (double)std::clock() / CLOCKS_PER_SEC;
  gridPat = patterns::gridLines(37.0f, 23.0f, 2.0f, kWhite);
  // patterns::speckle's tile IS the repeat (ledger 63) -- so a large one.
  spekPat = patterns::speckle(492.0f, 84, 5.0f, 21.0f, {kWhite});
  if (ctx.fonts) {
    strips[0] =
        bakeStrip(artOpArt(), *ctx.fonts, (int)kCellW, (int)kCellH, 0.30f, -1);
    strips[1] =
        bakeStrip(artArch(), *ctx.fonts, (int)kCellW, (int)kCellH, 0.30f, -1);
    strips[2] = bakeStrip(artCircuit(gridPat, spekPat), *ctx.fonts,
                          (int)kCellW, (int)kCellH, 0.42f, 903);
  }
  bakeMs = ((double)std::clock() / CLOCKS_PER_SEC - b0) * 1000.0;

  // ---- ONE atlas, THREE cells, ONE bake. The brief planned 64
  // pre-registered cells and a 64-step quantised crawl, because a Pool
  // could name a cell INDEX and never a RECT and Atlas::cell() drops the
  // whole sheet when you re-register. Pool::texWindows() landed while the
  // brief was being written, so the crawl is CONTINUOUS and the sheet is a
  // twentieth of the planned size.
  atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->filter(SkFilterMode::kNearest); // 1-bit artwork
  for (int i = 0; i < 3; ++i) {
    const Strip &S = strips[(size_t)i];
    atlas->cell(box().fill(Material::image(
                    S.image, SkTileMode::kClamp, SkTileMode::kClamp,
                    SkMatrix::Scale(kCellW / (float)std::max(S.w, 1),
                                    kCellH / (float)std::max(S.h, 1)),
                    SkSamplingOptions())),
                {kCellW, kCellH});
  }
  if (ctx.fonts && atlas->ensureBaked(*ctx.fonts) && atlas->image()) {
    sheetW = atlas->image()->width();
    sheetH = atlas->image()->height();
  }
  // A uniform white slit, for the measurement and for the sampling strips:
  // there the subject is the SPACING, not the artwork.
  flatAtlas = std::make_shared<instancing::Atlas>(1.0f);
  flatAtlas->filter(SkFilterMode::kNearest);
  flatAtlas->cell(box().fill(Fill::color(kWhite)), {kCellW, kCellH});

  wallA = std::make_shared<instancing::Pool>();
  wallB = std::make_shared<instancing::Pool>();
  monA = std::make_shared<instancing::Pool>();
  monB = std::make_shared<instancing::Pool>();
  rebuildWalls();

  // ---- the sampling strips: SIX REAL WALLS, rendered small. Two spacing
  // rules against three K. Each is an instances() leaf over its own pool,
  // so the argument is drawn by the same code that draws the picture.
  const int Ks[3] = {12, 48, kK};
  for (int r = 0; r < 2; ++r)
    for (int k = 0; k < 3; ++k) {
      auto pool = std::make_shared<instancing::Pool>();
      WallSpec w;
      w.vp = {0.0f, 10.0f};
      w.gel = kCold;
      w.uFar = 98.0f / kR; // uNear lands exactly at the strip's right edge
      w.gain = 46.0f;
      w.artwork = false;
      w.K = Ks[k];
      w.logSpaced = (r == 1);
      buildWall(*pool, w);
      s4[(size_t)(r * 3 + k)] = pool;
    }

  // ---- the two verifications
  if (ctx.fonts) {
    measureExposure(*ctx.fonts);
    roundTrip(*ctx.fonts);
  }
  std::fprintf(
      stderr,
      "[slitscan] sheet %d x %d px, bake %.1f ms\n"
      "[slitscan] 12-D  K=%d: p = %.4f  R2 = %.5f  p95 %.2f%%  max %.2f%%"
      "   |  K=%d: p = %.4f  max %.2f%%   (%d rays, %d pts, %.0f ms)\n"
      "[slitscan] 12-E  round trip: %d columns, corr %.6f, %.4f%% of pixels "
      "wrong, max abs err %.3f (%.0f ms)\n",
      sheetW, sheetH, bakeMs, slit::kKDisplay, fitP, fitR2, fitP95 * 100.0f,
      fitResid * 100.0f, slit::kK, fitPMin, fitResidMin * 100.0f, fitRays,
      fitPts, measMs, rtCols, rtCorr, rtMismatch * 100.0f, rtMaxErr, rtMs);

  // ---- THE FILM CLOCK. 24 Hz because the film runs at 24 fps: the first
  // fixed rate in this programme that is the artefact's own published
  // number rather than a reconstruction. The interpolant drives the shutter
  // bar and the sub-frame readout and NOTHING in the picture -- a projector
  // holds a frame for its whole 1/24 s and then replaces it; it does not
  // cross-fade. A correct NON-use of alphaOut is worth reporting.
  ctx.ticker.addFixed(
      24.0,
      [this] {
        ++filmNo;
        artOffset = std::fmod(artOffset + kAdvanceIn / kPanelIn, 1.0f);
        plateDeg += shotAt(shotFor(filmNo)).dPhi;
        return true;
      },
      8, &frameAlpha, &fixedStatus);

  // ---- the demonstration clock: tau sweeps once per 3.0 s.
  ctx.ticker.add([this](double dt) {
    elapsed += dt;
    // Phase-offset so the recommended capture lands with the carriage
    // two thirds down its fourteen feet, mid-exposure.
    tau = std::fmod(elapsed / 3.0 + 0.60, 1.0);
    rebuildWalls();
    return true;
  });

  if (ctx.fonts) {
    const SkSize a1 = measure(s1Quote(), *ctx.fonts, {kSideW, 4000});
    const SkSize a2 = measure(s2Lens(), *ctx.fonts, {kSideW, 4000});
    const SkSize a3 = measure(s3Law(), *ctx.fonts, {kSideW, 4000});
    const SkSize a4 = measure(s4Sampling(), *ctx.fonts, {kSideW, 4000});
    // A layout self-check, because a flex column silently overlaps its
    // children rather than complaining when they do not fit.
    const float total = a1.height() + a2.height() + a3.height() + a4.height();
    std::fprintf(stderr,
                 "[slitscan] sidebar %.0f + %.0f + %.0f + %.0f = %.0f / %.0f%s\n",
                 a1.height(), a2.height(), a3.height(), a4.height(), total,
                 kBodyH, total > kBodyH ? "   *** OVER BUDGET ***" : "");
  }
  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("readout", readoutEl());
  ctx.composer.renderSlot("expo", expoEl());
  ctx.composer.renderSlot("fit", fitEl());
  ctx.composer.renderSlot("ripple", rippleEl());
}

void SlitScan2001::update(double e, sketch::SketchContext &ctx) {
  if (fixedStatus.clamped)
    everClamped = true;
  // Shot cuts are HARD, and they land on a FILM FRAME (every 96th), not on
  // a wall-clock boundary. The film cuts.
  (void)e;
  const int s = shotFor(filmNo);
  if (s != shot) {
    shot = s;
    rebuildWalls();
    ctx.composer.render(describe(ctx));
  }
  // Eight live numbers through slot()/renderSlot() -- never a per-frame
  // re-render of the tree (ROADMAP 9, and the fourth study to land here).
  ctx.composer.renderSlot("readout", readoutEl());
  ctx.composer.renderSlot("expo", expoEl());
  ctx.composer.renderSlot("fit", fitEl());
}

SIGIL_SKETCH(SlitScan2001)
