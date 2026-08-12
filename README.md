# beamng-orbit-camera

BeamNG-style orbit camera for Assetto Corsa Rally, implemented as a UE4SS C++ mod.

> [!WARNING]
> This is an experimental, unofficial mod. It hooks into the running game and depends on undocumented Assetto Corsa Rally and UE4SS internals. Game updates, UE4SS changes, other mods, or an unsupported binary combination may cause incorrect camera behavior, failure to load, crashes, or corrupted settings. Use it at your own risk and keep backups of files you replace.

## Tested compatibility

This is the currently tested combination, not a promise of compatibility with later builds:

| Component | Tested version |
| --- | --- |
| BeamNG Orbit Camera | `0.12.12` |
| Assetto Corsa Rally (Steam app `3917090`) | Steam build ID `24097451`, tested 2026-08-12 |
| Unreal Engine detected by UE4SS | `5.4` |
| UE4SS | `v3.0.1 Beta #0`, Git commit [`1c1a1497f942c707f47ba668db75b25e86f6c08a`](https://github.com/UE4SS-RE/RE-UE4SS/commit/1c1a1497f942c707f47ba668db75b25e86f6c08a), `Game__Shipping__Win64` |

The expected UE4SS log header is:

```text
UE4SS - v3.0.1 Beta #0 - Git SHA #1c1a1497
UE4SS Build Configuration: Game__Shipping__Win64 (MSVC)
```

Do not replace this UE4SS build with the newest release unless this project explicitly updates its tested revision and the mod is rebuilt against it.

## Installation for players

Players do not need Visual Studio, CMake, Rust, or the source code. They need:

- a supported Assetto Corsa Rally build;
- the pinned UE4SS build listed above, preferably the exact bundle supplied with the mod release;
- the release build of `main.dll` and `Data/config.ini`.

Install UE4SS under the game's `acr/Binaries/Win64` directory. Then place the mod files as follows:

```text
acr/Binaries/Win64/ue4ss/Mods/BeamNGOrbitCamera/
├── Data/
│   └── config.ini
└── dlls/
    └── main.dll
```

Add this line to `acr/Binaries/Win64/ue4ss/Mods/mods.txt`:

```text
BeamNGOrbitCamera : 1
```

After starting the game, check `acr/Binaries/Win64/ue4ss/UE4SS.log` for the expected UE4SS version and for `[BeamNGOrbitCamera]` messages. The orbit override starts disabled and only replaces the game's `ThirdFar` camera after `Delete` is pressed.

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

`RE-UE4SS` must be the exact pinned checkout documented below. A C++ mod built against a different UE4SS revision may have an incompatible ABI even when it compiles successfully.

## Developer setup

Build requirements:

- Windows;
- Visual Studio 2022 version 17.13 or newer with Desktop development with C++ and MSVC toolset 14.43 or newer;
- CMake 3.22 or newer;
- Rust 1.73 or newer;
- Git and network access for the initial dependency checkout.

By default the build expects:

```text
beamng-orbit-camera/RE-UE4SS/
```

Prepare the exact UE4SS revision and its recorded submodules:

```powershell
git clone https://github.com/UE4SS-RE/RE-UE4SS.git RE-UE4SS
git -C RE-UE4SS checkout 1c1a1497f942c707f47ba668db75b25e86f6c08a
git -C RE-UE4SS submodule update --init --recursive
```

Do not use `git submodule update --remote`. You can keep the checkout elsewhere and pass `-UE4SSDir` to the build script. CMake rejects a Git checkout whose `HEAD` does not match the pinned revision; source archives without Git metadata produce a warning because their revision cannot be verified.

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

After any Assetto Corsa Rally or UE4SS update, treat the previous binary as unsupported until both Development and Shipping builds compile and the runtime checks below pass.

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

- `Delete` — orbit camera on/off. The camera starts disabled and replaces only the game's `ThirdFar` camera.
- `Insert` — collision on/off. Collision starts enabled.
  This also acts as a failsafe if a new session starts with the camera trapped by collision geometry.
- Right stick — orbit.
- `RB/R1 + Left Stick Y` — zoom.
- `R3` — recenter/reset.
- `L3` — recenter while keeping pitch/distance.
- NumPad orbit/zoom controls remain available.
- Development only: `End` camera manager dump, `Ctrl+B` vehicle component census, and compact 1 Hz camera telemetry.

## Compatibility risks

The mod is relatively fragile because it is an in-process C++ mod rather than a supported game plugin. The main update-sensitive dependencies are:

- the UE4SS C++ ABI and hook implementation;
- the game class names `BC_CarPlayerCameraManager_C` and `CameraModifier_CameraShake`;
- the `BlueprintModifyCamera` function and its `NewViewLocation`, `NewViewRotation`, and `NewFOV` parameters;
- `GetCurrentCamera` and the exact `ThirdFar` component name;
- the `ViewTarget.Target`/`ViewTarget.POV` reflected layout;
- the vehicle `Body_Component`, mesh-bound functions, transforms, and axis conventions;
- `KismetSystemLibrary.LineTraceSingle`, its reflected parameter layout, and the Camera trace channel.

Most reflected-property failures are checked and fail without writing a custom camera result. Body-geometry failures fall back to calibrated dimensions, and collision-layout failures disable collision with a warning. An incompatible UE4SS ABI is the highest-risk case because it can fail or crash before the mod's own checks and messages run.

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

At startup, the mod logs its version, tested Assetto Corsa Rally build, pinned UE4SS revision, and target camera. It also reports configuration errors, missing XInput, unavailable collision reflection, failure to discover the camera manager when enabling the mod, an unavailable `GetCurrentCamera` function, and camera-output reflection failures.

For compatibility testing after an update:

1. Confirm the UE4SS version and build configuration at the top of `UE4SS.log`.
2. Confirm the mod startup line reports the expected compatibility target.
3. Enter a driving session, select `ThirdFar`, press `Delete`, and look for `ThirdFar override active`.
4. Verify that `ThirdNear`, `FirstPerson`, `Gauge`, `Bonnet`, `Bumper`, pit, and cinematic cameras remain untouched.
5. In a Development build, use `End` to dump the camera manager and `Ctrl+B` to inspect vehicle components if names or layouts changed.

## License

MIT. See `LICENSE`.
