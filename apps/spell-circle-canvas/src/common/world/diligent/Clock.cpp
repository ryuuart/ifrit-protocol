/** @file
 * The device's clock over a frame's stages: a timestamp written at every
 * stage boundary of a frame that asks for one, read once the device has
 * finished that frame, and summed into one time per stage.
 */

#include <Graphics/GraphicsEngine/interface/Query.h>
#include <sigilworld/advanced/Skia.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** Whether the device writes timestamps at all, asked once. */
bool timestampsOffered(Gpu& gpu) {
  dg::IRenderDevice* device = gpu.device->renderDevice();
  return device && device->GetDeviceInfo().Features.TimestampQueries ==
                       dg::DEVICE_FEATURE_STATE_ENABLED;
}

/** The marks of one frame as stage times, or nothing while the device
 *  has not finished it. A mark read is a query free again, handed to
 *  @p spare. */
bool readFrame(std::vector<StageClock::Mark>& marks,
               std::vector<DeviceStage>& stages,
               std::vector<dg::RefCntAutoPtr<dg::IQuery>>& spare) {
  // THE LAST MARK ANSWERS FOR ALL OF THEM: the device finishes a frame's
  // work in the order it was submitted, so once the last timestamp is in,
  // every one before it is.
  dg::QueryDataTimestamp probe;
  if (!marks.back().query->GetData(&probe, sizeof probe, false)) return false;
  std::vector<double> seconds(marks.size(), 0.0);
  bool whole = true;
  for (size_t i = 0; i < marks.size(); ++i) {
    dg::QueryDataTimestamp data;
    if (!marks[i].query->GetData(&data, sizeof data, true) ||
        data.Frequency == 0) {
      whole = false;
      continue;
    }
    seconds[i] = (double)data.Counter / (double)data.Frequency;
  }
  stages.clear();
  if (whole) {
    for (size_t i = 1; i < marks.size(); ++i) {
      if (marks[i].stage.empty()) continue;
      const double ms = std::max(0.0, (seconds[i] - seconds[i - 1]) * 1000.0);
      auto same = std::find_if(stages.begin(), stages.end(),
                               [&](const DeviceStage& stage) {
                                 return stage.stage == marks[i].stage;
                               });
      if (same == stages.end())
        stages.push_back(DeviceStage{marks[i].stage, ms});
      else
        same->ms += ms;
    }
  }
  for (StageClock::Mark& mark : marks) spare.push_back(std::move(mark.query));
  marks.clear();
  return true;
}

}  // namespace

void Gpu::startClock(bool timed, const Targets& scene) {
  StageClock& c = clock;
  // The frame before closes here rather than at its own end, because its
  // final stage — the picture presented or read back — is drawn after the
  // frame's passes and belongs to it.
  if (c.open.marks.size() > 1) {
    c.pending.push_back(std::move(c.open));
    ++c.framesTimed;
  }
  c.open = {};
  while (c.pending.size() > StageClock::kFramesKept) c.pending.pop_front();
  while (!c.pending.empty()) {
    std::vector<DeviceStage> stages;
    const uint64_t drewIt = c.pending.front().scene;
    if (!readFrame(c.pending.front().marks, stages, c.spare)) break;
    c.pending.pop_front();
    if (stages.empty()) continue;
    c.latest = std::move(stages);
    c.latestScene = drewIt;
    c.latestFrame = c.framesTimed - (int64_t)c.pending.size() - 1;
  }
  c.on = false;
  c.open.scene = scene.serial();
  if (!timed) return;
  if (!c.probed) {
    c.unavailable = !timestampsOffered(*this);
    c.probed = true;
  }
  if (c.unavailable) return;
  c.on = true;
  markWhole("");
}

void Gpu::markWhole(std::string_view stage) {
  StageClock& c = clock;
  if (!c.on) return;
  dg::RefCntAutoPtr<dg::IQuery> query;
  if (!c.spare.empty()) {
    query = std::move(c.spare.back());
    c.spare.pop_back();
  } else {
    dg::QueryDesc desc;
    desc.Name = "world stage clock";
    desc.Type = dg::QUERY_TYPE_TIMESTAMP;
    device->renderDevice()->CreateQuery(desc, &query);
    if (!query) {
      c.unavailable = true;
      c.on = false;
      c.open = {};
      return;
    }
  }
  device->context()->EndQuery(query);
  c.open.marks.push_back(
      StageClock::Mark{std::string(stage), std::move(query)});
}

void Gpu::mark(std::string_view stage) {
  if (!clock.on) return;
  if (stage.empty() || clockPass.empty()) {
    markWhole(stage);
    return;
  }
  std::string named = clockPass;
  named += '.';
  named += stage;
  markWhole(named);
}

void Gpu::reportClock(Targets& targets) const {
  const StageClock& c = clock;
  if (c.unavailable) {
    targets.deviceStages({}, DeviceTiming::Unavailable, -1);
    return;
  }
  if (!c.on) return;
  if (c.latestFrame < 0 || c.latestScene != targets.serial()) {
    targets.deviceStages({}, DeviceTiming::Pending, -1);
    return;
  }
  targets.deviceStages(c.latest, DeviceTiming::Measured, c.latestFrame);
}

}  // namespace sigil::world::diligent
