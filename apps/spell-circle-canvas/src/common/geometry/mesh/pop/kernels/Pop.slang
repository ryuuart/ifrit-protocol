// THE POINT OPERATORS, written once.
//
// One source, compiled twice by the build: to C++, which the host
// executor calls, and to SPIR-V, which a device executor dispatches. The
// two are the same arithmetic because they are the same text, and that
// is the whole reason this file exists — a formula written twice is two
// formulas, and the day they disagree is the day nobody can say which
// one the picture came from.
//
// WHAT IS IN HERE. The operators whose body is a pure function of ONE
// point: its index, its own lanes, and the operator's parameters.
// Everything else stays on the host — a generator seeds the lanes, a
// neighbourhood operator reads points it does not own, a permutation
// rewrites the order rather than the values, and a primitive operator
// addresses triangles that do not exist yet.
//
// THE PORTABLE SUBSET. Arithmetic and the operations IEEE 754 pins
// exactly are the same on both targets as written. Every library
// intrinsic whose two targets are two different pieces of code is
// written out here instead: a lerp, a dot, a length, a smoothstep. What
// is NOT written out is `sqrt`, which is correctly rounded on the C++
// target and through this driver's SPIR-V; a target whose own sqrt is an
// approximation would need its precise form named here.
//
// TRIGONOMETRY IS NOT IN THE SUBSET and so no operator whose definition
// calls for it is in this file. A polynomial sine is a different
// function from a library sine, not a rounding of it, and swapping one
// for the other would change what a chain cooks rather than where it
// cooks.
//
// The float model is set downstream: `-ffp-contract=off` on the
// generated C++, and nothing in the SPIR-V forbids a driver to fuse a
// multiply-add, so nothing here may depend on the unfused answer.

/** ONE DISPATCH'S PARAMETERS. Every member is a four-component vector,
 *  so the block's bytes stand at the same offsets in a uniform buffer as
 *  they do in the struct the host fills — no layout has to be reported
 *  or guessed. */
struct PopArgs {
  /** x: which operator; y: how many points; z: the operator's seed;
   *  w: flags, bit 0 meaning a mask lane is bound. */
  uint4 code;
  /** The operator's whole numbers, one meaning per operator. */
  uint4 nums;
  /** …and its real ones. */
  float4 a;
  float4 b;
  float4 c;
  /** An affine's four COLUMNS, in the order the host holds them. */
  float4 m0;
  float4 m1;
  float4 m2;
  float4 m3;
};

uniform PopArgs args;

// THE LANES A DISPATCH TOUCHES, one binding per role rather than one per
// name: an operator reads at most three lanes, a mask and a table, and
// which lane is bound to which role is the host's to say. Every one is
// writable so that a lane bound to two roles at once — the common case,
// an operator reading and writing one lane — is one resource in one
// state and not the same resource claimed two ways.
RWStructuredBuffer<float4> dst;
RWStructuredBuffer<float4> srcA;
RWStructuredBuffer<float4> srcB;
RWStructuredBuffer<float4> srcC;
RWStructuredBuffer<float4> mask;
RWStructuredBuffer<float4> table;

// ---- the portable subset this file uses ------------------------------

float saturateP(float x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

float lengthP(float3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

/** One PCG step and its output permutation, squeezed to [0, 1) through
 *  the 24 mantissa bits a float holds exactly — integer arithmetic
 *  throughout, and one division by a power of two, so the answer is the
 *  same on every target that has a 32-bit integer. */
float pcgUnit(uint x) {
  uint s = x * 747796405u + 2891336453u;
  uint m = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
  return float(((m >> 22u) ^ m) & 0x00FFFFFFu) / 16777216.0;
}

// ---- what every operator shares --------------------------------------

/** THE MASK EVERY FILTER TAKES: a lane's .x clamped to [0, 1], and 1
 *  when no lane is bound. A whole mask stores the new value outright
 *  rather than blending it against the old one, so a masked write and an
 *  unmasked one land on the same bits. */
float maskAt(uint i) {
  if ((args.code.w & 1u) == 0u) return 1.0;
  return saturateP(mask[i].x);
}

void storeMasked(uint i, float4 v) {
  float m = maskAt(i);
  if (m >= 1.0) {
    dst[i] = v;
    return;
  }
  float4 old = dst[i];
  dst[i] = old + (v - old) * m;
}

// ---- the operators, by the index their description carries ------------
//
// The number IS the operator's index among the descriptions the language
// declares, so one numbering serves the host and the device and a new
// operator appended there needs no second table here.

static const uint kJitter = 1u;
static const uint kRamp = 3u;
static const uint kVary = 4u;
static const uint kLookAt = 5u;
static const uint kMath = 6u;
static const uint kFill = 9u;
static const uint kAtlas = 10u;
static const uint kLookup = 12u;
static const uint kSelect = 14u;
static const uint kAffine = 15u;
static const uint kPeak = 16u;
static const uint kMix = 18u;

/** lane += a stable random cube offset per point. */
void opJitter(uint i) {
  uint seed = args.code.z;
  float amplitude = args.a.x;
  float4 v = srcA[i];
  v.x = v.x + (pcgUnit(i * 3u + seed) - 0.5) * 2.0 * amplitude;
  v.y = v.y + (pcgUnit(i * 5u + seed + 1u) - 0.5) * 2.0 * amplitude;
  v.z = v.z + (pcgUnit(i * 7u + seed + 2u) - 0.5) * 2.0 * amplitude;
  storeMasked(i, v);
}

/** lane = from + (to - from) * T. */
void opRamp(uint i) {
  float t = srcA[i].x;
  storeMasked(i, args.a + (args.b - args.a) * t);
}

/** lane = base * (1 + spread * (hash * 2 - 1)), across all four. */
void opVary(uint i) {
  float value = args.a.x * (1.0 + args.a.y *
                                      (pcgUnit(i * 11u + args.code.z) * 2.0 -
                                       1.0));
  storeMasked(i, float4(value, value, value, value));
}

/** Dir = normalize(target - P), keeping Dir's own .w. */
void opLookAt(uint i) {
  float4 p = srcA[i];
  float3 d = float3(args.a.x - p.x, args.a.y - p.y, args.a.z - p.z);
  float len = lengthP(d);
  float3 dir = len > 1e-6 ? d * (1.0 / len) : float3(0.0, 0.0, 1.0);
  float w = srcB[i].w;
  storeMasked(i, float4(dir.x, dir.y, dir.z, w));
}

/** lane = lane * mul + add, per component. */
void opMath(uint i) { storeMasked(i, srcA[i] * args.a + args.b); }

/** lane = a constant. */
void opFill(uint i) { storeMasked(i, args.a); }

/** Tex = the atlas cell this point drew. */
void opAtlas(uint i) {
  int cols = int(args.nums.x);
  int rows = int(args.nums.y);
  float du = 1.0 / float(cols);
  float dv = 1.0 / float(rows);
  int cells = cols * rows;
  if (cells < 1) cells = 1;
  int drawn = int(pcgUnit(i * 13u + args.code.z) * float(cells));
  int cell = drawn < cells - 1 ? drawn : cells - 1;
  int cellRow = cell / cols;
  storeMasked(i, float4(float(cell % cols) * du, float(cellRow) * dv, du, dv));
}

/** to = the stop table sampled at dot(from, weights), remapped from
 *  [low, high] onto the table's span. */
void opLookup(uint i) {
  int n = int(args.nums.x);
  // An empty table is a no-op, on this target and on the other: there is
  // nothing between no stops to interpolate.
  if (n < 1) return;
  float4 s = srcA[i];
  float key = s.x * args.a.x + s.y * args.a.y + s.z * args.a.z +
              s.w * args.a.w;
  float low = args.b.x;
  float denom = args.b.y - low;
  float u = denom != 0.0 ? (key - low) / denom : 0.0;
  u = saturateP(u);
  float x = u * float(n - 1);
  int i0 = int(floor(x));
  i0 = i0 < 0 ? 0 : (i0 > n - 1 ? n - 1 : i0);
  int i1 = i0 + 1 < n ? i0 + 1 : n - 1;
  float f = x - float(i0);
  float4 lo = table[i0];
  float4 hi = table[i1];
  storeMasked(i, lo + (hi - lo) * f);
}

/** A selection lane written from a region of space. It takes no mask:
 *  the lane it writes IS what a mask is made of. */
void opSelect(uint i) {
  float3 size = float3(args.b.x != 0.0 ? args.b.x : 1e-6,
                       args.b.y != 0.0 ? args.b.y : 1e-6,
                       args.b.z != 0.0 ? args.b.z : 1e-6);
  float feather = saturateP(args.c.x);
  float4 s = srcA[i];
  float3 q = float3((s.x - args.a.x) / size.x, (s.y - args.a.y) / size.y,
                    (s.z - args.a.z) / size.z);
  float d;
  if (args.nums.x == 0u) {
    d = sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
  } else {
    d = max(abs(q.x), max(abs(q.y), abs(q.z)));
  }
  float inside;
  if (feather <= 0.0) {
    inside = d <= 1.0 ? 1.0 : 0.0;
  } else {
    // The smoothstep written out, so both targets run the same three
    // multiplies rather than two library functions.
    float t = saturateP((d - (1.0 - feather)) / feather);
    inside = 1.0 - t * t * (3.0 - 2.0 * t);
  }
  if (args.nums.z != 0u) inside = 1.0 - inside;
  float old = dst[i].x;
  float v = inside;
  if (args.nums.y == 1u) v = max(old, inside);
  if (args.nums.y == 2u) v = min(old, inside);
  if (args.nums.y == 3u) v = old * (1.0 - inside);
  dst[i] = float4(v, v, v, v);
}

/** lane = matrix * lane, as a position or as a direction. The matrix is
 *  applied COLUMN by column and the four products summed in pairs,
 *  which is the order the host's own matrices are multiplied in. */
float4 applyAffine(float4 v, float w) {
  float4 mul0 = args.m0 * v.x;
  float4 mul1 = args.m1 * v.y;
  float4 add0 = mul0 + mul1;
  float4 mul2 = args.m2 * v.z;
  float4 mul3 = args.m3 * w;
  float4 add1 = mul2 + mul3;
  return add0 + add1;
}

void opAffine(uint i) {
  float4 v = srcA[i];
  float4 r;
  if (args.nums.x != 0u) {
    float4 t = applyAffine(v, 0.0);
    float len = lengthP(float3(t.x, t.y, t.z));
    r = len > 1e-6 ? float4(t.x / len, t.y / len, t.z / len, v.w)
                   : float4(v.x, v.y, v.z, v.w);
  } else {
    float4 t = applyAffine(v, 1.0);
    r = float4(t.x, t.y, t.z, v.w);
  }
  storeMasked(i, r);
}

/** lane.xyz += normalize(along.xyz) * distance. */
void opPeak(uint i) {
  float4 along = srcA[i];
  float len = lengthP(float3(along.x, along.y, along.z));
  float4 v = dst[i];
  if (len > 1e-6) {
    float k = args.a.x / len;
    v.x = v.x + along.x * k;
    v.y = v.y + along.y * k;
    v.z = v.z + along.z * k;
  }
  storeMasked(i, v);
}

/** to = a + (b - a) * factor, the factor a constant or a lane's .x. */
void opMix(uint i) {
  float4 va = srcA[i];
  float4 vb = srcB[i];
  float f = args.nums.x != 0u ? srcC[i].x : args.a.x;
  storeMasked(i, va + (vb - va) * f);
}

void popPoint(uint i) {
  if (i >= args.code.y) return;
  uint op = args.code.x;
  if (op == kJitter) {
    opJitter(i);
  } else if (op == kRamp) {
    opRamp(i);
  } else if (op == kVary) {
    opVary(i);
  } else if (op == kLookAt) {
    opLookAt(i);
  } else if (op == kMath) {
    opMath(i);
  } else if (op == kFill) {
    opFill(i);
  } else if (op == kAtlas) {
    opAtlas(i);
  } else if (op == kLookup) {
    opLookup(i);
  } else if (op == kSelect) {
    opSelect(i);
  } else if (op == kAffine) {
    opAffine(i);
  } else if (op == kPeak) {
    opPeak(i);
  } else if (op == kMix) {
    opMix(i);
  }
}

/** ONE ENTRY POINT for every operator, the operator chosen by a uniform.
 *  A dispatch runs one operator over every point, so the branch is the
 *  same on every lane of it — and one entry point is one pipeline, one
 *  binding layout and one generated function, where twelve would be
 *  twelve of each for no arithmetic anyone could tell apart. */
[shader("compute")]
[numthreads(64, 1, 1)]
void sigilPopKernel(uint3 tid : SV_DispatchThreadID) { popPoint(tid.x); }
