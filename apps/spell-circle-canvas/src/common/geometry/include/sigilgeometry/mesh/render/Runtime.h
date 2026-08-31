#pragma once

/** @file
 * The seam a mesh draw executes through, as a VALUE. `Runtime` holds an
 * `Executor` — the steps a draw is made of — so the same drawMesh()
 * call, the same MeshStyle and the same geometry run on whichever
 * executor the caller carries. One executor ships here, the CPU one;
 * a feature that owns a GPU device supplies its own as a value, which
 * keeps every dependency pointing down and duplicates no file.
 *
 * What an executor replaces, and what it must reproduce: the mesh draw
 * transforms each vertex by model, view and projection, shades it under
 * MeshStyle (lit, device-space normals, or uv), rejects and sorts
 * triangles back to front, and emits them onto the canvas; the panel
 * draw concats the full perspective transform and runs the caller's 2D
 * drawing in panel-local coordinates. An executor is free to do all of
 * that differently, and is required to land within the tolerance its
 * agreement test states of what the CPU executor renders.
 */

#include <sigilcore/comparable/Erased.h>

#include <functional>
#include <glm/glm.hpp>
#include <utility>

class SkCanvas;
struct SkSize;

namespace sigil::geometry::mesh {

struct Mesh;
namespace camera {
struct Camera;
}

namespace render {

struct MeshStyle;

/** The two draws a runtime performs. An implementation owns whatever
 *  device it needs; the arguments carry no device, so a description
 *  built for one runtime draws on any of them. */
class Executor {
 public:
  virtual ~Executor() = default;

  /** Transform, shade, sort and emit @p mesh under @p style. */
  virtual void drawMesh(SkCanvas& canvas, const Mesh& mesh,
                        const glm::mat4& model, const camera::Camera& camera,
                        SkSize viewport, const MeshStyle& style) const = 0;

  /** Put @p draw's 2D content on the plane @p model describes, in
   *  panel-local coordinates. */
  virtual void drawPanel(SkCanvas& canvas, const glm::mat4& model,
                         const camera::Camera& camera, SkSize viewport,
                         const std::function<void(SkCanvas&)>& draw) const = 0;
};

/** The executor a draw runs on, carried as a comparable value. */
class Runtime : public core::Erased<Executor> {
 public:
  using core::Erased<Executor>::Erased;
  Runtime() = default;
  Runtime(core::Erased<Executor> erased)  // NOLINT: a Runtime IS its value
      : core::Erased<Executor>(std::move(erased)) {}

  /** The built-in executor: everything on the CPU, emitting onto the
   *  canvas. Every call returns the same value, so two default styles
   *  compare equal. */
  static Runtime cpu();
};

}  // namespace render

}  // namespace sigil::geometry::mesh
