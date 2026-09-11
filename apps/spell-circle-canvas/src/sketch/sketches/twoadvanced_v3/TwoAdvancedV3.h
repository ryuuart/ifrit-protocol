#pragma once

#include "Settings.h"

struct TwoAdvancedV3 : sketch::Sketch {
  /** THE PRODUCTION ART IS RUNTIME DATA, and a sketch over runtime data a
   *  machine may not have says so rather than drawing a second picture
   *  under the same name. Every bitmap on this page comes off the
   *  studio's own host through SigilIO's https path, which caches on
   *  disk; the use sites all keep a procedural stand-in, so a cold cache
   *  renders — but it renders the STAND-IN page, and the plate this
   *  sketch is judged on is then not the picture the header describes.
   *  Two plates, one name is the one thing a byte-identity sweep cannot
   *  survive.
   *
   *  So the probe asks SigilIO's own cache what is on disk. A machine
   *  that has fetched once is available forever after and offline; a
   *  machine that never has is UNAVAILABLE by name, with the first
   *  missing URL as the reason, and the ledger stands it down rather
   *  than hashing a different page. */
  static bool available(std::string* why);

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
  /** The sky footage's grade — a saturation-and-gain matrix, built once
   *  per declaration and held HERE. A function-local static would outlive
   *  the dylib a hot-reloaded sketch is compiled into, and a filter
   *  compared by pointer after that reload points into code that is
   *  gone. */
  sk_sp<SkImageFilter> cloudLook;
  Pattern dots;    // the dot-matrix filling every title bar's right half
  Pattern vticks;  // the tick-dash rail under the navbar

  int cloudFrame = -1;
  int discordFrame = -1;
  int bootPct = 0;
  bool booted = false;

  /** A bitmap stretched to exactly (w, h). */
  static mskia::Paint stretchFill(const ImagePtr& asset, float w, float h,
                                  SkTileMode tx = SkTileMode::kClamp,
                                  SkTileMode ty = SkTileMode::kClamp);

  // =========================================================================
  // The sky opening's coverage, baked once: the outline TRACED from the
  // plate (vertices in the bitmap's own 1277×385 pixels — the beam edge,
  // the right arch's inner edge down, the left arch's inner edge back
  // up), filled white through a blur mask filter. Feathering the
  // COVERAGE is what melts the overlay's boundary into the art while the
  // footage inside stays crisp — blurring the layer itself would smear
  // the clouds.

  /** The sky footage's grade: 0.60 saturation at 0.90 gain, so the
   *  62-frame loop sits in the plate's own exposure rather than in its
   *  own. */
  static sk_sp<SkImageFilter> buildCloudLook();

  void buildGapMask();

  // =========================================================================
  // Lifting the art out of the Rive file. A .riv stores each embedded
  // image asset as its name followed by the raw PNG, and SigilImage's
  // signature scan recovers both, so all this study states is which asset
  // name fills which slot.

  static int sectionFor(const std::string& name) {
    for (int s = 0; s < 6; ++s)
      if (name == kSections[s].asset) return s;
    return -1;
  }

  void extractRivImages(const std::vector<std::byte>& bytes);

  // =========================================================================
  // Regions, top to bottom.

  /** The steel module title bar every lower panel wears: glyph chip,
   *  tracked caps title, the dot-matrix field filling the right half,
   *  and the divider dots at the far end. */
  Element moduleBar(const char* glyph, const char* label, float w);

  /** The recessed steel button ("VISIT RIVE", "SUBMIT", …). */
  Element button(const char* label, float w, float h = 24);

  /** The little segmented load meter that trails the CTAs. */
  Element meter(int lit);

  Element bevelBar();

  Element headerStrip();

  Element wordmark();

  Element navBar();

  /** The tab row at `active` (-1 = home, nothing lit). The indicator is
   *  the riv's "nav indicator mark": a bright underline bar. */
  Element navTabs(int active);

  /** The two hairline bars between navbar and stage art. */
  Element hairlines();

  /** The stage viewport. Its CONTENT lives in a slot — the section
   *  cycle swaps art through it — while the entrance wipe (the riv's
   *  "MASK: white rec swipe" gesture) stays out here and plays once. */
  Element stageArt();

  /** One section's stage art at full opacity: the bitmap, and for the
   *  numbered sections a title spreader, sub-nav tabs and the RETURN TO
   *  MAIN affordance. `sec` < 0 is home. `settle` ∈ [0,1] drives the
   *  title spreader: tracking-in from wide, the riv's "title spreader". */
  Element sectionArt(int sec, float settle);

  /** The section change: the riv's "Section Transition Effect" — the
   *  incoming art claims the viewport behind a straight edge that jumps
   *  in FOURTEEN discrete steps ("shape trans step 1".."14"), with a
   *  bright leading band at the edge. Quantized on purpose: the smooth
   *  version reads as a crossfade, the stepped one as machinery. */
  Element transitionArt(int fromSec, int toSec, int step);

  Element scrollStrip();

  /** One lower module: title bar + bordered translucent body. */
  Element module(const char* glyph, const char* barLabel, Element body,
                 int order);

  /** The framed thumb plate the two outer modules share: a mini toolbar
   *  strip up top, a dark viewport with its top-right corner cut at 45°,
   *  and the labelled plate attached beneath with the mirrored cut. */
  Element thumbPlate(Element content, const char* btn);

  /** The lockup inside FEATURED.PARTNER's plate: 2a mark + R mark. */
  Element riveLockup();

  Element featuredPartner();

  Element subData();

  Element updates();

  Element mailingList();

  Element support2a();

  Element follow2a();

  Element footerRail();

  // ---- boot overlay: the site's own preloader page, compressed ------------
  // Its stylesheet is the spec: page #2A3753; the stacked lockup
  // (2a-logo@2x.png, drawn 197×94, inverted to white); a tracked
  // #7183A5 subtitle; and a HUGE #7183A5 percentage below — the real
  // page sets it at 200 px.

  Element bootOverlay();

  Element bootReadout();

  ch::Output<float> beaconAlpha{1.0f};

  // =========================================================================

  Element describe();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  // Slot content must carry its own dims: a slot node sizes from its
  // content, so an inset()-positioned filler inside one measures zero.
  void renderCloudFrame(sketch::SketchContext& ctx, int frame);

  void renderDiscordFrame(sketch::SketchContext& ctx, int frame);

  // --- the section cycle -----------------------------------------------
  // The home view holds until the cycle starts, then the simulated visitor
  // walks every tab in order and returns to main — each change playing
  // the stepped shape-wipe. All of it is a pure function of the clock,
  // so captures land on the same frame every run.
  static constexpr tv3::SectionCycle kCycle{
      .start = 8.0, .hold = 5.0, .transition = 0.7, .stops = 7};
  int stageSec = -1;   // section the stage currently shows (-1 = home)
  int stageStep = -1;  // -1 stable, else the transition step shown
  int navActive = -2;  // tab lit in the nav slot (-1 = none), -2 = unset

  static int stopTarget(int stop) { return stop < 6 ? stop : -1; }

  void renderStage(sketch::SketchContext& ctx, const Element& content,
                   bool hasHome);

  void renderNavTabs(sketch::SketchContext& ctx, int active) {
    navActive = active;
    ctx.composer.renderSlot("navtabs", navTabs(active));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override;
};
