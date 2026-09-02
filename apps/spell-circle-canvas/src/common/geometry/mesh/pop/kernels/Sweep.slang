// THE SWEPT RING VERTEX, written once.
//
// One source, compiled twice by the build: to C++, which the host
// executor calls, and to SPIR-V, which a device executor dispatches. The
// two are the same arithmetic because they are the same text, and that
// is the whole reason this file exists — a formula written twice is two
// formulas, and the day they disagree is the day nobody can say which
// one the picture came from.
//
// WHAT IS IN HERE is the whole of what a sweep computes: one vertex is a
// pure function of one rail frame, one profile point and the size that
// ring scales its profile by. Everything else a sweep is made of is
// integer or a reduction over triangles that do not exist until the
// vertices do — which quad joins four of them, the fan that closes an
// end, the averaging that forms a geometric normal — and none of it is
// arithmetic two backends could disagree about, so none of it is here.
//
// THE TAPER IS NOT HERE EITHER. It is an arbitrary function of t
// supplied by whoever asked for the sweep, so it is evaluated once per
// ring on the host and arrives as the number a ring scales by.
//
// THE PORTABLE SUBSET. Every operation below is arithmetic or a
// conversion IEEE 754 pins exactly, so both targets answer alike as
// written; no library intrinsic is called. The float model is set
// downstream — `-ffp-contract=off` on the generated C++, and one
// `NoContraction` decoration per arithmetic result in the SPIR-V,
// because the emitter puts none there and a driver is otherwise free to
// fuse a multiply-add and round once where the source rounds twice.

/** ONE SWEEP'S PARAMETERS. Every member is a four-component vector, so
 *  the block's bytes stand at the same offsets in a uniform buffer as
 *  they do in the struct the host fills — no layout has to be reported
 *  or guessed. */
struct SweepArgs {
  /** x: how many rings; y: how many points the profile has; z: 1 when a
   *  vertex normal is the profile's own offset and 0 when it is the
   *  rail's; w: how many spans u is divided across. */
  uint4 code;
};

uniform SweepArgs args;

// THE RAIL, one buffer per component rather than one of a struct: every
// binding is four floats wide, which is what makes a host's array of
// vectors and a device's structured buffer the same bytes. Every one is
// writable so that the bindings a dispatch takes are one kind of
// resource in one state.
/** Per ring: xyz the frame's position, w the size the profile scales
 *  by there. */
RWStructuredBuffer<float4> railPosition;
/** Per ring: xyz the frame's normal, w the frame's t. */
RWStructuredBuffer<float4> railNormal;
/** Per ring: xyz the frame's binormal. */
RWStructuredBuffer<float4> railBinormal;
/** Per profile point: xy the contour point, in the profile's own 2D
 *  space — x along the binormal, y against the normal. */
RWStructuredBuffer<float4> profile;

// TWO LANES, NOT THREE. A position and a normal each spend three of
// their four floats, and a uv is two numbers; carrying it in the two
// spare ones costs nothing and saves a third of everything this kernel
// writes and everything a reader of it moves.
/** Per vertex: xyz the position, w the u across the profile. */
RWStructuredBuffer<float4> outPosition;
/** Per vertex: xyz the normal, w the v along the rail. */
RWStructuredBuffer<float4> outNormal;

/** ONE VERTEX: the profile point placed on its ring's frame, offset by
 *  the frame's own axes and scaled by that ring's size. The vertices
 *  are RING-MAJOR — ring 0's whole profile, then ring 1's — which is
 *  what lets one index name both without a second buffer saying so. */
void sweepVertex(uint i) {
  uint ring = args.code.y;
  if (ring == 0u) return;
  if (i >= args.code.x * ring) return;
  uint r = i / ring;
  uint k = i - r * ring;

  float3 normal = railNormal[r].xyz;
  float3 offset = railBinormal[r].xyz * profile[k].x - normal * profile[k].y;
  outPosition[i] = float4(railPosition[r].xyz + offset * railPosition[r].w,
                          float(k) / float(args.code.w));
  outNormal[i] =
      float4(args.code.z != 0u ? offset : normal, railNormal[r].w);
}

/** ONE ENTRY POINT: one dispatch forms every vertex of one sweep, so
 *  there is one pipeline, one binding layout and one generated
 *  function. */
[shader("compute")]
[numthreads(64, 1, 1)]
void sigilSweepKernel(uint3 tid : SV_DispatchThreadID) { sweepVertex(tid.x); }
