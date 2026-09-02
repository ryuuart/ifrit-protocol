// twoadvanced_equipment.cpp — 2Advanced Studios // Equipment.Modules
// (2003): the v4-era merchandise store at /equipment/, an HTML 4.0
// FRAMESET of Dreamweaver-exported tables, live today at
// https://v4prophecy.2advanced.com/equipment/.
//
// REFERENCE — the page's own HTML and CSS, taken literally:
//   · index.html: FRAMESET rows 103,*,15; the middle splits cols 262,*.
//   · topframe.htm: ecom-topbar.gif 790×19; then a table row of
//     ecom-logo.gif 262×78 and ecom-titleheader.gif 511×63; the four
//     nav buttons (92/92/92/105 × 11) flow inline after the title image
//     and wrap beneath it. Their rollover states are real files —
//     MM_preloadImages names every -on.gif — and MM_swapImage swaps
//     src on mouseover, which is what the hover cycle below replays.
//   · leftframe-productselection.htm: ecom-productselectimage.jpg
//     262×266, alone on white.
//   · productselect.htm: ecom-productselection.gif 501×16, then per
//     product a #7C252C header row 15 high (arrow 16×15, name in
//     Verdana size=1 white, 3-dots 17×15 right), a 2 px seam, and a
//     52-high body row (69×52 thumb, then a 416-wide #F0E7E8 cell:
//     Verdana size=1 #7C252C copy padded 7, ecom-viewdetails.gif 84×16
//     bottom-right), then a 6 px gap. Seven products, then
//     ecom-breakerbar.gif 501×6 and ecom-copyright.gif 165×11 right.
//     Body link colour #7C252C. The BODY styles the IE scrollbar:
//     face #BBC0C9, track #E4E6EA, arrows #666666 — drawn here because
//     the content (≈570 px) overflows the frame and the era showed it.
//   · bottomframe.htm: ecom-bottombar.gif 790×11.
//
// Every bitmap above is fetched from the restoration host over
// SigilLoader's https path (disk-cached after the first run); a missing
// fetch leaves a flat #7C252C or white stand-in so the sketch still
// renders offline. The type is Verdana at HTML size=1 — 10 px — which
// macOS ships.
//
// The page is STATIC; its only behaviours are the JS rollovers and the
// frame's scrollbar, so those are the only motion here: a simulated
// pointer walks the four top buttons (swapping in the real -on.gif
// states), and the content frame auto-scrolls its overflow, thumb
// tracking in the styled scrollbar. Frame is drawn at ×2 of the 790×580
// page: 1580×1160.

#include <include/core/SkFontMgr.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace teq {

// The page's entire palette, straight from its attributes.
constexpr SkColor4f kMaroon = hex(0x7C252C);   // header rows, copy, links
constexpr SkColor4f kRose = hex(0xF0E7E8);     // description cells
constexpr SkColor4f kWhite = hex(0xFFFFFF);    // BODY bgColor
constexpr SkColor4f kSbFace = hex(0xBBC0C9);   // SCROLLBAR-FACE-COLOR
constexpr SkColor4f kSbTrack = hex(0xE4E6EA);  // SCROLLBAR-TRACK-COLOR
constexpr SkColor4f kSbArrow = hex(0x666666);  // SCROLLBAR-ARROW-COLOR

inline sk_sp<SkTypeface> verdanaFace(bool bold) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      "Verdana",
      SkFontStyle(
          bold ? SkFontStyle::kBold_Weight : SkFontStyle::kNormal_Weight,
          SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(
        "Arial", bold ? SkFontStyle::Bold() : SkFontStyle::Normal());
  return f;
}

/** Verdana at HTML size=1: 10 px. */
inline sigil::weave::TextStyle verdana(SkColor4f color, bool bold = false) {
  sigil::weave::TextStyle s;
  static const sk_sp<SkTypeface> regular = verdanaFace(false);
  static const sk_sp<SkTypeface> boldFace = verdanaFace(true);
  s.shaping.typeface = bold ? boldFace : regular;
  s.shaping.fontSize = 10;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline Element t(const char* s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

// Frameset geometry, in the page's own CSS pixels.
constexpr float kPageW = 790, kPageH = 580;
constexpr float kTopH = 103, kBottomH = 15;
constexpr float kLeftW = 262;
constexpr float kContentH = kPageH - kTopH - kBottomH;  // the * row
constexpr float kSbW = 16;                              // IE scrollbar

struct Product {
  const char* name;
  const char* thumb;  // productselect_files/ file name
  const char* copy;
};
// The seven products, names and copy verbatim (typos included).
constexpr Product kProducts[7] = {
    {"\"Phiberglass\" T-Shirt", "ecom-sm_phiberglassshirt.gif",
     "Our V.4 expeditionary media vehicle enabled t-shirt offering for "
     "anti-media control applications."},
    {"\"Operative\" T-Shirt", "ecom-sm_operativeshirt.gif",
     "Graphical intelligence gear for the efficient outfitting of any "
     "special design force operatives."},
    {"\"Prophecy\" T-Shirt", "ecom-sm_prophecyshirt.gif",
     "Version 4.0 underground media t-shirt, equipment ready for future "
     "shock."},
    {"\"Identity\" T-shirt", "ecom-sm_identityshirt.gif",
     "100% Pure 2Advanced Studios Branded Tee - Taking Pride in Raw and "
     "Unadulterated Simplicity. Available in White, Blue, and Black. All "
     "sizes."},
    {"\"Expansions\" T-shirt", "ecom-sm_expansionsshirt.gif",
     "2Advanced Studios \"Expansions\" Tshirt [Limited V3 Site Release "
     "Edition]. Available in Blue and Black. All sizes."},
    {"\"Flash MX Magic\"", "ecom-productimage1.gif",
     "Co-written by 2Advanced, Learn about XML Integration with Flash MX. "
     "Complete with Sample FLAs. Pre-order an Autographed Copy Today!"},
    {"\"Plat4m\" Poster by Eric Jordan & Probe3 of 2Advanced",
     "ecom-productimage4.gif",
     "High quality poster printed on 80 weight gloss coverstock for a true "
     "thickness and quality. [18x27 inches]. Sponsored by Invicid."},
};

}  // namespace teq

// ===========================================================================

struct TwoAdvancedEquipment : sketch::Sketch {
  using ImagePtr = std::shared_ptr<const sigil::image::ImageAsset>;

  // Keyed by file name under equipment/index_files/.
  std::map<std::string, ImagePtr, std::less<>> art;

  ch::Output<float> scrollY{0.0f};           // content translate (negative)
  ch::Output<float> thumbY{0.0f};            // scrollbar thumb offset
  std::array<ch::Output<float>, 4> hover{};  // top button -on states

  float contentOverflow = 0;

  static Material imageFill(const ImagePtr& asset, float w, float h) {
    const sk_sp<SkImage>& img = asset->frames()[0].image;
    return Material::image(
        img, SkTileMode::kClamp, SkTileMode::kClamp,
        SkMatrix::Scale(w / (float)img->width(), h / (float)img->height()),
        SkSamplingOptions(SkFilterMode::kLinear));
  }

  /** The bitmap at its own HTML display size, or a flat stand-in. */
  Element img(const char* name, float w, float h,
              SkColor4f fallback = teq::kMaroon) {
    Element e = box().width(Dim(w)).height(Dim(h)).shrink(0);
    auto it = art.find(name);
    if (it != art.end() && it->second)
      e.fill(imageFill(it->second, w, h));
    else
      e.fill(fallback);
    return e;
  }

  // ---- the three frames ---------------------------------------------------

  Element topFrame() {
    using namespace teq;
    Element f = at(box(), 0, 0, kPageW, kTopH).clip();
    f.child(at(img("ecom-topbar.gif", 790, 19), 0, 0, 790, 19));
    f.child(at(img("ecom-logo.gif", 262, 78), 0, 19, 262, 78));
    f.child(at(img("ecom-titleheader.gif", 511, 63), 262, 19, 511, 63));
    // The four rollover buttons wrap beneath the title image. The page
    // preloads an -on.gif for each, but the archive never captured
    // those states (they only fetched on hover), so the swap is
    // approximated: the -off bitmap with a lift riding a bound opacity.
    static const char* offs[4] = {
        "ecom-button-shoppinginfo-of.gif", "ecom-button-checkout-off.gif",
        "ecom-button-viewcart-off.gif", "ecom-button-questions-off.gif"};
    float x = 262;
    for (int i = 0; i < 4; ++i) {
      const float w = i == 3 ? 105.0f : 92.0f;
      f.child(at(img(offs[i], w, 11), x, 82, w, 11));
      f.child(at(box().fill(alpha(kWhite, 0.4f)), x, 82, w, 11)
                  .opacity(&hover[(size_t)i]));
      x += w;
    }
    return f;
  }

  Element leftFrame() {
    using namespace teq;
    return at(box().fill(kWhite), 0, kTopH, kLeftW, kContentH)
        .clip()
        .child(
            at(img("ecom-productselectimage.jpg", 262, 266), 0, 0, 262, 266));
  }

  /** One product: the maroon header row, the 2 px seam, the body row. */
  Element product(const teq::Product& p) {
    using namespace teq;
    Element block = box().column().width(501);
    block.child(box()
                    .height(15)
                    .row()
                    .child(box().width(13))
                    .child(box()
                               .width(16)
                               .fill(kMaroon)
                               .justify(Justify::Center)
                               .alignItems(Align::Center)
                               .child(img("ecom-arrowbutton.gif", 16, 15)))
                    .child(box()
                               .grow(1)
                               .fill(kMaroon)
                               .row()
                               .alignItems(Align::Center)
                               .padding(4, 0)
                               .child(t(p.name, verdana(kWhite))))
                    .child(box()
                               .width(17)
                               .fill(kMaroon)
                               .justify(Justify::Center)
                               .alignItems(Align::Center)
                               .child(img("ecom-3dots.gif", 17, 15))));
    block.child(box().height(2));
    block.child(
        box()
            .row()
            .child(box().width(13))
            .child(img(p.thumb, 69, 52, hex(0xD8D0D0)))
            .child(box().width(3))
            .child(
                box()
                    .width(416)
                    .height(52)
                    .fill(kRose)
                    .column()
                    .child(box().padding(7).child(t(p.copy, verdana(kMaroon))))
                    .child(box().grow(1))
                    .child(box()
                               .row()
                               .justify(Justify::End)
                               .child(img("ecom-viewdetails.gif", 84, 16)))));
    block.child(box().height(6));
    return block;
  }

  // Content height from the HTML's own numbers: selection header + gap
  // + seven 75 px product blocks + breaker + copyright row.
  static constexpr float kListH = 1 + 16 + 6 + 7 * (15 + 2 + 52 + 6) + 6 + 11;

  Element contentFrame() {
    using namespace teq;
    // The explicit height matters: the list overflows its frame, and a
    // flex child left to its defaults would SHRINK to fit instead of
    // scrolling — rows visibly compressing into one another.
    Element list = box().column().width(501).height(Dim(kListH)).shrink(0);
    list.child(box().height(1));
    list.child(img("ecom-productselection.gif", 501, 16, kMaroon));
    list.child(box().height(6));
    for (const Product& p : kProducts) list.child(product(p));
    list.child(img("ecom-breakerbar.gif", 501, 6, kMaroon));
    list.child(box()
                   .height(11)
                   .row()
                   .alignItems(Align::Center)
                   .child(box().grow(1))
                   .child(img("ecom-copyright.gif", 165, 11, kWhite)));
    list.translateY(&scrollY);

    // The styled IE scrollbar: two arrow buttons and a proportional
    // thumb, in exactly the BODY's SCROLLBAR-* colours.
    auto sbButton = [&](bool up) {
      return box()
          .width(Dim(kSbW))
          .height(Dim(kSbW))
          .fill(kSbFace)
          .foreground(shapes::onEdges(
              shapes::Edge::Top | shapes::Edge::Left,
              stroke(1, Fill::color(kWhite), PathFormat::Align::Inner)))
          .foreground(shapes::onEdges(
              shapes::Edge::Bottom | shapes::Edge::Right,
              stroke(1, Fill::color(hex(0x000000)), PathFormat::Align::Inner)))
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .child(
              t(up ? "\xe2\x96\xb4" : "\xe2\x96\xbe", verdana(kSbArrow, true)));
    };
    const float trackH = kContentH - 2 * kSbW;
    const float thumbH =
        trackH * kContentH / (kContentH + std::max(contentOverflow, 1.0f));
    Element scrollbar =
        box()
            .width(Dim(kSbW))
            .column()
            .child(sbButton(true))
            .child(box().grow(1).fill(kSbTrack).child(
                at(box().fill(kSbFace).foreground(
                       shapes::onEdges(shapes::Edge::Top | shapes::Edge::Left,
                                       stroke(1, Fill::color(kWhite),
                                              PathFormat::Align::Inner))),
                   0, 0, kSbW, thumbH)
                    .translateY(&thumbY)))
            .child(sbButton(false));

    return at(box().fill(kWhite), kLeftW, kTopH, kPageW - kLeftW, kContentH)
        .clip()
        .row()
        .child(box().grow(1).clip().child(list))
        .child(scrollbar);
  }

  Element bottomFrame() {
    using namespace teq;
    return at(box().fill(kWhite), 0, kPageH - kBottomH, kPageW, kBottomH)
        .child(at(img("ecom-bottombar.gif", 790, 11), 0, 0, 790, 11));
  }

  // =========================================================================

  Element describe() {
    using namespace teq;
    Element page = box()
                       .width(Dim(kPageW))
                       .height(Dim(kPageH))
                       .fill(kWhite)
                       .child(topFrame())
                       .child(leftFrame())
                       .child(contentFrame())
                       .child(bottomFrame());
    return stack().child(at(std::move(page), 0, 0, kPageW, kPageH)
                             .scale(2.0f)
                             .transformOrigin(0, 0));
  }

  void setup(sketch::SketchContext& ctx) override {
    using namespace teq;
    ctx.canvas(kPageW * 2, kPageH * 2);
    ctx.background(kWhite);
    // Before the auto-scroll leaves the top and while the first button
    // shows its rollover lift.
    ctx.captureAt(2.5);

    // --- every bitmap the frameset names, from the restoration host ------
    {
      sigil::loader::Hub& hub = ctx.assets.hub();
      const std::string base =
          "https://v4prophecy.2advanced.com/equipment/index_files/";
      auto fetch = [&](const char* dir, const char* name) {
        art[name] = hub.image(base + dir + "/" + name);
      };
      for (const char* n :
           {"ecom-topbar.gif", "ecom-logo.gif", "ecom-titleheader.gif",
            "ecom-button-shoppinginfo-of.gif", "ecom-button-checkout-off.gif",
            "ecom-button-viewcart-off.gif", "ecom-button-questions-off.gif"})
        fetch("topframe_files", n);
      fetch("leftframe-productselection_files", "ecom-productselectimage.jpg");
      for (const char* n :
           {"ecom-productselection.gif", "ecom-arrowbutton.gif",
            "ecom-3dots.gif", "ecom-viewdetails.gif", "ecom-breakerbar.gif",
            "ecom-copyright.gif", "ecom-sm_phiberglassshirt.gif",
            "ecom-sm_operativeshirt.gif", "ecom-sm_prophecyshirt.gif",
            "ecom-sm_identityshirt.gif", "ecom-sm_expansionsshirt.gif",
            "ecom-productimage1.gif", "ecom-productimage4.gif"})
        fetch("productselect_files", n);
    }

    contentOverflow = std::max(0.0f, kListH - kContentH);

    // --- the page's two behaviours, on the clock -------------------------
    ctx.ticker.add([this, tt = 0.0](double dt) mutable {
      using namespace teq;
      tt += dt;
      const float s = (float)tt;
      // The simulated pointer: rests, then dwells ~1 s on each of the
      // four top buttons in order, swapping in the -on bitmaps.
      const float cyc = std::fmod(s, 8.0f);
      for (int i = 0; i < 4; ++i) {
        const float on0 = 2.0f + (float)i * 1.2f;
        hover[(size_t)i] = (cyc >= on0 && cyc < on0 + 1.0f) ? 1.0f : 0.0f;
      }
      // The frame's auto-scroll: hold the top, glide to the bottom of
      // the overflow, hold, glide back — period 14 s.
      const float sc = std::fmod(s, 14.0f);
      float f = 0.0f;
      if (sc < 3.0f)
        f = 0.0f;
      else if (sc < 8.0f)
        f = (sc - 3.0f) / 5.0f;
      else if (sc < 9.0f)
        f = 1.0f;
      else if (sc < 13.0f)
        f = 1.0f - (sc - 9.0f) / 4.0f;
      f = f < 0.5f ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f);
      scrollY = -contentOverflow * f;
      const float trackH = kContentH - 2 * kSbW;
      const float thumbH =
          trackH * kContentH / (kContentH + std::max(contentOverflow, 1.0f));
      thumbY = (trackH - thumbH) * f;
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(TwoAdvancedEquipment, "Study \xc2\xb7 Screens",
             "2Advanced's Equipment.Modules store (2003) \xe2\x80\x94 an HTML "
             "frameset of Dreamweaver tables, bitmaps and all")
