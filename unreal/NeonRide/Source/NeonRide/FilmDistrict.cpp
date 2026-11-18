#include "FilmDistrict.h"
#include "FilmArt.h"
#include "Skyway.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AFilmDistrict::AFilmDistrict()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FilmScenery")));
    PrimaryActorTick.bCanEverTick = true;
}

void AFilmDistrict::AddModel(const TCHAR* Key, const FVector& PositionCm, const FRotator& Rotation, double SizeCm)
{
    auto* Found = Layers.Find(FName(Key));
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
            Layers.Add(FName(Key), Layer);
        }
        else Layers.Add(FName(Key), nullptr);
        Found = Layers.Find(FName(Key));
    }
    if (auto* Layer = Found->Get())
        Layer->AddInstance(NeonRide::FilmTransform(Layer->GetStaticMesh(), PositionCm, Rotation, SizeCm));
}

void AFilmDistrict::Populate(ASkyway* Skyway)
{
    Road = Skyway;
    const TCHAR* Towers[] = {TEXT("tower_a"), TEXT("tower_b"), TEXT("tower_c")};
    FRandomStream Random(372);
    for (int I = 0; I < 36; ++I)
    {
        const double Angle = I * (2.0 * PI / 36.0);
        const double Radius = Random.FRandRange(820.f, 1500.f);
        const FVector Base(300 + Radius * FMath::Cos(Angle), -310 + Radius * FMath::Sin(Angle), -160);
        AddModel(Towers[I % 3], Base * 100, FRotator(0, Random.FRandRange(-175.f, 175.f), 0), Random.FRandRange(28000.f, 61000.f));
    }
    int BlockIndex = 0;
    for (const auto& B : Road->GetDistrict().Buildings())
    {
        const FVector Roof = FVector(B.X, B.Y, NeonRide::FDistrict::TerraceZ + B.Height) * 100;
        if (BlockIndex % 4 == 0) AddModel(TEXT("building_crown"), Roof, FRotator::ZeroRotator, 2300);
        else if (BlockIndex % 3 == 0) AddModel(TEXT("water_tower"), Roof, FRotator::ZeroRotator, 900);
        ++BlockIndex;
    }
    for (int S = 10; S < 1070; S += S < 372 ? 26 : 16)
    {
        const FTransform Frame = Road->FrameAt(S).GetRelativeTransform(Road->GetActorTransform());
        for (int Side : {-1, 1})
        {
            if (Side < 0 && S > 165 && S < 195) continue;
            const bool GlassSection = S >= 372;
            const FVector Offset(0, Side * (GlassSection ? 340.0 : 595.0), GlassSection ? 0.0 : -90.0);
            AddModel(GlassSection ? TEXT("lamp_a") : TEXT("lamp_b"), Frame.TransformPosition(Offset),
                FRotator(0, Frame.Rotator().Yaw + (GlassSection && Side > 0 ? 180 : 0), 0), GlassSection ? 800 : 705);
        }
    }
    for (int I = 0; I < 16; ++I)
    {
        const FTransform Frame = Road->FrameAt(40 + I * 64).GetRelativeTransform(Road->GetActorTransform());
        const bool Ring = I % 2 == 0;
        AddModel(Ring ? TEXT("sign_a") : TEXT("sign_b"), Frame.TransformPosition(FVector(0, Ring ? -1550 : 680, Ring ? 510 : -80)),
            FRotator(0, Frame.Rotator().Yaw - (Ring ? 22 : 0), 0), Ring ? 900 : 810);
    }
    for (int S : {140,290,440,590,740})
    {
        const FTransform Frame = Road->FrameAt(S).GetRelativeTransform(Road->GetActorTransform());
        AddModel(TEXT("sign_c"), Frame.GetLocation(), FRotator(0,Frame.Rotator().Yaw,0), 1160);
        const FString Glyph = FString::Printf(TEXT("M_Glyph%d"),1+((S-140)/150)%6);
        AddPanel(*Glyph,Frame.TransformPosition(FVector(-32,0,500)),Frame.GetUnitAxis(EAxis::Y),570,145);
    }
    AddPanel(TEXT("M_Glyph6"),FVector(18000,-1310,1030),FVector::YAxisVector,1250,250);
    for (int I = 0; I < 8; ++I)
    {
        const double A = I * PI / 4.0;
        const FVector Radius(FMath::Cos(A),FMath::Sin(A),0);
        const FVector Centre = FVector(30000,-32000,22500) + 220000 * Radius;
        AddPanel(I % 3 == 1 ? TEXT("M_Skyline_b") : TEXT("M_Skyline_a"),Centre,
            FVector(-Radius.Y,Radius.X,0),181500,79000);
    }
    for (const TCHAR* Key : {TEXT("car"),TEXT("car_a"),TEXT("car_b"),TEXT("car_c")})
        for (int I = 0; I < 2; ++I) AddModel(Key, FVector::ZeroVector, FRotator::ZeroRotator, 500);
    PositionAircraft();
}

void AFilmDistrict::AddPanel(const TCHAR* Name, FVector Position, FVector Across, double WidthCm, double HeightCm)
{
    const FString Path = FString::Printf(TEXT("/Game/Environment/%s.%s"),Name,Name);
    auto* Material = LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn);
    if (!Material) return;
    auto* Part = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Part);
    Part->SetupAttachment(RootComponent);
    Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Part->SetMaterial(0,Material);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetCastShadow(false);
    Part->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXY(Across,FVector::UpVector).ToQuat(),Position,FVector(WidthCm/100,HeightCm/100,1)));
    Part->RegisterComponent();
}

void AFilmDistrict::PositionAircraft()
{
    const TCHAR* Types[] = {TEXT("car"),TEXT("car_a"),TEXT("car_b"),TEXT("car_c")};
    const double Sizes[] = {500,560,900,650};
    for (int Type = 0; Type < 4; ++Type)
    {
        auto* Entry = Layers.Find(FName(Types[Type]));
        if (!Entry || !Entry->Get()) continue;
        auto* Layer = Entry->Get();
        for (int I = 0; I < 2; ++I)
        {
            const int Lane = 2 * Type + I;
            const double Angle = Lane * 0.91 + FlightTime * (0.030 + 0.002 * Lane);
            const double Radius = 185.0 + 48.0 * Lane;
            const FVector Position(325 + Radius * FMath::Sin(Angle), -300 + 0.8 * Radius * FMath::Cos(Angle), 9 + 4.8 * Lane);
            const double Heading = FMath::RadiansToDegrees(FMath::Atan2(-0.8 * FMath::Sin(Angle), FMath::Cos(Angle)));
            const FTransform Pose = NeonRide::FilmTransform(Layer->GetStaticMesh(), Position * 100,
                FRotator(0, Heading + (Type == 3 ? -90.0 : 180.0), 0), Sizes[Type]);
            Layer->UpdateInstanceTransform(I, Pose, false, I == 1, true);
        }
    }
}

void AFilmDistrict::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FlightTime += DeltaSeconds;
    PositionAircraft();
}
