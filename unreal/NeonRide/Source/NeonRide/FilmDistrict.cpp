#include "FilmDistrict.h"
#include "FilmArt.h"
#include "CircuitPracticals.h"
#include "RidePawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AFilmDistrict::AFilmDistrict()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FilmScenery")));
    PrimaryActorTick.bCanEverTick = true;
}

void AFilmDistrict::AddModel(const TCHAR* Key, const FVector& PositionCm, const FRotator& Rotation, double SizeCm)
{
    auto* Found = ModelLayers.Find(FName(Key));
    if (!Found)
    {
        if (auto* Mesh = NeonRide::LoadFilmModel(Key))
        {
            auto* Layer = NewObject<UInstancedStaticMeshComponent>(this, FName(Key));
            AddInstanceComponent(Layer);
            Layer->SetupAttachment(RootComponent);
            Layer->SetMobility(EComponentMobility::Movable);
            Layer->SetStaticMesh(Mesh);
            Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Layer->RegisterComponent();
            ModelLayers.Add(FName(Key), Layer);
        }
        else ModelLayers.Add(FName(Key), nullptr);
        Found = ModelLayers.Find(FName(Key));
    }
    if (auto* Layer = Found->Get())
        Layer->AddInstance(NeonRide::FilmTransform(Layer->GetStaticMesh(), PositionCm, Rotation, SizeCm));
}

void AFilmDistrict::BeginPlay()
{
    Super::BeginPlay();
    FString Json;
    TSharedPtr<FJsonObject> Scene;
    const FString File = FPaths::ProjectContentDir() / TEXT("Circuit/scenery.json");
    if (FFileHelper::LoadFileToString(Json,*File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Scene) && Scene.IsValid())
    {
        for (const auto& Value : Scene->GetArrayField(TEXT("items")))
        {
            const auto& P=Value->AsArray();
            if (P.Num()!=6) continue;
            const FString Key=P[0]->AsString().Replace(TEXT("-"),TEXT("_"));
            AddModel(*Key,FVector(P[1]->AsNumber(),P[2]->AsNumber(),P[3]->AsNumber())*100.,
                FRotator(0,FMath::RadiansToDegrees(P[5]->AsNumber()),0),P[4]->AsNumber()*100.);
        }
        UCircuitPracticals::Populate(*this, Scene->GetArrayField(TEXT("items")));
        LoadAirspace(Scene->GetObjectField(TEXT("airspace")));
    }
    else UE_LOG(LogTemp,Error,TEXT("Unable to load the circuit placements: %s"),*File);
    PositionAircraft(0);
}

void AFilmDistrict::LoadAirspace(const TSharedPtr<FJsonObject>& Airspace)
{
    if (!Airspace) return;
    const auto& Origin = Airspace->GetArrayField(TEXT("origin"));
    OrbitOrigin = FVector2D(Origin[0]->AsNumber(), Origin[1]->AsNumber());
    OrbitAspect = Airspace->GetNumberField(TEXT("aspect"));
    for (const auto& Entry : Airspace->GetArrayField(TEXT("aircraft")))
    {
        const auto& Data = Entry->AsObject();
        const FString Key = Data->GetStringField(TEXT("model")).Replace(TEXT("-"), TEXT("_"));
        const double Size = Data->GetNumberField(TEXT("size"));
        AddModel(*Key, FVector::ZeroVector, FRotator::ZeroRotator, Size * 100.);
        auto* Layer = ModelLayers.FindRef(FName(*Key)).Get();
        if (Layer)
            Flights.Add({FName(*Key), Data->GetNumberField(TEXT("phase")), Data->GetNumberField(TEXT("rate")),
                Data->GetNumberField(TEXT("radius")), Data->GetNumberField(TEXT("altitude")), Size,
                Layer->GetInstanceCount() - 1});
    }
    const auto& Landmark = Airspace->GetObjectField(TEXT("jelly"));
    if (auto* Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Environment/SM_Hologram.SM_Hologram")))
    {
        const auto& Centre = Landmark->GetArrayField(TEXT("position"));
        JellyOrigin = FVector(Centre[0]->AsNumber(), Centre[1]->AsNumber(), Centre[2]->AsNumber()) * 100.;
        JellyAmplitude = Landmark->GetNumberField(TEXT("amplitude")) * 100.;
        JellyRate = Landmark->GetNumberField(TEXT("rate"));
        const double Scale = Landmark->GetNumberField(TEXT("size")) * 100. /
            FMath::Max(1., 2. * Mesh->GetBounds().BoxExtent.GetMax());
        Jelly = NeonRide::AttachFilmModel(RootComponent, Mesh,
            FTransform(FQuat::Identity, JellyOrigin, FVector(Scale)));
        Jelly->SetCastShadow(false);
    }
}

void AFilmDistrict::PositionAircraft(double Time)
{
    for (const auto& Flight : Flights)
    {
        auto* Layer = ModelLayers.FindRef(Flight.Key).Get();
        const double Angle = Flight.Phase + Time * Flight.Rate;
        const double Cos = FMath::Cos(Angle), Sin = FMath::Sin(Angle);
        const FVector P(OrbitOrigin.X + Flight.Radius * Cos,
            OrbitOrigin.Y + Flight.Radius * OrbitAspect * Sin, Flight.Altitude);
        const double Heading = FMath::RadiansToDegrees(FMath::Atan2(OrbitAspect * Cos, -Sin));
        const FTransform Pose = NeonRide::FilmTransform(Layer->GetStaticMesh(), P * 100.,
            FRotator(0, Heading + 180., 0), Flight.Size * 100.);
        Layer->UpdateInstanceTransform(Flight.Instance, Pose, false,
            Flight.Instance == Layer->GetInstanceCount() - 1, true);
    }
    if (Jelly)
        Jelly->SetRelativeLocation(JellyOrigin + FVector(0, 0, JellyAmplitude * FMath::Sin(Time * JellyRate)));
}

void AFilmDistrict::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* Player = GetWorld()->GetFirstPlayerController();
    const auto* Rider = Player ? Cast<ARidePawn>(Player->GetPawn()) : nullptr;
    PositionAircraft(Rider ? Rider->GetRide().elapsed : 0.);
}
