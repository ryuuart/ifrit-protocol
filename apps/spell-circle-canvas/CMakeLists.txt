# The floor is set by the policies this build depends on being NEW: link
# de-duplication by linker capability and its first-occurrence rule, and
# a GENERATED source property visible in every directory — which is what
# lets the wire schema's generated header be compiled in directories
# other than the one that generates it.
cmake_minimum_required(VERSION 3.31)

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

set(QT_QML_GENERATE_QMLLS_INI ON) # QMLLS Language Server config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A package found in two directories otherwise defines its imported
# targets twice — once per directory scope — and the same archive reaches
# ld64 under both names, since CMake collapses two link items only when
# they are one item. Promoting them makes the second find_package() see
# the targets the first defined.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# cmake/ holds the modules the whole tree is built from — the package
# prefixes and linker flags, the dependencies, the optional SDKs, the
# library helper, the instrumentation switch, the Qt target rule, the
# header self-test. A module that belongs to ONE library lives in that
# library's own cmake/ directory and is included by its path, so moving
# the library moves its build rules with it.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# workaround: vcpkg configs name some dependencies as imported targets and
# others as archive paths, so one archive reaches ld64 under both names.
if(APPLE)
  add_link_options("LINKER:-no_warn_duplicate_libraries")
endif()

# Opt-in network fetch of open-licensed demo assets into the build tree;
# nothing here runs during a normal build, and configuring the project
# never touches the network. The manifest and the downloader are
# scripts/fetch_assets.py, which needs no build tree of its own — this
# target is the same fetch reached through the build.
set(SIGIL_ASSET_DIR "${CMAKE_BINARY_DIR}/assets" CACHE PATH
    "Where fetch_assets writes downloaded demo assets")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(fetch_assets
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/fetch_assets.py
          --out ${SIGIL_ASSET_DIR}
  COMMENT "Fetching demo assets into ${SIGIL_ASSET_DIR}"
  VERBATIM)

# Per-library Doxygen sites (`--target docs`); nothing here runs during a
# normal build, and the target is simply absent when Doxygen is not
# installed. Each library root registers itself; sigil_finalize_docs()
# after add_subdirectory(src) writes the rules.
include("${CMAKE_CURRENT_LIST_DIR}/docs/Docs.cmake")

# sigil_library_root(), sigil_library(), sigil_test() and sigil_bench():
# the calls every library under src/ is built from.
include(Sigil)

enable_testing()

# Qt's source scanning is decided here rather than left to
# qt_standard_project_setup(), which turns moc on for every target in the
# tree unless the answer is already set. Almost nothing here is a Qt
# target, and moc costs a scan of every source plus an autogen build step
# on each target it is enabled on, found Q_OBJECT or not. So: off by
# default, on per target through sigil_qt_target(). Everything else the
# call arranges — the Qt policy level, GNUInstallDirs, IDE folders, the
# generated-source groups — is still wanted.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
include(QtTarget)

# Every benchmark binary hangs off this target, so one build command
# produces the set scripts/bench_ledger.py runs. Benchmarks are not
# tests: none is registered with ctest.
add_custom_target(benches)

add_subdirectory(src)

# Every library root has registered by now, which is what lets each
# library's Doxyfile name all the others in TAGFILES.
sigil_finalize_docs()

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
