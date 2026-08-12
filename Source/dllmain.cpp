#include "BeamNGOrbitCameraMod.hpp"

#define BEAMNG_ORBIT_CAMERA_API __declspec(dllexport)

extern "C"
{
    BEAMNG_ORBIT_CAMERA_API CppUserModBase* start_mod()
    {
        return new FBeamNGOrbitCameraMod();
    }

    BEAMNG_ORBIT_CAMERA_API void uninstall_mod(CppUserModBase* Mod)
    {
        delete Mod;
    }
}
