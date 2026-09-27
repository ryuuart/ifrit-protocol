# The light feature: Material's light placed in a set — its angles as a
# direction in the world, a sun, a point light, a spot, and their
# falloffs. No device, no registry, no renderer: Material's core value and
# glm, so a consumer that only needs to say where the lights are links
# this alone.
find_package(glm CONFIG REQUIRED)
sigil_library(SigilWorldLight
  SOURCES Light.cpp
  HEADERS Light.h
  PUBLIC SigilMaterialCore glm::glm)

# The emitter values are asserted in world_test, beside the
# description that carries them: the description links this feature, so a
# binary over the emitters alone would link a subset of nothing — every
# target that reaches Light reaches Element too.
sigil_bench(world_bench
  SOURCES bench/LightBench.cpp
  LIBRARIES SigilWorldLight)
