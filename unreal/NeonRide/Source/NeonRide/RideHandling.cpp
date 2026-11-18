#include "RideHandling.h"
#include <cmath>
#include <numbers>

void NeonRide::FRideState::Advance(const FDistrict& District, double Dt, double Throttle, double Brake, double Steer)
{
    const double OldSpeed = Speed;
    const double Drive = 7.5 * Throttle * (1.0 - Brake) - 18.0 * Brake - (0.45 + Speed * 0.015);
    Speed = std::clamp(Speed + Drive * Dt, 0.0, TopSpeed);
    // A tight low-speed turning circle also lets a stopped rider pivot to inspect a landmark.
    const double MaxTurnRate = std::min(1.65, 12.5 / std::max(Speed, 1.0));
    TurnRate = std::lerp(TurnRate, Steer * MaxTurnRate, 1.0 - std::exp(-4.6 * Dt));
    Heading = std::remainder(Heading + TurnRate * Dt, 2.0 * std::numbers::pi);
    const double Travel = (Speed + OldSpeed) * 0.5 * Dt;
    const FPoint Desired{Position.X + std::cos(Heading) * Travel, Position.Y + std::sin(Heading) * Travel, Position.Z};
    const auto Ground = District.Resolve(Desired, Position);
    const double Actual = std::hypot(Ground.Point.X - Position.X, Ground.Point.Y - Position.Y);
    if (Actual + 0.001 < Travel) Speed = std::min(Speed, Actual / Dt);
    if (Actual > 0.001)
    {
        const double Grade = std::atan2(Ground.Point.Z - Position.Z, Actual) * 180.0 / std::numbers::pi;
        Pitch = std::lerp(Pitch, std::clamp(Grade, -14.0, 14.0), 1.0 - std::exp(-6.0 * Dt));
    }
    Odometer += Actual;
    Position = Ground.Point;
    Place = Ground.Place;
    Acceleration = std::lerp(Acceleration, std::clamp((Speed - OldSpeed) / Dt, -18.0, 7.5), 1.0 - std::exp(-7.0 * Dt));
    const double TargetRoll = std::clamp(std::atan(Speed * TurnRate / 9.81) * 180.0 / std::numbers::pi, -54.0, 54.0);
    Roll = std::lerp(Roll, TargetRoll, 1.0 - std::exp(-6.0 * Dt));
}
