# RIG Diagnostics

An original, animated engineering console inspired by *Dead Space*. The primary
projection carries a rear suit schematic, health spine, biomedical traces,
oxygen reserve and module status. A separate projection carries a cutter
schematic, energy inventory, power routing and a maintenance advisory.

The interface uses native projected planes in a shared perspective space. The
main panel subtly turns through a 9-second cycle while health illumination and
the inspection scan follow the same phase. The suit and cutter are retained pen
drawings; the small vital-sign plot runs live. A file-backed material supplies
the retained hologram grid and scan texture. A separate gradient plane carries
the travelling illumination, so moving light does not invalidate the grid's
pixels. The study needs no raster assets or full-screen blur pass.

The canvas is 1440 × 960. Its catalogue still is at 4.2 seconds. The displayed
measurements are fictional interface content, not an instrument connected to a
device.

Reference direction: cyan projections, information attached to equipment,
segmented suit telemetry and utilitarian mining hardware. This is an invented
maintenance screen, not a copy of the game's inventory.

- [EA: Inside Dead Space, Back to the Beginning](https://careers.ea.com/en-gb/inside-ea/news/inside-dead-space-6-back-to-the-beginning)
- [Game UI Discoveries: What Players Want](https://www.gamedeveloper.com/design/game-ui-discoveries-what-players-want)

Open `dead_space_rig.cpp` in Grimoire, or select `dead_space_rig` in the registry
under **Study · Game UI**.
