/** @file
 * A GUEST IMAGE FOR THE LIVE HOST'S TESTS TO ADOPT, built beside the test
 * binary rather than by the host's compiler, so a case reaches the path a
 * rebuilt workspace sketch takes — dlopen, the ABI check, the entry, the
 * open — without a compile.
 *
 * Its one kind opens a session that asks the host's assets for the shader
 * standing beside the sketch, whenever it declares itself, and draws
 * nothing. `Assets::shader` is resolved out of the test binary the way a
 * compiled sketch resolves the framework out of Sketchbook, and the model
 * has no `==`, so nothing whose identity differs between two images is
 * compared across them.
 */

#include <include/effects/SkRuntimeEffect.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/core/Kind.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace sketch = sigil::sketch;

/** Asks for `fill.sksl` beside the sketch each time it declares itself. */
class AskingSession final : public sketch::Session {
 public:
  AskingSession(sketch::Assets& assets, std::string key)
      : m_assets(assets), m_key(std::move(key)) {
    m_canvas.size = {40, 30};
    declare();
  }

  [[nodiscard]] const sketch::CanvasSpecification& canvas() const override {
    return m_canvas;
  }
  void frame(SkCanvas&, double) override {}
  void repaint(SkCanvas&) override {}
  void still(SkCanvas&) override {}
  void redeclare() override { declare(); }
  [[nodiscard]] sketch::Timing timing() const override { return {}; }

 private:
  void declare() { (void)m_assets.shader("sketch://" + m_key + "/fill.sksl"); }

  sketch::Assets& m_assets;
  std::string m_key;
  sketch::CanvasSpecification m_canvas;
};

struct AskingKind final : sketch::KindOperations {
  [[nodiscard]] std::string_view runtime() const override { return "test"; }
  [[nodiscard]] std::unique_ptr<sketch::Session> open(
      sigil::weave::FontContext&, sketch::Assets& assets, bool,
      std::string_view key) const override {
    return std::make_unique<AskingSession>(assets, std::string(key));
  }
};

sketch::Kind askingKind() { return sketch::Kind(AskingKind{}); }

}  // namespace

extern "C" __attribute__((visibility("default"))) const sketch::Entry*
sigilSketchEntry() {
  static const sketch::Entry entry{"", "shader_asking_guest", "Test", "",
                                   &askingKind};
  return &entry;
}

extern "C" __attribute__((visibility("default"))) unsigned sigilSketchAbi() {
  return sketch::kAbiVersion;
}
