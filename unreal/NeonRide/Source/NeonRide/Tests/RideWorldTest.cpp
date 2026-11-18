#include "RidePawn.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Skyway.h"
#include "Components/SkeletalMeshComponent.h"
#include "RideInstruments.h"
#include "NeonRideSave.h"
#include "TrafficVehicleActor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideWorld, "NeonRide.Circuit.PossessedBike",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FNeonRideWorld::RunTest(const FString &)
{
    UWorld *World = nullptr;
    for (const auto &Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
        {
            World = Context.World();
            break;
        }
    if (!TestNotNull(TEXT("Game world open"), World))
        return false;
    auto *PC = World->GetFirstPlayerController();
    auto *Bike = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
    if (!TestNotNull(TEXT("Possessed circuit bike"), Bike))
        return false;
    int Cars = 0;
    for (TActorIterator<ATrafficVehicleActor> It(World); It; ++It)
        ++Cars;
    TestEqual(TEXT("Authored circuit traffic"), Cars, Circuit::TrafficCount);
    auto *Display = Bike->FindComponentByClass<URideInstruments>();
    if (!TestNotNull(TEXT("Physical dashboard exists"), Display))
        return false;
    auto *Mat = Cast<UMaterialInstanceDynamic>(Display->GetMaterial(0));
    TestNotNull(TEXT("Circuit cluster material cooked"), Mat);
    auto State = Circuit::Start();
    State.s = 1450;
    State.lateral = -3.;
    State.charge = .42;
    State.laps = 2;
    Bike->PreviewState(State);
    if (Mat)
        TestTrue(TEXT("Physical nitro gauge follows reservoir"),
                 FMath::IsNearlyEqual(Mat->K2_GetVectorParameterValue(TEXT("Nitro")).R, .42f, .001f));
    auto *Save = NewObject<UNeonRideSave>();
    Bike->CaptureLocation(*Save);
    Bike->PreviewState(Circuit::Start());
    TestTrue(TEXT("Can reload circuit save at rest"), Bike->RestoreLocation(*Save));
    TestTrue(TEXT("Reload preserves position and charge"),
             Bike->GetRide().s == 1450. && Bike->GetRide().charge == .42 && Bike->GetRide().v == 0);
    Save->Lane = 100;
    TestFalse(TEXT("Invalid save rejected"), Bike->RestoreLocation(*Save));
    State.crashAge = .46;
    State.crashStart = State.s;
    State.crashSpeed = 58;
    State.crashSide = 1;
    Bike->PreviewState(State);
    auto *Rig = Bike->FindComponentByClass<USkeletalMeshComponent>();
    if (TestNotNull(TEXT("Cooked Tripo character rig"), Rig))
    {
        TestTrue(TEXT("Exterior character is visible"), Rig->IsVisible());
        TestTrue(TEXT("Animation follows shared crash clock"), FMath::IsNearlyEqual(Rig->GetPosition(), .46f, .002f));
    }
    Bike->PreviewState(Circuit::Start());
    return true;
}
#endif
