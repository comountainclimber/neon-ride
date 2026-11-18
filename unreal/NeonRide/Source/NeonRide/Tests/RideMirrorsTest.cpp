#if WITH_DEV_AUTOMATION_TESTS
#include "RideMirrors.h"
#include "RidePawn.h"
#include "Skyway.h"
#include "TrafficVehicleActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"

namespace
{
    struct FOriginalSurface
    {
        TWeakObjectPtr<UStaticMeshComponent> Mesh;
        int32 Slot;
        TStrongObjectPtr<UMaterialInterface> Material;
    };

    // Mark the actual traffic mesh for one exposure. A paired, unmarked image at
    // precisely the same time lets image analysis isolate the car in the mirror.
    class FLiveMirrorFrames final : public IAutomationLatentCommand
    {
    public:
        FLiveMirrorFrames(ARidePawn* InPawn, URideMirrors* InCapture, ASkyway* InTrack, FAutomationTestBase* InTest)
            : Pawn(InPawn), Capture(InCapture), Track(InTrack), Test(InTest), OriginalRide(InPawn->GetRide()),
              WasTicking(InPawn->IsActorTickEnabled())
        {
            Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/RearMirrors"));
            IFileManager::Get().MakeDirectory(*Folder,true);
            TInlineComponentArray<UProceduralMeshComponent*> Parts(InPawn);
            for (auto* Part:Parts)
                if (Part->GetFName()==TEXT("MirrorGlass")) {Glass=Part;break;}
            if (Glass.IsValid()) WasGlassVisible=Glass->IsVisible();
        }

        ~FLiveMirrorFrames()
        {
            ClearTag();
            if (Glass.IsValid()) Glass->SetVisibility(WasGlassVisible);
            if (Pawn.IsValid() && Track.IsValid())
            {
                Pawn->PreviewState(OriginalRide);
                Pawn->SetActorTickEnabled(WasTicking);
            }
        }

        bool Update() override
        {
            if (!Pawn.IsValid() || !Track.IsValid() || !Capture.IsValid())
            {
                Test->AddError(TEXT("Lost the riding world during mirror capture"));
                return true;
            }
            if (Shot==8)
            {
                if (!Prepared)
                {
                    PreviousSerial=Capture->GetCaptureSerial();
                    PreviousTime=Capture->GetLastCaptureTime();
                    Pawn->SetActorTickEnabled(true);
                    Start=FPlatformTime::Seconds();
                    Prepared=true;
                }
                if (Capture->GetCaptureSerial()<=PreviousSerial+1 || Capture->GetLastCaptureTime()<=PreviousTime)
                {
                    if (FPlatformTime::Seconds()-Start<5.) return false;
                    Test->AddError(TEXT("Normal gameplay ticks failed to refresh the rear camera"));
                }
                Test->AddInfo(FString::Printf(TEXT("Rear capture serial %llu -> %llu during live riding ticks"),
                    static_cast<uint64>(PreviousSerial),static_cast<uint64>(Capture->GetCaptureSerial())));
                return true;
            }
            const bool Bare=Shot<0;
            const int32 Stage=Bare?0:Shot%4;
            const bool Left=Shot<4, Near=Stage>=2, Tagged=Stage==1 || Stage==2;
            const FString Path=Bare?Folder/TEXT("Original-Bezel.png"):
                Folder/FString::Printf(TEXT("%s-%s-%s.png"),Left?TEXT("Left"):TEXT("Right"),
                    Near?TEXT("Near"):TEXT("Far"),Tagged?TEXT("Tagged"):TEXT("Clean"));
            if (!Prepared)
            {
                ClearTag();
                if (!Test->TestNotNull(TEXT("Physical mirror insert geometry"),Glass.Get())) return true;
                Glass->SetVisibility(!Bare);
                Pawn->ClosePauseMenu();
                Pawn->SetActorTickEnabled(false);
                const int32 Index=Left?1:0;  // production cab on the left and truck on the right
                const double BaseTime=12.;
                const auto AtStart=Circuit::Traffic(Index,BaseTime);
                auto State=Circuit::Start();
                State.s=AtStart.s+(Left?15.:19.);
                State.lateral=0.;
                State.elapsed=BaseTime+(Near?.30:0.);
                const auto Car=Circuit::Traffic(Index,State.elapsed);
                State.elapsed-=.12;
                Pawn->PreviewState(State);
                State.elapsed+=.12;
                Pawn->PreviewState(State);
                const FVector Expected=Track->FrameAt(Car.s,Car.lane).GetLocation();
                ATrafficVehicleActor* Subject=nullptr;
                for (TActorIterator<ATrafficVehicleActor> It(Pawn->GetWorld());It;++It)
                    if (FVector::DistSquared(It->GetActorLocation(),Expected)<25.) {Subject=*It;break;}
                if (!Test->TestNotNull(TEXT("Production traffic occupies its scheduled rear position"),Subject)) return true;
                if (Tagged && !Tag(*Subject)) return true;
                // Replay a prior time before the final capture, so changing the material
                // exercises the normal 24 Hz capture gate at the same simulation instant.
                State.elapsed-=.12;
                Pawn->PreviewState(State);
                State.elapsed+=.12;
                PreviousSerial=Capture->GetCaptureSerial();
                Pawn->PreviewState(State);
                Test->TestTrue(TEXT("Rear camera accepted the scheduled traffic frame"),Capture->GetCaptureSerial()>PreviousSerial);
                PreviousWrite=IFileManager::Get().GetTimeStamp(*Path);
                Start=FPlatformTime::Seconds();
                Prepared=true;
                return false;
            }
            if (!Requested)
            {
                if (FPlatformTime::Seconds()-Start<.8) return false;
                if (!Bare && Stage==0 && !SaveGlassBounds(Left)) return true;
                FScreenshotRequest::RequestScreenshot(Path,false,false);
                Requested=true;
                Start=FPlatformTime::Seconds();
                return false;
            }
            if (FScreenshotRequest::IsScreenshotRequested() && FPlatformTime::Seconds()-Start<12.) return false;
            Test->TestTrue(TEXT("Fresh physical cockpit screenshot ")+Path,
                IFileManager::Get().FileSize(*Path)>0 && IFileManager::Get().GetTimeStamp(*Path)>PreviousWrite);
            ++Shot;Prepared=false;Requested=false;
            return false;
        }

    private:
        bool SaveGlassBounds(bool LeftTraffic)
        {
            auto* PC=Cast<APlayerController>(Pawn->GetController());
            if (!Glass.IsValid() || !PC) return false;
            FString Rectangles;
            for (int32 Side=0;Side<2;++Side)
            {
                auto* Section=Glass->GetProcMeshSection(Side);
                if (!Test->TestNotNull(TEXT("Both mirror glasses have renderable sections"),Section)) return false;
                FBox2D Bounds(ForceInit);
                for (const auto& Vertex:Section->ProcVertexBuffer)
                {
                    FVector2D Screen;
                    if (PC->ProjectWorldLocationToScreen(Glass->GetComponentTransform().TransformPosition(Vertex.Position),Screen))
                        Bounds+=Screen;
                }
                if (!Test->TestTrue(TEXT("Mirror projects to a nonempty cockpit pixel area"),
                    Bounds.bIsValid && Bounds.GetSize().GetMin()>3.)) return false;
                Rectangles+=FString::Printf(TEXT("%.3f %.3f %.3f %.3f\n"),Bounds.Min.X,Bounds.Min.Y,Bounds.Max.X,Bounds.Max.Y);
            }
            return FFileHelper::SaveStringToFile(Rectangles,*(Folder/(LeftTraffic?TEXT("Left-glass.txt"):TEXT("Right-glass.txt"))));
        }
        bool Tag(ATrafficVehicleActor& Subject)
        {
            auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Neon.M_Neon"));
            if (!Test->TestNotNull(TEXT("Probe material available"),Base)) return false;
            UMaterialInstanceDynamic* Green=UMaterialInstanceDynamic::Create(Base,&Subject);
            Green->SetVectorParameterValue(TEXT("Color"),FLinearColor(.002f,.60f,.002f));
            TInlineComponentArray<UStaticMeshComponent*> Parts(&Subject);
            for (auto* Part:Parts)
            {
                if (!Part->IsVisible() || !Part->GetStaticMesh()) continue;
                for (int32 Slot=0;Slot<Part->GetNumMaterials();++Slot)
                {
                    Originals.Add({Part,Slot,TStrongObjectPtr<UMaterialInterface>(Part->GetMaterial(Slot))});
                    Part->SetMaterial(Slot,Green);
                }
            }
            return Test->TestTrue(TEXT("Original traffic geometry has a visible diagnostic surface"),Originals.Num()>0);
        }
        void ClearTag()
        {
            for (const auto& Surface:Originals)
                if (Surface.Mesh.IsValid()) Surface.Mesh->SetMaterial(Surface.Slot,Surface.Material.Get());
            Originals.Reset();
        }
        TWeakObjectPtr<ARidePawn> Pawn;
        TWeakObjectPtr<URideMirrors> Capture;
        TWeakObjectPtr<ASkyway> Track;
        TWeakObjectPtr<UProceduralMeshComponent> Glass;
        FAutomationTestBase* Test;
        Circuit::Ride OriginalRide;
        bool WasTicking,WasGlassVisible=true,Prepared=false,Requested=false;
        int32 Shot=-1;
        uint64 PreviousSerial=0;
        double PreviousTime=0.,Start=0.;
        FString Folder;
        FDateTime PreviousWrite;
        TArray<FOriginalSurface> Originals;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRideMirrorsTest,"NeonRide.Visual.LiveRearMirrors",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FRideMirrorsTest::RunTest(const FString&)
{
    for (const auto& Context:GEngine->GetWorldContexts())
    {
        UWorld* World=Context.World();
        if (!World || World->WorldType!=EWorldType::Game) continue;
        auto* PC=World->GetFirstPlayerController();
        auto* Bike=PC?Cast<ARidePawn>(PC->GetPawn()):nullptr;
        if (!TestNotNull(TEXT("A possessed motorcycle is available"),Bike)) return false;
        auto* Capture=Bike->FindComponentByClass<URideMirrors>();
        if (!TestNotNull(TEXT("Live rear camera is installed"),Capture)) return false;
        TActorIterator<ASkyway> Track(World);
        if (!Track) {AddError(TEXT("Missing circuit"));return false;}
        ADD_LATENT_AUTOMATION_COMMAND(FLiveMirrorFrames(Bike,Capture,*Track,this));
        return true;
    }
    AddError(TEXT("Run in a standalone game viewport"));return false;
}
#endif
