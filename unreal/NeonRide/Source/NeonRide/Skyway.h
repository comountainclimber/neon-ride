#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "District.h"
#include "Skyway.generated.h"

class UInstancedStaticMeshComponent;
class USplineComponent;
class UStaticMesh;

UCLASS()
class ASkyway : public AActor
{
    GENERATED_BODY()

public:
    ASkyway();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    FTransform FrameAt(double DistanceMetres) const;
    const NeonRide::FDistrict& GetDistrict() const { return District; }

private:
    NeonRide::FDistrict District;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USplineComponent> Route;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Deck;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Markings;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Rails;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Supports;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Floor;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Buildings;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> MarketColour;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> TransitColour;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> OpenLights;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GlassLights;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> GlassWalls;
};
