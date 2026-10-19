#pragma once

/** @file
 * @ingroup sketch-window
 *
 * A window that shows one live host: opened by whoever needs a sketch on
 * screen, handed a host, pumped by the thread that made it, photographed
 * on request and closed. The header names no Qt, so a process that never
 * opens a window carries none.
 */

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace sigil::sketch {

class Host;

/** WHAT A WINDOW IS OPENED WITH. Every field stands at what a window an
 *  author opens from a prompt wants, so a caller states only the fields
 *  its situation differs in. */
struct WindowOptions {
  /** The pane's size in points; the sketch's own canvas is fitted into
   *  it, centred, and never stretched. */
  int width = 960;
  int height = 540;
  /** What the window's title bar reads. */
  std::string title = "Sigil";
  /** NO WINDOW ON SCREEN: the host is stepped on the wall's clock each
   *  time the window is pumped and photographed through its own capture
   *  path, with no window system and no graphics device asked for. A
   *  test, a machine with no display, or a script that only writes
   *  stills opens one this way. */
  bool headless = false;
};

/** ONE WINDOW AND THE HOST IT SHOWS.
 *
 *  The window belongs to the thread that opened it, which on macOS must
 *  be the process's main thread: that thread pumps it, and the frames are
 *  drawn there too, inside the pump, so a sketch's own code runs on the
 *  same thread as the caller's and never beside it. Between two pumps
 *  nothing moves.
 *
 *  An abstract door rather than a class: a process can reach it through a
 *  library it loaded at run time, by the one function below, without
 *  linking the window system itself. */
class Window {
 public:
  /** Opens a window, or answers null with the reason in @p why: the
   *  calling thread is not the main one, or the window system could not
   *  start. A process that already runs a Qt application has its window
   *  joined to it. */
  [[nodiscard]] static std::unique_ptr<Window> open(
      const WindowOptions& options, std::string* why = nullptr);

  virtual ~Window() = default;
  Window() = default;
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  /** PRESENTS @p host from the next frame on, letting go of the one shown
   *  before. The host's captures are routed through the window's own
   *  graphics context where it has one, so a still and a frame draw the
   *  same way. Null shows nothing. */
  virtual void show(std::unique_ptr<Host> host) = 0;
  /** The host on screen, or null. */
  [[nodiscard]] virtual Host* host() = 0;
  /** TURNS THE WINDOW for up to @p patience: its events are handled and,
   *  where a frame is due, the host is polled and drawn. Answers whether
   *  the window is still open — false once the reader has closed it. */
  virtual bool pump(std::chrono::milliseconds patience) = 0;
  /** WRITES THE SKETCH ON SCREEN to @p path as a picture, at @p density
   *  pixels per canvas unit. False, with nothing written, where no host
   *  is live or the picture could not be written. */
  virtual bool still(const std::filesystem::path& path, float density) = 0;
  /** Takes the window off screen and lets the host go. Pumping a closed
   *  window answers false. */
  virtual void close() = 0;
  /** Whether the window is on screen, or, headless, not yet closed. */
  [[nodiscard]] virtual bool isOpen() const = 0;
};

}  // namespace sigil::sketch

/** THE DOOR A PROCESS FINDS BY NAME in a window library it loaded itself:
 *  `Window::open` under C linkage, so `dlsym` reaches it without a
 *  mangled spelling. The caller owns the window answered; @p why is
 *  written when it is null. */
extern "C" sigil::sketch::Window* sigil_sketch_window_open(
    const sigil::sketch::WindowOptions* options, std::string* why);
