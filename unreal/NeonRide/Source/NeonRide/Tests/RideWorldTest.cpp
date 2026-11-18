#include "RidePawn.h"
#include "Skyway.h"
#include "NeonRideSave.h"
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
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
    TSet<FString> FilmModels;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Components(*It);
        for (const auto* Part : Components)
        {
            const auto* Instances = Cast<UInstancedStaticMeshComponent>(Part);
            if (!Part->IsVisible() || !Part->GetStaticMesh() || (Instances && Instances->GetInstanceCount() == 0)) continue;
            const FString Asset = Part->GetStaticMesh()->GetPathName();
            if (Asset.StartsWith(TEXT("/Game/Film/"))) FilmModels.Add(Asset);
        }
    }
    TestEqual(TEXT("All original film models are used by visible game components"), FilmModels.Num(), 18);

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
    TestEqual(TEXT("Rider can look around before setting off"),Bike->GetRide().Speed,0.0);
    Axis(TEXT("Throttle"), 1); Drive(760);
    TestTrue(TEXT("Throttle accelerates and widens the lens"), Bike->GetRide().Speed > 38 && Camera->FieldOfView > InitialFov);
    Action(TEXT("MountBike"));
    TestFalse(TEXT("Remain in the saddle at road speed"),Bike->IsWalking());
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
    Action(TEXT("MountBike"));
    TestTrue(TEXT("Parked rider can dismount"),Bike->IsWalking());
    Axis(TEXT("WalkForward"),1); Drive(240);
    TestTrue(TEXT("Walk away from the bike"),Bike->GetRide().Position.X>6.5 && !Bike->CanMount());
    Action(TEXT("MountBike"));
    TestTrue(TEXT("Parked motorcycle remains where it was left"),Bike->IsWalking() && Bike->GetParkedPosition().X==0);
    auto* Save = NewObject<UNeonRideSave>();
    Bike->CaptureLocation(*Save);
    const FString TestSlot = TEXT("NeonRide_QA_") + FGuid::NewGuid().ToString();
    TestTrue(TEXT("Exploration save reaches disk"),UGameplayStatics::SaveGameToSlot(Save,TestSlot,0));
    auto* RoundTrip = Cast<UNeonRideSave>(UGameplayStatics::LoadGameFromSlot(TestSlot,0));
    TestTrue(TEXT("Temporary test save is removed"),UGameplayStatics::DeleteGameInSlot(TestSlot,0));
    Action(TEXT("RestartRide"));
    if (!TestNotNull(TEXT("Saved exploration can be reloaded"),RoundTrip)) return false;
    TestTrue(TEXT("Restores the pedestrian and parked bike separately"),Bike->RestoreLocation(*RoundTrip)
        && Bike->IsWalking() && !Bike->CanMount() && Bike->GetRide().Position.X>6.5);
    Axis(TEXT("WalkForward"),-1); Drive(240); Axis(TEXT("WalkForward"),0);
    Action(TEXT("MountBike"));
    TestTrue(TEXT("Walk back and remount the motorcycle"),!Bike->IsWalking() && Bike->GetRide().Speed==0);
    RoundTrip->BikePosition = FVector(395,-185,-30);
    TestFalse(TEXT("A saved location inside a building is rejected"),Bike->RestoreLocation(*RoundTrip));
    Action(TEXT("PauseRide"));
    TestTrue(TEXT("In-game pause menu stops the world"),UGameplayStatics::IsGamePaused(World));
    Bike->ClosePauseMenu();
    TestFalse(TEXT("Resuming restores game input"),UGameplayStatics::IsGamePaused(World));
    return true;
}
#endif
