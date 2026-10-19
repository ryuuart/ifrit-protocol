#pragma once

/** @file
 * The window's evaluate door: what `--repl` opens on the endpoint the
 * window mounts.
 *
 * The protocol's own types stand behind a pointer, so the window's
 * translation units that read this header compile none of its headers.
 */

#include <filesystem>
#include <memory>

namespace sigil::protocol {
class Dispatcher;
}

/** THE WINDOW'S EVALUATE DOOR, opened by `--repl`: `session.evaluate`
 *  and `session.still` mounted on the window's dispatcher and answered
 *  for the sketch on screen. The window's frames are its render
 *  thread's, so each command is handed to that thread and runs there
 *  between two frames, where the sketch's own hooks run; its answer
 *  comes back to the thread that drives the dispatcher. Every other
 *  session command, and the clock, stay unmounted.
 *
 *  A still is the sketch on screen as it stands, the clock untouched,
 *  written as a PNG under @p stateRoot.
 *
 *  It is made after the dispatcher and let go before it, and after the
 *  window's render thread has stopped: an answer still on its way when
 *  it goes is answered `failed`. */
class WindowEvaluation {
 public:
  WindowEvaluation(sigil::protocol::Dispatcher& dispatcher,
                   std::filesystem::path stateRoot);
  ~WindowEvaluation();

  WindowEvaluation(const WindowEvaluation&) = delete;
  WindowEvaluation& operator=(const WindowEvaluation&) = delete;

 private:
  /** The replies owed and where their answers arrive. */
  struct State;
  std::unique_ptr<State> m_state;
};
