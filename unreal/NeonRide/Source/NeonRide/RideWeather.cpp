#include "RideWeather.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"

void URideWeather::StartWeather()
{
    auto Loop = [&](const TCHAR* Name, float Level)
    {
        const FString Asset = FString::Printf(TEXT("/Game/Environment/%s.%s"),Name,Name);
        auto* Sound = LoadObject<USoundWave>(nullptr,*Asset,nullptr,LOAD_NoWarn);
        if (!Sound) return static_cast<UAudioComponent*>(nullptr);
        auto* Audio = NewObject<UAudioComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(Audio);
        Audio->SetupAttachment(this);
        Audio->bAutoActivate = false;
        Audio->SetSound(Sound);
        Audio->SetVolumeMultiplier(Level);
        Audio->RegisterComponent();
        Audio->Play();
        return Audio;
    };
    EngineLoop = Loop(TEXT("A_Engine"),0.12f);
    WindLoop = Loop(TEXT("A_Wind"),0.08f);
    Loop(TEXT("A_Rain"),0.18f);

    auto* Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Glow = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Neon.M_Neon"),nullptr,LOAD_NoWarn);
    if (!Mesh || !Glow) return;
    Rain = NewObject<UInstancedStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Rain);
    Rain->SetupAttachment(this);
    Rain->SetMobility(EComponentMobility::Movable);
    Rain->SetStaticMesh(Mesh);
    Rain->SetCastShadow(false);
    Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Rain->RegisterComponent();
    if (auto* Tint=Rain->CreateDynamicMaterialInstance(0,Glow))
        Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.050f,0.074f,0.10f));
    FRandomStream Stream(1083);
    for (int I = 0; I < 180; ++I)
    {
        Seeds.Emplace(Stream.FRandRange(0.f,2800.f),Stream.FRandRange(0.f,2800.f),Stream.FRandRange(0.f,1600.f));
        Rain->AddInstance(FTransform::Identity);
    }
    UpdateWeather(0.f,0,0);
}

void URideWeather::UpdateWeather(float DeltaSeconds, double Speed, double Throttle, bool bWalking)
{
    if (EngineLoop)
    {
        EngineLoop->SetPitchMultiplier(0.68 + 0.019 * Speed);
        EngineLoop->SetVolumeMultiplier(bWalking ? 0.f : 0.10 + 0.08 * Throttle);
    }
    if (WindLoop) WindLoop->SetVolumeMultiplier(0.018 + 0.15 * FMath::Square(Speed / 56.0));
    if (!Rain) return;
    RainTime += DeltaSeconds;
    const FVector Here = GetComponentLocation();
    auto Around = [](double Value,double Centre,double Span)
    {
        const double T = (Value - Centre + 0.5 * Span) / Span;
        return Centre + Span * (T - FMath::Floor(T) - 0.5);
    };
    for (int I = 0; I < Seeds.Num(); ++I)
    {
        const FVector& Seed = Seeds[I];
        const FVector Position(Around(Seed.X - RainTime * 230, Here.X, 2800),Around(Seed.Y + RainTime * 60, Here.Y, 2800),
            Around(Seed.Z - RainTime * 1650, Here.Z + 350, 1600));
        Rain->UpdateInstanceTransform(I,FTransform(FRotator(-8,0,2),Position,FVector(0.004,0.004,0.23)),
            true,I+1 == Seeds.Num(),true);
    }
}
