#include "SketchCrash.h"

#include <array>
#include <atomic>
#include <cstring>

#if defined(__APPLE__) || defined(__unix__)
#include <execinfo.h>
#include <unistd.h>
#endif

namespace sigil::compose::sketch {

namespace {

// Everything the handler touches is either a plain scalar or a fixed
// buffer written BEFORE any fault can occur. No allocation, no locking,
// no printf — a handler that itself faults tells you nothing.
constexpr size_t kPathMax = 1024;
char g_sketchPath[kPathMax] = {0};
volatile std::sig_atomic_t g_phase = (int)Phase::Host;
volatile std::sig_atomic_t g_frame = -1;
volatile std::sig_atomic_t g_millis = 0; // elapsed ms, for the phase line
volatile std::sig_atomic_t g_installed = 0;

void emit(const char *text) {
#if defined(__APPLE__) || defined(__unix__)
  const size_t length = std::strlen(text);
  ssize_t written = ::write(STDERR_FILENO, text, length);
  (void)written;
#else
  (void)text;
#endif
}

/** Async-signal-safe unsigned decimal. */
void emitNumber(long value) {
  char digits[24];
  int index = (int)sizeof digits;
  digits[--index] = '\0';
  const bool negative = value < 0;
  unsigned long magnitude = negative ? (unsigned long)(-value) : (unsigned long)value;
  if (magnitude == 0)
    digits[--index] = '0';
  while (magnitude > 0 && index > 0) {
    digits[--index] = (char)('0' + (magnitude % 10));
    magnitude /= 10;
  }
  if (negative && index > 0)
    digits[--index] = '-';
  emit(digits + index);
}

const char *phaseName(int phase) {
  switch ((Phase)phase) {
  case Phase::Setup: return "Sketch::setup()";
  case Phase::Update: return "Sketch::update()";
  case Phase::Draw: return "Composer::draw() — painting the described tree";
  case Phase::Capture: return "capture (readback + PNG encode)";
  case Phase::Host: break;
  }
  return "host code (NOT inside the sketch)";
}

const char *signalName(int number) {
  switch (number) {
  case SIGSEGV: return "SIGSEGV (bad memory access)";
  case SIGBUS: return "SIGBUS (bad address / alignment)";
  case SIGILL: return "SIGILL (illegal instruction — often a bad vtable "
                      "or a pointer-authentication failure)";
  case SIGFPE: return "SIGFPE (arithmetic fault)";
  case SIGABRT: return "SIGABRT (abort — an uncaught exception reaches here, "
                       "e.g. bad_function_call from an empty easing)";
  default: break;
  }
  return "fatal signal";
}

void handler(int number) {
  emit("\n=== ComposeSketch: the sketch crashed ===\n  signal: ");
  emit(signalName(number));
  emit("\n  sketch: ");
  emit(g_sketchPath[0] ? g_sketchPath : "<unknown>");
  emit("\n  phase:  ");
  emit(phaseName((int)g_phase));
  if (g_frame >= 0) {
    emit("\n  frame:  ");
    emitNumber((long)g_frame);
    emit("  (t = ");
    emitNumber((long)g_millis);
    emit(" ms)");
  }
  emit("\n\nThis is a fault inside the SKETCH image, not the host. The two\n"
       "that account for almost all of them:\n"
       "  * a stock SkSL material whose main() is not monolithic, or that\n"
       "    has a uniform-guarded loop bound. A sketch dylib links its own\n"
       "    libskia.a, so the AST is built in the sketch's Skia image while\n"
       "    the inliner runs in the host's, and dispatch across that\n"
       "    boundary faults on PAC. See the rule beside patterns::grain in\n"
       "    Patterns.h, and sketches/stock_materials.cpp.\n"
       "  * a steppable that captured SketchContext& — it is a per-frame\n"
       "    value the host rebuilds, so the reference dangles. Capture\n"
       "    `this` and take the context as a parameter.\n\nstack:\n");

#if defined(__APPLE__) || defined(__unix__)
  std::array<void *, 64> frames{};
  const int depth = ::backtrace(frames.data(), (int)frames.size());
  ::backtrace_symbols_fd(frames.data(), depth, STDERR_FILENO);
#endif
  emit("\n");

  // Restore the default and re-raise so the shell still reports the real
  // signal (and a debugger still sees it) rather than a synthesised code.
  std::signal(number, SIG_DFL);
  std::raise(number);
}

} // namespace

void installCrashReporter(const std::filesystem::path &sketchPath) {
  const std::string text = sketchPath.string();
  const size_t length = text.size() < kPathMax - 1 ? text.size() : kPathMax - 1;
  std::memcpy(g_sketchPath, text.data(), length);
  g_sketchPath[length] = '\0';
  if (g_installed)
    return;
  g_installed = 1;
  for (int number : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
    std::signal(number, handler);
}

PhaseMark::PhaseMark(Phase phase) : m_previous((Phase)(int)g_phase) {
  g_phase = (int)phase;
}
PhaseMark::~PhaseMark() { g_phase = (int)m_previous; }

void noteFrame(int index, double elapsedSeconds) {
  g_frame = index;
  g_millis = (std::sig_atomic_t)(elapsedSeconds * 1000.0);
}

} // namespace sigil::compose::sketch
