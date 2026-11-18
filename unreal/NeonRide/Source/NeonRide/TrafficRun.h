#pragma once

#include "District.h"
#include <array>

namespace NeonRide
{
    struct FRideState;
    struct FStreetVehicle
    {
        bool Truck;
        double Lane, Distance, Speed;
        FStreetPose Pose{};
        double Cruise() const { return Truck ? 24.0 : 28.0; }
        double HalfLength() const { return Truck ? 6.5 : 2.4; }
        double HalfWidth() const { return Truck ? 1.25 : 0.95; }
    };

    struct FStreetTraffic
    {
        std::array<FStreetVehicle, 5> Vehicles{{
            {true, 1.8, 90, 24}, {false, -2.6, 250, 28}, {true, 1.8, 720, 24},
            {false, -2.6, 1350, 28}, {false, 1.8, 1740, 28}
        }};
        void Advance(const FDistrict& District, const FRideState& Rider, double Dt);
        bool Touches(const FPoint& Point) const;
    };
}
