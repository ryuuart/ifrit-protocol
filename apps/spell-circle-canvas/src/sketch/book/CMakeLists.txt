# Sketchbook — the one live application, and the one headless renderer.
#
# The app lists every sketch, live-edits any of them, and draws a set
# sketch through an orbitable viewport. `--headless` is the renderer
# every plate-ledger tier drives.
qt_add_executable(Sketchbook
  main.cpp
)
sigil_qt_target(Sketchbook)

# A real .app: a bare Mach-O in bin/ gets no bundle identity, so macOS
# gives it the executable's name in the menu bar and treats activation as
# a plain process. `--headless` still works through the inner binary
# (bin/<config>/Sketchbook.app/Contents/MacOS).
set_target_properties(Sketchbook PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.Sketchbook"
  MACOSX_BUNDLE_BUNDLE_NAME "Sketchbook"
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
  MACOSX_BUNDLE TRUE
)

# A file kept in qml/ is imported by NAME, so it is aliased to the
# module root beside the generated qmldir — which is what lets one of
# these files reach the singleton below without importing its own
# module. Both properties are read when the module is declared, so
# they are set before it.
foreach(sketchbook_qml IN ITEMS
    Main TopBar SketchList SketchGallery PlateThumb Inspector StatusStrip
    Theme)
  set_source_files_properties(qml/${sketchbook_qml}.qml
    PROPERTIES QT_RESOURCE_ALIAS ${sketchbook_qml}.qml)
endforeach()
# The palette is one object the whole window paints from, so it is
# registered as a singleton rather than instantiated per panel.
set_source_files_properties(qml/Theme.qml
  PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Sketchbook
  URI Sigil.Sketchbook
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/TopBar.qml
    qml/SketchList.qml
    qml/SketchGallery.qml
    qml/PlateThumb.qml
    qml/Inspector.qml
    qml/StatusStrip.qml
    qml/Theme.qml
  SOURCES
    SketchbookView.cpp SketchbookView.h
    SketchCatalog.cpp SketchCatalog.h
)
# WHERE THE BROWSER LOOKS FOR THE STILLS IT SHOWS: the quick tier's
# baseline, which a plate sweep writes beside its manifest. A machine
# that has never run one simply has no thumbnails, and `--plates <dir>`
# names another directory.
target_compile_definitions(Sketchbook PRIVATE
  SIGILSKETCH_PLATES_DIR="${CMAKE_BINARY_DIR}/plate_baseline_quick_$<CONFIG>.plates")
target_include_directories(Sketchbook PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(Sketchbook PRIVATE
  SigilSketchLive SigilSketchPlate SigilSketches SigilMeasure SigilWeavePorts
  Qt6::Gui Qt6::GuiPrivate Qt6::Quick Qt6::QuickControls2)

# The device lane for set sketches. Without it the binary reports rather
# than falling back: a flag that asked for the device answers with the
# device or with nothing.
if(TARGET SigilWorldDiligent)
  target_link_libraries(Sketchbook PRIVATE SigilWorldDiligent)
else()
  target_compile_definitions(Sketchbook PRIVATE SIGILSKETCH_NO_DEVICE=1)
endif()

# Draw straight into QQuickRhiItem's texture through the shared Skia
# Graphite context when the active QRhi backend supports it. Other builds
# keep the explicit CPU-raster/upload fallback in SketchbookView.cpp.
if(TARGET SigilSkiaQt)
  target_link_libraries(Sketchbook PRIVATE SigilSkiaQt)
  target_compile_definitions(Sketchbook PRIVATE SIGILSKETCH_BOOK_GPU)
endif()

# ---------------------------------------------------------------------------
# THE HOT-RELOAD LINK SURFACE.
#
# A sketch dylib links with `-undefined dynamic_lookup` and resolves the
# framework's symbols out of this executable, so the host must EXPORT
# them — and it must contain them, which for a static archive means
# force-loading it whether or not the host's own translation units
# reference anything inside.
#
# The list below and SigilSketches' PUBLIC dependencies are one fact
# stated twice: the latter decides what a sketch may #include, this
# decides what it may LINK. When they disagree, every sketch still
# compiles and every compiled-in sketch still runs — and the hot-reloaded
# one fails at dlopen with a symbol not found in flat namespace. So
# anything added there belongs here too, as the ARCHIVES it resolves to:
# an interface target has no file to force-load, which is why the tiers
# are named archive by archive.
if(APPLE)
  target_link_options(Sketchbook PRIVATE "LINKER:-export_dynamic")
  foreach(sketch_dep IN ITEMS
      SigilSketchCore SigilSketchCanvas SigilSketchSet SigilSketchLive
      SigilComposeCore SigilComposeKit SigilComposeBrush
      SigilComposeTypography SigilComposeInstances SigilComposeKit
      SigilComposeTexture
      SigilImageAsset SigilImageDecode SigilLoaderHub
      SigilMotionClock SigilMotionBind
      SigilCoreCache SigilCoreReconcile
      SigilGeometryPath SigilGeometryPathBlend SigilGeometryMesh
      SigilGeometryMeshCamera SigilGeometryMeshRender SigilGeometryMeshCurve
      SigilGeometryMeshPop SigilGeometryMeshCodec
      SigilMaterialCore SigilMaterialTexture SigilMaterialColor
      SigilMaterialSdf SigilMaterialPattern SigilMaterialField
      SigilMaterialSkia SigilMaterialKit SigilMaterialOcio
      SigilWorldElement SigilWorldFrame SigilWorldGraph SigilWorldScene
      SigilWorldLight SigilWorldKit
      SigilWeaveUnicode SigilWeaveFonts SigilWeaveParagraph SigilWeaveLayout
      SigilWeaveDecoration SigilWeavePaint SigilWeaveChoreograph
      SigilWeaveQuery SigilWeaveCache SigilWeavePorts
      unofficial::skia::skia)
    target_link_options(Sketchbook PRIVATE
      "LINKER:-force_load,$<TARGET_FILE:${sketch_dep}>")
  endforeach()
  # …and the archives an optional SDK produced, on the machines where it
  # did. A sketch over one of those is only in the registry where its
  # target exists, so its symbols are only wanted here on the same
  # machines — and where it IS compiled in, a hot-reload of it needs them
  # exactly as much as any other sketch needs the list above.
  foreach(sketch_sdk_dep IN ITEMS
      SigilScryPlatform SigilScryGpu SigilScryEngine SigilSubstance)
    if(TARGET ${sketch_sdk_dep})
      target_link_options(Sketchbook PRIVATE
        "LINKER:-force_load,$<TARGET_FILE:${sketch_sdk_dep}>")
    endif()
  endforeach()
else()
  target_link_options(Sketchbook PRIVATE "LINKER:--export-dynamic")
endif()

# The web engine reads its ICU tables and certificate bundle from a
# folder beside the binary, so every executable that links it stages one.
if(TARGET SigilScry)
  ultralight_copy_resources(Sketchbook)
endif()

# The response file is a tracked build output (not POST_BUILD): deleting
# it or reconfiguring — the compilation database is rewritten at generate
# time — regenerates it, independent of whether the executable relinks.
add_custom_command(
  OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/scripts/extract_sketch_flags.py
    --compdb ${CMAKE_BINARY_DIR}/compile_commands.json
    --anchor ${SIGIL_SKETCH_DIR}/Anchor.cpp
    --config $<CONFIG>
    --out ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp
    # No --extra: the host force-loads Skia and re-exports it, so a
    # sketch resolves Skia through -undefined dynamic_lookup like the
    # rest of the SDK. Linking the archive into the dylib instead would
    # put two Skia images in one process, whose objects cannot be
    # exchanged.
  DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/scripts/extract_sketch_flags.py
  COMMENT "Extracting sketch compile flags from compile_commands.json"
  VERBATIM)
add_custom_target(sketch_flags
  DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp)
add_dependencies(Sketchbook sketch_flags)

# Paints one of every stock material, so a material that cannot compile
# or draw fails here rather than in someone's sketch.
add_test(NAME sketch_reload_materials
  COMMAND Sketchbook
    ${SIGIL_SKETCH_DIR}/stock_materials.cpp
    --frame ${CMAKE_BINARY_DIR}/sketch_reload_materials.png)

# THE LINK-SURFACE GUARD, and the reason it is a third test rather than a
# case in either of the two above: what it watches is not a compile and
# not a pixel, it is the force-load list.
#
# A missing archive there is invisible everywhere else. Every sketch
# still compiles (the include surface is a different list) and every
# compiled-in sketch still runs (a host that links the object library
# resolves nothing dynamically) — and the hot-reloaded one fails at
# dlopen. So this test must go through the DYNAMIC path, which `--frame`
# does: it compiles the file with the response file, dlopens the result,
# runs it, and exits nonzero when the load fails. shapeworks_lab is the
# widest surface of the sketches that name geometry and material symbols,
# so it is the one most likely to catch a partial regression too; neither
# hello nor stock_materials can, because neither names one.
add_test(NAME sketch_reload_surface
  COMMAND Sketchbook
    ${SIGIL_SKETCH_DIR}/shapeworks_lab.cpp
    --frame ${CMAKE_BINARY_DIR}/sketch_reload_surface.png)

# …and the same guard for the other runtime: a set sketch names world
# symbols no canvas sketch does, so a world archive missing from the list
# above shows up here and nowhere else.
add_test(NAME sketch_reload_surface_set
  COMMAND Sketchbook
    ${SIGIL_SKETCH_DIR}/first_light.cpp
    --frame ${CMAKE_BINARY_DIR}/sketch_reload_surface_set.png)

# …and once more for each optional SDK, on the machines that have one.
# These name symbols nothing else in the registry names, so an SDK
# archive missing from the force-load list shows up here alone. A machine
# whose SDK is installed but whose runtime data is not still passes: the
# sketch draws what it is missing rather than failing to load.
if(TARGET SigilComposeWeb)
  add_test(NAME sketch_reload_surface_web
    COMMAND Sketchbook
      ${SIGIL_SKETCH_DIR}/web_panel.cpp
      --frame ${CMAKE_BINARY_DIR}/sketch_reload_surface_web.png)
endif()
if(TARGET SigilSubstance)
  add_test(NAME sketch_reload_surface_substance
    COMMAND Sketchbook
      ${SIGIL_SKETCH_DIR}/substance_swatches.cpp
      --frame ${CMAKE_BINARY_DIR}/sketch_reload_surface_substance.png)
endif()

# The OTHER registration form. A sketch filed under a name of its own
# takes SIGIL_SKETCH_AS, whose dylib expansion differs from
# SIGIL_SKETCH's — and nothing above compiles one, so a break in it would
# only show up the first time someone edited one of the sketches that use
# it.
add_test(NAME sketch_reload_named
  COMMAND Sketchbook
    ${SIGIL_SKETCH_DIR}/world_hud.cpp
    --frame ${CMAKE_BINARY_DIR}/sketch_reload_named.png)

# THE DIRECTORY FORM. A sketch that is a directory goes through the host
# as several translation units compiled apart and linked once, which
# nothing above exercises: every entry there is one file. This one is
# the entry of a directory sketch whose catalogue stands in a unit
# beside it, so a break in the unit list, the object cache or the link
# line shows up here and nowhere else.
add_test(NAME sketch_reload_directory
  COMMAND Sketchbook
    ${SIGIL_SKETCH_DIR}/dunhuang_star_chart/dunhuang_star_chart.cpp
    --frame ${CMAKE_BINARY_DIR}/sketch_reload_directory.png)
