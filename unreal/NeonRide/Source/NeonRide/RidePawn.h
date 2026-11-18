#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RideHandling.h"
#include "RidePawn.generated.h"

class ASkyway;
class UCameraComponent;

UCLASS()
class ARidePawn : public APawn
{
    GENERATED_BODY()

public:
    ARidePawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    const NeonRide::FRideState& GetRide() const { return Ride; }

private:
    UPROPERTY() TObjectPtr<ASkyway> Skyway;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    NeonRide::FRideState Ride;
    double Remainder = 0.0;
    double Throttle = 0.0;
    double Brake = 0.0;
    double Steer = 0.0;
    void SetThrottle(float Value);
    void SetBrake(float Value);
    void SetSteer(float Value);
    void RestartRide();
    void UpdatePose();
};
