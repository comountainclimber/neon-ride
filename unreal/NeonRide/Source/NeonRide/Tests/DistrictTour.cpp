#include "RidePawn.h"
#include "CircuitImpactFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
class FCircuitTour : public IAutomationLatentCommand
{
    TWeakObjectPtr<ARidePawn> Bike;
    FAutomationTestBase *Test;
    int Shot = 0;
    bool Prepared = false, Requested = false;
    double Start = 0;
    FDateTime Previous;

  public:
    FCircuitTour(ARidePawn *Rider, FAutomationTestBase *InTest) : Bike(Rider), Test(InTest)
    {
    }
    bool Update() override
    {
        if (!Bike.IsValid())
        {
            Test->AddError(TEXT("Lost bike"));
            return true;
        }
        const TCHAR *Names[] = {TEXT("Rainline"),      TEXT("GlassHeights"), TEXT("VioletReach"),
                                TEXT("CrashEject"),    TEXT("CrashRebound"), TEXT("CrashSlide"),
                                TEXT("RecoverOnFoot"), TEXT("Title"),        TEXT("Paused")};
        const auto Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Circuit"));
        const auto Path = Folder / (FString(Names[Shot]) + TEXT(".png"));
        if (!Prepared)
        {
            IFileManager::Get().MakeDirectory(*Folder, true);
            Previous = IFileManager::Get().GetTimeStamp(*Path);
            auto State = Circuit::Start();
            State.s = Shot == 0 ? 0. : Shot == 1 ? 1570. : Shot == 2 ? 2670. : 1450.;
            State.charge = Shot == 0 ? 1. : .45;
            State.boost = Shot == 1;
            State.v = Shot == 0 ? 0 : 45;
            State.elapsed = Shot == 0 ? 0 : 30;
            if (Shot >= 3 && Shot < 6)
            {
                const double Phase[] = {.48, 1.32, 2.3};
                State = CircuitImpactAt(Phase[Shot - 3]);
                if (State.crashAge < 0.)
                    Test->AddError(TEXT("Production traffic contact failed to start the crash shot"));
            }
            if (Shot == 6)
            {
                State = CircuitImpactAt(.15);
                for (int I = 0; I < 900 && State.crashAge >= 0.; ++I)
                    Circuit::Advance(State, 0., 0., 0., false, 1. / 120., false, true);
                Test->TestTrue(TEXT("Crash tour lands on foot beside the parked bike"), State.foot.active != 0.);
            }
            Bike->SetActorTickEnabled(false);
            Bike->PreviewState(State);
            if (Shot >= 7)
                Bike->PreviewMenu(Shot == 7);
            Prepared = true;
            Start = FPlatformTime::Seconds();
            return false;
        }
        if (!Requested)
        {
            if (FPlatformTime::Seconds() - Start < 4.)
                return false;
            FScreenshotRequest::RequestScreenshot(Path, Shot >= 7, false);
            Requested = true;
            Start = FPlatformTime::Seconds();
            return false;
        }
        if (FScreenshotRequest::IsScreenshotRequested() && FPlatformTime::Seconds() - Start < 20.)
            return false;
        if (IFileManager::Get().FileSize(*Path) <= 0 || IFileManager::Get().GetTimeStamp(*Path) <= Previous)
            Test->AddError(TEXT("Missing fresh circuit screenshot ") + Path);
        Prepared = Requested = false;
        if (Shot >= 7)
            Bike->ClosePauseMenu();
        if (++Shot < UE_ARRAY_COUNT(Names))
            return false;
        Bike->PreviewState(Circuit::Start());
        Bike->SetActorTickEnabled(true);
        return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideTour, "NeonRide.Visual.CircuitTour",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FNeonRideTour::RunTest(const FString &)
{
    for (const auto &Context : GEngine->GetWorldContexts())
    {
        auto *World = Context.World();
        if (!World || Context.WorldType != EWorldType::Game)
            continue;
        auto *PC = World->GetFirstPlayerController();
        if (auto *Bike = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr)
        {
            ADD_LATENT_AUTOMATION_COMMAND(FCircuitTour(Bike, this));
            return true;
        }
    }
    AddError(TEXT("Open a standalone circuit viewport"));
    return false;
}
#endif
