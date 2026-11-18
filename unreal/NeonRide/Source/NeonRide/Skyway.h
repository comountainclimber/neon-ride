#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Skyway.generated.h"
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UTextRenderComponent;
UCLASS()
class ASkyway : public AActor
{
    GENERATED_BODY()
  public:
    ASkyway();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Acknowledge(int Address);
    int StaffCount() const;
    UPROPERTY() TObjectPtr<UMaterialInterface> SignMaterial;
    UPROPERTY() TObjectPtr<UTextRenderComponent> RampSign;
    UPROPERTY() TObjectPtr<UTextRenderComponent> DawnMarquee;
    FTransform FrameAt(double DistanceMetres, double Lane = 0) const;

  private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Asphalt;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Steel;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Edge;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Reflector;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Clients;
    TArray<FTransform> ClientBases;
    int Greeting = -1;
    double GreetingTime = -2.;
    bool bRampOpen = false;
    bool bDawnScreening = false;
};
