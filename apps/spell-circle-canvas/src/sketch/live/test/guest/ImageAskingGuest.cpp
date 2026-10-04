/** @file
 * A GUEST IMAGE THAT LOADS A PICTURE THROUGH THE HOST'S HUB, built beside
 * the test binary as a rebuilt sketch is built beside its host and
 * compiled hidden as the host compiles a guest — so its `media::Image`
 * is a type identity of its own, and the one thing it shares with the
 * decoder the host registered is the meaning's name.
 *
 * Its canvas reports the size of `mark.png`, loaded beside the sketch
 * first and from its resource root when the local load fails. Without
 * an image, it reports the module-local declaration count by one pixel.
 * The size exposes both resource mounting and module isolation.
 * A local warn-on-missing-picture file opts into resource diagnostics.
 */

#include <sigilio/advanced/Problems.h>
#include <sigilio/hub/Hub.h>
#include <sigilmedia/core/Image.h>
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

/** Loads the local or resource `mark.png` each time it declares itself. */
class PictureSession final : public sketch::Session {
 public:
  PictureSession(sketch::Assets& assets, std::string key)
      : m_assets(assets), m_key(std::move(key)) {
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
  void declare() {
    static int declarations = 0;
    ++declarations;
    std::shared_ptr<const sigil::media::Image> picture =
        m_assets.hub().load<sigil::media::Image>("sketch://" + m_key +
                                                 "/mark.png");
    if (!picture)
      picture = m_assets.hub().load<sigil::media::Image>("res://mark.png");
    if (!picture &&
        m_assets.hub().read("sketch://" + m_key + "/warn-on-missing-picture"))
      sigil::io::reportProblem(
          m_assets.hub(), {"res://mark.png", "the picture is unavailable", {}});
    m_canvas.size = picture ? SkSize::Make((float)picture->size().x,
                                           (float)picture->size().y)
                            : SkSize{(float)declarations, 1};
  }

  sketch::Assets& m_assets;
  std::string m_key;
  sketch::CanvasSpecification m_canvas;
};

struct PictureKind final : sketch::KindOperations {
  [[nodiscard]] std::string_view runtime() const override { return "test"; }
  [[nodiscard]] std::unique_ptr<sketch::Session> open(
      sigil::weave::FontContext&, sketch::Assets& assets, bool,
      std::string_view key) const override {
    return std::make_unique<PictureSession>(assets, std::string(key));
  }
};

sketch::Kind pictureKind() { return sketch::Kind(PictureKind{}); }

}  // namespace

extern "C" __attribute__((visibility("default"))) const sketch::Entry*
sigilSketchEntry() {
  static const sketch::Entry entry{"", "image_asking_guest", "Test", "",
                                   &pictureKind};
  return &entry;
}

extern "C" __attribute__((visibility("default"))) unsigned sigilSketchAbi() {
  return sketch::kAbiVersion;
}
