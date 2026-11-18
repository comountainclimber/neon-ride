#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FilmDistrict.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class FJsonObject;
struct FFilmFlight
{
    FName Key;
    double Phase, Rate, Radius, Altitude, Size;
    int32 Instance;
};
UCLASS()
class AFilmDistrict : public AActor
{
    GENERATED_BODY()
public:
    AFilmDistrict();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TMap<FName,TObjectPtr<UInstancedStaticMeshComponent>> ModelLayers;
    TArray<FFilmFlight> Flights;
    FVector2D OrbitOrigin = FVector2D::ZeroVector;
    double OrbitAspect = 1;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Jelly;
    FVector JellyOrigin = FVector::ZeroVector;
    double JellyAmplitude = 0, JellyRate = 0;
    void LoadAirspace(const TSharedPtr<FJsonObject>& Airspace);
    void AddModel(const TCHAR* Key,const FVector& PositionCm,const FRotator& Rotation,double SizeCm);
    void PositionAircraft(double Time);
};
