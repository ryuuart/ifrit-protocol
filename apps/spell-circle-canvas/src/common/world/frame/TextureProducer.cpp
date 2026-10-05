/** @file
 * Pixels marked with the pass that paints them, and the question a frame
 * asks of a texture: which pass, if any, paints it.
 */

#include <sigilworld/advanced/TextureProducer.h>

#include <utility>

namespace sigil::world {

material::Texture produced(const material::Texture& texture, std::string pass) {
  material::Texture out(
      media::PixelSource(ProducedPixels{std::move(pass), texture.source()}));
  out.tile(texture.tileX(), texture.tileY())
      .uv(texture.uv())
      .sampling(texture.sampling());
  if (texture.region()) out.region(*texture.region());
  return out;
}

std::string producingPass(const material::Texture& texture) {
  const ProducedPixels* marked = texture.source().as<ProducedPixels>();
  return marked ? marked->pass : std::string();
}

}  // namespace sigil::world
