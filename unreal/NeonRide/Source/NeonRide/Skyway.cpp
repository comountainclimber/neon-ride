#include "Skyway.h"
#include "FilmDistrict.h"
#include "Engine/World.h"
#include "RideHandling.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASkyway::ASkyway()
{
    Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
    SetRootComponent(Route);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Layer = [&](const TCHAR* Name)
    {
        auto* Part = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName(Name));
        Part->SetupAttachment(Route);
        Part->SetStaticMesh(Cube.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return Part;
    };
    Deck = Layer(TEXT("Deck"));
    Markings = Layer(TEXT("LaneMarkings"));
    Rails = Layer(TEXT("Rails"));
    Supports = Layer(TEXT("Supports"));
    Floor = Layer(TEXT("CityDatum"));
    Buildings = Layer(TEXT("TerraceBlocks"));
    MarketColour = Layer(TEXT("MarketCanopies"));
    TransitColour = Layer(TEXT("TransitAndOverlook"));
    OpenLights = Layer(TEXT("BlueLaneLights"));
    GlassLights = Layer(TEXT("TealLaneLights"));
    GlassWalls = Layer(TEXT("GlassWalls"));
    GlassWalls->SetCastShadow(false);
    Deck->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

FTransform ASkyway::FrameAt(double DistanceMetres) const
{
    const float DistanceCm = FMath::Clamp(DistanceMetres, 0.0, NeonRide::TrackLength) * 100.0;
    FTransform Frame = Route->GetTransformAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World, false);
    const double Bank = -5.0 * NeonRide::Curvature(DistanceMetres) * 340.0;
    Frame.SetRotation(Frame.GetRotation() * FRotator(0.0, 0.0, Bank).Quaternion());
    return Frame;
}

void ASkyway::BeginPlay()
{
    Super::BeginPlay();
    auto* Art = GetWorld()->SpawnActor<AFilmDistrict>(GetActorLocation(), GetActorRotation());
    if (Art) Art->Populate(this);
}

void ASkyway::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Route->ClearSplinePoints(false);
    FVector Point = FVector::ZeroVector;
    double Heading = 0.0;
    constexpr int Step = 5;
    for (int S = 0; S <= int(NeonRide::TrackLength); S += Step)
    {
        Route->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        Route->SetSplinePointType(S / Step, ESplinePointType::CurveClamped, false);
        const double DeltaHeading = -Step * NeonRide::Curvature(S + 0.5 * Step);
        const double Middle = Heading + 0.5 * DeltaHeading;
        Point += FVector(FMath::Cos(Middle), FMath::Sin(Middle), 0.0) * (100.0 * Step);
        Heading += DeltaHeading;
    }
    Route->UpdateSpline();

    auto Prepare = [](UInstancedStaticMeshComponent* Layer, const FLinearColor& Colour)
    {
        Layer->ClearInstances();
        if (auto* Material = Layer->CreateDynamicMaterialInstance(0, Layer->GetStaticMesh()->GetMaterial(0)))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Colour);
        }
    };
    Prepare(Deck, FLinearColor(0.12f, 0.14f, 0.17f));
    Prepare(Markings, FLinearColor(0.9f, 0.91f, 0.92f));
    Prepare(Rails, FLinearColor(0.48f, 0.53f, 0.58f));
    Prepare(Supports, FLinearColor(0.30f, 0.36f, 0.42f));
    Prepare(Floor, FLinearColor(0.17f, 0.20f, 0.24f));
    Prepare(Buildings, FLinearColor(0.24f, 0.31f, 0.40f));
    Prepare(MarketColour, FLinearColor(0.94f, 0.38f, 0.10f));
    Prepare(TransitColour, FLinearColor(0.08f, 0.61f, 0.65f));
    Prepare(OpenLights, FLinearColor(0.2f,0.47f,1.f));
    Prepare(GlassLights, FLinearColor(0.06f,0.85f,0.69f));
    GlassWalls->ClearInstances();
    auto EnvironmentMaterial = [](const TCHAR* Key)
    {
        const FString Path = FString::Printf(TEXT("/Game/Environment/%s.%s"),Key,Key);
        return LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn);
    };
    if (auto* Wet = EnvironmentMaterial(TEXT("M_WetDeck"))) Deck->SetMaterial(0,Wet);
    if (auto* Facade = EnvironmentMaterial(TEXT("M_Facade"))) Buildings->SetMaterial(0,Facade);
    auto* Glass = EnvironmentMaterial(TEXT("M_Glass"));
    if (Glass) GlassWalls->SetMaterial(0,Glass);
    if (auto* Neon = EnvironmentMaterial(TEXT("M_Neon")))
    {
        auto Tint = [&](UInstancedStaticMeshComponent* Layer,FLinearColor C)
        {
            if (auto* Mat=Layer->CreateDynamicMaterialInstance(0,Neon))
                Mat->SetVectorParameterValue(TEXT("Color"),C);
        };
        Tint(OpenLights,FLinearColor(0.19f,0.43f,1.f));
        Tint(GlassLights,FLinearColor(0.035f,0.88f,0.64f));
        Tint(MarketColour,FLinearColor(1.f,0.32f,0.065f));
        Tint(TransitColour,FLinearColor(0.045f,0.50f,0.62f));
    }

    // Cube mesh is one metre across. Transforms below use metre dimensions and cm positions.
    auto Box = [](UInstancedStaticMeshComponent* Layer, const FTransform& Frame, FVector OffsetCm, FVector SizeM)
    {
        Layer->AddInstance(FTransform(Frame.GetRotation(), Frame.TransformPosition(OffsetCm), SizeM), true);
    };
    for (int S = 0; S < int(NeonRide::TrackLength); S += Step)
    {
        const FTransform Frame = FrameAt(S + Step * 0.5);
        Box(Deck, Frame, FVector(0, 0, -22), FVector(5.10, 10, 0.44));
        if ((S / Step) % 2 == 0)
        {
            Box(Markings, Frame, FVector(0, -40, 1), FVector(3.0, 0.10, 0.02));
        }
        for (int Side : {-1, 1})
        {
            Box(Markings, Frame, FVector(0, Side * 462, 1), FVector(5.03, 0.09, 0.02));
            if (!(Side == -1 && S >= 165 && S < 195) && S < 1070)
            {
                Box(Rails, Frame, FVector(0, Side * 495, 85), FVector(5.10, 0.10, 0.10));
                Box(S < 372 ? OpenLights.Get() : GlassLights.Get(), Frame, FVector(0,Side*488,89),FVector(5.08,0.065,0.055));
                if (Glass && S >= 372)
                    Box(GlassWalls,Frame,FVector(0,Side*502,230),FVector(5.03,0.04,4.6));
            }
        }
    }
    for (int S = 0; S < int(NeonRide::TrackLength); S += S < 372 ? 26 : 16)
    {
        const FTransform Frame = FrameAt(S);
        const double Height = S < 372 ? 1.1 : 4.2;
        for (int Side : {-1, 1})
        {
            if ((Side == -1 && S > 165 && S < 195) || S > 1070) continue;
            Box(Supports, Frame, FVector(0, Side * 495, Height * 50), FVector(0.18, 0.18, Height));
        }
    }
    Box(Floor, FrameAt(0), FVector(0, 0, -16050), FVector(3500, 3500, 1));
    auto CityBox = [&](UInstancedStaticMeshComponent* Layer, double X, double Y, double Z, FVector Size)
    {
        Box(Layer, GetActorTransform(), FVector(X, Y, Z) * 100.0, Size);
    };
    for (size_t I = size_t(NeonRide::TrackLength / Step); I < District.Surfaces().size(); ++I)
    {
        const auto& S = District.Surfaces()[I];
        const FVector A(S.A.X, S.A.Y, S.A.Z), B(S.B.X, S.B.Y, S.B.Z);
        const FVector Delta = B - A;
        FTransform Frame(Delta.Rotation(), (A + B) * 50.0);
        Frame = Frame * GetActorTransform();
        Box(Deck, Frame, FVector(0, 0, -25), FVector(Delta.Size() + 0.1, S.Width, 0.50));
        if (!S.bRoad) continue;
        for (int Side : {-1, 1})
            Box(Markings, Frame, FVector(0, Side * (S.Width * 50 - 35), 1.5), FVector(Delta.Size(), 0.18, 0.03));
    }
    for (const auto& B : District.Buildings())
    {
        const double Base = NeonRide::FDistrict::TerraceZ;
        CityBox(Buildings, B.X, B.Y, Base + B.Height / 2, FVector(B.WidthX, B.WidthY, B.Height));
        auto* Trim = B.X < 320 && B.X > 55 ? MarketColour.Get() : TransitColour.Get();
        CityBox(Trim, B.X, B.Y, Base + B.Height - 1.0, FVector(B.WidthX + 0.6, B.WidthY + 0.6, 0.70));
        if (B.X > 55 && B.X < 320)
        {
            CityBox(Trim, B.X, B.Y + B.WidthY / 2 + 0.8, Base + 4, FVector(B.WidthX, 2.4, 0.4));
        }
    }
    // Silhouettes identify the three junctions from across the district.
    for (int Side : {-1, 1})
    {
        CityBox(MarketColour, 180 + Side * 10, -13, 5.0, FVector(0.7, 0.8, 10));
        CityBox(TransitColour, -65 + Side * 10, -12, 6.0, FVector(0.8, 0.8, 12));
        CityBox(TransitColour, 637, -548 + Side * 14, 13.0, FVector(2.0, 2.0, 26));
    }
    CityBox(MarketColour, 180, -13, 10, FVector(20.7, 0.8, 0.6));
    CityBox(TransitColour, -65, -12, 12, FVector(20.8, 0.8, 0.6));
    CityBox(TransitColour, 637, -548, 26, FVector(2.0, 30, 1));
    for (int X = -120; X < 735; X += 12)
        CityBox(Markings, X, -556, NeonRide::FDistrict::TerraceZ + 0.02, FVector(4, 0.16, 0.04));
}
