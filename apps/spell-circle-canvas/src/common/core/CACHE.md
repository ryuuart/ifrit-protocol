# SigilCore — the caching proof

The chapter on the second kernel: what a host may keep between frames.
Given what a host declares about one node and what its children answered,
it decides whether a subtree is provably static for the frame, whether a
value memo may hold it, and whether the artefact in hand should be baked,
replayed or thrown away. `README.md` beside this file is the library;
`RECONCILE.md` is the kernel that drives the passes this one caches across,
and `COMPARABLE.md` is the leaf the bake seam's erased value comes from.

| header | holds |
|--------|-------|
| `cache/Policy.h` | `Cache` — the three-valued cache policy: `Auto`, `Always`, `Never` |
| `cache/Volatility.h` | `NodeVolatility`, `SubtreeVerdict`, `ChildVolatility` and `foldSubtree` — the settled-subtree proof |
| `cache/Settle.h` | `Settle<Values>` — the stability release: `observe`, `release`, `moved`, `frames`, `held`, `restart` |
| `cache/Bake.h` | `BakeOperations<Target>`, `Bake<Target>`, `BakeState`, `BakeAction`, `decideBake`, `runBake` — the bake seam |
| `cache/Rebuild.h` | `RebuildGuard<Keys...>`, `CachedValue<Value, Keys...>` and `quantizeKey` — rebuild on change, with the key one declared tuple rather than a row of members and an if |

`<sigilcore/cache/Cache.h>` includes all five.

## What a host reports is a DECLARATION, never a difference

`foldSubtree` takes one `NodeVolatility` — the `Cache` policy the author
stated (`Auto`, `Always`, `Never`), whether they asked for the subtree to
be held by a value memo, and five facts about what this node does off the
describe clock — and the `ChildVolatility` its children folded into. It
answers a `SubtreeVerdict`. A host that reports a term one frame late has
already replayed an artefact of a frame that has changed; a host that
reports one early pays a re-bake and nothing else. When in doubt, declare.

**The five facts are five different questions.** `ownPaint` is motion that
changes how the node COMPOSITES without changing what it draws — the node's
own artefact still replays, because it is drawn through the motion, while
an ancestor's would contain it. `ownContent` rebuilds what the node draws,
and blocks the node's artefact and every ancestor's. `memoOpaque` is
volatility no value comparison can SEE, which is the one term a value memo
turns on. `readsBackdrop` is not volatility at all: the node composites
against what is already on the canvas, so it cannot be inside a bake, whose
ground is transparent black. `samplesDestination` is the half of that which
refuses to be the bake's ROOT too — a root's blend and opacity are applied
outside its bake, a destination-sampling filter inside it.

**The promise is one-sided.** A subtree the proof calls settled cannot
change its pixels without the host being told first. A subtree it calls
volatile may well be standing perfectly still — proving THAT is the value
memo's job, and `Settle` is what makes it sound.

## A binding cannot say that it stopped

It stays connected for the life of the node it drives, so an entrance that
played once declares exactly what a loop declares. `Settle<Values>`
separates them by observation: `observe` counts consecutive frames on which
the values a node draws from resolved identically, `release` converts a
warmed-up count into the verdict, and `moved` — run once per draw over what
was released — re-declares the frame a value assigned from OUTSIDE moves
again. All three sides are required. Skipping one does not fail loudly; it
replays a frame that has already changed.

## The decision is the kernel's, the artefact is the host's

`BakeOperations` names four operations over a `Target` the kernel never looks
inside: take the bake, replay it, drop it, and say whether one is held.
`decideBake` answers `Live`, `Take` or `Replay` from three facts. A host
with several tiers — a recorded command list, a rasterized image, a whole
subtree composited into one layer — writes one model per tier and asks the
same question of all of them, which is what stops each tier from growing
its own copy of the rule.

## Rebuild on change

`RebuildGuard<Keys...>` and `CachedValue<Value, Keys...>` are the
"compare the inputs, rebuild when they differ" cache an animated consumer
otherwise hand-rolls, with the key one declared tuple rather than a row of
`m_last*` members and an if-condition that has to stay in step with them.
`quantizeKey` is the one arithmetic such a key wants: an input that drifts
by a fraction per frame is snapped, so most frames pose the same problem
and hit. Nothing here knows what is being rebuilt.
