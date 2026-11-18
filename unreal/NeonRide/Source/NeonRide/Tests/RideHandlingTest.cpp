#include "RideHandling.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideHandling, "NeonRide.Handling.ReferenceAndControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonRideHandling::RunTest(const FString& Parameters)
{
    using namespace NeonRide;
    constexpr double Dt = 1.0 / 120.0;
    auto Run = [&](FRideState& Ride, int Frames, double Throttle, double Brake, double Steer)
    {
        for (int I = 0; I < Frames; ++I) Ride.Advance(Dt, Throttle, Brake, Steer);
    };

    TestEqual(TEXT("Straight ends at 300 m"), Curvature(300.0), 0.0);
    TestTrue(TEXT("Ramp midpoint reaches half curvature"), FMath::IsNearlyEqual(Curvature(345.0), 0.5 / 340.0));
    TestTrue(TEXT("Curve radius at 390 m"), FMath::IsNearlyEqual(Curvature(390.0), 1.0 / 340.0));

    FRideState Rider;
    Run(Rider, 120, 0, 0, 0);
    TestTrue(TEXT("Cruise covers 34 m per second"), FMath::IsNearlyEqual(Rider.Distance, 34.0, 0.0001));
    Run(Rider, 16, 0, 0, -1);
    TestTrue(TEXT("Left input initiates a left lean"), Rider.Roll < -1.0);
    Run(Rider, 800, 0, 0, -1);
    TestTrue(TEXT("Left rail retains clearance"), Rider.Offset >= -EdgeLimit && Rider.Offset < -4.0);
    Run(Rider, 650, 0, 0, 1);
    TestTrue(TEXT("Rider can leave left edge and reach right edge"), Rider.Offset > 4.0 && Rider.Offset <= EdgeLimit);

    Rider = FRideState();
    Rider.Distance = 400;
    Run(Rider, 500, 1, 0, 0);
    TestEqual(TEXT("Throttle reaches film top speed"), Rider.Speed, TopSpeed);
    TestTrue(TEXT("Bank into the constant left turn"), Rider.Roll < -40.0 && Rider.Roll >= -54.0);
    Run(Rider, 600, 1, 1, 0);
    TestEqual(TEXT("Full brake can stop against throttle"), Rider.Speed, 0.0);
    const double StoppedAt = Rider.Distance;
    Run(Rider, 120, 0, 1, 1);
    TestEqual(TEXT("Stopped distance holds"), Rider.Distance, StoppedAt);
    TestTrue(TEXT("Stationary steering cannot slide the rider"), FMath::IsNearlyEqual(Rider.SideSpeed, 0.0));
    return true;
}
#endif
