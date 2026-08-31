/** @file
 * The built-in executor: a pass performed on the CPU, into raster
 * surfaces. It is a real answer for tests, for the plate ledger and for
 * a machine with no device, and it is not a substitute for one.
 */

#include "Cpu.h"

#include <sigilworld/frame/Runtime.h>

namespace sigil::world {

namespace {

/** Stateless, so every instance is the same value — which is what lets
 *  two frames that did not name a runtime compare equal on it. */
class CpuExecutor : public Executor {
 public:
  void execute(const PassWork& work, const View& view,
               Targets& targets) const override {
    if (!work.pass) return;
    const Pass& pass = *work.pass;
    // A declared body replaces the stage's own work and keeps the
    // stage's declarations, so the ordering around it is unchanged.
    if (pass.body()) {
      pass.body()->run(view, targets);
      return;
    }
    switch (pass.stage()) {
      case Stage::Geometry:
        cpu::paintGeometry(work, view, targets);
        return;
      case Stage::Compute:
        cpu::cookPoints(work, targets);
        return;
      case Stage::Post:
        cpu::applyPost(work, targets);
        return;
    }
  }

  bool operator==(const CpuExecutor&) const { return true; }
};

}  // namespace

Runtime Runtime::cpu() {
  static const Runtime value{CpuExecutor{}};
  return value;
}

namespace cpu {

const std::string* target(const Pass& pass) {
  const std::span<const std::string> writes = pass.writes();
  return writes.empty() ? nullptr : &writes.front();
}

}  // namespace cpu

}  // namespace sigil::world
