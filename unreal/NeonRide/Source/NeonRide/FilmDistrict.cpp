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

void AFilmDistrict::AddModel(const TCHAR *Key, const FVector &PositionCm, const FRotator &Rotation, double SizeCm)
{
    auto *Found = ModelLayers.Find(FName(Key));
    if (!Found)
    {
        if (auto *Mesh = NeonRide::LoadFilmModel(Key))
        {
            auto *Layer = NewObject<UInstancedStaticMeshComponent>(this, FName(Key));
            AddInstanceComponent(Layer);
            Layer->SetupAttachment(RootComponent);
            Layer->SetMobility(EComponentMobility::Movable);
            Layer->SetStaticMesh(Mesh);
            Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Layer->RegisterComponent();
            ModelLayers.Add(FName(Key), Layer);
        }
        else
            ModelLayers.Add(FName(Key), nullptr);
        Found = ModelLayers.Find(FName(Key));
    }
    if (auto *Layer = Found->Get())
        Layer->AddInstance(NeonRide::FilmTransform(Layer->GetStaticMesh(), PositionCm, Rotation, SizeCm));
}

void AFilmDistrict::BeginPlay()
{
    Super::BeginPlay();
    FString Json;
    TSharedPtr<FJsonObject> Scene;
    const FString File = FPaths::ProjectContentDir() / TEXT("Circuit/scenery.json");
    if (FFileHelper::LoadFileToString(Json, *File) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Scene) && Scene.IsValid())
    {
        for (const auto &Value : Scene->GetArrayField(TEXT("items")))
        {
            const auto &P = Value->AsArray();
            if (P.Num() != 6)
                continue;
            const FString Key = P[0]->AsString().Replace(TEXT("-"), TEXT("_"));
            AddModel(*Key, FVector(P[1]->AsNumber(), P[2]->AsNumber(), P[3]->AsNumber()) * 100.,
                     FRotator(0, FMath::RadiansToDegrees(P[5]->AsNumber()), 0), P[4]->AsNumber() * 100.);
        }
        UCircuitPracticals::Populate(*this, Scene->GetArrayField(TEXT("items")));
        const TArray<TSharedPtr<FJsonValue>> *Infill = nullptr;
        if (Scene->TryGetArrayField(TEXT("infill"), Infill))
            PopulateMidrises(*Infill);
        LoadAirspace(Scene->GetObjectField(TEXT("airspace")));
    }
    else
        UE_LOG(LogTemp, Error, TEXT("Unable to load the circuit placements: %s"), *File);
    PositionAircraft(0);
}

void AFilmDistrict::LoadAirspace(const TSharedPtr<FJsonObject> &Airspace)
{
    if (!Airspace)
        return;
    const auto &Origin = Airspace->GetArrayField(TEXT("origin"));
    OrbitOrigin = FVector2D(Origin[0]->AsNumber(), Origin[1]->AsNumber());
    OrbitAspect = Airspace->GetNumberField(TEXT("aspect"));
    for (const auto &Entry : Airspace->GetArrayField(TEXT("aircraft")))
    {
        const auto &Data = Entry->AsObject();
        const FString Key = Data->GetStringField(TEXT("model")).Replace(TEXT("-"), TEXT("_"));
        const double Size = Data->GetNumberField(TEXT("size"));
        AddModel(*Key, FVector::ZeroVector, FRotator::ZeroRotator, Size * 100.);
        auto *Layer = ModelLayers.FindRef(FName(*Key)).Get();
        if (Layer)
            Flights.Add({FName(*Key), Data->GetNumberField(TEXT("phase")), Data->GetNumberField(TEXT("rate")),
                         Data->GetNumberField(TEXT("radius")), Data->GetNumberField(TEXT("altitude")), Size,
                         Layer->GetInstanceCount() - 1});
    }
    const auto &Landmark = Airspace->GetObjectField(TEXT("jelly"));
    if (auto *Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/SM_Hologram.SM_Hologram")))
    {
        const auto &Centre = Landmark->GetArrayField(TEXT("position"));
        JellyOrigin = FVector(Centre[0]->AsNumber(), Centre[1]->AsNumber(), Centre[2]->AsNumber()) * 100.;
        JellyAmplitude = Landmark->GetNumberField(TEXT("amplitude")) * 100.;
        JellyRate = Landmark->GetNumberField(TEXT("rate"));
        const double Scale =
            Landmark->GetNumberField(TEXT("size")) * 100. / FMath::Max(1., 2. * Mesh->GetBounds().BoxExtent.GetMax());
        Jelly =
            NeonRide::AttachFilmModel(RootComponent, Mesh, FTransform(FQuat::Identity, JellyOrigin, FVector(Scale)));
        Jelly->SetCastShadow(false);
    }
}

void AFilmDistrict::PositionAircraft(double Time)
{
    for (const auto &Flight : Flights)
    {
        auto *Layer = ModelLayers.FindRef(Flight.Key).Get();
        const double Angle = Flight.Phase + Time * Flight.Rate;
        const double Cos = FMath::Cos(Angle), Sin = FMath::Sin(Angle);
        const FVector P(OrbitOrigin.X + Flight.Radius * Cos, OrbitOrigin.Y + Flight.Radius * OrbitAspect * Sin,
                        Flight.Altitude);
        const double Heading = FMath::RadiansToDegrees(FMath::Atan2(OrbitAspect * Cos, -Sin));
        const FTransform Pose = NeonRide::FilmTransform(Layer->GetStaticMesh(), P * 100.,
                                                        FRotator(0, Heading + 180., 0), Flight.Size * 100.);
        Layer->UpdateInstanceTransform(Flight.Instance, Pose, false, Flight.Instance == Layer->GetInstanceCount() - 1,
                                       true);
    }
    if (Jelly)
        Jelly->SetRelativeLocation(JellyOrigin + FVector(0, 0, JellyAmplitude * FMath::Sin(Time * JellyRate)));
}

void AFilmDistrict::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto *Player = GetWorld()->GetFirstPlayerController();
    const auto *Rider = Player ? Cast<ARidePawn>(Player->GetPawn()) : nullptr;
    PositionAircraft(Rider ? Rider->GetRide().elapsed : 0.);
}

void AFilmDistrict::PopulateMidrises(const TArray<TSharedPtr<FJsonValue>> &Rows)
{
    auto *Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto *Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_CircuitInfill.M_CircuitInfill"));
    auto *Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_Neon.M_Neon"));
    if (!Cube || !Glass || !Glow)
        return;
    auto Batch = [&](FName Name, UMaterialInterface *Material) {
        auto *Layer = NewObject<UInstancedStaticMeshComponent>(this, Name);
        AddInstanceComponent(Layer);
        Layer->SetupAttachment(RootComponent);
        Layer->SetStaticMesh(Cube);
        Layer->SetMaterial(0, Material);
        Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Layer->SetCanEverAffectNavigation(false);
        Layer->RegisterComponent();
        return Layer;
    };
    auto *Walls = Batch(TEXT("GraphiteMidrises"), Glass);
    auto *Roof = Batch(TEXT("MechanicalPenthouses"), Cube->GetMaterial(0));
    auto *Beacons = Batch(TEXT("AviationLights"), Glow);
    if (auto *Material = Roof->CreateDynamicMaterialInstance(0))
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(.077f, .106f, .126f));
    if (auto *Material = Beacons->CreateDynamicMaterialInstance(0))
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(.81f, .10f, .048f));
    Beacons->SetCastShadow(false);
    for (const auto &Entry : Rows)
    {
        const auto &P = Entry->AsArray();
        if (P.Num() != 9)
            continue;
        const FVector Base(P[0]->AsNumber() * 100., P[1]->AsNumber() * 100., P[2]->AsNumber() * 100.);
        const double Width = P[3]->AsNumber(), Depth = P[4]->AsNumber(), Height = P[5]->AsNumber();
        const FRotator Yaw(0, FMath::RadiansToDegrees(P[6]->AsNumber()), 0);
        const int Seed = int(P[7]->AsNumber()), RoofType = int(P[8]->AsNumber());
        auto Place = [&](UInstancedStaticMeshComponent *Layer, FVector Center, FVector Size) {
            Layer->AddInstance(FTransform(Yaw, Base + Yaw.RotateVector(Center) * 100., Size));
        };
        Place(Walls, FVector(0, 0, Height * .5), FVector(Width, Depth, Height));
        Place(Roof, FVector(0, 0, Height + .24), FVector(Width * 1.015, Depth * 1.015, .48));
        if (RoofType)
        {
            const double Tower = 1.9 + .9 * RoofType;
            Place(Roof, FVector(Width * .08, -Depth * .06, Height + Tower * .5),
                  FVector(Width * .61, Depth * .53, Tower));
            Place(Roof, FVector(Width * .06, -Depth * .03, Height + Tower + .14),
                  FVector(Width * .62, Depth * .55, .23));
        }
        if (Seed % 3 != 0)
            Place(Beacons, FVector(Width * .45, Depth * .45, Height + .64), FVector(.32, .32, .34));
    }
    UE_LOG(LogTemp, Display, TEXT("Circuit midrise skyline: %d placed"), Walls->GetInstanceCount());
}
