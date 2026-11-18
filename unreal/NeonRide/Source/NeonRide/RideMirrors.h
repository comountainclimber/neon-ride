#pragma once
#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "RideMirrors.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
UCLASS()
class URideMirrors : public USceneCaptureComponent2D
{
    GENERATED_BODY()
  public:
    URideMirrors();
    void Initialize(USceneComponent *Body, UStaticMeshComponent *FilmBike);
    void UpdateView(double Time, bool Crashed, const FTransform &Road, double HeadingError);
    void Invalidate()
    {
        LastCaptureTime = -1.;
    }
    uint32 GetCaptureSerial() const
    {
        return CaptureSerial;
    }
    double GetLastCaptureTime() const
    {
        return LastCaptureTime;
    }

  private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glass;
    double LastCaptureTime = -1.;
    uint32 CaptureSerial = 0;
};
