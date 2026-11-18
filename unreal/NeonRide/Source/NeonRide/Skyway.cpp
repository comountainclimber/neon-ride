#include "Skyway.h"
#include "CircuitCore.h"
#include "FilmDistrict.h"
#include "CourierStreets.h"
#include "CourierCore.h"
#include "FilmArt.h"
#include "RidePawn.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

ASkyway::ASkyway()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .055f;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Text(
        TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
    SignMaterial = Text.Object;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CircuitOrigin")));
    Asphalt = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContinuousRoad"));
    Asphalt->SetupAttachment(RootComponent);
    Asphalt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Layer = [&](const TCHAR *Name) {
        auto *Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(Cube.Object);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return Mesh;
    };
    Steel = Layer(TEXT("BridgeSteel"));
    Edge = Layer(TEXT("EdgeLighting"));
    Reflector = Layer(TEXT("AmberMarkers"));
}

FTransform ASkyway::FrameAt(double S, double Lane) const
{
    const auto F = Circuit::At(S, Lane);
    return FTransform(FRotator(FMath::RadiansToDegrees(F.grade), FMath::RadiansToDegrees(F.yaw),
                               FMath::RadiansToDegrees(F.bank)),
                      FVector(F.x, F.y, F.z) * 100.) *
           GetActorTransform();
}

void ASkyway::BeginPlay()
{
    Super::BeginPlay();
    CourierStreets::Build(*this, *Asphalt, *Steel, *Edge, *Reflector);
    const TCHAR *Keys[] = {TEXT("night_porter"), TEXT("arcade_clerk"),         TEXT("pump_operator"),
                           TEXT("canal_keeper"), TEXT("cinema_projectionist"), TEXT("clinic_physician")};
    for (int I = 0; I < Courier::AddressCount; ++I)
    {
        Clients.Add(nullptr);
        auto At = FrameAt(Courier::Stop(I).s).GetRelativeTransform(GetActorTransform());
        const auto Plinth = FTransform(FVector(630., 840., 0.)) * At;
        ClientBases.Add(Plinth);
        if (auto *Mesh = NeonRide::LoadFilmModel(Keys[I]))
        {
            FTransform Local =
                NeonRide::FilmTransform(Mesh, FVector::ZeroVector, FRotator(0., 50., 0.), I == 2 ? 182. : 173.);
            Local.AddToTranslation(FVector(0, 0, -Mesh->GetBoundingBox().TransformBy(Local).Min.Z + 1.));
            Clients[I] = NeonRide::AttachFilmModel(RootComponent, Mesh, Local * Plinth);
        }
    }
    GetWorld()->SpawnActor<AFilmDistrict>(GetActorLocation(), GetActorRotation());
}

void ASkyway::Acknowledge(int Address)
{
    Greeting = Address;
    GreetingTime = GetWorld()->GetTimeSeconds();
}
int ASkyway::StaffCount() const
{
    int Count = 0;
    for (const auto &Person : Clients)
        if (IsValid(Person.Get()))
            ++Count;
    return Count;
}

void ASkyway::Tick(float Dt)
{
    Super::Tick(Dt);
    const auto *PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
        return;
    const auto *Rider = Cast<ARidePawn>(PC->GetPawn());
    const bool Open = Rider && Courier::RampOpen(Rider->GetShift());
    if (RampSign && Open != bRampOpen)
    {
        bRampOpen = Open;
        RampSign->SetText(FText::FromString(Open ? TEXT("AURORA RAMP   >") : TEXT("AURORA RAMP   /   CLOSED")));
        RampSign->SetTextRenderColor(Open ? FColor(192, 255, 134) : FColor(255, 170, 93));
    }
    const bool Dawn =
        Rider && (Rider->GetShift().number > 3 || (Rider->GetShift().number == 3 && Rider->GetShift().finished));
    if (DawnMarquee && bDawnScreening != Dawn)
    {
        bDawnScreening = Dawn;
        DawnMarquee->SetText(
            FText::FromString(Dawn ? TEXT("NOW SHOWING  /  FIRST LIGHT") : TEXT("MIDNIGHT SCREENING")));
        DawnMarquee->SetTextRenderColor(Dawn ? FColor(255, 223, 129) : FColor(163, 95, 186));
    }
    const FVector Player = GetActorTransform().InverseTransformPosition(PC->GetPawn()->GetActorLocation());
    const double Time = GetWorld()->GetTimeSeconds();
    for (int I = 0; I < Clients.Num(); ++I)
    {
        auto *Client = Clients[I].Get();
        if (!Client)
            continue;
        const auto &Base = ClientBases[I];
        const FVector To = Base.InverseTransformPosition(Player);
        if (To.SizeSquared() > FMath::Square(8500.))
            continue;
        const double Yaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), -160., -30.) + 180.;
        const double Age = Time - GreetingTime;
        const double Nod = I == Greeting && Age >= 0. && Age < 1.4 ? FMath::Sin(Age * PI / 1.4) * 9. : 0.;
        auto Local = NeonRide::FilmTransform(Client->GetStaticMesh(), FVector::ZeroVector, FRotator(Nod, Yaw, 0.),
                                             I == 2 ? 182. : 173.);
        Local.AddToTranslation(FVector(0, 0, 1. - Client->GetStaticMesh()->GetBoundingBox().TransformBy(Local).Min.Z));
        Client->SetRelativeTransform(Local * Base);
    }
}
