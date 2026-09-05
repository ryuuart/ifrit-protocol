# The light feature: emitters as plain comparable values — a sun, a
# point light, a spot, their falloffs and the per-frame budget. No
# device, no registry, no renderer: glm and nothing else, so a consumer
# that only needs to say where the lights are links this alone.
find_package(glm CONFIG REQUIRED)
sigil_library(SigilWorldLight
  SOURCES Light.cpp
  HEADERS Light.h
  PUBLIC glm::glm)

# Links only the library it exercises, so a link edge that pulled the
# device in would show up as a build failure here.
sigil_test(world_light_test
  SOURCES test/LightTest.cpp
  LIBRARIES SigilWorldLight)

sigil_bench(world_light_bench
  SOURCES bench/LightBench.cpp
  LIBRARIES SigilWorldLight)
