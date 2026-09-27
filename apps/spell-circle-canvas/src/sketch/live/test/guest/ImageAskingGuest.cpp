/** @file
 * A GUEST IMAGE THAT LOADS A PICTURE THROUGH THE HOST'S HUB, built beside
 * the test binary as a rebuilt sketch is built beside Sketchbook and
 * compiled hidden as the host compiles a guest — so its `media::Image`
 * is a type identity of its own, and the one thing it shares with the
 * decoder the host registered is the meaning's name.
 *
 * Its one kind opens a session whose canvas is the size of `mark.png`
 * standing beside the sketch, asked for whenever it declares itself, or
 * one pixel square when the load answered nothing: the size is how a
 * case reads, from outside the guest, what the guest's load found.
 */

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

/** Loads `mark.png` beside the sketch each time it declares itself. */
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
    const std::shared_ptr<const sigil::media::Image> picture =
        m_assets.hub().load<sigil::media::Image>("sketch://" + m_key +
                                                 "/mark.png");
    m_canvas.size = picture ? SkSize::Make(picture->size()) : SkSize{1, 1};
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
