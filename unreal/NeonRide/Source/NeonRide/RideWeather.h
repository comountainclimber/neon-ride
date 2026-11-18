#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RideWeather.generated.h"

class UAudioComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class URideWeather : public USceneComponent
{
    GENERATED_BODY()
public:
    void StartWeather();
    void UpdateWeather(float DeltaSeconds, double Speed, double Throttle, bool bWalking=false);

private:
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rain;
    UPROPERTY() TObjectPtr<UAudioComponent> EngineLoop;
    UPROPERTY() TObjectPtr<UAudioComponent> WindLoop;
    TArray<FVector> Seeds;
    double RainTime = 0.0;
};
