# The Black Hours

A material facsimile of **The Black Hours, MS M.493, fol. 19v**, made in
Bruges around 1480 and held by The Morgan Library & Museum. The leaf begins
the Short Hours of the Holy Spirit. Its silver script, gold lettering,
green initials, blue border, flowers, foliage and marginal winged devil
come from the actual digitized leaf.

This is a photographic facsimile with selective material relighting, not
newly typeset medieval lettering or a vector reconstruction. The source
image preserves the exact hand, border proportions, ruling and wear.
`gold.sksl` selects warm yellow chroma while excluding neutral silver and
blue pigment. A metallic native surface and a beaten normal field give the
selected regions a moving gold-leaf response. The material is an artistic
interpretation: the scan does not supply measured normals or reflectance.

The local image keeps the sketch usable offline. `data/black-hours-019v.png`
is a lossless PNG conversion of the museum's full-resolution Zoomify TIFF
image (1427 × 2020); no lettering or ornament has been repainted. The
photographic image is credited to The Morgan Library & Museum. The
underlying manuscript is a public-domain work.

- [Leaf catalogue and iconography](https://ica.themorgan.org/manuscript/page/3/110803)
- [Museum image viewer](https://ica.themorgan.org/zoom/default.asp?id=m493.019v)
- [Source TIFF](https://ica.themorgan.org/zoom/images/4/m493.019v.zif)
- [The Black Hours collection](https://www.themorgan.org/collection/Black-Hours)

Open `manuscript.cpp` in Grimoire. The declared still is at 3 seconds;
the light completes its sweep without moving the page or its script.

The source image and gold coverage are retained at the displayed page
resolution. Native material lighting shades a smaller gold surface. The image
compositor stretches that smooth lighting field through the full-resolution
coverage mask, preserving fine edges and wear without a full-page per-pixel
lighting shader.
