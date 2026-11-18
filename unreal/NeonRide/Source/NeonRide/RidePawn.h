#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CircuitCore.h"
#include "CourierCore.h"
#include "RidePawn.generated.h"

class ASkyway;
class UCameraComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class ATrafficVehicleActor;
class URideWeather;
class URideMirrors;
class URideInstruments;
class SRideMenu;
class SCourierDispatch;
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
    const Courier::Shift& GetShift() const { return Shift; }
    int TrackedJob() const;
    int PannierJob(int Side) const { return Panniers[Side]; }
    int TrackedStop() const;
    bool ReserveParcel(int Job);
    bool ServiceParcel(int Job);
    void UseAtBike();
    void TrackParcel(int Job);
    void MarkAddress(int Address);
    void FinishShift();
    void ToggleDispatch();
    void CloseDispatch();
    FString CourierNotice() const;
    void CaptureLocation(UNeonRideSave &Save) const;
    bool RestoreLocation(const UNeonRideSave &Save);
#if WITH_DEV_AUTOMATION_TESTS
    void PreviewMenu(bool Opening) { OpenMenu(Opening); }
    void PreviewState(const Circuit::Ride &State)
    {
        Ride = State;
        UpdatePose();
        UpdateTraffic();
        UpdateDisplays();
    }
#endif
  private:
    UPROPERTY() TObjectPtr<ASkyway> Skyway;
    UPROPERTY() TObjectPtr<USceneComponent> Body;
    UPROPERTY() TObjectPtr<USceneComponent> Ejected;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> CrashMesh;
    UPROPERTY() TObjectPtr<UPointLightComponent> CrashFill;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ContactSparks;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<URideWeather> Weather;
    UPROPERTY() TObjectPtr<URideMirrors> Mirrors;
    UPROPERTY() TObjectPtr<URideInstruments> Instruments;
    UPROPERTY() TArray<TObjectPtr<ATrafficVehicleActor>> Vehicles;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Gloves;
    Circuit::Ride Ride = Circuit::Start();
    Courier::Shift Shift = Courier::NewShift();
    int Tracked = -1;
    int Waypoint = -1;
    int Panniers[2] = {-1,-1};
    void SyncPanniers();
    FString Notice;
    double NoticeUntil = 0.;
    TSharedPtr<SCourierDispatch> Dispatch;
    void ServiceStop();
    void Notify(const FString& Message);
    bool bBoost = false, bSaveEnabled = false, bTitlePending = false;
    double Remainder = 0, Throttle = 0, Brake = 0, Steer = 0;
    double FootForward=0., FootYawDelta=0.;
    void SetFootForward(float Value) { FootForward=FMath::Clamp(Value,-1.f,1.f); }
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
        if(Ride.foot.active) FootYawDelta+=FMath::DegreesToRadians(1.5*Value);
        else LookYaw = FMath::Clamp(LookYaw + 1.5f * Value, -140.f, 140.f);
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
    void RestartInput();
    void OpenPauseMenu() { OpenMenu(false); }
    void OpenMenu(bool Opening);
    bool SaveLocation() const;
    void UpdatePose();
    void UpdateTraffic();
    void UpdateDisplays();
    void BuildCrashRider();
    void UpdateCrashEffects(const Circuit::CrashPose &Pose, const FTransform &Deck);
};
