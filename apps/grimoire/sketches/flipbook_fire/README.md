# Flipbook fire

Two iron braziers burn on a dark stone floor. Everything that burns is
played from textures the sketch generates once at setup, in the forms an
effects tool hands over: a flame flipbook of 64 frames in an 8 × 8 sheet on
black, a mask packing three greys into one image's channels, and a small
flow map of directions.

- **The flames** are cards turned to the camera about the up axis, each
  giving off the flipbook as light three times brighter than white, added
  over the scene. Where a card dips into its bowl it fades out over a short
  distance, so there is no line along the coals.
- **The left flame blends its frames**; the right one steps from frame to
  frame and starts 23 frames later. Side by side, the difference is the
  stutter the right one has and the left one does not.
- **The embers** are one stamp — a small square — drawn as instanced
  geometry, forty per fire, each copy turned to the camera where it stands
  and added over the scene. Every ember carries its own cell of the flame
  sheet as a named scalar, so each plays a different frame, and its own
  brightness as its colour's alpha, fading as it rises. Where each one is
  is a function of the time and its own seed.
- **The coals** are dark stone glowing where the mask's green channel — a
  repeating noise — says they are hot, streaming slowly along the flow map.
  Their glow is brighter than white too.
- **A light above each brazier** burns with its flame, its strength
  flickering on three periods, and lights the bowl and the floor.
- **Bloom** on the frame spreads every light brighter than white — the
  flame cores, the hottest coals — into a soft glow.

The loop is two seconds: 64 frames at 32 a second, and every flicker a
whole number of cycles in it. The still is taken at 0.6 seconds.

It plays an effect and does not simulate one: no frame depends on the frame
before. It has no smoke, no lighting of the flame by the scene
(the flame is its own light) and no motion vectors between frames — the
blended flame cross-fades its cells.
