#pragma once

/** @file
 * @ingroup media-core
 * `Picture`, one decoded picture held as a shared, immutable value: what a
 * `Frame` carries and what a `PixelSource` holds in hand. The executor's
 * image stands behind it unnamed, so no header that carries a picture
 * reaches the renderer; `advanced/Skia.h` converts both ways.
 */

#include <glm/vec2.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace sigil::media {

class Picture;

/** HOW A RENDERER'S OWN IMAGE HANDLE BECOMES A `Picture` AND BACK:
 *  specialised by the header that speaks that renderer, with
 *  `static Picture wrap(Native)` and `static Native unwrap(const
 *  Picture&)`. `advanced/Skia.h` specialises it for Skia's image, which is
 *  why a caller holding one passes it wherever a picture is taken once
 *  that header is included. */
template <class Native>
struct PictureAdapter;

/** Whether @p Native is an image handle some included renderer header
 *  adapts to a `Picture`. */
template <class Native>
concept AdaptsToPicture = requires(Native native) {
  {
    PictureAdapter<std::remove_cvref_t<Native>>::wrap(std::move(native))
  } -> std::same_as<Picture>;
};

/** ONE DECODED PICTURE, premultiplied and immutable: a still, a frame of
 *  an animation or a video, a texture another renderer painted and bound.
 *  Copies share the one picture; two are equal when they are the same
 *  picture object, which is what a cache keys by. Empty until given one. */
class Picture {
 public:
  /** No picture. */
  Picture() = default;
  Picture(std::nullptr_t) {}  // NOLINT(google-explicit-constructor)
  /** A renderer's own image handle, once the header that adapts it is
   *  included. */
  template <class Native>
    requires AdaptsToPicture<Native> &&
             (!std::same_as<std::remove_cvref_t<Native>, Picture>)
  Picture(Native native)  // NOLINT(google-explicit-constructor)
      : Picture(PictureAdapter<std::remove_cvref_t<Native>>::wrap(
            std::move(native))) {}

  Picture(const Picture& other);
  Picture(Picture&& other) noexcept;
  Picture& operator=(const Picture& other);
  Picture& operator=(Picture&& other) noexcept;
  ~Picture();

  /** The picture as a renderer's own image handle, once the header that
   *  adapts it is included. */
  template <class Native>
    requires AdaptsToPicture<Native>
  operator Native() const {  // NOLINT(google-explicit-constructor)
    return PictureAdapter<Native>::unwrap(*this);
  }

  /** Whether a picture is held. */
  explicit operator bool() const { return m_native != nullptr; }
  /** Its size in pixels; zero for no picture. */
  glm::ivec2 size() const;
  /** The picture object itself, for a cache that keys by identity; null
   *  for no picture. */
  const void* identity() const { return m_native; }

  bool operator==(const Picture& other) const {
    return m_native == other.m_native;
  }
  bool operator==(std::nullptr_t) const { return m_native == nullptr; }

 private:
  template <class Native>
  friend struct PictureAdapter;
  /** The executor's image, declared and never defined here. */
  struct Handle;
  const Handle* m_native = nullptr;
};

}  // namespace sigil::media
