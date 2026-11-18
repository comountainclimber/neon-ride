# Unreal handoff: make Neon Ride playable

## Goal

Turn the 20-second film into something you can ride: first-person on the bike, throttle and lean under
player control, passing slower traffic on a rain-soaked skyway. The film
([`renders/neon-ride-final.mp4`](renders/neon-ride-final.mp4)) is the look and feel target. Watch it first.

The first milestone is a greybox that rides well. Looking like the film comes after.

## What carries over, and what does not

**Carries over as files**

- `models/*.glb`: all 18 Tripo models, with 4K base-colour textures embedded.
- `textures/`, `signs/`: road, facade and skyline paintings, and the glyph alphabet used on every sign.
- `audio/wav/`: the CC0 recordings used in the mix (rain, wind, engines, thunder).

**Exists only as Blender Python and must be rebuilt**

- The skyway, city layout and all placement: procedural in `scripts/build_ride.py`.
- The hologram jellyfish (`scripts/jelly.py`): shape keys, 64 tentacles and 4 ribbon arms animated in code.
- The dash display (`scripts/hud.py`): speed arc, glyph digits, horizon bar, lane radar.
- Rain (a looping streak mesh), haze, wet-road shading, bloom.
- All motion: it is keyframed per frame from formulas, not simulated.

## Known problems with the assets

- **Heavy meshes.** Each GLB is 15 to 26 MB with no LODs and no collision. The Blender script decimates on
  load (`glb_mesh(name, ratio)`, ratios 0.1 to 0.5). Expect to decimate or use Nanite, and to build collision.
- **No emissive maps.** Tripo bakes the glow into base colour. The film fakes emission in the shader:
  `strip_glow` masks by saturation and `lum_glow` by luminance (both in `build_ride.py`). Unreal materials
  need the same trick or authored emissive masks.
- **Normalized scale.** Tripo models are roughly 1 unit across. Real sizes are set in the script; see the table.
- **Orientation varies per model.** Yaw offsets are set per kind in `CAR_KINDS`, `ROAD_KINDS` and the
  `model_at(...)` calls.
- **Units.** Blender is metres, Z-up, right-handed. Unreal is centimetres, Z-up, left-handed.

## Numbers from the film

All from `scripts/build_ride.py`, which is the source of truth.

| Thing | Value |
| --- | --- |
| Deck | 10 m wide, two lanes: right lane centre +1.8 m, passing lane centre −2.6 m |
| Route | 300 m straight, 90 m curvature ramp, then a constant left curve of 340 m radius |
| Glass walls | start 372 m along the route; open balustrade before that |
| City ground | 160 m below the deck |
| Rider speed | 122 km/h cruise, 160 to 190 km/h on passes, about 200 km/h in the final charge (`SPEED_KEYS`) |
| Traffic speed | trucks 24 m/s (86 km/h), cabs 28 m/s (101 km/h); closing speeds 70 to 110 km/h |
| Bike | Tripo bike at scale 2.15; camera lens 17 mm on a full-frame sensor, widening to 14 mm at speed |
| Cab / truck length | 4.8 m / 13 m |
| Flying cars | 5 to 9 m long (4 kinds), 8 lanes above and beside the deck, all slower than the rider |
| Towers | about 120 instances of 3 kinds, scaled by height, standing on the ground plane |
| Jellyfish | 30 m bell radius, 50 m right of the road, rises from below the deck to 34 m above it |
| Lampposts | every 26 m on the open section, every 16 m inside the glass |

## Riding model in the film

This is what made the passes feel like a motorcycle; use it as the starting point for handling.

- **Lean follows lateral acceleration**: `lean = atan((v² · curvature − a_lateral) / g)`, capped near 54°.
  The film reaches about 35° on a lane flick and 52° in the fast curve.
- **Lane changes ease in**: lateral position follows a 7th-order ease over 2.2 to 2.6 s, so lean starts at zero
  rate, rolls in, snaps through upright and stands back up. An earlier cosine ease started at full lean rate
  and looked jerky.
- **Camera**: rolls with 42% of the lean, yaws slightly into the turn, pitches back and slides rearward under
  acceleration, and the field of view widens with speed. No shake.
- **Bike yaw** follows the velocity direction: `atan2(v_lateral, v)`.
- **Engine sound** pitch follows speed and gets louder under acceleration; wind level rises with speed
  (`scripts/mix_audio.py`).

In a game these become control laws: the player steers a target lean or lateral rate, and the same
relationships drive the bike roll, camera and audio.

## Look targets

- Night, heavy rain, wet road with strong streaked reflections of the lane lights.
- Lane lights shift from cool blue on the open section to teal green inside the glass.
- Dense lit towers on both sides, ring-crowned, with painted skyline backdrops behind them.
- Signage uses the invented glyph script only. No English, no real-world alphabets.
- Strong bloom and motion blur, AgX-style filmic tone mapping with a punchy look, slight vignette.
- The jellyfish is an advert, not a threat: a calm, slow-pulsing cyan wireframe hologram.

## Suggested order

1. **Greybox ride**: spline road with the route above, a bike pawn with throttle and lean, a first-person
   camera using the camera rules above. No art. Tune until riding feels good.
2. **Traffic**: cabs and trucks spawned ahead in both lanes at the film's speeds, with collision and a reset.
3. **Import assets**: bike and gloves first, then road vehicles, then lampposts, signs and towers. Decimate,
   add collision, build the emissive material.
4. **World streaming**: spawn and recycle road segments and city blocks along the spline so the ride can be endless.
5. **Atmosphere**: rain, wet road material, haze, bloom, the lane-light colour shift.
6. **Dash and audio**: speed, lean and lane radar on the dash; engine and wind driven by speed and throttle.
7. **Jellyfish and set pieces**: rebuild the hologram as a material and vertex animation.
8. **Game loop**: near-miss scoring, crash and restart, menu.

## Not decided

- Endless ride or a fixed course with an ending.
- Controls: keyboard, gamepad or both.
- Target platform and frame rate.
