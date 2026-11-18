#include "RideMirrors.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StaticMeshResources.h"
#include "Misc/App.h"

namespace
{
// Traced inside the original painted bezel in a 1600 x 900 Unreal cockpit capture.
// Projecting in bike space keeps the glass attached through changes of lean and head pose.
const FVector2D Outline[] = {{323, 591}, {336, 573}, {347, 565}, {370, 561}, {450, 559}, {508, 586},
                             {513, 592}, {506, 601}, {480, 626}, {469, 630}, {371, 629}, {351, 625}, {330, 603}};
const FVector Eye(-1., 0., 141.);
const double ConstructionPitch = FMath::DegreesToRadians(-12.);
const double CosPitch = FMath::Cos(ConstructionPitch), SinPitch = FMath::Sin(ConstructionPitch);
const double Focal = 800. / FMath::Tan(FMath::DegreesToRadians(91. * .5));
struct FGlassVertex
{
    FVector Position;
    // Homogeneous screen coordinates make clipping along the source triangle perspective-correct.
    FVector Image;
    explicit FGlassVertex(FVector P) : Position(P)
    {
        const FVector V = P - Eye;
        const double D = CosPitch * V.X + SinPitch * V.Z;
        Image = FVector(800. * D - Focal * FMath::Abs(P.Y), 450. * D - Focal * (-SinPitch * V.X + CosPitch * V.Z), D);
    }
    double Distance(const FVector2D &A, const FVector2D &B) const
    {
        return (B.X - A.X) * (Image.Y - A.Y * Image.Z) - (B.Y - A.Y) * (Image.X - A.X * Image.Z);
    }
};
using FPolygon = TArray<FGlassVertex, TInlineAllocator<20>>;

void ClipInsert(FPolygon &Polygon)
{
    FPolygon Output;
    for (int I = 0; I < UE_ARRAY_COUNT(Outline) && !Polygon.IsEmpty(); ++I)
    {
        const auto &A = Outline[I], &B = Outline[(I + 1) % UE_ARRAY_COUNT(Outline)];
        Output.Reset();
        FGlassVertex Last = Polygon.Last();
        double Prev = Last.Distance(A, B);
        for (const FGlassVertex &V : Polygon)
        {
            const double Next = V.Distance(A, B);
            if ((Prev >= 0.) != (Next >= 0.))
                Output.Emplace(FMath::Lerp(Last.Position, V.Position, Prev / (Prev - Next)));
            if (Next >= 0.)
                Output.Add(V);
            Last = V;
            Prev = Next;
        }
        Swap(Polygon, Output);
    }
}
struct FInsert
{
    TArray<FVector> Positions;
    TArray<FVector2D> UV;
    TArray<int32> Indices;
    void Add(const FPolygon &Polygon, int Side)
    {
        const int First = Positions.Num();
        for (const auto &V : Polygon)
        {
            // A sub-millimetre offset leaves the authored metal lip in front of the glass.
            Positions.Add(V.Position + (Eye - V.Position).GetSafeNormal() * .075);
            const double X = (V.Image.X / V.Image.Z - 310.) / 220., Y = (V.Image.Y / V.Image.Z - 546.) / 98.;
            UV.Emplace(Side == 0 ? FMath::Lerp(.96, .49, X) : FMath::Lerp(.06, .53, X), FMath::Lerp(.19, .72, Y));
        }
        for (int I = 2; I < Polygon.Num(); ++I)
            Indices.Append({First, First + I - 1, First + I});
    }
};

bool MakeGlass(UProceduralMeshComponent &Glass, const UStaticMeshComponent &Film)
{
    const UStaticMesh *Mesh = Film.GetStaticMesh();
    if (!Mesh || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty())
        return false;
    const auto &LOD = Mesh->GetRenderData()->LODResources[0];
    const auto &Buffer = LOD.VertexBuffers.PositionVertexBuffer;
    const auto Indices = LOD.IndexBuffer.GetArrayView();
    TArray<FVector> Points;
    TBitArray<> InGlass(false, Buffer.GetNumVertices());
    Points.Reserve(Buffer.GetNumVertices());
    for (uint32 I = 0; I < Buffer.GetNumVertices(); ++I)
    {
        const auto P = Film.GetRelativeTransform().TransformPosition(FVector(Buffer.VertexPosition(I)));
        Points.Add(P);
        InGlass[I] =
            P.X > 50. && P.X < 84. && P.Z > 101. && P.Z < 129. && FMath::Abs(P.Y) > 20. && FMath::Abs(P.Y) < 52.;
    }
    FInsert Inserts[2];
    for (int I = 0; I + 2 < Indices.Num(); I += 3)
    {
        const int A = Indices[I], B = Indices[I + 1], C = Indices[I + 2];
        if (!InGlass[A] || !InGlass[B] || !InGlass[C])
            continue;
        FPolygon Triangle;
        Triangle.Emplace(Points[A]);
        Triangle.Emplace(Points[B]);
        Triangle.Emplace(Points[C]);
        ClipInsert(Triangle);
        if (Triangle.Num() >= 3)
            Inserts[Points[A].Y < 0 ? 0 : 1].Add(Triangle, Points[A].Y < 0 ? 0 : 1);
    }
    for (int Side = 0; Side < 2; ++Side)
    {
        const auto &Insert = Inserts[Side];
        if (Insert.Indices.IsEmpty())
            return false;
        Glass.CreateMeshSection_LinearColor(Side, Insert.Positions, Insert.Indices, {}, Insert.UV, {}, {}, false);
    }
    UE_LOG(LogTemp, Display, TEXT("Live film mirror triangles: left %d, right %d"), Inserts[0].Indices.Num() / 3,
           Inserts[1].Indices.Num() / 3);
    return true;
}
}

URideMirrors::URideMirrors()
{
    bCaptureEveryFrame = false;
    bCaptureOnMovement = false;
    FOVAngle = FMath::RadiansToDegrees(2. * FMath::Atan((768. / 320.) * FMath::Tan(FMath::DegreesToRadians(67. * .5))));
    MaxViewDistanceOverride = 135000.f;
    CaptureSource = ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;
    ShowFlags.SetMotionBlur(false);
    ShowFlags.SetTemporalAA(false);
    ShowFlags.SetScreenSpaceReflections(false);
    ShowFlags.SetAmbientOcclusion(false);
}
void URideMirrors::Initialize(USceneComponent *Body, UStaticMeshComponent *FilmBike)
{
    if (!FilmBike || !Body || !FApp::CanEverRender())
        return;
    auto *Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/M_RearMirror.M_RearMirror"));
    if (!Base)
        return;
    Glass = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("MirrorGlass"));
    GetOwner()->AddInstanceComponent(Glass);
    Glass->SetupAttachment(Body);
    Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Glass->SetCanEverAffectNavigation(false);
    Glass->SetCastShadow(false);
    Glass->RegisterComponent();
    if (!MakeGlass(*Glass, *FilmBike))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not reproduce both glass inserts from the original bike LOD"));
        return;
    }
    TextureTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RearRoad"));
    TextureTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
    TextureTarget->ClearColor = FLinearColor(.017f, .028f, .04f);
    TextureTarget->InitAutoFormat(768, 320);
    TextureTarget->UpdateResourceImmediate(true);
    auto *Material = UMaterialInstanceDynamic::Create(Base, Glass);
    Material->SetTextureParameterValue(TEXT("RearView"), TextureTarget);
    Glass->SetMaterial(0, Material);
    Glass->SetMaterial(1, Material);
    HiddenActors.Add(GetOwner());
}
void URideMirrors::UpdateView(double Time, bool Crashed, const FTransform &Road, double HeadingError)
{
    if (!TextureTarget || Crashed || GetWorld()->IsPaused())
        return;
    if (Time >= LastCaptureTime && Time - LastCaptureTime < 1. / 24.)
        return;
    const FRotator Frame = Road.Rotator();
    const double Pitch = FMath::RadiansToDegrees(FMath::Atan(-.058 - FMath::Tan(FMath::DegreesToRadians(Frame.Pitch))));
    SetWorldLocationAndRotation(Road.GetLocation() + FVector(0, 0, 122),
                                FRotator(Pitch, Frame.Yaw + FMath::RadiansToDegrees(HeadingError) + 180., 0.));
    CaptureScene();
    LastCaptureTime = Time;
    ++CaptureSerial;
}
