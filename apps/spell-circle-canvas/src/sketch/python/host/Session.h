#pragma once

/** @file
 * What the host lends a Python body while its session is open, and the
 * checked handles the bindings hand out over it.
 *
 * The host rebuilds `SketchContext` every frame, so nothing here holds
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
#include <sigilsketch/canvas/Sketch.h>

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
 *  declaration opened, and the outputs a ticker call was handed. */
struct State {
  sigil::python::CallbackLifetime callbacks;
  compose::Composer* composer = nullptr;
  CanvasSpecification* specification = nullptr;
  motion::Ticker* ticker = nullptr;
  Assets* assets = nullptr;
  weave::FontContext* fonts = nullptr;
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;
  std::vector<std::shared_ptr<const void>> tickerOwners;
  std::unordered_map<io::Feed*, std::shared_ptr<void>> feedLeases;
  std::thread::id thread;
  std::string key;
  bool deterministic = false;
  bool valid = false;
  bool failed = false;

  void retainFeed(std::shared_ptr<io::Feed> feed) {
    auto* identity = feed.get();
    if (!feedLeases.contains(identity))
      feedLeases.emplace(identity,
                         sigil::python::retainSessionFeed(std::move(feed)));
  }

  void close() {
    valid = false;
    callbacks.clear();
    feedLeases.clear();
  }

  void update(SketchContext& ctx) {
    composer = &ctx.composer;
    ticker = &ctx.ticker;
    assets = &ctx.assets;
    fonts = ctx.fonts;
    scenes = ctx.scenes;
    specification = ctx.specification;
    thread = std::this_thread::get_id();
    key = ctx.key;
    deterministic = ctx.deterministic;
    valid = true;
  }

  SketchContext context() const {
    return {*composer,           *ticker,       *assets,
            specification->size, specification, fonts,
            deterministic,       scenes,        key};
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

}  // namespace sigil::sketch::python::detail
