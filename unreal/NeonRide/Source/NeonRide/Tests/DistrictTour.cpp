#include "RidePawn.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    class FDistrictTour : public IAutomationLatentCommand
    {
    public:
        FDistrictTour(APlayerController* InController, FAutomationTestBase* InTest)
            : PC(InController), Test(InTest), Started(FPlatformTime::Seconds()) {}

        virtual bool Update() override
        {
            if (!PC.IsValid()) { Test->AddError(TEXT("Lost player's game world")); return true; }
            const TCHAR* Names[] = {TEXT("Skyway"), TEXT("Market"), TEXT("Overlook"), TEXT("Terrace")};
            const FVector Eyes[] = {FVector::ZeroVector, FVector(14300,-29300,-2825), FVector(55600,-55100,500), FVector(-6650,-30700,-2830)};
            const FVector Targets[] = {FVector::ZeroVector, FVector(20500,-29400,-2690), FVector(65800,-57600,1850), FVector(-9100,-36300,-2680)};
            const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/DistrictTour"));
            const FString File = Folder / (FString(Names[Shot]) + TEXT(".png"));
            if (!bPrepared)
            {
                IFileManager::Get().MakeDirectory(*Folder,true);
                PriorCapture = IFileManager::Get().GetTimeStamp(*File);
                if (Shot == 0)
                {
                    OriginalTarget = PC->GetViewTarget();
                    if (auto* Bike = Cast<ARidePawn>(PC->GetPawn())) Bike->SetActorTickEnabled(false);
                }
                else
                {
                    if (!Camera.IsValid()) Camera = PC->GetWorld()->SpawnActor<ACameraActor>();
                    if (!Camera.IsValid()) { Test->AddError(TEXT("Could not create tour camera")); return true; }
                    Camera->SetActorLocationAndRotation(Eyes[Shot], (Targets[Shot]-Eyes[Shot]).Rotation());
                    Camera->GetCameraComponent()->SetFieldOfView(82);
                    PC->SetViewTarget(Camera.Get());
                }
                Started = FPlatformTime::Seconds();
                bPrepared = true;
                return false;
            }
            if (!bRequested)
            {
                if (FPlatformTime::Seconds() - Started < 5.0) return false;
                FScreenshotRequest::RequestScreenshot(File,false,false);
                bRequested = true;
                Started = FPlatformTime::Seconds();
                return false;
            }
            if (FScreenshotRequest::IsScreenshotRequested() && FPlatformTime::Seconds()-Started < 20.0) return false;
            if (IFileManager::Get().FileSize(*File) <= 0 || IFileManager::Get().GetTimeStamp(*File)<=PriorCapture)
                Test->AddError(TEXT("A fresh rendered view was not saved: ") + File);
            bPrepared = bRequested = false;
            if (++Shot < 4) return false;
            PC->SetViewTarget(OriginalTarget.Get());
            if (auto* Bike = Cast<ARidePawn>(PC->GetPawn())) Bike->SetActorTickEnabled(true);
            if (Camera.IsValid()) Camera->Destroy();
            return true;
        }
    private:
        TWeakObjectPtr<APlayerController> PC;
        TWeakObjectPtr<AActor> OriginalTarget;
        TWeakObjectPtr<ACameraActor> Camera;
        FAutomationTestBase* Test;
        double Started;
        FDateTime PriorCapture;
        int Shot = 0;
        bool bPrepared = false, bRequested = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonRideTour, "NeonRide.Visual.DistrictTour",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FNeonRideTour::RunTest(const FString& Parameters)
{
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (World && Context.WorldType == EWorldType::Game && World->GetFirstPlayerController())
        {
            ADD_LATENT_AUTOMATION_COMMAND(FDistrictTour(World->GetFirstPlayerController(), this));
            return true;
        }
    }
    AddError(TEXT("Open Neon Ride in a standalone game viewport to capture the district."));
    return false;
}
#endif
