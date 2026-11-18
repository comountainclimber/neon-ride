# Neon Ride · Unreal greybox

Open `NeonRide.uproject` with Unreal Engine 5.8.4, build the project module when prompted, then press **Play**. The game mode creates the course as play begins on the engine's Entry map. A source checkout on Windows requires Visual Studio 2022 C++ Build Tools and a Windows SDK.

| Ride | Keyboard | Gamepad |
| --- | --- | --- |
| Throttle | W | Right trigger |
| Brake | S | Left trigger |
| Steer | A / D | Left stick |
| Restart | R | Top face button (Y / Triangle) |

You start in the right lane at 122 km/h. Release the throttle to return to cruise speed. Hold the brake to stop. A run resets at 1.1 km so you can repeat the same corner while tuning.

## First ride

1. On the opening straight, hold W and ease into the passing lane. Release steering to settle at the new lateral position.
2. Carry speed through the left turn. The straight lasts 300 m, then curvature builds over 90 m to a 340 m radius. Watch the cockpit against the horizon to judge roll.
3. Brake mid-corner to feel the bike stand up as speed falls. Press R to repeat immediately.

## Project layout

- `Source/NeonRide/RideHandling.*` holds the fixed-step longitudinal and lateral model in metres and seconds. Steering commands lateral speed. Lateral acceleration and route curvature determine lean.
- `Skyway.*` builds the spline and instanced greybox geometry and converts to Unreal centimetres. The deck is 10 m wide, with a 5° bank at full curvature.
- `RidePawn.*` applies the input bindings, pose and first-person camera. The camera rolls 42% with the bike, looks into the turn, and responds to acceleration and speed.
- `NeonRideGameMode.*` creates the start, lights, and telemetry HUD. The course and cockpit use the engine's built-in cube and basic shape material.

The 1.1 km repeat course and desktop controls are tuning defaults. Traffic, crash recovery, film assets, rain and audio are later milestones in the repository's `UNREAL_HANDOFF.md`.

## Verification

The Unreal automation group `NeonRide` contains a numerical handling test and a game-world test that exercises the possessed pawn's throttle, steering, brake, camera, finish and restart bindings. Run the group in a standalone game session on the default map.
