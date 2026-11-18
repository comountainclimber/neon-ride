#include "CourierStreets.h"
#include "Skyway.h"
#include "CircuitCore.h"
#include "CourierCore.h"
#include "CourierPresentation.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
struct FRoadPoint
{
    int Road;
    FVector Centre;
};
FIntPoint Cell(const FVector &P)
{
    return {int32(FMath::FloorToDouble(P.X / 1600.)), int32(FMath::FloorToDouble(P.Y / 1600.))};
}
using FStreetGrid = TMap<FIntPoint, TArray<FRoadPoint>>;
bool OtherPavement(const FStreetGrid &Grid, const FVector &Edge, int Road)
{
    const auto C = Cell(Edge);
    for (int Y = -1; Y <= 1; ++Y)
        for (int X = -1; X <= 1; ++X)
            if (const auto *Points = Grid.Find(C + FIntPoint(X, Y)))
                for (const auto &Point : *Points)
                    if (Point.Road != Road && FMath::Abs(Edge.Z - Point.Centre.Z) < 210. &&
                        FVector::DistSquared2D(Edge, Point.Centre) < FMath::Square(Circuit::HalfWidth * 100. + 12.))
                        return true;
    return false;
}
UTextRenderComponent *Sign(ASkyway &Owner, const FTransform &Frame, const FVector &Local, const FString &Title,
                           float Size, FLinearColor Colour)
{
    auto *Text = NewObject<UTextRenderComponent>(&Owner);
    Owner.AddInstanceComponent(Text);
    Text->SetupAttachment(Owner.GetRootComponent());
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetVerticalAlignment(EVRTA_TextCenter);
    Text->SetText(FText::FromString(Title));
    Text->SetWorldSize(Size);
    if (Owner.SignMaterial)
        Text->SetTextMaterial(Owner.SignMaterial);
    Text->SetTextRenderColor(Colour.ToFColorSRGB());
    Text->SetCastShadow(false);
    Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Text->SetRelativeTransform(FTransform(FRotator(0, 180, 0), Local) * Frame);
    Text->RegisterComponent();
    return Text;
}
}
void CourierStreets::Build(ASkyway &World, UProceduralMeshComponent &Asphalt, UInstancedStaticMeshComponent &Steel,
                           UInstancedStaticMeshComponent &Edge, UInstancedStaticMeshComponent &Marker)
{
    auto *Wet = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_CircuitAsphalt.M_CircuitAsphalt"));
    auto *Neon = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_Neon.M_Neon"));
    auto *Facade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_CircuitInfill.M_CircuitInfill"));
    auto Frame = [&](double S, double Lane = 0.) {
        return World.FrameAt(S, Lane).GetRelativeTransform(World.GetActorTransform());
    };
    auto Block = [](UInstancedStaticMeshComponent &Layer, const FTransform &At, FVector Offset, FVector Size) {
        Layer.AddInstance(FTransform(FQuat::Identity, Offset * 100., Size) * At);
    };
    auto Layer = [&](FName Name, UMaterialInterface *M) {
        auto *Batch = NewObject<UInstancedStaticMeshComponent>(&World, Name);
        World.AddInstanceComponent(Batch);
        Batch->SetupAttachment(World.GetRootComponent());
        Batch->SetStaticMesh(Steel.GetStaticMesh());
        Batch->SetMaterial(0, M);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCanEverAffectNavigation(false);
        Batch->RegisterComponent();
        return Batch;
    };
    TArray<UInstancedStaticMeshComponent *> Lighting;
    for (int Road = 0; Road < Courier::RoadCount; ++Road)
    {
        auto *L = Road == 0 ? &Edge : Layer(*FString::Printf(TEXT("DistrictTrim%d"), Road), Neon);
        if (auto *M = L->CreateDynamicMaterialInstance(0, Neon))
            M->SetVectorParameterValue(TEXT("Color"), CourierPresentation::Colour(Road));
        Lighting.Add(L);
    }
    auto *Walls = Layer(TEXT("AddressFacades"), Facade);
    auto *Tanks = Layer(TEXT("PumpingEquipment"), Steel.GetMaterial(0));
    Tanks->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    auto *Paint = Layer(TEXT("DepotPaint"), Neon);
    if (auto *M = Paint->CreateDynamicMaterialInstance(0, Neon))
        M->SetVectorParameterValue(TEXT("Color"), FLinearColor(.85f, .77f, .30f));
    if (auto *M = Steel.CreateDynamicMaterialInstance(0))
        M->SetVectorParameterValue(TEXT("Color"), FLinearColor(.086f, .11f, .145f));
    if (auto *M = Marker.CreateDynamicMaterialInstance(0, Neon))
        M->SetVectorParameterValue(TEXT("Color"), FLinearColor(.99f, .23f, .04f));

    FStreetGrid Grid;
    for (int Road = 0; Road < Courier::RoadCount; ++Road)
    {
        int Count = FMath::CeilToInt(Courier::RoadLength(Road) / 4.);
        for (int I = 0; I <= Count; ++I)
        {
            const FVector P = Frame(Courier::RoadStart(Road) + Courier::RoadLength(Road) * I / Count).GetLocation();
            Grid.FindOrAdd(Cell(P)).Add({Road, P});
        }
    }
    for (int Road = 0; Road < Courier::RoadCount; ++Road)
    {
        const double Start = Courier::RoadStart(Road), Length = Courier::RoadLength(Road);
        const int Count = FMath::CeilToInt(Length / 4.);
        const double Step = Length / Count;
        TArray<FVector> V, N;
        TArray<FVector2D> UV;
        TArray<int32> Indices;
        TArray<FProcMeshTangent> Tangents;
        for (int I = 0; I <= Count; ++I)
        {
            const double S = Start + Step * I;
            for (int Side : {-1, 1})
            {
                const auto EdgeFrame = Frame(S, Circuit::HalfWidth * Side);
                // Tiny layer separation prevents duplicate coplanar surface samples inside the forks.
                V.Add(EdgeFrame.GetLocation() + FVector(0, 0, Road * .32));
                N.Add(EdgeFrame.GetUnitAxis(EAxis::Z));
                Tangents.Emplace(EdgeFrame.GetUnitAxis(EAxis::Y), true);
                UV.Emplace(Side * Circuit::HalfWidth, Step * I);
            }
            if (I == Count)
                break;
            const int A = I * 2;
            Indices.Append({A, A + 1, A + 2, A + 2, A + 1, A + 3});
            const double Mid = S + Step * .5;
            const auto At = Frame(Mid);
            Block(Steel, At, {0, 0, -.86}, {Step + .08, 13.42, 1.10});
            bool IsBay = false;
            for (int Stop = 0; Stop < Courier::AddressCount; ++Stop)
            {
                const double Address = Courier::Stop(Stop).s;
                double Distance = FMath::Abs(Mid - Address);
                if (Road == 0 && Courier::RoadOf(Address) == 0)
                    Distance = FMath::Min(Distance, Length - Distance);
                if (Courier::RoadOf(Address) == Road && Distance < 13.)
                    IsBay = true;
            }
            for (int Side : {-1, 1})
            {
                const auto Rim = Frame(Mid, 6.69 * Side);
                if ((IsBay && Side == 1) || OtherPavement(Grid, Rim.GetLocation(), Road))
                    continue;
                Block(Steel, Rim, {0, 0, .32}, {Step + .08, .28, .70});
                Block(*Lighting[Road], Rim, {0, -Side * .02, .69}, {Step + .04, .19, .045});
                if (I % 6 == 0)
                    Block(Marker, Rim, {0, -Side * .2, .44}, {.52, .07, .14});
            }
            if (I % 16 == 0)
            {
                const double Height = At.GetLocation().Z / 100. + 68.;
                Steel.AddInstance(FTransform(FRotator(0, At.Rotator().Yaw, 0),
                                             At.GetLocation() - FVector(0, 0, Height * 50. + 95.),
                                             FVector(2.5, 7, Height)));
            }
        }
        Asphalt.CreateMeshSection_LinearColor(Road, V, Indices, N, UV, {}, Tangents, false);
        Asphalt.SetMaterial(Road, Wet);
    }

    for (int A = 0; A < Courier::AddressCount; ++A)
    {
        const auto &Address = Courier::Stop(A);
        const int Road = Courier::RoadOf(Address.s);
        const auto At = Frame(Address.s);
        static const FColor Tints[] = {FColor(255, 197, 122), FColor(243, 139, 189), FColor(99, 213, 237),
                                       FColor(241, 175, 112), FColor(215, 155, 245), FColor(152, 231, 186)};
        const auto Colour = FLinearColor::FromSRGBColor(Tints[A]);
        auto *Accent = Layer(*FString::Printf(TEXT("AddressLights%d"), A), Neon);
        if (auto *M = Accent->CreateDynamicMaterialInstance(0, Neon))
            M->SetVectorParameterValue(TEXT("Color"), Colour * 1.6f);
        auto &Glow = *Accent;
        // Rider-facing service counter with enclosed wall, cantilever roof and a recessed curbside bay.
        Block(Steel, At, {0, 9.1, -.34}, {28, 5.5, .65});
        Block(*Walls, At, {3, 14.3, 4.95}, {16, 5.1, 9.9});
        Block(Steel, At, {.7, 9.4, 4.35}, {19, 5.55, .30});
        Block(Glow, At, {.7, 6.72, 4.34}, {18.8, .11, .13});
        for (int End : {-1, 1})
        {
            Block(Steel, At, {End * 8.2, 11.55, 2.15}, {.34, .4, 4.4});
            Block(Glow, At, {End * 8.2, 11.30, 2.4}, {.19, .08, 2.4});
            Block(*Paint, At, {End * 10.5, 5.60, .035}, {.13, 1.25, .024});
        }
        for (int Locker = 0; Locker < 6; ++Locker)
        {
            const double X = -5.4 + Locker * 1.65;
            Block(Steel, At, {X, 11.23, 1.06}, {1.5, .9, 2.1});
            Block(Glow, At, {X + .43, 10.76, 1.35}, {.12, .07, .34});
        }
        Block(Steel, At, {-9.1, 11.25, 4.81}, {.24, 8.7, 1.98});
        Block(Glow, At, {-9.24, 11.25, 5.80}, {.09, 8.7, .08});
        const FString Name = UTF8_TO_TCHAR(Address.name);
        Sign(World, At, {-931., 1125., 489.}, Name.ToUpper(), FMath::Min(69.f, 1170.f / FMath::Max(1, Name.Len())),
             FLinearColor(.91f, .95f, .83f));
        Sign(World, At, {-931., 1125., 433.},
             FString::Printf(TEXT("%02d   /   %s"), A + 1, UTF8_TO_TCHAR(Address.district)).ToUpper(), 30, Colour);
        switch (A)
        {
        case 0: // Workshop roller shutter, brass caution stripe and open service trench cover.
            for (int Slat = 0; Slat < 12; ++Slat)
                Block(Steel, At, {-3.8, 11.65, .24 + Slat * .23}, {5.1, .20, .19});
            for (int Chevron = 0; Chevron < 6; ++Chevron)
                Block(Glow, At, {-6.1 + Chevron * .88, 10.8, .045}, {.47, .46, .04});
            break;
        case 1: // A second tier of parcel lockers and the pink retrieval indicators.
            for (int I = 0; I < 9; ++I)
                for (int Tier = 0; Tier < 3; ++Tier)
                {
                    Block(Steel, At, {-6. + I * 1.35, 11.28, .57 + Tier * 1.03}, {1.23, .82, .9});
                    Block(Glow, At, {-5.61 + I * 1.35, 10.84, .64 + Tier * 1.03}, {.19, .07, .10});
                }
            break;
        case 2: // Paired pump pressure vessels and their illuminated inspection bands.
            for (int Side : {-1, 1})
            {
                Block(*Tanks, At, {Side * 5.8, 17., 5.6}, {5.9, 5.9, 11.2});
                for (int I = 0; I < 5; ++I)
                    Block(Steel, At, {Side * 5.8, 13.98, 2. + I * 1.8}, {4.15, .36, .26});
                Block(Glow, At, {Side * 5.8, 13.94, 6.2}, {.4, .08, 7.1});
            }
            break;
        case 3: // An exchange gantry, hanging hook and stack of freight cages.
            for (int Side : {-1, 1})
                Block(Steel, At, {Side * 9.3, 14.2, 6.}, {.65, .68, 12.});
            Block(Glow, At, {0, 14.2, 11.8}, {19.4, .7, .39});
            Block(Steel, At, {-3.5, 14.2, 9.6}, {.15, .15, 4.5});
            for (int I = 0; I < 4; ++I)
                Block(Steel, At, {.5 + I * 2.1, 10.65, 1.05}, {1.94, 1.47, 2.1});
            break;
        case 4: // Projecting cinema marquee with individual warm bulbs along the lip.
            for (int I = 0; I < 26; ++I)
                Block(*Paint, At, {-8.8 + I * .7, 6.59, 4.17}, {.17, .17, .17});
            for (int I = 0; I < 3; ++I)
                Block(Glow, At, {0, 8.3, 5.1 + I * .47}, {15.6 - I * 1.55, 3.15 - I * .41, .17});
            Block(Steel, At, {-9.1, 11.25, 6.34}, {.22, 8.7, .83});
            World.DawnMarquee =
                Sign(World, At, {-931, 1125, 635}, TEXT("MIDNIGHT SCREENING"), 37., FLinearColor(.64f, .35f, .73f));
            break;
        case 5: // Mint glass vestibule and illuminated medical cross.
            for (int I = 0; I < 6; ++I)
                Block(Glow, At, {-6.5 + I * 2.6, 11.56, 2.9}, {.11, .1, 5.8});
            Block(Glow, At, {-5.1, 11.4, 7.3}, {.65, .18, 3.25});
            Block(Glow, At, {-5.1, 11.39, 7.3}, {3.25, .18, .65});
            break;
        }
        auto *Light = NewObject<UPointLightComponent>(&World);
        World.AddInstanceComponent(Light);
        Light->SetupAttachment(World.GetRootComponent());
        Light->SetRelativeLocation(At.TransformPosition(FVector(350, 850, 325)));
        Light->SetIntensityUnits(ELightUnits::Lumens);
        Light->SetIntensity(580.);
        Light->SetSourceRadius(35.);
        Light->SetAttenuationRadius(1150.);
        Light->SetLightColor(Colour);
        Light->SetCastShadows(false);
        Light->RegisterComponent();
    }
    for (int Road = 1; Road < Courier::RoadCount; ++Road)
    {
        const double Entry = Courier::Entry(Road);
        const int Parent = Courier::RoadOf(Entry);
        const double Before = FMath::Max(Courier::RoadStart(Parent), Entry - 30.);
        const auto At = Frame(Before);
        Block(Steel, At, {0, 11.2, 2.0}, {.24, .24, 4.0});
        Block(Steel, At, {0, 11.2, 4.35}, {.32, 8., 1.58});
        Block(*Lighting[Road], At, {-.17, 11.2, 5.12}, {.06, 8., .10});
        if (Road == 4)
            World.RampSign =
                Sign(World, At, {-20, 1120, 436}, TEXT("AURORA RAMP   /   CLOSED"), 41., FLinearColor(.99f, .6f, .2f));
        else
            Sign(World, At, {-20, 1120, 436}, FString(UTF8_TO_TCHAR(Courier::RoadName(Road))).ToUpper() + TEXT("  >"),
                 48., FLinearColor::White);
    }
}
