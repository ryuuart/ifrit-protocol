#pragma once

/** @file
 * Texture — an image and how it is sampled, as a comparable value a
 * material tree can hold in a child slot. The image comes from a
 * TextureSource: a decoded still, a frame of an image asset, or a named
 * producer that bakes on first use. Sampling is the tiling per axis, a
 * uv matrix placing texture space in the sampled space, a region of the
 * image to read, and the filter.
 */

#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

namespace sigil::material {

/** WHERE A SOURCE'S PIXELS ALREADY LIVE, when they live on a GPU: the
 *  device that owns the texture, and the texture itself as the graphics
 *  API's own object bridged to opaque values — a pointer for one API's
 *  texture object, an integer for another's image handle, with the
 *  format and the layout it was left in.
 *
 *  Nothing in this library reads any of it. It is carried, unexamined,
 *  from a source that painted on a device to a renderer standing on the
 *  SAME device, which binds those pixels instead of uploading a copy of
 *  `image()`. A renderer holding another device — or none — compares
 *  `device` against its own, finds it different, and reads `image()`
 *  exactly as it reads every other source's. Empty unless a source was
 *  given a device to paint on, which most never are. */
struct DeviceImage {
  const void* device = nullptr;
  const void* pointer = nullptr;
  uint64_t handle = 0;
  uint32_t format = 0;
  uint32_t layout = 0;
  int width = 0;
  int height = 0;

  explicit operator bool() const {
    return device != nullptr && (pointer != nullptr || handle != 0);
  }
  bool operator==(const DeviceImage&) const = default;
};

/** What a texture source must be: it yields an image, says whether that
 *  image can change between frames, and compares by value. */
template <class S>
concept TextureSourceType =
    std::equality_comparable<S> && requires(const S& s) {
      { s.image() } -> std::convertible_to<sk_sp<SkImage>>;
      { s.animated() } -> std::convertible_to<bool>;
    };

/** …and what it MAY be besides: a source whose pixels stand on a device
 *  says where. One optional member, so every source that has no device
 *  is written exactly as it was. */
template <class S>
concept DeviceTextureSource = requires(const S& s) {
  { s.deviceImage() } -> std::convertible_to<DeviceImage>;
};

/** A source held by value with its type erased, comparable across the
 *  erasure: two sources are equal when they are the same source type and
 *  that type says they are equal. Empty until given a source. */
class TextureSource {
 public:
  TextureSource() = default;
  template <TextureSourceType S>
  TextureSource(S source)  // NOLINT(google-explicit-constructor)
      : m_impl(std::make_shared<Model<S>>(std::move(source))) {}

  bool valid() const { return m_impl != nullptr; }
  /** The source's image now; null when the source is empty or yields
   *  nothing. */
  sk_sp<SkImage> image() const { return m_impl ? m_impl->image() : nullptr; }
  bool animated() const { return m_impl && m_impl->animated(); }
  /** Where the source's pixels already stand, when they stand on a
   *  device; empty for every source that has none. */
  DeviceImage deviceImage() const {
    return m_impl ? m_impl->deviceImage() : DeviceImage{};
  }
  /** The held source when it is an @p S, else null. */
  template <class S>
  const S* as() const {
    auto* m = dynamic_cast<const Model<S>*>(m_impl.get());
    return m ? &m->value : nullptr;
  }

  bool operator==(const TextureSource& other) const {
    if (!m_impl || !other.m_impl) return m_impl == other.m_impl;
    return m_impl->equals(*other.m_impl);
  }

 private:
  struct Concept {
    virtual ~Concept() = default;
    virtual sk_sp<SkImage> image() const = 0;
    virtual bool animated() const = 0;
    virtual DeviceImage deviceImage() const = 0;
    virtual bool equals(const Concept& other) const = 0;
  };
  template <class S>
  struct Model final : Concept {
    explicit Model(S v) : value(std::move(v)) {}
    sk_sp<SkImage> image() const override { return value.image(); }
    bool animated() const override { return value.animated(); }
    DeviceImage deviceImage() const override {
      if constexpr (DeviceTextureSource<S>) return value.deviceImage();
      return DeviceImage{};
    }
    bool equals(const Concept& other) const override {
      auto* o = dynamic_cast<const Model<S>*>(&other);
      return o && value == o->value;
    }
    S value;
  };
  std::shared_ptr<const Concept> m_impl;
};

/** A decoded still. Equal when it is the same image object. */
struct ImageSource {
  sk_sp<SkImage> still;

  sk_sp<SkImage> image() const { return still; }
  bool animated() const { return false; }
  bool operator==(const ImageSource& other) const {
    return still.get() == other.still.get();
  }
};

/** One frame of an image asset, chosen by playback time. Equal when it
 *  is the same asset at the same time; animated when the asset is. */
struct AssetSource {
  std::shared_ptr<const image::ImageAsset> asset;
  double milliseconds = 0.0;

  sk_sp<SkImage> image() const {
    return asset ? asset->frameAt(milliseconds).image : nullptr;
  }
  bool animated() const { return asset && asset->animated(); }
  bool operator==(const AssetSource& other) const {
    return asset.get() == other.asset.get() &&
           milliseconds == other.milliseconds;
  }
};

/** An image baked on first use by a producer and kept. The key IS the
 *  identity: two producers under one key are the same texture, so the key
 *  must name the picture and every parameter that shaped it. */
class ProducerSource {
 public:
  ProducerSource(std::string key, std::function<sk_sp<SkImage>()> produce);

  const std::string& key() const { return m_key; }
  sk_sp<SkImage> image() const;
  bool animated() const { return false; }
  bool operator==(const ProducerSource& other) const {
    return m_key == other.m_key;
  }

 private:
  struct State;
  std::string m_key;
  std::shared_ptr<State> m_state;
};

/** An image and its sampling. A value: copy it, change a dial on the
 *  copy, keep both. As a Leaf it fills a material's child slot, and the
 *  Skia backend binds it as the image shader `shader()` builds. */
class Texture : public ShaderLeaf {
 public:
  Texture() = default;
  explicit Texture(TextureSource source) : m_source(std::move(source)) {}

  /** A texture over a decoded still. */
  static Texture of(sk_sp<SkImage> image) {
    return Texture(ImageSource{std::move(image)});
  }
  /** A texture over an asset's frame at @p milliseconds of playback. */
  static Texture of(std::shared_ptr<const image::ImageAsset> asset,
                    double milliseconds = 0.0) {
    return Texture(AssetSource{std::move(asset), milliseconds});
  }
  /** A texture baked by @p produce on first use, identified by @p key. */
  static Texture produce(std::string key,
                         std::function<sk_sp<SkImage>()> produce) {
    return Texture(ProducerSource(std::move(key), std::move(produce)));
  }

  /** The tiling per axis outside the image (or the region). */
  Texture& tile(SkTileMode x, SkTileMode y) {
    m_tileX = x;
    m_tileY = y;
    return *this;
  }
  Texture& tile(SkTileMode both) { return tile(both, both); }
  /** Texture space into the sampled space: where pixel (0, 0) lands and
   *  how the image is scaled and turned. */
  Texture& uv(const SkMatrix& matrix) {
    m_uv = matrix;
    return *this;
  }
  /** `uv(Translate(origin))`: the image's corner at @p origin. */
  Texture& at(SkPoint origin) {
    return uv(SkMatrix::Translate(origin.fX, origin.fY));
  }
  /** Reads only @p rect of the image, whose corner becomes pixel (0, 0). */
  Texture& region(SkIRect rect) {
    m_region = rect;
    return *this;
  }
  /** The filter between samples: linear (the default) or nearest. */
  Texture& filter(SkFilterMode mode) {
    m_filter = mode;
    return *this;
  }

  const TextureSource& source() const { return m_source; }
  SkTileMode tileX() const { return m_tileX; }
  SkTileMode tileY() const { return m_tileY; }
  const SkMatrix& uv() const { return m_uv; }
  const std::optional<SkIRect>& region() const { return m_region; }
  SkFilterMode filter() const { return m_filter; }

  bool valid() const { return m_source.valid(); }
  /** The image sampled: the source's, cut to the region when one is set.
   *  Null when the source yields nothing. */
  sk_sp<SkImage> image() const;
  /** The sampled image's size, zero when there is none. */
  SkISize size() const;
  /** The Skia shader: `image()` tiled, filtered and placed by `uv()`.
   *  Null when there is no image. */
  sk_sp<SkShader> shader() const override;

  bool animated() const override { return m_source.animated(); }
  /** Where the sampled pixels already stand, when the source painted
   *  them on a device. A REGION is not applied to it: what the device
   *  holds is the whole image, and a renderer binding it cuts the region
   *  itself. */
  DeviceImage deviceImage() const { return m_source.deviceImage(); }
  bool operator==(const Texture& other) const;

 protected:
  bool equals(const Leaf& other) const override {
    return *this == static_cast<const Texture&>(other);
  }

 private:
  TextureSource m_source;
  SkTileMode m_tileX = SkTileMode::kClamp;
  SkTileMode m_tileY = SkTileMode::kClamp;
  SkMatrix m_uv = SkMatrix::I();
  std::optional<SkIRect> m_region;
  SkFilterMode m_filter = SkFilterMode::kLinear;
  // The region cut from the source image it was cut from, so a texture
  // sampled every frame does not copy its pixels every frame. Derived
  // state: not part of equality.
  mutable sk_sp<SkImage> m_cutFrom;
  mutable sk_sp<SkImage> m_cut;
};

}  // namespace sigil::material
