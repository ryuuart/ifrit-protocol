sigil_library_root(SigilPython
  BRIEF "Reusable Python bindings for native Sigil library values and drawing")

# ONE DIRECTORY PER LIBRARY, ONE FILE PER PACKAGE. Every file this
# target compiles is named here, so an author filling in a package
# edits nothing another author is also editing.
sigil_library(SigilPython
  SOURCES
    Python.cpp Bindings.cpp Extend.cpp PublicNames.cpp compose/BrushMarks.cpp
    compose/Composer.cpp compose/Derive.cpp compose/ElementEdges.cpp
    compose/Feed.cpp compose/Instancing.cpp compose/KitRows.cpp
    compose/Masks.cpp compose/PaintPrograms.cpp compose/Schemes.cpp
    compose/Selectors.cpp compose/StrokeOptions.cpp compose/TextEffects.cpp
    compose/TextureScene.cpp core/Features.cpp core/RecordProtocol.cpp
    core/Values.cpp data/Connection.cpp draw/CanvasSeam.cpp geometry/Mesh.cpp
    geometry/MeshCamera.cpp geometry/MeshCurve.cpp geometry/Outlines.cpp
    geometry/PathOperations.cpp geometry/PointBuilder.cpp
    geometry/Profiles.cpp geometry/Shapes.cpp geometry/Solids.cpp
    media/Images.cpp media/Video.cpp io/Frames.cpp material/Core.cpp
    material/Environment.cpp material/Field.cpp material/PaintEffect.cpp
    material/Pattern.cpp material/Shader.cpp material/Substance.cpp
    material/Texture.cpp measure/Measure.cpp motion/AnimatableForms.cpp
    motion/Clock.cpp motion/Schedule.cpp motion/Signals.cpp weave/Cascade.cpp
    weave/Fonts.cpp weave/PaintLayers.cpp weave/Tables.cpp weave/Unicode.cpp
    compose/Convert.cpp compose/Document.cpp compose/Elements.cpp
    compose/Kit.cpp compose/Nodes.cpp data/Data.cpp draw/Brush.cpp
    draw/Pen.cpp geometry/Geometry.cpp io/Resources.cpp material/Material.cpp
    motion/Motion.cpp skia/Values.cpp weave/Layout.cpp weave/Type.cpp
    world/World.cpp
  HEADERS
    Bindings.h Extend.h PublicNames.h Python.h compose/Convert.h compose/Kit.h
    compose/Nodes.h compose/Operators.h compose/Registration.h
    core/Registration.h data/Convert.h data/Registration.h draw/Canvas.h
    draw/Registration.h geometry/Casters.h geometry/Registration.h io/Hub.h
    io/Registration.h media/Registration.h material/Convert.h
    material/Registration.h measure/Registration.h motion/Convert.h
    motion/Registration.h skia/Registration.h skia/Values.h
    weave/Registration.h world/Registration.h
  PUBLIC
    pybind11::headers unofficial::skia::skia SigilComposeCore SigilComposeKit
    SigilMotionValues SigilComposeBrush SigilGeometryPath SigilGeometryKit
  PRIVATE
    SigilCoreCompute SigilCoreHardware SigilCoreReconcile SigilComposeDraw
    SigilComposeTexture SigilComposeTypography
    SigilDataConnection SigilDataDecode SigilDataQuery SigilDataRead SigilDataScale
    SigilDataTable SigilDataValues SigilDraw SigilDrawBrush
    SigilDrawBrushFormat SigilGeometryDevice SigilGeometryMesh
    SigilGeometryMeshCamera SigilGeometryMeshCodec SigilGeometryMeshCurve
    SigilGeometryMeshPop SigilGeometryPathBlend
    SigilIOHub SigilIOFrames SigilIOSource SigilIOTransport
    SigilMaterialCore SigilMaterialField SigilMaterialMask
    SigilMaterialSurface
    SigilMaterialOcio SigilMaterialPattern SigilMaterialSdf SigilMaterialSkia
    SigilMaterialProgram SigilMaterialSubstance
    SigilMaterialSlang SigilMaterialTexture SigilMeasure
    SigilMedia SigilMotionBind SigilMotionClock SigilMotionPhysics
    SigilMotionSchedule SigilSkiaDraw SigilSkiaGraphite
    SigilWeaveChoreograph SigilWeaveFonts SigilWeaveKit
    SigilWeaveLayout SigilWeavePaint SigilWeaveParagraph SigilWeavePorts
    SigilWeaveQuery SigilWeaveStyle SigilWeaveUnicode SigilWorldNode
    SigilWorldFrame SigilWorldGraph SigilWorldKit
    SigilWorldScene)

# A library whose SDK this build may not carry. The target's absence
# leaves out its sources and its feature definition, and a Python
# process asks importlib whether the module it would register is
# there.
if(TARGET SigilScry)
  target_link_libraries(SigilPython PRIVATE SigilScry)
  target_compile_definitions(SigilPython PUBLIC SIGIL_PYTHON_HAS_SCRY)
endif()
if(TARGET SigilComposeWeb)
  target_link_libraries(SigilPython PRIVATE SigilComposeWeb)
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
  LIBRARIES SigilPython SigilWeaveKit
  DEFINITIONS SIGIL_PYTHON_PACKAGE_DIR="${SIGIL_PYTHON_PACKAGE_DIR}")

sigil_doc_probes(python
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  FLOORS 1,0,0,30,0
  LIBRARIES SigilPython)
