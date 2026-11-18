#include "RidePawn.h"
#include "Skyway.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideWorld, "NeonRide.Exploration.PossessedBike",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonRideWorld::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("Game world is open"), World)) return false;
    auto* PC = World->GetFirstPlayerController();
    auto* Bike = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
    if (!TestNotNull(TEXT("Player possesses the bike"), Bike)) return false;
    auto* Input = Bike->FindComponentByClass<UInputComponent>();
    auto* Camera = Bike->FindComponentByClass<UCameraComponent>();
    if (!TestNotNull(TEXT("Input exists"), Input) || !TestNotNull(TEXT("First-person camera exists"), Camera)) return false;
    TActorIterator<ASkyway> Route(World);
    if (!TestTrue(TEXT("District exists"), bool(Route))) return false;
    TestTrue(TEXT("Film spline bends left"), Route->FrameAt(600.0).GetLocation().Y < -1000.0);
    ++Route;
    TestFalse(TEXT("Only one district exists"), bool(Route));

    auto Axis = [&](FName Name, float Value)
    {
        for (auto& Binding : Input->AxisBindings)
            if (Binding.AxisName == Name) { Binding.AxisDelegate.Execute(Value); return; }
        AddError(FString::Printf(TEXT("Missing axis %s"), *Name.ToString()));
    };
    auto Action = [&](FName Name)
    {
        for (int I = 0; I < Input->GetNumActionBindings(); ++I)
        {
            auto& Binding = Input->GetActionBinding(I);
            if (Binding.GetActionName() == Name) { Binding.ActionDelegate.Execute(EKeys::AnyKey); return; }
        }
        AddError(FString::Printf(TEXT("Missing action %s"), *Name.ToString()));
    };
    auto Drive = [&](int Frames) { for (int I = 0; I < Frames; ++I) Bike->Tick(1.f / 120.f); };

    Action(TEXT("RestartRide"));
    const float InitialFov = Camera->FieldOfView;
    Axis(TEXT("Throttle"), 1); Drive(150);
    TestTrue(TEXT("Throttle accelerates and widens the lens"), Bike->GetRide().Speed > 40 && Camera->FieldOfView > InitialFov);
    Axis(TEXT("Throttle"), 0); Axis(TEXT("Brake"), 1); Drive(360);
    TestEqual(TEXT("Brake parks the rider"), Bike->GetRide().Speed, 0.0);
    const FVector Parked = Bike->GetActorLocation();
    Axis(TEXT("Brake"), 0); Axis(TEXT("Steer"), -1); Drive(240);
    TestTrue(TEXT("Can turn around in place"), std::cos(Bike->GetRide().Heading) < -0.8 && Bike->GetActorLocation().Equals(Parked, 1.0));
    Axis(TEXT("Steer"), 0);
    Axis(TEXT("LookX"), 40); Drive(1);
    TestTrue(TEXT("Free look moves the helmet camera"), Camera->GetRelativeRotation().Yaw > 45.0);
    Action(TEXT("LookAhead")); Drive(1);
    TestTrue(TEXT("Recenter returns camera forward"), FMath::Abs(Camera->GetRelativeRotation().Yaw) < 2.0);
    const bool WasOpen = Bike->IsMapOpen();
    Action(TEXT("DistrictMap"));
    TestTrue(TEXT("Map toggles"), WasOpen != Bike->IsMapOpen());
    Action(TEXT("DistrictMap")); Action(TEXT("RestartRide"));
    TestEqual(TEXT("Return to skyway resets trip distance"), Bike->GetRide().Odometer, 0.0);
    return true;
}
#endif
