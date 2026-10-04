# A native sketch consumes SDK usage requirements and resolves their
# implementations from the host. A separate build-tree package supplies
# the same compile surface when the monorepo targets are unavailable.
function(sigil_sketch_plugin target)
  cmake_parse_arguments(PARSE_ARGV 1 PLUGIN "" "" "SOURCES;LIBRARIES")
  if(PLUGIN_UNPARSED_ARGUMENTS OR NOT PLUGIN_SOURCES)
    message(FATAL_ERROR "sigil_sketch_plugin takes a target, SOURCES and optional LIBRARIES")
  endif()
  if(WIN32)
    message(FATAL_ERROR "Native sketch plugins require the dlopen host")
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(identity "${CMAKE_CURRENT_BINARY_DIR}/${target}_sigil_build.cpp")
  set(plugin_header "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${target}_sigil_metadata.h")
  file(GENERATE OUTPUT "${identity}" CONTENT
    "#include <${target}_sigil_metadata.h>\nnamespace { constexpr char metadata[] = \"SIGIL_SKETCH_METADATA_BEGIN\\n\" SIGIL_SKETCH_PLUGIN_METADATA \"\\0SIGIL_SKETCH_METADATA_END\"; }\nextern \"C\" __attribute__((visibility(\"default\"))) const char* sigilSketchBuild() noexcept { return metadata + sizeof(\"SIGIL_SKETCH_METADATA_BEGIN\\n\") - 1; }\n")
  add_library(${target} MODULE ${PLUGIN_SOURCES} "${identity}")
  target_compile_features(${target} PRIVATE cxx_std_20)
  set_target_properties(${target} PROPERTIES
    PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}" CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

  set(validation)
  set(in_tree FALSE)
  if(TARGET SigilSketch)
    get_target_property(sdk_directory SigilSketch SIGIL_SKETCH_SDK_DIRECTORY)
    if(sdk_directory)
      set(in_tree TRUE)
    endif()
  endif()
  list(PREPEND PLUGIN_LIBRARIES SigilSketch)
  set(origins)
  foreach(library IN LISTS PLUGIN_LIBRARIES)
    if(TARGET "${library}")
      get_target_property(alias "${library}" ALIASED_TARGET)
      if(alias)
        set(library "${alias}")
      endif()
    endif()
    list(APPEND origins "${library}")
  endforeach()
  set(PLUGIN_LIBRARIES ${origins})
  list(REMOVE_DUPLICATES PLUGIN_LIBRARIES)
  if(in_tree)
    set_property(GLOBAL APPEND PROPERTY SIGIL_SKETCH_PLUGIN_ORIGINS ${PLUGIN_LIBRARIES})
  else()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL SigilSketchSDK_COMPILER_ID OR
       NOT CMAKE_CXX_COMPILER_VERSION STREQUAL SigilSketchSDK_COMPILER_VERSION OR
       NOT CMAKE_SYSTEM_PROCESSOR STREQUAL SigilSketchSDK_SYSTEM_PROCESSOR)
      message(FATAL_ERROR "The plugin compiler, version and target architecture must match this Sigil build")
    endif()
    file(SHA256 "${CMAKE_CXX_COMPILER}" compiler_digest)
    if(NOT compiler_digest STREQUAL SigilSketchSDK_COMPILER_DIGEST)
      message(FATAL_ERROR "The plugin compiler binary must match this Sigil build")
    endif()
    if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL SigilSketchSDK_CONFIGURATION)
      message(FATAL_ERROR "CMAKE_BUILD_TYPE must match this SigilSketchSDK configuration")
    endif()
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
      OUTPUT_VARIABLE compiler_target OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE target_result)
    if(NOT target_result EQUAL 0 OR
       NOT compiler_target STREQUAL SigilSketchSDK_COMPILER_TARGET)
      message(FATAL_ERROR "The plugin compiler target must match this Sigil build")
    endif()
    if(APPLE)
      set(architecture "${SigilSketchSDK_SYSTEM_PROCESSOR}")
      if(SigilSketchSDK_ARCHITECTURE)
        set(architecture "${SigilSketchSDK_ARCHITECTURE}")
        set_target_properties(${target} PROPERTIES OSX_ARCHITECTURES "${architecture}")
      endif()
      if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL architecture)
        message(FATAL_ERROR "CMAKE_OSX_ARCHITECTURES must match this Sigil build")
      endif()
    endif()
    set(sdk_directory "${SigilSketchSDK_DIRECTORY}")
    set(validation --validate "${sdk_directory}/inputs.sha256"
      --origin-inputs "${sdk_directory}/origin-inputs.json" --libraries ${PLUGIN_LIBRARIES}
      --configuration "$<CONFIG>"
      --expected-config "${SigilSketchSDK_CONFIGURATION}")
    target_compile_options(${target} PRIVATE "@${SigilSketchSDK_FLAGS_FILE}")
    set_property(SOURCE ${PLUGIN_SOURCES} "${identity}" APPEND PROPERTY
      OBJECT_DEPENDS "${SigilSketchSDK_FLAGS_FILE};${sdk_directory}/inputs.sha256")
  endif()
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
  foreach(library IN LISTS PLUGIN_LIBRARIES)
    if(NOT TARGET ${library})
      message(FATAL_ERROR "The plugin's originating library target '${library}' does not exist")
    endif()
    target_link_libraries(${target} PRIVATE "$<COMPILE_ONLY:${library}>")
  endforeach()
  if(APPLE)
    target_link_options(${target} PRIVATE -undefined dynamic_lookup)
  endif()

  set(sdk_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SketchSDK.py")
  add_custom_command(OUTPUT "${plugin_header}"
    COMMAND ${Python3_EXECUTABLE} "${sdk_script}"
      --plugin-header "${plugin_header}" --boundaries "${sdk_directory}/boundaries.json"
      --libraries ${PLUGIN_LIBRARIES}
    DEPENDS "${sdk_directory}/boundaries.json" "${sdk_script}"
    VERBATIM)
  target_sources(${target} PRIVATE "${plugin_header}")
  if(in_tree)
    # The target graph regenerates identity when its inputs change.
    add_dependencies(${target} SigilSketchSDK)
  else()
    # An external package has no rule to rebuild its framework inputs.
    # Check them even when the module itself is already up to date.
    add_custom_target(${target}_sdk_inputs
      COMMAND ${Python3_EXECUTABLE} "${sdk_script}" ${validation}
      VERBATIM)
    add_dependencies(${target} ${target}_sdk_inputs)
  endif()
  # An unchanged module still restores a deleted sidecar. Read the identity
  # compiled into its bytes, rather than assigning the current header's ID.
  add_custom_target(${target}_sidecar
    COMMAND ${Python3_EXECUTABLE} "${sdk_script}"
      --ensure-sidecar "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)
  add_dependencies(${target} ${target}_sidecar)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} "${sdk_script}" ${validation}
      --stamp "$<TARGET_FILE:${target}>"
    VERBATIM)
endfunction()
