#pragma once

/** @file
 * @ingroup world-frame
 * A FRAME: the scene to draw, what surrounds it, the camera it is seen
 * through, the passes that draw it, and the readbacks the caller wants.
 * A frame states no order: the order is derived from what its passes
 * declared they read and write.
 */

#include <sigilcore/callable/Callable.h>
#include <sigilmedia/core/Picture.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/advanced/Runtime.h>
#include <sigilworld/frame/Environment.h>
#include <sigilworld/frame/Pass.h>

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::world {

/** A RESOURCE THE CALLER WANTS BACK, and what to do with it.
 *
 *  The callback runs THE FRAME AFTER the one that produced the content,
 *  because reading a device's memory costs a wait and a frame does not
 *  stop to pay it. */
class Readback {
 public:
  /** What a readback hands over. Exactly one of `image` and `points` is
   *  set, depending on what wrote the resource. */
  struct Result {
    std::string resource;
    media::Picture image;
    const geometry::mesh::Cloud* points = nullptr;
    /** The frame the content was produced in. */
    uint64_t frame = 0;
  };

  Readback() = default;
  /** A readback of the resource called @p name, with nothing to do
   *  with it yet. */
  explicit Readback(std::string name) : m_name(std::move(name)) {}

  /** What to do with the resource when it comes back. The result is
   *  offered and the callback names it or not — a readback asked for only
   *  to know the frame reached the device takes `[] {…}`. */
  Readback& then(core::Callable<void(const Result&)> callback) {
    m_callback = std::move(callback);
    return *this;
  }

  /** The resource this readback asks for. */
  [[nodiscard]] const std::string& name() const { return m_name; }
  /** What was set to run on the result; empty when nothing was. */
  [[nodiscard]] const core::Callable<void(const Result&)>& callback() const {
    return m_callback;
  }

 private:
  std::string m_name;
  core::Callable<void(const Result&)> m_callback;
};

/** The resource @p name, handed back the frame after it was written. */
Readback readback(std::string name);

/** ONE FRAME, DECLARED: built fresh every frame and thrown away, the way
 *  a node is, holding no surfaces, no order and no device. A frame with
 *  no passes IS its scene: it is reconciled and extracted — its bodies,
 *  its lights and its camera — and forms no picture, because a picture
 *  is what a pass writes. */
class Frame {
 public:
  Frame() = default;
  /** A frame that is nothing but @p scene — any node, a group of the
   *  set most often. */
  Frame(Node scene)  // NOLINT: a frame with no passes IS its scene
      : m_scene(std::move(scene)) {}

  /** @name The scene
   *  What is drawn, what surrounds it, and what it is seen through.
   *  @{ */
  /** The tree to describe. */
  Frame& scene(Node root);
  /** THE ENVIRONMENT the set stands in: @p map, every lit body sampling
   *  it for what reaches it from all around, read under @p options. A
   *  frame holds one. */
  Frame& environment(EnvironmentMap map, EnvironmentOptions options = {});
  /** WHICH CAMERA the frame looks through, by its key. A tree holding
   *  one camera is looked through it without naming it; a tree holding
   *  several must name one, and naming a key no camera answers to is an
   *  error that says so. */
  Frame& camera(std::string key);
  /** Adds a pass. The order passes are added in is not the order they
   *  run in — that is derived from what each one reads and writes. */
  Frame& pass(Pass p);
  /** Asks for a resource back once the frame has made it. */
  Frame& readback(Readback r);
  /** @} */

  /** @name What a host states
   *  Where the picture lands and what draws it — the host's to say, not
   *  the set's.
   *  @{ */
  /** The size the frame's targets are made at. A frame declaring passes
   *  needs one. */
  Frame& extent(glm::ivec2 size);
  /** WHERE THE TARGETS STAND IN A LARGER PICTURE: they are the part of a
   *  picture `offset.whole` pixels across whose top-left corner is at
   *  `offset.origin`, so the extent is the part's size and every
   *  projection is the whole picture's, carried off-centre onto it.
   *  Unset, the targets are the picture. */
  Frame& viewOffset(ViewOffset offset);
  /** The lens a tree holding no camera is seen through. */
  Frame& defaultCamera(geometry::mesh::camera::Camera lens);
  /** A LENS THAT WINS over the chosen camera's: the camera the tree
   *  chose still says how the picture is read — its exposure and tone
   *  curve — and this says where it is taken from. What a host that has
   *  taken hold of the viewpoint states. */
  Frame& cameraOverride(geometry::mesh::camera::Camera lens);
  /** The executor the passes run on. A frame that declares passes and
   *  names none runs none of them, and the scene says so. */
  Frame& runtime(Runtime r);
  /** The resource the finished picture is in. Unset means the last
   *  image any pass wrote. */
  Frame& present(std::string name);
  /** @} */

  /** The tree this frame describes. */
  [[nodiscard]] const Node& scene() const { return m_scene; }
  /** The panorama the set stands in; invalid when the frame states none. */
  [[nodiscard]] const EnvironmentMap& environmentMap() const {
    return m_environmentMap;
  }
  /** The environment's dials. */
  [[nodiscard]] const EnvironmentOptions& environmentOptions() const {
    return m_environmentOptions;
  }
  /** The key of the camera the frame names; empty when it names none. */
  [[nodiscard]] const std::string& camera() const { return m_cameraKey; }
  /** The size the targets are made at; zero when none was declared. */
  [[nodiscard]] glm::ivec2 extent() const { return m_extent; }
  /** Where the targets stand in the picture the camera frames. */
  [[nodiscard]] const ViewOffset& viewOffset() const { return m_viewOffset; }
  /** The lens used where the tree holds no camera. */
  [[nodiscard]] const geometry::mesh::camera::Camera& defaultCamera() const {
    return m_defaultCamera;
  }
  /** The lens that wins over the chosen camera's, where one was stated. */
  [[nodiscard]] const std::optional<geometry::mesh::camera::Camera>&
  cameraOverride() const {
    return m_cameraOverride;
  }
  /** The passes as they were added, which is not the order they run in. */
  [[nodiscard]] std::span<const Pass> passes() const { return m_passes; }
  /** The readbacks asked for, in the order they were asked. */
  [[nodiscard]] std::span<const Readback> readbacks() const {
    return m_readbacks;
  }
  /** The executor the passes run on. */
  [[nodiscard]] const Runtime& runtime() const { return m_runtime; }
  /** The resource the finished picture is in; empty means the last
   *  image any pass wrote. */
  [[nodiscard]] const std::string& present() const { return m_present; }

 private:
  Node m_scene;
  EnvironmentMap m_environmentMap;
  EnvironmentOptions m_environmentOptions;
  std::string m_cameraKey;
  glm::ivec2 m_extent{0, 0};
  ViewOffset m_viewOffset;
  geometry::mesh::camera::Camera m_defaultCamera;
  std::optional<geometry::mesh::camera::Camera> m_cameraOverride;
  std::vector<Pass> m_passes;
  std::vector<Readback> m_readbacks;
  Runtime m_runtime;
  std::string m_present;
};

}  // namespace sigil::world
