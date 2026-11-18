#include "CircuitCore.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideHandling, "NeonRide.Circuit.Dynamics",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FNeonRideHandling::RunTest(const FString &)
{
    const auto End = Circuit::At(Circuit::Length());
    const auto Start = Circuit::At(0.);
    TestTrue(TEXT("Closed loop joins at start"),
             FMath::Abs(End.x - Start.x) + FMath::Abs(End.y - Start.y) + FMath::Abs(End.z - Start.z) < .001);
    auto State = Circuit::Start();
    auto Step = [&](double Gas, double Brake, double Steer, bool Boost, int Count) {
        for (int I = 0; I < Count; ++I)
            Circuit::Advance(State, Gas, Brake, Steer, Boost, 1. / 120.);
    };
    State.lateral = 0;
    Step(1, 0, 0, true, 475);
    TestTrue(TEXT("Holding boost depletes reservoir"), State.locked != 0 && State.charge < .01 && State.v > 35);
    Step(0, 1, 0, false, 1250);
    TestTrue(TEXT("Reservoir recovers after cooldown"), State.charge > .99);
    State.s = Circuit::Length() - 1.;
    State.v = 45.;
    Step(0, 0, 0, false, 10);
    TestTrue(TEXT("Distance wraps and counts lap"), State.laps == 1 && State.s < 10.);
    auto Car = Circuit::Traffic(0, State.elapsed);
    State.s = Car.s - Car.half - 1.4;
    State.lateral = Car.lane;
    State.v = 72.;
    Step(1, 0, 0, false, 5);
    TestTrue(TEXT("Hard traffic impact ejects rider"), State.crashAge >= 0.);
    Step(0, 0, 0, false, 50);
    const auto Pose = Circuit::Ejection(State);
    TestTrue(TEXT("Rider launches above the deck"), Pose.riderHeight > 1.35);
    Step(0, 0, 0, false, 390);
    TestTrue(TEXT("Crash recovers to cockpit"), State.crashAge < 0.);
    return true;
}
#endif
