#include "RidePawn.h"
#include "Skyway.h"
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
    for (TActorIterator<ASkyway> It(GetWorld()); It; ++It)
    {
        Skyway = *It;
        break;
    }
    if (!Skyway)
    {
        Skyway = GetWorld()->SpawnActor<ASkyway>();
    }
    RestartRide();
}

void ARidePawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("Throttle", this, &ARidePawn::SetThrottle);
    Input->BindAxis("Brake", this, &ARidePawn::SetBrake);
    Input->BindAxis("Steer", this, &ARidePawn::SetSteer);
    Input->BindAction("RestartRide", IE_Pressed, this, &ARidePawn::RestartRide);
}

void ARidePawn::SetThrottle(float Value) { Throttle = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetBrake(float Value) { Brake = FMath::Clamp(Value, 0.f, 1.f); }
void ARidePawn::SetSteer(float Value) { Steer = FMath::Clamp(Value, -1.f, 1.f); }

void ARidePawn::RestartRide()
{
    Ride = NeonRide::FRideState();
    Remainder = 0.0;
    UpdatePose();
}

void ARidePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    constexpr double Step = 1.0 / 120.0;
    Remainder += FMath::Min(double(DeltaSeconds), 0.25);
    while (Remainder >= Step && Ride.Distance < NeonRide::TrackLength)
    {
        Ride.Advance(Step, Throttle, Brake, Steer);
        Remainder -= Step;
    }
    if (Ride.Distance >= NeonRide::TrackLength)
    {
        RestartRide();
        return;
    }
    UpdatePose();
}

void ARidePawn::UpdatePose()
{
    if (!Skyway) return;
    const FTransform Frame = Skyway->FrameAt(Ride.Distance);
    const FVector Position = Frame.TransformPosition(FVector(0, Ride.Offset * 100.0, 0));
    const double SlipYaw = FMath::RadiansToDegrees(FMath::Atan2(Ride.SideSpeed, FMath::Max(0.01, Ride.Speed)));
    SetActorLocationAndRotation(Position, FRotator(0, Frame.Rotator().Yaw + SlipYaw, 0));

    Body->SetRelativeRotation(FRotator(0, 0, Ride.Roll));
    const double RollRadians = FMath::DegreesToRadians(Ride.Roll);
    Camera->SetRelativeLocation(FVector(-1.4 * Ride.Acceleration, 52.0 * FMath::Sin(RollRadians),
        142.0 - 0.4 * Ride.Acceleration - 18.0 * (1.0 - FMath::Cos(RollRadians))));
    Camera->SetRelativeRotation(FRotator(-12.0 + 0.28 * Ride.Acceleration, 0.22 * Ride.Roll, 0.42 * Ride.Roll));
    const double LensMm = 17.0 - 3.0 * NeonRide::SmoothUnit((Ride.Speed - 36.0) / 22.0);
    Camera->SetFieldOfView(2.0 * FMath::RadiansToDegrees(FMath::Atan(18.0 / LensMm)));
}
