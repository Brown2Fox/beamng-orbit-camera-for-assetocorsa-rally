#include "Config/Config.hpp"

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr double DegToRad = Pi / 180.0;
    constexpr double RadToDeg = 180.0 / Pi;

    std::string Trim(std::string Value)
    {
        const auto IsSpace = [](unsigned char Character) {
            return std::isspace(Character) != 0;
        };

        Value.erase(Value.begin(), std::find_if(Value.begin(), Value.end(), [&](unsigned char Character) {
                    return !IsSpace(Character);
                }
            ));

        Value.erase(std::find_if(Value.rbegin(), Value.rend(), [&](unsigned char Character) {
                    return !IsSpace(Character);
                }
            ).base(), Value.end());

        return Value;
    }

    std::string Lower(std::string Value)
    {
        std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Character) {
                return static_cast<char>(std::tolower(Character));
            }
        );

        return Value;
    }

    std::string MakeKey(const std::string& Section, const std::string& Name)
    {
        return Lower(Section + "." + Name);
    }

    bool TryParseDouble(const std::unordered_map<std::string, std::string>& Values, const char* Section, const char* Name, double& InOutValue)
    {
        const auto It = Values.find(MakeKey(Section, Name));

        if (It == Values.end())
        {
            return false;
        }

        try
        {
            size_t Consumed = 0;
            const double Parsed = std::stod(It->second, &Consumed);

            if (Consumed == 0)
            {
                return false;
            }

            InOutValue = Parsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool TryParseBool(const std::unordered_map<std::string, std::string>& Values, const char* Section, const char* Name, bool& InOutValue)
    {
        const auto It = Values.find(MakeKey(Section, Name));

        if (It == Values.end())
        {
            return false;
        }

        const std::string Value = Lower(Trim(It->second));

        if (Value == "1" || Value == "true" || Value == "yes" || Value == "on")
        {
            InOutValue = true;
            return true;
        }

        if (Value == "0" || Value == "false" || Value == "no" || Value == "off")
        {
            InOutValue = false;
            return true;
        }

        return false;
    }

    std::unordered_map<std::string, std::string>
    ReadIni(const std::filesystem::path& ConfigPath)
    {
        std::unordered_map<std::string, std::string> Values;

        std::ifstream Input(ConfigPath);
        if (!Input)
        {
            return Values;
        }

        std::string Section;
        std::string Line;

        while (std::getline(Input, Line))
        {
            Line = Trim(Line);

            if (Line.empty() || Line.front() == ';' || Line.front() == '#')
            {
                continue;
            }

            if (Line.front() == '[' && Line.back() == ']')
            {
                Section = Trim(Line.substr(1, Line.size() - 2));
                continue;
            }

            const size_t Equals = Line.find('=');

            if (Equals == std::string::npos)
            {
                continue;
            }

            const std::string Name = Trim(Line.substr(0, Equals));

            std::string Value = Trim(Line.substr(Equals + 1));

            const size_t Semicolon = Value.find(';');

            if (Semicolon != std::string::npos)
            {
                Value = Trim(Value.substr(0, Semicolon));
            }

            if (!Section.empty() && !Name.empty())
            {
                Values[MakeKey(Section, Name)] = Value;
            }
        }

        return Values;
    }

    void ClampSettings(FBeamNGOrbitCameraSettings& Settings)
    {
        Settings.CameraDistanceCm = std::clamp(Settings.CameraDistanceCm, 300.0, 3000.0);

        Settings.CameraFovDeg = std::clamp(Settings.CameraFovDeg, 30.0, 120.0);

        Settings.TargetHeightOffsetCm = std::clamp(Settings.TargetHeightOffsetCm, -100.0, 200.0);

        Settings.CameraPitchRad = std::clamp(Settings.CameraPitchRad, -85.0 * DegToRad, 85.0 * DegToRad);

        Settings.CameraRelaxationCm = std::clamp(Settings.CameraRelaxationCm, 50.0, 2000.0);

        Settings.DynamicFovAtSpeedDeg = std::clamp(Settings.DynamicFovAtSpeedDeg, 0.0, 90.0);

        Settings.DynamicPitchAtSpeedRad = std::clamp(Settings.DynamicPitchAtSpeedRad, 0.0, 45.0 * DegToRad);

        Settings.DynamicHeightAtSpeedCm = std::clamp(Settings.DynamicHeightAtSpeedCm, 0.0, 200.0);

        Settings.GamepadOrbitDeadzone = std::clamp(Settings.GamepadOrbitDeadzone, 0.0, 0.50);

        Settings.GamepadZoomDeadzone = std::clamp(Settings.GamepadZoomDeadzone, 0.0, 0.50);

        Settings.GamepadResponseExponent = std::clamp(Settings.GamepadResponseExponent, 0.25, 4.0);

        Settings.GamepadOrbitSensitivity = std::clamp(Settings.GamepadOrbitSensitivity, 0.10, 4.0);

        Settings.GamepadZoomSensitivity = std::clamp(Settings.GamepadZoomSensitivity, 0.10, 4.0);
    }
}

std::filesystem::path
FBeamNGOrbitCameraConfig::ResolveConfigPath()
{
    wchar_t ModulePath[MAX_PATH]{};

    const DWORD Length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), ModulePath, static_cast<DWORD>(std::size(ModulePath)));

    if (Length == 0 || Length >= std::size(ModulePath))
    {
        return std::filesystem::current_path() / L"Data" / L"config.ini";
    }

    std::filesystem::path Path(ModulePath);

    const std::filesystem::path DllDirectory = Path.parent_path();

    const std::filesystem::path ModDirectory = DllDirectory.filename() == L"dlls"
            ? DllDirectory.parent_path() : DllDirectory;

    return ModDirectory / L"Data" / L"config.ini";
}

bool FBeamNGOrbitCameraConfig::Load(FBeamNGOrbitCameraSettings& OutSettings, const std::filesystem::path& ConfigPath)
{
    if (!std::filesystem::exists(ConfigPath))
    {
        return Save(OutSettings, ConfigPath);
    }

    const auto Values = ReadIni(ConfigPath);

    double CameraDistanceM = OutSettings.CameraDistanceCm / 100.0;

    double CameraPitchDeg = OutSettings.CameraPitchRad * RadToDeg;

    double CameraRelaxationM = OutSettings.CameraRelaxationCm / 100.0;

    double DynamicPitchAtSpeedDeg = OutSettings.DynamicPitchAtSpeedRad * RadToDeg;

    double DynamicHeightAtSpeedM = OutSettings.DynamicHeightAtSpeedCm / 100.0;

    TryParseDouble(Values, "Camera", "Distance", CameraDistanceM);

    TryParseDouble(Values, "Camera", "FOV", OutSettings.CameraFovDeg);

    TryParseDouble(Values, "Camera", "TargetHeightOffset", OutSettings.TargetHeightOffsetCm);

    TryParseDouble(Values, "Camera", "Pitch", CameraPitchDeg);

    TryParseDouble(Values, "Camera", "Relaxation", CameraRelaxationM);

    TryParseDouble(Values, "Dynamic", "FOVAtSpeed", OutSettings.DynamicFovAtSpeedDeg);

    TryParseDouble(Values, "Dynamic", "PitchAtSpeed", DynamicPitchAtSpeedDeg);

    TryParseDouble(Values, "Dynamic", "HeightAtSpeed", DynamicHeightAtSpeedM);

    TryParseDouble(Values, "Gamepad", "OrbitDeadzone", OutSettings.GamepadOrbitDeadzone);

    TryParseDouble(Values, "Gamepad", "ZoomDeadzone", OutSettings.GamepadZoomDeadzone);

    TryParseDouble(Values, "Gamepad", "ResponseExponent", OutSettings.GamepadResponseExponent);

    TryParseDouble(Values, "Gamepad", "OrbitSensitivity", OutSettings.GamepadOrbitSensitivity);

    TryParseDouble(Values, "Gamepad", "ZoomSensitivity", OutSettings.GamepadZoomSensitivity);

    TryParseBool(Values, "Collision", "Enabled", OutSettings.bCollisionEnabled);

    OutSettings.CameraDistanceCm = CameraDistanceM * 100.0;

    OutSettings.CameraPitchRad = CameraPitchDeg * DegToRad;

    OutSettings.CameraRelaxationCm = CameraRelaxationM * 100.0;

    OutSettings.DynamicPitchAtSpeedRad = DynamicPitchAtSpeedDeg * DegToRad;

    OutSettings.DynamicHeightAtSpeedCm = DynamicHeightAtSpeedM * 100.0;

    ClampSettings(OutSettings);
    return true;
}

bool FBeamNGOrbitCameraConfig::Save(const FBeamNGOrbitCameraSettings& Settings, const std::filesystem::path& ConfigPath)
{
    std::error_code Error;

    std::filesystem::create_directories(ConfigPath.parent_path(), Error);

    std::ofstream Output(ConfigPath);
    if (!Output)
    {
        return false;
    }

    Output
        << "; BeamNG Orbit Camera settings\n"
        << "; Units: Distance/Relaxation/HeightAtSpeed = metres, angles = degrees, TargetHeightOffset = centimetres.\n"
        << "; Gamepad deadzones and sensitivity values are normalized.\n\n"

        << "[Camera]\n"
        << "Distance="
        << Settings.CameraDistanceCm / 100.0
        << "\n"
        << "FOV="
        << Settings.CameraFovDeg
        << "\n"
        << "TargetHeightOffset="
        << Settings.TargetHeightOffsetCm
        << "\n"
        << "Pitch="
        << Settings.CameraPitchRad * RadToDeg
        << "\n"
        << "Relaxation="
        << Settings.CameraRelaxationCm / 100.0
        << "\n\n"

        << "[Dynamic]\n"
        << "FOVAtSpeed="
        << Settings.DynamicFovAtSpeedDeg
        << "\n"
        << "PitchAtSpeed="
        << Settings.DynamicPitchAtSpeedRad * RadToDeg
        << "\n"
        << "HeightAtSpeed="
        << Settings.DynamicHeightAtSpeedCm / 100.0
        << "\n\n"

        << "[Gamepad]\n"
        << "OrbitDeadzone="
        << Settings.GamepadOrbitDeadzone
        << "\n"
        << "ZoomDeadzone="
        << Settings.GamepadZoomDeadzone
        << "\n"
        << "ResponseExponent="
        << Settings.GamepadResponseExponent
        << "\n"
        << "OrbitSensitivity="
        << Settings.GamepadOrbitSensitivity
        << "\n"
        << "ZoomSensitivity="
        << Settings.GamepadZoomSensitivity
        << "\n\n"

        << "[Collision]\n"
        << "Enabled="
        << (Settings.bCollisionEnabled
                ? "true" : "false")
        << "\n";

    return Output.good();
}
