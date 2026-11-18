# Neon Ride · Night Courier

Ride Vesper's motorcycle through Rainline, Floodworks and Glass Heights. A 5.28 km express ring connects the district streets and six staffed service bays. Twenty cabs and freighters share the network. A high-speed collision can throw Vesper from the motorcycle. The two slide independently. Walk to the marked motorcycle and remount when it is within 2.4 metres. Parcels stay aboard and their delivery clocks keep running.

Open dispatch to reserve up to two of the eight parcels offered each shift. Follow the marked pickup or choose an address on the map to explore. At a signed fork, ride in the right lane to take the branch or keep left to continue. Slow below 11 km/h in the right lane at the counter to collect or deliver. Timed parcels can still be delivered after the requested arrival. A first delivery to Aurora Cinema opens its service ramp from Floodworks to Glass Heights. Complete all eight deliveries to start the next manifest. Client history and ramp access carry forward.

The three authored shifts are First Night, Regular Service and First Light. Completing First Light restores Aurora Cinema's dawn screening. Free riding and repeat manifests continue afterwards.

| Action | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Accelerate / brake | W / S | Right / left trigger |
| Turn the motorcycle | A / D | Left stick |
| Hold nitro | Shift | X / Square |
| Look around | Mouse | Right stick |
| Face forward | C | Press right stick |
| Pause, resume, save and quit | Esc | View / Create |
| Dispatch and city map | M or Tab | D-pad up |
| Collect or deliver at a service bay | E | A / Cross |
| Walk after a crash | W / A / S / D | Left stick |
| Remount beside the motorcycle | E | A / Cross |
| Return to Rainline Repair | R | Y / Triangle |

The two original mirror inserts show live views of the road and traffic behind the bike from a shared rear-facing camera. The physical dashboard shows speed, engine revs and nitro charge. Four engine layers respond to RPM, throttle load, shifts, braking and boost. The packaged game's audio attribution is in NeonRide/Content/Credits/engine-credits.txt. A full tank delivers about 3.8 seconds of boost. Let go to recharge after a 1.35-second cooldown. The tank refills in 8.5 seconds. Release the steering input to hold your world heading. Pausing or quitting saves your road, lane, heading, lap count, charge, waypoint and shift. Reserving, collecting or delivering also saves immediately.

## Run and build

Open `NeonRide.uproject` in Unreal Engine 5.8.4 on Windows and press **Play**, then **Start ride**. The start and pause screens pair the original film frame with Vesper. Enter or the bottom controller face button activates the focused action. A source build uses Visual Studio C++ Build Tools with a Windows SDK. The default Entry map constructs the circuit automatically. The standalone packaged game launches from `Windows/NeonRide.exe` in the archive.

The editor imports all 18 film GLBs from the repository's `models/` folder or a prepared `SourceArt/Models/` folder. It imports textures, signs and the original rain and wind recordings from the repository or `SourceArt/`. Vesper's rigged animation and fallback mesh live in `SourceArt/Rider/`. Wait for the first import to finish before playing.

From PowerShell in this project directory, set `$ue` to the engine installation and run:

```powershell
$ue = 'D:/UE_5.8'
$p = (Resolve-Path NeonRide.uproject).Path
& "$ue/Engine/Build/BatchFiles/Build.bat" NeonRideEditor Win64 Development "-Project=$p" -WaitMutex
& "$ue/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $p -run=pythonscript "-script=$PWD/Tools/import_assets.py" -unattended
& "$ue/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$p" -nop4 -platform=Win64 -clientconfig=Development -build -cook -map=/Engine/Maps/Entry -stage -pak -archive "-archivedirectory=$PWD/../NeonRideBuild" -unattended
```

## Shared simulation and art

`Source/NeonRide/CircuitCore.*` and `CourierCore.*` run the same 120 Hz riding simulation, street graph and courier rules natively and as WebAssembly in the browser. Both builds share the base `scenery.json` and the district additions and clearances in `courier-scenery.json`. Unreal stages them from `Content/Circuit/`. Positions and dimensions in the manifests are in metres, with Unreal X/Y horizontal, Z up and yaw in radians.

The five road meshes use wet asphalt, lane paint, parapets and edge lighting, with open junctions and curbside platforms. The rider camera rolls with the bike and the lens widens with speed. Vesper's crash camera tracks her pelvis as the animated character separates from the bike. The production rig is 1.75 m tall with 53 weighted Mixamo joints. A separate center of mass drives the tumbling motorcycle.

Run `Automation RunTests NeonRide.` in a standalone Development game. The courier test reserves and collects two parcels, uses throttle, brake and steering to deliver to Floodworks, restores the active cargo from a save and verifies cinema ramp access. Captures are written under `Saved/Screenshots/Courier/`, `Circuit/` and `RearMirrors/`.
