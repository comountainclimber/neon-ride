#include "RideInstruments.h"
#include "CircuitCore.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

URideInstruments::URideInstruments()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    SetStaticMesh(Plane.Object);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    const auto Orientation = FRotationMatrix::MakeFromXY(FVector::YAxisVector, FVector(-.574, 0, -.819)).ToQuat();
    SetRelativeTransform(FTransform(Orientation, FVector(78, 0, 109.05), FVector(.2432, .08064, 1)));
}

void URideInstruments::StartDisplay()
{
    if (auto *Material =
            LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_CircuitDisplay.M_CircuitDisplay")))
        Display = CreateDynamicMaterialInstance(0, Material);
    SetVisibility(Display != nullptr);
}

void URideInstruments::UpdateDisplay(const Circuit::Ride &Ride)
{
    if (!Display)
        return;
    Display->SetVectorParameterValue(TEXT("Telemetry"), FLinearColor(Ride.v * 3.6 / 300., Ride.rpm, Ride.gear / 6., 1));
    Display->SetVectorParameterValue(TEXT("Nitro"), FLinearColor(Ride.charge, Ride.boost, Ride.locked, 1));
}