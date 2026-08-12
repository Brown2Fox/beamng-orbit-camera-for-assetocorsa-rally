# beamng-orbit-camera

BeamNG-style orbit camera for Assetto Corsa Rally, implemented as a UE4SS C++ mod.

The camera logic is based on the BeamNG-style orbit behavior from the original Assetto Corsa implementation and includes movement-following heading, manual orbit/zoom, recentering, dynamic FOV/pitch/height, body-geometry references, and four-ray camera collision with the FULL/EDGE optimization.

## Repository layout

```text
beamng-orbit-camera/
├── .clang-format
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
├── Data/
│   └── config.ini
├── Source/
│   ├── CMakeLists.txt
│   ├── dllmain.cpp
│   ├── BeamNGOrbitCameraMod.cpp
│   ├── BeamNGOrbitCameraMod.hpp
│   ├── CameraHelpers/
│   ├── Config/
│   ├── Core/
│   └── Diagnostics/
└── tools/
    └── build.ps1
```

Local-only directories are intentionally not tracked:

```text
build/
RE-UE4SS/
```

`RE-UE4SS` must be the UE4SS checkout compatible with the runtime used by Assetto Corsa Rally. The C++ mod ABI should match that UE4SS build, so pin the exact revision before publishing binaries.

## Preparing UE4SS

By default the build expects:

```text
beamng-orbit-camera/RE-UE4SS/
```

You can instead keep the checkout elsewhere and pass `-UE4SSDir` to the build script.

## Build profiles

The repository uses two mod-level profiles:

- `Development` — diagnostics compiled in.
- `Shipping` — diagnostics compiled out.

Both profiles still compile against UE4SS configuration `Game__Shipping__Win64`; `Development` and `Shipping` here describe this mod's diagnostics policy, not a different Unreal/CRT target.

From the repository root:

```powershell
.\tools\build.ps1 Development
.\tools\build.ps1 Shipping
```

Build output is kept under:

```text
build/Development/
build/Shipping/
```

## Deploy

`-Deploy` works with either profile:

```powershell
.\tools\build.ps1 Development -Deploy -GameDir "D:\SteamLibrary\steamapps\common\Assetto Corsa Rally"
.\tools\build.ps1 Shipping -Deploy -GameDir "D:\SteamLibrary\steamapps\common\Assetto Corsa Rally"
```

You can set `ASSETO_CORSA_RALLY_HOME` once and then omit `-GameDir`:

```powershell
$env:ASSETO_CORSA_RALLY_HOME = "D:\SteamLibrary\steamapps\common\Assetto Corsa Rally"
.\tools\build.ps1 Shipping -Deploy
```

Or pass the exact destination explicitly:

```powershell
.\tools\build.ps1 Shipping -Deploy -GameModDll "D:\...\ue4ss\Mods\BeamNGOrbitCamera\dlls\main.dll"
```

The script builds `BeamNGOrbitCamera.dll`, deploys it as UE4SS `main.dll`, and seeds `Data/config.ini` only when the deployed config does not already exist.


## Configuration

The repository template lives at:

```text
Data/config.ini
```

At runtime the mod reads and writes:

```text
Mods/BeamNGOrbitCamera/Data/config.ini
```

Deploy preserves an existing runtime config.



## Runtime controls

- `Delete` — orbit camera on/off. The camera starts disabled.
- `Insert` — collision on/off. Collision starts enabled.
  This also acts as a failsafe if a new session starts with the camera trapped by collision geometry.
- Right stick — orbit.
- `RB/R1 + Left Stick Y` — zoom.
- `R3` — recenter/reset.
- `L3` — recenter while keeping pitch/distance.
- NumPad orbit/zoom controls remain available.
- Development only: `End` camera manager dump, `Ctrl+B` vehicle component census, and compact 1 Hz camera telemetry.

## Diagnostics

Development builds define:

```text
BEAMNG_ORBIT_CAMERA_DIAGNOSTICS=1
```

Shipping builds define:

```text
BEAMNG_ORBIT_CAMERA_DIAGNOSTICS=0
```

Diagnostic-only code is removed at compile time in Shipping builds.

## License

MIT. See `LICENSE`.
