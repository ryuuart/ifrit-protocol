#include "SceneRegistry.h"

#include <algorithm>

namespace gallery {

namespace {

std::vector<SceneDescriptor> &mutableRegistry() {
  // Function-local static: safe to touch from every registrar regardless of
  // TU initialization order.
  static std::vector<SceneDescriptor> registry;
  return registry;
}

} // namespace

namespace detail {

SceneRegistrar::SceneRegistrar(SceneDescriptor descriptor) {
  mutableRegistry().push_back(std::move(descriptor));
}

} // namespace detail

const std::vector<SceneDescriptor> &sceneRegistry() {
  static const bool sorted = [] {
    std::stable_sort(mutableRegistry().begin(), mutableRegistry().end(),
                     [](const SceneDescriptor &left,
                        const SceneDescriptor &right) {
                       if (left.displayOrder != right.displayOrder)
                         return left.displayOrder < right.displayOrder;
                       return left.name < right.name;
                     });
    return true;
  }();
  static_cast<void>(sorted);
  return mutableRegistry();
}

} // namespace gallery
