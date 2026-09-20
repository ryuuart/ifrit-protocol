---
kind: type
library: SigilCore
name: Provide
qualified: sigil::core::environment::Provide
group: Reconcile
status: stable
---

# Provide

An INHERITED VALUE, read where a component is described: a value bound
for a scope of the description tree and read by anything described
inside it, without being threaded through every call between.

A describe phase is an ordinary C++ call tree, evaluated eagerly and
bottom-up: `box().children({panel()})` calls `panel()` before the box
exists, and every component is a plain function whose arguments are
evaluated inside the enclosing scope. So the describe-time call stack IS
the description tree, and the C++ answer to "inherit down a call stack"
is dynamic scope:

```cpp
environment::Provide<Palette> theme(dark);      // binds for this scope
return box().children({panel()});      // panel() reads it

// …four levels down, in a component that was never handed it:
const Palette *p = environment::inherited<Palette>();
```

## Why this does not cost the prune

An inherited value is read DURING DESCRIBE and lands in the reading
node's own description, so the reconciler's structural comparison is
already an exact dependency tracker: a node whose description came out
identical prunes, whether or not it read the environment, and a node
whose value actually moved re-patches. The description tree the
reconciler sees is environment-INDEPENDENT — the value is baked in by
then — so no phase learns a new concept and nothing invalidates a
subtree wholesale.

## The one place the kernel had to learn it

The memo, the only site where a component function runs AFTER the
author's scope has ended. A memo therefore captures the ambient stack at
construction, compares it alongside its properties, and re-establishes
it around the deferred call — so a memo stays a pure function of
(properties, environment) and cannot serve a stale value. Anything else
that takes a callable and runs it later runs with NO scope: capture what
such a lambda needs by value at the call site, which is where the scope
still exists.

## Requirements on an inherited type

It is copyable and equality-comparable, and two values are equal exactly
when describing anything under them yields descriptions that compare
equal. The comparison is therefore structural and exact — never
perceptual and never epsilon'd, because the consumer of the answer is
the prune.

MATERIALISE DERIVED VALUES INTO THE TYPE. A value that carries a
`std::function` derivation rule instead of the results it produces is
incomparable, so it never compares equal to itself and every memo below
it becomes a permanent miss. Run the function once and store the
results.

Bindings are keyed by C++ TYPE, so this is a transport channel rather
than a design-token vocabulary: the key a component uses is its own
properties type.

## Unbinding

The destructor unbinds THIS scope's binding and no other. Destroying
providers out of LIFO order is misuse; when it happens, the destructor
locates its own entry by the held value's identity and removes exactly
that one — an unconditional pop would unbind a SIBLING that is still
alive. The misuse warns, unconditionally and with no switch: a scope
unbound out of order leaves the stack holding a binding nobody can name,
so the one line it prints is the only sign a sweep gets that the process
is wrong; a run that prints nothing is a run where the scopes nested.
The well-nested path stays a compare and a pop_back, allocation-free.

## See also

- `reconcile/Environment.h` — the header: `Provide`, `inherited`
- `RECONCILE.md` — the chapter the reconciler's phases are described in
