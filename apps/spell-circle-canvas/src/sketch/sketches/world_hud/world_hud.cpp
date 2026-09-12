// A 3D world with a composited status display and interaction overlays.

// TAGS: Interfaces/Game

#include "Hud.h"

namespace {

struct WorldHud final : sketch::Set {
  // Plain fractions in [0,1], not pixel widths. Every bar here is a
  // full-size fill whose growing edge is pinned with transformOrigin() and
  // whose extent is carried by scaleX, so these Outputs feed the transform
  // directly and none of them needs to know the bar's size.
  choreograph::Output<float> hp{0.62f}, energy{0.78f}, poise{0.55f};
  choreograph::Output<float> xp{0}, enemyHp{0.4f};
  choreograph::Output<float> lowPulse{0}, compass{0};
  std::array<choreograph::Output<float>, 4> cooldown{};

  std::shared_ptr<instancing::Atlas> slotAtlas;
  std::shared_ptr<instancing::Pool> slotPool;

  /** The HUD's own scene, asked of the session once and held by it: a
   *  scene standing on a device destroys the texture it painted into
   *  when it goes, and one asked for per frame would be one held per
   *  frame. */
  std::shared_ptr<compose::TextureScene> overlay;
  Element retained;
  worldhud::gm::camera::Camera lens;

  /** The terrain, cooked once and held for the sketch's life: it is a
   *  function of nothing, and rebuilding twenty thousand triangles per
   *  frame would be a statement about the terrain rather than about the
   *  HUD. Held here and not in a static inside `describe`, since this file
   *  is a dylib a reload unloads and a static outlives the code that
   *  filled it. */
  worldhud::gm::Mesh valley = worldhud::valley();

  void setup(sketch::SetContext& ctx) override {
    namespace wh = worldhud;
    ctx.canvas((int)kSceneSize.fWidth, (int)kSceneSize.fHeight);
    ctx.captureAt(6.0);
    ctx.background({0.086f, 0.118f, 0.165f, 1.0f});
    overlay =
        ctx.textureScene({(int)kSceneSize.fWidth, (int)kSceneSize.fHeight});

    lens.eye = wh::kEye;
    lens.target = wh::kLook;
    lens.up = {0.0f, 1.0f, 0.0f};
    lens.fovYDeg = 46.0f;
    lens.zNear = 8.0f;
    lens.zFar = 8192.0f;
    ctx.camera(lens);

    // The empty slot frame is one atlas cell stamped twelve times.
    slotAtlas = std::make_shared<instancing::Atlas>(2.0f);
    slotAtlas->cell(
        wh::boneFrame(wh::kSlotFrame, wh::kSlotFrame, 3)
            .child(wh::track(wh::kSlot - 4, wh::kSlot - 4).left(5).top(5)),
        {wh::kSlotFrame, wh::kSlotFrame});
    slotPool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < wh::kSlotCount; ++i)
      slotPool->add({arrange::cellRect({i, 0}, {wh::kSlotFrame, wh::kSlotFrame},
                                       {wh::kSlotGap, 0})
                             .fLeft +
                         wh::kSlotFrame * 0.5f,
                     wh::kSlotFrame * 0.5f});
    retained = hud();
  }

  /** THE COMBAT LOOP, as one function of the scene time: health drains,
   *  a heal lands, energy spends and regenerates, poise breaks and
   *  recovers, four abilities cool down on periods of their own. Every
   *  bar on the HUD reads this one clock, so there is one place that
   *  says what any second of the fight looks like. */
  void driveTo(double t) {
    const double cycle = std::fmod(t, 9.0);
    float h = 0.62f;
    if (cycle < 3.0)
      h = 0.62f - 0.42f * (float)(cycle / 3.0);
    else if (cycle < 3.5)
      h = 0.20f + 0.55f * (float)((cycle - 3.0) / 0.5);
    else
      h = 0.75f - 0.13f * (float)((cycle - 3.5) / 5.5);
    hp = h;
    // Veloren's hp_ani: below 20% the bar breathes.
    lowPulse = h < 0.25f ? 0.5f + 0.5f * (float)std::sin(t * 9.0) : 0.0f;
    energy = 0.35f + 0.45f * (float)(0.5 + 0.5 * std::sin(t * 0.9));
    poise = 0.30f + 0.60f * (float)(0.5 + 0.5 * std::sin(t * 0.55 + 1.7));
    compass = (float)std::fmod(t * 8.0, 360.0);
    xp = (float)std::fmod(t * 0.11, 1.0);
    // The boss bar drains on a sawtooth and must never reach zero: a
    // formulation that clamps at empty spends part of every cycle showing
    // a boss nameplate over an unfilled black slab.
    enemyHp = 0.85f - 0.77f * (float)std::fmod(t * 0.16, 1.0);
    for (size_t i = 0; i < cooldown.size(); ++i) {
      const double period = 2.4 + 0.9 * (double)i;
      // the dark cover keeps its top edge and its bottom edge rises
      cooldown[i] = 1.0f - (float)(std::fmod(t, period) / period);
    }
  }

  /** The overlay's quad: it stands a fixed distance in front of the eye
   *  and is exactly as wide and as tall as the frustum is there, so a
   *  texture pixel and a plate pixel are the same pixel.
   *
   *  THE EXTENT IS ASKED OF THE CAMERA, not of the field of view. The two
   *  do not agree: this projection's centre stands a unit behind the eye,
   *  so the frame at a distance is wider than the angle alone makes it,
   *  and a quad sized from `2 d tan(fov/2)` is short of the frustum by
   *  that unit — which is a resample of the whole overlay, the one thing
   *  this quad exists to avoid. */
  world::Element overlayQuad(material::Texture texture) {
    const glm::vec3 forward = glm::normalize(lens.target - lens.eye);
    constexpr float kAt = 60.0f;
    const SkSize frame =
        lens.extentAt(kAt, kSceneSize.fWidth / kSceneSize.fHeight);
    const float h = frame.height(), w = frame.width();
    const glm::vec3 at = lens.eye + forward * kAt;
    material::Material surface =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});
    surface.child(material::kit::kBaseColorSlot, std::move(texture));
    return world::Element()
        .key("overlay")
        .transform(
            ::sigil::geometry::mesh::camera::faceCamera(lens.eye, at, lens.up))
        .mesh(worldhud::gm::quad(w, h))
        .fill(std::move(surface))
        .tag("overlay");
  }

  world::Frame describe(float seconds) override {
    namespace wh = worldhud;
    driveTo((double)seconds);

    world::Element scene = world::Element().key("vale");
    scene.child(world::Element().key("sun").light(world::light::sun(
        {-0.44f, -0.78f, -0.44f}, {1.00f, 0.94f, 0.80f, 1.0f}, 1.05f)));
    scene.child(world::Element().key("sky").light(world::light::sun(
        {
            0.26f,
            0.52f,
            0.36f,
        },
        {0.42f, 0.56f, 0.78f, 1.0f}, 0.42f)));

    scene.child(world::Element()
                    .key("terrain")
                    .mesh(valley)
                    .fill(material::kit::surface(
                        {.baseColor = {1, 1, 1, 1}, .roughness = 0.92f}))
                    .tag("terrain"));

    // THE HUD IS DESCRIBED ONCE. Every bar on it is a bound Output on a
    // retained node, so the frames after the first cost a reconcile
    // against a tree that did not change — re-describing a hundred nodes
    // per frame would be paying for the bindings twice.
    overlay->render(retained, (double)seconds);
    scene.child(overlayQuad(overlay->texture()));
    return world::Frame(std::move(scene));
  }

  // ------------------------------------------------------------------

  Element barStack() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    Element stackEl = stack().inset(0);

    // health, with the decay ghost and the low-HP wash
    stackEl.child(box().left(wh::kBarX).top(wh::kBarY).child(
        wh::bar(wh::kHealthW, wh::kHealthH, wh::kHealthInnerW,
                wh::kHealthInnerH, 0.62f, wh::kHp, 0.14f)));
    // the live fill rides on top of the static frame so only IT repaints
    stackEl.child(box()
                      .left(wh::kBarX + 2)
                      .top(wh::kBarY + 3)
                      .width(Dimension(wh::kHealthInnerW))
                      .height(Dimension(wh::kHealthInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&hp)
                      .fill(Paint::linear({0, 0}, {0, wh::kHealthInnerH},
                                          {{0.0f, hexColor(0x7FE000)},
                                           {0.5f, wh::kHp},
                                           {1.0f, hexColor(0x2F5C00)}})));
    stackEl.child(box()
                      .left(wh::kBarX)
                      .top(wh::kBarY)
                      .width(Dimension(wh::kHealthW))
                      .height(Dimension(wh::kHealthH))
                      .corners({2})
                      .fill(Paint::solid({wh::kCritHp.fR, wh::kCritHp.fG,
                                          wh::kCritHp.fB, 0.55f}))
                      .opacity(&lowPulse)
                      .blend(SkBlendMode::kPlus));
    stackEl.child(text(toUtf8("640 / 1030"), wh::type(11, wh::kInk, 0.8f))
                      .left(wh::kBarX + wh::kHealthW * 0.5f - 30)
                      .top(wh::kBarY + 5));

    // energy
    const float ex = (wh::kW - wh::kEnergyW) * 0.5f;
    stackEl.child(
        box()
            .left(ex)
            .top(wh::kEnergyY)
            .child(wh::bar(wh::kEnergyW, wh::kEnergyH, wh::kEnergyInnerW,
                           wh::kEnergyInnerH, 1.0f, wh::kStamina)));
    stackEl.child(box()
                      .left(ex + 2)
                      .top(wh::kEnergyY + 3)
                      .width(Dimension(wh::kEnergyInnerW))
                      .height(Dimension(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&energy)
                      .fill(Paint::solid(wh::kStamina)));

    // poise, with skillbar.rs's 3x10 ticks along it
    stackEl.child(
        box()
            .left(ex)
            .top(wh::kPoiseY)
            .child(wh::bar(wh::kEnergyW, wh::kEnergyH, wh::kEnergyInnerW,
                           wh::kEnergyInnerH, 1.0f, wh::kPoise)));
    stackEl.child(box()
                      .left(ex + 2)
                      .top(wh::kPoiseY + 3)
                      .width(Dimension(wh::kEnergyInnerW))
                      .height(Dimension(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&poise)
                      .fill(Paint::solid(wh::kPoise)));
    // The ticks are a RAIL, not five boxes: one mark every sixth of the
    // bar, three wide and ten tall, declared once as the ladder it is.
    stackEl.child(
        box()
            .left(ex + 2)
            .top(wh::kPoiseY + 3)
            .width(Dimension(wh::kEnergyInnerW))
            .height(Dimension(wh::kEnergyInnerH))
            .foreground(styles::TickRail{.color = wh::kPoiseTick,
                                         .pitch = wh::kEnergyInnerW / 6.0f,
                                         .minor = 10.0f,
                                         .major = 10.0f,
                                         .width = 3.0f,
                                         .majorEvery = 0,
                                         .phase = 1.0f,
                                         .edge = path::Edge::Top}));
    return stackEl;
  }

  Element hotbar() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    static const struct {
      const char* key;
      wh::Glyph glyph;
      bool filled;
    } kSlots[] = {
        {"M1", wh::Glyph::Sword, true}, {"1", wh::Glyph::Fire, true},
        {"2", wh::Glyph::Frost, true},  {"3", wh::Glyph::Heal, true},
        {"4", wh::Glyph::Dash, true},   {"5", wh::Glyph::Shield, true},
        {"6", wh::Glyph::Bomb, true},   {"7", wh::Glyph::Bow, false},
        {"8", wh::Glyph::Sword, false}, {"9", wh::Glyph::Fire, false},
        {"0", wh::Glyph::Frost, false}, {"M2", wh::Glyph::Bow, true},
    };
    Element rail = stack()
                       .left(wh::kSlotsX)
                       .top(wh::kSlotsY)
                       .width(Dimension(wh::kSlotsW))
                       .height(Dimension(wh::kSlotFrame));
    rail.child(instances(slotAtlas, slotPool));
    for (int i = 0; i < wh::kSlotCount; ++i) {
      const float x =
          arrange::cellRect({i, 0}, {wh::kSlotFrame, wh::kSlotFrame},
                            {wh::kSlotGap, 0})
              .fLeft;
      if (kSlots[i].filled)
        rail.child(
            box()
                .left(x + 9)
                .top(9)
                .width(Dimension(24.0f))
                .height(Dimension(24.0f))
                .shape(wh::glyphPath(kSlots[i].glyph))
                .fill(Paint::linear({0, 0}, {0, 24},
                                    {{0.0f, wh::kBoneHi}, {1.0f, wh::kBone}}))
                // several glyphs are line-only (frost, dash, bow):
                // a fill alone leaves them invisible
                .stroke(stroke(2.2f, Fill::color(wh::kBoneHi)))
                .stroke(stroke(3.4f, Fill::color({0.04f, 0.03f, 0.02f, 0.75f}),
                               PathFormat::Align::Outer)));
      // four of them are cooling down: the sweep Veloren draws as a dark
      // wipe over the icon
      if (i >= 1 && i <= 4)
        rail.child(
            box()
                .left(x + 3)
                .top(3)
                .width(Dimension(wh::kSlot - 4))
                .height(Dimension(wh::kSlot - 4))
                .transformOrigin(0.5f, 0.0f)
                .scaleY(&cooldown[(size_t)i - 1])
                .fill(Paint::linear({0, 0}, {0, wh::kSlot - 4},
                                    {{0.0f, {0.06f, 0.10f, 0.16f, 0.86f}},
                                     {1.0f, {0.10f, 0.16f, 0.24f, 0.72f}}})));
      rail.child(text(toUtf8(kSlots[i].key), wh::type(9, wh::kInkDim, 0.6f))
                     .left(x + 4)
                     .top(wh::kSlotFrame - 13));
    }
    // the selected-exp chip skillbar.rs hangs off slot10
    rail.child(box()
                   .left(wh::kSlotsW + 3)
                   .top(2)
                   .width(Dimension(34.0f))
                   .height(Dimension(38.0f))
                   .child(worldhud::boneFrame(34, 38, 3).inset(0))
                   .child(box()
                              .left(3)
                              .top(20)
                              .width(Dimension(28.0f))
                              .height(Dimension(6.0f))
                              .fill(Paint::solid(worldhud::kTrack))
                              .child(box()
                                         .left(0)
                                         .top(0)
                                         .width(Dimension(28.0f))
                                         .height(Dimension(6.0f))
                                         .transformOrigin(0.0f, 0.5f)
                                         .scaleX(&xp)
                                         .fill(Paint::solid(worldhud::kXp))))
                   .child(text(toUtf8("34"), wh::type(13, wh::kInk, 0.4f, 640))
                              .left(9)
                              .top(3)));
    return rail;
  }

  /** The minimap: generated terrain under a bone ring, with a compass
   *  rose that counter-rotates and player/POI markers. */
  Element minimap() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    constexpr float d = 168;
    return stack()
        .key("minimap")
        .right(28)
        .top(28)
        .width(Dimension(d))
        .height(Dimension(d))
        .opacity(animate(motion::from(0.0f).to(1.0f), {420ms}))
        .child(
            box()
                .inset(0)
                .corners({d * 0.5f})
                .clip()
                .fill(Paint::solid(hexColor(0x2E4A2A)))
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.014f, 5, 3.0f)))
                           .opacity(0.85f)
                           .blend(SkBlendMode::kMultiply))
                // the height BANDS: three thresholds of one noise field,
                // which is how a world map reads as terrain rather than
                // as a texture
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.030f, 4, 2.0f)))
                           .opacity(0.55f)
                           .blend(SkBlendMode::kOverlay))
                .child(box()
                           .inset(0)
                           .fill(Paint::recipe(field::noise(0.070f, 2, 5.0f)))
                           .opacity(0.30f)
                           .blend(SkBlendMode::kMultiply))
                .child(box().inset(0).fill(
                    Paint::radial({d * 0.5f, d * 0.5f}, d * 0.55f,
                                  {{0.0f, {0, 0, 0, 0}},
                                   {0.72f, {0, 0, 0, 0.25f}},
                                   {1.0f, {0, 0, 0, 0.75f}}})))
                // the rivers Veloren's world always has
                .child(
                    box()
                        .inset(0)
                        .fill(Pattern(mpattern::stripes(2, 47,
                                                        mskia::toColor(hexColor(
                                                            0x2F6FA8, 0.30f))))
                                  .material())
                        .rotate(24.0f)
                        .opacity(0.7f)))
        // THE COMPASS ROSE, turning under the frame. It is a rose and not
        // a cross: small, at the middle, eight points, with the four
        // cardinal arms longer than the four between them.
        .child(
            box()
                .left(d * 0.5f - 23)
                .top(d * 0.5f - 23)
                .width(Dimension(46.0f))
                .height(Dimension(46.0f))
                .rotate(&compass)
                .child(box()
                           .inset(0)
                           .shape(shapes::star(8, 0.34f))
                           .fill(Paint::solid({wh::kBoneHi.fR, wh::kBoneHi.fG,
                                               wh::kBoneHi.fB, 0.30f})))
                .child(box()
                           .inset(9)
                           .shape(shapes::star(4, 0.22f))
                           .fill(Paint::solid({wh::kBoneHi.fR, wh::kBoneHi.fG,
                                               wh::kBoneHi.fB, 0.62f}))))
        .child(box()
                   .left(d * 0.5f - 4)
                   .top(d * 0.5f - 4)
                   .width(Dimension(8.0f))
                   .height(Dimension(8.0f))
                   .shape(shapes::polygon(3))
                   .fill(Paint::solid(hexColor(0xFFE9A8))))
        .child(box()
                   .left(d * 0.30f)
                   .top(d * 0.36f)
                   .width(Dimension(6.0f))
                   .height(Dimension(6.0f))
                   .corners({3})
                   .fill(Paint::solid(wh::kQualityLegendary)))
        .child(box()
                   .left(d * 0.68f)
                   .top(d * 0.62f)
                   .width(Dimension(6.0f))
                   .height(Dimension(6.0f))
                   .corners({3})
                   .fill(Paint::solid(wh::kEnemyHp)))
        // the ring
        .child(box()
                   .inset(0)
                   .corners({d * 0.5f})
                   .foreground(stroke(
                       5.0f,
                       linearGradient({0, 0}, {0, d},
                                      {wh::kBoneHi, wh::kBone, wh::kBoneLo}),
                       PathFormat::Align::Inner))
                   .foreground(
                       stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f}))))
        .child(text(toUtf8("N"), wh::type(11, wh::kInk, 1.0f, 640))
                   .left(d * 0.5f - 4)
                   .top(7))
        .child(box()
                   .row()
                   .left(0)
                   .right(0)
                   .bottom(-19)
                   .justify(Justify::Center)
                   .child(text(toUtf8("1204, -388"),
                               wh::type(10, wh::kInkDim, 1.2f))));
  }

  /** Buff and debuff pips with their drain rings — buffs.rs colours. */
  Element buffRow() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Pip {
      const char* label;
      SkColor4f color;
      float left;
    };
    // The ring is the outline itself, trimmed: a fraction of the way
    // round is a fraction of the buff left.
    auto drainRing = [](SkColor4f colour, float left) {
      PathFormat ring = stroke(2.6f, Fill::color(colour));
      ring.cap = SkPaint::kRound_Cap;
      ring.trimStart = 0.0f;
      ring.trimEnd = std::max(0.0f, std::min(1.0f, left));
      return ring;
    };
    static const Pip kPips[] = {
        {"REG", wh::kBuff, 0.72f},   {"HST", wh::kBuff, 0.35f},
        {"PRT", wh::kBuff, 0.88f},   {"BRN", wh::kDebuff, 0.51f},
        {"BLD", wh::kDebuff, 0.19f},
    };
    Element row = box()
                      .key("buffs")
                      .row()
                      .gap(6)
                      .left(28)
                      .top(28)
                      .zIndex(6)
                      .staggerChildren(70ms);
    for (const Pip& p : kPips)
      row.child(
          box()
              .width(Dimension(30.0f))
              .height(Dimension(30.0f))
              .corners({4})
              .opacity(animate(motion::from(0.0f).to(1.0f), {320ms}))
              .translateY(animate(motion::from(-10.0f).to(0.0f), {380ms}))
              .fill(Paint::linear(
                  {0, 0}, {0, 30},
                  {{0.0f, hexColor(0x2A2118)}, {1.0f, hexColor(0x120C08)}}))
              .foreground(stroke(1.4f, Fill::color({p.color.fR, p.color.fG,
                                                    p.color.fB, 0.28f})))
              // THE DRAIN RING: the same outline stroked again, trimmed
              // to what is left of the buff. One node, two decorations —
              // a trim window is per decoration, so the spent part and
              // the remaining part need no second element.
              .foreground(drainRing(p.color, p.left))
              .alignItems(Align::Center)
              .justify(Justify::Center)
              // the drain: a dark wipe from the bottom, under the label
              .child(box()
                         .left(0)
                         .bottom(0)
                         .width(Dimension(30.0f))
                         .height(Dimension(30.0f * (1.0f - p.left)))
                         .fill(Paint::solid({0, 0, 0, 0.62f}))
                         .zIndex(1))
              .child(text(toUtf8(p.label), wh::type(9, p.color, 0.6f, 640))
                         .zIndex(2)));
    return row;
  }

  /** The loot scroller: recent pickups, each in its quality colour. */
  Element lootFeed() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Line {
      const char* text;
      SkColor4f color;
    };
    static const Line kLines[] = {
        {"Sunsteel Greatsword", wh::kQualityLegendary},
        {"Cave Spider Silk x4", wh::kQualityLow},
        {"Velorite Fragment x2", wh::kQualityHigh},
        {"Rugged Hide", wh::kQualityModerate},
        {"Glowing Remains", wh::kQualityEpic},
    };
    Element feed = box()
                       .key("loot")
                       .column()
                       .gap(3)
                       .left(28)
                       .bottom(70)
                       .zIndex(6)
                       .staggerChildren(90ms);
    for (const Line& l : kLines)
      feed.child(
          box()
              .row()
              .alignItems(Align::Center)
              .gap(7)
              .opacity(animate(motion::from(0.0f).to(1.0f), {420ms}))
              .translateX(animate(motion::from(-24.0f).to(0.0f), {480ms}))
              .child(box()
                         .width(Dimension(16.0f))
                         .height(Dimension(16.0f))
                         .corners({2})
                         .fill(Paint::solid({l.color.fR * 0.28f,
                                             l.color.fG * 0.28f,
                                             l.color.fB * 0.28f, 1}))
                         .foreground(stroke(1.0f, Fill::color(l.color))))
              .child(text(toUtf8(l.text), wh::type(11, l.color, 0.4f))));
    return feed;
  }

  /** The enemy nameplate: ENEMY_HP_COLOR over a black track. */
  Element targetPlate() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    return box()
        .key("target")
        .column()
        .alignItems(Align::Center)
        .left(0)
        .right(0)
        .top(96)
        .zIndex(6)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {360ms, &choreograph::easeOutQuad, 220ms}))
        .child(text(toUtf8("CAVE TROLL"), wh::type(15, wh::kInk, 1.6f, 640)))
        .child(text(toUtf8("Lv 27"), wh::type(10, wh::kInkDim, 1.4f))
                   .margin(0, 2, 0, 4))
        .child(box()
                   .width(Dimension(168.0f))
                   .height(Dimension(9.0f))
                   .fill(Paint::solid(worldhud::kTrack))
                   .foreground(
                       stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f})))
                   .child(box()
                              .left(1)
                              .top(1)
                              .width(Dimension(166.0f))
                              .height(Dimension(7.0f))
                              .transformOrigin(0.0f, 0.5f)
                              .scaleX(&enemyHp)
                              .fill(Paint::solid(wh::kEnemyHp))));
  }

  /** The HUD itself: everything Veloren draws over the world. */
  Element hud() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;

    // The root paints NOTHING. What is behind the HUD is the frame's own
    // valley, and a scrim here would be this study answering its own
    // question.
    auto root = stack();

    root.child(box()
                   .column()
                   .left(28)
                   .top(70)
                   .zIndex(6)
                   .child(text(toUtf8("WELDRIN VALE"),
                               wh::type(20, wh::kInk, 2.6f, 640)))
                   .child(text(toUtf8("LEVEL 34  \xc2\xb7  CLEAR, LIGHT WIND"),
                               wh::type(11, wh::kInkDim, 0.9f))
                              .margin(0, 5, 0, 0)));

    root.child(buffRow());
    root.child(minimap());
    root.child(targetPlate());
    root.child(lootFeed());
    root.child(barStack());
    root.child(hotbar());
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(
    WorldHud, "world hud", "Catalog \xc2\xb7 Game UI",
    "Voxygen's own dimensions \xe2\x80\x94 bars, hotbar, minimap and "
    "nameplate \xe2\x80\x94 baked into one texture over a lit voxel "
    "valley, which is the thing a HUD has to stay legible on")
