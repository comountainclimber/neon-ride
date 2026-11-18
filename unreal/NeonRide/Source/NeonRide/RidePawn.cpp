#include "RidePawn.h"
#include "Skyway.h"
#include "TrafficVehicleActor.h"
#include "FilmArt.h"
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
    const TCHAR* SaveSlot = TEXT("NeonRide_District");
}

ARidePawn::ARidePawn()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Rider")));
    Body = CreateDefaultSubobject<USceneComponent>(TEXT("LeanPivot"));
    Body->SetupAttachment(RootComponent);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("HelmetCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = false;
    Weather = CreateDefaultSubobject<URideWeather>(TEXT("RideWeather"));
    Weather->SetupAttachment(RootComponent);
    Instruments = CreateDefaultSubobject<URideInstruments>(TEXT("InstrumentCluster"));
    Instruments->SetupAttachment(Body);
    auto* Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("DipBeam"));
    Headlight->SetupAttachment(Body);
    Headlight->SetRelativeLocationAndRotation(FVector(93,0,85),FRotator(-6.5,0,0));
    Headlight->SetCastShadows(false);
    Headlight->SetIntensity(24000.f);
    Headlight->SetOuterConeAngle(26.f);
    Headlight->SetInnerConeAngle(14.f);
    Headlight->SetAttenuationRadius(6800.f);
    Headlight->SetLightColor(FLinearColor(.74f,.86f,1.f));
    auto* Cockpit = CreateDefaultSubobject<UPointLightComponent>(TEXT("InstrumentSpill"));
    Cockpit->SetupAttachment(Body);
    Cockpit->SetRelativeLocation(FVector(53,0,141));
    Cockpit->SetCastShadows(false);
    Cockpit->SetIntensity(36.f);
    Cockpit->SetAttenuationRadius(130.f);
    Cockpit->SetLightColor(FLinearColor(.22f,.58f,.84f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Block = [&](const TCHAR* Name, FVector Position, FVector Scale, double Yaw)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
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
    if (auto* Bike = NeonRide::LoadFilmModel(TEXT("bike")))
    {
        TArray<USceneComponent*> Guides;
        Body->GetChildrenComponents(false, Guides);
        for (auto* Guide : Guides)
            if (Guide->IsA<UStaticMeshComponent>() && Guide!=Instruments) Guide->SetVisibility(false);
        NeonRide::AttachFilmModel(Body, Bike, NeonRide::FilmTransform(Bike, FVector(25.32,0,0), FRotator(0,180,0), 210.7));
        if (auto* Glove = NeonRide::LoadFilmModel(TEXT("glove")))
        {
            for (int Side : {-1,1})
            {
                const FVector Grip(22.74, Side * 33.0, 97.55);
                Gloves.Add(NeonRide::AttachFilmModel(Body, Glove, NeonRide::FilmTransform(Glove, Grip, FRotator(-84, Side * 9, 0), 42.14, Side < 0)));
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
    for (const auto& Spec : StreetTraffic.Vehicles)
    {
        auto* Vehicle = GetWorld()->SpawnActor<ATrafficVehicleActor>();
        Vehicles.Add(Vehicle);
        if (Vehicle) Vehicle->Configure(Spec.Truck);
    }
    RestartRide();
    Instruments->StartDisplay();
    Instruments->UpdateDisplay(0.f,Ride,StreetTraffic);
    bSaveEnabled = !FApp::IsUnattended() && GetWorld()->WorldType == EWorldType::Game;
    if (bSaveEnabled && UGameplayStatics::DoesSaveGameExist(SaveSlot,0))
        if (const auto* Save = Cast<UNeonRideSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0)))
            RestoreLocation(*Save);
    Weather->StartWeather();
}

void ARidePawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Skyway)) SaveLocation();
    if (Menu.IsValid()) ClosePauseMenu();
    if (Reason == EEndPlayReason::Destroyed)
    {
        for (ATrafficVehicleActor* Vehicle : Vehicles)
        {
            if (IsValid(Vehicle)) Vehicle->Destroy();
        }
    }
    Super::EndPlay(Reason);
}

void ARidePawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("Throttle", this, &ARidePawn::SetThrottle);
    Input->BindAxis("Brake", this, &ARidePawn::SetBrake);
    Input->BindAxis("Steer", this, &ARidePawn::SetSteer);
    Input->BindAction("RestartRide", IE_Pressed, this, &ARidePawn::RestartRide);
    Input->BindAction("DistrictMap", IE_Pressed, this, &ARidePawn::ToggleMap);
    Input->BindAxis("LookX", this, &ARidePawn::LookX);
    Input->BindAxis("LookY", this, &ARidePawn::LookY);
    Input->BindAxis("LookStickX", this, &ARidePawn::LookStickX);
    Input->BindAxis("LookStickY", this, &ARidePawn::LookStickY);
    Input->BindAction("LookAhead", IE_Pressed, this, &ARidePawn::LookAhead);
    Input->BindAction("MountBike", IE_Pressed, this, &ARidePawn::ToggleMount);
    Input->BindAxis("WalkForward", this, &ARidePawn::SetWalkForward);
    Input->BindAction("PauseRide", IE_Pressed, this, &ARidePawn::OpenPauseMenu);
}

void ARidePawn::SetThrottle(float Value) { Throttle = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetBrake(float Value) { Brake = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetSteer(float Value) { Steer = FMath::Clamp(Value, -1.f, 1.f); }
void ARidePawn::LookX(float Delta)
{
    if (bWalking) Ride.Heading = FMath::UnwindRadians(Ride.Heading + FMath::DegreesToRadians(1.5 * Delta));
    else LookYaw = FMath::Clamp(LookYaw + Delta * 1.5f,-145.f,145.f);
}
void ARidePawn::LookY(float Delta) { LookPitch = FMath::Clamp(LookPitch + Delta * 1.5f, -60.f, 70.f); }

void ARidePawn::RestartRide()
{
    if (bWalking) ReattachBike();
    Ride = NeonRide::FRideState();
    LookAhead();
    StreetTraffic = NeonRide::FStreetTraffic();
    if (Skyway) StreetTraffic.Advance(Skyway->GetDistrict(), Ride, 0.0);
    Remainder = 0.0;
    UpdatePose();
    UpdateTraffic();
}

bool ARidePawn::CanMount() const
{
    const auto& A = Ride.Position;
    const auto& B = ParkedRide.Position;
    return FVector(A.X-B.X,A.Y-B.Y,A.Z-B.Z).SizeSquared() <= 3.6*3.6;
}

void ARidePawn::ReattachBike()
{
    Body->AttachToComponent(RootComponent,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    for (USceneComponent* Glove : Gloves) Glove->SetVisibility(true);
    bWalking = false;
}

void ARidePawn::ToggleMount()
{
    if (!Skyway) return;
    if (bWalking)
    {
        if (!CanMount()) return;
        ReattachBike();
        Ride = ParkedRide;
    }
    else
    {
        if (Ride.Speed > 1.4) return;
        Ride.Speed = Ride.Acceleration = Ride.TurnRate = Ride.Roll = 0.0;
        UpdatePose();
        ParkedRide = Ride;
        Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        for (USceneComponent* Glove : Gloves) Glove->SetVisibility(false);
        bWalking = true;
        const auto Here = Ride.Position;
        const NeonRide::FPoint Beside{Here.X+1.35*FMath::Sin(Ride.Heading),Here.Y-1.35*FMath::Cos(Ride.Heading),Here.Z};
        Ride.Position = Skyway->GetDistrict().Resolve(Beside,Here).Point;
    }
    LookAhead();
    UpdatePose();
    SaveLocation();
}

void ARidePawn::CaptureLocation(UNeonRideSave& Save) const
{
    const auto& Bike = bWalking ? ParkedRide : Ride;
    Save.BikePosition = FVector(Bike.Position.X,Bike.Position.Y,Bike.Position.Z);
    Save.BikeHeading = Bike.Heading;
    Save.bOnFoot = bWalking;
    Save.FootPosition = FVector(Ride.Position.X,Ride.Position.Y,Ride.Position.Z);
    Save.FootHeading = Ride.Heading;
}

bool ARidePawn::RestoreLocation(const UNeonRideSave& Save)
{
    if (!Skyway || Save.Version != 1) return false;
    auto ReadPose = [&](FVector Position,double Heading,NeonRide::FRideState& State)
    {
        if (Position.ContainsNaN() || !FMath::IsFinite(Heading) || FMath::Abs(Heading)>2*PI) return false;
        if (Position.Z < -28.5 && Skyway->GetDistrict().InsideBuilding(Position.X,Position.Y)) return false;
        NeonRide::FPoint Point{Position.X,Position.Y,Position.Z};
        const auto Ground = Skyway->GetDistrict().Resolve(Point,Point);
        const auto& P = Ground.Point;
        if (!Ground.bOnSurface || !Position.Equals(FVector(P.X,P.Y,P.Z),0.35)) return false;
        State = NeonRide::FRideState();
        State.Position = P;
        State.Heading = FMath::UnwindRadians(Heading);
        State.Place = Ground.Place;
        return true;
    };
    NeonRide::FRideState Bike, Foot;
    if (!ReadPose(Save.BikePosition,Save.BikeHeading,Bike)) return false;
    if (Save.bOnFoot && !ReadPose(Save.FootPosition,Save.FootHeading,Foot)) return false;
    if (bWalking) ReattachBike();
    Ride = Bike;
    LookAhead();
    UpdatePose();
    if (Save.bOnFoot)
    {
        ParkedRide = Bike;
        Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        for (USceneComponent* Glove : Gloves) Glove->SetVisibility(false);
        bWalking = true;
        Ride = Foot;
        UpdatePose();
    }
    return true;
}

bool ARidePawn::SaveLocation() const
{
    if (!bSaveEnabled) return true;
    auto* Save = CastChecked<UNeonRideSave>(UGameplayStatics::CreateSaveGameObject(UNeonRideSave::StaticClass()));
    CaptureLocation(*Save);
    return UGameplayStatics::SaveGameToSlot(Save,SaveSlot,0);
}

void ARidePawn::OpenPauseMenu()
{
    auto* PC = Cast<APlayerController>(Controller);
    auto* View = GetWorld()->GetGameViewport();
    if (!PC || !View || Menu.IsValid() || !PC->SetPause(true)) return;
    Menu = SNew(SRideMenu).Rider(this);
    View->AddViewportWidgetContent(Menu.ToSharedRef(),100);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Menu->ResumeButton);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
    SaveLocation();
}

void ARidePawn::ClosePauseMenu()
{
    if (!Menu.IsValid()) return;
    if (auto* View = GetWorld()->GetGameViewport()) View->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    if (auto* PC = Cast<APlayerController>(Controller))
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
        UKismetSystemLibrary::QuitGame(this,Cast<APlayerController>(Controller),EQuitPreference::Quit,false);
    else if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1,8.f,FColor::Orange,TEXT("Could not save the current location. Check free disk space and try again."));
}

void ARidePawn::Walk(double Step)
{
    const FVector2D Input = FVector2D(WalkForward,Steer).GetClampedToMaxSize(1.0);
    const double C = FMath::Cos(Ride.Heading), S = FMath::Sin(Ride.Heading);
    const double Distance = 3.5 * Step;
    const auto Here = Ride.Position;
    const NeonRide::FPoint Target{Here.X+Distance*(C*Input.X-S*Input.Y),Here.Y+Distance*(S*Input.X+C*Input.Y),Here.Z};
    const auto Ground = Skyway->GetDistrict().Resolve(Target,Here);
    if (!StreetTraffic.Touches(Ground.Point) || StreetTraffic.Touches(Here))
    {
        Ride.Position = Ground.Point;
        Ride.Place = Ground.Place;
    }
}

void ARidePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Skyway) return;
    constexpr double Step = 1.0 / 120.0;
    LookX(float(LookStick.X) * DeltaSeconds * 70.f);
    LookPitch = FMath::Clamp(LookPitch + float(LookStick.Y) * DeltaSeconds * 80.f, -60.f, 70.f);
    Remainder += FMath::Min(double(DeltaSeconds), 0.25);
    while (Remainder >= Step)
    {
        const NeonRide::FPoint Previous = Ride.Position;
        const double PreviousOdometer = Ride.Odometer;
        StreetTraffic.Advance(Skyway->GetDistrict(), Ride, Step);
        if (bWalking)
        {
            Walk(Step);
            Remainder -= Step;
            continue;
        }
        Ride.Advance(Skyway->GetDistrict(), Step, Throttle, Brake, Steer);
        // A vehicle contact sheds speed and leaves the rider in the same district.
        if (StreetTraffic.Touches(Ride.Position) && !StreetTraffic.Touches(Previous))
        {
            Ride.Position = Previous;
            Ride.Odometer = PreviousOdometer;
            Ride.Speed = 0.0;
        }
        Remainder -= Step;
    }
    UpdatePose();
    UpdateTraffic();
    Weather->UpdateWeather(DeltaSeconds, Ride.Speed, bWalking ? 0.0 : Throttle, bWalking);
    Instruments->UpdateDisplay(DeltaSeconds,bWalking ? ParkedRide : Ride,StreetTraffic);
}

void ARidePawn::UpdateTraffic()
{
    if (!Skyway) return;
    for (int I = 0; I < Vehicles.Num(); ++I)
    {
        auto* Vehicle = Vehicles[I].Get();
        if (!Vehicle) continue;
        const auto& Pose = StreetTraffic.Vehicles[I].Pose;
        const FTransform Local(FRotator(Pose.Pitch, Pose.Yaw, Pose.Bank), FVector(Pose.Point.X, Pose.Point.Y, Pose.Point.Z) * 100);
        const FTransform World = Local * Skyway->GetActorTransform();
        Vehicle->SetActorLocationAndRotation(World.GetLocation(), World.GetRotation());
    }
}

void ARidePawn::UpdatePose()
{
    if (!Skyway) return;
    const FTransform Frame = Skyway->GetActorTransform();
    const FVector Position = Frame.TransformPosition(FVector(Ride.Position.X, Ride.Position.Y, Ride.Position.Z) * 100.0);
    if (bWalking)
    {
        SetActorLocationAndRotation(Position,Frame.GetRotation() * FRotator(0,FMath::RadiansToDegrees(Ride.Heading),0).Quaternion());
        Camera->SetRelativeLocation(FVector(0,0,168));
        Camera->SetRelativeRotation(FRotator(LookPitch,0,0));
        Camera->SetFieldOfView(85.f);
        return;
    }
    SetActorLocationAndRotation(Position, Frame.GetRotation() * FRotator(Ride.Pitch, FMath::RadiansToDegrees(Ride.Heading), 0).Quaternion());

    Body->SetRelativeRotation(FRotator(0, 0, Ride.Roll));
    const double RollRadians = FMath::DegreesToRadians(Ride.Roll);
    Camera->SetRelativeLocation(FVector(-1.4 * Ride.Acceleration, 52.0 * FMath::Sin(RollRadians),
        142.0 - 0.4 * Ride.Acceleration - 18.0 * (1.0 - FMath::Cos(RollRadians))));
    Camera->SetRelativeRotation(FRotator(-12.0 + LookPitch + 0.28 * Ride.Acceleration, LookYaw + 0.22 * Ride.Roll, 0.42 * Ride.Roll));
    const double LensMm = 17.0 - 3.0 * NeonRide::SmoothUnit((Ride.Speed - 36.0) / 22.0);
    Camera->SetFieldOfView(2.0 * FMath::RadiansToDegrees(FMath::Atan(18.0 / LensMm)));
}
