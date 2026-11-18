#include "NeonRideGameMode.h"
#include "RidePawn.h"
#include "CourierCore.h"
#include "Skyway.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

void ANeonRideGameMode::InitGame(const FString &MapName, const FString &Options, FString &ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UWorld *World = GetWorld();
    World->SpawnActor<APlayerStart>(FVector(0, 180, 115), FRotator::ZeroRotator);
    if (auto *Atmosphere = World->SpawnActor<ASkyAtmosphere>())
        Atmosphere->GetComponent()->SetSkyLuminanceFactor(FLinearColor(0.028f, 0.043f, 0.085f));
    if (auto *Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -5000), FRotator::ZeroRotator))
    {
        Fog->GetComponent()->SetFogDensity(0.015f);
        Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.018f, 0.031f, 0.065f));
        Fog->GetComponent()->SetStartDistance(2800.f);
    }
    auto *Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1200), FRotator(-22, -45, 0));
    if (Sun)
    {
        auto *Key = Sun->FindComponentByClass<UDirectionalLightComponent>();
        Key->SetMobility(EComponentMobility::Movable);
        Key->SetLightColor(FLinearColor(0.56f, 0.69f, 1.f));
        Key->SetIntensity(1.8f);
        Key->SetAtmosphereSunLight(true);
    }
    if (auto *Sky = World->SpawnActor<ASkyLight>())
    {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(0.8f);
        Sky->GetLightComponent()->RecaptureSky();
    }
}

ANeonRideGameMode::ANeonRideGameMode()
{
    DefaultPawnClass = ARidePawn::StaticClass();
    HUDClass = ANeonRideHUD::StaticClass();
}

void ANeonRideHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto *PC = GetOwningPlayerController();
    const auto *Rider = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
    if (!Canvas || !Rider || PC->GetViewTarget() != Rider)
        return;

    const auto &Ride = Rider->GetRide();
    const float Scale = FMath::Clamp(Canvas->ClipX / 1440.f, .65f, 1.5f);
    const float X = 26.f * Scale, Y = 26.f * Scale;
    const FLinearColor Ink(.009f, .018f, .032f, .78f), Cyan(.49f, .9f, .92f);
    const int Road = Courier::RoadOf(Ride.s);
    const int Job = Rider->TrackedJob(), Dest=Rider->TrackedStop();
    const auto& Shift=Rider->GetShift();
    const FLinearColor Accent(.81f,.96f,.28f), Warm(.99f,.65f,.27f);
    auto Line=[&](const FString& Value,float Offset,const FLinearColor& Colour) {
        DrawText(Value,Colour,X+13*Scale,Y+Offset*Scale,GEngine->GetSmallFont(),Scale);
    };
    if(Ride.foot.active) {
        const double Distance=Circuit::BikeDistance(Ride);
        const bool Reach=Distance<=2.4;
        DrawRect(Ink,X,Y,410*Scale,81*Scale);
        DrawRect(Warm,X,Y,2*Scale,81*Scale);
        Line(FString::Printf(TEXT("RECOVER MOTORCYCLE     %.1f m"),Distance),6,Warm);
        Line(Reach?TEXT("E / A   Remount motorcycle"):TEXT("WASD / left stick   Walk to the bike"),31,Accent);
        Line(TEXT("Mouse / right stick   Look around"),54,Cyan);
        FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
        const FVector Location=Rider->GetActorLocation()+FVector(0,0,75.);
        FVector2D Marker;
        const float Pad=38*Scale;
        if(!PC->ProjectWorldLocationToScreen(Location,Marker)) {
            const double Turn=FMath::FindDeltaAngleDegrees(View.Yaw,(Location-Eye).Rotation().Yaw);
            Marker=FVector2D(Turn<0.?Pad:Canvas->ClipX-Pad,Canvas->ClipY*.55);
        }
        Marker.X=FMath::Clamp(Marker.X,double(Pad),double(Canvas->ClipX-Pad));
        Marker.Y=FMath::Clamp(Marker.Y,double(Pad),double(Canvas->ClipY-Pad));
        const float Radius=9*Scale;
        for(int Side:{-1,1}) {
            DrawLine(Marker.X,Marker.Y-Radius,Marker.X+Side*Radius,Marker.Y,Warm,2*Scale);
            DrawLine(Marker.X+Side*Radius,Marker.Y,Marker.X,Marker.Y+Radius,Warm,2*Scale);
        }
        for(int I=0;I<Courier::JobCount;++I) {
            if(Shift.status[I]!=2) continue;
            const auto Parcel=Courier::Contract(I,Shift.number);
            if(Parcel.windowSeconds<=0) continue;
            const int Sec=FMath::Max(0,FMath::CeilToInt32(Parcel.windowSeconds-(Ride.elapsed-Shift.pickedAt[I])));
            DrawText(FString::Printf(TEXT("%s   /   %02d:%02d"),UTF8_TO_TCHAR(Parcel.parcel),Sec/60,Sec%60),
                     Warm,X,Y+86*Scale,GEngine->GetSmallFont(),Scale);
            break;
        }
        return;
    }
    DrawRect(Ink,X,Y,425*Scale,(Dest<0?57:101)*Scale);
    DrawRect(Dest<0?Cyan:Warm,X,Y,2*Scale,(Dest<0?57:101)*Scale);
    Line(FString::Printf(TEXT("%s     /     SHIFT %02d"),*FString(UTF8_TO_TCHAR(Courier::RoadName(Road))).ToUpper(),Shift.number),5,Cyan);
    if(Dest>=0) {
        const auto Contract=Courier::Contract(FMath::Max(0,Job),Shift.number); const auto& Address=Courier::Stop(Dest);
        const auto Route=Courier::Directions(Ride.s,Dest,Courier::RampOpen(Shift));
        const bool Loaded=Job>=0 && Shift.status[Job]==2;
        const bool NavigationOnly=Job<0 || Dest!=(Loaded?Contract.to:Contract.from);
        Line(NavigationOnly?TEXT("WAYPOINT"):FString::Printf(TEXT("%s   %s"),Loaded?TEXT("DELIVER"):TEXT("COLLECT"),UTF8_TO_TCHAR(Contract.parcel)),29,Accent);
        Line(FString::Printf(TEXT("%02d   %s    %.2f KM"),Dest+1,UTF8_TO_TCHAR(Address.name),Route.meters*.001),50,FLinearColor::White);
        FString Guide;
        if(Route.meters<55.) Guide=TEXT("Stop in the right-hand service bay");
        else if(Route.nextRoad>=0 && Route.turnIn<340.) Guide=FString::Printf(TEXT("Keep right   >   %s    %d m"),UTF8_TO_TCHAR(Courier::RoadName(Route.nextRoad)),FMath::Max(0,FMath::RoundToInt(Route.turnIn)));
        else if(Route.nextRoad>=0) Guide=FString::Printf(TEXT("Next exit   %s   %.1f km"),UTF8_TO_TCHAR(Courier::RoadName(Route.nextRoad)),Route.turnIn*.001);
        else Guide=TEXT("Follow the road to the address");
        double Next=FMath::Min(340.,Route.meters);
        for(int Branch=1;Branch<Courier::RoadCount;++Branch) {
            const auto Fork=Courier::Entry(Branch);
            if(Courier::RoadOf(Fork)!=Road || (Branch==4 && !Courier::RampOpen(Shift))) continue;
            double Dist=Fork-Ride.s;
            if(Road==0 && Dist<0.) Dist+=Circuit::Length();
            if(Dist<0. || Dist>=Next) continue;
            Next=Dist;
            const TCHAR* Instruction=Branch==Route.nextRoad?TEXT("Keep right  > "):TEXT("Stay left past");
            Guide=FString::Printf(TEXT("%s   %s    %d m"),Instruction,UTF8_TO_TCHAR(Courier::RoadName(Branch)),FMath::RoundToInt32(Dist));
        }
        Line(Guide,74,Warm);
        if(Loaded && Contract.windowSeconds>0) {
            const int Left=FMath::Max(0,FMath::CeilToInt(Contract.windowSeconds-(Ride.elapsed-Shift.pickedAt[Job])));
            DrawText(Left?FString::Printf(TEXT("Requested in   %02d:%02d"),Left/60,Left%60):TEXT("Window passed   /   delivery still accepted"),Warm,X, Y+107*Scale,GEngine->GetSmallFont(),Scale);
        }
    } else {
        Line(Shift.finished?TEXT("Shift complete     M  next manifest"):TEXT("Free ride     M  parcel dispatch"),29,FLinearColor::White);
    }
    const int Stop=Courier::Nearby(Ride);
    FString Hint=Rider->CourierNotice();
    if(Hint.IsEmpty() && Stop>=0) Hint=FString::Printf(TEXT("%s    /    E  service counter"),UTF8_TO_TCHAR(Courier::Stop(Stop).name));
    if(Hint.IsEmpty() && Ride.elapsed<20.) Hint=TEXT("W / S  throttle / brake      A / D  steer      Shift  boost      M  dispatch");
    if(!Hint.IsEmpty() && Ride.crashAge<0.) DrawText(Hint,Accent,X,Canvas->ClipY-34*Scale,GEngine->GetSmallFont(),Scale);
}
