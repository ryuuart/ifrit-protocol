# SigilWorld — the description and the scene it is reconciled onto

The chapter on the value side of this library: one node of a 3D scene as
a comparable value, what it is made of, where it stands, what it lights
and what looks at it, and the retained scene an Element tree is
reconciled onto. `README.md` beside the library is the front page; the
frame a scene is drawn in is `reference/FRAMES.md`.

## Element: one node, as a value

An `Element` is built fresh every frame and thrown away: it holds no
device resources, no entity and no running motion, and the retained tree
behind it is the `Scene`'s business. The chaining setters return the
element, so a node reads as one expression, and the value is
copy-on-write, so passing one around costs a refcount.

Where a concept exists in two dimensions this spells it the way
SigilCompose spells it — `Element::key`, `Element::children`,
`Element::at`, `Element::scale`, `Element::fill`, `Element::cache` — and
the new spellings are the ones a plane does not have: the z lanes, the
axis turn, the geometry slot, tags, emitters and viewpoints.

### Placement

`Element::rotate` is a turn about a direction the three axis lanes
cannot spell, and it applies after them. `Element::scale` writes all
three scale lanes, so binding it binds all three to one output.
`Element::transformOrigin` is the point the rotations and scales turn
about, in the node's own coordinates. `Element::transform` is THE
ESCAPE: a placement computed outside this vocabulary, and a node
carrying one ignores every lane above.

`Element::along` rides a curve: the node stands at a distance along a
spline, turned onto the curve's own moving frame. The distance is in the
spline's units and it is a lane like any other, so a binding tows
the node along. It replaces the translation lanes and the axis turn, and
composes with the rest: the three rotation lanes, the scales and the
origin still apply, inside the frame the curve put the node in.

`Transform` is that placement lane by lane: three of translation, three
of rotation in degrees about the x, y and z axes, three of scale, and
three that put the origin those rotations and scales turn about, with
`Transform::axis` and `Transform::axisDegrees` adding one turn about a
direction the three axis lanes cannot spell. Every lane is a
`motion::Animatable<float>`, so each takes a constant, a transition or a
live binding on its own. `Transform::matrix` is the escape, and a node
carrying one is placed by it alone.

### What it is made of

`Backface` is WHICH SIDES OF A BODY'S TRIANGLES ARE DRAWN.
`Backface::Hidden` is the default for closed solids;
`Backface::Visible` keeps the reverse side of a sheet or panel when the
viewpoint passes behind it. A flat panel that must survive an orbit asks
for it; a closed solid normally keeps the default.

`Element::stamp` is the body standing at every point of an
`Element::cloud` or an `Element::chain`, and on a node whose slot holds
neither it is ignored.

`Element::window` is A WINDOW INTO A LOOP: the leading edge and the
length trailing it, both in loop parameter and both bindable, so
advancing the head alone tows the window round. It addresses the chain in
this node's slot, whose first operator carries the loop; a slot holding
anything else ignores it. A moving window is moving GEOMETRY: every
distinct pair of values is a different chain and cooks its own points.

### The geometry slot

What a node is made of is the four shapes a geometry slot takes — a
formed mesh, a cloud with the body stamped at every point, a point chain
with the runtime that cooks it, or a generator that builds its own — and
the cook that turns any of them into the points and triangles a draw
uses.

There is no kind field anywhere in this library. The slot's value type IS
the kind: a node holding a mesh and a node holding a chain are told apart
by what they hold, so a node that changes from one to the other resolves
new resources and keeps its identity, its handle and its lanes.

`cook` evaluates a geometry: a mesh is already cooked, a stamped cloud
instances its stamp over its points, a chain cooks on its runtime and
then instances, and a generator is asked.

`stampKey` is THE NUMBER TWO STAMPINGS AGREE ON when they are the same
stamping: a fold over the VALUES of a cloud and a stamp, not over their
shapes and not over their addresses. A stamped point set is formed once
per distinct pair and uploaded once, so what says "distinct" has to be
the content — a cloud that has not moved between two frames must answer
with the same number, and a cloud that has moved must not. An address
cannot say it, a cloud freed and remade landing on the same memory, and
a shape cannot, two clouds of one size being two clouds; which is why it
reads the bytes. It costs one pass over the cloud against forming its
whole stamped mesh.

### Emitters, viewpoints and the sky

`Element::light` is an emitter standing where this node stands: the
light's position and direction are carried by the node's transform, so
`Element::at` and `Element::along` move it. `Element::intensity` is THE
EMITTER'S STRENGTH as a lane, scaling what the light declared, so binding
it dims and lifts a lamp without describing a new one; a node with no
emitter ignores it. `Element::emission` is its COLOUR, one lane per
channel, on the same terms, the emitter's own colour standing on every
channel the tree leaves out, and on a node carrying an environment map
those lanes are its tint.

`Element::environmentMap` is THE ENVIRONMENT MAP THIS SET STANDS IN: the
panorama every lit body samples by its normal for what falls on it from
all around, and by its reflected view vector for what it mirrors. The
node's transform ORIENTS it, the way a dome light is placed in every
authoring tool, so a turn about y turns the sky.

**A frame holds ONE.** A second one described is a warning naming both
keys, and the first in tree order is the one that shades — a silent
no-op would be a set lit by whichever node happened to come last.

`Element::diffuse` and `Element::specular` are how much of the map
reaches a surface as the light falling on it from everywhere, and how
much of it a surface mirrors; pushing one and not the other is a look,
not a physical claim. `Element::roughnessBias` is added to every
surface's roughness before it picks a prefiltered level, so a whole set
softens without a material being edited. `Element::crossfade` runs
between the map and a second one, both sampled and mixed, which is what
lets a sky change while the frame is running.

`Element::exposure` is THE EXPOSURE THE SET IS READ AT: what every
radiance is multiplied by before the tone curve compresses it onto what a
display can hold. Doubling it is one stop. It is the one dial here that
means something in a set carrying no panorama at all, because a lit sum
ends at the curve either way.

`Element::backdrop` is THE SKY SHOWN behind the set, at that strength —
zero draws none of it, so the dial is also the switch — blurred by
`Element::backdropBlur` in the same roughness units a reflection reads.

`Element::camera` is a viewpoint standing where this node stands, on the
same terms as an emitter: the camera's eye and target are carried by the
node's transform.

### Cascading an entrance

`Element::staggerChildren` CASCADES THE ENTRANCES of this node's children
as they mount, on the schedule SigilMotion speaks — an even ladder, a
fixed total divided across however many children there are, a cue table,
an origin and a distribution curve. The delay compounds down the subtree,
so a grandchild enters after its parent did. Only children that actually
mount are delayed.

`ElementNode::childStagger` is that cascade on the retained side: each
child's entrance is delayed by the start time the schedule gives its
ordinal, and the delay compounds down the subtree, so a set that arrives
arrives in an order rather than all at once. It delays only children that
actually MOUNT — the first describe cascades the whole list, a child
appended to a live list is the only new mount in its patch and enters at
once, and children already standing never re-enter.

### Deferring a describe

`memo` is a deferred description: the function runs only when its
properties changed, by `operator==`, since the last render at this
position or key, AND the ambient environment bindings are unchanged — a
memo is a pure function of its properties and its environment. The
captured stack is re-established around the deferred call, so an
inherited value read inside it reads what was bound where the memo was
WRITTEN.

`each` is THE CHILDREN A RANGE DESCRIBES, one per item in the range's
order, for a `children` block — the spelling a compose tree uses, on this
side of the seam. Its function takes the item, or the item and its index.

```cpp
rig.children({each(posts, post), each(4, lantern)});
```

The count overload is THE CHILDREN A COUNT DESCRIBES, one per index — the
ring of N posts whose only difference is where it stands.

## The environment a set stands in

The map itself is the material library's value: one equirectangular
panorama, prefiltered by roughness, with a cosine-convolved diffuse side.
What is added here is where it stands and how far it is believed — the
node's transform ORIENTS it, and the dials are the lanes a tree binds.

`Backdrop` is THE MAP SHOWN AS THE SET'S SKY, behind everything else in
it. A backdrop is a separate question from what the map lights: a set can
be lit by a sunset it does not show, or show one it barely takes any
light from. `Backdrop::intensity` is both the dial and the switch — at
zero nothing is drawn, which is one number rather than a flag and a
number that can disagree.

`Environment::exposure` is THE EXPOSURE THE SET IS READ AT, and it is the
dial that decides which part of the range the tone curve's shoulder falls
on: a set lit by a panorama with a sun in it wants a smaller one than a
set lit by a lamp. It is the one dial that stands where no panorama does,
a lit sum ending at the curve whether or not the set carries a sky.

`Environment`'s second map is crossfaded over the first: at 0 only the
first is read, at 1 only the second. Both sides are sampled and mixed
rather than one being rebuilt, which is what makes a sky able to change
while the frame is running.

## Light: emitters as plain values

A sun, a point light and a spot, each a comparable struct of where it is,
which way it faces, what colour it is and how far it reaches. Nothing
there renders, uploads or holds a device: a light is a value a renderer
reads, a scene writes to a stage, and a test compares.

Colours are LINEAR. A SUN has a direction and no position: it is
infinitely far away and shines the same everywhere. A POINT light has a
position and a range. A SPOT is a point light narrowed to a cone about
its direction, full strength inside the inner angle and dark outside the
outer one.

`attenuation` is how much of a light reaches a point, in [0, 1], before
any surface term. A sun reaches everything equally. A point light falls
off on a window — one minus the squared distance ratio, squared — rather
than an inverse square, which keeps an authored intensity in the same
small range as a sun's instead of running to thousands, and reaches
exactly zero at its range rather than trailing off forever. A spot
multiplies that by its cone.

## Selector: a set of nodes, described rather than enumerated

A default-constructed `Selector` matches everything, which is what a
caller that did not narrow anything means. The terms compose with `|`,
`&` and `!`, and the result is a value: two selectors built the same way
compare equal, so a description carrying one prunes like any other field.

## Lanes: the rows a patch ramps along

`lanesOf` fills a caller-owned vector with a node's lanes: always the
whole count of them, in slot order, with a null value on every row this
description does not carry the block for. The vector is the caller's so a
per-frame walk allocates nothing after the first node.

The four EMITTER rows and the seven ENVIRONMENT rows stand at their own
value's fields rather than at the fixed defaults: a light whose strength
lane is dropped ramps back to the strength the light itself declares,
which is what makes the lanes dials on the value instead of a second copy
of it.

## propertiesEqual: the structural prune

`propertiesEqual` asks whether two nodes are provably the same node
described twice. Every field of `ElementNode` is ruled on, and anything
that cannot be compared answers false — a field left out does not produce
a wrong answer where the mistake is, it produces a node that never
patches on that field again.

Two fields are deliberately excluded and both are compared elsewhere: the
memo is compared earlier and more strictly by the reconciler, first the
captured environment and then the author's own properties comparison, and
the children are reconciled by key rather than compared.

## Scene: the retained side

An author builds a fresh Element tree every frame and hands it to
`Scene::render`; the scene reconciles it onto what it already holds, so
only what changed is touched. THREE LIFETIMES run underneath and none of
them is the others':

- a NODE — its key, its lanes, the motions in flight on them and its
  entity — lives as long as its key is in the tree;
- a RESOURCE — what a geometry slot cooked to — lives in a content-keyed
  store, reference-counted, shared by every node that describes the same
  geometry, and dropped when the last of them lets go;
- an EXTRACTED FRAME lives for one draw.

So a node whose geometry slot changes resolves a new resource and drops
the old one while its entity and its running motions stand. Nothing is
welded to what a slot holds, which is why there is no kind field and why
nothing here ever remounts.

There are exactly two write paths: `Scene::render`, and the live values a
description's lanes are bound to. Nothing writes onto a retained node
from outside.

`Scene::render` is ONE FRAME: describe, sample the lanes, derive the
placements, extract, order the passes and execute them. A frame with no
passes is its scene — nothing is executed and the draw paints the bodies
extract left. A frame WITH passes has already been performed when it
returns, into the resources the ordering gave it, and the draw presents
what they wrote. Execution reads the extracted state and never the
Element tree.

`Scene::draw` presents what the last render produced. A frame that
declared passes has already run them, so the camera and the runtime
arguments are the ones the passes already used and do not enter into it.
The overload with no camera draws from the viewpoint the tree declared; a
tree with no camera draws from the frame's, and a frame that named none
from the default.

`Scene::handleOf` is THE HOST HANDLE of the node addressed by a key,
opaque and non-zero, and 0 when no node answers to that key. It is the
pin on a node's identity: a handle that survives a describe is the same
node, with the same lanes and the same motions on them.

`Scene::referencesOf` is how many nodes hold a reference to the cooked
artefact a keyed node resolved, and 0 when it resolved none. Two nodes
describing one geometry share one artefact and answer 2.

## See also

`reference/FRAMES.md` for the passes a scene executes, and
`reference/PRESETS.md` for the trees the kit composes.
