# SigilWorld — the lighting model

How a lit body is shaded on the device: what each light delivers, how it
falls off, the terms the surface answers with, how colour is carried, and
how the camera reads the result. The shading terms themselves live in
SigilMaterial's `core/shaders/Shading.slang` and are named in
`<sigilmaterial/advanced/Terms.h>`; the scaffold that composes them is
`diligent/shaders/Surface.slang`.

## In one paragraph

Every lit pixel is shaded on its own, in linear light. Each light reaches
the pixel at the falloff its distance and cone give there, and a light
that casts a shadow reaches it only where nothing stands between; the surface
answers through a microfacet model — a Lambert diffuse and a GGX specular
lobe, split by metalness and Fresnel — with a clear coat as a second lobe
over it, and, where a surface states them, a sheen, a brushing that
stretches the highlight, a thin film that tints the reflectance, and a
dielectric's own specular strength and colour; light arriving from everywhere (the environment, ambient and
hemisphere lights) adds its diffuse and mirrored share with the energy a
rough surface scatters more than once put back, darkened by occlusion;
emission adds its own light; and the sum is written as light into a
half-float target. Every pass over the picture — a blur, a composite, a
bloom — works on that light, and the final stage exposes it, passes it
through the camera's tone curve and encodes it for the display — last,
once, when the picture leaves the frame. Nothing
lights a scene that the scene did not state: a scene with no ambient light and
no environment has black shadows.

## What a light delivers

`color` is a colour as an author types it, decoded into linear light;
`intensity` scales it without bound.

| light | made by | delivers |
|---|---|---|
| directional | `directionalLight(direction, options)` | `color * intensity` to a surface square to it, everywhere; no falloff |
| point | `pointLight(options)` | `color * intensity / d²` at distance `d` |
| spot | `spotLight(direction, options)` | a point light within a cone |
| rect | `rectLight(direction, options)` | a panel `width` by `height` whose FACE has radiance `color * intensity`, shining from its front face only; far off, `intensity * width * height / d²` times the cosine from its face |
| sphere | `sphereLight(options)` | a point light with a `radius`: `color * intensity / d²` from the whole ball |
| disk | `diskLight(direction, options)` | a round panel of `radius` shining from its front face: `color * intensity / (d² + radius²)` times the cosine from its face |
| ambient | `ambientLight(options)` | radiance `color * intensity` arriving equally from every direction |
| hemisphere | `hemisphereLight(options)` | radiance `color * intensity` from above the horizon its node's up names, `groundColor * intensity` from below |

A node's scale sizes a light with a size as it sizes a body: a rect's
width and height by how far it stretches the axes they run along, a
disk's radius by the mean of those two, and a ball's by the mean of all
three — so a panel and the mesh `kit::visibleLight` stands beside it stay
one size under a scaled group.

A white matte surface square to a directional light of intensity one
shows one over pi of it — a matte surface scatters what it receives over
the whole hemisphere. Under an ambient light of one it shows all of it,
as under a uniform environment of that radiance.

**Stating a light by what it delivers.** A positioned light's intensity
is what it delivers at one unit, which at the pixel-sized distances a scene
is built at is a large number. `world::intensityAt(strength, distance)`
is the intensity that delivers `strength` at `distance` — the strength
times the distance squared — so a rig reads in the numbers it lights its
subject with. It takes a moving strength too, and the light follows it.
The kit's three-point rig states every lamp this way, by what it
delivers at the subject.

## Lights with a size

A rect, a sphere and a disk light have a size in scene units and stand
centred where their node is placed. A rect and a disk shine from their
FRONT face only — the face their direction leaves by — and nothing behind
that face is lit. A rect's width runs along its node's x axis made square
to its direction (its z axis for a light travelling along x) and its
height square to both, so turning the node turns the panel; a node's
scale does not resize a light.

**Units.** A rect's intensity is the RADIANCE of its face — how bright
the panel itself is, the way UsdLux states an emitter: a larger panel
at the same intensity delivers more, and far off a panel of area `A`
delivers what a disk light of intensity `intensity * A` would. A sphere
and a disk keep a point light's reading — the intensity is what the whole
light delivers at one unit along its axis — so their size softens the
light without changing how much of it there is, a sphere of radius zero
IS a point light, and `world::intensityAt` states either of them as it
states a point light. The face of a sphere or a disk has radiance
`intensity / (pi * radius²)`.

**The technique.** Each sized light is shaded by two readings of it.

- The DIFFUSE side receives what the whole light delivers. A rect's is
  exact: its cosine-weighted solid angle, integrated edge by edge
  (`quadIrradiance`) over the part of it above the surface's horizon, the
  part below clipped away. A sphere above the horizon delivers exactly
  what a point light at its centre would, and that is what it uses; a
  disk uses `intensity * cos / (d² + radius²)`, exact on its axis.
- The SPECULAR side is lit as by a point light at the light's
  REPRESENTATIVE POINT — the point of the light nearest the reflected ray
  (`rectPoint`, `diskPoint`, `spherePoint`), so a smooth surface mirrors
  the light's own outline — carrying the solid angle the light fills
  (`quadSolidAngle`, `sphereSolidAngle`, `diskSolidAngle`). The lobe's
  peak is scaled by `(a / a')²`, where `a'` is the lobe's width widened by
  half the angular radius of that solid angle (`areaNormalization`), so a
  highlight spread by a large light is no brighter in sum for it.

This is the representative-point treatment, not an integration of the
GGX lobe over the light: the outline of a rectangle on a mirror is exact,
but its brightness on a rough surface is an estimate, and the highlight
of a light partly below the horizon is lit as if all of it were above.

**Each lobe under a sized light.** The base diffuse and specular are as
above. A clear coat is a second representative point, found along the
coat's own reflected ray and widened at the coat's roughness. A brushing
keeps the isotropic representative point and stretches the lobe there as
for a point light. A sheen is lit as by a point light at the light's
centre carrying the whole light. A thin film moves the head-on
reflectance before any light is read, so it applies unchanged. A
dielectric's specular strength and colour apply unchanged.

**Shadows.** A sized light that states `castShadow` casts from its
centre through the same maps: a sphere as a point light's six faces, a
rect or a disk as a spot aimed along its face whose map opens fifty
degrees either side of its normal, so a body further off its face than
that casts nothing. Its shadows are no softer for its size; the edge is
the filter's, as for every light.

**Seeing the light.** A light is not a body; `world::kit::visibleLight`
puts an unlit mesh of its shape beside it.

## Falloff

- **Inverse square.** A point or spot light reaches a pixel at
  `1 / d²` of its intensity (`distanceFalloff`).
- **Range.** Past zero, `range` brings that smoothly to zero:
  the inverse square times `(1 − (d / range)⁴)²`, clamped, which has no
  edge and is exactly zero at the range. Zero, the default, reaches
  everywhere. A sphere light's falloff is windowed the same way, and a
  rect's or a disk's whole delivery — what its face sends the diffuse
  side and its highlight — by that window measured from its centre
  (`rangeWindow`).
- **Cone.** A spot is full strength within `innerAngle` degrees of its
  aim and dark beyond `outerAngle`; between them the square of where the
  cosine stands (`coneFalloff`).

`world::attenuation` is the same arithmetic on the host.

## The surface's terms

| term | what it computes |
|---|---|
| `diffuseColor` | the base colour less the four per cent a dielectric reflects head on; none for a metal |
| `specularColor` | the head-on reflectance: four per cent for a dielectric, the base colour for a metal |
| `reflectance90` | the grazing reflectance: white for any real material |
| `distributionGgx` | how densely the microfacets face the half vector; the width is roughness squared |
| `visibilityGgx` | the height-correlated Smith masking and shadowing, over the four cosines |
| `fresnelSchlick` | the reflectance rising from head-on to grazing |
| `directDiffuse`, `directSpecular` | one light's two sides, times the cosine it arrives at |
| `environmentSpecular` | what the surface mirrors of light from everywhere, weighted by the split sum |
| `environmentDiffuse` | the diffuse share of light from everywhere, with the energy lost to single scattering put back |
| `hemisphereIrradiance` | the exact cosine-weighted blend of two half-spaces |
| `clearcoatTransmittance` | what a coat of a weight lets through to the surface beneath |
| `distributionSheen`, `visibilitySheen` | the sheen's sideways-standing fibres, and how they shadow one another |
| `directSheen`, `environmentSheen` | one light's sheen, and the sheen of light from everywhere |
| `sheenScaling` | what the surface beneath a sheen keeps |
| `distributionGgxAnisotropic`, `visibilityGgxAnisotropic` | the GGX lobe and its visibility with a width of their own along the brushing |
| `directSpecularAnisotropic`, `anisotropicBentNormal` | one light's brushed highlight, and the normal light from everywhere is mirrored about |
| `anisotropyTangent` | the direction the brushing runs, turned and made square to the normal |
| `iridescenceFresnel`, `iridescentF0` | a thin film's reflectance, and the head-on reflectance it leaves |
| `dielectricReflectance`, `specularColorFrom`, `reflectance90From`, `diffuseColorFrom` | a dielectric's reflectance at its own strength and colour, and the diffuse side it leaves |

**Roughness drives the highlight**: there is no scene-wide gloss or
specular strength. A smooth surface concentrates a light into a small,
bright spot; a rough one spreads the same energy wide.

**Light from everywhere** is the environment's panorama — its cosine
convolution for the diffuse side, its prefiltered chain at the level the
roughness picks for the mirrored side — plus every ambient and hemisphere
light. The chain is filtered on the device the first time a panorama is
met: level k is the panorama convolved with the GGX lobe of roughness k
over the last level, gathered by direction, so a reflection is as wide
near a pole as at the horizon and a near-mirror resolves a small window
the panorama holds. A surface whose recipe is
`surface::Reflection::Additive` mirrors `reflectionWeight` of that
radiance instead, with no Fresnel and no energy taken from the rest. The split sum's scale and bias come from a table SigilMaterial
integrates from the same distribution and visibility the lights use
(`material::environmentBrdfTable`). The single-bounce share and the
multiply-scattered share together return all of a uniform surround to a
white metal at every roughness.

**Occlusion** darkens light from everywhere and nothing else. A light
from one direction is not occluded by a surface's own creases.

**Clear coat.** `SurfaceOptions::clearcoat` (weight),
`clearcoatRoughness` and `clearcoatNormal` describe a colourless
dielectric layer of index 1.5 over the surface: a second specular lobe
under every light and from everywhere, with the surface beneath dimmed by
what the coat reflects. Without its own normal the coat lies smooth over
the geometry.

**Transmission.** `SurfaceOptions::transmission` (weight, a number or a
map), `ior`, `thickness` (scene units, a number or a map; zero is a thin
wall), `attenuationColor` with `attenuationDistance`, and `dispersion`
describe light passing THROUGH the surface, in the glTF physical
material's terms. It is an optional lobe (`Lobes::transmission`): only a
surface that states it compiles it. Per pixel:

- the surface is read as a LENS: its front is the surface itself, bent as
  much as the surface curves where the pixel stands, and its back stands
  `thickness` behind the front along the normal, curved the same way — a
  flat card is a slab with a parallel back, a ball as thick as it is wide
  is a ball. The eye's ray is bent into the medium at the front by `ior`
  (`refraction`), crosses to the back, is bent again leaving it, and goes
  on until it meets what the depth says stands there, where the frame's
  **copy of the scene behind** is read. So a slab shifts what is behind it
  sideways and keeps its direction, and a ball lens turns what stands far
  behind it round. At an index of one, or through a thin wall, the ray
  carries straight on and what is behind is seen where it stands;
- where the ray lands off the picture, or on something the depth says
  stands nearer the eye than the surface, the panorama along the ray
  leaving stands in; with no panorama, the nearest part of the picture;
- `thicknessAttribute` names an instance scalar that multiplies
  `thickness` on each copy, so one body of drops of many sizes is as thick
  as each drop is;
- a rough surface reads a blurrier level of the copy — its roughness
  scattered by how strongly it bends (`transmissionRoughness`), so an
  index of one blurs nothing;
- with `dispersion` above zero the three colours are bent by three
  indices spread about `ior` (`dispersedIors`) and read at three places,
  so an edge seen through the glass fringes, red bent least;
- what comes through is tinted by the base colour and dimmed by the
  VOLUME: light that has crossed `attenuationDistance` of the medium has
  become `attenuationColor`, farther deeper still, over the length the
  bent ray travelled (`volumeTransmittance`); a distance of zero takes
  nothing out;
- energy: what passes is the share the reflection did not take
  (`transmittedShare` of the split sum's specular albedo), times the
  weight; the diffuse side gives up that weight, and a metal transmits
  nothing — so reflected and transmitted light never exceed what arrived;
- the reflection is the interface's: head on, what the index reflects
  (`reflectanceOfIndex`, four per cent at one and a half), times
  `specular` and its colour, rising at the grazing angle only as far as an
  interface reflecting so little can. At an index of one there is no
  interface, so a transmitting body mirrors nothing and the page behind a
  curved one is seen exactly as behind a flat one.

**Layers of refraction.** The copy of the scene is taken before the
first body that sees through itself, and taken again before one standing
in front of another drawn since the last copy, where the two overlap in
the picture — so glass behind glass shows both, as deep as the bodies are
layered, while drops standing side by side on one pane share one copy.
Nothing is cast through a transmissive body — no caustic — and it casts no
shadow at all (see Shadows).

**Distortion.** `distortion` (a two-channel DATA map, half-grey still)
with `distortionStrength` (scene units at the surface) displaces what
stands behind the surface along the surface's own directions — red the
way u runs, green the way a normal map's green points — so a map that
pushes along v on a revolved drop pushes along its radius however the drop
is turned. It is read from the same copy, with no light involved: the
surface's own lit colour is laid over the displaced picture by its alpha,
so an invisible surface is the displacement alone — heat haze, a
shockwave — and a surface that also transmits has what it passes displaced
the same way, whatever its alpha. It is the lobe `Lobes::distortion`,
carried only with a map at a strength other than zero; an unlit surface
does not carry it.

**Both sides.** A body whose back face is drawn is lit as the side it
shows.

## Shadows

A light that states `castShadow` darkens what stands behind a body from
it. The shadow takes that light's direct contribution — its diffuse, its
highlight, its sheen and its clear coat — and nothing else: ambient and
hemisphere lights, the environment and emission are never shadowed, so a
second light that does not cast still lights what the first one's shadow
covers.

**Which lights cast.** Off unless stated, so a scene written before shadows
looks the same.

| light | its map |
|---|---|
| directional | one orthographic map along the light, square across, covering the sphere round what the camera sees — out to `shadow.distance` in front of it when that is past zero — or the scene's own box where that is smaller, and deep enough to hold every body between the light and them; its corner snapped to whole texels in the light's own frame. The sphere's size is the lens's and not the way the camera faces, so the texels stand still in the world as the camera turns and a still shadow's edge does not crawl. With `shadow.cascades` past one, that reach is split along the camera's depth into as many slices — growing with the distance, three parts of the way from even to geometric — each fitted with its own map, so the nearer ones cover less at the same size; a point reads the slice its distance in front of the camera falls in, crossing into the next over the last tenth of it (`shadowCascade`, `shadowCascadeBlend`) |
| spot | one perspective map from where it stands along its aim, opening a little wider than its cone, reaching its range or the far side of the scene |
| point | six perspective maps, one along each axis each way; a point is read through the face it stands most squarely in front of |
| rect, disk | six perspective maps from the panel's centre, one along its face, one behind it and four across, so its whole front half is covered; a point is read through the face it stands most squarely in front of |
| sphere | a point light's six maps from its centre |
| ambient, hemisphere | none: light from everywhere casts nothing |

At most `kShadowCasters` (four) lights cast in one frame, counted in the
order the frame placed them; one past that casts nothing and says so once.

**Which bodies cast and receive.** Every body casts and every lit body
receives unless it says otherwise — `mesh(...).castShadow(false)`,
`.receiveShadow(false)` — so shadows appear as soon as a light casts. A
light's `shadowLink(collection)` narrows which of the casting bodies cast
ITS shadow, exactly as `lightLink` narrows which bodies it lights.

| body | casts | receives |
|---|---|---|
| opaque, laid over writing depth | yes | yes |
| dressed with a map or an opacity map | through its coverage: where coverage falls short of one half, nothing | yes |
| translucent, additive, multiplying, screening, fading, or writing no depth | no | yes, when lit |
| transmissive | it tints: what stands behind it from the light receives the share of the light it lets through — its colour, dimmed by its volume across its thickness, where it transmits | yes |
| distorting only | no | yes |
| unlit — its own light | no | no |
| instanced geometry | every instance | every instance |
| turned to the camera | the shape the eye sees it at, turned as the camera turns it | yes |

Both faces of a body block a light, so a card casts from behind as well as
in front.

**The stage.** A geometry pass under a light that casts opens with the
shadow stage, before its target is cleared: every such light's maps are
drawn into one depth atlas, each in a square tile — every tile the size of
the largest map asked for, at most 2048 texels and smaller when the frame
holds more maps than fit 4096 texels a side — from the bodies that cast,
seen from the light, writing depth and nothing else. A body whose surface
does not decide where it covers is drawn with one depth-only program; one
dressed with a map runs its own program's shadow build, which keeps only
the pixels its coverage reaches. A pass under no light that casts draws no
map and binds no atlas.

**What passes through glass.** In a frame where a casting light passes
through a transmissive body, the stage draws two more images laid out as
the atlas is: the depth of the nearest transmissive body from each light,
and the colour every transmissive body nearer the light than what blocks
it lets through, multiplied together, tested against the map's own depth
so glass behind a blocker tints nothing. A point standing behind the
nearest of them takes that colour of the light; one in front of them, or
where none stands, takes all of it. Both are drawn whole in a frame that
holds such a body and in which a map or a transmissive body moved, and
kept otherwise; the lit programs read them only in a frame that holds one
(`kVariantTintedShadows`), and a program drawn in a frame where no light casts,
or for a body that receives no shadow, holds no shadow code at all (`kVariantUnshadowed`).

**What a map costs.** A map is drawn only in a frame where something its
depth depends on moved — its light, its fit, or a body casting into it
that stands where the map can see it — and kept otherwise, so a still
scene pays for its shadows once, a surface that cuts its caster out is
kept while it is the same value, and a body turning under one face of a
point light redraws that face alone. A frame that draws one pays for
every casting body that reaches it once more and
for its tile's texels: doubling `shadow.mapSize` quadruples the texels
written and read, a point, sphere, rect or disk light draws six maps and a
directional one a map per cascade. Reading a map costs every lit pixel nine filtered taps
per casting light whatever the map's size — or, from a source with a
width, sixteen texels searched and, where something blocks, twenty-five
filtered taps. A frame timed with
`Frame::timeStages` reports the maps as `<pass>.shadow` and the tint
beside them as `<pass>.tint`; what the pages the bodies wear cost before
either — the wait for their painting and their smaller levels — is
`<pass>.pages`.

**Filtering.** A shaded point is moved out along its surface's normal,
carried into the light's map, and compared against nine taps `radius`
texels apart, each blended from the four texels around it — so a shadow's
edge is soft over about two radii and moves smoothly as the point does.
Every tap stays inside the light's own tile. A directional map that the
camera's view cut short fades its shadow out over the last twentieth of
the map. A pixel reads every casting light's shadow, and the colour glass
lets through to it, once and before any of its lobes, in a loop of its
own; each light's lobes then take the share it left, so the filter stands
in a surface's program once whatever lobes the surface carries.

**A source with a width.** A light whose source is wide — a directional
or positioned light stating `shadow.size`, and a rect, disk or sphere light
by its own size — casts a penumbra that widens with the distance between a
caster and what it shades: the map is searched around the point for what
blocks it, and the filter spreads, over five taps a side each blended from
four texels, as wide as that source casts across the distance from the
blockers found to the point, divided by their distance from the light for
a perspective map. A shadow is tight where a body rests on the ground and
wide where it stands far above it, never narrower than `radius` makes it,
and a point nothing blocks is not filtered at all. The search and the
filter each reach at most thirty-two texels. The terms are `shadowDistance`, `shadowDepthBias`,
`shadowNormalOffset`, `shadowTest`, `shadowBilinear`, `shadowTapOffset`,
`shadowTapWeight`, `shadowAtlasUv`, `shadowEdgeFade` and `shadowed`.

**Bias.** Every length is counted in texels of the light's map — the
width one texel covers where the point stands — so the same numbers serve
a set of any scale (`LightShadow`, on the light's options as `shadow`):

| option | default | what it does |
|---|---|---|
| `bias` | 1 | how much nearer the light a stored depth must stand to shadow a point; a share that grows with the tangent of the angle to the light, over the radius and one texel more, is added, the tangent capped at eight |
| `normalBias` | 1 | how far a point is moved out along its normal before it is looked up, all of it where the light grazes the surface and none where it meets it square on |
| `radius` | 1 | how far apart the filter's taps stand: larger is softer |
| `mapSize` | 2048 | how many texels a side the map has |
| `distance` | 0 | a directional light's reach in front of the camera; zero fits every body in view |
| `cascades` | 1 | how many maps a directional light's reach is split into along the camera's depth, at most four |
| `size` | 0 | how wide the source is: a directional light's angle across the sky in degrees, a point or spot light's diameter in scene units; zero is a point. A rect, disk or sphere light uses its own size |

## The optional lobes

Each is compiled into a surface's program only when the surface states
it (`material::Lobes`): a surface stating none is shaded by the plain
program, sampling no map and evaluating no term of theirs.

**Sheen.** `sheenColor` (black is none) and `sheenRoughness` describe a
layer of fibres standing sideways off the surface, as velvet, felt and
dust show. Their distribution is brightest with the half vector along the
surface, so a sheen shows at the rim and toward a light behind it, and it
has no Fresnel: its colour is how much it reflects at every angle. Under
every light it adds `directSheen`; from everywhere it adds the radiance
along the mirrored ray, read at the sheen's roughness, times its colour
and its directional albedo at the eye's cosine. The surface beneath gives
up what the sheen took: each light's base is scaled by `sheenScaling` at
the eye's and the light's cosines, and light from everywhere by the same
at the eye's. The directional albedo is a table SigilMaterial integrates
from the same distribution and visibility (`material::sheenAlbedoTable`),
packed in the split sum's table's blue, so a sheen costs no texture of
its own. A white sheen over a white surface in a uniform white surround
returns no more than it was lit by; within a few degrees of grazing the
fitted visibility would let the albedo pass one, and the table holds it
there.

**Anisotropy.** `anisotropy` (0 to 1) widens the GGX lobe along the
brushing toward one, by the square of the strength, and leaves it at the
roughness's own width across it, so a highlight is drawn out along the
way the surface was brushed. The brushing runs along the surface's u,
turned by `anisotropyRotation` degrees counter-clockwise toward the way a
normal map's green points, and further by an `anisotropyDirection` map.
Light from everywhere is mirrored about a normal bent across the
brushing, which smears the panorama along it. At zero strength every
term is the isotropic one.

**The tangent frame** is the surface's own uv, read off the screen
derivatives of the position and the uv at each pixel — the frame a
tangent-space normal map is authored against. A mesh says how it is
brushed through its uv layout: on a generated sphere u runs around the
equator and a brushing at zero turns runs around it, converging at the
poles. Where the mesh carries a tangent lane — a grid fills one along its
own u — the brushing runs along that, interpolated across each triangle,
so a coarse mesh's brushed highlight runs smoothly over its edges; where
it carries none the direction is read off the derivatives per triangle,
and a coarse mesh shows its facets in a strongly brushed highlight. Where
the uv does not vary across the pixel there is no frame to read, and the
brushing runs around the body's own up axis instead.

**Iridescence.** `iridescence` (a weight), `iridescenceIor` and
`iridescenceThickness` in nanometres describe a thin film over the
surface: light reflected off the film's top and off the surface beneath
interferes, and what comes back is tinted by the path between them, which
changes with the thickness and the angle. The film moves the head-on
reflectance toward the one whose Schlick curve reaches the film's
reflectance at the eye's cosine; every light and the light from
everywhere read the moved reflectance. A film of no weight or no
thickness is no film.

**Specular.** `specular` and `specularColor` move a dielectric's
head-on reflectance — four per cent tinted by the colour, scaled by the
strength — and its grazing one, which is the strength; the diffuse side
keeps what the reflection does not. At strength one and white this is
the four per cent every dielectric reflects; at zero a dielectric
reflects nothing at any angle. A metal reflects its base colour and
ignores both.

## Colour

- Colours an author types — base colour, emission, light colours, a
  hemisphere's ground — and colour textures — base colour and emissive
  maps, a page's base colour — are ENCODED, and are decoded into linear
  light before any light touches them.
- Data textures — normal, roughness, metallic, occlusion, clear coat,
  the sheen's roughness, the brushing, the film, the specular strength —
  are read as stored; the sheen's and the specular's colours are colours
  and are decoded.
- The base colour is part of the lit sum: it multiplies before the curve.
- Emission is linear light added to the sum before the exposure, so a
  strength above one is brighter than white and the curve holds it.
- An environment map is linear radiance.
- A surface that is its own light (`material::surface::unlit`) is drawn
  as authored: it is written as the light the final stage turns back into
  its colour, so where it is opaque the exposure and the curve leave it as
  it was authored.
- A data texture (`media::ColorSpace::Data`) is never decoded or
  premultiplied; a surface's normal, roughness, metalness, occlusion and
  clear-coat maps are data by their role.

## The camera's reading

`CameraOptions::exposure` multiplies the light a pass wrote; doubling it
is one stop. `CameraOptions::toneCurve` then compresses it, and the result
is encoded for the display — in the final stage, after every body and
every post pass:

| curve | reads |
|---|---|
| `ToneCurve::Reinhard` | each colour over one plus its luminance; never reaches white, and halves a white surface lit to show its own colour |
| `ToneCurve::Neutral` | almost the identity below a shoulder near 0.8 — a page reads as painted — and a smooth compression above it |
| `ToneCurve::Linear` | no curve: the exposed sum, cut off at one by the target |

The sky drawn behind a scene is read the same way, so a sun in the
backdrop and the same sun reflected in a body are one brightness.

## What it does not do

A transmissive body's shadow is one colour across it, read off its
material's numbers — a transmission, attenuation or thickness map does
not vary it — and a stack's or a recipe's that states no such numbers is
a clear pane's. A sized light's shadow is cast through maps from its centre, its size
widening the penumbra the filter draws rather than the maps holding the
panel's extent; its highlight is a
representative point's, not an integral of the lobe over it; a sized light
has no cone. The sheen reads the GGX chain
rather than one filtered for its own lobe. A sheen, a brushing and a film are
single-scattering terms with no energy put back of their own.
