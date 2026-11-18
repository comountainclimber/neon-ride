#include "Skyway.h"
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
            Box(Rails, Frame, FVector(0, Side * 495, 85), FVector(5.10, 0.10, 0.10));
        }
    }
    for (int S = 0; S < int(NeonRide::TrackLength); S += S < 372 ? 26 : 16)
    {
        const FTransform Frame = FrameAt(S);
        const double Height = S < 372 ? 1.1 : 4.2;
        for (int Side : {-1, 1})
        {
            Box(Supports, Frame, FVector(0, Side * 495, Height * 50), FVector(0.18, 0.18, Height));
        }
    }
    Box(Floor, FrameAt(0), FVector(0, 0, -16050), FVector(3500, 3500, 1));
}
