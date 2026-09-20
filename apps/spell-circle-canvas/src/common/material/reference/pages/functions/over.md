---
kind: function
library: SigilMaterial
name: over
qualified: sigil::material::over
group: The combinator
status: stable
---

# over

Stacking one material over another through a mask — the combinator that
makes local variation (rust over steel, dirt in the crevices) a
composition of materials rather than a bespoke recipe per pair.

## Description

A MASK is an ordinary material whose output is read as a scalar: its red
channel, clamped to the unit range, says how much of the top material
shows at that point. `over` returns a material like any other, so a
stack is built by applying it again, and every query — animated,
geometry-dependent, equality — answers over the whole stack, because the
operands are its children.

`amount` is how strongly the top shows where the mask is fully on: the
stack's own strength, which is a different question from where it
applies and is why it is on the signature rather than folded into the
mask. It is on the signature at all because a stack composed from its
operands has no parameter struct to write afterwards — its ABI is its
operands' fields, so setting `amount` on the result is a per-field write
a caller has to know to make, and a caller who does not make it gets a
stack at full strength that reads as a wrong mask.

`Blend` is how the top's output combines with the one beneath it where
the mask says: `Blend::Mix` moves the base toward the top by the mask,
`Blend::Add` adds the top scaled by the mask, and `Blend::Multiply`
moves the base toward base times top by the mask. `name` spells a blend
the way messages and a recipe name do. `OverParameters::amount` is the
uniform the combinator's recipes read.

## Two kinds of target read a stack

Only one of them can reach the operands.

A target whose slot is a SHADER — SkSL's is — samples each operand's own
program, so one body over three slots is the whole story.
`overRecipe` is that combinator recipe, defined once per blend, each
declaring the child slots `base`, `top` and `mask`.

A target handed exactly ONE body per material cannot reach a child
material at all. For it a stack is COMPOSED: `over` builds a recipe out
of its operands' own definitions, whose parameters are theirs under a
prefix per operand, whose sampled slots are theirs, and whose body
inlines all three of their bodies and mixes what they return. The
composition costs one recipe and one program per distinct triple of
definitions, and buys nothing for a target that samples its operands, so
it is built only where a compiler that needs it is installed.

A stack composed and a stack not composed are the same material
otherwise: the same operands as children, the same walk down, the same
recipe NAME — which is what says a material is a stack, since a composed
one carries a recipe built for its own operands. `stackName` is that
name.

Where the stack is composed the operands' parameter values and their
sampled slots are copied into the result at the moment of the call, so a
later edit to one of them is not seen and a live binding on one of them
does not reach the composed body. The operand still rides every query as
a child, so the stack still reports itself animated.

## Reading a stack back

`under` is the material a stack stands on: the `base` child when the
argument is an `over` result, else the argument itself. Applied until
the answer is not a stack, it is the bottom. `stackDepth` is how many
materials are stacked over that bottom — zero for a material `over`
never combined.

## See also

- `core/Combine.h` — the header: `over`, `under`, `stackDepth`, `Blend`,
  `OverParameters`, `overRecipe`, `stackName`, `name`
- [Material](../types/Material.md) — what a stack is made of and answers
  as
