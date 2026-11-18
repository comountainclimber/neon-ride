#include "RideHandling.h"
#include <cmath>
#include <numbers>

void NeonRide::FRideState::Advance(double Dt, double Throttle, double Brake, double Steer)
{
    const double TargetSpeed = std::lerp(CruiseSpeed, TopSpeed, Throttle) * (1.0 - Brake);
    const double SpeedBefore = Speed;
    const double Rate = Brake > 0.0 ? 20.0 : (Speed < TargetSpeed ? 7.0 : 4.0);
    Speed += std::clamp(TargetSpeed - Speed, -Rate * Dt, Rate * Dt);
    Acceleration = std::lerp(Acceleration, (Speed - SpeedBefore) / Dt, 1.0 - std::exp(-7.0 * Dt));

    // The stick commands lateral speed. Taper it on approach to the rail so holding
    // steer at the edge settles the bike upright. The physical clearance is 65 cm.
    const double Clearance = EdgeLimit - std::copysign(1.0, Steer) * Offset;
    const double TargetSideSpeed = Steer * std::min(3.5, Speed * 0.12) * SmoothUnit(Clearance / 1.5);
    const double TargetSideAccel = std::clamp((TargetSideSpeed - SideSpeed) * 5.0, -7.0, 7.0);
    SideAcceleration = std::lerp(SideAcceleration, TargetSideAccel, 1.0 - std::exp(-9.0 * Dt));
    SideSpeed += SideAcceleration * Dt;
    Offset += SideSpeed * Dt;
    if (std::abs(Offset) >= EdgeLimit)
    {
        Offset = std::clamp(Offset, -EdgeLimit, EdgeLimit);
        if (Offset * SideSpeed > 0.0)
        {
            SideSpeed = 0.0;
            SideAcceleration = 0.0;
        }
    }

    Distance = std::min(TrackLength, Distance + 0.5 * (SpeedBefore + Speed) * Dt);
    // Positive Unreal roll leans right. This route curves left.
    Roll = std::clamp(std::atan((SideAcceleration - Speed * Speed * Curvature(Distance)) / 9.81) * 180.0 / std::numbers::pi, -54.0, 54.0);
}
