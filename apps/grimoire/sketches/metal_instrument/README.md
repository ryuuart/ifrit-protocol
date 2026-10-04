# Metal instrument

A fictional precision audio monitor, “TONE / 03”, built as a real
three-dimensional set on a 1440 × 960 canvas. The enclosure has a chamfered
silhouette, a polished front rim, directional finish marks, lathed and knurled
controls, countersunk screw heads, raised vent strips, rubber feet and a
recessed luminous meter. Compose supplies the face markings and the live meter
as ordinary textures; World supplies the geometry, camera, depth, lighting and
reflections. The object is authored from dimensions.

Three finish coupons share the room: polished aluminium at roughness zero,
brushed aluminium with a roughness texture, and a roughness-one blasted
specimen. Two directional lights and a generated studio panorama with long
bright softboxes move around the object, so the machined edges and lathed caps
have reflections to show their shape.

The brushed finish combines directional colour marks, shallow normal grooves
and scalar roughness; it has no separate roughness along and across the grain,
so the highlight is not anisotropic. The markings are printed colour in the
face texture and do not change the mesh. The vent strips are raised geometry on
a closed enclosure, not openings. The screen's thin tinted cover asks for
transmission, which only the device renderer provides.

The lower-right specimen is a Compose plane with real perspective, depth
translation and backface hiding, deliberately smaller than the instrument:
Compose lights the plane in page space, while World shades the meshes by their
transformed normals. At the reverse moment the Compose plane disappears while
the closed instrument still shows its rear face.

## Timeline

The 24-second loop eases between named moments; each row is the moment's peak.

| Seconds | Moment |
| --- | --- |
| 0 | Facing view under the initial rig. |
| 2.4 | Hero view: labels, screw slots, knurling, chamfers and the meter. |
| 6 | 77° yaw: grazing highlights, finish differences and thin silhouettes. |
| 9 | Face and controls displaced forward: real depth and occlusion. |
| 12 | Near plane at 1390 slices the instrument; the status overlay remains. |
| 15 | 180° yaw: the closed rear survives; the front panels and Compose plane hide. |
| 18 | A zero-depth request becomes a finite 0.5-unit plate; the coupons keep their endpoints. |

At minimum depth the near plane moves forward to at least 200 so the
compressed layers keep their separation.

## Controls

`kControls` in `metal_instrument.cpp`:

| Control | Range and meaning |
| --- | --- |
| `yaw`, `pitch` | Hero orientation, clamped to ±180° and ±85°. The named moments override yaw. |
| `depth` | 0.5–240 world units; zero and negative requests clamp to 0.5. |
| `brushedRoughness`, `polishedRoughness`, `blastedRoughness` | 0–1; the coupons keep the endpoints. |
| `normalStrength` | 0–2 groove strength; zero removes the perturbation. |
| `lightSpeed` | −60–60 degrees per second; zero freezes the rig. |
| `perspective` | 0–3000 pixels on the Compose plane; zero disables perspective. |
| `nearPlane`, `farPlane` | Near 0.1–1700; far 100–12000 and at least 100 beyond near. |
| `glassTransmission` | 0–1 transmission request on the screen cover. |

Nonfinite values take their defaults. The pose and meter are functions of scene
time, so any moment can be reached directly.
