#include "RidePawn.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CircuitImpactFixture.h"
#include "NeonRideSave.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
class FRecoverMotorcycle final : public IAutomationLatentCommand
{
  public:
    FRecoverMotorcycle(ARidePawn *InPawn, FAutomationTestBase *InTest) : P(InPawn), Test(InTest)
    {
        Original.Reset(NewObject<UNeonRideSave>());
        P->CaptureLocation(*Original);
        OldDilation = P->CustomTimeDilation;
        Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Recovery"));
        IFileManager::Get().MakeDirectory(*Folder, true);
    }
    ~FRecoverMotorcycle()
    {
        if (!P.IsValid())
            return;
        Key(EKeys::W, IE_Released, 0);
        Key(EKeys::E, IE_Released, 0);
        P->CustomTimeDilation = OldDilation;
        P->RestoreLocation(*Original);
    }
    bool Update() override
    {
        if (!P.IsValid())
        {
            Test->AddError(TEXT("Lost recovery rider"));
            return true;
        }
        const double Now = FPlatformTime::Seconds();
        const auto &R = P->GetRide();
        if (Phase == 0)
        {
            P->ClosePauseMenu();
            P->CloseDispatch();
            P->PreviewState(Circuit::Start());
            Test->TestTrue(TEXT("Load timed parcel before impact"), P->ReserveParcel(1) && P->ServiceParcel(1));
            P->PreviewState(CircuitImpactAt(.1));
            P->SetActorTickEnabled(true);
            P->CustomTimeDilation = 2.f;
            Test->TestTrue(TEXT("Actual traffic impact starts ejection"), R.crashAge >= 0.);
            Start = Now;
            ++Phase;
            return false;
        }
        if (Now - Start > 23.)
        {
            Test->AddError(
                FString::Printf(TEXT("Recovery stalled in phase %d, %.2f m to bike"), Phase, Circuit::BikeDistance(R)));
            return true;
        }
        if (Phase == 1 && R.foot.active)
        {
            InitialDistance = Circuit::BikeDistance(R);
            InitialTime = R.elapsed;
            BikeS = R.s;
            BikeLane = R.lateral;
            Test->TestTrue(TEXT("Rider stands where her independent body landed"), InitialDistance > 3.);
            P->UseAtBike();
            Test->TestTrue(TEXT("Distant motorcycle rejects remount"), R.foot.active != 0.);
            auto *Save = NewObject<UNeonRideSave>();
            P->CaptureLocation(*Save);
            Test->TestTrue(TEXT("Restoring a dismounted save preserves both locations and cargo"),
                           P->RestoreLocation(*Save) && R.foot.active &&
                               FMath::Abs(Circuit::BikeDistance(R) - InitialDistance) < .02 &&
                               P->GetShift().status[1] == 2);
            Start = Now;
            ++Phase;
            return false;
        }
        if (Phase == 2 && Now - Start > .8)
        {
            Screenshot(TEXT("OnFoot"));
            ++Phase;
            return false;
        }
        if (Phase == 3 && Ready())
        {
            Key(EKeys::W, IE_Pressed, 1);
            Start = Now;
            ++Phase;
            return false;
        }
        if (Phase == 4)
        {
            const auto Foot = Circuit::At(R.foot.s, R.foot.lateral), Bike = Circuit::At(R.s, R.lateral);
            const double Target = FMath::Atan2(Bike.y - Foot.y, Bike.x - Foot.x);
            const double Turn = FMath::FindDeltaAngleRadians(R.foot.yaw, Target);
            if (FMath::Abs(Turn) > .008)
                Key(EKeys::MouseX, IE_Axis, FMath::Clamp(FMath::RadiansToDegrees(Turn) / 1.5, -6., 6.));
            if (Circuit::BikeDistance(R) > 2.05)
                return false;
            Key(EKeys::W, IE_Released, 0);
            Test->TestTrue(TEXT("WASD walks to the stationary motorcycle while the delivery clock runs"),
                           R.s == BikeS && R.lateral == BikeLane && R.elapsed > InitialTime + 2. &&
                               R.foot.active != 0. && P->GetShift().status[1] == 2);
            Start = Now;
            ++Phase;
            return false;
        }
        if (Phase == 5 && Now - Start > .8)
        {
            Test->TestTrue(TEXT("Standing beside the bike waits for explicit input"), R.foot.active != 0.);
            Screenshot(TEXT("BesideBike"));
            ++Phase;
            return false;
        }
        if (Phase == 6 && Ready())
        {
            Key(EKeys::E, IE_Pressed, 1);
            Start = Now;
            ++Phase;
            return false;
        }
        if (Phase == 7 && !R.foot.active)
        {
            Key(EKeys::E, IE_Released, 0);
            Test->TestTrue(TEXT("E restores cockpit control at rest with the same parcel"),
                           R.v < .4 && P->GetShift().status[1] == 2 && R.crashAge < 0.);
            Test->AddInfo(
                FString::Printf(TEXT("Walked from %.2f m to the parked bike and remounted on E"), InitialDistance));
            return true;
        }
        return false;
    }

  private:
    void Key(FKey Button, EInputEvent Event, double Amount)
    {
        if (auto *PC = Cast<APlayerController>(P->GetController()))
            PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Button, Event, Amount,
                                            false, FPlatformTime::Cycles64()));
    }
    void Screenshot(const TCHAR *Name)
    {
        Path = Folder / (FString(Name) + TEXT(".png"));
        Previous = IFileManager::Get().GetTimeStamp(*Path);
        FScreenshotRequest::RequestScreenshot(Path, false, false);
    }
    bool Ready() const
    {
        return !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().GetTimeStamp(*Path) > Previous;
    }
    TWeakObjectPtr<ARidePawn> P;
    FAutomationTestBase *Test;
    TStrongObjectPtr<UNeonRideSave> Original;
    FString Folder, Path;
    FDateTime Previous;
    int Phase = 0;
    double Start = 0., InitialDistance = 0., InitialTime = 0., BikeS = 0., BikeLane = 0.;
    float OldDilation = 1.;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryWalk, "NeonRide.Recovery.WalkAndRemount",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRecoveryWalk::RunTest(const FString &)
{
    for (const auto &C : GEngine->GetWorldContexts())
    {
        auto *W = C.World();
        if (!W || W->WorldType != EWorldType::Game)
            continue;
        auto *PC = W->GetFirstPlayerController();
        auto *P = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
        if (!TestNotNull(TEXT("Possessed motorcycle"), P))
            return false;
        ADD_LATENT_AUTOMATION_COMMAND(FRecoverMotorcycle(P, this));
        return true;
    }
    AddError(TEXT("Run in a standalone game viewport"));
    return false;
}
#endif
