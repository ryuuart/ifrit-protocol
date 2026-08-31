/** @file
 * The kit boundary's NEGATIVE CONTROL. Built on purpose, expected to FAIL.
 *
 * The kit may only see the public headers of the features beneath it. This
 * TU reaches for the retained side's own header the way a kit source would
 * if the boundary were not structural; because SigilWorldKit's include path
 * is include/ alone and SceneImpl.h lives in scene/, it cannot be found.
 *
 * The `world_kit_boundary_probe` test builds this target and requires
 * "'SceneImpl.h' file not found" in the output.
 */

#include "SceneImpl.h"

int main() { return (int)sizeof(sigil::world::Scene::Impl); }
