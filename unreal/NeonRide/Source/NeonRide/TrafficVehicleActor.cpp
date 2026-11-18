#include "TrafficVehicleActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATrafficVehicleActor::ATrafficVehicleActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Axle")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Part = [&](const TCHAR* Name)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(Cube.Object);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return Mesh;
    };
    Chassis = Part(TEXT("Body"));
    Cabin = Part(TEXT("Cabin"));
    RearWindow = Part(TEXT("RearPanel"));
    LeftLamp = Part(TEXT("LeftTailLamp"));
    RightLamp = Part(TEXT("RightTailLamp"));
}

void ATrafficVehicleActor::Configure(bool bTruck)
{
    auto SetPart = [](UStaticMeshComponent* Mesh, FVector Location, FVector Dimensions, const FLinearColor& Colour)
    {
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Dimensions);
        if (auto* Material = Mesh->CreateDynamicMaterialInstance(0))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Colour);
        }
    };
    const FLinearColor Dark(0.055f, 0.09f, 0.12f);
    const FLinearColor Tail(1.f, 0.045f, 0.015f);
    if (bTruck)
    {
        SetPart(Chassis, FVector(0, 0, 68), FVector(13, 2.5, 0.95), Dark);
        SetPart(Cabin, FVector(480, 0, 156), FVector(3.4, 2.45, 1.65), FLinearColor(0.35f, 0.42f, 0.49f));
        SetPart(RearWindow, FVector(-180, 0, 190), FVector(9.4, 2.48, 2.65), FLinearColor(0.50f, 0.55f, 0.60f));
    }
    else
    {
        SetPart(Chassis, FVector(0, 0, 55), FVector(4.8, 1.9, 0.7), FLinearColor(0.88f, 0.57f, 0.16f));
        SetPart(Cabin, FVector(10, 0, 107), FVector(2.6, 1.65, 0.65), Dark);
        SetPart(RearWindow, FVector(12, 0, 143), FVector(2.3, 1.66, 0.07), FLinearColor(0.85f, 0.78f, 0.62f));
    }
    const double Rear = bTruck ? -651.0 : -241.0;
    const double Side = bTruck ? 96.0 : 70.0;
    SetPart(LeftLamp, FVector(Rear, -Side, 67), FVector(0.06, 0.34, 0.14), Tail);
    SetPart(RightLamp, FVector(Rear, Side, 67), FVector(0.06, 0.34, 0.14), Tail);
}
