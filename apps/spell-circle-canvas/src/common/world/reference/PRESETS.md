# SigilWorld — the presets and the rails

The chapter on the kit: elements composed out of the verbs a tree is
already written in — a three-point rig, a turntable, and a set with both
over a ground plane — and the rails a body rides: the turntable's ring, a
loop that rises and falls, and a winding round a shell. `README.md`
beside the library is the front page.

## Nothing here decides a look

Each preset returns an ordinary `Element` whose every field the caller
can read, replace or ignore — or a plain spline a tree rides — and the
only constants in it are the geometry of the arrangement: where a key
light stands relative to a subject, how a rail circles it, plus one
neutral grey for a ground plane that was given no surface.

There is no shading model here, no catalog of surfaces and no material of
this library's own. A preset is a shorthand for a tree someone could have
written by hand, and it is worth having only for as long as that stays
true.

## Rig: where three lights stand round a subject

The arrangement is stated relative to the subject, so one rig serves a
thumbnail and a room: `Rig::extent` is how far across the subject is and
every distance is a multiple of it. `Rig::bearing` turns the whole rig
about the subject's up axis, so a set is re-lit by turning one number.

`Rig::distance` is how far the key stands from the subject, in extents.
`Rig::bearing` and `Rig::elevation` are the key's bearing round the
subject and its height above it, in degrees; the fill stands a quarter
turn the other way and lower, and the back light stands opposite and
higher.

`Rig::fill` and `Rig::back` are those two as fractions of the key's
strength. The key is what a subject is read by; the other two are what
keeps it from being read by the key alone. `Rig::intensity` and
`Rig::color` are the key's own strength and colour, and the other two
take the same colour, scaled.

`threePoint` is THE RIG, as one element with three keyed children — the
key, the fill and the back — each standing where the arrangement puts it
and carrying one emitter.

## Turntable: a camera circling a subject

How far out the camera stands, how high, and how long one turn takes.
`Turntable::period` is seconds for one full turn, and zero or less parks
the camera at its first station. `Turntable::stations` is how many points
the rail is drawn through: more is rounder, and the count is there
because it decides the curve and therefore the pixels, not because it is
a dial worth turning. Fewer than three is three, because a closed loop
needs three points to be one.

`rail` is THE RAIL a turntable rides: a closed loop of stations round the
subject, at the table's radius and height.

`turntable` is THE CAMERA at a given scene time, as one element riding
that rail and looking at the subject from wherever it has reached. The
viewpoint is written in the rail's own moving frame, which is what makes
the aim independent of how far the camera has travelled: the frame's
first axis points inward at every station, so the subject is one radius
along it and one height down, always.

## Wave: a rail that rises and falls

Stations round the subject, the even ones standing out at the outer
radius and above the centre, the odd ones in at the inner radius and
below it — so a body riding it, a comet scattered along it or a band
swept over it reads as a curve in space rather than as a ring seen at an
angle. The two radii are absolute and the two heights are signed, so both
may stand above the centre or both below it.

`Wave::knots` is the stations round the loop. An even count alternates
cleanly all the way round; an odd one puts two outer stations side by
side at the seam. Fewer than three is three.

`wave` is that shape as a closed spline through its stations.

## Winding: a rail that winds a shell

A closed loop on the ellipsoid of the given half-extents, climbing and
diving `Winding::wraps` times per lap while the plane it winds in turns
`Winding::turns` times. Two counts with no common factor keep a later
wrap from retracing an earlier one, so the loop crosses in front of and
behind itself and a body riding it is seen from every side in one lap;
the winding runs from +x toward −z.

`Winding::knots` is the stations round the loop. The curve is smooth
between them, so the count decides how faithfully the winding is drawn
rather than what it is; fewer than three is three.

`winding` is that shape as a closed spline through its stations.

## Set: a lit set

A ground plane, a rig over it, a turntable round it, and whatever is
being looked at. `Set::ground` is how wide the ground plane is, in the
rig's extents, and zero lays none. `Set::drop` is how far below the
subject the ground lies, in extents. `Set::surface` is what the ground is
made of: left empty it is one neutral grey, which is the only colour this
library states and is stated so that a set has a floor rather than
because grey is the right answer.

`litSet` is the set with a subject standing on it. The tree is the set
with the children ground, rig, camera and subject, in that order — a
caller who wants one of them alone builds it alone.

## See also

`reference/ELEMENTS.md` for the verbs these presets are composed out of.
