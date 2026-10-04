/** @file
 * A guest image whose exported callback throws an exception defined in
 * the guest itself.
 *
 * Built once per mode: `factory` throws a standard exception from the
 * kind factory, `metadata` from the ABI query, and `frame` opens a
 * session whose frame throws a value that is not a standard exception.
 */

#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/core/Kind.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <exception>
#include <memory>
#include <string_view>

namespace {

struct GuestException final : std::exception {
  const char* what() const noexcept override {
    return "the guest callback failed";
  }
};

#ifdef SIGIL_TEST_THROW_frame
/** Opens normally and throws an integer from every frame. */
class ThrowingFrameSession final : public sigil::sketch::Session {
 public:
  [[nodiscard]] const sigil::sketch::CanvasSpecification& canvas()
      const override {
    return m_canvas;
  }
  void frame(SkCanvas&, double) override { throw 7; }
  void repaint(SkCanvas&) override {}
  void still(SkCanvas&) override {}
  [[nodiscard]] sigil::sketch::Timing timing() const override { return {}; }

 private:
  sigil::sketch::CanvasSpecification m_canvas;
};

struct ThrowingFrameKind final : sigil::sketch::KindOperations {
  [[nodiscard]] std::string_view runtime() const override { return "test"; }
  [[nodiscard]] std::unique_ptr<sigil::sketch::Session> open(
      sigil::weave::FontContext&, sigil::sketch::Assets&, bool,
      std::string_view) const override {
    return std::make_unique<ThrowingFrameSession>();
  }
};

sigil::sketch::Kind throwingKind() {
  return sigil::sketch::Kind(ThrowingFrameKind{});
}
#else
sigil::sketch::Kind throwingKind() { throw GuestException{}; }
#endif

}  // namespace

extern "C" __attribute__((visibility("default"))) const sigil::sketch::Entry*
sigilSketchEntry() {
  static const sigil::sketch::Entry entry{"", "throwing_guest", "Test", "",
                                          &throwingKind};
  return &entry;
}

extern "C" __attribute__((visibility("default"))) unsigned sigilSketchAbi() {
#ifdef SIGIL_TEST_THROW_metadata
  throw GuestException{};
#else
  return sigil::sketch::kAbiVersion;
#endif
}
