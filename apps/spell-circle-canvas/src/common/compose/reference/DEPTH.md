# 3D, the CSS way

A chapter of [SigilCompose's README](../README.md).

A node is a **plane**. Five lanes turn it and move it in depth —
`rotateX`, `rotateY`, `translateZ`, `scaleZ`, and `rotate`, which is
the turn about z — and `perspective` on an ancestor is the
view that ancestor's children are seen through: a viewer standing that
many pixels in front of the ancestor's plane, over the point
`Element::perspectiveOrigin` names (its centre by default). Every one of
them is a `motion::Animatable` lane exactly like the 2D lanes: it
transitions, mounts, binds and prunes the same way, animating it never
relayouts, and the third length of `Element::transformOrigin` gives the
pivot a depth. The frame is CSS's — x right, y down, **+z toward the viewer** — so a
positive `translateZ` under a `perspective` comes closer and grows. The
three rotations compose as the CSS list `rotateX() rotateY() rotateZ()`,
x outermost, then the scale and the skew about the transform origin, with
the translate outermost of all.

```cpp
box().perspective(800)  // the view, for the children
    .children({box().rect(card).fill(paper).rotateY(bind(&turn).target(0, 360))});
```

**One 4x4 per node, flattened at paint.** A node composes its parent's
perspective, its layout offset and its own lanes about its origin into one
4x4 and projects its plane onto the plane its parent paints on — the 3x3
with a perspective row that Skia draws, one concat. Tree order stays draw
order; the node's fill, its text, its children and its caches all live in
its plane as they did before; a settled plane's recording is taken in
that plane and replayed through the projection, and a `Cache::Texture` on
a turned plane bakes in the plane at the projection's largest local
scale. A node with no depth lane is placed by the byte-exact elementary
ops it always was. A plane turned a quarter turn has no width and draws
nothing; a flattening with no inverse draws nothing and answers no hit.

**`Element::preserve3d` opens a shared space.** The children of such a
node keep the depth their lanes give them — their 4x4s compose with the
host's instead of flattening into it — and are painted **back to front by
the depth of their centres**, whatever order they were declared in. A
cube is six children of one host, each turned about its own centre and
then moved half an edge along the cube's axis in the host's frame, since
the translate is outermost:

```cpp
box().preserve3d().rotateY(bind(&yaw).target(0, 360))
    .children({face().translateZ(half)})                 // front
    .children({face().rotateY(90).translateX(half)})     // right
    .children({face().rotateX(90).translateY(-half)})    // top
    .children({face().rotateY(-90).translateX(-half)})   // left
    .children({face().rotateX(-90).translateY(half)})    // bottom
    .children({face().rotateY(180).translateZ(-half)});  // back
```

The view reaches into the space: the perspective declared on the host's
parent projects every face, a nested `preserve3d()` compounds the space,
and a child that does not declare it ends the space at its own plane, its
children flat inside it. The host's own paint stands at the front of its
own plane and is drawn before its children. An edge-on host still shows
the planes its space holds, because the space is drawn on the plane
beneath the host and takes no inverse of it. A host paints live — it
never records or bakes an artefact of its own, which is what keeps every
recording above the matrices it bakes — while its children keep theirs;
`Composer::profile` reports the refusal as `HostsSpace`.

**A grouping property flattens.** A node that composites as one layer — a
`overflow(Overflow::Clip)`, an opacity below 1, a blend that is not source-over, an
`filter()`, a `backdropFilter()`, a `mask()`, a `Boundary::Coverage` or an
explicit `Cache::Texture` or `Cache::Group` — cannot host a space, exactly
as CSS's grouping properties force a flat transform style: its children
are projected one by one onto its plane, in tree order, with no depth
between them.

**`Element::backface`** with `material::Backface::Hidden` draws nothing and answers
no hit while the plane's back faces the viewer — decided by the
inverse-transposed normal of the node's whole projection, so a half turn
about x or y hides it and a `scaleX(-1)` mirror does not. A flipping card
is a host turning on `rotateY` with a front and a back pre-turned half
round, both hidden.

**Hit testing goes back through the projection.** `Composer::hitTest`
inverts the flattened 4x4 the plane was drawn with — from the plane the
space is drawn on, for a node in a space — and a point whose pre-image is
at or behind the viewer, or off the plane's projection, misses; a host's
children are tested nearest first.

**What this is not.** Planes never intersect: a child that crosses
another is drawn whole, in the order their centres sort. Nothing is lit,
nothing casts, and a depth is not a position in a world. A scene with
those is a set — SigilWorld — and this stays the retained 2D tree with
CSS's model over it.
