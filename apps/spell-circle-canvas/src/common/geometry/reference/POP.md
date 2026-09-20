# SigilGeometry — the point operators

The chapter on what a point-operator chain is made of: every operator
in the variant, the mask each filter takes, the builder's verbs and its
sinks, which executor runs which member, and the two tables that map a
cloud's lane names onto a chain's attribute names and a stamp's dials.
`README.md` beside the library is the front page; `MESH.md` carries the
`mesh/pop` headers these values are declared in.

## The operators

`pop::Operation` is a variant over twenty-five operator values, and `pop::Chain`
is a vector of them. Generators seed a chain: `SplineScatter` (points along a
window of a closed loop), `MeshScatter` (points on a formed model's
faces) and `PointSet` (an existing `Cloud` — an import's `asCloud()`, a
previous cook — every lane riding in as an attribute, so a Houdini group
arrives as a mask under its own name). Filters rewrite attributes in place: `Jitter`, `Noise`, `Ramp`,
`Vary`, `LookAt`, `Math`, `Smooth`, `Fill`, `AtlasCell`, `Lookup`, `Affine`
(any `mat4` on a position or a direction lane), `Peak` (push along a
direction lane), `Deform` (twist, taper or bend about an axis), `Mix`
(blend two lanes into a third by a constant or a lane) and `Normal` (make
a direction lane unit-length and give every one of them the same sense,
outward from a centre or inward). `Select` is the selector: it writes a
mask lane from a sphere or box region, feathered at its edge and combined
into what the lane already holds (replace, union, intersect, subtract),
and `Delete` is its other half — it drops the points a mask names, which
is the one operator that changes the count. Four operators read points
they do not own, and two of them — `Relax` and `Transfer` — go through
`path::Neighbours` to find which: `Smooth` eases a lane
toward the midpoint of the two beside it IN THE CHAIN, `Relax` pushes
every point out of the way of the points within its radius IN SPACE
(they share Houdini's word and are different operators, which is why they
have different names here), `Cluster` groups the set by k-means in
whatever metric its `weights` name and writes which group each point
landed in, and `Transfer` carries one lane over from another cloud by a
distance-weighted gather with a taper back at the edge of its reach.
Beside them `connectAdjacent()` is a SINK, not an operator: a cloud is
positions plus lanes, all parallel and one value per point, and an edge
is neither — so the pairs are answered instead, which costs the cloud
nothing and is the currency a line, a mesh edge and a spring all already
want. `Promote` and `Sort` are the
primitive-class and permutation-class operators.

Every operator addresses attributes by name through `pop::AttributeReference`, with
`"P"`, `"T"`, `"Dir"`, `"Scale"`, `"Color"` and `"Tex"` as the well-known
names and anything else creating a custom lane on first write.

**Every filter takes a mask.** Each per-point filter carries a `mask`
field naming a lane; that lane's `.x`, clamped to `[0, 1]`, is how much of
the operator's write each point receives — `old + (new - old) * mask`. An
empty name (the default) is every point in full; naming a lane nothing has
written selects nobody, the way an empty group is empty. `Select` is one
way to write such a lane; a `Lookup`, a `Math` on a custom lane, or an
importer's attribute serve just as well. Both executors apply the mask
with the same expression.

`pop::on()` — over a loop, a `Mesh`, a `Chain` or a `Cloud` — returns a
`Builder` whose chained verbs (`count`, `window`,
`spread`, `seed`, `jitter`, `noise`, `vary`, `fade`, `tint`, `lookAt`,
`move`, `fill`, `atlas`, `rampBy`, `order`, `orderBy`, `promote`, `smooth`,
`select`, `drop`, `keep`, `masked`, `affine`, `orient`, `peak`, `twist`,
`taper`, `bend`, `mix`, `mixBy`, `copy`, `normal`, `relax`, `cluster`,
`transfer`, `operation`) append operators — `masked()` sets
the mask on the filter just added — and the builder converts to a
`Chain`, so you can reach into any operator afterwards and re-cook. Sinks
end a chain: `cook()` to a `Cloud`, `cookMesh()` to one mesh of stamps,
`cookSweep()` reading the cooked points as the path `pop::sweep()`
carries a profile along, and `cookBillboards()` splatting them onto a
canvas as camera-facing sprites — the one sink that forms no geometry,
because a billboard faces the eye and so is answered where the eye is
rather than in the world. The builder reaches all four as `cloud()`,
`stamps()`, `sweep()` and `billboards()`.


## The pop family, and where each member runs

Every member of the family is a described VALUE performed by an
EXECUTOR, and the two columns say which executors there are for it. A
"kernel" is one piece of Slang this feature compiles twice — to the C++
the host executor calls, and to the SPIR-V a device executor dispatches
— so the two tiers are held to bit identity rather than to a tolerance
wherever the device column says yes. A *host-only* entry is a stated
boundary and not a gap: the reason is in the operator's own doc comment
and repeated in one word here, and a device runtime DECLINES such an
operator by name rather than dropping it, so `pop::cook` stops with a
message naming both the operator and the runtime.

| Operator | Class | Host | Device | Why, where there is no kernel |
| --- | --- | --- | --- | --- |
| `SplineScatter` | generator | yes | seeded on the host, uploaded | a generator makes points rather than mapping over them |
| `MeshScatter` | generator | yes | seeded on the host, uploaded | as above |
| `PointSet` | generator | yes | seeded on the host, uploaded | as above |
| `Jitter` | filter | kernel | kernel | |
| `Noise` | filter | yes | declines | a field of library sines; a polynomial sine is a different function, not a rounding of one |
| `Ramp` | filter | kernel | kernel | |
| `Vary` | filter | kernel | kernel | |
| `LookAt` | filter | kernel | kernel | |
| `Math` | filter | kernel | kernel | |
| `Smooth` | filter | yes | declines | a point reads two it does not own, so one lane cannot be both what is read and what is written |
| `Fill` | creator | kernel | kernel | |
| `AtlasCell` | filter | kernel | kernel | |
| `Promote` | primitive | yes | declines | addresses triangles a sink has not formed yet |
| `Lookup` | filter | kernel | kernel | |
| `Sort` | permutation | yes | declines | a permutation is a sorting network, not a per-point map |
| `Select` | selector | kernel | kernel | |
| `Affine` | filter | kernel | kernel | |
| `Peak` | filter | kernel | kernel | |
| `Deform` | filter | yes | declines | twist and bend turn on library trigonometry |
| `Mix` | filter | kernel | kernel | |
| `Delete` | set | yes | declines | the count is what it changes, and a per-point map cannot change it |
| `Normal` | filter | kernel | kernel | |
| `Relax` | filter | yes | declines | a point reads the points near it in SPACE, which no per-point map can address |
| `Cluster` | filter | yes | declines | as above, and the groups are decided over the whole set at once |
| `Transfer` | filter | yes | declines | the points it reads are not even in the cloud being cooked |

And the sinks, which stand on the cooked cloud:

| Sink | What it forms | Host | Device |
| --- | --- | --- | --- |
| `cook()` | the `Cloud` itself | yes | the chain dispatched, read back once |
| `cookMesh()` / `points::instance()` | the stamp placed at every point | yes | the vertices dispatched, read back once |
| `cookSweep()` / `pop::sweep()` | the profile carried along the cooked points | yes | the ring vertices dispatched, read back once |
| `cookBillboards()` / `points::drawBillboards()` | camera-facing sprites on a canvas | yes | — |

**Against TouchDesigner's POP set**, the operators above answer Noise,
Transform, Math, Attribute Create, Attribute (blend and copy), Lookup and
Ramp, Group, Delete, Normal, Sort, Attribute Promote, Twist/Bend/Taper,
Smooth, Peak, Look At, Randomise and the texture cell pick; the generators answer Point
Generator, Scatter, SOP to POP; the sinks answer Copy/Instance and
Skin/Sweep. **Not present**: Ray (project points onto a surface along a
direction), Limit (clamp a lane to a range — a `Lookup` with a flat table
is the workaround), Trail (a point's history as a curve), Particle
(integrate velocity and force per frame) and Texture Sampler (read an
image at a point's uv into a lane; `AtlasCell` picks a cell, it does not
sample).

**One table for the lane convention, and one for the stamp.**
`pop::attributeFor` and `pop::cloudLaneFor` are the whole of the mapping
between a Cloud's lane names and the chain's attribute names —
`t`↔`T`, `size`↔`Scale`, `dir`↔`Dir`, `tint`↔`Color`, with `normal` also
seeding `Dir` because that is what a generator or an importer writes —
so a cloud seeded into a chain and exported back comes home to the lanes
it left from. `points::stampOptions(cloud)` is the whole of how a stamp
rides those lanes: "dir" where a chain produced one, "normal" where a
generator did, "size" scaling and "tint" colouring, each only where the
cloud carries it. Every stamping path takes its options from there,
because two tables mean one cloud standing its stamps up through one
caller and lying them flat through another.
