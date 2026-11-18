#include "NeonRideGameMode.h"
#include "RidePawn.h"
#include "Skyway.h"
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
    const float Scale = FMath::Clamp(Canvas->ClipX / 1440.f, 0.65f, 1.5f);
    const FLinearColor Ink(0.014f, 0.023f, 0.035f, 0.90f);
    const FLinearColor Accent(0.49f, 0.88f, 0.91f);
    const TCHAR* Place = TEXT("Skyway");
    switch (Ride.Place)
    {
        case NeonRide::EPlace::Market: Place = TEXT("Lantern Market"); break;
        case NeonRide::EPlace::Overlook: Place = TEXT("The Overlook"); break;
        case NeonRide::EPlace::Transit: Place = TEXT("Transit Yard"); break;
        case NeonRide::EPlace::Terrace: Place = TEXT("Lower Terrace"); break;
        default: break;
    }
    const float X = 20 * Scale, Y = Canvas->ClipY - 79 * Scale;
    DrawRect(Ink, X, Y, 315 * Scale, 59 * Scale);
    DrawText(FString::Printf(TEXT("%s    %.0f km/h"), Place, Ride.Speed * 3.6), FLinearColor::White,
        X + 10 * Scale, Y + 5 * Scale, GEngine->GetMediumFont(), Scale);
    DrawText(TEXT("M  map     Mouse  look     C  face forward"), Accent, X + 10 * Scale, Y + 32 * Scale, GEngine->GetSmallFont(), Scale);
    if (!Rider->IsMapOpen() || !Rider->GetSkyway()) return;

    const auto& District = Rider->GetSkyway()->GetDistrict();
    const float MapScale = FMath::Min(Canvas->ClipX / 1120.f, Canvas->ClipY / 1120.f);
    const float W = 1000.f * MapScale, H = 875.f * MapScale;
    const float Left = (Canvas->ClipX - W) / 2, Top = (Canvas->ClipY - H) / 2;
    auto Project = [&](double East, double South)
    {
        return FVector2D(Left + (East + 170) * MapScale, Top + (South + 765) * MapScale);
    };
    DrawRect(Ink, Left, Top, W, H);
    DrawText(TEXT("District map"), FLinearColor::White, Left + 12 * Scale, Top + 8 * Scale, GEngine->GetMediumFont(), Scale);
    const FVector2D Terrace = Project(-135, -715);
    DrawRect(FLinearColor(0.11f,0.17f,0.21f), Terrace.X, Terrace.Y, 880 * MapScale, 640 * MapScale);
    for (const auto& B : District.Buildings())
    {
        const FVector2D Corner = Project(B.X - B.WidthX / 2, B.Y - B.WidthY / 2);
        DrawRect(FLinearColor(0.23f,0.31f,0.36f), Corner.X, Corner.Y, B.WidthX * MapScale, B.WidthY * MapScale);
    }
    for (const auto& S : District.Surfaces())
    {
        if (!S.bRoad) continue;
        const auto A = Project(S.A.X,S.A.Y), B = Project(S.B.X,S.B.Y);
        DrawLine(A.X,A.Y,B.X,B.Y,Accent,FMath::Max(2.f, 5.f * MapScale));
    }
    auto Label = [&](double East, double South, const TCHAR* Name)
    {
        const auto At = Project(East, South);
        DrawText(Name, FLinearColor::White, At.X, At.Y, GEngine->GetSmallFont(), Scale);
    };
    Label(-105, -345, TEXT("Transit Yard"));
    Label(125, -300, TEXT("Lantern Market"));
    Label(530, -605, TEXT("The Overlook"));
    Label(15, 15, TEXT("Skyway"));
    const auto Here = Project(Ride.Position.X, Ride.Position.Y);
    DrawRect(FLinearColor::Yellow, Here.X - 4 * Scale, Here.Y - 4 * Scale, 8 * Scale, 8 * Scale);
    DrawLine(Here.X, Here.Y, Here.X + 20 * Scale * FMath::Cos(Ride.Heading),
        Here.Y + 20 * Scale * FMath::Sin(Ride.Heading), FLinearColor::Yellow, 3 * Scale);
}
