# The same registration code is a built-in module in Sketchbook and an
# extension in ordinary Python. Each process loads one native implementation.
pybind11_add_module(sigil_python NO_EXTRAS "${PROJECT_SOURCE_DIR}/src/sketch/python/Module.cpp")
target_link_libraries(sigil_python PRIVATE SigilPython SigilSketchPython)
set_target_properties(sigil_python PROPERTIES
  OUTPUT_NAME _sigil
  SIGIL_PYTHON_EXTENSION ON
  INSTALL_RPATH_USE_LINK_PATH ON
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/$<0:>")
if(APPLE)
  target_link_options(sigil_python PRIVATE "LINKER:-dead_strip_dylibs")
  # Wheel repair resolves dependencies after installation. CMake omits
  # in-tree link directories from the install rpath, so name vcpkg's path
  # explicitly; repair copies the libraries and removes this search path.
  if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set_property(TARGET sigil_python APPEND PROPERTY INSTALL_RPATH
      "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
  endif()
endif()
add_custom_target(sigil_python_package
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/sigil" "${CMAKE_BINARY_DIR}/python/sigil"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs/_sigil" "${CMAKE_BINARY_DIR}/python/_sigil")
add_dependencies(sigil_python sigil_python_package)

install(TARGETS sigil_python
  LIBRARY DESTINATION . COMPONENT SigilPython
  RUNTIME DESTINATION . COMPONENT SigilPython)
if(SKBUILD_STATE STREQUAL "editable")
  # The editable finder maps Python modules to their source files. Installed
  # copies take precedence over that mapping, so install only type metadata.
  install(DIRECTORY sigil DESTINATION . COMPONENT SigilPython
    FILES_MATCHING PATTERN "*.pyi" PATTERN "py.typed"
    PATTERN "__pycache__" EXCLUDE)
else()
  install(DIRECTORY sigil DESTINATION . COMPONENT SigilPython
    FILES_MATCHING PATTERN "*.py" PATTERN "*.pyi" PATTERN "py.typed"
    PATTERN "__pycache__" EXCLUDE)
endif()
install(DIRECTORY stubs/_sigil DESTINATION . COMPONENT SigilPython
  FILES_MATCHING PATTERN "*.pyi" PATTERN "py.typed")
file(GLOB SIGIL_PYTHON_EXAMPLES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/sketch/sketches/python_*.py")
install(FILES ${SIGIL_PYTHON_EXAMPLES}
  DESTINATION sigil/examples COMPONENT SigilPython)
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  file(GLOB SIGIL_PYTHON_DEPENDENCY_LICENSES
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright")
  foreach(license IN LISTS SIGIL_PYTHON_DEPENDENCY_LICENSES)
    get_filename_component(package "${license}" DIRECTORY)
    get_filename_component(package "${package}" NAME)
    install(FILES "${license}" DESTINATION "sigil/_licenses/${package}"
      COMPONENT SigilPython)
  endforeach()
endif()

add_test(NAME python_authoring
  COMMAND ${CMAKE_COMMAND} -E env
    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
    "SIGIL_TEST_SKETCHBOOK=$<TARGET_FILE:Sketchbook>"
    ${Python3_EXECUTABLE} -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/test" -p "test_*.py")
add_test(NAME python_stub_surface
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
    ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/typing/check_surface.py")

find_program(SIGIL_BASEDPYRIGHT basedpyright
  HINTS "${CMAKE_BINARY_DIR}/typing-tools/bin" "${CMAKE_BINARY_DIR}/typing-tools/Scripts"
        "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/bin" "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/Scripts")
# Stub generation imports the extension, so its interpreter must match the
# Python version the native module was built against.
set(SIGIL_TYPING_PYTHON "")
foreach(candidate IN ITEMS
    "${CMAKE_BINARY_DIR}/typing-tools/bin/python"
    "${CMAKE_BINARY_DIR}/typing-tools/Scripts/python.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/bin/python"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/Scripts/python.exe"
    "${Python3_EXECUTABLE}")
  if(EXISTS "${candidate}")
    execute_process(COMMAND "${candidate}" -c
      "import importlib.metadata, sys; assert sys.version_info[:2] == (${Python3_VERSION_MAJOR}, ${Python3_VERSION_MINOR}); assert importlib.metadata.version('pybind11-stubgen') == '2.5.5'; import pybind11_stubgen"
      RESULT_VARIABLE SIGIL_TYPING_PROBE OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
    if(SIGIL_TYPING_PROBE STREQUAL "0")
      set(SIGIL_TYPING_PYTHON "${candidate}")
      break()
    endif()
  endif()
endforeach()
if(SIGIL_BASEDPYRIGHT)
  add_test(NAME python_type_declarations
    COMMAND "${SIGIL_BASEDPYRIGHT}"
      -p "${CMAKE_CURRENT_SOURCE_DIR}/typing/pyrightconfig.json")
  add_test(NAME python_typing
    COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/test/typing/check.py"
      --python "${Python3_EXECUTABLE}" --checker "${SIGIL_BASEDPYRIGHT}"
      --extra-path "${CMAKE_BINARY_DIR}/python"
      --example "${PROJECT_SOURCE_DIR}/src/sketch/sketches/python_hello.py"
      --example "${PROJECT_SOURCE_DIR}/src/sketch/sketches/python_hello_compose.py"
      --example "${PROJECT_SOURCE_DIR}/src/sketch/sketches/python_compose_stamps.py"
      --example "${PROJECT_SOURCE_DIR}/src/sketch/sketches/python_live_signals.py"
      --example "${CMAKE_CURRENT_SOURCE_DIR}/sigil/examples/tools/send_live_signals.py")
endif()
if(SIGIL_TYPING_PYTHON)
  add_test(NAME python_stub_drift
    COMMAND ${CMAKE_COMMAND} -E env
      "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
      "${SIGIL_TYPING_PYTHON}"
      "${CMAKE_CURRENT_SOURCE_DIR}/typing/generate.py" --check)
endif()
