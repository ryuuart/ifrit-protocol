# The floor is set by the policies this build depends on being NEW: link
# de-duplication by linker capability, and a GENERATED source property
# visible in every directory.
cmake_minimum_required(VERSION 3.31)

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

option(SIGIL_BUILD_APPS "Build the Qt applications, Grimoire and its sketch catalogue among them" ON)
option(SIGIL_BUILD_PYTHON "Build Python bindings and the Python package" ON)
if(SIGIL_BUILD_APPS AND NOT SIGIL_BUILD_PYTHON)
  message(FATAL_ERROR "The bundled applications require Python bindings; set SIGIL_BUILD_APPS=OFF to build the native libraries alone")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(QT_QML_GENERATE_QMLLS_INI ON) # the QMLLS language server's config

# Only targets that declare Qt types enable moc through sigil_qt_target().
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)

# Feature directories share the imported targets their package searches find.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# workaround: vcpkg configs name some dependencies as imported targets and
# others as archive paths, so one archive reaches ld64 under both names.
if(APPLE)
  add_link_options("LINKER:-no_warn_duplicate_libraries")
endif()

# Keep multiply/add rounding independent of inlining by disabling contraction.
# Imported dependencies keep their own settings.
add_compile_options($<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-ffp-contract=off>)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Sigil) # the calls every library under src/ is built from

enable_testing()

# One build command produces the set the bench verb runs.
# Benchmarks are not tests: none is registered with ctest.
add_custom_target(benches)

# Opt-in network fetch of open-licensed demo assets; nothing here runs
# during a normal build, and configuring never touches the network.
set(SIGIL_ASSET_DIR "${CMAKE_BINARY_DIR}/assets" CACHE PATH
    "Where fetch_assets writes downloaded demo assets")
# workaround: vcpkg's Python wrapper selects its own development artifacts.
# Keep the headers and embedding library matched to the build interpreter.
if(SIGIL_BUILD_PYTHON)
  if(VCPKG_TARGET_TRIPLET AND COMMAND _find_package)
    _find_package(Python3 3.12 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
  else()
    find_package(Python3 3.12 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
  endif()
  set(PYBIND11_FINDPYTHON ON)
  find_package(pybind11 CONFIG REQUIRED)
else()
  find_package(Python3 3.12 COMPONENTS Interpreter REQUIRED)
endif()
add_test(NAME sigil_build_helpers
  COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test/SigilHelpersTest.py"
    --cmake "${CMAKE_COMMAND}" --compiler "${CMAKE_CXX_COMPILER}")
set_tests_properties(sigil_build_helpers PROPERTIES TIMEOUT 120
  LABELS integration)
# The build slot's cases run their jobs in a temporary state directory and
# never touch the shared build tree.
add_test(NAME sigil_build_slot
  COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/sketch-pass/test_buildslot.py")
set_tests_properties(sigil_build_slot PROPERTIES TIMEOUT 120)

add_custom_target(fetch_assets
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/sigil.py assets
          --out ${SIGIL_ASSET_DIR}
  COMMENT "Fetching demo assets into ${SIGIL_ASSET_DIR}"
  VERBATIM)

# The native libraries consume the package from its Python project. A source
# distribution supplies this path explicitly beside its bundled native sources.
get_filename_component(SIGIL_PYTHON_PACKAGE_DEFAULT
  "${CMAKE_CURRENT_SOURCE_DIR}/../python/sigil" ABSOLUTE)
set(SIGIL_PYTHON_PACKAGE_DIR "${SIGIL_PYTHON_PACKAGE_DEFAULT}" CACHE PATH
  "Python package project containing sigil, its typing pass, and packaging rules")

add_subdirectory(src)

# Grimoire, the stock host built on SigilSketch with this repository's
# sketches, is an application of its own beside this tree and consumes
# the framework through its public targets alone. Day to day it builds
# here, in this tree's own binary directory; a project that is not the
# top level leaves it out, because then it is Grimoire that brought this
# tree in.
get_filename_component(SIGIL_GRIMOIRE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../grimoire" ABSOLUTE)
if(SIGIL_BUILD_APPS AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory("${SIGIL_GRIMOIRE_DIR}" "${CMAKE_BINARY_DIR}/grimoire")
endif()

if(SIGIL_BUILD_PYTHON)
  add_subdirectory("${SIGIL_PYTHON_PACKAGE_DIR}" "python-package")
endif()

# Finalization needs every library, test contribution and host target,
# so the top level makes it; under another project that project does.
if(PROJECT_IS_TOP_LEVEL)
  sigil_finalize_docs()
  sigil_finalize_tests()
endif()

# Regenerate the protocol client, package modules and parity ledger in
# dependency order. The ledger requires bindings for every licensed SDK.
if(SIGIL_BUILD_PYTHON AND SIGIL_TYPING_PYTHON)
  add_custom_target(generate
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/src/common/protocol/definition/generate.py"
            --description "${SIGIL_PROTOCOL_DESCRIPTION}"
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${SIGIL_PYTHON_ROOT}"
            "${SIGIL_TYPING_PYTHON}" "${SIGIL_PYTHON_PACKAGE_DIR}/typing/generate.py"
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${SIGIL_PYTHON_ROOT}"
            "${SIGIL_TYPING_PYTHON}" "${SIGIL_PYTHON_PACKAGE_DIR}/typing/parity.py"
            --sketches "${SIGIL_GRIMOIRE_DIR}/sketches"
    COMMENT "Regenerating the derived files the checkout carries"
    VERBATIM)
  add_dependencies(generate sigil_python SigilProtocolDefinition)
endif()
if(PROJECT_IS_TOP_LEVEL AND COMMAND sigil_finalize_sketch_link_surfaces)
  sigil_finalize_sketch_link_surfaces()
endif()

include(GNUInstallDirs)
if(TARGET Seer)
  install(TARGETS Seer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
