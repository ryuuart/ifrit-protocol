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
// SIGIL_LIT is the one axis the source is specialised on. Defined, the
// lights, the environment and the camera's curve reach the surface;
// undefined, the surface colour stands as it is and the lighting, its
// uniforms and its loop are not in the program at all.
//
// THE LIT BUILD SHADES EVERY PIXEL, in view space and in LINEAR light:
//
//   1. the surface's colour is decoded;
//   2. every light reaching the pixel adds its radiance through the
//      microfacet model — a Lambert diffuse and a GGX specular lobe,
//      split between them by metalness and by Fresnel — and a positioned
//      light is reached at the falloff its distance and its cone give
//      HERE, not at some point standing in for the body;
//   3. the light arriving from everywhere — the environment's panorama,
//      ambient and hemisphere lights — adds its diffuse and its mirrored
//      share, with the energy a rough surface scatters more than once put
//      back, darkened by the surface's occlusion and by nothing else;
//   4. a clear coat, where the surface has one, reflects a second,
//      colourless lobe over all of that and dims it by what it reflected;
//   5. emission adds its own light;
//   6. the sum is read at the camera's exposure, through the camera's
//      tone curve, and encoded for the display target — last, once.

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
/** THE MAP the body is dressed with, multiplied into the surface's colour
 *  before the lighting. A body given no map reads one white texel, so a
 *  colour multiplied by a map it does not have is that colour. */
uniform Sampler2D uBaseColorMap;
/** Where that map is read at, over the mesh's own uv coordinates: the
 *  identity reads the image once across them. */
uniform float4x4 uBaseColorMapUv;

#ifdef SIGIL_LIT
/** view * model, for the position the shading is written in. */
uniform float4x4 uModelView;
/** The inverse transpose of view * model, its 3x3 in the upper left. */
uniform float4x4 uNormalMatrix;
/** THE LIGHTS, one record of four rows each, in the space the shading is
 *  written in and with every colour already decoded and scaled by its
 *  intensity.
 *
 *  position xyz: where a point or spot stands; w: the kind — 0
 *  directional, 1 point, 2 spot, 3 ambient, 4 hemisphere.
 *  direction xyz: the way a directional or spot light travels, or a
 *  hemisphere's up; w: the range a positioned light is windowed to, zero
 *  for none.
 *  color rgb: the radiance it shines with — a hemisphere's sky; w: a
 *  spot's outer cone, as a cosine.
 *  ground rgb: a hemisphere's ground; w: a spot's inner cone, as a
 *  cosine. `uLightCount.x` says how many of the sixteen are lit, and
 *  `uLightCount.y` how many texels a side the split sum's table has. */
uniform float4 uLightPosition[16];
uniform float4 uLightDirection[16];
uniform float4 uLightColor[16];
uniform float4 uLightGround[16];
uniform float4 uLightCount;

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
/** The projection's inverse, which turns a point on the near plane into
 *  the direction the eye is looking through it. */
uniform float4x4 uInvProj;
/** x: the diffuse side's own scale; y: the specular side's;
 *  z: added to every surface's roughness; w: how many levels the chain
 *  has — and zero, which is a frame with no environment at all. */
uniform float4 uEnvDials;
/** THE CAMERA'S READING of the picture. x: the exposure every radiance
 *  is multiplied by before the curve; y: which curve — 0 Reinhard on
 *  luminance, 1 the neutral curve, 2 none. */
uniform float4 uTone;
#endif

struct VSIn {
  float3 position : ATTRIB0;
  float3 normal : ATTRIB1;
  float2 uv : ATTRIB2;
  float4 color : ATTRIB3;
};

struct VSOut {
  float4 position : SV_Position;
  /** The mesh's own per-vertex tint. */
  float4 tint : COLOR0;
  float2 uv : TEXCOORD0;
#ifdef SIGIL_LIT
  /** Where the surface stands and which way it faces, in the space the
   *  shading is written in. */
  float3 viewNormal : NORMAL0;
  float3 viewPos : TEXCOORD1;
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
/** How much of what is behind the surface comes through it, at what
 *  index of refraction, through what thickness of a medium that takes
 *  `gSurfaceAbsorption` out of the light per unit of it. */
static float gSurfaceTransmission = 0.0;
static float gSurfaceIor = 1.5;
static float gSurfaceThickness = 0.0;
static float3 gSurfaceAbsorption = float3(0.0, 0.0, 0.0);
/** What a body states for an additive reflection. Nothing here reads it:
 *  the environment reaches every surface through its own reflectance. */
static float gSurfaceReflection = -1.0;
/** The light this pixel gives off of its own, in LINEAR radiance: added to
 *  the lit sum before the exposure and the curve, so a strength above one
 *  is brighter than white and the curve, not the target, holds it. */
static float3 gSurfaceEmission = float3(0.0, 0.0, 0.0);

[shader("vertex")]
VSOut vsMain(VSIn input) {
  VSOut out;
  out.position = mul(uViewProj, mul(uModel, float4(input.position, 1.0)));
  out.uv = input.uv;
  out.tint = input.color;
#ifdef SIGIL_LIT
  out.viewPos = mul(uModelView, float4(input.position, 1.0)).xyz;
  out.viewNormal = mul(uNormalMatrix, float4(input.normal, 0.0)).xyz;
#endif
  return out;
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
 *  and at a roughness. The two maps are sampled and mixed rather than
 *  one being rebuilt, which is what lets a sky change while the frame is
 *  running. */
float3 environmentAt(float3 direction, float roughness) {
  let d = normalizeP(mul(uEnvMatrix, float4(direction, 0.0)).xyz);
  let uv = equirectangularUv(d);
  let level = roughnessLevel(roughness + uEnvDials.z, uEnvDials.w);
  let a = uEnvironment.SampleLevel(uv, level).rgb;
  let b = uEnvironmentNext.SampleLevel(uv, level).rgb;
  return mixP(a, b, uEnvTint.a) * uEnvTint.rgb * uEnvDials.y;
}

/** …and what falls on a surface facing that way from everywhere. */
float3 irradianceAt(float3 normal) {
  let d = normalizeP(mul(uEnvMatrix, float4(normal, 0.0)).xyz);
  let uv = equirectangularUv(d);
  let a = uIrradiance.SampleLevel(uv, 0.0).rgb;
  let b = uIrradianceNext.SampleLevel(uv, 0.0).rgb;
  return mixP(a, b, uEnvTint.a) * uEnvTint.rgb * uEnvDials.x;
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
  float3 n = geometric;
  if (gSurfaceNormal.x != 0.0 || gSurfaceNormal.y != 0.0)
    n = normalizeP(t * gSurfaceNormal.x + b * gSurfaceNormal.y +
                   geometric * gSurfaceNormal.z);
  float3 coatNormal = geometric;
  if (gSurfaceClearcoatNormal.x != 0.0 || gSurfaceClearcoatNormal.y != 0.0)
    coatNormal = normalizeP(t * gSurfaceClearcoatNormal.x +
                            b * gSurfaceClearcoatNormal.y +
                            geometric * gSurfaceClearcoatNormal.z);

  let metal = saturateP(gSurfaceMetal);
  let rough = gSurfaceRoughness < 0.0 ? 0.5 : saturateP(gSurfaceRoughness);
  let diffuse = diffuseColor(base, metal);
  let f0 = specularColor(base, metal);
  let f90 = reflectance90(f0);
  let nDotV = max(dotP(n, v), 0.0);
  let r = n * (2.0 * nDotV) - v;

  let coat = saturateP(gSurfaceClearcoat);
  let coatRough = saturateP(gSurfaceClearcoatRoughness);
  let coatF0 = float3(0.04, 0.04, 0.04);
  let white = float3(1.0, 1.0, 1.0);
  let coatNDotV = max(dotP(coatNormal, v), 0.0);
  let coatR = coatNormal * (2.0 * coatNDotV) - v;

  float3 directDiffuseSum = float3(0.0, 0.0, 0.0);
  float3 directSpecularSum = float3(0.0, 0.0, 0.0);
  float3 coatDirect = float3(0.0, 0.0, 0.0);
  // What the ambient and hemisphere lights put on the surface from every
  // way at once: the cosine-weighted mean radiance at the normal, and the
  // radiance a mirror would see along each reflected ray.
  float3 aroundIrradiance = float3(0.0, 0.0, 0.0);
  float3 aroundMirrored = float3(0.0, 0.0, 0.0);
  float3 aroundCoat = float3(0.0, 0.0, 0.0);

  let count = int(uLightCount.x);
  for (int i = 0; i < 16; ++i) {
    if (i >= count) break;
    let kind = uLightPosition[i].w;
    let colour = uLightColor[i].rgb;
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
      aroundCoat = aroundCoat +
                   mixP(hemisphereIrradiance(coatR, up, colour, ground),
                        facing, coatRough);
      continue;
    }
    if (kind > 2.5) {
      aroundIrradiance = aroundIrradiance + colour;
      aroundMirrored = aroundMirrored + colour;
      aroundCoat = aroundCoat + colour;
      continue;
    }
    float3 l = -normalizeP(uLightDirection[i].xyz);
    float reach = 1.0;
    if (kind > 0.5) {
      let toLight = uLightPosition[i].xyz - input.viewPos;
      let far = lengthP(toLight);
      l = far > 0.0 ? toLight / far : n;
      reach = distanceFalloff(far, uLightDirection[i].w);
      if (kind > 1.5)
        reach = reach * coneFalloff(
                            dotP(-l, normalizeP(uLightDirection[i].xyz)),
                            uLightColor[i].w, uLightGround[i].w);
    }
    if (reach <= 0.0) continue;
    let radiance = colour * reach;
    directDiffuseSum =
        directDiffuseSum + radiance * directDiffuse(n, l, v, diffuse, f0, f90);
    directSpecularSum =
        directSpecularSum + radiance * directSpecular(n, l, v, f0, f90, rough);
    if (coat > 0.0)
      coatDirect = coatDirect + radiance * directSpecular(coatNormal, l, v,
                                                          coatF0, white,
                                                          coatRough);
  }

  // WHAT ARRIVES FROM EVERYWHERE: the panorama, where the set has one,
  // beside what the ambient and hemisphere lights put there.
  let sky = uEnvDials.w > 0.0;
  float3 irradiance = aroundIrradiance;
  float3 mirrored = aroundMirrored;
  float3 coatMirrored = aroundCoat;
  if (sky) {
    irradiance = irradiance + irradianceAt(n);
    mirrored = mirrored + environmentAt(r, rough);
    if (coat > 0.0) coatMirrored = coatMirrored + environmentAt(coatR, coatRough);
  }
  let occluded = saturateP(gSurfaceOcclusion);
  let tableSize = uLightCount.y;
  let brdf = uEnvironmentBrdf
                 .SampleLevel(environmentBrdfUv(rough, nDotV, tableSize), 0.0)
                 .rg;
  let aroundDiffuse =
      environmentDiffuse(irradiance, diffuse, f0, rough, nDotV, brdf) *
      occluded;
  let aroundSpecular =
      environmentSpecular(mirrored, f0, rough, nDotV, brdf) * occluded;

  // …AND WHAT PASSES THROUGH IT. The refracted ray reads the same
  // panorama, attenuated by what the medium takes out of it over the
  // thickness it crossed, and Fresnel decides how much of the light went
  // that way rather than bouncing; the diffuse side gives up the share
  // that passed through. This is glass against the WORLD; what stands
  // behind a body on screen is a backdrop pass and not a shading term.
  let transmit = sky ? saturateP(gSurfaceTransmission) : 0.0;
  float3 through = float3(0.0, 0.0, 0.0);
  if (transmit > 0.0) {
    let bent = refraction(-v, n, 1.0 / max(gSurfaceIor, 1.0));
    through = attenuate(environmentAt(bent, rough), gSurfaceAbsorption,
                        gSurfaceThickness) *
              (white - fresnel(f0, nDotV)) * transmit;
  }
  float3 colour = (directDiffuseSum + aroundDiffuse) * (1.0 - transmit) +
                  directSpecularSum + aroundSpecular + through;

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
  return colour;
}
#endif

/** The shaded colour, PREMULTIPLIED — which is what a target holding
 *  premultiplied pixels must receive, and what the blend the renderer
 *  sets expects. */
float4 shade(float4 surface, VSOut input) {
  let mapUv = mul(uBaseColorMapUv, float4(input.uv, 0.0, 1.0)).xy;
  let map = uBaseColorMap.Sample(mapUv);
  let colour = surface.rgb * input.tint.rgb * map.rgb;
  let a = saturateP(surface.a * input.tint.a) * map.a;
#ifdef SIGIL_LIT
  // THE LIT SUM, in light, then read the way the camera reads it: exposed,
  // compressed by its curve onto what a display can hold, and encoded.
  let radiance = shadeLit(input, linearFromSrgb(colour)) + gSurfaceEmission;
  let rgb = srgbFromLinear(toneCurve(radiance, uTone.x, uTone.y));
#else
  // The unlit build has no radiance to compress — its colour is authored,
  // not integrated — so it stands as it was typed, cut to the unit range.
  let rgb = saturateP(colour);
#endif
  return float4(rgb * a, a);
}

[shader("fragment")]
float4 fsFlat(VSOut input) : SV_Target {
  return shade(surfaceFlat(input.uv), input);
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
  // an eye moving through the set sees the horizon shift. The eye is
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
  // the sky IS, so it is divided back out here.
  let scale = uEnvDials.y > 0.0 ? 1.0 / uEnvDials.y : 0.0;
  // …and the sky ends at the same curve at the same exposure a surface
  // does, and is encoded the same way, so a sun in the backdrop and the
  // same sun reflected in a body standing in front of it are the same
  // brightness.
  let rgb = srgbFromLinear(toneCurve(sky * scale, uTone.x, uTone.y));
  return float4(rgb, 1.0);
}
#endif
