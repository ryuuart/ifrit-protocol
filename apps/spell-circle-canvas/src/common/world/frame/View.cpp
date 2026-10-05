/** @file
 * A body as a collection reads it, a texture as a mesh samples it, and the
 * crop that carries a whole picture onto the part of it a frame forms.
 */

#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/View.h>

namespace sigil::world {

Sampling samplingOf(const material::Texture& texture) {
  Sampling out;
  out.image = material::skia::image(texture);
  // A source may hold its pixels on a device and NOWHERE ELSE, in which
  // case there is no host image to read and the size comes from where
  // they stand — the placement is a question about the picture, not
  // about which side of the bus it is on.
  const material::DeviceImage where = texture.deviceImage();
  if (!out.image && !where) return {};
  // Either axis repeating is a repeat: a mesh's sampler has one wrap for
  // both, and clamping the axis that was asked to repeat would drag one
  // edge's pixels across the whole face.
  out.tile = texture.tileX() != material::Repeat::Pad ||
             texture.tileY() != material::Repeat::Pad;
  out.filter = texture.sampling();

  const SkISize size = out.image ? media::toSk(out.image.size())
                                 : SkISize::Make(where.width, where.height);
  SkMatrix lookup;
  if (size.isEmpty() ||
      !material::skia::toSkMatrix(texture.uv()).invert(&lookup))
    return out;
  SkMatrix uv =
      SkMatrix::Scale(1.0f / (float)size.width(), 1.0f / (float)size.height());
  uv.preConcat(lookup);
  uv.preConcat(SkMatrix::Scale((float)size.width(), (float)size.height()));
  out.uv = fromSk(uv);
  return out;
}

Member memberOf(const Draw& draw) {
  return Member{draw.key, draw.collections, draw.ancestors};
}

bool isPart(const ViewOffset& offset, glm::ivec2 extent) {
  if (offset.whole.x <= 0 || offset.whole.y <= 0) return false;
  if (extent.x <= 0 || extent.y <= 0) return false;
  return offset.whole != extent || offset.origin != glm::ivec2{0, 0};
}

glm::mat4 cropOf(const ViewOffset& offset, glm::ivec2 extent) {
  if (!isPart(offset, extent)) return glm::mat4(1.0f);
  // Clip x runs from -1 at the picture's left column to +1 at its right,
  // and clip y from +1 at its top row to -1 at its bottom — the rows run
  // down the picture and y up the clip space. The part's own edges are
  // put at those same values, so the part's columns and rows are the
  // whole picture's columns and rows from `origin` on.
  const glm::vec2 whole{(float)offset.whole.x, (float)offset.whole.y};
  const glm::vec2 part{(float)extent.x, (float)extent.y};
  const glm::vec2 origin{(float)offset.origin.x, (float)offset.origin.y};
  glm::mat4 crop(1.0f);
  crop[0][0] = whole.x / part.x;
  crop[1][1] = whole.y / part.y;
  // A shift in clip space is a multiple of w, which is what keeps it
  // exact through the perspective divide.
  crop[3][0] = (whole.x - 2.0f * origin.x - part.x) / part.x;
  crop[3][1] = (part.y - whole.y + 2.0f * origin.y) / part.y;
  return crop;
}

glm::ivec2 wholeOf(const View& view) {
  return isPart(view.offset, view.extent) ? view.offset.whole : view.extent;
}

glm::mat4 clipProjection(const View& view) {
  const glm::mat4 whole = view.camera.clipProjection(wholeOf(view));
  if (!isPart(view.offset, view.extent)) return whole;
  return cropOf(view.offset, view.extent) * whole;
}

}  // namespace sigil::world
