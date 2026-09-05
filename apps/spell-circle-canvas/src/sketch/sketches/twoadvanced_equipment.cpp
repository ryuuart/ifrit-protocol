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
//     and wrap beneath it. MM_preloadImages names an -on.gif for each and
//     MM_swapImage swaps src on mouseover; the archive never captured
//     those states, so the hover cycle below approximates them.
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
// SigilIO's https path (disk-cached after the first run); a missing
// fetch leaves a flat #7C252C or white stand-in so the sketch still
// renders offline. But a stand-in page is not the page this header
// describes, so `available()` asks SigilIO's cache first and stands
// the sketch down BY NAME on a machine that has never fetched, rather
// than publishing a second picture under the same one. The type is
// Verdana at HTML size=1 — 10 px — which macOS ships.
//
// The page is STATIC; its only behaviours are the JS rollovers and the
// frame's scrollbar, so those are the only motion here: a simulated
// pointer walks the four top buttons, and the content frame auto-scrolls
// its overflow with the thumb tracking in the styled scrollbar. The
// rollover is a LIFT over the -off bitmap rather than a swap, because
// the -on.gif files were only ever fetched on hover and the archive
// therefore never captured one. Frame is drawn at ×2 of the 790×580
// page: 1580×1160.
//
// EDIT THESE FIRST
//   the 14 s scroll envelope's four corners — hold, glide, hold, glide
//                       back. The declared moment sits in the first hold.
//   the 8 s hover cycle and its 1 s dwell — when each of the four
//                       buttons lights, and for how long.
//   kProducts           — the seven rows. Everything below the header is
//                       laid out from them.
//   the palette block   — the page's own attribute colours.

#include <shared/TwoAdvanced.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <array>
#include <boost/container/flat_map.hpp>
#include <cmath>
#include <string>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace weave = sigil::weave;

using namespace sigil::compose;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
namespace ch = choreograph;

namespace teq {
using namespace twoadvanced;

// The page's entire palette, straight from its attributes.
constexpr SkColor4f kMaroon = hex(0x7C252C);   // header rows, copy, links
constexpr SkColor4f kRose = hex(0xF0E7E8);     // description cells
constexpr SkColor4f kWhite = hex(0xFFFFFF);    // BODY bgColor
constexpr SkColor4f kSbFace = hex(0xBBC0C9);   // SCROLLBAR-FACE-COLOR
constexpr SkColor4f kSbTrack = hex(0xE4E6EA);  // SCROLLBAR-TRACK-COLOR
constexpr SkColor4f kSbArrow = hex(0x666666);  // SCROLLBAR-ARROW-COLOR

/** Verdana at HTML size=1: 10 px — the one register the whole store is
 *  set in, bold only in the product headers. Untracked, because an HTML
 *  table cell had no way to say otherwise. */
inline sigil::weave::TextStyle verdana(SkColor4f color, bool bold = false) {
  return sigil::weave::kit::tracked(verdanaFace(bold), 10, color);
}

// Frameset geometry, in the page's own CSS pixels.
constexpr float kPageW = 790, kPageH = 580;
constexpr float kTopH = 103, kBottomH = 15;
constexpr float kLeftW = 262;
constexpr float kContentH = kPageH - kTopH - kBottomH;  // the * row
constexpr float kSbW = 16;                              // IE scrollbar

// The two behaviours, as the numbers they are made of.
constexpr float kHoverCycle = 8.0f;    // the pointer's round of the four
constexpr float kHoverFirst = 2.0f;    // when the first button lights
constexpr float kHoverStep = 1.2f;     // and how far behind the next one is
constexpr float kHoverDwell = 1.0f;    // how long each stays lit
constexpr float kScrollCycle = 14.0f;  // hold, glide down, hold, glide back
constexpr float kScrollRise = 3.0f, kScrollHold = 8.0f;
constexpr float kScrollFall = 9.0f, kScrollRest = 13.0f;

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
  /** THE STORE'S BITMAPS ARE RUNTIME DATA, and a sketch over runtime data
   *  a machine may not have says so rather than drawing a second picture
   *  under the same name. Every GIF and JPEG on this frameset comes off
   *  the restoration host through SigilIO's https path, which caches
   *  on disk; `img()` keeps a flat maroon-or-white stand-in at every use
   *  site, so a cold cache still renders — but it renders the STAND-IN
   *  page, and the plate this sketch is judged on is then not the picture
   *  the header describes.
   *
   *  Four files stand for the sixteen: the top bar and the logo carry the
   *  masthead, the product-selection image is the whole left frame, and
   *  the first thumbnail is the row art. A cache holding those was
   *  written by a run that fetched them all. */
  static bool available(std::string* why) {
    return sketch::requireCached(
        {"https://v4prophecy.2advanced.com/equipment/index_files/"
         "topframe_files/ecom-topbar.gif",
         "https://v4prophecy.2advanced.com/equipment/index_files/"
         "topframe_files/ecom-logo.gif",
         "https://v4prophecy.2advanced.com/equipment/index_files/"
         "leftframe-productselection_files/ecom-productselectimage.jpg",
         "https://v4prophecy.2advanced.com/equipment/index_files/"
         "productselect_files/ecom-sm_phiberglassshirt.gif"},
        why);
  }

  using ImagePtr = std::shared_ptr<const sigil::image::ImageAsset>;

  // Keyed by file name under equipment/index_files/.
  boost::container::flat_map<std::string, ImagePtr, std::less<>> art;

  /** THE ONLY THING THE CLOCK WRITES. Both of the page's behaviours are
   *  periodic shapes of the elapsed seconds, so each is declared as a
   *  bound envelope off this one Output and nothing per-frame computes a
   *  position. */
  ch::Output<float> clock{0.0f};

  float contentOverflow = 0;

  /** THE THUMB'S LENGTH, which is the frame's share of what it scrolls —
   *  stated once, because the scrollbar draws it and the clock places it
   *  and the two disagreeing is a thumb that slides off its own track. */
  /** THE PAGE'S SCROLL, as one envelope: flat at the top, a glide down
   *  over five seconds, a beat at the bottom, and four seconds back. The
   *  corners are positions in the cycle; the quadratic ease rounds both
   *  shoulders without moving them. */
  motion::Bound scrollEnvelope() const {
    using namespace teq;
    return motion::bind(&clock)
        .source(0.0f, kScrollCycle)
        .trapezoid(kScrollRise / kScrollCycle, kScrollHold / kScrollCycle,
                   kScrollFall / kScrollCycle, kScrollRest / kScrollCycle)
        .map(ch::easeInOutQuad);
  }

  float thumbHeight() const {
    using namespace teq;
    return (kContentH - 2 * kSbW) * kContentH /
           (kContentH + std::max(contentOverflow, 1.0f));
  }

  /** The bitmap at its own HTML display size, or a flat stand-in. An
   *  IMG with WIDTH and HEIGHT stretches to them — the page states both
   *  on every one of its bitmaps, and half of them are stated at
   *  something other than the file's own size. */
  Element img(const char* name, float w, float h,
              SkColor4f fallback = teq::kMaroon) {
    auto it = art.find(name);
    if (it == art.end() || !it->second)
      return box().width(Dim(w)).height(Dim(h)).shrink(0).fill(fallback);
    return image(it->second).width(Dim(w)).height(Dim(h)).shrink(0);
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
      // The dwell: one second lit out of every eight, the four starting
      // 1.2 s apart, so the pointer walks the row.
      const float on0 = teq::kHoverFirst + (float)i * teq::kHoverStep;
      f.child(at(box().fill(alpha(kWhite, 0.4f)), x, 82, w, 11)
                  .opacity(motion::bind(&clock)
                               .source(on0, on0 + teq::kHoverCycle)
                               .square(teq::kHoverDwell / teq::kHoverCycle)));
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
    list.translateY(scrollEnvelope().target(0.0f, -contentOverflow));

    // The styled IE scrollbar: two arrow buttons and a proportional
    // thumb, in exactly the BODY's SCROLLBAR-* colours.
    auto sbButton = [&](bool up) {
      return box()
          .width(Dim(kSbW))
          .height(Dim(kSbW))
          .fill(kSbFace)
          .foreground(
              onEdges(path::Edge::Top | path::Edge::Left,
                      stroke(1, Fill::color(kWhite), PathFormat::Align::Inner)))
          .foreground(onEdges(
              path::Edge::Bottom | path::Edge::Right,
              stroke(1, Fill::color(hex(0x000000)), PathFormat::Align::Inner)))
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .child(
              t(up ? "\xe2\x96\xb4" : "\xe2\x96\xbe", verdana(kSbArrow, true)));
    };
    const float trackH = kContentH - 2 * kSbW;
    const float thumbH = thumbHeight();
    Element scrollbar = box()
                            .width(Dim(kSbW))
                            .column()
                            .child(sbButton(true))
                            .child(box().grow(1).fill(kSbTrack).child(
                                at(box().fill(kSbFace).foreground(onEdges(
                                       path::Edge::Top | path::Edge::Left,
                                       stroke(1, Fill::color(kWhite),
                                              PathFormat::Align::Inner))),
                                   0, 0, kSbW, thumbH)
                                    .translateY(scrollEnvelope().target(
                                        0.0f, trackH - thumbH))))
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
    // The plate at exactly 2x. One page pixel is two canvas px and four
    // device px, so the 10 px Verdana and every GIF edge land whole.
    // Before the auto-scroll leaves the top and while the first button
    // shows its rollover lift.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kPageW * 2, kPageH * 2),
                             .captureAt = 2.5,
                             .background = kWhite,
                             .oversample = 2});

    // --- every bitmap the frameset names, from the restoration host ------
    {
      sigil::io::Hub& hub = ctx.assets.hub();
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

    // --- the clock ---------------------------------------------------
    // Both behaviours are shapes of it, declared where they are drawn, so
    // this is the whole per-frame side of the page.
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double tt = ticker.elapsed();
      clock = (float)tt;
      return true;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(TwoAdvancedEquipment, "Study \xc2\xb7 Screens",
             "2Advanced's Equipment.Modules store (2003) \xe2\x80\x94 an HTML "
             "frameset of Dreamweaver tables, bitmaps and all")
