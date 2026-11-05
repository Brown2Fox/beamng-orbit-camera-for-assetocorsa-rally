# BeamNG Orbit Camera

An unofficial BeamNG-style orbit camera for Assetto Corsa Rally. It replaces the `ThirdFar` camera.

> [!WARNING]
> This is an experimental mod that depends on undocumented game and UE4SS internals. A game or UE4SS update may require a mod update.

## Installation

Download `BeamNGOrbitCamera-v<version>.zip` from [Releases](https://github.com/Brown2Fox/beamng-orbit-camera-for-assetocorsa-rally/releases) and extract it into the Assetto Corsa Rally installation directory—the folder containing `acr`. The archive includes the supported UE4SS build with the mod already enabled.

The installed files should include:

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
| Enable or disable orbit camera | — | `Delete` |
| Enable or disable collision | — | `Insert` or NumPad `0` |
| Orbit | Right stick | NumPad `4`/`6` and `8`/`2` |
| Zoom | `RB/R1` + left stick Y | NumPad `9`/`3` |
| Adjust target height | — | NumPad `7`/`1` |
| Recenter | `R3` | NumPad `5` |
| Recenter, keep pitch and distance | `L3` | `Ctrl` + NumPad `5` |

The orbit camera starts disabled. Collision starts enabled.

## Versions

This is the currently tested combination:

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

## How It Works

The mod loads as a UE4SS C++ mod. It loads a small packaged `BP_BeamNGOrbitModifier` asset and adds it to the vehicle's `APlayerCameraManager` when `BC_CarPlayerCameraManager_C` appears. UE4SS then provides a `ProcessEventPost` callback for the modifier's `BlueprintModifyCamera` event.

Each frame the game computes its camera normally. After `BlueprintModifyCamera` finishes, the mod reads that clean result and, only when `ThirdFar` is active and the mod is enabled, applies orbit, zoom, target height, and collision. The clean pose is restored before the next camera update so the custom result does not feed back into the game's blending or other camera modifiers.

The integration uses Unreal reflection and runtime object names rather than fixed addresses in a particular game executable. This makes it less sensitive to binary patches, but renamed classes, functions, properties, Blueprint changes, or an incompatible UE4SS ABI can still require an update.

## Development Setup

Requirements:

- Windows and Visual Studio 2022 17.13 or newer with Desktop development with C++;
- CMake 3.22 or newer;
- Rust 1.73 or newer;
- Git;
- Unreal Engine 5.6 only when rebuilding the camera-modifier bundle.

Clone the pinned UE4SS revision beside the repository:

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

Building and packaging are separate steps. Build `Shipping` after C++ changes, then run `tools/package.ps1` to create the [release package](#release-package) from that DLL. Packaging never starts a build or checks whether the DLL is up to date.

To rebuild the asset bundle, create any empty UE 5.6 Blueprint project; the project name does not matter. Add:

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
.\tools\build.ps1 Shipping -Deploy -GameDir "D:\SteamLibrary\steamapps\common\Assetto Corsa Rally"
```

Alternatively, set `ASSETO_CORSA_RALLY_HOME` and omit `-GameDir`. Deploy installs the mod DLL and asset bundle into the game directory and requires UE4SS to be installed first.

## Release package

Build once, then package the resulting Shipping DLL:

```powershell
.\tools\build.ps1 Shipping
.\tools\package.ps1
```

`package.ps1` creates `dist/BeamNGOrbitCamera-v<version>.zip` using the existing DLL in `build/Shipping/Source/Game__Shipping__Win64`, the complete supported UE4SS distribution, default settings, and the camera asset bundle. It stops with a build instruction if the DLL is missing. If Shipping is already up to date, run only `package.ps1`.

The release UE4SS archive is selected by SHA-256 from `D:\DevTools\UE4SS`, regardless of its filename; `zDEV` does not match. Pass `-UE4SSArchiveDir <path>` to search another directory. The mod is enabled in both UE4SS mod lists, and the archive follows the installation layout above. The `dist/` directory is ignored by Git. An existing archive with the same name is preserved; move or delete it before packaging again.

Attach this ZIP to the corresponding [GitHub release](https://github.com/Brown2Fox/beamng-orbit-camera-for-assetocorsa-rally/releases). Packaging creates the local archive; it does not upload it.

## Credits

- Supernova Games Studios and Kunos Simulazioni for Assetto Corsa Rally.
- The [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) contributors for the modding framework.
- BeamNG GmbH for the orbit-camera inspiration.

This mod is not affiliated with or endorsed by Supernova Games Studios, Kunos Simulazioni, BeamNG GmbH, or Epic Games.

## License

[MIT](LICENSE).
