#include "FilmArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

UStaticMesh* NeonRide::LoadFilmModel(const TCHAR* Key)
{
    const FString Package = FString::Printf(TEXT("/Game/Film/%s/SM_%s"), Key, Key);
    if (!FPackageName::DoesPackageExist(Package)) return nullptr;
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.SM_%s"), *Package, Key));
}

FTransform NeonRide::FilmTransform(const UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
    double LongestDimensionCm, bool bMirrorRight)
{
    const double Diameter = 2.0 * Mesh->GetBounds().BoxExtent.GetMax();
    const double Uniform = LongestDimensionCm / FMath::Max(Diameter, 0.001);
    // Interchange's glTF axes are X, Z, Y. Rotate the film's -Z forward to Unreal +X.
    const FTransform Basis(FRotator(0, 90, 0), FVector::ZeroVector, FVector(Uniform));
    return Basis * FTransform(Rotation, Location, FVector(1, bMirrorRight ? -1 : 1, 1));
}

UStaticMeshComponent* NeonRide::AttachFilmModel(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Local)
{
    auto* Component = NewObject<UStaticMeshComponent>(Parent->GetOwner());
    Parent->GetOwner()->AddInstanceComponent(Component);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetupAttachment(Parent);
    Component->SetStaticMesh(Mesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetRelativeTransform(Local);
    Component->RegisterComponent();
    return Component;
}
