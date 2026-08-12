#pragma once

#include <filesystem>

struct FBeamNGOrbitCameraSettings
{
    double CameraDistanceCm{500.0};
    double CameraFovDeg{65.0};
    double TargetHeightOffsetCm{0.0};
    double CameraPitchRad{17.0 * 3.14159265358979323846 / 180.0};
    double CameraRelaxationCm{600.0};

    double DynamicFovAtSpeedDeg{40.0};
    double DynamicPitchAtSpeedRad{7.0 * 3.14159265358979323846 / 180.0};
    double DynamicHeightAtSpeedCm{40.0};

    double GamepadOrbitDeadzone{0.15};
    double GamepadZoomDeadzone{0.10};
    double GamepadResponseExponent{1.0};
    double GamepadOrbitSensitivity{1.0};
    double GamepadZoomSensitivity{1.0};

    bool bCollisionEnabled{true};
};

class FBeamNGOrbitCameraConfig
{
public:
    static std::filesystem::path ResolveConfigPath();

    static bool Load(FBeamNGOrbitCameraSettings& OutSettings, const std::filesystem::path& ConfigPath);

    static bool Save(const FBeamNGOrbitCameraSettings& Settings, const std::filesystem::path& ConfigPath);
};
