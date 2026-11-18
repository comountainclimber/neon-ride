#include "RidePawn.h"
#include "Skyway.h"
#include "TrafficVehicleActor.h"
#include "FilmArt.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "RideWeather.h"
#include "RideInstruments.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "NeonRideSave.h"
#include "RideMenu.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Widgets/Input/SButton.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR *SaveSlot = TEXT("NeonRide_Circuit");
}

ARidePawn::ARidePawn()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Rider")));
    Body = CreateDefaultSubobject<USceneComponent>(TEXT("LeanPivot"));
    Body->SetupAttachment(RootComponent);
    Ejected = CreateDefaultSubobject<USceneComponent>(TEXT("Vesper"));
    Ejected->SetupAttachment(RootComponent);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("HelmetCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = false;
    Weather = CreateDefaultSubobject<URideWeather>(TEXT("RideWeather"));
    Weather->SetupAttachment(RootComponent);
    Instruments = CreateDefaultSubobject<URideInstruments>(TEXT("InstrumentCluster"));
    Instruments->SetupAttachment(Body);
    auto *Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("DipBeam"));
    Headlight->SetupAttachment(Body);
    Headlight->SetRelativeLocationAndRotation(FVector(93, 0, 85), FRotator(-6.5, 0, 0));
    Headlight->SetCastShadows(false);
    Headlight->SetIntensity(24000.f);
    Headlight->SetOuterConeAngle(26.f);
    Headlight->SetInnerConeAngle(14.f);
    Headlight->SetAttenuationRadius(6800.f);
    Headlight->SetLightColor(FLinearColor(.74f, .86f, 1.f));
    auto *Cockpit = CreateDefaultSubobject<UPointLightComponent>(TEXT("InstrumentSpill"));
    Cockpit->SetupAttachment(Body);
    Cockpit->SetRelativeLocation(FVector(53, 0, 141));
    Cockpit->SetCastShadows(false);
    Cockpit->SetIntensity(36.f);
    Cockpit->SetAttenuationRadius(130.f);
    Cockpit->SetLightColor(FLinearColor(.22f, .58f, .84f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Block = [&](const TCHAR *Name, FVector Position, FVector Scale, double Yaw) {
        auto *Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
        Part->SetupAttachment(Body);
        Part->SetStaticMesh(Cube.Object);
        Part->SetRelativeLocation(Position);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(FRotator(0, Yaw, 0));
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    };
    Block(TEXT("Tank"), FVector(26, 0, 69), FVector(0.85, 0.42, 0.32), 0);
    Block(TEXT("Instruments"), FVector(73, 0, 103), FVector(0.13, 0.41, 0.15), 0);
    Block(TEXT("LeftGrip"), FVector(46, -28, 94), FVector(0.13, 0.31, 0.07), -16);
    Block(TEXT("RightGrip"), FVector(46, 28, 94), FVector(0.13, 0.31, 0.07), 16);
}

void ARidePawn::BeginPlay()
{
    Super::BeginPlay();
    if (auto *Bike = NeonRide::LoadFilmModel(TEXT("bike")))
    {
        TArray<USceneComponent *> Guides;
        Body->GetChildrenComponents(false, Guides);
        for (auto *Guide : Guides)
            if (Guide->IsA<UStaticMeshComponent>() && Guide != Instruments)
                Guide->SetVisibility(false);
        NeonRide::AttachFilmModel(Body, Bike,
                                  NeonRide::FilmTransform(Bike, FVector(25.32, 0, 0), FRotator(0, 180, 0), 210.7));
        if (auto *Glove = NeonRide::LoadFilmModel(TEXT("glove")))
        {
            for (int Side : {-1, 1})
            {
                const FVector Grip(22.74, Side * 33.0, 97.55);
                Gloves.Add(NeonRide::AttachFilmModel(
                    Body, Glove, NeonRide::FilmTransform(Glove, Grip, FRotator(-84, Side * 9, 0), 42.14, Side < 0)));
            }
        }
    }
    for (TActorIterator<ASkyway> It(GetWorld()); It; ++It)
    {
        Skyway = *It;
        break;
    }
    if (!Skyway)
    {
        Skyway = GetWorld()->SpawnActor<ASkyway>();
    }
    BuildCrashRider();
    for (int I = 0; I < Circuit::TrafficCount; ++I)
    {
        auto *Vehicle = GetWorld()->SpawnActor<ATrafficVehicleActor>();
        Vehicles.Add(Vehicle);
        if (Vehicle)
            Vehicle->Configure(Circuit::Traffic(I, 0).truck != 0);
    }
    Instruments->StartDisplay();
    RestartRide();
    bSaveEnabled = !FApp::IsUnattended() && GetWorld()->WorldType == EWorldType::Game;
    if (bSaveEnabled && UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
        if (auto *Save = Cast<UNeonRideSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
            RestoreLocation(*Save);
    Weather->StartWeather();
}
void ARidePawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Skyway))
        SaveLocation();
    if (Menu.IsValid())
        ClosePauseMenu();
    if (Reason == EEndPlayReason::Destroyed)
    {
        for (ATrafficVehicleActor *Vehicle : Vehicles)
        {
            if (IsValid(Vehicle))
                Vehicle->Destroy();
        }
    }
    Super::EndPlay(Reason);
}

void ARidePawn::SetupPlayerInputComponent(UInputComponent *Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("Throttle", this, &ARidePawn::SetThrottle);
    Input->BindAxis("Brake", this, &ARidePawn::SetBrake);
    Input->BindAxis("Steer", this, &ARidePawn::SetSteer);
    Input->BindAction("RestartRide", IE_Pressed, this, &ARidePawn::RestartRide);
    Input->BindAxis("LookX", this, &ARidePawn::LookX);
    Input->BindAxis("LookY", this, &ARidePawn::LookY);
    Input->BindAxis("LookStickX", this, &ARidePawn::LookStickX);
    Input->BindAxis("LookStickY", this, &ARidePawn::LookStickY);
    Input->BindAction("LookAhead", IE_Pressed, this, &ARidePawn::LookAhead);
    Input->BindAction("Nitro", IE_Pressed, this, &ARidePawn::BoostPressed);
    Input->BindAction("Nitro", IE_Released, this, &ARidePawn::BoostReleased);
    Input->BindAction("PauseRide", IE_Pressed, this, &ARidePawn::OpenPauseMenu);
}

bool ARidePawn::SaveLocation() const
{
    if (!bSaveEnabled)
        return true;
    auto *Save = CastChecked<UNeonRideSave>(UGameplayStatics::CreateSaveGameObject(UNeonRideSave::StaticClass()));
    CaptureLocation(*Save);
    return UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

void ARidePawn::OpenPauseMenu()
{
    auto *PC = Cast<APlayerController>(Controller);
    auto *View = GetWorld()->GetGameViewport();
    if (!PC || !View || Menu.IsValid() || !PC->SetPause(true))
        return;
    bBoost = false;
    Menu = SNew(SRideMenu).Rider(this);
    View->AddViewportWidgetContent(Menu.ToSharedRef(), 100);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Menu->ResumeButton);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
    SaveLocation();
}

void ARidePawn::ClosePauseMenu()
{
    if (!Menu.IsValid())
        return;
    if (auto *View = GetWorld()->GetGameViewport())
        View->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    if (auto *PC = Cast<APlayerController>(Controller))
    {
        PC->SetPause(false);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

void ARidePawn::ReturnFromMenu()
{
    ClosePauseMenu();
    RestartRide();
    SaveLocation();
}

void ARidePawn::QuitFromMenu()
{
    if (SaveLocation())
        UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(Controller), EQuitPreference::Quit, false);
    else if (GEngine)
        GEngine->AddOnScreenDebugMessage(
            -1, 8.f, FColor::Orange, TEXT("Could not save the current location. Check free disk space and try again."));
}

void ARidePawn::RestartRide()
{
    Ride = Circuit::Start();
    Remainder = 0;
    bBoost = false;
    LookAhead();
    UpdateTraffic();
    UpdatePose();
    InstrumentsUpdate();
}

void ARidePawn::CaptureLocation(UNeonRideSave &Save) const
{
    Save.Distance = Ride.s;
    Save.Lane = Ride.lateral;
    Save.Charge = Ride.charge;
    Save.Laps = FMath::FloorToInt(Ride.laps);
}

bool ARidePawn::RestoreLocation(const UNeonRideSave &Save)
{
    if (!Skyway || Save.Version != 2 || !FMath::IsFinite(Save.Distance) || Save.Distance < 0 ||
        Save.Distance >= Circuit::Length() || !FMath::IsFinite(Save.Lane) ||
        FMath::Abs(Save.Lane) > Circuit::RideLimit || !FMath::IsFinite(Save.Charge) || Save.Charge < 0 ||
        Save.Charge > 1 || Save.Laps < 0)
        return false;
    RestartRide();
    Ride.s = Save.Distance;
    Ride.lateral = Save.Lane;
    Ride.charge = Save.Charge;
    Ride.laps = Save.Laps;
    UpdatePose();
    InstrumentsUpdate();
    return true;
}

void ARidePawn::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Skyway)
        return;
    LookX(LookStick.X * Dt * 70.f);
    LookY(LookStick.Y * Dt * 55.f);
    Remainder += FMath::Min(double(Dt), .25);
    constexpr double Step = 1. / 120.;
    while (Remainder >= Step)
    {
        Circuit::Advance(Ride, Throttle, Brake, Steer, bBoost, Step);
        Remainder -= Step;
    }
    UpdateTraffic();
    UpdatePose();
    Weather->UpdateWeather(Dt, Ride.v, Throttle, Ride.rpm, Ride.boost != 0);
    InstrumentsUpdate();
}

void ARidePawn::UpdateTraffic()
{
    if (!Skyway)
        return;
    for (int I = 0; I < Vehicles.Num(); ++I)
    {
        const auto Car = Circuit::Traffic(I, Ride.elapsed);
        if (Vehicles[I])
            Vehicles[I]->SetActorTransform(Skyway->FrameAt(Car.s, Car.lane));
    }
}

void ARidePawn::BuildCrashRider()
{
    // glTF +Z (suit forward) imports along Unreal +Y. Rotate the bind and its pelvis offset together.
    const FQuat Basis = FRotator(0, -90, 0).Quaternion();
    const FVector Pivot(1.679, 3.251, 104.948);
    const FTransform Mount(Basis, Basis.RotateVector(-Pivot));
    auto *Rig = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Rider/SK_Vesper.SK_Vesper"));
    auto *Clip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Rider/A_CrashEject.A_CrashEject"));
    if (Rig && Clip)
    {
        CrashMesh = NewObject<USkeletalMeshComponent>(this);
        AddInstanceComponent(CrashMesh);
        CrashMesh->SetupAttachment(Ejected);
        CrashMesh->SetSkeletalMeshAsset(Rig);
        CrashMesh->SetRelativeTransform(Mount);
        CrashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CrashMesh->RegisterComponent();
        CrashMesh->PlayAnimation(Clip, false);
        CrashMesh->SetPlayRate(0.f);
    }
    else if (auto *Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rider/SM_Vesper.SM_Vesper")))
        NeonRide::AttachFilmModel(Ejected, Mesh, Mount);
    Ejected->SetVisibility(false, true);
}

void ARidePawn::UpdatePose()
{
    if (!Skyway)
        return;
    const auto Frame = Skyway->FrameAt(Ride.s, Ride.lateral);
    SetActorTransform(Frame);
    const bool Crashed = Ride.crashAge >= 0.;
    if (Ejected->IsVisible() != Crashed)
        Ejected->SetVisibility(Crashed, true);
    for (USceneComponent *Glove : Gloves)
        if (Glove)
            Glove->SetVisibility(!Crashed);
    if (Crashed)
    {
        const auto Shot = Circuit::Ejection(Ride);
        if (CrashMesh)
            CrashMesh->SetPosition(float(Ride.crashAge), false);
        const auto RiderFrame = Skyway->FrameAt(Shot.riderS, Shot.riderLateral);
        const FVector RiderPos = RiderFrame.GetLocation() + FVector(0, 0, 100. * Shot.riderHeight);
        const FRotator Tumble(FMath::RadiansToDegrees(Shot.riderPitch), FMath::RadiansToDegrees(Shot.riderYaw),
                              FMath::RadiansToDegrees(Shot.riderRoll));
        Ejected->SetWorldLocationAndRotation(RiderPos, RiderFrame.GetRotation() * Tumble.Quaternion());
        Body->SetRelativeRotation(FRotator(0, 0, FMath::RadiansToDegrees(Shot.bikeRoll)));
        const FVector Eye = RiderFrame.TransformPosition(
            FVector(-Shot.cameraDistance * 55., -Ride.crashSide * 300., Shot.cameraHeight * 85.));
        const FVector Aim = RiderPos;
        Camera->SetWorldLocationAndRotation(Eye, (Aim - Eye).Rotation());
        Camera->SetFieldOfView(78);
        return;
    }
    const double Roll = FMath::RadiansToDegrees(Ride.lean);
    Body->SetRelativeRotation(
        FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Ride.sideV, FMath::Max(Ride.v, 5.))) * .75, Roll));
    const double SpeedUnit = FMath::Clamp(Ride.v / 82., 0., 1.);
    const double Ripple = FMath::Sin(Ride.elapsed * 23.) * SpeedUnit * .32;
    Camera->SetRelativeLocation(FVector(-1. - SpeedUnit * 3., 36. * FMath::Sin(Ride.lean),
                                        141. + Ripple - 9. * FMath::Square(FMath::Sin(Ride.lean))));
    Camera->SetRelativeRotation(
        FRotator(-12. + LookPitch + Throttle * .65 - Brake * 1.5, LookYaw + Roll * .08, Roll * .37));
    Camera->SetFieldOfView(91. + 9. * SpeedUnit);
}

void ARidePawn::InstrumentsUpdate()
{
    Instruments->UpdateDisplay(Ride);
}
