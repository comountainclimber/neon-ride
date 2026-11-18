#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
    virtual void OnConstruction(const FTransform& Transform) override;
    FTransform FrameAt(double DistanceMetres) const;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USplineComponent> Route;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Deck;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Markings;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Rails;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Supports;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Floor;
};
