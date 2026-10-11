# SigilWorld — the nodes and the scene they are reconciled onto

The chapter on the value side of this library: the five kinds of node a
3D scene is made of, what each is made of, where it stands, what it
lights and what looks at it, and the retained scene a tree is reconciled
onto. `README.md` beside the library is the front page; the frame a scene
is drawn in, with its environment and its passes, is `reference/FRAMES.md`.

## A node, as a value

A node is built fresh every frame and thrown away: it holds no device
resources, no entity and no running motion, and the retained tree behind
it is the `Scene`'s business. The chaining verbs return the node, so a
node reads as one expression, and every kind is a copy-on-write handle on
one description, so passing one around costs a refcount.

There are five kinds in three families — structure, geometry and
influences — and the camera a frame looks through, each started by a
factory and each a typed handle over the one `NodeDescription`
underneath:

| family | kind | factory | what it is |
|---|---|---|---|
| structure | `Group` | `group({…})` | nodes under one placement — the one kind with children, which are its argument |
| structure | `Null` | `null()` | a placement, a key and its collections, and nothing else: a pivot, or what a light or a camera looks at |
| geometry | `Mesh` | `mesh(triangles)` / `mesh(instances)` | one body wearing a surface: triangles drawn once, or instanced geometry drawn as its stamp repeated |
| influences | `Light` | `directionalLight`, `pointLight`, `spotLight`, `rectLight`, `sphereLight`, `diskLight`, `ambientLight`, `hemisphereLight` | one light, placed and aimed by its node |
| — | `Camera` | `camera(lens, options)` | one viewpoint, and how its picture is read |

Every kind converts to `Node`, the type-erased handle a group's children
are held as, so one braced list mixes them:
`group({mesh(body), pointLight(), each(posts, post)})`. A verb belongs to
a kind only where it means something there — `Mesh::fill` and
`Light::lightLink`, and no `fill` on a light or a null.

### What every kind says

The placement verbs — `Mesh::at`, `Mesh::rotateX`, `Mesh::rotateY`,
`Mesh::rotateZ`, `Mesh::scale`, `Mesh::along`, `Mesh::visible` and the
rest of `PlacementVerbs` — and the identity verbs — `Mesh::key`,
`Mesh::collection`, `Mesh::cache`, `Mesh::transition` from
`IdentityVerbs` — are on every kind alike.

`PlacementVerbs::rotate` is a turn about a direction the three axis lanes
cannot spell, and it applies after them. `PlacementVerbs::scale` writes
all three scale lanes, so binding it binds all three to one output.
`PlacementVerbs::transformOrigin` is the point the rotations and scales
turn about, in the node's own coordinates. `PlacementVerbs::transform` is
THE ESCAPE: a placement computed outside this vocabulary, and a node
carrying one ignores every lane above.

`PlacementVerbs::along` rides a curve: the node stands at a distance along
a spline, turned onto the curve's own moving frame. The distance is in
the spline's units and it is a lane like any other, so a binding tows
the node along. It replaces the translation lanes and the axis turn, and
composes with the rest: the three rotation lanes, the scales and the
origin still apply, inside the frame the curve put the node in.

`PlacementVerbs::lookAt` aims a node at a keyed one — usually a `null`
— resolved every frame against where that node stands. A light and a
camera read it as their own aim, below; any other node is turned so its
forward axis, `NodeDescription::forward`, points at the keyed node's
origin, its x axis held square to the aim, in place of its rotation
lanes, its axis turn and a rail's turn, while its position and its
scales stand. The forward axis is +z, the way a quad faces, unless
whatever built the node states the way its contents face, as
`kit::visibleLight` does, so a group carries what it holds round with
the aim. The aim is read where the target stood in the round of
placement before, and the rounds the placements converge in bring a
target moving this frame along; a node aimed at something it carries
turns its own target and may not come to rest.

`PlacementVerbs::visible` takes a node and everything under it out of the
scene: a hidden body is not drawn, a hidden light lights nothing, and a
hidden camera is not looked through. Placement and visibility are the two
things that flow down the tree, and nothing else does.

`IdentityVerbs::collection` joins a named collection, and a node may join
several. Membership does not flow down: a group that joins a collection
is a member, and its children are not unless they join too or a
`Collection` names the group's subtree by its key.

`Transform` is the placement lane by lane: three of translation, three
of rotation in degrees about the x, y and z axes, three of scale, and
three that put the origin those rotations and scales turn about, with
`Transform::axis` and `Transform::axisDegrees` adding one turn about a
direction the three axis lanes cannot spell. Every lane is a
`motion::Animatable<float>`, so each takes a constant, a transition or a
live binding on its own. `Transform::matrix` is the escape, and a node
carrying one is placed by it alone.

### What a body is made of

`SurfaceVerbs::fill` is the surface every face of a mesh is shaded by —
every copy of instanced geometry's stamp alike: a material's base is the base colour and its `surface({…})`
the lit response, lowered once at the verb into the surface program. A
material that states no response is lit at the stock one, so a plain
`material::Color` lights; `.surface({.unlit = true})` is its own light.
The per-face form takes one material per slot, in slot order, and each
triangle wears the slot its mesh's `"Material"` primitive lane names —
the device draws the body once a slot, each draw that slot's triangles.
A body is sorted into the frame's phases and casts its shadow by its
first slot, and instanced geometry wears the first slot on every face.

`Mesh::castShadow` and `Mesh::receiveShadow` say whether a body blocks
the lights that cast shadows and whether their shadows darken it. Both
are on unless the body says otherwise, so shadows appear as soon as a
light casts; what the surface is decides the rest — a blended or an
unlit body casts nothing, a transmissive one colours the light it lets
through rather than blocking it, and an unlit one receives nothing
([LIGHTING.md](LIGHTING.md) holds the table).

`Backface` is WHICH SIDES OF A BODY'S TRIANGLES ARE DRAWN.
`Backface::Hidden` is the default for closed solids;
`Backface::Visible` keeps the reverse side of a sheet or panel when the
viewpoint passes behind it.

### The geometry slot

What a mesh is made of is one of three shapes a geometry slot takes — a
formed mesh, instanced geometry, or a generator that builds its own — and
the cook that turns a mesh or a generator into the triangles a draw uses.

There is no kind field anywhere in this library. The slot's value type IS
what a body is: a node holding triangles and a node holding instanced
geometry are told apart by what they hold, so a key that describes one
and then the other resolves new resources and keeps its identity, its
handle and its lanes.

`cook` evaluates a geometry: a mesh is already cooked, a generator is
asked, and instanced geometry cooks to nothing because it is already what
is drawn — its stamp, once per instance, on the device in one instanced
draw. Making instanced geometry is SigilGeometry's: a point-operator chain
ends in `pop::Builder::instances(stamp)`, and a cloud of points in
`geometry::mesh::points::instances`. A window into the loop the chain
rides is the chain's own (`pop::Builder::window`), so a moving window is
moving geometry, described afresh each frame and cooked each time it
moves.

**Per instance**, every copy carries its colour, its index and up to four
named scalars to the surface it wears. A device program reads them as
`instanceColor()`, `instanceIndex()` and `instanceAttribute(key)`, a key
being `material::instanceAttributeKey` of the scalar's name; a flipbook
plays each copy's own frame from the scalar
`material::FlipbookOptions::frameAttribute` names. A body that faces the
camera turns every instance where it stands.

`stampKey` is THE NUMBER TWO STAMPINGS AGREE ON when they are the same
stamping — the stamp a geometry pass stands at every point a compute pass
cooked, which a frame's targets form into instanced geometry: a fold over the VALUES of a cloud and a stamp, not over their
shapes and not over their addresses. A stamped point set is formed once
per distinct pair and uploaded once, so what says "distinct" has to be
the content — a cloud that has not moved between two frames must answer
with the same number, and a cloud that has moved must not. An address
cannot say it, a cloud freed and remade landing on the same memory, and
a shape cannot, two clouds of one size being two clouds; which is why it
reads the bytes. It costs one pass over the cloud against forming its
instances again.

### Lights

A light is one node type made by eight factories. `directionalLight` is
a light the same everywhere, travelling along a direction toward what it
lights: it stands nowhere. `pointLight` shines every way from where its
node stands, falling off with the square of the distance and, past a
`range` of zero, brought smoothly to nothing at it. `spotLight` is a
point light narrowed to a cone about its direction, full strength inside
`SpotLightOptions::innerAngle` and dark outside
`SpotLightOptions::outerAngle`. Three lights have a size, centred where
their node stands: `rectLight` is a panel `RectLightOptions::width` by
`RectLightOptions::height` shining from its front face along its
direction, `sphereLight` a ball of `SphereLightOptions::radius` shining
every way, and `diskLight` a round panel of `DiskLightOptions::radius`
shining from its front face. `ambientLight` arrives equally from every
direction, and `hemisphereLight` from a sky above the horizon and a
ground below it, about its node's up. A light is aimed by the direction
it travels, and its node's rotation turns that direction — and a
hemisphere light's up — so a light placed under a turned group turns
with it. `PlacementVerbs::lookAt` aims a directional, spot, rect or disk light at a keyed
node instead — usually a `null` — from wherever the light stands, resolved
every frame against where that node stands; a light with no direction of
its own is not turned, and a key no node answers to stops the frame,
naming both.

A light lights every body in the scene wherever it stands in the tree.
`Light::lightLink` narrows that to a `Collection` — USD's light link —
and an unlinked light reaches everything. A light that states
`castShadow` in its options — a directional, a spot or a point light —
casts shadows, made as its `LightShadow` says; `Light::shadowLink`
narrows which of the bodies that cast cast ITS shadow — USD's shadow
link — the same way, and an unlinked light is blocked by every one of
them.

`LightDescription` is what a light node carries: its `LightKind`, the
direction, the colour, the strength and a hemisphere's ground colour —
all `motion::Animatable`, so a light bound to a live value dims and
tints while the tree stands still — the range, the cone, the two
links, whether it casts and how its shadow is made. `PlacedLight` is what a frame keeps of one: its description with
every animatable `held`, its direction carried into the world, and where
its node stands.

A light's colour is typed as an author types any colour and decoded into
linear light (`linearColor`). `attenuation` is how much of a light's
intensity reaches a point before any surface term: the inverse square,
windowed by the range, times a spot's cone, and one for a light that
stands nowhere. `radiance` is the decoded colour scaled by the strength,
what a renderer uploads as one value, and `intensityAt` the intensity
that delivers a strength at a distance. What each kind delivers and how
a surface answers it is **[LIGHTING.md](LIGHTING.md)**.

### Cameras

`camera(lens, options)` is a viewpoint standing where its node stands:
the lens's eye and target are carried by the node's placement, so a
camera riding a rail looks from wherever it has reached.
`CameraOptions::exposure` is what every radiance is multiplied by before
the tone curve compresses it onto what a display can hold — doubling it
is one stop. `CameraOptions::toneCurve` is that curve:
`ToneCurve::Reinhard` divides a colour by one plus its luminance,
`ToneCurve::Neutral` leaves a colour lit to show itself very nearly as
it is and compresses only above a shoulder, and `ToneCurve::Linear`
applies none. `PlacementVerbs::lookAt` aims the camera at a keyed node — its
target is that node's origin each frame, its eye where its placement puts
it.

A frame looks through one camera: the only one in the tree, or the one
`Frame::camera` names by key. Several with none named is an error naming
every one.

### Staggering an entrance

An entrance on a child of a scene is staggered by the child's OWN tween:
`.opacity(motion::animate({.from = 0.0f, .to = 1.0f, .delay =
motion::stagger(60ms)}))` resolves the delay at the child's place among
the siblings mounting with it — a step per child, a fixed total spread
across however many there are, a cue table, an origin and an easing
curve are all the stagger's own options. There is no stagger verb on the
parent, and nothing compounds down the subtree: a grandchild staggers
among its own siblings, by its own tween.

Only children that actually MOUNT are placed: the first describe mounts
the whole list, so the list arrives in an order rather than all at once;
a child appended to a live list is the only new mount in its patch and
enters at once; and children already standing never re-enter.

### Deferring a describe

`memo` is a deferred description: the function runs only when its
properties changed, by `operator==`, since the last render at this
position or key, AND the ambient environment bindings are unchanged — a
memo is a pure function of its properties and its environment. The
captured stack is re-established around the deferred call, so an
inherited value read inside it reads what was bound where the memo was
WRITTEN.

`each` is THE CHILDREN A RANGE DESCRIBES, one per item in the range's
order, for a group's braced list. Its function takes the item, or the
item and its index.

```cpp
group({each(posts, post), each(4, lantern)});
```

The count overload is THE CHILDREN A COUNT DESCRIBES, one per index — the
ring of N posts whose only difference is where it stands.

## Collections: a set of nodes, described rather than enumerated

A `Collection` includes and excludes TERMS, the way a USD collection
does. A plain name is the members of that collection — every node that
called `collection(name)`. A term beginning with `/` is the node with
that key and everything under it, a USD path-based include read against
keys. An empty `Collection::include` includes every node, so a
default-constructed collection is the whole scene, and
`Collection::exclude` takes away whatever any of its terms names. The
result is a value: two collections built the same way compare equal, so a
description carrying one prunes like any other field.

## Lanes: the rows a patch ramps along

`lanesOf` fills a caller-owned vector with a node's lanes: always the
whole count of them, in slot order, with a null value on every row this
description does not carry the block for. The vector is the caller's so a
per-frame walk allocates nothing after the first node.

The rows are placement alone. A light's colour and
strength, a camera's exposure and the environment's dials are animatable
values read once a frame, not lanes.

## propertiesEqual: the structural prune

`propertiesEqual` asks whether two nodes are provably the same node
described twice. Every field of `NodeDescription` is ruled on, and anything
that cannot be compared answers false — a field left out does not produce
a wrong answer where the mistake is, it produces a node that never
patches on that field again.

Two fields are deliberately excluded and both are compared elsewhere: the
memo is compared earlier and more strictly by the reconciler, first the
captured environment and then the author's own properties comparison, and
the children are reconciled by key rather than compared.

## Scene: the retained side

An author builds a fresh tree every frame and hands it to
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
passes is its scene — it is reconciled and extracted, nothing is
executed and nothing is drawn. A frame WITH passes has already been
performed when it returns, on the device runtime it carries and into the
resources the ordering gave it, and the draw presents what they wrote.
Execution reads the extracted state and never the node tree.

`draw(scene, canvas)`, in `advanced/Skia.h`, presents the picture the last
render's passes wrote, from the camera they were performed from. A
frame with no passes, or whose passes did not run, formed no picture and
draws nothing.

`Scene::handleOf` is THE HOST HANDLE of the node addressed by a key,
opaque and non-zero, and 0 when no node answers to that key. It is the
pin on a node's identity: a handle that survives a describe is the same
node, with the same lanes and the same motions on them.

`Scene::referencesOf` is how many nodes hold a reference to the cooked
artefact a keyed node resolved, and 0 when it resolved none. Two nodes
describing one geometry share one artefact and answer 2.

## See also

`reference/FRAMES.md` for the frame, its environment and the passes a
scene executes, and
`reference/PRESETS.md` for the trees the kit composes.
