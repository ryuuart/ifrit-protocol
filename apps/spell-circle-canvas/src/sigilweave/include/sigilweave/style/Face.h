#pragma once

/** @file
 * @ingroup weave-style
 *
 * A FACE: the typeface a style is set in, as every header of this library
 * holds it. Copies share the one face; two are equal when they are the
 * same face object, which is what the shape cache keys by. Null is no
 * face of its own — a style with none is set in the font context's
 * default family and its fallback chain.
 *
 * The typeface behind a face is Skia's, and `sigilweave/advanced/Skia.h`
 * is where the two meet: including it lets a Skia typeface stand wherever
 * a face is taken and a face be handed wherever a Skia typeface is, so a
 * caller that already holds one only adds that include.
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace sigil::weave {

class Face;

/** How a renderer's own typeface handle becomes a `Face` and back:
 *  `wrap` and `unwrap`, specialised by the header that names that
 *  renderer, which is why a caller holding one passes it wherever a face
 *  is taken once that header is included. */
template <class Native>
struct FaceAdapter;

/** Whether @p Native is a typeface handle some included renderer header
 *  adapts to a `Face`. */
template <class Native>
concept AdaptsToFace = requires(Native native) {
  {
    FaceAdapter<std::remove_cvref_t<Native>>::wrap(std::move(native))
  } -> std::same_as<Face>;
};

/** How upright a face is drawn. */
enum class FaceSlant : uint8_t { Upright, Italic, Oblique };

/** WHICH FACE OF A FAMILY is asked for: the CSS weight (100 thin to 900
 *  black, 400 regular), the width class (1 ultra-condensed to 9
 *  ultra-expanded, 5 normal) and the slant. A family answers with its
 *  nearest face. */
struct FaceStyle {
  int weight = 400;
  int width = 5;
  FaceSlant slant = FaceSlant::Upright;
  bool operator==(const FaceStyle&) const = default;
};

class Face {
 public:
  /** No face: the default family stands in. */
  Face() = default;
  Face(std::nullptr_t) {}  // NOLINT(google-explicit-constructor)
  /** A renderer's own typeface handle, once the header that adapts it is
   *  included. */
  template <class Native>
    requires AdaptsToFace<Native> &&
             (!std::same_as<std::remove_cvref_t<Native>, Face>)
  Face(Native native)  // NOLINT(google-explicit-constructor)
      : Face(FaceAdapter<std::remove_cvref_t<Native>>::wrap(
            std::move(native))) {}

  Face(const Face& other);
  Face(Face&& other) noexcept;
  Face& operator=(const Face& other);
  Face& operator=(Face&& other) noexcept;
  ~Face();

  /** The face as a renderer's own typeface handle, once the header that
   *  adapts it is included. */
  template <class Native>
    requires AdaptsToFace<Native>
  operator Native() const {  // NOLINT(google-explicit-constructor)
    return FaceAdapter<Native>::unwrap(*this);
  }

  /** Whether a face is held. */
  explicit operator bool() const { return m_native != nullptr; }
  /** The face object itself, for a cache that keys by identity; null for
   *  no face. */
  const void* identity() const { return m_native; }
  /** The family the face belongs to, as its font names it; empty for no
   *  face. */
  std::string familyName() const;
  /** The face's weight, width and slant, as its font states them; the
   *  regular upright for no face. */
  FaceStyle style() const;

  bool operator==(const Face& other) const {
    return m_native == other.m_native;
  }
  bool operator==(std::nullptr_t) const { return m_native == nullptr; }

 private:
  template <class Native>
  friend struct FaceAdapter;
  /** The renderer's typeface, declared and never defined here. */
  struct Handle;
  const Handle* m_native = nullptr;
};

}  // namespace sigil::weave
