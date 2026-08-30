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

#include <any>
#include <concepts>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <type_traits>
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

/** A VALUE that answers an interface.
 *
 *  `Ops` is an abstract class — the operations a caller calls through. A
 *  model is any class deriving from it; constructing an Erased from one
 *  copies the model into shared immutable state, so carrying the value
 *  costs a pointer and a copy is a refcount bump.
 *
 *  Copies of ONE value are equal; two separately-constructed values are
 *  equal when they hold the same model type and that type's `==` says
 *  so; a model with no `==` is the escape hatch and compares equal to
 *  nothing but its own copies. An empty Erased holds no operations and
 *  answers false to `bool`. */
template <typename Ops>
class Erased {
 public:
  Erased() = default;

  /** A comparable model: its type and its value take part in equality. */
  template <typename M>
    requires(std::derived_from<std::remove_cvref_t<M>, Ops> &&
             std::equality_comparable<std::remove_cvref_t<M>> &&
             !std::same_as<std::remove_cvref_t<M>, Erased>)
  Erased(M model) {  // NOLINT: implicit by design (a seam value IS the model)
    using Model = std::remove_cvref_t<M>;
    State state;
    state.held = model;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const Model&>(a) == std::any_cast<const Model&>(b);
    };
    state.ops = std::make_shared<const Model>(std::move(model));
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: a model with no `==`. Identity only. */
  template <typename M>
    requires(std::derived_from<std::remove_cvref_t<M>, Ops> &&
             !std::equality_comparable<std::remove_cvref_t<M>> &&
             !std::same_as<std::remove_cvref_t<M>, Erased>)
  explicit Erased(M model) {
    using Model = std::remove_cvref_t<M>;
    State state;
    state.ops = std::make_shared<const Model>(std::move(model));
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->ops; }
  const Ops* operator->() const {
    return m_state ? m_state->ops.get() : nullptr;
  }
  const Ops& operator*() const { return *m_state->ops; }
  /** The operations, or null when empty. */
  const Ops* get() const { return m_state ? m_state->ops.get() : nullptr; }
  /** Does this value take part in structural equality? (False for the
   *  escape hatch and for an empty value.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state is equal; comparable models of one type compare their
   *  values; anything else is conservative. */
  bool operator==(const Erased& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
    std::shared_ptr<const Ops> ops;
  };
  std::shared_ptr<const State> m_state;
};

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
class Runtime : public Erased<Executor> {
 public:
  using Erased<Executor>::Erased;
  Runtime() = default;
  Runtime(Erased<Executor> erased)  // NOLINT: a Runtime IS its erased value
      : Erased<Executor>(std::move(erased)) {}

  /** The built-in executor: everything on the CPU, emitting onto the
   *  canvas. Every call returns the same value, so two default styles
   *  compare equal. */
  static Runtime cpu();
};

}  // namespace render

}  // namespace sigil::geometry::mesh
