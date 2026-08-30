#include "SceneRegistry.h"

#include <algorithm>

namespace gallery {

namespace {

// Head of the registrar list. A plain pointer with constant initialisation
// is ready before any registrar's constructor runs, whatever the TU order.
const detail::SceneRegistrar* registrarHead = nullptr;

}  // namespace

namespace detail {

SceneRegistrar::SceneRegistrar(Factory factory) noexcept
    : factory(factory), next(registrarHead) {
  registrarHead = this;
}

}  // namespace detail

const std::vector<SceneDescriptor>& sceneRegistry() {
  static const std::vector<SceneDescriptor> registry = [] {
    std::vector<SceneDescriptor> scenes;
    for (const detail::SceneRegistrar* registrar = registrarHead; registrar;
         registrar = registrar->next)
      scenes.push_back(registrar->factory());
    // The list is newest-first; registration order is the tie-break.
    std::reverse(scenes.begin(), scenes.end());
    std::stable_sort(
        scenes.begin(), scenes.end(),
        [](const SceneDescriptor& left, const SceneDescriptor& right) {
          if (left.displayOrder != right.displayOrder)
            return left.displayOrder < right.displayOrder;
          return left.name < right.name;
        });
    return scenes;
  }();
  return registry;
}

}  // namespace gallery
