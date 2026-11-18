#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

namespace NeonRide
{
    UStaticMesh* LoadFilmModel(const TCHAR* Key);
    // Film-local space is +X forward, +Y right, +Z up. Dimensions use centimetres.
    FTransform FilmTransform(const UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
        double LongestDimensionCm, bool bMirrorRight = false);
    UStaticMeshComponent* AttachFilmModel(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Local);
}
