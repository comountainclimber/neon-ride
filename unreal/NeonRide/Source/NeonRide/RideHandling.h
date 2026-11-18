#pragma once

#include <algorithm>

namespace NeonRide
{
    // Physics uses metres and seconds. Only the world geometry uses Unreal centimetres.
    constexpr double TrackLength = 1100.0;
    constexpr double HalfWidth = 5.0;
    constexpr double CruiseSpeed = 34.0;
    constexpr double TopSpeed = 56.0;
    constexpr double EdgeLimit = HalfWidth - 0.65;

    inline double SmoothUnit(double Value)
    {
        const double T = std::clamp(Value, 0.0, 1.0);
        return T * T * (3.0 - 2.0 * T);
    }

    inline double Curvature(double Distance)
    {
        return SmoothUnit((Distance - 300.0) / 90.0) / 340.0;
    }

    struct FRideState
    {
        double Distance = 0.0;
        double Offset = 1.8;
        double Speed = CruiseSpeed;
        double SideSpeed = 0.0;
        double Acceleration = 0.0;
        double SideAcceleration = 0.0;
        double Roll = 0.0;

        void Advance(double Dt, double Throttle, double Brake, double Steer);
    };
}
