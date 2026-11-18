#include "RidePawn.h"
#include "Skyway.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideWorld, "NeonRide.Gameplay.PlayableCourse",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonRideWorld::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
        {
            World = Context.World();
            break;
        }
    }
    if (!TestNotNull(TEXT("Game world is open"), World)) return false;
    auto* PC = World->GetFirstPlayerController();
    auto* Bike = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
    if (!TestNotNull(TEXT("Player possesses the bike"), Bike)) return false;
    auto* Input = Bike->FindComponentByClass<UInputComponent>();
    auto* Camera = Bike->FindComponentByClass<UCameraComponent>();
    if (!TestNotNull(TEXT("Input exists"), Input) || !TestNotNull(TEXT("First-person camera exists"), Camera)) return false;

    TActorIterator<ASkyway> Route(World);
    if (!TestTrue(TEXT("Skyway exists"), bool(Route))) return false;
    const FTransform Turn = Route->FrameAt(600.0);
    TestTrue(TEXT("Spline bends left"), Turn.GetLocation().Y < -1000.0 && Turn.Rotator().Yaw < -15.0);
    ++Route;
    TestFalse(TEXT("Only one skyway is present"), bool(Route));

    auto Axis = [&](FName Name, float Value)
    {
        for (auto& Binding : Input->AxisBindings)
        {
            if (Binding.AxisName == Name) { Binding.AxisDelegate.Execute(Value); return true; }
        }
        AddError(FString::Printf(TEXT("Missing input axis %s"), *Name.ToString()));
        return false;
    };
    auto Restart = [&]()
    {
        for (int I = 0; I < Input->GetNumActionBindings(); ++I)
        {
            auto& Action = Input->GetActionBinding(I);
            if (Action.GetActionName() == TEXT("RestartRide"))
            {
                Action.ActionDelegate.Execute(EKeys::R);
                return true;
            }
        }
        AddError(TEXT("Missing restart action"));
        return false;
    };
    auto Drive = [&](int Frames)
    {
        for (int I = 0; I < Frames; ++I) Bike->Tick(1.f / 120.f);
    };

    if (!Restart()) return false;
    const float StartFov = Camera->FieldOfView;
    Axis(TEXT("Throttle"), 1);
    Axis(TEXT("Steer"), -1);
    Drive(50);
    TestTrue(TEXT("Steering moves the bike left"), Bike->GetRide().Offset < 1.6);
    TestTrue(TEXT("Helmet rolls with a fraction of bike lean"),
        FMath::IsNearlyEqual(Camera->GetRelativeRotation().Roll, 0.42 * Bike->GetRide().Roll, 0.01));
    Axis(TEXT("Steer"), 0);
    Drive(1050);
    TestTrue(TEXT("Throttle reaches the curve and widens the lens"),
        Bike->GetRide().Distance > 390.0 && Camera->FieldOfView > StartFov + 5.0);
    Axis(TEXT("Brake"), 1);
    Drive(490);
    TestEqual(TEXT("Brake stops the bike"), Bike->GetRide().Speed, 0.0);
    Axis(TEXT("Brake"), 0);
    Drive(2000);
    TestTrue(TEXT("Course end restarts the run"), Bike->GetRide().Distance < 500.0);
    Axis(TEXT("Throttle"), 0);
    Restart();
    TestEqual(TEXT("Restart clears distance"), Bike->GetRide().Distance, 0.0);
    TestEqual(TEXT("Restart restores cruise speed"), Bike->GetRide().Speed, NeonRide::CruiseSpeed);
    return true;
}
#endif
