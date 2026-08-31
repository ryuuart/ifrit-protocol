/** @file
 * A body as a selector reads it.
 */

#include <sigilworld/frame/View.h>

namespace sigil::world {

Sampling samplingOf(const material::Texture& texture) {
  Sampling out;
  out.image = texture.image();
  // A source may hold its pixels on a device and NOWHERE ELSE, in which
  // case there is no host image to read and the size comes from where
  // they stand — the placement is a question about the picture, not
  // about which side of the bus it is on.
  const material::DeviceImage where = texture.deviceImage();
  if (!out.image && !where) return {};
  // Either axis repeating is a repeat: a mesh's sampler has one wrap for
  // both, and clamping the axis that was asked to repeat would drag one
  // edge's pixels across the whole face.
  out.tile = texture.tileX() != SkTileMode::kClamp ||
             texture.tileY() != SkTileMode::kClamp;
  out.filter = texture.filter();

  const SkISize size = out.image ? out.image->dimensions()
                                 : SkISize::Make(where.width, where.height);
  SkMatrix lookup;
  if (size.isEmpty() || !texture.uv().invert(&lookup)) return out;
  out.uv =
      SkMatrix::Scale(1.0f / (float)size.width(), 1.0f / (float)size.height());
  out.uv.preConcat(lookup);
  out.uv.preConcat(SkMatrix::Scale((float)size.width(), (float)size.height()));
  return out;
}

Subject subjectOf(const Draw& draw) {
  return Subject{draw.key, draw.tags, draw.ancestors, draw.material};
}

}  // namespace sigil::world
