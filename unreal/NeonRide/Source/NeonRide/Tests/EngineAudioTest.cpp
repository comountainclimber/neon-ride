#include "RidePawn.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

class FRecordEngineRide : public IAutomationLatentCommand
{
public:
    FRecordEngineRide(ARidePawn *Pawn,FAutomationTestBase *InTest):Bike(Pawn),Test(InTest) {}
    bool Update() override
    {
        if (!Bike.IsValid()) { Test->AddError(TEXT("Lost motorcycle during engine recording")); return true; }
        if (Phase==0)
        {
            Bike->PreviewState(Circuit::Start());
            Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("AudioQA"));
            IFileManager::Get().MakeDirectory(*Folder,true);
            File = Folder/TEXT("EngineRide.wav");
            Previous=IFileManager::Get().GetTimeStamp(*File);
            UAudioMixerBlueprintLibrary::StartRecordingOutput(Bike.Get(),10.f);
            Start=FPlatformTime::Seconds(); ++Phase;
        }
        const double Sec=FPlatformTime::Seconds()-Start;
        if (Phase==1 && Sec>=1.4) { Key(EKeys::W,true); ++Phase; }
        if (Phase==2 && Sec>=4.6)
        {
            Test->TestTrue(TEXT("Throttle moves the bike through gears"),Bike->GetRide().v>13. && Bike->GetRide().gear>=2.);
            Key(EKeys::LeftShift,true); ++Phase;
        }
        if (Phase==3 && Sec>=6.3)
        {
            Test->TestTrue(TEXT("Boost burns charge under live input"),Bike->GetRide().charge<.89);
            Key(EKeys::LeftShift,false); Key(EKeys::W,false); ++Phase;
        }
        if (Phase==4 && Sec>=7.1) { Key(EKeys::S,true); ++Phase; }
        if (Phase==5 && Sec>=8.3)
        {
            Key(EKeys::S,false);
            UAudioMixerBlueprintLibrary::StopRecordingOutput(Bike.Get(),EAudioRecordingExportType::WavFile,TEXT("EngineRide"),Folder);
            ++Phase;
        }
        if (Phase==6)
        {
            const bool Written=IFileManager::Get().FileSize(*File)>200000 && IFileManager::Get().GetTimeStamp(*File)>Previous;
            if (!Written && Sec<20.) return false;
            Test->TestTrue(TEXT("Packaged mixer renders a fresh driving WAV"),Written);
            Bike->PreviewState(Circuit::Start());
            return true;
        }
        return false;
    }
private:
    void Key(FKey Button,bool Down)
    {
        auto *PC=Cast<APlayerController>(Bike->GetController());
        if(PC) PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Button,
            Down?IE_Pressed:IE_Released,Down?1.f:0.f,false,FPlatformTime::Cycles64()));
    }
    TWeakObjectPtr<ARidePawn> Bike;
    FAutomationTestBase *Test;
    int Phase=0;
    double Start=0;
    FString File,Folder;
    FDateTime Previous;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEngineRideAudio,"NeonRide.Audio.EngineRide",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEngineRideAudio::RunTest(const FString&)
{
    if (FParse::Param(FCommandLine::Get(),TEXT("nosound"))) { AddInfo(TEXT("Audio mixer disabled by -nosound")); return true; }
    for (const auto &Context:GEngine->GetWorldContexts())
    {
        UWorld *World=Context.World();
        if(!World || Context.WorldType!=EWorldType::Game) continue;
        auto *PC=World->GetFirstPlayerController();
        if(auto *Pawn=PC?Cast<ARidePawn>(PC->GetPawn()):nullptr)
        {
            TSet<FName> Waves;
            TInlineComponentArray<UAudioComponent*> Components(Pawn);
            for(const UAudioComponent* Audio:Components)
                if(const auto *Wave=Cast<USoundWave>(Audio->Sound);Wave && Wave->IsLooping()) Waves.Add(Wave->GetFName());
            for (FName Layer : {FName("A_EngineIdle"),FName("A_EngineDrive"),FName("A_EngineRedline"),FName("A_EngineNitro")})
                TestTrue(TEXT("Loaded looping engine stem ")+Layer.ToString(),Waves.Contains(Layer));
            ADD_LATENT_AUTOMATION_COMMAND(FRecordEngineRide(Pawn,this));
            return true;
        }
    }
    AddError(TEXT("Open a standalone ride to record the game mixer.")); return false;
}
#endif
