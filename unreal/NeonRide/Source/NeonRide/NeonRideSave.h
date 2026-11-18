#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NeonRideSave.generated.h"
UCLASS()
class UNeonRideSave : public USaveGame
{
    GENERATED_BODY()
  public:
    UPROPERTY(SaveGame) int32 Version = 3;
    UPROPERTY(SaveGame) double Distance = 0;
    UPROPERTY(SaveGame) double Lane = 0;
    UPROPERTY(SaveGame) double Heading = 0;
    UPROPERTY(SaveGame) double Charge = 1;
    UPROPERTY(SaveGame) int32 Laps = 0;
    UPROPERTY(SaveGame) double Elapsed = 0.;
    UPROPERTY(SaveGame) int32 ShiftNumber = 1;
    UPROPERTY(SaveGame) int32 ShiftsFinished = 0;
    UPROPERTY(SaveGame) int32 TrackedJob = -1;
    UPROPERTY(SaveGame) int32 Waypoint = -1;
    UPROPERTY(SaveGame) TArray<int32> Panniers;
    UPROPERTY(SaveGame) TArray<int32> ParcelStatus;
    UPROPERTY(SaveGame) TArray<int32> Visits;
    UPROPERTY(SaveGame) TArray<double> PickupTimes;
    UPROPERTY(SaveGame) TArray<int32> OnTime;
};
