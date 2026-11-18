#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CircuitCore.h"
#include "RidePawn.generated.h"

class ASkyway;
class UCameraComponent;
class ATrafficVehicleActor;
class URideWeather;
class URideInstruments;
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
    virtual void SetupPlayerInputComponent(UInputComponent *Input) override;
    const Circuit::Ride &GetRide() const
    {
        return Ride;
    }
    void ClosePauseMenu();
    void ReturnFromMenu();
    void QuitFromMenu();
    void CaptureLocation(UNeonRideSave &Save) const;
    bool RestoreLocation(const UNeonRideSave &Save);
#if WITH_DEV_AUTOMATION_TESTS
    void PreviewState(const Circuit::Ride &State)
    {
        Ride = State;
        UpdatePose();
        UpdateTraffic();
        InstrumentsUpdate();
    }
#endif
  private:
    UPROPERTY() TObjectPtr<ASkyway> Skyway;
    UPROPERTY() TObjectPtr<USceneComponent> Body;
    UPROPERTY() TObjectPtr<USceneComponent> Ejected;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> CrashMesh;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<URideWeather> Weather;
    UPROPERTY() TObjectPtr<URideInstruments> Instruments;
    UPROPERTY() TArray<TObjectPtr<ATrafficVehicleActor>> Vehicles;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Gloves;
    Circuit::Ride Ride = Circuit::Start();
    bool bBoost = false, bSaveEnabled = false;
    double Remainder = 0, Throttle = 0, Brake = 0, Steer = 0;
    float LookYaw = 0, LookPitch = 0;
    FVector2D LookStick = FVector2D::ZeroVector;
    TSharedPtr<SRideMenu> Menu;
    void SetThrottle(float Value)
    {
        Throttle = FMath::Clamp(Value, 0.f, 1.f);
    }
    void SetBrake(float Value)
    {
        Brake = FMath::Clamp(Value, 0.f, 1.f);
    }
    void SetSteer(float Value)
    {
        Steer = FMath::Clamp(Value, -1.f, 1.f);
    }
    void BoostPressed()
    {
        bBoost = true;
    }
    void BoostReleased()
    {
        bBoost = false;
    }
    void LookX(float Value)
    {
        LookYaw = FMath::Clamp(LookYaw + 1.5f * Value, -140.f, 140.f);
    }
    void LookY(float Value)
    {
        LookPitch = FMath::Clamp(LookPitch + 1.5f * Value, -60.f, 65.f);
    }
    void LookStickX(float Value)
    {
        LookStick.X = Value;
    }
    void LookStickY(float Value)
    {
        LookStick.Y = Value;
    }
    void LookAhead()
    {
        LookYaw = LookPitch = 0;
    }
    void RestartRide();
    void OpenPauseMenu();
    bool SaveLocation() const;
    void UpdatePose();
    void UpdateTraffic();
    void InstrumentsUpdate();
    void BuildCrashRider();
};
