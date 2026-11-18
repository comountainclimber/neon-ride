#include "District.h"
#include "RideHandling.h"
#include <cmath>
#include <limits>
#include <numbers>

using namespace NeonRide;

void FDistrict::AddStrip(FPoint A, FPoint B, double Width, EPlace Place, bool bRoad, double Bank)
{
    const double Length = std::hypot(B.X - A.X, B.Y - A.Y);
    Strips.push_back({A, B, Width, Bank, Place, bRoad, Length, (B.X - A.X) / Length, (B.Y - A.Y) / Length});
}

FDistrict::FDistrict()
{
    // The film's skyway is the east rim of the district. Three junctions connect it
    // to a continuous lower terrace that can be crossed in any direction.
    FPoint P{0, 0, 0};
    double Heading = 0.0;
    constexpr double Step = 5.0;
    for (double S = 0; S < TrackLength; S += Step)
    {
        const double K = Curvature(S + Step / 2);
        const double Middle = Heading - Step * K / 2;
        FPoint Q{P.X + Step * std::cos(Middle), P.Y + Step * std::sin(Middle), 0};
        AddStrip(P, Q, 10, EPlace::Skyway, true, -5.0 * K * 340.0);
        Heading -= Step * K;
        P = Q;
    }
    AddStrip({-75, 0, 0}, {0, 0, 0}, 15, EPlace::Skyway, true);
    AddStrip({-65, 6, 0}, {-65, -14, 0}, 26, EPlace::Transit, false);
    AddStrip({-65, -14, 0}, {-65, -264, TerraceZ}, 15, EPlace::Transit, true);
    AddStrip({180, 6, 0}, {180, -14, 0}, 25, EPlace::Market, false);
    AddStrip({180, -14, 0}, {180, -264, TerraceZ}, 16, EPlace::Market, true);
    AddStrip({P.X-36, P.Y, 0}, {P.X+36, P.Y, 0}, 70, EPlace::Overlook, false);
    AddStrip({P.X-28, P.Y-10, 0}, {325, P.Y-10, TerraceZ}, 16, EPlace::Overlook, true);
    AddStrip({-135, -395, TerraceZ}, {745, -395, TerraceZ}, 640, EPlace::Terrace, false);

    TrafficLoop.assign(Strips.begin(), Strips.begin() + int(TrackLength / Step));
    auto Connect = [&](FPoint B)
    {
        const FPoint A = TrafficLoop.back().B;
        const double L = std::hypot(B.X - A.X, B.Y - A.Y);
        TrafficLoop.push_back({A, B, 10, 0, EPlace::Terrace, true, L, (B.X-A.X)/L, (B.Y-A.Y)/L});
    };
    Connect({P.X-28, P.Y-10, 0});
    Connect({325, P.Y-10, TerraceZ});
    Connect({-65, P.Y-10, TerraceZ});
    Connect({-65, -264, TerraceZ});
    Connect({-65, -14, 0});
    Connect({-65, 0, 0});
    Connect({0, 0, 0});
    double Total = 0;
    for (const FSurface& S : TrafficLoop) { Total += S.Length; TrafficEnds.push_back(Total); }

    // Leave generous streets between city blocks, and keep all three ramp mouths open.
    for (int X : {25, 105, 270, 395, 495, 590, 680})
    {
        for (int Y : {-185, -325, -435, -645})
        {
            if ((X == 270 || X == 395) && Y == -435) continue;
            const double Height = 9.0 + ((X / 5 + (-Y) / 5) % 18);
            Blocks.push_back({double(X), double(Y), 40, 43, Height});
        }
    }
    // A walkable shopping lane opens onto the foot of the market ramp.
    for (int X : MarketStallColumns)
        for (int Y : {-305,-279})
            Blocks.push_back({double(X),double(Y),8.4,5.0,2.45,EBuilding::MarketStall});
    // Platforms flank an open concourse beside the transit ramp.
    for (int X : {-94,-35})
        for (int Y : {-320,-365})
            Blocks.push_back({double(X),double(Y),9,25,3.6,EBuilding::Station});
    for (int Y : {-320,-365})
        Blocks.push_back({-85.5,double(Y),2.2,4.6,1.8,EBuilding::ParkedCab});
}

FStreetPose FDistrict::TrafficAt(double Distance, double Lane) const
{
    double At = std::fmod(Distance, TrafficEnds.back());
    if (At < 0) At += TrafficEnds.back();
    const auto I = size_t(std::upper_bound(TrafficEnds.begin(), TrafficEnds.end(), At) - TrafficEnds.begin());
    const FSurface& S = TrafficLoop[I];
    const double T = (At - (I ? TrafficEnds[I-1] : 0.0)) / S.Length;
    const double Deg = 180.0 / std::numbers::pi;
    return {{std::lerp(S.A.X,S.B.X,T) - Lane * S.ForwardY, std::lerp(S.A.Y,S.B.Y,T) + Lane * S.ForwardX,
             std::lerp(S.A.Z,S.B.Z,T) - Lane * std::tan(S.Bank / Deg)},
            std::atan2(S.ForwardY, S.ForwardX) * Deg, std::atan2(S.B.Z - S.A.Z, S.Length) * Deg, S.Bank};
}

bool FDistrict::InsideBuilding(double X, double Y) const
{
    for (const auto& Block : Blocks)
    {
        if (std::abs(X - Block.X) < Block.WidthX / 2 + 0.55 && std::abs(Y - Block.Y) < Block.WidthY / 2 + 0.55)
            return true;
    }
    return false;
}

FGround FDistrict::Resolve(FPoint Candidate, FPoint Previous) const
{
    double Best = std::numeric_limits<double>::max();
    FGround Ground{Previous, EPlace::Skyway, false};
    for (const auto& S : Strips)
    {
        const double DX = Candidate.X - S.A.X, DY = Candidate.Y - S.A.Y;
        const double Along = std::clamp(DX * S.ForwardX + DY * S.ForwardY, 0.0, S.Length);
        const double Across = std::clamp(-DX * S.ForwardY + DY * S.ForwardX, -S.Width * 0.5 + 0.5, S.Width * 0.5 - 0.5);
        const double Z = std::lerp(S.A.Z, S.B.Z, Along / S.Length) - Across * std::tan(S.Bank * std::numbers::pi / 180.0);
        if (std::abs(Z - Previous.Z) > 1.25) continue;
        const FPoint Nearest{S.A.X + Along * S.ForwardX - Across * S.ForwardY,
                             S.A.Y + Along * S.ForwardY + Across * S.ForwardX, Z};
        const double EX = Nearest.X - Candidate.X, EY = Nearest.Y - Candidate.Y;
        const double PlanarError = EX * EX + EY * EY;
        const double RampPreference = S.A.Z != S.B.Z && PlanarError < 1e-6 ? 0.005 : 0.0;
        const double Error = PlanarError + 0.001 * (Z - Previous.Z) * (Z - Previous.Z) - RampPreference;
        if (Error < Best)
        {
            Best = Error;
            Ground = {Nearest, S.Place};
        }
    }
    if (Ground.Point.Z < -28.5 && InsideBuilding(Ground.Point.X, Ground.Point.Y))
    {
        if (!InsideBuilding(Ground.Point.X, Previous.Y)) Ground.Point.Y = Previous.Y;
        else if (!InsideBuilding(Previous.X, Ground.Point.Y)) Ground.Point.X = Previous.X;
        else Ground.Point = Previous;
    }
    if (Ground.Place == EPlace::Terrace)
    {
        if (Ground.Point.X < 55) Ground.Place = EPlace::Transit;
        else if (Ground.Point.X < 320) Ground.Place = EPlace::Market;
    }
    return Ground;
}
