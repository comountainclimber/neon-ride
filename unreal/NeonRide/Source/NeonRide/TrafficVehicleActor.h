#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficVehicleActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ATrafficVehicleActor : public AActor
{
    GENERATED_BODY()
public:
    ATrafficVehicleActor();
    void Configure(bool bTruck);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Chassis;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Cabin;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RearWindow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftLamp;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightLamp;
};
