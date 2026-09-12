// The 2Advanced expansion site: a page assembled from production images and
// reusable modules.

// TAGS: Interfaces/Web

#include "TwoAdvancedV3.h"

Element TwoAdvancedV3::describe() {
  using namespace tv3;
  Element page = stack();
  if (pageTile) {
    // The 10×1600 strip exactly as the CSS places it: repeated across,
    // clamped down (the page is shorter than the strip).
    page.fill(stretchFill(pageTile, 10, 1600, SkTileMode::kRepeat));
  } else {
    page.fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
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
  Element ground = at(box().clip(), kStageX, 640, kStageW, 400);
  if (lowerPanelBg)
    ground.fill(stretchFill(lowerPanelBg, kStageW, 400));
  else
    ground.fill(mskia::Paint::linearUnit(
        {0, 0}, {1, 1}, {{0.0f, hexColor(0x22304A)}, {1.0f, kPage}}));
  ground.opacity(
      animate(motion::from(0.0f).to(1.0f), {380ms, &ch::easeOutQuad, 2250ms}));
  page.child(ground);

  Element mods = at(box().row().gap(10), kStageX, kModY, kStageW, kModH);
  mods.child(featuredPartner()).child(subData()).child(updates());
  page.child(mods);
  // the dark divider band that closes the module row
  page.child(at(box().fill(mskia::withAlpha(hexColor(0x26314A), 0.9f)), kStageX,
                kModY + kModH + 2, kStageW, 8)
                 .opacity(animate(motion::from(0.0f).to(1.0f),
                                  {320ms, &ch::easeOutQuad, 2650ms})));

  Element row = at(box().row().gap(10), kStageX, kRowY, kStageW, kRowH);
  row.child(mailingList()).child(support2a()).child(follow2a());
  page.child(row);

  page.child(footerRail());
  page.child(bootOverlay());
  return page;
}

void TwoAdvancedV3::setup(sketch::SketchContext& ctx) {
  using namespace tv3;
  // Everything has entered by ~3.3 s; 7.6 s puts the discord orb loop
  // (20 fps, 102 frames) on its bright crest, frame 50.
  sketch::kit::stage(
      ctx,
      {.size = SkSize::Make(kW, kH), .captureAt = 7.6, .background = kPage});

  diag =
      patterns::stripes(2, 9, mskia::toColor(mskia::withAlpha(kSteelHi, 0.5f)));
  diag.rotate(45);
  dots = patterns::halftone(5, 1.3f,
                            mskia::toColor(mskia::withAlpha(kInk, 0.55f)));
  vticks = patterns::stripes(1.5f, 5.5f,
                             mskia::toColor(mskia::withAlpha(kSteelHi, 0.5f)));

  // --- the production assets, from the live site ------------------------
  // https fetches cache on disk (CacheFirst): the first run downloads,
  // every later run is served locally; a failed fetch leaves the
  // pointer null and the use site builds its steel stand-in.
  {
    sigil::io::Hub& hub = ctx.assets.hub();
    const std::string site = "https://v3.2advanced.com/";
    pageTile = hub.image(site + "V3ExpansionsReboot/assets/background.gif");
    socialSprite = hub.image(site +
                             "V3ExpansionsReboot/assets/images/social-icons"
                             "@2x.png");
    logoMark =
        hub.image(site + "v3expansionsreboot/assets/2advancedLogo_Preload.svg",
                  {.width = 120});
    pageLogo =
        hub.image(site + "V3ExpansionsReboot/assets/images/2a-logo@2x.png");
    if (auto blob = hub.blob(site + "v3expansionsreboot/mainstage.riv"))
      extractRivImages(blob->bytes);
    buildGapMask();
    cloudLook = buildCloudLook();
  }

  // --- idle motion ------------------------------------------------------
  ctx.ticker.add([this, &ticker = ctx.ticker](double) {
    const double tAcc = ticker.elapsed();
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

void TwoAdvancedV3::renderCloudFrame(sketch::SketchContext& ctx, int frame) {
  using namespace tv3;
  cloudFrame = frame;
  ctx.composer.renderSlot("clouds",
                          box().width(310).height(255).fill(
                              stretchFill(clouds[(size_t)frame], 310, 255)));
}

void TwoAdvancedV3::renderDiscordFrame(sketch::SketchContext& ctx, int frame) {
  using namespace tv3;
  discordFrame = frame;
  ctx.composer.renderSlot("discord",
                          box().width(64).height(64).fill(
                              stretchFill(discordSeq[(size_t)frame], 64, 64)));
}

void TwoAdvancedV3::renderStage(sketch::SketchContext& ctx,
                                const Element& content, bool hasHome) {
  ctx.composer.renderSlot("stage", content);
  // The clouds slot was just rebuilt empty inside fresh home art —
  // re-push the current frame so the sky never blanks for a frame.
  if (hasHome && !clouds.empty())
    renderCloudFrame(ctx, std::max(0, cloudFrame));
}

void TwoAdvancedV3::update(double elapsed, sketch::SketchContext& ctx) {
  using namespace tv3;
  // The section cycle state, then the frame sequences — everything on
  // the DATA path, each slot swap touching only its own subtree.
  int sec = -1, step = -1, from = -1;
  const tv3::SectionCycle::At now = kCycle.at(elapsed);
  if (now.running) {
    sec = stopTarget(now.stop);
    from = now.previous < 0 ? -1 : stopTarget(now.previous);
    step = tv3::SectionCycle::step(now.phase, 14);
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

SIGIL_SKETCH(TwoAdvancedV3, "Study \xc2\xb7 Screens",
             "2Advanced Studios V3 Expansions Reboot (2024) \xe2\x80\x94 the "
             "production art, lifted from the live site's own Rive file")
