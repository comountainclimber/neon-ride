#include "NeonRideGameMode.h"
#include "RidePawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

void ANeonRideGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UWorld* World = GetWorld();
    World->SpawnActor<APlayerStart>(FVector(0, 180, 115), FRotator::ZeroRotator);
    World->SpawnActor<ASkyAtmosphere>();
    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1200), FRotator(-38, -30, 0));
    if (Sun)
    {
        auto* Key = Sun->FindComponentByClass<UDirectionalLightComponent>();
        Key->SetMobility(EComponentMobility::Movable);
        Key->SetIntensity(3.4f);
        Key->SetAtmosphereSunLight(true);
    }
    if (auto* Sky = World->SpawnActor<ASkyLight>())
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
    const auto* PC = GetOwningPlayerController();
    const auto* Rider = PC ? Cast<ARidePawn>(PC->GetPawn()) : nullptr;
    if (!Canvas || !Rider) return;

    const auto& Ride = Rider->GetRide();
    const float Scale = FMath::Clamp(Canvas->ClipX / 1440.f, 0.75f, 1.5f);
    const float Y = Canvas->ClipY - 55.f * Scale;
    const FString Speed = FString::Printf(TEXT("%.0f km/h    %.0f / %.0f m"), Ride.Speed * 3.6, Ride.Distance, NeonRide::TrackLength);
    DrawText(Speed, FLinearColor::White, 24.f * Scale, Y, GEngine->GetMediumFont(), Scale);
    DrawText(TEXT("W throttle   S brake   A/D steer   R restart"), FLinearColor::White,
        24.f * Scale, Y + 23.f * Scale, GEngine->GetSmallFont(), Scale);
}
