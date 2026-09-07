# SigilCore — the comparable leaf

The chapter on what a value needs before anything can decide it did not
change: type erasure that keeps its equality, so a set of operations can
ride on a value and two holders can still ask whether they carry the same
one; and the field pin, which fails the build when a struct grows a member
a hand-written comparator does not mention. `README.md` beside this file
is the library; `RECONCILE.md` and `CACHE.md` are the two kernels that ask
the question this leaf answers, and `COMPUTE.md` is the other leaf that
links nothing at all.

| header | holds |
|--------|-------|
| `comparable/Erased.h` | `Erased<Ops>` — comparable type erasure: a set of operations carried on the value that implements them |
| `comparable/Fields.h` | `kFieldCount<T>` — how many direct non-static data members an aggregate has, and the pin a hand-written comparator sits under |

`<sigilcore/comparable/Comparable.h>` includes both.

## A comparable value carries its own equality

`Erased<Ops>` holds a model behind an abstract interface, and copies of
one value are equal by their shared state; two separately built values are
equal when they hold the same model type and that type's `==` says so. A
model with no `==` is the escape hatch and compares equal to nothing but
its own copies — conservative on purpose, because a value that cannot
answer must never answer "unchanged". That is what lets a seam — which
executor draws, which resolver runs, which painter paints — ride on a
description that something else compares.

## A hand-written comparator forgets silently

Leave a field out and two different values compare equal, the holder
concludes nothing changed, and it keeps producing what the old value
produced for as long as it lives. Nothing detects that from outside,
because the wrong answer looks exactly like a value that really did not
change. `kFieldCount<T>` reads the member count off the type, so
`static_assert(kFieldCount<T> == N)` beside the comparator turns adding a
field into a build failure that names the comparator to go and fix.
