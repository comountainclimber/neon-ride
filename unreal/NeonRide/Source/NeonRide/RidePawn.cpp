#include "RidePawn.h"
#include "Skyway.h"
#include "TrafficVehicleActor.h"
#include "FilmArt.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "RideWeather.h"
#include "RideMirrors.h"
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
    CrashFill = CreateDefaultSubobject<UPointLightComponent>(TEXT("CrashBounce"));
    CrashFill->SetupAttachment(Camera);
    CrashFill->SetRelativeLocation(FVector(110, 85, 60));
    CrashFill->SetIntensityUnits(ELightUnits::Lumens);
    CrashFill->SetIntensity(380.f);
    CrashFill->SetLightColor(FLinearColor(.69f, .83f, 1.f));
    CrashFill->SetAttenuationRadius(1150);
    CrashFill->SetSourceRadius(80);
    CrashFill->SetCastShadows(false);
    CrashFill->SetVisibility(false);
    Weather = CreateDefaultSubobject<URideWeather>(TEXT("RideWeather"));
    Weather->SetupAttachment(RootComponent);
    Mirrors = CreateDefaultSubobject<URideMirrors>(TEXT("RearCapture"));
    Mirrors->SetupAttachment(RootComponent);
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
    ContactSparks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadContactSparks"));
    ContactSparks->SetupAttachment(RootComponent);
    ContactSparks->SetStaticMesh(Cube.Object);
    ContactSparks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ContactSparks->SetCastShadow(false);
    ContactSparks->SetCanEverAffectNavigation(false);
    ContactSparks->SetVisibility(false);
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
        auto *FilmBike=NeonRide::AttachFilmModel(Body, Bike,
                                  NeonRide::FilmTransform(Bike, FVector(25.32, 0, 0), FRotator(0, 180, 0), 210.7));
        Mirrors->Initialize(Body,FilmBike);
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
    if (auto *Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_Neon.M_Neon")))
    {
        auto *Ember = ContactSparks->CreateDynamicMaterialInstance(0, Glow);
        if (Ember)
            Ember->SetVectorParameterValue(TEXT("Color"), FLinearColor(3.2f, .83f, .14f));
        for (int I = 0; I < 24; ++I)
            ContactSparks->AddInstance(FTransform::Identity);
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
    bTitlePending = !FApp::IsUnattended();
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

void ARidePawn::OpenMenu(bool Opening)
{
    auto *PC = Cast<APlayerController>(Controller);
    auto *View = GetWorld()->GetGameViewport();
    if (!PC || !View || Menu.IsValid() || !PC->SetPause(true))
        return;
    bBoost = false;
    Weather->SetAudioPaused(true);
    Menu = SNew(SRideMenu).Rider(this).Opening(Opening);
    View->AddViewportWidgetContent(Menu.ToSharedRef(), 100);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Menu->ResumeButton);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
    if (!Opening)
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
        Weather->SetAudioPaused(false);
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
    Mirrors->Invalidate();
    Remainder = 0;
    bBoost = false;
    LookAhead();
    UpdateTraffic();
    UpdatePose();
    UpdateDisplays();
}

void ARidePawn::CaptureLocation(UNeonRideSave &Save) const
{
    Save.Distance = Ride.s;
    Save.Lane = FMath::Clamp(Ride.lateral, -Circuit::RideLimit, Circuit::RideLimit);
    Save.Heading = Ride.headingError;
    Save.Charge = Ride.charge;
    Save.Laps = FMath::FloorToInt(Ride.laps);
}

bool ARidePawn::RestoreLocation(const UNeonRideSave &Save)
{
    if (!Skyway || Save.Version != 2 || !FMath::IsFinite(Save.Distance) || Save.Distance < 0 ||
        Save.Distance >= Circuit::Length() || !FMath::IsFinite(Save.Lane) ||
        FMath::Abs(Save.Lane) > Circuit::RideLimit || !FMath::IsFinite(Save.Charge) || Save.Charge < 0 ||
        Save.Charge > 1 || Save.Laps < 0 || !FMath::IsFinite(Save.Heading))
        return false;
    RestartRide();
    Ride.s = Save.Distance;
    Ride.lateral = Save.Lane;
    Ride.headingError = FMath::UnwindRadians(Save.Heading);
    Mirrors->Invalidate();
    Ride.charge = Save.Charge;
    Ride.laps = Save.Laps;
    UpdatePose();
    UpdateDisplays();
    return true;
}

void ARidePawn::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Skyway)
        return;
    if (bTitlePending && Controller && GetWorld()->GetGameViewport())
    {
        OpenMenu(true);
        bTitlePending = !Menu.IsValid();
        if (Menu.IsValid())
            return;
    }
    LookX(LookStick.X * Dt * 70.f);
    LookY(LookStick.Y * Dt * 55.f);
    Remainder += FMath::Min(double(Dt), .25);
    const bool WasCrashing = Ride.crashAge >= 0.;
    constexpr double Step = 1. / 120.;
    while (Remainder >= Step)
    {
        Circuit::Advance(Ride, Throttle, Brake, Steer, bBoost, Step);
        Remainder -= Step;
    }
    UpdateTraffic();
    UpdatePose();
    if (!WasCrashing && Ride.crashAge >= 0.)
        Weather->PlayImpact(Ride.severity);
    Weather->UpdateWeather(Dt,Ride,Throttle,Brake);
    UpdateDisplays();
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
    const auto Road = Skyway->FrameAt(Ride.s, Ride.lateral);
    const bool Crashed = Ride.crashAge >= 0.;
    const FQuat Heading = FRotator(0, FMath::RadiansToDegrees(Ride.headingError), 0).Quaternion();
    SetActorLocationAndRotation(Road.GetLocation(), Crashed ? Road.GetRotation() : Road.GetRotation() * Heading);
    CrashFill->SetVisibility(Crashed);
    if (Ejected->IsVisible() != Crashed)
        Ejected->SetVisibility(Crashed, true);
    for (USceneComponent *Glove : Gloves)
        if (Glove)
            Glove->SetVisibility(!Crashed);
    if (Crashed)
    {
        const auto Shot = Circuit::Ejection(Ride);
        if (CrashMesh)
            CrashMesh->SetPosition(float(Shot.clipTime), false);

        const auto BikeDeck = Skyway->FrameAt(Shot.bikeS, Shot.bikeLateral);
        const auto RiderDeck = Skyway->FrameAt(Shot.riderS, Shot.riderLateral);
        const FVector BikeCOM = BikeDeck.TransformPosition(FVector(0, 0, 100. * Shot.bikeHeight));
        const FVector Pelvis = RiderDeck.TransformPosition(FVector(0, 0, 100. * Shot.riderHeight));
        const FQuat BikeRotation = BikeDeck.GetRotation() * FRotator(FMath::RadiansToDegrees(Shot.bikePitch),
                                                                     FMath::RadiansToDegrees(Shot.bikeYaw),
                                                                     FMath::RadiansToDegrees(Shot.bikeRoll))
                                                                .Quaternion();
        const FQuat RiderRotation = RiderDeck.GetRotation() * FRotator(FMath::RadiansToDegrees(Shot.riderPitch),
                                                                       FMath::RadiansToDegrees(Shot.riderYaw),
                                                                       FMath::RadiansToDegrees(Shot.riderRoll))
                                                                  .Quaternion();
        Ejected->SetWorldLocationAndRotation(Pelvis, RiderRotation);
        Body->SetWorldLocationAndRotation(BikeCOM - BikeRotation.RotateVector(FVector(0, 0, 54.)), BikeRotation);
        UpdateCrashEffects(Shot, BikeDeck);

        const double Gap = FVector::Distance(Pelvis, BikeCOM);
        const FVector Focus = FMath::Lerp(BikeCOM, Pelvis, .68);
        // Back away across the road as the bodies separate; keep the raised rider and sliding machine in view.
        const FVector Offset(Shot.cameraDistance * 40. + Gap * .14,
                             Ride.crashSide * (320. + FMath::Min(820., Gap * .53)),
                             Shot.cameraHeight * 76. + FMath::Min(270., Gap * .14));
        FVector EyeLocal = RiderDeck.InverseTransformPosition(Focus) + Offset;
        const double Lane = 100. * Shot.riderLateral;
        const double WithinDeck = FMath::Clamp(Lane + EyeLocal.Y, -550., 550.) - Lane;
        const double Excess = FMath::Abs(EyeLocal.Y - WithinDeck);
        EyeLocal.Y = WithinDeck;
        EyeLocal.X += .67 * Excess;
        EyeLocal.Z += .36 * Excess;
        const FVector Eye = RiderDeck.TransformPosition(EyeLocal);
        const double Jolt = (Shot.bikeHit * .11 + Shot.riderHit * .08) * FMath::Sin(Ride.crashAge * 52.);
        const FRotator Aim = (Focus - Eye).Rotation() + FRotator(Jolt, 0., Jolt * .8);
        Camera->SetWorldLocationAndRotation(Eye, Aim);
        CrashFill->SetWorldLocation(Pelvis + (Eye - Pelvis).GetSafeNormal() * 260. + FVector(0, 0, 85));
        Camera->SetFieldOfView(71. + FMath::Min(11., Gap * .008));
        return;
    }
    ContactSparks->SetVisibility(false);
    const double Roll = FMath::RadiansToDegrees(Ride.lean);
    Body->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0, 0, Roll));
    const double SpeedUnit = FMath::Clamp(Ride.v / 82., 0., 1.);
    const double Ripple = FMath::Sin(Ride.elapsed * 23.) * SpeedUnit * .32;
    Camera->SetRelativeLocation(FVector(-1. - SpeedUnit * 3., 36. * FMath::Sin(Ride.lean),
                                        141. + Ripple - 9. * FMath::Square(FMath::Sin(Ride.lean))));
    Camera->SetRelativeRotation(
        FRotator(-12. + LookPitch + Throttle * .65 - Brake * 1.5, LookYaw + Roll * .08, Roll * .37));
    Camera->SetFieldOfView(91. + 9. * SpeedUnit);
}

void ARidePawn::UpdateDisplays()
{
    Instruments->UpdateDisplay(Ride);
    if (Skyway) Mirrors->UpdateView(Ride.elapsed,Ride.crashAge>=0.,Skyway->FrameAt(Ride.s,Ride.lateral),Ride.headingError);
}

void ARidePawn::UpdateCrashEffects(const Circuit::CrashPose &Pose, const FTransform &Deck)
{
    const double Contact = Pose.bikeGround > .5 ? 1. : 0.;
    const double Scrape = Contact * FMath::Clamp((Pose.bikeSpeed - 1.5) / 18., 0., 1.);
    const double Strength = FMath::Clamp(FMath::Max(Scrape * .78, Pose.bikeHit), 0., 1.);
    const bool Active = Strength > .06 && Pose.bikeHeight < .9;
    ContactSparks->SetVisibility(Active);
    if (!Active)
        return;

    // Integrate short streaks in road space. The source follows the scraping metal at the contact patch.
    const int Count = ContactSparks->GetInstanceCount();
    const double Side = FMath::Sin(Pose.bikeRoll) > 0. ? -1. : 1.;
    for (int I = 0; I < Count; ++I)
    {
        const double Phase = FMath::Frac(Ride.crashAge * (3.1 + (I % 5) * .23) + I * .618034);
        const double Age = Phase * .24;
        const double Lift = 90. + 22. * (I % 7);
        const double Out = Side * (35. + 12. * (I % 6));
        const FVector Position(-20. - Age * (340. + Pose.bikeSpeed * 24. + (I % 4) * 85.), Side * 22. + Age * Out,
                               FMath::Max(1., 2. + Age * Lift - 490. * Age * Age));
        const FVector Direction(-340. - Pose.bikeSpeed * 24., Out, Lift - 980. * Age);
        const double Fade = FMath::Max(0., 1. - Phase) * Strength;
        const double Radius = (.004 + .008 * Fade) * (I + 1. <= Count * Strength + 1. ? 1. : 0.);
        const FTransform Streak(Direction.Rotation(), Position,
                                FVector((.045 + .19 * Fade) * (Radius > 0.), Radius, Radius));
        ContactSparks->UpdateInstanceTransform(I, Streak * Deck, true, I + 1 == Count, true);
    }
}
