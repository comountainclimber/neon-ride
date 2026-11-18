#include "RideHandling.h"
#include "TrafficRun.h"
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideHandling, "NeonRide.Exploration.DistrictAndHandling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonRideHandling::RunTest(const FString& Parameters)
{
    using namespace NeonRide;
    FDistrict District;
    TestEqual(TEXT("Straight ends at 300 m"), Curvature(300.0), 0.0);
    TestTrue(TEXT("Curve reaches 340 m radius"), FMath::IsNearlyEqual(Curvature(390.0), 1.0 / 340.0));
    for (double ExitX : {-65.0, 180.0})
    {
        FPoint At{ExitX, 0, 0};
        for (int Y = 0; Y >= -275; --Y) At = District.Resolve({ExitX, double(Y), At.Z}, At).Point;
        TestTrue(TEXT("Ramp reaches lower terrace"), FMath::IsNearlyEqual(At.Z, FDistrict::TerraceZ));
        for (int Y = -275; Y <= 0; ++Y) At = District.Resolve({ExitX, double(Y), At.Z}, At).Point;
        TestTrue(TEXT("Ramp can be ridden in reverse"), FMath::IsNearlyZero(At.Z));
    }
    const FPoint Under{395, -214, FDistrict::TerraceZ};
    for (int X=178; X<=182; ++X)
        for (int Y=-264; Y>=-292; --Y)
            if (District.InsideBuilding(X,Y)) { AddError(TEXT("A market stall blocks the ramp entrance")); return false; }
    for (int X=-65; X<=180; ++X)
        if (District.InsideBuilding(X,-292)) { AddError(TEXT("The pedestrian aisle is blocked")); return false; }
    TestTrue(TEXT("Buildings block the street-level bike"), District.Resolve({395,-185,-30}, Under).Point.Y != -185
        || District.Resolve({395,-185,-30}, Under).Point.X != 395);
    FRideState Rider;
    Rider.Speed = CruiseSpeed;
    for (int I = 0; I < 300; ++I) Rider.Advance(District, 1.0 / 120, 0, 1, 0);
    TestEqual(TEXT("Braking stops the bike"), Rider.Speed, 0.0);
    const FPoint Parked = Rider.Position;
    for (int I = 0; I < 240; ++I) Rider.Advance(District, 1.0 / 120, 0, 0, -1);
    TestTrue(TEXT("Parked bike stays put while turning around"), std::hypot(Rider.Position.X-Parked.X,Rider.Position.Y-Parked.Y) < 0.001);
    TestTrue(TEXT("Turn back towards the start"), std::cos(Rider.Heading) < -0.8);
    FStreetTraffic Traffic;
    Traffic.Advance(District, Rider, 0.0);
    TestTrue(TEXT("Vehicle footprint is occupied"), Traffic.Touches(Traffic.Vehicles[0].Pose.Point));
    return true;
}
#endif
