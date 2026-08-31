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
};

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

/** The shaded colour, PREMULTIPLIED — which is what a target holding
 *  premultiplied pixels must receive, and what the blend the renderer
 *  sets expects. */
float4 shade(float4 surface, VSOut input) {
  let straight = float4(surface.rgb * input.tint.rgb, surface.a * input.tint.a);
  let shaded = saturateP(straight.rgb * input.shadeMul + input.shadeAdd);
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
