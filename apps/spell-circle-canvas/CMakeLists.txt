# The floor is set by the policies this build depends on being NEW: link
# de-duplication by linker capability, and a GENERATED source property
# visible in every directory.
cmake_minimum_required(VERSION 3.31)

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(QT_QML_GENERATE_QMLLS_INI ON) # the QMLLS language server's config

# Off by default and turned on per target by sigil_qt_target(), which
# says why. qt_standard_project_setup() leaves an answer already given.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)

# Every directory searches for what it links, so one package is found
# several times. Promoting the imported targets makes the second search
# see what the first defined, instead of naming the same archive twice.
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Sigil) # the calls every library under src/ is built from
include("${CMAKE_CURRENT_LIST_DIR}/docs/Docs.cmake") # `--target docs`

enable_testing()

# One build command produces the set scripts/bench_ledger.py runs.
# Benchmarks are not tests: none is registered with ctest.
add_custom_target(benches)

# Opt-in network fetch of open-licensed demo assets; nothing here runs
# during a normal build, and configuring never touches the network.
set(SIGIL_ASSET_DIR "${CMAKE_BINARY_DIR}/assets" CACHE PATH
    "Where fetch_assets writes downloaded demo assets")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(fetch_assets
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/fetch_assets.py
          --out ${SIGIL_ASSET_DIR}
  COMMENT "Fetching demo assets into ${SIGIL_ASSET_DIR}"
  VERBATIM)

add_subdirectory(src)

# Every library root and every test binary has registered by now: the
# first lets each Doxyfile name all the others, the second lets a
# binary's ctest entries be written over its whole set of cases.
sigil_finalize_docs()
sigil_finalize_tests()

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
