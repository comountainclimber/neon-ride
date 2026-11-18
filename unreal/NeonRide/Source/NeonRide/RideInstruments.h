#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RideInstruments.generated.h"
namespace Circuit
{
struct Ride;
}
class UMaterialInstanceDynamic;
UCLASS()
class URideInstruments : public UStaticMeshComponent
{
    GENERATED_BODY()
  public:
    URideInstruments();
    void StartDisplay();
    void UpdateDisplay(const Circuit::Ride &Ride);

  private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Display;
};
