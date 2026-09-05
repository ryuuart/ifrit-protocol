# Every package MORE THAN ONE library needs, found once. A package a
# single library needs is found in that library, beside the target that
# links it — that is still once, and it is where a reader of the target
# is looking. What is here is what would otherwise be searched for in
# two directories at two spellings and drift apart.
#
# Imported targets are promoted to global scope by the top level, so a
# leaf that links what is found here needs no find_package() of its own.

# workaround: Qt ships a FindFFmpeg module that collides with the ffmpeg
# port's FindFFMPEG on a case-insensitive filesystem. The port's module —
# which is what resolves the release and debug archives as a pair — is
# reached by asking for the package before Qt appends its own module
# directory to the search path.
find_package(FFMPEG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts QuickDialogs2 CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(yoga CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)

# Wanted by more than one library apiece: the maths types, the reflected
# tuple, the JSON reader, the device engine and the Vulkan handles.
find_package(glm CONFIG REQUIRED)
find_package(boost_pfr CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

# The administrative steps around the build — extracting the sketch
# compile flags, fetching assets, generating the API documentation — are
# Python scripts under scripts/, and the build calls them rather than
# re-implementing text and file work in CMake.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# The macOS frameworks, found once for every target that names one.
if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
endif()

# Syphon publishes a canvas as a GPU texture other applications attach to.
# Absent, the targets that publish one skip themselves.
find_package(unofficial-syphon CONFIG)
