# SpellCircleMac — the native macOS SwiftUI companion app. A separate
# executable from the Qt `SpellCircle` receiver: same UDP wire protocol,
# same shared C++ core (SpellCircleScene decode/resolve/draw +
# SpellCircleSkia Metal/Graphite plumbing), same Syphon server name — but
# AppKit/SwiftUI chrome with Liquid Glass panels instead of Qt Quick. The
# Qt app remains the cross-platform build.
#
# Layering:
#   bridge/  ObjC++ static lib (SpellCircleMacBridge): SCKEngine (UDP +
#            decode + Graphite render + Syphon) and SCKCanvasView
#            (CAMetalLayer + pan/zoom). Pure-ObjC headers exported to Swift
#            via module.modulemap.
#   app/     SwiftUI executable importing that Clang module.
if(NOT SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  message(STATUS "SpellCircleMac skipped: needs SPELLCIRCLE_ENABLE_SKIA_CANVAS")
  return()
endif()

include(CheckLanguage)
check_language(Swift)
if(NOT CMAKE_Swift_COMPILER)
  message(STATUS "SpellCircleMac skipped: no Swift compiler found")
  return()
endif()
enable_language(Swift)

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

if(NOT SYPHON_FRAMEWORK)
  message(STATUS "SpellCircleMac skipped: Syphon.framework not found")
  return()
endif()

# SHARED, deliberately: the Swift link runs through swift-driver, which
# rejects the raw `-framework X` strings vcpkg's skia config carries in its
# link interface. Absorbing the whole C++ world (scene core, Skia, TextFlow,
# ICU, HarfBuzz, Syphon) into one dylib keeps those flags on the clang++
# side; the Swift executable links only this library.
add_library(SpellCircleMacBridge SHARED
  bridge/SCKEngine.mm bridge/SCKEngine.h
  bridge/SCKCanvasView.mm bridge/SCKCanvasView.h
)

# ARC for the ObjC++ engine/view (the shared Skia .mm TUs stay non-ARC and
# manage CoreFoundation refs explicitly — see SkiaGraphiteContextMetal.mm).
target_compile_options(SpellCircleMacBridge PRIVATE -fobjc-arc)

# No Qt anywhere in this target; keep the global AUTOMOC out of it.
set_target_properties(SpellCircleMacBridge PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

target_include_directories(SpellCircleMacBridge PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/bridge
)

target_link_libraries(SpellCircleMacBridge
  PRIVATE
    ${FOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    SpellCircleScene
    SpellCircleSkia
    SpellCircleNet
    ${SYPHON_FRAMEWORK}
)

add_executable(SpellCircleMac MACOSX_BUNDLE
  app/SpellCircleMacApp.swift
  app/Models/AppearancePreference.swift
  app/Models/Comparable+Clamped.swift
  app/Models/EngineModel.swift
  app/Models/FeedEntry.swift
  app/Models/FontStyleOption.swift
  app/Views/ActivityPanel.swift
  app/Views/AppSettingsView.swift
  app/Views/CanvasHostView.swift
  app/Views/ContentView.swift
  app/Views/FeedTableView.swift
  app/Views/FontComboBoxes.swift
  app/Views/GlassToolbarStrip.swift
  app/Views/SettingRows.swift
  app/Views/SettingsPane.swift
)

set_target_properties(SpellCircleMac PROPERTIES
    # qt_standard_project_setup() enables AUTOMOC globally; a moc object
    # injected into a Swift target's source list breaks swift-driver.
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.SpellCircleMac"
    MACOSX_BUNDLE_BUNDLE_NAME "SpellCircle"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
)

target_compile_options(SpellCircleMac PRIVATE
  # @main app entry point in a multi-file module.
  "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
)

# swiftc discovers bridge/module.modulemap through this import path, making
# `import SpellCircleMacBridge` resolve to the ObjC headers.
target_include_directories(SpellCircleMac PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bridge
)

target_link_libraries(SpellCircleMac PRIVATE SpellCircleMacBridge)
