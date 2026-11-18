#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NeonRideSave.generated.h"

UCLASS()
class UNeonRideSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) FVector BikePosition = FVector::ZeroVector;
    UPROPERTY(SaveGame) double BikeHeading = 0;
    UPROPERTY(SaveGame) bool bOnFoot = false;
    UPROPERTY(SaveGame) FVector FootPosition = FVector::ZeroVector;
    UPROPERTY(SaveGame) double FootHeading = 0;
};
