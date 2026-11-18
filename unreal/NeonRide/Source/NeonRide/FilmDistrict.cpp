#include "FilmDistrict.h"
#include "FilmArt.h"
#include "Skyway.h"
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
        if (B.Kind != NeonRide::EBuilding::CityBlock) continue;
        const FVector Roof = FVector(B.X, B.Y, NeonRide::FDistrict::TerraceZ + B.Height) * 100;
        if (BlockIndex % 4 == 0) AddModel(TEXT("building_crown"), Roof, FRotator::ZeroRotator, 2300);
        else if (BlockIndex % 3 == 0) AddModel(TEXT("water_tower"), Roof, FRotator::ZeroRotator, 900);
        ++BlockIndex;
    }
    DressLowerStreets();
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
        AddPanel(I % 3 == 1 ? TEXT("M_Horizon_b") : TEXT("M_Horizon_a"),Centre,
            FVector(-Radius.Y,Radius.X,0),232000,95000);
    }
    if (auto* Jelly = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Environment/SM_Hologram.SM_Hologram")))
    {
        const FVector Centre(65800,-57600,3400);
        auto* Hologram = NeonRide::AttachFilmModel(RootComponent,Jelly,FTransform(Centre));
        Hologram->SetCastShadow(false);
        Hologram->SetBoundsScale(1.2f);
    }
    for (const TCHAR* Key : {TEXT("car"),TEXT("car_a"),TEXT("car_b"),TEXT("car_c")})
        for (int I = 0; I < 2; ++I) AddModel(Key, FVector::ZeroVector, FRotator::ZeroRotator, 500);
    PositionAircraft();
}

void AFilmDistrict::DressLowerStreets()
{
    const double Floor = NeonRide::FDistrict::TerraceZ * 100;
    auto* Glow = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Neon.M_Neon"));
    auto* Sphere = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Rod = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto MakeLayer = [&](const TCHAR* Name,UStaticMesh* Mesh,FLinearColor Colour,bool Lit)
    {
        auto* Part = NewObject<UInstancedStaticMeshComponent>(this,FName(Name));
        AddInstanceComponent(Part);
        Part->SetupAttachment(RootComponent);
        Part->SetStaticMesh(Mesh);
        Part->SetCastShadow(false);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->RegisterComponent();
        if (auto* Mat = Part->CreateDynamicMaterialInstance(0,Lit ? Glow : Mesh->GetMaterial(0)))
            Mat->SetVectorParameterValue(TEXT("Color"),Colour);
        return Part;
    };
    auto* Amber = MakeLayer(TEXT("PaperLanterns"),Sphere,FLinearColor(.98f,.155f,.015f),true);
    auto* Coral = MakeLayer(TEXT("CoralLanterns"),Sphere,FLinearColor(.79f,.027f,.085f),true);
    auto* Cables = MakeLayer(TEXT("MarketCables"),Rod,FLinearColor(.045f,.032f,.035f),false);
    auto Cable = [&](FVector A,FVector B,double Width)
    {
        FVector Delta = B-A;
        Cables->AddInstance(FTransform(FRotationMatrix::MakeFromZ(Delta).ToQuat(),(A+B)*.5,
            FVector(Width/100,Width/100,Delta.Size()/100)));
    };
    auto Lamp = [&](FVector Position,FLinearColor Color,float Power,float Reach)
    {
        auto* Light = NewObject<UPointLightComponent>(this);
        AddInstanceComponent(Light);
        Light->SetupAttachment(RootComponent);
        Light->SetRelativeLocation(Position);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetCastShadows(false);
        Light->SetLightColor(Color);
        Light->SetIntensity(Power);
        Light->SetAttenuationRadius(Reach);
        Light->SetSourceRadius(28);
        Light->RegisterComponent();
    };
    int Line = 0;
    for (int X : {160,175,190,205,220})
    {
        FVector Prev = FVector::ZeroVector;
        for (int J=0; J<=10; ++J)
        {
            const double T = J/10.;
            const FVector P(X*100,-30500+2600*T,Floor+660-196*FMath::Sin(PI*T));
            if (J) Cable(Prev,P,3.5);
            else Cable(P-FVector(0,0,410),P,10);
            if (J==10) Cable(P-FVector(0,0,410),P,10);
            Prev=P;
            if (J%2==1)
            {
                Cable(P,P-FVector(0,0,34),2);
                auto* Bulb=(J+Line)%3 ? Amber : Coral;
                Bulb->AddInstance(FTransform(FQuat::Identity,P-FVector(0,0,56),FVector(.46,.46,.66)));
            }
        }
        const FString Glyph = FString::Printf(TEXT("M_Glyph%d"),1+Line%6);
        AddPanel(*Glyph,FVector(X*100,-30260,Floor+182),FVector::XAxisVector,518,90);
        AddPanel(*Glyph,FVector(X*100,-28140,Floor+180),-FVector::XAxisVector,450,85);
        Lamp(FVector(X*100,-29200,Floor+415),FLinearColor(1.f,.32f,.085f),10000,1500);
        ++Line;
    }
    for (int Y : {-320,-365})
    {
        AddModel(TEXT("cab"),FVector(-8550,Y*100,Floor),FRotator(0,90,0),460);
        AddModel(TEXT("lamp_a"),FVector(-4050,Y*100,Floor),FRotator(0,180,0),750);
        AddPanel(TEXT("M_Glyph2"),FVector(-3950,Y*100,Floor+292),FVector::YAxisVector,660,140);
        Lamp(FVector(-4200,Y*100,Floor+450),FLinearColor(.10f,.67f,1.f),16000,2000);
    }
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
    Part->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXY(Across,-FVector::UpVector).ToQuat(),Position,FVector(WidthCm/100,HeightCm/100,1)));
    Part->RegisterComponent();
}

void AFilmDistrict::PositionAircraft()
{
    const TCHAR* Types[] = {TEXT("car"),TEXT("car_a"),TEXT("car_b"),TEXT("car_c")};
    const double Sizes[] = {500,560,900,650};
    for (int Type = 0; Type < 4; ++Type)
    {
        auto* Entry = ModelLayers.Find(FName(Types[Type]));
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
