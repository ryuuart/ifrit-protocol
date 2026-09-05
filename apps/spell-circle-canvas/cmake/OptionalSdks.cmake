# The SDKs this build uses when they are installed and does without when
# they are not: what each one is for, the switch that turns it off, and
# the search that turns itself off with a warning. Each of these is a
# licensed or large download that is never vendored; scripts/setup.py
# finds an installed copy and writes what it found into
# CMakeUserPresets.json.

# Builds SigilScry (src/common/scry): HTML/CSS layout rendered by the
# Ultralight SDK onto SkImage frames for the scene canvases.
option(SPELLCIRCLE_ENABLE_ULTRALIGHT "Build the SigilScry Ultralight HTML-layout backend" ON)

# Builds SigilSubstance (src/common/substance): Adobe Substance 3D
# archives (.sbsar) rendered to texture sets through the Substance
# Engine, for SigilWorld materials.
option(SPELLCIRCLE_ENABLE_SUBSTANCE "Build the SigilSubstance .sbsar material renderer" ON)
set(SUBSTANCE_SDK_DIR "" CACHE PATH "Adobe Substance 3D SDK directory (holds substance-config.cmake)")

# Builds SigilUsd (src/common/usd): the world's data written to and read
# from USD (OpenUSD core from vcpkg — tbb and zlib only).
option(SPELLCIRCLE_ENABLE_USD "Build the SigilUsd writer/reader over OpenUSD" ON)

if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  # SigilScry carries the find module for the SDK it is the only consumer of.
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../src/common/scry/cmake")
  find_package(Ultralight)
  if(NOT Ultralight_FOUND)
    message(WARNING "Ultralight SDK not found; disabling the SigilScry backend")
    set(SPELLCIRCLE_ENABLE_ULTRALIGHT OFF)
  endif()
endif()

if(SPELLCIRCLE_ENABLE_USD)
  find_package(pxr CONFIG QUIET)
  if(NOT pxr_FOUND)
    message(WARNING "OpenUSD (pxr) not found; disabling SigilUsd")
    set(SPELLCIRCLE_ENABLE_USD OFF)
  endif()
endif()

if(SPELLCIRCLE_ENABLE_SUBSTANCE)
  if(SUBSTANCE_SDK_DIR AND EXISTS "${SUBSTANCE_SDK_DIR}/substance-config.cmake")
    # The CPU engine ("blend": results in system memory), matched to the
    # host: NEON on Apple Silicon, SSE2 on x86-64. Headless, no GPU
    # context to own, and its outputs land as bytes SigilWorld uploads
    # like any other image.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
      set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT neon_blend)
    else()
      set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT sse2_blend)
    endif()
    include("${SUBSTANCE_SDK_DIR}/substance-config.cmake")
    message(STATUS "Substance SDK: ${SUBSTANCE_SDK_DIR} (${SUBSTANCE_FRAMEWORK_ENGINE_VARIANT})")
  else()
    message(WARNING "Substance SDK not found (SUBSTANCE_SDK_DIR unset or invalid); disabling SigilSubstance")
    set(SPELLCIRCLE_ENABLE_SUBSTANCE OFF)
  endif()
endif()
