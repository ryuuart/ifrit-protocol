#pragma once

/** @file
 * A guest crash, made attributable.
 *
 * The live host loads a sketch as a dylib and calls into it, so a fault
 * inside one is a fault inside the host process — and without a handler
 * the process dies with a bare signal and prints nothing: not the
 * sketch's name, not what it was doing, not which frame. Localising a
 * fault from that costs hours, and the failure reads as arbitrary
 * because whether it lands at all can depend on how deep a shader
 * compiler inlined.
 *
 * So: name the sketch, name the phase, print the stack. Handlers are
 * async-signal-safe — write(2) and backtrace_symbols_fd(3) only, no
 * allocation, no printf.
 */

#include <csignal>
#include <filesystem>

namespace sigil::sketch {

/** What the host was doing when the fault landed. Set around every call
 *  INTO the guest; `sig_atomic_t` underneath, because the handler reads
 *  it. */
enum class Phase : int {
  Host = 0,  // not inside the sketch at all — a host bug
  Setup,     // the sketch declaring itself
  Update,    // the sketch reacting to a frame
  Draw,      // the runtime painting what the sketch described
  Capture,   // readback + PNG encode
};

/** Installs handlers for SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT. Idempotent
 *  and safe to call before a host exists. */
void installCrashReporter(const std::filesystem::path& sketchPath);

/** Scoped phase marker: `PhaseMark mark(Phase::Setup);` */
class PhaseMark {
 public:
  explicit PhaseMark(Phase phase);
  ~PhaseMark();
  PhaseMark(const PhaseMark&) = delete;
  PhaseMark& operator=(const PhaseMark&) = delete;

 private:
  Phase m_previous;
};

/** The frame counter the reporter prints. */
void noteFrame(int index, double elapsedSeconds);

}  // namespace sigil::sketch
