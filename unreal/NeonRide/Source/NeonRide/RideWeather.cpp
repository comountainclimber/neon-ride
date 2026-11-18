#include "RideWeather.h"
#include "CircuitCore.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

void URideWeather::StartWeather()
{
    auto Loop = [&](const TCHAR *Name, float Level) {
        const FString Asset = FString::Printf(TEXT("/Game/Environment/%s.%s"), Name, Name);
        auto *Sound = LoadObject<USoundWave>(nullptr, *Asset, nullptr, LOAD_NoWarn);
        if (!Sound)
            return static_cast<UAudioComponent *>(nullptr);
        auto *Audio = NewObject<UAudioComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(Audio);
        Audio->SetupAttachment(this);
        Audio->bAutoActivate = false;
        Audio->SetSound(Sound);
        Audio->SetVolumeMultiplier(Level);
        Audio->RegisterComponent();
        Audio->Play();
        return Audio;
    };
    Impact = LoadObject<USoundWave>(nullptr, TEXT("/Game/Environment/A_Impact.A_Impact"));
    for (const TCHAR *Stem : {TEXT("A_EngineIdle"),TEXT("A_EngineDrive"),TEXT("A_EngineRedline"),TEXT("A_EngineNitro")})
        EngineStems.Add(Loop(Stem,.0001f));
    WindLoop = Loop(TEXT("A_Wind"), 0.08f);
    RainLoop = Loop(TEXT("A_Rain"), .18f);

    auto *Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto *Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_Neon.M_Neon"), nullptr, LOAD_NoWarn);
    if (!Mesh || !Glow)
        return;
    Rain = NewObject<UInstancedStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Rain);
    Rain->SetupAttachment(this);
    Rain->SetMobility(EComponentMobility::Movable);
    Rain->SetStaticMesh(Mesh);
    Rain->SetCastShadow(false);
    Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Rain->RegisterComponent();
    if (auto *Tint = Rain->CreateDynamicMaterialInstance(0, Glow))
        Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.050f, 0.074f, 0.10f));
    FRandomStream Stream(1083);
    for (int I = 0; I < 180; ++I)
    {
        Seeds.Emplace(Stream.FRandRange(0.f, 2800.f), Stream.FRandRange(0.f, 2800.f), Stream.FRandRange(0.f, 1600.f));
        Rain->AddInstance(FTransform::Identity);
    }
    UpdateWeather(0.f,Circuit::Start(),0.,0.);
}

void URideWeather::SetAudioPaused(bool Paused)
{
    for (UAudioComponent *Audio : EngineStems) if (Audio) Audio->SetPaused(Paused);
    if (WindLoop) WindLoop->SetPaused(Paused);
    if (RainLoop) RainLoop->SetPaused(Paused);
}

void URideWeather::UpdateWeather(float DeltaSeconds,const Circuit::Ride &Ride,double Throttle,double Brake)
{
    const double Revs = FMath::Clamp((Ride.rpm-.24)/.73,0.,1.);
    const double Load = Brake>.01 || Ride.crashAge>=0. ? 0. : FMath::Clamp(Throttle,0.,1.);
    const double Motor = Ride.crashAge>=0. ? .20 : 1.;
    const double Gains[] = {
        Motor*FMath::Square(1.-Revs)*.38,
        Motor*(.18+.42*Load)*(.4+.6*FMath::Sin(PI*Revs)),
        Motor*FMath::Pow(Revs,1.6)*(.15+.35*Load),
        .36*Ride.boost
    };
    const double Pitches[] = {.86+.66*Revs,.78+.78*Revs,.79+.48*Revs,.9+.23*Revs};
    const float Blend = DeltaSeconds>0.f ? 1.f-FMath::Exp(-DeltaSeconds/.070f) : 1.f;
    for (int Index=0; Index<EngineStems.Num(); ++Index)
    {
        if (auto *Stem=EngineStems[Index].Get())
        {
            Stem->SetVolumeMultiplier(FMath::Lerp(Stem->VolumeMultiplier,float(Gains[Index]),Blend));
            Stem->SetPitchMultiplier(FMath::Lerp(Stem->PitchMultiplier,float(Pitches[Index]),Blend));
        }
    }
    if (WindLoop) WindLoop->SetVolumeMultiplier(.018+.21*FMath::Square(Ride.v/82.)+(Ride.boost ? .035 : 0.));
    if (!Rain)
        return;
    RainTime += DeltaSeconds;
    const FVector Here = GetComponentLocation();
    auto Around = [](double Value, double Centre, double Span) {
        const double T = (Value - Centre + 0.5 * Span) / Span;
        return Centre + Span * (T - FMath::Floor(T) - 0.5);
    };
    for (int I = 0; I < Seeds.Num(); ++I)
    {
        const FVector &Seed = Seeds[I];
        const FVector Position(Around(Seed.X - RainTime * 230, Here.X, 2800),
                               Around(Seed.Y + RainTime * 60, Here.Y, 2800),
                               Around(Seed.Z - RainTime * 1650, Here.Z + 350, 1600));
        Rain->UpdateInstanceTransform(I, FTransform(FRotator(-8, 0, 2), Position, FVector(0.004, 0.004, 0.23)), true,
                                      I + 1 == Seeds.Num(), true);
    }
}

void URideWeather::PlayImpact(double Severity)
{
    if (Impact)
        UGameplayStatics::PlaySound2D(this, Impact, .38f + .44f * float(Severity), 1.13f - .21f * float(Severity));
}