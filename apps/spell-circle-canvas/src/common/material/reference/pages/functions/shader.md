---
kind: function
library: SigilMaterial
name: shader
qualified: sigil::material::shader
group: The shader
status: stable
---

# shader

A shader as a material: its source and a struct whose fields are its
uniforms, in one line.

## Make one

| Spelling | Language | What it is |
|---|---|---|
| `material::shader(source, Parameters{…})` | C++ | the body over the struct's fields |
| `material::shader(source)` | C++ | a body with no parameters of its own |
| `material::shader(source, Parameters{…}, options)` | C++ | the same with a `ShaderOptions` last |
| `material::shader(hub, uri, Parameters{…})` | C++ | the body kept in a file, read through SigilIO |
| `material::placeholder()` | C++ | what a file with no program that compiled paints |
| `material.shader(source, {"heat": 0.6})` | Python | the parameters as a dict or a NamedTuple, in order |
| `material.shader(hub, uri, parameters)` | Python | the file form |

## Description

The body alone is written; the declarations are generated. In SkSL a body
is `half4 main(float2 p)` returning premultiplied colour, in Slang
`float4 surface(float2 uv)` returning straight colour. Before it stand one
uniform per field of the parameter struct, one sampled texture per name
the options list, and each of `uTime`, `uResolution`, `uContentScale` and
`uWorld` the body spells.

The answer is a `Material` like any other — a base, a layer source, a
mask — and its fields are written with `set(name, value)` and followed with
`bind(name, animatable)`, exactly as a Substance graph's inputs are. The
struct holds plain values, because its memory is the upload.

One source is one definition: the source, the key, the language, the
parameter struct and the texture names together identify it, so
describing the same shader again answers an equal material and a node
over it prunes, and a differing source is another definition. With no
`key` the name a message calls it is derived from the source.

## A file, live-coded

`material::shader(hub, uri, Parameters{…})` reads the file as the hub
holds it on every call, so an edit the hub's poll has seen compiles anew
on the next describe. A text that does not compile never replaces one
that did: the newest text that compiled keeps painting. While none has —
the file is missing, its first text is broken, its extension names no
language — the material is `material::placeholder()`, a magenta and black
checker sixteen pixels a cell: a diagnostic, never a look. What is wrong
stands on the hub's `problems()` under the URI, with the compiler's
message and the body's line, until a text compiles. A text is judged once
a compiler for its language is registered, and drawn as it stands before
that.

## The options

`ShaderOptions`: `key` (the name in messages; empty derives one),
`target` (the language; unset reads a text as SkSL and a file by its
extension), `sampling` (how every texture is read between its pixels;
unset reads linearly) and `textures` (each a `ShaderTexture`, a name the
body samples and the pixels behind it; empty pixels declare the name for
`Material::slot` to fill).

## See also

- `program/Shader.h` — the header: `shader`, `placeholder`,
  `ShaderOptions`, `ShaderTexture`
- [Material](../types/Material.md) — what the answer is
- The program model a shader is built from — a recipe, its program and
  the cache — is the library's `ADVANCED.md`.
