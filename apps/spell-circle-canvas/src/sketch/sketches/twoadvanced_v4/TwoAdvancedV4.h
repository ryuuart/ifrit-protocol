#pragma once

#include "Settings.h"

struct TwoAdvancedV4 : sketch::Sketch {
  /** THE PRODUCTION GIFS ARE RUNTIME DATA, and a sketch over runtime data
   *  a machine may not have says so rather than drawing a second picture
   *  under the same name. The rails, the page ground, the footer strip
   *  and the logo bug come off 2Advanced's own restoration host through
   *  SigilIO's https path, which caches on disk; every use site keeps
   *  its procedural stand-in, so a cold cache still renders — but it
   *  renders REBUILT chrome, and the plate this sketch is judged on is
   *  then not the picture the header describes. */
  static bool available(std::string* why);

  // --- bound outputs: every idle motion is DECLARED, none re-describes ---
  ch::Output<float> stripePan{0.0f};    // hazard-stripe conveyor
  ch::Output<float> portalGlow{54.0f};  // MAINFRAME portal glow radius
  ch::Output<float> pressScroll{0.0f};  // PRESS UPDATES auto-scroll
  float pressOverflow = 0;              // entry list minus well, measured once
  ch::Output<float> vuLeft{0.4f}, vuRight{0.6f};
  std::array<ch::Output<float>, 21> dot{};  // 7 clusters × 3 dots
  std::array<ch::Output<float>, 3> gauge{};
  std::array<ch::Output<float>, 3> gaugeAlpha{};

  // --- generated materials, HELD so their identity prunes across renders ---
  Pattern hazard;          // baked 45° stripe tile — the STATIC reuse path
  Pattern hatchA, hatchB;  // footer-dock crosshatch (two passes = crosshatch)
  Pattern dither;          // teal readout-box dither
  mskia::Paint grain;      // page-background film grain (luminance, not RGB)
  mskia::Paint spectrum, stripesLive, waterStreaks;

  // --- the production shell artefacts, fetched from the restoration host.
  // Any of these may be null (no network, cold cache); every use site
  // keeps its procedural stand-in for exactly that case.
  std::shared_ptr<const sigil::image::ImageAsset> railLeftGif, railRightGif;
  std::shared_ptr<const sigil::image::ImageAsset> siteBgGif;   // 1×1600 ramp
  std::shared_ptr<const sigil::image::ImageAsset> footerGif;   // 970×110
  std::shared_ptr<const sigil::image::ImageAsset> logoBugSvg;  // circular 2A

  /** A bitmap stretched to exactly (w, h) — how every shell GIF is
   *  placed: the 2004 page scaled them with IMG width/height attributes,
   *  and this sketch is a ×2 enlargement of those numbers. */
  static mskia::Paint stretchFill(
      const std::shared_ptr<const sigil::image::ImageAsset>& asset, float w,
      float h, SkTileMode tx = SkTileMode::kClamp);

  // --- instancing: the footer dock's chevron tick array ---
  std::shared_ptr<instancing::Atlas> dockAtlas;
  std::shared_ptr<instancing::Pool> dockPool;

  /** The MAINFRAME hero, rendered once as a world scene and held. */
  sk_sp<SkImage> heroPlate;

  int bootPct = 0;
  bool booted = false;

  // --- the section cycle: the GLOBAL NAVIGATOR walked in order ----------
  // The SWF changed sections by collapsing the MAINFRAME viewport behind
  // sliding panels and a loading readout, then reopening on the new
  // content. The cycle here replays that transition grammar on the nav
  // taxonomy: shutters close L→R, the ACCESSING readout flashes up, the
  // shutters reopen — while the selection mark glides to the next item.
  static constexpr const char* kNavItems[7] = {
      "COMPANY",      "SERVICES",  "PORTFOLIO", "ACCOLADES",
      "EXPERIMENTAL", "EQUIPMENT", "CONTACT"};
  static constexpr tav::SectionCycle kCycle{
      .start = 8.0, .hold = 4.9, .transition = 0.9, .stops = 7};
  std::array<ch::Output<float>, 6> shutter{};  // per-slat cover fraction
  ch::Output<float> shutterInfo{0.0f};         // ACCESSING plate opacity
  ch::Output<float> navIndX{0.0f};             // selection mark X offset
  int mfSection = -2;                          // section in the readout

  /** Nav item i's centre inside the 584-wide bar (SpaceEvenly over the
   *  572 inner px), as the translateX for a 24-wide mark at left 0. */
  static float navMarkX(int i) {
    return 6.0f + 572.0f * ((float)i + 0.5f) / 7.0f - 12.0f;
  }
  /** The section the cycle rests on after `stop` changes; the interface
   *  opens on PORTFOLIO and walks onward from there. */
  static int cycleTarget(int stop) { return (3 + stop) % 7; }

  // =========================================================================
  // Live materials (SkSL).

  /** The audio spectrum: a bar field whose heights are hashed per column
   *  per TIME STEP. uTime arrives quantized at 10 Hz (quantizeTime), so the
   *  bars STEP rather than slide — the era's digital readout feel, and the
   *  same reason a stylised meter animates on a beat instead of smoothly. */
  static sk_sp<SkRuntimeEffect> spectrumFx();

  /** The diagonal hazard stripe as a LIVE material so every header bar can
   *  run its slow conveyor pan — 20 px per 8 s — off ONE bound uniform.
   *  This exact value is reused by the nav bar and four panel headers. */
  static sk_sp<SkRuntimeEffect> stripeFx();

  /** Horizontal streak water, dark teal-black, drifting slowly. */
  static sk_sp<SkRuntimeEffect> waterFx();

  // =========================================================================
  // Small parts.

  Element tickDots(int cluster, SkColor4f c = tav::kCyan);

  /** The two-weight panel header: heavy half + regular half over the
   *  hazard-stripe ground, with a flavour line, a tick cluster and a tick
   *  rail. Four panels wear this identically. */
  Element panelHeader(const char* boldHalf, const char* restHalf,
                      const char* flavor, int cluster);

  /** CTA: chamfered, blood-red ramp in UNIT space so the gradient follows
   *  whatever height the layout hands the button, gloss band on top. */
  Element cta(const char* lbl, float w = 116, float h = 34,
              SkColor4f hairline = tav::kNear);

  /** A dark readout window: chamfered, inset-bevelled, bracketed. */
  Element readout(float w, float h, SkColor4f ground = hexColor(0x1B0708));

  /** A radar wedge: shapes::sector, rotation BOUND. Every gauge on the
   *  page is this with a different bezel. */
  Element radarSweep(int i, SkColor4f tint, float inner = 0.30f);

  // =========================================================================
  // Regions.

  Element statusBar();

  Element audioModule();

  Element navBar();

  Element masthead();

  // ---- the hero -----------------------------------------------------------

  /** Composite ORDER is what sells this: the baked render → the horizon
   *  haze → the portal's glow → the orbital ring → the water's light →
   *  the horizon hairline. `still` builds the same scene with no live
   *  material and no entrance, so the bloom duplicate stays provably
   *  static and bakes ONCE. */
  // =========================================================================
  // THE MAINFRAME HERO IS A REAL 3D RENDER, BAKED ONCE.
  //
  // The site's own press copy name-checks Maxon, and the reference is a
  // Cinema 4D composite: mechanical pods rising out of water in front of
  // a teal city, with depth and a bright halo behind them. Flat SDF
  // shapes could carry the vocabulary and not the depth, so the city and
  // the pods are a world scene here — lit bodies, a real camera, real
  // occlusion — rendered ONCE into an image at setup and composited into
  // the page like any other bitmap.
  //
  // IT STAYS A CANVAS SKETCH and does not become a set. The page is a
  // DOCUMENT: resolution-independent, so its plate is re-rendered at the
  // capture scale, and every panel on it is 2D chrome. A set forms at one
  // resolution and would drag two thousand lines of chrome through a
  // texture for the sake of one panel. The hero is a picture INSIDE the
  // page, so it is a picture.
  //
  // The bake is at twice the panel's pixels, because a plate is taken at
  // up to twice the canvas and a hero resampled up is the one thing a
  // rebuild of this page cannot afford.

  /** A deterministic value in [0,1) — the skyline is the same skyline on
   *  every machine and in every run. */
  static float cityHash(int i, int salt) {
    uint32_t h = (uint32_t)i * 2654435761u ^ (uint32_t)salt * 2246822519u;
    h = (h ^ (h >> 15U)) * 2654435761u;
    return (float)((h ^ (h >> 16U)) & 0xFFFFFFu) / 16777216.0f;
  }

  /** One axis-aligned block, five faces (nothing sees the underside),
   *  flat normals, one colour in the vertex lane. */
  static void cityBlock(sigil::geometry::mesh::Mesh& out, glm::vec3 lo,
                        glm::vec3 hi, glm::vec4 tint);

  /** The city: four ranks of blocks receding from the water line, each
   *  rank a step darker and bluer, which is the fog this renderer has —
   *  a colour that is a function of distance, written into the vertices. */
  static sigil::geometry::mesh::Mesh cityMesh();

  /** The backdrop: one quad whose four corners carry the ramp, so the
   *  sky is a body the scene sorts like any other. */
  static sigil::geometry::mesh::Mesh skyMesh();

  /** One pod: a dome on the water with a dark visor band and a lit maw. */
  static world::Element pod(const std::string& key, float x, float z,
                            float scale);

  /** The hero's world, PHOTOGRAPHED ONCE into an image @p w x @p h. The
   *  set is built here and handed to the host, which stands the frame in
   *  a scene of its own for the length of one call and gives back a
   *  picture rather than a view onto something still standing. */
  sk_sp<SkImage> bakeHero(int w, int h, sketch::SketchContext& ctx);

  Element heroScene(float w, float h, bool still);

  Element hero(float w, float h);

  Element mainframe();

  Element mfLoadReadout(int section);

  // ---- teal monitor panels ------------------------------------------------

  Element monitorBody(float h);

  /** Four procedural "stills" — each a different flat-shape composition
   *  over the same portal recipe, tinted per index. */
  std::vector<Element> relatedStills();

  /** A label-over-value pair on a teal panel — the tabular voice. */
  Element specPair(const char* k, const char* v);

  Element featureSystem();

  /** The entry column alone, so setup() can measure its laid-out height
   *  against the well and derive the real scroll overflow. kPressWellW is
   *  the width the entries wrap at inside the well: the 694 monitor body
   *  less its two 11 px paddings, the 8 px gap, the 16 px scrollbar and
   *  the clip's two 9 px paddings. */
  static constexpr float kPressWellW = 694 - 2 * 11 - 8 - 16 - 2 * 9;
  /** The row the well and its bar share: 376 less the two 11 px paddings,
   *  the 9 px column gap and the 34 px footer row. */
  static constexpr float kPressRowH = 376 - 2 * 11 - 9 - 34;
  /** The viewport the entries scroll through — the row less the clip's
   *  two 9 px paddings. */
  static constexpr float kPressWellH = kPressRowH - 2 * 9;
  /** And the track the thumb runs in: the row less its two 16 px
   *  steppers and the two 3 px gaps that stand them off. */
  static constexpr float kPressTrackH = kPressRowH - 2 * 16 - 2 * 3;

  /** WHAT THE PRESS WELL SCROLLS, as the one reading the bar and the
   *  clock both take: the entries measured against the well in setup, and
   *  the track the thumb says their share in. */
  sketch::kit::Scrolled pressScrolled() const {
    return {.view = kPressWellH,
            .content = kPressWellH + pressOverflow,
            .track = kPressTrackH};
  }
  Element pressList();

  Element pressUpdates();

  /** A module's red title bar inside AUXILIARY PANEL. */
  Element auxBar(const char* label);

  /** The teal full-width VIEW bar the two right modules end on. */
  Element auxView();

  Element auxiliary();

  /** A tiny chamfered radio key — the SUB SYSTEM row's preference bank. */
  Element toggle(const char* lbl, bool on);

  std::vector<Element> footerLinks();

  Element subSystem();

  Element legalStrip();

  /** The dock's oscilloscope bars — monochrome oxblood, like the rest of
   *  sitefooter.gif. Every height is a closed-form function of the bar
   *  index and never of time, so the whole strip stays picture-cached. */
  std::vector<Element> dockBars();

  /** The 970×110 sitefooter.gif, ×2 — deliberately MONOCHROME oxblood,
   *  no accent colour anywhere (the real asset saves colour for the SWF's
   *  live states). Crosshatch dither, bracketed windows, chevrons, and the
   *  three-circle gauge cluster in its dark bezel. */
  Element footerDock();

  Element rail(bool right);

  // ---- boot overlay: dot, reticle, percentage, flash ----------------------

  Element bootOverlay();

  Element bootReadout();

  // =========================================================================

  /** THE PAGE: one grid, the panels on it, the strip underneath. */
  Element describe();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  void update(double elapsed, sketch::SketchContext& ctx) override;
};
