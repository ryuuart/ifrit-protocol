/** @file
 * What each resource is, how long it lives, and which of them can share
 * a surface. A resource lives from the step that first writes it to the
 * last step that touches it; two whose lives do not overlap are handed
 * one surface, because a frame that keeps a surface per name pays for
 * every name at once. A resource that outlives the frame — read back,
 * read as a previous, or the one the picture is presented from — is
 * never aliased, since something reads it after the last pass has run.
 */

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph::detail {

namespace {

Resource* find(std::vector<Resource>& resources, const std::string& name) {
  for (Resource& resource : resources)
    if (resource.name == name) return &resource;
  return nullptr;
}

}  // namespace

void lives(std::span<const PassWork> steps, const Frame& frame,
           std::vector<Resource>& into, std::string& present,
           std::vector<std::string>& kept) {
  into.clear();
  const auto touch = [&](const std::string& name, size_t step, Kind kind,
                         bool written) {
    Resource* resource = find(into, name);
    if (!resource) {
      into.push_back(
          Resource{.name = name, .kind = kind, .first = step, .last = step});
      return;
    }
    resource->last = std::max(resource->last, step);
    // A resource read before anything wrote it still starts where it is
    // first touched; a write only ever moves the kind, never the start.
    if (written && resource->kind != kind) resource->kind = kind;
  };

  for (size_t step = 0; step < steps.size(); ++step) {
    const Touches touches = touchesOf(steps[step]);
    const Kind kind = steps[step].pass->stage() == Stage::Compute ? Kind::Points
                                                                  : Kind::Image;
    for (const std::string& name : touches.writes)
      touch(name, step, kind, /*written=*/true);
    for (const std::string& name : touches.reads)
      touch(name, step, kind, /*written=*/false);
  }

  // What outlives the frame.
  kept.clear();
  const auto persist = [&](const std::string& name, bool keep) {
    if (Resource* resource = find(into, name)) resource->persistent = true;
    if (!keep) return;
    if (std::find(kept.begin(), kept.end(), name) == kept.end())
      kept.push_back(name);
  };
  for (const PassWork& step : steps)
    for (const std::string& name : step.pass->previous()) persist(name, true);
  for (const Readback& back : frame.readbacks()) persist(back.name(), true);

  present.clear();
  if (!frame.present().empty()) {
    present = frame.present();
  } else {
    for (size_t step = steps.size(); step-- > 0;) {
      const Touches touches = touchesOf(steps[step]);
      if (steps[step].pass->stage() == Stage::Compute) continue;
      if (touches.writes.empty()) continue;
      present = touches.writes.front();
      break;
    }
  }
  if (!present.empty()) persist(present, false);
}

int alias(std::vector<Resource>& resources) {
  // The last step using each shared surface. A resource joins the
  // lowest-numbered surface whose last user finished before it starts,
  // so the count is the largest number of image resources alive at once
  // rather than the number of names.
  std::vector<size_t> lastUser;
  std::vector<bool> used;
  std::vector<size_t> transients;
  for (size_t i = 0; i < resources.size(); ++i)
    if (resources[i].kind == Kind::Image && !resources[i].persistent)
      transients.push_back(i);
  std::stable_sort(transients.begin(), transients.end(),
                   [&resources](size_t a, size_t b) {
                     return resources[a].first < resources[b].first;
                   });

  int own = 0;
  for (const Resource& resource : resources)
    if (resource.kind == Kind::Image && resource.persistent) ++own;

  for (size_t index : transients) {
    Resource& resource = resources[index];
    int slot = -1;
    for (size_t candidate = 0; candidate < lastUser.size(); ++candidate) {
      if (lastUser[candidate] >= resource.first) continue;
      slot = (int)candidate;
      break;
    }
    if (slot < 0) {
      slot = (int)lastUser.size();
      lastUser.push_back(0);
      used.push_back(false);
    } else {
      used[(size_t)slot] = true;
    }
    resource.slot = slot;
    lastUser[(size_t)slot] = resource.last;
  }
  // A surface serving two resources makes both of them aliased: the
  // first one's pixels are the ones the second overwrites.
  for (size_t index : transients)
    if (resources[index].slot >= 0 && used[(size_t)resources[index].slot])
      resources[index].aliased = true;
  return (int)lastUser.size() + own;
}

}  // namespace sigil::world::graph::detail
