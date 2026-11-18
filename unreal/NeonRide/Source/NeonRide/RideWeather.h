#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RideWeather.generated.h"

class UAudioComponent;
class USoundWave;
class UInstancedStaticMeshComponent;
namespace Circuit { struct Ride; }

UCLASS()
class URideWeather : public USceneComponent
{
    GENERATED_BODY()
public:
    void StartWeather();
    void SetAudioPaused(bool Paused);
    void PlayImpact(double Severity);
    void UpdateWeather(float DeltaSeconds,const Circuit::Ride &Ride,double Throttle,double Brake);
private:
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rain;
    UPROPERTY() TObjectPtr<USoundWave> Impact;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> EngineStems;
    UPROPERTY() TObjectPtr<UAudioComponent> WindLoop;
    UPROPERTY() TObjectPtr<UAudioComponent> RainLoop;
    TArray<FVector> Seeds;
    double RainTime=0.;
};
