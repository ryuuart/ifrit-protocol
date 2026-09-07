#pragma once

/** @file
 * A FRAME: the scene to draw, the passes that draw it, and the
 * readbacks the caller wants — three declared things and nothing else.
 * A frame states no order: the order is derived from what its passes
 * declared they read and write.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Runtime.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::world {

/** A RESOURCE THE CALLER WANTS BACK, and what to do with it.
 *
 *  The callback runs THE FRAME AFTER the one that produced the content
 *  — which is the only honest answer where reading a device's memory
 *  costs a wait, and is therefore what the CPU executor promises too, so
 *  a caller cannot write code that only works without a device. */
class Readback {
 public:
  /** What a readback hands over. Exactly one of `image` and `points` is
   *  set, depending on what wrote the resource. */
  struct Result {
    std::string resource;
    sk_sp<SkImage> image;
    const geometry::mesh::Cloud* points = nullptr;
    /** The frame the content was produced in. */
    uint64_t frame = 0;
  };

  Readback() = default;
  explicit Readback(std::string name) : m_name(std::move(name)) {}

  /** What to do with the resource when it comes back. */
  Readback& then(std::function<void(const Result&)> callback) {
    m_callback = std::move(callback);
    return *this;
  }

  [[nodiscard]] const std::string& name() const { return m_name; }
  [[nodiscard]] const std::function<void(const Result&)>& callback() const {
    return m_callback;
  }

 private:
  std::string m_name;
  std::function<void(const Result&)> m_callback;
};

/** The resource @p name, handed back the frame after it was written. */
Readback readback(std::string name);

/** ONE FRAME, DECLARED.
 *
 *  A Frame is built fresh every frame and thrown away, the way an
 *  Element is: it holds no surfaces, no order and no device. A frame
 *  with no passes IS its scene — the bodies are drawn straight from the
 *  viewpoint the tree declared — so an author reaches for passes only
 *  when there is something to say about how the picture is made. */
class Frame {
 public:
  Frame() = default;
  Frame(Element scene)  // NOLINT: a frame with no passes IS its scene
      : m_scene(std::move(scene)) {}

  /** The tree to describe. */
  Frame& scene(Element root);
  /** The size the frame's targets are made at. A frame declaring passes
   *  needs one. */
  Frame& extent(SkISize size);
  /** The viewpoint, for a tree that declares none of its own. */
  Frame& camera(geometry::mesh::camera::Camera c);
  /** Adds a pass. The order passes are added in is not the order they
   *  run in — that is derived from what each one reads and writes. */
  Frame& pass(Pass p);
  /** Asks for a resource back once the frame has made it. */
  Frame& readback(Readback r);
  /** The executor the passes run on. */
  Frame& runtime(Runtime r);
  /** The resource the finished picture is in. Unset means the last
   *  image any pass wrote. */
  Frame& present(std::string name);

  [[nodiscard]] const Element& scene() const { return m_scene; }
  [[nodiscard]] SkISize extent() const { return m_extent; }
  [[nodiscard]] const geometry::mesh::camera::Camera& camera() const {
    return m_camera;
  }
  [[nodiscard]] std::span<const Pass> passes() const { return m_passes; }
  [[nodiscard]] std::span<const Readback> readbacks() const {
    return m_readbacks;
  }
  [[nodiscard]] const Runtime& runtime() const { return m_runtime; }
  [[nodiscard]] const std::string& present() const { return m_present; }

 private:
  Element m_scene;
  SkISize m_extent{0, 0};
  geometry::mesh::camera::Camera m_camera;
  std::vector<Pass> m_passes;
  std::vector<Readback> m_readbacks;
  Runtime m_runtime = Runtime::cpu();
  std::string m_present;
};

}  // namespace sigil::world
