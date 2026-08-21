cmake_minimum_required(VERSION 3.16)

# Our third-party interfaces overlap heavily: skia, freetype (via harfbuzz),
# OpenImageIO and OpenColorIO each flatten libpng/libz/libexpat/Imath into
# their own link interface, so every executable named several archives twice
# and ld64 answered with `ignoring duplicate libraries` on each. Under
# cmake_minimum_required(3.16) both de-duplication policies default to OLD,
# which is what kept the repeats. ld64 records symbols as it scans and
# re-visits earlier archives, so one occurrence is enough.
#
# These must be set BEFORE project(): CMP0156's NEW behavior is carried by
# CMAKE_<LANG>_LINK_LIBRARIES_PROCESSING, which the platform modules
# initialize while enabling the language.
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW) # de-duplicate by linker capability
endif()
if(POLICY CMP0179)
  cmake_policy(SET CMP0179 NEW) # ...keeping the first occurrence, everywhere
endif()

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

set(QT_QML_GENERATE_QMLLS_INI ON) # QMLLS Language Server config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Redirects final executables (.exe, ELF binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Redirects dynamically linked libraries (.so, .dll)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects static libraries / archive files (.a, .lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects QT modules
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Compiles the Skia Graphite CanvasSceneBackend (Metal on Apple platforms,
# Vulkan elsewhere) and its generic GPU interop plumbing under
# src/common/skia. At runtime the renderer uses it when a compatible
# Graphite context can be created; without it the canvas renders no scene
# content. Turning this off removes the Graphite implementation and
# leaves the same renderer-facing factory backed by its always-null stub.
option(SPELLCIRCLE_ENABLE_SKIA_CANVAS "Build the Skia Graphite (Metal/Vulkan) offscreen-canvas backend" ON)

# Builds SigilScry (src/common/scry): HTML/CSS layout rendered by the
# Ultralight SDK onto SkImage frames for the scene canvases. Auto-disables
# with a warning when the SDK isn't installed (see cmake/FindUltralight.cmake).
option(SPELLCIRCLE_ENABLE_ULTRALIGHT "Build the SigilScry Ultralight HTML-layout backend" ON)

# Builds SigilSubstance (src/common/substance): Adobe Substance 3D
# archives (.sbsar) rendered to texture sets through the Substance
# Engine, for SigilWorld materials. The SDK is a licensed download and is
# never vendored; scripts/setup.py finds an installed copy and writes
# SUBSTANCE_SDK_DIR into CMakeUserPresets.json. Auto-disables with a
# warning when the SDK is not there.
option(SPELLCIRCLE_ENABLE_SUBSTANCE "Build the SigilSubstance .sbsar material renderer" ON)
# Builds SigilUsd (src/common/usd): the world's data written to and read
# from USD (OpenUSD core from vcpkg — tbb and zlib only). Auto-disables
# with a warning when the pxr package is not found.
option(SPELLCIRCLE_ENABLE_USD "Build the SigilUsd writer/reader over OpenUSD" ON)
set(SUBSTANCE_SDK_DIR "" CACHE PATH "Adobe Substance 3D SDK directory (holds substance-config.cmake)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
# Skia is needed unconditionally now: the SigilWeave text-layout library
# (src/sigilweave) builds on it even when the Skia scene backend is disabled.
find_package(unofficial-skia CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(yoga CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)

if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
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

# Instruments this project's objects for LLVM source-based coverage
# (llvm-profdata / llvm-cov). Meant for a dedicated build tree — configure a
# separate binary dir with this ON (scripts/coverage.py does exactly that)
# rather than toggling it inside a primary tree, because flipping the flag
# recompiles every object file. vcpkg dependencies arrive as prebuilt
# archives and are never instrumented; reports therefore cover this
# repository's sources, which is the point.
option(SPELLCIRCLE_COVERAGE "Instrument C/C++/ObjC++ targets for llvm-cov source-based coverage" OFF)
if(SPELLCIRCLE_COVERAGE)
  # The instrumentation flags are Clang's; this gate covers AppleClang and
  # clang-cl too (both carry Clang in their compiler id, and clang-cl
  # accepts these flags even with its MSVC-style frontend).
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
      "SPELLCIRCLE_COVERAGE requires a Clang toolchain: "
      "-fprofile-instr-generate/-fcoverage-mapping are Clang's source-based "
      "coverage and ${CMAKE_CXX_COMPILER_ID} does not implement them. Under "
      "MSVC cl.exe the alternative is OpenCppCoverage — PDB-based binary "
      "instrumentation of an ordinary build, no recompile, a standalone "
      "tool rather than a library — not integrated here because the "
      "Windows port is not yet buildable.")
  endif()
  # Compile flags are gated on language because SpellCircleMac's Swift
  # sources go through swiftc, which rejects these clang flags. The link
  # flag is gated the same way via LINK_LANGUAGE: a C++/ObjC++ link runs
  # through the clang++ driver, which embeds the profile runtime, while the
  # Swift executable's swift-driver link must not see the flag. The Swift
  # app still links the instrumented SpellCircleMacBridge dylib, which
  # carries its own runtime reference.
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fprofile-instr-generate;-fcoverage-mapping>")
  add_link_options("$<$<LINK_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fprofile-instr-generate>")
endif()

# Builds this project's objects under Clang's runtime sanitizers. The value
# is a CMake list of -fsanitize= names — "address;undefined" for the
# ASan+UBSan lane, "thread" for TSan. Meant for a dedicated build tree —
# configure a separate binary dir with this set (scripts/sanitize.py does
# exactly that) rather than toggling it inside a primary tree, because
# changing the value recompiles every object file, and a sanitized binary
# is not a binary anyone ships. vcpkg dependencies arrive as prebuilt,
# uninstrumented archives; ASan's interceptors still see every allocation
# and libc call those archives make, so bugs in this repository's use of
# them are caught — but checks that need both sides instrumented (container
# overflow) misfire across the boundary, which is why sanitize.py disables
# that one check at runtime.
set(SPELLCIRCLE_SANITIZE "" CACHE STRING
  "Sanitizers to compile and link with (e.g. address;undefined or thread); empty builds none")
if(SPELLCIRCLE_SANITIZE)
  # The flag spellings and runtime libraries are Clang's; this gate covers
  # AppleClang and clang-cl too (both carry Clang in their compiler id).
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
      "SPELLCIRCLE_SANITIZE requires a Clang toolchain: the -fsanitize= "
      "runtimes configured here ship with Clang and "
      "${CMAKE_CXX_COMPILER_ID} does not provide them.")
  endif()
  if("thread" IN_LIST SPELLCIRCLE_SANITIZE AND "address" IN_LIST SPELLCIRCLE_SANITIZE)
    message(FATAL_ERROR
      "SPELLCIRCLE_SANITIZE cannot combine thread with address: each "
      "runtime interposes the allocator and shadows the address space its "
      "own way, and a process can only host one. Build separate trees.")
  endif()
  list(JOIN SPELLCIRCLE_SANITIZE "," spellcircle_sanitize_names)
  set(spellcircle_sanitize_compile_flags
    "-fsanitize=${spellcircle_sanitize_names}"
    # Sanitizer reports unwind through frame pointers; without them the
    # stacks in a report degrade to unusable fragments.
    "-fno-omit-frame-pointer")
  if("undefined" IN_LIST SPELLCIRCLE_SANITIZE)
    # UBSan's default is print-and-continue, which lets a finding scroll
    # past inside a passing test. Non-recoverable checks abort instead, so
    # a UBSan finding fails the test that triggered it.
    list(APPEND spellcircle_sanitize_compile_flags
      "-fno-sanitize-recover=undefined")
  endif()
  if("address" IN_LIST SPELLCIRCLE_SANITIZE)
    # Abseil's swisstable headers turn on iterator-generation bookkeeping
    # in any translation unit compiled under ASan, which changes the
    # table layout and makes generation pointers load-bearing. The
    # function that supplies the empty-table generation pointer lives in
    # the prebuilt (unsanitized) vcpkg archive, where that bookkeeping
    # compiled out — it returns null, and the first find() on an absl map
    # dereferences it. NDEBUG_SANITIZER is Abseil's own switch for
    # keeping generations off under sanitizers; defining it makes our
    # sanitized objects agree with the archive about the layout. The cost
    # is Abseil's iterator-invalidation checking, which cannot work
    # against a prebuilt archive anyway.
    add_compile_definitions(NDEBUG_SANITIZER)
  endif()
  # Compile flags are gated on language because SpellCircleMac's Swift
  # sources go through swiftc, which rejects these clang flags. The link
  # flag is gated the same way via LINK_LANGUAGE: a C++/ObjC++ link runs
  # through the clang++ driver, which links the sanitizer runtime dylib,
  # while the Swift executable's swift-driver link must not see the flag.
  # The Swift app still links the sanitized SpellCircleMacBridge dylib,
  # which carries its own runtime reference.
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:${spellcircle_sanitize_compile_flags}>")
  add_link_options("$<$<LINK_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fsanitize=${spellcircle_sanitize_names}>")
endif()

# Opt-in network fetch of open-licensed demo assets into the build tree
# (`--target fetch_assets`); nothing here runs during a normal build.
include(FetchAssets)

enable_testing()

# What the policies above cannot reach: vcpkg's exported configs disagree
# about how to name the same archive. freetype pulls libpng and zlib through
# the PNG::PNG / ZLIB::ZLIB imported targets, while unofficial-skia flattens
# its whole closure into raw find_library() paths inside one
# $<LINK_ONLY:$<$<CONFIG:DEBUG>:...>> blob. CMake de-duplicates targets
# against targets and paths against paths, never a target against a path, so
# libpng/libz/libexpat/libjpeg still reach ld64 twice. The second copy is
# ignored by the linker, which is correct but noisy — this silences the note
# without pretending the link line is minimal.
if(APPLE AND NOT CMAKE_VERSION VERSION_LESS 3.18) # CheckLinkerFlag
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-Wl,-no_warn_duplicate_libraries"
    HAVE_NO_WARN_DUPLICATE_LIBRARIES) # ld64 from Xcode 15 on
  if(HAVE_NO_WARN_DUPLICATE_LIBRARIES)
    add_link_options("LINKER:-no_warn_duplicate_libraries")
  endif()
endif()

# MacOS Frameworks
if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
  find_library(SYPHON_FRAMEWORK Syphon)
endif()

qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(src)

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
