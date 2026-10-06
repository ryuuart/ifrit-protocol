# SigilWorld — 3D scenes as comparable values, executed on a GPU through
# Diligent Engine (Vulkan/MoltenVK on macOS) over a device SigilSkia
# shares. Each feature is its own static archive with its own tests and
# benchmarks, and links only the features beneath it, so a consumer that
# needs one links that one alone. `SigilWorld` is the umbrella: an
# interface over every feature, for a consumer of the whole library.
#
# The fixtures every test binary here shares live in test/, on no
# library's include path: nothing shipped may reach a fixture.
sigil_library_root(SigilWorld
  BRIEF "3D scenes as values, executed on a GPU over a shared device"
  DOCS docs/Groups.dox reference/DEVICE.md reference/STUDIES.md
       reference/NODES.md reference/FRAMES.md reference/PRESETS.md)

# The description: one node of a 3D scene as a comparable value, in its
# five kinds, and the light values a light node carries. It stands on the
# vocabularies beneath it and on the reconciler's kernels, and holds
# nothing retained, so a consumer that only authors trees links this.
add_subdirectory(node)

# A frame as three declared things — the scene, an ordered list of
# passes and the readbacks — the resources those passes write, and the
# seam a pass is performed through. No device: the executor that draws is
# supplied by the device feature below, as a value.
add_subdirectory(frame)

# The ordering: what runs when, which resources share a surface, the
# barriers between them, and how a pass's selection reaches the pixels.
# It reads declarations and draws nothing.
add_subdirectory(graph)

# The retained side: the reconcile host, the entity store, the resource
# store and the declared phases, executing a frame's passes on the
# runtime it carries.
add_subdirectory(scene)

# The frame executor on the device SigilGeometryDevice creates. Its
# tests skip, and say so, on a machine with no Vulkan runtime.
add_subdirectory(diligent)

# The presets: nodes composed out of the verbs above — a three-point rig,
# a turntable, a lit set, a panorama — and the front door every scene
# includes. It stands on the description and the frame, and nothing here
# draws or retains.
add_subdirectory(kit)

# The umbrella, for a consumer that takes the library rather than a
# feature of it.
sigil_library(SigilWorld
  INTERFACE SigilWorldNode SigilWorldFrame SigilWorldGraph SigilWorldScene
            SigilWorldKit SigilWorldDiligent)

# Every API name the prose spells, compiled against the headers that own
# it — the README and both chapters under reference/, because a chapter
# is the same canon as the page that links it, and the device tier's own
# vocabulary lives there.
# The libraries this one stands on are scanned so a borrowed name
# resolves where it lives: the mesh currency and its runtimes, the
# reconcile phases and the settle policy, the shader compiler, and the
# stagger whose origins mean the same thing in a scene as in a line of
# type. The entity store's own names are EnTT's, whose headers no
# include root here carries.
sigil_doc_probes(world
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/DEVICE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/STUDIES.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/NODES.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/FRAMES.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/PRESETS.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry
           ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial
           ${CMAKE_CURRENT_SOURCE_DIR}/../media/include/sigilmedia
           ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
  PRELUDES "<sigilmotion/schedule/Stagger.h>"
           "<sigilcore/reconcile/Phases.h>"
           "<sigilcore/cache/Settle.h>"
           "<sigilcore/cache/Cache.h>"
           "<sigilcore/cache/Volatility.h>"
           "<sigilmaterial/advanced/Combine.h>"
           "<sigilmaterial/advanced/Terms.h>"
           "<sigilmaterial/slang/SlangCompiler.h>"
           "<sigilmaterial/surface/Surface.h>"
           "<sigilmaterial/core/Instance.h>"
           "<sigilmaterial/texture/Flipbook.h>"
           "<sigilgeometry/mesh/pop/Kernel.h>"
           "<sigilgeometry/mesh/pop/device/Cook.h>"
           "<sigilgeometry/mesh/pop/device/Sweep.h>"
           "<sigilgeometry/mesh/render/device/Painter.h>"
  EXCLUDE "geometry::device::MeshResidency=declared in SigilGeometry's device residency feature's private headers, which no consumer include path carries"
          "entt::entity=EnTT's own, named as the dependency the entity store is: its headers are not scanned"
          "entt::registry=EnTT's own, named as the dependency the entity store is: its headers are not scanned"
  FLOORS 44,34,40,0,0
  LIBRARIES SigilWorld SigilGeometry SigilCoreReconcile SigilMotionSchedule
            SigilMaterial)
