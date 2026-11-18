#pragma once

#include "Components/ActorComponent.h"
#include "CircuitPracticals.generated.h"

class FJsonValue;
class USpotLightComponent;

UCLASS()
class UCircuitPracticals : public UActorComponent
{
    GENERATED_BODY()
public:
    UCircuitPracticals();
    static void Populate(AActor& District, const TArray<TSharedPtr<FJsonValue>>& Placements);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    struct FLamp
    {
        FVector Position;
        FRotator Aim;
        FLinearColor Color;
    };
    TArray<FLamp> Lamps;
    TArray<int32> ActiveLampIndices;
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Lights;
};
