#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "NeonRideGameMode.generated.h"

UCLASS()
class ANeonRideHUD : public AHUD
{
    GENERATED_BODY()
    virtual void DrawHUD() override;
};

UCLASS()
class ANeonRideGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ANeonRideGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
