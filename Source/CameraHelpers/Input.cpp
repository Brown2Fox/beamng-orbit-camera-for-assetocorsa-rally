#include "BeamNGOrbitCameraMod.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace
{
    constexpr SHORT KeyDownMask = static_cast<SHORT>(0x8000);
    constexpr SHORT KeyPressedMask = 0x0001;

    bool IsKeyDown(SHORT KeyState)
    {
        return (KeyState & KeyDownMask) != 0;
    }

    bool WasKeyPressed(SHORT KeyState)
    {
        return (KeyState & KeyPressedMask) != 0;
    }

    bool IsKeyDown(int VirtualKey)
    {
        return IsKeyDown(GetAsyncKeyState(VirtualKey));
    }

    double NormalizeXInputAxis(SHORT Raw)
    {
        const double Value = Raw < 0
                ? static_cast<double>(Raw) / 32768.0 : static_cast<double>(Raw) / 32767.0;

        return std::clamp(Value, -1.0, 1.0);
    }

    double ShapeGamepadAxis(SHORT Raw, double Deadzone, double Exponent)
    {
        const double Value = NormalizeXInputAxis(Raw);

        const double Magnitude = std::abs(Value);

        if (Magnitude <= Deadzone)
        {
            return 0.0;
        }

        const double NormalizedMagnitude = std::clamp((Magnitude - Deadzone) / (1.0 - Deadzone), 0.0, 1.0);

        const double ShapedMagnitude = std::pow(NormalizedMagnitude, Exponent);

        return Value < 0.0
                ? -ShapedMagnitude : ShapedMagnitude;
    }

    void ShapeGamepadStick(SHORT RawX, SHORT RawY, double Deadzone, double Exponent, double& OutX, double& OutY)
    {
        const double X = NormalizeXInputAxis(RawX);

        const double Y = NormalizeXInputAxis(RawY);

        const double Magnitude = std::sqrt(X * X + Y * Y);

        if (Magnitude <= Deadzone || Magnitude <= 0.000001)
        {
            OutX = 0.0;
            OutY = 0.0;
            return;
        }

        const double DirectionX = X / Magnitude;

        const double DirectionY = Y / Magnitude;

        const double ClampedMagnitude = std::min(Magnitude, 1.0);

        const double NormalizedMagnitude = std::clamp((ClampedMagnitude - Deadzone) / (1.0 - Deadzone), 0.0, 1.0);

        const double ShapedMagnitude = std::pow(NormalizedMagnitude, Exponent);

        OutX = DirectionX * ShapedMagnitude;

        OutY = DirectionY * ShapedMagnitude;
    }
}

void FBeamNGOrbitCameraMod::PollHotkeys()
{
    const SHORT DeleteKeyState = GetAsyncKeyState(VK_DELETE);

    const bool bDeleteDown = IsKeyDown(DeleteKeyState);

    const bool bDeletePressed = WasKeyPressed(DeleteKeyState) || (bDeleteDown && !bDeleteWasDown);

    // Some keyboards expose the physical Insert key as VK_INSERT, while
    // NumPad Insert can arrive as VK_NUMPAD0. Accept both and also use the
    // transition bit so a short tap cannot be missed between engine ticks.
    const SHORT InsertKeyState = GetAsyncKeyState(VK_INSERT);

    const SHORT NumpadInsertKeyState = GetAsyncKeyState(VK_NUMPAD0);

    const bool bInsertDown = IsKeyDown(InsertKeyState) || IsKeyDown(NumpadInsertKeyState);

    const bool bInsertPressed = WasKeyPressed(InsertKeyState) || WasKeyPressed(NumpadInsertKeyState) || (bInsertDown && !bInsertWasDown);
#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    const bool bEndPressed = IsKeyDown(VK_END);

    const bool bCtrlPressed = IsKeyDown(VK_CONTROL);
    const bool bBPressed = IsKeyDown('B');
    const bool bCtrlBPressed = bCtrlPressed && bBPressed;
#endif

    if (bDeletePressed)
    {
        const bool bNextEnabled = !bEnabled.load(std::memory_order_acquire);

        bEnabled.store(bNextEnabled, std::memory_order_release);

        ResetCameraState();
        Output::send<LogLevel::Verbose>(bNextEnabled
                ? STR("[BeamNGOrbitCamera] ORBIT CAMERA: ON\n") : STR("[BeamNGOrbitCamera] ORBIT CAMERA: OFF\n"));

        if (bNextEnabled && !CameraManager)
        {
            Output::send<LogLevel::Warning>(
                STR("[BeamNGOrbitCamera] Camera manager has not been discovered; the session may not be ready or the game version may be incompatible.\n")
            );
        }
    }

    if (bInsertPressed)
    {
        const bool bNextCollisionEnabled = !bCollisionEnabled.load(std::memory_order_acquire);

        bCollisionEnabled.store(bNextCollisionEnabled, std::memory_order_release);

        ResetCollisionState();

        Output::send<LogLevel::Verbose>(bNextCollisionEnabled
                ? STR("[BeamNGOrbitCamera] COLLISION: ON\n") : STR("[BeamNGOrbitCamera] COLLISION: OFF\n"));
    }

#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    if (bEndPressed && !bEndWasDown)
    {
        bCameraManagerDumpRequested.store(true, std::memory_order_release);
        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] Camera manager dump requested.\n"));
    }

    if (bCtrlBPressed && !bCtrlBWasDown)
    {
        bComponentDumpRequested.store(true, std::memory_order_release);
        Output::send<LogLevel::Verbose>(STR("[BeamNGOrbitCamera] Vehicle component dump requested.\n"));
    }
#endif

    bDeleteWasDown = bDeleteDown;
    bInsertWasDown = bInsertDown;
#if BEAMNG_ORBIT_CAMERA_DIAGNOSTICS
    bEndWasDown = bEndPressed;
    bCtrlBWasDown = bCtrlBPressed;
#endif
}

FOrbitInput FBeamNGOrbitCameraMod::ReadOrbitInput(double DeltaTimeSeconds)
{
    FOrbitInput Out{};

    const bool bLeft = IsKeyDown(VK_NUMPAD4);
    const bool bRight = IsKeyDown(VK_NUMPAD6);
    const bool bUp = IsKeyDown(VK_NUMPAD8);
    const bool bDown = IsKeyDown(VK_NUMPAD2);
    const bool bZoomOut = IsKeyDown(VK_NUMPAD3);
    const bool bZoomIn = IsKeyDown(VK_NUMPAD9);
    const bool TargetUp = IsKeyDown(VK_NUMPAD7);
    const bool TargetDown = IsKeyDown(VK_NUMPAD1);


    double YawInput = 0.0;
    double PitchInput = 0.0;
    double ZoomInput = 0.0;
    double TargetHeightInput = 0.0;

    if (bLeft) YawInput += 1.0;
    if (bRight) YawInput -= 1.0;
    if (bUp) PitchInput += 1.0;
    if (bDown) PitchInput -= 1.0;
    if (bZoomOut) ZoomInput += 1.0;
    if (bZoomIn) ZoomInput -= 1.0;
    if (TargetUp) TargetHeightInput += 1.0;
    if (TargetDown) TargetHeightInput -= 1.0;

    Out.YawStepRad = std::clamp(YawInput, -1.0, 1.0) * OrbitYawSpeedRad * DeltaTimeSeconds;
    Out.PitchStepRad = std::clamp(PitchInput, -1.0, 1.0) * OrbitPitchSpeedRad * DeltaTimeSeconds;
    Out.ZoomStep = std::clamp(ZoomInput, -1.0, 1.0) * DeltaTimeSeconds;
    Out.TargetHeightStepCm = std::clamp(TargetHeightInput, -1.0, 1.0)
        * TargetHeightAdjustCmPerSec
        * DeltaTimeSeconds;


    const bool bNum5 = IsKeyDown(VK_NUMPAD5);
    const bool bCtrl = IsKeyDown(VK_CONTROL);
    if (bNum5 && !bNum5WasDown)
    {
        if (bCtrl) Out.bRecenterKeep = true;
        else Out.bRecenter = true;
    }
    bNum5WasDown = bNum5;

    MergeGamepadInput(Out, DeltaTimeSeconds);
    return Out;
}

void FBeamNGOrbitCameraMod::InitializeXInput()
{
    static constexpr const wchar_t* candidates[] = {
        L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"
    };

    for (const wchar_t* DllName : candidates)
    {
        HMODULE module = LoadLibraryW(DllName);
        if (!module)
            continue;

        const FARPROC proc = GetProcAddress(module, "XInputGetState");

        // Win64: FARPROC and typed Function pointers have the same
        // representation. std::bit_cast avoids MSVC C4191 while keeping
        // the exact XInputGetState signature checked by our typedef.
        static_assert(sizeof(FARPROC) == sizeof(XInputGetStateFn));
        const XInputGetStateFn GetState = std::bit_cast<XInputGetStateFn>(proc);

        if (GetState)
        {
            XInputModule = module;
            XInputGetState = GetState;

            return;
        }

        FreeLibrary(module);
    }

    Output::send<LogLevel::Warning>(STR("[BeamNGOrbitCamera] XInput DLL not available; gamepad camera controls disabled. Keyboard controls still work.\n"));
}

bool FBeamNGOrbitCameraMod::PollXInputState(XINPUT_STATE& OutState)
{
    if (!XInputGetState)
        return false;

    auto TryIndex = [&](DWORD Index) -> bool {
        XINPUT_STATE State{};

        if (XInputGetState(Index, &State) != ERROR_SUCCESS)
            return false;

        OutState = State;

        if (!bGamepadConnected || GamepadUserIndex != static_cast<int>(Index))
        {
            bGamepadConnected = true;
            GamepadUserIndex = static_cast<int>(Index);

        }

        return true;
    };

    if (GamepadUserIndex >= 0 && TryIndex(static_cast<DWORD>(GamepadUserIndex)))
    {
        return true;
    }

    for (DWORD Index = 0; Index < XUSER_MAX_COUNT; ++Index)
    {
        if (TryIndex(Index))
            return true;
    }

    if (bGamepadConnected)
    {
        bGamepadConnected = false;
        GamepadUserIndex = -1;
        bGamepadR3WasDown = false;
        bGamepadL3WasDown = false;

    }

    return false;
}

void FBeamNGOrbitCameraMod::MergeGamepadInput(FOrbitInput& Out, double DeltaTimeSeconds)
{
    XINPUT_STATE State{};
    if (!PollXInputState(State))
        return;

    const XINPUT_GAMEPAD& pad = State.Gamepad;

    double RightX = 0.0;
    double RightY = 0.0;

    ShapeGamepadStick(pad.sThumbRX, pad.sThumbRY, Settings.GamepadOrbitDeadzone, Settings.GamepadResponseExponent, RightX, RightY);

    const double LeftY = ShapeGamepadAxis(pad.sThumbLY, Settings.GamepadZoomDeadzone, Settings.GamepadResponseExponent);

    const bool bZoomModifier = (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;

    const bool bRightStickClick = (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
    const bool bLeftStickClick = (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;

    // Edge-triggered bRecenter actions:
    //   R3 = full recenter/reset To default pitch + distance.
    //   L3 = recenter heading while preserving current pitch + distance.
    if (bRightStickClick && !bGamepadR3WasDown)
        Out.bRecenter = true;

    if (bLeftStickClick && !bGamepadL3WasDown)
        Out.bRecenterKeep = true;

    bGamepadR3WasDown = bRightStickClick;
    bGamepadL3WasDown = bLeftStickClick;

    const double GamepadYaw = RightX * Settings.GamepadOrbitSensitivity;
    const double GamepadPitch = -RightY * Settings.GamepadOrbitSensitivity;
    const double GamepadZoom = bZoomModifier
            ? -LeftY * Settings.GamepadZoomSensitivity : 0.0;

    const double MaxYawStep = OrbitYawSpeedRad * DeltaTimeSeconds;

    const double MaxPitchStep = OrbitPitchSpeedRad * DeltaTimeSeconds;

    Out.YawStepRad = std::clamp(Out.YawStepRad + GamepadYaw * MaxYawStep, -MaxYawStep, MaxYawStep);

    Out.PitchStepRad = std::clamp(Out.PitchStepRad + GamepadPitch * MaxPitchStep, -MaxPitchStep, MaxPitchStep);

    Out.ZoomStep = std::clamp(Out.ZoomStep + GamepadZoom * DeltaTimeSeconds, -DeltaTimeSeconds, DeltaTimeSeconds);
}
