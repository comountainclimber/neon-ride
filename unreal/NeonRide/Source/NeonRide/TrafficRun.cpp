#include "TrafficRun.h"
#include "RideHandling.h"
#include <cmath>
#include <numbers>

namespace
{
    struct FSeparation { double Forward, Side; };
    FSeparation RelativeTo(const NeonRide::FStreetVehicle& Car, const NeonRide::FPoint& Point)
    {
        const double Yaw = Car.Pose.Yaw * std::numbers::pi / 180.0;
        const double X = Point.X - Car.Pose.Point.X, Y = Point.Y - Car.Pose.Point.Y;
        return {X * std::cos(Yaw) + Y * std::sin(Yaw), -X * std::sin(Yaw) + Y * std::cos(Yaw)};
    }
}

void NeonRide::FStreetTraffic::Advance(const FDistrict& District, const FRideState& Rider, double Dt)
{
    for (auto& Car : Vehicles)
    {
        Car.Pose = District.TrafficAt(Car.Distance, Car.Lane);
        const auto Gap = RelativeTo(Car, Rider.Position);
        double Target = Car.Cruise();
        if (std::abs(Car.Pose.Point.Z - Rider.Position.Z) < 2.0 && Gap.Forward >= 0.0 && std::abs(Gap.Side) < Car.HalfWidth() + 0.9)
            Target = std::clamp((Gap.Forward - Car.HalfLength() - 3.0) * 0.7, 0.0, Target);
        Car.Speed += std::clamp(Target - Car.Speed, -13.0 * Dt, 3.0 * Dt);
        Car.Distance += Car.Speed * Dt;
        Car.Pose = District.TrafficAt(Car.Distance, Car.Lane);
    }
}

bool NeonRide::FStreetTraffic::Touches(const FPoint& Point) const
{
    for (const auto& Car : Vehicles)
    {
        if (std::abs(Car.Pose.Point.Z - Point.Z) > 2.0) continue;
        const auto Gap = RelativeTo(Car, Point);
        if (std::abs(Gap.Forward) < Car.HalfLength() + 0.9 && std::abs(Gap.Side) < Car.HalfWidth() + 0.4)
            return true;
    }
    return false;
}
