/** A Set module exposes whether device preparation precedes body creation. */
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/frame/Frame.h>

#include <cstdio>

namespace {

struct DeviceSetCapture {
  DeviceSetCapture() { std::puts("PLUGIN_SET_BODY"); }
  void setup(sigil::sketch::SetContext& ctx) {
    std::puts("PLUGIN_SET_SETUP");
    ctx.canvas(32, 24);
  }
  sigil::world::Frame describe() { return {}; }
};

}  // namespace

SIGIL_SKETCH(DeviceSetCapture, "Test", "Set executor preparation coverage")
