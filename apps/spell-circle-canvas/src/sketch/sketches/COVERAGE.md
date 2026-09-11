# Feature coverage

The catalog demonstrates the public visual feature families below. A sketch
is an example of the stated scenario, not proof of every option, boundary
condition or combination in that family. Unit tests own those contracts;
plate and window runs check the rendered result on a particular backend.
Compiling a sketch or seeing `available: true` does not prove it rendered
successfully or exercised a device implementation.

Start with a small API sketch to learn an operation. The reference studies
show how those operations compose into a complete picture.

| Feature family | Examples | What to inspect |
| --- | --- | --- |
| Component props, slots and shared arrangements | [surface_components](surface_components.cpp), [border_weave](border_weave.cpp), [net_policy](net_policy.cpp) | One card accepts different content and paint values; incomplete panel rows retain their track widths. |
| Flex, track and table layout | [grid_layouts](grid_layouts.cpp), [spacejam_1996](spacejam_1996/spacejam_1996.cpp), [twoadvanced_equipment](twoadvanced_equipment.cpp) | Child placement, baseline alignment, automatic table sizing and spans. |
| Keyed updates, feeds and memoization | [daemon_console](daemon_console.cpp), [loot_grid](loot_grid/loot_grid.cpp), [volatility_cost](volatility_cost.cpp) | Entries arrive and leave; stable descriptions keep their retained identities. |
| Theme and environment values | [env_theme](env_theme.cpp), [env_faces](env_faces.cpp), [env_lanes](env_lanes.cpp) | Nested overrides and the point at which values become component props. Deferred callbacks capture resolved values. |
| Cache and layer boundaries | [live_settling](live_settling.cpp), [coverage_boundary](coverage_boundary.cpp), [surface_components](surface_components.cpp) | Live descendants, silhouette tracing and resizing a style under texture caching. |
| Shapes and path construction | [shape_tour](shape_tour.cpp), [path_booleans](path_booleans.cpp), [svg_silhouette](svg_silhouette.cpp) | Generated silhouettes, boolean operations and fitting imported contours. |
| Contour sampling and placement | [contour_poses](contour_poses.cpp), [exact_tangent](exact_tangent.cpp), [formation_bands](formation_bands.cpp) | Arc-length placement, tangents, shared geometry and swept bands. |
| Connections and routing | [routers_straight](routers_straight.cpp), [routes_probe](routes_probe.cpp), [ds2_bench](ds2_bench/ds2_bench.cpp) | Obstacles, ports, rails and a composed circuit. |
| Brushes, stroke spans and crossings | [stroke_atlas](stroke_atlas/stroke_atlas.cpp), [crossing_rule](crossing_rule.cpp), [border_weave](border_weave.cpp) | Stroke stacks, patterned marks, reveals and crossing order. |
| Decorations and layer styles | [y2k_chrome](y2k_chrome.cpp), [chrome_type](chrome_type.cpp), [corner_notched](corner_notched.cpp) | Gel, chrome, glyph boundaries and edge treatments. |
| Masks, blending and filtering | [over_under](over_under.cpp), [matte_luma](matte_luma.cpp), [blur_falloff](blur_falloff.cpp), [crt_bloom](crt_bloom.cpp) | Material masks, luma coverage, blur support and post-processing. |
| Paragraph styles and line breaking | [paragraph_sheet](paragraph_sheet.cpp), [keeps_and_frames](keeps_and_frames.cpp), [spacing_passes](spacing_passes.cpp) | Indents, leading, justification, hyphenation, keeps and frame boundaries. |
| Text flow and threaded frames | [horizontal_flow](horizontal_flow.cpp), [mawarikomi](mawarikomi.cpp), [threaded_story](threaded_story.cpp), [frame_grid](frame_grid.cpp) | Exclusions, columns, frame chains and constrained measures. |
| Rich text and inline content | [rich_slot_reserve](rich_slot_reserve.cpp), [warichu_placeholder](warichu_placeholder.cpp), [bullets_dropcap](bullets_dropcap.cpp) | Reserved inline boxes, annotations, bullets and initial letters. |
| CJK and vertical typography | [tategaki](tategaki/tategaki.cpp), [cjk_rules](cjk_rules.cpp), [ruby_kenten](ruby_kenten.cpp), [bousen](bousen.cpp) | Orientation, punctuation, ruby, emphasis and decoration placement. |
| Font metrics and glyph paint | [optical_kerning](optical_kerning.cpp), [axis_ripple](axis_ripple.cpp), [text_paints](text_paints.cpp), [paragraph_paints](paragraph_paints.cpp) | Spacing, variable axes and paints resolved against text geometry. |
| Text queries and choreography | [annotated_margin](annotated_margin.cpp), [shipping_forecast](shipping_forecast.cpp), [matrix_rain](matrix_rain.cpp), [elastic_type](elastic_type.cpp) | Selecting ranges, nested schedules, per-glyph transforms and live paint. |
| Motion values, bindings and retargeting | [bound_lane](bound_lane.cpp), [lane_retarget](lane_retarget.cpp), [decay_step](decay_step.cpp), [ticker_lanes](ticker_lanes.cpp) | Bound chains, interrupted motion, decay and independent clocks. |
| Schedules and physics | [karaoke_wipe](karaoke_wipe.cpp), [set_stagger](set_stagger.cpp), [hitman_verlet](hitman_verlet/hitman_verlet.cpp) | Timing shared by different effects, stagger and constrained bodies. |
| Compute values and procedural signals | [noise_shelf](noise_shelf.cpp), [compute_variant](compute_variant.cpp), [curve_shelf](curve_shelf.cpp) | Comparable signals, executor choice and sampled curves. |
| Meshes, cameras and point operators | [mesh_generators](mesh_generators.cpp), [pop_math](pop_math.cpp), [pop_order](pop_order.cpp), [pop_deform](pop_deform.cpp), [pop_billboards](pop_billboards.cpp) | Mesh construction, ordered operators, deformation and facing the camera. |
| Mesh groups, instancing and codecs | [geo_groups](geo_groups.cpp), [pop_prims](pop_prims.cpp), [pop_stamps](pop_stamps.cpp), [codec_roundtrip](codec_roundtrip.cpp) | Group selection, primitive instances, stamps and imported/exported geometry. |
| World scenes and lighting | [first_light](first_light.cpp), [key_light](key_light.cpp), [lantern_room](lantern_room.cpp), [world_hud](world_hud/world_hud.cpp) | Scene hierarchy, cameras, lights and a 2D interface in a 3D scene. |
| Frame graphs and native texture interop | [frame_inputs](frame_inputs.cpp), [import_native](import_native.cpp), [painter_gpu](painter_gpu.cpp), [scene_surfaces](scene_surfaces.cpp) | Frame inputs, device images and composition across runtimes. |
| Material recipes and shader backends | [stock_materials](stock_materials.cpp), [material_child](material_child.cpp), [slang_portable](slang_portable.cpp) | Stock recipe registration, live child materials and portable shader execution. |
| Color and color management | [chevreul_circle](chevreul_circle/chevreul_circle.cpp), [ocio_view](ocio_view.cpp), [xcom_battlescape](xcom_battlescape/xcom_battlescape.cpp) | Derived palettes, OCIO views and indexed color. |
| Patterns, fields and distance shapes | [pattern_sequence](pattern_sequence.cpp), [field_shelf](field_shelf.cpp), [sdf_star](sdf_star.cpp), [black_watch](black_watch/black_watch.cpp) | Reusable pattern recipes, fields, signed-distance shapes and cloth construction. |
| PBR and reflective materials | [material_lab](material_lab.cpp), [reflection_lab](reflection_lab.cpp), [mesh_normal_bridge](mesh_normal_bridge.cpp) | Metallic-roughness parameters versus normal/environment-map reflection. These are distinct models. |
| Textures and environment maps | [material_atlas](material_atlas.cpp), [env_faces](env_faces.cpp), [nine_slice](nine_slice.cpp), [tile_map](tile_map.cpp) | Atlas sampling, environment faces, scalable image borders and tiles. |
| Image formats, fields and encoding | [exr_channels](exr_channels.cpp), [half_float](half_float.cpp), [gif_frames](gif_frames.cpp), [encode_write](encode_write.cpp), [reflection_lab](reflection_lab.cpp) | Channel selection, pixel precision, animated images, output and coverage-derived normals. |
| Resource access and hot reload | [hub_reload](hub_reload.cpp), [net_policy](net_policy.cpp), [sticker_collection](sticker_collection.cpp) | Mounts, refreshed content, network policy and reusable image/video assets. |
| Video decode and composition | [video_compose](video_compose.cpp), [video_compositing](video_compositing.cpp) | Playback surfaces and native-frame composition. Encoding is exercised through Sketchbook's video export lane. |
| Tabular data, scales and reported checks | [data_scales](data_scales.cpp), [nightingale_coxcomb](nightingale_coxcomb.cpp), [dunhuang_star_chart](dunhuang_star_chart/dunhuang_star_chart.cpp), [minard_1869](minard_1869/minard_1869.cpp) | Decoding tables, domain mapping and calculations displayed beside the drawing. |
| Immediate drawing | [p5_hello](p5_hello.cpp), [p5_mixed_forms](p5_mixed_forms.cpp), [p5_flow_field](p5_flow_field.cpp) | Pen state, primitives, retained/immediate composition and persistent marks. |
| Natural-media brushes | [brush_engine_atlas](brush_engine_atlas.cpp), [brush_dynamics](brush_dynamics.cpp), [brush_custom](brush_custom.cpp), [brush_live_tutorial](brush_live_tutorial.cpp) | Brush families, pressure/dynamics, custom tips and persistent drawing. |
| Optional integrations | [web_panel](web_panel.cpp), [web_script](web_script.cpp), [substance_swatches](substance_swatches.cpp), [usd_roundtrip](usd_roundtrip.cpp) | HTML/script, Substance and USD. SDK/runtime/assets must be available for the relevant example. |

## What a sketch cannot certify

- Core reconciliation, task scheduling, resource lifetime, decoder errors and
  cache bookkeeping have library tests. A visible example uses those systems
  but cannot demonstrate every failure or lifetime boundary.
- Qt controls, browser selection, keyboard focus, live compilation, capture
  delivery and thumbnail updates belong to the host window. Use the window
  and reload lanes; rendering a canvas plate does not exercise them.
- Video output must be encoded and probed. Displaying decoded frames is not
  encode coverage. GPU parity needs the device lane; a raster image is not a
  substitute.
- Typography examples do not enumerate every OpenType tag, locale, font
  fallback policy, breaker option or combination of annotations. The feature
  catalog and its library tests remain the detailed contract.

Keep an API example when a new capability would otherwise be invisible.
Extend an existing small example when it can demonstrate the behavior
clearly; a new exported symbol alone does not require another sketch or
another benchmark.
