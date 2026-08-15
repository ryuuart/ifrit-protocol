// ONE definition of every compute-parameter block, seen by BOTH
// compilers: the Slang kernels include this and see float4 members;
// World.cpp includes it and sees float[4] members (identical layout,
// and every existing params.a[0] call site keeps compiling). This
// retires the layout-drift bug class the reflection-codegen idea was
// aimed at — with a preprocessor branch instead of a build pipeline.
// Adding a field here changes both sides in the same commit or
// neither.
#ifndef SIGIL_WORLD_SHADER_PARAMS_H
#define SIGIL_WORLD_SHADER_PARAMS_H

#ifdef __cplusplus
#define SIGIL_FLOAT4(name) float name[4]
namespace sigil::world::shaderparams {
#else
#define SIGIL_FLOAT4(name) float4 name
#endif

struct SweepParamsData {
  SIGIL_FLOAT4(window);  // x head, y span, z width, w sections
  SIGIL_FLOAT4(loop);    // x point count, y tangent epsilon
};

struct FlockParamsData {
  SIGIL_FLOAT4(windowA);  // x head, y span, z radius, w count
  SIGIL_FLOAT4(windowB);  // x scale, y noise amp, z noise freq, w seed
  SIGIL_FLOAT4(tintTail);
  SIGIL_FLOAT4(tintHead);
  SIGIL_FLOAT4(loop);  // x point count, y tangent epsilon
};

struct PopParamsData {
  SIGIL_FLOAT4(a);
  SIGIL_FLOAT4(b);
  SIGIL_FLOAT4(c);
  SIGIL_FLOAT4(d);  // x count, y lane slot, z loop point count, w eps
  SIGIL_FLOAT4(e);  // Transform: matrix column 0; Deform: axis
  SIGIL_FLOAT4(f);  // Transform: matrix column 1; Deform: origin
  SIGIL_FLOAT4(g);  // Transform: matrix column 2; Deform: direction
  SIGIL_FLOAT4(h);  // Transform: matrix column 3
  SIGIL_FLOAT4(m);  // x mask slot (-1 = none), yzw unused
};

#ifdef __cplusplus
}  // namespace sigil::world::shaderparams
static_assert(sizeof(sigil::world::shaderparams::SweepParamsData) == 32,
              "cbuffer packing is 16-byte rows");
static_assert(sizeof(sigil::world::shaderparams::FlockParamsData) == 80,
              "cbuffer packing is 16-byte rows");
static_assert(sizeof(sigil::world::shaderparams::PopParamsData) == 144,
              "cbuffer packing is 16-byte rows");
#endif

#undef SIGIL_FLOAT4
#endif  // SIGIL_WORLD_SHADER_PARAMS_H
