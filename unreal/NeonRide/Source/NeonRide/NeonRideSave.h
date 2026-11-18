#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NeonRideSave.generated.h"
UCLASS()
class UNeonRideSave : public USaveGame
{
    GENERATED_BODY()
  public:
    UPROPERTY(SaveGame) int32 Version = 2;
    UPROPERTY(SaveGame) double Distance = 0;
    UPROPERTY(SaveGame) double Lane = 0;
    UPROPERTY(SaveGame) double Heading = 0;
    UPROPERTY(SaveGame) double Charge = 1;
    UPROPERTY(SaveGame) int32 Laps = 0;
};
