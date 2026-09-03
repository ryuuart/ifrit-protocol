/** @file
 * THE LINK-SURFACE PROBE FOR THE DEVICE BACKEND. Not a sketch of the
 * registry — a file the host is pointed at, compiled and dlopened
 * exactly as a hot-reloaded sketch is, whose whole subject is that a
 * symbol resolves.
 *
 * It names `world::diligent::importNative`, which no sketch compiled
 * into the host names. A symbol only the host's OWN link line carries —
 * a device backend is one, because a bare consumer of the sketch target
 * must not be made to link a renderer — is reachable from a compiled-in
 * sketch and, if the force-load list is read off the sketch target
 * alone, not from a reloaded one. That gap is invisible in a compile and
 * in a picture; it appears as "symbol not found in flat namespace" at
 * dlopen, and only here.
 *
 * It draws a plain card so that `--frame` has something to write: the
 * verdict is the load, and the picture is only proof that the loaded
 * code ran.
 */

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/diligent/Import.h>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;

using namespace sigil::compose;

namespace {

struct DeviceImportProbe : sketch::Sketch {
  bool imported = false;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(360, 120);
    ctx.background({0.06f, 0.07f, 0.10f, 1});

    // The door under test. On the CPU tier there is no device and the
    // call never runs — the reference is what matters, because a symbol
    // the host does not carry stops the dylib at load whether or not the
    // branch is ever taken. On a device it runs and refuses an empty
    // texture, which is the value's stated answer.
    if (auto* on = sketch::device()) {
      const sigil::material::Texture texture = world::diligent::importNative(
          *on, sigil::core::hardware::NativeTexture{});
      imported = texture.valid();
    }

    ctx.composer.render(
        stack()
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .child(text(imported ? u8"imported" : u8"no device",
                        weave::textStyle({.size = 22,
                                          .color = hex(0xd8e2f0)}))));
  }
};

}  // namespace

SIGIL_SKETCH(DeviceImportProbe, "Start & fixtures",
             "The device backend's symbols, resolved at dlopen")
