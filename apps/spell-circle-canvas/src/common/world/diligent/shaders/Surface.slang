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
// alpha. Lighting is not its business.
//
// A BODY'S MAP reaches this text as one sampled slot and the placement
// it is read at, so a surface function never has to know whether it was
// dressed: the slot an unmapped body leaves empty reads one white texel,
// and the map multiplies the shaded colour rather than the surface.
//
// SIGIL_LIT is the one axis the source is specialised on. Defined, the
// emitters reach the surface and the vertex stage carries the shading;
// undefined, the surface colour stands as it is and the lighting, its
// uniforms and its loop are not in the program at all.
//
// SHADING IS PER VERTEX, in view space, and it is written as
//
//     colour = surface * (ambient + sum(light * lambert))
//              + sum(light * blinn) + rim
//
// so the two bracketed halves — a multiplier the surface colour scales
// and an addend it does not — are what the vertex stage interpolates.
// That decomposition is what lets one surface function serve both builds
// with no second shading path.
//
// A BODY MAY ASK FOR IT AGAIN, per pixel. There is one reason to: a MAP
// that varies the surface across a face — a normal map above all — is a
// per-pixel answer, and no per-vertex evaluation can carry it. A body
// dressed with one writes what it found into the three variables below
// and the emitter loop runs again where those values can be seen. A body
// that writes nothing keeps the terms the vertex stage interpolated,
// down to the bit, which is what keeps a scene with no such map the
// picture it already was.

import Portable;

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
/** THE MAP the body is dressed with, multiplied into the SHADED colour.
 *  Not into the surface before the lighting: the host tier's rasteriser
 *  can only modulate a texture against the colour it already shaded, and
 *  a map that landed on one side of the lighting here and the other side
 *  there would make the two tiers different pictures. A body given no
 *  map reads one white texel, so a colour multiplied by a map it does
 *  not have is that colour. */
uniform Sampler2D uBaseColorMap;
/** Where that map is read at, over the mesh's own uv coordinates: the
 *  identity reads the image once across them. */
uniform float4x4 uBaseColorMapUv;

#ifdef SIGIL_LIT
/** view * model, for the position the shading is written in. */
uniform float4x4 uModelView;
/** The inverse transpose of view * model, its 3x3 in the upper left. */
uniform float4x4 uNormalMatrix;
/** The inverse transpose of view, which is what carries a world-space
 *  light direction into the space the shading is written in. */
uniform float4x4 uLightMatrix;
/** The light every surface receives before any emitter reaches it. */
uniform float4 uAmbient;
/** Per emitter: the direction it TRAVELS in world space, and its colour
 *  already scaled by its intensity. `uShading.w` says how many of the
 *  four are lit. */
uniform float4 uLightDir[4];
uniform float4 uLightColor[4];
/** x: specular strength; y: specular exponent; z: rim strength;
 *  w: how many emitters. */
uniform float4 uShading;
#endif

struct VSIn {
  float3 position : ATTRIB0;
  float3 normal : ATTRIB1;
  float2 uv : ATTRIB2;
  float4 color : ATTRIB3;
};

struct VSOut {
  float4 position : SV_Position;
  /** The factor the surface colour is scaled by. */
  float3 shadeMul : COLOR0;
  /** The term added after it — specular and rim, which no surface
   *  colour scales. */
  float3 shadeAdd : COLOR1;
  /** The mesh's own per-vertex tint. */
  float4 tint : COLOR2;
  float2 uv : TEXCOORD0;
#ifdef SIGIL_LIT
  /** Where the surface stands and which way it faces, in the space the
   *  shading is written in — what a per-pixel evaluation needs and the
   *  two halves above have already spent. */
  float3 viewNormal : NORMAL0;
  float3 viewPos : TEXCOORD1;
#endif
};

// ---- what a body may say about its surface --------------------------
// Declared in both builds, so one body compiles for either; read only
// where there is lighting to re-evaluate.

/** The surface normal in TANGENT space, as a normal map encodes it.
 *  (0, 0, 1) is the surface as its geometry describes it. */
static float3 gSurfaceNormal = float3(0.0, 0.0, 1.0);
/** The Blinn exponent this pixel wants; below zero takes the scaffold's
 *  own. */
static float gSurfaceGloss = -1.0;
/** How much of a metal this pixel is: it takes the light out of the
 *  diffuse term and puts the surface's own colour into the highlight. */
static float gSurfaceMetal = 0.0;
/** Did the body write any of the three? Nothing is re-evaluated until
 *  one is, so a body that asks for nothing costs nothing and changes
 *  nothing. */
static bool gSurfacePerPixel = false;

[shader("vertex")]
VSOut vsMain(VSIn input) {
  VSOut out;
  out.position = mul(uViewProj, mul(uModel, float4(input.position, 1.0)));
  out.uv = input.uv;
  out.tint = input.color;
  out.shadeMul = float3(1.0, 1.0, 1.0);
  out.shadeAdd = float3(0.0, 0.0, 0.0);

#ifdef SIGIL_LIT
  let viewPos = mul(uModelView, float4(input.position, 1.0)).xyz;
  let n = normalizeP(mul(uNormalMatrix, float4(input.normal, 0.0)).xyz);
  let v = normalizeP(-viewPos);
  out.viewNormal = n;
  out.viewPos = viewPos;

  float3 factor = uAmbient.rgb;
  float3 term = float3(0.0, 0.0, 0.0);
  let count = int(uShading.w);
  for (int i = 0; i < 4; ++i) {
    if (i >= count) break;
    let l = normalizeP(mul(uLightMatrix, float4(-uLightDir[i].xyz, 0.0)).xyz);
    let lambert = max(dotP(n, l), 0.0);
    factor = factor + uLightColor[i].rgb * lambert;
    if (uShading.x > 0.0 && lambert > 0.0) {
      let h = normalizeP(l + v);
      let blinn = powiP(max(dotP(n, h), 0.0), int(uShading.y)) * uShading.x;
      term = term + uLightColor[i].rgb * blinn;
    }
  }
  if (uShading.z > 0.0) {
    let facing = 1.0 - max(dotP(n, v), 0.0);
    let rim = facing * facing * facing * uShading.z;
    term = term + float3(rim, rim, rim);
  }
  out.shadeMul = factor;
  out.shadeAdd = term;
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
 *  but at one scale, which is all a perturbation needs. */
void tangentAxes(float3 n, float3 p, float2 uv, out float3 t, out float3 b) {
  let dpx = ddx(p);
  let dpy = ddy(p);
  let duvx = ddx(uv);
  let duvy = ddy(uv);
  let px = cross(dpy, n);
  let py = cross(n, dpx);
  t = px * duvx.x + py * duvy.x;
  b = px * duvx.y + py * duvy.y;
  let scale = max(dotP(t, t), dotP(b, b));
  let inv = scale > 0.0 ? 1.0 / sqrtP(scale) : 0.0;
  t = t * inv;
  b = b * inv;
}

/** The two shading halves again, where the body's per-pixel surface can
 *  be seen. @p albedo is what a metal tints its highlight with. */
void shadeAgain(VSOut input, float3 albedo, out float3 outMul,
                out float3 outAdd) {
  let geometric = normalizeP(input.viewNormal);
  float3 n = geometric;
  if (gSurfaceNormal.x != 0.0 || gSurfaceNormal.y != 0.0) {
    float3 t;
    float3 b;
    tangentAxes(geometric, input.viewPos, input.uv, t, b);
    n = normalizeP(t * gSurfaceNormal.x + b * gSurfaceNormal.y +
                   geometric * gSurfaceNormal.z);
  }
  let v = normalizeP(-input.viewPos);
  let metal = saturateP(gSurfaceMetal);
  let exponent = gSurfaceGloss < 0.0 ? int(uShading.y) : int(gSurfaceGloss);
  // WHAT A METAL IS, in a model that has a Lambert term and a Blinn
  // highlight and nothing else: the light stops reaching the diffuse and
  // the highlight takes the surface's own colour. It is not a microfacet
  // BRDF and there is no Fresnel here to make one.
  let tint = mixP(float3(1.0, 1.0, 1.0), albedo, metal);

  float3 factor = uAmbient.rgb;
  float3 term = float3(0.0, 0.0, 0.0);
  let count = int(uShading.w);
  for (int i = 0; i < 4; ++i) {
    if (i >= count) break;
    let l = normalizeP(mul(uLightMatrix, float4(-uLightDir[i].xyz, 0.0)).xyz);
    let lambert = max(dotP(n, l), 0.0);
    factor = factor + uLightColor[i].rgb * (lambert * (1.0 - metal));
    if (uShading.x > 0.0 && lambert > 0.0) {
      let h = normalizeP(l + v);
      let blinn = powiP(max(dotP(n, h), 0.0), exponent) * uShading.x;
      term = term + uLightColor[i].rgb * blinn * tint;
    }
  }
  if (uShading.z > 0.0) {
    let facing = 1.0 - max(dotP(n, v), 0.0);
    let rim = facing * facing * facing * uShading.z;
    term = term + float3(rim, rim, rim);
  }
  outMul = factor;
  outAdd = term;
}
#endif

/** The shaded colour, PREMULTIPLIED — which is what a target holding
 *  premultiplied pixels must receive, and what the blend the renderer
 *  sets expects. */
float4 shade(float4 surface, VSOut input) {
  let straight = float4(surface.rgb * input.tint.rgb, surface.a * input.tint.a);
  float3 shadeMul = input.shadeMul;
  float3 shadeAdd = input.shadeAdd;
#ifdef SIGIL_LIT
  if (gSurfacePerPixel) shadeAgain(input, straight.rgb, shadeMul, shadeAdd);
#endif
  let shaded = saturateP(straight.rgb * shadeMul + shadeAdd);
  let mapUv = mul(uBaseColorMapUv, float4(input.uv, 0.0, 1.0)).xy;
  let map = uBaseColorMap.Sample(mapUv);
  let rgb = shaded * map.rgb;
  let a = saturateP(straight.a) * map.a;
  return float4(rgb * a, a);
}

[shader("fragment")]
float4 fsFlat(VSOut input) : SV_Target {
  return shade(surfaceFlat(input.uv), input);
}
