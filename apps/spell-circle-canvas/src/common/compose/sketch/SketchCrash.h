#pragma once

/** @file
 * A guest crash used to surface as nothing at all.
 *
 * `ComposeSketch` loads the sketch as a dylib and calls into it. When the
 * sketch faults, the process died with exit 139 and printed NOTHING — not
 * the sketch's name, not what it was doing, not a frame. Four agents in
 * the overnight sketch program spent most of a night localising ONE bad
 * shader that way, and the failure looked arbitrary because it depended
 * on how deep the SkSL inliner got.
 *
 * So: name the sketch, name the phase, print the stack, and say the two
 * things that are almost always true. Handlers are async-signal-safe —
 * write(2) and backtrace_symbols_fd(3) only, no allocation, no printf.
 */

#include <csignal>
#include <filesystem>

namespace sigil::compose::sketch {

/** What the host was doing when the fault landed. Set by SketchHost
 *  around every call INTO the guest; `sig_atomic_t` because the handler
 *  reads it. */
enum class Phase : int {
  Host = 0,   // not inside the sketch at all — a host bug
  Setup,      // Sketch::setup()
  Update,     // Sketch::update()
  Draw,       // Composer::draw() — painting what the sketch described
  Capture,    // readback + PNG encode
};

/** Installs handlers for SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT. Idempotent
 *  and safe to call before the host exists. */
void installCrashReporter(const std::filesystem::path &sketchPath);

/** Scoped phase marker: `PhaseMark mark(Phase::Setup);` */
class PhaseMark {
public:
  explicit PhaseMark(Phase phase);
  ~PhaseMark();
  PhaseMark(const PhaseMark &) = delete;
  PhaseMark &operator=(const PhaseMark &) = delete;

private:
  Phase m_previous;
};

/** The frame counter the reporter prints; bumped by SketchHost::frame. */
void noteFrame(int index, double elapsedSeconds);

} // namespace sigil::compose::sketch
