#pragma once

/** @file
 * DECLARED MOTION for world entities — the second animation door, next
 * to (never instead of) the imperative setters.
 *
 * Until 2026-07-29 SigilWorld's only way to move anything was "two
 * floats per frame": the caller computed a value and called a setter
 * (`setTransform`, `setSweepWindow`, a `MaterialComponent` field). That
 * door stays exactly as it is and is still the right tool for a one-off
 * poke. What it cannot do is DECLARE — say once that this surface's
 * uv window rides that phase through this curve, and then never think
 * about it again. `Animatable<float>` is that declaration, and it
 * became reachable here when it moved out of SigilCompose into
 * <sigilmotion/Animation.h> (SigilMotion links choreograph and nothing
 * else, so world can hold it without swallowing a drawing library).
 *
 * The shape of the door:
 *
 *     choreograph::Output<float> phase{0};
 *     ticker.timeline().apply(&phase).then<ch::RampTo>(1.0f, 8.0f);
 *
 *     auto &reg = world.registry();
 *     reg.emplace<AnimatedMaterial>(entity(bandId)).uvOffsetX =
 *         bind(&phase).target(0.0f, 1.0f);
 *     reg.emplace<AnimatedTransform>(entity(dartId), base).yawDeg =
 *         bind(&phase).map(ease::inOutBack()).target(0.0f, 360.0f);
 *
 *     for (int frame = 0; frame < 600; ++frame) {
 *       ticker.tick(1.0 / 60.0);   // the CALLER owns the clock
 *       world.render();            // resolves, then draws
 *     }
 *
 * Three rulings are worth knowing before you use it (the arguments are
 * in world/README.md, dated 2026-07-29):
 *
 * 1. **Every lane is a float.** Not `Animatable<glm::vec3>` — a
 *    position is three lanes. `bind()`'s normalise → curve → affine
 *    chain (`source`/`window`/`map`/`target`/`quantize`/`clamp`) is
 *    FLOAT-ONLY, and that chain is most of the value of the door; a
 *    vec3 slot could only hold a plain constant or a raw binding, which
 *    is a weaker lane wearing a fancier type. `Animatable<float>`
 *    converts implicitly from float, so `at(0, 60, 0)` still reads like
 *    a position.
 *
 * 2. **World owns no clock.** There is no `world.tick()`, no
 *    `render(dt)`, no FrameClock in here. `FrameClock::tick()` reads
 *    `steady_clock`; a world that ticked one inside `render()` would
 *    make every headless plate a function of wall time, and world_demo's
 *    13 artifacts must be byte-reproducible. The caller steps a
 *    `motion::Ticker` with the delta it chooses (`1.0/60.0` in a
 *    headless loop) and `render()` is a pure function of whatever the
 *    Outputs hold at that instant.
 *
 * 3. **The `animate(...)` form lands on its SETTLED value.** These
 *    lanes accept `animate(to(v))` / `animate(from(a).to(b))` because
 *    they are the same slot type compose uses — but ramp-on-change
 *    needs a CHANGE event, and world has no describe/diff over
 *    components (they are mutated in place). So a transitioned value
 *    resolves to its target with no ramp, exactly as compose's
 *    `snapshot()` bakes the settled value. To actually ramp, put the
 *    ramp on the timeline and bind it.
 *
 * The caller owns the `choreograph::Output`s a lane points at; a bound
 * lane outliving its Output dangles (the same contract compose has).
 *
 * The camera joined on 2026-07-29 as `AnimatedCamera` — a fifth
 * component on the same footing as the rest, because a camera is
 * already a registry entity (`CameraComponent`) and needed no new home.
 *
 * WHAT THIS HEADER DOES NOT REACH: `scene::Scene`-managed surfaces.
 * The reconciler keeps its entity ids private, so there is no supported
 * way to attach an `Animated*` to a declared node; and a leaf whose
 * mesh or material changes is remove+add, i.e. a NEW entity, so a
 * component found by other means would vanish there. Camera lanes are
 * the exception that composes freely — the camera is not a scene node.
 * Pinned by the `WorldSceneAnimation` tests and argued in the README.
 */

#include "sigilworld/Components.h"

#include <sigilmotion/Animation.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <optional>

namespace sigil::world {

/** The value vocabulary, spelled without a namespace prefix at world
 *  call sites — the same re-export SigilCompose does into
 *  `sigil::compose`. `motion::` remains the home. */
using motion::Animatable;
using motion::Bound;
using motion::Transition;
using motion::Transitioned;
using motion::animate;
using motion::bind;
using motion::from;
using motion::through;
using motion::to;
namespace ease = motion::ease;

/** A COMPLETE placement, composed each resolve as
 *  `model = base * translate * yaw * pitch * roll * scale` — the same
 *  TRS convention (and the same order) as `scene::Node::localMatrix()`,
 *  so a node and an animated entity agree about what "turned 30" means.
 *
 *  Unlike the material and light components below, the lanes here are
 *  NOT optional, because this component describes the whole transform
 *  rather than overriding part of one: an unmentioned lane is genuinely
 *  "no translation" / "unit scale". A surface that already has a static
 *  placement puts it in @ref base and animates around it — TRS is then
 *  read in the surface's own parent frame.
 *
 *  This component OWNS its entity's TransformComponent. Do not also
 *  drive it with `setTransform()`; the next resolve would win. */
struct AnimatedTransform {
  /** The static placement the animated TRS rides on top of. */
  glm::mat4 base{1.0f};
  Animatable<float> x = 0.0f, y = 0.0f, z = 0.0f;
  Animatable<float> yawDeg = 0.0f, pitchDeg = 0.0f, rollDeg = 0.0f;
  Animatable<float> scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
};

/** PARTIAL overrides of a MaterialComponent's live scalars: only the
 *  lanes you engage are written, everything else on the material is
 *  left exactly as authored. That is what the `optional` buys and it is
 *  not decoration — a component with plain defaults would slam
 *  `opacity` to 1 on a pane authored at 0.4 the moment you engaged
 *  `uvOffsetX`.
 *
 *  `opacity` writes `Material::baseColor.w`, so it also routes the
 *  surface between the opaque and the blended pass, live, exactly as a
 *  hand-written alpha does.
 *
 *  Colour is deliberately absent — see the README ruling: three
 *  independent linear-RGB float lanes are the WRONG default for a
 *  colour ramp (this repo already runs OKLab in `shape::blend` for
 *  precisely that reason), so a colour lane wants a colour type, not
 *  four floats in a trench coat. */
struct AnimatedMaterial {
  std::optional<Animatable<float>> opacity;
  std::optional<Animatable<float>> emissiveStrength;
  std::optional<Animatable<float>> uvOffsetX, uvOffsetY;
  std::optional<Animatable<float>> uvScaleX, uvScaleY;
};

/** PARTIAL overrides of a LightComponent (same optional rule as
 *  AnimatedMaterial). `x/y/z` drive `LightComponent::position`, so they
 *  are a point light's lanes; a directional light animates only
 *  `intensity` here (its `direction` wants to stay normalised, which
 *  three free float lanes cannot promise). */
struct AnimatedLight {
  std::optional<Animatable<float>> intensity;
  std::optional<Animatable<float>> x, y, z;
};

/** PARTIAL overrides of a `CameraComponent`'s placement and lens (the
 *  same optional rule as AnimatedMaterial and for the same reason: the
 *  caller authors the camera, these lanes drive part of it).
 *
 *  The camera needed NO new home. Every other lane in this header hangs
 *  off a registry entity, and since Components.h a camera is a registry
 *  entity too — `CameraComponent{camera, active}`, whose documented,
 *  already-pinned rule is that an ACTIVE one overrides `World::setCamera`
 *  while it exists. So an animated camera is just a camera entity whose
 *  fields a system writes, this resolve stays in the device-free half
 *  (a camera lane is pinnable on a machine with no Vulkan), and the
 *  precedence question answers itself — see the README, 2026-07-29.
 *
 *  Eight lanes, and the two absences are the argument:
 *
 *  - `up` gets NO lanes. It must stay a unit vector roughly out of the
 *    view axis, which three free floats cannot promise — the same
 *    refusal AnimatedLight makes for a directional light's `direction`.
 *    @ref rollDeg is the safe single-float parameterisation of it (and
 *    the only way to declare a dutch tilt at all): it turns
 *    @ref rollReference right-handed about the eye→target axis, so the
 *    camera rolls CLOCKWISE seen from behind it and the scene tips
 *    counter-clockwise in frame. Recomputed from the fixed reference
 *    every resolve, so it neither drifts nor accumulates.
 *  - `zNear`/`zFar` get no lanes. Nobody ramps a clip plane on purpose:
 *    a moving near plane buys nothing and spends depth precision
 *    (z-fighting that pops as it slides). They are scene-scale
 *    constants — set them on the component or through `setCamera()`.
 *
 *  Lanes resolve in order, so @ref rollDeg sees the eye/target this
 *  frame's own lanes just produced. `active` is NOT consulted: it gates
 *  the RENDERER's choice of camera, not this system, so toggling a
 *  camera on never replays a backlog of missed frames.
 *
 *  This component OWNS the CameraComponent fields it engages. Do not
 *  also write them by hand — the next resolve would win. */
struct AnimatedCamera {
  std::optional<Animatable<float>> eyeX, eyeY, eyeZ;
  std::optional<Animatable<float>> targetX, targetY, targetZ;
  std::optional<Animatable<float>> fovYDeg;
  std::optional<Animatable<float>> rollDeg;
  /** The un-rolled up vector @ref rollDeg turns about the view axis.
   *  Not a lane — the fixed reference that keeps roll idempotent. */
  glm::vec3 rollReference{0, 1, 0};
};

/** The GPU generator window — `addSweep`/`addFlock`/`addPoints`'s
 *  `(head, span)`, the single most-animated pair in the whole corpus
 *  (world_demo's entire flight is this).
 *
 *  It is the one lane in this header that sits in front of a GPU
 *  RE-COOK rather than in front of a live shader parameter, and that
 *  makes it the one lane that could be a trap: an unconditional write
 *  would mark the surface dirty EVERY frame and re-dispatch the compute
 *  pass forever, even while the bound Output sits still (a 300k-point
 *  flock re-scattering for nothing). So this lane — and, for one rule
 *  everywhere, every other lane too — is CHANGE-DETECTED: resolve
 *  writes only what moved and reports it in AnimationStats. A constant
 *  lane costs exactly one re-cook, ever.
 *
 *  Resolution routes through the three public window setters, each of
 *  which is a documented no-op on a surface of the wrong kind, so one
 *  component covers sweeps, flocks and pop chains without world having
 *  to publish which kind an entity is. */
struct AnimatedWindow {
  Animatable<float> head = 1.0f;
  Animatable<float> span = 1.0f;

  /** Written by resolveAnimation(); the last pair actually pushed. */
  float appliedHead = 0, appliedSpan = 0;
  bool applied = false;
};

/** What the last resolve MOVED — the same "pruning is observable"
 *  contract `scene::Scene::Stats` sets. Zero across the board means the
 *  resolve was a no-op, which is exactly what a settled scene should
 *  report and what the idempotence pin asserts. */
struct AnimationStats {
  int transforms = 0;
  int materials = 0;
  int lights = 0;
  int cameras = 0;
  int windows = 0;
};

/** One lane's current number. The three live forms, in the order the
 *  slot discriminates them; a transitioned value yields its settled
 *  target (ruling 3 at the top of this file). */
inline float resolveValue(const Animatable<float> &v) {
  if (const choreograph::Output<float> *bound = v.binding()) {
    if (const motion::BoundFloat *shape = v.boundMap())
      return shape->apply(bound->value());
    return bound->value();
  }
  if (const float *plain = v.plain())
    return *plain;
  return v.transitioned()->value;
}

/** THE SYSTEM, in its device-free half: resolve every animated
 *  component that writes nothing but registry state.
 *
 *  A free function over the registry is the entt-idiomatic answer and
 *  it is also the TESTABLE one — this half needs no Vulkan, so the
 *  animation semantics stay pinned on a machine where every
 *  device-backed world test skips. `World::render()` calls the overload
 *  below, so nothing has to remember to call either. */
inline AnimationStats resolveAnimation(entt::registry &registry) {
  constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
  AnimationStats stats;

  for (auto [e, animated, transform] :
       registry.view<AnimatedTransform, TransformComponent>().each()) {
    glm::mat4 m = glm::translate(
        animated.base,
        glm::vec3{resolveValue(animated.x), resolveValue(animated.y),
                  resolveValue(animated.z)});
    // Zero-angle / unit-scale steps are skipped rather than multiplied
    // through, matching scene::Node::localMatrix() exactly.
    if (const float yaw = resolveValue(animated.yawDeg); yaw != 0)
      m = glm::rotate(m, yaw * kDegToRad, glm::vec3{0, 1, 0});
    if (const float pitch = resolveValue(animated.pitchDeg); pitch != 0)
      m = glm::rotate(m, pitch * kDegToRad, glm::vec3{1, 0, 0});
    if (const float roll = resolveValue(animated.rollDeg); roll != 0)
      m = glm::rotate(m, roll * kDegToRad, glm::vec3{0, 0, 1});
    const glm::vec3 scale{resolveValue(animated.scaleX),
                          resolveValue(animated.scaleY),
                          resolveValue(animated.scaleZ)};
    if (scale != glm::vec3{1, 1, 1})
      m = glm::scale(m, scale);
    if (!(transform.model == m)) {
      transform.model = m;
      ++stats.transforms;
    }
  }

  const auto put = [](float &dst,
                      const std::optional<Animatable<float>> &lane,
                      bool &changed) {
    if (!lane)
      return;
    const float v = resolveValue(*lane);
    if (dst != v) {
      dst = v;
      changed = true;
    }
  };

  for (auto [e, animated, material] :
       registry.view<AnimatedMaterial, MaterialComponent>().each()) {
    bool changed = false;
    Material &m = material.material;
    put(m.baseColor.w, animated.opacity, changed);
    put(m.emissiveStrength, animated.emissiveStrength, changed);
    put(m.uvOffset.x, animated.uvOffsetX, changed);
    put(m.uvOffset.y, animated.uvOffsetY, changed);
    put(m.uvScale.x, animated.uvScaleX, changed);
    put(m.uvScale.y, animated.uvScaleY, changed);
    if (changed)
      ++stats.materials;
  }

  for (auto [e, animated, light] :
       registry.view<AnimatedLight, LightComponent>().each()) {
    bool changed = false;
    put(light.intensity, animated.intensity, changed);
    put(light.position.x, animated.x, changed);
    put(light.position.y, animated.y, changed);
    put(light.position.z, animated.z, changed);
    if (changed)
      ++stats.lights;
  }

  for (auto [e, animated, cameraComponent] :
       registry.view<AnimatedCamera, CameraComponent>().each()) {
    bool changed = false;
    shape::space::Camera &c = cameraComponent.camera;
    put(c.eye.x, animated.eyeX, changed);
    put(c.eye.y, animated.eyeY, changed);
    put(c.eye.z, animated.eyeZ, changed);
    put(c.target.x, animated.targetX, changed);
    put(c.target.y, animated.targetY, changed);
    put(c.target.z, animated.targetZ, changed);
    put(c.fovYDeg, animated.fovYDeg, changed);
    // Roll last, so it turns around the view axis the eye/target lanes
    // just produced. `up` is derived from the fixed rollReference, never
    // from its own previous value, so resolving twice lands twice.
    if (animated.rollDeg) {
      const glm::vec3 forward = c.target - c.eye;
      if (const float length = glm::length(forward); length > 0) {
        const float roll = resolveValue(*animated.rollDeg) * kDegToRad;
        const glm::vec3 up = glm::vec3(
            glm::rotate(glm::mat4(1.0f), roll, forward / length) *
            glm::vec4(animated.rollReference, 0.0f));
        if (c.up != up) {
          c.up = up;
          changed = true;
        }
      }
    }
    if (changed)
      ++stats.cameras;
  }

  return stats;
}

/** THE SYSTEM, whole: the registry half above plus AnimatedWindow,
 *  which needs the World because a generator window is a device
 *  parameter rather than a component field.
 *
 *  `World::render()` calls this before anything else it does, so the
 *  cook/draw passes see the frame's values. Call it yourself when you
 *  want the resolved state WITHOUT a frame — before a `readPoints()`
 *  query, say — or when you want the Stats. It is idempotent: a second
 *  call with unmoved Outputs writes nothing and reports zeros. */
inline AnimationStats resolveAnimation(World &world) {
  entt::registry &registry = world.registry();
  AnimationStats stats = resolveAnimation(registry);
  for (auto [e, window] : registry.view<AnimatedWindow>().each()) {
    const float head = resolveValue(window.head);
    const float span = resolveValue(window.span);
    if (window.applied && head == window.appliedHead &&
        span == window.appliedSpan)
      continue;
    const uint32_t id = (uint32_t)e;
    // Exactly one of these three does anything; the other two are the
    // documented no-op on a surface of another kind.
    world.setSweepWindow(id, head, span);
    world.setFlockWindow(id, head, span);
    world.setPointsWindow(id, head, span);
    window.appliedHead = head;
    window.appliedSpan = span;
    window.applied = true;
    ++stats.windows;
  }
  return stats;
}

} // namespace sigil::world
