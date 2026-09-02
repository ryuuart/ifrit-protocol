// THE STAMPED VERTEX, written once.
//
// One source, compiled twice by the build: to C++, which the host
// executor calls, and to SPIR-V, which a device executor dispatches. The
// two are the same arithmetic because they are the same text, and that
// is the whole reason this file exists — a formula written twice is two
// formulas, and the day they disagree is the day nobody can say which
// one the picture came from.
//
// WHAT IS IN HERE is the whole of what stamping computes: one output
// vertex is a pure function of one point's lanes and one vertex of the
// stamp. Everything else is integer — which stamp vertex an index names,
// and the run of triangle indices each point contributes — so none of it
// is here.
//
// THE PORTABLE SUBSET. Arithmetic and the operations IEEE 754 pins
// exactly, plus `sqrt`, which is correctly rounded on the C++ target and
// through this driver's SPIR-V. Every library intrinsic whose two
// targets are two different pieces of code is written out instead. The
// float model is set downstream — `-ffp-contract=off` on the generated
// C++, and one `NoContraction` decoration per arithmetic result in the
// SPIR-V, because the emitter puts none there and a driver is otherwise
// free to fuse a multiply-add and round once where the source rounds
// twice.

/** ONE STAMPING'S PARAMETERS. Every member is a four-component vector,
 *  so the block's bytes stand at the same offsets in a uniform buffer as
 *  they do in the struct the host fills — no layout has to be reported
 *  or guessed. */
struct StampArgs {
  /** x: how many vertices one stamp has; y: how many points; z: flags,
   *  bit 0 meaning a direction lane orients the stamp; w: how many
   *  vertices in all. */
  uint4 code;
  /** xyz the vector the orientation basis is seeded from. */
  float4 up;
};

uniform StampArgs args;

// THE STAMP, one buffer per lane rather than one of a struct: every
// binding is four floats wide, which is what makes a host's array of
// vectors and a device's structured buffer the same bytes. Every one is
// writable so that the bindings a dispatch takes are one kind of
// resource in one state.
/** Per stamp vertex: xyz the position in the stamp's own space. */
RWStructuredBuffer<float4> stampPosition;
/** Per stamp vertex: xyz the normal, or zero where the stamp has none. */
RWStructuredBuffer<float4> stampNormal;
/** Per stamp vertex: xy the uv, or zero where the stamp has none. */
RWStructuredBuffer<float4> stampUv;
/** Per stamp vertex: the colour, white where the stamp has none. */
RWStructuredBuffer<float4> stampColor;

// THE POINTS, laid out so that a lane the cloud does not carry costs no
// branch: the host fills the absent lane with what it would have been
// read as, once, rather than the kernel asking per vertex.
/** Per point: xyz where it stands, w the size its stamp scales by. */
RWStructuredBuffer<float4> pointOrigin;
/** Per point: xyz the direction its stamp faces. */
RWStructuredBuffer<float4> pointDir;
/** Per point: the tint its stamp is coloured by. */
RWStructuredBuffer<float4> pointColor;
/** Per point: the texture window {uOffset, vOffset, uScale, vScale} its
 *  stamp's uvs are remapped through — the identity where there is
 *  none. */
RWStructuredBuffer<float4> pointTex;

// TWO LANES CARRY THREE. A position and a normal each spend three of
// their four floats and a uv is two numbers, so the uv rides in the two
// spare ones and nothing is written twice.
/** Per vertex: xyz the position, w the u. */
RWStructuredBuffer<float4> outPosition;
/** Per vertex: xyz the normal, w the v. */
RWStructuredBuffer<float4> outNormal;
/** Per vertex: the colour. */
RWStructuredBuffer<float4> outColor;

// ---- the portable subset this file uses ------------------------------

float dotP(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float3 crossP(float3 a, float3 b) {
  return float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

/** Unit vector along v, or the fallback where v is too short to have a
 *  direction — the case a library normalize leaves undefined, and the
 *  same threshold the host's own policy uses. */
float3 normalizedP(float3 v, float3 fallback) {
  float len = sqrt(dotP(v, v));
  return len < 1e-12 ? fallback : v * (1.0 / len);
}

/** ONE VERTEX: a vertex of the stamp placed on one point's orientation
 *  basis, scaled by that point's size and moved to where it stands. The
 *  vertices are POINT-MAJOR — point 0's whole stamp, then point 1's —
 *  which is what lets the triangle indices be a run per point and what
 *  makes a primitive lane a run of triangles sharing one owner. */
void stampVertex(uint i) {
  uint verts = args.code.x;
  if (verts == 0u) return;
  if (i >= args.code.w) return;
  uint p = i / verts;
  uint v = i - p * verts;

  float4 origin = pointOrigin[p];

  // THE ORIENTATION BASIS, and it is the host's own policy written out:
  // +z along the direction, x across the seeded up, y completing it, and
  // a second seed where the two are parallel. A cloud must render
  // identically stamped on either tier, which is one basis and not two.
  float3 bx = float3(1.0, 0.0, 0.0);
  float3 by = float3(0.0, 1.0, 0.0);
  float3 bz = float3(0.0, 0.0, 1.0);
  if ((args.code.z & 1u) != 0u) {
    bz = normalizedP(pointDir[p].xyz, float3(0.0, 0.0, 1.0));
    float3 side = crossP(args.up.xyz, bz);
    if (dotP(side, side) < 1e-8) side = crossP(float3(1.0, 0.0, 0.0), bz);
    bx = normalizedP(side, float3(0.0, 0.0, 1.0));
    by = crossP(bz, bx);
  }

  float4 tex = pointTex[p];
  float2 uv = stampUv[v].xy;

  float3 sp = stampPosition[v].xyz;
  float3 placed = origin.xyz + (bx * sp.x + by * sp.y + bz * sp.z) * origin.w;
  outPosition[i] = float4(placed, tex.x + uv.x * tex.z);

  // The normal rides the basis and is NOT renormalized: the stamp's own
  // normal is what a caller authored, and a basis of unit vectors does
  // not change its length.
  float3 sn = stampNormal[v].xyz;
  outNormal[i] =
      float4(bx * sn.x + by * sn.y + bz * sn.z, tex.y + uv.y * tex.w);

  outColor[i] = pointColor[p] * stampColor[v];
}

/** ONE ENTRY POINT: one dispatch forms every vertex of one stamping, so
 *  there is one pipeline, one binding layout and one generated
 *  function. */
[shader("compute")]
[numthreads(64, 1, 1)]
void sigilStampKernel(uint3 tid : SV_DispatchThreadID) { stampVertex(tid.x); }
