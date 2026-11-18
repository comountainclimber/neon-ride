#include "NeonRideGameMode.h"
#include "RidePawn.h"
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
    const TCHAR *Names[] = {TEXT("RAINLINE"), TEXT("GLASS HEIGHTS"), TEXT("VIOLET REACH"), TEXT("LOWER RING")};
    const TCHAR *Sector = Names[FMath::Clamp(int(Ride.s * 4. / Circuit::Length()), 0, 3)];
    const auto Label = FString::Printf(TEXT("%s     LAP %02d     %.1f / %.1f KM"), Sector, 1 + int(Ride.laps),
                                       Ride.s / 1000., Circuit::Length() / 1000.);
    DrawRect(Ink, X, Y, 302 * Scale, 34 * Scale);
    DrawRect(Cyan, X, Y, 2 * Scale, 34 * Scale);
    DrawText(Label, FLinearColor::White, X + 13 * Scale, Y + 7 * Scale, GEngine->GetSmallFont(), Scale);
    if (Ride.elapsed < 18. && Ride.crashAge < 0.)
        DrawText(TEXT("W  accelerate     S  brake     A / D  steer     Shift  nitro"), Cyan, X,
                 Canvas->ClipY - 32 * Scale, GEngine->GetSmallFont(), Scale);
}