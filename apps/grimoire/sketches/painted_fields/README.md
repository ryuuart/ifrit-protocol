# Painted fields

A thick satin-steel plate lies on a dark bench, and a gessoed field on it
carries a painting whose marks are its material: blue oil dragged in
ridges over a dry ground, rust scrubbed in short strokes, a copper
overprint pressed down in bands, heavy gilt lettering, pools of wet oil
medium, and one stroke of hot alloy standing proud of the field. The eye
should see the dry ridges catch a low key light that travels round the
plate on a rail, the wet pools turn into mirrors of a soft overhead panel
while the dry paint beside them stays matte, the copper and the gilt flash
as the key passes, and the alloy stroke glow, bloom and throw a warm stain
onto the paint around it while casting a real shadow across the field.
Compose writes every brush stroke into separate maps — height, wet
coverage and glow — and paints the page from them; World's lights,
environment, shadows and bloom do all of the shading.

## What it shows

- **Dry pigment ridges.** The blue and rust strokes are written into a
  height map as three passes, each narrower and higher, softened into one
  domed ridge and grooved by bristle noise, with a strand of its own for
  each bristle that breaks away at the stroke's ends; the page's normal
  is derived from that height, so the ridges stand up under a raking
  light and fall flat when it is square on.
- **Wet pools.** A second map holds where the medium is still wet. There
  the roughness drops toward a mirror, a clear coat appears and an amber
  glaze of medium deepens the paint, so the overhead panel shows in the
  pools as a sheet of light while the dry paint beside them stays matte.
- **Metal in the paint.** The copper bands and the gilt letters are
  metallic where they are painted and dielectric everywhere else on the
  page, so they swing between dark and bright as the key light travels.
- **A glowing alloy mark.** One stroke is also a body: the same band,
  lifted into a solid that stands on the field, glowing above white so
  the bloom pass picks it up, and casting the key light's shadow across
  the paint. A small warm sphere light over it lights the paint round
  it, and a blurred copy of it in the glow map is the paint's own faint
  emission there.

## How it is built

- `ctx.page` makes the page; `page.render(tree)` paints it as the maps of
  a surface and `page.surface()` is what the plate's face wears.
- `ctx.texture` paints three grey maps from one list of strokes:
  height, wet coverage and glow. Every stroke is a
  `compose::brush::ribbon` along an SVG spine with a `path::Profile` width
  law.
- `material::surface::normalFromHeight` turns the height into the
  normal; the wet map masks a smooth roughness and a clear coat over the
  dry ones; the glow map is every material's `emissionMap`.
- The alloy mark's solid is `path::band` of its spine lifted by
  `geometry::mesh::extrude`, wearing a surface with `emission` and an
  `emissionStrength` past one; `world::bloom` spreads it.
- The plate is a `geometry::mesh::box` with its face left open for a
  `geometry::mesh::quad` wearing the page.
- The glow's light on the paint is a `world::sphereLight` with a range.
- The key is a spot light riding `world::kit::rail` with `.along`, aimed
  at the plate with `lookAt` and casting shadows; the soft light is a
  `world::rectLight` shown by `world::kit::visibleLight`; a
  `world::kit::environmentMap` with two soft boxes gives the metals
  something to reflect.

## What to look at

The loop is 16 seconds, one lap of the key light's rail, which runs low
round the plate from behind it on the right, past the near right corner,
across the front to the left and round behind it again. At the capture
moment the key stands behind the plate on the left: the wet pools and the
wet-looking crests of the blue strokes light up against it, every bristle
strand throws a hairline of light, the copper band flashes, and the alloy
stroke's shadow falls toward the camera across the gesso. With the key at
the near right the field is lit square on and the ridges flatten, while
the overhead panel still shows in the middle pools; with it low at the
left the gesso's brush tooth and the strands stand out across the whole
field. The overhead panel never moves. The alloy stroke breathes twice a
lap, and the warm light it gives breathes with it.

The relief of the paint is shaded, not displaced: only the alloy stroke
casts a shadow, and the ridges keep the plate's silhouette flat.
