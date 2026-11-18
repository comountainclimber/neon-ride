#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Skyway.generated.h"
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
UCLASS()
class ASkyway : public AActor
{
    GENERATED_BODY()
  public:
    ASkyway();
    virtual void BeginPlay() override;
    FTransform FrameAt(double DistanceMetres, double Lane = 0) const;

  private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Asphalt;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Steel;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Edge;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Reflector;
};
