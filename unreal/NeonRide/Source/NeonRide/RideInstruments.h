#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RideInstruments.generated.h"

namespace NeonRide { struct FRideState; struct FStreetTraffic; }
class UMaterialInstanceDynamic;

UCLASS()
class URideInstruments : public UStaticMeshComponent
{
    GENERATED_BODY()
public:
    URideInstruments();
    void StartDisplay();
    void UpdateDisplay(float DeltaSeconds,const NeonRide::FRideState& Ride,const NeonRide::FStreetTraffic& Traffic);
private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Display;
    float TimeUntilSample = 0.f;
};
