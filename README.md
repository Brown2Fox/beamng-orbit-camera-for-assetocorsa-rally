# BeamNG Orbit Camera

An unofficial BeamNG-style orbit camera for Assetto Corsa Rally. It replaces the `ThirdFar` camera.

> [!WARNING]
> This is an experimental mod that depends on undocumented game and UE4SS internals. A game or UE4SS update may require a mod update.

## Installation

Download `BeamNGOrbitCamera-v<version>.zip` from [Releases](https://github.com/Brown2Fox/beamng-orbit-camera-for-assetocorsa-rally/releases) and extract it into the Assetto Corsa Rally directory containing `acr`. UE4SS is included and the mod is registered with the loader.

```text
Assetto Corsa Rally/
└── acr/
    ├── Binaries/Win64/
    │   ├── acr.exe
    │   ├── dwmapi.dll
    │   └── ue4ss/
    │       ├── UE4SS.dll
    │       └── Mods/
    │           ├── mods.txt
    │           └── BeamNGOrbitCamera/
    │               ├── Data/
    │               │   └── config.ini
    │               └── dlls/
    │                   └── main.dll
    └── Content/Paks/
        ├── BeamNGOrbitModifier.pak
        ├── BeamNGOrbitModifier.ucas
        └── BeamNGOrbitModifier.utoc
```

Start a driving session, select the `ThirdFar` camera, and press `Delete` to enable the mod. Settings are stored in `Mods/BeamNGOrbitCamera/Data/config.ini`.

### Manual installation with an existing UE4SS

If the supported UE4SS version is already installed, copy only `ue4ss/Mods/BeamNGOrbitCamera` and the three `BeamNGOrbitModifier` files from the archive. Add the following line to `acr/Binaries/Win64/ue4ss/Mods/mods.txt`:

```text
BeamNGOrbitCamera : 1
```

## Controls

| Action | Gamepad | Keyboard |
| --- | --- | --- |
| Enable or disable orbit camera | - | `Delete` |
| Enable or disable collision | - | `Insert` or NumPad `0` |
| Orbit | Right stick | NumPad `4`/`6` and `8`/`2` |
| Zoom | `RB/R1` + left stick Y | NumPad `9`/`3` |
| Adjust target height | - | NumPad `7`/`1` |
| Recenter | `R3` | NumPad `5` |
| Recenter, keep pitch and distance | `L3` | `Ctrl` + NumPad `5` |

The orbit camera starts disabled. Collision starts enabled.

## Tested versions

| Component | Version |
| --- | --- |
| BeamNG Orbit Camera | `0.13.4` |
| Assetto Corsa Rally | Steam build `24097451` |
| Unreal Engine | `5.6` |
| UE4SS | `v3.0.1 Beta #0`, commit [`2bfa839f`](https://github.com/UE4SS-RE/RE-UE4SS/commit/2bfa839fe2945f9230204145646b5be4ae84fde8), `Game__Shipping__Win64` |

Using a different UE4SS revision may cause an ABI mismatch even if the mod compiles.

## Uninstalling

To remove the complete bundled installation, delete:

```text
acr/Binaries/Win64/dwmapi.dll
acr/Binaries/Win64/ue4ss/
acr/Content/Paks/BeamNGOrbitModifier.pak
acr/Content/Paks/BeamNGOrbitModifier.ucas
acr/Content/Paks/BeamNGOrbitModifier.utoc
```

If UE4SS is also used by other mods, remove only BeamNG Orbit Camera:

1. Remove `BeamNGOrbitCamera : 1` from `ue4ss/Mods/mods.txt`.
2. Delete `ue4ss/Mods/BeamNGOrbitCamera`.
3. Delete the three `BeamNGOrbitModifier` files from `acr/Content/Paks`.

## Camera integration

The UE4SS C++ mod adds `BP_BeamNGOrbitModifier` to the `APlayerCameraManager` of `BC_CarPlayerCameraManager_C`.

When enabled in `ThirdFar`, it applies orbit, zoom, target height, and collision after `BlueprintModifyCamera`. The original camera pose is restored before the next update to avoid affecting the game's camera blending.

## Development Setup

Requirements:

- Windows and Visual Studio 2022 17.13 or newer with Desktop development with C++;
- CMake 3.22 or newer;
- Rust 1.73 or newer;
- Git;
- Unreal Engine 5.6 only when rebuilding the camera-modifier bundle.

From the repository root, clone the pinned UE4SS revision:

```powershell
git clone https://github.com/UE4SS-RE/RE-UE4SS.git RE-UE4SS
git -C RE-UE4SS checkout 2bfa839fe2945f9230204145646b5be4ae84fde8
git -C RE-UE4SS submodule update --init --recursive
```

Build the C++ mod from the repository root:

```powershell
.\tools\build.ps1 Development
.\tools\build.ps1 Shipping
```

`Development` includes extra diagnostics; `Shipping` compiles them out. Both target UE4SS `Game__Shipping__Win64`. Pass `-UE4SSDir <path>` if the pinned checkout is stored elsewhere.

### Camera asset bundle

In a UE 5.6 Blueprint project, add:

```text
/Game/Mods/BeamNGOrbitModifier/BP_BeamNGOrbitModifier
```

The Blueprint must inherit from `CameraModifier` and override `BlueprintModifyCamera`. Put a `PrimaryAssetLabel` in the same directory, enable `Label Assets in My Directory`, set `Cook Rule` to `Always Cook`, and assign a dedicated chunk such as `1001`.

Enable `Use Pak File`, `Use Io Store`, and `Generate Chunks`, also set `IsEditorOnly` for `PrimaryAssetLabel` to false in Asset Manager. Then package for Windows. Rename the resulting chunk files and place them in the repository's `Data` directory:

```text
pakchunk1001-Windows.pak  -> BeamNGOrbitModifier.pak
pakchunk1001-Windows.ucas -> BeamNGOrbitModifier.ucas
pakchunk1001-Windows.utoc -> BeamNGOrbitModifier.utoc
```

## Deploy

Build and deploy the DLL, configuration, and asset bundle with:

```powershell
.\tools\build.ps1 Development -Deploy -GameDir "D:\SteamLibrary\steamapps\common\Assetto Corsa Rally"
```

Requires UE4SS in the game directory. Use `Shipping` for a build without diagnostics. `ASSETO_CORSA_RALLY_HOME` can replace `-GameDir`. Existing mod settings are preserved.

## Release package

```powershell
.\tools\build.ps1 Shipping
.\tools\package.ps1
```

`package.ps1` packages the existing Shipping DLL without rebuilding. Output: `dist/BeamNGOrbitCamera-v<version>.zip`.

Requires `UE4SS_v3.0.1-1127-g2bfa839f.zip` in `D:\DevTools\UE4SS`; override the directory with `-UE4SSArchiveDir <path>`. Move or delete an existing output ZIP before packaging again.

## Credits

- Supernova Games Studios and Kunos Simulazioni for Assetto Corsa Rally.
- The [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) contributors for the modding framework.
- BeamNG GmbH for the orbit-camera inspiration.

This mod is not affiliated with or endorsed by Supernova Games Studios, Kunos Simulazioni, BeamNG GmbH, or Epic Games.

## License

[MIT](LICENSE).
