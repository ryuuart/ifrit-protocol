#include "GalleryScenes.h"

#include "scenes/SceneFactories.h"

namespace gallery {

std::vector<std::unique_ptr<Scene>> makeScenes() {
  std::vector<std::unique_ptr<Scene>> scenes;
  scenes.push_back(makeExclusionsScene());
  scenes.push_back(makeKnuthPlassScene());
  scenes.push_back(makeLoopScene());
  scenes.push_back(makeRainScene());
  scenes.push_back(makeRippleScene());
  scenes.push_back(makeVerticalScene());
  scenes.push_back(makeHyperScriptsScene());
  scenes.push_back(makeEffectsScene());
  scenes.push_back(makeEffectsStressScene());
  scenes.push_back(makeLoudShadersScene());
  scenes.push_back(makeMarkersScene());
  scenes.push_back(makeSlotsScene());
  scenes.push_back(makeOverflowScene());
  return scenes;
}

} // namespace gallery
