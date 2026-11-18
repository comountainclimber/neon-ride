#include "Skyway.h"
#include "CircuitCore.h"
#include "FilmDistrict.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

ASkyway::ASkyway()
{
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
    Paint = Layer(TEXT("Thermoplastic"));
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
    auto *Wet = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_RainAsphalt.M_RainAsphalt"));
    auto *Neon = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_Neon.M_Neon"));
    Asphalt->SetMaterial(0, Wet);
    auto Tint = [](UInstancedStaticMeshComponent *Layer, UMaterialInterface *Base, FLinearColor Color) {
        if (auto *Material = Layer->CreateDynamicMaterialInstance(0, Base))
            Material->SetVectorParameterValue(TEXT("Color"), Color);
    };
    Tint(Paint, Paint->GetMaterial(0), FLinearColor(.68f, .72f, .69f));
    Tint(Steel, Steel->GetMaterial(0), FLinearColor(.086f, .11f, .145f));
    Tint(Edge, Neon, FLinearColor(.04f, .65f, .87f));
    Tint(Reflector, Neon, FLinearColor(.99f, .23f, .04f));
    const int Sections = FMath::CeilToInt(Circuit::Length() / 4.);
    const double Step = Circuit::Length() / Sections;
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UV;
    TArray<int32> Triangles;
    TArray<FProcMeshTangent> Tangents;
    for (int I = 0; I <= Sections; ++I)
    {
        const double S = Step * I;
        for (int Side : {-1, 1})
        {
            const auto Frame = FrameAt(S, Circuit::HalfWidth * Side).GetRelativeTransform(GetActorTransform());
            Vertices.Add(Frame.GetLocation());
            Normals.Add(Frame.GetUnitAxis(EAxis::Z));
            Tangents.Emplace(Frame.GetUnitAxis(EAxis::X), false);
            UV.Emplace((Side + 1) * .5, S / 10.);
        }
        if (I < Sections)
        {
            int A = I * 2;
            Triangles.Append({A, A + 1, A + 2, A + 2, A + 1, A + 3});
        }
    }
    Asphalt->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV, TArray<FLinearColor>(), Tangents,
                                           false);
    auto Block = [&](UInstancedStaticMeshComponent *Layer, double S, double Lane, double Elevation,
                     const FVector &Size) {
        const auto Base = FrameAt(S, Lane).GetRelativeTransform(GetActorTransform());
        Layer->AddInstance(FTransform(FQuat::Identity, FVector(0, 0, Elevation * 100), Size) * Base);
    };
    // Use one continuous ribbon for the tires; position striping and the parapets from the same sampling frame.
    for (int I = 0; I < Sections; ++I)
    {
        const double S = (I + .5) * Step;
        Block(Steel, S, 0, -.57, FVector(Step + .08, 13.42, 1.10));
        if (I % 3 == 0)
            Block(Paint, S, 0, .014, FVector(3.7, .13, .018));
        for (int Side : {-1, 1})
        {
            Block(Paint, S, Side * 5.98, .018, FVector(Step + .015, .12, .016));
            Block(Steel, S, Side * 6.69, .32, FVector(Step + .08, .28, .70));
            Block(Edge, S, Side * 6.67, .69, FVector(Step + .045, .19, .045));
            if (I % 6 == 0)
                Block(Reflector, S, Side * 6.48, .44, FVector(.52, .07, .14));
        }
        if (I % 16 == 0)
        {
            const double Height = FrameAt(S).GetLocation().Z / 100. + 68.;
            const auto Base = FrameAt(S);
            Steel->AddInstance(FTransform(FRotator(0, Base.Rotator().Yaw, 0),
                                          Base.GetLocation() - FVector(0, 0, Height * 50 + 95),
                                          FVector(2.5, 7, Height)),
                               true);
        }
    }
    GetWorld()->SpawnActor<AFilmDistrict>(GetActorLocation(), GetActorRotation());
}
