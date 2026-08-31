// twoadvanced_v3.cpp — 2Advanced Studios, "V3 Expansions Reboot" (2024):
// the studio's own rebuild of its 2001 v3 "Expansions" Flash site, live at
// https://v3.2advanced.com/v3expansionsreboot/ — a React shell around one
// Rive artboard ("Main Stage") that carries the entire interface. Creative
// direction: Eric Jordan.
//
// REFERENCE (everything measured, not remembered):
//   · https://v3.2advanced.com/v3expansionsreboot/mainstage.riv — the
//     production Rive file (5.8 MB). Its embedded PNG assets are the
//     interface's actual art — home-background, ui-top-header,
//     ui-navbar-background, ui-lower-panel-bg, a 62-frame cloud sequence,
//     rive-R-logo, ddd-logo, the discord icon — and its strings name every
//     layer this sketch rebuilds: "upper bevel bar", "top bar navigation",
//     "mid ui top modules", "lwr txt left module.stdby", "SCROLL.EXTENDED.
//     CONTENT", "AMBIENCE.MUTE". This sketch downloads that exact file
//     over SigilLoader's https path and lifts the art straight out of it
//     (PNG signature scan; each embedded image is preceded by its asset
//     name). The type inside is Berthold Akzidenz-Grotesk — substituted
//     here with the nearest faces the platform ships.
//   · https://v3.2advanced.com/V3ExpansionsReboot/assets/background.gif
//     (10×1600 page tile), .../assets/images/social-icons@2x.png (436×32
//     sprite), and /v3expansionsreboot/assets/2advancedLogo_Preload.svg
//     (the circular 2A mark) — the out-of-band assets, fetched directly.
//   · https://www.ericjordan.com/wp-content/uploads/2025/07/2AV3Rive_1.png
//     — the studio's 1920×1080 capture of the composed home view. Every
//     band position and colour here is sampled from it: stage x∈[300,1620]
//     (1320 wide), top bevel 8 px, header strip to y=80, logo panel to
//     y=168, navbar at y=174, stage art y∈[217,615], the three lower
//     modules on the poly-textured ground, and the footer rail at y=1045.
//   · The shipped CSS and JS are the spec where they speak. The main
//     canvas renders Fit.contain / Alignment.center (the stage keeps its
//     aspect, gutters show the page tile). assets/preloader.css defines
//     the boot screen exactly: page #2A3753, the 2a-logo@2x.png lockup at
//     197×94 CSS px inverted to white, a 0.4em-tracked #7183A5 line, and
//     a 200 px #7183A5 percentage; the entry markup pairs it with
//     "SOLACE IN TECHNOLOGY. BELIEF IN THE FUTURE.". Video never touches
//     the canvas anywhere on the site — the "Video Player" artboard only
//     frames a plain HTML iframe overlay (.modal-video, 16:9 letterbox).
//
// THE PALETTE IS ONE HUE. Steel-blue throughout — page #182337, art
// #1C283C, chrome #7886A6 up to #F1F4F8 — with exactly one saturated
// pixel-group on the page: the orange hosting-partner mark in the footer.
// Do not introduce a second accent.
//
// Techniques kept from the original: the interface is IMAGES ON A GRID —
// big pre-rendered art panels placed by a thin layout, with live type on
// top; the clouds are a looping still sequence composited into the art's
// sky opening through a soft mask (the riv masks it with vector shapes,
// this sketch with an alpha ramp); everything enters once, in order, then
// idles almost still — until the SECTION CYCLE begins. After the home
// hold a simulated visitor walks every nav tab in order and returns to
// main. Each change plays the riv's transition grammar: the incoming
// section's art claims the viewport behind an edge that jumps in
// fourteen discrete steps ("shape trans step 1".."14"), the section
// title tracks in from stretched-wide ("title spreader"), and the nav
// indicator moves. Network assets may be absent (no network, cold
// cache) — every use site falls back to flat steel construction so the
// sketch still renders offline.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkData.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/Patterns.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace tv3 {

// ---------------------------------------------------------------------------
// Palette — sampled from the studio's own 1920×1080 capture, never eyed.

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) noexcept {
  return {(float)((rgb >> 16u) & 0xffu) / 255.0f,
          (float)((rgb >> 8u) & 0xffu) / 255.0f, (float)(rgb & 0xffu) / 255.0f,
          a};
}

constexpr SkColor4f kPage = C(0x182337);    // outer page ground
constexpr SkColor4f kPageHi = C(0x314361);  // page ground, top of ramp
constexpr SkColor4f kDeep = C(0x1C283C);    // stage art darks
constexpr SkColor4f kSeam = C(0x2E3C57);    // seams, scroll strip
constexpr SkColor4f kNavbar = C(0x4A5972);  // navbar body
constexpr SkColor4f kSteel = C(0x7886A6);   // the mid chrome steel
constexpr SkColor4f kSteelDim = C(0x68758A);
constexpr SkColor4f kSteelHi = C(0xC3CCD8);  // lifted chrome
constexpr SkColor4f kNear = C(0xF1F4F8);     // titles, wordmark
constexpr SkColor4f kBody = C(0xA8B2C0);     // module body copy
constexpr SkColor4f kInk = C(0x202B3F);      // dark type on steel bars
constexpr SkColor4f kHost = C(0xE8920A);     // the ONE saturated mark

inline SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// Type — Akzidenz-Grotesk substituted with what the platform ships.

inline sk_sp<SkTypeface> face(const char* family, int weight, int width,
                              const char* fallbackFamily = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f && fallbackFamily)
    f = mgr->matchFamilyStyle(
        fallbackFamily,
        SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f) f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
  return f;
}

inline const sk_sp<SkTypeface>& grot() {  // the workhorse medium
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kMedium_Weight,
           SkFontStyle::kNormal_Width, "Arial");
  return f;
}
inline const sk_sp<SkTypeface>& grotBold() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::kBold_Weight,
                                    SkFontStyle::kNormal_Width, "Arial");
  return f;
}

inline sigil::weave::TextStyle type(const sk_sp<SkTypeface>& tf, float size,
                                    SkColor4f color, float trackUnits = 0,
                                    float stretch = 1.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = size * trackUnits / 1000.0f;
  s.shaping.scaleX = stretch;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline sigil::weave::TextStyle micro(float size, SkColor4f c, float tr = 160) {
  return type(grotBold(), size, c, tr, 0.96f);
}
inline sigil::weave::TextStyle title(float size, SkColor4f c, float tr = 80) {
  return type(grotBold(), size, c, tr, 1.0f);
}
inline sigil::weave::TextStyle prose(float size, SkColor4f c) {
  return type(grot(), size, c, 30);
}

inline Element t(const char* s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

inline Element place(Element e, float x, float y, float w, float h) {
  e.left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
  return e;
}

// ---------------------------------------------------------------------------
// Frame constants — the capture's own geometry.

constexpr float kW = 1920, kH = 1080;
constexpr float kStageX = 300, kStageW = 1320;
constexpr float kArtY = 217, kArtH = 398;  // the home art band
constexpr float kModY = 645;               // three-module row
constexpr float kModH = 252;               // bodies end on the divider band
constexpr float kRowY = 959, kRowH = 73;   // mailing/support/follow row
constexpr float kPanelW = (kStageW - 2 * 10) / 3;  // 433⅓ — three across

/** The 45°-cut corner rect: `mask` selects which corners are cut. */
enum Cut : uint8_t { kTL = 1, kTR = 2, kBR = 4, kBL = 8 };
inline std::function<SkPath(SkSize)> chamfer(float cut, uint8_t mask) {
  return [cut, mask](SkSize s) {
    const float w = s.width(), h = s.height();
    const float c = std::min({cut, w * 0.5f, h * 0.5f});
    SkPathBuilder b;
    if (mask & kTL)
      b.moveTo(c, 0);
    else
      b.moveTo(0, 0);
    if (mask & kTR) {
      b.lineTo(w - c, 0);
      b.lineTo(w, c);
    } else {
      b.lineTo(w, 0);
    }
    if (mask & kBR) {
      b.lineTo(w, h - c);
      b.lineTo(w - c, h);
    } else {
      b.lineTo(w, h);
    }
    if (mask & kBL) {
      b.lineTo(c, h);
      b.lineTo(0, h - c);
    } else {
      b.lineTo(0, h);
    }
    if (mask & kTL) b.lineTo(0, c);
    b.close();
    return b.detach();
  };
}

}  // namespace tv3

// ===========================================================================

struct TwoAdvancedV3 : sketch::Sketch {
  using ImagePtr = std::shared_ptr<const sigil::image::ImageAsset>;

  // --- the production art, out of mainstage.riv --------------------------
  ImagePtr homeBg;                   // "home-background"      1277×385
  ImagePtr topHeader;                // "ui-top-header"        2761×155
  ImagePtr navbarBg;                 // "ui-navbar-background" 2676×66
  ImagePtr lowerPanelBg;             // "ui-lower-panel-bg"    1412×340
  ImagePtr riveLogo;                 // "rive-R-logo"          260×260
  ImagePtr dddLogo;                  // "ddd-logo"             389×508
  std::vector<ImagePtr> clouds;      // "Cloud Seq00".."Cloud Seq61", 317×246
  std::vector<ImagePtr> discordSeq;  // "discord icon 1".."102" — the orb
                                     // lighting up, 100×100 per frame

  // The six nav sections and their stage art, in tab order. SUBCULTURE
  // wears "portfolio-background": the section was renamed from the 2001
  // site's PORTFOLIO but its asset kept the old name (and EQUIPMENT's
  // asset ships with the file's own spelling). A null sub-nav row means
  // the section pages by arrows instead of tabs.
  struct SectionSpec {
    const char* tab;
    const char* asset;
    const char* subnav[3];
  };
  static constexpr SectionSpec kSections[6] = {
      {"PROFILE", "profile-background", {"ABOUT", "MISSION", "PARTNERS"}},
      {"EQUIPMENT",
       "equipment-backgound",
       {"CLOTHING", "ACCESSORIES", "SUPPORT US"}},
      {"SUBCULTURE",
       "portfolio-background",
       {"SYMBIOTIC", "PRIME", "AUXILARY"}},
      {"ACCOLADES", "accolades-background", {nullptr, nullptr, nullptr}},
      {"EXPLORATORY", "exploratory-background", {nullptr, nullptr, nullptr}},
      {"CONTACT", "contact-background", {"GENERAL", "PARTNERS", "CAREERS"}},
  };
  std::array<ImagePtr, 6> sectionBg{};

  // --- the out-of-band assets, fetched directly --------------------------
  ImagePtr pageTile;      // background.gif, 10×1600
  ImagePtr socialSprite;  // social-icons@2x.png, 436×32
  ImagePtr logoMark;      // 2advancedLogo_Preload.svg
  ImagePtr pageLogo;      // 2a-logo@2x.png, 394×188 — the preloader lockup

  Pattern diag;            // faint 45° sheen for the fallback steel panels
  sk_sp<SkImage> gapMask;  // feathered coverage of the plate's sky opening
  Pattern dots;    // the dot-matrix filling every title bar's right half
  Pattern vticks;  // the tick-dash rail under the navbar

  int cloudFrame = -1;
  int discordFrame = -1;
  int bootPct = 0;
  bool booted = false;

  /** A bitmap stretched to exactly (w, h). */
  static Material stretchFill(const ImagePtr& asset, float w, float h,
                              SkTileMode tx = SkTileMode::kClamp,
                              SkTileMode ty = SkTileMode::kClamp) {
    const sk_sp<SkImage>& img = asset->frames()[0].image;
    return Material::image(
        img, tx, ty,
        SkMatrix::Scale(w / (float)img->width(), h / (float)img->height()),
        SkSamplingOptions(SkFilterMode::kLinear));
  }

  // =========================================================================
  // The sky opening's coverage, baked once: the outline TRACED from the
  // plate (vertices in the bitmap's own 1277×385 pixels — the beam edge,
  // the right arch's inner edge down, the left arch's inner edge back
  // up), filled white through a blur mask filter. Feathering the
  // COVERAGE is what melts the overlay's boundary into the art while the
  // footage inside stays crisp — blurring the layer itself would smear
  // the clouds.

  void buildGapMask() {
    static const SkPoint kGap[] = {
        {448, 62},  {700, 92},  {688, 125}, {658, 172}, {628, 215}, {588, 252},
        {548, 258}, {512, 238}, {490, 196}, {468, 145}, {450, 95},
    };
    SkBitmap bm;
    if (!bm.tryAllocPixels(SkImageInfo::MakeN32Premul(1277, 385))) return;
    bm.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bm);
    SkPathBuilder b;
    b.moveTo(kGap[0].fX, kGap[0].fY);
    for (size_t i = 1; i < std::size(kGap); ++i)
      b.lineTo(kGap[i].fX, kGap[i].fY);
    b.close();
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 4.5f));
    canvas.drawPath(b.detach(), paint);
    bm.setImmutable();
    gapMask = bm.asImage();
  }

  // =========================================================================
  // Lifting the art out of the Rive file. A .riv stores each embedded
  // image asset as its name followed by the raw PNG, so the extraction is
  // a signature scan: find \x89PNG..IEND, then take the LAST printable
  // ASCII run (≥4 chars) between the previous image's end and this
  // signature as the asset's name.

  static int sectionFor(const std::string& name) {
    for (int s = 0; s < 6; ++s)
      if (name == kSections[s].asset) return s;
    return -1;
  }

  void extractRivImages(const std::vector<std::byte>& bytes) {
    static const unsigned char sig[8] = {0x89, 'P',  'N',  'G',
                                         0x0D, 0x0A, 0x1A, 0x0A};
    const auto* d = reinterpret_cast<const unsigned char*>(bytes.data());
    const size_t n = bytes.size();
    clouds.assign(62, nullptr);
    discordSeq.assign(102, nullptr);

    auto nameBefore = [&](size_t from, size_t upTo) -> std::string {
      std::string last, cur;
      for (size_t i = from; i < upTo; ++i) {
        const unsigned char c = d[i];
        if (c >= 0x20 && c < 0x7F) {
          cur.push_back((char)c);
        } else {
          if (cur.size() >= 4) last = cur;
          cur.clear();
        }
      }
      if (cur.size() >= 4) last = cur;
      return last;
    };

    size_t prevEnd = 0;
    for (size_t i = 0; i + 8 <= n;) {
      if (std::memcmp(d + i, sig, 8) != 0) {
        ++i;
        continue;
      }
      // Walk chunks to IEND to bound this PNG.
      size_t k = i + 8;
      bool ok = false;
      while (k + 8 <= n) {
        const uint32_t len = (uint32_t)d[k] << 24u | (uint32_t)d[k + 1] << 16u |
                             (uint32_t)d[k + 2] << 8u | (uint32_t)d[k + 3];
        const bool iend = std::memcmp(d + k + 4, "IEND", 4) == 0;
        if (k + 8 + (size_t)len + 4 > n) break;
        k += 8 + len + 4;
        if (iend) {
          ok = true;
          break;
        }
      }
      if (!ok) break;

      const std::string name = nameBefore(prevEnd, i);
      auto want = [&](const char* w) { return name == w; };
      ImagePtr* dest = nullptr;
      if (want("home-background"))
        dest = &homeBg;
      else if (want("ui-top-header"))
        dest = &topHeader;
      else if (want("ui-navbar-background"))
        dest = &navbarBg;
      else if (want("ui-lower-panel-bg"))
        dest = &lowerPanelBg;
      else if (want("rive-R-logo"))
        dest = &riveLogo;
      else if (want("ddd-logo"))
        dest = &dddLogo;
      else if (sectionFor(name) >= 0)
        dest = &sectionBg[(size_t)sectionFor(name)];
      else if (name.rfind("discord icon ", 0) == 0) {
        int idx = 0;
        for (size_t p = 13; p < name.size() && idx >= 0; ++p)
          idx = (name[p] >= '0' && name[p] <= '9') ? idx * 10 + (name[p] - '0')
                                                   : -1;
        if (idx >= 1 && idx <= 102) dest = &discordSeq[(size_t)(idx - 1)];
      } else if (name.rfind("Cloud Seq", 0) == 0 && name.size() == 11) {
        const int idx = (name[9] - '0') * 10 + (name[10] - '0');
        if (idx >= 0 && idx < 62) dest = &clouds[(size_t)idx];
      }
      if (dest && !*dest) {
        if (auto img = sigil::image::ImageAsset::decode(
                SkData::MakeWithCopy(d + i, k - i)))
          *dest = std::make_shared<sigil::image::ImageAsset>(std::move(*img));
      }
      prevEnd = k;
      i = k;
    }
    // Sequences are used dense-or-not-at-all: one absent frame would
    // strobe, so a partial extraction drops the whole loop and the use
    // site falls back to its static form.
    for (const ImagePtr& f : clouds)
      if (!f) {
        clouds.clear();
        break;
      }
    for (const ImagePtr& f : discordSeq)
      if (!f) {
        discordSeq.clear();
        break;
      }
  }

  // =========================================================================
  // Regions, top to bottom.

  /** The steel module title bar every lower panel wears: glyph chip,
   *  tracked caps title, the dot-matrix field filling the right half,
   *  and the divider dots at the far end. */
  Element moduleBar(const char* glyph, const char* label, float w) {
    using namespace tv3;
    return box()
        .width(Dim(w))
        .height(26)
        .row()
        .alignItems(Align::Center)
        .padding(7, 0)
        .gap(8)
        .fill(Material::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, C(0x8B98B2)}, {0.55f, C(0x64738F)}, {1.0f, C(0x4C5A73)}}))
        .foreground(shapes::onEdges(
            shapes::Edge::Bottom,
            stroke(1, Fill::color(fade(kInk, 0.6f)), PathFormat::Align::Inner)))
        .foreground(shapes::onEdges(shapes::Edge::Top,
                                    stroke(1, Fill::color(fade(kSteelHi, 0.7f)),
                                           PathFormat::Align::Inner)))
        .child(box()
                   .width(16)
                   .height(16)
                   .fill(kInk)
                   .justify(Justify::Center)
                   .alignItems(Align::Center)
                   .child(t(glyph, micro(9, kSteelHi, 0))))
        .child(t(label, micro(13, kInk, 140)))
        .child(box().width(6))
        .child(box().grow(1).height(16).fill(dots.material()).opacity(0.85f))
        .child(box().width(4).height(4).fill(fade(kInk, 0.8f)))
        .child(box().width(4).height(4).fill(fade(kInk, 0.5f)))
        .child(box().width(4).height(4).fill(fade(kInk, 0.3f)));
  }

  /** The recessed steel button ("VISIT RIVE", "SUBMIT", …). */
  Element button(const char* label, float w, float h = 24) {
    using namespace tv3;
    return box()
        .width(Dim(w))
        .height(Dim(h))
        .fill(Material::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, kSteelHi}, {0.5f, kSteel}, {1.0f, kSteelDim}}))
        .stroke(
            stroke(1, Fill::color(fade(kInk, 0.7f)), PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t(label, micro(11, kInk, 140)));
  }

  /** The little segmented load meter that trails the CTAs. */
  Element meter(int lit) {
    using namespace tv3;
    Element m = box().row().gap(2).alignItems(Align::Center);
    for (int i = 0; i < 5; ++i)
      m.child(box().width(9).height(7).fill(i < lit ? fade(kSteelHi, 0.9f)
                                                    : fade(kSteelDim, 0.4f)));
    return m;
  }

  Element bevelBar() {
    using namespace tv3;
    return place(
               box().fill(Material::linearUnit(
                   {0, 0}, {0, 1}, {{0.0f, C(0x98A3BA)}, {1.0f, C(0x66738F)}})),
               kStageX, 0, kStageW, 8)
        .translateY(
            animate(from(-10.0f).to(0.0f), {300ms, &ch::easeOutQuint, 1450ms}));
  }

  Element headerStrip() {
    using namespace tv3;
    Element strip = place(box().clip(), kStageX, 8, kStageW, 74);
    if (topHeader) {
      // Drawn at the bitmap's own half-res size and CROPPED at the stage
      // edge, exactly as the page shows it — squeezing it to fit reads
      // measurably lighter than the reference.
      strip.child(
          place(box().fill(stretchFill(topHeader, 1381, 77)), 0, 0, 1381, 77));
    } else {
      strip.fill(Material::linearUnit(
          {0, 0}, {1, 0.4f}, {{0.0f, C(0x2E3F5D)}, {1.0f, C(0x25334C)}}));
      strip.child(
          place(box().fill(diag.material()).opacity(0.18f), 0, 0, kStageW, 74));
    }
    return strip
        .translateY(
            animate(from(-84.0f).to(0.0f), {380ms, &ch::easeOutQuint, 1500ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {280ms, &ch::easeOutQuad, 1500ms}));
  }

  Element wordmark() {
    using namespace tv3;
    // The mark and BOTH text lines are near-white on the steel — the
    // panel carries all the contrast, the lockup none of it.
    Element mark = box().width(46).height(46);
    if (logoMark) {
      mark.fill(kNear).mask(by::alpha(stretchFill(logoMark, 46, 46)));
    } else {
      mark.corners({23})
          .stroke(stroke(3, Fill::color(kNear), PathFormat::Align::Inner))
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .child(t("2a", type(grotBold(), 18, kNear, 0, 1.0f)));
    }
    Element panel =
        place(box().row().alignItems(Align::Center).padding(30, 0).gap(16),
              kStageX, 82, kStageW, 86)
            .fill(Material::linearUnit(
                {0, 0}, {0, 1},
                {{0.0f, C(0x8C99B4)}, {0.6f, kSteel}, {1.0f, C(0x67748E)}}))
            .foreground(shapes::onEdges(shapes::Edge::Bottom,
                                        stroke(2, Fill::color(fade(kInk, 0.5f)),
                                               PathFormat::Align::Inner)))
            .child(mark)
            .child(
                box()
                    .column()
                    .gap(2)
                    .child(box()
                               .row()
                               .alignItems(Align::Start)
                               .gap(4)
                               .child(t("2 A D V A N C E D",
                                        type(grotBold(), 27, kNear, 80, 1.02f)))
                               .child(t("\xc2\xae", micro(9, kNear, 0))))
                    .child(t("S T U D I O S",
                             type(grotBold(), 12, kNear, 560, 1.0f))))
            .child(box().grow(1));
    return panel
        .translateY(
            animate(from(-60.0f).to(0.0f), {420ms, &ch::easeOutQuint, 1600ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 1600ms}));
  }

  Element navBar() {
    using namespace tv3;
    Element bar =
        place(box().row().alignItems(Align::Center), kStageX, 174, kStageW, 33);
    if (navbarBg)
      // Half-res native size, cropped at the stage edge (see the header
      // strip note — squeezing lightens the render).
      bar.clip().fill(stretchFill(navbarBg, 1338, 33));
    else
      bar.fill(Material::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, C(0x5A6A88)}, {0.5f, kNavbar}, {1.0f, C(0x3C4A63)}}));

    // Left: the section label window (dark, baked into the bitmap).
    bar.child(
        box()
            .width(230)
            .height(33)
            .row()
            .alignItems(Align::Center)
            .padding(12, 0)
            .gap(7)
            .fill(fade(C(0x39445C), 0.92f))
            .foreground(shapes::onEdges(shapes::Edge::Right,
                                        stroke(1, Fill::color(fade(kInk, 0.8f)),
                                               PathFormat::Align::Inner)))
            .child(t("\xe2\x86\x92", micro(11, kSteelHi, 0)))
            .child(t("2A.V3..2024 // EXPANSIONS", micro(11.5f, kNear, 80))));
    // Right: the six tab slots live in a slot so the active-section
    // indicator can move without re-describing the bar.
    bar.child(slot("navtabs"));
    return bar
        .translateY(
            animate(from(-40.0f).to(0.0f), {380ms, &ch::easeOutQuint, 1750ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {280ms, &ch::easeOutQuad, 1750ms}));
  }

  /** The tab row at `active` (-1 = home, nothing lit). The indicator is
   *  the riv's "nav indicator mark": a bright underline bar. */
  Element navTabs(int active) {
    using namespace tv3;
    Element row = box().width(Dim(kStageW - 230)).height(33).row();
    for (int i = 0; i < 6; ++i) {
      const bool on = i == active;
      row.child(box()
                    .grow(1)
                    .height(33)
                    .column()
                    .justify(Justify::Center)
                    .alignItems(Align::Center)
                    .gap(2)
                    .child(t(kSections[i].tab,
                             micro(11, on ? kNear : fade(kNear, 0.82f), 170)))
                    .child(box().width(46).height(2).fill(
                        on ? fade(kSteelHi, 0.95f) : SkColor4f{0, 0, 0, 0})));
    }
    return row;
  }

  /** The two hairline bars between navbar and stage art. */
  Element hairlines() {
    using namespace tv3;
    return place(box()
                     .column()
                     .gap(2)
                     .child(box().height(2).fill(fade(kSteelHi, 0.8f)))
                     .child(box()
                                .height(3)
                                .fill(fade(kSeam, 0.95f))
                                .child(box()
                                           .inset(0)
                                           .fill(vticks.material())
                                           .opacity(0.55f))),
                 kStageX, 210, kStageW, 7)
        .opacity(
            animate(from(0.0f).to(1.0f), {280ms, &ch::easeOutQuad, 1800ms}));
  }

  /** The stage viewport. Its CONTENT lives in a slot — the section
   *  cycle swaps art through it — while the entrance wipe (the riv's
   *  "MASK: white rec swipe" gesture) stays out here and plays once. */
  Element stageArt() {
    using namespace tv3;
    return place(box().clip().child(slot("stage")), kStageX, kArtY, kStageW,
                 kArtH)
        .mask(by::edge(0, animate(from(0.0f).to(1.0f),
                                  {650ms, &ch::easeOutQuint, 1900ms})));
  }

  /** One section's stage art at full opacity: the bitmap, and for the
   *  numbered sections a title spreader, sub-nav tabs and the RETURN TO
   *  MAIN affordance. `sec` < 0 is home. `settle` ∈ [0,1] drives the
   *  title spreader: tracking-in from wide, the riv's "title spreader". */
  Element sectionArt(int sec, float settle) {
    using namespace tv3;
    // Keyed per section AND per settle bucket: structurally different
    // content must REPLACE in the slot, not be patched over — a text
    // leaf patched onto a container trips the layout engine.
    char key[32];
    std::snprintf(key, sizeof key, "sec:%d:%d", sec,
                  settle >= 1.0f ? 1 : (int)(settle * 14));
    Element art = box().key(key).width(Dim(kStageW)).height(Dim(kArtH)).clip();
    const ImagePtr& bg = sec < 0 ? homeBg : sectionBg[(size_t)sec];
    if (bg)
      art.fill(stretchFill(bg, kStageW, kArtH));
    else
      art.fill(Material::linearUnit({0, 0}, {0, 1},
                                    {{0.0f, C(0x2A3A58)}, {1.0f, kDeep}}));

    if (sec < 0) {
      if (!clouds.empty() && gapMask) {
        // The 62-frame loop is FULL sky footage — blue sky, clouds and
        // all — so it REPLACES the opening rather than tinting it:
        // drawn behind the structure through the feathered opening
        // coverage, slightly desaturated and dimmed so the footage
        // sits in the plate's own exposure.
        static const sk_sp<SkImageFilter> kCloudLook = [] {
          const float sat = 0.60f, gain = 0.90f;
          const float lr = 0.2126f * (1 - sat), lg = 0.7152f * (1 - sat),
                      lb = 0.0722f * (1 - sat);
          const float m[20] = {gain * (lr + sat),
                               gain * lg,
                               gain * lb,
                               0,
                               0,
                               gain * lr,
                               gain * (lg + sat),
                               gain * lb,
                               0,
                               0,
                               gain * lr,
                               gain * lg,
                               gain * (lb + sat),
                               0,
                               0,
                               0,
                               0,
                               0,
                               1,
                               0};
          return SkImageFilters::ColorFilter(SkColorFilters::Matrix(m),
                                             nullptr);
        }();
        art.child(box()
                      .inset(0)
                      .child(place(box().clip().child(slot("clouds")), 415, 35,
                                   310, 255))
                      .mask(by::alpha(Material::image(
                          gapMask, SkTileMode::kClamp, SkTileMode::kClamp,
                          SkMatrix::Scale(kStageW / (float)gapMask->width(),
                                          kArtH / (float)gapMask->height()))))
                      .effect(Effect::filter(kCloudLook))
                      .opacity(0.95f));
      }
      // Idle beacon on the art's readout cluster: the one light that
      // never stops blinking.
      art.child(place(box().corners({3}), kStageW - 116, kArtH - 62, 6, 6)
                    .fill(fade(kSteelHi, 0.9f))
                    .opacity(&beaconAlpha));
      return art;
    }

    const SectionSpec& spec = kSections[(size_t)sec];
    // Title spreader, top right: the letterform block settles from
    // stretched-wide to rest as the section engages.
    art.child(
        place(box()
                  .row()
                  .justify(Justify::End)
                  .alignItems(Align::Center)
                  .gap(10)
                  .child(box().grow(1).height(1).fill(fade(kSteelHi, 0.55f)))
                  .child(t(spec.tab, micro(14, kNear, 600)))
                  .child(box().width(24).height(8).fill(fade(kSteelHi, 0.8f))),
              kStageW - 560, 12, 540, 22)
            .opacity(0.25f + 0.75f * settle)
            .scaleX(1.5f - 0.5f * settle)
            .transformOrigin(1.0f, 0.5f));
    // Sub-nav tabs, centre top — sections without them page by arrows.
    if (spec.subnav[0]) {
      Element tabs = box().row().gap(2);
      for (const char* s : spec.subnav)
        if (s)
          tabs.child(box()
                         .height(17)
                         .padding(10, 0)
                         .fill(fade(kSeam, 0.92f))
                         .stroke(stroke(1, Fill::color(fade(kSteelHi, 0.45f)),
                                        PathFormat::Align::Inner))
                         .justify(Justify::Center)
                         .alignItems(Align::Center)
                         .child(t(s, micro(9, kNear, 200))));
      art.child(place(box().row().justify(Justify::Center).child(tabs),
                      kStageW / 2 - 220, 26, 440, 17)
                    .opacity(settle));
    }
    // RETURN TO MAIN, bottom right.
    art.child(place(t("[ RETURN TO MAIN ]", micro(9, fade(kNear, 0.85f), 200)),
                    kStageW - 190, kArtH - 30, 180, 14)
                  .opacity(settle));
    // MODULE.ENGAGED tick, bottom left — the riv's load-state voice.
    art.child(place(t(settle >= 1.0f ? "MODULE.ENGAGED" : "LOADING.MODULE",
                      micro(9, fade(kSteelHi, 0.8f), 240)),
                    18, kArtH - 30, 220, 14));
    return art;
  }

  /** The section change: the riv's "Section Transition Effect" — the
   *  incoming art claims the viewport behind a straight edge that jumps
   *  in FOURTEEN discrete steps ("shape trans step 1".."14"), with a
   *  bright leading band at the edge. Quantized on purpose: the smooth
   *  version reads as a crossfade, the stepped one as machinery. */
  Element transitionArt(int fromSec, int toSec, int step) {
    using namespace tv3;
    const float f = (float)step / 14.0f;
    char key[32];
    std::snprintf(key, sizeof key, "trans:%d:%d", fromSec, toSec);
    Element out = box().key(key).width(Dim(kStageW)).height(Dim(kArtH)).clip();
    out.child(box().inset(0).child(sectionArt(fromSec, 1.0f)));
    out.child(box().inset(0).child(sectionArt(toSec, f)).mask(by::edge(0, f)));
    // the leading band, one step wide, brightest at mid-sweep
    const float x = kStageW * f;
    out.child(place(box().fill(fade(kSteelHi, 0.85f)), x - 5, 0, 10, kArtH)
                  .blend(SkBlendMode::kScreen)
                  .opacity(0.28f + 0.5f * std::sin(f * 3.14159f)));
    return out;
  }

  Element scrollStrip() {
    using namespace tv3;
    return place(box()
                     .row()
                     .alignItems(Align::Center)
                     .padding(10, 0)
                     .gap(6)
                     .fill(C(0x4B5870))
                     .foreground(shapes::onEdges(
                         shapes::Edge::Top,
                         stroke(1, Fill::color(fade(kSteelHi, 0.55f)),
                                PathFormat::Align::Inner)))
                     .child(t("\xe2\x86\x93", micro(9, kSteelHi, 0)))
                     .child(t("SCROLL.EXTENDED.CONTENT",
                              micro(9, fade(kSteelHi, 0.85f), 180)))
                     .child(box().grow(1))
                     .child(
                         t("AMBIENCE.MUTE", micro(9, fade(kSteel, 0.9f), 180))),
                 kStageX, 617, kStageW, 16)
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 2200ms}));
  }

  /** One lower module: title bar + bordered translucent body. */
  Element module(const char* glyph, const char* barLabel, Element body,
                 int order) {
    using namespace tv3;
    return box()
        .width(Dim(kPanelW))
        .column()
        .child(moduleBar(glyph, barLabel, kPanelW))
        .child(box()
                   .grow(1)
                   .fill(fade(C(0x4A5872), 0.80f))
                   .stroke(stroke(1, Fill::color(fade(kSteelHi, 0.55f)),
                                  PathFormat::Align::Inner))
                   .child(body.inset(0)))
        .translateY(animate(from(46.0f).to(0.0f),
                            {420ms, &ch::easeOutQuint,
                             std::chrono::milliseconds(2300 + 120 * order)}))
        .opacity(animate(from(0.0f).to(1.0f),
                         {320ms, &ch::easeOutQuad,
                          std::chrono::milliseconds(2300 + 120 * order)}));
  }

  /** The framed thumb plate the two outer modules share: a mini toolbar
   *  strip up top, a dark viewport with its top-right corner cut at 45°,
   *  and the labelled plate attached beneath with the mirrored cut. */
  Element thumbPlate(Element content, const char* btn) {
    using namespace tv3;
    return box()
        .width(150)
        .shrink(0)
        .column()
        .gap(2)
        // the toolbar strip: a small lit segment on a dark rail
        .child(
            box()
                .height(8)
                .fill(C(0x2A3550))
                .row()
                .alignItems(Align::Center)
                .padding(3, 0)
                .child(box().width(28).height(4).fill(fade(kSteelHi, 0.85f))))
        .child(box()
                   .height(96)
                   .shape(chamfer(20, kTR))
                   .fill(C(0x232E48))
                   .stroke(stroke(1, Fill::color(fade(kSteelHi, 0.5f)),
                                  PathFormat::Align::Inner))
                   .justify(Justify::Center)
                   .alignItems(Align::Center)
                   .child(std::move(content)))
        .child(box()
                   .height(22)
                   .shape(chamfer(14, kBL))
                   .fill(C(0x313D5A))
                   .stroke(stroke(1, Fill::color(fade(kSteelHi, 0.45f)),
                                  PathFormat::Align::Inner))
                   .justify(Justify::Center)
                   .alignItems(Align::Center)
                   .child(t(btn, micro(11, kNear, 140))));
  }

  /** The lockup inside FEATURED.PARTNER's plate: 2a mark + R mark. */
  Element riveLockup() {
    using namespace tv3;
    Element row = box().row().gap(7).alignItems(Align::Center);
    if (logoMark)
      row.child(box().width(34).height(34).fill(kNear).mask(
          by::alpha(stretchFill(logoMark, 34, 34))));
    row.child(t("+", type(grotBold(), 13, fade(kNear, 0.9f), 0)));
    if (riveLogo)
      row.child(box().width(44).height(44).fill(stretchFill(riveLogo, 44, 44)));
    else
      row.child(t("R", type(grotBold(), 26, kNear, 0)));
    return row;
  }

  Element featuredPartner() {
    using namespace tv3;
    Element body =
        box()
            .row()
            .padding(12)
            .gap(12)
            .child(thumbPlate(riveLockup(), "VISIT RIVE"))
            .child(box()
                       .grow(1)
                       .column()
                       .gap(8)
                       .child(box()
                                  .row()
                                  .gap(7)
                                  .alignItems(Align::Center)
                                  .child(meter(2))
                                  .child(t("2ADVANCED POWERED BY RIVE",
                                           micro(12, kNear, 20))))
                       .child(t("WE HAVE REARCHITECTED OUR 2001 WEBSITE \"V.3 "
                                "EXPANSIONS\", (PREVIOUSLY AWARDED \"MOST "
                                "INFLUENTIAL FLASH WEBSITE OF THE DECADE\") "
                                "USING THE RIVE INTERACTIVE ANIMATION PLATFORM "
                                "IN COMBINATION WITH REACT JS",
                                prose(12, C(0xC7D0DD)))));
    return module("F", "FEATURED.PARTNER", std::move(body), 0);
  }

  Element subData() {
    using namespace tv3;
    Element icon = box().width(56).height(56);
    if (!discordSeq.empty())
      icon.child(slot("discord"));
    else
      icon.corners({32}).fill(fade(kSteel, 0.5f));
    Element body =
        box()
            .column()
            .padding(12)
            .gap(7)
            .alignItems(Align::Center)
            .child(box()
                       .row()
                       .gap(6)
                       .alignItems(Align::Center)
                       .child(meter(3))
                       .child(t("JOIN THE 2A DISCORD COMMUNITY",
                                micro(13.5f, kNear, 60))))
            .child(icon)
            .child(t("2ADVANCED IS BUILDING THE ULTIMATE INDUSTRY DISCORD "
                     "SPACE FOR REALTIME CREATIVE COLLABORATION, SHARING OF "
                     "INTERESTS AND BROAD PEER SUPPORT - JOIN US HERE.",
                     prose(12, C(0xC7D0DD))))
            .child(box().grow(1))
            .child(box()
                       .row()
                       .gap(8)
                       .alignItems(Align::Center)
                       .child(t("JOIN OUR DISCORD", micro(12, kNear, 120)))
                       .child(meter(2)));
    return module("S", "SUB.DATA", std::move(body), 1);
  }

  Element updates() {
    using namespace tv3;
    Element body =
        box()
            .row()
            .padding(12)
            .gap(12)
            .child(thumbPlate(dddLogo
                                  ? box().width(56).height(72).fill(
                                        stretchFill(dddLogo, 56, 72))
                                  : t("DDD", type(grotBold(), 20, kNear, 100)),
                              "VISIT DDD"))
            .child(
                box()
                    .grow(1)
                    .column()
                    .gap(8)
                    .child(box()
                               .row()
                               .gap(7)
                               .alignItems(Align::Center)
                               .child(meter(2))
                               .child(t("CATCH 2A LIVE AT DDD IN MILAN",
                                        micro(12, kNear, 20))))
                    .child(t("DIGITAL DESIGN DAYS IS OFFICIALLY BACK IN 2024 "
                             "AND 2ADVANCED WILL BE THERE. COME SEE FOUNDERS "
                             "ERIC JORDAN & TONY NOVAK SPEAK AT THE UPCOMING "
                             "DDD EVENT IN MILAN, ITALY OCT 6TH-8TH. GET YOUR "
                             "TICKETS BEFORE THEY'RE GONE!",
                             prose(12, C(0xC7D0DD)))));
    return module("U", "UPDATES", std::move(body), 2);
  }

  Element mailingList() {
    using namespace tv3;
    Element body =
        box()
            .column()
            .padding(12, 8)
            .gap(6)
            .child(t("ENTER EMAIL ADDRESS:", micro(11, C(0xC7D0DD), 100)))
            .child(box()
                       .row()
                       .gap(8)
                       .alignItems(Align::Center)
                       .child(box()
                                  .grow(1)
                                  .height(22)
                                  .fill(fade(kPage, 0.9f))
                                  .stroke(
                                      stroke(1, Fill::color(fade(kSteel, 0.6f)),
                                             PathFormat::Align::Inner))
                                  .row()
                                  .alignItems(Align::Center)
                                  .padding(7, 0)
                                  .child(t("EMAILADDRESS@DOMAIN.COM",
                                           micro(9, fade(kBody, 0.7f), 100))))
                       .child(button("SUBMIT", 64)));
    return module("M", "MAILING LIST", std::move(body), 3);
  }

  Element support2a() {
    using namespace tv3;
    auto half = [&](const char* head, const char* copy, const char* btn) {
      return box()
          .grow(1)
          .basis(Dim(0))
          .column()
          .gap(4)
          .alignItems(Align::Center)
          .child(t(head, micro(11, kNear, 60)))
          .child(t(copy, prose(9, fade(C(0xC7D0DD), 0.95f))))
          .child(box().grow(1))
          .child(box()
                     .row()
                     .gap(6)
                     .alignItems(Align::Center)
                     .child(meter(2))
                     .child(t(btn, micro(10, kNear, 120))));
    };
    Element body =
        box()
            .row()
            .padding(12, 4)
            .gap(14)
            .child(half("2A ON PATREON",
                        "GET 2A SOURCE CODE, DIGITAL ASSETS, MUSIC, "
                        "DOCUMENTS, FILES AND MORE...",
                        "GET SOURCE"))
            .child(half("2A MERCH SHOP",
                        "GET T-SHIRTS, HATS, APARREL, POSTERS, WORKSPACE "
                        "AND CREATIVE TOYS AND MORE...",
                        "SHOP MERCH"));
    return module("S", "SUPPORT 2A", std::move(body), 4);
  }

  Element follow2a() {
    using namespace tv3;
    Element icons = box().height(16);
    if (socialSprite) {
      // The sprite is authored @2x (436×32); the layout shows it at 1×.
      const float iw = (float)socialSprite->width() * 0.5f;
      icons.width(Dim(iw)).fill(stretchFill(socialSprite, iw, 16));
    } else {
      icons.row().gap(12);
      for (int i = 0; i < 7; ++i)
        icons.child(
            box().width(16).height(16).corners({8}).fill(fade(kSteelHi, 0.8f)));
    }
    Element body = box()
                       .column()
                       .padding(12, 8)
                       .justify(Justify::Center)
                       .alignItems(Align::Center)
                       .child(icons);
    return module("F", "FOLLOW 2A", std::move(body), 5);
  }

  Element footerRail() {
    using namespace tv3;
    return place(
               box()
                   .row()
                   .alignItems(Align::Center)
                   .padding(10, 0)
                   .gap(8)
                   .fill(Material::linearUnit(
                       {0, 0}, {0, 1},
                       {{0.0f, C(0x5A6880)}, {1.0f, C(0x49556C)}}))
                   .child(t("(C) 2024 2ADVANCED STUDIOS", micro(9, kInk, 140)))
                   .child(t("//", micro(9, fade(kInk, 0.5f), 0)))
                   .child(t("CONDITIONS OF USE", micro(9, kInk, 140)))
                   .child(t("//", micro(9, fade(kInk, 0.5f), 0)))
                   .child(t("PRIVACY POLICY", micro(9, kInk, 140)))
                   .child(box().grow(1))
                   .child(t("HOSTING PARTNER:", micro(9, kInk, 140)))
                   .child(box().width(12).height(12).corners({6}).fill(kHost)),
               kStageX, 1045, kStageW, 20)
        .opacity(
            animate(from(0.0f).to(1.0f), {320ms, &ch::easeOutQuad, 2900ms}));
  }

  // ---- boot overlay: the site's own preloader page, compressed ------------
  // Its stylesheet is the spec: page #2A3753; the stacked lockup
  // (2a-logo@2x.png, drawn 197×94, inverted to white); a tracked
  // #7183A5 subtitle; and a HUGE #7183A5 percentage below — the real
  // page sets it at 200 px.

  Element bootOverlay() {
    using namespace tv3;
    const SkColor4f kPreBg = C(0x2A3753), kPreInk = C(0x7183A5);
    Element lockup = box().width(197).height(94);
    if (pageLogo)
      // The bitmap is near-black art; the page shows it inverted. A
      // white fill through its coverage is that filter's visible result.
      lockup.fill(kNear).mask(by::alpha(stretchFill(pageLogo, 197, 94)));
    else
      lockup.justify(Justify::Center)
          .alignItems(Align::Center)
          .child(t("2ADVANCED", type(grotBold(), 24, kNear, 200, 1.0f)));

    Element o = stack().inset(0).zIndex(90);
    o.child(box().inset(0).fill(kPreBg).opacity(
        animate(through({{0ms, 1.0f}, {1250ms, 1.0f}, {1450ms, 0.0f}}))));
    o.child(
        place(box().column().alignItems(Align::Center).gap(18), kW / 2 - 300,
              kH / 2 - 170, 600, 360)
            .opacity(animate(through(
                {{0ms, 0.0f}, {150ms, 1.0f}, {1200ms, 1.0f}, {1350ms, 0.0f}})))
            .child(lockup)
            .child(t("SOLACE IN TECHNOLOGY. BELIEF IN THE FUTURE.",
                     type(grot(), 10, kPreInk, 400, 1.0f)))
            .child(slot("bootpct")));
    o.opacity(animate(through({{1400ms, 1.0f}, {1450ms, 0.0f}})));
    return o;
  }

  Element bootReadout() {
    using namespace tv3;
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", bootPct);
    return t(buf, type(grot(), 150, C(0x7183A5), 0, 1.0f));
  }

  ch::Output<float> beaconAlpha{1.0f};

  // =========================================================================

  Element describe() {
    using namespace tv3;
    Element page = stack();
    if (pageTile) {
      // The 10×1600 strip exactly as the CSS places it: repeated across,
      // clamped down (the page is shorter than the strip).
      page.fill(stretchFill(pageTile, 10, 1600, SkTileMode::kRepeat));
    } else {
      page.fill(Material::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, kPageHi}, {0.55f, kPage}}));
    }
    page.child(bevelBar());
    page.child(headerStrip());
    page.child(wordmark());
    page.child(navBar());
    page.child(hairlines());
    page.child(stageArt());
    page.child(scrollStrip());

    // The poly-textured ground every lower module sits on.
    Element ground = place(box().clip(), kStageX, 640, kStageW, 400);
    if (lowerPanelBg)
      ground.fill(stretchFill(lowerPanelBg, kStageW, 400));
    else
      ground.fill(Material::linearUnit({0, 0}, {1, 1},
                                       {{0.0f, C(0x22304A)}, {1.0f, kPage}}));
    ground.opacity(
        animate(from(0.0f).to(1.0f), {380ms, &ch::easeOutQuad, 2250ms}));
    page.child(ground);

    Element mods = place(box().row().gap(10), kStageX, kModY, kStageW, kModH);
    mods.child(featuredPartner()).child(subData()).child(updates());
    page.child(mods);
    // the dark divider band that closes the module row
    page.child(place(box().fill(fade(C(0x26314A), 0.9f)), kStageX,
                     kModY + kModH + 2, kStageW, 8)
                   .opacity(animate(from(0.0f).to(1.0f),
                                    {320ms, &ch::easeOutQuad, 2650ms})));

    Element row = place(box().row().gap(10), kStageX, kRowY, kStageW, kRowH);
    row.child(mailingList()).child(support2a()).child(follow2a());
    page.child(row);

    page.child(footerRail());
    page.child(bootOverlay());
    return page;
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    using namespace tv3;
    ctx.canvas(kW, kH);
    ctx.background(kPage);
    // Everything has entered by ~3.3 s; 7.6 s puts the discord orb loop
    // (20 fps, 102 frames) on its bright crest, frame 50.
    ctx.captureAt(7.6);

    diag = patterns::stripes(2, 9, fade(kSteelHi, 0.5f));
    diag.rotate(45);
    dots = patterns::halftone(5, 1.3f, fade(kInk, 0.55f));
    vticks = patterns::stripes(1.5f, 5.5f, fade(kSteelHi, 0.5f));

    // --- the production assets, from the live site ------------------------
    // https fetches cache on disk (CacheFirst): the first run downloads,
    // every later run is served locally; a failed fetch leaves the
    // pointer null and the use site builds its steel stand-in.
    {
      sigil::loader::Hub& hub = ctx.assets.hub();
      const std::string site = "https://v3.2advanced.com/";
      pageTile = hub.image(site + "V3ExpansionsReboot/assets/background.gif");
      socialSprite = hub.image(site +
                               "V3ExpansionsReboot/assets/images/social-icons"
                               "@2x.png");
      logoMark = hub.image(
          site + "v3expansionsreboot/assets/2advancedLogo_Preload.svg",
          {.width = 120});
      pageLogo =
          hub.image(site + "V3ExpansionsReboot/assets/images/2a-logo@2x.png");
      if (auto blob = hub.blob(site + "v3expansionsreboot/mainstage.riv"))
        extractRivImages(blob->bytes);
      buildGapMask();
    }

    // --- idle motion ------------------------------------------------------
    ctx.ticker.add([this, tAcc = 0.0](double dt) mutable {
      tAcc += dt;
      const float s = (float)tAcc;
      // the art beacon: sharp on, slow decay, period 2.4 s
      const float ph = std::fmod(s, 2.4f);
      beaconAlpha = ph < 0.12f ? 1.0f : std::max(0.15f, 1.0f - ph * 0.8f);
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("bootpct", bootReadout());
    renderNavTabs(ctx, -1);
    renderStage(ctx, sectionArt(-1, 1.0f), true);
    if (!discordSeq.empty()) renderDiscordFrame(ctx, 0);
  }

  // Slot content must carry its own dims: a slot node sizes from its
  // content, so an inset()-positioned filler inside one measures zero.
  void renderCloudFrame(sketch::SketchContext& ctx, int frame) {
    using namespace tv3;
    cloudFrame = frame;
    ctx.composer.renderSlot("clouds",
                            box().width(310).height(255).fill(
                                stretchFill(clouds[(size_t)frame], 310, 255)));
  }

  void renderDiscordFrame(sketch::SketchContext& ctx, int frame) {
    using namespace tv3;
    discordFrame = frame;
    ctx.composer.renderSlot(
        "discord", box().width(64).height(64).fill(
                       stretchFill(discordSeq[(size_t)frame], 64, 64)));
  }

  // --- the section cycle -----------------------------------------------
  // The home view holds until kCycleStart, then the simulated visitor
  // walks every tab in order and returns to main — each change playing
  // the stepped shape-wipe. All of it is a pure function of the clock,
  // so captures land on the same frame every run.
  static constexpr double kCycleStart = 8.0, kHold = 5.0, kTrans = 0.7;
  int stageSec = -1;   // section the stage currently shows (-1 = home)
  int stageStep = -1;  // -1 stable, else the transition step shown
  int navActive = -2;  // tab lit in the nav slot (-1 = none), -2 = unset

  static int stopTarget(int stop) { return stop < 6 ? stop : -1; }

  void renderStage(sketch::SketchContext& ctx, const Element& content,
                   bool hasHome) {
    ctx.composer.renderSlot("stage", content);
    // The clouds slot was just rebuilt empty inside fresh home art —
    // re-push the current frame so the sky never blanks for a frame.
    if (hasHome && !clouds.empty())
      renderCloudFrame(ctx, std::max(0, cloudFrame));
  }

  void renderNavTabs(sketch::SketchContext& ctx, int active) {
    navActive = active;
    ctx.composer.renderSlot("navtabs", navTabs(active));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    using namespace tv3;
    // The section cycle state, then the frame sequences — everything on
    // the DATA path, each slot swap touching only its own subtree.
    int sec = -1, step = -1, from = -1;
    if (elapsed >= kCycleStart) {
      const double u = std::fmod(elapsed - kCycleStart, 7.0 * kHold);
      const int stop = (int)(u / kHold);
      const double within = u - stop * kHold;
      sec = stopTarget(stop);
      from = stop == 0 ? -1 : stopTarget(stop - 1);
      if (within < kTrans)
        step = std::min(14, 1 + (int)(within / kTrans * 14.0));
    }
    if (navActive != sec) renderNavTabs(ctx, sec);
    if (sec != stageSec || step != stageStep) {
      const bool hasHome = sec < 0 || (step >= 0 && from < 0);
      if (step < 0)
        renderStage(ctx, sectionArt(sec, 1.0f), sec < 0);
      else
        renderStage(ctx, transitionArt(from, sec, step), hasHome);
      stageSec = sec;
      stageStep = step;
    }

    const bool homeVisible = stageSec < 0 || (stageStep >= 0 && from < 0);
    if (!clouds.empty() && homeVisible) {
      const int frame = (int)(elapsed * 8.0) % 62;
      if (frame != cloudFrame) renderCloudFrame(ctx, frame);
    }
    if (!discordSeq.empty()) {
      // The orb is authored as a full dark→lit→dark pulse, so the loop
      // plays every frame; the capture moment lands on its bright crest.
      const int frame = (int)(elapsed * 20.0) % 102;
      if (frame != discordFrame) renderDiscordFrame(ctx, frame);
    }
    // Boot percentage, text content via its own slot.
    if (booted) return;
    const double u = (elapsed - 0.12) / 1.05;
    const int pct = (int)std::lround(std::clamp(u, 0.0, 1.0) * 100.0);
    if (pct == bootPct) return;
    bootPct = pct;
    if (pct >= 100) booted = true;
    ctx.composer.renderSlot("bootpct", bootReadout());
  }
};

SIGIL_SKETCH(TwoAdvancedV3, "Study \xc2\xb7 Screens",
             "2Advanced Studios V3 Expansions Reboot (2024) \xe2\x80\x94 the "
             "production art, lifted from the live site's own Rive file")
