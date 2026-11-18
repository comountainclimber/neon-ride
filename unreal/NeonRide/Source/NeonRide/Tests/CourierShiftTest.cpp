#include "RidePawn.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CourierCore.h"
#include "NeonRideSave.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Skyway.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

namespace
{
bool RideTo(Circuit::Ride &R, int Destination, bool Ramp)
{
    constexpr double Dt = 1. / 120.;
    Courier::Route Route{};
    for (int Step = 0; Step < 120 * 480; ++Step)
    {
        if (Courier::Nearby(R) == Destination)
            return true;
        if (R.crashAge >= 0.)
            return false;
        if (Step % 12 == 0)
            Route = Courier::Directions(R.s, Destination, Ramp);
        const bool Exit = Route.nextRoad > 0 && Route.turnIn < 100.;
        const double Lane = Exit || Courier::RoadOf(R.s) != 0 || Route.meters < 80. ? 4.8 : -.28;
        const double Goal = FMath::Clamp(FMath::Atan2((Lane - R.lateral) * 1.7, FMath::Max(4., R.v)), -.35, .35);
        const auto Frame = Circuit::At(R.s, R.lateral);
        const double MaxRate = FMath::Max(.08, FMath::Min(21. / (R.v + 2.), FMath::Min(R.v * .15, 1.12)));
        const double Input = FMath::Clamp((Frame.curvature * R.v + (Goal - R.headingError) * 3.4) / MaxRate, -1., 1.);
        const double Target = FMath::Min(13., FMath::Sqrt(12. * FMath::Max(0., Route.meters - 8.)));
        Circuit::Advance(R, FMath::Clamp((Target - R.v) * .65 + .12, 0., 1.),
                         FMath::Clamp((R.v - Target) * .45, 0., 1.), Input, false, Dt, Ramp);
    }
    return false;
}
class FCourierFrames : public IAutomationLatentCommand
{
  public:
    FCourierFrames(ARidePawn *P, FAutomationTestBase *T) : Pawn(P), Test(T)
    {
        Original.Reset(NewObject<UNeonRideSave>());
        P->CaptureLocation(*Original);
    }
    ~FCourierFrames()
    {
        if (Pawn.IsValid())
        {
            Pawn->CloseDispatch();
            Pawn->RestoreLocation(*Original);
            Pawn->SetActorTickEnabled(true);
        }
    }
    bool Update() override
    {
        if (!Pawn.IsValid())
        {
            Test->AddError(TEXT("Lost the courier bike"));
            return true;
        }
        const TCHAR *Labels[] = {TEXT("Dispatch"), TEXT("Rainline"),      TEXT("Floodworks"), TEXT("Cinema"),
                                 TEXT("Clinic"),   TEXT("ClinicCounter"), TEXT("FirstLight"), TEXT("ShiftComplete")};
        const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Courier"));
        const FString File = Dir / (FString(Labels[Shot]) + TEXT(".png"));
        if (!Prepared)
        {
            auto &P = *Pawn;
            P.SetActorTickEnabled(false);
            P.ClosePauseMenu();
            if (Shot == 0)
            {
                const auto R = Circuit::Start();
                P.PreviewState(R);
                Test->TestTrue(TEXT("Reserve two panniers"), P.ReserveParcel(0) && P.ReserveParcel(1));
                Test->TestFalse(TEXT("Third parcel cannot reserve a loaded pannier"), P.ReserveParcel(2));
                Test->TestTrue(TEXT("Original repair bay accepts both physical pickups"),
                               P.ServiceParcel(0) && P.ServiceParcel(1));
                P.ToggleDispatch();
            }
            else
            {
                if (Shot == 1)
                    P.CloseDispatch();
                const int Address = Shot == 1 ? 0 : Shot == 2 ? 2 : Shot == 3 || Shot >= 6 ? 4 : 5;
                auto R = Circuit::Start();
                if (Shot == 2)
                {
                    Test->TestTrue(TEXT("Ride from Rainline to Floodworks using throttle, braking and fork steering"),
                                   RideTo(R, 2, false));
                    P.PreviewState(R);
                    Test->TestTrue(TEXT("Floodworks accepts the carried pump controller"), P.ServiceParcel(0));
                    Test->TestFalse(TEXT("Cinema cannot receive its reel at the canal"), P.ServiceParcel(1));
                    UNeonRideSave *Save = NewObject<UNeonRideSave>();
                    P.CaptureLocation(*Save);
                    TArray<uint8> Bytes;
                    Test->TestTrue(TEXT("Saved courier record serializes"),
                                   UGameplayStatics::SaveGameToMemory(Save, Bytes));
                    Save = Cast<UNeonRideSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
                    Test->TestTrue(TEXT("Save restores active cargo on a branch road"),
                                   Save && P.RestoreLocation(*Save) && P.GetShift().status[0] == 3 &&
                                       P.GetShift().status[1] == 2 && P.PannierJob(0) == -1 && P.PannierJob(1) == 1 &&
                                       Courier::RoadOf(P.GetRide().s) == 2);
                }
                if (Shot == 3)
                {
                    R.s = Courier::Stop(4).s;
                    R.lateral = 4.;
                    R.elapsed = 180.;
                    P.PreviewState(R);
                    Test->TestTrue(TEXT("Cinema recipient unlocks the service road"),
                                   P.ServiceParcel(1) && Courier::RampOpen(P.GetShift()));
                }
                R.s = Courier::Stop(Address).s - 25.;
                if (Address == 0)
                    R.s = Circuit::Length() - 25.;
                R.lateral = 2.7;
                if (Shot == 5)
                {
                    R.s = Courier::Stop(1).s;
                    R.lateral = 4.;
                    R.elapsed = 125.;
                    P.PreviewState(R);
                    Test->TestTrue(TEXT("Collect clinic shipment"), P.ReserveParcel(2) && P.ServiceParcel(2));
                    R.s = Courier::Stop(5).s - 2.;
                    R.lateral = 5.35;
                }
                R.elapsed = 185.;
                P.PreviewState(R);
                if (Shot == 5)
                    Test->TestTrue(TEXT("Hand clinic parcel to recipient"), P.ServiceParcel(2));
                if (Shot == 7)
                    P.ToggleDispatch();
            }
            IFileManager::Get().MakeDirectory(*Dir, true);
            Old = IFileManager::Get().GetTimeStamp(*File);
            Start = FPlatformTime::Seconds();
            Prepared = true;
            return false;
        }
        if (!Requested)
        {
            if (FPlatformTime::Seconds() - Start < 1.6)
                return false;
            FScreenshotRequest::RequestScreenshot(File, Shot == 0 || Shot == 7, false);
            Requested = true;
            Start = FPlatformTime::Seconds();
            return false;
        }
        if (FScreenshotRequest::IsScreenshotRequested() && FPlatformTime::Seconds() - Start < 15.)
            return false;
        Test->TestTrue(FString(TEXT("Fresh courier screenshot ")) + Labels[Shot],
                       IFileManager::Get().FileSize(*File) > 0 && IFileManager::Get().GetTimeStamp(*File) > Old);
        Prepared = Requested = false;
        if (++Shot != 6)
            return Shot >= 8;
        auto R = Circuit::Start();
        for (int ShiftNumber = 1; ShiftNumber <= 3; ++ShiftNumber)
        {
            for (int I = ShiftNumber == 1 ? 3 : 0; I < Courier::JobCount; ++I)
            {
                const auto Job = Courier::Contract(I, Pawn->GetShift().number);
                Test->TestTrue(TEXT("Reserve remaining run"), Pawn->ReserveParcel(I));
                R.s = Courier::Stop(Job.from).s;
                Pawn->PreviewState(R);
                Test->TestTrue(TEXT("Pickup at assigned address"), Pawn->ServiceParcel(I));
                R.s = Courier::Stop(Job.to).s;
                Pawn->PreviewState(R);
                Test->TestTrue(TEXT("Handoff at destination"), Pawn->ServiceParcel(I));
            }
            Test->TestTrue(TEXT("Eight completed runs close the shift"), Pawn->GetShift().finished != 0);
            Pawn->CloseDispatch();
            if (ShiftNumber < 3)
                Pawn->FinishShift();
        }
        Test->TestTrue(TEXT("Final First Light shift keeps returning clients and ramp access"),
                       Pawn->GetShift().number == 3 && Pawn->GetShift().visits[4] >= 3 &&
                           Courier::RampOpen(Pawn->GetShift()));
        return false;
    }

  private:
    TWeakObjectPtr<ARidePawn> Pawn;
    FAutomationTestBase *Test;
    TStrongObjectPtr<UNeonRideSave> Original;
    int Shot = 0;
    bool Prepared = false, Requested = false;
    double Start = 0.;
    FDateTime Old;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierShiftTest, "NeonRide.Courier.Delivery",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCourierShiftTest::RunTest(const FString &)
{
    for (const auto &C : GEngine->GetWorldContexts())
    {
        auto *W = C.World();
        if (!W || W->WorldType != EWorldType::Game)
            continue;
        auto *PC = W->GetFirstPlayerController();
        auto *Bike = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
        if (!TestNotNull(TEXT("Courier motorcycle"), Bike))
            return false;
        for (TActorIterator<ASkyway> Road(W); Road; ++Road)
        {
            auto *Mesh = Road->FindComponentByClass<UProceduralMeshComponent>();
            if (TestNotNull(TEXT("City street mesh"), Mesh))
                TestEqual(TEXT("Five drivable road surfaces"), Mesh->GetNumSections(), Courier::RoadCount);
            if (FPlatformProperties::RequiresCookedData())
                TestEqual(TEXT("Six staffed addresses in cooked game"), Road->StaffCount(), Courier::AddressCount);
        }
        ADD_LATENT_AUTOMATION_COMMAND(FCourierFrames(Bike, this));
        return true;
    }
    AddError(TEXT("Run in a standalone city viewport"));
    return false;
}
#endif
