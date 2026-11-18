#include "RidePawn.h"
#include "Skyway.h"
#include "TrafficVehicleActor.h"
#include "FilmArt.h"
#include "RideWeather.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

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
        for (auto* Guide : Guides) Guide->SetVisibility(false);
        NeonRide::AttachFilmModel(Body, Bike, NeonRide::FilmTransform(Bike, FVector(25.32,0,0), FRotator(0,180,0), 210.7));
        if (auto* Glove = NeonRide::LoadFilmModel(TEXT("glove")))
        {
            for (int Side : {-1,1})
            {
                const FVector Grip(22.74, Side * 33.0, 97.55);
                NeonRide::AttachFilmModel(Body, Glove, NeonRide::FilmTransform(Glove, Grip, FRotator(-84, Side * 9, 0), 42.14, Side < 0));
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
    Weather->StartWeather();
}

void ARidePawn::EndPlay(const EEndPlayReason::Type Reason)
{
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
}

void ARidePawn::SetThrottle(float Value) { Throttle = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetBrake(float Value) { Brake = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetSteer(float Value) { Steer = FMath::Clamp(Value, -1.f, 1.f); }
void ARidePawn::LookX(float Delta) { LookYaw = FMath::Clamp(LookYaw + Delta * 1.5f, -145.f, 145.f); }
void ARidePawn::LookY(float Delta) { LookPitch = FMath::Clamp(LookPitch + Delta * 1.5f, -60.f, 70.f); }

void ARidePawn::RestartRide()
{
    Ride = NeonRide::FRideState();
    LookAhead();
    StreetTraffic = NeonRide::FStreetTraffic();
    if (Skyway) StreetTraffic.Advance(Skyway->GetDistrict(), Ride, 0.0);
    Remainder = 0.0;
    UpdatePose();
    UpdateTraffic();
}

void ARidePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Skyway) return;
    constexpr double Step = 1.0 / 120.0;
    LookYaw = FMath::Clamp(LookYaw + float(LookStick.X) * DeltaSeconds * 105.f, -145.f, 145.f);
    LookPitch = FMath::Clamp(LookPitch + float(LookStick.Y) * DeltaSeconds * 80.f, -60.f, 70.f);
    Remainder += FMath::Min(double(DeltaSeconds), 0.25);
    while (Remainder >= Step)
    {
        const NeonRide::FPoint Previous = Ride.Position;
        const double PreviousOdometer = Ride.Odometer;
        StreetTraffic.Advance(Skyway->GetDistrict(), Ride, Step);
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
    Weather->UpdateWeather(DeltaSeconds, Ride.Speed, Throttle);
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
    SetActorLocationAndRotation(Position, Frame.GetRotation() * FRotator(0, FMath::RadiansToDegrees(Ride.Heading), 0).Quaternion());

    Body->SetRelativeRotation(FRotator(0, 0, Ride.Roll));
    const double RollRadians = FMath::DegreesToRadians(Ride.Roll);
    Camera->SetRelativeLocation(FVector(-1.4 * Ride.Acceleration, 52.0 * FMath::Sin(RollRadians),
        142.0 - 0.4 * Ride.Acceleration - 18.0 * (1.0 - FMath::Cos(RollRadians))));
    Camera->SetRelativeRotation(FRotator(-12.0 + LookPitch + 0.28 * Ride.Acceleration, LookYaw + 0.22 * Ride.Roll, 0.42 * Ride.Roll));
    const double LensMm = 17.0 - 3.0 * NeonRide::SmoothUnit((Ride.Speed - 36.0) / 22.0);
    Camera->SetFieldOfView(2.0 * FMath::RadiansToDegrees(FMath::Atan(18.0 / LensMm)));
}
