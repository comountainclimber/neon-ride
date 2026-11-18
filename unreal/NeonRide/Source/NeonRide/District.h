#pragma once

#include <vector>
#include <array>

namespace NeonRide
{
    enum class EPlace { Skyway, Overlook, Market, Transit, Terrace };
    struct FPoint { double X, Y, Z; };
    struct FSurface
    {
        FPoint A, B;
        double Width, Bank;
        EPlace Place;
        bool bRoad;
        double Length, ForwardX, ForwardY;
    };
    enum class EBuilding { CityBlock, MarketStall, Station, ParkedCab };
    inline constexpr std::array<int,5> MarketStallColumns{146,160,170,190,205};
    struct FBuilding { double X, Y, WidthX, WidthY, Height; EBuilding Kind = EBuilding::CityBlock; };
    struct FGround { FPoint Point; EPlace Place; bool bOnSurface = true; };
    struct FStreetPose { FPoint Point; double Yaw, Pitch, Bank; };

    class FDistrict
    {
    public:
        FDistrict();
        FGround Resolve(FPoint Candidate, FPoint Previous) const;
        FStreetPose TrafficAt(double Distance, double Lane) const;
        bool InsideBuilding(double X, double Y) const;
        const std::vector<FSurface>& Surfaces() const { return Strips; }
        const std::vector<FBuilding>& Buildings() const { return Blocks; }
        static constexpr double TerraceZ = -30.0;

    private:
        std::vector<FSurface> Strips;
        std::vector<FBuilding> Blocks;
        std::vector<FSurface> TrafficLoop;
        std::vector<double> TrafficEnds;
        void AddStrip(FPoint A, FPoint B, double Width, EPlace Place, bool bRoad, double Bank = 0.0);
    };
}
