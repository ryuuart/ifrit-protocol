# Sketch presentation audit

The catalogue has three presentation needs. Comparison sheets use the shared
SketchKit page, captions and wells. Reference studies preserve the typography,
colour, arrangement and proportions of their source. Drawing and 3D sketches
keep the image itself as the subject; a page is not required around every image.

This inventory covers **232 managed entries**. The untracked `spell_circle`
project is user-owned and excluded. Each entry below has an explicit disposition;
retaining a visual treatment is not a claim that the sketch was redesigned.

## Refined families

- The twelve transport and schema demonstrations use `instrument()` to place a
  bounded preview beside a readable connection or source panel. The picture keeps
  its authored coordinate space. Titles explain what the sketch does, notes explain
  the visual encoding, and footers name the sender or editable source.
- Sixty-nine API and data comparisons use `specimenTheme()` for stronger title
  hierarchy and brighter notes at compact margins. Fixed specimen geometry and
  subject-specific colours remain local. Short headings leave API detail to the
  explanation, captions and source.
- The house theme is unchanged. Reference reconstructions, composed Python examples,
  full-frame drawings and 3D studies retain their visual vocabulary.

## Review coverage

The first review used available cached thumbnails as contact sheets. These show
composition and hierarchy, but do not establish that a current source renders.
A rendered plate in the table means the current source was rebuilt and captured
in the headless GPU lane. All 81 changed sketches and ten retained entries that
had no cached preview have a fresh plate: **91 rendered, 141 cached previews**.
Native application chrome is reviewed separately.

The refinement includes extra caption-to-footer clearance on 24 dense sheets,
aligned blur examples, shorter Japanese typography labels, an open blend curve
contained within its well, and a scatter word with room for its full travel.

The review artifacts contain individual plates and contact sheets under
`/private/tmp/sketch-design-plates/`, with `results.json` for the initial sweep
`density-results.json` for corrected layouts, and `final-results.json` for
the final scatter and billboard captures. A current source can be
captured independently with:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless /tmp/sketch-plates --gpu --sketch <key>
```

The billboard GPU sweep completed after exceeding the capture runner's timeout;
its rendering performance remains an explicit item in the application findings.

| Disposition | Entries |
|---|---:|
| Refined · compact specimen | 69 |
| Refined · instrument | 12 |
| Retain · 3D subject | 14 |
| Retain · Python composition | 18 |
| Retain · authored catalog | 20 |
| Retain · drawing as the subject | 31 |
| Retain · focused specimen | 27 |
| Retain · reference study | 41 |

## Per-entry disposition

| Sketch | Disposition | Visual evidence |
|---|---|---|
| `aero_desktop` | Retain · authored catalog | Cached preview |
| `y2k_chrome` | Retain · authored catalog | Cached preview |
| `daemon_console` | Retain · authored catalog | Rendered plate |
| `loot_grid` | Retain · authored catalog | Cached preview |
| `passive_tree` | Retain · authored catalog | Cached preview |
| `persona_menu` | Retain · authored catalog | Cached preview |
| `world_hud` | Retain · authored catalog | Cached preview |
| `flourish` | Retain · authored catalog | Cached preview |
| `night_network` | Retain · authored catalog | Cached preview |
| `zellige` | Retain · authored catalog | Cached preview |
| `annotated_margin` | Retain · authored catalog | Cached preview |
| `beethoven` | Retain · authored catalog | Cached preview |
| `bousen` | Retain · authored catalog | Cached preview |
| `chrome_type` | Retain · authored catalog | Cached preview |
| `horizontal_flow` | Retain · authored catalog | Cached preview |
| `manuscript` | Retain · authored catalog | Cached preview |
| `mawarikomi` | Retain · authored catalog | Cached preview |
| `ruby_kenten` | Retain · authored catalog | Cached preview |
| `tategaki` | Retain · authored catalog | Cached preview |
| `threaded_story` | Retain · authored catalog | Cached preview |
| `artnet_lights` | Refined · instrument | Rendered plate |
| `data_scales` | Retain · focused specimen | Cached preview |
| `data_sources` | Refined · compact specimen | Rendered plate |
| `feed_events` | Refined · instrument | Rendered plate |
| `feed_sky` | Refined · instrument | Rendered plate |
| `grpc_watch` | Refined · instrument | Rendered plate |
| `midi_pads` | Refined · instrument | Rendered plate |
| `osc_desk` | Refined · instrument | Rendered plate |
| `phone_sky` | Refined · instrument | Rendered plate |
| `quic_sky` | Refined · instrument | Rendered plate |
| `schema_scene` | Refined · instrument | Rendered plate |
| `serial_sensor` | Refined · instrument | Rendered plate |
| `shared_sky` | Refined · instrument | Rendered plate |
| `webrtc_sky` | Refined · instrument | Rendered plate |
| `bristle_current` | Retain · drawing as the subject | Cached preview |
| `p5_hello` | Retain · drawing as the subject | Cached preview |
| `p5_mixed_forms` | Retain · drawing as the subject | Cached preview |
| `p5_attractor_loom` | Retain · drawing as the subject | Cached preview |
| `p5_flow_field` | Retain · drawing as the subject | Cached preview |
| `p5_fractal_garden` | Retain · drawing as the subject | Cached preview |
| `p5_liquid_layers` | Retain · drawing as the subject | Cached preview |
| `p5_refractive_metaballs` | Retain · drawing as the subject | Cached preview |
| `observable_circle_packing` | Retain · drawing as the subject | Cached preview |
| `observable_circle_packing_contained` | Retain · drawing as the subject | Cached preview |
| `observable_fibonacci` | Retain · drawing as the subject | Cached preview |
| `observable_fibonacci_rectangles` | Retain · drawing as the subject | Cached preview |
| `observable_flowfield_1` | Retain · drawing as the subject | Cached preview |
| `observable_flowfield_2` | Retain · drawing as the subject | Cached preview |
| `observable_flowfield_3` | Retain · drawing as the subject | Cached preview |
| `observable_grid` | Retain · drawing as the subject | Cached preview |
| `observable_l_system` | Retain · drawing as the subject | Cached preview |
| `observable_l_system_tree` | Retain · drawing as the subject | Cached preview |
| `observable_noise` | Retain · drawing as the subject | Cached preview |
| `observable_noise_map` | Retain · drawing as the subject | Cached preview |
| `observable_random_walker` | Retain · drawing as the subject | Cached preview |
| `observable_reaction_diffusion` | Retain · drawing as the subject | Cached preview |
| `observable_reynolds_steering` | Retain · drawing as the subject | Cached preview |
| `bristle_bloom` | Retain · drawing as the subject | Cached preview |
| `brush_botanical_study` | Retain · drawing as the subject | Cached preview |
| `brush_custom` | Retain · drawing as the subject | Cached preview |
| `brush_dynamics` | Retain · drawing as the subject | Cached preview |
| `brush_engine_atlas` | Retain · drawing as the subject | Cached preview |
| `brush_live_tutorial` | Retain · drawing as the subject | Cached preview |
| `brush_rain` | Retain · drawing as the subject | Cached preview |
| `brushwork_currents` | Retain · drawing as the subject | Cached preview |
| `blend_options` | Refined · compact specimen | Rendered plate |
| `blur_falloff` | Refined · compact specimen | Rendered plate |
| `border_weave` | Refined · compact specimen | Rendered plate |
| `bullets_dropcap` | Refined · compact specimen | Rendered plate |
| `cascade` | Refined · compact specimen | Rendered plate |
| `channel_bind` | Refined · compact specimen | Rendered plate |
| `cjk_rules` | Refined · compact specimen | Rendered plate |
| `codec_roundtrip` | Refined · compact specimen | Rendered plate |
| `compute_variant` | Retain · 3D subject | Cached preview |
| `contour_poses` | Refined · compact specimen | Rendered plate |
| `corner_notched` | Refined · compact specimen | Rendered plate |
| `coverage_boundary` | Refined · compact specimen | Rendered plate |
| `crossing_rule` | Refined · compact specimen | Rendered plate |
| `crt_bloom` | Refined · compact specimen | Rendered plate |
| `decay_step` | Refined · compact specimen | Rendered plate |
| `ember_decode` | Retain · focused specimen | Cached preview |
| `encode_write` | Refined · compact specimen | Rendered plate |
| `env_faces` | Refined · compact specimen | Rendered plate |
| `env_lanes` | Refined · compact specimen | Rendered plate |
| `exact_tangent` | Refined · compact specimen | Rendered plate |
| `exr_channels` | Refined · compact specimen | Rendered plate |
| `feed_vitals` | Refined · compact specimen | Rendered plate |
| `floating_panels` | Retain · focused specimen | Cached preview |
| `formation_bands` | Refined · compact specimen | Rendered plate |
| `frame_grid` | Refined · compact specimen | Rendered plate |
| `frame_inputs` | Refined · compact specimen | Rendered plate |
| `fx_scatter_mix` | Refined · compact specimen | Rendered plate |
| `geo_groups` | Refined · compact specimen | Rendered plate |
| `gif_frames` | Refined · compact specimen | Rendered plate |
| `grid_layouts` | Refined · compact specimen | Rendered plate |
| `half_float` | Refined · compact specimen | Rendered plate |
| `hit_slots` | Retain · focused specimen | Cached preview |
| `hub_reload` | Refined · compact specimen | Rendered plate |
| `import_native` | Retain · 3D subject | Cached preview |
| `keeps_and_frames` | Refined · compact specimen | Rendered plate |
| `lane_retarget` | Refined · compact specimen | Rendered plate |
| `live_settling` | Refined · compact specimen | Rendered plate |
| `material_atlas` | Refined · compact specimen | Rendered plate |
| `material_slots` | Refined · compact specimen | Rendered plate |
| `matte_luma` | Refined · compact specimen | Rendered plate |
| `mesh_generators` | Retain · focused specimen | Cached preview |
| `mesh_normal_bridge` | Retain · focused specimen | Cached preview |
| `net_policy` | Refined · compact specimen | Rendered plate |
| `nine_slice` | Refined · compact specimen | Rendered plate |
| `ocio_view` | Refined · compact specimen | Rendered plate |
| `optical_kerning` | Retain · focused specimen | Cached preview |
| `over_under` | Refined · compact specimen | Rendered plate |
| `painter_gpu` | Refined · compact specimen | Rendered plate |
| `path_booleans` | Retain · focused specimen | Cached preview |
| `pattern_sequence` | Refined · compact specimen | Rendered plate |
| `pixfont_dotsprite` | Refined · compact specimen | Rendered plate |
| `place_repeat_tiles` | Refined · compact specimen | Rendered plate |
| `pop_billboards` | Refined · compact specimen | Rendered plate |
| `pop_deform` | Refined · compact specimen | Rendered plate |
| `pop_math` | Refined · compact specimen | Rendered plate |
| `pop_order` | Refined · compact specimen | Rendered plate |
| `pop_prims` | Retain · focused specimen | Cached preview |
| `pop_stamps` | Retain · focused specimen | Cached preview |
| `rich_slot_reserve` | Refined · compact specimen | Rendered plate |
| `routers_straight` | Refined · compact specimen | Rendered plate |
| `routes_probe` | Refined · compact specimen | Rendered plate |
| `sdf_star` | Refined · compact specimen | Rendered plate |
| `slang_portable` | Refined · compact specimen | Rendered plate |
| `spacing_passes` | Retain · focused specimen | Cached preview |
| `surface_components` | Retain · focused specimen | Cached preview |
| `svg_silhouette` | Refined · compact specimen | Rendered plate |
| `ticker_lanes` | Refined · compact specimen | Rendered plate |
| `tile_map` | Refined · compact specimen | Rendered plate |
| `usd_roundtrip` | Refined · compact specimen | Rendered plate |
| `volatility_cost` | Refined · compact specimen | Rendered plate |
| `warichu_placeholder` | Refined · compact specimen | Rendered plate |
| `web_script` | Refined · compact specimen | Rendered plate |
| `yarn_marquee` | Refined · compact specimen | Rendered plate |
| `card_flip` | Retain · focused specimen | Cached preview |
| `guest_body` | Retain · 3D subject | Cached preview |
| `guest_picture` | Retain · focused specimen | Cached preview |
| `sticker_collection` | Retain · focused specimen | Cached preview |
| `video_compose` | Retain · focused specimen | Cached preview |
| `video_compositing` | Retain · focused specimen | Cached preview |
| `python_botanical_study` | Retain · Python composition | Cached preview |
| `python_compose_stamps` | Retain · Python composition | Cached preview |
| `python_dashboard` | Retain · Python composition | Cached preview |
| `python_data_garden` | Retain · Python composition | Cached preview |
| `python_hello` | Retain · Python composition | Cached preview |
| `python_hello_compose` | Retain · Python composition | Cached preview |
| `python_kit_specimen` | Retain · Python composition | Cached preview |
| `python_liquid_glass` | Retain · Python composition | Cached preview |
| `python_liquid_layers` | Retain · Python composition | Cached preview |
| `python_live_signals` | Retain · Python composition | Cached preview |
| `python_memo_station` | Retain · Python composition | Cached preview |
| `python_mesh_observatory` | Retain · Python composition | Cached preview |
| `python_motion_signals` | Retain · Python composition | Cached preview |
| `python_observable_flowfield` | Retain · Python composition | Cached preview |
| `python_observable_l_system` | Retain · Python composition | Cached preview |
| `python_observable_reaction_diffusion` | Retain · Python composition | Cached preview |
| `python_observable_reynolds` | Retain · Python composition | Cached preview |
| `python_orbits` | Retain · Python composition | Cached preview |
| `dart_flight` | Retain · 3D subject | Cached preview |
| `deformed_cloud` | Retain · 3D subject | Cached preview |
| `first_light` | Retain · 3D subject | Cached preview |
| `glow_trail` | Retain · 3D subject | Cached preview |
| `key_light` | Retain · 3D subject | Cached preview |
| `lantern_room` | Retain · 3D subject | Cached preview |
| `material_lab` | Retain · 3D subject | Cached preview |
| `reflection_lab` | Retain · 3D subject | Cached preview |
| `scattered_model` | Retain · 3D subject | Cached preview |
| `scene_surfaces` | Retain · 3D subject | Cached preview |
| `set_stagger` | Retain · 3D subject | Cached preview |
| `bound_lane` | Refined · compact specimen | Rendered plate |
| `curve_shelf` | Refined · compact specimen | Rendered plate |
| `field_shelf` | Refined · compact specimen | Rendered plate |
| `gerstner_grid` | Retain · focused specimen | Cached preview |
| `kinetic_card` | Refined · compact specimen | Rendered plate |
| `noise_shelf` | Refined · compact specimen | Rendered plate |
| `paint_shelf` | Refined · compact specimen | Rendered plate |
| `paragraph_paints` | Refined · compact specimen | Rendered plate |
| `paragraph_sheet` | Retain · focused specimen | Cached preview |
| `shape_tour` | Retain · focused specimen | Cached preview |
| `stroke_atlas` | Retain · focused specimen | Cached preview |
| `text_paints` | Refined · compact specimen | Rendered plate |
| `ui_particles` | Retain · focused specimen | Cached preview |
| `hello` | Retain · focused specimen | Cached preview |
| `shapeworks_lab` | Retain · focused specimen | Cached preview |
| `stock_materials` | Retain · focused specimen | Cached preview |
| `substance_swatches` | Retain · focused specimen | Cached preview |
| `web_panel` | Retain · focused specimen | Cached preview |
| `dunhuang_star_chart` | Retain · reference study | Rendered plate |
| `sigillum_aemeth` | Retain · reference study | Rendered plate |
| `thunder_fulu` | Retain · reference study | Rendered plate |
| `eva_magi_defense` | Retain · reference study | Cached preview |
| `eva_magi_deliberation` | Retain · reference study | Cached preview |
| `eva_magi_interior` | Retain · reference study | Cached preview |
| `lain_navi` | Retain · reference study | Cached preview |
| `astral_tome` | Retain · reference study | Cached preview |
| `bg3_dice_roll` | Retain · reference study | Cached preview |
| `ds2_bench` | Retain · reference study | Rendered plate |
| `fallout2_charsheet` | Retain · reference study | Rendered plate |
| `ksp_mapview` | Retain · reference study | Rendered plate |
| `psx_doom_fire` | Retain · reference study | Cached preview |
| `thaumonomicon` | Retain · reference study | Cached preview |
| `vagrant_story_target` | Retain · reference study | Cached preview |
| `xcom_battlescape` | Retain · reference study | Cached preview |
| `genesis_fire` | Retain · reference study | Cached preview |
| `hitman_verlet` | Retain · reference study | Cached preview |
| `slitscan_2001` | Retain · reference study | Cached preview |
| `vertigo_titles` | Retain · reference study | Cached preview |
| `black_watch` | Retain · reference study | Cached preview |
| `cosmati` | Retain · reference study | Cached preview |
| `kumiko_asanoha` | Retain · reference study | Cached preview |
| `penrose_paving` | Retain · reference study | Rendered plate |
| `chaucer_astrolabe` | Retain · reference study | Rendered plate |
| `chevreul_circle` | Retain · reference study | Cached preview |
| `chladni_tab1` | Retain · reference study | Cached preview |
| `minard_1869` | Retain · reference study | Cached preview |
| `nightingale_coxcomb` | Retain · reference study | Cached preview |
| `cde_motif` | Retain · reference study | Cached preview |
| `spacejam_1996` | Retain · reference study | Rendered plate |
| `twoadvanced_equipment` | Retain · reference study | Cached preview |
| `twoadvanced_v3` | Retain · reference study | Cached preview |
| `twoadvanced_v4` | Retain · reference study | Cached preview |
| `winamp_base` | Retain · reference study | Cached preview |
| `axis_ripple` | Retain · reference study | Cached preview |
| `elastic_type` | Retain · reference study | Cached preview |
| `karaoke_wipe` | Retain · reference study | Cached preview |
| `matrix_rain` | Retain · reference study | Cached preview |
| `rota_convocationis` | Retain · reference study | Cached preview |
| `shipping_forecast` | Retain · reference study | Cached preview |
