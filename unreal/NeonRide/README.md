# Neon Ride · Night Circuit

Ride a 5.28 km elevated circuit through Rainline, Glass Heights, Violet Reach and Lower Ring. The route climbs from 26 to 85 metres, banks through the skyline and returns to the start. Fourteen cabs and freighters share the lanes. Hitting traffic or striking a barrier at speed can eject Vesper. The rider and bike bounce and slide independently, and the cockpit returns after recovery.

| Action | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Accelerate / brake | W / S | Right / left trigger |
| Turn the motorcycle | A / D | Left stick |
| Hold nitro | Shift | X / Square |
| Look around | Mouse | Right stick |
| Face forward | C | Press right stick |
| Pause, resume, save and quit | Esc | View / Create |
| Restart at the circuit entrance | R | Y / Triangle |

The two original mirror inserts show live views of the road and traffic behind the bike from a shared rear-facing camera. The physical dashboard shows speed, engine revs and nitro charge. Four engine layers respond to RPM, throttle load, shifts, braking and boost. The packaged game's audio attribution is in NeonRide/Content/Credits/engine-credits.txt. A full tank delivers about 3.8 seconds of boost. Let go to recharge after a 1.35-second cooldown. The tank refills in 8.5 seconds. Release the steering input to hold your world heading. You steer through each bend. Pausing or quitting saves your distance, lane, heading, lap count and charge. Your next session starts at rest.

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

`Source/NeonRide/CircuitCore.*` runs the same 120 Hz simulation natively and as WebAssembly in the browser. The three road PBR maps in SourceArt/Circuit/ decode the browser albedo, normal and roughness images without changing their pixels. Both builds use the same baked `scenery.json` placements. The Unreal copy is staged from `Content/Circuit/`. Positions and dimensions in the manifest are in metres, with Unreal X/Y horizontal, Z up and yaw in radians. Film geometry, material textures, mesh axes and the helmed rider are imported into Unreal units at build time.

The Unreal road uses a continuous banked mesh, wet asphalt, lane paint, parapets and edge lighting. The rider camera rolls with the bike and the lens widens with speed. Vesper's crash camera tracks her pelvis as the animated character separates from the bike. The production rig is 1.75 m tall with 53 weighted Mixamo joints. The rig samples the crash clip from physical flight and landing. A separate center of mass drives the tumbling motorcycle. The 147 glass midrises use the shared browser placement manifest.

Run `Automation RunTests NeonRide.` in a standalone Development game to check lap closure, boost, traffic impacts, automatic recovery, save/restore and the physical instrument display. The visual tour writes cockpit and ejection captures to `Saved/Screenshots/Circuit/`.
