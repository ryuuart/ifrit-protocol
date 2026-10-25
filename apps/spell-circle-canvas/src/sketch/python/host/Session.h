#pragma once

/** @file
 * What the host lends a Python body while its session is open, and the
 * checked handles the bindings hand out over it.
 *
 * The host rebuilds `Scene2DContext` every frame, so nothing here holds
 * one: the state keeps the pointers out of the newest one and every
 * handle reads them back through a weak reference to the state. A
 * context an author stashed therefore answers with an error rather than
 * reading a frame that has gone.
 *
 * The state and the handles are declared here because the session that
 * opens them and the surface a sketch is handed are written in
 * different files.
 */

#include <pybind11/pybind11.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/io/Hub.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sigil::sketch::python::detail {

/** EVERYTHING ONE OPEN SESSION LENDS PYTHON, and what it holds on
 *  Python's behalf: the callables a description retained, the feeds a
 *  declaration opened and the connections opened over them. */
struct State {
  sigil::python::CallbackLifetime callbacks;
  compose::Composer* composer = nullptr;
  CanvasSpecification* specification = nullptr;
  motion::Engine* engine = nullptr;
  Assets* assets = nullptr;
  weave::FontContext* fonts = nullptr;
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;
  std::vector<std::shared_ptr<compose::SurfaceScene>>* pages = nullptr;
  /** The device runtime the session bakes scene3d frames on, null for the
   *  process's, and the trees it keeps painted by value: the host's, as
   *  they are for a C++ sketch in the same session. */
  const sigil::world::Runtime* bakeRuntime = nullptr;
  compose::Textures* textures = nullptr;
  std::unordered_map<io::Feed, std::shared_ptr<io::Feed>> feedLeases;
  /** What a declaration opened over a feed and Python only borrows, such
   *  as a connection: owned here so that closing the session ends it. */
  std::vector<std::shared_ptr<void>> held;
  std::thread::id thread;
  std::string key;
  bool deterministic = false;
  bool valid = false;
  bool failed = false;

  std::shared_ptr<io::Feed> retainFeed(io::Feed feed) {
    auto held = feedLeases.find(feed);
    if (held == feedLeases.end())
      held = feedLeases.emplace(feed, sigil::python::retainSessionFeed(feed))
                 .first;
    return held->second;
  }

  void close() {
    valid = false;
    callbacks.clear();
    held.clear();
    feedLeases.clear();
  }

  void update(Scene2DContext& ctx) {
    composer = &ctx.composer;
    engine = &ctx.engine;
    assets = &ctx.assets;
    fonts = ctx.fonts;
    scenes = ctx.scenes;
    pages = ctx.pages;
    bakeRuntime = ctx.bakeRuntime;
    textures = ctx.textures;
    specification = ctx.specification;
    thread = std::this_thread::get_id();
    key = ctx.key;
    deterministic = ctx.deterministic;
    valid = true;
  }

  Scene2DContext context() const {
    return {*composer, *engine,      *assets, specification->size,
            specification, fonts,    deterministic, scenes,
            key,       pages,        bakeRuntime, textures};
  }
};

/** Session-scoped access without retaining a stack-allocated context. */
class Context {
 public:
  explicit Context(const std::shared_ptr<State>& state) : m_state(state) {}
  std::shared_ptr<State> state() const {
    const auto state = m_state.lock();
    if (!state || !state->valid)
      throw std::runtime_error(
          "This sketch context belongs to a closed session");
    if (state->failed)
      throw std::runtime_error(
          "This sketch session failed; save the sketch to reload it");
    if (state->thread != std::this_thread::get_id())
      throw std::runtime_error(
          "Sketch context operations run on the sketch thread");
    return state;
  }

 private:
  std::weak_ptr<State> m_state;
};

class AssetsView : public Context {
 public:
  using Context::Context;
};

/** WHAT A SCENE3D BODY LENDS PYTHON WHILE ITS SETUP RUNS: the context the
 *  session hands once, at setup. The body clears it when setup returns,
 *  because a scene3d sketch's every frame is a function of the scene time
 *  and nothing it asks of the context after that could reach a frame. */
struct Scene3DLoan {
  Scene3DContext* context = nullptr;
  std::thread::id thread;
};

/** A scene3d context as Python holds it: answers while its setup runs and
 *  refuses afterwards, naming why. */
class Scene3DContextView {
 public:
  explicit Scene3DContextView(const std::shared_ptr<Scene3DLoan>& loan)
      : m_loan(loan) {}
  Scene3DContext& context() const {
    const auto loan = m_loan.lock();
    if (!loan || !loan->context)
      throw std::runtime_error(
          "A scene3d sketch's context is handed at setup; ask for its pages, "
          "screens and camera there");
    if (loan->thread != std::this_thread::get_id())
      throw std::runtime_error(
          "Sketch context operations run on the sketch thread");
    return *loan->context;
  }

 private:
  std::weak_ptr<Scene3DLoan> m_loan;
};

}  // namespace sigil::sketch::python::detail
