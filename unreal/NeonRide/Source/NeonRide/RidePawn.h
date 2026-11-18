#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RideHandling.h"
#include "TrafficRun.h"
#include "RidePawn.generated.h"

class ASkyway;
class UCameraComponent;
class ATrafficVehicleActor;
class URideWeather;

UCLASS()
class ARidePawn : public APawn
{
    GENERATED_BODY()

public:
    ARidePawn();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    const NeonRide::FRideState& GetRide() const { return Ride; }
    bool IsMapOpen() const { return bMapOpen; }
    const ASkyway* GetSkyway() const { return Skyway; }

private:
    UPROPERTY() TObjectPtr<ASkyway> Skyway;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<URideWeather> Weather;
    UPROPERTY() TArray<TObjectPtr<ATrafficVehicleActor>> Vehicles;
    NeonRide::FRideState Ride;
    NeonRide::FStreetTraffic StreetTraffic;
    bool bMapOpen = false;
    double Remainder = 0.0;
    double Throttle = 0.0;
    double Brake = 0.0;
    double Steer = 0.0;
    float LookYaw = 0.f, LookPitch = 0.f;
    FVector2D LookStick = FVector2D::ZeroVector;
    void LookX(float Delta);
    void LookY(float Delta);
    void LookStickX(float Value) { LookStick.X = Value; }
    void LookStickY(float Value) { LookStick.Y = Value; }
    void LookAhead() { LookYaw = 0.f; LookPitch = 0.f; }
    void SetThrottle(float Value);
    void SetBrake(float Value);
    void SetSteer(float Value);
    void RestartRide();
    void ToggleMap() { bMapOpen = !bMapOpen; }
    void UpdatePose();
    void UpdateTraffic();
};
