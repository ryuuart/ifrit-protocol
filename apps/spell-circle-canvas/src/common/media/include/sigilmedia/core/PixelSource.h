#pragma once

/** @file
 * @ingroup media-core
 * `PixelSource`: THE ONE SEAM PIXELS CROSS into another library. A
 * decoded image, an animation, a video on the engine's clock, a picture
 * in hand, a producer that bakes once, frames another application
 * publishes, a rendered scene — each is a source, and a material's
 * texture, a leaf and a pen take any of them.
 */

#include <glm/vec2.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "sigilmedia/advanced/Source.h"
#include "sigilmedia/core/Frame.h"
#include "sigilmedia/core/Picture.h"

namespace sigil::media {

/** A PICTURE BAKED ON FIRST USE by a producer and kept: what
 *  `PixelSource::produce` holds. The key IS the identity — two producers
 *  under one key are the same picture — so the key names the picture and
 *  every parameter that shaped it. */
class Produced {
 public:
  Produced(std::string key, std::function<Picture()> producer);

  /** The key the picture was produced under. */
  const std::string& key() const { return m_key; }
  /** The picture, produced on the first ask and kept. */
  Frame frameAt(std::chrono::duration<double> time) const;
  bool isRunning() const { return false; }
  bool operator==(const Produced& other) const { return m_key == other.m_key; }

 private:
  struct State;
  std::string m_key;
  std::shared_ptr<State> m_state;
};

/**
 * WHERE PIXELS COME FROM, whatever they come from: a value holding one
 * source with its type erased, comparable across the erasure — two
 * sources are equal when they are the same kind of source and that kind
 * says they are equal. Empty until given a source.
 *
 * `frameAt()` answers the frame standing at a time on the caller's clock:
 * a document places it by its `Timing`, a still answers itself at any
 * time, a live source answers what has most recently arrived. A frame
 * that stands on a device carries no image until `deviceImage()` binds
 * it.
 */
class PixelSource {
 public:
  /** No source: every frame is empty. */
  PixelSource() = default;

  /** A picture in hand, the same at every time. Equal when it is the same
   *  picture object. */
  PixelSource(Picture picture);  // NOLINT(google-explicit-constructor)

  /** A renderer's own image handle, once the header that adapts it is
   *  included: the same as the picture it wraps. */
  template <class Native>
    requires AdaptsToPicture<Native>
  PixelSource(Native native)  // NOLINT(google-explicit-constructor)
      : PixelSource(Picture(std::move(native))) {}

  /** A document — an `Image`, a `Video` — read under @p timing. Equal when
   *  it is the same document under the same timing. */
  template <TimedDocument Document>
  PixelSource(std::shared_ptr<const Document> document,  // NOLINT
              Timing timing = {})
      : m_impl(std::make_shared<Model<Timed<Document>>>(
            Timed<Document>{std::move(document), timing})) {}

  /** The same, from a document held mutable. */
  template <TimedDocument Document>
  PixelSource(std::shared_ptr<Document> document,  // NOLINT
              Timing timing = {})
      : PixelSource(std::shared_ptr<const Document>(std::move(document)),
                    timing) {}

  /** ANY OTHER SOURCE: a type that answers `frameAt()` and `isRunning()`
   *  and compares by value — a scene rendered to a texture, frames
   *  another application publishes. */
  template <PixelSourceType Source>
  PixelSource(Source source)  // NOLINT(google-explicit-constructor)
      : m_impl(std::make_shared<Model<Source>>(std::move(source))) {}

  /** A picture baked by @p producer on first use and kept, identified by
   *  @p key. */
  static PixelSource produce(std::string key,
                             std::function<Picture()> producer);

  /** Whether a source is held. */
  explicit operator bool() const { return m_impl != nullptr; }

  /** The frame standing at @p time on the caller's clock; empty with no
   *  source. */
  Frame frameAt(std::chrono::duration<double> time) const {
    return m_impl ? m_impl->frameAt(time) : Frame{};
  }
  /** How many frames have stood: bumps when a new one does. Zero for a
   *  source that does not count them. */
  uint64_t revision() const { return m_impl ? m_impl->revision() : 0; }
  /** Whether the frame can change from one time to the next. */
  bool isRunning() const { return m_impl && m_impl->isRunning(); }
  /** The frame size in pixels; empty with no source. */
  glm::ivec2 size() const { return m_impl ? m_impl->size() : glm::ivec2{0, 0}; }

  /** The held source when it is an @p Source, else null — how a caller
   *  asks a `Produced` source for its key. */
  template <class Source>
  const Source* as() const;

  bool operator==(const PixelSource& other) const {
    if (!m_impl || !other.m_impl) return m_impl == other.m_impl;
    return m_impl->equals(*other.m_impl);
  }

 private:
  /** A document and the timing it is read under. */
  template <class Document>
  struct Timed {
    std::shared_ptr<const Document> document;
    Timing timing;
    Frame frameAt(std::chrono::duration<double> time) const {
      return document ? document->frameAt(time, timing) : Frame{};
    }
    bool isRunning() const { return document && document->isRunning(); }
    glm::ivec2 size() const {
      return document ? glm::ivec2(document->size()) : glm::ivec2{0, 0};
    }
    bool operator==(const Timed& other) const {
      return document == other.document && timing == other.timing;
    }
  };
  /** A picture in hand. */
  struct Still {
    Picture picture;
    Frame frameAt(std::chrono::duration<double>) const {
      Frame frame;
      frame.image = picture;
      return frame;
    }
    bool isRunning() const { return false; }
    glm::ivec2 size() const { return picture.size(); }
    bool operator==(const Still& other) const {
      return picture == other.picture;
    }
  };

  struct Concept {
    virtual ~Concept() = default;
    virtual Frame frameAt(std::chrono::duration<double> time) const = 0;
    virtual uint64_t revision() const = 0;
    virtual bool isRunning() const = 0;
    virtual glm::ivec2 size() const = 0;
    virtual bool equals(const Concept& other) const = 0;
  };
  template <class Source>
  struct Model final : Concept {
    explicit Model(Source source) : value(std::move(source)) {}
    Frame frameAt(std::chrono::duration<double> time) const override {
      return value.frameAt(time);
    }
    uint64_t revision() const override {
      if constexpr (RevisedPixelSource<Source>) return value.revision();
      return 0;
    }
    bool isRunning() const override { return value.isRunning(); }
    glm::ivec2 size() const override {
      if constexpr (SizedPixelSource<Source>) {
        return value.size();
      } else {
        const Frame frame = value.frameAt(std::chrono::duration<double>{});
        if (frame.image) return frame.image.size();
        return {frame.device.width, frame.device.height};
      }
    }
    bool equals(const Concept& other) const override {
      const auto* same = dynamic_cast<const Model<Source>*>(&other);
      return same && value == same->value;
    }
    Source value;
  };

  std::shared_ptr<const Concept> m_impl;
};

template <class Source>
const Source* PixelSource::as() const {
  const auto* model = dynamic_cast<const Model<Source>*>(m_impl.get());
  return model ? &model->value : nullptr;
}

}  // namespace sigil::media
