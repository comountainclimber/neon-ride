# Neon Ride · Skyway district

Open `NeonRide.uproject` in Unreal Engine 5.8.4 and press **Play**. A source checkout on Windows needs Visual Studio 2022 C++ Build Tools and a Windows SDK. The game creates its district on the engine's Entry map when play begins.

Start on the film's elevated skyway. Ride into the long left curve to find the Overlook, or brake at the amber gantry and turn down the market ramp. The terrace lies 30 m below, with open plazas and streets between city blocks. Two more ramps connect the terrace to the Transit Yard and Overlook. All three ramps work in either direction.

| Ride | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Throttle / brake | W / S | Right / left trigger |
| Steer | A / D | Left stick |
| Look around | Mouse | Right stick |
| Face forward | C | Press right stick |
| Toggle district map | M | Menu / Options |
| Return to the skyway entrance | R | Y / Triangle |

Release the throttle to coast. Brake to park and use steering to turn around, even at a standstill. The map shows street blocks, ramps, landmarks and your heading. Ambient cabs and freighters circulate between the two levels. Vehicle contact slows the bike in place.

## Implementation

- `District.*` defines driveable surfaces, block footprints, landmarks' locations and the traffic loop in metres. Surface selection respects elevation where streets cross beneath the skyway.
- `RideHandling.*` implements throttle, braking and free heading at 120 Hz. Turn rate and speed determine bike lean.
- `RidePawn.*` places the bike and independent first-person camera. The helmet rolls 42% with the bike and the lens widens with speed.
- `Skyway.*` builds the film's 300 m straight, 90 m curvature transition and 340 m-radius left curve, plus a connected lower district. The greybox uses the engine's built-in cube and material.
- `TrafficRun.*` handles street traffic. `NeonRideGameMode.*` supplies spawn, light, location display and map.

Run the `NeonRide.Exploration` automation group in a standalone game session to check controls, ramp traversal, vehicle occupancy, map toggle and camera behavior. The Blender film and `UNREAL_HANDOFF.md` on the repository's main branch are the visual references for the art pass.
