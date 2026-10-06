#pragma once

/** @file
 * @ingroup world-scene
 * What one frame of a Scene did, counted — the reconcile pass's own
 * tallies beside the retained side's.
 */

#include <sigilcore/reconcile/Stats.h>
#include <sigilmeasure/advanced/Counters.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::world {

/** ONE STAGE OF A FRAME ON THE DEVICE and the GPU time it took: the time
 *  between the device finishing the stage before it and finishing this
 *  one, as the device's own timestamps read it. A stage is named by the
 *  pass it belongs to and what it does there — `main.shadow`,
 *  `main.laid`, `main.colour-copy`, `main.mips`, `main.depth-copy`,
 *  `main.seen`, `main.blended`, `main.extras`, a post pass by its own
 *  name, and `present` or `readback` for the final stage a picture leaves
 *  the frame through. */
struct DeviceStage {
  std::string stage;
  double ms = 0.0;
};

/** WHETHER A FRAME'S DEVICE STAGES WERE TIMED. */
enum class DeviceTiming : uint8_t {
  /** Nobody asked: the frame wrote no timestamp. */
  Off,
  /** Asked, and the device cannot write timestamps. */
  Unavailable,
  /** Asked, and no timed frame has finished on the device yet. */
  Pending,
  /** `deviceStages` holds a finished frame's stages. */
  Measured,
};

/** THE FRAME'S COST, as numbers a test can assert and a table can print.
 *  Plain integers on the hot path; `report()` publishes them under their
 *  names beside the reconciler's. */
struct SceneStats {
  /** What the reconcile pass did — described, memo hits, patched,
   *  mounted, retired. */
  core::ReconcileStats reconcile;
  int64_t nodes = 0;            ///< nodes standing in the retained tree
  int64_t extracted = 0;        ///< nodes extract wrote components for
  int64_t cooked = 0;           ///< geometry slots cooked this frame
  int64_t resources = 0;        ///< distinct cooked artefacts held
  int64_t baked = 0;            ///< subtrees whose draw order was taken
  int64_t replayed = 0;         ///< subtrees whose draw order was replayed
  int64_t drawn = 0;            ///< entities in the extracted draw order
  int64_t rounds = 0;           ///< converging rounds the phase runner ran
  int64_t passes = 0;           ///< passes executed
  int64_t barriers = 0;         ///< hazards the ordering stated
  int64_t aliased = 0;          ///< resources handed a surface another one uses
  int64_t surfaces = 0;         ///< surfaces the frame's image resources needed
  int64_t texturesPainted = 0;  ///< texture passes that painted
  int64_t texturesSkipped = 0;  ///< texture passes that found nothing changed
  int64_t shadowMaps = 0;       ///< lights whose shadow map the frame draws
  /** Maps the executor drew this frame — six a point light, one a
   *  cascade — leaving out every map nothing moved in, which is kept. */
  int64_t shadowMapsDrawn = 0;
  /** Bodies drawn as a stand-in because their program was still
   *  compiling: zero in a frame that waits for every program. */
  int64_t programsPending = 0;
  int64_t readbacks = 0;  ///< images read back into host memory this frame
  /** Pictures presented by sampling the executor's own texture where it
   *  stands, with nothing read back. */
  int64_t presentedOnDevice = 0;
  /** THE GPU TIME OF EACH STAGE, on a frame that asked for it
   *  (`Frame::timeStages`). A device answers a timestamp only once the
   *  frame that wrote it has finished there, so these are the stages of
   *  the newest timed frame that has — a frame or a few before this one —
   *  and `deviceStagesFrame` numbers that frame, so a reader averaging
   *  over frames counts each one once. Empty unless `deviceTiming` is
   *  `Measured`. */
  std::vector<DeviceStage> deviceStages;
  DeviceTiming deviceTiming = DeviceTiming::Off;
  int64_t deviceStagesFrame = -1;

  /** The sum of `deviceStages`: the frame's GPU time from its first
   *  stage to its last. */
  [[nodiscard]] double deviceMs() const {
    double total = 0.0;
    for (const DeviceStage& stage : deviceStages) total += stage.ms;
    return total;
  }

  /** Every count back to zero, for the next frame. */
  void reset() {
    reconcile.reset();
    nodes = extracted = cooked = baked = replayed = drawn = rounds = 0;
    passes = barriers = aliased = surfaces = 0;
    texturesPainted = texturesSkipped = shadowMaps = 0;
    readbacks = presentedOnDevice = shadowMapsDrawn = programsPending = 0;
    deviceStages.clear();
    deviceTiming = DeviceTiming::Off;
    deviceStagesFrame = -1;
  }

  /** Adds every count under its name (`world.nodes`, `world.extracted`,
   *  `world.cooked`, `world.resources`, `world.baked`,
   *  `world.replayed`, `world.drawn`, `world.rounds`, `world.passes`,
   *  `world.barriers`, `world.aliased`, `world.surfaces`,
   *  `world.texturesPainted`, `world.texturesSkipped`,
   *  `world.shadowMaps`, `world.shadowMapsDrawn`,
   *  `world.programsPending`, `world.readbacks`,
   *  `world.presentedOnDevice`),
   *  and the
   *  reconciler's under theirs. */
  void report(measure::Counters& counters) const {
    reconcile.report(counters);
    counters.add("world.nodes", nodes);
    counters.add("world.extracted", extracted);
    counters.add("world.cooked", cooked);
    counters.add("world.resources", resources);
    counters.add("world.baked", baked);
    counters.add("world.replayed", replayed);
    counters.add("world.drawn", drawn);
    counters.add("world.rounds", rounds);
    counters.add("world.passes", passes);
    counters.add("world.barriers", barriers);
    counters.add("world.aliased", aliased);
    counters.add("world.surfaces", surfaces);
    counters.add("world.texturesPainted", texturesPainted);
    counters.add("world.texturesSkipped", texturesSkipped);
    counters.add("world.shadowMapsDrawn", shadowMapsDrawn);
    counters.add("world.programsPending", programsPending);
    counters.add("world.readbacks", readbacks);
    counters.add("world.presentedOnDevice", presentedOnDevice);
    counters.add("world.shadowMaps", shadowMaps);
  }
};

}  // namespace sigil::world
