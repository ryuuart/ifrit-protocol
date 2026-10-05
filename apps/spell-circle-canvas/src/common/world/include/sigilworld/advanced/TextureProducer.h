#pragma once

/** @file
 * @ingroup world-frame
 * THE SEAM A TEXTURE-PRODUCING PASS IS PERFORMED THROUGH: a pass that
 * paints textures a body then wears — a page painted as a surface's
 * maps, most often — declared on the frame like any other pass, so
 * nothing paints unless its pass is there. The library that paints
 * implements it; this one only orders it, runs it at the frame's time,
 * and refuses a frame whose bodies wear what no pass on it paints.
 */

#include <sigilcore/comparable/Erased.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace sigil::world {

/** WHAT A TEXTURE PASS DOES when its turn comes. */
class TextureProducerOperations {
 public:
  TextureProducerOperations() = default;
  TextureProducerOperations(const TextureProducerOperations&) = default;
  TextureProducerOperations(TextureProducerOperations&&) = default;
  TextureProducerOperations& operator=(const TextureProducerOperations&) =
      default;
  TextureProducerOperations& operator=(TextureProducerOperations&&) = default;
  virtual ~TextureProducerOperations() = default;

  /** Paints the textures at the scene time @p time. False when nothing
   *  it paints had changed and no pixel was touched — the work the frame
   *  counts as skipped. */
  virtual bool produce(std::chrono::duration<double> time) const = 0;
  /** Whether @p texture is one of the textures this producer paints. */
  [[nodiscard]] virtual bool paints(const material::Texture& texture) const = 0;
  /** WHAT A BODY WEARING THESE TEXTURES WEARS once they are painted, asked
   *  after `produce` of every body whose material samples one of them: a
   *  producer whose painting decides what the material needs — a page
   *  that learned which maps its content states — answers the material
   *  restated, and the body is drawn wearing that this frame. None leaves
   *  @p worn as it was described. */
  [[nodiscard]] virtual std::optional<material::Material> restated(
      const material::Material& worn) const {
    (void)worn;
    return std::nullopt;
  }
};

/** A producer carried as a comparable value. */
using TextureProducer = core::Erased<TextureProducerOperations>;

/** PIXELS A TEXTURE PASS PAINTS: another source's pixels, marked with the
 *  name of the pass that paints them, so a frame can tell a body wearing
 *  them that the pass is missing. It answers everything the source it
 *  marks answers. */
struct ProducedPixels {
  /** The name of the pass that paints these pixels. */
  std::string pass;
  media::PixelSource pixels;

  media::Frame frameAt(std::chrono::duration<double> time) const {
    return pixels.frameAt(time);
  }
  uint64_t revision() const { return pixels.revision(); }
  bool isRunning() const { return pixels.isRunning(); }
  glm::ivec2 size() const { return pixels.size(); }
  bool operator==(const ProducedPixels& other) const {
    return pass == other.pass && pixels == other.pixels;
  }
};

/** @p texture, its pixels marked as painted by the pass called @p pass —
 *  every other dial of it as it was. */
[[nodiscard]] material::Texture produced(const material::Texture& texture,
                                         std::string pass);

/** The name of the pass that paints @p texture; empty when no pass does. */
[[nodiscard]] std::string producingPass(const material::Texture& texture);

}  // namespace sigil::world
