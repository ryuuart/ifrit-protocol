sigil_library_root(SigilPython
  BRIEF "Reusable Python bindings for native Sigil library values and drawing")

# ONE DIRECTORY PER LIBRARY, ONE FILE PER PACKAGE. Every file this
# target compiles is named here, so an author filling in a package
# edits nothing another author is also editing.
sigil_library(SigilPython
  SOURCES
    Python.cpp Bindings.cpp Extend.cpp PublicNames.cpp
    compose/BrushComposites.cpp compose/BrushMarks.cpp compose/Composer.cpp
    compose/DecorationPrimitives.cpp compose/DecorationSeam.cpp
    compose/Derive.cpp compose/ElementEdges.cpp compose/Feed.cpp
    compose/Instancing.cpp
    compose/KitAnnotations.cpp compose/KitEras.cpp compose/KitKinetic.cpp
    compose/KitLegibility.cpp compose/KitOrnament.cpp
    compose/KitPixelType.cpp compose/KitRoutes.cpp compose/KitRows.cpp
    compose/KitSprites.cpp compose/KitStrokes.cpp compose/KitTypeset.cpp
    compose/LayerStyles.cpp compose/Lines.cpp compose/Masks.cpp
    compose/MediaLeaves.cpp compose/PaintPrograms.cpp compose/PixelStyles.cpp
    compose/Schemes.cpp compose/Selectors.cpp compose/Sheets.cpp
    compose/TextEffects.cpp
    compose/TextureScene.cpp core/Compute.cpp core/Features.cpp
    core/RecordProtocol.cpp core/Values.cpp data/Connection.cpp
    data/Schema.cpp data/Tables.cpp draw/CanvasSeam.cpp draw/Standalone.cpp
    geometry/Charts.cpp geometry/DeviceHandle.cpp geometry/Frames.cpp
    geometry/Mesh.cpp geometry/MeshCamera.cpp geometry/MeshCodec.cpp
    geometry/MeshCurve.cpp geometry/MeshPoints.cpp geometry/MeshRender.cpp
    geometry/PathEditing.cpp geometry/PathOperations.cpp
    geometry/PointBuilder.cpp geometry/PointKernels.cpp
    geometry/PointOperations.cpp geometry/Polylines.cpp geometry/Profiles.cpp
    geometry/Regions.cpp geometry/Seams.cpp geometry/Shapes.cpp
    geometry/Structures.cpp image/Meaning.cpp image/Values.cpp
    io/Dispatch.cpp io/Publish.cpp io/Sources.cpp material/Core.cpp
    material/Environment.cpp material/Field.cpp material/KitGrained.cpp
    material/KitText.cpp material/PaintEffect.cpp material/Pattern.cpp
    material/Shading.cpp material/SkiaDraw.cpp material/Texture.cpp
    material/TextureSets.cpp measure/Measure.cpp motion/AnimatableForms.cpp
    motion/Clock.cpp motion/Lanes.cpp motion/Particles.cpp motion/Physics.cpp
    motion/Schedule.cpp motion/Signals.cpp skia/Effects.cpp skia/Fonts.cpp
    skia/Paths.cpp skia/Surfaces.cpp video/Video.cpp weave/Cascade.cpp
    weave/Choreography.cpp weave/Flows.cpp weave/Fonts.cpp weave/Ports.cpp
    weave/Shaping.cpp weave/Tables.cpp weave/Unicode.cpp
    world/Description.cpp world/Device.cpp world/Environment.cpp
    world/Geometry.cpp world/Passes.cpp world/Plan.cpp world/Targets.cpp
    world/View.cpp compose/Convert.cpp compose/Document.cpp
    compose/Elements.cpp compose/Kit.cpp compose/Nodes.cpp
    data/Data.cpp draw/Brush.cpp draw/Pen.cpp geometry/Geometry.cpp
    io/Resources.cpp material/Material.cpp motion/Motion.cpp skia/Values.cpp
    weave/Layout.cpp weave/Type.cpp world/World.cpp
  HEADERS
    Bindings.h Extend.h PublicNames.h Python.h compose/Convert.h
    compose/Kit.h compose/Nodes.h compose/Registration.h
    core/Registration.h data/Convert.h
    data/Registration.h draw/Canvas.h draw/Registration.h geometry/Casters.h
    geometry/Registration.h image/Registration.h io/Hub.h io/Registration.h
    material/Convert.h material/Registration.h measure/Registration.h
    motion/Convert.h motion/Registration.h scry/Registration.h
    skia/Registration.h skia/Values.h substance/Registration.h
    usd/Registration.h video/Registration.h weave/Registration.h
    world/Registration.h
  PUBLIC
    pybind11::headers unofficial::skia::skia SigilComposeCore SigilComposeKit
    SigilMotionValues SigilComposeBrush SigilGeometryPath SigilGeometryKit
  PRIVATE
    SigilCoreCompute SigilCoreHardware SigilCoreReconcile SigilComposeDraw
    SigilComposeTexture SigilComposeTypography SigilComposeVideo
    SigilDataConnection SigilDataDecode SigilDataQuery SigilDataScale
    SigilDataTable SigilDataValues SigilDraw SigilDrawBrush
    SigilDrawBrushFormat SigilGeometryDevice SigilGeometryMesh
    SigilGeometryMeshCamera SigilGeometryMeshCodec SigilGeometryMeshCurve
    SigilGeometryMeshPop SigilGeometryMeshRender SigilGeometryPathBlend
    SigilImageAsset SigilImageDecode SigilImageEncode SigilImageField
    SigilIOHub SigilIOPublish SigilIOSource SigilIOTransport
    SigilMaterialCore SigilMaterialField SigilMaterialKit SigilMaterialMask
    SigilMaterialOcio SigilMaterialPattern SigilMaterialSdf SigilMaterialSkia
    SigilMaterialSlang SigilMaterialStock SigilMaterialTexture SigilMeasure
    SigilMotionBind SigilMotionClock SigilMotionPhysics SigilMotionSchedule
    SigilSkiaDraw SigilSkiaGraphite SigilVideoCore SigilVideoDecode
    SigilVideoEncode SigilWeaveChoreograph SigilWeaveFonts SigilWeaveKit
    SigilWeaveLayout SigilWeavePaint SigilWeaveParagraph SigilWeavePorts
    SigilWeaveQuery SigilWeaveStyle SigilWeaveUnicode SigilWorldElement
    SigilWorldFrame SigilWorldGraph SigilWorldKit SigilWorldLight
    SigilWorldScene)

# A library whose SDK this build may not carry. The target's absence
# leaves out its sources and its feature definition, and a Python
# process asks importlib whether the module it would register is
# there.
if(TARGET SigilScry)
  target_sources(SigilPython PRIVATE scry/Scry.cpp)
  target_link_libraries(SigilPython PRIVATE SigilScry)
  target_compile_definitions(SigilPython PUBLIC SIGIL_PYTHON_HAS_SCRY)
endif()
if(TARGET SigilComposeWeb)
  target_link_libraries(SigilPython PRIVATE SigilComposeWeb)
  target_compile_definitions(SigilPython PUBLIC SIGIL_PYTHON_HAS_WEB)
endif()
if(TARGET SigilSubstance)
  target_sources(SigilPython PRIVATE substance/Substance.cpp)
  target_link_libraries(SigilPython PRIVATE SigilSubstance)
  target_compile_definitions(SigilPython PUBLIC SIGIL_PYTHON_HAS_SUBSTANCE)
endif()
if(TARGET SigilUsd)
  target_sources(SigilPython PRIVATE usd/Usd.cpp)
  target_link_libraries(SigilPython PRIVATE SigilUsdRead SigilUsdRuntime SigilUsdWrite)
  target_compile_definitions(SigilPython PUBLIC SIGIL_PYTHON_HAS_USD)
endif()

target_include_directories(SigilPython SYSTEM PUBLIC ${Python3_INCLUDE_DIRS})
# Executables embed Python; extension modules resolve Python symbols from
# their importing interpreter and must not load another libpython.
target_link_libraries(SigilPython INTERFACE
  "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:SIGIL_PYTHON_EXTENSION>>>:Python3::Python>")

# This consumer deliberately links no sketch target. Common registration,
# retained callbacks and native kit values must work in another host.
sigil_test(python_test
  SOURCES test/BindingsTest.cpp
  LIBRARIES SigilPython
  DEFINITIONS SIGIL_PYTHON_PACKAGE_DIR="${SIGIL_PYTHON_PACKAGE_DIR}")

sigil_doc_probes(python
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  FLOORS 1,0,0,30,0
  LIBRARIES SigilPython)
