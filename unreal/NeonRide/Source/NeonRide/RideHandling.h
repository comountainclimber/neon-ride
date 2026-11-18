#pragma once

#include "District.h"
#include <algorithm>

namespace NeonRide
{
    constexpr double TrackLength = 1100.0;
    constexpr double CruiseSpeed = 34.0;
    constexpr double TopSpeed = 56.0;

    inline double SmoothUnit(double Value)
    {
        const double T = std::clamp(Value, 0.0, 1.0);
        return T * T * (3.0 - 2.0 * T);
    }

    inline double Curvature(double Distance) { return SmoothUnit((Distance - 300.0) / 90.0) / 340.0; }

    struct FRideState
    {
        FPoint Position{0, 1.8, 0};
        EPlace Place = EPlace::Skyway;
        double Heading = 0.0; // radians; positive turns right
        double Speed = 0.0;
        double Acceleration = 0.0;
        double TurnRate = 0.0;
        double Pitch = 0.0;
        double Roll = 0.0;
        double Odometer = 0.0;
        void Advance(const FDistrict& District, double Dt, double Throttle, double Brake, double Steer);
    };
}
