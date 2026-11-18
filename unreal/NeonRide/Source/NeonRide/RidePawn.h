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
class SRideMenu;
class UNeonRideSave;

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
    bool IsWalking() const { return bWalking; }
    bool CanMount() const;
    const NeonRide::FPoint& GetParkedPosition() const { return ParkedRide.Position; }
    const ASkyway* GetSkyway() const { return Skyway; }
    void ClosePauseMenu();
    void ReturnFromMenu();
    void QuitFromMenu();
    void CaptureLocation(UNeonRideSave& Save) const;
    bool RestoreLocation(const UNeonRideSave& Save);

private:
    UPROPERTY() TObjectPtr<ASkyway> Skyway;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<URideWeather> Weather;
    UPROPERTY() TArray<TObjectPtr<ATrafficVehicleActor>> Vehicles;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Gloves;
    NeonRide::FRideState Ride;
    NeonRide::FRideState ParkedRide;
    NeonRide::FStreetTraffic StreetTraffic;
    bool bMapOpen = false;
    bool bWalking = false;
    float WalkForward = 0.f;
    double Remainder = 0.0;
    double Throttle = 0.0;
    double Brake = 0.0;
    double Steer = 0.0;
    float LookYaw = 0.f, LookPitch = 0.f;
    FVector2D LookStick = FVector2D::ZeroVector;
    TSharedPtr<SRideMenu> Menu;
    bool bSaveEnabled = false;
    void OpenPauseMenu();
    bool SaveLocation() const;
    void LookX(float Delta);
    void LookY(float Delta);
    void LookStickX(float Value) { LookStick.X = Value; }
    void LookStickY(float Value) { LookStick.Y = Value; }
    void LookAhead() { LookYaw = 0.f; LookPitch = 0.f; }
    void SetThrottle(float Value);
    void SetBrake(float Value);
    void SetSteer(float Value);
    void SetWalkForward(float Value) { WalkForward = FMath::Clamp(Value,-1.f,1.f); }
    void ToggleMount();
    void ReattachBike();
    void Walk(double Step);
    void RestartRide();
    void ToggleMap() { bMapOpen = !bMapOpen; }
    void UpdatePose();
    void UpdateTraffic();
};
