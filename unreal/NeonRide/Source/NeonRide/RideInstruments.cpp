#include "RideInstruments.h"
#include "RideHandling.h"
#include "TrafficRun.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

URideInstruments::URideInstruments()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    SetStaticMesh(Plane.Object);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    const auto Orientation = FRotationMatrix::MakeFromXY(FVector::YAxisVector,FVector(-.574,0,-.819)).ToQuat();
    SetRelativeTransform(FTransform(Orientation,FVector(74,0,112.55),FVector(.34,.11,1)));
}

void URideInstruments::StartDisplay()
{
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_InstrumentCluster.M_InstrumentCluster")))
        Display = CreateDynamicMaterialInstance(0,Material);
    SetVisibility(Display!=nullptr);
}

void URideInstruments::UpdateDisplay(float DeltaSeconds,const NeonRide::FRideState& Ride,const NeonRide::FStreetTraffic& Traffic)
{
    TimeUntilSample-=DeltaSeconds;
    if (!Display || TimeUntilSample>0) return;
    TimeUntilSample=.065f;
    Display->SetVectorParameterValue(TEXT("Telemetry"),FLinearColor(Ride.Speed*3.6/202.,FMath::DegreesToRadians(Ride.Roll),Ride.Acceleration/8.,1));
    const double C=FMath::Cos(Ride.Heading),S=FMath::Sin(Ride.Heading);
    FVector2D Blips[2] = {FVector2D(-2,-2),FVector2D(-2,-2)};
    double Range[2] = {150.,150.};
    for (const auto& Car : Traffic.Vehicles)
    {
        const auto& P=Car.Pose.Point;
        if (FMath::Abs(P.Z-Ride.Position.Z)>4.) continue;
        const double X=P.X-Ride.Position.X,Y=P.Y-Ride.Position.Y;
        const double Forward=X*C+Y*S,Right=-X*S+Y*C;
        if (Forward < -24. || Forward>100. || FMath::Abs(Right)>8.) continue;
        const double Dist=FMath::Abs(Forward);
        const FVector2D Point(.5+Right/17.,.78-Forward*.0075);
        if (Dist<Range[0])
        {
            Blips[1]=Blips[0]; Range[1]=Range[0];
            Blips[0]=Point; Range[0]=Dist;
        }
        else if (Dist<Range[1]) { Blips[1]=Point; Range[1]=Dist; }
    }
    Display->SetVectorParameterValue(TEXT("Traffic"),FLinearColor(Blips[0].X,Blips[0].Y,Blips[1].X,Blips[1].Y));
}
