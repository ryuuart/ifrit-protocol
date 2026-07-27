# SigilWorld

Namespace `sigil::world`, target `SigilWorld`, headers
`include/sigilworld/`. Diegetic surfaces in real 3D, rendered by
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine)
(the `diligent-engine` port from the sigil-vcpkg-registry). Where
SigilShape's `Space.h` fakes depth inside one SkCanvas, SigilWorld owns
a GPU device and renders meshes with a depth buffer, MSAA, and a
PBR-lite shading model — panels IN a scene, not sprites OVER one.

## The three bridge contracts

- geometry is `sigil::shape::Mesh` — extrude/revolve/grid/cylinderPanel
  output uploads directly;
- panel content is any `SkImage` — a compose scene, a web view, a
  loader-decoded SVG — uploaded as the surface's baseColor texture
  (`Material::unlit` for self-lit screens);
- the camera is `sigil::shape::space::Camera`, so a Skia-composited
  scene and a World render agree about where things sit.

Headless by design: `World::create()` needs no window, `render()` draws
into an offscreen target (RGBA8 + D32, MSAA resolve), `readback()`
returns the frame as a raster SkImage, `savePng()` encodes it. A
swapchain path can join later without touching the scene API.

## Backend story (macOS)

Vulkan over MoltenVK: `brew install molten-vk vulkan-loader`. The
Metal backend is upstream-commercial and not in the port; the GL
backend is compiled but unwired here (the engine layer keeps D3D/GL
open for the Windows/Linux ports). Two local pieces make the Vulkan
path work without environment surgery:

- **VolkShim.c** — the port's static archives expect the volk loader;
  the shim compiles vendored volk (`thirdparty/volk`, MIT, pinned
  vulkan-sdk-1.4.321.0) with a replaced `volkInitialize` that also
  tries the absolute Homebrew paths (`/opt/homebrew/lib`) and points
  the ICD loader at MoltenVK's manifest when the environment doesn't
  (macOS `dlopen` searches neither `/opt/homebrew/lib` nor
  already-loaded leaf names, so stock volk cannot find a Homebrew
  install). `SIGILWORLD_VULKAN_LIBRARY` overrides the candidate list.
- **Shading**: one HLSL source (compiled to SPIR-V by the glslang
  Diligent ships) with a GGX + hemisphere-ambient PBR-lite pixel
  shader; matrices upload as raw column-major SkM44 dumps with
  `mul(M, v)` column-vector math — deliberately dodging HLSL
  `row_major` translation quirks. Diligent's Vulkan backend normalizes
  clip-space y, so the projection carries no flip (the
  `QuadAtPositiveYAppearsInTopHalf` test pins this).

`Material`: baseColor (alpha < 1 routes to the blended, depth-sorted
pass), metallic/roughness, emissive, texture (sRGB view), unlit.
`Lighting`: one sun + sky/ground hemisphere.

## The scene layer

`Scene.h` applies SigilCompose's core lesson without importing its
kernel: describe the 3D scene as a value tree
(`scene::group/surface/panel` with keys, transforms, children), call
`Scene::render(root)`, and a reconciler diffs against the last render —
transform-only changes are `setTransform`, identical leaves are kept,
only genuinely new/changed surfaces upload. `render()` returns
`Stats{added, removed, moved, kept}` so pruning is observable, the same
visibility compose's ledgers taught. Identity is the key path; meshes
reuse by shared_ptr identity; `panel()` quads are cached per size so
panels are stable by construction.

## Demo and tests

```
./build/bin/Debug/world_demo [outdir] [assetdir]   # 4 PNG camera shots
./build/bin/Debug/world_test                        # skips without a Vulkan runtime
```

The demo builds a cockpit: three emissive UI cards, a curved
`cylinderPanel` ticker, a brushed floor slab, gold extruded star,
chrome superellipsoid, a glass pane — and, when `fetch_assets` has
run, the Ghostscript tiger decoded from SVG through SigilLoader onto a
poster panel (`world_poster.png` frames it). Tests skip — not fail —
when no Vulkan runtime exists, so CI without a GPU stays green.
