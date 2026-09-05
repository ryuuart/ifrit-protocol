# The light feature: emitters as plain comparable values — a sun, a
# point light, a spot, their falloffs and the per-frame budget. No
# device, no registry, no renderer: glm and nothing else, so a consumer
# that only needs to say where the lights are links this alone.
find_package(glm CONFIG REQUIRED)
sigil_library(SigilWorldLight
  SOURCES Light.cpp
  HEADERS Light.h
  PUBLIC glm::glm)

# The emitter values are asserted in world_element_test, beside the
# description that carries them: the description links this feature, so a
# binary over the emitters alone would link a subset of nothing — every
# target that reaches Light reaches Element too.
sigil_bench(world_light_bench
  SOURCES bench/LightBench.cpp
  LIBRARIES SigilWorldLight)
