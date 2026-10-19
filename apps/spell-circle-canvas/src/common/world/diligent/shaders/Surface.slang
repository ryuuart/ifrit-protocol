// The GEOMETRY SCAFFOLD: everything a body's draw needs that its
// material does not supply.
//
// This text is compiled two ways from one source. On its own it is the
// FLAT program — the one a body whose material has no Slang body is
// drawn with, and the one a coverage or a variant re-draw uses, which
// wants a colour and no lighting. Prepended to a material's declarations
// and its body it becomes that material's program, and the fragment
// entry point appended after that text calls the body's `surface`.
//
// THE CONTRACT A SLANG BODY ANSWERS is one function:
//
//     float4 surface(float2 uv)
//
// returning the body's own colour with straight (not premultiplied)
// alpha, ENCODED the way an author types a colour and an image stores
// one. Lighting is not its business; what the surface is made of — how
// rough, how metal, which way it faces, what it gives off — it may state
// in the variables below.
//
// A BODY'S MAP reaches this text as one sampled slot and the placement
// it is read at, so a surface function never has to know whether it was
// dressed: the slot an unmapped body leaves empty reads one white texel,
// and the map multiplies the surface's colour before any light does.
//
// THE SHADOW STAGE draws with this text too: `fsShadow` writes nothing but
// depth, for a body whose surface does not decide where it covers, and
// `fsShadowFlat` and a material's `fsMaterialShadow` keep only the pixels
// whose coverage reaches one half, so a card cut out by its map casts
// the shape it shows.
//
// SIGIL_LIT is the axis the source is specialised on. Defined, the
// lights, the environment and the camera's curve reach the surface;
// undefined, the surface colour stands as it is and the lighting, its
// uniforms and its loop are not in the program at all. Within the lit
// build each optional lobe is an axis of its own, defined by the
// compiler when the body it is compiled with writes that lobe's
// variables below.
//
// THE LIGHTS THAT REACH A BODY are axes of the lit build too, set by the
// renderer from the lights it hands the draw: SIGIL_AREA_LIGHTS when one
// of them has a size, SIGIL_CASCADED_SHADOWS when one reads a directional
// light's cascades, SIGIL_POINT_SHADOWS when one reads a point light's six
// faces, SIGIL_TINTED_SHADOWS when a casting light passes through a
// transmissive body in the frame. A program built without one of them has
// no code for that kind of light at all, and is drawn only under lights that are not of it, so a
// body pays in its program for the lights that reach it rather than for
// every kind a scene could hold.
//
// THE LIT BUILD SHADES EVERY PIXEL, in view space and in LINEAR light:
//
//   1. the surface's colour is decoded;
//   2. every light reaching the pixel adds its radiance through the
//      microfacet model — a Lambert diffuse and a GGX specular lobe,
//      split between them by metalness and by Fresnel — and a positioned
//      light is reached at the falloff its distance and its cone give
//      HERE, not at some point standing in for the body; a light with a
//      size — a rect, a sphere, a disk — gives the diffuse side what the
//      whole of it delivers and lights each highlight from its point
//      nearest the reflected ray, widened by the solid angle it fills;
//      a light that
//      casts a shadow is multiplied, before anything else is made of it,
//      by the share of its map's filtered comparisons the pixel passes,
//      and nothing else in the sum is;
//   3. the light arriving from everywhere — the environment's panorama,
//      ambient and hemisphere lights — adds its diffuse and its mirrored
//      share, with the energy a rough surface scatters more than once put
//      back, darkened by the surface's occlusion and by nothing else;
//   4. the optional lobes a surface states — a sheen over the base, a
//      highlight stretched along a brushing, a thin film tinting the
//      reflectance, a dielectric's own specular strength and colour, the
//      clear coat of step 6 — are each compiled in only when the body this
//      text is compiled with states them (SIGIL_SHEEN, SIGIL_ANISOTROPY,
//      SIGIL_IRIDESCENCE, SIGIL_SPECULAR, SIGIL_CLEARCOAT), so a body that
//      states none compiles to the program it would be if the lobes were
//      not in this text;
//   5. a surface that states TRANSMISSION (SIGIL_TRANSMISSION) gives up
//      its diffuse side to what passes through it and reflects what its
//      index says: the eye's ray is bent in at the surface, carried
//      across the body to a back the stated thickness behind, bent out
//      there, followed on to the scene behind and read from the frame's
//      copy of it — a blurrier level of it for a rougher surface, one
//      reading per colour where the index disperses — tinted by the base
//      colour, dimmed by the volume, and weighted by what the reflection
//      left;
//   6. a clear coat, where the surface has one, reflects a second,
//      colourless lobe over all of that and dims it by what it reflected;
//   7. emission adds its own light;
//   8. the sum is written as LIGHT, premultiplied, into a target that
//      holds light: the camera's exposure and tone curve read it once, in
//      the final stage, after every body and every pass over the picture.
//
// THE UNLIT BUILD is a surface drawn as authored: its colour is what a
// display shows, so it is written as the light the camera's reading turns
// back into exactly that colour — the curve run backward. A surface that
// must shine brighter than white is lit and states its emission.
//
// EVERY BODY IS DRAWN AS INSTANCES of its mesh: instanced geometry with
// one per copy it carries, any other body with one neutral instance at its
// own origin. An instance places the mesh within the body — a position,
// a turn and a scale — or, for a body facing the camera, turns each copy
// toward the camera where it stands; its colour multiplies the mesh's
// tint and its texture window remaps the uv. What a material may read of
// it — its colour, its index and its named scalars — is below.
//
// A SURFACE THAT FADES WHERE IT MEETS THE SCENE reads the depth of what
// is opaque behind it, which the renderer copies once the opaque bodies
// are drawn, and loses its coverage over the distance it named.
//
// A SURFACE THAT SEES THROUGH ITSELF — one that transmits, one that
// distorts (SIGIL_DISTORTION) — reads the frame's lit colour as it stood
// once every body that does not was drawn, which the renderer copies with
// its chain of smaller, blurrier levels beside the depth. The copy is
// taken again before such a surface that stands in front of another drawn
// since, so one sees another through itself.

import Portable;
import Shading;

// ---- what one draw supplies ----------------------------------------
// Every one of these is a global `uniform`, so they land in one uniform
// buffer whose member offsets the compiler reports; a renderer writes
// its bytes at those offsets and never at guessed ones.

/** The camera's view-projection at the target's size. */
uniform float4x4 uViewProj;
/** The body's world placement. */
uniform float4x4 uModel;
/** The colour a body wears when its material has no program to run. */
uniform float4 uBaseColor;
/** WHAT A TRANSMISSIVE BODY LETS THROUGH, for its tint beside the shadow
 *  maps: x the share it transmits, y how deep it is in scene units, z the
 *  distance after which light has become `uTintAttenuation` (zero takes
 *  nothing); each of the first two multiplied by its map, the distance
 *  its volume map's where that is dressed, and the colour multiplied by
 *  its attenuation map's. */
uniform float4 uTintVolume;
uniform float4 uTintAttenuation;
uniform Sampler2D uTintTransmissionMap;
uniform Sampler2D uTintThicknessMap;
uniform Sampler2D uTintVolumeMap;
uniform Sampler2D uTintAttenuationMap;
/** THE MAP the body is dressed with, multiplied into the surface's colour
 *  before the lighting. A body given no map reads one white texel, so a
 *  colour multiplied by a map it does not have is that colour. */
uniform Sampler2D uBaseColorMap;
/** Where that map is read at, over the mesh's own uv coordinates: the
 *  identity reads the image once across them. */
uniform float4x4 uBaseColorMapUv;

/** THE CAMERA'S READING of the picture. x: the exposure every radiance
 *  is multiplied by before the curve; y: which curve — 0 Reinhard on
 *  luminance, 1 the neutral curve, 2 none. A lit body does not apply it —
 *  the final stage does — and an unlit one runs it backward. */
uniform float4 uTone;
/** The projection's inverse, which turns a point on the near plane into
 *  the direction the eye is looking through it, and a depth into how far
 *  in front of the eye it stands. */
uniform float4x4 uInvProj;
/** THE DEPTH OF WHAT IS OPAQUE BEHIND, for a surface that fades into it. */
uniform Sampler2D uSceneDepth;
/** x, y: one over the target's width and height; z: one when the depth
 *  above is the scene's, zero when nothing was copied. */
uniform float4 uDepthFade;

#if defined(SIGIL_TRANSMISSION) || defined(SIGIL_DISTORTION)
/** THE SCENE BEHIND, for a surface that sees through itself: the lit
 *  colour of every body that does not, as LIGHT, with a chain of smaller
 *  levels each the average of the one above — a blurrier picture per
 *  level — read with `SampleLevel`. */
uniform Sampler2D uSceneColor;
/** x, y: one over the target's width and height; z: how many levels the
 *  chain has; w: one when the picture above is the scene's, zero when
 *  nothing was copied. */
uniform float4 uSceneColorDials;
/** The camera's projection, which carries a point in front of the eye
 *  onto the picture. */
uniform float4x4 uProjection;
#endif

/** The camera's view, which carries a point of the world into the space
 *  the shading is written in. */
uniform float4x4 uView;
/** WHETHER EACH INSTANCE FACES THE CAMERA. x: 0 as placed, 1 square on to
 *  the picture, 2 turned about the world's up axis only. */
uniform float4 uFacing;
/** The camera's own axes in the world — the picture's right, its up, and
 *  backward toward the eye — and where the eye stands. */
uniform float4 uCameraRight;
uniform float4 uCameraUp;
uniform float4 uCameraBack;
uniform float4 uCameraEye;
/** THE KEYS OF THE FOUR SCALARS each instance carries, in the order they
 *  were packed: the number each name folds to, zero where none. */
uniform float4 uInstanceAttributeKeys;

#ifdef SIGIL_LIT
/** view * model, for the position the shading is written in. */
uniform float4x4 uModelView;
/** The inverse transpose of view * model, its 3x3 in the upper left. */
uniform float4x4 uNormalMatrix;
/** THE LIGHTS, one record of four rows each, in the space the shading is
 *  written in and with every colour already decoded and scaled by its
 *  intensity.
 *
 *  position xyz: where a positioned light stands — a sized light's
 *  centre; w: the kind — 0 directional, 1 point, 2 spot, 3 ambient,
 *  4 hemisphere, 5 rect, 6 sphere, 7 disk.
 *  direction xyz: the way a directional, spot, rect or disk light
 *  travels, or a hemisphere's up; w: the range a positioned light is
 *  windowed to, zero for none.
 *  color rgb: the radiance it shines with — a hemisphere's sky; w: a
 *  spot's outer cone, as a cosine, or a rect's half width, or a sphere's
 *  or a disk's radius.
 *  ground rgb: a hemisphere's ground, or a rect's unit width axis; w: a
 *  spot's inner cone, as a cosine, or a rect's half height. `uLightCount.x` says how many of the sixteen are lit, and
 *  `uLightCount.y` how many texels a side the split sum's table has. */
uniform float4 uLightPosition[16];
uniform float4 uLightDirection[16];
uniform float4 uLightColor[16];
uniform float4 uLightGround[16];
uniform float4 uLightCount;

/** THE SHADOW MAPS, every light's in one depth atlas, read texel by
 *  texel: a texel holds the depth, zero at a map's near plane and one at
 *  its far one, of the nearest body blocking that light. */
uniform Sampler2D uShadowAtlas;
/** …the same atlas read through a sampler that compares: a gather of four
 *  texels answers, for each, one where the reference handed with it is no
 *  deeper than what the texel stores. */
uniform Sampler2DShadow uShadowCompare;
#ifdef SIGIL_TINTED_SHADOWS
/** WHAT A TRANSMISSIVE BODY LETS THROUGH, laid out as the atlas is: the
 *  colour of a light that comes through, white where nothing transmissive
 *  stands in front of what blocks it, and the depth of the nearest
 *  transmissive body, which says which points stand behind it. */
uniform Sampler2D uShadowTint;
uniform Sampler2D uShadowGlass;
#endif
/** x: one over the atlas's side in texels; y: one when this body is
 *  darkened by shadows, zero when it is not; z: how many maps there are. */
uniform float4 uShadowDials;
/** WHICH MAP EACH LIGHT READS, four lights to a row in the order of the
 *  light records: the map's number, or below zero for none. */
uniform float4 uLightShadow[4];
/** PER MAP — up to twenty-four of them, a point light taking six — the
 *  four rows of the matrix carrying a point in the space the
 *  shading is written in to the light's clip space — map n's rows are
 *  4n to 4n + 3; */
uniform float4 uShadowMatrix[96];
/** …the corner (xy) and size (zw) of its tile in the atlas, in the atlas's
 *  own uv; */
uniform float4 uShadowTile[24];
/** …x, y: its near and far planes, in scene units; z: zero for an
 *  orthographic map, one for a perspective one, two for the first of a
 *  point light's six faces, which follow it in the order +x, -x, +y, -y,
 *  +z, -z; w: the width one texel covers — at a distance of one unit from
 *  the light for a perspective map; */
uniform float4 uShadowDepth[24];
/** …and x: its depth bias, y: its normal bias, z: its filter radius, all
 *  in texels; w: the share of the map over which a shadow fades out at
 *  its edge. */
uniform float4 uShadowBias[24];
/** …and how wide the source of its light is, which is what widens a
 *  shadow's edge with the distance between a caster and what it shades.
 *  x: on an orthographic map, how far the edge spreads per unit of that
 *  distance; on a perspective one, the source's width in scene units, the
 *  spread then shrinking with the caster's distance from the light. Zero
 *  is a point, and every edge is as soft as the filter radius alone. */
uniform float4 uShadowSource[24];
/** …and on the first of a directional light's cascades, which follow it
 *  one after another, how far in front of the camera each reaches, zero
 *  past the last; zero throughout on every other map. */
uniform float4 uShadowCascade[24];

/** THE SET'S ENVIRONMENT MAP, and the second one it is crossfading to.
 *  Both are equirect panoramas of linear radiance whose mip levels are the
 *  prefiltered chain a roughness picks among — not a filtering aid, a
 *  different image per level — so they are read with `SampleLevel` and
 *  bound with a sampler of their own, wrapping in azimuth and clamping at
 *  the poles.
 *
 *  Beside them the cosine convolution each was reduced to, which is what
 *  a surface facing a direction receives from everywhere. A frame with
 *  no environment binds one white texel to all four and says so in
 *  `uEnvDials.w`, so nothing below reads them. */
uniform Sampler2D uEnvironment;
uniform Sampler2D uEnvironmentNext;
uniform Sampler2D uIrradiance;
uniform Sampler2D uIrradianceNext;
/** THE SPLIT SUM'S TABLE: for every roughness and viewing angle, the scale
 *  and bias a surface's reflectance takes to say how much of what arrives
 *  from everywhere it mirrors — read at `environmentBrdfUv` over its
 *  `uLightCount.y` texels a side. Bound whether or not there is a
 *  panorama, because ambient and hemisphere lights are mirrored too. */
uniform Sampler2D uEnvironmentBrdf;
/** Carries a direction in the space the shading is written in into the
 *  panorama's own frame: the placement of the node that put the sky
 *  there, times the view's inverse. */
uniform float4x4 uEnvMatrix;
/** rgb: the strength and tint every sample is multiplied by;
 *  a: how far along the crossfade to the second map. */
uniform float4 uEnvTint;
/** THE BACKDROP'S OWN DIALS, for the pass that DRAWS the sky rather than
 *  shades a body with it. x: its strength, which is also its switch —
 *  at zero nothing is drawn, so there is no flag that can disagree with
 *  a number. y: how blurred it is shown, in the same roughness units a
 *  reflection reads, so racking a sky out of focus costs nothing. */
uniform float4 uBackdrop;
/** GROUND PROJECTION, for the same pass. xyz: where the sphere the sky
 *  is projected onto is centred, in the space the shading is written in
 *  — where the eye is the origin; w: its radius, and zero is a sky at
 *  infinity. */
uniform float4 uGround;
/** x: the diffuse side's own scale; y: the specular side's;
 *  z: added to every surface's roughness; w: how many levels the chain
 *  has — and zero, which is a frame with no environment at all. */
uniform float4 uEnvDials;
#endif

// Numbered explicitly, because the instance elements skip the fifth
// input, which a primitive lane takes when a layout declares one.
struct VSIn {
  [[vk::location(0)]] float3 position : ATTRIB0;
  [[vk::location(1)]] float3 normal : ATTRIB1;
  [[vk::location(2)]] float2 uv : ATTRIB2;
  [[vk::location(3)]] float4 color : ATTRIB3;
  /** ONE INSTANCE: xyz where it stands within the body. */
  [[vk::location(5)]] float4 instancePosition : ATTRIB5;
  /** …its turn, a quaternion x y z w… */
  [[vk::location(6)]] float4 instanceOrientation : ATTRIB6;
  /** …xyz its scale along the mesh's own axes… */
  [[vk::location(7)]] float4 instanceScale : ATTRIB7;
  /** …the colour it multiplies the mesh's by… */
  [[vk::location(8)]] float4 instanceColor : ATTRIB8;
  /** …the window {uOffset, vOffset, uScale, vScale} its uv is read
   *  through… */
  [[vk::location(9)]] float4 instanceTextureWindow : ATTRIB9;
  /** …and its four named scalars. */
  [[vk::location(10)]] float4 instanceAttributes : ATTRIB10;
  /** THE WAY u RUNS along the surface where the mesh states it, and in w
   *  the side v runs on; no length where it states none. */
  [[vk::location(11)]] float4 tangent : ATTRIB11;
  uint instanceIndex : SV_InstanceID;
};

struct VSOut {
  float4 position : SV_Position;
  /** The mesh's own per-vertex tint, times its instance's colour. */
  float4 tint : COLOR0;
  float2 uv : TEXCOORD0;
  /** The instance this pixel's copy is: its colour, its scalars and its
   *  index, the same over the whole copy. */
  nointerpolation float4 perInstanceColor : COLOR1;
  nointerpolation float4 perInstanceAttributes : TEXCOORD2;
  nointerpolation float perInstanceIndex : TEXCOORD3;
#ifdef SIGIL_LIT
  /** Where the surface stands and which way it faces, in the space the
   *  shading is written in. */
  float3 viewNormal : NORMAL0;
  float3 viewPos : TEXCOORD1;
  /** …and the way u runs along it where the mesh states that, with w the
   *  side v runs on; no length where the mesh states none. */
  float4 viewTangent : TANGENT0;
#endif
#if defined(SIGIL_LIT) && defined(SIGIL_TRANSMISSION)
  /** The plane at the copy's own zero depth, in the space the shading is
   *  written in: its unit normal and its distance along it from the eye —
   *  the back a flat-backed body's light leaves through. */
  nointerpolation float4 viewBackPlane : TEXCOORD4;
#endif
};

// ---- what a body may say about its surface --------------------------
// Declared in both builds, so one body compiles for either; read only
// where there is lighting.

/** The surface normal in TANGENT space, as a normal map encodes it.
 *  (0, 0, 1) is the surface as its geometry describes it. */
static float3 gSurfaceNormal = float3(0.0, 0.0, 1.0);
/** How much of a metal this pixel is: a metal has no diffuse side and
 *  reflects in its own colour. */
static float gSurfaceMetal = 0.0;
/** How rough, in [0, 1]; below zero is a body that did not say, and the
 *  scaffold shades it at one half. */
static float gSurfaceRoughness = -1.0;
/** How much of the light arriving from everywhere reaches this pixel past
 *  its own creases. Lights from one direction ignore it. */
static float gSurfaceOcclusion = 1.0;
/** A CLEAR COAT: its weight, its roughness, and its normal in tangent
 *  space — (0, 0, 1) lies smooth over the geometry. */
static float gSurfaceClearcoat = 0.0;
static float gSurfaceClearcoatRoughness = 0.0;
static float3 gSurfaceClearcoatNormal = float3(0.0, 0.0, 1.0);
/** TRANSMISSION: how much of what is behind the surface comes through
 *  it, at what index of refraction, across how many scene units of a
 *  medium in which `gSurfaceAttenuationDistance` units leave
 *  `gSurfaceAttenuationColor` (linear; a distance of zero takes nothing),
 *  and how far the index spreads across the colours. */
static float gSurfaceTransmission = 0.0;
static float gSurfaceIor = 1.5;
static float gSurfaceThickness = 0.0;
/** Above one half, the volume's back is the plane at the body's own zero
 *  depth rather than a face the thickness behind the front. */
static float gSurfaceFlatBack = 0.0;
static float3 gSurfaceAttenuationColor = float3(1.0, 1.0, 1.0);
static float gSurfaceAttenuationDistance = 0.0;
static float gSurfaceDispersion = 0.0;
/** DISTORTION: how far, in scene units at the surface, what stands behind
 *  is displaced — along the way u runs and the way a normal map's green
 *  points. */
static float2 gSurfaceDistortion = float2(0.0, 0.0);
/** Where the distortion moves what is seen behind the surface, in the
 *  space the shading is written in: its two numbers laid along the
 *  surface where it is lit, and across and up the picture where it is
 *  not. */
static float3 gDistortionMoved = float3(0.0, 0.0, 0.0);
/** What a body states for an ADDITIVE reflection: the share of what it
 *  mirrors it adds, with no Fresnel. Below zero is the split sum, where
 *  the environment reaches the surface through its own reflectance. */
static float gSurfaceReflection = -1.0;
/** The light this pixel gives off of its own, in LINEAR radiance: added to
 *  the lit sum before the exposure and the curve, so a strength above one
 *  is brighter than white and the curve, not the target, holds it. */
static float3 gSurfaceEmission = float3(0.0, 0.0, 0.0);
/** A SHEEN: its colour, in linear light — black is none — and how wide
 *  it spreads. */
static float3 gSurfaceSheenColor = float3(0.0, 0.0, 0.0);
static float gSurfaceSheenRoughness = 0.0;
/** A BRUSHING: how strongly the highlight stretches, along which
 *  direction in the surface's own frame — x the way u runs, y the way a
 *  normal map's green points — and turned how many degrees from it. */
static float gSurfaceAnisotropy = 0.0;
static float2 gSurfaceAnisotropyDirection = float2(1.0, 0.0);
static float gSurfaceAnisotropyRotation = 0.0;
/** A THIN FILM: its weight, its index of refraction and its thickness in
 *  nanometres. */
static float gSurfaceIridescence = 0.0;
static float gSurfaceIridescenceIor = 1.3;
static float gSurfaceIridescenceThickness = 0.0;
/** A DIELECTRIC'S SPECULAR: its strength, and its colour in linear light. */
static float gSurfaceSpecular = 1.0;
static float3 gSurfaceSpecularColor = float3(1.0, 1.0, 1.0);
/** The distance, in scene units, over which the surface fades out as it
 *  nears the opaque surface behind it; zero does not fade. */
static float gSurfaceDepthFade = 0.0;

/** @p v turned by the unit quaternion @p q. The identity turn leaves
 *  every vector exactly as it was. */
float3 rotateByQuaternion(float4 q, float3 v) {
  let t = 2.0 * cross(q.xyz, v);
  return v + q.w * t + cross(q.xyz, t);
}

/** One over each of @p s, a zero scale read as one so a flattened copy's
 *  normal stays a direction. */
float3 inverseScale(float3 s) {
  return float3(s.x != 0.0 ? 1.0 / s.x : 1.0, s.y != 0.0 ? 1.0 / s.y : 1.0,
                s.z != 0.0 ? 1.0 / s.z : 1.0);
}

#if defined(SIGIL_LIT) && defined(SIGIL_TRANSMISSION)
/** The plane through @p point facing @p normal: its unit normal and its
 *  distance along it from the origin. */
float4 planeThrough(float3 point, float3 normal) {
  let length2 = dot(normal, normal);
  let unit = length2 > 0.0 ? normal / sqrt(length2) : float3(0.0, 0.0, 1.0);
  return float4(unit, dot(unit, point));
}
#endif

[shader("vertex")]
VSOut vsMain(VSIn input) {
  VSOut out;
  let q = input.instanceOrientation;
  let s = input.instanceScale.xyz;
  if (uFacing.x > 0.5) {
    // EACH COPY TURNED TO THE CAMERA where it stands, keeping its own
    // scale and the body's: its +z toward the eye.
    let centre = mul(uModel, float4(input.instancePosition.xyz, 1.0)).xyz;
    let bodyScale = float3(length(mul(uModel, float4(1.0, 0.0, 0.0, 0.0)).xyz),
                           length(mul(uModel, float4(0.0, 1.0, 0.0, 0.0)).xyz),
                           length(mul(uModel, float4(0.0, 0.0, 1.0, 0.0)).xyz));
    var right = uCameraRight.xyz;
    var up = uCameraUp.xyz;
    var back = uCameraBack.xyz;
    if (uFacing.x > 1.5) {
      up = float3(0.0, 1.0, 0.0);
      var toward = uCameraEye.xyz - centre;
      toward.y = 0.0;
      // Straight above or below, the camera's own backward direction on
      // the ground decides which way the copy stands turned.
      if (dot(toward, toward) < 1e-8) toward = float3(uCameraBack.x, 0.0, uCameraBack.z);
      back = dot(toward, toward) > 1e-8 ? normalize(toward)
                                        : float3(0.0, 0.0, 1.0);
      right = cross(up, back);
    }
    let p = input.position * s * bodyScale;
    let world = float4(centre + right * p.x + up * p.y + back * p.z, 1.0);
    let n = input.normal * inverseScale(s * bodyScale);
    let worldNormal = right * n.x + up * n.y + back * n.z;
    out.position = mul(uViewProj, world);
#ifdef SIGIL_LIT
    out.viewPos = mul(uView, world).xyz;
    out.viewNormal = mul(uView, float4(worldNormal, 0.0)).xyz;
    let along = input.tangent.xyz * s * bodyScale;
    out.viewTangent =
        float4(mul(uView, float4(right * along.x + up * along.y +
                                     back * along.z,
                                 0.0))
                   .xyz,
               input.tangent.w);
#endif
#if defined(SIGIL_LIT) && defined(SIGIL_TRANSMISSION)
    out.viewBackPlane =
        planeThrough(mul(uView, float4(centre, 1.0)).xyz,
                     mul(uView, float4(back, 0.0)).xyz);
#endif
  } else {
    let local = input.instancePosition.xyz +
                rotateByQuaternion(q, input.position * s);
    let localNormal = rotateByQuaternion(q, input.normal * inverseScale(s));
    out.position = mul(uViewProj, mul(uModel, float4(local, 1.0)));
#ifdef SIGIL_LIT
    out.viewPos = mul(uModelView, float4(local, 1.0)).xyz;
    out.viewNormal = mul(uNormalMatrix, float4(localNormal, 0.0)).xyz;
    // A tangent lies in the surface, so it moves as a position does.
    let localTangent = rotateByQuaternion(q, input.tangent.xyz * s);
    out.viewTangent =
        float4(mul(uModelView, float4(localTangent, 0.0)).xyz,
               input.tangent.w);
#endif
#if defined(SIGIL_LIT) && defined(SIGIL_TRANSMISSION)
    // The copy's own z of zero: through where it stands, facing its own
    // z as a normal does.
    out.viewBackPlane = planeThrough(
        mul(uModelView, float4(input.instancePosition.xyz, 1.0)).xyz,
        mul(uNormalMatrix,
            float4(rotateByQuaternion(q, float3(0.0, 0.0, 1.0) *
                                             inverseScale(s)),
                   0.0))
            .xyz);
#endif
  }
  let window = input.instanceTextureWindow;
  out.uv = window.xy + input.uv * window.zw;
  out.tint = input.color * input.instanceColor;
  out.perInstanceColor = input.instanceColor;
  out.perInstanceAttributes = input.instanceAttributes;
  out.perInstanceIndex = (float)input.instanceIndex;
  return out;
}

// ---- what a material may read of the instance it is drawn on ---------

static float4 gInstanceColor = float4(1.0, 1.0, 1.0, 1.0);
/** The plane at this pixel's copy's own zero depth, as `VSOut` carries
 *  it. */
static float4 gBackPlane = float4(0.0, 0.0, 1.0, 0.0);
static float4 gInstanceAttributes = float4(0.0, 0.0, 0.0, 0.0);
static float gInstanceIndex = 0.0;

/** The colour of the instance this pixel's copy is: white for a body that
 *  is not instanced geometry. */
float4 instanceColor() { return gInstanceColor; }
/** Which instance it is, counted from zero. */
float instanceIndex() { return gInstanceIndex; }
/** Whether the instances carry the scalar whose name folds to @p key. */
bool instanceCarries(float key) {
  return key != 0.0 &&
         (uInstanceAttributeKeys.x == key || uInstanceAttributeKeys.y == key ||
          uInstanceAttributeKeys.z == key || uInstanceAttributeKeys.w == key);
}
/** The scalar whose name folds to @p key, on this pixel's instance; zero
 *  where the instances carry none. */
float instanceAttribute(float key) {
  if (key == 0.0) return 0.0;
  if (uInstanceAttributeKeys.x == key) return gInstanceAttributes.x;
  if (uInstanceAttributeKeys.y == key) return gInstanceAttributes.y;
  if (uInstanceAttributeKeys.z == key) return gInstanceAttributes.z;
  if (uInstanceAttributeKeys.w == key) return gInstanceAttributes.w;
  return 0.0;
}

/** How far in front of the eye a depth at @p uv on the target stands. */
float eyeDistance(float2 uv, float depth) {
  let at = mul(uInvProj,
               float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0));
  return at.w != 0.0 ? -at.z / at.w : 0.0;
}

#if defined(SIGIL_TRANSMISSION) || defined(SIGIL_DISTORTION)
/** Where on the target, in its own uv, a point in front of the eye is
 *  seen. */
float2 screenUvOf(float3 p) {
  let clip = mul(uProjection, float4(p, 1.0));
  let ndc = clip.w != 0.0 ? clip.xy / clip.w : float2(0.0, 0.0);
  return float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

/** …and the point in front of the eye a depth at @p uv stands at. */
float3 viewPointAt(float2 uv, float depth) {
  let at = mul(uInvProj,
               float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0));
  return at.w != 0.0 ? at.xyz / at.w : float3(0.0, 0.0, 0.0);
}
#endif

/** Makes this pixel's instance what the readers above answer. Every
 *  fragment entry calls it before the surface is asked. */
void enterInstance(VSOut input) {
  gInstanceColor = input.perInstanceColor;
  gInstanceAttributes = input.perInstanceAttributes;
  gInstanceIndex = input.perInstanceIndex;
#if defined(SIGIL_LIT) && defined(SIGIL_TRANSMISSION)
  gBackPlane = input.viewBackPlane;
#endif
}

/** The scaffold's own surface, for a draw with no material program: the
 *  colour the frame extracted, and nothing else. */
float4 surfaceFlat(float2 uv) { return uBaseColor; }

#ifdef SIGIL_LIT
/** THE TANGENT FRAME A MESH DOES NOT CARRY, read off how the surface
 *  moves across the screen: the screen derivatives of the view position
 *  and of the uv coordinates together say which way u and v run. It is
 *  the frame a tangent-space normal map is authored against, and asking
 *  the geometry for it instead would mean a tangent lane every mesh
 *  would have to carry and every generator would have to fill.
 *
 *  @p n must already be unit length. The two axes come back unnormalised
 *  but at one scale, which is all a perturbation needs: `t` along which
 *  u increases, `b` up the picture, the way a map's green points. */
void tangentAxes(float3 n, float3 p, float2 uv, out float3 t, out float3 b) {
  let dpx = ddx(p);
  let dpy = ddy(p);
  let duvx = ddx(uv);
  let duvy = ddy(uv);
  let px = cross(dpy, n);
  let py = cross(n, dpx);
  t = px * duvx.x + py * duvy.x;
  b = px * duvx.y + py * duvy.y;
  // Those are the gradients of u and v scaled by the screen's own
  // orientation, which flips with the direction the framebuffer's rows run;
  // its sign puts them back to u increasing and v increasing, and a map's
  // green points UP the picture, against v.
  let orientation = dotP(dpx, cross(dpy, n)) < 0.0 ? -1.0 : 1.0;
  t = t * orientation;
  b = -b * orientation;
  let scale = max(dotP(t, t), dotP(b, b));
  let inv = scale > 0.0 ? 1.0 / sqrtP(scale) : 0.0;
  t = t * inv;
  b = b * inv;
}

/** THE PANORAMA, at a direction in the space the shading is written in
 *  and at a roughness. While a crossfade is under way the two maps are
 *  sampled and mixed rather than one being rebuilt, which is what lets a
 *  sky change while the frame is running; with none, the second is not
 *  read. */
float3 environmentAt(float3 direction, float roughness) {
  let d = normalizeP(mul(uEnvMatrix, float4(direction, 0.0)).xyz);
  let uv = equirectangularUv(d);
  let level = roughnessLevel(roughness + uEnvDials.z, uEnvDials.w);
  var radiance = uEnvironment.SampleLevel(uv, level).rgb;
  if (uEnvTint.a > 0.0)
    radiance = mixP(radiance, uEnvironmentNext.SampleLevel(uv, level).rgb,
                    uEnvTint.a);
  return radiance * uEnvTint.rgb * uEnvDials.y;
}

/** …and what falls on a surface facing that way from everywhere. */
float3 irradianceAt(float3 normal) {
  let d = normalizeP(mul(uEnvMatrix, float4(normal, 0.0)).xyz);
  let uv = equirectangularUv(d);
  var irradiance = uIrradiance.SampleLevel(uv, 0.0).rgb;
  if (uEnvTint.a > 0.0)
    irradiance = mixP(irradiance, uIrradianceNext.SampleLevel(uv, 0.0).rgb,
                      uEnvTint.a);
  return irradiance * uEnvTint.rgb * uEnvDials.x;
}

#ifdef SIGIL_TRANSMISSION
/** HOW THE SURFACE BENDS where the pixel stands: how far its facing turns
 *  per scene unit across the picture — one over the radius of a ball,
 *  zero on a flat face, below zero where it hollows away from the eye.
 *  Read off the screen derivatives, so every pixel finds it alike. */
float curvatureOf(float3 geometric, float3 p) {
  let dpx = ddx(p);
  let dpy = ddy(p);
  let across = dotP(dpx, dpx) + dotP(dpy, dpy);
  if (across <= 0.0) return 0.0;
  return (dotP(ddx(geometric), dpx) + dotP(ddy(geometric), dpy)) / across;
}

/** WHERE THE EYE'S RAY LEAVES THE BODY and which way it goes on. The body
 *  is read as a lens: its front is the surface at @p p facing @p n, bent
 *  by @p curvature; its back stands @p thickness behind the front along
 *  the normal, curved the same way, so a flat face is a slab whose back is
 *  parallel to it and a ball as thick as it is wide is a ball — or, for a
 *  body whose back is flat, its back is the plane at its own zero depth,
 *  `gBackPlane`. The ray, bent into the medium at the front, crosses to
 *  the back, and is bent again leaving it. @p travelled says how far it
 *  went inside. */
void passage(float3 p, float3 v, float3 n, float curvature, float ior,
             out float3 leftAt, out float3 leaving, out float travelled) {
  leftAt = p;
  leaving = -v;
  travelled = 0.0;
  let bent = refraction(-v, n, 1.0 / ior);
  if (dotP(bent, bent) <= 0.0) return;
  if (gSurfaceFlatBack > 0.5) {
    // THE FLAT BACK, met by the bent ray where it reaches the plane; a
    // point on or behind the plane has no volume in front of it, and the
    // ray leaves where it entered. It leaves through the plane's own
    // normal, turned the way the ray crosses it.
    let facing = gBackPlane.xyz;
    let toward = dotP(facing, bent);
    if (abs(toward) > 1e-5)
      travelled = max((gBackPlane.w - dotP(facing, p)) / toward, 0.0);
    leftAt = p + bent * travelled;
    let outward = toward >= 0.0 ? facing : -facing;
    let out = refraction(bent, -outward, ior);
    leaving = dotP(out, out) > 0.0 ? normalizeP(out) : bent;
    return;
  }
  // A ball cannot be thicker than it is wide.
  let k = curvature;
  var thick = max(gSurfaceThickness, 0.0);
  if (k > 0.0) thick = min(thick, 2.0 / k);
  // THE BACK, a sphere of the front's radius whose centre stands the
  // thickness behind the front's own, met by the bent ray: the root
  // written so that it stays exact as the curvature goes to zero.
  let q = min(dotP(n, bent), -1e-4);
  let a = 1.0 - k * thick;
  let d = max(a * a * q * q + k * thick * (2.0 - k * thick), 0.0);
  let across = a >= 0.0 ? thick * (2.0 - k * thick) / (sqrtP(d) - a * q)
                        : (a * q + sqrtP(d)) / k;
  travelled = max(across, 0.0);
  leftAt = p + bent * travelled;
  if (thick <= 0.0) return;
  let outward = normalizeP(bent * (travelled * k) - n * a);
  let out = refraction(bent, -outward, ior);
  leaving = dotP(out, out) > 0.0 ? normalizeP(out) : bent;
}

/** WHAT IS SEEN THROUGH THE SURFACE at @p p, facing @p n with
 *  @p curvature and seen from @p v, along a medium of index @p ior: the
 *  eye's ray carried through the body as `passage` carries it, displaced
 *  along the surface by @p moved, then followed on to whatever the scene
 *  behind holds where it goes, and read from it at @p level. Where that
 *  lands off the picture, or on something nearer the eye than the surface
 *  itself — which cannot be behind it — the panorama along the ray
 *  leaving stands in, or, with no panorama, the picture at the nearest
 *  place it has. @p travelled says how far the ray went inside. */
float3 seenThrough(float3 p, float3 v, float3 n, float curvature, float ior,
                   float3 moved, float rough, float level, float2 here,
                   out float travelled) {
  float3 leftAt;
  float3 leaving;
  passage(p, v, n, curvature, ior, leftAt, leaving, travelled);
  leftAt = leftAt + moved;
  if (uSceneColorDials.w > 0.5) {
    // ON TO THE SCENE BEHIND: the reach along the leaving ray at which it
    // stands as deep as what the depth holds where it lands, found by
    // stepping to that depth a few times over.
    float reach = 0.0;
    if (uDepthFade.z > 0.5 && leaving.z < -1e-4) {
      [ForceUnroll]
      for (int step = 0; step < 3; ++step) {
        let uv = clamp(screenUvOf(leftAt + leaving * reach), float2(0.0, 0.0),
                       float2(1.0, 1.0));
        let behind = viewPointAt(uv, uSceneDepth.SampleLevel(uv, 0.0).r);
        reach = max((behind.z - leftAt.z) / leaving.z, 0.0);
      }
    }
    let uv = screenUvOf(leftAt + leaving * reach);
    var landed = uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
    if (landed && uDepthFade.z > 0.5) {
      let behind = uSceneDepth.SampleLevel(uv, 0.0).r;
      landed = eyeDistance(uv, behind) >= -p.z;
    }
    if (landed) return uSceneColor.SampleLevel(uv, level).rgb;
  }
  if (uEnvDials.w > 0.0) return environmentAt(leaving, rough);
  if (uSceneColorDials.w > 0.5)
    return uSceneColor.SampleLevel(here, level).rgb;
  return float3(0.0, 0.0, 0.0);
}
#endif

/** ONE STORED DEPTH, compared: whether the point @p receiver scene units
 *  from the light, brought @p bias nearer it, is lit past what map @p map
 *  stored at the atlas texel @p texel. */
float shadowTexel(int map, float2 texel, float receiver, float bias) {
  let depth = uShadowDepth[map];
  let stored =
      uShadowAtlas.SampleLevel((texel + float2(0.5, 0.5)) * uShadowDials.x, 0.0)
          .r;
  return shadowTest(receiver, shadowDistance(stored, depth.x, depth.y, depth.z),
                    bias);
}

/** HOW FAR FROM THE LIGHT the stored depth at the atlas texel @p texel of
 *  map @p map stands, in scene units. */
float shadowStored(int map, float2 texel) {
  let depth = uShadowDepth[map];
  let stored =
      uShadowAtlas.SampleLevel((texel + float2(0.5, 0.5)) * uShadowDials.x, 0.0)
          .r;
  return shadowDistance(stored, depth.x, depth.y, depth.z);
}

/** FOUR NEIGHBOURING COMPARISONS, from the texel @p corner and the three
 *  after it, blended by @p f: one gather through the comparison sampler
 *  against @p reference, the depth a map would store for the point brought
 *  @p bias nearer the light — or, where the block reaches past the light's
 *  own tile, each texel kept inside it and compared on its own. */
float shadowFour(int map, float2 corner, float2 f, float receiver, float bias,
                 float reference, float2 low, float2 high) {
  if (all(corner >= low) && all(corner + float2(1.0, 1.0) <= high)) {
    // The gather's four, read at the corner the texels share, come back
    // as (0, 1), (1, 1), (1, 0) and (0, 0) of the block.
    let passed = uShadowCompare.GatherCmp(
        (corner + float2(1.0, 1.0)) * uShadowDials.x, reference);
    return shadowBilinear(passed.w, passed.z, passed.x, passed.y, f);
  }
  let t00 = clamp(corner, low, high);
  let t11 = clamp(corner + float2(1.0, 1.0), low, high);
  return shadowBilinear(
      shadowTexel(map, t00, receiver, bias),
      shadowTexel(map, float2(t11.x, t00.y), receiver, bias),
      shadowTexel(map, float2(t00.x, t11.y), receiver, bias),
      shadowTexel(map, t11, receiver, bias), f);
}

/** A SHADOW FROM A SOURCE WITH A WIDTH: what blocks the point is looked
 *  for around it first, and the filter then spreads as wide as the
 *  penumbra the source casts across the distance between the blockers
 *  found and the point — tight where a body rests on what it shades,
 *  wide where it stands far above it. A point no blocker covers is lit
 *  without filtering at all. @p centre is the point's texel in the atlas,
 *  @p receiver its distance from the light, @p texel the width one texel
 *  covers there and @p compareBias the bias its comparisons take. */
float shadowSoft(int map, float2 centre, float receiver, float texel,
                 float nDotL, float2 low, float2 high) {
  let depth = uShadowDepth[map];
  let bias = uShadowBias[map];
  let source = uShadowSource[map].x;
  let compareBias = shadowDepthBias(bias.x, texel, nDotL, bias.z);
  // THE SEARCH reaches as far as the widest penumbra a blocker at the
  // light's own near plane could cast here, in texels, within a bound.
  let widest = depth.z > 0.5 ? source * (receiver - depth.x) /
                                   max(depth.x, 1e-3)
                             : source * (receiver - depth.x);
  let reach = clamp(widest / max(texel, 1e-6) * 0.5, 1.0, 32.0);
  float blockers = 0.0;
  float found = 0.0;
  [loop]
  for (int y = 0; y < 4; ++y) {
    [loop]
    for (int x = 0; x < 4; ++x) {
      let at = centre + (float2(float(x), float(y)) / 3.0 * 2.0 -
                         float2(1.0, 1.0)) *
                            reach;
      let stored = shadowStored(map, clamp(floor(at + 0.5), low, high));
      if (stored < receiver - compareBias) {
        blockers = blockers + stored;
        found = found + 1.0;
      }
    }
  }
  if (found <= 0.0) return 1.0;
  let blocker = blockers / found;
  let spread = depth.z > 0.5
                   ? source * (receiver - blocker) / max(blocker, 1e-3)
                   : source * (receiver - blocker);
  let radius = clamp(max(spread / max(texel, 1e-6) * 0.5, bias.z), 0.5, 32.0);
  let filterBias = shadowDepthBias(bias.x, texel, nDotL, radius);
  let reference =
      shadowDepthOf(receiver - filterBias, depth.x, depth.y, depth.z);
  // THE FILTER: five taps a side across the penumbra, each blended from
  // the four texels around it.
  float lit = 0.0;
  [loop]
  for (int ty = 0; ty < 5; ++ty) {
    [loop]
    for (int tx = 0; tx < 5; ++tx) {
      let at = centre + (float2(float(tx), float(ty)) * 0.5 -
                         float2(1.0, 1.0)) *
                            radius;
      let corner = floor(at);
      let f = at - corner;
      lit = lit + shadowFour(map, corner, f, receiver, filterBias,
                             reference, low, high);
    }
  }
  return lit / 25.0;
}

/** HOW MUCH OF A CASTING LIGHT REACHES the point @p p, whose geometric
 *  normal is @p geometric, from the direction @p l: one where nothing it
 *  stored stands between, zero where something does, and between along a
 *  shadow's filtered edge. Nine taps a radius apart, each blended from the
 *  four texels around it, every one kept inside the light's own tile — or,
 *  from a source with a width, the penumbra it casts (`shadowSoft`). */
float shadowAt(int map, float3 p, float3 geometric, float3 l) {
  let depth = uShadowDepth[map];
  let bias = uShadowBias[map];
  let nDotL = max(dotP(geometric, l), 0.0);
  // The texel's width where the point stands: the same everywhere in an
  // orthographic map, growing with the distance in a perspective one.
  let lightRow = uShadowMatrix[map * 4 + 3];
  let distanceAway = depth.z > 0.5 ? max(dotP(lightRow, float4(p, 1.0)), 0.0)
                                   : 1.0;
  let texel = depth.w * distanceAway;
  let moved = p + geometric * shadowNormalOffset(bias.y, texel, nDotL);
  let position = float4(moved, 1.0);
  let clip = float4(dotP(uShadowMatrix[map * 4 + 0], position),
                    dotP(uShadowMatrix[map * 4 + 1], position),
                    dotP(uShadowMatrix[map * 4 + 2], position),
                    dotP(uShadowMatrix[map * 4 + 3], position));
  if (clip.w <= 0.0) return 1.0;
  let ndc = clip.xyz / clip.w;
  let uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
  // Beyond the map's far plane, or off its edge, nothing was drawn.
  if (ndc.z > 1.0 || uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
    return 1.0;
  let receiver = shadowDistance(ndc.z, depth.x, depth.y, depth.z);
  let movedTexel = depth.z > 0.5 ? depth.w * receiver : depth.w;
  let compareBias = shadowDepthBias(bias.x, movedTexel, nDotL, bias.z);
  let tile = uShadowTile[map];
  let side = 1.0 / uShadowDials.x;
  let low = tile.xy * side;
  let high = (tile.xy + tile.zw) * side - float2(1.0, 1.0);
  let centre = shadowAtlasUv(uv, tile) * side - float2(0.5, 0.5);
  if (uShadowSource[map].x > 0.0)
    return shadowed(shadowSoft(map, centre, receiver, movedTexel, nDotL, low,
                               high),
                    shadowEdgeFade(uv, bias.w));
  float lit = 0.0;
  // A FILTER ONE TEXEL WIDE reads overlapping blocks: its three taps a side
  // stand on three consecutive texels, so the nine blocks of four lie in
  // one block of sixteen, and each of those is compared once and read by
  // every tap that covers it. Where the taps' corners are not consecutive
  // — a wider filter, or a tap rounded across a texel's edge — each tap
  // compares its own four.
  let first = floor(centre + float2(shadowTapOffset(0.0, bias.z),
                                    shadowTapOffset(0.0, bias.z)));
  let middle = floor(centre + float2(shadowTapOffset(1.0, bias.z),
                                     shadowTapOffset(1.0, bias.z)));
  let last = floor(centre + float2(shadowTapOffset(2.0, bias.z),
                                   shadowTapOffset(2.0, bias.z)));
  if (all(middle == first + float2(1.0, 1.0)) &&
      all(last == first + float2(2.0, 2.0))) {
    float compared[16];
    if (all(first >= low) && all(first + float2(3.0, 3.0) <= high)) {
      // INSIDE THE TILE the sixteen are four gathers through the
      // comparison sampler, each the block of four around the corner its
      // texels share, returned (0, 1), (1, 1), (1, 0), (0, 0).
      let reference =
          shadowDepthOf(receiver - compareBias, depth.x, depth.y, depth.z);
      [ForceUnroll]
      for (int by = 0; by < 2; ++by) {
        [ForceUnroll]
        for (int bx = 0; bx < 2; ++bx) {
          let passed = uShadowCompare.GatherCmp(
              (first + float2(float(bx * 2 + 1), float(by * 2 + 1))) *
                  uShadowDials.x,
              reference);
          let k = by * 8 + bx * 2;
          compared[k] = passed.w;
          compared[k + 1] = passed.z;
          compared[k + 4] = passed.x;
          compared[k + 5] = passed.y;
        }
      }
    } else {
      [ForceUnroll]
      for (int y = 0; y < 4; ++y) {
        [ForceUnroll]
        for (int x = 0; x < 4; ++x)
          compared[y * 4 + x] = shadowTexel(
              map, clamp(first + float2(float(x), float(y)), low, high),
              receiver, compareBias);
      }
    }
    [ForceUnroll]
    for (int ty = 0; ty < 3; ++ty) {
      [ForceUnroll]
      for (int tx = 0; tx < 3; ++tx) {
        let at = centre + float2(shadowTapOffset(float(tx), bias.z),
                                 shadowTapOffset(float(ty), bias.z));
        let f = at - floor(at);
        let k = ty * 4 + tx;
        lit = lit + shadowTapWeight() *
                        shadowBilinear(compared[k], compared[k + 1],
                                       compared[k + 4], compared[k + 5], f);
      }
    }
    return shadowed(lit, shadowEdgeFade(uv, bias.w));
  }
  let reference =
      shadowDepthOf(receiver - compareBias, depth.x, depth.y, depth.z);
  [loop]
  for (int ty = 0; ty < 3; ++ty) {
    [loop]
    for (int tx = 0; tx < 3; ++tx) {
      let at = centre + float2(shadowTapOffset(float(tx), bias.z),
                               shadowTapOffset(float(ty), bias.z));
      let corner = floor(at);
      let f = at - corner;
      lit = lit + shadowTapWeight() * shadowFour(map, corner, f, receiver,
                                                 compareBias, reference, low,
                                                 high);
    }
  }
  return shadowed(lit, shadowEdgeFade(uv, bias.w));
}

/** HOW MUCH OF A CASTING LIGHT REACHES @p p through the map that starts
 *  at @p map: that map; for a directional light split into cascades, the
 *  one the point's distance in front of the camera falls in, crossing
 *  into the next over the last tenth of it; or for a point light the one
 *  of its six faces the point stands most squarely in front of.
 *
 *  The maps are chosen first and then read in one loop, so the filter
 *  stands in the program once however many maps a light may blend. */
float shadowOf(int map, float3 p, float3 geometric, float3 l) {
  int first = map;
  float crossed = 0.0;
#ifdef SIGIL_CASCADED_SHADOWS
  let ends = uShadowCascade[map];
  if (ends.x > 0.0) {
    let depth = -p.z;
    let index = int(shadowCascade(depth, ends));
    let end = index == 0 ? ends.x : index == 1 ? ends.y : index == 2 ? ends.z
                                                                     : ends.w;
    let start = index == 0 ? 0.0 : index == 1 ? ends.x : index == 2 ? ends.y
                                                                    : ends.z;
    let next = index == 0 ? ends.y : index == 1 ? ends.z : index == 2 ? ends.w
                                                                      : 0.0;
    first = map + index;
    crossed =
        next > 0.0 ? shadowCascadeBlend(depth, end, (end - start) * 0.1) : 0.0;
  }
#endif
#ifdef SIGIL_POINT_SHADOWS
#ifdef SIGIL_CASCADED_SHADOWS
  if (ends.x <= 0.0 && uShadowDepth[map].z >= 1.5) {
#else
  if (uShadowDepth[map].z >= 1.5) {
#endif
    let position = float4(p, 1.0);
    float squarest = -1e30;
    for (int face = 0; face < 6; ++face) {
      let at = map + face;
      let x = dotP(uShadowMatrix[at * 4 + 0], position);
      let y = dotP(uShadowMatrix[at * 4 + 1], position);
      let w = dotP(uShadowMatrix[at * 4 + 3], position);
      let square = w - max(abs(x), abs(y));
      if (square > squarest) {
        squarest = square;
        first = at;
      }
    }
  }
#endif
  float own = 1.0;
  float next = 1.0;
  let reads = crossed > 0.0 ? 2 : 1;
  [loop]
  for (int k = 0; k < reads; ++k) {
    let lit = shadowAt(first + k, p, geometric, l);
    if (k == 0)
      own = lit;
    else
      next = lit;
  }
  if (crossed <= 0.0) return own;
  return mixP(own, next, crossed);
}

#ifdef SIGIL_TINTED_SHADOWS
/** WHICH OF THE MAPS STARTING AT @p map the point @p p reads: for a
 *  directional light split into cascades, the one its distance in front
 *  of the camera falls in; for a point light, the face it stands most
 *  squarely in front of; otherwise that map. */
int shadowMapFor(int map, float3 p) {
  let ends = uShadowCascade[map];
  if (ends.x > 0.0) return map + int(shadowCascade(-p.z, ends));
  if (uShadowDepth[map].z < 1.5) return map;
  let position = float4(p, 1.0);
  int best = map;
  float squarest = -1e30;
  for (int face = 0; face < 6; ++face) {
    let at = map + face;
    let x = dotP(uShadowMatrix[at * 4 + 0], position);
    let y = dotP(uShadowMatrix[at * 4 + 1], position);
    let w = dotP(uShadowMatrix[at * 4 + 3], position);
    let square = w - max(abs(x), abs(y));
    if (square > squarest) {
      squarest = square;
      best = at;
    }
  }
  return best;
}

/** WHAT COLOUR OF A CASTING LIGHT COMES THROUGH to the point @p p, read
 *  through the maps starting at @p first: the colour the transmissive
 *  bodies between it and the light let through, where the point stands
 *  behind the nearest of them, and white where it does not. */
float3 shadowTintOf(int first, float3 p, float3 geometric, float3 l) {
  let map = shadowMapFor(first, p);
  let depth = uShadowDepth[map];
  let bias = uShadowBias[map];
  let nDotL = max(dotP(geometric, l), 0.0);
  let lightRow = uShadowMatrix[map * 4 + 3];
  let distanceAway = depth.z > 0.5 ? max(dotP(lightRow, float4(p, 1.0)), 0.0)
                                   : 1.0;
  let texel = depth.w * distanceAway;
  let position =
      float4(p + geometric * shadowNormalOffset(bias.y, texel, nDotL), 1.0);
  let clip = float4(dotP(uShadowMatrix[map * 4 + 0], position),
                    dotP(uShadowMatrix[map * 4 + 1], position),
                    dotP(uShadowMatrix[map * 4 + 2], position),
                    dotP(uShadowMatrix[map * 4 + 3], position));
  if (clip.w <= 0.0) return float3(1.0, 1.0, 1.0);
  let ndc = clip.xyz / clip.w;
  let uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
  if (ndc.z > 1.0 || uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
    return float3(1.0, 1.0, 1.0);
  let receiver = shadowDistance(ndc.z, depth.x, depth.y, depth.z);
  let movedTexel = depth.z > 0.5 ? depth.w * receiver : depth.w;
  let compareBias = shadowDepthBias(bias.x, movedTexel, nDotL, bias.z);
  let at = shadowAtlasUv(uv, uShadowTile[map]);
  // Both reads are made before either is looked at, so the device waits
  // for them once rather than once each.
  let stored = uShadowGlass.SampleLevel(at, 0.0).r;
  let tint = uShadowTint.SampleLevel(at, 0.0).rgb;
  let glass = shadowDistance(stored, depth.x, depth.y, depth.z);
  return receiver - compareBias <= glass ? float3(1.0, 1.0, 1.0) : tint;
}
#endif

/** THE MAP light @p light reads, or below zero for none. */
int shadowMapOf(int light) {
  if (uShadowDials.y < 0.5) return -1;
  let row = uLightShadow[light / 4];
  let lane = light % 4;
  let found = lane == 0 ? row.x : lane == 1 ? row.y : lane == 2 ? row.z : row.w;
  return int(found);
}

/** HOW MUCH OF POSITIONED LIGHT @p light ITS DISTANCE AND ITS CONE LET
 *  REACH a point @p far away along @p l, the unit direction toward it,
 *  before any shadow: the falloff its range gives, and for a spot light
 *  the share of its cone the direction stands in. */
float positionedReach(int light, float far, float3 l) {
  var reach = distanceFalloff(far, uLightDirection[light].w);
  if (uLightPosition[light].w > 1.5)
    reach = reach * coneFalloff(
                        dotP(-l, normalizeP(uLightDirection[light].xyz)),
                        uLightColor[light].w, uLightGround[light].w);
  return reach;
}

/** WHETHER A LIGHT OF KIND @p kind CAN CAST: a directional, point or spot
 *  light, or one with a size; an ambient or hemisphere light shines from
 *  everywhere and has no map. */
bool castsAShadow(float kind) {
#ifdef SIGIL_AREA_LIGHTS
  if (kind > 4.5) return true;
#endif
  return kind <= 2.5;
}

/** WHAT OF EACH CASTING LIGHT REACHES ONE PIXEL past every shadow: its
 *  share, one where nothing stands between, in the order the lights are
 *  given — at most four lights cast in a frame — and, where a casting
 *  light passes through glass, the colour that comes through. */
struct ShadowShares {
  float4 lit;
#ifdef SIGIL_TINTED_SHADOWS
  float3 tint0;
  float3 tint1;
  float3 tint2;
  float3 tint3;
#endif
};

/** The share of the casting light numbered @p casting among the casting
 *  lights. */
float shadowShareOf(ShadowShares shares, int casting) {
  return casting == 0   ? shares.lit.x
         : casting == 1 ? shares.lit.y
         : casting == 2 ? shares.lit.z
                        : shares.lit.w;
}

#ifdef SIGIL_TINTED_SHADOWS
/** The colour of the casting light numbered @p casting that comes through
 *  the glass between it and the point. */
float3 shadowTintShareOf(ShadowShares shares, int casting) {
  return casting == 0   ? shares.tint0
         : casting == 1 ? shares.tint1
         : casting == 2 ? shares.tint2
                        : shares.tint3;
}
#endif

/** EVERY CASTING LIGHT'S SHADOW AT @p p, READ ONCE, before any lobe of
 *  the surface is evaluated: the point's geometric normal @p geometric
 *  and its shading normal @p n are all the filter needs. Read here, the
 *  filter and the tint stand in the program once, in a loop of their own
 *  that holds none of the lobes' values, and the loop over the lights
 *  that evaluates the lobes holds no shadow code — each light's lobes
 *  read the share it left. A light its distance or its cone leaves dark
 *  is not read. */
ShadowShares shadowSharesAt(float3 p, float3 geometric, float3 n) {
  ShadowShares shares;
  shares.lit = float4(1.0, 1.0, 1.0, 1.0);
#ifdef SIGIL_TINTED_SHADOWS
  shares.tint0 = float3(1.0, 1.0, 1.0);
  shares.tint1 = float3(1.0, 1.0, 1.0);
  shares.tint2 = float3(1.0, 1.0, 1.0);
  shares.tint3 = float3(1.0, 1.0, 1.0);
#endif
  if (uShadowDials.y < 0.5) return shares;
  let count = int(uLightCount.x);
  int casting = 0;
  [loop]
  for (int i = 0; i < 16; ++i) {
    if (i >= count || casting >= 4) break;
    let kind = uLightPosition[i].w;
    let map = shadowMapOf(i);
    if (map < 0 || !castsAShadow(kind)) continue;
    let slot = casting;
    casting = casting + 1;
    // THE DIRECTION TOWARD THE LIGHT, as its lobes take it: a sized
    // light's shadow is read from its centre.
    float3 toward;
#ifdef SIGIL_AREA_LIGHTS
    if (kind > 4.5) {
      let centre = uLightPosition[i].xyz - p;
      toward = centre / max(lengthP(centre), 1e-4);
    } else
#endif
    if (kind > 0.5) {
      let toLight = uLightPosition[i].xyz - p;
      let far = lengthP(toLight);
      toward = far > 0.0 ? toLight / far : n;
      if (positionedReach(i, far, toward) <= 0.0) continue;
    } else {
      toward = -normalizeP(uLightDirection[i].xyz);
    }
    let lit = shadowOf(map, p, geometric, toward);
    shares.lit = slot == 0   ? float4(lit, shares.lit.yzw)
                 : slot == 1 ? float4(shares.lit.x, lit, shares.lit.zw)
                 : slot == 2 ? float4(shares.lit.xy, lit, shares.lit.w)
                             : float4(shares.lit.xyz, lit);
#ifdef SIGIL_TINTED_SHADOWS
    // What comes through glass is read only where some of the light does.
    if (lit > 0.0) {
      let tint = shadowTintOf(map, p, geometric, toward);
      if (slot == 0)
        shares.tint0 = tint;
      else if (slot == 1)
        shares.tint1 = tint;
      else if (slot == 2)
        shares.tint2 = tint;
      else
        shares.tint3 = tint;
    }
#endif
  }
  return shares;
}

/** THE LIT SUM at one pixel, in linear radiance, for a surface whose
 *  decoded colour is @p base. Emission is the caller's. */
float3 shadeLit(VSOut input, float3 base) {
  let v = normalizeP(-input.viewPos);
  // THE SIDE OF THE SURFACE THE EYE SEES. A body whose back face is drawn
  // is lit as the side it shows: the triangle's own facing, read off the
  // screen derivatives and turned toward the eye, says which side that
  // is, and a mesh normal pointing the other way is turned round with it.
  let across = cross(ddx(input.viewPos), ddy(input.viewPos));
  let shown = dotP(across, v) < 0.0 ? -across : across;
  var geometric = normalizeP(input.viewNormal);
  if (dotP(geometric, shown) < 0.0) geometric = -geometric;
  // The tangent frame is read off the screen derivatives, so it is found
  // for every pixel alike rather than inside a branch a neighbour may not
  // take.
  float3 t;
  float3 b;
  tangentAxes(geometric, input.viewPos, input.uv, t, b);
#ifdef SIGIL_DISTORTION
  // A DISTORTION is stated along the surface's own directions — across
  // the way u runs, and up the way a map's green points — so it follows
  // the surface however it is turned.
  let alongU = dotP(t, t) > 0.0 ? normalizeP(t) : float3(1.0, 0.0, 0.0);
  let alongV = dotP(b, b) > 0.0 ? normalizeP(b) : float3(0.0, 1.0, 0.0);
  gDistortionMoved =
      alongU * gSurfaceDistortion.x + alongV * gSurfaceDistortion.y;
#endif
#ifdef SIGIL_TRANSMISSION
  let curvature = curvatureOf(geometric, input.viewPos);
#endif
  float3 n = geometric;
  if (gSurfaceNormal.x != 0.0 || gSurfaceNormal.y != 0.0)
    n = normalizeP(t * gSurfaceNormal.x + b * gSurfaceNormal.y +
                   geometric * gSurfaceNormal.z);
  // Every casting light's shadow, read before any lobe holds a value.
  let shares = shadowSharesAt(input.viewPos, geometric, n);
#ifdef SIGIL_CLEARCOAT
  float3 coatNormal = geometric;
  if (gSurfaceClearcoatNormal.x != 0.0 || gSurfaceClearcoatNormal.y != 0.0)
    coatNormal = normalizeP(t * gSurfaceClearcoatNormal.x +
                            b * gSurfaceClearcoatNormal.y +
                            geometric * gSurfaceClearcoatNormal.z);
#endif

  let metal = saturateP(gSurfaceMetal);
  let rough = gSurfaceRoughness < 0.0 ? 0.5 : saturateP(gSurfaceRoughness);
  var diffuse = diffuseColor(base, metal);
  var f0 = specularColor(base, metal);
  var f90 = reflectance90(f0);
#ifdef SIGIL_SPECULAR
  // A dielectric's own strength and colour move its head-on and grazing
  // reflectance, and its diffuse side keeps what the reflection does not.
  let dielectric = dielectricReflectance(gSurfaceSpecularColor, gSurfaceSpecular);
  diffuse = diffuseColorFrom(base, metal, dielectric);
  f0 = specularColorFrom(base, metal, dielectric);
  f90 = reflectance90From(metal, gSurfaceSpecular);
#endif
#ifdef SIGIL_TRANSMISSION
  // A SURFACE LIGHT PASSES THROUGH reflects what its index says it does: an
  // interface's own head-on share, rising at the grazing angle only as far
  // as an interface that reflects so little can, so at an index of one —
  // no interface at all — it mirrors nothing and what is behind it is seen
  // whole, however it curves.
#ifdef SIGIL_SPECULAR
  let interfaceStrength = gSurfaceSpecular;
  let interfaceColor = gSurfaceSpecularColor;
#else
  let interfaceStrength = 1.0;
  let interfaceColor = float3(1.0, 1.0, 1.0);
#endif
  let atIndex = dielectricReflectance(interfaceColor, interfaceStrength,
                                      gSurfaceIor);
  diffuse = diffuseColorFrom(base, metal, atIndex);
  f0 = specularColorFrom(base, metal, atIndex);
  f90 = reflectance90From(
      metal, interfaceStrength *
                 saturateP(reflectanceOfIndex(gSurfaceIor) * 50.0));
#endif
  let nDotV = max(dotP(n, v), 0.0);
#ifdef SIGIL_IRIDESCENCE
  // A thin film moves the head-on reflectance by what its interference
  // makes of it at the angle the eye meets it, for every light alike.
  f0 = iridescentF0(f0, nDotV, gSurfaceIridescence, gSurfaceIridescenceIor,
                    gSurfaceIridescenceThickness);
#endif
  var r = n * (2.0 * nDotV) - v;
#ifdef SIGIL_ANISOTROPY
  // THE BRUSHING'S TANGENT, in the surface's own frame. Where the uv does
  // not vary across the pixel there is no frame to read, and the brushing
  // runs around the body's own up axis instead.
  let bodyUp = mul(uModelView, float4(0.0, 1.0, 0.0, 0.0)).xyz;
  var around = bodyUp - n * dotP(bodyUp, n);
  if (dotP(around, around) < 1e-8) around = float3(1.0, 0.0, 0.0) - n * n.x;
  around = normalizeP(cross(around, n));
  // …read off the mesh's own tangent where it states one, which runs
  // smoothly across a triangle's edge where the uvs' derivatives step.
  var brushT = t;
  var brushB = b;
  let stated = input.viewTangent.xyz - n * dotP(input.viewTangent.xyz, n);
  if (dotP(stated, stated) > 1e-12) {
    brushT = normalizeP(stated);
    // v grows along cross(n, t) times w, and a map's green points up the
    // picture, against v.
    brushB = -cross(n, brushT) * (input.viewTangent.w < 0.0 ? -1.0 : 1.0);
  }
  let brushed = anisotropyTangent(n, brushT, brushB,
                                  gSurfaceAnisotropyDirection,
                                  gSurfaceAnisotropyRotation, around);
  let anisotropy = saturateP(gSurfaceAnisotropy);
  // Light from everywhere is mirrored about a normal bent across the
  // brushing, which smears the panorama along it.
  let bent = anisotropicBentNormal(n, brushed, v, rough, anisotropy);
  r = bent * (2.0 * max(dotP(bent, v), 0.0)) - v;
#endif
  let tableSize = uLightCount.y;
#ifdef SIGIL_SHEEN
  let sheenColor = max(gSurfaceSheenColor, float3(0.0, 0.0, 0.0));
  let sheenRough = saturateP(gSurfaceSheenRoughness);
  // The sheen's directional albedo rides in the split sum's table, blue.
  let sheenAlbedoView =
      uEnvironmentBrdf
          .SampleLevel(environmentBrdfUv(sheenRough, nDotV, tableSize), 0.0)
          .b;
  let plainR = n * (2.0 * nDotV) - v;
  float3 sheenDirect = float3(0.0, 0.0, 0.0);
  float3 aroundSheen = float3(0.0, 0.0, 0.0);
#endif

#ifdef SIGIL_CLEARCOAT
  let coat = saturateP(gSurfaceClearcoat);
  let coatRough = saturateP(gSurfaceClearcoatRoughness);
  let coatF0 = float3(0.04, 0.04, 0.04);
  let white = float3(1.0, 1.0, 1.0);
  let coatNDotV = max(dotP(coatNormal, v), 0.0);
  let coatR = coatNormal * (2.0 * coatNDotV) - v;
  float3 coatDirect = float3(0.0, 0.0, 0.0);
  float3 aroundCoat = float3(0.0, 0.0, 0.0);
#endif

  float3 directDiffuseSum = float3(0.0, 0.0, 0.0);
  float3 directSpecularSum = float3(0.0, 0.0, 0.0);
  // What the ambient and hemisphere lights put on the surface from every
  // way at once: the cosine-weighted mean radiance at the normal, and the
  // radiance a mirror would see along each reflected ray.
  float3 aroundIrradiance = float3(0.0, 0.0, 0.0);
  float3 aroundMirrored = float3(0.0, 0.0, 0.0);

  let count = int(uLightCount.x);
  // Which of the casting lights the next one to cast is, counted as
  // `shadowSharesAt` counted them.
  int casting = 0;
  for (int i = 0; i < 16; ++i) {
    if (i >= count) break;
    let kind = uLightPosition[i].w;
    let colour = uLightColor[i].rgb;
    int shadowSlot = -1;
    if (shadowMapOf(i) >= 0 && castsAShadow(kind)) {
      shadowSlot = casting;
      casting = casting + 1;
    }
#ifdef SIGIL_AREA_LIGHTS
    if (kind > 4.5) {
      // A LIGHT WITH A SIZE: the diffuse side receives what the whole
      // light delivers, and the highlights are lit from the point of the
      // light nearest each reflected ray, carrying the solid angle the
      // light fills.
      let centre = uLightPosition[i].xyz - input.viewPos;
      let face = normalizeP(uLightDirection[i].xyz);
      let size = uLightColor[i].w;
      let far = max(lengthP(centre), 1e-4);
      let toCentre = centre / far;
      let facing = max(dotP(n, toCentre), 0.0);
      // What the diffuse side receives per unit of the colour, the solid
      // angle the light fills, and what a highlight receives from it.
      float irradiance;
      float solid;
      float strength;
      float3 lit;
#ifdef SIGIL_CLEARCOAT
      float3 litCoat;
#endif
      if (kind > 6.5) {
        let cosFace = dotP(-toCentre, face);
        if (cosFace <= 0.0) continue;
        let fall = rangeWindow(far, uLightDirection[i].w) /
                   (far * far + size * size);
        irradiance = cosFace * fall * facing;
        solid = diskSolidAngle(far, size, cosFace);
        strength = cosFace * fall;
        lit = normalizeP(diskPoint(centre, face, size, r));
#ifdef SIGIL_CLEARCOAT
        litCoat = normalizeP(diskPoint(centre, face, size, coatR));
#endif
      } else if (kind > 5.5) {
        let fall = distanceFalloff(far, uLightDirection[i].w);
        irradiance = fall * facing;
        solid = sphereSolidAngle(far, size);
        strength = fall;
        lit = normalizeP(spherePoint(centre, size, r));
#ifdef SIGIL_CLEARCOAT
        litCoat = normalizeP(spherePoint(centre, size, coatR));
#endif
      } else {
        if (dotP(centre, face) >= 0.0) continue;
        let across = uLightGround[i].xyz;
        let up = normalizeP(cross(face, across));
        let halfHeight = uLightGround[i].w;
        let corner0 = centre - across * size - up * halfHeight;
        let corner1 = centre + across * size - up * halfHeight;
        let corner2 = centre + across * size + up * halfHeight;
        let corner3 = centre - across * size + up * halfHeight;
        // The panel's reach ends in the window its range states, measured
        // from its centre.
        let window = rangeWindow(far, uLightDirection[i].w);
        irradiance =
            window * quadIrradiance(corner0, corner1, corner2, corner3, n);
        solid = quadSolidAngle(corner0, corner1, corner2, corner3);
        strength = window * solid;
        lit = normalizeP(rectPoint(centre, face, across, up, size, halfHeight,
                                   r));
#ifdef SIGIL_CLEARCOAT
        litCoat = normalizeP(rectPoint(centre, face, across, up, size,
                                       halfHeight, coatR));
#endif
      }
      if (irradiance <= 0.0 && strength <= 0.0) continue;
      // Its shadow, read from its centre before the lobes.
      float shade = 1.0;
      if (shadowSlot >= 0) {
        shade = shadowShareOf(shares, shadowSlot);
        if (shade <= 0.0) continue;
      }
#ifdef SIGIL_TINTED_SHADOWS
      let shaded = shadowSlot >= 0
                       ? colour * shade * shadowTintShareOf(shares, shadowSlot)
                       : colour * shade;
#else
      let shaded = colour * shade;
#endif
      let highlight = strength * areaNormalization(rough, solid);
      var areaDiffuseOne =
          shaded * areaDiffuse(n, toCentre, v, diffuse, f0, f90, irradiance);
#ifdef SIGIL_ANISOTROPY
      var areaSpecularOne =
          shaded * highlight *
          directSpecularAnisotropic(n, brushed, lit, v, f0, f90, rough,
                                    anisotropy);
#else
      var areaSpecularOne =
          shaded * highlight * directSpecular(n, lit, v, f0, f90, rough);
#endif
#ifdef SIGIL_SHEEN
      // A sheen is lit as by a point light at the light's centre carrying
      // the whole light.
      let areaSheenAlbedo =
          uEnvironmentBrdf
              .SampleLevel(environmentBrdfUv(sheenRough, facing, tableSize),
                           0.0)
              .b;
      let areaScaling =
          sheenScaling(sheenColor, sheenAlbedoView, areaSheenAlbedo);
      areaDiffuseOne = areaDiffuseOne * areaScaling;
      areaSpecularOne = areaSpecularOne * areaScaling;
      sheenDirect = sheenDirect + shaded * strength *
                                      directSheen(n, toCentre, v, sheenColor,
                                                  sheenRough);
#endif
      directDiffuseSum = directDiffuseSum + areaDiffuseOne;
      directSpecularSum = directSpecularSum + areaSpecularOne;
#ifdef SIGIL_CLEARCOAT
      if (coat > 0.0)
        coatDirect = coatDirect +
                     shaded * (strength * areaNormalization(coatRough, solid)) *
                         directSpecular(coatNormal, litCoat, v, coatF0, white,
                                        coatRough);
#endif
      continue;
    }
#endif
    if (kind > 3.5) {
      let up = uLightDirection[i].xyz;
      let ground = uLightGround[i].rgb;
      let facing = hemisphereIrradiance(n, up, colour, ground);
      aroundIrradiance = aroundIrradiance + facing;
      // A rough surface's reflection is as blurred as the light that
      // falls on it, which for two half-spaces is the blend at the normal.
      aroundMirrored = aroundMirrored +
                       mixP(hemisphereIrradiance(r, up, colour, ground),
                            facing, rough);
#ifdef SIGIL_CLEARCOAT
      aroundCoat = aroundCoat +
                   mixP(hemisphereIrradiance(coatR, up, colour, ground),
                        facing, coatRough);
#endif
#ifdef SIGIL_SHEEN
      aroundSheen = aroundSheen +
                    mixP(hemisphereIrradiance(plainR, up, colour, ground),
                         facing, sheenRough);
#endif
      continue;
    }
    if (kind > 2.5) {
      aroundIrradiance = aroundIrradiance + colour;
      aroundMirrored = aroundMirrored + colour;
#ifdef SIGIL_CLEARCOAT
      aroundCoat = aroundCoat + colour;
#endif
#ifdef SIGIL_SHEEN
      aroundSheen = aroundSheen + colour;
#endif
      continue;
    }
    float3 l = -normalizeP(uLightDirection[i].xyz);
    float reach = 1.0;
    if (kind > 0.5) {
      let toLight = uLightPosition[i].xyz - input.viewPos;
      let far = lengthP(toLight);
      l = far > 0.0 ? toLight / far : n;
      reach = positionedReach(i, far, l);
    }
    if (reach <= 0.0) continue;
    // THE SHADOW takes its share of this light, and of nothing else.
    var through = float3(1.0, 1.0, 1.0);
    if (shadowSlot >= 0) {
      reach = reach * shadowShareOf(shares, shadowSlot);
      if (reach <= 0.0) continue;
#ifdef SIGIL_TINTED_SHADOWS
      through = shadowTintShareOf(shares, shadowSlot);
#endif
    }
    let radiance = colour * through * reach;
    var diffuseOne = radiance * directDiffuse(n, l, v, diffuse, f0, f90);
#ifdef SIGIL_ANISOTROPY
    var specularOne = radiance * directSpecularAnisotropic(n, brushed, l, v, f0,
                                                           f90, rough,
                                                           anisotropy);
#else
    var specularOne = radiance * directSpecular(n, l, v, f0, f90, rough);
#endif
#ifdef SIGIL_SHEEN
    // The base beneath a sheen gives up what the sheen reflected, seen
    // from the eye and from this light.
    let sheenAlbedoLight =
        uEnvironmentBrdf
            .SampleLevel(environmentBrdfUv(sheenRough, max(dotP(n, l), 0.0),
                                           tableSize),
                         0.0)
            .b;
    let scaling = sheenScaling(sheenColor, sheenAlbedoView, sheenAlbedoLight);
    diffuseOne = diffuseOne * scaling;
    specularOne = specularOne * scaling;
    sheenDirect = sheenDirect +
                  radiance * directSheen(n, l, v, sheenColor, sheenRough);
#endif
    directDiffuseSum = directDiffuseSum + diffuseOne;
    directSpecularSum = directSpecularSum + specularOne;
#ifdef SIGIL_CLEARCOAT
    if (coat > 0.0)
      coatDirect = coatDirect + radiance * directSpecular(coatNormal, l, v,
                                                          coatF0, white,
                                                          coatRough);
#endif
  }

  // WHAT ARRIVES FROM EVERYWHERE: the panorama, where the scene has one,
  // beside what the ambient and hemisphere lights put there.
  let sky = uEnvDials.w > 0.0;
  float3 irradiance = aroundIrradiance;
  float3 mirrored = aroundMirrored;
#ifdef SIGIL_CLEARCOAT
  float3 coatMirrored = aroundCoat;
#endif
  if (sky) {
    irradiance = irradiance + irradianceAt(n);
    mirrored = mirrored + environmentAt(r, rough);
#ifdef SIGIL_CLEARCOAT
    if (coat > 0.0) coatMirrored = coatMirrored + environmentAt(coatR, coatRough);
#endif
  }
  let occluded = saturateP(gSurfaceOcclusion);
  let brdf = uEnvironmentBrdf
                 .SampleLevel(environmentBrdfUv(rough, nDotV, tableSize), 0.0)
                 .rg;
  var aroundDiffuse =
      environmentDiffuse(irradiance, diffuse, f0, f90, rough, nDotV, brdf) *
      occluded;
  // AN ADDITIVE REFLECTION is the stated share of what is mirrored, with
  // no Fresnel and no energy taken from anything else; the split sum is
  // the surface's own reflectance. The body writes one or the other as a
  // constant, so a program carries only the one it states.
  var aroundSpecular =
      gSurfaceReflection >= 0.0
          ? environmentReflection(mirrored, gSurfaceReflection) * occluded
          : environmentSpecular(mirrored, f0, f90, rough, nDotV, brdf) *
                occluded;
#ifdef SIGIL_SHEEN
  // From everywhere the base gives up what the sheen reflects toward the
  // eye, so a white sheen over a white surface in a uniform surround
  // returns no more than it was lit by.
  let aroundScaling = sheenScaling(sheenColor, sheenAlbedoView, 0.0);
  aroundDiffuse = aroundDiffuse * aroundScaling;
  aroundSpecular = aroundSpecular * aroundScaling;
  float3 sheenRadiance = aroundSheen;
  if (sky) sheenRadiance = sheenRadiance + environmentAt(plainR, sheenRough);
  let sheenAround =
      environmentSheen(sheenRadiance, sheenColor, sheenAlbedoView) * occluded;
#endif

#ifdef SIGIL_TRANSMISSION
  // …AND WHAT PASSES THROUGH IT: the scene behind, seen along the bent
  // ray — one reading per colour where the index disperses — tinted by
  // the base colour and dimmed by the volume it crossed. What the
  // reflection took is not transmitted, and the diffuse side gives up
  // the share that passed through, so reflected and transmitted light
  // together never exceed what arrived. A metal transmits nothing.
  let transmit = saturateP(gSurfaceTransmission) * (1.0 - metal);
  let ior = max(gSurfaceIor, 1.0);
  let level = roughnessLevel(transmissionRoughness(rough, ior),
                             uSceneColorDials.z);
  let here = input.position.xy * uSceneColorDials.xy;
  float3 seen;
  float travelled;
  if (gSurfaceDispersion > 0.0) {
    let iors = dispersedIors(ior, gSurfaceDispersion);
    float spreadRed;
    float spreadBlue;
    seen = float3(seenThrough(input.viewPos, v, n, curvature, iors.x,
                              gDistortionMoved, rough, level, here, spreadRed)
                      .r,
                  seenThrough(input.viewPos, v, n, curvature, iors.y,
                              gDistortionMoved, rough, level, here, travelled)
                      .g,
                  seenThrough(input.viewPos, v, n, curvature, iors.z,
                              gDistortionMoved, rough, level, here, spreadBlue)
                      .b);
  } else {
    seen = seenThrough(input.viewPos, v, n, curvature, ior, gDistortionMoved,
                       rough, level, here, travelled);
  }
  let reflected = fresnelRough(f0, f90, nDotV, rough) * brdf.x + f90 * brdf.y;
  let through = seen * base *
                volumeTransmittance(gSurfaceAttenuationColor,
                                    gSurfaceAttenuationDistance, travelled) *
                transmittedShare(reflected, transmit);
  float3 colour = (directDiffuseSum + aroundDiffuse) * (1.0 - transmit) +
                  directSpecularSum + aroundSpecular + through;
#else
  float3 colour =
      directDiffuseSum + aroundDiffuse + directSpecularSum + aroundSpecular;
#endif
#ifdef SIGIL_SHEEN
  colour = colour + sheenDirect + sheenAround;
#endif

#ifdef SIGIL_CLEARCOAT
  // THE COAT lies over all of it: what it reflects is added, and what it
  // reflected is what the surface beneath no longer receives or shows.
  if (coat > 0.0) {
    let coatAngle = max(coatNDotV, 0.1);
    let coatBrdf =
        uEnvironmentBrdf
            .SampleLevel(environmentBrdfUv(coatRough, coatAngle, tableSize), 0.0)
            .rg;
    let coatAround =
        environmentSpecular(coatMirrored, coatF0, coatRough, coatAngle,
                            coatBrdf) *
        occluded;
    colour = colour * clearcoatTransmittance(coat, coatNDotV) +
             (coatDirect + coatAround) * coat;
  }
#endif
  return colour;
}
#endif

/** HOW MUCH OF THE SURFACE STANDS at the pixel @p position: all of it, or
 *  where it fades into the scene, the share of its fade distance that
 *  still lies between it and the opaque surface behind. */
float depthFadeAt(float4 position) {
  if (gSurfaceDepthFade <= 0.0 || uDepthFade.z < 0.5) return 1.0;
  let uv = position.xy * uDepthFade.xy;
  let behind = uSceneDepth.SampleLevel(uv, 0.0).r;
  let gap = eyeDistance(uv, behind) - eyeDistance(uv, position.z);
  return saturateP(gap / gSurfaceDepthFade);
}

/** The shaded colour as LIGHT, PREMULTIPLIED — which is what a target
 *  holding premultiplied radiance must receive, and what the blend the
 *  renderer sets expects. */
float4 shade(float4 surface, VSOut input) {
  let mapUv = mul(uBaseColorMapUv, float4(input.uv, 0.0, 1.0)).xy;
  let map = uBaseColorMap.Sample(mapUv);
  let colour = surface.rgb * input.tint.rgb * map.rgb;
  let fade = depthFadeAt(input.position);
  let a = saturateP(surface.a * input.tint.a) * map.a * fade;
#ifdef SIGIL_LIT
  // THE LIT SUM, in light. The camera reads it later, once.
  let radiance = shadeLit(input, linearFromSrgb(colour)) + gSurfaceEmission;
#else
  // The unlit build's colour is authored, not integrated: it is written as
  // the light the camera's reading turns back into it, cut to the unit
  // range a display holds.
  let radiance = toneCurveInverse(linearFromSrgb(saturateP(colour)), uTone.x,
                                  uTone.y);
#endif
#ifdef SIGIL_DISTORTION
  // A DISTORTING SURFACE is drawn over what stands behind it AS what
  // stands behind it, displaced along the surface by the map at the
  // surface's own depth; its own colour is laid over that by its alpha. A
  // surface that also transmits has the same displacement in what it
  // passes, whatever its alpha.
  if (uSceneColorDials.w > 0.5) {
    let here = input.position.xy * uSceneColorDials.xy;
    let at = viewPointAt(here, input.position.z);
#ifdef SIGIL_LIT
    let moved = screenUvOf(at + gDistortionMoved);
#else
    let moved = screenUvOf(at + float3(gSurfaceDistortion, 0.0));
#endif
    let behind = uSceneColor
                     .SampleLevel(clamp(moved, float2(0.0, 0.0),
                                        float2(1.0, 1.0)),
                                  0.0)
                     .rgb;
    return float4(radiance * a + behind * (1.0 - a), 1.0);
  }
#endif
  return float4(radiance * a, a);
}

[shader("fragment")]
float4 fsFlat(VSOut input) : SV_Target {
  enterInstance(input);
  return shade(surfaceFlat(input.uv), input);
}

/** WHERE A BODY BLOCKS A LIGHT, for one whose coverage its map decides:
 *  the pixels whose coverage — the surface's alpha, its tint's and its
 *  map's — falls short of one half are dropped, and the rest write
 *  depth. */
void shadowCoverage(float4 surface, VSOut input) {
  let mapUv = mul(uBaseColorMapUv, float4(input.uv, 0.0, 1.0)).xy;
  let map = uBaseColorMap.Sample(mapUv);
  if (surface.a * input.tint.a * map.a < 0.5) discard;
}

/** THE SHADOW STAGE'S TINT for a transmissive body: the colour of a light
 *  it lets through, which the stage lays over what is there by
 *  multiplying. */
[shader("fragment")]
float4 fsShadowTint(VSOut input) : SV_Target {
  // The share it transmits and how deep it is, each as its map says where
  // it has one — an unbound map reads white, which says the number.
  let weight = clamp(uTintVolume.x * uTintTransmissionMap.Sample(input.uv).r,
                     0.0, 1.0);
  let depth = max(uTintVolume.y * uTintThicknessMap.Sample(input.uv).r, 0.0);
  // The volume point by point where a map states it: the distance in the
  // volume map's blue as distance / (distance + 1), the colour the
  // attenuation map's as typed.
  var distance = uTintVolume.z;
  let volume = uTintVolumeMap.Sample(input.uv);
  if (volume.r < 1.0 || volume.g < 1.0 || volume.b < 1.0) {
    let held = min(volume.b, 0.999);
    distance = held / (1.0 - held);
  }
  let attenuation = uTintAttenuation.rgb *
                    linearFromSrgb(uTintAttenuationMap.Sample(input.uv).rgb);
  var colour = uBaseColor.rgb;
  if (distance > 0.0)
    colour = colour * pow(max(attenuation, float3(1e-4, 1e-4, 1e-4)),
                          float3(depth / distance));
  return float4(float3(1.0 - weight) + colour * weight, 1.0);
}

/** THE SHADOW STAGE'S OWN DRAW for a body whose surface does not decide
 *  where it covers: depth, and nothing else. */
[shader("fragment")]
void fsShadow(VSOut input) {}

/** …and for a body with no material program whose map does. */
[shader("fragment")]
void fsShadowFlat(VSOut input) {
  enterInstance(input);
  shadowCoverage(surfaceFlat(input.uv), input);
}

#ifdef SIGIL_LIT
// ---- THE SKY ITSELF ------------------------------------------------
// One triangle over the whole target, drawn before every body in the
// frame, with each pixel reading the panorama along the ray the eye
// looks through it. It is a separate pass and not a body because a body
// would need a mesh, a placement and a depth, and a sky has none of the
// three: it is what is there when nothing else is.

struct BackdropOut {
  float4 position : SV_Position;
  /** Where on the near plane this pixel stands, in clip coordinates. */
  float2 ndc : TEXCOORD0;
};

[shader("vertex")]
BackdropOut vsBackdrop(uint vertex : SV_VertexID) {
  // One oversized triangle rather than two: no shared edge to seam and
  // no vertex buffer to bind.
  BackdropOut out;
  out.ndc = float2((vertex == 2) ? 3.0 : -1.0, (vertex == 1) ? 3.0 : -1.0);
  out.position = float4(out.ndc, 0.0, 1.0);
  return out;
}

[shader("fragment")]
float4 fsBackdrop(BackdropOut input) : SV_Target {
  if (uBackdrop.x <= 0.0 || uEnvDials.w <= 0.0) discard;
  // THE RAY THROUGH THIS PIXEL, in the space the shading is written in.
  // The depth this renderer clips in runs from zero at the near plane to
  // one at the far one, so the point named here stands on the FAR plane
  // — which is the same ray every point along it stands on, and is what
  // this pass wants.
  let onRay = mul(uInvProj, float4(input.ndc, 1.0, 1.0));
  var ray = normalizeP(onRay.xyz / onRay.w);
  // A view-space point in front of the eye has a negative z; a
  // homogeneous divide by a negative w would answer the ray behind it.
  if (ray.z > 0.0) ray = -ray;
  // A SKY PROJECTED ONTO A GROUND SPHERE is read where the ray leaves
  // the sphere, along the direction from its centre to that point, so
  // an eye moving through the scene sees the horizon shift. The eye is
  // this space's origin, so it stands at minus the centre relative to
  // it; inside the sphere the quadratic has one positive root, the
  // exit. An eye on or outside the sphere, or a radius of zero, reads
  // by direction: the sky at infinity.
  if (uGround.w > 0.0) {
    let o = -uGround.xyz;
    let b = dotP(o, ray);
    let c = dotP(o, o) - uGround.w * uGround.w;
    if (c < 0.0) ray = normalizeP(o + ray * (sqrtP(b * b - c) - b));
  }
  let sky = environmentAt(ray, uBackdrop.y) * uBackdrop.x;
  // The specular dial belongs to what a surface mirrors and not to what
  // the sky IS, so it is divided back out here. The sky is light like a
  // lit surface's, read by the same final stage, so a sun in the backdrop
  // and the same sun reflected in a body standing in front of it are the
  // same brightness.
  let scale = uEnvDials.y > 0.0 ? 1.0 / uEnvDials.y : 0.0;
  return float4(sky * scale, 1.0);
}
#endif
