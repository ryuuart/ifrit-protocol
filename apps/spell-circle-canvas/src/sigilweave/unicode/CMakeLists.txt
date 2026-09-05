# The Unicode leaf: transcoding, case mapping, script itemization, bidi and
# segmentation as plain values over UTF-16 text. No Skia and no other weave
# header, so it compiles in isolation and any consumer that needs text
# analysis without fonts can take it alone. It links ICU, which answers
# every question here but one: the tag a shaper is told a run's script in
# is HarfBuzz's translation of an ICU script code, so the leaf also links
# HarfBuzz and its ICU bridge and the features above it never open an ICU
# header to shape.

# The bridge ships as an archive of its own and the package config exports
# targets for the main library and the subsetter only, so it is named
# directly. Debug and release archives are separate builds; each config
# links its own.
find_library(SIGIL_HARFBUZZ_ICU_RELEASE NAMES harfbuzz-icu
  PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
  NO_DEFAULT_PATH REQUIRED)
find_library(SIGIL_HARFBUZZ_ICU_DEBUG NAMES harfbuzz-icu
  PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib"
  NO_DEFAULT_PATH)
if(SIGIL_HARFBUZZ_ICU_DEBUG)
  set(SIGIL_HARFBUZZ_ICU
      "$<IF:$<CONFIG:Debug>,${SIGIL_HARFBUZZ_ICU_DEBUG},${SIGIL_HARFBUZZ_ICU_RELEASE}>")
else()
  set(SIGIL_HARFBUZZ_ICU "${SIGIL_HARFBUZZ_ICU_RELEASE}")
endif()

find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
sigil_library(SigilWeaveUnicode
  SOURCES Unicode.cpp
  HEADERS Unicode.h
  PRIVATE ICU::i18n ICU::uc ICU::data ${SIGIL_HARFBUZZ_ICU}
          harfbuzz::harfbuzz)

# The leaf shapes nothing, so its test links neither fonts nor a font port
# and asks nothing of the machine it runs on.
sigil_test(weave_test
  SOURCES test/UnicodeTest.cpp
  LIBRARIES SigilWeaveUnicode)

sigil_bench(weave_bench
  SOURCES bench/UnicodeBench.cpp
  LIBRARIES SigilWeaveUnicode)
