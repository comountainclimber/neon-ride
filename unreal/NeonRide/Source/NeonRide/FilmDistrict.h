#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FilmDistrict.generated.h"

class ASkyway;
class UInstancedStaticMeshComponent;

UCLASS()
class AFilmDistrict : public AActor
{
    GENERATED_BODY()
public:
    AFilmDistrict();
    void Populate(ASkyway* Skyway);
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TObjectPtr<ASkyway> Road;
    UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Layers;
    double FlightTime = 0.0;
    void AddModel(const TCHAR* Key, const FVector& PositionCm, const FRotator& Rotation, double SizeCm);
    void PositionAircraft();
    void AddPanel(const TCHAR* MaterialName, FVector Position, FVector Across, double WidthCm, double HeightCm);
};
