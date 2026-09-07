#pragma once

/** @file
 * A guest crash, made attributable.
 *
 * EVERY HOST HERE HAS A GUEST. The live host loads a sketch as a dylib
 * and calls into it; the headless sweep opens a hundred sketches in one
 * process and calls into each. A fault inside one is a fault inside the
 * host either way — and without a handler the process dies with a bare
 * signal and prints nothing: not the sketch's name, not what it was
 * doing, not which frame, not how far a run had got. Localising a fault
 * from that costs hours, and the failure reads as arbitrary because
 * whether it lands at all can depend on how deep a shader compiler
 * inlined.
 *
 * So: name the sketch, name the phase, say how many plates are behind
 * it, print the stack. Handlers are async-signal-safe — write(2) and
 * backtrace_symbols_fd(3) only, no allocation, no printf — and
 * everything they read is a fixed buffer or a scalar written before any
 * fault could occur.
 */

#include <csignal>
#include <filesystem>
#include <string_view>

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

/** THE SKETCH THE REPORTER NAMES, for a host that walks many of them.
 *  A sweep's fault is attributable only by the entry it was on, and the
 *  last line on stderr is not that — it is whatever the sketch before it
 *  printed. Copied into a fixed buffer, so the handler reads a name that
 *  cannot move under it; it supersedes the path `installCrashReporter`
 *  was given, and an empty name restores that path. */
void noteSketch(std::string_view name);

/** How many plates a run has finished. It is what says whether a fault
 *  is at the start of a sweep or two hours into one, which decides
 *  whether the next run can be narrowed to the sketch that faulted. */
void notePlates(int count);

}  // namespace sigil::sketch
