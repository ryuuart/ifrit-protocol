# SigilCore — the callable leaf

The chapter on a callable called with the parameters it named. A verb that
hands a caller several things — a canvas and the context it is painted in, a
frame delta and the clock behind it, a pen and the dab it is going down,
the box a shape is drawn into — cannot know which of them any one caller
reads, and a signature that demands all of them makes every caller that
reads one spell the rest to ignore them. This leaf is the rule that lets
them go: the parameters a callable names are the FIRST of what the verb
offers, and a caller drops the ones it does not read from the END.
`README.md` beside this file is the library.

| header | holds |
|--------|-------|
| `callable/Callable.h` | `namedParameters` — how many of a signature's parameters a callable named, or -1; `PrefixCallable` — the concept over it; `callPrefix` — the call, for a verb that erases its callable itself; `Callable<Signature>` — the copyable, type-erased call a verb holds one in |

## A prefix, never a subset

Given `void(SkCanvas&, const PaintContext&)`, three callables are that
signature's: one naming both parameters, one naming the canvas alone, and
one naming neither. A callable naming the context alone is NOT — there is no
way to hand it what it asks for without also deciding what the first
parameter meant, and a rule that guessed by type would silently rebind the
day a verb grew a second parameter of the same type. So the search walks
DOWN from the full offer and stops at the longest prefix the callable
accepts, which is what `namedParameters` answers and `PrefixCallable`
asks. A callable that takes several prefixes — a generic one over
`auto&&...` — is called with the longest.

The signature's result decides what the call must answer, with `void`
accepting any answer, which is what `std::is_invocable_r_v` already means
by it. That is what lets a verb whose callable may answer nothing OR a
verdict — a steppable that returns whether it still needs frames — search
once and read the answer afterwards.

## The unnamed parameters are still built

What the verb offers is the verb's to build: dropping a parameter drops the
READING of it, not the work behind it. A caller that wants the work not to
happen has to ask the verb for that, not leave an argument out.

## Held, not compared

`Callable<Signature>` holds what a `std::function` holds — one call, copied
with the value, false while empty — and nothing more. A call is not a
value: two of them never compare equal, so whatever prunes on a description
carrying one keys on something else, a scheme's own equality or a key the
caller states beside the callable. A verb that already erases its callable
into a store of its own — a vector of steppables, a field spelled as a
`std::function` — takes the same rule through `callPrefix` and keeps its
store.
