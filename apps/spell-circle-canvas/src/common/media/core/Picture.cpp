/** @file
 * A picture's sharing: the executor's image is reference-counted, and a
 * `Picture` holds one reference for as long as it stands.
 */

#include "sigilmedia/core/Picture.h"

#include "sigilmedia/advanced/Skia.h"

namespace sigil::media {

namespace {

const SkImage* image(const void* native) {
  return static_cast<const SkImage*>(native);
}

}  // namespace

Picture::Picture(const Picture& other) : m_native(other.m_native) {
  SkSafeRef(image(m_native));
}

Picture::Picture(Picture&& other) noexcept
    : m_native(std::exchange(other.m_native, nullptr)) {}

Picture& Picture::operator=(const Picture& other) {
  if (this != &other) {
    SkSafeRef(image(other.m_native));
    SkSafeUnref(image(m_native));
    m_native = other.m_native;
  }
  return *this;
}

Picture& Picture::operator=(Picture&& other) noexcept {
  if (this != &other) {
    SkSafeUnref(image(m_native));
    m_native = std::exchange(other.m_native, nullptr);
  }
  return *this;
}

Picture::~Picture() { SkSafeUnref(image(m_native)); }

glm::ivec2 Picture::size() const {
  if (!m_native) return {0, 0};
  return {image(m_native)->width(), image(m_native)->height()};
}

Picture PictureAdapter<sk_sp<SkImage>>::wrap(sk_sp<SkImage> native) {
  Picture picture;
  picture.m_native = reinterpret_cast<const Picture::Handle*>(native.release());
  return picture;
}

sk_sp<SkImage> PictureAdapter<sk_sp<SkImage>>::unwrap(const Picture& picture) {
  return sk_ref_sp(const_cast<SkImage*>(
      reinterpret_cast<const SkImage*>(picture.m_native)));
}

}  // namespace sigil::media
