# SigilWorld — the lighting model

How a lit body is shaded on the device: what each light delivers, how it
falls off, the terms the surface answers with, how colour is carried, and
how the camera reads the result. The shading terms themselves live in
SigilMaterial's `core/shaders/Shading.slang` and are named in
`<sigilmaterial/advanced/Terms.h>`; the scaffold that composes them is
`diligent/shaders/Surface.slang`.

## In one paragraph

Every lit pixel is shaded on its own, in linear light. Each light reaches
the pixel at the falloff its distance and cone give there; the surface
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
lights a set that the set did not state: a set with no ambient light and
no environment has black shadows.

## What a light delivers

`color` is a colour as an author types it, decoded into linear light;
`intensity` scales it without bound.

| light | made by | delivers |
|---|---|---|
| directional | `directionalLight(direction, options)` | `color * intensity` to a surface square to it, everywhere; no falloff |
| point | `pointLight(options)` | `color * intensity / d²` at distance `d` |
| spot | `spotLight(direction, options)` | a point light within a cone |
| ambient | `ambientLight(options)` | radiance `color * intensity` arriving equally from every direction |
| hemisphere | `hemisphereLight(options)` | radiance `color * intensity` from above the horizon its node's up names, `groundColor * intensity` from below |

A white matte surface square to a directional light of intensity one
shows one over pi of it — a matte surface scatters what it receives over
the whole hemisphere. Under an ambient light of one it shows all of it,
as under a uniform environment of that radiance.

**Stating a light by what it delivers.** A positioned light's intensity
is what it delivers at one unit, which at the pixel-sized distances a set
is built at is a large number. `world::intensityAt(strength, distance)`
is the intensity that delivers `strength` at `distance` — the strength
times the distance squared — so a rig reads in the numbers it lights its
subject with. It takes a moving strength too, and the light follows it.
The kit's three-point rig states every lamp this way, by what it
delivers at the subject.

## Falloff

- **Inverse square.** A point or spot light reaches a pixel at
  `1 / d²` of its intensity (`distanceFalloff`).
- **Range.** Past zero, `range` brings that smoothly to zero:
  the inverse square times `(1 − (d / range)⁴)²`, clamped, which has no
  edge and is exactly zero at the range. Zero, the default, reaches
  everywhere.
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
light. The split sum's scale and bias come from a table SigilMaterial
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

**Transmission** reads the panorama along the refracted ray, attenuated
over the thickness, where the set has an environment; the diffuse side
gives up the share that passed through.

**Both sides.** A body whose back face is drawn is lit as the side it
shows.

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
tangent-space normal map is authored against. No mesh lane carries an
authored tangent, so a mesh says how it is brushed through its uv layout:
on a generated sphere u runs around the equator and a brushing at zero
turns runs around it, converging at the poles. Where the uv does not vary
across the pixel there is no frame to read, and the brushing runs around
the body's own up axis instead. The frame is read per triangle, so a
coarse mesh shows its facets in a strongly brushed highlight.

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

The sky drawn behind a set is read the same way, so a sun in the
backdrop and the same sun reflected in a body are one brightness.

## What it does not do

No shadows between bodies, no area lights, and the environment's
prefiltered chain is a set of wrap-aware blurs rather than a GGX
convolution of the panorama — the sheen reads the same chain rather than
one filtered for its own lobe. A sheen, a brushing and a film are
single-scattering terms with no energy put back of their own.
