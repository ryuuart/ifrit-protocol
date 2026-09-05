// THE MESH PAINTER on a device: what `render::MeshStyle` says, said in
// a pipeline.
//
// This is not the material scaffold. A painter's draw has no material
// and no body to call: everything it shades with is a field of the style
// the caller handed over — the mode, the colour, the lights and their
// terms, the texture and its placement — so one program answers every
// draw and the mode is a uniform rather than a specialisation.
//
// SHADING IS PER VERTEX, in view space, and it is the same three modes
// the host executor has:
//
//   Lit      ambient + Lambert per emitter, scaling the surface colour,
//            plus Blinn specular and a rim term that nothing scales
//   Normals  the view-space normal as a colour, +y DOWN so it matches
//            the canvas the host writes into
//   Uv       the mesh's own uv as a ramp
//
// The whole shaded colour is computed at the vertex and interpolated
// across the triangle, which is what the host does too — it shades each
// vertex and hands the three colours to the rasterizer.
//
// WHAT A HOST DOES AND A DEVICE CANNOT, and the other way about: the
// host sorts triangles back to front and antialiases their edges; this
// depth-tests them and does not. Those are the two differences a parity
// test measures, and neither is a shading disagreement.

import Portable;
import Shading;

// ---- what one draw supplies ----------------------------------------
// Every one of these is a global `uniform`, so they land in one uniform
// buffer whose member offsets the compiler reports; a renderer writes
// its bytes at those offsets and never at guessed ones.

/** The camera's view-projection in CLIP space at the target's size. */
uniform float4x4 uViewProj;
/** The mesh's world placement. */
uniform float4x4 uModel;
/** view * model, for the space the shading is written in. */
uniform float4x4 uModelView;
/** The inverse transpose of view * model, its 3x3 in the upper left. */
uniform float4x4 uNormalMatrix;
/** The inverse transpose of view, which carries a world-space light
 *  direction into the space the shading is written in. */
uniform float4x4 uLightMatrix;
/** The style's base colour, straight alpha. */
uniform float4 uBaseColor;
/** The light every surface receives before any emitter reaches it. */
uniform float4 uAmbient;
/** Per emitter: the direction it TRAVELS in world space, and its colour
 *  already scaled by its intensity. `uShading.w` says how many of the
 *  four are lit. */
uniform float4 uLightDir[8];
uniform float4 uLightColor[8];
/** x: specular strength; y: specular exponent; z: rim strength;
 *  w: how many emitters. */
uniform float4 uShading;
/** x: THE EXPOSURE the whole picture is read at — what the lit sum is
 *  multiplied by before the tone curve compresses it. */
uniform float4 uTone;
/** x: 0 lit, 1 the normal buffer, 2 the uv ramp; y: 1 when the emitters
 *  reach this surface at all — a surface that is its own light shows its
 *  colour and its tint and nothing else. */
uniform float4 uMode;
/** Where the texture is read at, over the mesh's own uv coordinates:
 *  the identity reads the image once across them. */
uniform float4x4 uTextureUv;
/** The texture the shaded colour is modulated by. A draw with none reads
 *  one white texel, so a colour multiplied by a map it does not have is
 *  that colour. */
uniform Sampler2D uTexture;

struct VSIn {
  float3 position : ATTRIB0;
  float3 normal : ATTRIB1;
  float2 uv : ATTRIB2;
  float4 color : ATTRIB3;
  /** The PRIMITIVE lane: one value per triangle, carried on each of its
   *  three vertices because a triangle has nowhere else to hold one. It
   *  is 1 wherever no lane was named. */
  float4 prim : ATTRIB4;
};

struct VSOut {
  float4 position : SV_Position;
  /** The whole shaded colour, straight alpha, already saturated. */
  float4 color : COLOR0;
  float4 prim : COLOR1;
  float2 uv : TEXCOORD0;
};

/** The three modes, at one vertex. */
float4 shadeVertex(VSIn input) {
  let n = normalizeP(mul(uNormalMatrix, float4(input.normal, 0.0)).xyz);

  if (uMode.x > 1.5) {
    // The uv ramp: the mesh's own coordinates, and a constant blue so a
    // flat region still reads as a colour.
    return float4(input.uv.x, input.uv.y, 0.5, 1.0);
  }
  if (uMode.x > 0.5) {
    // The normal buffer: view-space, +y DOWN — the convention a surface
    // recipe reads, and the one the canvas beneath it counts in.
    return float4(n.x * 0.5 + 0.5, -n.y * 0.5 + 0.5, n.z * 0.5 + 0.5, 1.0);
  }

  // Lit. The mesh's own tint lane multiplies the surface colour, the
  // way a per-vertex instance tint does, and does not reach the
  // specular or the rim.
  float3 base = uBaseColor.rgb * input.color.rgb;
  float alpha = uBaseColor.a * input.color.a;
  // A SURFACE THAT IS ITS OWN LIGHT stops here: no ambient under it and
  // no emitter, specular or rim over it.
  if (uMode.y < 0.5) return float4(saturateP(base), saturateP(alpha));

  let viewPos = mul(uModelView, float4(input.position, 1.0)).xyz;
  let v = normalizeP(-viewPos);
  float3 accum = uAmbient.rgb * base;
  let count = int(uShading.w);
  for (int i = 0; i < 8; ++i) {
    if (i >= count) break;
    let l = normalizeP(mul(uLightMatrix, float4(-uLightDir[i].xyz, 0.0)).xyz);
    let lambert = max(dotP(n, l), 0.0);
    accum = accum + base * uLightColor[i].rgb * lambert;
    if (uShading.x > 0.0 && lambert > 0.0) {
      let h = normalizeP(l + v);
      let blinn = powiP(max(dotP(n, h), 0.0), int(uShading.y)) * uShading.x;
      accum = accum + uLightColor[i].rgb * blinn;
    }
  }
  if (uShading.z > 0.0) {
    let facing = 1.0 - max(dotP(n, v), 0.0);
    accum = accum + facing * facing * facing * uShading.z;
  }
  // WHERE THE LIT SUM ENDS: a radiance, read at the set's exposure and
  // compressed onto what a display can hold rather than cut off at one.
  // The two buffer modes and a surface that is its own light returned
  // above; none of the three is a radiance.
  return float4(toneMap(accum, uTone.x), saturateP(alpha));
}

[shader("vertex")]
VSOut vsPaint(VSIn input) {
  VSOut out;
  out.position = mul(uViewProj, mul(uModel, float4(input.position, 1.0)));
  out.uv = input.uv;
  out.color = shadeVertex(input);
  // The primitive tint multiplies the SHADED colour, which is where the
  // host multiplies it too — into the vertex colour it has already
  // finished shading. Only the lit mode carries one: the two buffers
  // would be corrupted by a tint.
  out.prim = uMode.x > 0.5 ? float4(1.0, 1.0, 1.0, 1.0) : input.prim;
  return out;
}

/** The shaded colour, PREMULTIPLIED — which is what a target holding
 *  premultiplied pixels must receive, and what the blend the renderer
 *  sets expects. */
[shader("fragment")]
float4 fsPaint(VSOut input) : SV_Target {
  let tinted = float4(input.color.rgb * input.prim.rgb,
                      input.color.a * input.prim.a);
  let uv = mul(uTextureUv, float4(input.uv, 0.0, 1.0)).xy;
  let map = uTexture.Sample(uv);
  let rgb = tinted.rgb * map.rgb;
  let a = saturateP(tinted.a) * map.a;
  return float4(rgb * a, a);
}
