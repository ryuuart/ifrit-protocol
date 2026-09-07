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
  BRIEF "3D scenes as values, executed on a GPU over a shared device")

# The light feature stands alone: emitter VALUES over glm, with no
# device and no registry, so a consumer that only needs to say where the
# lights are links it without anything that draws.
add_subdirectory(light)

# The description: one node of a 3D scene as a comparable value. It
# stands on the vocabularies beneath it and on the reconciler's kernels,
# and holds nothing retained, so a consumer that only authors trees links
# this and the light feature above.
add_subdirectory(element)

# A frame as three declared things — the scene, an ordered list of
# passes and the readbacks — the resources those passes write, and the
# seam a pass is performed through. No device: the executor that ships
# here is the CPU one.
add_subdirectory(frame)

# The ordering: what runs when, which resources share a surface, the
# barriers between them, and how a pass's selection reaches the pixels.
# It reads declarations and draws nothing.
add_subdirectory(graph)

# The retained side: the reconcile host, the entity store, the resource
# store and the declared phases, drawing through SigilGeometry's mesh
# runtime and executing a frame's passes on the runtime it carries.
add_subdirectory(scene)

# The frame executor on the device SigilGeometryDevice creates. Its
# tests skip, and say so, on a machine with no Vulkan runtime.
add_subdirectory(diligent)

# The presets: elements composed out of the verbs above — a three-point
# rig, a turntable, a lit set. It stands on the description and on
# SigilMaterial's kit, and nothing here draws or retains.
add_subdirectory(kit)

# The umbrella, for a consumer that takes the library rather than a
# feature of it.
sigil_library(SigilWorld
  INTERFACE SigilWorldElement SigilWorldFrame SigilWorldGraph SigilWorldScene
            SigilWorldLight SigilWorldKit SigilWorldDiligent)
